# Shared, serialized game test for parallel work. Only ONE test runs at a time (global mutex), because the game window must be the
# foreground window for scripted keys and because GPU timings are only meaningful when nothing else renders.
#
#   powershell -NoProfile -File C:\CLAUDE\TOOLS\agent_test.ps1 -Dll <path\tyrender.dll> [-IniFile <ini>] [-Snaps "a,b"] [-Keys "W,W,D"] [-Perf]
#                                                            [-OutDir <dir>] [-Cam "dx dy dz yaw pitch"] [-HoldMs 120] [-Settle 8]
#
#  -Dll      the DLL to test (your own build output). It is copied over <game>\tyrender.dll for the duration of the test and the
#            original DLL + tyrender.ini are ALWAYS restored afterwards. Omit to test the DLL that is installed.
#  -IniFile  full tyrender.ini to use for this test (text file). Omit to use the installed one.
#  -Snaps    comma list of snapshot names; each is an in-game back-buffer capture (2560x1440 PNG) written to -OutDir as <name>.png
#  -Keys     comma list of keys sent after gameplay is reached (W,A,S,D,UP,DOWN,LEFT,RIGHT,SPACE,ENTER,ESC...) before the snapshots
#  -CamSeq   several views in ONE session: "name=dx dy dz yaw pitch;name2=dx dy dz yaw pitch"; the camera file is changed live and a snapshot named <name> is taken per view (wait -CamWait s each).
#  -AB       IN-SESSION A/B (the only pixel-comparable setup: same game session, same camera, same Ty): "A=C:\x\a.ini;B=C:\x\b.ini". For each variant the ini is swapped and a reload triggered\n#            (live-reloadable keys only), then every -CamSeq view is snapshotted as <variant>__<view>.png; with -Perf each variant prints PERF[<variant>] .... No -CamSeq = default view.
#  -Soak N   after boot, play for N minutes (random walk keys every ~20 s), checking every 30 s that the game is alive and TyOutputLog has no 'Out of memory'; prints SOAK OK / SOAK FAIL <reason>.
#  -Perf     wait for GPU-timer lines and print: frame ms | ao gi atmos post | engine+rest  (average of two 300-frame samples, gameplay only)
#  -Cam      debug camera offset "dx dy dz yawDeg pitchDeg" (right/up/forward in world units in the camera frame, then turn). Applied by the DLL via tyrender\cam.txt (debugcam.inl); the file is removed afterwards.
# Output: lines "PNG <path>", "PERF ...", "LOG <path to copy of tyrender.log>", "CAM ..." (last logged camera). Exit code 0 even on soft failures; read the lines.
param([string]$Dll = '', [string]$IniFile = '', [string]$Snaps = '', [string]$Keys = '', [switch]$Perf, [string]$OutDir = 'C:\CLAUDE\RESEARCH\shots\agents',
      [string]$Cam = '', [int]$Soak = 0, [string]$AB = '', [string]$CamSeq = '', [int]$CamWait = 4, [int]$HoldMs = 120, [int]$Settle = 8, [string]$Game = 'C:\CLAUDE\TY the Tasmanian Tiger', [int]$LockMinutes = 45)
$T = 'C:\CLAUDE\TOOLS'; $log = "$Game\tyrender.log"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$mutex = New-Object Threading.Mutex($false, 'Global\TYGameTest')
if (-not $mutex.WaitOne([TimeSpan]::FromMinutes($LockMinutes))) { 'LOCK TIMEOUT: another test held the game for too long'; return }
# crash-safe backup: fixed location + flag. If a previous run died (agent killed, rate limit...), the next run first restores the game folder from it.
$bk = 'C:\CLAUDE\TOOLS\_test_backup'; New-Item -ItemType Directory -Force $bk | Out-Null
if (Test-Path "$bk\IN_TEST.flag") { Copy-Item "$bk\tyrender.dll", "$bk\tyrender.ini" $Game -Force; Remove-Item "$Game\tyrender\cam.txt" -ErrorAction SilentlyContinue; 'recovered game folder from an interrupted test' }
Copy-Item "$Game\tyrender.dll", "$Game\tyrender.ini" $bk -Force; Set-Content "$bk\IN_TEST.flag" (Get-Date).ToString() -Encoding ascii
function GameplayLines { @(Select-String $log -Pattern "\[gpu\] frame ([\d.]+) ms \| ao ([\d.]+) gi ([\d.]+) atmos ([\d.]+) post ([\d.]+) \| engine\+rest ([\d.]+) ms \((\d+) draws" -ErrorAction SilentlyContinue |
    % { $g = $_.Matches[0].Groups; [pscustomobject]@{ f = [double]$g[1].Value; ao = [double]$g[2].Value; gi = [double]$g[3].Value; at = [double]$g[4].Value; po = [double]$g[5].Value; en = [double]$g[6].Value; d = [int]$g[7].Value } } | ? { $_.d -gt 1000 }) }
try {
    if ($Dll) { Copy-Item $Dll "$Game\tyrender.dll" -Force }
    if ($IniFile) { Copy-Item $IniFile "$Game\tyrender.ini" -Force }
    $extra = '-skipvideo -novsync'; if ($Cam) { Set-Content "$Game\tyrender\cam.txt" $Cam -Encoding ascii } else { Remove-Item "$Game\tyrender\cam.txt" -ErrorAction SilentlyContinue }
    $ready = $false
    for ($attempt = 1; $attempt -le 3 -and -not $ready; $attempt++) {
        Stop-Process -Name TY_tyrender -Force -ErrorAction SilentlyContinue; Start-Sleep 2
        try { & powershell -NoProfile -File "$T\ty_play.ps1" -Extra $extra -Settle 3 | Out-Null } catch { continue }
        & powershell -NoProfile -File "$T\key.ps1" -Keys 'ENTER' -DelayMs 1500 -Proc TY_tyrender | Out-Null; Start-Sleep 2
        & powershell -NoProfile -File "$T\key.ps1" -Keys 'ENTER' -DelayMs 1500 -Proc TY_tyrender | Out-Null
        for ($i = 0; $i -lt 40 -and -not $ready; $i++) { Start-Sleep 3; if ((GameplayLines).Count -ge 1) { $ready = $true } }
        if (-not $ready) { "boot attempt $attempt did not reach gameplay (game window must be foreground; retrying)" }
    }
    if (-not $ready) { 'FAILED: gameplay not reached after 3 attempts'; return }
    if ($env:TY_CAMWAIT) { $CamWait = [int]$env:TY_CAMWAIT }
    if ($env:TY_SETTLE) { Start-Sleep ([int]$env:TY_SETTLE) } else { Start-Sleep $Settle }
    if ($Keys) { & powershell -NoProfile -File "$T\key.ps1" -Keys $Keys -HoldMs $HoldMs -DelayMs 400 -Proc TY_tyrender | Out-Null; Start-Sleep 3 }
    if ($AB) {
        $views = @(); foreach ($v in ($CamSeq -split ';' | ? { $_.Trim() })) { $views += , ($v -split '=', 2) }; if (-not $views) { $views = @(, @('default', '')) }
        foreach ($var in ($AB -split ';' | ? { $_.Trim() })) {
            $vn, $vp = $var -split '=', 2; $vn = $vn.Trim(); Copy-Item $vp.Trim() "$Game\tyrender.ini" -Force; New-Item -ItemType File -Force "$Game\tyrender\reload" | Out-Null; Start-Sleep 3
            $before = (GameplayLines).Count
            foreach ($vw in $views) {
                if ($vw[0].Trim() -like 'mot*') {   # motion-stability capture: view text = "yawStepDeg frames [x y z yaw pitch]" (see debugcam.inl MotionPoll) -> <variant>__<view>.raw
                    $raw = "$Game\tyrender\snaps\motion_$vn.raw"; Remove-Item $raw -ErrorAction SilentlyContinue; Remove-Item "$Game\tyrender\cam.txt" -ErrorAction SilentlyContinue; Start-Sleep $CamWait
                    Set-Content "$Game\tyrender\motion.req" ("{0} {1}" -f $vn, $vw[1].Trim()) -Encoding ascii
                    $t0 = Get-Date; while (-not (Test-Path $raw) -and ((Get-Date) - $t0).TotalSeconds -lt 90) { Start-Sleep 1 }; Start-Sleep 1
                    if (Test-Path $raw) { New-Item -ItemType Directory -Force $OutDir | Out-Null; Copy-Item $raw ("$OutDir\{0}__{1}.raw" -f $vn, $vw[0].Trim()) -Force; "RAW $vn" } else { "MOTION FAILED $vn" }
                    continue
                }
                if ($vw[1]) { Set-Content "$Game\tyrender\cam.txt" $vw[1].Trim() -Encoding ascii } else { Remove-Item "$Game\tyrender\cam.txt" -ErrorAction SilentlyContinue }
                Start-Sleep $CamWait
                try { $p2 = & powershell -NoProfile -File "$T\snap.ps1" -Name ("{0}__{1}" -f $vn, $vw[0].Trim()) -OutDir $OutDir; "PNG $p2" } catch { "SNAP FAILED $vn $($vw[0])" }
            }
            if ($Perf) { if ($env:TY_PERFCAM) { Set-Content "$Game\tyrender\cam.txt" $env:TY_PERFCAM -Encoding ascii } else { Remove-Item "$Game\tyrender\cam.txt" -ErrorAction SilentlyContinue }; Start-Sleep $CamWait; $before = (GameplayLines).Count   # PERF is always measured at the DEFAULT camera (scene cost depends on the view)
                $dl2 = (Get-Date).AddSeconds(150); while ((Get-Date) -lt $dl2 -and (GameplayLines).Count -lt $before + 2) { Start-Sleep 3 }
                $u2 = @(GameplayLines) | select -Last 2; if ($u2) { "PERF[$vn] frame {0:N2} ms | ao {1:N2} gi {2:N2} atmos {3:N2} post {4:N2} | engine+rest {5:N2} ms" -f ($u2 | measure f -Average).Average, ($u2 | measure ao -Average).Average, ($u2 | measure gi -Average).Average, ($u2 | measure at -Average).Average, ($u2 | measure po -Average).Average, ($u2 | measure en -Average).Average } else { "PERF[$vn] no samples" } }
        }
        $CamSeq = ''; $Perf = $false
    }    if ($Soak -gt 0) {
        $end = (Get-Date).AddMinutes($Soak); $keysPool = 'W', 'A', 'S', 'D', 'W', 'D', 'A', 'W'; $rnd = New-Object Random; $why = ''; $tyLog = "$Game\TyOutputLog.txt"
        while ((Get-Date) -lt $end -and -not $why) {
            if (-not (Get-Process TY_tyrender -ErrorAction SilentlyContinue)) { $why = 'game process exited (crash)'; break }
            if ((Test-Path $tyLog) -and (Select-String $tyLog -Pattern 'Out of memory' -Quiet)) { $why = 'engine reported Out of memory'; break }
            if (Select-String $log -Pattern '!!!|access violation' -Quiet) { $why = 'crash logger fired (!!!)'; break }
            & powershell -NoProfile -File "$T\key.ps1" -Keys $keysPool[$rnd.Next($keysPool.Count)] -HoldMs ($rnd.Next(300, 1500)) -DelayMs 200 -Proc TY_tyrender | Out-Null; Start-Sleep 20
        }
        if ($why) { "SOAK FAIL after $([int]((Get-Date) - $end.AddMinutes(-$Soak)).TotalMinutes) min: $why" } else { "SOAK OK $Soak minutes" }
        $snapEnd = & powershell -NoProfile -File "$T\snap.ps1" -Name soak_end -OutDir $OutDir 2>$null; if ($snapEnd) { "PNG $snapEnd" }
    }    foreach ($v in ($CamSeq -split ';' | ? { $_.Trim() })) {
        $nm, $pose = $v -split '=', 2; Set-Content "$Game\tyrender\cam.txt" $pose.Trim() -Encoding ascii; Start-Sleep $CamWait
        try { $p2 = & powershell -NoProfile -File "$T\snap.ps1" -Name $nm.Trim() -OutDir $OutDir; "PNG $p2" } catch { "SNAP FAILED $nm : $($_.Exception.Message)" }
    }
    foreach ($n in ($Snaps -split ',' | ? { $_.Trim() })) {
        try { $p = & powershell -NoProfile -File "$T\snap.ps1" -Name $n.Trim() -OutDir $OutDir; "PNG $p" } catch { "SNAP FAILED $n : $($_.Exception.Message)" }
    }
    if ($Perf) {
        $dl = (Get-Date).AddSeconds(300); $res = @()
        $start = (GameplayLines).Count
        while ((Get-Date) -lt $dl) { $res = @(GameplayLines); if ($res.Count -ge $start + 2) { break }; Start-Sleep 3 }
        $u = $res | select -Last 2
        if ($u) { "PERF frame {0:N2} ms | ao {1:N2} gi {2:N2} atmos {3:N2} post {4:N2} | engine+rest {5:N2} ms" -f ($u | measure f -Average).Average, ($u | measure ao -Average).Average, ($u | measure gi -Average).Average, ($u | measure at -Average).Average, ($u | measure po -Average).Average, ($u | measure en -Average).Average }
        else { 'PERF no samples' }
    }
    $camLine = Select-String $log -Pattern '\[cam\]' | select -Last 1 | % Line; if ($camLine) { "CAM $camLine" }
    $lcopy = Join-Path $OutDir 'tyrender_last.log'; Copy-Item $log $lcopy -Force; "LOG $lcopy"
    $err = Select-String $log -Pattern '!!!|FAILED|does not compile|stage .* compile' | select -First 6 | % Line; if ($err) { "LOG ERRORS:"; $err }
} finally {
    Stop-Process -Name TY_tyrender -Force -ErrorAction SilentlyContinue; Start-Sleep 2; Remove-Item "$Game\tyrender\cam.txt" -ErrorAction SilentlyContinue
    Copy-Item "$bk\tyrender.dll", "$bk\tyrender.ini" $Game -Force; Remove-Item "$bk\IN_TEST.flag" -ErrorAction SilentlyContinue
    $mutex.ReleaseMutex()
}

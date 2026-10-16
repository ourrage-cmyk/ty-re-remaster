# Merge one agent's work into the STAGING tree (C:\CLAUDE\SRC\merge), build it, and run a regression suite. The master is never touched.
#   powershell -NoProfile -File merge_agent.ps1 -Agent atmos [-NoTest]
# Method (research: NEXT_STEPS_RESEARCH.md (b)): base = master (agents forked from it; unchanged files hash-equal). Owned files are copied from the agent folder (they are that agent's
# alone); tyrender.cpp (several agents touch different places) is three-way merged with `git merge-file`; any OTHER file the agent changed is reported and three-way merged too.
# Exit text contains CONFLICT (cpp conflict, staging left unchanged, conflict file written) or FAIL (build/test) so todo_runner marks the item failed.
param([Parameter(Mandatory)][ValidateSet('atmos', 'gi', 'ao', 'tess', 'light')][string]$Agent, [switch]$NoTest)
$master = 'C:\CLAUDE\SRC\tyrender'; $stage = 'C:\CLAUDE\SRC\merge'; $ag = "C:\CLAUDE\SRC\agents\$Agent"; $res = 'C:\CLAUDE\RESEARCH\results'; $git = 'C:\Program Files\Git\cmd\git.exe'
$owned = @{ atmos = @('atmos.inl'); gi = @('gi.inl', 'gi2.inl'); ao = @('ao.inl', 'gpuprof.inl'); tess = @('tess.inl', 'pbr.inl'); light = @('light.inl', 'fx.inl') }
$utf8 = New-Object Text.UTF8Encoding($false); $log = @()
if (-not (Test-Path "$stage\tyrender.cpp")) { New-Item -ItemType Directory -Force $stage | Out-Null; Get-ChildItem $master -File | ? { $_.Extension -in '.cpp', '.inl', '.inc', '.c', '.ps1', '.md' } | Copy-Item -Destination $stage -Force; $log += "staging created from master" }
# 1) owned files
foreach ($f in $owned[$Agent]) { if (Test-Path "$ag\$f") { Copy-Item "$ag\$f" "$stage\$f" -Force; $log += "owned: $f copied" } else { $log += "WARNING owned file missing in agent folder: $f" } }
# 2) shared / other changed files: three-way merge (staging=ours, master=base, agent=theirs)
$others = Get-ChildItem $ag -File | ? { $_.Extension -in '.cpp', '.inl' -and $_.Name -notin $owned[$Agent] } | ? { (Test-Path "$master\$($_.Name)") -and ((Get-FileHash $_.FullName).Hash -ne (Get-FileHash "$master\$($_.Name)").Hash) }
$conflict = 0
foreach ($o in $others) {
    $n = $o.Name; if ($n -in 'debugcam.inl', 'stateguard.inl') { continue }   # infrastructure: master version wins
    $tmp = "$env:TEMP\merge_$n"; Copy-Item "$stage\$n" $tmp -Force
    $out = & $git merge-file -p -L staging -L base -L "agent_$Agent" "$stage\$n" "$master\$n" "$ag\$n" 2>&1; $c = $LASTEXITCODE
    if ($c -eq 0) { [IO.File]::WriteAllText("$stage\$n", ($out -join "`n") + "`n", $utf8); $log += "three-way merged $n cleanly" }
    elseif ($c -gt 0) { [IO.File]::WriteAllText("$stage\$n.CONFLICT", ($out -join "`n") + "`n", $utf8); $log += "CONFLICT in $n ($c hunks) -> $stage\$n.CONFLICT (staging file unchanged)"; $conflict += $c }
    else { $log += "FAIL git merge-file error on $n" ; $conflict += 1 }
}
if ($conflict) { $log; "CONFLICT merge of $Agent stopped"; return }
# 3) build staging
$b = & powershell -NoProfile -File "$stage\build.ps1" 2>&1 | Out-String
if (-not (Test-Path "$stage\build.ps1")) { Copy-Item "$master\build.ps1" "$stage\build.ps1" -Force; $b = & powershell -NoProfile -File "$stage\build.ps1" 2>&1 | Out-String }
$dll = "$stage\out\tyrender.dll"
if (-not (Test-Path $dll) -or ((Get-Item $dll).LastWriteTime -lt (Get-Date).AddMinutes(-3))) { $log; ($b -split "`n" | Select-String 'error' | select -First 6); "FAIL build after merging $Agent"; return }
$log += "build OK ($([int]((Get-Item $dll).Length/1KB)) KB)"
Add-Content "$res\merge_log.md" "- $(Get-Date -f 'MM-dd HH:mm') merged ${Agent}: $($log -join '; ')" -Encoding utf8
if ($NoTest) { $log; "OK merged $Agent (no test)"; return }
# 4) regression: feature on vs off (live keys) + the previous merged state is the staging DLL itself; check characters (ty_strip) and errors
$inis = "$res\inis"; $feat = @{ atmos = 'atmos_froxel'; ao = 'ao_v2'; gi = 'gi_hiz1'; tess = 'merged_tess'; light = 'merged_light' }[$Agent]
if (-not (Test-Path "$inis\$feat.ini")) { [IO.File]::WriteAllText("$inis\$feat.ini", (Get-Content "$inis\ultra_baseline.ini" -Raw), $utf8) }
$suiteOut = & powershell -NoProfile -File C:\CLAUDE\TOOLS\suite.ps1 -Name "m_$Agent" -Dll $dll -AB "off=$inis\all_off.ini;feature=$inis\$feat.ini;ultra=$inis\ultra_baseline.ini" 2>&1 | Out-String
$log; $suiteOut
if ($suiteOut -match 'errors [1-9]' -or $suiteOut -match 'images 0/') { "FAIL regression suite for $Agent (see results\m_$Agent)" } else { "OK merged $Agent; review results\m_$Agent\sheet.png + ty_strip.png" }



# Zero-token status collector: gathers every agent's state from FILES (step logs, reports, run outputs, builds, screenshots, queue) into
#   C:\CLAUDE\RESEARCH\STATUS_RAW.md     (machine-collected facts; the Haiku agent / next session turns this into STATUS_<date>.md)
# Usage: powershell -NoProfile -File C:\CLAUDE\TOOLS\status.ps1 [-Sync]      (-Sync mirrors the result to OpenViking)
param([switch]$Sync)
$utf8 = New-Object Text.UTF8Encoding($false); $sb = New-Object Text.StringBuilder
function P([string]$s) { [void]$sb.AppendLine($s) }
$agents = 'atmos', 'gi', 'ao', 'tess', 'light'; $logs = 'C:\CLAUDE\RESEARCH\sessions\2026-10-12_session5\agents'
P "# STATUS_RAW (collected $(Get-Date -f 'yyyy-MM-dd HH:mm:ss'))"; P ''
P "## System"; P "- game running: $((Get-Process TY_tyrender -EA SilentlyContinue | % { 'pid ' + $_.Id + ' since ' + $_.StartTime.ToString('HH:mm:ss') }) -join ', ')"
P "- IN_TEST.flag: $(Test-Path C:\CLAUDE\TOOLS\_test_backup\IN_TEST.flag); focus_daemon procs: $((Get-CimInstance Win32_Process | ? { $_.CommandLine -match 'focus_daemon' } | % ProcessId) -join ',')"
$q = @(Get-CimInstance Win32_Process -Filter "Name='powershell.exe'" | ? { $_.CommandLine -match 'agent_test' } | sort CreationDate)
P "- queued/running harness runs: $($q.Count)"; foreach ($x in $q) { $c = ($x.CommandLine -replace '.*agent_test.ps1', '').Trim(); P "  - pid $($x.ProcessId) since $($x.CreationDate.ToString('HH:mm:ss')): $($c.Substring(0,[math]::Min(110,$c.Length)))" }
$ov = & { try { . C:\CLAUDE\TOOLS\ov.ps1; 'ok' } catch { 'unavailable' } }; P "- OpenViking: $ov"
$gl = 'C:\CLAUDE\TY the Tasmanian Tiger\TyOutputLog.txt'; if (Test-Path $gl) { P "- last game log lines: $((Get-Content $gl -Tail 3) -join ' / ')" }
P ''
foreach ($a in $agents) {
    P "## Agent: $a"; $src = "C:\CLAUDE\SRC\agents\$a"; $rd = "C:\CLAUDE\RESEARCH\agents\$a"; $sh = "C:\CLAUDE\RESEARCH\shots\agents\$a"
    $lf = "$logs\$a.md"; if (Test-Path $lf) { P "- step log: $lf (updated $((Get-Item $lf).LastWriteTime.ToString('MM-dd HH:mm')))"; P '```'; P ((Get-Content $lf -Raw)); P '```' } else { P '- step log: none' }
    $rp = "$src\REPORT.md"; if (Test-Path $rp) { P "- REPORT.md (updated $((Get-Item $rp).LastWriteTime.ToString('MM-dd HH:mm'))), first 80 lines:"; P '```'; P ((Get-Content $rp -TotalCount 80) -join "`n"); P '```' } else { P '- REPORT.md: none' }
    $src_files = Get-ChildItem $src -File -EA SilentlyContinue | ? { $_.Extension -in '.inl', '.cpp' } | sort LastWriteTime -desc | select -First 6 | % { "$($_.Name)@$($_.LastWriteTime.ToString('HH:mm')) ($([int]($_.Length/1024)) KB)" }; P "- source files (newest): $($src_files -join ', ')"
    $dlls = Get-ChildItem "$src\out", $rd -Filter *.dll -EA SilentlyContinue | sort LastWriteTime -desc | select -First 4 | % { "$($_.Name)@$($_.LastWriteTime.ToString('HH:mm'))" }; P "- built dlls: $($dlls -join ', ')"
    $perf = @(); foreach ($f in Get-ChildItem $rd, $sh -Recurse -File -Include *.txt, *.out, *.log -EA SilentlyContinue) { $perf += Select-String $f.FullName -Pattern '^PERF ' -EA SilentlyContinue | % { "$($f.Name): $($_.Line)" } }
    P "- PERF lines found ($($perf.Count)), last 8:"; $perf | select -Last 8 | % { P "  - $_" }
    $errs = @(); foreach ($f in Get-ChildItem $rd, $sh -Recurse -File -Include *.txt, *.out, *.log -EA SilentlyContinue) { $errs += Select-String $f.FullName -Pattern 'FAILED|LOG ERRORS|does not compile|!!!|Out of memory|FOCUS LOST' -EA SilentlyContinue | select -First 2 | % { "$($_.Filename): $($_.Line.Trim())" } }
    P "- error lines ($($errs.Count)), first 6:"; $errs | select -First 6 | % { P "  - $($_.Substring(0,[math]::Min(160,$_.Length)))" }
    $png = @(Get-ChildItem $sh -Recurse -Filter *.png -EA SilentlyContinue | sort LastWriteTime -desc); P "- screenshots: $($png.Count) (newest: $(($png | select -First 5 | % { $_.FullName.Replace('C:\CLAUDE\RESEARCH\shots\agents\','') }) -join ', '))"; P ''
}
P '## Other logs'; foreach ($n in 'lead', 'research_top10') { $f = "$logs\$n.md"; if (Test-Path $f) { P "- $n.md updated $((Get-Item $f).LastWriteTime.ToString('HH:mm'))" } }
$out = 'C:\CLAUDE\RESEARCH\STATUS_RAW.md'; [IO.File]::WriteAllText($out, $sb.ToString(), $utf8); "wrote $out ($([int]((Get-Item $out).Length/1024)) KB)"
if ($Sync) { try { . C:\CLAUDE\TOOLS\ov.ps1; ovSync $out 'viking://resources/TECHNICAL/ty-hd-renderer/research/docs/local-mirror/STATUS_RAW.md' | Out-Null; 'synced to OpenViking' } catch { "WARNING: OpenViking sync failed: $($_.Exception.Message)" } }

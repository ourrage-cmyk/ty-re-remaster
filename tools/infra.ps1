# Infrastructure helpers for the agent source copies (all zero-LLM). Usage: powershell -NoProfile -File infra.ps1 <command>
#   sync-infra   copy shared infrastructure files from the master (debugcam.inl, stateguard.inl) into every agent folder; patch gpuprof.inl of agents that still carry the
#                master's old version so the StateGuard hook is present (ao keeps its own gpuprof.inl: sub-timers; it gets stateguard.inl only).
#   build-all    build every agent folder (out\tyrender.dll) and print OK/FAIL + size; also copies each DLL to RESEARCH\results\dlls\<agent>.dll for the test queue
#   hashes       write RESEARCH\results\merge_prep.csv: per file, sha1 in master and in each agent folder + CHANGED flag (the three-way merge input)
param([Parameter(Mandatory, Position = 0)][string]$Cmd)
$master = 'C:\CLAUDE\SRC\tyrender'; $agents = 'atmos', 'gi', 'ao', 'tess', 'light'; $res = 'C:\CLAUDE\RESEARCH\results'; New-Item -ItemType Directory -Force "$res\dlls" | Out-Null
switch ($Cmd) {
 'sync-infra' {
    foreach ($a in $agents) { $d = "C:\CLAUDE\SRC\agents\$a"
        foreach ($f in 'debugcam.inl', 'stateguard.inl') { Copy-Item "$master\$f" "$d\$f" -Force }
        if ($a -ne 'ao') { $g = [IO.File]::ReadAllText("$d\gpuprof.inl"); if ($g -notmatch 'SGMark') { Copy-Item "$master\gpuprof.inl" "$d\gpuprof.inl" -Force; "$a : gpuprof.inl replaced with master (StateGuard hook)" } else { "$a : gpuprof.inl already has StateGuard" } }
        else { $g = [IO.File]::ReadAllText("$d\gpuprof.inl"); if ($g -notmatch 'stateguard') { $g = $g.Replace('static int g_gpEnable = 1;', "#include `"stateguard.inl`"`nstatic int g_gpEnable = 1;"); $g = $g.Replace("static void GProf(int m) {`n", "static void GProf(int m) {`n    SGMark(m);`n"); [IO.File]::WriteAllText("$d\gpuprof.inl", $g, (New-Object Text.UTF8Encoding($false))); 'ao : StateGuard hook patched into its own gpuprof.inl' } else { 'ao : already patched' } } } }
 'build-all' {
    foreach ($a in $agents) { $d = "C:\CLAUDE\SRC\agents\$a"; $out = & powershell -NoProfile -File "$d\build.ps1" 2>&1 | Out-String; $dll = "$d\out\tyrender.dll"
        if ($LASTEXITCODE -eq 0 -and (Test-Path $dll) -and ((Get-Item $dll).LastWriteTime -gt (Get-Date).AddMinutes(-5))) { Copy-Item $dll "$res\dlls\$a.dll" -Force; "{0,-6} OK   {1:N0} KB" -f $a, ((Get-Item $dll).Length / 1KB) } else { "{0,-6} FAIL" -f $a; ($out -split "`n" | Select-String 'error' | select -First 4) } } }
 'hashes' {
    $rows = @(); $names = @(Get-ChildItem $master -File | ? { $_.Extension -in '.cpp', '.inl', '.inc', '.c' } | % Name)
    foreach ($a in $agents) { $names += Get-ChildItem "C:\CLAUDE\SRC\agents\$a" -File | ? { $_.Extension -in '.cpp', '.inl' } | % Name }; $names = $names | Sort-Object -Unique
    foreach ($n in $names) { $row = [ordered]@{ file = $n; master = $(if (Test-Path "$master\$n") { (Get-FileHash "$master\$n" -Algorithm SHA1).Hash.Substring(0, 8) } else { '-' }) }; $chg = @()
        foreach ($a in $agents) { $p = "C:\CLAUDE\SRC\agents\$a\$n"; $h = if (Test-Path $p) { (Get-FileHash $p -Algorithm SHA1).Hash.Substring(0, 8) } else { '-' }; $row[$a] = $h; if ($h -ne '-' -and $h -ne $row.master) { $chg += $a } }
        $row['changed_by'] = ($chg -join '+'); $rows += [pscustomobject]$row }
    $rows | Export-Csv "$res\merge_prep.csv" -NoTypeInformation; $rows | ? { $_.changed_by } | ft file, master, changed_by -auto | Out-String }
 default { throw "unknown command $Cmd" }
}

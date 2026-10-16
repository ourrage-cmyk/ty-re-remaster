# Preset matrix on the merged DLL (zero LLM tokens): one fresh game session per preset (restart-only keys differ), default camera, GPU timers.
#   powershell -NoProfile -File perf_matrix.ps1 [-Dll C:\CLAUDE\SRC\merge\out\tyrender.dll]    -> results\preset_matrix.csv + results\preset_matrix.md (+ one suite dir per preset: pm_<name>)
param([string]$Dll = 'C:\CLAUDE\SRC\merge\out\tyrender.dll')
. C:\CLAUDE\TOOLS\presets.ps1
$res = 'C:\CLAUDE\RESEARCH\results'; $inis = "$res\inis"; $utf8 = New-Object Text.UTF8Encoding($false); $rows = @(); $stockIni = "$inis\all_off.ini"
function Run1($name, $ini) {
    $out = & powershell -NoProfile -File C:\CLAUDE\TOOLS\suite.ps1 -Name "pm_$name" -Dll $Dll -AB "$name=$ini" -Views 'default=' 2>&1 | Out-String
    $m = [regex]::Match($out, "$name => frame ([\d.]+) ms \| ao ([\d.]+) gi ([\d.]+) atmos ([\d.]+) post ([\d.]+)"); if ($m.Success) { [pscustomobject]@{ name = $name; frame = [double]$m.Groups[1].Value; ao = $m.Groups[2].Value; gi = $m.Groups[3].Value; atmos = $m.Groups[4].Value; post = $m.Groups[5].Value } } else { $out; throw "FAIL no perf sample for $name" }
}
$stock = Run1 'stock' $stockIni; $rows += $stock
foreach ($k in $Presets.Keys) { $ini = "$inis\preset_$k.ini"; [IO.File]::WriteAllText($ini, (NewIni $Presets[$k]), $utf8); $rows += Run1 $k $ini }
$rows | % { $_ | Add-Member delta ([math]::Round($_.frame - $stock.frame, 2)) -PassThru | Add-Member fps ([math]::Round(1000 / $_.frame, 1)) -PassThru } | Export-Csv "$res\preset_matrix.csv" -NoTypeInformation
$md = "| preset | frame ms | added ms | fps | ao pass | gi pass | atmos pass |`n|---|---|---|---|---|---|---|`n"; foreach ($r in $rows) { $md += "| $($r.name) | $($r.frame) | $($r.delta) | $($r.fps) | $($r.ao) | $($r.gi) | $($r.atmos) |`n" }
[IO.File]::WriteAllText("$res\preset_matrix.md", $md, $utf8); $md; 'OK preset matrix'

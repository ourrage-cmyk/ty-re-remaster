# Find the highest-quality Balanced candidate that meets the 60 fps budget at the default view (<= 16.7 ms total frame). Zero LLM tokens.
#   powershell -NoProfile -File balanced_search.ps1 [-Dll ...] [-Budget 16.7]  -> results\inis\preset_Balanced.ini + results\balanced_result.md
param([string]$Dll = 'C:\CLAUDE\SRC\merge\out\tyrender.dll', [double]$Budget = 16.7)
. C:\CLAUDE\TOOLS\presets.ps1
$res = 'C:\CLAUDE\RESEARCH\results'; $inis = "$res\inis"; $utf8 = New-Object Text.UTF8Encoding($false); $log = @(); $chosen = ''
foreach ($k in $BalancedCandidates.Keys) {
    $ini = "$inis\cand_$k.ini"; [IO.File]::WriteAllText($ini, (NewIni $BalancedCandidates[$k]), $utf8)
    $out = & powershell -NoProfile -File C:\CLAUDE\TOOLS\suite.ps1 -Name "bal_$k" -Dll $Dll -AB "$k=$ini" -Views 'default=' 2>&1 | Out-String
    $m = [regex]::Match($out, "$k => frame ([\d.]+) ms"); if (-not $m.Success) { $log += "$k : no sample"; continue }
    $f = [double]$m.Groups[1].Value; $log += "$k : $f ms ($([math]::Round(1000/$f,1)) fps)"
    if ($f -le $Budget) { $chosen = $k; Copy-Item $ini "$inis\preset_Balanced.ini" -Force; break }
}
$md = "# Balanced search (budget $Budget ms at default view)`n" + (($log | % { "- $_" }) -join "`n") + "`n`nchosen: $(if($chosen){$chosen}else{'NONE - no candidate fits; lower the candidates or raise the budget'})`n"
[IO.File]::WriteAllText("$res\balanced_result.md", $md, $utf8); $md
if ($chosen) { "OK balanced = $chosen" } else { 'FAIL no candidate meets the budget' }

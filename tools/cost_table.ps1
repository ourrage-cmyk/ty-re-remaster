# T40: merged-build cost of each option relative to Balanced (C1), default view, GPU frame ms. Zero LLM tokens.
#   powershell -NoProfile -File cost_table.ps1 [-Dll ...] -> results\cost_table.md
param([string]$Dll = 'C:\CLAUDE\SRC\merge\out\tyrender.dll')
. C:\CLAUDE\TOOLS\presets.ps1
$res = 'C:\CLAUDE\RESEARCH\results'; $inis = "$res\inis"; $utf8 = New-Object Text.UTF8Encoding($false)
$base = $BalancedCandidates['C1']
function V([hashtable]$o, $kv) { $c = $o.Clone(); foreach ($k in $kv.Keys) { $c[$k] = $kv[$k] }; $c }
$var = [ordered]@{
  C1 = @{}
  contact035 = @{ contact = 0.35 }
  map4_taps32 = @{ shMap = 4; shTaps = 32 }
  gi_hiz1 = @{ gi = 1 }
  scope_all = @{ tessScope = 'all'; tessLevel = 12 }
  ao_off = @{ ao = 0 }
  atmos_off = @{ atmos = 0 }
  tess_off = @{ tess = 0 }
  taa_off = @{ aa = 0 }
}
$rows = @()
foreach ($n in $var.Keys) {
    $ini = "$inis\ct_$n.ini"; [IO.File]::WriteAllText($ini, (NewIni (V $base $var[$n])), $utf8)
    $out = & powershell -NoProfile -File C:\CLAUDE\TOOLS\suite.ps1 -Name "ct_$n" -Dll $Dll -AB "$n=$ini" -Views 'default=' 2>&1 | Out-String
    $m = [regex]::Match($out, "$n => frame ([\d.]+) ms"); $rows += [pscustomobject]@{ v = $n; ms = $(if ($m.Success) { [double]$m.Groups[1].Value } else { -1 }) }
}
$b = ($rows | ? { $_.v -eq 'C1' }).ms
$md = "# Cost table: Balanced (C1) +/- option, default view, merged build`n| variant | frame ms | vs C1 |`n|---|---|---|`n" + (($rows | % { "| $($_.v) | $($_.ms) | $(if($_.ms -gt 0 -and $b -gt 0){'{0:+0.00;-0.00}' -f ($_.ms-$b)}else{'n/a'}) |" }) -join "`n") + "`n"
[IO.File]::WriteAllText("$res\cost_table.md", $md, $utf8); $md
if ($b -gt 0) { 'OK cost table' } else { 'FAIL no baseline' }

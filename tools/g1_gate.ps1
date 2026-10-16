# T45 G1 release gate on the SHIPPED dll: soak 10 min (Balanced), forced fallback (ForceFallback=31), perf at default + vista + heavy views. Zero LLM tokens.
param([string]$Dll = 'C:\CLAUDE\SRC\tyrender\out\tyrender.dll')
$res = 'C:\CLAUDE\RESEARCH\results'; $inis = "$res\inis"; $sha = (Get-FileHash $Dll -Algorithm SHA1).Hash.Substring(0, 10); $log = @("dll $Dll sha1 $sha"); $fail = $false
$s = & powershell -NoProfile -File C:\CLAUDE\TOOLS\soak.ps1 -Minutes 10 -Dll $Dll 2>&1 | Out-String; $ok = $s -match 'OK soak'; $log += "soak 10 min: $(if($ok){'OK'}else{'FAIL'})"; if (-not $ok) { $fail = $true }
$f = & powershell -NoProfile -File C:\CLAUDE\TOOLS\suite.ps1 -Name g1_forced -Dll $Dll -AB "forced=$inis\bal_forced31.ini" -Views 'default=' 2>&1 | Out-String
$m = [regex]::Match($f, 'forced => frame ([\d.]+) ms'); $fm = if ($m.Success) { [double]$m.Groups[1].Value } else { -1 }
$cams = [ordered]@{ default = ''; vista = '0 -60 300 -50 8'; sand_graze = '-100 -60 700 -15 12'; fence_mid = '-250 -40 350 -35 12' }; $bal = 0
foreach ($k in $cams.Keys) {
    $env:TY_PERFCAM = $cams[$k]
    $o = & powershell -NoProfile -File C:\CLAUDE\TOOLS\suite.ps1 -Name "g1_$k" -Dll $Dll -AB "bal=$inis\preset_Balanced.ini" -Views 'default=' 2>&1 | Out-String
    $mm = [regex]::Match($o, 'bal => frame ([\d.]+) ms'); $v = if ($mm.Success) { [double]$mm.Groups[1].Value } else { -1 }; $log += "perf $k : $v ms"; if ($k -eq 'default') { $bal = $v }; if ($v -gt 16.7 -or $v -lt 0) { $log += "  ^ over budget / no sample" }
}
$env:TY_PERFCAM = ''
$log += "forced fallback: $fm ms (balanced default $bal)"; if ($fm -lt 0 -or $fm -gt $bal) { $fail = $true }
$md = "# G1 gate`n" + (($log | % { "- $_" }) -join "`n") + "`n"; [IO.File]::WriteAllText("$res\g1_gate.md", $md, (New-Object Text.UTF8Encoding $false)); $md
if ($fail) { 'FAIL G1' } else { 'OK G1 (see over-budget notes for views)' }

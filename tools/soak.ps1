# Soak test of a DLL with a preset: plays N minutes with random movement and watches for crashes / out-of-memory / crash-logger markers. Zero LLM tokens.
#   powershell -NoProfile -File soak.ps1 [-Minutes 10] [-Dll C:\CLAUDE\SRC\merge\out\tyrender.dll] [-Ini C:\CLAUDE\RESEARCH\results\inis\preset_Balanced.ini]
param([int]$Minutes = 10, [string]$Dll = 'C:\CLAUDE\SRC\merge\out\tyrender.dll', [string]$Ini = 'C:\CLAUDE\RESEARCH\results\inis\preset_Balanced.ini')
if (-not (Test-Path $Ini)) { $Ini = 'C:\CLAUDE\RESEARCH\results\inis\preset_High.ini' }
$out = 'C:\CLAUDE\RESEARCH\results\soak'; New-Item -ItemType Directory -Force $out | Out-Null
$o = & powershell -NoProfile -File C:\CLAUDE\TOOLS\agent_test.ps1 -Dll $Dll -IniFile $Ini -Soak $Minutes -OutDir $out 2>&1 | Out-String
$o | Set-Content "$out\soak_output.txt" -Encoding utf8; $o
if ($o -match 'SOAK OK') { "OK soak $Minutes min with $(Split-Path $Ini -Leaf)" } else { 'FAIL soak (see results\soak\soak_output.txt)' }

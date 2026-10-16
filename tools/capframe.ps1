# Ask the running tyrender-patched game to log ONE full frame call-by-call, then extract it from tyrender.log.
# Usage: powershell -NoProfile -File capframe.ps1 -Out <file.txt> [-Game <dir>]
param([Parameter(Mandatory)][string]$Out, [string]$Game = 'C:\CLAUDE\TY the Tasmanian Tiger')
$log = "$Game\tyrender.log"
$before = (Get-Content $log | Select-String '=== END CAPTURE').Count
New-Item -ItemType File -Force "$Game\tyrender\cap.req" | Out-Null
for ($i = 0; $i -lt 100; $i++) { Start-Sleep -Milliseconds 100; if ((Get-Content $log | Select-String '=== END CAPTURE').Count -gt $before) { break } }
$lines = Get-Content $log
$end = ($lines | Select-String '=== END CAPTURE' | Select-Object -Last 1).LineNumber
$begin = ($lines[0..($end - 1)] | Select-String '=== BEGIN CAPTURE' | Select-Object -Last 1).LineNumber
if (-not $end -or -not $begin) { throw 'capture not found in tyrender.log' }
$lines[($begin - 1)..($end - 1)] | Set-Content $Out -Encoding utf8
"captured $($end - $begin + 1) lines -> $Out"

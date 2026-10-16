# Ask the running (tyrender-patched) game for an in-process snapshot of its own back buffer and convert it to PNG.
# Privacy-safe: pixels come from glReadPixels inside the game, never from the desktop.
# Usage: powershell -NoProfile -File C:\CLAUDE\TOOLS\snap.ps1 -Name t3_menu [-OutDir <dir>]   -> prints the PNG path
param([Parameter(Mandatory)][string]$Name, [string]$OutDir = 'C:\CLAUDE\RESEARCH\shots', [string]$Game = 'C:\CLAUDE\TY the Tasmanian Tiger', [int]$TimeoutSec = 10)
Add-Type -AssemblyName System.Drawing
$req = "$Game\tyrender\snap.req"; $bmp = "$Game\tyrender\snaps\$Name.bmp"
Remove-Item $bmp -ErrorAction SilentlyContinue
Set-Content $req $Name -Encoding ascii
$t = 0; while ((Test-Path $req) -and $t -lt $TimeoutSec * 10) { Start-Sleep -Milliseconds 100; $t++ }
if (-not (Test-Path $bmp)) { throw "no snapshot (game not running/not patched, or window minimised?)" }
New-Item -ItemType Directory -Force $OutDir | Out-Null
$png = Join-Path $OutDir "$Name.png"
$img = [Drawing.Image]::FromFile($bmp); $img.Save($png, [Drawing.Imaging.ImageFormat]::Png); $img.Dispose()
Remove-Item $bmp
$png

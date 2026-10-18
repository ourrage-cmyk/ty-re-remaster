# Build and package a shareable zip: SRC\tyrender\dist\TYHD_Renderer_<ver>.zip (setup, uninstaller, dll, launcher, default ini, README). Never includes game files.
param([string]$Version = 'v0.1')
$ErrorActionPreference = 'Stop'
$src = $PSScriptRoot
& powershell -NoProfile -File (Join-Path $src 'build.ps1') | Out-Null
$stage = Join-Path $src "dist\TYHD_Renderer_$Version"; Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue; New-Item -ItemType Directory -Force $stage | Out-Null
Copy-Item "$src\out\tyrender.dll", "$src\out\TYLauncher.exe", "$src\README.md", "$src\PERFORMANCE.md" $stage
Copy-Item "$src\out\TY HD Renderer Setup.exe", "$src\out\TY HD Renderer Uninstall.exe" $stage   # one-file installer + its uninstaller
Copy-Item "$src\default_tyrender.ini" "$stage\tyrender.ini"   # = Balanced preset (see presets.ps1 C1)
$zip = "$stage.zip"; Remove-Item $zip -ErrorAction SilentlyContinue
Compress-Archive -Path "$stage\*" -DestinationPath $zip
Get-ChildItem $stage | Select-Object Name, Length; "zip: $zip"

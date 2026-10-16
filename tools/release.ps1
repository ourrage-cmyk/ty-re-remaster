# Release: promote the merged staging tree to the master, build + install, package v0.3, verify the zip and the Large-Address-Aware patch. Zero LLM tokens.
#   powershell -NoProfile -File release.ps1 [-Version v0.3]
# Safe: the master is backed up first (SRC\backup_master_<timestamp>); nothing is deleted. Fails (text FAIL) if the build, the zip contents or the LAA check is wrong.
param([string]$Version = 'v0.3')
$master = 'C:\CLAUDE\SRC\tyrender'; $stage = 'C:\CLAUDE\SRC\merge'; $ts = Get-Date -f 'yyyyMMdd_HHmm'; $bk = "C:\CLAUDE\SRC\backup_master_$ts"
if (Get-Process TY_tyrender -ErrorAction SilentlyContinue) { 'FAIL game is running; stop it first'; return }
if (-not (Test-Path "$stage\out\tyrender.dll")) { 'FAIL no merged build in staging'; return }
New-Item -ItemType Directory $bk | Out-Null; Get-ChildItem $master -File | Copy-Item -Destination $bk -Force; "master backed up to $bk"
foreach ($f in Get-ChildItem $stage -File | ? { $_.Extension -in '.cpp', '.inl', '.inc' -and $_.Name -ne 'launcher_ui.inc' }) { Copy-Item $f.FullName "$master\$($f.Name)" -Force }; 'staging sources promoted to master'
$b = & powershell -NoProfile -File "$master\build.ps1" -Install 2>&1 | Out-String; if ($b -notmatch 'installed to') { $b; 'FAIL master build/install'; return }; 'master built and installed to the game folder'
$z = & powershell -NoProfile -File "$master\package.ps1" -Version $Version 2>&1 | Out-String; $zip = "$master\dist\TYHD_Renderer_$Version.zip"; if (-not (Test-Path $zip)) { $z; 'FAIL package'; return }
Add-Type -AssemblyName System.IO.Compression.FileSystem; $names = [IO.Compression.ZipFile]::OpenRead($zip) | % { $_.Entries | % FullName }
$need = 'tyrender.dll', 'TYLauncher.exe', 'tyrender.ini', 'README.md', 'PERFORMANCE.md'; $missing = @($need | ? { $_ -notin $names }); $bad = @($names | ? { $_ -match '\.rkv$|^TY\.exe$|\.iso$' })
if ($missing -or $bad) { "FAIL zip content: missing=$($missing -join ',') forbidden=$($bad -join ',')"; return }; "zip OK: $($names -join ', ')"
$t = "$env:TEMP\ty_laa_check.exe"; & powershell -NoProfile -File "$master\patch_imports.ps1" -In 'C:\CLAUDE\TY the Tasmanian Tiger\TY.exe' -Out $t | Out-Null
$by = [IO.File]::ReadAllBytes($t); $ch = [BitConverter]::ToUInt16($by, [BitConverter]::ToInt32($by, 0x3C) + 22); Remove-Item $t -Force
if (-not ($ch -band 0x20)) { 'FAIL patcher output is not Large Address Aware'; return }; 'LAA patch check OK'
"OK release $Version : $zip"


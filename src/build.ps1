# Build tyrender.dll + TYLauncher.exe (32-bit, llvm-mingw i686, static runtime). Output: SRC\tyrender\out\
# Usage: powershell -NoProfile -File C:\CLAUDE\SRC\tyrender\build.ps1 [-Install]   (-Install copies both into the working game folder)
param([switch]$Install, [string]$Version = 'v0.3.5-rc')
$ErrorActionPreference = 'Stop'
$bin = 'C:\CLAUDE\TOOLS\llvm-mingw\llvm-mingw-20261006-ucrt-x86_64\bin'
$src = $PSScriptRoot; $out = Join-Path $src 'out'
New-Item -ItemType Directory -Force $out | Out-Null
# Version stamp for the launcher title bar: <Version> plus the short git sha of this source tree when available.
$sha = (& git -C $src rev-parse --short HEAD 2>$null); $stamp = if ($sha) { "$Version ($sha)" } else { $Version }
Set-Content -Path "$out\ty_version.h" -Value "#define TY_VERSION L`"$stamp`"" -Encoding ASCII
& "$bin\i686-w64-mingw32-clang++.exe" -O2 -shared -static -std=c++17 -Wall -o "$out\tyrender.dll" "$src\tyrender.cpp" '-Wl,--kill-at'
if ($LASTEXITCODE) { throw 'tyrender.dll build failed' }
# build id = short git sha (+ -dirty when the tree has local edits), shown in the launcher title bar
$sha = (git -C $src rev-parse --short HEAD 2>$null); if (-not $sha) { $sha = 'nogit' }
if (git -C $src status --porcelain 2>$null) { $sha += '-dirty' }
Set-Content -Path (Join-Path $out 'build_id.h') -Value ('#define TY_BUILD_ID L"' + $sha + '"') -Encoding ascii
& "$bin\i686-w64-mingw32-clang.exe" -O2 -static -municode -mwindows -Wall "-I$out" -o "$out\TYLauncher.exe" "$src\TYLauncher.c" -lshell32 -lcomctl32
if ($LASTEXITCODE) { throw 'TYLauncher.exe build failed' }
Get-ChildItem $out | Select-Object Name, Length
if ($Install) {
    $g = 'C:\CLAUDE\TY the Tasmanian Tiger'
    Copy-Item "$out\tyrender.dll", "$out\TYLauncher.exe" $g -Force
    "installed to $g (remove tyrender.dll + TYLauncher.exe + tyrender\ + tyrender.log/ini to return to stock)"
}

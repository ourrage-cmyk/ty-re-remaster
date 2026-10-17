# Build tyrender.dll + TYLauncher.exe (32-bit, llvm-mingw i686, static runtime). Output: SRC\tyrender\out\
# Usage: powershell -NoProfile -File C:\CLAUDE\SRC\tyrender\build.ps1 [-Install]   (-Install copies both into the working game folder)
param([switch]$Install)
$ErrorActionPreference = 'Stop'
$bin = 'C:\CLAUDE\TOOLS\llvm-mingw\llvm-mingw-20261006-ucrt-x86_64\bin'
$src = $PSScriptRoot; $out = Join-Path $src 'out'
New-Item -ItemType Directory -Force $out | Out-Null
# build id = short git sha (+ -dirty when the tree has local edits), shown in the launcher title bar and in the A/B screenshot names
$sha = (git -C $src rev-parse --short HEAD 2>$null); if (-not $sha) { $sha = 'nogit' }
if (git -C $src status --porcelain 2>$null) { $sha += '-dirty' }
# build_id.h: the wide id for the launcher title and the narrow id for tyrender.dll (A/B screenshot names, abshot.inl)
Set-Content -Path (Join-Path $out 'build_id.h') -Value @(('#define TY_BUILD_ID L"' + $sha + '"'), ('#define TY_BUILD_ID_A "' + $sha + '"')) -Encoding ascii
& "$bin\i686-w64-mingw32-clang++.exe" -O2 -shared -static -std=c++17 -Wall "-I$out" -o "$out\tyrender.dll" "$src\tyrender.cpp" '-Wl,--kill-at' -lshell32 -lgdi32
if ($LASTEXITCODE) { throw 'tyrender.dll build failed' }
# Embed the matching tyrender.dll and default ini in the launcher (resources 1 and 2): TYLauncher.exe alone is the release.
$rc = Join-Path $out 'payload.rc'
Set-Content -Path $rc -Value @(('1 RCDATA "' + ("$out\tyrender.dll" -replace '\\', '/') + '"'), ('2 RCDATA "' + ("$src\default_tyrender.ini" -replace '\\', '/') + '"')) -Encoding ascii
& "$bin\i686-w64-mingw32-windres.exe" -O coff -o "$out\payload.res.o" $rc
if ($LASTEXITCODE) { throw 'payload resource build failed' }
& "$bin\i686-w64-mingw32-clang.exe" -O2 -static -municode -mwindows -Wall "-I$out" -o "$out\TYLauncher.exe" "$src\TYLauncher.c" "$out\payload.res.o" -lshell32 -lcomctl32
if ($LASTEXITCODE) { throw 'TYLauncher.exe build failed' }
Get-ChildItem $out | Select-Object Name, Length
if ($Install) {
    $g = 'C:\CLAUDE\TY the Tasmanian Tiger'
    Copy-Item "$out\tyrender.dll", "$out\TYLauncher.exe" $g -Force
    "installed to $g (remove tyrender.dll + TYLauncher.exe + tyrender\ + tyrender.log/ini to return to stock)"
}

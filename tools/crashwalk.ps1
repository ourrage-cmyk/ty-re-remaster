# Repro harness for the 2026-10-11 walk crash: boot to the save spot with the given feature switches, walk W,A,W,D,S,W,A,D, report survival.
# Usage: powershell -NoProfile -File crashwalk.ps1 -AO 1 -Atmos 1 -Post 1 -Shadows 1 -Tess 1   (ini is restored afterwards)
param([int]$AO = 1, [int]$Atmos = 1, [int]$Post = 1, [int]$Shadows = 1, [int]$Tess = 1, [string]$Game = 'C:\CLAUDE\TY the Tasmanian Tiger')
$ini = "$Game\tyrender.ini"; Copy-Item $ini "$ini.walkbak" -Force
try {
    $sec = ''; $out = foreach ($l in Get-Content $ini) {
        if ($l -match '^\[(\w+)\]') { $sec = $Matches[1]; $l; continue }
        if ($l -match '^Enable=') { switch ($sec) { 'ao' { "Enable=$AO" } 'atmos' { "Enable=$Atmos" } 'post' { "Enable=$Post" } 'tess' { "Enable=$Tess" } default { $l } }; continue }
        if ($sec -eq 'shadows' -and $l -match '^Patch=') { "Patch=$Shadows"; continue }
        $l
    }
    Set-Content $ini $out -Encoding ascii
    & powershell -NoProfile -File (Join-Path $PSScriptRoot 'ty_play.ps1') -Settle 10 | Out-Null
    $died = ''
    foreach ($k in 'W', 'A', 'W', 'D', 'S', 'W', 'A', 'D') {
        & powershell -NoProfile -File (Join-Path $PSScriptRoot 'key.ps1') -Keys $k -HoldMs 2500 -DelayMs 200 | Out-Null
        if (-not (Get-Process TY_tyrender -ErrorAction SilentlyContinue)) { $died = $k; break }
    }
    $ex = Select-String -Path "$Game\tyrender.log" -Pattern '!!!' | Select-Object -First 1
    "AO=$AO Atmos=$Atmos Post=$Post Shadows=$Shadows Tess=$Tess -> " + $(if ($died) { "CRASH after $died : $($ex.Line)" } else { 'survived' })
} finally {
    Get-Process TY_tyrender -ErrorAction SilentlyContinue | Stop-Process -Force
    Copy-Item "$ini.walkbak" $ini -Force; Remove-Item "$ini.walkbak"
}

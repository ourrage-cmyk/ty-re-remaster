# Automated playtest for the TY renderer: boots each variant to a fixed spot, takes A/B screenshots, measures flicker,
# checks the F10 overlay open/close, collects tyrender.log, and writes report.md and contact_sheet.png.
# Read-and-screenshot only. It backs up tyrender.ini and restores it, never writes into the install, and leaves the game closed.
# Usage: powershell -NoProfile -File tools\playtest\autoplaytest.ps1 [-Game 'C:\CLAUDE\TY the Tasmanian Tiger'] [-Variants stock,mod] [-Spot rainbow] [-Out <dir>]
# See docs/AUTOPLAYTEST.md for what it covers and what it does not.
param(
    [string]$Game = 'C:\CLAUDE\TY the Tasmanian Tiger',
    [string[]]$Variants = @('stock', 'mod', 'overlay', 'fog_seethru1', 'emit1'),
    [ValidateSet('rainbow', 'slot2')][string]$Spot = 'rainbow',
    [int]$FlickerFrames = 10,
    [double]$FlickerRatioMax = 1.5,
    [double]$OverlayClosedMax = 3.0,
    [string]$Out = '',
    [switch]$Copy              # test a copy of the game folder instead of the live one (for a run that must not touch the live folder)
)
. "$PSScriptRoot\common.ps1"
# Phase gate and game lock: nothing launches unless the phase is TEST, and this run holds the one game slot until it ends.
Assert-TestPhase
$lockToken = Enter-GameLock 'autoplaytest'

if (-not $Out) { $Out = Join-Path 'C:\CLAUDE\RESEARCH\autoplaytest' (Get-Date -Format 'yyyyMMdd_HHmmss') }
New-Item -ItemType Directory -Force $Out | Out-Null
if ($Copy) {
    $copy = Join-Path $Out 'game'
    & robocopy $Game $copy /E /XD Screenshots /NFL /NDL /NJH /NJS /NP | Out-Null
    if ($LASTEXITCODE -ge 8) { throw "robocopy of the game folder failed (exit $LASTEXITCODE)" }
    $Game = $copy
}
$ini = Join-Path $Game 'tyrender.ini'
$iniBackup = Join-Path $Out 'tyrender.ini.orig'
$hadIni = Test-Path $ini
if ($hadIni) { Copy-Item $ini $iniBackup -Force }

# Each variant: the exe to start, the launch args, ini keys to set on top of the installed ini, and which checks to run.
$defs = @{
    stock        = @{ Exe = 'TY.exe';          Args = '-skipvideo'; Ini = @{};                              Flicker = $true;  Overlay = $false }
    mod          = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{};                              Flicker = $true;  Overlay = $false }
    overlay      = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'overlay.Key' = 'F10' };        Flicker = $false; Overlay = $true }
    fog_seethru1 = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'fog.SeeThruDepth' = '1' };       Flicker = $false; Overlay = $false }
    emit1        = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'emit.Enable' = '1' };            Flicker = $false; Overlay = $false }
    # Requested A/B variants, opt-in with -Variants. The water, trees and portal spots are not wired yet (see docs), so these run at the Rainbow Cliffs spot.
    dbg_water_0  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'pbr.DebugWater' = '0' };         Flicker = $true;  Overlay = $false }
    dbg_water_1  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'pbr.DebugWater' = '1' };         Flicker = $true;  Overlay = $false }
    dbg_water_2  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'pbr.DebugWater' = '2' };         Flicker = $true;  Overlay = $false }
    dbg_water_4  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'pbr.DebugWater' = '4' };         Flicker = $true;  Overlay = $false }
    dbg_water_8  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'pbr.DebugWater' = '8' };         Flicker = $true;  Overlay = $false }
    dbg_water_5  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'pbr.DebugWater' = '5' };         Flicker = $true;  Overlay = $false }
    skyfog_012   = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'atmos.SkyFogDist' = '0.12' };     Flicker = $false; Overlay = $false }   # needs #58 on main
    skyfog_045   = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'atmos.SkyFogDist' = '0.45' };     Flicker = $false; Overlay = $false }   # needs #58 on main
    trees_see0   = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'fog.SeeThruDepth' = '0' };        Flicker = $false; Overlay = $false }
    trees_see1   = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'fog.SeeThruDepth' = '1' };        Flicker = $false; Overlay = $false }
    portal_comp0 = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'emit.Enable' = '1'; 'emit.Compensate' = '0' }; Flicker = $false; Overlay = $false }
    portal_comp1 = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'emit.Enable' = '1'; 'emit.Compensate' = '1' }; Flicker = $false; Overlay = $false }
}

function Stop-Game { Get-GameProcs $Game | Stop-Process -Force -ErrorAction SilentlyContinue; Start-Sleep -Milliseconds 800 }

function Save-GameLog($r) {
    $src = Join-Path $Game 'tyrender.log'
    if (-not (Test-Path $src)) { $r.LogHits = 'no tyrender.log'; return }
    Copy-Item $src (Join-Path $r.Dir 'tyrender.log') -Force
    $hits = @(Select-String -Path $src -Pattern '(?i)\berror\b|\bfailed\b|assert' | Select-Object -First 5)
    $r.LogHits = "$($hits.Count) (first: " + (($hits | ForEach-Object { $_.Line.Trim() }) -join ' | ') + ')'
    if ($hits.Count -eq 0) { $r.LogHits = '0' }
}

function Run-Variant([string]$Name, [hashtable]$Def) {
    $dir = Join-Path $Out $Name; New-Item -ItemType Directory -Force $dir | Out-Null
    $r = @{ Name = $Name; Dir = $dir; Boot = ''; Alive = 'n/a'; Base = @(); ShotCount = 0; Flicker = 'n/a'; FlickerLower = -1; Overlay = 'n/a'; OverlayPass = $null; OverlayRows = @(); LogHits = 'n/a' }
    if (Get-GameProcs $Game) { $r.Boot = 'FAIL a game process is already running from this folder (slot busy; announce in the Fix plan thread)'; return $r }
    $proc = [IO.Path]::GetFileNameWithoutExtension($Def.Exe)

    # Every variant starts from the backed-up ini, so overrides never stack.
    if ($hadIni) { Copy-Item $iniBackup $ini -Force }
    foreach ($k in $Def.Ini.Keys) { $sec, $key = $k.Split('.', 2); Set-IniKey $ini $sec $key $Def.Ini[$k] }

    $boot = & powershell -NoProfile -File (Join-Path $PSScriptRoot 'boot_spot.ps1') -Game $Game -Exe $Def.Exe -Extra $Def.Args -Spot $Spot 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { $r.Boot = "FAIL $($boot.Trim())"; Stop-Game; Save-GameLog $r; return $r }
    $r.Boot = 'OK'

    # Two A/B pairs at the start spot (the first is the baseline for the overlay check).
    $r.Base = @(Take-Pair $Game $proc $dir 'base01') + @(Take-Pair $Game $proc $dir 'base02')
    $r.ShotCount = $r.Base.Count

    if ($Def.Flicker) {
        # Static camera, N presses, first frame of each pair. Std-dev over time is the flicker measure; the stock variant is the baseline for animated grass and clouds.
        $frames = @()
        for ($i = 1; $i -le $FlickerFrames; $i++) { $p = @(Take-Pair $Game $proc $dir ('flk{0:D2}' -f $i)); if ($p.Count -ge 1) { $frames += $p[0] } }
        if ($frames.Count -ge 3) {
            $s = [PlaytestImg]::TemporalStd([string[]]$frames, $TyX0, $TyY0, $TyX1, $TyY1)
            $r.FlickerLower = $s[2]
            $r.Flicker = 'whole {0:N2} / upper {1:N2} / lower {2:N2} (n={3})' -f $s[0], $s[1], $s[2], $frames.Count
        } else { $r.Flicker = "FAIL only $($frames.Count) frames" }
    }

    if ($Def.Overlay) {
        # F10 open, shot, F10 close, shot. The closed frame must match the baseline, or the overlay broke the geometry.
        if ($r.Base.Count -lt 1) { $r.Overlay = 'FAIL no baseline frame'; $r.OverlayPass = $false }
        else {
            Send-GameKey $Game $proc 'F10' 2000
            $open = @(Take-Pair $Game $proc $dir 'ov_open')
            Send-GameKey $Game $proc 'F10' 2000
            $closed = @(Take-Pair $Game $proc $dir 'ov_closed')
            $alive = [bool](Get-GameProcs $Game -Names @($proc))
            if ($open.Count -lt 1 -or $closed.Count -lt 1) { $r.Overlay = 'FAIL no frame captured'; $r.OverlayPass = $false }
            else {
                $dOpen = [PlaytestImg]::MeanAbsDiff($r.Base[0], $open[0], $TyX0, $TyY0, $TyX1, $TyY1)
                $dClosed = [PlaytestImg]::MeanAbsDiff($r.Base[0], $closed[0], $TyX0, $TyY0, $TyX1, $TyY1)
                $r.OverlayPass = ($alive -and $dClosed -ge 0 -and $dClosed -le $OverlayClosedMax)
                $r.Overlay = 'open diff {0:N2}, closed diff {1:N2}, alive {2}' -f $dOpen, $dClosed, $alive
                if ($dOpen -lt 1) { $r.Overlay += ' (CHECK: overlay did not visibly open)' }
                $r.OverlayRows = @(@{ Label = 'overlay open / closed'; Files = @($open[0], $closed[0]) })
            }
        }
    }

    $r.Alive = if (Get-GameProcs $Game -Names @($proc)) { 'yes' } else { 'NO (exited before the end)' }
    Stop-Game
    Save-GameLog $r
    return $r
}

$results = @()
try {
    foreach ($name in $Variants) {
        if (-not $defs.ContainsKey($name)) { throw "unknown variant $name" }
        $results += Run-Variant $name $defs[$name]
    }
} finally {
    Stop-Game
    if ($hadIni) { Copy-Item $iniBackup $ini -Force }
    Exit-GameLock $lockToken
}

# Report
$stock = $results | Where-Object { $_.Name -eq 'stock' -and $_.FlickerLower -ge 0 } | Select-Object -First 1
$iniOk = (-not $hadIni) -or ((Get-FileHash $ini -Algorithm SHA1).Hash -eq (Get-FileHash $iniBackup -Algorithm SHA1).Hash)
$md = @(
    '# Autoplaytest', '', "Game folder: $Game $(if ($Copy) { '(copy of the live folder)' } else { '(LIVE folder: the newest build)' })", "Spot: $Spot", "Run: $(Get-Date -Format s)", "tyrender.ini restored: $(if ($iniOk) { 'yes' } else { 'NO, check it' })", '',
    '| variant | boot | game alive | shots | flicker (temporal std of luma) | overlay | tyrender.log error/failed/assert |',
    '|---|---|---|---|---|---|---|'
)
foreach ($r in $results) { $md += "| $($r.Name) | $($r.Boot) | $($r.Alive) | $($r.ShotCount) | $($r.Flicker) | $($r.Overlay) | $($r.LogHits) |" }
$md += ''
$md += '## Checklist'
$md += ('- [{0}] boot to gameplay for every variant' -f $(if (@($results | Where-Object { $_.Boot -ne 'OK' }).Count -eq 0) { 'PASS' } else { 'FAIL' }))
$md += ('- [{0}] screenshots captured' -f $(if (@($results | Where-Object { $_.Boot -eq 'OK' -and $_.ShotCount -lt 2 }).Count -eq 0) { 'PASS' } else { 'FAIL' }))
foreach ($r in @($results | Where-Object { $_.FlickerLower -ge 0 -and $_.Name -ne 'stock' })) {
    if (-not $stock) { $md += "- [CHECK] flicker $($r.Name): no stock baseline in this run"; continue }
    $ratio = $r.FlickerLower / [Math]::Max(0.0001, $stock.FlickerLower)
    $st = if ($ratio -le $FlickerRatioMax) { 'PASS' } else { 'CHECK' }
    $md += ('- [{0}] flicker {1} lower-half vs stock: ratio {2:N2} (limit {3}, heuristic; judge the frames too)' -f $st, $r.Name, $ratio, $FlickerRatioMax)
}
foreach ($r in @($results | Where-Object { $null -ne $_.OverlayPass })) { $md += ('- [{0}] overlay open/close keeps geometry ({1})' -f $(if ($r.OverlayPass) { 'PASS' } else { 'FAIL' }), $r.Overlay) }
$md += ('- [{0}] restored tyrender.ini matches the backup' -f $(if ($iniOk) { 'PASS' } else { 'FAIL' }))
$md += ''
$md += '## Not covered by this script'
$md += '- Visual judgement (fog colour, shadow quality, portal light, water look): only frames and numbers are produced. A person or a session has to look at contact_sheet.png.'
$md += '- Flicker is a luma heuristic over a static camera. Animated grass and clouds count as flicker too, which is why the stock ratio is the check.'
$md += '- Spots other than Rainbow Cliffs (Billabong water, level two trees, red rock, portal) are not wired: they need save files or scripted walking.'
$md += '- Per-frame GPU and frame times are not parsed from tyrender.log here. The log copies are in each variant folder.'
$md += '- The game window must be foreground. key.ps1 fails visibly when it is not.'
$md -join "`n" | Set-Content (Join-Path $Out 'report.md') -Encoding utf8

$rows = @($results | Where-Object { $_.Base.Count -ge 1 } | ForEach-Object { @{ Label = $_.Name; Files = @($_.Base[0], $_.Base[1]) } })
$rows += @($results | Where-Object { $_.OverlayRows.Count -gt 0 } | ForEach-Object { $_.OverlayRows[0] })
if ($rows.Count -gt 0) { New-ContactSheet -Rows $rows -OutFile (Join-Path $Out 'contact_sheet.png') }

$md -join "`n"
"Output: $Out"

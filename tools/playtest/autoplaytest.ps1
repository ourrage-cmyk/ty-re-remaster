# Automated playtest for the TY renderer: boots each variant to a fixed spot, takes A/B screenshots, measures flicker,
# checks the F10 overlay open/close, collects tyrender.log, and writes report.md and contact_sheet.png.
# Read-and-screenshot only. It backs up tyrender.ini and restores it, never writes into the install, and leaves the game closed.
# Usage: powershell -NoProfile -File tools\playtest\autoplaytest.ps1 [-Game 'C:\CLAUDE\TY the Tasmanian Tiger'] [-Variants stock,mod] [-Spot rainbow] [-Out <dir>]
# See docs/AUTOPLAYTEST.md for what it covers and what it does not.
param(
    [string]$Game = 'C:\CLAUDE\TY the Tasmanian Tiger',
    [string[]]$Variants = @('stock', 'mod', 'overlay', 'fog_seethru1', 'emit1'),
    [ValidateSet('rainbow', 'billabong', 'slot2')][string]$Spot = 'rainbow',
    [string]$Plan = '',        # REQUIRED: a plan file (tools\playtest\plans\<name>.json). Every variant must be used by a check, or the run refuses to start.
    [string]$Cam = '',         # optional view offset "dx dy dz yawDeg pitchDeg" written to tyrender\cam.txt (debugcam.inl), relative to the spot's start camera. Empty = none.
    [int]$FlickerFrames = 10,
    [double]$FlickerRatioMax = 1.5,
    [double]$OverlayClosedMax = 3.0,
    [string]$Out = '',
    [switch]$Copy              # test a copy of the game folder instead of the live one (for a run that must not touch the live folder)
)
. "$PSScriptRoot\common.ps1"
# Phase gate and game lock: nothing launches unless the phase is TEST, and this run holds the one game slot until it ends.
if (-not $Plan) { throw 'a plan file is required: -Plan tools\playtest\plans\<name>.json' }
if (-not (Test-Path $Plan)) { throw "plan not found: $Plan" }
$planObj = Get-Content -Raw $Plan | ConvertFrom-Json
$planChecks = @($planObj.checks)
$referenced = @($planChecks | ForEach-Object { @($_.variants) })
$unreferenced = @($Variants | Where-Object { $referenced -notcontains $_ })
if ($unreferenced.Count -gt 0) { throw "refusing to start: variant(s) not referenced by any check in $($planObj.name): $($unreferenced -join ', ')" }
Assert-TestPhase
$lockToken = Enter-GameLock 'autoplaytest'

if (-not $Out) { $Out = Join-Path 'C:\CLAUDE\RESEARCH\autoplaytest' (Get-Date -Format 'yyyyMMdd_HHmmss') }
New-Item -ItemType Directory -Force $Out | Out-Null
$hbJob = Start-Heartbeat $Game
Write-RunLog "run start: plan=$($planObj.name) variants=$($Variants -join ',') spot=$Spot game=$Game out=$Out"
Write-RunLog "purpose: $($planObj.purpose)"
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
$camFileLive = Join-Path $Game 'tyrender\cam.txt'
$camBackup = Join-Path $Out 'cam.txt.orig'
$hadCam = Test-Path $camFileLive
if ($hadCam) { Copy-Item $camFileLive $camBackup -Force }

# Each variant: the exe to start, the launch args, ini keys to set on top of the installed ini, and which checks to run.
$defs = @{
    stock        = @{ Exe = 'TY.exe';          Args = '-skipvideo'; Ini = @{};                              Flicker = $true;  Overlay = $false }
    mod          = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{};                              Flicker = $true;  Overlay = $false }
    overlay      = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'overlay.Key' = 'F10' };        Flicker = $false; Overlay = $true }
    overlay_billabong = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'overlay.Key' = 'F10' }; Flicker = $false; Overlay = $true; Spot = 'billabong' }
    fog_seethru1 = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'fog.SeeThruDepth' = '1' };       Flicker = $false; Overlay = $false }
    emit1        = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'emit.Enable' = '1' };            Flicker = $false; Overlay = $false }
    # Requested A/B variants, opt-in with -Variants. The water, trees and portal spots are not wired yet (see docs), so these run at the Rainbow Cliffs spot.
    dbg_water_0  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'pbr.DebugWater' = '0' };         Flicker = $true;  Overlay = $false; Spot = 'billabong' }
    dbg_water_1  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'pbr.DebugWater' = '1' };         Flicker = $true;  Overlay = $false; Spot = 'billabong' }
    dbg_water_2  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'pbr.DebugWater' = '2' };         Flicker = $true;  Overlay = $false; Spot = 'billabong' }
    dbg_water_4  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'pbr.DebugWater' = '4' };         Flicker = $true;  Overlay = $false; Spot = 'billabong' }
    dbg_water_8  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'pbr.DebugWater' = '8' };         Flicker = $true;  Overlay = $false; Spot = 'billabong' }
    dbg_water_5  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'pbr.DebugWater' = '5' };         Flicker = $true;  Overlay = $false; Spot = 'billabong' }
    skyfog_012   = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'atmos.SkyFogDist' = '0.12' };     Flicker = $false; Overlay = $false }   # needs #58 on main
    skyfog_045   = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'atmos.SkyFogDist' = '0.45' };     Flicker = $false; Overlay = $false }   # needs #58 on main
    trees_see0   = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'fog.SeeThruDepth' = '0' };        Flicker = $false; Overlay = $false; Spot = 'billabong' }
    trees_see1   = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'fog.SeeThruDepth' = '1' };        Flicker = $false; Overlay = $false; Spot = 'billabong' }
    portal_comp0 = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'emit.Enable' = '1'; 'emit.Compensate' = '0' }; Flicker = $false; Overlay = $false }
    gi_checker1  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'gi.Checker' = '1' };     Flicker = $false; Overlay = $false }
    lod_enable1  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'lod.Enable' = '1' };      Flicker = $false; Overlay = $false }
    portal_comp1 = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'emit.Enable' = '1'; 'emit.Compensate' = '1' }; Flicker = $false; Overlay = $false }
    # Cost and A/B numbers run (open-issue numbers plan). GI is off in the default ini, so every GI/LOD cost variant switches it on explicitly. Compare the [gpu]/[gpusub] means with tools\playtest\gpu_summary.ps1.
    cost_base    = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'gi.Enable' = '1' };                                   Flicker = $false; Overlay = $false }
    cost_checker = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'gi.Enable' = '1'; 'gi.Checker' = '1' };               Flicker = $false; Overlay = $false }
    cost_atrous  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'gi.Enable' = '1'; 'gi.AtrousFar' = '1' };             Flicker = $false; Overlay = $false }
    cost_lod_gi  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'gi.Enable' = '1'; 'lod.Enable' = '1'; 'lod.GI' = '1' }; Flicker = $false; Overlay = $false }
    cost_lod_all = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'gi.Enable' = '1'; 'lod.Enable' = '1'; 'lod.GI' = '1'; 'lod.AO' = '1'; 'lod.Fog' = '1'; 'lod.Shadow' = '1'; 'lod.TessEnable' = '1' }; Flicker = $false; Overlay = $false }
    # WaterMaskSoft 0 vs 1 from above the water (Billabong; camera 500 up, looking down 60 degrees). Placeholder camera: look at the frames before trusting the number.
    wms_0        = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'fog.WaterMaskSoft' = '0' }; Flicker = $true; Overlay = $false; Spot = 'billabong'; Cam = '0 500 0 0 -60' }
    wms_1        = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{ 'fog.WaterMaskSoft' = '1' }; Flicker = $true; Overlay = $false; Spot = 'billabong'; Cam = '0 500 0 0 -60' }
    sky_yaw000  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{}; Flicker = $false; Overlay = $false; Spot = 'rainbow'; Cam = '0 0 0 0 0' }
    sky_yaw090  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{}; Flicker = $false; Overlay = $false; Spot = 'rainbow'; Cam = '0 0 0 90 0' }
    sky_yaw180  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{}; Flicker = $false; Overlay = $false; Spot = 'rainbow'; Cam = '0 0 0 180 0' }
    sky_yaw270  = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{}; Flicker = $false; Overlay = $false; Spot = 'rainbow'; Cam = '0 0 0 270 0' }
    # Static water camera: slot 1 (no cutscene), fixed yaw and pitch from the spawn. PLACEHOLDER yaw/pitch: set them from the sky_yaw sweep before the owner relies on this view.
    water_static = @{ Exe = 'TY_tyrender.exe'; Args = '-skipvideo'; Ini = @{}; Flicker = $true; Overlay = $false; Spot = 'rainbow'; Cam = '0 0 0 270 -20' }
}

function Stop-Game { Get-GameProcs $Game | Stop-Process -Force -ErrorAction SilentlyContinue; Start-Sleep -Milliseconds 800 }

function Save-GameLog($r) {
    $src = Join-Path $Game 'tyrender.log'
    if (-not (Test-Path $src)) { $r.LogHits = 'no tyrender.log'; return }
    Copy-Item $src (Join-Path $r.Dir 'tyrender.log') -Force
    $hits = @(Select-String -Path $src -Pattern '(?i)\berror\b|\bfailed\b|assert' | Select-Object -First 5)
    # [overlay] GL state lines (issue #12): the before/during/after dumps the overlay writes.
    $r.OverlayLog = @(Select-String -Path $src -Pattern '\[overlay\]' | Select-Object -First 40 | ForEach-Object { $_.Line.Trim() })
    $r.LogHits = "$($hits.Count) (first: " + (($hits | ForEach-Object { $_.Line.Trim() }) -join ' | ') + ')'
    if ($hits.Count -eq 0) { $r.LogHits = '0' }
}

function Run-Variant([string]$Name, [hashtable]$Def) {
    $dir = Join-Path $Out $Name; New-Item -ItemType Directory -Force $dir | Out-Null
    Write-RunLog "variant $Name start (spot=$(if ($Def.ContainsKey('Spot')) { $Def.Spot } else { $Spot }))"
    $r = @{ Name = $Name; Dir = $dir; Boot = ''; Alive = 'n/a'; Base = @(); ShotCount = 0; Flicker = 'n/a'; FlickerLower = -1; Overlay = 'n/a'; OverlayPass = $null; OverlayRows = @(); LogHits = 'n/a'; OverlayLog = @(); TyBlue = -1; FgMiss = 0 }
    if (Get-GameProcs $Game) { $r.Boot = 'FAIL a game process is already running from this folder (slot busy; announce in the Fix plan thread)'; return $r }
    $proc = [IO.Path]::GetFileNameWithoutExtension($Def.Exe)

    # Every variant starts from the backed-up ini, so overrides never stack.
    if ($hadIni) { Copy-Item $iniBackup $ini -Force }
    foreach ($k in $Def.Ini.Keys) { $sec, $key = $k.Split('.', 2); Set-IniKey $ini $sec $key $Def.Ini[$k] }
    # View offset: the game reads tyrender\cam.txt each frame. The original file (if any) was saved to $Out\cam.txt.orig and is put back in the finally block.
    $camFile = Join-Path $Game 'tyrender\cam.txt'
    $cam = if ($Def.ContainsKey('Cam')) { $Def.Cam } else { $Cam }
    $spot = if ($Def.ContainsKey('Spot')) { $Def.Spot } else { $Spot }
    # Billabong (slot 2) starts with a real-time cutscene on every load, so give it longer to settle.
    $settle = if ($spot -eq 'billabong' -or $spot -eq 'slot2') { 45 } else { 12 }
    if ($cam) { New-Item -ItemType Directory -Force (Split-Path $camFile) | Out-Null; Set-Content -Path $camFile -Value $cam -Encoding ascii }
    else { Remove-Item $camFile -ErrorAction SilentlyContinue }

    $boot = & powershell -NoProfile -File (Join-Path $PSScriptRoot 'boot_spot.ps1') -Game $Game -Exe $Def.Exe -Extra $Def.Args -Spot $spot -Settle $settle 2>&1 | Out-String
    if ($LASTEXITCODE -ne 0) { $r.Boot = "FAIL $($boot.Trim())"; Write-RunLog "variant $Name boot FAILED: $($boot.Trim())"; Stop-Game; Save-GameLog $r; return $r }
    $r.Boot = 'OK'
    Write-RunLog "variant $Name booted to gameplay, taking shots"

    # Two A/B pairs at the start spot (the first is the baseline for the overlay check).
    $script:FgMiss = 0
    $r.Base = @(Take-Pair $Game $proc $dir 'base01') + @(Take-Pair $Game $proc $dir 'base02')
    $r.ShotCount = $r.Base.Count
    $r.FgMiss = $script:FgMiss

    if ($Def.Flicker) {
        # Static camera, N presses, first frame of each pair. Std-dev over time is the flicker measure; the stock variant is the baseline for animated grass and clouds.
        $frames = @()
        for ($i = 1; $i -le $FlickerFrames; $i++) { $p = @(Take-Pair $Game $proc $dir ('flk{0:D2}' -f $i)); if ($p.Count -ge 1) { $frames += $p[0] } }
        if ($frames.Count -ge 3) {
            $s = [PlaytestImg]::TemporalStd([string[]]$frames, $TyX0, $TyY0, $TyX1, $TyY1)
            $r.FlickerLower = $s[2]
            $r.TyBlue = ([PlaytestImg]::RegionBlueStd([string[]]$frames, $TyX0, $TyY0, $TyX1, $TyY1))[0]
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
    $r.FgMiss = $script:FgMiss
    Write-RunLog "variant $Name done: shots=$($r.ShotCount) notForeground=$($r.FgMiss) flicker=[$($r.Flicker)] overlay=[$($r.Overlay)] alive=$($r.Alive)"
    return $r
}

$results = @()
try {
    $n = 0
    foreach ($name in $Variants) {
        if (-not $defs.ContainsKey($name)) { throw "unknown variant $name" }
        Write-RunStatus @{ state = 'running'; current = $name; done = $n; total = $Variants.Count }
        $results += Run-Variant $name $defs[$name]
        $n++
    }
    Write-RunStatus @{ state = 'finished'; current = ''; done = $n; total = $Variants.Count }
} finally {
    Stop-Game
    if ($hadIni) { Copy-Item $iniBackup $ini -Force }
    if ($hadCam) { Copy-Item $camBackup $camFileLive -Force } else { Remove-Item $camFileLive -ErrorAction SilentlyContinue }
    Stop-Heartbeat $hbJob
    Write-RunLog 'run end'
    Exit-GameLock $lockToken
}

# Check evaluation. Each check decides PASS, FAIL or INCONCLUSIVE from the run's results and logs. Nothing here guesses: a missing variant or log gives INCONCLUSIVE.
function Get-CheckResult($c, $results) {
    $vs = @($c.variants)
    $rs = @{}; foreach ($r in $results) { $rs[$r.Name] = $r }
    $missing = @($vs | Where-Object { -not $rs.ContainsKey($_) })
    if ($missing.Count -gt 0) { return @{ Status = 'INCONCLUSIVE'; Evidence = "variant(s) not in this run: $($missing -join ', ')" } }
    switch ($c.kind) {
        'boot' {
            $bad = @($vs | Where-Object { $rs[$_].Boot -ne 'OK' })
            if ($bad.Count -gt 0) { return @{ Status = 'FAIL'; Evidence = "boot failed: $($bad -join ', ')" } }
            return @{ Status = 'PASS'; Evidence = "booted to gameplay: $($vs -join ', ')" }
        }
        'overlay' {
            $v = $rs[$vs[0]]
            if ($null -eq $v.OverlayPass) { return @{ Status = 'INCONCLUSIVE'; Evidence = 'overlay check did not run' } }
            return @{ Status = $(if ($v.OverlayPass) { 'PASS' } else { 'FAIL' }); Evidence = $v.Overlay }
        }
        'flicker' {
            $v = $rs[$vs[0]]; $b = $rs[$c.baseline]
            if (-not $b -or $v.FlickerLower -lt 0 -or $b.FlickerLower -lt 0) { return @{ Status = 'INCONCLUSIVE'; Evidence = "no flicker values for $($vs[0]) or baseline $($c.baseline)" } }
            $ratio = $v.FlickerLower / [Math]::Max(0.0001, $b.FlickerLower)
            return @{ Status = $(if ($ratio -le $c.limit) { 'PASS' } else { 'FAIL' }); Evidence = ('lower-half ratio {0:N2} vs {1} (limit {2})' -f $ratio, $c.baseline, $c.limit) }
        }
        'log' {
            $log = Join-Path (Join-Path $Out $vs[0]) 'tyrender.log'
            if (-not (Test-Path $log)) { return @{ Status = 'INCONCLUSIVE'; Evidence = "no tyrender.log for $($vs[0])" } }
            $hits = @(Select-String -Path $log -Pattern $c.pattern)
            $ok = if ($c.mustMatch) { $hits.Count -gt 0 } else { $hits.Count -eq 0 }
            $first = (@($hits | Select-Object -First 3 | ForEach-Object { $_.Line.Trim() })) -join ' | '
            return @{ Status = $(if ($ok) { 'PASS' } else { 'FAIL' }); Evidence = "$($hits.Count) matching line(s) in $($vs[0])$(if ($first) { ': ' + $first })" }
        }
        'tyflicker' {
            $v = $rs[$vs[0]]; $b = $rs[$c.baseline]
            if (-not $b -or $v.TyBlue -lt 0 -or $b.TyBlue -lt 0) { return @{ Status = 'INCONCLUSIVE'; Evidence = "no Ty-region values for $($vs[0]) or baseline $($c.baseline)" } }
            $ratio = $v.TyBlue / [Math]::Max(0.0001, $b.TyBlue)
            return @{ Status = $(if ($ratio -le $c.limit) { 'PASS' } else { 'FAIL' }); Evidence = ('Ty blue-excess std {0:N3} vs {1} {2:N3}: ratio {3:N2} (limit {4})' -f $v.TyBlue, $c.baseline, $b.TyBlue, $ratio, $c.limit) }
        }
        'manual' { return @{ Status = 'INCONCLUSIVE'; Evidence = "needs a person to look: $(Join-Path $Out 'contact_sheet.png') (variants: $($vs -join ', '))" } }
        default { return @{ Status = 'INCONCLUSIVE'; Evidence = "unknown check kind $($c.kind)" } }
    }
}

$checkResults = @()
# Written by the visual reviewer after the run: [{ variant, verdict: ok|suspect|broken, evidence }]. Missing file = no visual review yet.
$visualReview = @()
$vrFile = Join-Path $Out 'visual_review.json'
if (Test-Path $vrFile) { $visualReview = @(Get-Content $vrFile -Raw | ConvertFrom-Json) }
foreach ($c in $planChecks) {
    $res = Get-CheckResult $c $results
    # A shot taken while the game was not foreground is not a valid measurement: the check cannot PASS or FAIL on it.
    $fgBad = @($results | Where-Object { @($c.variants) -contains $_.Name -and $_.FgMiss -gt 0 })
    if ($fgBad.Count -gt 0 -and $c.kind -in @('flicker', 'tyflicker', 'overlay')) { $res = @{ Status = 'INCONCLUSIVE'; Evidence = "game not foreground for shots in: $(($fgBad | ForEach-Object { $_.Name }) -join ', '); shots invalid" } }
    # Visual review (tools/playtest/review/RUBRIC.md) is advisory: a suspect or broken verdict turns a PASS into INCONCLUSIVE and flags the frame. It never turns a FAIL into a PASS.
    $vrBad = @($visualReview | Where-Object { @($c.variants) -contains $_.variant -and $_.verdict -in @('suspect', 'broken') })
    if ($vrBad.Count -gt 0 -and $res.Status -eq 'PASS') { $res = @{ Status = 'INCONCLUSIVE'; Evidence = 'visual review flagged the frames, check the user: ' + (($vrBad | ForEach-Object { "$($_.variant) $($_.verdict) ($($_.evidence))" }) -join '; ') } }
    $checkResults += [pscustomobject]@{ id = $c.id; status = $res.Status; evidence = $res.Evidence; issue = $c.issue; pass = $c.pass; next = $c.next; decides = $c.decides; requests = @($c.requests) }
    Write-RunLog ("check [{0}] {1}: {2}" -f $res.Status, $c.id, $res.Evidence)
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
$ovr = @($results | Where-Object { $_.OverlayLog.Count -gt 0 })
if ($ovr.Count -gt 0) { $md += '## [overlay] GL state lines from tyrender.log'; foreach ($r in $ovr) { $md += "### $($r.Name)"; $md += '```'; $md += $r.OverlayLog; $md += '```' } }
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
$md += '- Spots: rainbow (slot 1, the Red Mountain start per the owner) and billabong (slot 2, cutscene start). The red rock view is found by the sky_yaw sweep (free camera via tyrender\cam.txt). Portal: not yet.'
$md += '- Per-frame GPU and frame times are not parsed from tyrender.log here. The log copies are in each variant folder.'
$md += '- The game window must be foreground. key.ps1 fails visibly when it is not.'
$md = @("# Purpose", "", $planObj.purpose, "", "Plan: $($planObj.name) ($Plan)", "") + $md
$md += '## Check results'
$md += ''
$md += '| check | status | issue | requests | decides | evidence |'
$md += '|---|---|---|---|---|---|'
foreach ($cr in $checkResults) { $md += "| $($cr.id) | $($cr.status) | #$($cr.issue) | $(@($cr.requests) -join ', ') | $($cr.decides) | $($cr.evidence) |" }
$md += ''
$md += '## Issues'
foreach ($cr in $checkResults) {
    $line = switch ($cr.status) {
        'PASS' { "- #$($cr.issue) ($($cr.id)): supported by this run. Issues close only after the owner's playtest." }
        'FAIL' { "- #$($cr.issue) ($($cr.id)): not supported. Next: $($cr.next)" }
        default { "- #$($cr.issue) ($($cr.id)): inconclusive. $($cr.evidence)" }
    }
    $md += $line
}
$md -join "`n" | Set-Content (Join-Path $Out 'report.md') -Encoding utf8

$rows = @($results | Where-Object { $_.Base.Count -ge 1 } | ForEach-Object { @{ Label = $_.Name; Files = @($_.Base[0], $_.Base[1]) } })
$rows += @($results | Where-Object { $_.OverlayRows.Count -gt 0 } | ForEach-Object { $_.OverlayRows[0] })
if ($rows.Count -gt 0) { New-ContactSheet -Rows $rows -OutFile (Join-Path $Out 'contact_sheet.png') }

# Hand the sheet to the visual reviewer: latest_for_review holds only the newest run's sheet and the rubric pointer.
$reviewDir = 'C:\CLAUDE\RESEARCH\autoplaytest\latest_for_review'
New-Item -ItemType Directory -Force $reviewDir | Out-Null
Get-ChildItem $reviewDir -File -ErrorAction SilentlyContinue | Remove-Item -Force
if (Test-Path (Join-Path $Out 'contact_sheet.png')) { Copy-Item (Join-Path $Out 'contact_sheet.png') $reviewDir -Force }
@"
Run: $Out
Rubric: tools/playtest/review/RUBRIC.md. Write $(Join-Path $Out 'visual_review.json') as [{variant, verdict, evidence}].
"@ | Set-Content (Join-Path $reviewDir 'REVIEW.txt') -Encoding utf8

$md -join "`n"
"Output: $Out"

# Boot the game to a known spot and wait for gameplay (state 5). Wraps tools/ty_play.ps1, which owns the menu route.
#   powershell -NoProfile -File tools\playtest\boot_spot.ps1 [-Game <dir>] [-Exe TY_tyrender.exe] [-Extra -skipvideo] [-Spot rainbow|slot2]
# Refuses unless the phase is TEST, and holds the game lock for the boot (unless the caller already holds it).
# Spots: rainbow = save slot 1 (level 1, Rainbow Cliffs, the default). billabong = save slot 2 (level 2, Billabong; slot2 is the same).
# Billabong is taken from the harness notes (TY_SLOT=2 = Billabong) and has not been verified on the PC yet.
# Red rock, skybox and portal views are not wired: they need a camera offset (-Cam) or a save from the owner.
# Exit code 0 = gameplay reached (state 5). 1 = the boot failed or was refused; the reason is printed.
param(
    [string]$Game = 'C:\CLAUDE\TY the Tasmanian Tiger',
    [string]$Exe = 'TY_tyrender.exe',
    [string]$Extra = '-skipvideo',
    [ValidateSet('rainbow', 'billabong', 'slot2')][string]$Spot = 'rainbow',
    [int]$Settle = 12
)
. "$PSScriptRoot\common.ps1"
$code = 1
$ownLock = -not $env:TY_LOCK_TOKEN
$tok = $null
try {
    Assert-TestPhase
    $tok = Enter-GameLock 'boot_spot'
    $play = Join-Path $PSScriptRoot '..\ty_play.ps1'
    # billabong = level 2 (save slot 2), per docs/RESUME.md: TY_SLOT=2 selects slot 2.
    if ($Spot -eq 'slot2' -or $Spot -eq 'billabong') { $env:TY_SLOT = '2' } else { Remove-Item Env:TY_SLOT -ErrorAction SilentlyContinue }
    & powershell -NoProfile -File $play -Extra $Extra -Settle $Settle -Exe $Exe -Game $Game
    if ($LASTEXITCODE -ne 0) { throw "ty_play exited $LASTEXITCODE" }
    'BOOT OK'
    $code = 0
} catch {
    "BOOT FAIL: $($_.Exception.Message)"
} finally {
    if ($ownLock -and $tok) { Exit-GameLock $tok }
}
exit $code

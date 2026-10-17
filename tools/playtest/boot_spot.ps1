# Boot the game to a known spot and wait for gameplay (state 5). Wraps tools/ty_play.ps1, which owns the menu route.
#   powershell -NoProfile -File tools\playtest\boot_spot.ps1 [-Game <dir>] [-Exe TY_tyrender.exe] [-Extra -skipvideo] [-Spot rainbow|slot2]
# Refuses unless the phase is TEST, and holds the game lock for the boot (unless the caller already holds it).
# Spots: rainbow = save slot GAME 1 (Rainbow Cliffs start, the default). slot2 = save slot GAME 2 (set TY_SLOT=2).
# Billabong water, level two, red rock and the portal have no save yet: they need save files or scripted walking, not built here.
# Exit code 0 = gameplay reached (state 5). 1 = the boot failed or was refused; the reason is printed.
param(
    [string]$Game = 'C:\CLAUDE\TY the Tasmanian Tiger',
    [string]$Exe = 'TY_tyrender.exe',
    [string]$Extra = '-skipvideo',
    [ValidateSet('rainbow', 'slot2')][string]$Spot = 'rainbow',
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
    if ($Spot -eq 'slot2') { $env:TY_SLOT = '2' } else { Remove-Item Env:TY_SLOT -ErrorAction SilentlyContinue }
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

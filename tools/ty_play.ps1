# Boot the tyrender-patched game straight to the deterministic test spot (save GAME 2 start, Rainbow Cliffs) and wait until gameplay.
# Route (2026-10-11): TY_tyrender.exe -skipvideo -> main menu (state 8) -> ENTER (LOAD GAME) -> ENTER (GAME 2 under the mouse/selected) -> state 5.
# NOTE: `-level z1` stopped working in windowed mode, so we use the save. Needs the save in slot GAME 2 (created by NEW GAME on 2026-10-11).
# Usage: powershell -NoProfile -File ty_play.ps1 [-Settle 12] [-Exe TY_tyrender.exe]
param([string]$Extra = '-skipvideo', [int]$Settle = 12, [string]$Exe = 'TY_tyrender.exe', [string]$Game = 'C:\CLAUDE\TY the Tasmanian Tiger')
$log = "$Game\TyOutputLog.txt"
Get-Process ([IO.Path]::GetFileNameWithoutExtension($Exe)) -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 1; Remove-Item $log, "$Game\tyrender.log" -ErrorAction SilentlyContinue
Start-Process "$Game\$Exe" -ArgumentList $Extra -WorkingDirectory $Game | Out-Null
function WaitState($n, $sec) { for ($i = 0; $i -lt $sec; $i++) { Start-Sleep 1; if ((Get-Content $log -ErrorAction SilentlyContinue) -match "^New State: $n") { return $true } }; return $false }
if (-not (WaitState 8 90)) { throw 'menu (state 8) not reached' }
Start-Sleep 2
$key = Join-Path $PSScriptRoot 'key.ps1'; $proc = [IO.Path]::GetFileNameWithoutExtension($Exe)
# The menus select by MOUSE HOVER, so the real pointer position decides which item/slot ENTER activates (root cause of the 1-in-3 boot failures:
# a pointer resting over the EMPTY save slot GAME 2 made ENTER do nothing). Park the pointer in a neutral corner for the main menu, then put it on
# save slot GAME 1 (the saved Rainbow Cliffs start) before the second ENTER. Fractions are of the client area, so any window size works.
Add-Type @'
using System; using System.Runtime.InteropServices;
public class MM { [StructLayout(LayoutKind.Sequential)] public struct PT { public int X, Y; } [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
 [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref PT p); [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
 public static void MoveFrac(IntPtr h, double fx, double fy) { RECT r; GetClientRect(h, out r); PT p; p.X = (int)((r.R - r.L) * fx); p.Y = (int)((r.B - r.T) * fy); ClientToScreen(h, ref p); SetCursorPos(p.X, p.Y); } }
'@
function Pointer($fx, $fy) { $gp = Get-Process $proc -ErrorAction SilentlyContinue | Select-Object -First 1; if ($gp) { [MM]::MoveFrac($gp.MainWindowHandle, $fx, $fy); Start-Sleep -Milliseconds 500 } }
Pointer 0.01 0.01
& powershell -NoProfile -File $key -Keys 'ENTER' -DelayMs 2500 -Proc $proc | Out-Null
Start-Sleep 1; $sx = if ($env:TY_SLOT -eq '2') { 0.50 } else { 0.324 }; Pointer ($sx - 0.044) 0.42; Pointer $sx 0.489   # the hover selection needs a real mouse-MOVE on the Load screen: approach from an offset, then land on slot GAME 1
& powershell -NoProfile -File $key -Keys 'ENTER' -DelayMs 500 -Proc $proc | Out-Null
if (-not (WaitState 5 25)) { Pointer ($sx - 0.024) 0.45; Pointer $sx 0.489; & powershell -NoProfile -File $key -Keys 'ENTER' -DelayMs 500 -Proc $proc | Out-Null; if (-not (WaitState 5 60)) { throw 'gameplay (state 5) not reached' } }
Start-Sleep $Settle
'gameplay ready'


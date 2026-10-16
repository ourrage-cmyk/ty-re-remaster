# Send key(s) to the running TY game window, then optionally take an in-process snapshot (snap.ps1).
# Keys: comma list of names: ENTER, ESC, UP, DOWN, LEFT, RIGHT, SPACE, TAB, or a single letter/digit (e.g. R).
# Uses keybd_event with scancodes and a HOLD (default 120 ms) — SendKeys' instant down/up is missed by the game at high fps,
# and computer-use key presses are ignored by TY entirely (verified 2026-10-11). Only sends while the game is foreground.
# Usage: powershell -NoProfile -File key.ps1 -Keys "DOWN,ENTER" [-HoldMs 120] [-DelayMs 600] [-Snap name] [-OutDir dir] [-Proc TY_tyrender]
param([string]$Keys = '', [int]$HoldMs = 120, [int]$DelayMs = 600, [string]$Snap = '', [string]$OutDir = 'C:\CLAUDE\RESEARCH\shots', [string]$Proc = 'TY_tyrender', [int]$TargetPid = 0)
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class KW {
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int c);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint type);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint idAttach, uint idAttachTo, bool fAttach);
  [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
  // Foreground-lock workaround without fake key presses: briefly attach our input queue to the current foreground thread, then activate.
  public static bool Force(IntPtr h) {
    IntPtr fg = GetForegroundWindow(); if (fg == h) return true;
    uint pid; uint fgThread = GetWindowThreadProcessId(fg, out pid); uint me = GetCurrentThreadId(); bool att = false;
    if (fgThread != 0 && fgThread != me) att = AttachThreadInput(me, fgThread, true);
    ShowWindow(h, 9); SetForegroundWindow(h);
    if (att) AttachThreadInput(me, fgThread, false);
    return GetForegroundWindow() == h;
  }
}
'@
$vkMap = @{ ENTER = 0x0D; ESC = 0x1B; UP = 0x26; DOWN = 0x28; LEFT = 0x25; RIGHT = 0x27; SPACE = 0x20; TAB = 0x09 }
$extended = @('UP', 'DOWN', 'LEFT', 'RIGHT')
$h = if ($TargetPid) { [IntPtr](Get-Process -Id $TargetPid -ErrorAction Stop).MainWindowHandle } else { [IntPtr](Get-Process $Proc -ErrorAction Stop | Select-Object -First 1).MainWindowHandle }
foreach ($k in ($Keys -split ',' | ForEach-Object { $_.Trim().ToUpper() } | Where-Object { $_ })) {
    $fgOk = $false   # verify focus right before the key; retry (research: SetForegroundWindow only succeeds under narrow conditions) and fail VISIBLY
    # agents' subprocesses are not allowed to steal focus; the lead's TOOLS\focus_daemon.ps1 does it for them, so wait up to ~12 s for focus to arrive
    for ($try = 0; $try -lt 24 -and -not $fgOk; $try++) { $fgOk = [KW]::Force($h); if (-not $fgOk) { Start-Sleep -Milliseconds 500 } else { Start-Sleep -Milliseconds 120 } }
    if (-not $fgOk -or [KW]::GetForegroundWindow() -ne $h) { "FOCUS LOST - $k NOT sent (game not foreground after 12 s; is TOOLS\focus_daemon.ps1 running?)"; continue }
    $vk = if ($vkMap.ContainsKey($k)) { $vkMap[$k] } else { [int][char]$k[0] }
    $scan = [byte][KW]::MapVirtualKey([uint32]$vk, 0)
    $ext = if ($extended -contains $k) { 1 } else { 0 }
    [KW]::keybd_event([byte]$vk, $scan, $ext, [UIntPtr]::Zero)
    Start-Sleep -Milliseconds $HoldMs
    [KW]::keybd_event([byte]$vk, $scan, ($ext -bor 2), [UIntPtr]::Zero)
    "sent $k"; Start-Sleep -Milliseconds $DelayMs
}
if ($Snap) { & powershell -NoProfile -File (Join-Path $PSScriptRoot 'snap.ps1') -Name $Snap -OutDir $OutDir }

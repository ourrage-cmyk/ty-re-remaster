# Bring the game window to the foreground and confirm it is there. Exit 0 = foreground, 1 = could not get focus, 2 = no window for that PID.
# Ladder: plain SetForegroundWindow, then restore + ALT tap (Windows lets a process take focus after a key press from it), up to 5 tries.
#   powershell -NoProfile -File tools\focus_game.ps1 -TargetPid <pid>
param([Parameter(Mandatory)][int]$TargetPid)
if (-not ('FG3' -as [type])) {
Add-Type @'
using System; using System.Runtime.InteropServices;
public class FG3 { [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow(); [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int c); [DllImport("user32.dll")] public static extern void keybd_event(byte k, byte s, uint f, UIntPtr e); }
'@
}
$p = Get-Process -Id $TargetPid -ErrorAction SilentlyContinue
if (-not $p -or $p.MainWindowHandle -eq 0) { exit 2 }
$h = $p.MainWindowHandle
for ($t = 0; $t -lt 5; $t++) {
    if ([FG3]::GetForegroundWindow() -eq $h) { exit 0 }
    [FG3]::ShowWindow($h, 9) | Out-Null
    [FG3]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero); [FG3]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
    [FG3]::SetForegroundWindow($h) | Out-Null
    Start-Sleep -Milliseconds 400
}
if ([FG3]::GetForegroundWindow() -eq $h) { exit 0 }
exit 1

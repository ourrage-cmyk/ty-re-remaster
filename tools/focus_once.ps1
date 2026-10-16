# One focus attempt (run as a fresh short-lived process by focus_daemon.ps1): ladder of techniques, exits 0 if the game is foreground afterwards.
Add-Type @"
using System; using System.Runtime.InteropServices;
public class FO {
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow(); [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int c); [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h); [DllImport("user32.dll")] public static extern void SwitchToThisWindow(IntPtr h, bool alt);
 [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid); [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool f);
 [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId(); [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
}
"@
$g = Get-Process TY_tyrender -ErrorAction SilentlyContinue | Select-Object -First 1; if (-not $g -or $g.MainWindowHandle -eq [IntPtr]::Zero) { exit 2 }; $h = $g.MainWindowHandle
function Ok { Start-Sleep -Milliseconds 350; [FO]::GetForegroundWindow() -eq $h }
if ([FO]::IsIconic($h)) { [void][FO]::ShowWindow($h, 9) }
[void][FO]::SetForegroundWindow($h); if (Ok) { exit 0 }
[FO]::SwitchToThisWindow($h, $true); if (Ok) { exit 0 }
[FO]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero); [FO]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero); [void][FO]::SetForegroundWindow($h); if (Ok) { exit 0 }
$fg = [FO]::GetForegroundWindow(); $pp = 0; $ft = [FO]::GetWindowThreadProcessId($fg, [ref]$pp); $me = [FO]::GetCurrentThreadId(); $att = [FO]::AttachThreadInput($me, $ft, $true)
[void][FO]::SetForegroundWindow($h); [void][FO]::BringWindowToTop($h); if ($att) { [void][FO]::AttachThreadInput($me, $ft, $false) }; if (Ok) { exit 0 }
exit 1

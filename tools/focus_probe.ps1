Add-Type @"
using System; using System.Runtime.InteropServices;
public class FT2 {
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow(); [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int c); [DllImport("user32.dll")] public static extern void SwitchToThisWindow(IntPtr h, bool alt);
 [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid); [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a, uint b, bool f);
 [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId(); [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x,int y,int cx,int cy,uint f);
 [DllImport("user32.dll")] public static extern bool AllowSetForegroundWindow(int pid); [DllImport("user32.dll")] public static extern bool LockSetForegroundWindow(uint code);
}
"@
$out='C:\CLAUDE\TOOLS\focus_probe.txt'; Remove-Item $out -ErrorAction SilentlyContinue
$g=Get-Process TY_tyrender -ErrorAction SilentlyContinue | select -First 1; if(-not $g){ 'no game' | Set-Content $out; return }; $h=$g.MainWindowHandle
function Chk { Start-Sleep -Milliseconds 500; [FT2]::GetForegroundWindow() -eq $h }
function Att([scriptblock]$act){ $fg=[FT2]::GetForegroundWindow(); $pid2=0; $ft=[FT2]::GetWindowThreadProcessId($fg,[ref]$pid2); $me=[FT2]::GetCurrentThreadId(); $a=[FT2]::AttachThreadInput($me,$ft,$true); & $act; if($a){ [void][FT2]::AttachThreadInput($me,$ft,$false) } }
$r=[ordered]@{}
[FT2]::SetForegroundWindow($h)|Out-Null; $r['plain SetForegroundWindow']=Chk
if(-not (Chk)){ [FT2]::SwitchToThisWindow($h,$true); $r['SwitchToThisWindow']=Chk }
if(-not (Chk)){ [FT2]::keybd_event(0x12,0,0,[UIntPtr]::Zero); [FT2]::keybd_event(0x12,0,2,[UIntPtr]::Zero); [FT2]::SetForegroundWindow($h)|Out-Null; $r['ALT tap + SetForeground']=Chk }
if(-not (Chk)){ Att { [FT2]::SetForegroundWindow($h)|Out-Null }; $r['AttachThreadInput + SetForeground']=Chk }
if(-not (Chk)){ [FT2]::keybd_event(0x12,0,0,[UIntPtr]::Zero); Att { [FT2]::SetForegroundWindow($h)|Out-Null; [FT2]::BringWindowToTop($h)|Out-Null }; [FT2]::keybd_event(0x12,0,2,[UIntPtr]::Zero); $r['ALT + Attach + SetForeground']=Chk }
if(-not (Chk)){ [FT2]::ShowWindow($h,6)|Out-Null; Start-Sleep -Milliseconds 300; [FT2]::ShowWindow($h,9)|Out-Null; [FT2]::SetForegroundWindow($h)|Out-Null; $r['minimize+restore']=Chk }
if(-not (Chk)){ [FT2]::SetWindowPos($h,[IntPtr](-1),0,0,0,0,0x43)|Out-Null; $r['TOPMOST (stays on top, not focus)']=Chk }
$fg=[FT2]::GetForegroundWindow(); $p2=0; [void][FT2]::GetWindowThreadProcessId($fg,[ref]$p2)
($r.GetEnumerator() | %{ "$($_.Key) => $($_.Value)" }) + "final foreground: $((Get-Process -Id $p2 -ErrorAction SilentlyContinue).Name)" | Set-Content $out

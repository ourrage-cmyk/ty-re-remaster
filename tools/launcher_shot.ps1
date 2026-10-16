# Screenshot the launcher: powershell -NoProfile -File launcher_shot.ps1 -Out x.png [-Ini file] [-Exe path] [-Hover "x,y"] [-Click "x,y"]   (client coordinates)
param([string]$Out = 'C:\CLAUDE\RESEARCH\shots\launcher_new.png', [string]$Ini = 'C:\CLAUDE\RESEARCH\results\inis\preset_Balanced.ini', [string]$Exe = 'C:\CLAUDE\SRC\tyrender\out\TYLauncher.exe', [string]$Hover = '', [string]$Click = '')
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices;
public class WC3 { [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f); [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
 [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r); [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref PT p);
 [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y); [DllImport("user32.dll")] public static extern void mouse_event(uint f, int x, int y, uint d, UIntPtr e);
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; } [StructLayout(LayoutKind.Sequential)] public struct PT { public int X,Y; } }
'@
$t = 'C:\CLAUDE\RESEARCH\results\launcher_test'; New-Item -ItemType Directory -Force $t | Out-Null; Copy-Item $Exe "$t\TYLauncher.exe" -Force; if ($Ini) { Copy-Item $Ini "$t\tyrender.ini" -Force }
$p = Start-Process "$t\TYLauncher.exe" -WorkingDirectory $t -PassThru; Start-Sleep 2; $p.Refresh(); $h = $p.MainWindowHandle
function ToScreen($s) { $xy = $s -split ','; $pt = New-Object WC3+PT; $pt.X = [int]$xy[0]; $pt.Y = [int]$xy[1]; [void][WC3]::ClientToScreen($h, [ref]$pt); $pt }
if ($Click) { $pt = ToScreen $Click; [WC3]::SetCursorPos($pt.X, $pt.Y) | Out-Null; Start-Sleep -Milliseconds 200; [WC3]::mouse_event(2, 0, 0, 0, [UIntPtr]::Zero); [WC3]::mouse_event(4, 0, 0, 0, [UIntPtr]::Zero); Start-Sleep -Milliseconds 400 }
if ($Hover) { $pt = ToScreen $Hover; [WC3]::SetCursorPos($pt.X, $pt.Y) | Out-Null; Start-Sleep -Milliseconds 600 }
$r = New-Object WC3+RECT; [void][WC3]::GetWindowRect($h, [ref]$r); $w = $r.R - $r.L; $ht = $r.B - $r.T
$bmp = New-Object Drawing.Bitmap $w, $ht; $g = [Drawing.Graphics]::FromImage($bmp); $dc = $g.GetHdc(); [void][WC3]::PrintWindow($h, $dc, 2); $g.ReleaseHdc($dc); $g.Dispose()
$bmp.Save($Out, [Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose(); Stop-Process $p -Force; "$w x $ht -> $Out"

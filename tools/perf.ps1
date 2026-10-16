# Throughput benchmark: boots the patched game (-novsync) to the test spot and counts frames over a wall-clock window.
# Usage: powershell -NoProfile -File perf.ps1 -Label name -Ini "<full tyrender.ini text>" [-Secs 8]   (restores tyrender.ini afterwards; retries boot up to 3x)
# The game window is forced to the foreground first: a background window is throttled by Windows (~15 fps) and invalidates the numbers.
param([Parameter(Mandatory)][string]$Label, [Parameter(Mandatory)][string]$IniText, [int]$Secs = 8, [string]$Game = 'C:\CLAUDE\TY the Tasmanian Tiger')
Add-Type @'
using System; using System.Runtime.InteropServices;
public class FG2 { [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow(); [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid); [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h); }
'@
$ini = "$Game\tyrender.ini"; $orig = Get-Content $ini -Raw; $T = 'C:\CLAUDE\TOOLS'
function FrameNo { $l = Select-String "$Game\tyrender.log" -Pattern '\[snap\].*frame=(\d+)' | select -Last 1; [int][regex]::Match($l.Line, 'frame=(\d+)').Groups[1].Value }
function FgName { $h = [FG2]::GetForegroundWindow(); $p = 0; [void][FG2]::GetWindowThreadProcessId($h, [ref]$p); try { (Get-Process -Id $p).Name } catch { '?' } }
try {
  Set-Content $ini $IniText
  for ($try = 0; $try -lt 3; $try++) {
    try { & powershell -NoProfile -File $T\ty_play.ps1 -Extra '-skipvideo -novsync' -Settle 5 | Out-Null; break } catch { Stop-Process -Name TY_tyrender -Force -ErrorAction SilentlyContinue; Start-Sleep 2; if ($try -eq 2) { "$Label : BOOT FAILED"; return } }
  }
  & powershell -NoProfile -File $T\key.ps1 -Keys 'ENTER' -DelayMs 1500 -Proc TY_tyrender | Out-Null; Start-Sleep 2; & powershell -NoProfile -File $T\key.ps1 -Keys 'ENTER' -DelayMs 1500 -Proc TY_tyrender | Out-Null; Start-Sleep 3
  $gp = Get-Process TY_tyrender -ErrorAction SilentlyContinue; if ($gp) { [void][FG2]::SetForegroundWindow($gp.MainWindowHandle); Start-Sleep 1 }
  $fg = FgName
  # GPU-side ms from the DLLs [gpu] lines (gameplay only: draws>1000); skip the first sample (warm-up), average the next two (max 240 s)
  $res=@(); $deadline=(Get-Date).AddSeconds(240)
  while ((Get-Date) -lt $deadline) {
    $res=@(Select-String "$Game\tyrender.log" -Pattern "\[gpu\] frame ([\d.]+) ms \| ao ([\d.]+) gi ([\d.]+) atmos ([\d.]+) post ([\d.]+) \| engine\+rest ([\d.]+) ms \((\d+) draws" | % { $g=$_.Matches[0].Groups; [pscustomobject]@{f=[double]$g[1].Value; ao=[double]$g[2].Value; gi=[double]$g[3].Value; at=[double]$g[4].Value; po=[double]$g[5].Value; en=[double]$g[6].Value; d=[int]$g[7].Value} } | ?{ $_.d -gt 1000 })
    if ($res.Count -ge 3) { break }; Start-Sleep 3
  }
  if ($res.Count -lt 3) { "$Label : NO GAMEPLAY SAMPLES"; return }
  $u=$res | select -Skip 1 -First 2
  "{0,-20} frame {1,6:N1} ms | ao {2,5:N1} gi {3,5:N1} atmos {4,5:N1} post {5,4:N1} | engine+rest {6,6:N1} ms" -f $Label, ($u|measure f -Average).Average, ($u|measure ao -Average).Average, ($u|measure gi -Average).Average, ($u|measure at -Average).Average, ($u|measure po -Average).Average, ($u|measure en -Average).Average} finally { Stop-Process -Name TY_tyrender -Force -ErrorAction SilentlyContinue; Set-Content $ini $orig }



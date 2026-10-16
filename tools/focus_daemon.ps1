# Focus keeper for scripted game tests (see BRIEFING.md "Test-run prerequisites"). Run once, hidden, from the LEAD session:
#   Start-Process powershell -ArgumentList '-NoProfile','-WindowStyle','Hidden','-File','C:\CLAUDE\TOOLS\focus_daemon.ps1'
# While a harness test is running (IN_TEST.flag) and the game window is not foreground, it runs focus_once.ps1 (a fresh short-lived process: long-lived
# background processes lose the right to take focus, fresh children of the lead's shell keep it) and logs every outcome to focus_daemon.log.
# Stop: create C:\CLAUDE\TOOLS\focus_daemon.stop
Add-Type @"
using System; using System.Runtime.InteropServices;
public class FD2 { [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow(); }
"@
$flag = 'C:\CLAUDE\TOOLS\_test_backup\IN_TEST.flag'; $stop = 'C:\CLAUDE\TOOLS\focus_daemon.stop'; $log = 'C:\CLAUDE\TOOLS\focus_daemon.log'; $ok = 0; $bad = 0
"[$(Get-Date -f HH:mm:ss)] focus_daemon v2 started" | Add-Content $log
while ($true) {
    if (Test-Path $stop) { Remove-Item $stop -Force; "[$(Get-Date -f HH:mm:ss)] stopped (ok=$ok failed=$bad)" | Add-Content $log; break }
    if (Test-Path $flag) {
        $p = Get-Process TY_tyrender -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($p -and $p.MainWindowHandle -ne [IntPtr]::Zero -and [FD2]::GetForegroundWindow() -ne $p.MainWindowHandle) {
            $pr = Start-Process powershell -ArgumentList '-NoProfile', '-WindowStyle', 'Hidden', '-File', 'C:\CLAUDE\TOOLS\focus_once.ps1' -PassThru -WindowStyle Hidden; [void]$pr.WaitForExit(15000)
            if ($pr.ExitCode -eq 0) { $ok++; "[$(Get-Date -f HH:mm:ss)] focus restored (total ok=$ok)" | Add-Content $log } else { $bad++; "[$(Get-Date -f HH:mm:ss)] focus attempt FAILED exit=$($pr.ExitCode) (failed=$bad)" | Add-Content $log }
        }
    }
    Start-Sleep -Milliseconds 800
}

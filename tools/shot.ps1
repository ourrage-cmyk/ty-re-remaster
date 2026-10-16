# Launch the game, capture ONLY the game window rect (never the whole desktop - privacy), log lifetime/exit code, keep the game's own log.
# Timer mode : powershell -File shot.ps1 -Out C:\path\prefix -Times "15,30,45" -Keys "10:{ESC};20:{DOWN}" [-Game <dir>]
# Auto mode  : powershell -File shot.ps1 -Out C:\path\prefix -Auto -AutoShots "12,16,20"
#              (deletes stale log, waits for menu 'New State: 8', DOWN+ENTER = New Game, retries ENTER until 'New State: 5' = gameplay, then captures N seconds after gameplay starts)
# -Launcher  : start via <Game>\TYLauncher.exe (injects tyrender.dll); also copies tyrender.log to <Out>.tyrender.log
# Outputs: <Out>_<t>.png (timer) or <Out>_g<N>.png (auto), <Out>.log (copy of TyOutputLog.txt), <Out>.run.txt (timeline + exit code)
param([string]$Out='C:\CLAUDE\RESEARCH\shots\shot', [string]$Times='20,40', [string]$Game='C:\CLAUDE\TY the Tasmanian Tiger', [string]$Keys='',
      [switch]$Auto, [string]$AutoShots='12,16,20', [int]$MaxSeconds=200, [string]$Exe='TY.exe', [string]$ExeArgs='', [switch]$NoKeys, [switch]$Launcher)
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class W {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x,int y,int cx,int cy, uint f);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr h, int attr, out RECT r, int size);
}
'@
[W]::SetProcessDPIAware() | Out-Null     # real pixel rects, not DPI-scaled
New-Item -ItemType Directory -Force (Split-Path $Out) | Out-Null
$timeline = New-Object System.Collections.ArrayList
function Note($m){ $s = "[{0,6:N1}s] {1}" -f ((Get-Date)-$script:t0).TotalSeconds, $m; [void]$timeline.Add($s); $s }
$logPath = "$Game\TyOutputLog.txt"
if(Test-Path $logPath){ try { [IO.File]::Delete($logPath) } catch {} }      # avoid stale-log confusion
$seenLog=@{}
function ReadLog(){ try{ $fs=[IO.File]::Open($logPath,'Open','Read','ReadWrite'); $sr=New-Object IO.StreamReader($fs); $t=$sr.ReadToEnd(); $sr.Close(); $fs.Close(); return $t }catch{ return '' } }
function PollLog(){
  $txt = ReadLog; if(-not $txt){ return '' }
  foreach($m in [regex]::Matches($txt,'(?m)^(New State: \d+|Main_InitGameSystems.*|Game_Init : Finished|Cache MISS: Room.*|Shader .*Info Log.*|Failed to create shader.*)\r?$')){
    $k=$m.Value.Trim(); if(-not $seenLog.ContainsKey($k)){ $seenLog[$k]=1; Note "LOG: $k" | Out-Null }
  }
  return $txt
}
function SendKeyToGame($h,$key){
  [W]::SetWindowPos($h,[IntPtr](-1),0,0,0,0,0x43) | Out-Null; [W]::SetForegroundWindow($h) | Out-Null; Start-Sleep -Milliseconds 200
  if([W]::GetForegroundWindow() -eq $h){ [System.Windows.Forms.SendKeys]::SendWait($key); Note "sent key $key" | Out-Null; return $true }
  Note "game not foreground - key $key skipped" | Out-Null; return $false
}
function Grab($h,$file){
  [W]::SetWindowPos($h,[IntPtr](-1),0,0,0,0,0x43) | Out-Null; [W]::SetForegroundWindow($h) | Out-Null; Start-Sleep -Milliseconds 300
  if([W]::GetForegroundWindow() -ne $h){ Note "game window NOT foreground - capture skipped (privacy)" | Out-Null; return $false }
  # DWMWA_EXTENDED_FRAME_BOUNDS (9) = visible frame; GetWindowRect includes the invisible resize border -> desktop pixels leaked (privacy)
  $r = New-Object W+RECT; if([W]::DwmGetWindowAttribute($h,9,[ref]$r,16) -ne 0){ [W]::GetWindowRect($h,[ref]$r) | Out-Null }
  $w=$r.R-$r.L; $hh=$r.B-$r.T; if($w -le 0 -or $hh -le 0){ Note "bad rect" | Out-Null; return $false }
  $bmp = New-Object System.Drawing.Bitmap $w,$hh
  $g = [System.Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($r.L,$r.T,0,0,(New-Object System.Drawing.Size $w,$hh))
  $bmp.Save($file); $g.Dispose(); $bmp.Dispose(); Note "saved $file (${w}x${hh})" | Out-Null; return $true
}
if($Launcher){
  # start via TYLauncher.exe (injects tyrender.dll), then track the TY.exe it spawns
  $before = @(Get-Process TY -ErrorAction SilentlyContinue | % Id)
  $la = if($ExeArgs){ Start-Process "$Game\TYLauncher.exe" -ArgumentList $ExeArgs -WorkingDirectory $Game -PassThru } else { Start-Process "$Game\TYLauncher.exe" -WorkingDirectory $Game -PassThru }
  $p = $null; for($i=0; $i -lt 60 -and -not $p; $i++){ Start-Sleep -Milliseconds 250; $p = Get-Process TY -ErrorAction SilentlyContinue | ? { $before -notcontains $_.Id } | Select-Object -First 1 }
  if(-not $p){ throw "TYLauncher did not start TY.exe (launcher exit: $($la.HasExited) $($la.ExitCode))" }
} else {
  $p = if($ExeArgs){ Start-Process "$Game\$Exe" -ArgumentList $ExeArgs -WorkingDirectory $Game -PassThru } else { Start-Process "$Game\$Exe" -WorkingDirectory $Game -PassThru }
}
$t0 = Get-Date; Note "launched pid $($p.Id) $(if($Launcher){'(via TYLauncher + tyrender.dll)'})" | Out-Null
$exited = $false
try {
  if($Auto){
    $shots = @($AutoShots -split ',' | % { [int]$_ }); $shotDone=@{}
    $menuAt=$null; $lastEsc=$null; $lastEnter=$null; $downSent=$false; $enterCount=0; $playAt=$null
    while(((Get-Date)-$t0).TotalSeconds -lt $MaxSeconds){
      Start-Sleep -Milliseconds 500; $p.Refresh(); if($p.HasExited){ $exited=$true; break }
      $txt = PollLog; $now=((Get-Date)-$t0).TotalSeconds; $h=$p.MainWindowHandle
      if($h -eq [IntPtr]::Zero){ continue }
      if($txt -match '(?m)^New State: 5\r?$'){ if(-not $playAt){ $playAt=$now; Note "gameplay started" | Out-Null } }
      else {
        if(-not $menuAt -and $txt -match '(?m)^New State: 8\r?$'){ $menuAt=$now; Note "state 8 reached" | Out-Null }
        # proven timeline: ESC every ~4s from 3s to ~32s (skips intro videos), DOWN once at ~35s (NEW GAME), then ENTER every ~5s until gameplay
        if($NoKeys){ }
        elseif($now -ge 3 -and $now -lt 33){ if(-not $lastEsc -or ($now-$lastEsc) -ge 4){ if(SendKeyToGame $h '{ESC}'){ $lastEsc=$now } } }
        elseif($now -ge 35 -and -not $downSent){ if(SendKeyToGame $h '{DOWN}'){ $downSent=$true; $lastEnter=$now-3 } }
        elseif($downSent -and $enterCount -lt 10 -and ($now-$lastEnter) -ge 5 -and $txt -notmatch '(?m)^New State: 9\r?$'){ if(SendKeyToGame $h '{ENTER}'){ $enterCount++; $lastEnter=$now } }
      }      if($playAt){ foreach($s in $shots){ if(-not $shotDone[$s] -and ($now-$playAt) -ge $s){ if(Grab $h "${Out}_g$s.png"){ $shotDone[$s]=$true } } } ; if(($shots | ? { -not $shotDone[$_] }).Count -eq 0){ break } }
    }
    if(-not $playAt -and -not $exited){ Note "TIMEOUT: gameplay never started" | Out-Null; $h=$p.MainWindowHandle; if($h -ne [IntPtr]::Zero){ Grab $h "${Out}_timeout.png" | Out-Null } }
  } else {
    $secs = $Times -split ',' | % { [int]$_ }
    $keyList = @($Keys -split ';' | ? { $_ -match '^(\d+):(.+)$' } | % { [pscustomobject]@{ At=[int]($_ -split ':',2)[0]; Key=($_ -split ':',2)[1]; Done=$false } })
    foreach($t in $secs){
      while((((Get-Date)-$t0).TotalSeconds) -lt $t){
        Start-Sleep -Milliseconds 500; $p.Refresh(); PollLog | Out-Null
        if($p.HasExited){ $exited=$true; break }
        foreach($k in ($keyList | ? { -not $_.Done -and $_.At -le ((Get-Date)-$t0).TotalSeconds })){
          $h=$p.MainWindowHandle; if($h -ne [IntPtr]::Zero){ if(SendKeyToGame $h $k.Key){ $k.Done=$true } }
        }
      }
      if($exited -or $p.HasExited){ $exited=$true; break }
      $h = $p.MainWindowHandle; if($h -eq [IntPtr]::Zero){ Note "no window at ${t}s" | Out-Null; continue }
      Grab $h "${Out}_$t.png" | Out-Null
    }
  }
  if($exited){ Note "GAME EXITED, code $($p.ExitCode)" | Out-Null }
} finally {
  Get-CimInstance Win32_Process -Filter "Name='$Exe'" -ErrorAction SilentlyContinue | % { try { Invoke-CimMethod -InputObject $_ -MethodName Terminate -ErrorAction Stop | Out-Null } catch {} }
  Start-Sleep -Milliseconds 500
  if(Test-Path $logPath){ Copy-Item $logPath "$Out.log" -Force }
  if($Launcher -and (Test-Path "$Game\tyrender.log")){ Copy-Item "$Game\tyrender.log" "$Out.tyrender.log" -Force }
  Set-Content "$Out.run.txt" $timeline -Encoding utf8
  $timeline | ? { $_ -match 'saved|menu ready|gameplay|EXITED|TIMEOUT|NOT foreground|skipped|Info Log|Failed' }
}





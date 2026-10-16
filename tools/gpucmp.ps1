# Live A/B inside ONE game session using the DLL's GPU timers: for each variant, patch tyrender.ini keys, trigger reload, wait for 2 fresh [gpu] lines
# (gameplay only), print the last, and snap a screenshot. Usage: powershell -NoProfile -File gpucmp.ps1 -Variants "name|sec.key=val;sec.key=val", "name2|..." [-Snap]
param([Parameter(Mandatory)][string[]]$Variants, [switch]$Snap, [string]$Game = 'C:\CLAUDE\TY the Tasmanian Tiger')
$T = 'C:\CLAUDE\TOOLS'; $ini = "$Game\tyrender.ini"; $log = "$Game\tyrender.log"; $orig = Get-Content $ini -Raw
function Boot { for ($attempt = 0; $attempt -lt 4; $attempt++) { try { & powershell -NoProfile -File $T\ty_play.ps1 -Extra '-skipvideo -novsync' -Settle 5 | Out-Null; return $true } catch { Stop-Process -Name TY_tyrender -Force -ErrorAction SilentlyContinue; Start-Sleep 3 } }; $false }
function SetKey($text, $sec, $key, $val) {
  $lines = [System.Collections.Generic.List[string]]($text -split "`r?`n"); $in = $false; $done = $false
  for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match '^\[(.+)\]') { if ($in -and -not $done) { $lines.Insert($i, "$key=$val"); $done = $true; break }; $in = ($Matches[1] -eq $sec) }
    elseif ($in -and $lines[$i] -match "^$key\s*=") { $lines[$i] = "$key=$val"; $done = $true; break }
  }
  if (-not $done) { if (-not ($lines -contains "[$sec]")) { $lines.Add("[$sec]") }; $idx = $lines.IndexOf("[$sec]"); $lines.Insert($idx + 1, "$key=$val") }
  $lines -join "`n"
}
try {
  if (-not (Boot)) { 'BOOT FAILED'; return }
  & powershell -NoProfile -File $T\key.ps1 -Keys 'ENTER' -DelayMs 1500 -Proc TY_tyrender | Out-Null; Start-Sleep 2; & powershell -NoProfile -File $T\key.ps1 -Keys 'ENTER' -DelayMs 1500 -Proc TY_tyrender | Out-Null; Start-Sleep 4
  foreach ($v in $Variants) {
    $name, $kv = $v -split '\|', 2; $txt = $orig
    foreach ($p in ($kv -split ';' | ? { $_ })) { $a, $val = $p -split '=', 2; $sec, $key = $a -split '\.', 2; $txt = SetKey $txt $sec $key $val }
    Set-Content $ini $txt; New-Item -ItemType File -Force "$Game\tyrender\reload" | Out-Null
    Start-Sleep 3; $before = (Select-String $log -Pattern '\[gpu\]').Count
    for ($i = 0; $i -lt 60; $i++) { Start-Sleep 3; if ((Select-String $log -Pattern '\[gpu\]').Count -ge $before + 2) { break } }
    $line = Select-String $log -Pattern '\[gpu\]' | select -Last 1 | % Line
    if ($Snap) { & powershell -NoProfile -File $T\snap.ps1 -Name ("cmp_" + ($name -replace '\W', '_')) | Out-Null }
    "{0,-18} {1}" -f $name, ($line -replace '^\[gpu\] ', '')
  }
} finally { Stop-Process -Name TY_tyrender -Force -ErrorAction SilentlyContinue; Set-Content $ini $orig }

# Unattended, resumable executor for the plan in C:\CLAUDE\RESEARCH\todo.json. Zero LLM tokens: it only runs kind=auto items.
#   powershell -NoProfile -File todo_runner.ps1            run every runnable auto item (deps done) until none is left, then print the board
#   powershell -NoProfile -File todo_runner.ps1 -Status    just print the board (what is done / next / waiting for an agent / waiting for the user)
#   powershell -NoProfile -File todo_runner.ps1 -Id T05    run exactly this item (ignores deps)
#   -Max N stop after N items;  create C:\CLAUDE\RESEARCH\runner.stop to stop gracefully.   Start detached:  Start-Process powershell -WindowStyle Hidden -ArgumentList '-NoProfile','-File','C:\CLAUDE\TOOLS\todo_runner.ps1'
# todo.json item fields: id, title, kind (auto|agent|lead|director|user), status (open|running|done|failed|blocked), deps ("T01,T02"), needs_game (bool), command (PowerShell, kind=auto),
#   fail_if (regex: output matching it = failure), est_tokens, task (prompt file for agent/director), notes, attempts, started, finished, result.
# Crash safety: a 'running' item whose runner is gone is reset to open at the next start; the JSON is rewritten atomically after every state change; each run's output is kept in results\runs\<id>.log.
param([switch]$Status, [string]$Id = '', [int]$Max = 100)
$todo = 'C:\CLAUDE\RESEARCH\todo.json'; $runs = 'C:\CLAUDE\RESEARCH\results\runs'; New-Item -ItemType Directory -Force $runs | Out-Null; $utf8 = New-Object Text.UTF8Encoding($false)
function Load { @([IO.File]::ReadAllText($todo, [Text.Encoding]::UTF8) | ConvertFrom-Json) }
function Save($items) { $tmp = "$todo.tmp"; [IO.File]::WriteAllText($tmp, (ConvertTo-Json -InputObject @($items) -Depth 6), $utf8); Move-Item $tmp $todo -Force }
function Deps($it) { @(([string]$it.deps) -split '[,\s]+' | ? { $_ }) }
function Board($items) {
    $done = @($items | ? { $_.status -eq 'done' } | % id)
    foreach ($it in $items) { $ready = (@(Deps $it | ? { $_ -notin $done }).Count -eq 0); $tag = switch ($it.status) { 'done' { 'DONE   ' } 'running' { 'RUNNING' } 'failed' { 'FAILED ' } default { if ($ready) { 'READY  ' } else { 'waiting' } } }
        "{0} {1,-5} {2,-8} {3}{4}" -f $tag, $it.id, $it.kind, $it.title, $(if ($it.status -eq 'failed' -or $it.status -eq 'done') { "  -> $($it.result)" } else { '' }) } }
function EnsureDaemon { $d = Get-CimInstance Win32_Process | ? { $_.CommandLine -match 'focus_daemon\.ps1' }; if (-not $d) { Start-Process powershell -WindowStyle Hidden -ArgumentList '-NoProfile', '-WindowStyle', 'Hidden', '-File', 'C:\CLAUDE\TOOLS\focus_daemon.ps1' | Out-Null; 'started focus_daemon'; Start-Sleep 2 } }
$items = Load
if ($Status) { Board $items; return }
foreach ($it in $items) { if ($it.status -eq 'running') { $it.status = 'open'; $it.result = 'reset after interrupted run' } }; Save $items
$count = 0
while ($count -lt $Max) {
    if (Test-Path 'C:\CLAUDE\RESEARCH\runner.stop') { 'runner.stop found: stopping'; break }
    $items = Load; $done = @($items | ? { $_.status -eq 'done' } | % id)
    $next = if ($Id) { $items | ? { $_.id -eq $Id } | select -First 1 } else { $items | ? { $_.kind -eq 'auto' -and $_.status -eq 'open' -and (@(Deps $_ | ? { $_ -notin $done }).Count -eq 0) } | select -First 1 }
    if (-not $next) { 'no runnable auto item left'; break }
    $next.status = 'running'; $next.started = (Get-Date).ToString('s'); $next.attempts = [int]$next.attempts + 1; Save $items
    "[{0}] running {1}: {2}" -f (Get-Date -f HH:mm:ss), $next.id, $next.title
    if ($next.needs_game) { EnsureDaemon }
    $log = "$runs\$($next.id).log"; $cmd = [string]$next.command
    $enc = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($cmd))   # encoded command: immune to quoting/semicolon problems in the command text
    $outText = try { & powershell -NoProfile -ExecutionPolicy Bypass -EncodedCommand $enc 2>&1 | Out-String } catch { "EXCEPTION: $($_.Exception.Message)" }; $code = $LASTEXITCODE
    [IO.File]::WriteAllText($log, "# $($next.id) $($next.title)`r`n# command: $cmd`r`n# exit: $code`r`n`r`n$outText", $utf8)
    $items = Load; $cur = $items | ? { $_.id -eq $next.id }
    $bad = ($code -ne 0 -and $null -ne $code) -or ($cur.fail_if -and $outText -match [string]$cur.fail_if)
    $cur.finished = (Get-Date).ToString('s'); $tail = (($outText -split "`r?`n" | ? { $_.Trim() } | select -Last 2) -join ' | '); $cur.result = $tail.Substring(0, [math]::Min(220, $tail.Length))
    if ($bad) { $cur.status = $(if ([int]$cur.attempts -ge 2) { 'failed' } else { 'open' }) } else { $cur.status = 'done' }
    Save $items; "[{0}] {1} -> {2}  {3}" -f (Get-Date -f HH:mm:ss), $cur.id, $cur.status, $cur.result
    & powershell -NoProfile -File C:\CLAUDE\TOOLS\steplog.ps1 -Agent runner -Msg "$($cur.id) $($cur.title): $($cur.status). $($cur.result)" -Files $log | Out-Null
    $count++; if ($Id) { break }
}
Board (Load)

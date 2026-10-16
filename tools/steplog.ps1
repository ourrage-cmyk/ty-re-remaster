# Step logger: append a timestamped entry to this agent's log in the research folder AND sync it (plus optional files) to OpenViking.
# Call after EVERY meaningful step (design decision, build result, test run with numbers, finding, failure):
#   powershell -NoProfile -File C:\CLAUDE\TOOLS\steplog.ps1 -Agent atmos -Msg "built froxel pass; compile ok; first PERF atmos 3.1 ms (was 10.2)" [-Files "C:\path\a.inl,C:\path\REPORT.md"]
# Log file : C:\CLAUDE\RESEARCH\sessions\2026-10-12_session5\agents\<Agent>.md   (OpenViking mirror: .../research/sessions/2026-10-12_session5/agents/<Agent>.md)
# -Files   : comma list of local files (source, reports, PNG paths are text-only: paths are logged but images are not uploaded); each is mirrored to
#            .../sessions/2026-10-12_session5/agents/<Agent>/<filename>  (code files are stored as <name>.md with a code fence).
# Never fails the caller: OpenViking errors are printed as a warning and the local log is still written.
param([Parameter(Mandatory)][string]$Agent, [Parameter(Mandatory)][string]$Msg, [string]$Files = '')
$dir = 'C:\CLAUDE\RESEARCH\sessions\2026-10-12_session5\agents'; New-Item -ItemType Directory -Force $dir | Out-Null
$log = Join-Path $dir "$Agent.md"
if (-not (Test-Path $log)) { [IO.File]::WriteAllText($log, "# Step log: $Agent (session 5, 2026-10-12)`r`n`r`n", (New-Object Text.UTF8Encoding($false))) }
$stamp = (Get-Date).ToString('yyyy-MM-dd HH:mm')
$entry = "## $stamp`r`n$Msg`r`n"; if ($Files) { $entry += "Files: $Files`r`n" }
[IO.File]::AppendAllText($log, "$entry`r`n", (New-Object Text.UTF8Encoding($false)))
try {
    . C:\CLAUDE\TOOLS\ov.ps1
    $R = 'viking://resources/TECHNICAL/ty-hd-renderer/research/sessions/2026-10-12_session5/agents'
    ovSync $log "$R/$Agent.md" | Out-Null
    foreach ($f in ($Files -split ',' | % { $_.Trim() } | ? { $_ })) {
        if ((Test-Path $f) -and ($f -notmatch '\.(png|jpg|bmp|dll|exe|zip)$')) { ovSync $f "$R/$Agent/$(Split-Path $f -Leaf)" | Out-Null }
    }
    "logged + synced: $log"
} catch { "WARNING: logged locally ($log) but OpenViking sync failed: $($_.Exception.Message)" }

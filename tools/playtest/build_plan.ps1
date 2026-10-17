# Merge all pending playtest requests into one plan (the request queue, see docs/AUTOPLAYTEST.md).
#   powershell -NoProfile -File tools\playtest\build_plan.ps1 [-Name <stamp>]
# - Reads tools\playtest\requests\*.json with status "pending".
# - Keeps only variants autoplaytest.ps1 knows. Unknown variants go to requests\BACKLOG.md and the request is marked blocked; nothing is dropped silently.
# - De-duplicates checks (same kind, variants, pattern, baseline, limit) and unions their request ids into one check.
# - Writes plans\merged_<name>.json, which is run with: autoplaytest.ps1 -Plan <that file> -Variants <list it prints>.
param([string]$Name = (Get-Date -Format 'yyyyMMdd_HHmm'))
$dir = $PSScriptRoot
$reqDir = Join-Path $dir 'requests'
$known = @([regex]::Matches((Get-Content (Join-Path $dir 'autoplaytest.ps1') -Raw), '(?m)^\s{4}([a-z0-9_]+)\s*= @\{') | ForEach-Object { $_.Groups[1].Value })
$backlog = @()
$checks = @{}
$variants = @()
$purposes = @()
foreach ($f in Get-ChildItem $reqDir -Filter 'req-*.json' -ErrorAction SilentlyContinue) {
    $req = Get-Content -Raw $f.FullName | ConvertFrom-Json
    if ($req.status -ne 'pending') { continue }
    $unknown = @(@($req.variants | ForEach-Object { $_.name }) + @($req.checks | ForEach-Object { @($_.variants) }) | Sort-Object -Unique | Where-Object { $known -notcontains $_ })
    if ($unknown.Count -gt 0) {
        $backlog += "- $($req.id): needs variant(s) not in autoplaytest.ps1: $($unknown -join ', '). Add them to `$defs, then re-run build_plan."
        continue
    }
    $purposes += "$($req.id): $($req.question)"
    foreach ($v in @($req.variants | ForEach-Object { $_.name })) { if ($variants -notcontains $v) { $variants += $v } }
    foreach ($c in @($req.checks)) {
        $key = '{0}|{1}|{2}|{3}|{4}|{5}' -f $c.kind, (@($c.variants) -join ','), $c.pattern, $c.baseline, $c.limit, $c.mustMatch
        if (-not $checks.ContainsKey($key)) {
            $checks[$key] = [ordered]@{ id = $c.id; kind = $c.kind; variants = @($c.variants); pattern = $c.pattern; mustMatch = $c.mustMatch; baseline = $c.baseline; limit = $c.limit; issue = $req.requester.issue; pass = $c.pass; decides = $c.decides; next = $c.next; requests = @() }
        }
        $checks[$key].requests = @($checks[$key].requests) + $req.id
    }
}
$plan = [ordered]@{ name = "merged_$Name"; purpose = ($purposes -join ' | '); variants = $variants; checks = @($checks.Values) }
$planFile = Join-Path (Join-Path $dir 'plans') "merged_$Name.json"
New-Item -ItemType Directory -Force (Split-Path $planFile) | Out-Null
$plan | ConvertTo-Json -Depth 6 | Set-Content -Path $planFile -Encoding utf8
if ($backlog.Count -gt 0) { Set-Content -Path (Join-Path $reqDir 'BACKLOG.md') -Value ("# Playtest backlog (requests that need harness work)`n`n" + ($backlog -join "`n")) -Encoding utf8 }
"plan: $planFile"
"variants: $($variants -join ',')"
"checks: $($checks.Count)"
if ($backlog.Count -gt 0) { "blocked (see requests\BACKLOG.md): $($backlog.Count)" }

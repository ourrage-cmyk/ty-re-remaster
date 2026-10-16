# wf.ps1 - local-markdown tracker operations for the wayfinder skill (C:\CLAUDE\.claude\skills\wayfinder\SKILL.md).
# Tracker root: C:\CLAUDE\RESEARCH\wayfinder\<map>\   MAP.md (the map issue, label wayfinder:map), tickets\T01-slug.md (child issues), GLOSSARY.md (domain model)
#
#   wf.ps1 new-map    -Map <slug> -Title <t> -Destination <d> [-Notes <n>]
#   wf.ps1 new-ticket -Map <slug> -Title <t> -Type research|prototype|grilling|task -Mode AFK|HITL -Question <q>     -> prints the new id (T01...)
#   wf.ps1 block      -Map <slug> -Ticket T03 -By "T01,T02"          native-style blocking: T03 stays off the frontier until T01+T02 are closed
#   wf.ps1 frontier   -Map <slug>                                     open, unblocked, unclaimed tickets (the edge of the known)
#   wf.ps1 claim      -Map <slug> -Ticket T03 -Who <name>             atomic (create-new claim file); fails if already claimed. Claim BEFORE any work.
#   wf.ps1 resolve    -Map <slug> -Ticket T03 -Answer <text> -Gist <one line>   resolution section + close + append to the map's Decisions-so-far
#   wf.ps1 rule-out   -Map <slug> -Ticket T05 -Why <text>             close a ticket as beyond the destination; one line under Out of scope
#   wf.ps1 fog        -Map <slug> -Text <loose note>                  add a line to Not yet specified      (wf.ps1 unfog -Text <exact line> removes it)
#   wf.ps1 list       -Map <slug>                                     every ticket: id, status, type/mode, claim, blockers, title
#   wf.ps1 show       -Map <slug> -Ticket T03                         print one ticket
# Add -Sync to any mutating command to mirror the map folder to OpenViking afterwards.
# Names, not ids, are what humans read: always refer to tickets by their Title.
param([Parameter(Mandatory, Position = 0)][string]$Cmd, [string]$Map = '', [string]$Title = '', [string]$Destination = '', [string]$Notes = '',
      [string]$Type = 'grilling', [string]$Mode = 'HITL', [string]$Question = '', [string]$Ticket = '', [string]$By = '', [string]$Who = '',
      [string]$Answer = '', [string]$Gist = '', [string]$Why = '', [string]$Text = '', [switch]$Sync)
$root = 'C:\CLAUDE\RESEARCH\wayfinder'; $utf8 = New-Object Text.UTF8Encoding($false)
function WriteText([string]$path, [string]$text) { [IO.File]::WriteAllText($path, $text, $utf8) }
function ReadText([string]$path) { [IO.File]::ReadAllText($path, [Text.Encoding]::UTF8) }
function MapDir { if (-not $Map) { throw 'need -Map <slug>' }; Join-Path $root $Map }
function TicketFile([string]$id) { $f = Get-ChildItem (Join-Path (MapDir) 'tickets') -Filter "$id-*.md" -ErrorAction SilentlyContinue | Select-Object -First 1; if (-not $f) { throw "ticket $id not found in map $Map" }; $f.FullName }
function Meta([string]$text, [string]$key) { $m = [regex]::Match($text, "(?m)^$key[ \t]*:[ \t]*([^\r\n]*)"); if ($m.Success) { $m.Groups[1].Value.Trim() } else { '' } }
function SetMeta([string]$text, [string]$key, [string]$val) { $re = New-Object Text.RegularExpressions.Regex("(?m)^$key[ \t]*:[^\r\n]*"); $ev = [Text.RegularExpressions.MatchEvaluator]{ param($mm) "${key}: $val" }; $re.Replace($text, $ev, 1) }
function AllTickets { $d = Join-Path (MapDir) 'tickets'; if (-not (Test-Path $d)) { return @() }
    foreach ($f in Get-ChildItem $d -Filter 'T*.md' | Sort-Object Name) { $t = ReadText $f.FullName
        [pscustomobject]@{ Id = Meta $t 'id'; Title = Meta $t 'title'; Type = Meta $t 'type'; Mode = Meta $t 'mode'; Status = Meta $t 'status'; Claim = Meta $t 'claimed_by'; Blocked = (Meta $t 'blocked_by'); File = $f.FullName } } }
function BlockerIds([string]$s) { @($s -split '[,\s\[\]]+' | Where-Object { $_ -match '^T\d+$' }) }
function Slug([string]$s) { $x = ($s.ToLower() -replace '[^a-z0-9]+', '-').Trim('-'); if ($x.Length -gt 40) { $x = $x.Substring(0, 40).Trim('-') }; $x }
function AppendUnder([string]$mapText, [string]$heading, [string]$line) {   # append a line at the end of a "## heading" section (before the next "## ")
    $pat = "(?ms)(^## $([regex]::Escape($heading))\s*\r?\n.*?)(?=^## |\z)"; $m = [regex]::Match($mapText, $pat)
    if (-not $m.Success) { return $mapText + "`r`n## $heading`r`n`r`n$line`r`n" }
    $sec = $m.Groups[1].Value.TrimEnd(); $re = New-Object Text.RegularExpressions.Regex($pat); $ev = [Text.RegularExpressions.MatchEvaluator]{ param($mm) "$sec`r`n$line`r`n`r`n" }; $re.Replace($mapText, $ev, 1) }
function Sync-Map { if ($Sync) { try { . C:\CLAUDE\TOOLS\ov.ps1; $r = 'viking://resources/TECHNICAL/ty-hd-renderer/research/wayfinder/' + $Map
        foreach ($f in Get-ChildItem (MapDir) -Recurse -File) { $rel = $f.FullName.Substring((MapDir).Length + 1).Replace('\', '/'); ovSync $f.FullName "$r/$rel" | Out-Null }; 'synced to OpenViking' } catch { "WARNING: OpenViking sync failed: $($_.Exception.Message)" } } }

switch ($Cmd) {
 'new-map' {
    if (-not $Title -or -not $Destination) { throw 'need -Title and -Destination' }
    $d = MapDir; if (Test-Path "$d\MAP.md") { throw "map $Map already exists" }; New-Item -ItemType Directory -Force "$d\tickets" | Out-Null
    $body = "---`r`ntype: wayfinder:map`r`ntitle: $Title`r`ncreated: $((Get-Date).ToString('yyyy-MM-dd HH:mm'))`r`n---`r`n# $Title`r`n`r`n## Destination`r`n`r`n$Destination`r`n`r`n## Notes`r`n`r`n$Notes`r`n`r`n## Decisions so far`r`n`r`n<!-- the index: one line per closed ticket: - [title](tickets/T01-x.md): gist -->`r`n`r`n## Not yet specified`r`n`r`n<!-- in-scope fog that cannot be ticketed yet; graduates as the frontier advances -->`r`n`r`n## Out of scope`r`n`r`n<!-- ruled beyond the destination; closed, never graduates -->`r`n"
    WriteText "$d\MAP.md" $body; WriteText "$d\GLOSSARY.md" "# Glossary (domain model) - $Title`r`n`r`nOne term per entry: **term** - meaning in this effort; aliases to avoid.`r`n"
    "map created: $d\MAP.md"; Sync-Map }
 'new-ticket' {
    if (-not $Title -or -not $Question) { throw 'need -Title and -Question' }
    if ($Type -notin 'research', 'prototype', 'grilling', 'task') { throw 'type must be research|prototype|grilling|task' }
    if ($Mode -notin 'AFK', 'HITL') { throw 'mode must be AFK|HITL' }
    $d = Join-Path (MapDir) 'tickets'; New-Item -ItemType Directory -Force $d | Out-Null
    $n = (Get-ChildItem $d -Filter 'T*.md' | Measure-Object).Count + 1; $id = 'T{0:D2}' -f $n
    $body = "---`r`nid: $id`r`ntitle: $Title`r`ntype: $Type`r`nmode: $Mode`r`nstatus: open`r`nclaimed_by: `r`nblocked_by: []`r`nparent: $Map`r`nlabels: wayfinder:$Type`r`n---`r`n# $Title`r`n`r`n## Question`r`n`r`n$Question`r`n"
    WriteText (Join-Path $d "$id-$(Slug $Title).md") $body; $id; Sync-Map }
 'block' {
    $f = TicketFile $Ticket; $t = ReadText $f; $ids = BlockerIds $By; foreach ($b in $ids) { [void](TicketFile $b) }
    $cur = BlockerIds (Meta $t 'blocked_by'); $all = @($cur + $ids | Select-Object -Unique)
    WriteText $f (SetMeta $t 'blocked_by' ('[' + ($all -join ', ') + ']')); "$Ticket blocked by: $($all -join ', ')"; Sync-Map }
 'frontier' {
    $tk = @(AllTickets); $closed = @($tk | Where-Object { $_.Status -eq 'closed' } | ForEach-Object { $_.Id })
    $fr = @($tk | Where-Object { $_.Status -eq 'open' -and -not $_.Claim -and (@(BlockerIds $_.Blocked | Where-Object { $_ -notin $closed }).Count -eq 0) })
    if (-not $fr) { 'frontier empty (no open, unblocked, unclaimed tickets)' } else { $fr | ForEach-Object { "{0}  [{1}/{2}]  {3}" -f $_.Id, $_.Type, $_.Mode, $_.Title } } }
 'claim' {
    if (-not $Who) { throw 'need -Who' }; $f = TicketFile $Ticket; $lock = $f + '.claim'
    try { $fs = [IO.File]::Open($lock, 'CreateNew', 'Write') ; $b = $utf8.GetBytes("$Who $((Get-Date).ToString('s'))"); $fs.Write($b, 0, $b.Length); $fs.Close() } catch { throw "ticket $Ticket is already claimed ($(ReadText $lock))" }
    $t = ReadText $f; WriteText $f (SetMeta $t 'claimed_by' $Who); "claimed $Ticket for $Who"; Sync-Map }
 'resolve' {
    if (-not $Answer -or -not $Gist) { throw 'need -Answer and -Gist' }
    $f = TicketFile $Ticket; $t = ReadText $f; if ((Meta $t 'status') -eq 'closed') { throw "$Ticket is already closed" }
    $t = SetMeta $t 'status' 'closed'; $t = $t.TrimEnd() + "`r`n`r`n## Resolution ($((Get-Date).ToString('yyyy-MM-dd HH:mm')))`r`n`r`n$Answer`r`n"; WriteText $f $t
    $mp = Join-Path (MapDir) 'MAP.md'; $title = Meta $t 'title'; $rel = 'tickets/' + (Split-Path $f -Leaf)
    WriteText $mp (AppendUnder (ReadText $mp) 'Decisions so far' "- [$title]($rel): $Gist"); "resolved $Ticket; map index updated"; Sync-Map }
 'rule-out' {
    if (-not $Why) { throw 'need -Why' }
    $f = TicketFile $Ticket; $t = ReadText $f; $t = SetMeta $t 'status' 'closed'; $t = $t.TrimEnd() + "`r`n`r`n## Ruled out of scope ($((Get-Date).ToString('yyyy-MM-dd HH:mm')))`r`n`r`n$Why`r`n"; WriteText $f $t
    $mp = Join-Path (MapDir) 'MAP.md'; $title = Meta $t 'title'; $rel = 'tickets/' + (Split-Path $f -Leaf)
    WriteText $mp (AppendUnder (ReadText $mp) 'Out of scope' "- [$title]($rel): $Why"); "$Ticket closed as out of scope"; Sync-Map }
 'fog' {
    if (-not $Text) { throw 'need -Text' }; $mp = Join-Path (MapDir) 'MAP.md'; WriteText $mp (AppendUnder (ReadText $mp) 'Not yet specified' "- $Text"); 'fog line added'; Sync-Map }
 'unfog' {
    if (-not $Text) { throw 'need -Text (exact line without the leading dash)' }; $mp = Join-Path (MapDir) 'MAP.md'; $t = ReadText $mp
    $t2 = ($t -split "`r?`n" | Where-Object { $_.Trim() -ne "- $Text" }) -join "`r`n"; WriteText $mp $t2; 'fog line removed'; Sync-Map }
 'list' { AllTickets | ForEach-Object { "{0}  {1,-6}  {2,-9}/{3,-4}  claim:{4,-10} blocked:{5,-12}  {6}" -f $_.Id, $_.Status, $_.Type, $_.Mode, $_.Claim, $_.Blocked, $_.Title } }
 'show' { Get-Content (TicketFile $Ticket) -Raw }
 default { throw "unknown command '$Cmd' (see the header of wf.ps1)" }
}

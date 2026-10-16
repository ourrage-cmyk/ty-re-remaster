# Mirror the whitelisted project files into the git repo working tree (C:\CLAUDE\REPO\ty-hd-renderer). Never copies game files, textures, saves, recordings, images, zips, DLL/EXE, backups, keys.
#   powershell -NoProfile -File repo_sync.ps1            (copy + show git status)
$repo = 'C:\CLAUDE\REPO\ty-hd-renderer'; New-Item -ItemType Directory -Force $repo | Out-Null
function CopyF($src, $dstDir, $filter) { New-Item -ItemType Directory -Force "$repo\$dstDir" | Out-Null
    Get-ChildItem $src -File -ErrorAction SilentlyContinue | ? { $_.Name -match $filter -and $_.Name -notmatch '\.bak|\.CONFLICT|\.orig$' } | Copy-Item -Destination "$repo\$dstDir" -Force }
# source (master)
CopyF 'C:\CLAUDE\SRC\tyrender' 'src' '\.(cpp|inl|inc|c|ps1|ini)$'
Copy-Item 'C:\CLAUDE\SRC\tyrender\README.md' "$repo\README.md" -Force
Copy-Item 'C:\CLAUDE\SRC\tyrender\PERFORMANCE.md' "$repo\docs\PERFORMANCE.md" -Force -ErrorAction SilentlyContinue
# harness tools (scripts only)
CopyF 'C:\CLAUDE\TOOLS' 'tools' '\.ps1$'
# docs: entry points, plans, findings (text only)
New-Item -ItemType Directory -Force "$repo\docs", "$repo\docs\recording", "$repo\data" | Out-Null
foreach ($n in 'START_HERE.md', 'DOC_MAP.md', 'GOAL.md', 'RESUME.md', 'PLAN_EXEC.md', 'PLAN_NEXT.md', 'PLAN_R6.md', 'STATUS_2026-10-13.md') { if (Test-Path "C:\CLAUDE\RESEARCH\$n") { Copy-Item "C:\CLAUDE\RESEARCH\$n" "$repo\docs\$n" -Force } }
Get-ChildItem 'C:\CLAUDE\RESEARCH\recording' -File -Include *.md, *.txt -ErrorAction SilentlyContinue | % { Copy-Item $_.FullName "$repo\docs\recording\" -Force }
Get-ChildItem 'C:\CLAUDE\RESEARCH\recording' -File | ? { $_.Extension -in '.md', '.txt' } | % { Copy-Item $_.FullName "$repo\docs\recording\" -Force }
Copy-Item 'C:\CLAUDE\RESEARCH\recording\texture_tags.csv' "$repo\data\texture_tags.csv" -Force -ErrorAction SilentlyContinue
Get-ChildItem 'C:\CLAUDE\RESEARCH\results' -File | ? { $_.Extension -eq '.md' -and $_.Length -lt 200000 } | % { New-Item -ItemType Directory -Force "$repo\docs\results" | Out-Null; Copy-Item $_.FullName "$repo\docs\results\" -Force }
Get-ChildItem 'C:\CLAUDE\RESEARCH\agents\tasks' -File -ErrorAction SilentlyContinue | % { New-Item -ItemType Directory -Force "$repo\docs\agent-tasks" | Out-Null; Copy-Item $_.FullName "$repo\docs\agent-tasks\" -Force }
# secret guard: refuse if anything looks like a token
$hits = Get-ChildItem $repo -Recurse -File -ErrorAction SilentlyContinue | ? { $_.FullName -notmatch '\\\.git\\' -and $_.Name -ne 'repo_sync.ps1' } | Select-String -Pattern 'ghp_[A-Za-z0-9]{20,}|github_pat_|sk-[A-Za-z0-9]{20,}|AKIA[0-9A-Z]{16}' -List
if ($hits) { 'SECRET-LIKE STRINGS FOUND, aborting:'; $hits | % { $_.Path }; exit 1 }
Set-Location $repo; if (Test-Path "$repo\.git") { git status --short | Select-Object -First 30; "files tracked/changed: $((git status --short | Measure-Object).Count)" } else { 'repo not initialised yet (git init)' }

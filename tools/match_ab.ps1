# Match cutscene frames across presets (frame rate shifts the timing): reference = vanilla pool frames; for each chosen reference scene find the most similar frame
# (edge-correlation) in every other preset's pool. Output AB_shots\<Out>\sceneNN\{vanilla,low,medium,ultra}.png + matches.md
param([string]$Pool = 'C:\CLAUDE\RESEARCH\AB_shots\level2_pool', [string]$Out = 'level2_billabong_cutscene', [int]$Scenes = 10)
. C:\CLAUDE\TOOLS\imgmetrics.ps1
$dest = "C:\CLAUDE\RESEARCH\AB_shots\$Out"; New-Item -ItemType Directory -Force $dest | Out-Null
$frames = @(Get-ChildItem $Pool -Directory | ? { Test-Path "$($_.FullName)\vanilla.png" } | sort Name)
$X = 640; $Y = 360; $W = 1280; $H = 720; $SX = 300; $SY = 1230; $SW = 1960; $SH = 190   # subtitle band: same text = same moment in every preset
# pick distinct reference scenes
$refs = @(); $last = $null; $since = 99
foreach ($f in ($frames | select -Skip 10 -First 30)) { $p = "$($f.FullName)\vanilla.png"
    if ($last -eq $null) { $refs += $f; $last = $p; $since = 0; continue }
    $c = [ImgM]::GradCorr($last, $p, $SX, $SY, $SW, $SH); $since++
    if (($c -lt 0.55 -or $since -ge 6) -and $refs.Count -lt $Scenes) { $refs += $f; $last = $p; $since = 0 } }
$md = "# Cutscene matches ($Out)`n| scene | ref frame | low | medium | ultra | (edge correlation of best match) |`n|---|---|---|---|---|---|`n"; $n = 0
foreach ($r in $refs) { $n++; $sd = "$dest\scene{0:D2}" -f $n; New-Item -ItemType Directory -Force $sd | Out-Null; Copy-Item "$($r.FullName)\vanilla.png" "$sd\vanilla.png" -Force
    $row = "| $n | $($r.Name) |"
    foreach ($pr in 'low', 'medium', 'ultra') { $best = $null; $bc = -2
        foreach ($f in $frames) { $q = "$($f.FullName)\$pr.png"; if (-not (Test-Path $q)) { continue }; $c = [ImgM]::GradCorr("$($r.FullName)\vanilla.png", $q, $SX, $SY, $SW, $SH) + 0.35 * [ImgM]::GradCorr("$($r.FullName)\vanilla.png", $q, $X, $Y, $W, $H); if ($c -gt $bc) { $bc = $c; $best = $f } }
        Copy-Item "$($best.FullName)\$pr.png" "$sd\$pr.png" -Force; $row += " $($best.Name) ({0:N2}) |" -f $bc }
    $md += "$row`n" }
[IO.File]::WriteAllText("$dest\matches.md", $md, (New-Object Text.UTF8Encoding $false)); $md

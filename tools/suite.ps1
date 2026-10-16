# Standard evaluation suite (zero LLM tokens): in-session A/B of a DLL over the standard views, with perf, metrics, contact sheets and a ledger entry.
#   powershell -NoProfile -File suite.ps1 -Name atmos_ab1 -Dll C:\...\tyrender.dll -AB "froxel=C:\a.ini;raymarch=C:\b.ini" [-Views "default=;vista=0 -60 300 -50 8"] [-NoPerf]
# Outputs in C:\CLAUDE\RESEARCH\results\<Name>\ : <variant>__<view>.png, sheet.png (rows = views, columns = variants), ty_strip.png (Ty crop per variant, default view),
#   manifest.json (perf per variant, errors, metrics: rock-wall saturation/luma per variant), tyrender_last.log ; appends a line to results\ledger.csv ; steplog + OpenViking sync.
# Only live-reloadable ini keys can differ between variants (the game stays running). A variant name may not contain spaces.
param([Parameter(Mandatory)][string]$Name, [Parameter(Mandatory)][string]$Dll, [Parameter(Mandatory)][string]$AB,
      [string]$Views = 'default=;sand_wide=0 0 450 0 12;fence_mid=-250 -40 350 -35 12;sand_graze=-100 -60 700 -15 12;vista=0 -60 300 -50 8', [switch]$NoPerf, [switch]$NoSync)
. C:\CLAUDE\TOOLS\imgmetrics.ps1
$out = "C:\CLAUDE\RESEARCH\results\$Name"; New-Item -ItemType Directory -Force $out | Out-Null
$firstIni = ((($AB -split ';')[0]) -split '=', 2)[1].Trim()   # the game boots with the FIRST variant's ini (so restart-only keys can be set); later variants swap live-reloadable keys
$argsList = @('-NoProfile', '-File', 'C:\CLAUDE\TOOLS\agent_test.ps1', '-Dll', $Dll, '-IniFile', $firstIni, '-AB', $AB, '-CamSeq', $Views, '-OutDir', $out); if (-not $NoPerf) { $argsList += '-Perf' }
$lines = & powershell @argsList 2>&1 | % { "$_" }; $lines | Set-Content "$out\harness_output.txt" -Encoding utf8
$varNames = @($AB -split ';' | ? { $_.Trim() } | % { ($_ -split '=', 2)[0].Trim() }); $viewNames = @($Views -split ';' | ? { $_.Trim() } | % { ($_ -split '=', 2)[0].Trim() })
$perf = @{}; foreach ($l in $lines) { if ($l -match '^PERF\[(\w+)\] (.*)$') { $perf[$Matches[1]] = $Matches[2] } }
$err = @($lines | ? { $_ -match 'FAILED|LOCK TIMEOUT|FOCUS LOST|LOG ERRORS|!!!' })
# contact sheet (rows = views, columns = variants) + Ty strip
$tw = 480; $th = 270; $sheetOk = 0
$sheet = New-Object Drawing.Bitmap ($tw * [math]::Max(1, $varNames.Count)), ($th * [math]::Max(1, $viewNames.Count)); $g = [Drawing.Graphics]::FromImage($sheet); $g.InterpolationMode = 'HighQualityBicubic'; $f = New-Object Drawing.Font 'Arial', 11
for ($r = 0; $r -lt $viewNames.Count; $r++) { for ($c = 0; $c -lt $varNames.Count; $c++) { $p = "$out\$($varNames[$c])__$($viewNames[$r]).png"; if (Test-Path $p) { $img = [Drawing.Image]::FromFile($p); $g.DrawImage($img, $c * $tw, $r * $th, $tw, $th); $img.Dispose(); $sheetOk++ }
        $g.FillRectangle([Drawing.Brushes]::Black, $c * $tw, $r * $th, 230, 18); $g.DrawString("$($varNames[$c]) / $($viewNames[$r])", $f, [Drawing.Brushes]::White, $c * $tw + 2, $r * $th + 1) } }
$sheet.Save("$out\sheet.png"); $g.Dispose(); $sheet.Dispose()
$strip = New-Object Drawing.Bitmap (350 * [math]::Max(1, $varNames.Count)), 460; $g2 = [Drawing.Graphics]::FromImage($strip); $metrics = [ordered]@{}
for ($c = 0; $c -lt $varNames.Count; $c++) { $p = "$out\$($varNames[$c])__default.png"; if (Test-Path $p) { $img = [Drawing.Image]::FromFile($p); $g2.DrawImage($img, (New-Object Drawing.Rectangle ($c * 350), 0, 350, 460), (New-Object Drawing.Rectangle 1000, 560, 560, 736), 'Pixel'); $img.Dispose()
        $metrics[$varNames[$c]] = [ordered]@{ rock_sat = [math]::Round([ImgM]::MeanSat($p, 1100, 80, 1200, 500), 3); rock_luma = [math]::Round([ImgM]::MeanLuma($p, 1100, 80, 1200, 500), 1) } }
    $g2.FillRectangle([Drawing.Brushes]::Black, $c * 350, 0, 140, 18); $g2.DrawString($varNames[$c], $f, [Drawing.Brushes]::White, $c * 350 + 2, 1) }
$strip.Save("$out\ty_strip.png"); $g2.Dispose(); $strip.Dispose()
$man = [ordered]@{ name = $Name; time = (Get-Date).ToString('s'); dll = $Dll; dll_sha1 = (Get-FileHash $Dll -Algorithm SHA1).Hash.Substring(0, 10); variants = $varNames; views = $viewNames; perf = $perf; metrics = $metrics; errors = $err; images_found = $sheetOk; images_expected = ($varNames.Count * $viewNames.Count); dir = $out }
($man | ConvertTo-Json -Depth 6) | Set-Content "$out\manifest.json" -Encoding utf8
$ledger = 'C:\CLAUDE\RESEARCH\results\ledger.csv'; if (-not (Test-Path $ledger)) { 'time,name,dll_sha1,images,errors,perf' | Set-Content $ledger -Encoding utf8 }
Add-Content $ledger ('{0},{1},{2},{3}/{4},{5},"{6}"' -f $man.time, $Name, $man.dll_sha1, $sheetOk, $man.images_expected, $err.Count, (($perf.GetEnumerator() | % { "$($_.Key): $($_.Value)" }) -join ' || ')) -Encoding utf8
$summary = "suite $Name : images $sheetOk/$($man.images_expected), errors $($err.Count). " + (($perf.GetEnumerator() | % { "$($_.Key) => $($_.Value)" }) -join ' | ')
if (-not $NoSync) { & powershell -NoProfile -File C:\CLAUDE\TOOLS\steplog.ps1 -Agent suite -Msg $summary -Files "$out\manifest.json,$out\harness_output.txt" | Out-Null }
$summary; "sheet: $out\sheet.png  ty strip: $out\ty_strip.png"

# Mean GPU ms per variant from the [gpu] / [gpusub] lines in <run>\<variant>\tyrender.log, with the delta against a baseline variant.
# Usage: powershell -NoProfile -File tools\playtest\gpu_summary.ps1 -Run C:\CLAUDE\RESEARCH\autoplaytest\<time> -Baseline cost_base
param([Parameter(Mandatory)][string]$Run, [string]$Baseline = 'cost_base')
$rows = @()
foreach ($d in Get-ChildItem $Run -Directory) {
    $log = Join-Path $d.FullName 'tyrender.log'; if (-not (Test-Path $log)) { continue }
    $t = Get-Content $log
    $gpu = $t | Select-String '^\[gpu\] frame ([\d.]+) ms \| ao ([\d.]+) gi ([\d.]+) atmos ([\d.]+) post ([\d.]+)'
    $sub = $t | Select-String '\[gpusub\] .*\| emit ([\d.]+)'
    if (-not $gpu) { continue }
    function Avg($m, $i) { ($m | ForEach-Object { [double]$_.Matches[0].Groups[$i].Value } | Measure-Object -Average).Average }
    $rows += [pscustomobject]@{ Variant = $d.Name; Samples = @($gpu).Count; FrameMs = [math]::Round((Avg $gpu 1), 2); AoMs = [math]::Round((Avg $gpu 2), 2); GiMs = [math]::Round((Avg $gpu 3), 2); AtmosMs = [math]::Round((Avg $gpu 4), 2); PostMs = [math]::Round((Avg $gpu 5), 2); EmitMs = if ($sub) { [math]::Round((Avg $sub 1), 3) } else { $null } }
}
$base = $rows | Where-Object Variant -eq $Baseline | Select-Object -First 1
$rows | ForEach-Object { $_ | Add-Member DeltaFrame $(if ($base) { [math]::Round($_.FrameMs - $base.FrameMs, 2) } else { $null }) -PassThru } | Format-Table -AutoSize | Out-String -Width 200

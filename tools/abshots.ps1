# Community A/B shots (zero tokens): one fresh game session per preset (restart-only keys differ), identical camera views, 2560x1440 PNGs, saved as
#   RESEARCH\AB_shots\<levelname>\<view>\{vanilla,low,medium,ultra}.png   (single images, same size/camera, for a swipe/compare tool)
#   powershell -NoProfile -File abshots.ps1 -Level 1 [-Slot 2] [-Name level1_rainbow_cliffs] [-Views "default=;..."]
# vanilla = tyrender loaded with every effect and shader patch OFF (engine-only look).
param([int]$Slot = 1, [string]$Name = 'level1_rainbow_cliffs', [string]$Views = 'default=;sand_wide=0 0 450 0 12;fence_mid=-250 -40 350 -35 12;fence_close=300 -40 350 35 12;sand_graze=-100 -60 700 -15 12;vista=0 -60 300 -50 8;rope_b=-60 0 400 25 6;ty_feet=0 -60 150 0 15;wide_left=0 -20 300 -70 6;wide_right=0 -20 300 70 6', [string]$Dll = 'C:\CLAUDE\SRC\tyrender\out\tyrender.dll')
$res = 'C:\CLAUDE\RESEARCH\results'; $inis = "$res\inis"; $dest = "C:\CLAUDE\RESEARCH\AB_shots\$Name"; New-Item -ItemType Directory -Force $dest | Out-Null
$vanilla = "[ao]`r`nEnable=0`r`n[gi]`r`nEnable=0`r`n[atmos]`r`nEnable=0`r`nContactShadows=0`r`n[tess]`r`nEnable=0`r`n[shadows]`r`nPatch=0`r`nMapScale=1`r`n[post]`r`nEnable=0`r`nAA=0`r`n[pbr]`r`nEnable=0`r`n[light]`r`nEnable=0`r`n"
[IO.File]::WriteAllText("$inis\ab_vanilla.ini", $vanilla, (New-Object Text.UTF8Encoding $false))
$sets = [ordered]@{ vanilla = "$inis\ab_vanilla.ini"; low = "$inis\preset_Low.ini"; medium = "$inis\preset_Balanced.ini"; ultra = "$inis\preset_Ultra.ini" }
$env:TY_SLOT = "$Slot"; $log = @()
foreach ($k in $sets.Keys) {
    $o = & powershell -NoProfile -File C:\CLAUDE\TOOLS\suite.ps1 -Name "ab_${Name}_$k" -Dll $Dll -AB "$k=$($sets[$k])" -Views $Views -NoPerf 2>&1 | Out-String
    $n = 0; foreach ($v in ($Views -split ';' | ? { $_.Trim() })) { $vn = ($v -split '=', 2)[0].Trim(); $src = "$res\ab_${Name}_$k\${k}__$vn.png"
        if (Test-Path $src) { New-Item -ItemType Directory -Force "$dest\$vn" | Out-Null; Copy-Item $src "$dest\$vn\$k.png" -Force; $n++ } }
    $log += "$k : $n images"
}
$env:TY_SLOT = ''
$md = "# A/B shots $Name`n" + (($log | % { "- $_" }) -join "`n") + "`nFolder per view; files vanilla.png low.png medium.png ultra.png (2560x1440, same camera).`n"; [IO.File]::WriteAllText("$dest\README.md", $md, (New-Object Text.UTF8Encoding $false)); $md

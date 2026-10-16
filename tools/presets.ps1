# Preset/candidate ini builders shared by perf_matrix.ps1 and balanced_search.ps1 (dot-source). Keys follow the merged build: [ao] Legacy Slices Steps; [gi] Mode Rays; [atmos] Froxel Steps ContactShadows;
# [tess] Enable Scope MaxLevel; [shadows] Patch MapScale Taps; [post] AA; [pbr] Enable. Everything else keeps the DLL defaults.
function NewIni([hashtable]$o) {
    $t = ''
    $t += "[ao]`nEnable=$($o.ao)`nLegacy=0`nSlices=$($o.aoSlices)`nSteps=$($o.aoSteps)`n"
    $t += "[gi]`nEnable=$($o.gi)`nMode=1`nRays=$($o.giRays)`n"
    $t += "[atmos]`nEnable=$($o.atmos)`nFroxel=1`nSteps=$($o.atmosSteps)`nContactShadows=$($o.contact)`nIntensity=$(if($o.atmosInt){$o.atmosInt}else{1.2})`nDesaturation=0.3`nHazeLift=0.15`n"
    $t += "[tess]`nEnable=$($o.tess)`nScope=$($o.tessScope)`nMaxLevel=$($o.tessLevel)`n"
    $t += "[shadows]`nPatch=1`nMapScale=$($o.shMap)`nTaps=$($o.shTaps)`n"
    $t += "[post]`nEnable=1`nAA=$($o.aa)`nBrightness=$(if($o.bright){$o.bright}else{0.94})`nSaturation=$(if($o.sat){$o.sat}else{1.18})`n[pbr]`nEnable=1`nDetailStrength=1.0`nDetailScale=1.0`n[light]`nEnable=1`n"
    $t
}
$Presets = [ordered]@{
    Low    = @{ bright = 1.0; sat = 1.03; atmosInt = 2.5; ao = 0; aoSlices = 2; aoSteps = 4; gi = 0; giRays = 1; atmos = 0; atmosSteps = 12; contact = 0;    tess = 0; tessScope = 'skin'; tessLevel = 4;  shMap = 1; shTaps = 8;  aa = 1 }
    Medium = @{ ao = 1; aoSlices = 2; aoSteps = 4; gi = 0; giRays = 1; atmos = 1; atmosSteps = 16; contact = 0; tess = 1; tessScope = 'skin'; tessLevel = 8;  shMap = 2; shTaps = 12; aa = 2 }
    High   = @{ ao = 1; aoSlices = 3; aoSteps = 6; gi = 1; giRays = 1; atmos = 1; atmosSteps = 24; contact = 0.35; tess = 1; tessScope = 'skin'; tessLevel = 8; shMap = 2; shTaps = 16; aa = 2 }
    Ultra  = @{ ao = 1; aoSlices = 4; aoSteps = 8; gi = 1; giRays = 2; atmos = 1; atmosSteps = 32; atmosInt = 1.0; contact = 0.35; tess = 1; tessScope = 'all';  tessLevel = 24; shMap = 4; shTaps = 32; aa = 2 }
}
# Balanced candidates, highest quality first; balanced_search.ps1 takes the first one that fits the 60 fps budget (<= 16.7 ms at the default view).
$BalancedCandidates = [ordered]@{
    B1 = @{ ao = 1; aoSlices = 3; aoSteps = 6; gi = 1; giRays = 1; atmos = 1; atmosSteps = 24; contact = 0; tess = 1; tessScope = 'all';  tessLevel = 12; shMap = 2; shTaps = 16; aa = 2 }
    B2 = @{ ao = 1; aoSlices = 3; aoSteps = 6; gi = 0; giRays = 1; atmos = 1; atmosSteps = 24; contact = 0; tess = 1; tessScope = 'all';  tessLevel = 12; shMap = 2; shTaps = 16; aa = 2 }
    B3 = @{ ao = 1; aoSlices = 3; aoSteps = 6; gi = 0; giRays = 1; atmos = 1; atmosSteps = 16; contact = 0; tess = 1; tessScope = 'all';  tessLevel = 8;  shMap = 2; shTaps = 12; aa = 2 }
    C1 = @{ ao = 1; aoSlices = 3; aoSteps = 6; gi = 0; giRays = 1; atmos = 1; atmosSteps = 24; atmosInt = 1.2; contact = 0; tess = 1; tessScope = 'skin'; tessLevel = 8;  shMap = 2; shTaps = 16; aa = 2 }
    C2 = @{ ao = 1; aoSlices = 3; aoSteps = 6; gi = 0; giRays = 1; atmos = 1; atmosSteps = 16; contact = 0; tess = 1; tessScope = 'skin'; tessLevel = 8;  shMap = 2; shTaps = 12; aa = 2 }
    B4 = @{ ao = 1; aoSlices = 2; aoSteps = 4; gi = 0; giRays = 1; atmos = 1; atmosSteps = 16; contact = 0; tess = 1; tessScope = 'skin'; tessLevel = 8;  shMap = 2; shTaps = 12; aa = 2 }
    B5 = @{ ao = 1; aoSlices = 2; aoSteps = 4; gi = 0; giRays = 1; atmos = 1; atmosSteps = 12; contact = 0;    tess = 1; tessScope = 'skin'; tessLevel = 8;  shMap = 1; shTaps = 8;  aa = 2 }
}



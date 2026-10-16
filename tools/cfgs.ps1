# Builds a tyrender.ini with only the selected feature groups on, other keys as given. Usage: . cfgs.ps1; Cfg @{atmos=12}
function Cfg([hashtable]$on) {
  $d = @{ pbr = 0; ao = 0; atmos = 0; tess = 0; post = 0; shadows = 0; gi = 0 }
  $t = "[pbr]`nEnable=$(if($on.ContainsKey('pbr')){1}else{0})`n[ao]`nEnable=$(if($on.ContainsKey('ao')){1}else{0})`nSlices=4`nSteps=8`n"
  $t += "[atmos]`nEnable=$(if($on.ContainsKey('atmos')){1}else{0})`nSteps=$(if($on.atmos){$on.atmos}else{48})`nContactShadows=$(if($on.ContainsKey('atmos')){0.35}else{0})`n"
  $t += "[tess]`nEnable=$(if($on.ContainsKey('tess')){1}else{0})`nScope=all`nMaxLevel=$(if($on.tess){$on.tess}else{24})`nDisplacement=2.52`n"
  $t += "[post]`nEnable=$(if($on.ContainsKey('post')){1}else{0})`nAA=2`n[shadows]`nPatch=$(if($on.ContainsKey('shadows')){1}else{0})`nMapScale=$(if($on.shadows){$on.shadows}else{4})`nTaps=32`n"
  $t += "[gi]`nEnable=$(if($on.ContainsKey('gi')){1}else{0})`nRays=$(if($on.gi){$on.gi}else{8})`n"
  $t
}

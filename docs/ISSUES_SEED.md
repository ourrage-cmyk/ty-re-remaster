# Seed issues (one ticket per problem; one PR per fix). Labels: bug, enhancement, research, perf, tooling. Milestone: v0.3.5.
Format: `#id title [labels] - acceptance`. Internal ids (T..) refer to todo.json; evidence in docs/recording/FINDINGS_ROUND*.md.

1. AO: noisy, dark cloud behind the character, shimmers at distance [bug, perf] (T73/T90) - distance fade slider (FadeStart/FadeEnd), character not an AO occluder, blue-noise + history clamp; far mountains no shimmer at High/Ultra, Medium +<=1 ms.
2. TAA: trails behind fast-moving objects, grass ghosting [bug] (T91) - depth-guided history reject + velocity-adaptive blend; no trails on fast Ty turns.
3. Fog colour changes with camera direction [bug] (T92) - fog hue constant under yaw sweep (suspect HazeSky 1x1 sky estimate / GI feeding froxel).
4. Fog inconsistent: Billabong distant trees look in front of the fog, water less fogged than rocks, cave fire not hazed [bug] (T92) - alpha-tested/transparent/emissive draws fogged by depth; AO/shadow fade by transmittance.
5. Water makes the character flicker blue ("disco ball") even on Medium (GI off) [bug] (C1) - isolate hemisphere/specular/engine reflection; Ty stable in the slot-2 pond scene.
6. High and Ultra flicker (Medium stable) [bug, perf] (T78/C2) - bisect GI vs contact shadows, then Ultra extras at low-fps emulation; decide Ultra preset.
7. High GI cost +2.5 ms (mask/blur) [perf] - <= +1 ms vs v0.3.2 without bringing back flicker.
8. Footprints glow (near-white decals read emissive) [bug] (T77/C4) - tag-based luminance clamp.
9. Portal/crystal do not light the ground [enhancement] (T56) - half-res additive spill (design in docs/agent-tasks/light*.md); v0.4 unless trivial.
10. Distance LOD for all effects [perf] (T76) - smooth falloff (tess/relief, AO, shadow taps, fog steps, GI rays); Medium <= 10 ms, no popping.
11. Shadows: confirm checkerboard/dotted line fixed (textureLod fix in v0.3.4) incl. Ultra MapScale=4 [bug] - verification only.
12. In-game overlay menu on an unused hotkey (live settings) [enhancement, tooling] (D1).
13. Launcher vs in-game settings clash guard (single source of truth = tyrender.ini; read game settings.ini SSAO/MSAA) [tooling] (D2).
14. A/B screenshot hotkey (shaders on/off + capture) [enhancement] - wish, last.
15. Research: world-space alternatives for AO/GI/fog colour; technique ideas from docs/results (shader-pack study; licence: ideas only, no code copy) [research] (A1/A4).
16. Portability: NVIDIA/Intel/Proton tyrender.log reports; vendor-guard audit of GL calls [research] (T50).
17. Release v0.3.5: re-measure preset ladder, G1 gate (soak 10 min + 4 cams + forced fallback), zip, community A/B grids (Medium first) [release].

# Workflow
- Branch per ticket: `fix/<id>-<slug>` from `main`; agents work in their own folders, the lead applies the merged change to the branch (repo_sync.ps1 mirrors the working source), commits, pushes and opens a PR: "Closes #<id>" + the PR template evidence.
- A release = tag `v0.3.5` on main + zip attached (zips are never committed).

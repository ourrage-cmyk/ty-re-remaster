# VERDICTS (lead review T07, 2026-10-13 00:30) - evidence: results\v_atmos, v_gi, v_ao, v_atmos_r1/r2, ref_master (in-session A/B, stock frame 4.76 ms, 2560x1440)

| agent | measured (total frame delta over stock) | visual / metric verdict | decision |
|---|---|---|---|
| atmos (froxel) | froxel +8.2 ms (pass 3.9); raymarch +8.8 (pass 4.3); ~4 ms is HIDDEN outside the pass | far-rock saturation 0.85 -> 0.50 (-41%), smooth gradients; raymarch shows posterised blotches on far rock; near sand/fence/Ty unchanged; Ty intact in 3 more fresh sessions (r1,r2) | GO on quality (goal "far rocks less saturated" met). Cost too high: attribute the hidden 4 ms (T31) |
| ao v2 (XeGTAO-style) | v2 +1.4 ms vs legacy +11.0 ms | DEFECT: close grazing view (sand_graze) has a large dark smear at the bottom of the frame (MAD 26 vs legacy 1.4), vista shifts (MAD 5); sand_wide/fence_mid fine | FIX near-field darkening before merge (T11); keep legacy as fallback |
| gi hiz | hiz1 +4.1, hiz2 +4.8, hiz4 +6.1 vs legacy4 +11.6 | GI changes the image by only 0.8-3.1/255 (shade luma +0.6): nearly invisible at the tuned strength; hiz variants sit as far from legacy4 as legacy4 sits from off | LOW VALUE FOR COST: keep Hi-Z (1 ray) as optional/Ultra, default OFF in Balanced; stop optimising a-trous; only increase strength if the user wants visible bounce (user decision) |
| tess, light | not yet verified (restart-only toggles / agent-specific A/B) | tess: seam gap + jaggy paint edges open; light: new filter slower than old (+5.7 vs +1.6 ms) | agent follow-ups T12, T13 |
| infra | ref_master: off 4.75 ms, Ultra 40.0 ms (ao 4.5, gi 9.5, atmos 3.4 in-pass; engine+rest 21.8) | T02 images fine (summary bug fixed) | done |

Observations that change the plan:
1. Every full-res effect pays 2-5 ms OUTSIDE its own pass (atmos +4.3, GI +2.6..6): likely FB0 8x-MSAA resolve copies + full-res composites writing the MSAA surface once per pass. Biggest lever for the 60 fps goal -> after merges, one agent hunts it on the merged tree (T32): single colour resolve shared by all passes, one combined composite.
2. Cross-session image metrics cannot judge characters; use in-session A/B + ty_strip.
3. Visible-effect size matters: AO/GI at current strengths change the picture by only ~1-3/255; the director should weigh cost vs visibility.
## Correction (00:25, T31)
PERF in suites T03-T05/ref_master was sampled at the LAST view of each variant (vista = heavier scene), so their ABSOLUTE ms are inflated; comparisons inside one suite remain valid. Fixed: agent_test -AB now measures perf at the default camera. At the default view (stock 5.38 ms): atmosphere = contact shadows only +2.74 (pass 1.21), froxel fog without contact +2.12 (pass 0.82), froxel + contact +3.69 (pass 1.69). So the atmosphere's real cost is ~3.7 ms, 2/3 of it contact shadows; hidden-cost share ~1.5-2 ms.

## Standard-view numbers (default camera, in-session A/B, stock = 5.38 ms; suites p_ref, p_ao, p_gi, v_atmos_cost) - USE THESE
| effect | total frame delta |
|---|---|
| AO v2 (XeGTAO-style) | +1.42 ms |
| AO legacy | +5.76 |
| GI hiz1 / hiz2 / hiz4 | +4.68 / +5.50 / +7.13 |
| GI legacy 4 rays | +16.94 |
| Atmosphere froxel (fog + contact shadows) | +3.69 (contact shadows alone +2.74, fog alone +2.12) |
| Ultra baseline (old settings: 8192 shadows, tess L24, GI 8 rays, AO 4x8, atmos 48) | +49.49 (54.87 ms = 18 fps) |
Budget for 60 fps at 1440p: 16.7 - 5.38 = 11.3 ms for all effects (post/TAA 0.8 included). AO v2 + atmos = 5.1 ms leaves ~6 ms for shadows + tessellation + (GI hiz1 4.7 does not fit with them).
## Round 2 (00:55): tessellation + lighting, in-session-comparable numbers at the default view (stock 5.38 ms)
| item | cost | visual verdict |
|---|---|---|
| tess v4 (Scope=all, relief+PBR extras) | +4.08 ms (target <= 3) | 95 programs tessellated, 0 failures, feet fine (no sinking) BUT the effect is nearly INVISIBLE: MAD(off,on) 1.4-2.9/255; 100% crop of the cracked sand identical on/off. ROOT CAUSE: the level ground texture is very low-resolution (visible 4-8 px blocks) because the HD pack does not replace level textures; relief derived from it can only be blocky. |
| light: old filter 4096x16 | +1.66 ms | reference |
| light: new filter + [light] on | +1.06 ms (cheaper than old) | no stipple visible; tint/hemisphere almost invisible (luma -4/255 on default, saturation -0.012); new ~ notint ~ old at this scale |
| light: new filter, [light] off | +1.55 ms | |
Pattern across AO, GI, relief, light: each changes the picture by only ~1-3/255. Only the atmosphere is clearly visible. The user's wishes are VISIBLE results (sand profile, natural light), so tasks T12/T13 now require a minimum visible effect (MAD vs off >= 6 on the relevant views, evident in 100% crops) within the cost caps.
## Round 3 (light agent, suite light_final2, in-session A/B)
light: GO. Shadows on fence/sand change from grey-brown to a clearly cooler red-violet and lighter (sheet.png), Ty and NPC intact, cost ~0 ms on top of the new filter (+1.06 vs stock at 4096x16), luma within 2.1/255, rock_sat 0.843 -> 0.806 (-4%, red rock slightly less saturated). Known: sunlit surfaces are not warmed; ShadowFill > 0.05 washes out the red rock. Merge: tyrender.cpp changes in SRC\agents\light\tyrender_cpp.diff. A/B pitfall: the first variant's ini decides whether shaders are rewritten -> put the Enable=1 variant first.
atmos: ACCEPTED as-is (froxel; real vista A/B now exists in v_atmos; defaults tuned at preset time by the lead). No further agent work (T09 closed).

ao: GO. Smear fixed (one-line depth-mip lod bug); verified by the lead from ao_fix3 images: sand_graze MAD(off,v2)=1.52 (was 26.0), MAD(legacy,v2)=0.50, bottom-strip luma equal (62.9 vs 63.0), cost +1.44 ms (legacy +5.75). Open: thin occluders (fence/rope/grass) and shimmer under motion not yet re-checked -> check on the merged build.

## Round 4
tess: GO (with caveats). The 100% crop results\tess_d8\crop_sand_close.png (left off, right on) shows the cracked sand turning from blocky blur into real pebbled grain with crevice shading: the user's 'profile/roughness' wish is now visible. Mechanism: the vertex-lit sand/fence shaders (LIT=0, SHADOWNORMS=1) were never touched by PBR; they now get a world-space detail layer (bump + relight) driven by albedo. Cost: default (Scope=skin + detail) +0.3 ms; Scope=all world tessellation ~+3.9 ms for little extra, optional. Defaults to judge/tune on the merged build (DetailStrength). Open: rope close-up unverified, dark speckle on rug paint, slight rail stipple, Scope=all seam, feet checked FS-only (geometry unmoved).
merge staging: atmos, ao, light merged clean (three-way tyrender.cpp clean x2); forced-fallback mechanism validated (AO->legacy, froxel->raymarch, light->shadow-patch-alone).

## G1 gate (2026-10-13, shipped dll D5FB7DA039)
soak 10 min OK; Balanced default 11.12 / vista 9.91 / sand_graze 10.06 / fence_mid 9.70 ms (all <= 16.7); forced fallback 6.94 ms. Open: motion shimmer (T47), atmos intensity (T48), rope view (T49), other GPUs.

## v0.3.1 (recording round) G1 on 839C01C3AD
soak OK; default 11.15 / vista 9.85 / sand_graze 9.90 / fence_mid 9.61; forced fallback 6.86. Open: rug smear grazing, ground smoother/streaky, far wall pink-white, sign/rope/gate/steel/koala-machine unchecked, portal spill unbuilt.

## v0.3.2 G1 gate (2026-10-13, dll sha1 6571393575) - PASSED
soak 10 min OK; default 12.08 / vista 10.45 / sand_graze 10.47 / fence_mid 10.21 ms (Medium preset, 2560x1440, RX 7600 XT); forced fallback 7.44 ms. Playtest 3 on v0.3.2: no flicker reported; AO, fog and light praised; open bugs in recording\FINDINGS_ROUND3.md. Formal motion metric (T47), rope crop (T49) still open.

## v0.3.4 G1 (2026-10-13 evening, dll 3EE5DA28A7)
perf default 10.73 / vista 9.68 / sand_graze 9.76 / fence_mid 9.50 ms, forced fallback 5.96; soak 10 min OK on rerun (first run: clean exit at 9 min when random walk left the level).

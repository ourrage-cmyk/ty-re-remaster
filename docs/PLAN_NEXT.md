# Plan going forward (updated 2026-10-13 19:23, after playtest 4 and the v0.3.3 test build)
Status: v0.3.2 shipped as preview (G1 passed). v0.3.3 test build packaged and installed in the game folder (G1 not run). Playtest 4: Medium stable, High and Ultra flicker, water flicker, shadow bright lines, footprints, portal light, fire haze. The user has not yet played v0.3.3. Current state: `RESUME.md`.

## Batch 1 - stability + the visible bugs (progress)
1. **T78 High/Ultra flicker** - PARTLY DONE in v0.3.3: GI source mask (`[gi] ExcludeSources=3`), GI blur/noise (`SourceBlur 0.15`, `NoiseCycle 8`, `TemporalBlend 0.85`). Contact shadows only +0.035 flicker in the static test. Static tests cannot reproduce the flicker. **Remaining:** user playtest of High and Ultra on v0.3.3; if still flickering, bisect with a low-fps emulated walk (`motion.req sleepMs`) per setting (GI rays, contact shadows, Scope=all relief, AO wide pass). If a setting cannot be made stable, change the Ultra preset (not the code) and say so.
2. **T73 AO** - NOT STARTED. AO distance-fade slider (strength 1 at Ty, 0 far away, long smooth gradient); character not an AO occluder (capsule mask like GI, fixes the halo behind Ty); white rim on fern shadows.
3. **T74 + T75/T82 shadows and water** - PARTLY DONE. T82 (light) added debug keys and reported done, but the bright lines on the face and bag are NOT fixed (root cause not found). T74 water brightness pulse on Ty (W1): not started; the water blue flicker (GI sampling water) is addressed by the v0.3.3 GI mask, to be confirmed by playtest.

## Batch 2 - speed and polish
4. **T76 distance LOD** - NOT STARTED. Smooth distance falloff for tess/relief, AO, shadow taps, fog steps, GI rays; measure savings on Medium (target 10 ms or less) without popping. Also optimise High GI cost (+2.5 ms in v0.3.3).
5. **T77 emissive from texture tags** - MOSTLY DONE (T80: texture-name hook works, 206 of 342 uploads tagged, emissive API and per-draw flags). **Remaining:** footprints still glow (cause unknown; hypothesis: decals blended after the depth copy), portal light spill (T56 / T65, design in `SRC\agents\light\REPORT.md`).
6. **Atmosphere and Billabong** - NOT STARTED. Fire is not hazed (G3, playtest 4), Billabong pink-purple haze (G8), distance trees.

## Batch 3 - release
7. User playtest of v0.3.3 first. Then merge fixes (`merge_agent.ps1`), G1 gate (`g1_gate.ps1`: soak + 4 cameras + forced fallback), re-measure the full preset ladder (High and Ultra were not re-measured on v0.3.2), update docs and zip (v0.3.3 final), new A/B shots (`abshots.ps1` + `match_ab.ps1`) for the community post. Check the `dist` ini of the test zip first (see `RESUME.md`, known inconsistencies).
8. Portability: README asks for NVIDIA/Intel/Proton logs; vendor-guard audit of GL calls (T50); optional GL 3.3/4.0 context test.

## How we keep it safe (usage limits)
- Max 3 agents at once, short task files (`agents\tasks\*.md`), every step in step logs and `REPORT.md`; `todo.json` is the board; `RESUME.md` is the state; OpenViking synced after each step (`ovsync_all.ps1 -Quick`, plus single-file sync for `recording\`, `archive\`, `agents\tasks\`).
- Usage limit lesson: 3 agents were cut off mid-work at 17:49 (resets 17:30). Resume by SendMessage to the agent id, or relaunch with a `*4.md` task file, which reads its own step log first.
- Harness runs are single-game; do not touch mouse or keyboard during a game test. Close the launcher in the game folder before a new build is installed.

## Decisions the user still needs to answer (no written answer recorded in these docs)
A. Ultra: until T78 is fixed, use the stable High-style settings plus extras that are not suspects (default proposal: yes, keep the old Ultra as 'Ultra (experimental)' in the ini only). Not applied yet; the v0.3.3 game-folder ini is still Ultra.
B. Priority: batch 1 first (followed in v0.3.3 work) or distance-LOD speedups first?
C. Footprints and emissive: texture names (done, T80) plus a clamp for bright decals from the tagged list, or more work on the decal path?
D. Another GPU: any chance to test on NVIDIA or Intel (even one launch and the `tyrender.log`)?
E. Release cadence: ship v0.3.3 after batch 1 + 2 with one playtest in between, or ship batch 1 alone quickly?

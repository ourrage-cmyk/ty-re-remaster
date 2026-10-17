# Consolidated plan (2026-10-16)

One place for the work order. It joins the sources listed under "Where the detail lives" so nobody has to read eight files to know what happens next. Ticket numbers (#1 to #17) are the GitHub issues on milestone v0.3.5, seeded from `docs/ISSUES_SEED.md`. Internal ids (T.., C.., R.., E.. ..) are the ones in `todo.json` and the playtest findings.

## Where things stand

- **Shipped:** v0.3.2 as preview / community beta. G1 gate passed.
- **Latest build:** v0.3.4 (`SRC\tyrender\dist\TYHD_Renderer_v0.3.4.zip`), installed in the game folder. Its G1 gate passed on rerun: default 10.73 ms, vista 9.68, sand_graze 9.76, fence_mid 9.50, forced fallback 5.96 ms at 2560x1440 on RX 7600 XT. The one soak failure was the random walk leaving the level (clean shutdown), not a crash.
- **v0.3.4 changes:** shadow dotted-line root cause fixed (`textureLod` in the shadow taps), colour match (Medium/High/Ultra look), character draws excluded from GI sources, `TAAReject` on for grass ghosting (unproven), texture-tag system, GI source mask.
- **Not yet verified by the user:** High and Ultra flicker, red on the ground, grass ghosting, checkerboard. The user has not played v0.3.4 yet.
- **Backup:** full OpenViking upload done at 2026-10-13 20:59 (`ovsync_all.ps1` plus `ovsync_extras.ps1`, 465 files). It has to be re-run after this document and the new issues land (see step 1 below).

- **#11 closed (2026-10-16):** the owner playtested v0.3.4 (DLL sha1 3ee5da28a7) on Medium and confirmed the lines across the character shadows are fixed. High and Ultra were not yet playtested; reopen if they show the lines.
- **Ultra/High playtest (2026-10-17, after Medium):** Ultra shows AO flicker and heavy TAA ghosting; High shows the same AO flicker with less ghosting. This confirms #6. AO flicker (#1) and TAA ghosting (#2) are back in batch 1 with the fog colour fix. If a setting cannot be made stable, change the Ultra preset, not the code.
- **Latest playtest (2026-10-17 000242, Medium only):** shadows no flicker; TAA looks good on Medium (no concerns on Medium only); fog colour still changes with camera direction (#3/#4, now priority 1); AO under the character's feet grainy (#1, priority 2). High and Ultra still untested.

## Decisions accepted 2026-10-16

The owner said "agree to all of them" to the defaults below. A: yes, Ultra stays experimental in the ini. B: batch 1 first. C: footprints by texture clamp first. D: other-GPU reports welcome, not blocking. E: ship after batch 1 and 2 with one playtest. Nothing ships without the owner's sign-off.

## Work order


Rules that apply to every item: one PR per fix, branch `fix/<id>-<slug>`, "Closes #<n>" in the body, evidence from a measured run (not a guess), max 3 agents at once, no game test while the user is playing.

### Step 0. Get the user's playtest of v0.3.4 (blocks step 2)
Playtest Medium, High, and Ultra on v0.3.4. Check flicker, the red ground spot, grass ghosting, checkerboard, footprints, and water. Their answers decide the priority of steps 2 and 3.

### Step 1. Keep the record in sync (now)
- Commit this plan and the refreshed issue list to GitHub (this PR).
- Re-run the OpenViking sync on the device for `docs/`, `.github/`, and `todo.json` so the backup matches GitHub.
- Verify counts per folder and spot-check content. Report the counts in the thread.

### Step 2. Batch 1: stability and the visible bugs
| Issue | Work | Done when |
|---|---|---|
| #6 High and Ultra flicker (T78, C2) | Bisect GI vs contact shadows with a low-fps walk (`motion.req sleepMs`). If a setting cannot be made stable, change the Ultra preset and say so. | Playtest shows no flicker on High or Ultra |
| #5 Water makes Ty flicker blue (C1, T74, W1) | Isolate hemisphere, specular, and engine reflection. Water GI side already handled by the v0.3.3 mask. | Ty stable in the slot-2 pond scene |
| #1 AO: dark cloud behind Ty, shimmer (T73/T90) | Distance fade slider (FadeStart/FadeEnd), character not an AO occluder (capsule mask like GI), blue-noise plus history clamp, white rim on fern shadows (R3-F1). | No halo behind Ty; Medium +1 ms at most |
| #2 TAA trails and grass ghosting (T91) | Depth-guided history reject, velocity-adaptive blend. Check `TAAReject` from v0.3.4 first. | No trails on fast Ty turns |
| #11 Shadow checkerboard / dotted line | Closed 2026-10-16 on the owner's Medium playtest of v0.3.4. | Done |

### Step 3. Batch 2: speed and look
| Issue | Work | Done when |
|---|---|---|
| #10 Distance LOD for all effects (T76) | Smooth falloff for tess/relief, AO, shadow taps, fog steps, and GI rays. | Medium 10 ms or less, no popping |
| #7 High GI cost +2.5 ms | Optimise the mask and blur cost. | Within +1 ms of v0.3.2, no flicker returns |
| #3 Fog colour changes with camera yaw (T92) | Suspect HazeSky 1x1 sky estimate or the GI feed into the froxel. | Fog hue constant under a yaw sweep |
| #4 Fog inconsistent: Billabong trees, water, cave fire (T92) | Fog alpha-tested, transparent, and emissive draws by depth; AO and shadow fade by transmittance. | Trees and water sit in the fog; fire hazed |
| #8 Footprints glow (T77, C4) | Tag-based luminance clamp for near-white decals (cause unknown; hypothesis: decals blended after the depth copy). | No glow on footprints |
| #9 Portal and crystal do not light the ground (T56) | Half-res additive spill, design in `docs/agent-tasks/light*.md`. v0.4 unless trivial. | Deferred unless trivial |

### Step 4. Release (milestone v0.3.5, #17)
1. Merge fixes via staging (`merge_agent.ps1`).
2. G1 gate on the merged build (`g1_gate.ps1`: 10-minute soak, 4 cameras, forced fallback).
3. Re-measure the full ladder (High and Ultra were not re-measured since v0.3.2).
4. Update `README.md`, `PERFORMANCE.md`, and `STATUS`, and fix the known inconsistencies in `RESUME.md` (zip ini, stale headers).
5. New A/B shots for the community post (`abshots.ps1` plus `match_ab.ps1`).
6. Tag `v0.3.5` on main; zip attached to the release, never committed.

### Release rule (owner, 2026-10-16)
- The release artifact is always **TYLauncher.exe**. It is the primary asset, with its sha1 in the release notes. The launcher embeds the matching `tyrender.dll` and `default_tyrender.ini` (built by `build.ps1`) and installs them next to itself on start, so users only run the exe. An existing `tyrender.ini` is never overwritten.
- The zip is a secondary asset. Zips, DLLs and exes are never committed.
- The launcher title bar shows the version and build id (`TY HD Renderer v0.3.5-rc  build <sha>`, with `-dirty` on a modified tree), so a tester can tell the exe is current.
- Pre-releases (untested in game) are marked as pre-release on GitHub.


### Parallel tracks (do not block the batches)
- #12 In-game overlay menu on an unused hotkey (D1) and #13 launcher vs in-game settings clash guard (D2): tooling, can start any time.
- #14 A/B screenshot hotkey: last.
- #15 Research: world-space alternatives for AO, GI, and fog colour (A1/A4). Ideas only, no code copied from the shader-pack study.
- #16 Portability: NVIDIA, Intel, and Proton `tyrender.log` reports, vendor-guard audit of GL calls (T50). Needs hardware the project does not have yet (decision D below).

## Decisions the user still needs to make
A. **Ultra preset:** until #6 is fixed, use the stable High-style settings and keep the old Ultra as "Ultra (experimental)" in the ini only? (Default proposal: yes.)
B. **Priority:** batch 1 first (default in this plan) or distance-LOD speedups first?
C. **Footprints and emissive:** texture-name tags (done) plus a clamp for bright decals, or more work on the decal path?
D. **Other GPU:** can anyone test on NVIDIA or Intel, even one launch with `tyrender.log`?
E. **Release cadence:** ship after batch 1 and 2 with one playtest in between, or ship batch 1 alone quickly?

## Known inconsistencies to fix before any release
- `SRC\tyrender\default_tyrender.ini` header says "about 90 fps" (old number); Medium is about 12 ms.
- `dist\TYHD_Renderer_v0.3.3\tyrender.ini` in the test zip is not the Ultra test ini; check before any release. Zips are not edited.
- `TOOLS\launcher_shot.ps1`, `soak.ps1`, `balanced_search.ps1` default to `preset_Balanced.ini` (that file is the Medium preset).
- `TOOLS\ty_play.ps1` header mentions save slot GAME 2; the code defaults to slot 1.
- `GOAL.md` still says "Balanced" in acceptance text; read it as Medium.
- `ovsync_all.ps1` skips `recording\`, `archive\`, and `agents\tasks\`; `ovsync_extras.ps1` covers them.

## Where the detail lives
- `docs/PLAN_3DAY_20261016.md`: the 3-day pickup plan (sections 5 to 9 are the fix list).
- `docs/FIX_PLAN_20261016.md`: item-level plan for batches 1 to 3 (files, dependencies, acceptance).
- `docs/research/minecraft_shaderpack_port_ranking.md`: ranked ideas from the shader-pack study (ideas only, no code copied).
- `docs/todo.json`: the task board (ids used above).
- `docs/START_HERE.md`: entry point, commands, rules and traps.
- `docs/RESUME.md`: current state and the dated save-point log (latest entry 2026-10-13 20:59).
- `docs/PLAN_NEXT.md`: the batch order this document refines (last edited 2026-10-13 19:23; the RESUME log is newer).
- `docs/ISSUES_SEED.md`: the seed text for issues #1 to #17.
- `docs/GOAL.md`: acceptance criteria and decisions (append only).
- `docs/STATUS_2026-10-13.md`: measured numbers.
- `docs/recording/INDEX.md` and `FINDINGS_ROUND*.md`: playtest evidence and problem ids.

# Known issues
> Status: documented at commit 4db1d7d; last verified 2026-10-18 (static read of the clone). Audience: B, C, A.

## Purpose
This page is the single list of known defects in the renderer, with the issue number, what the player sees, the cause if known, what was tried, the current status and the evidence. It answers questions Q173 to Q185 and Q192 from the coverage plan. Testers can use it to check whether a symptom is already known. Contributors can use it to find the evidence before changing code.

Status words used here:
- **open**: reported and not fixed in any build the record describes.
- **partly**: a change exists but the record does not show it solved the symptom.
- **closed on Medium**: confirmed fixed on the Medium preset only. High and Ultra not yet confirmed.
- **disputed**: two sources disagree about whether it is fixed. Both are cited.
- **untested**: a change exists on `main` and the CHANGELOG marks it untested in game.

## How it works
Issues are tracked in four places, and the numbers differ between them:
1. **Issue numbers** (`#1` to `#17` are the seed issues on milestone v0.3.5, `docs/ISSUES_SEED.md:4-20`; higher numbers are PRs and issues from the CHANGELOG).
2. **Internal ids** in `docs/todo.json` (`T73`, `T78` and so on) and in the plans (`C1`, `D1`, and so on). Mapping is on [history-and-timeline.md](history-and-timeline.md#internal-ids).
3. **Playtest problem ids** in `docs/recording/INDEX.md` (`F1`-`F12`, `G1`-`G10`, `W1`, `S1`, `A1`, `E1`, `R3-F1`, `R3-F2`, `R4-1` to `R4-9`).
4. **The shipped README** lists known issues for players (`README.md:211-226`). Its list is the player-facing summary.

Where these sources disagree, the table gives the newest source first and marks it.

## Code pointers

| What | file:line | Note |
|---|---|---|
| Water stable mode (drops bump, relief, specular, sky Fresnel on water draws) | `src/pbr.inl:19-20` | `[pbr] WaterStable`, default 1 |
| Water debug bitmask (bisect blue flicker) | `src/pbr.inl:21-25` | `[pbr] DebugWater`, default 0 |
| Water fog mask softening | `src/atmos.inl:93`, `src/atmos.inl:303` | `[fog] WaterMaskSoft`, default 0 |
| Late emitter haze (fire) | `src/atmos.inl:328-329`, `src/atmos.inl:301` | `[fog] LateEmitters` default 0, `LateEmitBoost` default 1.0 |
| See-through depth write | `src/atmos.inl:346-357` | `[fog] SeeThruDepth`, default 0 |
| Overlay key default | `src/overlay.inl:43` | code default "0"; shipped ini F10 (`src/default_tyrender.ini:115`) |
| Clash guard (warn only) | `src/tyrender.cpp:632-659` | `[clash] Check`, default 1 |
| Shadow tap explicit LOD (dotted-line fix) | `src/tyrender.cpp:202` | `textureLod(t,p,0.0)` |
| TAA history reject and motion blend | `src/fx.inl:128-137`, `src/fx.inl:151` | `[post] TAAReject`, default 1.0 |
| Decal luminance clamp (footprints) | `src/tex.inl:72`, `src/default_tyrender.ini:67-69` | `[decal] Clamp`, default 0 |
| Emitter light (portal) | `src/emit.inl:34` | `[emit] Enable`, default 0 |
| AO fade and character mask keys | `src/ao.inl:507-509` | `FadeStart`/`FadeEnd` default 0; `CharRadius` default 40 |
| Character draws excluded from GI sources | `src/gi2.inl:33`, `src/gi.inl:93` | `[gi] ExcludeSources` default 7 |

## Settings
The keys named in this page are documented with defaults and live or restart status in [../reference/settings-atmos-fog-emit.md](../reference/settings-atmos-fog-emit.md), [../reference/settings-post-shadows-light-pbr-tess.md](../reference/settings-post-shadows-light-pbr-tess.md), [../reference/settings-ao-gi-lod.md](../reference/settings-ao-gi-lod.md) and [../reference/settings-system-debug.md](../reference/settings-system-debug.md). Do not change a default to "fix" a known issue without a measured run (see [../guides/debugging.md](../guides/debugging.md)).

## Cost
Issue fixes that add cost are listed with the cost in [failed-and-abandoned.md](failed-and-abandoned.md#cost). The known performance issues are: Ultra about 31 ms per frame on the test card (`README.md:220`), High GI +2.5 ms in v0.3.3 (`docs/RESUME.md:32`, not optimised), and the cost of Scope=all relief (`docs/STATUS_2026-10-13.md:34`).

## Limits
- Most "untested" items have no playtest result. A changed status needs the owner's playtest (`docs/PLAN_NEXT.md:44`, rule "Issues close only after the owner's playtest").
- The clone contains no issue tracker data. Issue titles and states come only from text in the repository.
- Harness runs do not reproduce the High and Ultra flicker (`docs/STATUS_2026-10-13.md:64`). Motion metrics cannot see shadow-edge flicker under TAA jitter (`docs/STATUS_2026-10-13.md:61`).

## Known issues

### Open defects (Q173)

| ID | Issue | Symptom (as reported) | Cause (known or suspected) | Attempted fixes | Status | Evidence |
|---|---|---|---|---|---|---|
| K-01 | #5, W1, R4-2, T74 | Ty's brightness pulses and turns blue in water ("disco ball") | Suspected: hemisphere light, specular, or the engine's water reflection on Ty. Not GI on Medium, where GI is off | `WaterStable=1` (untested); GI source exclusion (v0.3.3, partly); `DebugWater` bisect bits (untested) | open | `CHANGELOG.md:12,21,36`; `docs/PLAN_EXEC.md:11`; `docs/results/R6_config_diff.md:5`; `docs/PLAN_NEXT.md:15,34` |
| K-02 | #4 | Lily pads and rocks look brighter than the water under them; fog heavier when looking down | Suspected: water fog not applied to lily textures. Lily texture names not in `WaterNames`. Not confirmed (colour mask empty in the 2026-10-17 test) | `[fog] WaterBoost` (#74, untested); `[fog] WaterNames` (#106, empty by default) | open | `CHANGELOG.md:26,44`; `docs/PLAN_NEXT.md:11,24`; `docs/PLAYTEST_2026-10-17_FINDINGS.md:145-149` |
| K-03 | #4, G3, R4-4 | Cave fire and emissive effects are not hazed | Emissive draws drawn after the fog pass are not fogged (`src/atmos.inl:328`). Inferred from the code comment | `[fog] LateEmitters` (default 0, untested); `[fog] LateEmitBoost` (#115, default 1.0, untested) | open | `docs/recording/FINDINGS_ROUND4.md:7`; `docs/PLAN_NEXT.md:24`; `src/default_tyrender.ini:82-83` |
| K-04 | #4, T92b | Level 2 distant trees look in front of the fog (not confirmed in the 2026-10-17 frame) | Suspected: alpha-tested foliage does not write depth before the fog pass | `[fog] SeeThruDepth` (default 0, untested). A/B planned | open, unconfirmed | `docs/PLAYTEST_2026-10-17_FINDINGS.md:77-79,140`; `docs/FIX_PLAN_20261016.md:34`; `src/atmos.inl:346-357` |
| K-05 | #4, #58 | Fog on water less than on rocks (rock outlines visible on water) | Suspected: water draws fogged inconsistently | WaterBoost, WaterMaskSoft (#92, default 0) | open (partly) | `CHANGELOG.md:24,44`; `docs/PLAN_R6.md:11` |
| K-06 | #6, T78, R4-1 | High and Ultra flicker; Medium stable | Disputed. 2026-10-13 records: GI and contact shadows. 2026-10-17 record: AO flicker and TAA ghosting. R6-1: all flicker-free runs were Medium | GI blur, noise cycle, temporal blend, source mask (v0.3.3, partly); AO and TAA work is in batch 1 | open | `docs/RESUME.md:28`; `docs/PLAN_CONSOLIDATED.md:14-15`; `docs/results/R6_config_diff.md:6` |
| K-07 | #1, T73, A1, R3-F1, R4-8 | Dark cloud behind Ty (AO halo); AO shimmer on far mountains; white rim on fern shadows | Screen-space AO with the character as an occluder. Suspected: AO sampled from the character depth | `[ao] CharRadius` (default 40, `src/ao.inl:509`); `FadeStart`/`FadeEnd` keys (default 0, off, `src/ao.inl:507-508`); whether a halo fix is effective is not stated | disputed | `docs/PLAN_CONSOLIDATED.md:14,39`; `docs/todo.json` T73 (open); `CHANGELOG.md:50` |
| K-08 | #2, T91, R5 | TAA trails behind fast objects; grass ghosting | Motion-less wind grass and characters: history is the old geometry. Depth-guided reject and motion blend are in `src/fx.inl:128-137` at HEAD, but FIX_PLAN lists them as to-do | `[post] TAAReject` (default 1.0, untested); v0.3.5 "ghosting reduced" | partly | `CHANGELOG.md:51`; `docs/PLAN_CONSOLIDATED.md:14`; `README.md:222`; `src/fx.inl:130-137` |
| K-09 | #3, T92 | Fog colour changes with camera direction (red near red things, white otherwise) | Suspected: 1x1 sky estimate (`HazeSky`) or GI feeding the froxel. Not confirmed | Fog hue fix in v0.3.5 (`CHANGELOG.md:42`, untested after latest change) | disputed | `docs/PLAN_NEXT.md:13`; `docs/PLAN_CONSOLIDATED.md:15,48`; `docs/PLAN_R6.md:10` |
| K-10 | #8, T77, E1, R4-7 | Footprints glow at certain camera positions | Disputed. First hypothesis: decals blended after the depth copy. Later analysis: a bright texture clips to white, which is a shading issue, not a light | Texture tags (v0.3.3); decal luminance clamp (`[decal] Clamp`, default 0, untested) | disputed | `docs/PLAN_NEXT.md:16`; `docs/PLAN_CONSOLIDATED.md:50`; `docs/MATERIAL_CLASSIFICATION_PLAN.md:23,39`; `CHANGELOG.md:57` |
| K-11 | #9, T56, T65, F9, G6, R4-6 | Portal and crystal light does not reach the ground | No spill pass for emitters. Emitter path off by default; a white band appears when on | `[emit] Enable` (off, `CHANGELOG.md:39` white band); design in `docs/agent-tasks/light*.md`; test plan prepared (`docs/EMIT_PORTAL_TEST_PLAN.md`, not run) | open. v0.4 | `docs/FIX_PLAN_20261016.md:37`; `docs/PLAN_NEXT.md:17,35`; `CHANGELOG.md:17,39` |
| K-12 | #61, #84 | Portal emitter spill draws a flat white band and stripes in one view | Not stated. Recorded in the autoplaytest view | Keep `[emit] Enable=0` (default) | open | `CHANGELOG.md:39`; `tools/playtest/plans/current_run.json:8` |
| K-13 | #103 | Sharp shadow line across the bridge near the fire | Not stated | None recorded | open | `README.md:215`; `docs/PLAN_NEXT.md:24` |
| K-14 | #104 | Water outline shows when viewed from above | Not stated | None recorded | open | `README.md:213`; `docs/PLAN_NEXT.md:11` |
| K-15 | #11, S1, R4-3, T75, T82 | Dotted bright line in character shadows; checkerboard | Divergent-branch texture sampling. Fixed by an explicit LOD in v0.3.4 | `textureLod` fix (`src/tyrender.cpp:202`) | closed on Medium (2026-10-16). High and Ultra not confirmed. MapScale=4 not checked | `docs/PLAN_CONSOLIDATED.md:13`; `docs/PLAN_EXEC.md:8`; `docs/RESUME.md:68`; `docs/ISSUES_SEED.md:14` |
| K-16 | #10, T76, R3-F2, R4-9 | Ultra "slow-motion"; High GI +2.5 ms; relief and effects do not fade with distance | Stacked effect cost. Distance LOD keys (`[lod]`) exist and are off by default | `[lod]` keys (off by default, untested); relief fade `[tess] ReliefFadeStart/End` (700/1600) | open | `CHANGELOG.md:13-15`; `docs/RESUME.md:32`; `src/tess.inl:57-58`; `README.md:220` |
| K-17 | R4-5, T80 | Bag not tessellated; some items tessellated, some not | Texture tags; bag texture not verified | Texture tags (v0.3.3, partly) | partly | `docs/recording/INDEX.md` R4-5; `docs/RESUME.md:14` |
| K-18 | RESUME, T84 | Red tint on the ground under Ty's trousers | Culprit not isolated. Red also in sand samples in all variants | Character excluded from GI sources (v0.3.4, partly) | open | `docs/RESUME.md:66`; `docs/todo.json` T84 |
| K-19 | #13 | Launcher and in-game settings can disagree; the game's own SSAO or MSAA doubles effects | Settings in two places | Clash guard, warn only (`src/tyrender.cpp:632-659`) | untested | `CHANGELOG.md:20`; `docs/PLAN_NEXT.md:20` |
| K-20 | #12 | F10 overlay does nothing in build `4a2ef95`; regression in `cda9364` (opening it breaks all geometry until restart and cannot be closed) | Existing ini keeps `Key=0`. Regression cause not stated | Default F10 (#102); focus-free toggle (#54, #59, #63) | open. In-level check pending (N-24) | `README.md:225`; `docs/PLAN_NEXT.md:19,29`; `CHANGELOG.md:11,22` |
| K-21 | #16, T50, N-21 | Behaviour on NVIDIA, Intel and Proton unknown; one GPU tested | Not tested | Vendor-guard audit planned for v0.4 | open | `README.md:230`; `docs/RESUME.md:36`; `docs/todo.json` T50 |
| K-22 | (none) | Shimmer during fast camera moves | Not measured formally | None | open (unmeasured) | `README.md:224`; `docs/todo.json` T47 |
| K-23 | (none) | Ultra costs a lot of GPU time | By design: stacked options | Ultra preset decision (P-A) | open | `CHANGELOG.md:63`; `README.md:220` |
| K-24 | (none) | Trees look dark in thick fog on level 2 | Not stated | None recorded | open | `CHANGELOG.md:63` |
| K-25 | (none) | Textures not emissive (known) | Texture tag coverage | Tags (v0.3.3) | open | `CHANGELOG.md:36` |
| K-26 | (none) | Thin objects (rope, grass) get little relief | Source textures are flat | None | known limit | `README.md:221` |
| K-27 | (none) | Low-resolution textures get weaker relief | Relief comes from texture colour | `[tess] ReliefCurve`/`ReliefKnee` (low priority) | known limit | `README.md:223`; `docs/PLAN_NEXT.md:38` |
| K-28 | (none) | Fern shadows have a white rim | Not stated | Batch 1 in FIX_PLAN (T73) | open | `docs/FIX_PLAN_20261016.md:19`; `README.md:219` |
| K-29 | (none) | Dark halo behind a character (side away from camera) | See K-07 | See K-07 | disputed | `README.md:218` |
| K-30 | (none) | Leaves look translucent through other leaves (G10) | Not reported again; unverified | None | open (unverified) | `docs/recording/INDEX.md` G10 |
| K-31 | (none) | Billabong fog looks like a swamp (G8) | Per-level fog tuning | Fog intensity slider exists | open | `docs/recording/INDEX.md` G8 |
| K-32 | (none) | Far trees grey while the sky is blue (G4) | Haze desaturates toward grey | `[atmos] HazeSky` (0.6); `DepthBlur` option (off) | partly | `docs/recording/INDEX.md` G4; `README.md:294,298` |
| K-33 | (none) | Atmosphere gives a "sunset evening" look (G9) | Haze colour | `[atmos] Intensity` written as 2.2 for Medium (T48 unconfirmed) | partly | `docs/recording/INDEX.md` G9; `docs/RESUME.md:39` |

### Items the record says are fixed, with the evidence level

| Issue | Claim | Evidence level | Source |
|---|---|---|---|
| #3 (fog colour) | "closed" | Playtest of `cda9364` looked steady. Later 2026-10-17 playtest says it still changes (disputed) | `docs/PLAN_NEXT.md:13`; `docs/PLAN_CONSOLIDATED.md:15` |
| #8 (footprints) | "closed" | Owner: "bright white, not a light". Still listed open in the plan | `docs/PLAN_NEXT.md:16`; `docs/PLAN_CONSOLIDATED.md:50` |
| #11 (shadow lines) | closed | Owner playtest on Medium, v0.3.4 | `docs/PLAN_CONSOLIDATED.md:13` |
| #58 (sky and red rock) | fixed, untested | Owner playtest of `4a2ef95` says the sky and red rock fade smoothly (`CHANGELOG.md:35`). CHANGELOG marks the change untested (`CHANGELOG.md:43`) | `CHANGELOG.md:35,43` |
| #1, #6 (AO flicker) | fixed in v0.3.5 | Plan of 2026-10-16 and playtest of 2026-10-17 say flicker persists | `CHANGELOG.md:50`; `docs/PLAN_CONSOLIDATED.md:14` |

## Question answers (Q173 to Q185, Q192)

**Q174 F10 overlay does nothing in `4a2ef95`: cause and status.** Cause: the overlay key is read from `[overlay] Key` (`src/overlay.inl:43`); the code default is "0" (off). The shipped ini sets `Key=F10` (`src/default_tyrender.ini:115`), but an existing `tyrender.ini` keeps its old `Key=0` (`README.md:225`, `CHANGELOG.md:22`). Status: default changed to F10 by #102. In-level check is open (#79, `docs/PLAN_NEXT.md:29`, NEEDS INVESTIGATION N-24). A regression in `cda9364` broke geometry until restart (`docs/PLAN_NEXT.md:19`).

**Q175 Water flicker: causes tried (#5, #62, #92).** #5: `WaterStable` (default 1, `src/pbr.inl:20`) drops bump, relief, specular and sky Fresnel on water-tagged draws; `SkyFresnelSkipWater` is an older subset (default 0, `src/default_tyrender.ini:63`). #62: `DebugWater` bitmask to bisect the blue flicker (`src/pbr.inl:21-25`). #92: `WaterMaskSoft` softens the water fog mask edge (`src/atmos.inl:93,303`, default 0). Status: all untested in game. Cause of the Medium-only pulse not isolated (K-01).

**Q176 Fog inconsistencies (#4, #74, #106, #3, #58).** #4: `SeeThruDepth` (default 0), `LateEmitters` (default 0), `LateEmitBoost` (default 1.0), `WaterNames` (empty). #74: `WaterBoost` fogs water draws (untested). #106: `WaterNames` adds lily-pad names (empty default). #3: fog hue stable as the camera turns (v0.3.5, `CHANGELOG.md:42,49`; disputed, K-09). #58: `[atmos] SkyFogDist` default 0.45 (`src/atmos.inl:251`), untested. Status: partly.

**Q177 Shadow bright lines and checkerboard (S1, T82, #11, #103; `textureLod` fix v0.3.4).** Cause: shadow taps sampled inside divergent branches picked the wrong filter level (`docs/RESUME.md:68`). Fix: explicit LOD 0 in the shadow tap function (`src/tyrender.cpp:202`) and in the atmosphere sun-visibility taps (`src/atmos.inl:35,37`). Status: closed on Medium (K-15). The bridge line (#103) is a separate open item (K-13). Ultra with `MapScale=4` not verified (`docs/ISSUES_SEED.md:14`).

**Q178 AO flicker and halo (#1, #6, T73).** CHANGELOG v0.3.5 says AO flicker and grain are gone, and that AO fades out in thick fog with `FogFadePower` (default 2, `src/ao.inl:514`) (`CHANGELOG.md:50`). The 2026-10-17 playtest says AO flicker persists on High and Ultra (`docs/PLAN_CONSOLIDATED.md:14`). Halo fix: `CharRadius` exists with default 40 (`src/ao.inl:509`), while T73 is still open in todo.json. Status: disputed (K-06, K-07).

**Q179 TAA trails (#2).** CHANGELOG says ghosting reduced (`CHANGELOG.md:51`). The code has a history reject on disocclusion and a depth-guided reject with a motion-based blend (`src/fx.inl:128-137`). The FIX_PLAN lists these as to-do (`docs/FIX_PLAN_20261016.md:20`). The 2026-10-17 playtest says heavy ghosting on Ultra, less on High, and no concerns on Medium (`docs/PLAN_CONSOLIDATED.md:14-15`). Status: partly (K-08).

**Q180 High and Ultra flicker (#6, T78).** Open. The 2026-10-13 records blame GI and contact shadows (`docs/RESUME.md:28`). The 2026-10-17 record blames AO flicker (`docs/PLAN_CONSOLIDATED.md:14`). R6-1 says every flicker-free run was Medium (`docs/results/R6_config_diff.md:6`). See K-06 and Discrepancies.

**Q181 Footprint glow (#8).** Open in the plan (`docs/PLAN_CONSOLIDATED.md:50`), "closed" in PLAN_NEXT for the bright-white finding (`docs/PLAN_NEXT.md:16`). Cause per the material plan: the texture is not emissive but clips to white (`docs/MATERIAL_CLASSIFICATION_PLAN.md:23`). Decal clamp exists and is off (`src/tex.inl:72`). Status: disputed (K-10).

**Q182 Portal and emitter light (#9, #61, #84).** Emitter light is off by default (`src/emit.inl:34`, `src/default_tyrender.ini:107`). With it on, a white band appears (`CHANGELOG.md:39`). Blur (#61) is off by default (`CHANGELOG.md:21`). #84 is a log check in the playtest plan (`tools/playtest/plans/current_run.json:8`). The portal does not light the ground in the 2026-10-17 record, and it is unclear whether `[emit] Enable=1` was set (`docs/PLAN_NEXT.md:17`). Status: open, v0.4 (K-11, K-12).

**Q183 Skybox red rock (#58, playtest 7).** Owner's `4a2ef95` playtest: sky and red rock fade smoothly (`CHANGELOG.md:35`). The `cda9364` record said not fogged (`docs/PLAN_NEXT.md:14`). The 2026-10-17 frame at 9:30 shows a colour difference of 33.4 between the mesa top and the fog at its base, but the record says it is not confirmed as a skybox error rather than distance fog (`docs/PLAYTEST_2026-10-17_FINDINGS.md:86-88,144`). Status: fixed per owner on `4a2ef95`; untested per CHANGELOG.

**Q184 Frozen frames in the recording (game or recording?).** The recording file itself has byte-identical frames from 2:07.5 to 2:15 (`docs/PLAYTEST_2026-10-17_FINDINGS.md:111-114`). Whether the game froze or the capture duplicated frames cannot be told apart from this file. The separate capture file that would settle it is not on disk. Status: NEEDS INVESTIGATION (N-26 below).

**Q185 Trees through fog, level 2 (playtest 5, FIX_PLAN).** The 2026-10-17 frame at 5:40 is a cutscene, and no poke-through is visible there. The record says "not confirmed" (`docs/PLAYTEST_2026-10-17_FINDINGS.md:77-79`). The fix plan item T92b proposes depth writes for alpha above 0.5 (`docs/FIX_PLAN_20261016.md:34`). `[fog] SeeThruDepth` is off by default. Status: unconfirmed (K-04).

**Q192 Status conflicts between documents.** Covered in the Discrepancies below and in [history-and-timeline.md](history-and-timeline.md#discrepancies).

## Build-specific defects from the CHANGELOG
- `[emit] Enable=1` renders a flat white upper band and stripes in the autoplaytest view. Keep it off (`CHANGELOG.md:39`).
- Trees can look slightly dark in thick fog on the second level. Character and grass shadows show a faint checkerboard. Ultra costs a lot of GPU time. The title says `v0.3.5-rc` (`CHANGELOG.md:62-64`).
- Open list from the `4a2ef95` playtest (`CHANGELOG.md:36`): F10 does nothing; slow water ramp on the character; lilies and rocks brighter than the water; heavier fog from the top down; sharp shadow line at the bridge; fog on fire; tree frog still broken (not noticeable); water outline from above; textures not emissive (known).

## Repository hygiene defects
These are not renderer defects but they mislead readers.
- `src/gpuprof.inl:3` says `[debug] GpuProf=0` disables the GPU timers. No code reads a `GpuProf` key (grep finds only the comment), so the switch does nothing.
- `src/tyrender.cpp:1` begins with mojibake (UTF-8 text decoded as another code page). Cosmetic.
- Stale header comments in `src/gi2.inl:1` (claims a legacy fallback, see [failed-and-abandoned.md](failed-and-abandoned.md)).
- `src/atmos.inl:257,260` log text says "ray-march path" for a path that no longer exists.
- `src/package.ps1:2` default version `v0.1`.
- `src/TYLauncher.c:22` and `src/sesslog.inl:16` say `v0.3.5-rc`.
- `src/launcher_ui.inc:46` comment gives preset totals (Medium 6.8 ms, Balanced 5.4 ms) that do not match `docs/STATUS_2026-10-13.md` (Medium about 12.1 ms).
- `README.md:163` links `docs/FINDINGS_ROUND4.md`, which is at `docs/recording/FINDINGS_ROUND4.md`.

## Status conflicts (Q192)
| Topic | Source A | Source B | Note |
|---|---|---|---|
| Current release | `docs/RESUME.md:7`: v0.3.2 shipped | `CHANGELOG.md:46`: v0.3.5 last full release | Dates differ; use the newer |
| Next release name | `docs/FIX_PLAN_20261016.md:54`: v0.3.3 | `docs/PLAN_EXEC.md:7`: v0.3.5 | See decisions-log D-14 |
| Medium frame rate in header | `docs/RESUME.md:57`: header says about 90 fps | `src/default_tyrender.ini:1`: about 80 fps | The repository header is already corrected |
| Shipped `[gi] Enable` | `src/gi.inl:91` default 1 | `src/default_tyrender.ini:8` Enable=0 | Intentional; the README table says so (`README.md:284`) |
| Tessellation scope default | `src/tess.inl:36` env-detail | `src/default_tyrender.ini:43` skin | Same behaviour (no environment tessellation) |
| Overlay key default | `src/overlay.inl:43` "0" | `src/default_tyrender.ini:115` F10 | Shipped default for new ini files only |
| `[gi] ExcludeSources` default | `src/gi.inl:93` 7 | `docs/RESUME.md:15`: "ExcludeSources=3" | v0.3.3 test build value vs code default |
| Dead link | `README.md:163` | `docs/recording/FINDINGS_ROUND4.md` | Path differs |
| Depth-guided TAA reject | `docs/PLAN_EXEC.md:10`: "Missing" | `src/fx.inl:134-137`: present | Code wins; effect unverified in game |
| AO fade keys | `docs/PLAN_EXEC.md:9`: "Missing: distance-fade slider" | `src/ao.inl:507-508`: keys exist, default 0 | No launcher slider found by grep |

## NEEDS INVESTIGATION
- **N-24 F10 overlay in a level.** Method: `tools/playtest` overlay check (`docs/AUTOPLAYTEST.md`, "Overlay check") with `[overlay] Key=F10`; read the `[overlay]` lines in the report.
- **N-26 Frozen frames at 2:07 to 2:15 (Q184).** Method: obtain the separate 57-second game capture the record mentions (`docs/PLAYTEST_2026-10-17_FINDINGS.md:113`). It is not in the repository or on the disk used for this review.
- **N-27 Water-pulse cause (K-01).** Method: `[pbr] DebugWater` bits 1, 2, 4 and 8, one per playtest, at the slot-2 pond scene (`docs/PLAN_CONSOLIDATED.md:38`).
- **N-28 Footprint cause (K-10).** Method: a short clip at the footprint spot with the decal clamp on and off (`[decal] Clamp`), then a measured luminance check.
- **N-29 Trees through fog in level 2 (K-04).** Method: level 2 footage at a point where trees are outdoors (`docs/PLAYTEST_2026-10-17_FINDINGS.md:125-140` shows the scan).
- **N-30 Lily pads (K-02).** Method: a fog-compensated colour mask or a manual pad mask (`docs/PLAYTEST_2026-10-17_FINDINGS.md:173`).
- **N-31 Which AO and TAA changes are on in the current build (K-07, K-08).** Method: run `motion.ps1` on a build with the batch 1 changes and compare with `tools/`.
- **N-19 Issue titles and states (K-series and seed table).** Method: GitHub issue list (see [history-and-timeline.md](history-and-timeline.md)).

## Discrepancies
The status conflicts are listed in the table above. Two need a decision:
1. **Footprints (#8).** `docs/PLAN_NEXT.md:16` marks it closed; `docs/ISSUES_SEED.md:11` and `docs/PLAN_CONSOLIDATED.md:50` keep it open. The later source is the owner's finding of "not a light", which closes the glow part but not the clipping.
2. **Fog colour (#3).** `docs/PLAN_NEXT.md:13` says #3 is closed; `docs/PLAN_CONSOLIDATED.md:15` (2026-10-17) says the colour still changes with the camera. The later source is the owner's playtest. Treat #3 as open.

## Seed issues 1 to 17
Status as written in the record. "Seed" means the milestone v0.3.5 list in `docs/ISSUES_SEED.md`.

| # | Seed title (short) | Status | Evidence |
|---|---|---|---|
| 1 | AO noisy, dark cloud behind character, shimmer | disputed (K-07) | `CHANGELOG.md:50`; `docs/PLAN_CONSOLIDATED.md:14-15` |
| 2 | TAA trails and grass ghosting | partly (K-08) | `CHANGELOG.md:51`; `docs/PLAN_CONSOLIDATED.md:14-15` |
| 3 | Fog colour changes with camera direction | disputed (K-09) | `docs/PLAN_NEXT.md:13`; `docs/PLAN_CONSOLIDATED.md:15` |
| 4 | Fog inconsistent (Billabong trees, water, fire) | partly (K-02 to K-05) | `docs/PLAN_NEXT.md:11,24` |
| 5 | Water makes Ty flicker blue | open (K-01) | `docs/PLAN_NEXT.md:15` |
| 6 | High and Ultra flicker | open (K-06) | `docs/PLAN_CONSOLIDATED.md:14` |
| 7 | High GI cost +2.5 ms | open (K-16); the GI mask saves time per CHANGELOG | `CHANGELOG.md:52` |
| 8 | Footprints glow | disputed (K-10) | see above |
| 9 | Portal and crystal do not light the ground | open, v0.4 (K-11) | `docs/FIX_PLAN_20261016.md:37` |
| 10 | Distance LOD for all effects | open; LOD options showed no difference | `docs/PLAN_NEXT.md:18,36` |
| 11 | Shadow checkerboard and dotted line | closed on Medium (K-15); verification on High, Ultra and MapScale=4 open | `docs/PLAN_CONSOLIDATED.md:13` |
| 12 | In-game overlay menu on an unused hotkey | F10 overlay exists (regression open, K-20); live settings menu not built | `CHANGELOG.md:11`; `docs/PLAN_CONSOLIDATED.md:69` |
| 13 | Launcher versus in-game settings clash guard | implemented, warn only, untested (K-19) | `src/tyrender.cpp:632-659`; `CHANGELOG.md:20` |
| 14 | A/B screenshot hotkey | implemented in v0.3.5 (`[shots] Key=F11`) | `CHANGELOG.md:58`; `src/abshot.inl:22`; `src/default_tyrender.ini:99` |
| 15 | Research: world-space alternatives | research note only; no code | `docs/research/WORLD_SPACE_AO_GI_FOG.md` |
| 16 | Portability: NVIDIA, Intel, Proton reports | open (K-21) | `docs/PLAN_CONSOLIDATED.md:72` |
| 17 | Release v0.3.5 (G1, zip, A/B grids) | released per CHANGELOG (c255ae8). Release steps not verifiable here | `CHANGELOG.md:46`; N-19 |

## See also
- [history-and-timeline.md](history-and-timeline.md) for the dates and builds
- [decisions-log.md](decisions-log.md) for the decisions taken for each defect
- [failed-and-abandoned.md](failed-and-abandoned.md) for approaches that did not work
- [roadmap-and-open-work.md](roadmap-and-open-work.md) for the plan to fix these
- [../guides/debugging.md](../guides/debugging.md) for how to report a problem with a log

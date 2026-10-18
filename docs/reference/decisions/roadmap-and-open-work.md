# Roadmap and open work
> Status: documented at commit 4db1d7d; last verified 2026-10-18 (static read of the clone). Audience: C, A.

## Purpose
This page says what is planned, what is open, and what blocks a release, as the repository records it on 2026-10-18. It answers Q228. It does not promise any date. The plans in the repository disagree about the order of work, so this page lists each plan and says which one is newest. Each open item links to the defect it fixes on [known-issues.md](known-issues.md).

## How it works
Work is planned in four layers, each with a different owner and horizon:
1. **Next build order** (owner threads, newest): `docs/PLAN_NEXT.md` (2026-10-18). It lists five owner threads in priority order, and a rule that issues close only after the owner's playtest (`docs/PLAN_NEXT.md:26-46`).
2. **Batches** (2026-10-16 fix plan): `docs/FIX_PLAN_20261016.md` has three batches with file, dependency and acceptance per item. `docs/PLAN_CONSOLIDATED.md:21-59` puts the same batches in steps 0 to 4 with a release sequence.
3. **Task board**: `docs/todo.json`, 83 items. Status counts: 65 done, 17 open, 1 dropped (T32). Open ids are listed below.
4. **Longer-term ideas**: `docs/research/*.md` (see [research-notes.md](research-notes.md)) and `docs/MATERIAL_CLASSIFICATION_PLAN.md`, both marked as plans or ideas only.

`docs/PLAN_EXEC.md:2` says it "supersedes PLAN_NEXT.md / PLAN_R6.md for scheduling". `docs/PLAN_CONSOLIDATED.md:3` says it is "one place for the work order". PLAN_NEXT was written later (2026-10-18), so this page follows PLAN_NEXT for priority and the other plans for detail. See Discrepancies.

## Code pointers

| What | file:line | Note |
|---|---|---|
| Items with stubs in code (not yet wired) | `src/tess.inl:57-58` | `ReliefFadeStart/End` exist; other effects have no distance fade yet (T76) |
| AO fade and character keys (exist, default off) | `src/ao.inl:507-509` | Batch 1 item T73 |
| Overlay in-game menu (not built) | `src/overlay.inl:1-3` | Overlay exists; the full in-game menu on F12 is planned (`docs/PLAN_3DAY_20261016.md:135`) |
| Emitter light (off) | `src/emit.inl:34` | v0.4 item T56 |
| Decal clamp (off) | `src/tex.inl:72` | Footprints (T77) |
| Packaging (default version) | `src/package.ps1:2` | Release blocker: default version string |

## Settings
Each planned change is a setting or a code change. Planned defaults are listed in the plans (for example `docs/FIX_PLAN_20261016.md:17-49`). No planned default has been applied to the shipped ini yet. The keys that already exist are in [../reference/settings-ao-gi-lod.md](../reference/settings-ao-gi-lod.md) and the other settings pages.

## Cost
Planned cost targets (from the plans, not measured):
- Medium at or under 10 ms after distance LOD (`docs/PLAN_CONSOLIDATED.md:46`).
- High GI within +1 ms of v0.3.2 (`docs/PLAN_CONSOLIDATED.md:47`). The v0.3.3 cost was +2.5 ms (`docs/STATUS_2026-10-13.md:73`).
- AO fade: "Medium +1 ms at most" (`docs/PLAN_CONSOLIDATED.md:39`).
- Not measured: costs of LOD, checker, emitter, overlay on `main` (N-20).

## Limits
- The plans assume a single test GPU (AMD RX 7600 XT). Other GPUs are open (K-21 on [known-issues.md](known-issues.md)).
- The harness cannot reproduce High and Ultra flicker at high fps. Testing needs `sleepMs` emulation (`docs/RESUME.md:53`, `docs/START_HERE.md:76`).
- Owner playtests set the pace. Plans say "no game test while the user is playing" (`docs/PLAN_CONSOLIDATED.md:24`).

## Known issues
The open defects are on [known-issues.md](known-issues.md). The table below maps each open todo item to the defect it fixes.

## Open work

### Priority order (PLAN_NEXT, 2026-10-18)
Source: `docs/PLAN_NEXT.md:28-39`.

| # | Owner thread | Item | Defect |
|---|---|---|---|
| 1 | Desktop permissions and install of the newest `main` | Practical | - |
| 2 | F10 overlay | Verify in a level with `Key=F10` (#12, #79) | K-20 |
| 3 | Fog (#4) | Sky (merged, untested), trees (A/B `SeeThruDepth=1`), water and lilies (`WaterBoost`, check lilies) | K-02, K-04, K-05 |
| 4 | Water blue flicker (#5) | Bisect with in-game measurement, no guesses | K-01 |
| 5 | Portal light (#9) | Test with `[emit] Enable=1`; forward-render light cap is 16 (`docs/PLAN_NEXT.md:35`) | K-11 |
| 6 | LOD follow-ups (#10) | Tessellation (#60) and shadow (#78) LOD, both off; measure in game | K-16 |
| 7 | Low priority | Tessellation tuning (`ReliefCurve`, `ReliefKnee`) for low-resolution textures | K-27 |
| 8 | Release | v0.3.6, only after the next playtest | - |

### Batches (FIX_PLAN, 2026-10-16)
Source: `docs/FIX_PLAN_20261016.md:15-49`. Status for all items: open, as of todo.json.

| Batch | ID | Item | Files | Depends on | Status | Acceptance (short) |
|---|---|---|---|---|---|---|
| 1 | T73 | AO distance fade, character not an AO occluder, neighbourhood clamp, blue-noise rotation, fern rim | `src/ao.inl` | none | open | No flicker on far mountains; no halo behind Ty; cost +1 ms max at Medium |
| 1 | T91 | TAA ghosting: YCoCg clip, depth-guided reject, velocity-adaptive blend | `src/fx.inl` | none | open (code partly present, `src/fx.inl:128-137`) | No trails on fast Ty turns; no grass ghosting |
| 1 | T75 | Shadow checkerboard and bright lines: texel-aligned noise, Vogel rotation, depth-weighted upsample, bias | `src/light.inl` | none | open | No checkerboard on characters; no 1 px lines; S1 closed |
| 1 | T74 | Water reflection pulses brightness on Ty | `src/gi2.inl` (ExcludeSources) | none | open | Water does not sample GI; Ty does not pulse blue |
| 2 | T92 | Fog GI decouple: froxel emission from sun and sky only | `src/atmos.inl` | T73 | open | Fog hue constant under camera rotation |
| 2 | T92b | Trees through fog: Z-write for alpha above 0.50 | `src/pbr.inl`, `src/atmos.inl` | none | open | Fog covers foliage |
| 2 | T76 | Distance LOD for tess, AO, shadow taps, fog steps, GI rays. Medium 10 ms or less | `src/tess.inl`, `ao.inl`, `atmos.inl`, `gi2.inl` | T73, T92 | open | No popping; Medium at or under 10 ms |
| 2 | T77 | Footprint glow: luminance clamp on the decal path | `src/pbr.inl`, `src/tex_tags.inc` | none | open | Footprints do not glow |
| 2 | T56, T65 | Portal spill | none | none | deferred to v0.4 | - |
| 3 | T78 | Ultra flicker: bisect GI rays, contact shadows, Scope=all, AO wide pass with `sleepMs`. If unstable, change the Ultra preset | ini presets, `src/*.inl` | T73, T91, T75 | open | No full-screen flicker on Ultra |
| 3 | (none) | In-game menu on F12 with `stateguard.inl` around the UI pass | `src/tyrender.cpp`, `src/stateguard.inl`, `src/launcher_ui.inc` | none | not started | Menu opens; no skinned-mesh corruption |
| 3 | (none) | Launcher clash guard (warn, not override) | `src/tyrender.cpp` | none | implemented (warn only, `src/tyrender.cpp:632-659`) | Warning shown |
| 3 | (none) | A/B screenshot hotkey (optional) | `src/tess.inl` | menu | implemented (`[shots] Key=F11`, `src/abshot.inl:22`) | Optional |
| 3 | (none) | Release gate (G1), `release.ps1`, zip verify, LAA check, A/B shots | `package.ps1`, docs | all above | open | G1 pass; release checklist |

### Open todo.json items
Source: `docs/todo.json` (status `open`). Titles are shortened.

| ID | Kind | Title (short) | Related defect |
|---|---|---|---|
| T29 | user | Owner reviews the final contact sheets, chooses defaults, signs off; test on another GPU if available | Release blocker |
| T43 | lead | Optional `[light] SunWarmth` key; defer to v0.4 if larger than one session | - |
| T47 | auto | v0.3.1 motion suite: 2 s pan, frame-diff on static geometry | K-22 |
| T48 | lead | Atmosphere Intensity 2.5 to about 2.0 for Balanced; A/B three views | K-33 |
| T49 | auto | Rope and fence-grain 100% crops with a camera on a rope | K-26 |
| T50 | agent | v0.4: vendor-guard audit of GL calls and a real GL 3.3 or 4.0 context test | K-21 |
| T56 | lead | Portal and emissive light spill design | K-11 |
| T65 | lead | Emissive light spill, stronger relief on walls and props, level 2 checks | K-11 |
| T73 | agent | AO distance fade, character halo, fern rim | K-07, K-28 |
| T74 | agent | Water reflection pulsing brightness on Ty | K-01 |
| T75 | agent | Character shadow double edge, one-pixel line, grid | K-15 |
| T76 | agent | Distance fade of tess and relief; precision drop-off for all effects | K-16 |
| T77 | agent | Emissive handling from texture tags (footprints glow) | K-10 |
| T78 | agent | Ultra flicker is back (user): bisect Ultra settings and fix | K-06 |
| T90 | agent | Day 1 AO agent (Haiku): fade, character mask, history clamp, blue noise | K-07 |
| T91 | agent | Day 1 FX agent (Haiku): TAA trails and ghosting | K-08 |
| T92 | agent | Day 1 Atmos agent (Haiku): fog colour decouple, trees, water and fire through fog | K-03, K-04, K-09 |

Dropped: T32 (hidden-cost hunt), "run only if T23 fails" (`docs/results/DIRECTION_2026-10-13_checkpoint2.md:23`).

Referenced in the records but absent from todo.json: T52, T55, T66 to T69, T87 to T89 (N-28).

### Planned features (not in batches)
| Item | Source | Status |
|---|---|---|
| World-space AO field (baked, behind a `[ao]` key, default off) | `docs/research/WORLD_SPACE_AO_GI_FOG.md:24` | idea, not started |
| Irradiance probes for GI, as a separate issue | `docs/research/WORLD_SPACE_AO_GI_FOG.md:26` | idea, not started |
| Fog colour anchored to sun and height | `docs/research/WORLD_SPACE_AO_GI_FOG.md:28` | "cheapest experiment"; not started |
| Material class model (M-1 on the decisions page) | `docs/MATERIAL_CLASSIFICATION_PLAN.md:43-70` | plan only (2026-10-17) |
| Shadow prototype with few jittered taps behind a tier flag | `docs/research/minecraft_shaderpack_port_ranking.md:22` | idea; test only after batch 1 |
| Tonemap change (dark lift, white compression) | `docs/research/minecraft_shaderpack_port_ranking.md:20` | idea; cheap A/B |
| Emitter spill (half-resolution additive) | `docs/agent-tasks/light*.md`, `docs/EMIT_PORTAL_TEST_PLAN.md` | design; test prepared, not run |
| In-game menu and live reload | `docs/PLAN_EXEC.md:46` | Phase D; v0.4 if it slips |
| Sun warmth (`[light] SunWarmth`) | `docs/todo.json` T43 | deferred |

## Release blockers
From the records. None is complete at 4db1d7d.

1. **G1 gate on the final DLL**: 10-minute soak, four cameras, forced fallback (`docs/PLAN_CONSOLIDATED.md:55`, `docs/RESUME.md:46`). Soak has failed once and passed on rerun (`docs/RESUME.md:76-78`).
2. **Re-measure the full preset ladder** (High and Ultra not re-measured since v0.3.2) (`docs/PLAN_CONSOLIDATED.md:56`).
3. **Update the shipped docs** (README, PERFORMANCE, STATUS) and fix the known inconsistencies before any release (`docs/PLAN_CONSOLIDATED.md:57,82-87`).
4. **Check the test zip's ini** (`docs/RESUME.md:58`). Zips are not in the repository.
5. **Owner sign-off** on defaults and the Ultra decision (T29, P-A on the decisions page).
6. **Community A/B grids**, Medium first (`docs/PLAN_CONSOLIDATED.md:58`). The current grids are drafts (`docs/PLAN_R6.md:2`).
7. **Hard-coded owner paths** in the tools (the owner's tools folder) block a clean build and a clean-machine test (N-12, N-13).
8. **Licence** for the project's own code (O-2 on the decisions page).
9. **Tag and zip** (`docs/PLAN_CONSOLIDATED.md:59`). Zips are never committed (D-11 on the decisions page).
10. **Version string**: `src/TYLauncher.c:22` still says `v0.3.5-rc`.

## Portability and build (open)
- One GPU tested (`docs/GOAL.md:18`). NVIDIA, Intel and Proton logs requested from testers (`docs/PLAN_CONSOLIDATED.md:72`, `docs/RESUME.md:36`).
- Vendor-guard audit and a real GL 3.3 or 4.0 context test: T50, v0.4 (`docs/RESUME.md:41`).
- Linux cross-compile with llvm-mingw (N-14), CI workflows (N-15): not in repository, not checked.
- Clean-machine build: `build.ps1`, `package.ps1` and the tools hard-code the owner's paths (N-12, N-13).

## NEEDS INVESTIGATION
- **N-12 Clean-machine build.** Method: build in a clean VM or container with llvm-mingw from releases, then patch the scripts. A follow-up code change (not a docs task).
- **N-13 Hard-coded owner paths.** Method: search `tools` and `src` for an absolute drive path (for example `grep -rn 'C:' tools src`). The usage line in `src/build.ps1:2` and `docs/START_HERE.md` carry the owner's tools-folder path. Decide on a config file; `tools/env.ps1` already exists in the clone and sets environment variables only.
- **N-14 Linux cross-compile.** Method: try `i686-w64-mingw32-clang++` in a Linux container.
- **N-15 CI.** Method: `gh api repos/<owner>/<repo>/actions/workflows`. Only templates are in the repository (`.github/`).
- **N-20 Costs of unreleased `main` features.** Method: a GPU-timer run with `tools/perf_matrix.ps1` or `tools/g1_gate.ps1`. Until then, "not measured".
- **N-28 Missing todo ids.** Method: `grep -rn 'T52' docs` and read the `notes` field in `docs/todo.json`.
- **N-22 Shadow cascade behaviour on other levels.** Method: playtest level 2 with `MapScale=4`.

## Discrepancies
1. **Order of work.** `docs/PLAN_NEXT.md:28-39` (2026-10-18) puts the F10 overlay and fog first. `docs/PLAN_CONSOLIDATED.md:26-59` puts batch 1 (AO, TAA, shadows, water) first. `docs/PLAN_EXEC.md:26-44` puts phase B (AO, TAA, fog) first and water in phase C. These are three different orders for the same work.
2. **Next release.** `docs/FIX_PLAN_20261016.md:54` (v0.3.3), `docs/PLAN_EXEC.md:7` (v0.3.5), `docs/PLAN_NEXT.md:39` (v0.3.6). See [decisions-log.md](decisions-log.md) D-14.
3. **Batch 1 status.** `docs/PLAN_EXEC.md:9-10` says the depth-guided TAA reject and the AO fade slider are missing. `src/fx.inl:134-137` and `src/ao.inl:507-508` already have them (the AO slider keys exist with default 0, and no launcher slider was found by grep). T73 and T91 stay open in todo.json.
4. **Ray-march.** `docs/results/DIRECTION_2026-10-13_checkpoint1.md:22` would keep a ray-march path for GL below 4.3. The code has no ray-march path (see [failed-and-abandoned.md](failed-and-abandoned.md)).
5. **Hotkey for the menu.** The planned in-game menu uses F12 (`docs/PLAN_3DAY_20261016.md:135`); the overlay that exists uses F10 (`CHANGELOG.md:11`).
6. **Stale plan items.** `docs/RESUME.md:52,54,58` gives the old traps and the test zip issue. Both are still true of the code.

## See also
- [known-issues.md](known-issues.md) for the defects behind each item
- [decisions-log.md](decisions-log.md) for the owner's open decisions (O-1 to O-6)
- [failed-and-abandoned.md](failed-and-abandoned.md) for approaches not to repeat
- [history-and-timeline.md](history-and-timeline.md) for the release ladder
- [research-notes.md](research-notes.md) for the research behind the planned ideas
- [legal-and-licensing.md](legal-and-licensing.md) for the licence blocker

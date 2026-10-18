# RESUME - current state only (rewritten 2026-10-13 19:23, docs pass 2)

> Historical handoff snapshot (2026-10-13 era). For the current state see [docs/reference/](reference/README.md) and [CHANGELOG.md](../CHANGELOG.md).


Entry point and how-to: `START_HERE.md`. Numbers: `STATUS_2026-10-13.md`. Problem ids from playtests: `recording\INDEX.md`. Plan order: `PLAN_NEXT.md`. Older running notes (appended log) are in `archive\20261013\RESUME_appended_log_20261013_1923.md`; the earlier consolidated versions are in `archive\20261013\`.

## State right now (evening 2026-10-13)
- **Running agents: none known.** Todo T80, T81, T82 are done (texture tags, GI, light). No game test is queued. Max 3 agents when starting new ones.
- **Shipped release: v0.3.2 preview / community beta.** Zip `SRC\tyrender\dist\TYHD_Renderer_v0.3.2.zip` (DLL sha1 prefix `6571393575`). G1 gate passed: 10-minute soak OK; Medium default 12.08 ms, vista 10.45, sand_graze 10.47, fence_mid 10.21 ms (2560x1440, RX 7600 XT); forced fallback 7.44 ms.
- **Test build: v0.3.3** (NOT released). Zip `SRC\tyrender\dist\TYHD_Renderer_v0.3.3.zip`, DLL sha1 prefix `0753BB1B82`, DLL installed in the game folder. The game folder `tyrender.ini` is the **Ultra** preset for testing. The launcher exe in the game folder was NOT updated (the user's open `TYLauncher.exe` locks it). **G1 gate NOT run on v0.3.3.** Smoke only: Medium 12.05 ms, High 19.80 ms (was about 17.2: GI mask and blur cost about +2.5 ms).
- **Presets:** Low / Medium / High / Ultra. "Balanced" is retired; Medium is the old Balanced and the default.
- **Launcher:** simple mode shipped in v0.3.2 (presets, overall impact, Play, "More options >>").
- **Community A/B shots:** `RESEARCH\AB_shots\` (level 1 Rainbow Cliffs, level 2 Billabong, 10 views each) and a copy on `E:\TY_AB_shots` (Samsung T7; re-copy after changes with `robocopy C:\CLAUDE\RESEARCH\AB_shots E:\TY_AB_shots /E`). Failed attempts are `*_DO_NOT_SHARE`; do not publish them.

## What v0.3.3 changes (test build, for the user to play)
- **Texture tags:** the texture file name is read when the game loads a texture (file-read hooks + `glCompressedTexImage2D`). 279 of 342 uploads got a name, 206 are tagged from `recording\texture_tags.csv` (generated into `tex_tags.inc` by `gen_tex_tags.ps1`). Character textures get no bump, relief or specular. Skinned shaders use `[tex] SkinnedByTag=1`. The tessellation shader (TES) also reads the per-draw `tyTex` flag.
- **GI sources:** `[gi] ExcludeSources=3`: draws tagged emissive or transparent (water, portal glow, fire) write alpha 0, so they are not bounce sources. Portal glow no longer bounces onto the ground. `ExcludeSources=2` keeps the portal glow as a source.
- **GI noise:** `SourceBlur 0.15`, `NoiseCycle 8`, `TemporalBlend 0.85` (roughly halves GI flicker next to water in the static test; static scene 0.94 Medium, 1.19 GI, 1.06 new).
- **Light debug keys (all off by default):** `[debug] ShadowView`, `PlaneBias`, `PlaneClamp`, `SlopeBias`, `SkinBias`. An 8-tap early-out probe was added (+0.02 ms).
- **Test options:** `motion.req` takes `fwd`, `side` and `sleepMs` (low-fps emulation, moving camera).
- **Not fixed in v0.3.3:** character shadow bright lines (root cause not found, T82), footprints glow (cause unknown), water brightness pulse (T74), AO halo (T73), fire haze, distance fades (T76).

## User playtest history
1. v0.3 (Balanced): first list (F1-F12). 2. v0.3.1 Ultra (G1-G10). 3. v0.3.2 (W1, S1, A1, E1 plus requests R3-F1, R3-F2). Medium/High "no flicker" in playtest 3; Ultra flicker came back.
4. Playtest 4 (v0.3.2, "Recording 2026-10-13 155438.mp4", voice `voice4.txt`): **Medium stable; High and Ultra flicker.** Water flickers blue every frame on Ty (GI sampling water). Jagged bright lines on the face and bag shadows. Fire not hazed. Bag not tessellated (tags should fix). Portal light lights Ty, not the ground. Footprints glow. AO halo behind Ty. Ultra is slow. Details: `recording\FINDINGS_ROUND4.md`. The user said they would play Medium rather than Ultra.

## Open items (todo.json status, plus the v0.3.3 state)
| id | what | state |
|---|---|---|
| **T78** | High/Ultra flicker (playtest 4 says High too). Suspects GI and contact shadows. | v0.3.3 has the GI changes (ExcludeSources, blur, noise cycle). Contact shadows only +0.035 flicker in the static test. Needs **user playtest of High and Ultra**. Todo still `open`. |
| T74 / W1 | water brightness pulses Ty's brightness; blue flicker from water as a GI source | GI side in v0.3.3 (R4 water). Water reflection on Ty (W1) not done. |
| T73 / A1, R3-F1 | AO distance-fade slider; AO halo behind Ty (character as occluder); white rim on fern shadows | not started (todo `open`) |
| T75 / T82 / S1 | character shadow jagged bright lines (face, bag), grid | T82 reported done, but the bright lines are NOT fixed; root cause not found (only stipple on thin rims). Re-open as needed. |
| T76 / R3-F2 | distance fade of relief/tess and precision drop-off for all effects; savings | not started (todo `open`). Also: High GI cost +2.5 ms in v0.3.3 needs optimising. |
| T77 / E1 | footprints glow | tag system is in v0.3.3, but footprints still glow in the test; cause unknown (hypothesis: decals blended after the depth copy). |
| R4 | portal light on Ty not the ground (F9/G6, T56/T65); fire not hazed (G3); Billabong fog (G8); bag tessellation (tags) | open; atmos and light work |
| T56 / T65 | emissive light spill (portal, crystal), stronger relief on walls/props, level-2 checks | open, v0.4 |
| T29 | user sign-off on default look and presets; NVIDIA/Intel/Proton logs from beta testers | user |
| T43 | optional `[light] SunWarmth` key | deferred |
| T47 | formal motion-shimmer metric on Medium | not recorded |
| T48 | Atmos Intensity 2.2 for Medium; confirm far walls on fence_close | open |
| T49 | rope and fence-grain 100% crops with a camera on a rope | open |
| T50 | vendor-guard audit of GL calls + real GL 3.3/4.0 context test (v0.4) | open |

## Next actions (in order)
1. **User playtest of v0.3.3** (High and Ultra flicker, water, footprints, bag). Do not run game tests while the user plays. Ask the user before changing the game folder.
2. Work on the open ids in `PLAN_NEXT.md` order: T73 (AO fade + halo), T74 (water), T75/T82 (shadow lines, root cause), T76 (distance LOD + High GI cost), atmos (fire haze, Billabong), footprints (E1). Max 3 agents, each with a task file in `agents\tasks\`.
3. After the playtest and the fixes: merge (`merge_agent.ps1`) into staging `SRC\merge`, then G1 gate (`g1_gate.ps1`, soak + 4 views + forced fallback), re-measure the full ladder (High and Ultra were not re-measured on v0.3.2), `release.ps1`, update `README.md` / `PERFORMANCE.md` / `STATUS`, and new A/B shots with `abshots.ps1` + `match_ab.ps1`.
4. Only then call anything beyond "preview". T50 and hardware reports first.

## How to resume (exact steps)
1. Read `START_HERE.md` section 4 (commands). Then this file, then `PLAN_NEXT.md`.
2. Board: `powershell -NoProfile -File C:\CLAUDE\TOOLS\todo_runner.ps1 -Status`. Raw facts: `status.ps1` writes `STATUS_RAW.md`.
3. Test build check: game folder `tyrender.dll` sha1 should start `0753BB1B82`; `tyrender.ini` there is the Ultra test preset.
4. A game test: start the focus keeper first (`focus_daemon.ps1`, hidden), then `agent_test.ps1` / `suite.ps1` / `motion.ps1`. Never start the game by hand.
5. Traps for this build (see `START_HERE.md` section 6): `-IniFile` is not read at game start (only the `-AB` variants apply; restart-only keys such as `MapScale` need the ini copied into the game folder before the run); `merge_agent.ps1` cannot copy NEW files (copy them by hand, e.g. `tex.inl`, `tex_tags.inc`, `gen_tex_tags.ps1`); whole-file CRLF conflicts need `diff -u` + `patch`.

## Known inconsistencies (not fixed: code or data, not docs)
- `SRC\tyrender\default_tyrender.ini` header says Medium with "about 90 fps" (old 11.1 ms number); Medium is about 12 ms, about 80 fps. Cosmetic, fix at the next repackage.
- **`dist\TYHD_Renderer_v0.3.3\tyrender.ini` (in the test zip) is not the Ultra test ini:** it has Medium-like values (`[gi] Enable=0`, `[atmos] Intensity=2.2`), `[tess] Scope=skin` (README default is `env-detail`), and the stale "about 90 fps" header. Check and fix before any release; the zip is not edited (rule: keep zips untouched).
- `TOOLS\launcher_shot.ps1`, `soak.ps1`, `balanced_search.ps1` default to `results\inis\preset_Balanced.ini` (file name only; that ini is the Medium preset).
- `TOOLS\ty_play.ps1` header comment mentions save slot GAME 2; the code defaults to slot GAME 1 and `TY_SLOT=2` selects slot 2 (Billabong).
- `GOAL.md` acceptance text still says "Balanced preset" and a 4.6 ms stock frame; read it as Medium and about 5.7 ms (4.6 was a different view/build). The status line at the top of GOAL.md was updated.
- `ovsync_all.ps1` mirrors only root-level RESEARCH files, `agents\*.md` (root), sessions, src and tools. It does NOT cover `recording\`, `archive\`, `agents\tasks\` (use single-file `ovSync` for those, see `START_HERE.md` section 5). With `-Quick`, files older than 3 h (e.g. `PLAN_NEXT.md`, `DOC_MAP.md` at 15:2x) are skipped.

- 19:56 COLOUR MATCH adopted (T85): measured vs vanilla A/B (results\cm1, cm2): old Medium lum 1.12 sat 0.83; variant f = lum 1.003 sat 0.927 -> Medium/High/Ultra now: [atmos] Intensity 1.2 (Ultra 1.0), Desaturation 0.3, HazeLift 0.15; [post] Brightness 0.94, Saturation 1.18; Low unchanged (1.0/1.03). Applied in launcher_ui.inc (kPresetHaze/kPresetGrade, slider defaults; backup .bak_colour), presets.ps1 (backup .bak_colour), preset_*.ini, default_tyrender.ini. NOT yet in built exe/zip (needs launcher rebuild + package; game-folder ini unchanged = old Ultra).

- 20:19 T84 gi agent R5 REPORTED: red-on-ground culprit NOT isolated (sand R-G 1-5 in all variants on Billabong sand, Ty hidden by grass); changed GI mask: [gi] ExcludeSources bit 4 = character draws (TexIsCharacter) not GI sources, default 7 (gi.inl gi2.inl in SRC\agents\gi; unverified). Medium has GI off -> if user sees red on Medium: test [light] Hemisphere=0 and engine bloom. High +2.5 ms GI cost not addressed. Waiting for light agent (shadow dotted line / checkerboard / grass ghosting, Opus a6ff7f62c6ac3ed82).

- 20:26 v0.3.4 BUILT+INSTALLED (dll+launcher) zip dist\TYHD_Renderer_v0.3.4.zip: light R5 ROOT CAUSE of dotted bright line in shadows = shadow taps sampled inside divergent branches (GPU picks wrong LOD/filter) -> textureLod(t,p,0) in TyS + atmos SunVis (same-session proof at fence_mid; also the inner light outline on leaf shadows = 'checkerboard'?); [post] TAAReject=1.0 (grass ghosting, unproven); [shadows] CloseCracks=0 off; gi: character draws not GI sources (ExcludeSources bit4, default 7, unproven); colour match defaults (Medium look = new). Game folder ini set to Medium(new colours) (old in tyrender.ini.bak_pre_v034). G1 gate running (results\g1_gate.md).

## SAVE POINT 2026-10-13 20:37 (user warned of token limit)
- Current build: v0.3.4 (zip SRC\tyrender\dist\TYHD_Renderer_v0.3.4.zip), installed in game folder (dll+launcher simple mode), game ini = Medium with new colour match. G1 gate RUNNING in background via TOOLS\g1_gate.ps1 -> results\g1_out.txt / g1_gate.md (do not touch mouse/keyboard; if the harness crashed, the game folder self-heals at next run via IN_TEST flag).
- Done today: community A/B shots (E:\TY_AB_shots, 80 imgs), launcher simple mode, texture-tag system, GI source mask, shadow dotted-line root cause fix, colour match, docs consolidated (START_HERE.md, DOC_MAP.md, PLAN_NEXT.md).
- Open: (1) user playtest of v0.3.4 (High/Ultra flicker, Ty red on ground location, grass ghosting, checkerboard); (2) High GI cost +2.5 ms; (3) AO distance-fade slider + halo behind Ty (T73); (4) footprints glow; (5) fire/haze + Billabong pink fog; (6) distance LOD (T76); (7) re-measure High/Ultra ladder; (8) docs refresh for v0.3.4 (README/PERFORMANCE + RESUME), OpenViking sync (ovsync_all.ps1 -Quick + single files for recording/archive/agents\tasks).
- No agents running. Agent folders SRC\agents\{light,gi,tess,...} hold R5 work (merged).

- 20:45 G1 v0.3.4: perf OK (default 10.73, vista 9.68, sand_graze 9.76, fence_mid 9.50, forced fallback 5.96 ms) BUT soak FAIL at 9 min 'game process exited': log ends with clean wglMakeCurrent(0,0) = normal exit, cam pos 1347,1.5,-210 (far from start; random walk probably entered portal/level exit) -> likely NOT a crash; soak rerun in progress (results\soak\soak_output.txt).

- 20:57 v0.3.4 soak RERUN OK (10 min, preset Medium, cam far from start) -> first soak exit was the random walk leaving the level (clean shutdown), not a renderer crash. G1 for v0.3.4 effectively PASSED (perf default 10.73 / vista 9.68 / sand_graze 9.76 / fence_mid 9.50 / fallback 5.96 ms; soak OK on rerun). Game ini = Medium (new colours).

- 20:59 FULL OpenViking upload done: ovsync_all.ps1 (357 files: research docs, sessions, agents, SRC\tyrender source, tools, agent folders) + TOOLS\ovsync_extras.ps1 (108: recording md/txt/csv, archive, agents\tasks, results md/csv/txt, todo.json, AB_shots README, agent REPORT*.md). Re-run both after each step; extras script covers the paths ovsync_all skips.

- 21:22 COMMUNITY GRIDS task: recapturing Ultra v0.3.4 (suite grid_ultra_v034; old Ultra AB shots invalid: MapScale restart issue). Game ini temporarily = Ultra; Medium ini backup at tyrender.ini.bak_pre_grid -> RESTORE after run. Then 2x2 grids (top wide vanilla|Ultra, bottom close vanilla|Ultra) into AB_shots\community_grids + E:\TY_AB_shots\community_grids.

- NEXT SESSION START HERE: RESEARCH\PLAN_R6.md (user feedback after v0.3.4 runs: AO noise = main flicker, fog colour changes with view, Billabong trees/water vs fog, water disco-ball, trails/ghosting, launcher vs in-game settings, in-game overlay hotkey; first task R6-1 = identify the 20:22/20:23/20:27 harness configs the user saw as flicker-free). Community grids in AB_shots\community_grids are DRAFT (Ultra artefacts), not for sharing. Game ini restored to Medium.
- 23:07 2026-10-16 resume: R6-1 DONE (results\R6_config_diff.md): flicker-free runs were MEDIUM; water disco-ball occurs on Medium (GI off) -> not GI. Following D:\Downloads\TY_HD_3Day_Plan.md (copy in RESEARCH\TY_HD_3Day_Plan.md). Day1 agents: AO(T73), FX/TAA, Atmos (swapped for Light since S1 fixed by textureLod).
- 23:08 2026-10-16 PLAN BUILT: RESEARCH\PLAN_EXEC.md (supersedes PLAN_NEXT/PLAN_R6 for scheduling; awaiting user approval of decisions A-G; NO agents launched; task files ao6/fx6/atmos6 + todo T90-T92 ready, agent folders SRC\agents\{ao,light,atmos} fresh from v0.3.4).
- 23:12 A4 Haiku agent (shader pack research, a494ed1d9e11a8a01) launched; output shaders\analysis\complementary_findings.md; PLAN_EXEC.md addendum section 7.

- 23:15 A4 DONE: shaders\analysis\complementary_findings.md (+filelist). LICENCE: Complementary License 1.7 = all rights reserved, NO code reuse/redistribution -> ideas/technique only, own implementations. Top ideas: AO ramped occlusion band + per-frame rotation; TAA velocity + 0.09 depth-edge term; entity shadows static noise ~2x taps smaller kernel; shadow normal offset by distance + centre pull + foliage z-jitter; fog colour from sun+altitude only + water fog alpha trick. Phase B agents read it. GITHUB: no GitHub tool in session (connectors: Docs, visualize, scheduled-tasks); gh CLI not installed; repo prepared locally at C:\CLAUDE\REPO\ty-hd-renderer via TOOLS\repo_sync.ps1 (whitelist copy, secret guard).
- 23:16 GITHUB: local repo ready C:\CLAUDE\REPO\ty-hd-renderer (2 commits, 0.9 MB, no binaries, OpenViking scripts/LAN IP excluded), docs\ISSUES_SEED.md has 17 issues; remote NOT created: needs gh CLI+login or a GitHub connector or user-created empty repo URL (awaiting user choice). TOOLS\repo_sync.ps1 mirrors working files into the repo.

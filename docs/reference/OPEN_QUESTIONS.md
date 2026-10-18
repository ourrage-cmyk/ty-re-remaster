# Open questions

> Status: collected from every page's NEEDS INVESTIGATION and Discrepancies sections at commit 4db1d7d. Not a reference page. Item ids (N-nn, NI-xx, P12-nn) are the writers' ids; the master list is in the project plan, section 6. Audience: owner and maintainers.

This file de-duplicates the open items of the whole set. Each line names the pages that carry the detail. Where code and a document disagree the code is taken as right, and the stale text is listed in section C.

## A. Owner decisions

| Id | Question | Detail in |
|---|---|---|
| A1 (N-16, L-1) | Licence of the project's own code (no LICENSE file); legal status of redistributing the compiled DLL and launcher | [legal-and-licensing](decisions/legal-and-licensing.md), [decisions-log](decisions/decisions-log.md) |
| A2 (L-3, L-4, NI-D4) | Keep or reduce published game frames (`docs/ab/`, `docs/playtest_frames/`) and voice transcripts (`docs/recording/*.txt`); check transcripts for personal data before publishing | [legal-and-licensing](decisions/legal-and-licensing.md), [data-files](reference/data-files.md) |
| A3 (L-5) | Trademark use in the project name | [legal-and-licensing](decisions/legal-and-licensing.md) |
| A4 (N-17, NI-S1, NI-D2, NI-T4) | Publish sanitised copies of the missing reverse-engineering notes, `RESEARCH` files, `ovsync_all.ps1`, `vm.ps1`? | [tools-reference](reference/tools-reference.md), [source-map](reference/source-map.md) |
| A5 (N-13, N-12, P5) | One config value for the hard-coded workstation root (suggested `tools/env.ps1`); clean-machine build | [tools-reference](reference/tools-reference.md), [build-from-scratch](guides/build-from-scratch.md), [branching-and-process](guides/branching-and-process.md) |
| A6 | Which preset source is the reference: launcher (`src/launcher_ui.inc`) or `tools/presets.ps1`; which Medium cost comment is right | [presets](reference/presets.md), [add-a-preset-step](guides/add-a-preset-step.md) |
| A7 (N-26) | Intended `[ao] Radius`, `TemporalBlend`, `Power` (header comment versus code) | [settings-ao-gi-lod](reference/settings-ao-gi-lod.md) |
| A8 | `[atmos] HazeSky` default 0.6 (README) versus 0 (code); and the other README haze defaults | [sky-estimate-and-haze-color](shaders/sky-estimate-and-haze-color.md), [settings-atmos-fog-emit](reference/settings-atmos-fog-emit.md) |
| A9 | `[post] Sharpen` default: code 0.25, launcher 0.35 | [grade-and-sharpen](shaders/grade-and-sharpen.md), [add-a-setting](guides/add-a-setting.md) |
| A10 | `[tess] Scope` default: code `env-detail`, shipped `skin` (which turns geometry tessellation off) | [tessellation-and-relief](shaders/tessellation-and-relief.md), [decisions-log](decisions/decisions-log.md) |
| A11 | Remove the `[debug] GpuProf` comment or add the key | [settings-system-debug](reference/settings-system-debug.md) |
| A12 | Which fallback is intended for atmosphere with `ContactShadows` above 0 (ray march or off) | [atmosphere-froxel-compute](shaders/atmosphere-froxel-compute.md), [11-fallbacks](how-it-works/11-fallbacks-and-failure-handling.md) |
| A13 | Is `[lod] TessEnable` meant to work without the `[lod] Enable` master switch? | [distance-lod](shaders/distance-lod.md) |
| A14 | Keep legacy GI (`Mode=0`) and legacy AO (`Legacy=1`)? Is a 1-ray default for High wanted (todo T10)? | [gi-legacy](shaders/gi-legacy.md), [gi-hiz](shaders/gi-hiz.md), [ao-gtao](shaders/ao-gtao.md) |
| A15 | Was the planned gamma range dropped on purpose (FIX_PLAN)? | [taa-fxaa](shaders/taa-fxaa.md) |
| A16 | Acceptance of the 2026-10-16 plan (P-A to P-H); next-release naming; whether the release rule was adopted | [decisions-log](decisions/decisions-log.md) |
| A17 | Branch protection, push rights, which `docs/agent-tasks` files are live (P3, P4) | [branching-and-process](guides/branching-and-process.md) |
| A18 | Rationale for the forward-rendering design and for a patched exe instead of a proxy DLL | [constraints-and-limits](concepts/constraints-and-limits.md), [02-loading](how-it-works/02-loading-and-injection.md) |
| A19 | Supported `TY.exe` builds and whether to add a hash check (N-7) | [installer-and-uninstaller](reference/installer-and-uninstaller.md) |

## B. Code defects and behaviours to test

Items marked (suspected) are readings of the code, not observed failures.

| Id | Item | Method | Detail in |
|---|---|---|---|
| B1 (N-24, NI-O1, P12-14) | F10 overlay in a level (#12, #79, #102); arrow input reaching the game; focus-free toggle | Autoplaytest "Overlay check" | [09-overlay](how-it-works/09-overlay-input-and-hotkeys.md), [tester-guide](guides/tester-guide.md) |
| B2 | Reload clears every `tyEmit` location (suspected defect) | Enable emit, reload, look for emitter light | [emitter-light](shaders/emitter-light.md) |
| B3 | Emitter white band still present? | `docs/EMIT_PORTAL_TEST_PLAN.md` | [emitter-light](shaders/emitter-light.md), [04-frame-anatomy](how-it-works/04-frame-anatomy-and-pass-order.md) |
| B4 | Late-fog boost may outlive its draw on the tessellation early return; late boosts in the F11 off frame | Enable `[fog] Water`/`LateBoost`, compare frames | [04-frame-anatomy](how-it-works/04-frame-anatomy-and-pass-order.md), [atmosphere-fog](shaders/atmosphere-fog.md) |
| B5 | Vertex and fragment PBR rewrites can disagree and fail to link | Count link failures in the log | [pbr-surface-detail](shaders/pbr-surface-detail.md), [05-rewriting](how-it-works/05-shader-source-rewriting.md) |
| B6 | Every transparent texture gets `waterTag=1` (sky Fresnel skip side effect) | Look at transparent props near sky | [material-model](concepts/material-model.md) |
| B7 | Tag-name comparison: generator case-insensitive, runtime exact | Compare tag results for mixed-case names | [07-texture-names](how-it-works/07-texture-names-and-tags.md) |
| B8 | `snap.req` name is not sanitised; crash line `stage=` always `init`; no `PERF` log line exists | Read code, try an invalid name | [hotkeys-and-files](reference/hotkeys-and-files.md), [log-and-error-messages](reference/log-and-error-messages.md) |
| B9 | `[gpu]` cadence counts samples, not frames | Compare `[frame]` and `[gpu]` lines | [10-logging](how-it-works/10-logging-timers-and-session-report.md) |
| B10 | Session report copies the full `settings.ini`; privacy claim in the launcher and report header may be too strong (NI-R1) | Inspect real reports | [session-report](guides/session-report.md), [legal-and-licensing](decisions/legal-and-licensing.md) |
| B11 | Uninstaller leaves some files and `settings.ini.tyhd_backup`, yet says the game is back to original | Install, uninstall, diff the folder | [installer-and-uninstaller](reference/installer-and-uninstaller.md) |
| B12 | `-IniFile` takes effect at boot? Slot 2 boot reliability; `req-82-waterboost` blocked or pending | Run the harness | [test-harness](guides/test-harness.md) |
| B13 | `[shadows] Taps`, `ShadowView`, `[tess] Alpha/TargetPx/MaxLevel`, `[light]`, `[emit] Enable`, `[fog] WaterNames`: live or restart | Change via the reload file and observe | [settings-post-shadows-light-pbr-tess](reference/settings-post-shadows-light-pbr-tess.md), [08-settings-pipeline](how-it-works/08-settings-pipeline-and-live-reload.md) |
| B14 | `MapScale=3` (accepted, gives 6144); MapScale after reload; cascade size on other levels (N-22) | Set it and read the log line | [shadow-filter](shaders/shadow-filter.md) |
| B15 | `[tess] Enable=0` leaves fragment relief; `MaxLevel`, `Alpha`, `EdgeLength` inert for world tessellation; `Scope=off` untested | Per-key A/B | [tessellation-and-relief](shaders/tessellation-and-relief.md) |
| B16 | `[gi] DebugView=3` not handled by the composite; `[debug] GpuProf=0` unread; `[lod] Shadow`/`TessEnable` consumers not fully traced | Read code, set the value | [gi-hiz](shaders/gi-hiz.md), [settings-ao-gi-lod](reference/settings-ao-gi-lod.md) |
| B17 | Launcher reads `[gi] Enable` as 0 and `Rays` as 1 when missing; a launcher save writes every slider (Sharpen 0.35, SunIntensity 2.0, AO strength 0.6) | Fresh folder, press Play, diff the ini | [launcher](reference/launcher.md), [presets](reference/presets.md) |
| B18 | `taa_off` shows +0.40 ms (sign suspicious) | GPU timers, AA 0 to 3 | [taa-fxaa](shaders/taa-fxaa.md), [shaders/README](shaders/README.md) |
| B19 | Visual issues: AO flicker, character halo, red ground tint (T84), High/Ultra blue GI flicker, water pulse (#5), footprints (#8), fog hue (#3), trees through fog (#4), TAA grass ghosting, lily pads | Playtest or `tools/motion.ps1` per issue | [known-issues](decisions/known-issues.md), [ao-gtao](shaders/ao-gtao.md), [gi-hiz](shaders/gi-hiz.md), [water-fog-and-decals](shaders/water-fog-and-decals.md) |
| B20 | Engine threading, GL calls bypassing the IAT hooks, per-frame call counts (N-8, NI-GL-1, NI-GL-2) | Thread id in hooks; capture | [03-gl-hooking](how-it-works/03-gl-hooking.md), [hooks-reference](reference/hooks-reference.md) |
| B21 | Other hardware and platforms: NVIDIA/Intel, Proton, Steam Deck, Steam overlay with the renamed exe, ultrawide, 1080p (N-9, N-10, N-21, N-23); GL 4.3 without image load/store | Needs that hardware | [constraints-and-limits](concepts/constraints-and-limits.md), [tester-guide](guides/tester-guide.md) |
| B22 | Cost not measured on current `main` for LOD, checker, emit, overlay, tess options, presets (N-20) | `tools/perf_matrix.ps1`, `tools/g1_gate.ps1` | [presets](reference/presets.md), [distance-lod](shaders/distance-lod.md) |
| B23 | Engine facts: 17 shader files, GLSL versions, frame anatomy, MSAA resolve in `fx.inl:208`, RT sizes, light cap 16, why `SSAO=true` stalls the menu, MSAA default (N-1 to N-5, N-11, NI-Ch03-1..12) | Dump shaders, `capframe.ps1` | [game-and-engine-facts](concepts/game-and-engine-facts.md), [06-render-targets](how-it-works/06-render-targets-depth-and-state.md) |
| B24 | Game switches `-novsync`, `-DebugModeCamera`, `-StopFrame` etc.; save location; purpose of `cm_run.ps1`, `grid_run.ps1`, `focus_probe.ps1`; PowerShell 7 | Try each | [hotkeys-and-files](reference/hotkeys-and-files.md), [tools-reference](reference/tools-reference.md) |

## C. Stale documents (code wins; the set already follows the code)

| Id | Stale statement | Correct per code | Where recorded |
|---|---|---|---|
| C1 | README haze defaults (`HazeSky` 0.6, `SkyHaze` 0.5, `SkyHazeHeight` 0.25, `HazeHue` 0.25), `ContactShadows` 0, `TemporalBlend` 0.85 | 0 / 0.8 / 0.45 / 0.15, 0.35, 0.92 | [settings-atmos-fog-emit](reference/settings-atmos-fog-emit.md), [settings-ao-gi-lod](reference/settings-ao-gi-lod.md) |
| C2 | Header comments in `ao.inl`, `atmos.inl`, `gi.inl`, `fx.inl`, `light.inl` show other defaults | Loader calls | [ao-gtao](shaders/ao-gtao.md), [atmosphere-fog](shaders/atmosphere-fog.md), [gi-legacy](shaders/gi-legacy.md) |
| C3 | "Legacy GI slow path removed" and `gi2.inl:1` "falls back to legacy" | Legacy kept as `Mode=0`; Hi-Z failure turns GI off | [failed-and-abandoned](decisions/failed-and-abandoned.md), [11-fallbacks](how-it-works/11-fallbacks-and-failure-handling.md) |
| C4 | Fallback log text "ray-march path" and README "no slower path" | True only for `ContactShadows=0` | [log-and-error-messages](reference/log-and-error-messages.md) |
| C5 | "Taps"/"ShadowView" restart-only | Live via reload | [shadow-filter](shaders/shadow-filter.md), [natural-light](shaders/natural-light.md) |
| C6 | `ui_part_a.inc` and `ui_part_c.inc` described as live launcher parts | Not included by `TYLauncher.c` (N-18) | [launcher](reference/launcher.md), [source-map](reference/source-map.md) |
| C7 | `--noinject` uses the patched exe; launcher injects "into a running start" | Starts untouched `TY.exe`; injection starts suspended only if the copy is missing | [02-loading](how-it-works/02-loading-and-injection.md) |
| C8 | Status documents naming v0.3.2/v0.3.3 as current; "Balanced" preset name; 80/90 fps header; 2.2 or 2.1 haze | v0.3.5 line in CHANGELOG; Medium; launcher values | [history-and-timeline](decisions/history-and-timeline.md), [presets](reference/presets.md) |
| C9 | Build id `3ee6ea7` versus `4a2ef95`; `v0.3.5-rc` title strings on HEAD; CHANGELOG lacks v0.3.1 to v0.3.4 | Unresolved identifiers (N-25) | [history-and-timeline](decisions/history-and-timeline.md) |
| C10 | README dead link `docs/FINDINGS_ROUND4.md` | `docs/recording/FINDINGS_ROUND4.md` | [history-and-timeline](decisions/history-and-timeline.md) |
| C11 | `START_HERE` env-var list omits `TY_PERFCAM`, `TY_LOCK_TOKEN`; says post RTs 1024x512 and MSAA 2x | Not in code | [test-harness](guides/test-harness.md), [06-render-targets](how-it-works/06-render-targets-depth-and-state.md) |
| C12 | Issue status conflicts: #3 closed or open, #8 closed or open, #12/#14 seed text, #64 to #122 titles (N-19) | Needs the tracker | [known-issues](decisions/known-issues.md), [contributor-onboarding](guides/contributor-onboarding.md) |
| C13 | Shipped ini comment "Tess/relief distance LOD not wired" and the misplaced `[fog]` comments under `[light]`; `package.ps1` says "Balanced" | Cosmetic | [settings-ao-gi-lod](reference/settings-ao-gi-lod.md), [settings-atmos-fog-emit](reference/settings-atmos-fog-emit.md) |
| C14 | `tyLt[6]` in plan and docs | Declared `tyLt[8]`, indices to 7 | [shaders/README](shaders/README.md), [natural-light](shaders/natural-light.md) |
| C15 | Cost totals differ between PERFORMANCE, STATUS, VERDICTS, launcher comments (Medium 12.08 / 11.1 / 10.87 ms; stock 5.7 / 4.6 ms; GI, tess, legacy GI figures) | Each page names its build and source | [presets](reference/presets.md), [gi-hiz](shaders/gi-hiz.md), [tessellation-and-relief](shaders/tessellation-and-relief.md) |

## See also

- [Index](README.md)
- [Known issues](decisions/known-issues.md)
- [Roadmap and open work](decisions/roadmap-and-open-work.md)

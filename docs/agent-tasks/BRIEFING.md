# Briefing for tyrender agents (written session 5 2026-10-12; harness section updated 2026-10-13)

> **Status 2026-10-13 (evening):** v0.3.2 preview / community beta is released; v0.3.3 is a test build awaiting the user's playtest (texture tags, GI source mask; see `RESEARCH\RESUME.md`). Presets Low / Medium / High / Ultra; "Balanced" is retired, Medium is the old Balanced and the default. Entry point for humans and fresh sessions: `RESEARCH\START_HERE.md`, current state `RESEARCH\RESUME.md`, numbers `RESEARCH\STATUS_2026-10-13.md`. Your task file is `agents\tasks\<name>.md` (rules: `_common.md`). Max 3 agents at once. Characters (Ty, NPCs) are never relieved/tessellated and are masked out of GI/AO bounce; keep it that way.

## Project
We ship a drop-in graphics mod for **TY the Tasmanian Tiger HD** (PC, 32-bit OpenGL, compat profile, 2016 "Merkury" engine, build r4704_v1.44).
The mod is `tyrender.dll` (C++17, llvm-mingw i686) injected into a copy of the game exe (`TY_tyrender.exe`). It hooks GL calls, rewrites the engine's GLSL at
`glShaderSource`, and runs its own passes (AO, GI, atmosphere, post/TAA) right after the engine copies the scene depth. Hardware: **AMD RX 7600 XT**, Windows 11,
GL 4.6 compatibility, 2560x1440 windowed. Must also work on Intel/NVIDIA: keep to GL 3.3-4.5 features available in a compat context, check support at runtime, never crash.
Users also have an HD diffuse texture pack (no normal/roughness maps). Game code is the engine's; we cannot change engine draw submission.

## Source layout (`C:\CLAUDE\SRC\agents\<yourname>\` = YOUR private copy; the master is `C:\CLAUDE\SRC\tyrender\`, do NOT edit it or other agents' folders)
- `tyrender.cpp` hooks, shader rewrite order (PBR -> shadow patch -> LightRewriteFS), config loading, `.inl` includes. `ao.inl` GTAO + helpers (`CompileProg`, `MakeTex`, `LoadGX`, `gx.*`, `Invert4`, camera derivation).
  `gi.inl` SSGI. `atmos.inl` fog/shafts/contact shadows (half-res). `fx.inl` TAA/FXAA/grade. `tess.inl` PN/Phong tessellation + texture relief. `pbr.inl` bump+GGX.
  `light.inl` natural light (shadow tint, hemisphere, foliage, shadow filter patch in tyrender.cpp), `debugcam.inl` test camera, `gpuprof.inl` GPU timers.
- Engine facts: the scene is rendered into FB0 (default framebuffer, MSAA via pixel format); after the main pass the engine does glCopyTexImage2D of colour then depth (our passes run at the depth copy,
  `h_glCopyTexImage2D` in tyrender.cpp). Standard shader template: `RESEARCH\shaders\orig\Shaders\standard.shader`; dumped variants in `<game>\tyrender\dump\*.glsl`.
  Lighting = vertex colour * light matrices; shadows = 2-3 cascade `sampler2DShadow` + `shadowAmount*0.5` darkening. World: Y up, units ~cm (Ty ~ 100-150 units tall).
- Config: `tyrender.ini` next to the game; reload at runtime by creating `tyrender\reload` (the test harness does not do this: pass the ini you want).

## Build
`powershell -NoProfile -File C:\CLAUDE\SRC\agents\<name>\build.ps1`  -> `out\tyrender.dll` (+ launcher). It builds from its own folder. Compile errors show as clang output. Never use `-Install`.

## Testing: ONLY through the shared harness (one game at a time, global lock; the game window must be foreground)
`powershell -NoProfile -File C:\CLAUDE\TOOLS\agent_test.ps1 -Dll C:\CLAUDE\SRC\agents\<name>\out\tyrender.dll -IniFile <your ini> -Snaps "a,b" -Perf -OutDir C:\CLAUDE\RESEARCH\shots\agents\<name> [-Cam "dx dy dz yaw pitch"] [-Keys "W,W"]`
- Waits for the lock (other agents test too; a run takes 1-3 minutes), swaps in your DLL + ini, boots to the test spot, takes snapshots (PNG 2560x1440 in OutDir), prints `PERF frame X ms | ao gi atmos post | engine+rest` (GPU time from the DLL's timestamp queries), `LOG <path>` (copy of tyrender.log: grep it for `[gpu]`, `FAILED`, `does not compile`, your own log lines), and ALWAYS restores the game folder. Do not launch the game any other way. Do not run it more often than needed; batch several snapshot names per run.
- Pass `-IniFile` as a full ini file you write (e.g. under `C:\CLAUDE\RESEARCH\agents\<name>\`). A minimal ini with only your sections is fine (other keys default). To isolate your pass, switch the others off (`[ao] Enable=0`, `[gi] Enable=0`, `[atmos] Enable=0`, `[tess] Enable=0`, `[post] Enable=0`, `[pbr] Enable=0`, `[shadows] Patch=0`) - this baseline costs 4.6 to 5.7 ms depending on the view (stock game 5.7 ms at the default view).
- Always A/B with the GPU timers (never wall-clock): same ini except your switch; compare `PERF` lines. Timings are stable to ~0.1 ms. Current preset costs (v0.3.2, 2560x1440, RX 7600 XT): Low 6.0, Medium about 12, High about 18, Ultra about 31 ms; the 60 fps budget is 16.7 ms (see `RESEARCH\STATUS_2026-10-13.md`). The old "Ultra reference 110 ms" figure from session 5 is obsolete.
- Read PNGs with the Read tool to judge them visually; crop/zoom with System.Drawing in PowerShell if needed (see `C:\CLAUDE\TOOLS\imgdiff.ps1`). Do not describe an image you have not actually viewed.
- Test views: default = Ty at the Rainbow Cliffs start (sand path, wooden fence left/right, rope railings, red rock wall behind). Close-ups: `-Cam` offsets (see list below, filled in by the lead). Sand ground, wooden fence and ropes are the user's reference materials for relief/PBR.
- PowerShell pitfalls: variable names are case-insensitive (`$ini` == `$Ini`); `powershell -File` cannot take array parameters (use comma strings).

## Rules
- Work only in your folder. Report any needed change to `tyrender.cpp` precisely (diff text in your REPORT) - the lead merges; prefer to keep all your code in your own `.inl` file(s). New `.inl` files must be `#include`d from your copy of tyrender.cpp (note it in the report).
- Safety: any shader you generate must compile or fall back to the original (the engine int3-crashes on a bad shader). GL calls must be guarded (extension/version checks, `failed` flags). Restore all GL state you change (see existing passes for the save/restore pattern).
- Quality bar: equal or better look than the current implementation at lower GPU cost, verified with before/after screenshots + timings. If an idea from the research PDF does not pan out (slower, artifacts), say so and keep what works. The PDF's millisecond figures are projections, not measurements - trust only the GPU timers.
- Keep config keys backwards compatible (add new keys with sane defaults to `[section]` of tyrender.ini; document each).
- Final deliverable: (1) your modified files in your folder, building cleanly; (2) `C:\CLAUDE\SRC\agents\<name>\REPORT.md`: what you changed, new ini keys (+defaults), before/after PERF numbers (same scene, same ini otherwise), screenshot paths with a one-line verdict each, known issues, exact `tyrender.cpp` changes needed, and ideas not done. Keep the final chat reply short (the report is the record).

## Test views (use with `-Cam "dx dy dz yaw pitch"`; positive pitch looks DOWN; camera frame offsets in world units; the camera must stay above the ground)
Default view (no -Cam): Ty at the start spot, rock wall, fences left/right, rope railings, bridge in the distance, distant canyon. Reference sheet of the views below: `C:\CLAUDE\RESEARCH\shots\agents\main\sheet_g.png` (g1..g8).
| name | -Cam | shows |
|---|---|---|
| sand_wide (g1) | `0 0 450 0 12` | sand path with cracked pattern, rope railings at the top corners, bridge |
| sand_mid (g2) | `0 -40 450 0 15` | sand + the red rug |
| sand_close (g3) | `0 -80 450 0 20` | very close grazing sand/crack profile |
| sand_graze (g6) | `-100 -60 700 -15 12` | cracked sand at a grazing angle (best for relief/profile) |
| fence_mid (g4) | `-250 -40 350 -35 12` | wooden fence + sign + sand |
| fence_close (g5) | `300 -40 350 35 12` | wooden fence rails close, distant canyon behind |
| vista (g8) | `-400 -60 200 -60 10` | far rock formation + haze (atmosphere test) |
Several views in one game session (recommended, saves minutes): `-CamSeq "sand_wide=0 0 450 0 12;fence_close=300 -40 350 35 12"` (+ `-CamWait 4`); each name becomes `<name>.png` in OutDir. Add your own views by scanning with -CamSeq (rope railings are the thick twisted ropes at the corners of sand_wide; scan closer to them).
Ty can move/animate between runs: when diffing images, mask Ty and foliage. Keep the camera still for A/B pairs (same -Cam both times).

## LOGGING (mandatory, added by the user's request)
After EVERY meaningful step (design decision, successful/failed build, each test run WITH its PERF numbers and screenshot paths, finding, failure, idea dropped) call
`powershell -NoProfile -File C:\CLAUDE\TOOLS\steplog.ps1 -Agent <yourname> -Msg "what happened + numbers" [-Files "comma,list,of,files"]`
It appends to `C:\CLAUDE\RESEARCH\sessions\2026-10-12_session5\agents\<yourname>.md` (the working-directory research log) and syncs it, plus any -Files (source/report), to OpenViking.
Do it at least after: plan chosen, first successful build, each A/B measurement, each visual verdict, and at the end (-Files your source .inl files + REPORT.md). Keep every entry self-contained (a future session must be able to continue from your log alone).
Your rate-limit/interruption safety: if you get cut off, the log + your source folder must be enough to resume. Also keep `REPORT.md` current as you go, not only at the end.
Harness notes: agent_test.ps1 now self-heals the game folder after an interrupted test; it holds a global lock, so a run can wait several minutes for others.

## Current harness (2026-10-13) - use these instead of calling `agent_test.ps1` repeatedly
All in `C:\CLAUDE\TOOLS`, all zero-LLM-token, all serialised by the same global lock, all restore the game folder:
- `suite.ps1 -Name <n> -Dll <dll> -AB "a=<ini>;b=<ini>" [-Views] [-NoPerf]` - in-session A/B over the standard views: `results\<n>\sheet.png`, `ty_strip.png`, `manifest.json`, ledger line. View the sheet and strip (2 images) rather than every PNG. Only live-reloadable ini keys can differ between variants (the first variant's ini decides which shaders are rewritten: put the Enable=1 variant first).
- `motion.ps1 -Name <n> -Dll <dll> -AB "a=<ini>;b=<ini>" [-Step 0.12 -Frames 16 -Cam "x y z yaw pitch" -Reps 2]` - flicker under camera motion (DLL smooth yaw, temporal 2nd difference, Ty excluded) -> `results\<n>\motion.md` + heat maps. Acceptance used: each pass <= 1.5x the `off` variant, cost +<= 1 ms. A `warm` first variant avoids a warm-up artefact. It runs at high fps; it does not emulate a slow GPU.
- `flicker.ps1 -Name <n> -Dll <dll> -AB "..." [-Cam] [-Frames 10]` - static-camera temporal std per variant -> `results\<n>\flicker.md`.
- `abshots.ps1 -Level 1 [-Slot 2] [-Name <folder>]` (community A/B shots, one session per preset) and `match_ab.ps1` (cutscene frame matching); `launcher_shot.ps1 -Out x.png [-Click "x,y"] [-Hover "x,y"]` (launcher UI screenshot); `g1_gate.ps1`, `soak.ps1`, `cost_table.ps1`, `perf_matrix.ps1` (lead/auto items).
- Environment variables read by `ty_play.ps1` / `agent_test.ps1` (set them in the calling process): `TY_SLOT=2` boots save slot 2 (Billabong, level 2), default is slot 1 (Rainbow Cliffs start, the standard test spot); `TY_SETTLE=<s>` seconds to wait after the game reaches gameplay before the first snapshot (overrides `-Settle`; use 1-3 for cutscene captures); `TY_CAMWAIT=<s>` seconds to wait after each camera move (overrides `-CamWait`).
- Standard extra views beyond the table below: `rope_b=-60 0 400 25 6`, `vista=0 -60 300 -50 8` (the `suite.ps1`/`abshots.ps1` defaults; `vista` in the table is the older pose).
- Debug: `[debug] StateGuard=1` (GL leak log), `[debug] ForceFallback=<bits>`; renderer log `tyrender.log` in the suite output dir.
- Merging is the lead's job: `merge_agent.ps1 -Agent <atmos|gi|ao|tess|light>` into staging `SRC\merge`. Report exact `tyrender.cpp` changes (diff file next to REPORT.md).

## Test-run prerequisites (added 2026-10-12 10:05, still valid)
- Subprocesses started by agents are NOT allowed by Windows to take the foreground, so scripted keys silently went to Explorer. The lead runs C:\CLAUDE\TOOLS\focus_daemon.ps1 (hidden, brings the game window to the front while a harness test is running). If your run prints 'FOCUS LOST' or stays on the menu, tell the lead (do not start your own game). TY's menus select by mouse hover, so ty_play.ps1 positions the pointer on save slot GAME 1 itself (GAME 2 with `TY_SLOT=2`); do not move the mouse during a run. Start the focus keeper before testing: `Start-Process powershell -WindowStyle Hidden -ArgumentList '-NoProfile','-File','C:\CLAUDE\TOOLS\focus_daemon.ps1'`.


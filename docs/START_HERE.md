# START HERE - TY HD renderer (single entry point)

Updated 2026-10-13 19:23 (docs pass 2, after playtest 4 and the v0.3.3 test build). Read this file, then `RESUME.md` (current state, open items), then `PLAN_NEXT.md` (order of work), and `DOC_MAP.md` if you need to find a document. Everything older is in `archive\20261013\` (index: `archive\20261013\MOVED.md`).

## 1. What the project is
A drop-in graphics mod for **TY the Tasmanian Tiger HD** (PC, Steam build r4704_v1.44, 32-bit OpenGL, "Merkury" engine; TY.exe sha256 7DF4EA56...21E0). `tyrender.dll` is loaded into `TY_tyrender.exe` (a copy of TY.exe, imports patched, marked Large Address Aware; TY.exe itself is never modified). It hooks GL, rewrites the engine's GLSL at load and adds passes: AO (XeGTAO-style), bounce light / GI (Hi-Z), froxel atmosphere + contact shadows, shadow filter (4096/8192 maps), TAA/FXAA/sharpen/grade, texture-driven relief + PBR bump, natural-light tint, and per-texture tags from the texture file name. Launcher `TYLauncher.exe` has four presets: **Low / Medium / High / Ultra** ("Balanced" is retired; Medium is the old Balanced and the default). Goal and acceptance criteria: `GOAL.md`. The user's HD texture pack is separate (`<game>\PC_External`); texture pixel work is out of scope.
Targets: AMD, NVIDIA, Intel, Windows (ideally Linux/Proton). Only an AMD RX 7600 XT has ever been tested.

## 2. Current release and test build (details in RESUME.md / STATUS_2026-10-13.md)
- **Shipped: v0.3.2 preview / community beta**, zip `C:\CLAUDE\SRC\tyrender\dist\TYHD_Renderer_v0.3.2.zip` (dll sha1 prefix `6571393575`). G1 gate passed. Medium about 12 ms at 2560x1440 (stock 5.7 ms).
- **Test build: v0.3.3** (not released), zip `...\dist\TYHD_Renderer_v0.3.3.zip`, dll sha1 prefix `0753BB1B82`, DLL installed in the game folder (the game-folder ini is the Ultra test preset; the launcher exe there was not updated). G1 NOT run. Smoke: Medium 12.05 ms, High 19.80 ms.
- Open: T78 High/Ultra flicker (waiting for the user's playtest of v0.3.3), T73-T77 bug and feature list, T82 shadow bright lines (not fixed), T29 user sign-off. See `RESUME.md`.

## 3. Where everything is
| What | Where |
|---|---|
| Master source (build, README, PERFORMANCE, dist zips) | `C:\CLAUDE\SRC\tyrender\` (do not edit while agents work; merges go through staging `SRC\merge`) |
| Agent working copies and their `REPORT.md` | `C:\CLAUDE\SRC\agents\<name>\` |
| Tools (all scripts) | `C:\CLAUDE\TOOLS\` |
| Game working copy (patched, test target) | `C:\CLAUDE\TY the Tasmanian Tiger` ; Steam copy `C:\CLAUDE\STEAM\...` (byte-identical, untouched) ; originals `C:\CLAUDE\BACKUP\original` |
| Research, plan, status | `C:\CLAUDE\RESEARCH\` (this folder; map in `DOC_MAP.md`) |
| Plan and task board | `RESEARCH\todo.json` (the lead edits it; read via `todo_runner.ps1 -Status`); order of work `RESEARCH\PLAN_NEXT.md` |
| Task prompts for coding agents | `RESEARCH\agents\BRIEFING.md`, `agents\tasks\_common.md`, `agents\tasks\<name>.md` |
| Measured results (csv/json/png/manifests) | `RESEARCH\results\<suite>\`, `results\ledger.csv`; verdicts in `results\VERDICTS.md` |
| User playtest feedback | `RESEARCH\recording\` (`INDEX.md` = problem ids with status; `FINDINGS_ROUND4.md` = latest playtest) |
| Texture flags | `RESEARCH\recording\texture_tags.csv` (user-filled, 1933 textures) -> generated `SRC\tyrender\tex_tags.inc` by `gen_tex_tags.ps1` |
| Community A/B screenshots | `RESEARCH\AB_shots\` (level 1 + level 2), copy on `E:\TY_AB_shots` (Samsung T7; 80 images) |
| Step logs per session | `RESEARCH\sessions\<date>_sessionN\LOG.md` and `...\agents\<name>.md` |
| Reverse-engineering notes | `01_findings.md`, `decomp\`, `shaders\`, `rkv_index\`, Ghidra project `ghidra_project` |
| OpenViking mirror | `viking://resources/TECHNICAL/ty-hd-renderer/` (research docs under `research/docs/local-mirror/`, sessions under `research/sessions/<date>/`, source under `src/`) |

## 4. How to resume (fresh session, costs no LLM tokens)
```
cd C:\CLAUDE
claude --continue          # or a fresh session: everything needed is in these files
. C:\CLAUDE\TOOLS\env.ps1  # PATH / JAVA_HOME / GHIDRA_INSTALL_DIR (only for Ghidra / REA work)
powershell -NoProfile -File C:\CLAUDE\TOOLS\todo_runner.ps1 -Status   # the board
powershell -NoProfile -File C:\CLAUDE\TOOLS\status.ps1                # writes RESEARCH\STATUS_RAW.md (machine facts: logs, reports, perf lines, errors)
```
Check `claude mcp list`: `rea` should be connected (launcher `TOOLS\rea-mcp.cmd`). Start the focus keeper before any game test (hidden): `Start-Process powershell -WindowStyle Hidden -ArgumentList '-NoProfile','-File','C:\CLAUDE\TOOLS\focus_daemon.ps1'` (stop file `TOOLS\focus_daemon.stop`). Unattended runner for `auto` items: `Start-Process powershell -WindowStyle Hidden -ArgumentList '-NoProfile','-File','C:\CLAUDE\TOOLS\todo_runner.ps1'` (safe to restart; stop with the file `RESEARCH\runner.stop`).

Who does what: `auto` = scripts only; `agent` = coding agent (prompt: `Read C:\CLAUDE\RESEARCH\agents\tasks\_common.md then ...\tasks\<name>.md and continue; read your step log first.`); `lead` = the lead session; `director` = strongest model at checkpoints (`results\DIRECTION_*.md`); `user` = human decisions. After finishing a non-auto item: set `status` done + one-line `result` in todo.json (the lead does this), then `ovsync_all.ps1 -Quick`.

## 5. Tools (C:\CLAUDE\TOOLS, one line each; run as `powershell -NoProfile -File <tool> ...`)
Test and measurement (game tests: always through these, never launch the game by hand):
- `agent_test.ps1 -Dll <dll> [-IniFile] [-Snaps] [-Cam "dx dy dz yaw pitch"] [-Perf] [-OutDir]` shared harness: global lock, swaps DLL/ini in, boots to the test spot, snapshots + PERF line, restores the game folder (crash-safe backup in `TOOLS\_test_backup`). See trap on `-IniFile` in section 6.
- `suite.ps1 -Name <n> -Dll <dll> -AB "a=a.ini;b=b.ini" [-Views] [-NoPerf]` in-session A/B over the standard views -> `results\<n>\sheet.png`, `ty_strip.png`, `manifest.json`, ledger line. Live ini keys only per variant.
- `motion.ps1 -Name <n> -Dll <dll> -AB "a=a.ini;b=b.ini" [-Step 0.12 -Frames 16 -Cam -Reps 2]` DLL smooth-yaw capture, temporal 2nd difference (flicker under motion) -> `results\<n>\motion.md` + heat maps. `motion.req` options: `fwd`, `side` (moving camera), `sleepMs` (low-fps emulation; use for the T78 Ultra test).
- `flicker.ps1 -Name <n> -Dll <dll> -AB "..." [-Cam] [-Frames 10]` static-camera flicker per variant.
- `abshots.ps1 -Level 1 [-Slot 2] [-Name <folder>] [-Views]` community A/B shots: one fresh session per preset (vanilla/low/medium/ultra), 2560x1440 PNGs -> `AB_shots\<name>\<view>\<preset>.png`. `match_ab.ps1` matches cutscene frames across presets (level 2 pool).
- `launcher_shot.ps1 -Out x.png [-Ini] [-Exe] [-Hover "x,y"] [-Click "x,y"]` screenshot of the launcher UI (default ini name in the script still says preset_Balanced).
- `g1_gate.ps1 [-Dll]` release gate (soak 10 min + forced fallback + perf at 4 views); `soak.ps1 [-Minutes]`; `cost_table.ps1`, `perf_matrix.ps1`, `balanced_search.ps1`, `presets.ps1` (preset ini builders) produce the numbers in `STATUS_2026-10-13.md`.
- `perf.ps1`, `gpucmp.ps1`, `cfgs.ps1` older throughput/GPU-timer benchmarks (trust only `[gpu]`/PERF lines). `ty_play.ps1` boots to the test spot; `snap.ps1` in-game screenshot; `capframe.ps1` one-frame GL call log; `key.ps1` key input; `imgdiff.ps1`, `cropcmp.ps1`, `imgmetrics.ps1` image diff.
Build, merge, release:
- `SRC\tyrender\build.ps1` (never `-Install` from an agent folder); `merge_agent.ps1 -Agent <atmos|gi|ao|tess|light>` merge into staging `SRC\merge` (master untouched; cannot copy NEW files, see section 6); `release.ps1 -Version vX` promote staging to master (now also `.inc` files, not `launcher_ui.inc`), build, package, verify zip + LAA; `presets.ps1`.
- `gen_tex_tags.ps1` (in `SRC\tyrender`): regenerates `tex_tags.inc` from `RESEARCH\recording\texture_tags.csv`. Run it after the CSV changes, then build.
Project bookkeeping:
- `todo_runner.ps1` plan runner; `status.ps1` -> `STATUS_RAW.md`; `steplog.ps1 -Agent <n> -Msg "..." [-Files "a,b"]` append a step log + sync; `ovsync_all.ps1 [-Quick]` mirror to OpenViking (scope in section 5b); `ov.ps1` OpenViking helpers (`. ov.ps1; ovLs viking://`, `ovSync <local> <uri>` for one file); `wf.ps1` wayfinder tracker.
Reverse engineering: `rkvtool.ps1` (RKV list/extract/repack, no size limit), `shot.ps1` (window-only capture + log timeline), `ghidra_scripts\StringXrefDecomp.java`, `vm.ps1` (LAN Linux VM, not used: see rules), `crashwalk.ps1`, `infra.ps1`.

Env vars read by the harness (set per process): `TY_SLOT` (2 = boot save slot 2 = Billabong; default slot 1 = Rainbow Cliffs start; used by `ty_play.ps1`, set by `abshots.ps1`), `TY_SETTLE` (seconds to wait after boot before the first snapshot; overrides `-Settle`), `TY_CAMWAIT` (seconds to wait after each camera move; overrides `-CamWait`). `abshots.ps1` / `ab2_run.ps1` set them for cutscene captures (TY_SETTLE=1..3, TY_CAMWAIT=1). No new env vars in v0.3.3.

### 5b. OpenViking sync scope (ovsync_all.ps1 and single files)
- `ovsync_all.ps1` mirrors: RESEARCH root `*.md|*.txt|*.ini`; `sessions\**` (text); `agents\*` (root files only); `SRC\tyrender` (`.cpp .inl .inc .c .md .ps1`); `TOOLS\*.ps1` and `ghidra_scripts`; `SRC\agents\*` source. With `-Quick`, only files changed in the last 3 hours.
- NOT covered by the script: `recording\`, `archive\`, `agents\tasks\`. Sync those with one-file `ovSync` calls (same API as the script; see `ov.ps1`). URIs used: `research/docs/local-mirror/recording/<name>`, `research/docs/local-mirror/archive/20261013/<name>`, `research/agents/tasks/<name>`.

## 6. Rules and traps (learned the hard way)
- **Max 3 agents in parallel** (2 is safer). Game tests are serialised by the global lock; code work happens in `SRC\agents\<name>\`, never in the master. Merge via staging only. Never hardlink agent DLL/ini to the game folder.
- **Usage limits cut agents off mid-work** (17:49 today, resets 17:30). Each agent must have a step log; resume by SendMessage to its id or relaunch with a task file that reads its own step log first.
- **-IniFile is not read at game start.** Only the `-AB` variants of `suite.ps1`/`motion.ps1` apply. Restart-only keys (`[shadows] MapScale/Taps`, `[tess] Enable/Scope/MaxLevel`, `[pbr] Enable`) need the ini copied into the game folder before the run. Otherwise the test silently uses the old values (this invalidated the Ultra shots of the light agent once).
- **merge_agent.ps1 cannot copy NEW files** (copy them by hand, e.g. `tex.inl`, `tex_tags.inc`, `gen_tex_tags.ps1`). **Whole-file CRLF conflicts** fail: use `diff -u` + `patch` on the hunks, keep a `.bak_pre_*` of the target first.
- **The user's open `TYLauncher.exe` locks the game-folder launcher exe**, so `release`/install cannot update it. Ask the user to close the launcher before an install.
- **Mouse and focus:** TY menus select by mouse HOVER; do not move the mouse over the game during runs. The game window must be foreground (`focus_daemon.ps1` runs for that). Computer-use screenshots and key presses cannot see or drive the GL window: use snap.ps1/key.ps1 and the harness. Never capture the whole desktop (privacy): window-only/in-process snapshots.
- **Shell traps:** PowerShell 5.1 variables are case-insensitive (`$ini` == `$Ini`); `powershell -File` cannot take array parameters (use comma strings); `Set-Content`/`Get-Content` without `-Encoding` corrupt UTF-8; **Bash heredocs and Bash command lines collapse backslashes** - pass Windows paths through PowerShell or the Write/Edit tools, not Bash; shell here-strings in PowerShell need the closing `'@` at column 0.
- **OpenViking key:** only in the Windows user environment variable `OPENVIKING_API_KEY` (server http <local-openviking-host>). Never write it to a file (the old `openviking.env` was deleted), never print it. Edit docs in place; never create `_v2` copies.
- **Measuring:** use only the DLL's GPU timer lines (`[gpu]` / PERF) and total frame ms; per-pass timers under-count (hidden full-res copy/resolve cost). Compare in-session A/B only; images from different sessions are not pixel-comparable. Look at images before giving a visual verdict. Static and yaw tests run at high fps and do NOT reproduce the user's flicker; use `sleepMs` for slow-GPU emulation.
- **No credentials in files.** No VM testing (user decision 2026-10-11, laptop/this PC only; the NVIDIA VM `cachyvm` at <local-openviking-host> exists but is parked). Never delete: move to `archive\` or keep `*.bak_*`.
- Always restore original game files after tests (the harness does); `TY.exe` is never modified; `settings.ini` backups exist in `BACKUP\original`.
- Fallback policy in the shipped DLL: a failing effect is switched OFF (no slow legacy path). `[debug] ForceFallback=<bits>` simulates failures; `[debug] StateGuard=1` logs GL state leaks; `[debug] ShadowView` and `PlaneBias / PlaneClamp / SlopeBias / SkinBias` are light debug keys (all off by default).
- Build env: llvm-mingw i686; GL 3.3-4.5 compat-context features with runtime probes; no vendor-specific calls without guards.
- Texture tags: the DLL reads the texture file name from the game's file reads (kernel32 hooks) and names the GL upload; a per-draw `tyTex` uniform carries the flags. Names not in `tex_tags.inc` get no flags. The API is `TexIsEmissive`, `TexIsCharacter`, `TexIsTransparent`.

## 7. Engine facts worth knowing (full detail: `01_findings.md`, `archive\20261013\00_RESUME_HERE.md`)
- Shaders: 17 `.shader` files in `Override_PC.rkv` (real RKV; repack has no size limit). Game GLSL 120/150, compile errors go to `TyOutputLog.txt` then int3. The DLL rewrites shader text at `glShaderSource` (order: PBR -> shadow patch -> light rewrite; skinned shaders get `[tex] SkinnedByTag=1`).
- Pipeline: scene into the default framebuffer (MSAA by pixel format, 2x default), LDR, post RTs for engine AO/bloom fixed at 1024x512; the engine copies colour then depth (`glCopyTexImage2D`) and our passes run at the depth copy (`h_glCopyTexImage2D` in `tyrender.cpp`). GI source mask: `GIMaskBegin/End` around glDrawElements/glDrawArrays.
- Key addresses (ImageBase 0x400000): GL init 0x005b9ea0 (compat context, no ARB attribs), engine init 0x0059e0d0, ShaderManager ctor 0x005c0950, shader compile wrapper 0x005d03f0, Game_Init (RTs) 0x0050a9f0, material parser 0x0059f240.
- Fast boot switches: `-skipvideo -novsync`; `-level z1` no longer works in windowed mode (the harness loads the save instead). Other switches: `-DebugModeCamera x,y,z,rx,ry,rz -StopFrame N -fastLoad -tydev -disableEnemies` ...
- `settings.ini` (Documents\TY the Tasmanian Tiger): game runs 2560x1440 windowed on this PC; engine `SSAO=true` stalls the menu (unexplained, not used).
- Game log: `<game>\TyOutputLog.txt`; renderer log: `<game>\tyrender.log` (`[gpu]`, `FAILED`, `does not compile`). Debug camera: `<game>\tyrender\cam.txt` ("dx dy dz yaw pitch"; positive pitch looks down), harness `-CamSeq` views. Standard views (sand_wide, sand_mid, sand_close, sand_graze, fence_mid, fence_close, vista, rope_b ...): table in `agents\BRIEFING.md`.
- Community context: TygerFramework (ElusiveFluffy) plugin loader, PC_External loader (see `discord_chat_text.txt`); our DLL is injected via a patched exe copy, not a proxy DLL.

## 8. Methods kept from the research (full text: `archive\20261013\NEXT_STEPS_RESEARCH.md`)
- GL state leak after a custom pass: ini bisect per pass first, then `[debug] StateGuard=1`, then add the matching save/restore; apitrace only if that fails (RenderDoc cannot replay the compat profile).
- Merging parallel agent copies: files owned by one agent are copied, shared files (`tyrender.cpp`) are three-way merged with `git merge-file` (`merge_agent.ps1`). One agent per merge, regression suite after each.
- A/B checks: in-session toggle pairs, A/A noise floor first, mask Ty and foliage, masked metric plus a human look (NVIDIA FLIP is an option, threshold not calibrated).
- Unattended work: state-file queue (`todo.json`), idempotent jobs, graceful stop on rate limit, scripts for everything measurable, LLM only for code and visual judgement.

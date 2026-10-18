# Game and engine facts

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A, B, C.

## Purpose

This page lists what is known about the game the mod targets, and the engine inside it, as far as the repo can show. Every fact carries a confidence label:

- **code** means the claim was checked against a file and line in the repo.
- **doc** means a repo document states it, but no code in the repo was found to confirm it.
- **inferred** means a reasoned guess from the code.
- **NI** means NEEDS INVESTIGATION. The repo cannot answer it. The method to find out is given.

The engine itself is closed. Almost everything about it was learned by reverse engineering that is not in the repo (see N-2 and N-17).

## How it works

The mod sees the game only through its OpenGL calls, its file reads and its shader source. It does not read engine data structures directly. That is why most facts below are about the GL state and the shader text, not about game logic. The hook layer is covered in [how-it-works/03-gl-hooking.md](../how-it-works/03-gl-hooking.md).

## Facts by topic

### Target build and executable

| Fact | Label | Source |
|---|---|---|
| Steam "HD" build, labelled `r4704_v1.44`, 32-bit | doc | `docs/START_HERE.md:6`, `docs/agent-tasks/BRIEFING.md:6` |
| Built against the 2020-04-04 `TY.exe` (date, not a hash). Other builds untested | doc | `README.md:226` |
| `TY.exe` SHA-256 prefix recorded as `7DF4EA56...21E0` | doc. No code in the repo compares a hash. | `docs/START_HERE.md:6` |
| The patch checks only the DOS and PE signatures and the 32-bit optional-header magic, and refuses an input that already has a `.tyimp` section | code | `src/patch_exe.inc:11`, `src/patch_exe.inc:13` |
| The launcher and installer do not check the exe size, date or hash. They only check that the file exists | code | `src/TYLauncher.c:55-56`, `src/installer/setup.c:102-103` |
| Which builds are supported | NI (N-7) | No list in the repo |

How to verify: compute the hash of `TY.exe` in the game folder and compare with the prefix. To test another build, copy the folder and run the launcher's Patch step, then play.

### Engine identity and history

- The name "Merkury" appears in a script comment: `tools/rkvtool.ps1:1` says "Krome Merkury, Ty 1 PC". `docs/agent-tasks/BRIEFING.md:6` calls it a "2016 Merkury engine". Both are **doc** claims. The 2016 date has no source in the repo.
- A web search for the engine name returned pages about Krome Studios as the developer of the TY games (for example the [Wikipedia article on the series](https://en.wikipedia.org/wiki/Ty_the_Tasmanian_Tiger) and the [StrategyWiki entry on Krome Studios](https://strategywiki.org/wiki/Krome_Studios)). The search results did not confirm the engine name "Merkury" or its history. Status: **NI** (N-1). Method: find a dated source that names the engine, or ask the owner for the notes.

### Graphics API and GL context

- The game uses OpenGL. The mod logs vendor, renderer, GL version and GLSL version once, at the first `wglMakeCurrent` (`src/tyrender.cpp:489-499`), and again into the session report (`src/sesslog.inl:240-245`).
- The mod hooks `wglCreateContext` only to log it (`src/tyrender.cpp:485-486`, `src/tyrender.cpp:668`). The mod's own code does not call `wglCreateContextAttribsARB` (grep finds no use). **inferred:** the game creates its context without ARB attributes, so the context is a compatibility profile. The address `0x005b9ea0` for GL init is given in `docs/START_HERE.md:86` as an owner RE result, not checked here. Status: **doc / inferred**; the exact profile is **NI** (N-2).
- The mod uses GL functions from core GL 1.x through the import table (`src/tyrender.cpp:665-667`), and GL 2.0 and later functions through `wglGetProcAddress` (`src/ao.inl:37-41`).

### Shader system

- The engine is shader-based, not fixed-function. Shader text from the game contains `#if (LIT==1) || (OMNI==1)` and `diffuseColour *= light;` (`src/pbr.inl:60-61`), which only makes sense for GLSL programs. **code.**
- The engine's GLSL version is given as 120 and 150 in `docs/START_HERE.md:84`. **doc.** The mod's own shaders use 330, 400 and 430 (see the Q023 table below). The game's shader versions can be confirmed by reading the `#version` line of a dumped shader in `<game folder>\tyrender\dump` (set `[debug] DumpShaders=1`, which is the default in code at `src/tyrender.cpp:60`).
- Shader files: the repo doc says the game stores 17 `.shader` files in `Override_PC.rkv` (`docs/START_HERE.md:84`). **doc.** The names and variants are **NI** (N-3). Method: `tools/rkvtool.ps1 -Mode list` on that archive, or dump the shaders at run time.
- The RKV archive format is documented in the header of `tools/rkvtool.ps1:1-4`. The script lists, extracts and repacks RKV files. The layout note credits a third-party reference in the script header. **code.**

### Textures

- Textures come from two places: loose files in `PC_External` (`<game folder>\PC_External\<name>.dds`) or offsets inside `Data_PC.rkv`. The mod names each texture by watching the game's file reads (`src/tex.inl:2-4`). **code.**
- Upload hooks: `glCompressedTexImage2D` and level-0 `glTexImage2D` (`src/tex.inl:4`). Whether the engine uploads DXT-compressed data for all textures is not confirmed in the repo. **NI** (Ch04 detail).
- Texture-name tables: `src/tex_tags.inc` is generated and has 2423 name entries (`grep -c` on `src/tex_tags.inc`, lines beginning with `{"`). See [material-model.md](material-model.md).

### Render targets and frame structure

- The game draws the scene into the default framebuffer, with multisampling set by the pixel format. `docs/START_HERE.md:85` says "MSAA 2x by default" and "post RTs for engine AO/bloom fixed at 1024x512". **doc.** The mod does not create these; no 1024 constant appears in mod code. Method: the mod logs every render-target allocation with its size and sample count (`src/tyrender.cpp:276`, `src/tyrender.cpp:325-326`), so `tyrender.log` shows the real values.
- Frame order, as the mod hooks it:
  1. The game copies colour and depth out of the default framebuffer with `glCopyTexImage2D` (`src/tyrender.cpp:280-287`).
  2. At the depth copy the mod runs AO, GI, emitter light and atmosphere (`src/tyrender.cpp:290-293`).
  3. If the mod changed the scene, it re-copies the colour texture so that the game's bloom reads the changes (`src/tyrender.cpp:294-299`).
  4. The final image passes run at the first `glViewport(64, 64)` after that, which is the start of the game's brightness downsample (`src/tyrender.cpp:254`, `src/fx.inl:2-3`).
- The engine's own frame description is referenced in code comments as a file in a research folder that is not in the repo (`src/ao.inl:2`, `src/fx.inl:2`). **NI** (N-4). Method: capture one frame with `[debug] CaptureFrame=N` and read the logged calls (`src/tyrender.cpp:58`, `src/tyrender.cpp:622`).

### Uniforms the mod relies on

The mod finds these engine uniforms by name when the program is queried (`src/tyrender.cpp:340-342`):

| Name | Used for | Source |
|---|---|---|
| `VPMatrix` | camera matrix, tracked for passes | `src/tyrender.cpp:340` |
| `shadowVPMatrix1`, `shadowVPMatrix2` | cascade shadow matrices, captured for atmosphere | `src/tyrender.cpp:341`, `src/atmos.inl:10` |
| `shadowTexture1`, `shadowTexture2` | the two cascade shadow maps | `src/tyrender.cpp:341`, `src/tyrender.cpp:94` |
| `shadowDir` | sun direction | `src/tyrender.cpp:341`, `src/atmos.inl:395` |
| `fogColour` | fog colour | `src/tyrender.cpp:341`, `src/atmos.inl:397` |
| `fogCoords` | engine fog parameters, overridden by the mod | `src/tyrender.cpp:341`, `src/tyrender.cpp:404-425` |
| `diffuseTexture` | albedo sampler unit | `src/tyrender.cpp:341` |
| `pushOutSpheres`, `grassTextureUVOffset` | identify the grass vertex template | `src/tyrender.cpp:376` |
| `lightDirMatrix`, omni lights | engine lighting that the relief normal must feed (mentioned in comments; the mod checks for the string in shader text) | `src/pbr.inl:4-5`, `src/light.inl:37` |

### Fog

- The engine fog is a four-value uniform. `FogOverride` scales the distance term by `[fog] Distance` and the strength by `[fog] Amount`, then keeps a copy for the atmosphere pass (`src/tyrender.cpp:404-408`). The formula is written in the comment on `src/tyrender.cpp:404` as `v_fog = x + clamp((viewDepth - z) * w) * y`. **code** (comment and body agree on the scaling).
- The override runs when the game sets `fogCoords`, via `glUniform4f` and `glUniform4fv` hooks (`src/tyrender.cpp:409-425`).

### Scale and camera

- `src/atmos.inl:7` says world units are about centimetres, with a camera height near 2744 and a near plane of 15. **inferred** from a comment. No constant in the code sets these values. The near plane is derived from the projection matrix at `src/ao.inl:575-582`.

### Command-line switches

- The mod's tools start the game with `-skipvideo` (`tools/playtest/autoplaytest.ps1:52-53`) and `-novsync` (`tools/perf.ps1:1`, `tools/gpucmp.ps1:5`). **code** (tools, not the game).
- `src/pbr.inl:229` mentions `-DebugModeCamera` in a comment. **doc.**
- `docs/START_HERE.md:87` lists `-StopFrame`, `-fastLoad`, `-tydev` and `-disableEnemies`, and says `-level z1` no longer works in windowed mode. **doc.** Not used in any code in the repo.
- The launcher has its own switches: `--play`, `--wait`, `--noinject` (`src/TYLauncher.c:5`). Everything else is passed to the game (`src/TYLauncher.c:6`).

### Files the game and mod read and write

| File | Who uses it | Source |
|---|---|---|
| `settings.ini` in `Documents\TY the Tasmanian Tiger\` | Read only, by the clash guard, to check `[Display] SSAO` and `MSAA` | `src/tyrender.cpp:642-643`, `src/tyrender.cpp:650-656` |
| `TyOutputLog.txt` | The game's own log. Not referenced by mod code. Compile errors go there (`docs/START_HERE.md:84`) | **doc** |
| `tyrender.log` in the game folder | Written by the mod, opened fresh each start | `src/tyrender.cpp:708` |
| `tyrender.ini` in the game folder | Read by the mod at load and on reload | `src/tyrender.cpp:56-64`, `src/tyrender.cpp:597` |
| `tyrender\logs\TYHD_report_*.txt` | Session report, written locally | `src/sesslog.inl:258-264` |
| `Screenshots\` in the game folder | F11 A/B pairs | `src/abshot.inl:92-93` |
| Saves | Not in the repo. The save slots are referenced in tool scripts only (`docs/START_HERE.md:61`) | **NI**. Method: inspect the game's save folder. |

### Settings from the game's `settings.ini`

The mod reads only `[Display] SSAO` and `MSAA` (`src/tyrender.cpp:645-656`). When `SSAO=true`, it logs a warning that the menu can stall (`src/tyrender.cpp:650`). The cause of the stall is **NI** (N-5). Method: change the value in the game's settings and watch the menu with the log on.

### Platform, GPU and threads

- Tested on one card: AMD Radeon RX 7600 XT on Windows 11 (`README.md:11`, `docs/GOAL.md:18`). The goal names AMD, NVIDIA, Intel and Linux/Proton as targets (`docs/GOAL.md:6`). **doc.**
- Threading: the mod creates no threads. A search of `src` for thread creation (`CreateThread`, `_beginthread`, `std::thread`) finds none. **code.** The engine's threading is **NI** (N-8). Method: log `GetCurrentThreadId()` from the hooks, or check the GL calls from another thread.
- Linux and Proton: **NI** (N-10). The gamepad support added for Big Picture and Steam Deck is listed as untested (`CHANGELOG.md:27`).

### GPU features the mod needs

These are the gates found in code. Each row is checked in code unless marked otherwise.

| Feature | Needed by | Gate in code | Source |
|---|---|---|---|
| GLSL 3.30 | Most passes (AO, GI, atmosphere, emitter, FX) | Shader text; compile failure switches the pass off | `src/ao.inl:95`, `src/fx.inl:8`, `src/gi.inl:133`, `src/gi2.inl:354`, `src/emit.inl:64` |
| GL 4.0 and GLSL 400 | Tessellation | Version parsed from `GL_VERSION`; below 4.0 tessellation is skipped when `[portability] TessVersionGate=1` (default) | `src/tess.inl:179`, `src/tess.inl:397`, `src/tess.inl:76-84`, `src/default_tyrender.ini:101-102` |
| Tessellation entry points | Tessellation | Checked once with `wglGetProcAddress`; tessellation stays off if one is missing | `src/tess.inl:87-95` |
| GL 4.3 and GLSL 430 | Froxel atmosphere (compute) | Version string parsed; below 4.3 the compute path is not used | `src/atmos.inl:160`, `src/atmos.inl:258-260` |
| Compute entry points and image load/store | Froxel atmosphere | Each entry point fetched and checked; a compile or link failure is logged | `src/atmos.inl:261-270`; `glBindImageTexture` at `src/atmos.inl:242` |
| Timer queries (`GL_TIMESTAMP`) | GPU profiling | `glGenQueries` and `glQueryCounter` must be present, or profiling is marked failed | `src/gpuprof.inl:11-13`, `src/gpuprof.inl:20` (token `0x8E28`) |
| Extension strings | Session report only | Listed in the report; the gates above use versions and entry points, not these strings | `src/sesslog.inl:251-253` |

What happens when a gate fails is covered in [how-it-works/11-fallbacks-and-failure-handling.md](../how-it-works/11-fallbacks-and-failure-handling.md) and in [constraints-and-limits.md](constraints-and-limits.md). In short: AO and the atmosphere are switched off, and the atmosphere has one exception described there.

**Minimum hardware:** no minimum is stated anywhere in the repo. **NI** (proposed N-29, not in the plan list). Method: test on GL 3.3 and GL 4.0 hardware or with a forced-fallback run (`[debug] ForceFallback`, `src/tyrender.cpp` and `README.md:318`).

### Game-side settings the mod does not touch

- The game has its own detail settings, including "reflections" (`src/launcher_ui.inc:88`, `src/launcher_ui.inc:474-475`). The launcher reads and writes these values (`src/launcher_ui.inc:84-107`). The mod adds no reflections of its own. Screen-space reflections are not implemented (see constraints).

## Cost

Not applicable to engine facts. Costs are on the reference pages and in [what-is-this.md](what-is-this.md).

## Limits

- The repo has no engine source and no engine documents. Every engine fact is either a reverse-engineering result quoted by a doc, or a code observation.
- Facts from the owner's reverse-engineering notes are not available: `01_findings.md`, `decomp\`, and the Ghidra project are named in the docs but are not in the repo (N-17).

## Known issues

- Engine SSAO on: the menu can stall (N-5, `src/tyrender.cpp:650`).
- Engine fog and the mod's fog: see [shaders/atmosphere-fog.md](../shaders/atmosphere-fog.md) (hue stability #3, `CHANGELOG.md:42`).
- The mod's tools hard-code the owner's folder layout (N-13). The repo's `src/gen_tex_tags.ps1:4`, `:10`, `:21` name owner paths. This is a limit on building and testing, not on play.

## How to verify

- Vendor, renderer, GL and GLSL strings: open `tyrender.log` after one start and read the lines at `src/tyrender.cpp:495-498`.
- Shader list: set `[debug] DumpShaders=1` and look in `<game folder>\tyrender\dump`.
- Render-target sizes and MSAA: search `tyrender.log` for `glTexImage2D` and `glRenderbufferStorageMultisample` lines.
- Frame order: set `[debug] CaptureFrame=N` (or create the `cap.req` trigger file, `src/tyrender.cpp:624-626`) and read the logged calls.

## NEEDS INVESTIGATION

- N-1 Engine name and history. Method: a dated source for "Merkury"; the web search above did not confirm it.
- N-2 How the GL version and compatibility profile were found. Method: owner's RE notes, or the `GL_VERSION` line in `tyrender.log`.
- N-3 The 17 shader files, their names and variants. Method: dump the shaders, or list `Override_PC.rkv` with `tools/rkvtool.ps1`.
- N-4 Full engine frame anatomy. Method: capture a frame and read the call log.
- N-5 Why engine `SSAO=true` stalls the menu. Method: experiment with `settings.ini` and read `TyOutputLog.txt`.
- N-7 Supported `TY.exe` builds. Method: read the launcher, then test other builds if available.
- N-8 Engine threading. Method: log thread IDs from the hooks.
- N-10 Linux and Proton behaviour. Method: a Proton run.
- N-17 RE artefacts missing from the repo. Method: owner decision on publishing sanitized copies.
- N-29 (proposed) Minimum GPU and GL requirements. Method: forced-fallback runs and tests on other hardware.
- Saves location and format. Method: inspect the game's save folder.

## Discrepancies

- Plan Q025 (in the plan's section 2.2) says the DLL is loaded "not remote-thread injection". `src/TYLauncher.c:2-4` and `src/TYLauncher.c:61-70` describe a fallback that starts the game suspended and injects with `CreateRemoteThread` and `LoadLibraryW`. The primary path is the import patch. This belongs to Ch02, recorded here for the reviewer.
- `src/tyrender.cpp:1-4` (header comment) says "Stage 1 (this file): GL call logger" and "No inline hooks". The file now does more than log. The header is out of date.
- `README.md:265` says a failing effect switches off and "the renderer does not fall back to a slower path". The atmosphere code has an exception: a ray-march path runs when the froxel path is off or unavailable and contact shadows are on (see [constraints-and-limits.md](constraints-and-limits.md)).

## See also

- [what-is-this.md](what-is-this.md)
- [constraints-and-limits.md](constraints-and-limits.md)
- [material-model.md](material-model.md)
- [../how-it-works/02-loading-and-injection.md](../how-it-works/02-loading-and-injection.md)
- [../how-it-works/03-gl-hooking.md](../how-it-works/03-gl-hooking.md)
- [../how-it-works/04-frame-anatomy-and-pass-order.md](../how-it-works/04-frame-anatomy-and-pass-order.md)

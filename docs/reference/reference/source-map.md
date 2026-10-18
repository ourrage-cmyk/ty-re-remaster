# Source map (every file in src/)

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributors).

## Purpose

A map of the `src/` folder for a contributor who needs to find code fast. For each file it gives the line count, the byte size, the role, who includes it, and the functions a reader is most likely to need, with line numbers. It also shows the include graph and the globals that cross file boundaries.

The page covers the 31 tracked files in `src/` and its `installer/` folder. Mechanism descriptions are on the how-it-works pages; this page only says where things are.

## How it works

There are four build units. Each has its own entry file.

| Unit | Entry file | Output | Built by |
|---|---|---|---|
| Renderer DLL | `src/tyrender.cpp` (one translation unit with all `.inl` files) | `tyrender.dll` | `src/build.ps1:13` |
| Launcher | `src/TYLauncher.c` (includes `launcher_ui.inc`) | `TYLauncher.exe` | `src/build.ps1:20` |
| Uninstaller | `src/installer/uninstall.c` | `TY HD Renderer Uninstall.exe` | `src/build.ps1:24` |
| Setup | `src/installer/setup.c` | `TY HD Renderer Setup.exe` | `src/build.ps1:30` |

The `.inl` files are not compiled alone. `src/build.ps1:13` compiles only `tyrender.cpp`. Each `.inl` is a text include, so all of them share one scope. That is why a global defined in `tyrender.cpp` can be used from any `.inl`, and why the order of includes matters.

Generated files (not in the repo):

- `build_id.h`, written by `src/build.ps1:12` from the git short hash.
- `payload.rc` and `setup_payload.rc`, written by `src/build.ps1:17` and `:27`, which embed the DLL, the ini and the launcher as resources.

### Include graph

```
tyrender.cpp  (one translation unit)
 ├─ tex.inl ............... line 434   ── tex_tags.inc (tex.inl:11)
 ├─ ao.inl ................ line 540
 ├─ fx.inl ................ line 541
 ├─ gi.inl ................ line 542   ── gi2.inl (gi.inl:10)
 ├─ atmos.inl ............. line 543
 ├─ emit.inl .............. line 544
 ├─ tess.inl .............. line 545
 ├─ pbr.inl ............... line 546
 ├─ light.inl ............. line 547
 ├─ debugcam.inl .......... line 548
 ├─ sesslog.inl ........... line 549   ── build_id.h (sesslog.inl:13)
 ├─ gpuprof.inl ........... line 550   ── stateguard.inl (gpuprof.inl:4)
 ├─ abshot.inl ............ line 551   ── build_id.h (abshot.inl:7)
 └─ overlay.inl ........... line 552   ── build_id.h (overlay.inl:8)

TYLauncher.c
 ├─ patch_exe.inc ......... line 18
 ├─ build_id.h ............ line 21 (generated)
 └─ launcher_ui.inc ....... line 79

installer/setup.c
 ├─ build_id.h ............ line 6 (generated)
 ├─ common.inc ............ line 7
 └─ ../patch_exe.inc ...... line 8

installer/uninstall.c
 └─ common.inc ............ line 4
```

Files in `src/` that nothing includes: `ui_part_a.inc`, `ui_part_c.inc` (no `#include` or reference to either anywhere in the repo), and the ps1 scripts (run directly).

## Code pointers

### File table

Lines and bytes are from `wc -l` and `wc -c` at 4db1d7d. "Included by" gives the include line.

| File | Lines | Bytes | Role | Included by |
|---|---|---|---|---|
| `src/tyrender.cpp` | 734 | 55462 | DLL core: IAT patch, GL hooks, shader rewrite dispatch, frame hooks, SwapBuffers, DllMain, crash handler | build entry |
| `src/ao.inl` | 724 | 49078 | GTAO pass and its shaders | `tyrender.cpp:540` |
| `src/gi2.inl` | 515 | 35138 | Hi-Z screen-space GI pipeline | `gi.inl:10` |
| `src/gi.inl` | 200 | 14527 | Legacy GI and the GI entry point | `tyrender.cpp:542` |
| `src/atmos.inl` | 518 | 41767 | Volumetric atmosphere and fog | `tyrender.cpp:543` |
| `src/emit.inl` | 266 | 20601 | Emitter light pass | `tyrender.cpp:544` |
| `src/fx.inl` | 262 | 18355 | Final image: FXAA, TAA, grade, sharpen | `tyrender.cpp:541` |
| `src/tess.inl` | 441 | 37289 | Tessellation and relief | `tyrender.cpp:545` |
| `src/pbr.inl` | 230 | 19247 | Material upgrade (bump, specular, fresnel) | `tyrender.cpp:546` |
| `src/light.inl` | 130 | 11298 | Shadow tint, hemisphere light, foliage | `tyrender.cpp:547` |
| `src/tex.inl` | 280 | 22113 | Texture names, tags, kernel32 hooks, decal rewrite | `tyrender.cpp:434` |
| `src/tex_tags.inc` | 4852 | 121364 | Generated texture tag table | `tex.inl:11` |
| `src/overlay.inl` | 257 | 16738 | In-game settings overlay (F10) | `tyrender.cpp:552` |
| `src/abshot.inl` | 132 | 9060 | A/B screenshot hotkey (F11) | `tyrender.cpp:551` |
| `src/debugcam.inl` | 76 | 7728 | Debug camera and motion test offset | `tyrender.cpp:548` |
| `src/sesslog.inl` | 376 | 20964 | Session report writer | `tyrender.cpp:549` |
| `src/gpuprof.inl` | 50 | 3619 | GPU timer marks | `tyrender.cpp:550` |
| `src/stateguard.inl` | 40 | 4470 | GL state leak detector (opt-in) | `gpuprof.inl:4` |
| `src/default_tyrender.ini` | 115 | 5893 | Shipped settings (Medium) | resource 2 of the launcher and resource 4 of Setup (`src/build.ps1:17`, `:27`) |
| `src/launcher_ui.inc` | 566 | 53370 | Launcher window, presets, sliders, cost model | `TYLauncher.c:79` |
| `src/ui_part_c.inc` | 253 | 27995 | Old copy of the launcher window code (not used) | none |
| `src/ui_part_a.inc` | 35 | 4303 | Old copy of the settings model (not used) | none |
| `src/TYLauncher.c` | 79 | 5464 | Launcher entry, payload, starting the game | build entry |
| `src/patch_exe.inc` | 49 | 4407 | TY.exe to TY_tyrender.exe patch (shared) | `TYLauncher.c:18`, `setup.c:8` |
| `src/installer/common.inc` | 193 | 11896 | Shared installer and uninstaller code | `setup.c:7`, `uninstall.c:4` |
| `src/installer/setup.c` | 119 | 6588 | Setup entry | build entry |
| `src/installer/uninstall.c` | 91 | 6080 | Uninstaller entry | build entry |
| `src/build.ps1` | 37 | 3540 | Builds the DLL, launcher, uninstaller and Setup | none (run by hand) |
| `src/package.ps1` | 12 | 1032 | Builds and zips a release | none |
| `src/patch_imports.ps1` | 83 | 5135 | Standalone form of the TY.exe patch | none |
| `src/gen_tex_tags.ps1` | 22 | 1844 | Generates `tex_tags.inc` | none |

Totals: the files in this table add up to about 11,700 lines and about 646 KB. The generated table `tex_tags.inc` is most of the bytes.

### Key functions by file

Only the functions a reader is likely to need are listed. Lines are the definition line.

**`src/tyrender.cpp`**

- `LoadConfig` `:56`; `LoadShadowConfig` `:150`
- `logf` `:44` (writes to `tyrender.log`)
- `PatchIAT` `:108` (the import-table hook, no inline trampoline)
- `Fnv1a` `:132` (name hash for shader dumps and overrides)
- `PatchShadowShader` `:174`
- `HOOK` macro `:250`; hooks for `glEnable`, `glBindTexture`, `glDrawElements`, and others follow from `:258` (see the hooks reference)
- `FogOverride` `:404`
- `g_procHooks` table `:438`; `h_wglGetProcAddress` `:457`
- `TestCompile` `:471`
- `Snapshot` `:506`
- `ApplyShadowStrength` `:554`; `AfterLink` `:561`
- `h_SwapBuffers` `:591` (the per-frame entry point)
- `CheckGameSettingsClash` `:637`
- `Install` `:662`; `tyrender_init` `:674` (dummy export)
- `CrashVEH` `:685`
- `DllMain` `:701`

**`src/ao.inl`**: `LoadGX` `:42`; `LoadLodConfig` `:478`; `LoadAOConfig` `:487`; `PollAOFlags` `:518`; `InitAO` `:533`; `AODraw` `:565`; `RunAO` `:566`.

**`src/gi.inl`**: `LoadGIConfig` `:89`; `PollGIFlags` `:98`; `RunGI` `:104`.

**`src/gi2.inl`**: `GISweepApply` `:22`; `GIMaskBegin` `:38`; `GIMaskEnd` `:52`; `GIMarkBegin` `:70`; `GIHizInit` `:349`.

**`src/atmos.inl`**: `InitVolume` `:254`; `LoadAtmosConfig` `:290`; `PollAtmosFlags` `:309`; `FogDiagDraw` `:319`; `LateFogBegin` `:329`; `LateFogEnd` `:345`; `SeeThruDepthBegin` `:349`; `SeeThruDepthEnd` `:357`; `RunAtmos` `:358`.

**`src/emit.inl`**: `LoadEmitConfig` `:32`; `EmitRewriteFS` `:47`; `EmitInit` `:100`; `EmitQBegin` `:137`; `EmitQEnd` `:152`; `EmitRankPoll` `:154`; `EmitBegin` `:173`; `EmitEnd` `:211`; `RunEmit` `:226`.

**`src/fx.inl`**: `LoadFXConfig` `:146`; `RunFX` `:177`. The file starts with a byte-order mark.

**`src/tess.inl`**: `LoadTessConfig` `:33`; `RewriteVSForTess` `:98`; `TessDrawPrep` `:431`.

**`src/pbr.inl`**: `LoadPbrConfig` `:16`; `PbrRewriteVS` `:48`; `PbrRewriteFS` `:57`; `TessApply` `:175`; `PbrApply` `:189`; `TessABPoll` `:206`.

**`src/light.inl`**: `LoadLightConfig` `:16`; `LightRewriteFS` `:34`; `LightApply` `:120`.

**`src/tex.inl`**: `LoadTexTags` `:65`; `TexInstallHooks` `:166`; `TexDrawPrep` `:224`; `SpillLogDraw` `:242`; `DecalRewriteFS` `:248`.

**`src/overlay.inl`**: `LoadOverlayConfig` `:41` (the `Key` default is `"0"` at `:43`); `OvDraw` `:161`.

**`src/abshot.inl`**: `LoadShotConfig` `:20`.

**`src/debugcam.inl`**: `PollDebugCam` `:8`; `DebugCamApply` `:16`; `MotionPoll` `:47`.

**`src/sesslog.inl`**: `SessInit` `:370`. Other session functions are declared in `tyrender.cpp:52-54`.

**`src/gpuprof.inl`**: `GProfInit` `:9`; `GProf` `:17`; `GProfNextFrame` `:23`.

**`src/stateguard.inl`**: `SGCapture` `:12`; `SGMark` `:31`.

**`src/TYLauncher.c`**: `PayloadRes` `:28`; `PayloadWrite` `:34`; `EnsurePayload` `:45`; `StartGame` `:51`.

**`src/launcher_ui.inc`**: `CostEstimate` `:44`; `SaveSettings` `:61`; `LoadSettings` `:70`; `SetGameDetail` `:82`; `GameClashText` `:121`; `LiveReload` `:166`; `RefreshCosts` `:221`; `SaveAll` `:246`; `EnsurePatched` `:255`; `ApplyLayout` `:277`; `GamepadTick` `:374`; `WndProc` `:397`; `wWinMain` `:542`.

**`src/patch_exe.inc`**: `Align` `:5`; `PatchExe` `:6`.

**`src/installer/common.inc`**: `Log` `:37`; `SetupLog` `:52`; `ParseArgs` `:59`; `GameRunning` `:95`; `RegAddEntry` `:107`; `IniMergeMissing` `:125`; `FindSteamGame` `:145`; `BrowseGame` `:173`; `BackupOriginal` `:186`.

**`src/installer/setup.c`**: `Payload` `:15`; `InstallPatched` `:29`; `InstallSettings` `:43`; `DoInstall` `:77`; `wWinMain` `:89`.

**`src/installer/uninstall.c`**: `RestoreOne` `:9`; `RestoreAll` `:23`; `RemoveRuntimeFiles` `:39`; `ScheduleSelfRemoval` `:47`; `wWinMain` `:63`.

## Settings

Not applicable. The keys each module reads are on the settings pages. The Load functions above are where each module reads its keys. The shipped values are in `src/default_tyrender.ini`.

## Cost

Not applicable.

## Limits

- Cross-file state lives in globals defined in `tyrender.cpp` and read by the `.inl` files. Examples: `g_dir` `:36`, `g_log` `:37`, `g_frame` `:39`, `g_curProg` `:67`, `g_jitterX`/`g_jitterY` `:73`, `g_screenW`/`g_screenH` `:79`, `g_aoDebug`/`g_giDebug` `:81`, `g_shadowTex` `:94`, `g_fogDistance`/`g_fogAmount` `:92`. A change to one of these affects every module that reads it.
- Because the `.inl` files share one scope, a name collision breaks the build and not just one module.
- `tyrender.cpp` is large (734 lines) and mixes hook code, config loading and frame control. The split is by include, not by file boundary.
- The launcher and installer share `patch_exe.inc`. A change there affects both (`TYLauncher.c:18`, `setup.c:8`).
- Build id is a generated header. A clean checkout without the build step cannot compile the launcher or the DLL (inferred from `src/TYLauncher.c:21`).

## Known issues

- **Unused copies.** `src/ui_part_a.inc` and `src/ui_part_c.inc` are not included anywhere. They hold older values, and a reader might edit them by mistake. See [launcher.md](launcher.md), Discrepancies.
- **Comment encoding.** The first line of `src/tyrender.cpp` is a comment with a broken character encoding (the bytes show as mojibake). It is cosmetic.
- **Comments point outside the repo.** Several comments cite files under a `RESEARCH\` folder that is not in the repo, for example `src/ao.inl:3`. Those citations are not checked here (NI-S1).
- **Outdated header.** The header comment of `src/fx.inl:5` gives values (`Sharpen=0.35`, `Contrast=1.04`, `Saturation=1.03`) that are not the in-code defaults (`src/fx.inl:153`, `:144`). The header is documentation for an older preset; it is not used by the code.
- **Missing README and PERFORMANCE in src.** `src/package.ps1:7` copies two files that are not in `src/`. See [tools-reference.md](tools-reference.md).

## How to verify

1. Line counts: `wc -l src/*.cpp src/*.inl src/*.inc src/*.c` should match the table.
2. Include graph: `grep -n '#include "' src/*.cpp src/*.inl src/*.inc src/*.c src/installer/*` should match the include graph above.
3. Unused files: `grep -rn "ui_part" .` (excluding `.git`) should return nothing except this page and the launcher page.
4. Globals: `grep -n "^static" src/tyrender.cpp | head` should show the globals listed under Limits.
5. Build order: `src/build.ps1` lines 13 to 33 show the four build units.

## NEEDS INVESTIGATION

- NI-S1 (N-17): The `RESEARCH\` files cited in code comments are not in the repo. Method: list every comment citation with `grep -rn 'RESEARCH' src`, then ask the owner which can be published.
- NI-S2 (Q022, N-8): Which thread calls each hook. The source map shows the single translation unit but not the threads. Method: log `GetCurrentThreadId` in the hooks.
- Answered: `stateguard.inl` is compiled into every build. The include at `src/gpuprof.inl:4` has no preprocessor guard. The opt-in is a runtime key, named in the file header as `[debug] StateGuard` (`src/stateguard.inl:1`). Still open: where that key is read (NI-S3). Method: `grep -n StateGuard src/*.inl src/*.cpp`.
- NI-S4: Whether `src/tyrender.cpp` has a companion header. None is in the repo (inferred from the include list).

## Discrepancies

- First pass `ARCHITECTURE.md:90` lists `ui_part_a.inc` and `ui_part_c.inc` as launcher UI parts with "not counted" for lines. `LAUNCHER.md:10` and `:15` call them included parts. Nothing includes them, so they are not part of the build. Only `launcher_ui.inc` is used (`src/TYLauncher.c:79`).

## See also

- [../how-it-works/01-architecture-overview.md](../how-it-works/01-architecture-overview.md): the big picture of these files
- [../how-it-works/04-frame-anatomy-and-pass-order.md](../how-it-works/04-frame-anatomy-and-pass-order.md): what each hook does in a frame
- [hooks-reference.md](hooks-reference.md): every hooked function
- [launcher.md](launcher.md): the launcher files in detail
- [installer-and-uninstaller.md](installer-and-uninstaller.md): the installer files in detail
- [tools-reference.md](tools-reference.md): the scripts in `src/` and `tools/`

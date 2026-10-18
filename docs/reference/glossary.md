# Glossary

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A, B, C.

Terms are listed A to Z. Internal identifiers are in code font and point to the file where they are defined or used. Links to pages in other chapters point to the planned paths in `docs/reference/`. Entries marked **doc** come from a repo document and were not found in code.

## Numbers and symbols

**`[ao]`, `[gi]`, `[atmos]`, `[fog]`, `[post]` and other `[section]` names.** Sections of `tyrender.ini`. See [reference/settings-ao-gi-lod.md](reference/settings-ao-gi-lod.md) and the other settings pages.

**`#4`, `#5`, ... (issue or PR numbers).** GitHub issue or pull request numbers quoted in the CHANGELOG and README. Example: #5 is water flicker on Ty, #9 is the portal light (`CHANGELOG.md:12`, `docs/MATERIAL_CLASSIFICATION_PLAN.md:1-3`). The tracker itself is not in the repo, so titles and states are **NI** (N-19). See [decisions/known-issues.md](decisions/known-issues.md).

**`T##` (task ids).** Board ids in `docs/todo.json`, such as T78 (High and Ultra flicker). Used in `docs/RESUME.md:26-41`. The file has more ids than the RESUME table lists (N-28).

## A

**A/B shot.** A pair of frames of the same view, with the effects on and off. The F11 key takes one. The frames are written to `Screenshots\` in the game folder (`src/abshot.inl:92-93`). File names are `ty_<build>_<preset>_<shaders-on|shaders-off>_<stamp>.png` (`src/abshot.inl:4`, `:93`). The "off" frame skips AO, GI, atmosphere, post and most of the light extras (`CHANGELOG.md:32`). See [guides/ab-captures.md](guides/ab-captures.md).

**Ambient occlusion (AO).** Darkening in creases and corners, computed in screen space from the depth buffer. Implemented in `src/ao.inl`. Keys in `[ao]`. See [shaders/ao-gtao.md](shaders/ao-gtao.md).

**Atmosphere.** The haze and light-shaft pass, in `src/atmos.inl`. Keys in `[atmos]`. See [shaders/atmosphere-fog.md](shaders/atmosphere-fog.md).

## B

**Bounce light.** Colour that one surface passes to another. Also called GI. Implemented in `src/gi2.inl` (Hi-Z) and `src/gi.inl` (legacy). See GI below.

**Build id.** The short commit identifier shown in the launcher title and in screenshot names. Set at build time into `build_id.h` (`src/TYLauncher.c:21`, `src/abshot.inl:93`). The launcher title format is at `src/TYLauncher.c:22-23`.

## C

**Character class.** The material class of a texture that belongs to Ty, an NPC or an animal. Character draws do not get relief, tessellation or GI colour bounce. Set from the `character` column of `data/texture_tags.csv`. See [concepts/material-model.md](concepts/material-model.md).

**Clash guard.** A check at start that logs a warning if the game's own `SSAO` or `MSAA` setting duplicates a mod effect. `src/tyrender.cpp:637-658`, `[clash] Check`.

**Contact shadows.** Short screen-space shadows near contact points, computed in the atmosphere pass. Keys `[atmos] ContactShadows`, `ContactLength`, `ContactThickness` (`src/atmos.inl:296`). Shipped value 0 (`src/default_tyrender.ini:35`).

**CRASH line.** The last line of a session report, written by `SessCrash` in `src/sesslog.inl`.

## D

**Decal.** A flat texture drawn on the ground, such as the footprints. Named in `[decal] Names` (`src/tex.inl:74`). Clamp is off by default (`src/default_tyrender.ini:69`).

**DDS.** A texture file format. The game loads DDS files from `PC_External` or from `Data_PC.rkv` (`src/tex.inl:2`).

**Debug view.** A mode that shows one effect alone, such as `[ao] DebugView`. Several are set by flag files (`src/tyrender.cpp:81`). See [guides/debugging.md](guides/debugging.md).

**DebugWater.** Bitmask in `[pbr]`, for bisecting the blue flicker. Bits: 1 specular, Fresnel and sheen; 2 bump and relief; 4 atmosphere water fog; 8 hemisphere and shadow tint (`default_tyrender.ini:60`).

**Depth copy.** The `glCopyTexImage2D` the game makes of its depth buffer. The mod's screen-space passes run at this point (`src/tyrender.cpp:280-293`).

**DLL.** The library `tyrender.dll`, built from `src/tyrender.cpp` and the `.inl` and `.inc` files it includes. Entry point is `DllMain`, which installs the hooks.

## E

**Emitter.** A draw whose texture glows (emissive flag). Emitter light is off by default (`[emit] Enable=0`, `src/default_tyrender.ini:107`). Code in `src/emit.inl`. See [shaders/emitter-light.md](shaders/emitter-light.md).

**env, env-detail, all, skin (`[tess] Scope`).** `env-detail` is the default. `env` and `all` are the same setting (`src/tess.inl:36`), which also tessellates environment geometry. `skin` is a legacy value that behaves like `env-detail` (`src/tess.inl:7`, `src/tess.inl:36`). The shipped ini sets `Scope=skin` (`src/default_tyrender.ini:43`). An earlier version of this glossary said `skin` tessellates character skin only. The code does not do that. See Discrepancies.

## F

**FB0.** The game's default framebuffer, the frame the player sees. Several passes read or write it (`src/fx.inl:2`, `src/gi.inl:156`, `src/atmos.inl:412`).

**Flag file.** An empty or small file in the game's `tyrender` folder that the renderer polls every few frames. Examples: `reload` (`src/tyrender.cpp:597`), `post_off` (`src/tyrender.cpp:596`), `cap.req` (`src/tyrender.cpp:626`). The full list is in [reference/hotkeys-and-files.md](reference/hotkeys-and-files.md).

**Fog.** Two sources. The engine's own fog is overridden by `FogOverride` (`src/tyrender.cpp:404-408`). The mod's volumetric fog is in the atmosphere pass.

**FXAA.** A fast edge anti-aliasing filter, in `src/fx.inl:8` (`kFXAA`). Option `[post] AA=1` (`README.md:305`).

**Froxel.** A cell of a camera-aligned 3D grid. The volumetric fog is computed on a grid of width x height x 64 cells (`src/atmos.inl:160-189`, `src/atmos.inl:279`). Needs GL 4.3 (`src/atmos.inl:258-260`).

## G

**G1 gate.** The release check: a 10-minute soak, a forced-fallback run and performance checks at four views (`docs/START_HERE.md:52`, `tools/g1_gate.ps1`).

**GGX.** A specular model used for the sun highlight on surfaces (`src/pbr.inl`). Described in [shaders/pbr-surface-detail.md](shaders/pbr-surface-detail.md).

**GI (global illumination).** Colour bounce between surfaces, in screen space. Hi-Z version in `src/gi2.inl`, legacy in `src/gi.inl`. Keys in `[gi]`. See [shaders/gi-hiz.md](shaders/gi-hiz.md).

**GI mask.** A per-draw alpha written by `GIMaskBegin` and `GIMaskEnd`, decided by the material class (`src/gi2.inl:38-55`, `src/gi2.inl:41`). Draws whose alpha is zero are not colour sources.

**GTAO.** Ground-truth ambient occlusion, a horizon-based AO method. The current version is in `src/ao.inl` (`kGTAO2`, `src/ao.inl:221`). The older method is selected with `[ao] Legacy=1` (`src/ao.inl:506`) (**inferred** that this is the "v1" method).

## H

**Halton jitter.** A sub-pixel offset sequence, base 2 and 3, applied to the camera for TAA (`src/fx.inl:159-166`).

**Harness.** The owner's scripts that boot the game, take screenshots and run checks. Covered in [guides/test-harness.md](guides/test-harness.md).

**Henyey-Greenstein (HG) phase.** A function that decides how strongly light scatters forward in the fog. Used in `src/atmos.inl` (header line 3).

**Hi-Z.** A depth mip pyramid used to speed up screen-space ray steps for GI (`src/gi2.inl:434-448`).

**Hook.** A replacement function. The mod replaces entries in the game's import table, so the game calls the mod's function first (`src/tyrender.cpp:107`, `:664-668`). See also IAT.

## I

**IAT (import address table).** The table of function addresses a Windows executable fills at load. Patching an entry redirects calls to that function. The mod patches the OpenGL and GDI entries (`src/tyrender.cpp:664-668`).

## L

**LAA (Large Address Aware).** A flag in the executable header that lets a 32-bit program use up to 4 GB on 64-bit Windows. Set on the patched copy `TY_tyrender.exe` (`src/patch_exe.inc:38`). The release script checks it (`tools/release.ps1:15`).

**Launcher.** `TYLauncher.exe`, the Win32 program with presets, sliders and the Play button. Source in `src/TYLauncher.c` and `src/launcher_ui.inc`. See [reference/launcher.md](reference/launcher.md).

**Lock file.** A file used by the test harness to stop two test runs at once, named `game.lock` in the lock folder (`tools/playtest/common.ps1`, `CHANGELOG.md:23`).

**LOD (distance level of detail).** Less work for far pixels. Configured by `[lod]`: `Start` 1500, `End` 6000, `MinScale` 0.35 (`src/default_tyrender.ini:20-28`). Off by default. Untested (`CHANGELOG.md:13`).

## M

**Material class.** One class per texture, derived from its flags: unknown, solid, transparent, emitter, transparent emitter, character, emissive character (`src/tex.inl:35`). See [concepts/material-model.md](concepts/material-model.md).

**MatPolicy, policy row.** The row of `kMatPolicy` for a texture's class (`src/tex.inl:38-56`). Consumers ask the row.

**Medium.** The default preset and the one recommended by the README (`README.md:159`). Shipped `tyrender.ini` is the Medium preset (`src/default_tyrender.ini:1`).

**Merkury.** The name of the engine according to a script comment (`tools/rkvtool.ps1:1`). **NI** (N-1) for the history.

## O

**Overlay (F10).** The in-game settings panel with eight items (`src/overlay.inl:22-31`). Key is set in `[overlay] Key`. Shipped `F10` (`src/default_tyrender.ini:115`). In code the default is 0 (off). See [how-it-works/09-overlay-input-and-hotkeys.md](how-it-works/09-overlay-input-and-hotkeys.md).

## P

**PBR.** The mod's surface model: bump from texture colour, relief, GGX specular and a sheen. Described in `src/pbr.inl:1-6` and [shaders/pbr-surface-detail.md](shaders/pbr-surface-detail.md). Not physically based in the full sense.

**PCF.** Percentage-closer filtering for shadow maps. The mod's shadow patch replaces the game's filter (`PatchShadowShader`, `src/tyrender.cpp:174-247`). `TyPCF` and `TyPCFL` are the filter helpers (`src/tyrender.cpp`).

**Phase (EDIT, FREEZE, TEST).** The owner's process states. Game launches are allowed only in TEST (`tools/playtest/common.ps1:91`).

**Playtest.** A recorded session of the owner's play or an automated run (`tools/playtest/autoplaytest.ps1`). See [guides/test-harness.md](guides/test-harness.md).

**Preset.** One of Low, Medium, High or Ultra (`src/launcher_ui.inc:6`). Values are in `kPresetVals` (`src/launcher_ui.inc:9`). A "Custom" state applies when no preset button is selected (`src/launcher_ui.inc:5`).

## R

**RKV.** The archive format for game data, used by `Data_PC.rkv` and `Override_PC.rkv`. Listed and extracted by `tools/rkvtool.ps1`. See [reference/tools-reference.md](reference/tools-reference.md).

**Relief.** Surface depth derived from the texture's colour. It drives tessellation displacement in the tess shader (`kTyHeightGLSL`, `src/tess.inl`, also used in `src/pbr.inl`) and per-pixel bump. See [shaders/tessellation-and-relief.md](shaders/tessellation-and-relief.md).

**Reload.** The file `tyrender\reload`. When it is found, the renderer re-reads most settings (`src/tyrender.cpp:597-603`). Some keys need a restart; see each settings page.

## S

**Session report.** The local text report written at each start to `tyrender\logs\TYHD_report_<time>_<build>.txt`. Never sent (`src/sesslog.inl:258-264`, `README.md:246`). See [guides/session-report.md](guides/session-report.md).

**Shadow patch.** The change to the game's cascade-shadow lookup (`PatchShadowShader`, `src/tyrender.cpp:174-247`). See [shaders/shadow-filter.md](shaders/shadow-filter.md).

**SkyFogDist.** The fog depth for sky and painted rock, as a fraction of `MaxDist` (`src/atmos.inl:251`). Default 0.45 in code, 0.45 shipped (`src/default_tyrender.ini:40`).

**Spill.** Light leaking from an emitter onto other surfaces, such as the portal light. Logged by `[spill] LogEmitters` (`src/tex.inl:69`, `:266`).

**Stock.** The original, unmodified game, used as a reference. Also the "stock" behaviour of a pass, meaning what the game does without the mod.

**StateGuard.** An opt-in check that logs graphics-state changes made by a pass (`[debug] StateGuard=1`, `src/stateguard.inl`).

**Slot 2.** A save slot the test harness can load with `TY_SLOT=2`, which starts the Billabong level (`docs/START_HERE.md:61`).

## T

**TAA.** Temporal anti-aliasing: each frame reuses the previous one, with a sub-pixel jitter. `kTAA` in `src/fx.inl:99`. Options `[post] AA=2` and `AA=3` (`README.md:305`). See [shaders/taa-fxaa.md](shaders/taa-fxaa.md).

**Tag table.** The list of textures and their flags, from the owner's CSV (`data/texture_tags.csv`, 1933 rows), embedded as `src/tex_tags.inc`.

**TCS, TES.** Tessellation control and evaluation shader stages. Generated in `src/tess.inl` (`#version 400`, `src/tess.inl:179`, `:207`).

**Tonemap.** The curve in the final grade pass, with `TonemapKnee` and `TonemapRoll` (`src/fx.inl`). See [shaders/grade-and-sharpen.md](shaders/grade-and-sharpen.md).

**tyCam, tySunDir, tyPbr.** Per-program uniforms set by the PBR pass (`src/pbr.inl:6`, `:64`).

**tyLt.** The light block uniform `tyLt[6]`, declared and filled by the light pass (`src/light.inl:7`, `src/tyrender.cpp:101`).

**tyTex.** The per-draw uniform that switches relief, bump and specular off for a draw (`src/tex.inl:230-231`, `src/pbr.inl:76`).

**tyrender.dll.** See DLL.

**tyrender.ini.** The settings file in the game folder. Created from the embedded default when missing (`src/TYLauncher.c:49`). Existing files are not overwritten.

**TY_tyrender.exe.** The patched copy of `TY.exe` that loads `tyrender.dll`, made by the installer or the Play step. The original `TY.exe` is not changed (`src/patch_exe.inc:7`, `src/installer/setup.c:31-39`).

**TyOutputLog.txt.** The game's own log file. Compile errors go there (`docs/START_HERE.md:84`).

## V

**Vanilla.** Another word for stock (the unmodified game).

**VPMatrix.** The engine's camera matrix uniform, tracked by the mod (`src/tyrender.cpp:340`).

## W

**wglGetProcAddress.** The Windows call that returns the address of an OpenGL function that is not in the basic library. The mod hooks it so that the function pointers it hands out are also wrapped (`src/tyrender.cpp:432`, and `src/ao.inl:37-41` for the mod's own lookups).

**Water flags.** Water is a texture name containing "water" or a name in `[fog] WaterNames` (`src/tex.inl:190-191`). `WaterStable` (`[pbr]`) drops bump, relief, specular and sky Fresnel from water draws (`src/default_tyrender.ini:58-59`).

**WaterBoost, WaterNames, WaterMaskSoft.** Keys in `[fog]` (and `[pbr]`/`[atmos]` where noted) that control extra fog on water draws and the edge of the GI water mask. See [shaders/water-fog-and-decals.md](shaders/water-fog-and-decals.md).

## Y

**YCoCg.** A colour space used to clip TAA history (`src/fx.inl`). See [shaders/taa-fxaa.md](shaders/taa-fxaa.md).

## Terms not in the code

**Trampoline.** A code-patching technique that the mod does not use. Searching the source finds no trampoline; the mod replaces import table entries (IAT), not code bytes (see IAT).

## Discrepancies

- **`skin` (tessellation Scope).** This glossary previously said `skin` "tessellates character skin only, per the shipped ini". The code says `skin` is a legacy value treated as `env-detail`, with no geometry tessellation (`src/tess.inl:7`, `src/tess.inl:36`). Characters are never tessellated (`src/tess.inl:107`). The shipped ini sets `Scope=skin` (`src/default_tyrender.ini:43`). The entry above follows the code.
- **Screenshot label.** The previous glossary wrote `<shaders-on|off>`. The code writes `shaders-off` (`src/abshot.inl:4`, `:121`, `:128`).
- **GTAO v1.** The previous glossary said v1 and v2 are both in `src/ao.inl`. The file has v2 (`src/ao.inl:183`) and a legacy path (`Legacy=1`). Calling the legacy path "v1" is an inference.
- **Overlay default.** The code default for `[overlay] Key` is `0` (off) (`src/overlay.inl:43`). The shipped ini sets `F10` (`src/default_tyrender.ini:115`). `CHANGELOG.md:22` notes that existing inis keep `Key=0`, so the key does nothing until the ini is changed.
- **Preset numbers.** The launcher's comment at `src/launcher_ui.inc:10` gives other frame times than the README (see [concepts/what-is-this.md](concepts/what-is-this.md)).

## See also

- [README.md](README.md)
- [concepts/material-model.md](concepts/material-model.md)
- [concepts/game-and-engine-facts.md](concepts/game-and-engine-facts.md)
- [concepts/constraints-and-limits.md](concepts/constraints-and-limits.md)
- [decisions/known-issues.md](decisions/known-issues.md)

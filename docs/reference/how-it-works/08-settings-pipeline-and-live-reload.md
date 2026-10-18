# Settings pipeline and live reload

> Status: documented at commit 4db1d7d (`main`, Merge PR #122); last verified 2026-10-18. Audience: B (tuning and bug reports), C (review).

## Purpose

This page explains where settings come from, which file they are read from, when they are read, and what happens after a change. It covers Q115 (how settings are read and where defaults live), Q116 (which keys are live and which need a restart) and Q117 (how the reload file works). Q118 (where the presets are defined) is answered briefly here and in full on [presets](../reference/presets.md). The key-by-key tables are on the four settings pages: [AO, GI and LOD](../reference/settings-ao-gi-lod.md), [atmosphere, fog, emitter](../reference/settings-atmos-fog-emit.md), [post, shadows, light, PBR, tessellation](../reference/settings-post-shadows-light-pbr-tess.md) and [system, debug, reports](../reference/settings-system-debug.md).

## How it works

Diagram (ASCII, paths relative to the game folder):

```
  code defaults ----------+                 +-- one loader per section
  (GetPrivateProfile*     |                 |   (LoadAOConfig, LoadFXConfig, ...)
   second argument)       v                 |
  <game>\tyrender.ini <--- launcher ---+    v
     ^   ^                  (writes it)  DLL reads it at startup (DllMain)
     |   |                               and on every reload request
  shipped copy:            tools/presets.ps1 (test harness copy)
  src/default_tyrender.ini
  (written only if no ini exists)

  launcher writes <game>\tyrender\reload  -->  DLL deletes it, re-reads, logs
  flag files <game>\tyrender\*_off, *_debug, ao_reload, post_off  -->  polled
```

1. **Three sources of defaults.** The code default is the second argument of every read (table column "Code default" on the settings pages). The shipped file `src/default_tyrender.ini` is packaged as resource 2 of the launcher (`src/TYLauncher.c:49`, `EnsurePayload` at `src/TYLauncher.c:45`). It is written to `tyrender.ini` only if that file does not exist yet; the write flag stops it from overwriting the user's file. The launcher has its own defaults for its controls (`src/launcher_ui.inc:17-32` and `:71-78`).
2. **The file.** The DLL and the launcher both use `<game folder>\tyrender.ini`. The DLL builds the path from the folder of the game executable (`src/tyrender.cpp:705-706`, then e.g. `src/ao.inl:488`). The launcher builds the same path at `src/launcher_ui.inc:544`. The flag files are one folder down in `<game folder>\tyrender\`.
3. **Startup order.** `DllMain` (`src/tyrender.cpp:701`) runs in this order: `LoadConfig` (line 714: `[debug]` capture and shader switches), `CheckGameSettingsClash` (715), `LoadAOConfig` (716, also reads `[lod]`), `LoadFXConfig` (717), `LoadShadowConfig` (718), `LoadAtmosConfig` (719), `LoadGIConfig` (720), the fog reads (722-723), `LoadTessConfig` (724), then `LoadPbrConfig`, `LoadLightConfig`, `LoadShotConfig`, `LoadOverlayConfig`, `LoadTexTags` and `LoadEmitConfig` (725). It logs `tyrender stage1 loaded` (726), installs the hooks (727) and starts the session report (`SessInit`, 731).
4. **How a loader reads.** Each key is one `GetPrivateProfileIntA` or `GetPrivateProfileStringA` call with the code default. Strings are converted with `atof`; `[atmos] SunColor` is parsed with `sscanf` (`src/atmos.inl:306-307`). Some keys are clamped (for example `[shadows] MapScale` to 1 to 4, `src/tyrender.cpp:153`). Otherwise there is no validation: a bad value is read as its numeric prefix (inferred from the use of `atof` and `atoi`).
5. **Live reload from the launcher.** Any change in the launcher writes the ini (`SaveSettings`, `src/launcher_ui.inc:61-68`; all sliders through `SliderToIni`, `:182-184`). If the game window is open, `LiveReload` creates the empty file `<game folder>\tyrender\reload` (`src/launcher_ui.inc:166-169`). `SaveAll` (`:246`), the slider drag (`:487-489`) and the preset click (`:493`) all do this.
6. **Live reload in the DLL (Q117).** `SwapBuffers` runs the check every 8 frames (`src/tyrender.cpp:594`: `(g_frame & 7) == 0`). It polls the flag files (`:595`), tests for `tyrender\reload` (`:597`), and when the file exists it deletes it and calls `LoadAOConfig`, `LoadFXConfig`, `LoadAtmosConfig`, `LoadGIConfig`, `LoadEmitConfig`, `LoadShadowConfig`, `LoadPbrConfig`, `LoadTessConfig`, `LoadLightConfig`, `LoadShotConfig` and `LoadOverlayConfig` (`:598`). It then re-applies shadow strength to every program (`:599`), re-reads the three fog keys (`:601-602`), clears the program stamps so the uniforms are sent again (`:598`, `g_pbrStamp` and `g_tessStamp`) and logs `[cfg] reloaded all` (`:603`). A reload therefore takes effect within about 8 frames.
7. **What a reload does not do.** `LoadConfig` is not called again, so `[debug] CaptureFrame`, `CaptureFrame2`, `DumpShaders` and `OverrideShaders` need a restart. `CheckGameSettingsClash` runs only at start-up. `LoadTexTags` runs only at start-up (`[tex]`, `[spill]`, `[decal]`, and `[fog] WaterNames`). `SessInit` runs only at start-up (`[report]`). Shader source that is already compiled keeps its text, so rewrite-time keys (see the table below) need a restart to show in the shaders.
8. **Flag files.** The per-effect flag files are polled at the same cadence: `PollAOFlags`, `PollAtmosFlags` and `PollGIFlags` (`src/tyrender.cpp:595`), and `post_off` (`:596`). `ao_reload` deletes itself and reloads the AO config (`src/ao.inl:523`). For the debug views a flag file overrides the ini key when the file exists (`src/ao.inl:521`, `src/gi.inl:101`). The file names are on [hotkeys and files](../reference/hotkeys-and-files.md).
9. **Overlay.** A change in the F10 panel writes one key with `WritePrivateProfileStringA` (`src/overlay.inl:61`), logs it to the session report (`:62`) and reloads that section (`OvReload`, `overlay.inl:52-57`, called from `overlay.inl:72`).
10. **Session report.** After every write from the launcher, the overlay or a reload, the report compares the ini with its last snapshot and logs up to 20 changed keys (`src/sesslog.inl:131-145`). This is the record testers send.
11. **The game's own settings.** The launcher's "Game detail settings" check box edits seven detail keys in the game's own `settings.ini`, in the user's Documents folder (`SetGameDetail`, `src/launcher_ui.inc:82-95`). It keeps one backup named `*.tyhd_backup` (lines 86-87). The DLL only reads two `[Display]` keys from that file, to warn (`src/tyrender.cpp:637-661`). Neither the DLL nor the launcher changes `tyrender.ini` in response to those keys.
12. **Test tools.** `tools/presets.ps1` writes a complete ini of its own (`NewIni`, lines 3-12). The harness uses it to set up test configurations. It is not used by the game or the launcher.

## Live or restart (Q116)

Rules: "live" = read by the reload path (item 6) and used per frame or per draw. "restart" = read only at start-up, or used when a shader is compiled or linked, or when a texture is allocated. Keys marked "(inferred)" were not traced to every use. The full per-key status is in the Live or restart column of each settings page.

| Group | Keys | Status | Basis |
|---|---|---|---|
| Per-frame gates and uniforms | `[ao]` (most), `[gi]` (most), `[atmos]` (haze, density, steps), `[fog] Amount`, `Distance`, `LateBoost`, `[post]` (all), `[shadows] Radius`, `Strength`, `Temporal`, `Grid`, plane and slope bias, `[pbr]` material numbers, `[lod]` switches | live | reload path (item 6); per-frame use: `src/ao.inl:567`, `src/gi.inl:105`, `src/atmos.inl:362`, `src/fx.inl:178`, `src/light.inl:108-117`, `src/tyrender.cpp:599` |
| Start-up only | `[debug] CaptureFrame`, `CaptureFrame2`, `DumpShaders`, `OverrideShaders`; `[clash] Check`; all of `[report]`; `[tex]`; `[spill]`; `[decal]`; `[fog] WaterNames` | restart | `src/tyrender.cpp:714`, `:715`, `:725`, `:731` |
| Shader rewrite or allocation | `[shadows] Patch` (compile), `[shadows] MapScale` (allocation, `src/tyrender.cpp:274`), `[pbr] Enable`, `LitProps`, `Debug` (compile, `src/pbr.inl:39`, `:46`, `:165`), `[tess] Enable`, `Scope`, `Debug`, `Cull`, `WorldMaxLevel`, `MaxLevel` (inferred, `src/tess.inl:99`, `:107`), `[lod] TessEnable` (inferred), `[portability] TessVersionGate` (inferred), `[debug] ForceFallback` (inferred) | restart | the rewrite runs when the program is compiled; the reload does not recompile existing programs (inferred) |
| Mixed | `[light] Enable` (per frame at `src/light.inl:109`, rewrite at `src/light.inl:35`), `[emit] Enable` (per frame at `src/emit.inl:139`, `:175`, `:227`; rewrite at `src/emit.inl:45`) | per-frame part live, rewrite part restart | as shown |
| Conflict | `[shadows] Taps` | docs say restart (README; `docs/PERFORMANCE.md` line 85); the code writes it into the per-frame light block (`src/light.inl:108-110`) | not resolved; see the post and shadows page |
| Conflict | `[debug] ShadowView` | comment says restart (`src/light.inl:24`); code re-reads and uses it per frame (`src/tyrender.cpp:598`, `src/light.inl:117`) | behaves as live in code (inferred) |

Overlay items (`src/overlay.inl:22-30`) are live by this definition. The launcher's own preset values are written to the ini, then the DLL picks them up through the reload.

## Code pointers

| What | File:line | Note |
|---|---|---|
| Startup load order | `src/tyrender.cpp:701-731` | `DllMain` |
| Reload poll and reload | `src/tyrender.cpp:594-603` | every 8 frames |
| Flag polls | `src/tyrender.cpp:595-596`; `src/ao.inl:521-523`; `src/gi.inl:100-101`; `src/atmos.inl:309-312` | flag files |
| Launcher writes | `src/launcher_ui.inc:61-68`, `:166-169`, `:246`, `:493` | save, live reload, preset click |
| Launcher defaults | `src/launcher_ui.inc:17-32`, `:71-78` | sliders and read-back defaults |
| Shipped copy | `src/default_tyrender.ini`; `src/TYLauncher.c:45-50` | written only when missing |
| Session ini check | `src/sesslog.inl:111-145` | logs changes |

## Settings

Every key is on the four settings pages. Keys are counted by the grep in the settings pages' legend: 227 section and key pairs. The `GpuProf` key named in comments is not one of them, because no code reads it (see Discrepancies).

## Cost

Reading the ini costs nothing measurable per frame (the reads happen at start-up and on reload). The reload itself is a few `GetPrivateProfile*` calls plus one ini scan for the session report (inferred). Not measured.

## Limits

- Restart-only keys are restart-only for a reason: shader text and textures are built once. A running game keeps the old text until the next start.
- There is one ini per game folder. A second copy of the game with its own folder has its own ini.
- Every launcher save (any toggle, slider or preset click, or Play) rewrites the 15 slider keys from the slider positions (`src/launcher_ui.inc:246`, `:182-184`). A hand edit to one of them survives only at slider precision (1 percent of the slider range) and only inside the slider's range; out-of-range values are clamped to the range limit on the next save.
- The reload waits up to 8 frames, and the launcher's own UI is not in the game process.

## Known issues

The README known-issues list (README.md:209-227) has no item on the reload path. The issue tracker was not checked for this page.

## How to verify

1. Start the game with the DLL. Open `tyrender.log` and find `tyrender stage1 loaded` (`src/tyrender.cpp:726`). The line lists `capture`, `dump` and `override` values.
2. Change a live key in the ini (for example `[post] Sharpen`), create an empty file `tyrender\reload` next to the exe, and wait a second. The log should show `[cfg] reloaded all` (`src/tyrender.cpp:603`) and the session report should show `setting changed (reload): post.Sharpen = ...`.
3. For a start-up key (for example `[report] KeepFiles`), the reload does not log it. Restart the game and check the report's `setting` lines instead.
4. For a section-specific config line, look for `[ao] config:` (`src/ao.inl:516`) or `[light] config` (`src/light.inl:27`) in the log after a reload.

## NEEDS INVESTIGATION

- **Rewrite-time keys after a reload.** Does a running game pick up `[pbr] Debug`, `[tess] Enable` or `[shadows] Patch` changes without a restart? Method: change each through the reload file and check whether the shaders in `tyrender\dump` change for new programs only. Owner playtest.
- **MapScale after a reload.** Method: change `[shadows] MapScale` through the reload and check whether `[shadows] shadow map enlarged to` appears again (`src/tyrender.cpp:274`). If not, the change needs a restart.
- **Taps.** See the Conflict row above (plan item N-22 is the related shadow-cascade question).
- **Whether stale shader programs keep old settings.** Method: change a per-program setting (for example `[pbr] DebugWater`) and compare two frames.

## Discrepancies

1. **Location of tyrender.ini.** First-pass SETTINGS_REFERENCE.md says the renderer reads `tyrender.ini` "from the game's `tyrender` folder". The code reads `<game folder>\tyrender.ini` (`src/tyrender.cpp:705-706`; `src/launcher_ui.inc:544`). The `tyrender` subfolder holds the flag files only. README ("edit `tyrender.ini` in the game folder") agrees with the code.
2. **Reload-file deletion.** First-pass SETTINGS_REFERENCE.md marks the deletion "(inferred), not verified". It is verified: `src/tyrender.cpp:598` deletes the file.
3. **Shot preset default.** `src/abshot.inl:89` defaults to `custom`; the launcher writes `Custom`.
4. **[debug] ShadowView comment** versus reload path (see the live table).
5. **Header comment in the shipped file.** `src/default_tyrender.ini` line 1 says "TYLauncher writes these". The launcher writes only the keys in `SaveSettings` and the 15 sliders. The rest stay as shipped or as code defaults.
6. **Launcher-only defaults.** The launcher reads `[gi] Enable` as 0 and `Rays` as 1 when the key is missing (`src/launcher_ui.inc:77`), while the code defaults are 1 and 4 (`src/gi.inl:91`).
7. **Stale launcher files.** `src/ui_part_a.inc` and `src/ui_part_c.inc` are not included by anything. First-pass LAUNCHER.md treats them as live (see the presets page).
8. **Unread key.** `[debug] GpuProf` (see the system page).

## See also

- [AO, GI and LOD settings](../reference/settings-ao-gi-lod.md), [atmosphere, fog and emitter settings](../reference/settings-atmos-fog-emit.md), [post, shadows, light, PBR and tessellation settings](../reference/settings-post-shadows-light-pbr-tess.md), [system, debug and reports settings](../reference/settings-system-debug.md)
- [presets](../reference/presets.md), [hotkeys and files](../reference/hotkeys-and-files.md)
- [launcher](../reference/launcher.md), [overlay and hotkeys](09-overlay-input-and-hotkeys.md)
- [architecture overview](01-architecture-overview.md), [log and error messages](../reference/log-and-error-messages.md)

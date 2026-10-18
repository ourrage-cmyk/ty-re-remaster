# Launcher (TYLauncher.exe)

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: B (testers and modders), A for the code map.

## Purpose

`TYLauncher.exe` is the window most people use. It writes the renderer settings file (`tyrender.ini`), starts the game with the renderer, and offers four presets, a set of live sliders, a performance estimate, a Steam launch-option helper, and a button that opens the logs folder.

This page explains what the launcher does, in what order, and which files it writes. It does not repeat key tables. Key meanings and defaults live in the [settings pages](settings-ao-gi-lod.md). Preset values live in [presets.md](presets.md). Hotkeys and flag files live in [hotkeys-and-files.md](hotkeys-and-files.md).

Scope: the launcher source is `src/TYLauncher.c` (79 lines) plus `src/launcher_ui.inc` (566 lines). `src/ui_part_a.inc` and `src/ui_part_c.inc` are old copies that nothing includes (see Discrepancies).

## How it works

Start-up:

1. `wWinMain` sets the launcher folder as the game folder and builds the path of `tyrender.ini` next to the exe (`src/launcher_ui.inc:542-544`).
2. `EnsurePayload` runs once (`src/launcher_ui.inc:545`, body at `src/TYLauncher.c:45-50`). It writes the embedded `tyrender.dll` next to the exe when the bytes differ. It creates `tyrender.ini` only when that file is missing, so user settings are never overwritten. If the DLL cannot be written (usually because the game is running), a warning box says so and the old DLL stays in use (`src/TYLauncher.c:47-48`).
3. The command line is read (`src/launcher_ui.inc:546-554`). The flags are in the table below.
4. With no flag that skips the window, the window opens (`src/launcher_ui.inc:557-564`).

Command-line flags (`src/launcher_ui.inc:548-553`):

| Flag | Effect | Code |
|---|---|---|
| `--play` | Skip the window. Patch if needed, then start the game with the DLL. | `:550`, `:555-556` |
| `--wait` | Wait for the game to exit and return its exit code. | `:548`, `src/TYLauncher.c:73,75` |
| `--noinject` | Skip the window and start the untouched `TY.exe` with no DLL. | `:549`, `:556` (`inject=0`), `src/TYLauncher.c:55-56` |
| anything else | Passed to the game, except an argument that ends in `TY.exe`. Steam passes that path in `%command%`, so it is dropped. | `:552-553` |

The window is created at `src/launcher_ui.inc:562`. Its title is `TY HD Renderer v0.3.5-rc build <id>` (`src/TYLauncher.c:22-23`). The build id comes from `build_id.h`, which `src/build.ps1:8-12` writes from the git short hash (with `-dirty` when the tree has local edits).

When the window opens (`WM_CREATE`, `src/launcher_ui.inc:399-485`):

1. The game's `settings.ini` is read to get the resolution (`GameResolution`, `:201-208`). The cost estimate scales with the pixel count.
2. Settings are loaded into the UI (`LoadSettings`, `:70-78`; `ToUI`, `:189-195`; `SlidersFromIni`, `:185-188`).
3. `RefreshCosts` runs (`:482`). It also writes `[shots] Preset` into `tyrender.ini` when that file exists (`:242`). Simply opening the window therefore changes the file.
4. Three timers start (`:483`): a 2 s status refresh, a 90 ms hover refresh (help panel), and a 30 ms gamepad poll.

Every change goes through `SaveAll` (`src/launcher_ui.inc:246`):

1. `FromUI` reads the checkboxes and combo boxes (`:196-200`).
2. `RefreshCosts` recalculates the tags and the total (`:221-245`).
3. `SaveSettings` writes the checkbox-driven keys (`:61-69`).
4. `SliderToIni` writes all 15 slider keys (`:183`, loop at `:246`).
5. If the game window is found, `LiveReload` creates `tyrender\reload` (`:166-169`). The DLL checks that flag every 8 frames (`src/tyrender.cpp:597-598`) and reloads its config loaders (`:599-603`). This is how sliders act on a running game.

Buttons (`WM_COMMAND`, `src/launcher_ui.inc:490-512`):

| Control | What it does | Code |
|---|---|---|
| Preset (Low, Medium, High, Ultra) | Copies the preset's checkbox and slider values, writes extra `[atmos]` and `[post]` values, then saves. | `:492-494`; values `:7-15` |
| Checkbox or combo change | `SaveAll` and a short description. | `:495` |
| Game detail checkbox | Edits the game's own settings file. See below. | `:496`, `:82-99` |
| Play with HD renderer | `SaveAll`, then `EnsurePatched`, then `StartGame(1)`. | `:497` |
| Play original (no mod) | `SaveAll`, then `StartGame(0)`. | `:498` |
| Customise settings / Simpler view | Toggles the expanded view. | `:501`, `:277-293` |
| Steam setup... | Copies `"<game>\TYLauncher.exe" --play %command%` to the clipboard and shows the steps. | `:503-511` |
| Advanced (ini)... | `SaveAll`, then opens `tyrender.ini` in Notepad. | `:477`, `:499` |
| Reset sliders | Writes the launcher's slider defaults and saves. | `:478`, `:502` |
| Report a problem / open logs folder | Creates `<game>\tyrender\logs` and opens it. | `:479`, `:500` |

Starting the game (`StartGame`, `src/TYLauncher.c:51-76`):

1. The exe is `TY_tyrender.exe` when injection is wanted and that file exists (`:55-56`). Otherwise it is `TY.exe`.
2. Only when `TY_tyrender.exe` is absent and injection is wanted, the launcher starts `TY.exe` suspended, writes the DLL path into the game, runs `LoadLibraryW` in a remote thread, waits up to 30 s, and then resumes the game (`:59-71`). This is the fallback path. Its failure messages suggest the Patch button (`:65-67`).
3. For `--wait`, the launcher waits for the game and returns its exit code (`:73-75`).

Patching (`EnsurePatched`, `src/launcher_ui.inc:255-260`): if `TY_tyrender.exe` is missing, it calls `PatchExe` (`src/patch_exe.inc:6`). That function writes a copy of `TY.exe` with an extra import. `TY.exe` is only opened for reading. If patching fails, the launcher shows a warning and Play still runs through the fallback path.

The game's own detail settings (`SetGameDetail`, `src/launcher_ui.inc:82-99`): the checkbox rewrites seven keys (`DetailLevel`, `Props`, `PropDetailLevel`, `Grass`, `Anisotropic`, `Detail`, `ReflectionDetail`) in the game's `settings.ini` in the Documents folder (`:88`). The value is `high`, or `low`/`mid` for the other keys (`:94`). Before the first change, the original is copied to `settings.ini.tyhd_backup` (`:86-87`).

Clash guard (`src/launcher_ui.inc:102-130`, issue #13): the launcher reads the game's own `settings.ini` in the Documents folder, `[Display]` section. It checks `SSAO` (true) and `MSAA` (2x or more). It warns in the status line when SSAO is on, or when MSAA is on together with the renderer's edge smoothing (`[post] AA` not 0). It never rewrites the game's file. The check runs on every status tick (`:173`), so the warning follows changes made in the game within about 2 s. `[clash] Check=0` turns it off. The DLL applies the same rules and logs them to `tyrender.log` (`src/tyrender.cpp:637-659`).

Gamepad (`src/launcher_ui.inc:294-396`): XInput is loaded at run time (`:375-376`). The D-pad and left stick move focus (`:381-384`, `:348-360`). A presses the focused control, B goes back, LB and RB switch tabs (`:392-395`). Pad input is ignored when another window is in front (`:378`). The feature is labelled **untested** (`CHANGELOG.md:27`).

Game running check: the launcher looks for a window titled `TY the Tasmanian Tiger` (`src/launcher_ui.inc:165`). This drives the "Game running: sliders apply live" status text (`:170-177`). The installer uses a different check (process names); see [installer-and-uninstaller.md](installer-and-uninstaller.md).

## Code pointers

| What | File:line | Note |
|---|---|---|
| Entry point, payload, start | `src/TYLauncher.c:45-76` | Payload write, `StartGame` |
| Include of the window code | `src/TYLauncher.c:79` | Only `launcher_ui.inc` is included |
| Preset values | `src/launcher_ui.inc:7-15` | Haze, grade, then field table |
| Slider table (15 entries) | `src/launcher_ui.inc:17-33` | Section, key, range, label |
| Cost model | `src/launcher_ui.inc:37-56` | `CostEstimate` |
| Tag thresholds | `src/launcher_ui.inc:148-149` | `Lvl`, `TotLvl` |
| Save and load | `src/launcher_ui.inc:57-78` | `SaveSettings`, `LoadSettings` |
| Game detail file | `src/launcher_ui.inc:80-100` | `SetGameDetail` |
| Clash warning | `src/launcher_ui.inc:102-130` | Text only; DLL logs the same rules (`src/tyrender.cpp:637-659`) |
| Live reload | `src/launcher_ui.inc:166-169` | Writes `tyrender\reload` |
| Status text | `src/launcher_ui.inc:170-177` | Timer 1, every 2 s (`:483`) |
| Tabs and slider placement | `src/launcher_ui.inc:426`, `:460` | Five tabs |
| Description panel | `src/launcher_ui.inc:143-163`, `:261-268` | Help text and impact tag |
| Gamepad | `src/launcher_ui.inc:294-396` | XInput at run time |
| Window procedure | `src/launcher_ui.inc:397-540` | Create, timers, commands |
| Command line and main | `src/launcher_ui.inc:542-566` | Flags at `:546-556` |
| Patch of TY.exe | `src/patch_exe.inc:6-48` | Shared with the installer |
| Embedded payload build | `src/build.ps1:16-18` | Resource 1 = DLL, 2 = default ini |

## Settings

The launcher writes these keys. Meanings and defaults are on the settings pages; this list only says who writes them.

- Checkbox keys, written on every save (`src/launcher_ui.inc:61-69`): `[ao] Enable, Legacy (set to 0), Slices, Steps`; `[atmos] Enable, Froxel (set to 1), Steps`; `[tess] Enable, Scope (all or skin), MaxLevel, Debug (set to 0)`; `[post] Enable, AA`; `[shadows] Patch (set to 1), MapScale, Taps`; `[gi] Enable, Mode (set to 1), Rays`; `[debug] CaptureFrame` and `CaptureFrame2` (both set to 0).
- Preset click only (`src/launcher_ui.inc:493`): `[atmos] ContactShadows` (0.35 or 0), `Intensity`, `Desaturation`, `HazeLift`; `[post] Brightness`, `Saturation`.
- Slider keys, written on every save (`src/launcher_ui.inc:17-33`): `[post] Brightness, Contrast, Saturation, Sharpen`; `[ao] Strength, WideStrength`; `[shadows] Strength, GrassStrength`; `[gi] Strength`; `[fog] Amount, Distance`; `[atmos] Intensity, SunIntensity, ContactShadows`; `[tess] ReliefDepth`.
- `[shots] Preset`, written on every refresh (`src/launcher_ui.inc:242`). It names the A/B screenshot files. Value: the preset name or `Custom`.
- The game's own `settings.ini` (not the mod's file): the seven detail keys listed above.

Slider label and range (from `src/launcher_ui.inc:18-32`):

| Label | Key | Range |
|---|---|---|
| Brightness | `[post] Brightness` | 0.70 to 1.50 |
| Contrast | `[post] Contrast` | 0.80 to 1.30 |
| Colour intensity | `[post] Saturation` | 0.60 to 1.60 |
| Crevice shadows | `[ao] Strength` | 0.00 to 1.50 |
| Corner darkness | `[ao] WideStrength` | 0.00 to 2.00 |
| Shadow darkness | `[shadows] Strength` | 0.00 to 1.00 |
| Shadows on grass | `[shadows] GrassStrength` | 0.00 to 1.00 |
| Bounce light | `[gi] Strength` | 0.00 to 3.00 |
| Fog amount | `[fog] Amount` | 0.00 to 3.00 |
| Fog distance | `[fog] Distance` | 0.30 to 4.00 |
| Distance haze | `[atmos] Intensity` | 0.00 to 4.00 |
| Light shafts | `[atmos] SunIntensity` | 0.00 to 8.00 |
| Contact shadows | `[atmos] ContactShadows` | 0.00 to 1.00 |
| Surface relief | `[tess] ReliefDepth` | 0.00 to 10.00 |
| Sharpening | `[post] Sharpen` | 0.00 to 1.00 |

Slider positions are integers 0 to 100 (`src/launcher_ui.inc:179-183`), so a stored value is always the nearest step of 1 percent of the range.

Shadow sharpness combo: the three items write `[shadows] MapScale` 1, 2 and 4 (`src/launcher_ui.inc:199`, `:453-455`). The item text says the change applies on the next game start (`:456`).

Edge smoothing combo: four items, `[post] AA` 0 to 3 (`src/launcher_ui.inc:447-449`). Presets use only 1 and 2.

## Cost

The launcher shows a model, not a measurement. `CostEstimate` (`src/launcher_ui.inc:44-56`) adds a fixed cost per effect, scaled by pixel count (`g_pixScale`, `:207`). It then multiplies the total by `1 + 0.0075 * (total - 7)` when the total is above 7 ms (`:55`).

Tag thresholds (ms): per option `Free` below 0.2, `Low` below 1.5, `Medium` below 3.5, `High` at 3.5 or more (`:148`). Overall label: `LOW` below 1.5, `MEDIUM` below 7, `HIGH` below 14, `EXTREME` at 14 or more (`:149`, `:237`).

The overall label is a model output, so the values below are checked against two sources. The model's own comment (`src/launcher_ui.inc:46-47`) and the measured table `docs/results/preset_matrix.md` do not agree.

| Preset | Model value (hand-evaluated from `:44-56`, inferred, not run) | Measured added ms (`docs/results/preset_matrix.md`) | Measured frame ms (same file) |
|---|---|---|---|
| Stock | 0 | 0 | 5.73 |
| Low | about 1.1 | 0.29 | 6.02 |
| Medium | about 5.7 | 5.14 | 10.87 |
| High | about 12.4 | 11.51 | 17.24 |
| Ultra | about 24.9 | 26.38 | 32.11 |

Notes on the table:
- The measurement file does not name the build or the GPU. The launcher comment says RX 7600 XT at 2560x1440 (`src/launcher_ui.inc:10`).
- The launcher comment at `:46-47` quotes Medium 6.8, High 17.9, Ultra 26.4 over stock. Its own formula gives different Medium and High values, so the comment's figures do not match the code.
- Low is over-estimated by about 0.8 ms. The shadow branch uses a flat 0.6 for map scale 1 (`:53`), and that cost is not in the measured Low result.
- The model is a fit, with no error bar. Accuracy is an open question (NI-L1 below).
- Fps is never shown by the launcher. The status line shows only the overall label and the resolution (`:238`).

## Limits

- Windows only. Win32 and GDI controls, with XInput loaded at run time (`src/launcher_ui.inc:375-376`).
- Every save writes all 15 slider keys (`:246`, `:183`). A value typed by hand in `tyrender.ini` is rounded to the nearest 1 percent step on the next slider move, preset click, checkbox change, Play, Play original, Advanced, or Steam setup. This is inferred from the code and was not run.
- Every save also resets `[debug] CaptureFrame`, `CaptureFrame2`, and `[tess] Debug` (`:64`, `:68`). It also sets `[ao] Legacy`, `[gi] Mode`, `[shadows] Patch` and `[atmos] Froxel` to the launcher's values (`:62`, `:63`, `:66`, `:67`). Testers who use those keys by hand will lose the change.
- The game detail checkbox edits a file outside the mod folder. Uninstall does not restore it (see the installer page).
- Slider defaults in the launcher are not the same as the DLL's in-code defaults for seven keys. See Discrepancies. The launcher value wins once any save has happened, and Play always saves.
- The Steam option string is copied to the clipboard (`:506`). It is not checked against what Steam stores.
- The launcher does not check whether the game's files match stock. There is no hash check (see [installer-and-uninstaller.md](installer-and-uninstaller.md)).
- Live changes only work while the game window title matches exactly (`:165`).
- The cost model is per GPU-time, at 2560x1440 as the reference. Other resolutions are scaled linearly (`:207`, `:51-53`). This is inferred from the formula.

## Known issues

- Gamepad support is untested (`CHANGELOG.md:27`). Steam Big Picture and Steam Deck behaviour is not verified (NI-L3).
- The launcher redesign is tracked as #111 and #112 (`CHANGELOG.md:27`). The clash guard is #13 (`src/launcher_ui.inc:102`).
- The cost numbers in the code comments disagree with each other and with the measured table (see Cost).
- The F10 in-game overlay is a separate feature. Its issues (#12, #54, #59, #63, #79, #102) are covered on the [overlay page](../how-it-works/09-overlay-input-and-hotkeys.md).

## How to verify

1. Start `TYLauncher.exe` with no game running. Before the first Play, the status line reads "Ready to play..." (`:172-175`). Check that `tyrender.dll` now sits next to the launcher (`src/TYLauncher.c:47`).
2. Click Medium. Open `tyrender.ini`. `[ao] Slices` should be 3 and `Steps` 6, and `[post] Brightness` should be 0.94 (`src/launcher_ui.inc:12`, `:493`).
3. Click Ultra. The overall label should read EXTREME (`:149`, `:237`).
4. Open `tyrender.ini`. `[shots] Preset` should match the preset you clicked (`:242`).
5. Run `TYLauncher.exe --noinject` from a shell. The game starts with the untouched `TY.exe`, and the DLL is not loaded, so no new `tyrender.log` lines appear (`src/launcher_ui.inc:549`, `src/TYLauncher.c:55-56`).
6. Start the game with Play. If the game is running, move a slider. The status line shows "Game running" (`:174`), and the DLL should log `[cfg] reloaded all` in `tyrender.log` (`src/tyrender.cpp:603`).
7. Click Report a problem. The folder `<game>\tyrender\logs` opens (`:500`).

## NEEDS INVESTIGATION

- NI-L1 (Q120, accuracy of the cost model): Run the four presets with the timer tools (see [tools-reference.md](tools-reference.md), `tools/perf_matrix.ps1`) on a named GPU and build. Compare with the output of `CostEstimate` for the same settings. Until then, the model is "fitted, not validated".
- NI-L2 (N-20, N-23): costs at 1920x1080 and 21:9 are not measured. The launcher scales linearly. Method: run the same matrix at those sizes.
- NI-L3 (N-10): Steam Deck and Proton behaviour. Method: run the launcher and gamepad under Proton on a Steam Deck.
- NI-L4 (N-9): Steam overlay and achievements with `TY_tyrender.exe`. Method: launch from Steam with the launch option and check overlay and achievements.
- NI-L5: What happens to Play if `TY_tyrender.exe` is running or locked. The code path reads the file attributes only (`src/launcher_ui.inc:257`). Method: run Play twice, once with the patched exe open.

## Discrepancies

Between the first-pass `LAUNCHER.md` and the code (code wins):

1. `--noinject`. First pass says the patched exe is used. Code starts the untouched `TY.exe` with no DLL (`src/launcher_ui.inc:549`, `:556`, `src/TYLauncher.c:55-56`).
2. `ui_part_a.inc` and `ui_part_c.inc` are described as "included parts of the launcher UI". Nothing in the repo includes them, and no file mentions them (`src/TYLauncher.c:18-79` includes only `patch_exe.inc`, `build_id.h`, `launcher_ui.inc`). They are stale copies. `ui_part_a.inc` has older values: haze default 2.20 (`src/ui_part_a.inc:27`), Saturation default 1.03 (`:19`), and Surface relief bound to `[tess] Displacement` instead of `ReliefDepth` (`:30`). The header of `ui_part_a.inc:1` claims it is included by `TYLauncher.c`, which is false.
3. `Lvl` location. First pass cites `src/ui_part_c.inc` line 13. The active code is `src/launcher_ui.inc:148`.
4. Slider table. First pass lists 13 sliders and says the last two were not read. The code has 15 (`src/launcher_ui.inc:17-33`). The two missing sliders are Surface relief (`[tess] ReliefDepth`, `:31`) and Sharpening (`[post] Sharpen`, `:32`).
5. `[shadows] Strength` and `GrassStrength`. First pass says their effect is not verified. The DLL reads both with defaults 0.5 and 0.35 (`src/tyrender.cpp:156-157`). Neither key is in the shipped ini (`src/default_tyrender.ini`), so the DLL defaults apply until a save.
6. Backup restore. First pass says the uninstaller restores `settings.ini.tyhd_backup`. It does not. The name appears only in `src/launcher_ui.inc:86`, and `src/installer/uninstall.c` never reads it.
7. Haze value 2.2. First pass attributes 2.2 to the README preset text. The README says "presets write 2.2" (`README.md:292`). The code writes 2.5, 1.2, 1.2 and 1.0 (`src/launcher_ui.inc:7`). The 2.2 value only appears in the stale `ui_part_a.inc:7`.
8. Preset grade comment. Minor. The comment on `:8` is attached to the wrong declaration and refers to haze.
9. AA values. First pass says only 1 and 2 are meaningful. The UI offers 0 to 3 (`src/launcher_ui.inc:448-449`). The cost model treats 2 and 3 alike (`:52`). Presets use 1 and 2.
10. Cost totals. The code comment (`src/launcher_ui.inc:46-47`) and the measured table (`docs/results/preset_matrix.md`) disagree (see Cost).
11. Slider defaults. The launcher's slider defaults differ from the DLL's in-code defaults for these keys (launcher value first, DLL value second; both cited):

| Key | Launcher default (`src/launcher_ui.inc`) | DLL in-code default | Shipped `default_tyrender.ini` |
|---|---|---|---|
| `[post] Brightness` | 0.94 (`:18`) | 1.0 (`src/fx.inl:154`) | 0.94 (line 52) |
| `[post] Saturation` | 1.18 (`:20`) | 1.0 (`src/fx.inl:153`) | 1.18 (line 53) |
| `[post] Sharpen` | 0.35 (`:32`) | 0.25 (`src/fx.inl:153`) | not present |
| `[ao] Strength` | 0.60 (`:21`) | 0.85 (`src/ao.inl:500`) | not present |
| `[atmos] Intensity` | 1.20 (`:28`) | 2.5 (`src/atmos.inl:297`) | 1.2 (line 36) |
| `[atmos] SunIntensity` | 2.00 (`:29`) | 1.4 (`src/atmos.inl:295`) | not present |
| `[atmos] ContactShadows` | 0.00 (`:30`) | 0.35 (`src/atmos.inl:296`) | 0 (line 35) |

Effect: a setup install (which uses the shipped ini) gets the launcher values on the first save, because `SaveAll` writes every slider key (`src/launcher_ui.inc:246`). A user who edits the ini by hand and never saves from the launcher gets the DLL values, so the sliders and the picture disagree.

12. Preset Medium differs between the launcher and `tools/presets.ps1` (`:15`), which lists `aoSlices` 2, `aoSteps` 4, `atmosSteps` 16, `shTaps` 12. The launcher and the shipped ini agree on 3, 6, 24 and 16. See [presets.md](presets.md).

## See also

- [presets.md](presets.md): preset values and the cost ladder
- [settings-ao-gi-lod.md](settings-ao-gi-lod.md), [settings-post-shadows-light-pbr-tess.md](settings-post-shadows-light-pbr-tess.md), [settings-atmos-fog-emit.md](settings-atmos-fog-emit.md): what the keys do
- [installer-and-uninstaller.md](installer-and-uninstaller.md): how the launcher gets onto the disk
- [hotkeys-and-files.md](hotkeys-and-files.md): the reload flag and other flag files
- [../how-it-works/08-settings-pipeline-and-live-reload.md](../how-it-works/08-settings-pipeline-and-live-reload.md): where defaults come from
- [../guides/session-report.md](../guides/session-report.md): what the report button collects

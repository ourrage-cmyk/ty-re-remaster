# Presets: Low, Medium, High, Ultra

> Status: documented at commit 4db1d7d (`main`, Merge PR #122); last verified 2026-10-18. Audience: B (tuning and bug reports), C (review).

## Purpose

This page answers Q118, Q119, Q120, Q124 and Q125. It says what each of the four launcher presets writes into `tyrender.ini`, what they cost in GPU time as measured so far, how the launcher estimates cost, what "Custom" means, and where the presets are copied in the repo. The presets are defined in the launcher (`src/launcher_ui.inc:7-14`). The shipped `tyrender.ini` is the Medium preset.

## Preset values (Q119)

Source: `kPresetVals` (`src/launcher_ui.inc:9-15`), `kPresetHaze` (line 7) and `kPresetGrade` (line 8). Each preset also writes the grade, haze and contact values described below.

| Setting | Low | Medium | High | Ultra | Written to |
|---|---|---|---|---|---|
| AO switch | 0 (off) | 1 | 1 | 1 | `[ao] Enable` |
| AO slices x steps | 2 x 4 | 3 x 6 | 3 x 6 | 4 x 8 | `[ao] Slices`, `Steps` |
| Volumetric fog switch | 0 (off) | 1 | 1 | 1 | `[atmos] Enable` |
| Fog steps | 12 | 24 | 24 | 32 | `[atmos] Steps` |
| Surface relief | 0 (off) | 1 | 1 | 1 | `[tess] Enable` |
| Relief scope | skin | skin | skin | all | `[tess] Scope` |
| Relief max level | 4 | 8 | 8 | 24 | `[tess] MaxLevel` |
| Post chain | on | on | on | on | `[post] Enable` |
| Anti-aliasing mode | 1 (FXAA) | 2 (TAA) | 2 (TAA) | 2 (TAA) | `[post] AA` |
| Shadow map scale | 1 (2048) | 2 (4096) | 2 (4096) | 4 (8192) | `[shadows] MapScale` |
| Shadow taps | 8 | 16 | 16 | 32 | `[shadows] Taps` |
| Bounce light switch | 0 (off) | 0 (off) | 1 (on) | 1 (on) | `[gi] Enable` |
| Bounce light rays | 1 | 1 | 1 | 2 | `[gi] Rays` |
| Contact shadows | off | off | 0.35 | 0.35 | `[atmos] ContactShadows` |
| Haze intensity | 2.5 | 1.2 | 1.2 | 1.0 | `[atmos] Intensity` |
| Haze desaturation / lift | 0.8 / 0.35 | 0.3 / 0.15 | 0.3 / 0.15 | 0.3 / 0.15 | `[atmos] Desaturation`, `HazeLift` |
| Brightness / saturation | 1.00 / 1.03 | 0.94 / 1.18 | 0.94 / 1.18 | 0.94 / 1.18 | `[post] Brightness`, `Saturation` |

Source for every row: `src/launcher_ui.inc:9-15` (field order ao, aoSlices, aoSteps, atmos, atmosSteps, tess, tessAll, tessLevel, post, aa, shScale, shTaps, gi, giRays, contact), lines 7-8 for haze and grade.

Settings that every preset writes unchanged (`SaveSettings`, `src/launcher_ui.inc:61-68`): `[ao] Legacy` = 0, `[atmos] Froxel` = 1, `[tess] Debug` = 0, `[shadows] Patch` = 1, `[gi] Mode` = 1, `[debug] CaptureFrame` and `CaptureFrame2` = 0. So the launcher never writes `Legacy=1`, `Froxel=0` or the patch off.

The shipped `tyrender.ini` (`src/default_tyrender.ini`) agrees with the Medium column on every row it contains: AO 1 / 3 / 6 (`ini:3-6`), GI off with 1 ray (`ini:8-10`), fog 24 steps and haze 1.2 (`ini:34`, `ini:36`), relief `skin` 8 (`ini:42-44`), shadows 2 and 16 (`ini:46-48`), AA 2 and grade 0.94 / 1.18 (`ini:51-53`). It does not contain `[tess] Scope` as `env` or `all`.

## What a preset click does

Source: `src/launcher_ui.inc:493`.

1. Copies the preset into the launcher's settings struct and refreshes the toggles (`ToUI`).
2. Writes four values straight into the ini: `ContactShadows` (0.35 or 0), `Intensity` (haze), `Desaturation` and `HazeLift` (rows above), and `Brightness` and `Saturation` (grade).
3. Reloads the slider values from the ini (`SlidersFromIni`), then calls `SaveAll` (`src/launcher_ui.inc:246`). `SaveAll` writes every setting and all 15 slider keys, and if the game is running it writes the reload file.

After a click, the ini holds the slider values, not only the preset values. The 15 slider keys are listed in the table below.

## Launcher sliders

The sliders are in `g_sl[]` (`src/launcher_ui.inc:17-32`). Their defaults are the launcher's own and are written on every save.

| Slider | Ini key | Range | Launcher default | Code default (file:line) |
|---|---|---|---|---|
| Brightness | `[post] Brightness` | 0.70 to 1.50 | 0.94 | 1.0 (`src/fx.inl:154`) |
| Contrast | `[post] Contrast` | 0.80 to 1.30 | 1.00 | 1.0 (`src/fx.inl:153`) |
| Colour intensity | `[post] Saturation` | 0.60 to 1.60 | 1.18 | 1.0 (`src/fx.inl:153`) |
| Crevice shadows | `[ao] Strength` | 0.00 to 1.50 | 0.60 | 0.85 (`src/ao.inl:500`) |
| Corner darkness | `[ao] WideStrength` | 0.00 to 2.00 | 1.00 | 1.0 (`src/ao.inl:494`) |
| Shadow darkness | `[shadows] Strength` | 0.00 to 1.00 | 0.50 | 0.5 (`src/tyrender.cpp:156`) |
| Shadows on grass | `[shadows] GrassStrength` | 0.00 to 1.00 | 0.35 | 0.35 (`src/tyrender.cpp:157`) |
| Bounce light | `[gi] Strength` | 0.00 to 3.00 | 0.70 | 0.7 (`src/gi.inl:93`) |
| Fog amount | `[fog] Amount` | 0.00 to 3.00 | 1.00 | 1.0 (`src/tyrender.cpp:723`) |
| Fog distance | `[fog] Distance` | 0.30 to 4.00 | 1.00 | 1.0 (`src/tyrender.cpp:722`) |
| Distance haze | `[atmos] Intensity` | 0.00 to 4.00 | 1.20 | 2.5 (`src/atmos.inl:297`) |
| Light shafts | `[atmos] SunIntensity` | 0.00 to 8.00 | 2.00 | 1.4 (`src/atmos.inl:295`) |
| Contact shadows | `[atmos] ContactShadows` | 0.00 to 1.00 | 0.00 | 0.35 (`src/atmos.inl:296`) |
| Surface relief | `[tess] ReliefDepth` | 0.00 to 10.00 | 3.00 | 3 (`src/tess.inl:48`) |
| Sharpening | `[post] Sharpen` | 0.00 to 1.00 | 0.35 | 0.25 (`src/fx.inl:153`) |

Slider rows: `src/launcher_ui.inc:18-32`. The fog defaults are read at `src/tyrender.cpp:722-723` at startup and again at `src/tyrender.cpp:601-602` on reload. Where the slider default differs from the code default, the file that the launcher writes is what most users run. The code default is used only when the key is missing from the file.

## Measured frame times (Q119, Q124)

GPU milliseconds per frame, 2560x1440, RX 7600 XT (the only GPU tested). Each row names its source build. The table does not replace the sources.

| Source | Build | Stock | Low | Medium | High | Ultra |
|---|---|---|---|---|---|---|
| `docs/results/preset_matrix.md` lines 3-7 | v0.3 merged build, one run | 5.73 | 6.02 | 10.87 | 17.24 | 32.11 |
| `src/launcher_ui.inc:10` comment | merged build | 5.7 | 6.0 | 11.1 | 17.2 | 32.1 |
| `docs/PERFORMANCE.md` lines 11-15 | v0.3.2 text, rounded | 5.7 | about 6.0 | about 12 | about 18 | about 31 |
| `docs/STATUS_2026-10-13.md` lines 9-12 | v0.3.2, G1 gate for Medium | - | 6.0 | 12.1 (gate 12.08) | about 18 | about 31 |
| `docs/STATUS_2026-10-13.md` lines 13-14 | v0.3.3 test build, smoke run, one run | - | - | 12.05 | 19.80 | - |
| `docs/PERFORMANCE.md` line 36 | v0.3.2 G1 gate, default view | - | - | 12.08 | - | - |

Budget: 16.7 ms is 60 fps (`docs/GOAL.md` lines 11 and 16; `docs/STATUS_2026-10-13.md` line 15; `docs/PERFORMANCE.md` line 3). Low and Medium are inside it on the test card; High and Ultra are above it by design (STATUS line 15).

Stock frame time, which is needed for "cost over stock", has four values in the repo: 4.6 ms (`docs/GOAL.md` line 11; `src/launcher_ui.inc:39`), 5.38 ms (`docs/STATUS_2026-10-13.md` line 42, session 5 A/B at that view), 5.4 to 5.7 ms (`src/launcher_ui.inc:45`), and 5.7 ms (`docs/PERFORMANCE.md` line 3; README). They are not all the same test view or build. Use the Stock row of the table for each build.

Forced fallback (every effect off by the safety path) renders at 7.44 ms (`docs/PERFORMANCE.md` line 40, v0.3.2).

## Cost of each option (Q125)

Reference is "C1" (the Balanced candidate, which has the same settings as the shipped file, see below), 11.09 ms in the merged v0.3 build (`docs/results/cost_table.md` line 11). Positive means the option makes the frame slower.

| Variant | Frame ms | vs C1 | Source line |
|---|---|---|---|
| contact shadows 0.35 | 12.8 | +1.71 | cost_table.md line 12 |
| shadow map 4 (8192) with 32 taps | 16.21 | +5.12 | cost_table.md line 13 |
| bounce light Hi-Z, 1 ray | 15.51 | +4.42 | cost_table.md line 14 |
| relief scope all | 17.31 | +6.22 | cost_table.md line 15 |
| AO off | 9.94 | -1.15 | cost_table.md line 16 |
| fog and light shafts off | 9.00 | -2.09 | cost_table.md line 17 |
| relief off | 11.04 | -0.05 | cost_table.md line 18 |
| TAA off (FXAA or none) | 11.49 | +0.40 | cost_table.md line 19 |

The same effects appear in `docs/PERFORMANCE.md` lines 48-58 as values measured against Medium on the v0.3 build. Effects interact, so the costs are not additive: `docs/STATUS_2026-10-13.md` line 40 gives about +20% when combined and up to +67% at Ultra (cause unexplained).

## How the launcher estimates cost (Q120)

`CostEstimate` (`src/launcher_ui.inc:44-56`) adds one fitted cost per effect in milliseconds at 2560x1440 and scales it by the pixel count of the game's resolution (`g_pixScale`, read in `GameResolution`). The fitted values sit in the comments at `src/launcher_ui.inc:39-46`: AO about 0.5 plus 0.051 per slice-step product, fog about 2.1, contact shadows 1.6, relief 0.3 for `skin` and 4.2 for `all`, shadows 1.06 at 4096 with 16 taps, TAA 0.8, bounce light 4.7 for one ray plus 0.8 per extra ray. When the total is over 7 ms the total gets a small extra factor (0.75 percent per ms over 7). The per-slider tags use the same numbers.

How accurate it is: the comments in the same file disagree with each other. The preset comment (`src/launcher_ui.inc:10`) gives Medium 11.1 ms in total, while the cost comment (`src/launcher_ui.inc:46`) gives Medium 6.8 ms over stock. Those two cannot both be right with a stock of 5.4 to 5.7 ms. The estimate was fitted on one GPU and is a guide, as the launcher itself says (`docs/PERFORMANCE.md` line 88).

## Custom

`MatchPreset` (`src/launcher_ui.inc:209-214`) compares the fields of the settings struct: the toggles, AO slices and steps, fog steps, relief scope and level, AA, shadow scale and taps, GI rays, and whether contact shadows are on. It does not compare the sliders, the grade or the haze values. A slider therefore leaves the preset name in place, except the contact-shadow slider, which sets the contact flag (`src/launcher_ui.inc:222`). A toggle change shows "Custom mix". When no preset matches, the note says "Custom mix: you changed something" and the screenshot preset name becomes `Custom` (`src/launcher_ui.inc:242`).

## Where the presets are copied

- `tools/presets.ps1` (`NewIni` at lines 3-12, `$Presets` at lines 13-18, `$BalancedCandidates` at lines 20-28) is a second copy for the test harness. Its `NewIni` writes a complete ini with the other keys left to the DLL defaults.
- `src/ui_part_a.inc` and `src/ui_part_c.inc` are older launcher copies. No file in the repo includes them: `src/TYLauncher.c` includes `launcher_ui.inc` (line 79) and nothing else from that group. Their values differ (for example `ui_part_a.inc:7` gives haze 2.5 / 2.2 / 2.2 / 2.1, and `ui_part_c.inc` holds the preset titles "Low - potato mode" and similar, line 110). The first-pass LAUNCHER.md describes these titles as live; they are not in the built launcher.

Differences between `tools/presets.ps1` and the launcher:

| Field | Launcher (`src/launcher_ui.inc:9-15`) | `tools/presets.ps1` |
|---|---|---|
| Medium AO slices x steps | 3 x 6 | 2 x 4 (line 15) |
| Medium fog steps | 24 | 16 (line 15) |
| Medium shadow taps | 16 | 12 (line 15) |
| Low and all presets Desaturation / HazeLift | 0.8 / 0.35 for Low, 0.3 / 0.15 for others | 0.3 / 0.15 for every preset (line 7) |
| Low, High, Ultra core fields | as in the table above | match |

"Balanced" (`presets.ps1` candidate C1, line 24) has the same values as the shipped file (AO 3 x 6, fog 24, relief skin 8, shadows 2 and 16, AA 2, bounce light off). `docs/GOAL.md` line 26 says the name "Balanced" is retired and Medium is the old Balanced. The name still appears in `src/package.ps1` line 9 (comment "= Balanced preset"), in the cost comment at `src/launcher_ui.inc:46`, and in the title of `docs/results/cost_table.md`. `src/default_tyrender.ini` line 1 calls the same values "Medium".

## Discrepancies

1. **Medium frame time.** 12.08 ms (G1 gate, `docs/PERFORMANCE.md` line 36), about 12 ms (README), 11.1 ms (launcher comment, `src/launcher_ui.inc:10`), 10.87 ms (`docs/results/preset_matrix.md` line 5, merged v0.3 build), 12.05 ms (v0.3.3 smoke run, STATUS line 14). These come from different builds. The G1 gate number is the release figure.
2. **Ultra haze.** Launcher 1.0 (`src/launcher_ui.inc:7`). `docs/STATUS_2026-10-13.md` line 12 says haze 2.1. The README says presets write 2.2. Launcher value used.
3. **Stock frame time.** Four values (see above).
4. **Name "Balanced" and "Medium".** Same values, two names (see above).
5. **Launcher cost comments.** `src/launcher_ui.inc:10` and `:46` disagree on the Medium total (see above).
6. **STATUS relief label.** `docs/STATUS_2026-10-13.md` line 10 says Medium uses `env-detail` relief; the launcher and the shipped file use `skin` (`src/launcher_ui.inc:74`, `ini:43`).
7. **Presets in tools.** `tools/presets.ps1` differs from the launcher (table above).
8. **Slider defaults versus code defaults.** Sharpen, Light shafts, Contact shadows and Crevice shadows have launcher defaults that differ from the code defaults (table above). Each save writes the launcher value into the file.

## NEEDS INVESTIGATION

- **Preset costs on the current build.** None of the table rows is measured on the current `main` (4db1d7d). Method: `tools/perf_matrix.ps1` or `tools/g1_gate.ps1` on the test card (plan N-20).
- **Cost model on other GPUs.** Method: a second GPU with the same runs (plan N-21).
- **Which preset source is the owner's reference.** Launcher versus `tools/presets.ps1`. Owner decision.
- **Ultra haze 2.1 in STATUS.** Stale value or a build difference. Method: owner or the STATUS author; compare with a v0.3.2 launcher build.
- **Medium cost comment in `src/launcher_ui.inc`.** Method: owner decision on the right fitted total (see Discrepancies 5).

## See also

- [settings-ao-gi-lod.md](settings-ao-gi-lod.md), [settings-atmos-fog-emit.md](settings-atmos-fog-emit.md), [settings-post-shadows-light-pbr-tess.md](settings-post-shadows-light-pbr-tess.md), [settings-system-debug.md](settings-system-debug.md)
- [how-it-works/08-settings-pipeline-and-live-reload.md](../how-it-works/08-settings-pipeline-and-live-reload.md)
- [launcher](launcher.md), [performance costs](../../PERFORMANCE.md) (`docs/PERFORMANCE.md`)
- [test harness](../guides/test-harness.md)

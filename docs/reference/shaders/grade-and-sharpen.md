# Sharpen, colour grade and tonemap (final colour pass)

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributors), B (testers who tune `[post]`).

## Purpose

This is the last shader on the image before the HUD. It does four jobs in one fragment program:

1. a light sharpen that, with the default halo clamp, cannot create dark or bright rims;
2. a colour grade (brightness, saturation, vibrance, contrast);
3. a soft shoulder and highlight roll-off, with a small split tone;
4. gamma and an optional vignette.

It writes into the default framebuffer, so the HUD and overlay are drawn on top of it afterwards. It runs after the AA passes of the same module. The AA side is on [taa-fxaa.md](taa-fxaa.md).

The program is `kGrade` in `src/fx.inl:57-95`. Its settings are read by `LoadFXConfig` (`src/fx.inl:146-157`). The first-pass note that sharpen was "(inferred) part of this program" is now confirmed: sharpen is the first block of `kGrade` (`src/fx.inl:63-71`).

## How it works

Steps run in this order, per pixel, on the output of TAA or FXAA (`src/fx.inl:57-95`):

1. **Sharpen** (only when `Sharpen` is above 0, `src/fx.inl:64`). It reads the four edge neighbours. It computes the local minimum and maximum of the five samples (line 67). The amount is high in flat areas and low where local contrast is already high (line 68). It applies a negative-weight neighbour kernel scaled by `Sharpen` (line 69). The result is then clamped to the local minimum and maximum, mixed by `SharpenHaloClamp` (line 70). With the clamp at 1.0, the result cannot leave the local range, so no dark or bright halo appears (comment on line 70).
2. **Brightness** multiplies the colour (line 73).
3. **Saturation and vibrance.** The colour is pulled toward its luma by `Saturation`. Dull colours get more push from `Vibrance` than saturated ones (lines 74-76).
4. **Contrast** scales the colour around mid-grey 0.5 (line 77). The colour is then clamped to 0 to 1 (line 78).
5. **Shoulder.** A blend toward a cubic S-curve that keeps 0 and 1 fixed, weighted by `Shoulder` (line 79). The code comment calls it a filmic S-curve.
6. **Tonemap roll** (only when `TonemapRoll` is above 0 and `Tonemap` is 1, `src/fx.inl:252`). Above `TonemapKnee`, the brightest channel is compressed by a quadratic roll-off. The colour is scaled to keep its hue (lines 80-82). Highlights above about 0.82 are partly desaturated (lines 83-84).
7. **Split tone** (only when `SplitTone` is above 0, `src/fx.inl:252`). Shadows lean slightly cool and highlights slightly warm (lines 86-90).
8. **Gamma** raises the colour to 1 over `Gamma` (line 91). A value of 1.0 changes nothing.
9. **Vignette** darkens toward the corners by `Vignette` times twice the squared distance from the centre (line 92). The default 0 turns it off.

Uniform values are set per frame from the config in `RunFX` (`src/fx.inl:248-252`).

## Code pointers

| What | file:line | Note |
|---|---|---|
| Grade program | `src/fx.inl:57-95` | `kGrade` |
| Sharpen block | `src/fx.inl:63-71` | neighbour kernel, amplitude, halo clamp |
| Grade and tonemap | `src/fx.inl:72-92` | order listed above |
| Uniform upload | `src/fx.inl:248-252` | `Tonemap=0` zeroes roll and split |
| Defaults in the loader | `src/fx.inl:144`, `src/fx.inl:153-155` | the two lists agree |
| Write to default framebuffer | `src/fx.inl:246` | HUD and overlay follow |
| Post on/off and A/B | `src/fx.inl:178` | `post_off` file or A/B off frame skip the whole pass |

## Settings

Keys that matter here. The full table is in [settings-post-shadows-light-pbr-tess.md](../reference/settings-post-shadows-light-pbr-tess.md).

| Key | Code default | Shipped Medium | Live or restart | Effect |
|---|---|---|---|---|
| `[post] Sharpen` | 0.25 (`src/fx.inl:153`) | not set in the shipped file (see Discrepancies) | live | Sharpen strength. Launcher slider range 0 to 1 (`src/launcher_ui.inc:32`) |
| `[post] SharpenHaloClamp` | 1.0 (`src/fx.inl:153`) | not set | live | 1 keeps sharpen inside the local range. 0 lets halos through |
| `[post] Brightness` | 1.0 (`src/fx.inl:154`) | 0.94 (`default_tyrender.ini:52`) | live | Overall brightness multiplier |
| `[post] Saturation` | 1.0 (`src/fx.inl:153`) | 1.18 (`default_tyrender.ini:53`) | live | Colour intensity |
| `[post] Vibrance` | 0.04 (`src/fx.inl:154`) | not set | live | Extra push for dull colours |
| `[post] Contrast` | 1.0 (`src/fx.inl:153`) | not set | live | Contrast about mid-grey |
| `[post] Shoulder` | 0.12 (`src/fx.inl:154`) | not set | live | Strength of the S-curve |
| `[post] Tonemap` | 1 (`src/fx.inl:155`) | not set | live | 0 turns off roll and split tone |
| `[post] TonemapKnee`, `TonemapRoll` | 0.72, 0.15 (`src/fx.inl:154`) | not set | live | Where the roll starts and how strong it is |
| `[post] SplitTone` | 0.12 (`src/fx.inl:154`) | not set | live | Cool shadow, warm highlight tint |
| `[post] Gamma`, `Vignette` | 1.0, 0.0 (`src/fx.inl:154`) | not set | live | Gamma and corner darkening |

Live: all `[post]` keys are re-read on reload (`src/tyrender.cpp:598`) and by the overlay (`src/overlay.inl:52`). `docs/PERFORMANCE.md:84` lists `[post]` as live.

## Cost

- **No measurement of the grade alone** exists in the repo. Per-pass timers under-count (`docs/PERFORMANCE.md:60`).
- **Launcher cost model (estimate).** The finishing pass is charged 0.3 ms with AA off, 0.5 ms with FXAA and 0.8 ms with TAA, at 2560x1440 (`src/launcher_ui.inc:55`). The model does not split out sharpen or the grade.
- **Sharpen halo clamp:** "negligible" (`docs/PERFORMANCE.md:82`).
- **What scales it:** pixel count. Sharpen reads four extra taps. The rest is arithmetic.

## Limits

- **LDR in and LDR out.** The pass works on 8-bit colour (`src/fx.inl:72`, `src/fx.inl:171`). Values above 1.0 are clipped at the clamp on line 78 before the shoulder and roll see them. The roll therefore cannot recover highlights that the engine already clipped.
- **Sharpen uses only the four edge neighbours.** Fine features that are narrower than a pixel are not restored.
- **Sharpen amplifies residual AA noise,** because it runs after TAA (inferred).
- **The halo clamp can flatten edges.** With the clamp at 1.0, a sharpen edge can never overshoot its local range. Strong edges therefore sharpen less than a plain unsharp mask would (inferred from line 70).
- **Global curves.** Shoulder, roll, split tone and vignette are fixed functions of the colour or the screen position. They do not follow exposure or scene content (inferred from the code).
- **Failure takes the whole post module.** If `kGrade` fails to compile, `fx.failed` is set and no grade runs for the session (`src/fx.inl:184`). See [taa-fxaa.md](taa-fxaa.md).

## Known issues

- **Over-sharpened look.** The user recording summary lists "dark outline around leaves (over sharpened)" and white outlines on shadows (`docs/recording/FINDINGS_ROUND2.md:14`). The first-pass summary records that item as partly fixed, with shadow residue still open (`KNOWN_ISSUES_AND_HISTORY.md`, first pass, row G5). Current status is not verified at this commit (N-07.4).
- **Halo guard turned off.** `SharpenHaloClamp` at 0 is a bisect aid only. The default is 1.0 (`src/fx.inl:153`).
- **Overlay exposes Sharpen live.** The F10 overlay has a sharpen entry (`CHANGELOG.md`, Added, "F10 settings overlay (#12)"). The overlay was reported as not working in build 4a2ef95 (`README.md`, Known issues, F10 overlay line). Its live behaviour is untested.

## How to verify

1. **Sharpen on and off.** Set `[post] Sharpen=0` and compare an F11 pair with the shipped value. Edges should look softer, with no change to brightness or colour.
2. **Halo clamp.** Set `SharpenHaloClamp=0` and look at a leaf or fence edge against bright sky at 100 percent. Dark or bright rims appear with the clamp off, and not with it on (comment at `src/fx.inl:70`).
3. **Tonemap.** Set `Tonemap=0`. Highlights should stop rolling off and the split tone should disappear (`src/fx.inl:252`).
4. **Whole pass off.** Create the `post_off` flag file in `<game folder>\tyrender\` (`src/fx.inl:6`). The grade, sharpen and AA all stop. Restart or wait for the next reload check.
5. **Log lines.** `[fx] first run ok at frame N` shows the pass ran (`src/fx.inl:261`).
6. **Which value is live.** Read the `[post]` section of `<game folder>\tyrender.ini` after a launcher save. If it says `Sharpen=0.350`, the launcher wrote the 0.35 slider default (see Discrepancies).
7. **No debug view.** `src/fx.inl` has no debug output for this pass (inferred). Use A/B pairs ([ab-captures.md](../guides/ab-captures.md)).

## NEEDS INVESTIGATION

- **N-07.4 Current over-sharpening and shadow-rim status.** Method: take F11 pairs of leaves, fence and shadow edges at Sharpen 0.25 and 0.35, and at halo clamp 0 and 1. Owner review of the crops.
- **N-07.5 Effective Sharpen value in launcher-written files.** Method: start from a fresh game folder, press Play once in the launcher without touching the sliders, then read `[post] Sharpen` in `<game folder>\tyrender.ini`. Expected 0.350 from the code path at `src/launcher_ui.inc:182-184` and `:246`. Not run by this writer.
- **N-07.6 Cost of the grade on its own.** Method: GPU timers with `[post] Enable` on and off at fixed AA, on one build, default view. Plan item N-20 covers the timing runs.

## Discrepancies

- **Sharpen default conflicts in three places.** Code default is 0.25 (`src/fx.inl:144`, `:153`), and `README.md:306` agrees. The launcher slider default is 0.35 (`src/launcher_ui.inc:32`). The header comment in `src/fx.inl:5` also says 0.35. The shipped `default_tyrender.ini` has no `Sharpen` key, so the code default applies to a hand-edited file. The launcher writes every slider on save (`src/launcher_ui.inc:182-184`, `:246`), so a launcher-managed file ends with 0.35 (inferred from the code path; N-07.5).
- **Stale header comment.** `src/fx.inl:5` lists `Sharpen=0.35 Contrast=1.04 Saturation=1.03 Vibrance=0.10 Shoulder=0.25 Vignette=0.18`. The code defaults are 0.25, 1.0, 1.0, 0.04, 0.12 and 0.0. Code wins.
- **Dead launcher file.** `src/ui_part_a.inc:31` also sets the Sharpen slider to 0.35. That file is not included by `src/TYLauncher.c`, which includes only `launcher_ui.inc` (`src/TYLauncher.c:79`). Not relevant to the shader, but it is a second copy of the slider table (see the Ch10 launcher page for the include question).
- **First-pass `SHADERS.md` section 5.3** marks sharpen as "(inferred)". Now confirmed at `src/fx.inl:63-71`.

## See also

- [Temporal and fast anti-aliasing](taa-fxaa.md)
- [Tessellation and surface relief](tessellation-and-relief.md)
- [Settings: post, shadows, light, PBR, tessellation](../reference/settings-post-shadows-light-pbr-tess.md)
- [Presets](../reference/presets.md)
- [Overlay, input and hotkeys](../how-it-works/09-overlay-input-and-hotkeys.md)

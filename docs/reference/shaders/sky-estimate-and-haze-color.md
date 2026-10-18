# Haze colour, sky estimate, hue lock and painted sky

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), B (tester who tunes haze).

## Purpose

The haze colour decides what distant rock turns into. This page covers where that colour comes from, how a 1x1 GPU sky estimate can nudge it toward the real sky, how the hue lock keeps the fog from changing hue as the camera turns (issue #3), how far the sky sits in the fog (`SkyFogDist`, issue #58), and how painted mountains on the sky get the same aerial perspective as the world.

The fog density and the composite are on [atmosphere-fog.md](atmosphere-fog.md). The froxel path that consumes the ambient colour is on [atmosphere-froxel-compute.md](atmosphere-froxel-compute.md).

## How it works

1. **CPU haze colour (`src/atmos.inl:396-398`).** The engine's fog colour, captured from its `fogColour` uniform (`src/atmos.inl:518`; `src/tyrender.cpp:341`), is the source. If no fog colour has been captured, the default is (0.6, 0.7, 0.85) (`src/atmos.inl:397`). The colour is normalised to a luminance of 0.75 and clamped to 1.5 per channel. It is then mixed with neutral grey 0.75 by `HazeHue` (default 0.15), so most of the haze is neutral and only a share of the level fog hue remains (`src/atmos.inl:398`). The ambient colour and the haze colour both use this value (`:445`, `:478`).
2. **GPU sky estimate (`kSkyEst`, `src/atmos.inl:219-235`).** A 1x1 target averages the colour of sky pixels (depth at the far plane) sampled on a 16 by 10 grid (`:224-226`). Each sample is weighted by `exp(-5 * |elevation|) + 0.02`, so the horizon dominates, which is the colour the haze blends into (`:227-228`). The result is blended into the previous estimate with a factor of 0.08 (`src/atmos.inl:427`, `:234`). When no sky is in view, the output alpha is set to 0 and the haze falls back to the CPU colour (`:232`).
3. **Gate.** The estimate runs only when `HazeSky > 0` and the program compiled (`src/atmos.inl:416`). `HazeSky` defaults to 0 (`src/atmos.inl:238`, `:297`) and is not in the shipped ini, so by default the estimate does not run.
4. **HazeCol (`src/atmos.inl:25-29`).** When the estimate is valid (alpha at least 0.5) and `HazeSky > 0`, the haze colour is the CPU colour mixed toward the estimate, normalised the same way and scaled by `AmbientScale`, with `HazeSky` as the weight. Otherwise the CPU colour is returned unchanged.
5. **Hue lock (`src/atmos.inl:88-91`).** The in-scatter keeps its luminance and takes the hue of the haze colour, weighted by `HueLock` (default 1.0, `:252`). The comment says the sun term is warm and forward-peaked while the ambient term is cool, so their mix hue depended on yaw. The lock removes that dependence. It runs only in composite mode 3.
6. **Aerial perspective (`src/atmos.inl:113-116`).** Distant colours lose saturation toward luminance (`Desaturation`, default 0.7) and lift toward the haze colour (`HazeLift`, default 0.35), each weighted by how much light was removed.
7. **Fog saturation (`src/atmos.inl:87`).** The in-scatter is pulled toward its luminance by `FogSaturation` (default 0.5), so haze does not tint the scene.
8. **Painted sky (`src/atmos.inl:104-112`).** Sky pixels get a horizon band whose weight is `SkyHaze * (1 - smoothstep(0, SkyHazeHeight, elevation))`. Defaults are 0.8 and 0.45. The band desaturates and lifts the sky colour toward the haze colour, then adds the in-scatter with the same weight.
9. **Sky distance (`src/atmos.inl:80`, `:136`).** Sky pixels are treated as sitting `SkyFogDist * MaxDist` away. The default is 0.45, which is 18000 units at the default `MaxDist` (`src/atmos.inl:251`). The shipped ini comment says 0.12 is the old fixed value that left the red rock almost clear (`default_tyrender.ini:39-40`).

```
engine fogColour --> normalise to L=0.75 --> mix by HazeHue (0.15) --> CPU haze colour
                                                                         |
GPU sky estimate (only if HazeSky > 0) --> HazeCol mix by HazeSky ------+
                                                                         v
composite mode 3: desaturate + lift (HazeCol) ; hue lock keeps luminance, fixes hue
painted sky (depth = far): horizon band, weight SkyHaze, height SkyHazeHeight
```

## Code pointers

| What | File:line | Note |
|---|---|---|
| CPU haze colour | `src/atmos.inl:396-398` | default (0.6, 0.7, 0.85) at `:397` |
| Fog colour capture | `src/atmos.inl:518`, `src/tyrender.cpp:341` | named uniform `fogColour` |
| Sky estimate shader | `src/atmos.inl:219-235` | 16 by 10 grid, horizon weight `:228` |
| Sky estimate pass | `src/atmos.inl:415-435` | gate at `:416`, blend 0.08 at `:427` |
| HazeCol | `src/atmos.inl:25-29` | mixes by `u_hazeSky` |
| Hue lock | `src/atmos.inl:88-91` | composite mode 3 only |
| Fog saturation | `src/atmos.inl:87` | composite mode 3 only |
| Aerial perspective | `src/atmos.inl:113-116` | composite mode 3 only |
| Painted sky | `src/atmos.inl:104-112` | depth at the far plane |
| Sky distance | `src/atmos.inl:80`, `:136` | `SkyFogDist` |
| Config | `src/atmos.inl:292-297`, `:249-252` | defaults in the globals and in `F(...)` |

## Settings

Full tables are in [settings-atmos-fog-emit.md](../reference/settings-atmos-fog-emit.md). All keys below are read in `LoadAtmosConfig`, which the `reload` trigger calls, so they are live.

| Key | Default in code | Shipped Medium | Live or restart | Effect |
|---|---|---|---|---|
| `[atmos] HazeHue` | 0.15 (`src/atmos.inl:249`, `:297`) | not set | live | share of the engine fog hue in the haze colour |
| `[atmos] HazeSky` | 0.0 (`src/atmos.inl:238`, `:297`) | not set | live | how much the GPU sky estimate pulls the haze toward the real sky. 0 = estimate not run |
| `[atmos] AmbientScale` | 1.0 (`src/atmos.inl:295`) | not set | live | scales the ambient in-scatter and the sky-estimate colour |
| `[atmos] HueLock` | 1.0 (`src/atmos.inl:252`, `:297`) | not set | live | 0 = old hue behaviour, 1 = luminance kept, hue fixed |
| `[atmos] FogSaturation` | 0.5 (`src/atmos.inl:249`, `:297`) | not set | live | in-scatter saturation. 0 = grey haze, 1 = full colour |
| `[atmos] Desaturation` | 0.7 (`src/atmos.inl:249`, `:297`) | 0.3 (`default_tyrender.ini:37`) | live | far colour loss |
| `[atmos] HazeLift` | 0.35 (`src/atmos.inl:249`, `:297`) | 0.15 (`default_tyrender.ini:38`) | live | far lift toward the haze colour |
| `[atmos] SkyHaze` | 0.8 (`src/atmos.inl:249`, `:297`) | not set | live | strength of the painted-sky horizon band |
| `[atmos] SkyHazeHeight` | 0.45 (`src/atmos.inl:249`, `:297`) | not set | live | elevation at which the painted-sky band fades out |
| `[atmos] SkyFogDist` | 0.45 (`src/atmos.inl:251`, `:297`) | 0.45 (`default_tyrender.ini:40`) | live | sky depth as a fraction of `MaxDist`. 0.12 = old look |
| `[atmos] Intensity` | 2.5 (`src/atmos.inl:249`, `:297`) | 1.2 (`default_tyrender.ini:36`) | live | haze amount, shared with the density. Covered on the fog page |

## Cost

- `docs/PERFORMANCE.md`, settings table: `[atmos] HazeSky` is listed as "negligible". No separate number is recorded.
- The sky estimate is a 1x1 pass that loops over 160 grid samples, each with a depth fetch and, for sky samples, a colour fetch (`src/atmos.inl:223-230`). It is small, but it has not been timed (inferred).
- The hue lock, desaturation, lift and sky band are a few multiplies per pixel in the composite, and are part of the atmosphere pass cost on [atmosphere-fog.md](atmosphere-fog.md).

## Limits

- **One haze colour per view.** Every distant pixel uses the same colour. There is no per-region or per-height haze colour.
- **Sky estimate is coarse and lagged.** A 16 by 10 grid over the whole frame, blended at 0.08 per frame, follows changes over many frames rather than instantly (the lag is inferred from the blend factor).
- **Painted sky has no depth.** Its distance is a fixed fraction of `MaxDist` (`SkyFogDist`), so the rock "depth" does not change with the real mountain (`src/atmos.inl:80`, `:136`).
- **Hue lock and the aerial terms need the composite.** The ray-march fallback has no hue lock, no fog saturation, no desaturation or lift, and no painted-sky band (`src/atmos.inl:62-117` gating; inferred for the fallback path). With the froxel off, the #3 fix does not apply.
- **HazeSky has no effect on the ray march.** The ray march does not read `u_hazeSky` (`src/atmos.inl:133-156`).

## Known issues

- **#3, fog colour changes with camera direction.** History: `CHANGELOG.md` v0.3.5 "Fixed" says the colour no longer changes with camera direction (#3). The Unreleased section adds the hue lock, default on, and says "Untested after the latest change". `docs/PLAN_CONSOLIDATED.md:15` (playtest 2026-10-17) says the colour "still changes with camera direction". `docs/PLAN_NEXT.md:13` says "#3 is closed". Suspicions listed in `docs/PLAN_CONSOLIDATED.md:48` are the sky estimate and the GI feed into the froxel. Status: conflicting, see Discrepancies.
- **#58, sky and red rock not fogged.** `SkyFogDist` (default 0.45) and the painted-sky band were merged. `CHANGELOG.md` marks it untested. The 9:30 playtest frame shows a mesa with haze at its base: the top is about RGB (135, 84, 78) and the base about (112, 72, 58), a colour delta of 33.4 that the findings file calls indicative only (`docs/PLAYTEST_2026-10-17_FINDINGS.md:86-89`, `:144`). The findings do not confirm it is a skybox problem rather than distance fog.
- **Q183, skybox red rock.** Same as #58. The findings file's "likely pass" for it is `src/atmos.inl`, which matches the code.

## How to verify

1. **Capture check.** The first composite logs `[atmos] first run: shadows=... sun=(...) fog=(r g b)` (`src/atmos.inl:507`). The `fog=` triple is the haze colour after the HazeHue mix (`src/atmos.inl:398`), which is what the shader receives. Compare it with the level's fog colour, expecting it to be closer to neutral grey.
2. **Hue lock on and off.** Set `[atmos] HueLock=0`, reload, then turn the camera through a full yaw sweep at a distant rock. Repeat with `HueLock=1`. Expected: the haze hue stays the same with 1 and drifts with 0. The motion harness in `tools/motion.ps1` captures a yaw sweep (see [../guides/test-harness.md](../guides/test-harness.md)).
3. **Sky estimate.** Set `[atmos] HazeSky=0.6`, reload, and look at a horizon with a bright sky. The haze near the horizon should move toward the sky colour. Setting it back to 0 turns the estimate off (`src/atmos.inl:416`).
4. **Old sky depth.** Set `[atmos] SkyFogDist=0.12` to get the pre-#58 look on the painted rock.
5. **Painted sky band.** Set `[atmos] SkyHaze=0` and compare the horizon. The band should disappear and the mountain keep its red.

## NEEDS INVESTIGATION

- **NI, status of #3.** Is the yaw-dependent hue gone with `HueLock=1` on the current build? Method: the yaw sweep in How to verify 2, at the default level and at level two, plus a playtest. The owner's 2026-10-17 note (`docs/PLAN_CONSOLIDATED.md:15`) is the only record that it still changes.
- **NI, whether `HazeSky` should default to 0.6.** `README.md:294` says 0.6, the code says 0. Method: owner decision, backed by an A/B on the sky-facing views (vista and the mesa spot).
- **NI, red rock and #58 status.** Method: playtest at the 9:30 spot with `SkyFogDist` 0.12 and 0.45, with the frame logged.
- **NI, sky estimate cost.** Method: GPU timer on a `HazeSky=0.6` run, once `[gpusub]` shows the pass. Not in the repository (N-20 in [../decisions/known-issues.md](../decisions/known-issues.md)).

## Discrepancies

- `README.md:294` gives `[atmos] HazeSky` a default of 0.6 ("0 = old look"). The code default is 0.0 (`src/atmos.inl:238`, `:297`) and the shipped ini does not set it. The README is wrong for the default build.
- `README.md:292-296` lists values that differ from the code and the shipped ini:
  - `Intensity`: README "2.5 in code (presets write 2.2)". The code is 2.5 (`src/atmos.inl:297`). The compiled launcher writes 1.2 for Medium (`src/launcher_ui.inc:7`). `src/ui_part_a.inc:7` has 2.2, but the launcher does not include that file (`src/TYLauncher.c`, which includes `launcher_ui.inc`).
  - `SkyHaze` / `SkyHazeHeight`: README 0.5 / 0.25. Code 0.8 / 0.45 (`src/atmos.inl:297`).
  - `HazeHue`: README 0.25. Code 0.15 (`src/atmos.inl:297`).
  - `Desaturation` / `HazeLift`: README 0.7 / 0.35 matches the code, but the shipped ini and the launcher's Medium write 0.3 / 0.15 (`default_tyrender.ini:37-38`, `src/launcher_ui.inc:493`).
- First-pass `SHADERS.md` (section 4.3) says the sky estimate is "used as fog colour and ambient light" and that its use for `HazeSky` is "not verified". The gate and the use are both in code: it runs only when `HazeSky > 0` (`src/atmos.inl:416`), and it feeds the ambient term and `HazeCol` in the froxel path (`:169-174`, `:25-29`).
- `docs/PLAN_NEXT.md:13` says "#3 is closed". `docs/PLAN_CONSOLIDATED.md:15` says the colour still changes. `docs/TROUBLESHOOTING.md:52` says "Fixed in v0.3.5 (#3)". `CHANGELOG.md` (Unreleased) says "Untested after the latest change". Four statements, three states.

## See also

- [atmosphere-fog.md](atmosphere-fog.md): the density, the composite and the engine fog hook
- [atmosphere-froxel-compute.md](atmosphere-froxel-compute.md): the ambient colour inside the froxel
- [water-fog-and-decals.md](water-fog-and-decals.md): water boosts on top of the same fog
- [../reference/settings-atmos-fog-emit.md](../reference/settings-atmos-fog-emit.md): every `[atmos]` key with its default
- [../decisions/known-issues.md](../decisions/known-issues.md): issue table

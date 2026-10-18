# Atmosphere fog (kAtmos, haze, light shafts and engine fog hooks)

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), B (tester who tunes `tyrender.ini`).

## Purpose

The atmosphere pass adds volumetric haze and light shafts on top of the engine's own distance fog. It makes distant rock lose colour and lean toward a haze colour, and it lets the sun show through the air. It also owns the hooks that change the engine fog on draws that come after the fog pass.

This page covers the per-pixel composite, the ray-march fallback, contact shadows, the engine fog hook and the late-draw boosts. The froxel volume that the default path uses is on [atmosphere-froxel-compute.md](atmosphere-froxel-compute.md). The haze colour is on [sky-estimate-and-haze-color.md](sky-estimate-and-haze-color.md). Water handling is on [water-fog-and-decals.md](water-fog-and-decals.md).

## How it works

1. **Trigger.** The pass runs at the engine's scene-depth copy, which is the point where the engine has finished opaque geometry and grabs its depth. The order inside that hook is AO, GI, emitter light, then atmosphere (`src/tyrender.cpp:290-293`). It is skipped while the AO or GI debug view is on (`src/tyrender.cpp:293`).
2. **Gates.** `RunAtmos` returns early when `[atmos] Enable=0` and `ContactShadows` is 0, when the flag file `tyrender\atmos_off` exists, during the F11 "off" frame, or when the frame has no usable view-projection or colour copy (`src/atmos.inl:362`).
3. **Engine inputs.** The engine's own uniforms are captured by name while the main pass is bound: the two cascade shadow matrices and shadow maps, the sun direction and the fog colour (`src/tyrender.cpp:341-342`, `src/atmos.inl:510-518`). The camera position and view matrix are re-derived from the scene matrix (`src/atmos.inl:384-391`).
4. **Input copy.** The current colour buffer, with AO already applied, is copied into a private texture (`src/atmos.inl:376-381`, `:413`).
5. **Path choice.** If `[atmos] Froxel=1` and the compute path is ready, the pass uses the froxel volume and one composite (`src/atmos.inl:453`, `:454-479`). Otherwise it uses the ray march. The ray march is half resolution by default: mode 1 writes scattering and transmittance at half size, then mode 2 upsamples them depth-aware (`src/atmos.inl:481-493`, `:118-131`).
6. **Composite (mode 3, the default).** For each pixel:
   - optional far-field softening when `DepthBlur > 0` (`src/atmos.inl:62-78`);
   - a lookup into the froxel volume by screen position and distance (`src/atmos.inl:79-84`);
   - saturation control (`:87`) and the hue lock (`:88-91`);
   - water fog from the GI source mask (`:92-102`, see the water page);
   - desaturation and lift toward the haze colour, weighted by how much light was removed (`:113-115`);
   - the result `colour * transmittance + inscatter` (`:116`).
   Sky pixels (depth at the far plane) take a separate branch that adds aerial perspective to painted mountains (`:104-112`).
7. **Ray march (modes 0 and 1).** Each pixel marches from the camera toward the scene for `Steps` samples (default 24), up to `MaxDist`. Sky pixels stop at `SkyFogDist * MaxDist` (`src/atmos.inl:133-136`). Each sample takes density from an exponential height falloff, sun visibility from the cascade maps, and a Henyey-Greenstein phase term with `Anisotropy` as g (`:138`, `:146`, `:149`). Slices are integrated with the energy-conserving form that the code comment attributes to Hillaire (2015) (`:150`). The start offset is jittered per pixel (`:141`), and `ContactStable=1` removes the per-frame part of the jitter (`:448`).
8. **Sun visibility.** `SunVis` projects a point into cascade 1, then cascade 2, and reads the shadow map when the point is inside the map. The result is `1 - lookup`, following the engine's convention that a lookup of 1 means shadowed (`src/atmos.inl:32-39`). The fragment path uses an explicit LOD of 0 (`:35`).
9. **Density.** The uniform is `Density * Intensity * FogAmount * fog.Amount / max(fog.Distance, 0.05)` (`src/atmos.inl:446`, `:465`). With the code defaults and `[fog]` Distance and Amount at 1.0, that is 0.000015 * 2.5 * 0.8 = 0.00003 per unit (inferred from the defaults). The height term is `exp(-(y - camera.y) * HeightFalloff)` with the height difference clamped at -2000 (`src/atmos.inl:146`). Points 2000 units below the camera therefore get about 20 times the density of camera height (inferred from the formula).
10. **Contact shadows.** For pixels that the shadow map calls lit, the pass marches 12 samples toward the sun for up to `ContactLength` units. A sample counts as an occluder when the depth difference is between 0.5 and `ContactThickness` units, which keeps thin objects from making long streaks (`src/atmos.inl:44-61`). The darkening fades out between camera distances 2000 and 6000 (`:59`). It runs in modes 0, 2 and 3 (`:45`).
11. **Engine fog hook.** The engine writes its fog coordinates through `glUniform4fv` and `glUniform4f` with the uniform named `fogCoords`. `FogOverride` scales the fog range by `[fog] Distance` and the strength by `[fog] Amount`, and clamps the strength so the total stays at or below 1 (`src/tyrender.cpp:404-408`, `:413`, `:422`).
12. **Late draws.** A draw that starts after the fog pass has run this frame can be see-through (transparent-tagged) or emissive. For those draws the engine fog strength is multiplied by one of `[fog] LateBoost`, `WaterBoost`, `FoliageBoost` or `LateEmitBoost`, and the original value is restored after the draw (`src/atmos.inl:329-345`, `src/tyrender.cpp:267-269`). All four default to 1.0, which means off. The code comment says that a LateBoost of 2.0 "fogged everything" (`src/atmos.inl:326`).
13. **See-through depth.** With `[fog] SeeThruDepth=1`, see-through draws that come before the depth copy get depth writes on. The fog pass then reads the surface behind a leaf card or water instead of the card itself (`src/atmos.inl:346-356`).
14. **Diagnostics.** Once in 60 frames `FogDiagDraw` counts transparent-tagged draws by depth-write and blend state, and logs the totals as `[fogdiag]` every 600 frames (`src/atmos.inl:318-324`, `:359-360`).

```
depth copy (src/tyrender.cpp:284-293)
  RunAO -> RunGI -> RunEmit -> RunAtmos
                                 |-- contact shadows (modes 0, 2, 3)
                                 |-- froxel: kAtmosCS -> volume -> composite mode 3
                                 '-- ray march: mode 1 (half res) -> mode 2 upsample
later draws (src/tyrender.cpp:267-271)
  LateFogBegin -> draw -> LateFogEnd ; SeeThruDepthBegin -> draw -> SeeThruDepthEnd
```

## Code pointers

| What | File:line | Note |
|---|---|---|
| Fragment shader `kAtmos` | `src/atmos.inl:14-158` | modes 0 to 3 selected by `u_mode` |
| Contact shadow march | `src/atmos.inl:44-61` | 12 samples, thin-depth-window test |
| Composite mode 3 | `src/atmos.inl:62-117` | softening, lookup, hue lock, water, aerial perspective, painted sky |
| Mode 2 upsample | `src/atmos.inl:118-132` | depth-aware 2x2 |
| Ray march | `src/atmos.inl:133-156` | density falloff, phase, slice integration |
| Sun visibility | `src/atmos.inl:32-39` | two cascades, explicit LOD 0 |
| Captured engine state | `src/atmos.inl:9-11`, `:510-518` | shadow matrices, sun, fog colour |
| Pass driver `RunAtmos` | `src/atmos.inl:358-508` | gates at `:362`, paths at `:453` and `:481` |
| Config loader | `src/atmos.inl:290-308` | `LoadAtmosConfig`, also run on reload |
| Flag poll | `src/atmos.inl:309-313` | `tyrender\atmos_off`, `tyrender\atmos_debug` |
| Late boost | `src/atmos.inl:329-345` | `LateFogBegin` / `LateFogEnd` |
| See-through depth | `src/atmos.inl:346-357` | `SeeThruDepthBegin` / `End` |
| Engine fog override | `src/tyrender.cpp:404-408` | `FogOverride` |
| Fog uniform hooks | `src/tyrender.cpp:409-425` | `glUniform4fv`, `glUniform4f` |
| Draw hooks | `src/tyrender.cpp:267-271` | late fog and see-through around each draw |
| Depth-copy call order | `src/tyrender.cpp:284-293` | AO, GI, emit, atmos |
| Fog globals | `src/tyrender.cpp:91-92` | `g_lateFog`, `g_fogDistance`, `g_fogAmount` |

## Settings

Full tables are in [settings-atmos-fog-emit.md](../reference/settings-atmos-fog-emit.md). Only the keys that matter here are listed. "Shipped Medium" is the value in `src/default_tyrender.ini`. "Live" means the key is re-read by the `reload` trigger (`src/tyrender.cpp:598`).

| Key | Default in code | Shipped Medium | Live or restart | Effect |
|---|---|---|---|---|
| `[atmos] Enable` | 1 (`src/atmos.inl:292`) | 1 (`default_tyrender.ini:32`) | live | master switch for the volumetric fog |
| `[atmos] Steps` | 24 (`src/atmos.inl:292`) | 24 (`:34`) | live | ray-march sample count. No effect on the froxel path (see the froxel page) |
| `[atmos] ContactShadows` | 0.35 (`src/atmos.inl:296`) | 0 (`:35`) | live | strength of the screen-space shadow toward the sun |
| `[atmos] ContactLength` | 25 (`src/atmos.inl:296`) | not set | live | march length of contact shadows, world units |
| `[atmos] ContactThickness` | 6 (`src/atmos.inl:296`) | not set | live | depth window that counts as an occluder |
| `[atmos] Intensity` | 2.5 (`src/atmos.inl:297`) | 1.2 (`:36`) | live | multiplies the density. The value is shared with the haze tuning |
| `[atmos] Density` | 0.000015 (`src/atmos.inl:294`) | not set | live | base density per unit |
| `[atmos] HeightFalloff` | 0.0015 (`src/atmos.inl:294`) | not set | live | how fast density falls with height above the camera |
| `[atmos] MaxDist` | 40000 (`src/atmos.inl:294`) | not set | live | far end of the fog, world units |
| `[atmos] SunIntensity` | 1.4 (`src/atmos.inl:295`) | not set | live | light-shaft brightness, multiplies `SunColor` |
| `[atmos] Anisotropy` | 0.6 (`src/atmos.inl:295`) | not set | live | Henyey-Greenstein g. High values make the glow toward the sun stronger |
| `[atmos] FogAmount` | 0.8 (`src/atmos.inl:296`) | not set | live | scales the volumetric density. Not the same key as `[fog] Amount` |
| `[atmos] ShaftContrast` | 0.5 (`src/atmos.inl:296`) | not set | live | how much sun visibility shows in the shafts (0 = none, 1 = full) |
| `[atmos] HalfRes` | 1 (`src/atmos.inl:292`) | not set | live | ray-march path only: march at half resolution |
| `[atmos] DepthBlur` | 0 (`src/atmos.inl:297`) | not set | live | far-field softening. Composite mode 3 only |
| `[fog] Distance` | 1.0 (`src/tyrender.cpp:601`) | not set | live | scales the engine fog range (`FogOverride`) |
| `[fog] Amount` | 1.0 (`src/tyrender.cpp:602`) | not set | live | scales the engine fog strength and the volumetric density |
| `[fog] LateBoost` | 1.0 (`src/tyrender.cpp:602`) | not set | live | engine fog multiplier for late draws, when the other late boosts are 1 |
| `[fog] SeeThruDepth` | 0 (`src/atmos.inl:299`) | 0 (`default_tyrender.ini:77`) | live | write depth for see-through draws drawn before the fog pass |
| `[fog] LateEmitters` | 0 (`src/atmos.inl:299`) | 0 (`:78`) | live | let LateBoost also cover emissive draws |
| `[fog] LateEmitBoost` | 1.0 (`src/atmos.inl:301`) | 1.0 (`:83`) | live | fog multiplier for emissive late draws (fire) |
| `[fog] FoliageBoost` | 1.0 (`src/atmos.inl:302`) | 1.0 (`:85`) | live | fog multiplier for non-water see-through late draws |
| `[debug] ForceFallback` bit 1 | 0 (`src/ao.inl:491`) | not set | restart | forces the froxel setup to fail. Read only once, see How to verify |

## Cost

- **Measured, full pass (in-session A/B, default view, stock frame 5.38 ms).** `docs/results/VERDICTS.md` (section "Standard-view numbers"): froxel fog plus contact shadows +3.69 ms; fog alone +2.12 ms; contact shadows alone +2.74 ms. The file does not name the GPU. The same file's correction note gives the per-pass numbers as 0.82 ms (fog alone), 1.21 ms (contact alone) and 1.69 ms (both).
- **Measured, switch off.** `docs/results/cost_table.md` (Balanced C1, merged build): `atmos_off` is 2.09 ms below C1 at 11.09 ms. `contact035` is +1.71 ms.
- **Measured, Medium preset.** `docs/results/preset_matrix.md`: Medium atmosphere pass 1.01 ms of a 10.87 ms frame. The GPU is not named in the file.
- **Repository summary (v0.3.2, RX 7600 XT, 2560x1440).** `docs/PERFORMANCE.md` gives about 2.1 ms for fog and light shafts off and about 2 ms for `[atmos] Enable` at Medium, and about 1.7 ms for contact shadows at 0.35.
- Per-pass numbers are "not measured" in the repository for the ray-march path on its own. The VERDICTS numbers compare the two paths, but the early table in that file was sampled at the last view and was later corrected (see [atmosphere-froxel-compute.md](atmosphere-froxel-compute.md)).
- Cost grows with screen size (one composite pass) and, for the ray march, with `Steps` and `HalfRes`. Contact shadows add a 12-sample march per lit pixel.

## Limits

- **Only what the depth copy shows.** The pass runs once, at the depth copy. Anything drawn later is not in the depth it sees, which is why late draws need their own boosts (`src/atmos.inl:325-327`).
- **Two shadow cascades.** Sun visibility uses only the two cascades captured from the main pass (`src/atmos.inl:35-37`). Any other shadow map is ignored (inferred).
- **One density for the whole view.** There is no per-region or per-material density. Material effects come from the boosts and the water mask, not from the density volume.
- **Engine fog and volumetric fog are separate.** The engine fog keeps running. The two can disagree on surfaces that the depth copy sees differently (inferred from the design; see the fog-consistency issues below).
- **Ray-march fallback is a different look.** Several features only exist in the composite (mode 3): the hue lock, saturation control, desaturation and lift, the painted-sky branch, water fog and `DepthBlur` (`src/atmos.inl:62-117`). The fallback has none of them (inferred from the gating; not run).
- **Contact shadows only where the shadow map is lit.** The contact pass skips pixels that the shadow map already calls shadowed (`src/atmos.inl:46-47`).

## Known issues

- **#3, fog colour changes with camera yaw.** The fix is the hue lock (`src/atmos.inl:88-91`), default 1.0 (`:252`). Status conflicts between documents: `CHANGELOG.md` (Unreleased) says "Untested after the latest change"; `docs/PLAN_CONSOLIDATED.md:15` (playtest 2026-10-17) says the colour "still changes with camera direction"; `docs/PLAN_NEXT.md:13` says "#3 is closed". The hue lock is only in composite mode 3, so the ray-march fallback still has the old behaviour (inferred). See the sky page.
- **#4, fog inconsistent on trees, water, lilies and fire.** Attempts: `SeeThruDepth` (default 0, untested), `FoliageBoost`, `WaterBoost` (see the water page), `LateEmitBoost` (default off, untested). `README.md:213-214` lists the open items. The 2026-10-17 playtest did not confirm that trees poke through fog. The one tree check it made (5:40) is a dialogue cutscene, not an outdoor view (`docs/PLAYTEST_2026-10-17_FINDINGS.md:25`, `:77-78`, `:125-140`).
- **Q185, trees through fog on level two.** `docs/FIX_PLAN_20261016.md` (T92b) proposed forcing depth writes for alpha above 0.50. The code does not do that. `SeeThruDepth` writes depth for every non-emissive see-through draw (`src/atmos.inl:346-352`). The two are different designs; the code is the one that exists.
- **Fire is not fogged (#4).** `LateEmitBoost` defaults to 1.0 (off). `default_tyrender.ini:82` suggests 2.0 to 3.0 in playtest. Untested.
- **Sky and red rock (#58).** `SkyFogDist` (default 0.45, `src/atmos.inl:251`) fogs the painted sky. CHANGELOG marks it untested. See the sky page.
- **Q176, fog inconsistencies overall.** #3, #4, #58, #74 and #106 are all listed in CHANGELOG under Unreleased. #74 and #106 are water items (see the water page).

## How to verify

1. **Fog off.** Set `[atmos] Enable=0` and `ContactShadows=0` in `tyrender.ini`, then create the `reload` trigger file in `tyrender\` (`src/tyrender.cpp:597`). With both off, `RunAtmos` returns at `src/atmos.inl:362` and no new `[atmos] first run` line is written (that line is written once, at `src/atmos.inl:507`, the first time the pass draws). Expected look: distant rock keeps full colour.
2. **Inscatter only.** Write `1` into `tyrender\atmos_debug`. The flag is polled every 8 frames (`src/tyrender.cpp:595`, `src/atmos.inl:312`). Expected: a haze-only image (`src/atmos.inl:85`, `:154`).
3. **Sun visibility.** Write `2` into `tyrender\atmos_debug`. Expected: a black and white sun-visibility image that matches the engine's shadows on surfaces (`src/atmos.inl:86`, `:155`).
4. **Capture check.** Look for `[atmos] first run: shadows=1 sun=(...) fog=(...)` in `tyrender.log` (`src/atmos.inl:507`). `shadows=0` means the two cascade matrices or maps were not captured. The sun vector should point away from the camera toward the sun.
5. **Late boost.** Set `[fog] LateBoost=2.0` and reload. Water, foliage and fire that come after the fog pass should look more hazed. Read `[fogdiag]` every 600 frames to count the late draws (`src/atmos.inl:359`).
6. **Forced failure.** Set `[debug] ForceFallback=1` and restart. The log should show `[fallback] FORCED froxel failure -> ray-march path` (`src/atmos.inl:257`). Restart is needed because `InitVolume` runs once (`src/atmos.inl:255`).

## NEEDS INVESTIGATION

- **NI, fire and emissive late draws (#4).** Does `LateEmitBoost` of 2 to 3 haze the fire without washing out glow textures? Method: playtest at the cave fire spot with `[fog] LateEmitBoost` at 1, 2 and 3, one change per run.
- **NI, trees through fog (#4, T92b).** Does `SeeThruDepth=1` fix the level-two tree cards? Method: an outdoor level-two spot (the 5:35 frame in `docs/PLAYTEST_2026-10-17_FINDINGS.md` is the nearest), A/B with `SeeThruDepth` 0 and 1.
- **NI, late boosts in the F11 off frame.** `RunAtmos` sets the frame marker before its off-frame return (`src/atmos.inl:361-362`), so `LateFogBegin` may still boost in the off frame (inferred). Method: set `[fog] LateBoost=2`, take an F11 pair, and check whether the "off" frame is hazed.
- **NI, engine fog in the off frame.** `FogOverride` applies in every frame (`src/tyrender.cpp:404-408`). Method: change `[fog] Amount` and compare the off frame with the vanilla game.

## Discrepancies

- `src/atmos.inl:5-6` (header) gives `Density=0.00006` and `AmbientScale=0.35`. The code defaults are 0.000015 (`src/atmos.inl:288`, `:294`) and 1.0 (`:295`). Code wins.
- `src/atmos.inl:2-4` (header) says the pass marches from the camera to the scene depth. The default path is the froxel lookup (`src/atmos.inl:62`, `:454`). The march is the fallback.
- The first-pass `SHADERS.md` (section 4.1) describes modes 0 to 3 as "haze, height fog, painted-sky aerial perspective, and a desaturation mode". The code has: 0 full-resolution march, 1 half-resolution scattering, 2 upsample, 3 froxel lookup with composite (`src/atmos.inl:62`, `:118`, `:133`, `:153`). Code wins.
- `[atmos] ContactShadows`: `README.md:297` lists the default as 0. The code default is 0.35 (`src/atmos.inl:296`). The shipped ini and the launcher slider are both 0 (`default_tyrender.ini:35`, `src/launcher_ui.inc:30`). The README matches what a player gets, not the code default.
- `[atmos] Intensity` has four values in the repository: 2.5 in code (`src/atmos.inl:297`), 1.2 in the shipped ini (`default_tyrender.ini:36`) and in the compiled launcher's Medium preset (`src/launcher_ui.inc:7`), and 2.2 in `src/ui_part_a.inc:7`, which the launcher does not include. `README.md:292` says presets write 2.2. The compiled launcher writes 1.2 for Medium. See [settings-atmos-fog-emit.md](../reference/settings-atmos-fog-emit.md).
- `CHANGELOG.md` (Unreleased, Fixed, #96) says the F11 "off" frame skips "AO, GI, atmosphere, FX, shadow strength, tess LOD, light amounts and emit". The volumetric pass is skipped (`src/atmos.inl:362`). The late boosts (`src/atmos.inl:330`) and the engine fog hook (`src/tyrender.cpp:404-408`) are not skipped by that check (inferred; see NEEDS INVESTIGATION).
- `docs/FIX_PLAN_20261016.md` T92b describes an alpha-threshold depth prepass. The code implements `SeeThruDepth` instead (`src/atmos.inl:346-352`).
- `docs/PERFORMANCE.md` and `README.md:291` describe `Steps` as the fog quality for Medium and Ultra ("fog 24 steps", "quality"). On the default froxel path `Steps` has no effect: `kAtmosCS` has no step uniform (`src/atmos.inl:160-217`). See the froxel page.

## See also

- [atmosphere-froxel-compute.md](atmosphere-froxel-compute.md): the default froxel volume and its fallback
- [sky-estimate-and-haze-color.md](sky-estimate-and-haze-color.md): haze colour, hue lock, painted sky
- [water-fog-and-decals.md](water-fog-and-decals.md): water boosts, water mask, decals
- [distance-lod.md](distance-lod.md): the `[lod] Fog` ramp
- [../reference/settings-atmos-fog-emit.md](../reference/settings-atmos-fog-emit.md): every `[atmos]`, `[fog]` key
- [../how-it-works/04-frame-anatomy-and-pass-order.md](../how-it-works/04-frame-anatomy-and-pass-order.md): where the pass sits in the frame

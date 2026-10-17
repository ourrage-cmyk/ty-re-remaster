# TY HD Renderer (tyrender)

> **G'day, mate!** Ty's Australian outback was painted by hand, and it deserves a light that does it justice. TY HD Renderer gives the outback a bit more depth: shadows that actually shade the ground, haze that rolls over the far rocks, and sand and fences with real texture when you get up close.
>
> Ty still looks like Ty, and the game plays exactly the same. You just see more of the bush around you.
>
> Nothing in the game files gets changed. Pick a preset, press Play, and go and find a dunny.

A modern rendering layer for **TY the Tasmanian Tiger** on PC.

**Status: preview / community beta, version 0.3.2.** Tested on one graphics card only: AMD Radeon RX 7600 XT on Windows 11. NVIDIA, Intel and Linux/Proton are not tested yet. Reports are very welcome.

## Why this exists

TY the Tasmanian Tiger has bold, flat, colourful art, and that art deserves to be shown at its best. The renderer it shipped with is from the mid-2000s, though, and it holds the look back in a few specific ways:

- **Shadows are capped and hard.** Only the first shadow cascade is softened. The others are single hard taps. The shadow map is fixed at 2048, and a shadow can never be darker than 50%, so a bright day looks washed out instead of properly shaded.
- **Fog is a flat colour blend.** Distant rocks and hills fade into a solid colour, with no light shafts through the haze.
- **Surfaces are flat.** The game has no relief beyond what is painted on the textures, so sand, fences and rope look flat up close, especially at grazing angles.
- **Light never bounces.** A coloured rock next to sand does not tint the sand.
- **Few settings.** The game's own options are limited, with no way to tune shadow softness, haze or relief.

This mod fixes those problems at the source, while keeping the game's look:

- **Relief from the texture itself.** Cracks, grain, wood and rope get bumps and roughness from their own colour, so the ground has real profile. Feet stay on the ground, and characters are not changed.
- **Contact shadows.** Ambient occlusion darkens crevices and corners where light cannot reach. It fades with distance and stays steady from frame to frame.
- **Better shadows.** Adjustable shadow map size, a smooth filter with no grainy noise, and a cooler shade colour, so shadows read as shade instead of grey smudges.
- **Atmosphere.** Volumetric haze with light shafts. Distant rocks and the horizon lose saturation, and the haze follows the real sky colour.
- **Bounce light (High and Ultra).** Surfaces pick up a little colour from their neighbours.
- **Smoother image.** Temporal anti-aliasing reduces shimmer on moving objects, and sharpening and grading keep it clean.
- **A launcher with sliders.** Every effect has a slider, a short description and a rough cost tag, and your settings are saved.

## Why you should try it

- **Nothing in the game is changed.** The original `TY.exe` is never modified. To remove the mod, delete its files.
- **You see the cost before you pay it.** The launcher shows what each option costs in frame time, so you can choose the look that suits your PC.
- **Medium is the default.** It adds ambient occlusion, haze, relief, better shadows and anti-aliasing, and it runs at about 80 fps at 2560x1440 on the test card.
- **Made to look like TY.** The aim is for TY to look like TY, only with shadows that behave, surfaces with texture and atmosphere that reads as atmosphere. Most effects are easiest to see up close. Haze and shadow shading are the most obvious changes from a distance.

This is a hobby project, made with Claude for the community. It is for people who remember playing TY as kids, and for new players who are discovering it.

## What it does

| Feature | What it does | Settings |
|---|---|---|
| Ambient occlusion (AO) | Soft shadows in corners and crevices. | `[ao]` |
| Better shadows | Smoother shadow edges with no per-pixel noise. Shadow maps are 4096 (8192 in Ultra). Cooler shadow colour. | `[shadows]` |
| Volumetric haze | Dusty haze with light shafts. Far rocks and the horizon look hazier and less saturated. The haze follows the real sky colour. | `[atmos]` |
| Anti-aliasing | Smoother edges, using TAA or FXAA. | `[post] AA` |
| Sharpen and grade | Gentle sharpening, a soft filmic curve and vibrance for colour. | `[post]` |
| Surface relief | Cracks, grain, wood and rope get relief and roughness from their own texture colour. Feet do not sink into the ground. Characters are not changed. | `[pbr]`, `[tess]` |
| Bounce light (High and Ultra) | Light bounces between nearby surfaces. | `[gi]` |
| Natural light | Cool, sky-tinted shade, sky and ground bounce, and translucent grass. | `[light]` |

## What is new in v0.3.2

- Less flicker. The AO and shadow filters were steadied, and shadow edges no longer strobe.
- Haze takes its colour from the real sky, so far trees and rocks follow it.
- AO fades with fog, so it no longer fights the fog in the distance.
- The launcher has a simple view with four presets (Low, Medium, High, Ultra), an overall performance impact and the Play button. **More options >>** shows every effect, the sliders and a description for each.

## Presets

Frame times are GPU milliseconds per frame. They are author-measured at 2560x1440, windowed, on one AMD Radeon RX 7600 XT. Other GPUs will be faster or slower. Lower is faster.

| Preset | Frame time | About fps | What it adds |
|---|---|---|---|
| Stock game (no mod) | 5.7 ms | 175 | The original game, for comparison |
| Low | about 6.0 ms | 166 | FXAA smoothing and 2048 shadow maps. No soft corner shadows, no haze |
| **Medium** (recommended, default) | about 12 ms | 80 | Soft corner shadows, haze, surface relief, 4096 shadow maps, TAA smoothing. No bounce light |
| High | about 18 ms | 55 | Medium plus bounce light (1 ray) and contact shadows |
| Ultra | about 31 ms | 32 | More AO samples, bounce light (2 rays), higher-quality haze, environment relief, 8192 shadow maps |

### Rough cost of each option vs Medium, one GPU, author-measured

| Option | Change | Rough frame cost |
|---|---|---|
| Ambient occlusion (AO) | Off | saves about 1.15 ms |
| Volumetric haze | Off | saves about 2.1 ms |
| Contact shadows | On | about +1.7 ms |
| Bounce light | On, 1 ray | about +4.4 ms |
| Shadow maps | 8192, 32 taps | about +5.1 ms |
| Environment relief | `[tess] Scope=env` | about +6.2 ms |

These numbers are approximate. Effects interact, so the costs do not simply add up. They come from the v0.3 measurements in `docs/PERFORMANCE.md`. The v0.3.2 changes have not been timed one by one. Other GPUs will differ.

## Install

1. Download `TYLauncher.exe` from the release.
2. Put it in the game folder, next to `TY.exe`.
3. Run `TYLauncher.exe`, pick a preset (Medium is the recommended one) and press **Play with HD renderer**.
   The first start installs the renderer files next to it. The first Play creates `TY_tyrender.exe`, a copy of `TY.exe` that loads `tyrender.dll`. `TY.exe` itself is never changed.
4. Keep the launcher open to move the sliders. Most of them apply live.
5. Optional: **Steam setup** copies a launch option. Paste it into Steam > TY the Tasmanian Tiger > Properties > General > Launch options. Steam's Play button then starts the mod.

The single-file launcher applies from v0.3.5-rc.

**Play original (no mod)** starts the untouched game. **Advanced (ini)...** opens `tyrender.ini`.

**Memory:** `TY_tyrender.exe` is marked Large Address Aware (4 GB instead of 2 GB). Without this, the 32-bit game can run out of memory with the HD texture pack (log line: `CMKHeap - Out of memory`).

## Uninstall

Delete `tyrender.dll`, `TYLauncher.exe`, `TY_tyrender.exe`, `tyrender.ini`, `tyrender.log`, and the `tyrender` folder if it is there. If you added the Steam launch option, remove it in Steam. The original game keeps working without these files.

## Known issues

These are open issues from playtests. They are not fixed yet.

- **Water:** when Ty is in water, his brightness pulses, because the water reflection reacts to him.
- **Character shadows:** a thin bright line can run through the shadow edge on characters (face, bag), and the shadow looks slightly grid-like.
- **Dark halo:** a dark halo can appear behind the character, on the side away from the camera.
- **White rim:** fern shadows on the ground have a white rim.
- **Glowing footprints:** some footprints can glow.
- **Fire and light:** fire is not yet hazed by the fog, and the portal light does not reach the ground.
- **Flicker on High and Ultra:** Medium is the stable preset in playtests. High and Ultra flickered in playtest 4. If you see flicker, use Medium.
- **Ultra is slow:** about 31 ms per frame on the test card.
- **Thin objects:** rope and grass get little benefit from relief, because their source textures are flat.
- **Trails:** TAA can leave a faint trail on fast animation.
- **Low-resolution textures:** relief comes from the texture's own colour, so it is weaker on low-resolution textures.
- **Shimmer:** shimmer during fast camera moves has not been measured formally.
- **Game version:** built against the 2020-04-04 `TY.exe`. Other builds are untested.

## Reporting problems

Send your `tyrender.log` and a screenshot, with your GPU model and driver version. NVIDIA and Intel owners: a `tyrender.log` from one run is especially welcome, since those GPUs are untested.

## Compatibility and credits

- Works with the base game.
- Gets more from the ty2k mod. More texture detail means more precise relief.
- The HD texture pack is separate from this project.
- Made with Claude as a hobby project for the community. Thank you to everyone who tested and sent reports.

## For modders

Presets cover most needs, and the launcher writes the values it needs. To tune by hand, edit `tyrender.ini` in the game folder, or open it from **Advanced (ini)...** in the launcher. Costs per option are in `docs/PERFORMANCE.md`.

### Fallback

If your graphics card or driver cannot run an effect (for example no GL 4.3 compute for the fog, or a shader that does not compile), **that effect switches itself off** and the frame is drawn without it. The renderer does not fall back to a slower path. Check `tyrender.log` for the reason.

<details>
<summary>Settings reference (tyrender.ini)</summary>

Live means the change applies while the game runs (through the launcher or the `tyrender\reload` file). Restart means the game must be restarted.

| Section | Key | Default in code | Meaning |
|---|---|---|---|
| `[ao]` | Enable | 1 | AO on or off |
| `[ao]` | Slices / Steps | 3 / 6 | AO quality (Ultra uses 4 / 8) |
| `[ao]` | Radius / Strength / Power | 60 / 0.85 / 1.0 | Crevice AO look |
| `[ao]` | WideRadius / WideStrength / WideSlices | 400 / 1.0 / 0 (= Slices) | Corner darkening over a larger area |
| `[ao]` | Thin / MultiBounce | 0.5 / 0.6 | Thin objects (grass, fence) and bounce darkening |
| `[ao]` | Temporal / TemporalMode / TemporalBlend | 1 / 1 / 0.85 | Temporal smoothing (mode 1 = bilinear history with variance clamp; 0 = the v0.3.1 method) |
| `[ao]` | NoiseFrames / BlurRadius / BlurFalloff | 64 / 2 / 0.15 | Noise pattern and denoise filter |
| `[ao]` | FogFade | 1.0 | AO fades with fog (0 = AO ignores fog, the old behaviour) |
| `[ao]` | Legacy | 0 | 1 = the older, slower AO (not recommended) |
| `[ao]` | DebugView | 0 | 1 = show AO only (test aid) |
| `[gi]` | Enable | 1 in code, 0 in the shipped ini and in Low/Medium | Bounce light on or off |
| `[gi]` | Mode / Rays / Steps | 1 / 4 / 8 | Method 1 = Hi-Z (recommended); presets use 1 ray (High) or 2 rays (Ultra) |
| `[gi]` | Strength / Radius / Thickness | 0.7 / 400 / 120 | Look |
| `[gi]` | Characters | 1 | Characters take part in bounce light |
| `[gi]` | CharacterColour | 0.0 | How much of a character's colour is passed on (0 = none) |
| `[gi]` | CharacterGI | 0.3 | How much light the character receives from the scene |
| `[gi]` | CharacterHeight / CharacterRadius | 160 / 70 | Size of the mask around a character |
| `[atmos]` | Enable / Froxel / Steps | 1 / 1 / 24 | Volumetric fog on or off, compute method, quality |
| `[atmos]` | Intensity | 2.5 in code (presets write 2.2) | Overall haze amount (live) |
| `[atmos]` | Desaturation / HazeLift | 0.7 / 0.35 | Far rocks lose colour and get lighter |
| `[atmos]` | HazeSky | 0.6 | Haze colour follows the real sky (0 = old look) |
| `[atmos]` | SkyHaze / SkyHazeHeight | 0.5 / 0.25 | Haze on the sky and horizon |
| `[atmos]` | HazeHue | 0.25 | How much haze takes the sky's hue |
| `[atmos]` | ShaftContrast / ContactShadows / ContactLength | 0.5 / 0 / 25 | Light shafts and contact shadows toward the sun (ContactShadows > 0 costs time) |
| `[atmos]` | DepthBlur / BlurStart / BlurRange | 0 / 4000 / 12000 | Optional blur of distant scenery (try 2 to 3; off by default) |
| `[shadows]` | Patch | 1 | Use the new shadow filter |
| `[shadows]` | MapScale | 2 | 1 = 2048, 2 = 4096, 4 = 8192 (restart) |
| `[shadows]` | Taps | 16 | Filter taps (restart). With `Grid=1` the grid is 3x3 (Medium) or 4x4 (Ultra) |
| `[shadows]` | Grid | 1 | Fixed filter grid, no per-pixel noise (recommended) |
| `[shadows]` | Temporal | 0 | Shadow pattern changes per frame (not recommended) |
| `[shadows]` | Radius / Strength / GrassStrength | 1.4 / 0.5 / 0.35 (launcher) | Softness, shadow darkness, grass shadow strength |
| `[post]` | Enable / AA | 1 / 2 | AA: 0 = none, 1 = FXAA, 2 = TAA, 3 = TAA + FXAA |
| `[post]` | Sharpen / SharpenHaloClamp | 0.25 / 1 | Sharpening amount; clamp stops dark and bright halos |
| `[post]` | Contrast / Saturation / Vibrance | 1.0 / 1.0 / 0.04 | Grade |
| `[post]` | Shoulder / Vignette / Gamma | 0.12 / 0 / 1.0 | Grade |
| `[tess]` | Enable | 1 | Relief on or off |
| `[tess]` | Scope | env-detail | `env-detail` = relief on the ground and props (default); `env` or `all` = also tessellate the environment (about +4 to +6 ms, Ultra only); `off` = nothing |
| `[tess]` | Alpha / TargetPx / MaxLevel | 0.7 / 12 / 8 | Tessellation detail (restart) |
| `[tess]` | ReliefSteps / ReliefFeature / ReliefDepth | 8 / 1.0 / 3 | Relief quality and depth. ReliefDepth is the launcher's "Surface relief" slider |
| `[tess]` | ReliefCurve / ReliefKnee / ReliefKneeSlope | 1.0 / 0 / 0.35 | Relief shaping. Curve below 1 lifts weak relief (the floor rises). Knee (world units, 0 = off) makes relief past it grow at KneeSlope of its rate, so strong textures stop growing when ReliefDepth goes up |
| `[pbr]` | Enable | 1 | Bump and specular from the texture (restart) |
| `[pbr]` | DetailStrength / DetailScale / DetailDisp | 1.0 / 1.0 / 0.8 | Relief strength, scale, inward depth |
| `[pbr]` | LitProps | 1 | 0 = props keep the stock look |
| `[light]` | Enable / ShadowTint / Hemisphere / Foliage | 1 / 1 / 1 / 1 | Natural light; each `*Amount` key scales its part |
| `[debug]` | ForceFallback | 0 | Test aid: bits 1 fog, 2 GI, 4 AO, 8 relief, 16 light shaders are forced to fail |
| `[debug]` | StateGuard | 0 | Test aid: logs graphics-state changes |

The shipped `tyrender.ini` is the Medium preset. The launcher writes the values it needs, so you normally do not need to edit the file by hand.

</details>

# TY HD Renderer (tyrender) - v0.3.2 preview / community beta

Modern rendering for **TY the Tasmanian Tiger** (PC, Steam build r4704_v1.44, 32-bit OpenGL). Drop-in: the game files are not modified.

**Status: preview / community beta.** Tested on one graphics card only (AMD Radeon RX 7600 XT, Windows 11). Other GPUs, NVIDIA, Intel and Linux/Proton are not tested yet. Reports with your `tyrender.log` are very welcome.

## Test build v0.3.3 (not released yet)
A test build is available for testers who want to check the open problems. It is not the recommended version: use v0.3.2 for normal play.

What changed in v0.3.3:
- **Textures are recognised by name.** A list of about 200 textures tells the renderer which ones are characters (no bumps, no relief, no shiny highlights on them), which ones glow (for example light sources) and which ones are see-through (water, glass, particles). Everything else gets the ground and prop relief.
- **Bounce light ignores water, glow and see-through things.** Water no longer makes Ty flicker blue, and the portal glow no longer lights the ground. Setting `[gi] ExcludeSources=2` keeps the portal glow as a light source again.
- **Bounce light is calmer.** It is blurred a little more and changes more slowly over time (`[gi] SourceBlur`, `NoiseCycle`, `TemporalBlend`).
- **High costs about 2.5 ms more** than in v0.3.2 (about 19.8 ms at 2560x1440 on an RX 7600 XT, one run). Medium is unchanged at about 12 ms.
- **Shadow edge tests:** a `[debug]` section of light settings (`ShadowView`, `PlaneBias`, `PlaneClamp`, `SlopeBias`, `SkinBias`) lets us look at shadow edges. They are all off by default.

Please test High and Ultra and tell us whether they still flicker. Still open in this test build: the jagged bright lines on the edges of Ty's face and bag shadows, the water that still pulses Ty's brightness, footprints that may still glow, the dark halo behind Ty, fire that is not hazed, the portal light that does not reach the ground, and Ultra being slow.

## What it does
| Feature | What it does | Settings |
|---|---|---|
| Ambient occlusion (AO) | Darkens crevices, corners, the base of rocks and grass. Temporal, so it does not shimmer. | `[ao]` |
| Better shadows | 4096 shadow maps (8192 in Ultra), a fixed filter grid with no per-pixel noise, cooler shadow colour. | `[shadows]` |
| Volumetric haze | Dusty fog with sun shafts. Far rocks and the horizon turn hazier and less saturated; haze follows the real sky colour. | `[atmos]` |
| Anti-aliasing | TAA or FXAA on top of the game's own MSAA. | `[post] AA` |
| Sharpen and grade | Contrast-adaptive sharpening (clamped so it does not make dark or bright halos), gentle filmic curve, vibrance. HUD is not affected. | `[post]` |
| Surface relief | Cracks, grain, wood and rope get relief and roughness from the texture's own colour. Ty's feet never sink into the ground. Characters are not changed. | `[pbr]`, `[tess]` |
| Bounce light (High and Ultra) | Light bounces between surfaces. Ty's red trousers do not tint the ground. | `[gi]` |
| Natural light | Cool sky-tinted shade, sky and ground bounce, translucent grass. | `[light]` |

## What is new in v0.3.2
- Flicker is reduced: the AO temporal filter uses history length and variance clamping, and the shadow filter no longer changes every frame.
- Shadows: a fixed grid filter (`[shadows] Grid=1`) replaces the noisy filter. Shadow edges no longer strobe. Leaf rims from the sharpen pass are removed (`[post] SharpenHaloClamp=1`).
- Haze: far trees and rocks take their colour from the real sky (`[atmos] HazeSky=0.6`). Optional distance blur (`DepthBlur`, off by default).
- AO fades with fog (`[ao] FogFade=1`), so AO no longer fights the fog in the distance.
- Characters: a capsule mask keeps Ty and NPCs from bouncing colour onto the ground (`[gi] Characters`, `CharacterColour`, `CharacterGI`, ...).
- Relief scope defaults to `env-detail`: the texture-driven relief on the ground and props, characters excluded.
- Launcher: a simple view with the four presets (Low, Medium, High, Ultra), an overall performance impact and the Play button; **More options >>** expands to every effect, the sliders and a description panel. Every option shows a performance tag (Free, Low, Medium, High).

## Presets
| Preset | Frame time at 2560x1440 (RX 7600 XT) | What it contains |
|---|---|---|
| Low | about 6.0 ms (about 166 fps) | No AO, no volumetric fog, FXAA, 2048 shadow maps |
| **Medium** (recommended) | about 12 ms (about 80 fps) | AO, fog, relief, 4096 shadow maps, TAA. No bounce light |
| High | about 18 ms (about 55 fps) | Medium + bounce light (1 ray) + contact shadows |
| Ultra | about 31 ms (about 32 fps) | AO 4x8, bounce light (2 rays), fog 32 steps, environment relief, 8192 shadow maps |

Stock game for comparison: about 5.7 ms. Full costs and the settings reference are in `PERFORMANCE.md`.

## Install
1. Copy `tyrender.dll`, `TYLauncher.exe` and `tyrender.ini` into the game folder (next to `TY.exe`).
2. Run `TYLauncher.exe`, pick a preset, press **Play with HD renderer**.
   The first Play creates `TY_tyrender.exe`, a copy of TY.exe that loads tyrender.dll. TY.exe itself is never changed.
3. Keep the launcher open and move the sliders. Most of them apply live.
4. Optional: **Steam setup** copies a launch option. Paste it into Steam > TY > Properties > Launch options, and Steam's Play button starts the mod.

`Play original (no mod)` starts the untouched game. `Advanced (ini)...` opens every setting in `tyrender.ini`.

Memory: `TY_tyrender.exe` is marked Large Address Aware (4 GB instead of 2 GB). Without this the 32-bit game runs out of address space with the HD texture pack (log: `CMKHeap - Out of memory`).

Uninstall: delete `tyrender.dll`, `TYLauncher.exe`, `TY_tyrender.exe`, `tyrender.ini`, `tyrender.log` and the `tyrender` folder.

## Settings file (`tyrender.ini`)
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
| `[gi]` | ExcludeSources | 3 (test build v0.3.3) | Which textures do NOT send bounce light: 1 = glowing (emissive) textures, 2 = see-through (water, glass, particles) textures, 3 = both, 0 = none (old behaviour). `DebugView=3` shows the mask |
| `[gi]` | SourceBlur / NoiseCycle / TemporalBlend | 0.15 / 8 / 0.85 (test build values) | Smoothness of the bounce light: blur of the sources, noise pattern length, how much of the previous frame is kept |
| `[debug]` | ShadowView, PlaneBias, PlaneClamp, SlopeBias, SkinBias | off | Test aids for shadow edges (v0.3.3 test build) |
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
| `[tess]` | ReliefSteps / ReliefFeature / ReliefDepth | 8 / 1.0 / 3 | Relief quality and depth |
| `[pbr]` | Enable | 1 | Bump and specular from the texture (restart) |
| `[pbr]` | DetailStrength / DetailScale / DetailDisp | 1.0 / 1.0 / 0.8 | Relief strength, scale, inward depth |
| `[pbr]` | LitProps | 1 | 0 = props keep the stock look |
| `[light]` | Enable / ShadowTint / Hemisphere / Foliage | 1 / 1 / 1 / 1 | Natural light; each `*Amount` key scales its part |
| `[debug]` | ForceFallback | 0 | Test aid: bits 1 fog, 2 GI, 4 AO, 8 relief, 16 light shaders are forced to fail |
| `[debug]` | StateGuard | 0 | Test aid: logs graphics-state changes |

The shipped `tyrender.ini` is the Medium preset. Its header comment quotes an older figure ("about 90 fps"); Medium measures about 12 ms (about 80 fps) in v0.3.2. The launcher writes the values it needs, so you normally do not need to edit the file by hand.

## Fallback
If your graphics card or driver cannot run an effect (for example no GL 4.3 compute for the fog, or a shader that does not compile), **that effect switches itself off** and the frame is drawn without it. The renderer does not fall back to a slower path. Check `tyrender.log` for the reason.

## Known limitations and open issues
Known issues from playtests 3 and 4 (v0.3.2, still in progress):
- **Water pulsing on Ty:** when Ty drops into water his brightness pulses (the water reflection reacts to him). Water no longer adds bounce light in the test build, but the reflection pulse is not fixed yet.
- **Character shadows:** a thin bright line runs through the shadow edge on characters (face, bag), and the shadow still looks slightly grid-like.
- **AO halo behind the character:** a dark halo appears on the side away from the camera, whatever the light direction.
- **White rim on fern shadows** on the ground.
- **Footprints glow:** some footprint textures read as emissive (almost white). The texture list is in place, but the glow is not confirmed fixed.
- **Fire and glowing lights:** the fire is not yet hazed by the fog, and the portal light does not light the ground.
- **Flicker on High and Ultra:** Medium is stable in playtests. High and Ultra flickered in playtest 4 (v0.3.2). The v0.3.3 test build has changes for this; please report whether it is better. If you see flicker, use Medium.
- **Ultra is slow** (about 31 ms per frame on the test card): use Medium or High on most computers.

Requested and planned, not yet built: AO and relief distance-fade sliders, light spill from glowing objects (portal, crystal), per-level fog tuning (the Billabong level can look too hazy). A/B comparison screenshots (vanilla, Low, Medium, Ultra) for the community exist and will be published separately.

Other limits:
- Only tested on an AMD RX 7600 XT. NVIDIA, Intel and Linux/Proton are untested.
- Motion shimmer and AO shimmer during fast camera moves have not been measured formally (playtest 3 on Medium/High reported no flicker; Ultra: see above).
- Thin objects (rope, grass) get little or no relief benefit: the source textures are flat.
- TAA can leave a faint trail on fast animation (no per-object motion vectors yet).
- Relief is inward only and derived from the texture colour (the game has no height maps), so it is subtle on low-resolution textures. Shadows come from the untessellated mesh.
- Built against the 2020-04-04 TY.exe. Other builds are untested.

Feedback: send `tyrender.log` and a screenshot with your GPU model and driver version.

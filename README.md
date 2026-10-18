# TY HD Renderer

*Better light for the outback, a fan mod for TY the Tasmanian Tiger on PC.*

G'day! TY HD Renderer gives the bush a bit more depth: shadows that look like shade, distant rocks that fade back, and light that bounces off things. Pick a preset, hit Play, and Ty's world looks a little more lived in. The game itself is left alone.

> **Fan project.** This is an unofficial, community-made mod. It is not made by or connected to the game's developers or publishers. It includes no game files, so you need your own copy of TY the Tasmanian Tiger for PC.

**Status:** community beta. The latest release is v0.3.5. Changes on `main` since then are listed in [CHANGELOG.md](CHANGELOG.md). Build `4a2ef95` was partly playtested on Ultra on 2026-10-18; later changes are not.

**Tested on:** one graphics card, AMD Radeon RX 7600 XT on Windows 11. NVIDIA, Intel and Linux/Proton are not tested yet, so reports from those are very welcome.

**Jump to:** [Side by side](#side-by-side) · [Presets](#presets) · [Install](#install) · [Known issues](#known-issues)

## What it changes

Ty's world is bold, flat and sunny, but its lighting is stuck in the early 2000s. Shadows are hard and capped at 50%. Distant rocks fade into a flat colour. Surfaces have no depth beyond their paint. Light never bounces off anything.

The mod fixes those four things and keeps the paint job:

- **Shadows.** Softer edges, a larger shadow map, and a cooler shade colour, so shade reads as shade.
- **Surface relief.** Cracks, grain, wood and rope get bumps and roughness from their own colour. Feet stay on the ground, and characters are not changed.
- **Contact shadows.** Ambient occlusion (AO) darkens crevices and corners. It fades with distance and stays steady from frame to frame.
- **Haze.** Volumetric fog with light shafts. Distant rocks lose a little colour, and the haze follows the real sky.
- **Bounce light** (High and Ultra). Surfaces pick up a little colour from their neighbours.
- **Smoother image.** TAA or FXAA for edges, with gentle sharpening and grading.
- **A launcher with sliders.** Every effect has a slider, a short description and a rough cost, and your settings are saved.

Nothing in the game files is changed. The original `TY.exe` is never modified, so to remove the mod, delete its files.

## Side by side

Ty's world, before and after. Each pair is an F11 capture from build `3ee6ea7` on the **Ultra** preset, taken on 2026-10-18. Left is F11 off, right is shaders on. Ultra is the slowest preset, so Medium will show less difference.

The F11 off frame skips AO, bounce light, atmosphere, anti-aliasing, shadow strength and light extras, so it is close to the plain game. It is not proven to be fully vanilla (the shadow filter and the tessellated draw path still run), so read "off" as F11 off, not as the stock game. Each caption names the effect and where to look, and describes only what is visible in the frames.

All images are in [`docs/ab/`](docs/ab/).

### Fog disabled

A common comment is that the mod only adds fog. These four pairs were taken with **fog disabled**, so no haze is involved. What changes is ambient occlusion, shadow strength and softness, relief and tessellation, and the material lighting.

<table>
<tr>
<th align="left">F11 off</th>
<th align="left">Shaders on</th>
</tr>
<tr>
<td><img src="docs/ab/nofog_1_house_gate_off.jpg" alt="Ty on the launch pad with fence and rope bridge, F11 off" width="100%"></td>
<td><img src="docs/ab/nofog_1_house_gate_on.jpg" alt="Ty on the launch pad with fence and rope bridge, shaders on" width="100%"></td>
</tr>
<tr>
<td colspan="2"><sub><b>1 Launch pad (fog disabled).</b> Fog is off in both frames, so everything that differs here is something else. Look at the red rug around Ty's feet (2x crop). Frame A: the rug is a flat painted texture. Frame B: relief (normals derived from the albedo, plus tessellation) gives the rug a bumpy surface with shaded pits, and the AO/shadow term puts a dark contact patch under and between the feet, which frame A does not have. The rock wall behind has deeper shading in its creases.</sub></td>
</tr>
<tr>
<td><img src="docs/ab/nofog_1_house_gate_crop_off.jpg" alt="Ty on the launch pad with fence and rope bridge, 2x crop, F11 off" width="100%"></td>
<td><img src="docs/ab/nofog_1_house_gate_crop_on.jpg" alt="Ty on the launch pad with fence and rope bridge, 2x crop, shaders on" width="100%"></td>
</tr>
<tr>
<td><img src="docs/ab/nofog_2_house_waterfall_off.jpg" alt="Ty at the cliff overlook above the waterfall pond, F11 off" width="100%"></td>
<td><img src="docs/ab/nofog_2_house_waterfall_on.jpg" alt="Ty at the cliff overlook above the waterfall pond, shaders on" width="100%"></td>
</tr>
<tr>
<td colspan="2"><sub><b>2 Waterfall overlook (fog disabled).</b> Look at the cliff face behind Ty (2x crop). Frame A: the red rock columns are evenly lit with little separation. Frame B: the creases between the columns are occluded (AO) and the faces carry stronger relief shading, so the columns read as rounded forms. Ty's face and neck are shaded on the underside in B (shadow strength and AO on the character), where A is flat. The leaf shadows on the ground at the bottom are present in both frames, so the shadow map itself is the same; B changes how dark and how soft it is.</sub></td>
</tr>
<tr>
<td><img src="docs/ab/nofog_2_house_waterfall_crop_off.jpg" alt="Ty at the cliff overlook above the waterfall pond, 2x crop, F11 off" width="100%"></td>
<td><img src="docs/ab/nofog_2_house_waterfall_crop_on.jpg" alt="Ty at the cliff overlook above the waterfall pond, 2x crop, shaders on" width="100%"></td>
</tr>
<tr>
<td><img src="docs/ab/nofog_3_station_sign_off.jpg" alt="Ty under the Bli Bli Station sign, F11 off" width="100%"></td>
<td><img src="docs/ab/nofog_3_station_sign_on.jpg" alt="Ty under the Bli Bli Station sign, shaders on" width="100%"></td>
</tr>
<tr>
<td colspan="2"><sub><b>3 Bli Bli Station (fog disabled).</b> Look at where the large rock meets the ground and grass on the left (2x crop). Frame A: the rock, the wall behind it and the grass are all lit evenly, and the grass is uniformly bright yellow. Frame B: the wall behind the rock is darker where the rock shields it (AO), the grass is darker at the roots and brighter at the tips, and the cracked ground in front of Ty shows grooves shaded on one side (relief and tessellation) instead of flat painted lines.</sub></td>
</tr>
<tr>
<td><img src="docs/ab/nofog_3_station_sign_crop_off.jpg" alt="Ty under the Bli Bli Station sign, 2x crop, F11 off" width="100%"></td>
<td><img src="docs/ab/nofog_3_station_sign_crop_on.jpg" alt="Ty under the Bli Bli Station sign, 2x crop, shaders on" width="100%"></td>
</tr>
<tr>
<td><img src="docs/ab/nofog_4_shade_ao_off.jpg" alt="Ty standing in the shade of a gum grove, F11 off" width="100%"></td>
<td><img src="docs/ab/nofog_4_shade_ao_on.jpg" alt="Ty standing in the shade of a gum grove, shaders on" width="100%"></td>
</tr>
<tr>
<td colspan="2"><sub><b>4 Shade under the trees (fog disabled).</b> The ambient occlusion case. Look at the tree trunks and grass at the left and the back of Ty (2x crop). Frame A: bright, flat light, the trunk in the top left is pale and the grass is evenly lit down to the ground. Frame B: the shaded area is clearly darker, the grass roots and the trunk bases are occluded, and the foliage in the distance separates into lit and shaded clumps. This is the AO and shadow term only, with no fog involved.</sub></td>
</tr>
<tr>
<td><img src="docs/ab/nofog_4_shade_ao_crop_off.jpg" alt="Ty standing in the shade of a gum grove, 2x crop, F11 off" width="100%"></td>
<td><img src="docs/ab/nofog_4_shade_ao_crop_on.jpg" alt="Ty standing in the shade of a gum grove, 2x crop, shaders on" width="100%"></td>
</tr>
</table>

### With fog on

Fog is separate. These pairs show the volumetric fog in the default setup, plus what else changes with it.

<table>
<tr>
<th align="left">F11 off</th>
<th align="left">Shaders on</th>
</tr>
<tr>
<td><img src="docs/ab/fog_1_canyon_gate_off.jpg" alt="Launch pad with fence, rope bridge and canyon behind, F11 off" width="100%"></td>
<td><img src="docs/ab/fog_1_canyon_gate_on.jpg" alt="Launch pad with fence, rope bridge and canyon behind, shaders on" width="100%"></td>
</tr>
<tr>
<td colspan="2"><sub><b>5 Launch pad (fog on).</b> Fog, same spot as pair 1. Look at the canyon behind the right-hand fence (2x crop). Frame A: the rock keeps full saturation and a hard edge to the sky. Frame B: the froxel volumetric fog is integrated along the view ray, so the distant rock and ground dissolve into a warm pink haze that is denser the further away it is, while the foreground fence and rug are barely touched.</sub></td>
</tr>
<tr>
<td><img src="docs/ab/fog_1_canyon_gate_crop_off.jpg" alt="Launch pad with fence, rope bridge and canyon behind, 2x crop, F11 off" width="100%"></td>
<td><img src="docs/ab/fog_1_canyon_gate_crop_on.jpg" alt="Launch pad with fence, rope bridge and canyon behind, 2x crop, shaders on" width="100%"></td>
</tr>
<tr>
<td><img src="docs/ab/fog_3_waterfall_reeds_off.jpg" alt="Waterfall, palms and reeds across the water, F11 off" width="100%"></td>
<td><img src="docs/ab/fog_3_waterfall_reeds_on.jpg" alt="Waterfall, palms and reeds across the water, shaders on" width="100%"></td>
</tr>
<tr>
<td colspan="2"><sub><b>6 Waterfall and reeds (fog on).</b> Fog and shading together. Look at the palms and the cliff beside the waterfall (2x crop). Frame B adds a haze that grows with depth: the far palms, the cliff and the waterfall mist lose contrast and take the fog colour. The reeds in front are also darker in B because of AO and shadow, which is separate from the fog.</sub></td>
</tr>
<tr>
<td><img src="docs/ab/fog_3_waterfall_reeds_crop_off.jpg" alt="Waterfall, palms and reeds across the water, 2x crop, F11 off" width="100%"></td>
<td><img src="docs/ab/fog_3_waterfall_reeds_crop_on.jpg" alt="Waterfall, palms and reeds across the water, 2x crop, shaders on" width="100%"></td>
</tr>
<tr>
<td><img src="docs/ab/fog_2_lily_pond_off.jpg" alt="Pond with reeds, a cliff wall and the sun flare, F11 off" width="100%"></td>
<td><img src="docs/ab/fog_2_lily_pond_on.jpg" alt="Pond with reeds, a cliff wall and the sun flare, shaders on" width="100%"></td>
</tr>
<tr>
<td colspan="2"><sub><b>7 Lily pond (fog on).</b> Height and distance fog on a large flat surface. Look at the cliff face behind the pond (2x crop). Frame A: the rock columns keep full contrast and saturation to the top. Frame B: the froxel fog lays a warm red veil over the whole face and lowers its contrast, and the foreground reeds are darker (AO and shadow). The sun flare is the same in both frames. Known issue: the lily pads and rocks in the water still look pale against the water and the fog looks heavy from above (#4); this pair is about the cliff, not the water.</sub></td>
</tr>
<tr>
<td><img src="docs/ab/fog_2_lily_pond_crop_off.jpg" alt="Pond with reeds, a cliff wall and the sun flare, 2x crop, F11 off" width="100%"></td>
<td><img src="docs/ab/fog_2_lily_pond_crop_on.jpg" alt="Pond with reeds, a cliff wall and the sun flare, 2x crop, shaders on" width="100%"></td>
</tr>
<tr>
<td><img src="docs/ab/fog_4_koala_lvl2_off.jpg" alt="Level 2 grass and ground with the koala, F11 off" width="100%"></td>
<td><img src="docs/ab/fog_4_koala_lvl2_on.jpg" alt="Level 2 grass and ground with the koala, shaders on" width="100%"></td>
</tr>
<tr>
<td colspan="2"><sub><b>8 Level 2 (fog on).</b> Little fog is visible here, because the scene is close and the air is clear, so this pair shows what else changes with the default setup. Look at the ground shadows and the far wall (2x crop). Frame A: shadows are neutral grey and the far wall is crisp. Frame B: shadows take a cooler sky-blue shade colour, the far wall and grass sit in a light haze, and the grass blades show surface relief. The koala is a character and is not changed by relief.</sub></td>
</tr>
<tr>
<td><img src="docs/ab/fog_4_koala_lvl2_crop_off.jpg" alt="Level 2 grass and ground with the koala, 2x crop, F11 off" width="100%"></td>
<td><img src="docs/ab/fog_4_koala_lvl2_crop_on.jpg" alt="Level 2 grass and ground with the koala, 2x crop, shaders on" width="100%"></td>
</tr>
</table>

Water and the portal are not shown here. The blue flicker on Ty in water, the lily pads against the water and the portal light are known issues. See [Known issues](#known-issues).

## Presets

Frame times are GPU milliseconds per frame, measured by the author at 2560x1440 windowed on one AMD Radeon RX 7600 XT. Other GPUs will be faster or slower. Lower is faster.

| Preset | Frame time | About fps | What it adds |
|---|---|---|---|
| Stock game (no mod) | 5.7 ms | 175 | The original game, for comparison |
| Low | about 6.0 ms | 166 | FXAA smoothing and 2048 shadow maps. No soft corner shadows, no haze |
| **Medium** (recommended, default) | about 12 ms | 80 | Soft corner shadows, haze, surface relief, 4096 shadow maps, TAA smoothing. No bounce light |
| High | about 18 ms | 55 | Medium plus bounce light (1 ray) and contact shadows |
| Ultra | about 31 ms | 32 | More AO samples, bounce light (2 rays), higher-quality haze, environment relief, 8192 shadow maps |

Start with Medium. It is the preset that has held up best in playtests, and it is the default. High and Ultra flickered in playtest 4 (v0.3.2, see `docs/FINDINGS_ROUND4.md`). The AO flicker has since been fixed and Ultra on build `4a2ef95` played without it, but use Medium if you see flicker.

### What each option costs

Measured against Medium, on one GPU:

| Option | Change | Rough frame cost |
|---|---|---|
| Ambient occlusion (AO) | Off | saves about 1.15 ms |
| Volumetric haze | Off | saves about 2.1 ms |
| Contact shadows | On | about +1.7 ms |
| Bounce light | On, 1 ray | about +4.4 ms |
| Shadow maps | 8192, 32 taps | about +5.1 ms |
| Environment relief | `[tess] Scope=env` | about +6.2 ms |

These are approximate, because effects interact. They come from the v0.3 measurements in [`docs/PERFORMANCE.md`](docs/PERFORMANCE.md). The v0.3.2 changes have not been timed one by one.

## Install

Two ways in. The first is the easy one. The second is for people who like poking at config files.

**Easiest:** download `TY HD Renderer Setup.exe` from the release and run it. It finds the game (Steam libraries first, otherwise you pick the folder with `TY.exe`), installs the renderer, and adds "TY HD Renderer (mod)" to Apps and Features.
Files it replaces are backed up in `tyrender\backup`. Your existing `tyrender.ini` is kept, and only missing settings are added. The overlay key is F10.

**By hand:**

1. Download `TYLauncher.exe` from the release.
2. Put it in the game folder, next to `TY.exe`.
3. Run `TYLauncher.exe`, pick a preset (Medium is the recommended one) and press **Play with HD renderer**.
   The first start installs the renderer files next to it. The first Play creates `TY_tyrender.exe`, a copy of `TY.exe` that loads `tyrender.dll`. `TY.exe` itself is never changed.
4. Keep the launcher open to move the sliders. Most of them apply live.
5. Optional: **Steam setup** copies a launch option. Paste it into Steam > TY the Tasmanian Tiger > Properties > General > Launch options, and Steam's Play button will start the mod.

The launcher is a single file. The renderer DLL and default settings are built into it. The title bar shows the version and build id.

**Play original (no mod)** starts the untouched game. **Advanced (ini)...** opens `tyrender.ini`.

**Memory:** `TY_tyrender.exe` is marked Large Address Aware (4 GB instead of 2 GB). Without this, the 32-bit game can run out of memory with the HD texture pack (log line: `CMKHeap - Out of memory`).

## Uninstall

No worries if you want out. Run **`TY HD Renderer Uninstall.exe`** in the game folder, or use Apps and Features > **TY HD Renderer (mod)**. It is separate from the game's own uninstall.
It deletes only the files the install added and puts back the originals it saved. It asks whether to keep your `tyrender.ini` settings. The game folder's own files are not touched.

By hand: delete `tyrender.dll`, `TYLauncher.exe`, `TY_tyrender.exe`, `tyrender.ini`, `tyrender.log`, and the `tyrender` folder if it exists. If you added the Steam launch option, remove it in Steam. The original game keeps working without these files.

## Known issues

Ty's still got a few rough edges. Here they are, straight from playtests. Where a fix is on `main`, it is marked untested.

- **Water:** in build `4a2ef95`, the light on Ty ramps up and down slowly in water instead of flickering (#5). Lily pads and rocks look brighter than the water under them, and fog is heavier looking down than across (#4). A water outline shows when viewed from above (#104).
- **Fog:** sky and red rock now fade smoothly, and fog on trees is slight. Fire is not fogged (#4).
- **Shadows:** a sharp shadow line crosses the bridge near the fire (#103).
- **Portal light:** the rainbow portal does not light the ground (#9). `[emit] Enable` is off by default because it draws a white band when on.
- **Character shadows:** a thin bright line can run through the shadow edge on characters (face, bag), and the shadow looks slightly grid-like.
- **Dark halo:** a dark halo can appear behind a character, on the side away from the camera.
- **White rim:** fern shadows on the ground have a white rim.
- **Ultra is slow:** about 31 ms per frame on the test card.
- **Thin objects:** rope and grass get little benefit from relief, because their source textures are flat.
- **Trails:** TAA can leave a faint trail on fast animation.
- **Low-resolution textures:** relief comes from the texture's own colour, so it is weaker on low-resolution textures.
- **Shimmer:** shimmer during fast camera moves has not been measured formally.
- **F10 overlay:** the in-game settings overlay broke geometry in an earlier playtest. `main` now defaults to `[overlay] Key=F10`, but in build `4a2ef95` F10 did nothing, because an existing `tyrender.ini` keeps its old `Key=0`. Set `Key=F10` yourself. It is still untested in a level (#79).
- **Game version:** built against the 2020-04-04 `TY.exe`. Other builds are untested.

## Reporting problems

Hit a glitch? Ta for reporting it. Send your `tyrender.log`, a screenshot (the F11 hotkey takes comparison shots), your GPU model and your driver version. NVIDIA and Intel owners: a `tyrender.log` from one run is especially welcome, since those GPUs are untested.

## Reporting a bug

Every time you press Play with the HD renderer, the mod writes a short report file next to the game: `tyrender\logs\TYHD_report_<time>_<build>.txt`. Press **Report a problem / open logs folder** in the launcher's Advanced tab to open that folder.

To report a problem, send:

1. The newest `TYHD_report_` file from that folder.
2. A screenshot of the problem.
3. Your GPU model (the file has it too).

**What the file contains:** the build and preset, Windows version and build, OS architecture, CPU model and core count, RAM size, display resolution and refresh rate, GPU vendor and model, driver version (inside the OpenGL version line), a few OpenGL limits, the game's `settings.ini` and the mod's `tyrender.ini`, frame times for the first 5 minutes of play, and events such as GL errors, shader failures, settings changes, screenshots and window changes.

**What it does not contain:** usernames, the computer name, serial numbers, IP or MAC addresses, installed programs, running processes, file listings or personal folder paths. Paths are relative to the game folder.

**Privacy:** the file is only written to your disk. Nothing is sent anywhere. You decide whether to send it.

**Settings:** `[report]` in `tyrender.ini`. `Enable=0` turns it off. `CaptureSeconds` sets how long the frame-time part runs (default 300), `MaxBytes` caps the file size (default 1 MB), and `KeepFiles` keeps only the newest reports (default 10).

## Compatibility and credits

Cheers to everyone who sends a log, a screenshot or a "it flickers on my card" message. That is how the mod gets better.

- Works with the base game.
- Gets more from the ty2k mod, because more texture detail means more precise relief.
- The HD texture pack is separate from this project.
- The cast you will meet: Ty, Shazza the dingo, Sly the tiger rival, Julius the koala scientist, Maurie the cockatoo and Boss Cass the cassowary. Characters are left out of surface relief.
- Ty and the cast belong to their own rights holders. This mod is a fan project and ships none of their work.
- A hobby project, made with Claude. Test reports are welcome.

## For modders

Presets cover most needs, and the launcher writes the values it needs. This part is for tinkerers. To tune by hand, edit `tyrender.ini` in the game folder, or open it from **Advanced (ini)...** in the launcher. Per-option costs are in [`docs/PERFORMANCE.md`](docs/PERFORMANCE.md).

If a graphics card or driver cannot run an effect (for example no GL 4.3 compute for the fog, or a shader that does not compile), **that effect switches itself off** and the frame is drawn without it. The renderer does not fall back to a slower path. Check `tyrender.log` for the reason.

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

## Changes and history

The good oil is in [CHANGELOG.md](CHANGELOG.md). Builds are identified by the git commit in the launcher title bar.

# TY HD Renderer v0.3.2 - performance costs and settings reference

All numbers are **GPU milliseconds per frame** (total frame time from the renderer's GPU timers, `[gpu]` / PERF lines in `tyrender.log`), at **2560x1440, windowed, vsync off**, on an **AMD Radeon RX 7600 XT** (the only GPU tested so far). 16.7 ms = 60 fps. The stock game frame there is about **5.7 ms** (v0.3 measurement).

Other GPUs: scale by their speed relative to a 7600 XT. Heavier views cost more than the default view (see the test-spot table below).

## Presets

| Preset | Frame | ~fps | What it contains |
|---|---|---|---|
| Stock game | 5.7 ms | 175 | |
| Low | about 6.0 ms | about 166 | No AO, no fog, FXAA, 2048 shadow maps |
| **Medium** (recommended, default) | **about 12 ms** | **about 80** | AO 3x6, fog 24 steps, relief on the ground and props, 4096 shadow maps x16 taps, TAA, no bounce light |
| High | about 18 ms | about 55 | Medium + bounce light (Hi-Z, 1 ray) + contact shadows |
| Ultra | about 31 ms | about 32 | AO 4x8, bounce light (2 rays), fog 32 steps, environment relief, 8192 shadow maps x32 taps |

On the test card, Low and Medium are inside the 16.7 ms (60 fps) budget. High (about 18 ms) and Ultra (about 31 ms) are above it: they are for faster cards or for players who accept a lower frame rate.

## Test build v0.3.3 (not released; smoke numbers only)

The v0.3.3 test build changes only the bounce light (High and Ultra) and the texture flags. Medium does not use bounce light, so Medium is unchanged.

| Preset | v0.3.2 | v0.3.3 test build (one run) |
|---|---|---|
| Medium | about 12 ms | 12.05 ms |
| High | about 17 to 18 ms | 19.80 ms |

- High gets about 2.5 ms more because of the bounce-light changes (source mask, blur and noise). The split between the parts is not measured yet, and the cost of the mask is to be optimised.
- The texture flags (character, glow, see-through) cost nothing measurable.
- No full gate was run for v0.3.3 (no soak, no forced-fallback run). Do not treat these numbers as a release figure.

## Test spots (v0.3.2 release gate, Medium preset)

| Test spot | Frame |
|---|---|
| Default view | 12.08 ms |
| Vista (heavy distant view) | 10.45 ms |
| Sand, grazing angle | 10.47 ms |
| Fence, middle distance | 10.21 ms |
| Forced fallback (every effect switched off by the safety path) | 7.44 ms |

Soak test: 10 minutes of play, no crash.

## Cost of each option relative to Medium (default view)

These are v0.3 measurements; the v0.3.2 changes (shadow grid, AO temporal mode, haze, character mask) are small and not yet re-timed one by one.

| Option | Change | Frame ms |
|---|---|---|
| Ambient occlusion off | | about -1.15 |
| Volumetric fog and light shafts off | | about -2.1 |
| Relief (skin/detail) off | the detail layer rides on it; cost is tiny | about -0.05 |
| TAA off | (FXAA/none) | about +0.4 (TAA is not the expensive part) |
| Contact shadows 0.35 | | about +1.7 |
| Bounce light (Hi-Z, 1 ray) | barely visible on its own | about +4.4 |
| Shadow maps 8192 x32 taps | | about +5.1 |
| Environment relief (`[tess] Scope=env`) | optional; little visible gain | about +6.2 |
| Shadow grid filter (v0.3.2 default) vs old filter | Medium | about +0.7 |

Per-pass GPU timers under-count: the hidden extra is full-resolution copies and resolves. Use the frame totals above. Effects interact, so costs are "Medium plus or minus one option", not a sum.

## Fallback behaviour
If a GPU or driver cannot run an effect (no GL 4.3 compute for the fog, a shader that fails to compile, ...), **that effect switches itself off** and the frame is drawn without it. The renderer never switches to a slower path. Forced-failure test of every effect at once: 7.44 ms (renders normally, no crash).

`[debug] ForceFallback=<bits>` simulates failures: 1 fog, 2 bounce light, 4 AO, 8 relief, 16 light shaders.

## Settings reference (`tyrender.ini`, next to the game)

The full key list with defaults is in `README.md` (section "Settings file"). Summary of what costs time:

| Setting | Cost |
|---|---|
| `[ao] Enable` (AO off saves time) | about 1.2 ms at Medium |
| `[atmos] Enable` (fog off saves time) | about 2 ms at Medium |
| `[gi] Enable` (bounce light) | about 4 ms at 1 ray; 2 rays and more in Ultra |
| `[atmos] ContactShadows` > 0 | about 1.7 ms |
| `[shadows] MapScale 4 / Taps 32` | about 5 ms more than Medium |
| `[tess] Scope=env` | about 6 ms more than Medium |
| `[shadows] Grid` (1 = fixed grid, recommended) | about 0.7 ms more than the old filter |
| `[ao] Legacy=1` (old AO) | about 5 ms more than the new AO; not recommended |
| `[ao] TemporalMode`, `[atmos] HazeSky`, `[post] SharpenHaloClamp`, `[gi] Characters` | negligible |

Live (applied while the game runs): `[ao] [gi] [atmos] [post] [fog] [light]`, `[pbr]` uniforms, `[shadows] Strength GrassStrength`.
Needs a game restart: `[shadows] MapScale Taps`, `[tess] Enable Scope MaxLevel`, `[pbr] Enable` (shaders are rewritten at load).

## Launcher performance tags
The launcher shows **Free / Low / Medium / High** next to every option instead of milliseconds. The tags come from the measured costs above (Free < 0.2 ms, Low < 1.5 ms, Medium < 3.5 ms, High >= 3.5 ms at 2560x1440, scaled by your resolution). They rank the options for any GPU without claiming exact numbers. The total at the bottom of the launcher is a guide, not a promise.

## Notes on measuring
- Only the DLL's GPU timer lines count. Compare in-session A/B images only; images from different sessions are not pixel-comparable.
- Measurements are for one GPU and the default test views. Your numbers will differ.

# Screen-space bounce light, Hi-Z path (`[gi] Mode=1`, default)

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), B (tester who tunes `[gi]` or reports flicker).

## Purpose

This pass adds one diffuse bounce of colour from lit, on-screen surfaces into shaded areas. It is the default GI path (`[gi] Mode=1`, `src/gi.inl:95`). The effect is subtle: `docs/results/VERDICTS.md` measured the frame changing by 0.8 to 3.1 of 255 on average. It is on in the High and Ultra presets and off in Low and Medium (`src/launcher_ui.inc:11-14`). The shipped `default_tyrender.ini` has `[gi] Enable=0` (`default_tyrender.ini:8`). The screen-space legacy path is in [gi-legacy](gi-legacy.md). Both share the settings, the colour source and the composite formula.

## How it works

The Hi-Z path is a set of GL 3.3 passes at half resolution plus one full-resolution composite (`src/gi2.inl:1-9`). It runs at the depth copy (`src/tyrender.cpp:291`, after AO).

```
engine depth copy ---> [0] depth pyramid R32F (1/viewZ, max-pooled)        gi2.inl:104-122, :434-444
engine colour copy -.
                     [1] trace, half res, MRT:                               gi2.inl:446-460
                         raw bounce radiance -> texRaw (RGBA16F)
                         normal + view depth  -> texGB (RGBA16F)
                     [2] temporal, half res, MRT:                            gi2.inl:461-475
                         colour history -> texHC (RGBA16F)
                         luminance moments + history length -> texHM (RGBA32F)
                     [3] a-trous filter, half res, Atrous iterations          gi2.inl:476-497
                     [4] composite, full res, additive onto FB0              gi2.inl:499-511
```

1. **Pyramid (`kGIPyr0`, `kGIPyrDown`).** The helper block `kGIC2` (`gi2.inl:98-102`) declares the projection scale and full-res size, and `PosFromH` rebuilds a view-space position from a pixel and its 1/z value. Level 0 holds `1 / view depth` for every full-res pixel. Sky is 0 (`gi2.inl:104-111`). Each further level keeps the maximum of its 2x2 children. Odd sizes read a third texel (`gi2.inl:113-122`). The level count is the number of halvings until one dimension reaches 1 (`gi2.inl:371`). The pyramid is what lets the trace skip empty space.
2. **Trace (`kGITrace2`, `gi2.inl:134-227`).**
   - Each half-res pixel reads the depth at its full-res twin pixel (`gi2.inl:200-201`). The normal comes from the pyramid (`gi2.inl:203-209`). Sky pixels write zero radiance and a sky flag (`gi2.inl:202`).
   - The ray start is 2 units off the surface along the normal (`gi2.inl:213`).
   - `Rays` directions are drawn cosine-weighted around the normal (`gi2.inl:218-224`), with interleaved gradient noise per pixel and a per-frame offset (`gi2.inl:211`). The noise frame is `frame % NoiseCycle` (default 8, `gi2.inl:455`).
   - A distance LOD test keeps each ray with probability `lk` and divides by the expected count so the mean stays unbiased (`gi2.inl:216-219`, `:225`).
   - `Trace()` (`gi2.inl:142-197`) clips the ray to the screen, then walks the pyramid. It goes up a level when the ray is in front of a cell, down a level when it is behind one, and stops at level 0. A hit is accepted when the ray is behind the nearest surface by less than `Thickness`, measured in view-space units (`gi2.inl:175`). The walk is limited to `Steps*4` iterations, or `HiZIter` if set (`gi2.inl:453`).
   - Source exclusion: a hit on a masked texel (alpha below 0.25) gives no bounce (`gi2.inl:187`). The mask is described below.
   - Footprint blur: the hit colour is taken from a small cross of samples when the hit is far away. The width grows with distance and `SourceBlur` and is capped at 40 pixels (`gi2.inl:189-194`). This is meant to tame animated, high-contrast sources such as water (comment, `gi2.inl:189`).
   - Characters: a hit inside a character capsule has its colour mixed toward its luminance by `CharacterColour` (`gi2.inl:195`, capsule test `gi2.inl:127-132`).
   - Output: the average of the rays, and the normal and view depth for the next pass (`gi2.inl:225-226`).
3. **Temporal (`kGITemporal2`, `gi2.inl:230-263`).** Each texel is reprojected with last frame's VP and bilinearly blended from four history texels. A history texel is rejected when its depth differs by more than 5 % plus 3 units (`gi2.inl:249`). The blend weight is `max(1/(n+1), 1-TemporalBlend)`, with `n` the history length (`gi2.inl:258`). Luminance mean, mean of squares and history length go to `texHM` (`gi2.inl:259-261`). These moments feed the filter.
4. **A-trous filter (`kGIAtrous`, `gi2.inl:265-312`).** The variance per texel comes from the moments. Where history is short (fewer than 4 frames) a 3x3 spatial variance is added (`gi2.inl:269-280`). Each iteration is a 5x5 kernel with step `1 << i` (`gi2.inl:294-297`, `:491`). The 5x5 becomes 3x3 beyond `[lod] Start` when `AtrousFar=1` (`gi2.inl:294`). The weights combine the kernel, a high power of the normal dot product (`gi2.inl:301`), a plane-distance term (`gi2.inl:302-303`), and a luminance term whose width is `SigmaL` times the local standard deviation (`gi2.inl:289`, `:305`). `Atrous` is clamped to 0..5 (`gi2.inl:479`). The default 3 gives three passes with steps 1, 2 and 4.
5. **Composite (`kGIComposite2`, `gi2.inl:314-344`).** A 2x2 bilinear upsample weighted by depth (`gi2.inl:322-332`). The bounce is multiplied by the albedo proxy (a hue-preserving, desaturated colour at a modest level; `gi2.inl:339`) and by a shade factor that falls to zero on bright surfaces (`gi2.inl:340`). Received GI is scaled by `CharacterGI` on character pixels (`gi2.inl:335-337`). The output is clamped to 0.25 (`gi2.inl:342`) and added to FB0 with blend `ONE, ONE` (`gi2.inl:509`).

**Source colour.** The bounce colour comes from the engine colour copy taken before AO (`tyrender.cpp:285` sets `g_colorCopyTex`, and the re-copy that includes our work happens later at `tyrender.cpp:294-297`). So the bounce source does not include AO or earlier GI in the same frame (inferred from those lines and `gi2.inl:458`, `:508`).

**Source mask (`GIMaskBegin`/`GIMaskEnd`, `gi2.inl:38-55`).** The draw hooks call `GIMaskBegin` after each draw (`tyrender.cpp:269` and `:271`). If the draw's diffuse texture is in a material class with a GI bit set in `[gi] ExcludeSources`, the draw is repeated with blending that writes alpha 0 into FB0 (`gi2.inl:47`). The bits are:

| Bit | Meaning | Material classes whose `giBits` include the bit, with their `giBits` value (`src/tex.inl:39-47`) |
|---|---|---|
| 1 | emissive tagged | EMITTER (1), TRANS_EMITTER (3) |
| 2 | transparent tagged | TRANSPARENT (2), TRANS_EMITTER (3) |
| 4 | character | CHARACTER (4), EMIT_CHARACTER (4) |

`UNKNOWN` and `SOLID` have bit 0, so unknown texture names are colour sources. The default `ExcludeSources=7` masks all three bits (`gi2.inl:33`, `gi.inl:93`). Before the trace, FB0 alpha is copied at half resolution into a mask texture with nearest sampling (`gi2.inl:412-426`), and only when at least one masked draw happened this frame. The water textures in `data/texture_tags.csv` are tagged transparent and not emissive (for example line 3, `data/texture_tags.csv:3`), so they are masked by default. Texture classes are explained in [material model](../concepts/material-model.md) and [texture names and tags](../how-it-works/07-texture-names-and-tags.md).

**Character capsules.** Up to 8 character origins are collected from skinned draws (`src/tyrender.cpp:84-89`, `:347`). Each becomes a vertical capsule of height `CharacterHeight` and radius `CharacterRadius`. The bottom 14 units are ignored (`gi2.inl:129`). The capsules are used only for colour and received light. They are not used to decide which draws are sources; that is the mask's job (`gi2.inl:33`, see Discrepancies).

**Fallback.** If the Hi-Z programs or framebuffers cannot be built, `g2.failed` is set and stays set (`gi2.inl:353`, `:362`, `:387`). `RunGI` then turns GI off for the session (`src/gi.inl:130`, log `[fallback] Hi-Z GI unavailable -> GI OFF (no legacy fallback)`). The legacy path is not used (`gi.inl:132`, `:139`). `ForceFallback` bit 2 makes every `gi_*` program fail to compile, which takes this path to OFF (`src/ao.inl:441`).

**Dev-only helper.** `[gi] Sweep=1` cycles through nine variants of mode, rays, Atrous and iterations every 150 frames (`gi2.inl:21-28`). It is for benchmarking, not for users.

## Code pointers

| What | File:line | Note |
|---|---|---|
| Call site (after AO, before emitter and atmosphere) | `src/tyrender.cpp:291` | `RunGI` |
| Mask hooks in the draw calls | `src/tyrender.cpp:269`, `:271` | `GIMaskBegin` / `GIMaskEnd` |
| Mask begin and end | `src/gi2.inl:38-55` | |
| Pass resource struct and timers | `src/gi2.inl:61-95` | sub-timers `GIMark(1..5)` |
| Shared character uniforms | `src/gi2.inl:125-132` | `GI_CHAR_GLSL` |
| Pyramid level 0 and max-pool | `src/gi2.inl:104-122` | `kGIPyr0`, `kGIPyrDown` |
| Trace shader | `src/gi2.inl:134-227` | `Trace()` :142-197, main :198-227 |
| Temporal shader | `src/gi2.inl:230-263` | |
| A-trous shader | `src/gi2.inl:265-312` | |
| Composite shader | `src/gi2.inl:314-344` | |
| Driver: targets, passes, state | `src/gi2.inl:349-515` | `GIHizInit` :349, `GIHiz` :394 |
| Mask copy to half-res texture | `src/gi2.inl:412-426` | |
| Hi-Z and legacy switch | `src/gi.inl:129-130` | |
| Config loader | `src/gi.inl:89-97` | keys and defaults in code |
| Flag files | `src/gi.inl:98-102` | `gi_off`, `gi_debug` |
| Material class to bit table | `src/tex.inl:36`, `:39-47` | `kMatPolicy` |
| Force-fail for `gi_*` programs | `src/ao.inl:441` | |

## Settings

Full table in [settings-ao-gi-lod](../reference/settings-ao-gi-lod.md). Keys that matter here:

| Key | Code default (file:line) | Shipped `default_tyrender.ini` | Live or restart | Effect |
|---|---|---|---|---|
| `[gi] Enable` | 1 (`gi.inl:91`) | 0 (`:8`) | live | master switch |
| `[gi] Mode` | 1 (`gi.inl:95`) | 1 (`:9`) | live | 0 = legacy path, 1 = Hi-Z |
| `[gi] Rays` | 4 (`gi.inl:91`) | 1 (`:10`) | live | rays per pixel; High 1, Ultra 2 (`launcher_ui.inc:13-14`) |
| `[gi] Steps` | 8 (`gi.inl:91`) | not set | live | Hi-Z iterations = 4 x Steps unless `HiZIter` is set |
| `[gi] HiZIter` | 0 (`gi.inl:95`) | not set | live | explicit trace iterations, 0 = 4 x Steps |
| `[gi] Strength` | 0.7 (`gi.inl:93`) | not set | live | output multiplier; launcher "Bounce light" default 0.70 (`launcher_ui.inc:25`) |
| `[gi] Radius` | 400 (`gi.inl:93`) | not set | live | ray length, world units |
| `[gi] Thickness` | 120 (`gi.inl:93`) | not set | live | hit thickness, world units |
| `[gi] TemporalBlend` | 0.85 (`gi.inl:93`) | not set | live | history weight floor is 1 - this |
| `[gi] Atrous` | 3 (`gi.inl:95`) | not set | live | filter iterations, 0..5 |
| `[gi] AtrousFar` | 0 (`gi.inl:95`) | 0 (`:14`) | live | 3x3 instead of 5x5 beyond `[lod] Start` |
| `[gi] SigmaL` | 4 (`gi.inl:93`) | not set | live | luminance edge width of the filter |
| `[gi] SourceBlur` | 0.15 (`gi.inl:93`) | not set | live | footprint blur for far hits, 0 = off |
| `[gi] NoiseCycle` | 8 (`gi.inl:93`) | not set | live | 0 = noise every frame, N = repeats every N frames, 1 = static |
| `[gi] ExcludeSources` | 7 (`gi.inl:93`) | not set | live | mask bits, see the table above |
| `[gi] Checker` | 0 (`gi.inl:95`) | 0 (`:12`) | live | trace half the pixels each frame |
| `[gi] Characters` | 1 (`gi.inl:94`) | not set | live | capsule colour and received-light effects |
| `[gi] CharacterColour` | 0.0 (`gi.inl:94`) | not set | live | colour kept by hits on a character |
| `[gi] CharacterGI` | 0.3 (`gi.inl:94`) | not set | live | GI received by character pixels |
| `[gi] CharacterHeight` / `CharacterRadius` | 160 / 70 (`gi.inl:94`) | not set | live | capsule size |
| `[gi] DebugView` | 0 (`gi.inl:95`) | not set | live | 1 GI only, 2 character mask (Hi-Z) |
| `[gi] Prof` | 0 (`gi.inl:95`) | not set | live | per-pass GPU timers, logged every 300 samples |
| `[gi] Sweep` | 0 (`gi.inl:95`) | not set | live | dev-only benchmark sweep |

"Live" means the config is reloaded with the other passes (see [AO](ao-gtao.md) for the reload triggers). Changing `Mode` or `Enable` takes effect on the next frame. A failure is sticky and needs a restart (`gi2.inl:350`).

## Cost

All numbers are GPU time or whole-frame deltas. Build, GPU and view are given per source.

| Setting | Delta | Source |
|---|---|---|
| Hi-Z, 1 ray | +4.68 ms over stock 5.38 ms (default camera) | `docs/results/VERDICTS.md`, "Standard-view numbers" |
| Hi-Z, 2 rays / 4 rays | +5.50 ms / +7.13 ms | same table |
| Hi-Z, 1 ray, off Balanced base 11.09 ms | +4.42 ms | `docs/results/cost_table.md` |
| "Bounce light (Hi-Z, 1 ray)" | about +4.4 ms, "barely visible on its own" | `docs/PERFORMANCE.md`, cost table at v0.3.2 |
| High, GI pass alone (GPU timer) | 2.11 ms (under-counts) | `docs/results/preset_matrix.md` |
| Ultra, GI pass alone (GPU timer) | 2.50 ms (under-counts, 2 rays) | same |
| First Hi-Z breakdown, 1440p, 1 ray (GI only) | pyramid 0.16, trace 0.43, temporal 0.12, a-trous 1.20, composite 0.27; total 2.18 ms | `docs/agent-tasks/gi.md`, "State" |
| v0.3.3 test build, High vs v0.3.2 High | +2.5 ms from mask, blur and noise (not split) | `docs/PERFORMANCE.md`, "Test build v0.3.3" |

The per-pass GPU timers under-count. Use whole-frame deltas, as `docs/results/VERDICTS.md` and `docs/PERFORMANCE.md` do. The a-trous filter was the biggest single pass in the early breakdown. The later agent task asked for it to be cut, but the file does not say whether it was (see Known issues).

Scaling (inferred): rays and iterations scale the trace; `Atrous` passes and the mask copy are per-frame costs that do not depend on rays.

Not measured: the distance LOD on GI (`[lod] GI`), and `Checker` (`docs/PERFORMANCE.md` says it is untested). See NI-GIH-5.

## Limits

- **Screen space only.** Light from off-screen surfaces never reaches a pixel. Hits must be inside the depth buffer. See [constraints](../concepts/constraints-and-limits.md).
- **Source is the engine colour copy.** AO and earlier bounce in the same frame are not in the source (see How it works).
- **Thickness test.** Thin objects are accepted as hits within `Thickness`. Very thick or very thin geometry can produce false hits or misses.
- **Mask resolution.** The mask is half-resolution and uses a hard threshold at alpha 0.25. Thin masked objects can partly leak into the trace (inferred from `gi2.inl:412-426` and `:187`).
- **Noise and history.** At 1 ray the result is noisy before the filter. The filter and history smooth it at the cost of lag and smear on moving content.
- **Small visible effect.** At the tuned strength the mean change is small (`docs/results/VERDICTS.md`, "gi hiz" row). Raising `Strength` makes glow appear on bright grass if the shade factor is wrong (`docs/agent-tasks/gi.md`, step 3).
- **Character handling is partial.** Ty's coloured clothing can tint the ground through the bounce if the draw is not masked (`docs/agent-tasks/gi5.md`; todo T84 "red culprit unproven").

## Known issues

- **#5 Water flicker on Ty ("disco ball").** Ty flickers blue when water is in view. `docs/recording/FINDINGS_ROUND4.md` (playtest 4, v0.3.2) saw it at 3:18 and 3:48 with Ty almost still, and blamed GI sampling the water. A source mask was added in the v0.3.3 test build (`docs/recording/FINDINGS_ROUND4.md`, header). `CHANGELOG.md` (playtest of `4a2ef95`) still lists "water brightness ramps slowly on the character (GI off the blue texture, not a frame flicker, #5)" as open. `docs/ISSUES_SEED.md` #5 says the flicker also happens on Medium with GI off. So GI is not the only source. Status: NI-GIH-1.
- **#6 High and Ultra flicker.** Medium is stable. The suspects were GI at 1 ray and contact shadows (`docs/recording/FINDINGS_ROUND4.md`). todo T78 is "open" and T81 (the GI agent) is "partial" (`docs/todo.json`).
- **#7 High GI cost.** High adds about 2.5 ms over v0.3.2 from the mask and blur (`docs/PERFORMANCE.md`). Target: within 1 ms. Open.
- **#7 and #75 GI checker.** `Checker=1` is in the code and the shipped ini as an option. The changelog marks it untested (`CHANGELOG.md`).
- **#93 AtrousFar.** Option in code, off, untested (`CHANGELOG.md`).
- **#10 distance LOD.** `[lod] GI=1` is in code, untested, not measured (`CHANGELOG.md`, `docs/PERFORMANCE.md`).
- **Red tint on the ground from Ty's trousers.** Reported in playtests 3 and 5 (`docs/recording/FINDINGS_ROUND4.md`). todo T84: "character GI mask bit4; red culprit unproven".
- **Ghosting on grass (playtest 5).** `docs/recording/FINDINGS_ROUND4.md` lists it. Attribution to TAA (#2) or GI is not settled. See NI-GIH-4.

## How to verify

1. **GI only.** Write `1` to `<game folder>\tyrender\gi_debug`, or set `[gi] DebugView=1`. The view shows the bounce term alone (`gi2.inl:341`). It is polled every 8 frames (`gi.inl:98-102`, `tyrender.cpp:594-595`).
2. **Character mask.** `gi_debug=2` writes 1 in the red channel where a pixel is inside a character capsule (`gi2.inl:335-336`). Only the Hi-Z path has this view.
3. **Off and on.** `gi_off` in `<game folder>\tyrender\`, or `[gi] Enable=0`. The difference should be soft bounce in shade only.
4. **Log lines** (`tyrender.log`): `[gi] Hi-Z initialised <w>x<h>, <n> pyramid levels` (`gi2.inl:388`), `[gi] Hi-Z path first run ok (rays=.. iters=.. atrous=..)` (`gi2.inl:513`), and every 600 frames `[gi] source mask: <n> marked draws this frame, <n> total, copy <0|1>` (`gi2.inl:428`). Failures: `[gi] Hi-Z disabled: glDrawBuffers missing` (`gi2.inl:353`), `[gi] Hi-Z framebuffers incomplete` (`gi2.inl:387`), `[gi] Hi-Z shaders failed` (`gi2.inl:362`), and `[fallback] Hi-Z GI unavailable -> GI OFF` (`gi.inl:130`). The second and third say "using legacy path"; see Discrepancies.
5. **Timers.** `[gi] Prof=1` logs `[gi] prof [...]: pyramid .. trace .. temporal .. atrous .. composite .. | total .. ms` every 300 samples (`gi2.inl:86-88`). The `[gpu]` line gives the GI pass as a whole.
6. **Source mask.** Set `[gi] ExcludeSources=0` and compare with the default around a water or character scene. This is the test for issue #5 in the slot-2 pond scene (`docs/ISSUES_SEED.md` #5). Use `tools/motion.ps1` (see [test harness](../guides/test-harness.md)) to measure the flicker on Ty.
7. **Force the fallback.** `[debug] ForceFallback=2` makes the Hi-Z programs fail. Expect GI off, no crash (`src/ao.inl:441`).

## NEEDS INVESTIGATION

- **NI-GIH-1 Water flicker on Ty: GI or not.** The sources disagree (see Known issues). Method: slot-2 pond scene, Ty still, with `[gi] Enable=0` and with `ExcludeSources=0` and `7`, then the `[pbr] DebugWater` bits (named as a switch in `docs/PLAYTEST_2026-10-17_FINDINGS.md`, issue 2). Measure the frame-to-frame change on Ty with `tools/motion.ps1`.
- **NI-GIH-2 Which pass tints the ground red (T84).** Method: close-up of Ty standing on sand at Ultra, High and Medium, with `gi_debug=1`, `gi_off`, then `[light]` and bloom toggled one at a time (`docs/agent-tasks/gi5.md` lists the steps). Not reproduced in this repo's text.
- **NI-GIH-3 High and Ultra flicker cause (T78).** Method: bisect `[gi] Enable`, `[atmos] ContactShadows` and `[gi] Rays` on High, at low fps (`sleepMs` in `motion.req`), walking Ty.
- **NI-GIH-4 Ghosting on grass.** Method: compare with TAA reduced or off and with GI off, in motion.
- **NI-GIH-5 Cost of the source mask and blur.** The v0.3.3 test build's +2.5 ms on High is not split (`docs/PERFORMANCE.md`). Method: `[gi] Prof=1` with the mask on and off (`ExcludeSources=0`), `[gpusub]` lines, or a whole-frame A/B.
- **NI-GIH-6 Whether the mask cost is caused by the extra draws.** Every eligible draw is drawn twice in the mask pass (`tyrender.cpp:269`, `:271`). Method: count the draws (`g_giMaskDraws` is logged at `gi2.inl:428`) and time the frame with and without the mask.
- **NI-GIH-7 Whether a 1-ray Hi-Z default is wanted (todo T10).** `docs/todo.json` T10 says "Hi-Z 1 ray as optional Ultra-only, default OFF in Balanced". The launcher gives High a 1-ray GI. Method: owner decision.

## Discrepancies

1. **Legacy fallback.** `src/gi2.inl:1` says Hi-Z "falls back to the legacy linear-march path in gi.inl on any failure". Code turns GI off (`src/gi.inl:130`) and skips the legacy block (`gi.inl:132`, `:139`). The log text at `src/gi2.inl:362` and `:387` says "using legacy path", which is misleading. Code wins. Policy in code and in `docs/todo.json` T44: failure gives OFF.
2. **`DebugView=3`.** `src/gi2.inl:33` says "Debug: DebugView=3 shows the mask". The composite handles only 1 and 2 (`src/gi2.inl:336`, `:341`). Value 3 gives the bounce term alone over the frame, because blending is off for any nonzero debug value (`gi2.inl:509`). Code wins.
3. **Capsule and mask.** `src/gi2.inl:33` says bit 4 "replaces the capsule heuristic". The capsule still tints hit colour (`gi2.inl:195`) and scales received light (`gi2.inl:335-337`). The mask only removes character draws as sources. Code wins.
4. **README "Characters take part in bounce light"** (`README.md` settings table, `[gi] Characters`). In code, characters are excluded as colour sources by default (mask bit 4, `tex.inl:39-47`), and they receive scaled GI (`CharacterGI`). Code wins; the README sentence is loose.
5. **High preset.** `docs/todo.json` T10 says "Ultra-only" for GI. `docs/PERFORMANCE.md` and `src/launcher_ui.inc:13` give High GI with 1 ray. The launcher is what ships. Code wins; the decision is NI-GIH-7.
6. **Water flicker source.** `docs/ISSUES_SEED.md` #5 says it happens on Medium with GI off. `docs/recording/FINDINGS_ROUND4.md` and `docs/PLAN_CONSOLIDATED.md` attribute it to GI sampling the water. Both can be true. Not settled here (NI-GIH-1).
7. **Cost figures.** `docs/results/VERDICTS.md` gives +4.68 ms (standard view), `docs/results/cost_table.md` +4.42 ms (over Balanced), `docs/PERFORMANCE.md` "about 4.4 ms". Different bases; all kept.

## See also

- [GI legacy: the screen-space linear march](gi-legacy.md)
- [AO: the other screen-space pass](ao-gtao.md)
- [Settings: AO, GI and LOD](../reference/settings-ao-gi-lod.md)
- [Debugging](../guides/debugging.md)
- [Known issues](../decisions/known-issues.md)

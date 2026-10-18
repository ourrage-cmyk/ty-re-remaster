# Ambient occlusion (GTAO v2, legacy GTAO v1)

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), B (tester who tunes `[ao]`).

## Purpose

Ambient occlusion (AO) darkens creases, contact lines and corners: the base of rocks and posts, the space under a ledge, the roots of grass. The renderer computes it in screen space from the engine's own depth copy, then multiplies the result into the frame. It is one of the few effects that the user sees at every preset above Low (Low sets `[ao] Enable=0`, Medium and up set it to 1, see [presets](../reference/presets.md)). The page covers the default v2 path (`kGTAO2` and its helpers) and the older v1 path (`kGTAO`), which only runs when `[ao] Legacy=1`.

## How it works

Pipeline per frame (AO runs inside the depth-copy hook, see [frame anatomy](../how-it-works/04-frame-anatomy-and-pass-order.md)):

```
engine depth copy (depth texture from glCopyTexImage2D)          tyrender.cpp:284-290
  |
  v  kPre0   full-res depth -> half-res linear view depth, mip 0  ao.inl:613-618
  v  kPreN   mips 1..4 of the same R32F texture                    ao.inl:620-627
  v  kGTAO2  horizon search, Slices x Steps, plus wide pass        ao.inl:631-651
  |          -> texA (half res, RG16F: visibility, view depth)
  v  kTemporal  reprojected history, texH ping-pong (half res)     ao.inl:666-684
  v  kBlur   depth-weighted 5x5 (BlurRadius) -> texB               ao.inl:688-690
  v  kComposite  bilateral upsample, power, fades, multi-bounce,
                 Strength; result multiplied into FB0              ao.inl:694-707
  v  colour is re-copied into the engine capture texture (bloom reads it)  tyrender.cpp:294-299
```

Steps in prose:

1. **Camera and positions.** The scene view-projection matrix is captured from `VPMatrix` uploads made while the default framebuffer is bound and the matrix is perspective (`TrackVP`, `ao.inl:58-68`). `RunAO` derives the camera position and view matrix from that matrix (`ao.inl:571-585`). If the VP was not captured in this frame, or no colour copy exists, AO is skipped for the frame (`ao.inl:567`).
2. **Depth mips.** `kPre0` converts the depth texture to linear view depth at half resolution. For each 2x2 quad it keeps the real sample closest to the quad mean when the quad straddles an edge, so no depth is invented at silhouettes (`ao.inl:196-205`). `kPreN` builds four more levels, each weighted toward the farther sample (`ao.inl:214-218`).
3. **Horizon search (`Occ`, `ao.inl:247-286`).** Each pixel uses `Slices` directions spread over half a turn, with a per-pixel rotation. For each direction it marches `Steps` samples on each side. The sample distance grows with the square of the step index (`ao.inl:263-267`), and coarse depth mips are read further out (`ao.inl:266`, `DepthMipOffset`). Each sample becomes a view-space vector. The search keeps the highest horizon angle on each side. The visibility for the slice is the cosine-weighted arc left open between those horizons along the projected normal. This is the GTAO idea of Jimenez et al. (2016), written here in our own words.
4. **Thin occluders.** A sample that lies below the current horizon does not snap the horizon down. The horizon drops by only a fraction `Thin` (`ao.inl:274-275`, `ao.inl:166-168` in v1). The stated aim is that grass blades, ropes and leaves do not act like solid walls (comment at `ao.inl:164-165`).
5. **Characters.** If `CharRadius > 0` and character draws were seen this frame, up to 8 capsules are passed to the shader (`ao.inl:633-642`). Each capsule runs from a base point to a point `CharHeight` units above it along world +Y (`ao.inl:639`, the view-space image of world up). A sample inside a capsule is skipped, so a character never occludes the ground behind it (`ao.inl:230-237`, `ao.inl:274-275`). The capsule base is the model-matrix origin of a skinned draw (`tyrender.cpp:84-89` and `:347`). (inferred: the origin is at the character's feet.)
6. **Wide pass.** When `WideStrength > 0`, a second horizon search runs on the coarse depth mips with radius `WideRadius` and `max(steps-2, 3)` steps (`ao.inl:311-314`). Its result multiplies the first. `WideSlices = 0` means "use `Slices`" (`ao.inl:511`, `ao.inl:650`). The README table agrees (`README.md` settings table).
7. **Distance LOD.** When `[lod] Enable=1` and `[lod] AO=1`, the number of steps per slice falls with view depth, with a smooth ramp between `Start` and `End` and a stochastic rounding per pixel (`ao.inl:308-309`). The code comment calls it issue #10.
8. **Noise.** The per-pixel sample pattern is a Hilbert index over 64x64 tiles, offset by the frame index, and the rotation comes from an R2 quasi-random sequence (`ao.inl:305-306`). The frame index advances every 8 frames and cycles over `NoiseFrames` (default 64) (`ao.inl:650`, and the 8-frame gate at `tyrender.cpp:594`). The pattern is stable within the temporal window, so the history can converge.
9. **Temporal (`kTemporal`, `ao.inl:320-376`).** Default `TemporalMode=1` (`ao.inl:513`):
   - the previous frame is looked up with last frame's VP (bilinear over four half-res texels);
   - a history texel is rejected if its camera distance differs from the expected value by more than 3 % plus 2 units (`ao.inl:340`);
   - the history is clamped to the 3x3 min/max of the current frame and to two standard deviations of a 5x5 window (`ao.inl:349-354`);
   - the blend weight is `max(1/(n+1), 1-TemporalBlend)` where `n` is the history length, capped at 32 (`ao.inl:356-357`).
   `TemporalMode=0` keeps the older nearest-texel method with a 3x3 clamp (`ao.inl:361-373`, described as the v0.3.1 method at `ao.inl:513`).
   Setting `[ao] Temporal=0` skips the pass entirely (`ao.inl:666`).
10. **Blur (`kBlur`, `ao.inl:378-393`).** A square kernel of radius `BlurRadius` (clamped to 1..5, `ao.inl:512`). Spatial weight falls off with `BlurFalloff` (`ao.inl:388`). The depth weight is a fixed Gaussian in relative depth difference (`ao.inl:387-388`); by arithmetic from the constant, the weight is e^-1 at a 5 % relative depth step (inferred). The README calls `BlurFalloff` a denoise setting; the code treats it as a spatial falloff, not a depth falloff (see Discrepancies).
11. **Composite (`kComposite`, `ao.inl:395-435`).**
    - bilateral 2x2 upsample from half-res to full-res using the same depth-weight form (`ao.inl:407-416`);
    - power curve `Power` (`ao.inl:417`);
    - optional distance fade to 1 between `FadeStart` and `FadeEnd` (`ao.inl:418`, off by default);
    - fog fade: AO acts only on the unfogged part of the pixel. The engine fog fraction and the atmosphere transmittance use the same formulas as the fog passes (`ao.inl:419-425`). `FogFadePower` is the exponent (`ao.inl:424`);
    - multi-bounce (Jimenez et al., 2016): a cubic in the AO value whose coefficients depend on an albedo estimate. The albedo estimate is the lit colour times `MultiBounce` (`ao.inl:428-430`). Bright surfaces therefore keep their colour in shade;
    - `Strength` mixes the result toward 1 (`ao.inl:431`);
    - the output is multiplied into FB0 with blend `ZERO, SRC_COLOR` (`ao.inl:707`).

**v1 path (`kGTAO`, `ao.inl:125-181`).** The same horizon idea at full resolution, with the engine depth texture read directly. It has no depth mips, no wide pass, no capsule mask, no distance LOD and no `DepthMipOffset`. It runs only when `[ao] Legacy=1` (`ao.inl:609` chooses v2 when `!g_aoLegacy`, else `ao.inl:653-661` runs v1). The temporal and composite passes are shared with v2.

**Fallback.** If the v2 programs fail to compile or link, and `Legacy=0`, AO switches off for the rest of the session (`ao.inl:546`, flag `ao.failed` at `ao.inl:534`, which nothing clears: inferred from a full search of the file). If `Legacy=1`, v1 runs instead (`ao.inl:547` logs this). See [fallbacks](../how-it-works/11-fallbacks-and-failure-handling.md).

**Shared shader pieces.** `kVS` makes a full-screen triangle from `gl_VertexID`, so no vertex buffer is needed (`ao.inl:95-98`). `kCommon` declares the depth sampler and the inverse-VP, view and size uniforms (`ao.inl:101-106`). `WorldPos` turns a UV and a depth sample into world space and maps depth with `ZMode` (0 = GL range, 1 = raw) (`ao.inl:107-112`). `ViewPos` moves that into view space (`ao.inl:113`). `ViewNormal` builds a normal from five taps and, on each axis, uses the neighbour with the smaller depth step, which rejects silhouettes (`ao.inl:114-122`). GI uses the same helpers ([gi-hiz](gi-hiz.md)).

**Textures.** `texA` and `texB` are half-res RG16F (`ao.inl:526` default format, `ao.inl:551`). `texH[0..1]` are half-res RGBA16F (`ao.inl:552`). `texD` is the linear-depth chain, R32F, half-res, with mips 1 to 4 (`ao.inl:553-557`).

## Code pointers

| What | File:line | Note |
|---|---|---|
| Hook that runs AO after the depth copy | `src/tyrender.cpp:284-290` | `RunAO` is called before `RunGI` |
| Debug views exclusive of GI, emitters, atmosphere | `src/tyrender.cpp:291-293` | `if (!g_aoDebug)` gates RunGI |
| Scene VP capture | `src/ao.inl:58-68` | `TrackVP`, perspective test at :63-64 |
| Shared shader helpers (`kVS`, `kCommon`) | `src/ao.inl:95-123` | `kCommon` is also used by GI (`gi.inl:133`) |
| v1 horizon shader (`kGTAO`) | `src/ao.inl:125-181` | full-res, no mips |
| `kPre0`, `kPreN` depth mips | `src/ao.inl:184-220` | R32F linear view depth |
| v2 horizon shader (`kGTAO2`) | `src/ao.inl:221-317` | `Occ` :247-286, `main` :287-316 |
| Capsule test | `src/ao.inl:230-237` | `CapHit` |
| Hilbert index and R2 noise | `src/ao.inl:238-246`, `:305-306` | |
| Distance LOD inside v2 | `src/ao.inl:308-309` | `u_lodS/E/Min` set at `ao.inl:648` |
| Wide pass | `src/ao.inl:311-314` | |
| Temporal shader (`kTemporal`) | `src/ao.inl:320-376` | `TemporalMode` at :329 and :361 |
| Blur (`kBlur`) | `src/ao.inl:378-393` | radius clamp at `ao.inl:512` |
| Composite (`kComposite`) | `src/ao.inl:395-435` | fog fade :419-425, multi-bounce :428-430 |
| Force-fail mechanism (bit 4) | `src/ao.inl:440` | `[debug] ForceFallback` |
| Config loader | `src/ao.inl:487-517` | every `[ao]` key, defaults in code |
| Flag files | `src/ao.inl:518-524` | `ao_off`, `ao_debug`, `ao_reload` |
| Resource setup and fallback | `src/ao.inl:533-562` | |
| Frame driver | `src/ao.inl:566-724` | `RunAO` |
| Multiply blend and experiment bits | `src/ao.inl:565`, `:707-708` | see Settings, `Exp` |
| AO fog helper | `src/gpuprof.inl` (`AtmosFogParams`) | defined there, used at `ao.inl:700` |

## Settings

Full table with every key lives in [settings-ao-gi-lod](../reference/settings-ao-gi-lod.md). Keys that matter for AO:

| Key | Code default (file:line) | Shipped `default_tyrender.ini` | Live or restart | Effect |
|---|---|---|---|---|
| `[ao] Enable` | 1 (`ao.inl:492`) | 1 (`:3`) | live | master switch |
| `[ao] Legacy` | 0 (`ao.inl:506`) | 0 (`:4`) | live | 1 = v1 path |
| `[ao] Slices` | 3 (`ao.inl:502`) | 3 (`:5`) | live | horizon directions per pixel |
| `[ao] Steps` | 6 (`ao.inl:502`) | 6 (`:6`) | live | samples per direction and side |
| `[ao] Radius` | 60 (`ao.inl:495`) | not set | live | search radius, world units |
| `[ao] Strength` | 0.85 (`ao.inl:500`) | not set | live | mix toward 1 |
| `[ao] Power` | 1.0 (`ao.inl:501`) | not set | live | curve |
| `[ao] Thin` | 0.5 (`ao.inl:496`) | not set | live | thin-occluder horizon drop |
| `[ao] TemporalBlend` | 0.92 (`ao.inl:497`) | not set | live | history weight floor is 1 - this |
| `[ao] TemporalMode` | 1 (`ao.inl:513`) | not set | live | 0 = nearest, 1 = bilinear |
| `[ao] Temporal` | 1 (`ao.inl:499`) | not set | live | 0 skips the temporal pass |
| `[ao] MultiBounce` | 0.6 (`ao.inl:498`) | not set | live | albedo scale for multi-bounce |
| `[ao] WideRadius` / `WideStrength` / `WideSlices` | 400 / 1.0 / 0 (`ao.inl:493-494`, `:511`) | not set | live | wide pass; WideSlices 0 = Slices |
| `[ao] FadeStart` / `FadeEnd` | 0 / 0 (`ao.inl:507-508`) | not set | live | distance fade, off when FadeEnd is 0 |
| `[ao] CharRadius` / `CharHeight` | 40 / 120 (`ao.inl:509-510`) | not set | live | character capsule; CharRadius 0 = off |
| `[ao] FogFade` / `FogFadePower` | 1.0 / 2.0 (`ao.inl:514`) | not set | live | fog fade strength and curve |
| `[ao] BlurRadius` / `BlurFalloff` | 2 / 0.15 (`ao.inl:512`) | not set | live | blur radius (clamped 1..5) and spatial falloff |
| `[ao] NoiseFrames` | 64 (`ao.inl:512`) | not set | live | length of the noise cycle |
| `[ao] DepthMipOffset` | 3.15 (`ao.inl:505`) | not set | live | mip bias for far samples |
| `[ao] ZMode` | 0 (`ao.inl:503`) | not set | live | 0 = GL depth range, 1 = raw depth |
| `[ao] DebugView` | 0 (`ao.inl:515`) | not set | live | 1 AO only, 2 normals, 3 depth bands |
| `[ao] Exp` | 0 (`ao.inl:504`) | not set | live | undocumented experiment bits, see below |
| `[lod] AO` (with `[lod] Enable`) | 0 (`ao.inl:480-481`) | 0 (`default_tyrender.ini` `[lod]`) | live | distance LOD for horizon steps |

"Live" means `LoadAOConfig` runs again on the reload trigger (`tyrender.cpp:598`), on the `ao_reload` flag (`ao.inl:522-523`), and from the overlay (`overlay.inl:54`). Caveat: if AO was switched off by a failure, toggling keys does not bring it back until the game restarts (`ao.inl:534`, inferred).

`[ao] Exp` is not documented anywhere else in the repo. It is a bit mask read at `ao.inl:504`. Bit 1 skips the colour re-copy (`tyrender.cpp:294`), bit 2 skips the AO composite draw (`ao.inl:708`), bit 8 skips every AO draw (`ao.inl:565`). The purpose is inferred to be A/B experiments. Users should leave it at 0.

**Preset values** (for context): the launcher table sets Medium to `Slices=3, Steps=6`, High the same, Ultra to `4 x 8` (`src/launcher_ui.inc:11-14`). `tools/presets.ps1:15-17` sets Medium to `2 x 4`. That conflicts with the launcher, see Discrepancies.

**Launcher slider** "Crevice shadows" maps to `[ao] Strength` with a launcher default of 0.60 (`src/launcher_ui.inc:21`). Reset writes that default (`launcher_ui.inc:502`). The code default is 0.85. See Discrepancies.

## Cost

All figures are whole-frame deltas against the stock frame, measured in-session A/B at the default camera, 2560x1440. The GPU is not named in `docs/results/VERDICTS.md`; the only GPU named in `docs/PERFORMANCE.md` is an AMD Radeon RX 7600 XT, so the machine is (inferred) the same.

| Setting | Delta | Source |
|---|---|---|
| AO v2, 3 x 6 | +1.42 ms (stock 5.38 ms) | `docs/results/VERDICTS.md` "Standard-view numbers" |
| AO v1 (`Legacy=1`) | +5.76 ms | same table |
| AO off, Balanced preset base 11.09 ms | -1.15 ms | `docs/results/cost_table.md` |
| AO pass alone (GPU timer), Medium / High / Ultra | 0.51 / 0.57 / 0.66 ms | `docs/results/preset_matrix.md` |
| Launcher estimate for AO | 0.5 + 0.051 x slices x steps (3 x 6 gives 1.42) | `src/launcher_ui.inc:48` (an estimate, not a measurement) |

The per-pass timers under-count. `docs/results/VERDICTS.md` and `docs/PERFORMANCE.md` both say to use whole-frame deltas. The v2 path was about 4 times cheaper than v1 in the same session.

Not measured: the distance LOD for AO (`[lod] AO`). See N-20 in [NEEDS INVESTIGATION](#needs-investigation).

## Limits

- **Screen space only.** Occluders that are off screen, or hidden from the depth buffer, do not contribute. The renderer has no world-space geometry data to use instead ([constraints](../concepts/constraints-and-limits.md)).
- **Radius in pixels is capped.** The search radius in pixels is limited to 25 % of screen height (`ao.inl:303`). Pixels with a smaller radius than 0.75 half-res pixels get no AO (`ao.inl:304`).
- **Temporal reuse.** The history is reprojected with last frame's camera. Moving objects and camera cuts are rejected or smeared (`ao.inl:340`, `:356`).
- **Characters are a capsule.** The mask is a rough capsule per character origin, not a silhouette (`ao.inl:230-237`).
- **Thin objects are a heuristic.** `Thin` only softens the horizon drop. It does not model the geometry.
- **Fog fade uses the engine fog and the atmosphere formulas.** It is not a separate model (`ao.inl:419-425`).
- **Half resolution.** The search and the blur run at half resolution. Fine detail is recovered only by the bilateral upsample.

## Known issues

Issue numbers are from `CHANGELOG.md` and `docs/ISSUES_SEED.md`. Status sources disagree; see the NEEDS INVESTIGATION list.

- **#1 AO flicker, grain, dark cloud behind Ty, shimmer at distance.** This is the main open AO item.
  - `CHANGELOG.md` (v0.3.5, Fixed) says "AO flicker and grain gone (#1, #6)".
  - `docs/PLAN_CONSOLIDATED.md` (playtest of 2026-10-17) says Ultra and High "show AO flicker", that this "confirms #6", and that #1 is "back"; Medium shows grainy AO under the character's feet.
  - `README.md` says the AO flicker "has since been fixed" and that Ultra on build `4a2ef95` "played without it".
  - `docs/todo.json`: T73 and T90 (AO fade, character mask, history clamp, blue noise) are still "open".
  - Attempts so far: temporal mode 1 with variance clamp (`ao.inl:329-357`); character capsule skip (`ao.inl:274-275`); Hilbert/R2 noise on an 8-frame cycle (`ao.inl:305-306`, `:650`); fog fade (`ao.inl:419-425`).
- **#6 High and Ultra flicker.** `docs/recording/FINDINGS_ROUND4.md` (playtest 4 of v0.3.2) says Medium is stable and High and Ultra flicker; the suspects are GI and contact shadows. The AO part is not isolated there. See [gi-hiz](gi-hiz.md) for the GI side.
- **Dark smear at grazing angles (fixed).** A v2 defect at the close grazing view `sand_graze` gave a large dark smear at the bottom of the frame (MAD 26.0 against off). A depth-mip level bug was fixed; MAD became 1.52 (`docs/results/VERDICTS.md` "ao: GO"; `docs/agent-tasks/ao.md` update).
- **Thin occluders and motion shimmer (open).** `docs/results/VERDICTS.md` says fence, rope and grass were not re-checked after the merge, and the motion metric (T47) is open.
- **Distance fade is off by default.** The plan asked for FadeStart 1500 and FadeEnd 3500 (`docs/agent-tasks/ao6.md`, `docs/PLAN_EXEC.md` B1). The code default is 0 and 0 (`ao.inl:507-508`), so the fade is off unless the user sets it.
- **Fern shadow white rim (T73 item 4).** Not addressed in the code this page reads. Status: NEEDS INVESTIGATION.
- **Trees darker in thick fog on the second level.** Listed under "Known" in `CHANGELOG.md` (v0.3.5). FogFadePower was the change for this.
- **Legacy path.** Slower by about 4 times and "not recommended" in `README.md` (`[ao] Legacy`).

## How to verify

1. **AO only.** Write `1` to `<game folder>\tyrender\ao_debug`, or set `[ao] DebugView=1`. The value is polled every 8 frames (`ao.inl:521`, `tyrender.cpp:594`). Expect a grey image with dark creases. Write `2` to see normals (`ao.inl:299`), and `3` for depth bands (`ao.inl:300`). Debug views turn off GI, emitters and atmosphere for the frame (`tyrender.cpp:291-293`).
2. **Off and on.** Write `ao_off` in `<game folder>\tyrender\` to switch AO off, or set `[ao] Enable=0`. The difference should be creases and corners only. For a proper pair, use F11 A/B ([ab captures](../guides/ab-captures.md)).
3. **Log lines** (`tyrender.log`). Expect `[ao] initialised <w>x<h> (half <w/2>x<h/2>)` (`ao.inl:560`), `[ao] config: debugview=...` (`ao.inl:516`), and `[ao] first run ok: cam=...` (`ao.inl:723`). A failure shows `[fallback] AO v2 unavailable -> AO OFF` (`ao.inl:546`) or `[ao] ... compile FAILED` (`ao.inl:450`). The forced-failure test prints `[fallback] FORCED compile failure of AO v2 program` (`ao.inl:440`).
4. **Timers.** `[gpu]` prints the `ao` column (`gpuprof.inl`, summary line). `[gpusub]` prints `sub0` to `sub4`. The mapping is inferred from the marks: `sub0` = depth mips (marks 10-11), `sub1` = temporal and blur (marks 12-13), `sub2` = composite (marks 14-15), `sub3` = horizon search (marks 16-17), `sub4` = colour re-copy (marks 18-19). The v1 path only sets marks 10-11 and 12.
5. **Temporal stability.** Use `tools/motion.ps1` (described in [test harness](../guides/test-harness.md)). Compare the temporal noise of AO on and off, and look at the heat maps. Do not rely on a single still.

## NEEDS INVESTIGATION

- **NI-AO-1 Current AO flicker status.** The sources disagree (see Known issues). Method: play build `4a2ef95` or later on Ultra and High with `tools/motion.ps1` at low fps (`sleepMs` in `motion.req`) and compare the flicker metric with AO on and off. Confirm which build the 2026-10-17 playtest used.
- **NI-AO-2 Character halo and dark patch behind Ty.** Method: a close-up of Ty in front of a light wall at Ultra, `ao_debug=1`, compare with `CharRadius=0`. T73 and T90 are open.
- **NI-AO-3 Fern shadow white rim.** Method: a crop of the fern shadows (`docs/ab` naming in [ab captures](../guides/ab-captures.md)) with AO on and off.
- **NI-AO-4 Why v1 is kept.** The repo does not state a reason beyond "kept as fallback" in `docs/results/VERDICTS.md` and "kept via `[ao] Legacy=1`" in `docs/agent-tasks/ao.md`. The code does not fall back to it automatically (`ao.inl:546-547`). Method: ask the owner.
- **NI-AO-5 Cost of the distance LOD.** Method: `tools/perf_matrix.ps1` or `g1_gate.ps1` with `[lod] Enable=1 AO=1`, then compare with the default (N-20 in the master plan).
- **NI-AO-6 Temporal shimmer under motion (T47).** Method: `tools/motion.ps1` with translation steps, on Medium and Ultra.

## Discrepancies

Each item lists the sources, the code value and the verdict. Code wins.

1. **`[ao] Radius`.** `src/ao.inl:6` (header) says `Radius=1.5 Strength=1.0 Power=1.5`. Code default is 60 (`ao.inl:495`), Strength 0.85 (`ao.inl:500`), Power 1.0 (`ao.inl:501`). README table agrees with the code (`README.md` line 276). **Resolved in favour of the code.** The header is stale. This is N-26 in the master plan.
2. **`[ao] TemporalBlend`.** README line 279 says 0.85. Code default is 0.92 (`ao.inl:497`). Code wins.
3. **`[ao] WideSlices`.** The first-pass `SHADERS.md` said "0 = off". Code: 0 means "same as Slices" (`ao.inl:511`, `:650`). README line 277 agrees with the code.
4. **`[ao] BlurFalloff`.** First-pass `SHADERS.md` and README call it a depth falloff. Code uses it as the spatial falloff (`ao.inl:388`); the depth weight is fixed (`ao.inl:387-388`).
5. **`[lod]` effect on AO.** First-pass `SHADERS.md` says "AO strength ramps". Code reduces the number of horizon steps per slice, not the strength (`ao.inl:308-309`).
6. **Character capsule source.** First-pass `SHADERS.md` says the AO capsule list is "shared with GI". AO builds its own capsule uniform from `g_chars` (`ao.inl:633-642`); GI uses its own uniform (`gi2.inl:406-409`). They share the source array, not the uniform.
7. **Character halo status.** First-pass `SHADERS.md` lists the dark patch behind the capsule as a known issue. The capsule skip (`ao.inl:274-275`) is in the code now. The status is NI-AO-2.
8. **Medium AO slices.** `tools/presets.ps1:15` gives Medium `aoSlices=2, aoSteps=4`. Launcher (`src/launcher_ui.inc:12`) and `docs/PERFORMANCE.md` give 3 x 6. Launcher and PERFORMANCE agree with the code default. The tool file is stale or for a different experiment.
9. **Distance fade defaults.** `docs/agent-tasks/ao6.md` and `docs/PLAN_EXEC.md` say FadeStart 1500 and FadeEnd 3500 are the defaults. Code is 0 and 0 (`ao.inl:507-508`). Code wins; the planned default is not in the code.
10. **Launcher AO strength.** The launcher slider default is 0.60 (`src/launcher_ui.inc:21`). The launcher shows 0.60 whenever the key is missing from the ini (`launcher_ui.inc:186`), and Reset writes 0.60 into the ini (`:502`). The code default is 0.85 (`ao.inl:500`). A user who never touches the slider and has no `Strength` key therefore sees 0.60 in the launcher and gets 0.85 in game.
11. **AO flicker status.** See NI-AO-1.
12. **AO cost.** `README.md` line 171 says "saves about 1.15 ms" (Balanced cost table) and `docs/results/VERDICTS.md` says +1.42 ms (v2 on stock 5.38 ms). The two numbers come from different base presets. Both are kept with their sources.

## See also

- [GI Hi-Z: screen-space bounce light](gi-hiz.md) and [GI legacy](gi-legacy.md) (the other screen-space pass)
- [Shader inventory](README.md) and [distance LOD](distance-lod.md)
- [Frame anatomy and pass order](../how-it-works/04-frame-anatomy-and-pass-order.md)
- [Settings: AO, GI and LOD](../reference/settings-ao-gi-lod.md)
- [Debugging](../guides/debugging.md) and [known issues](../decisions/known-issues.md)

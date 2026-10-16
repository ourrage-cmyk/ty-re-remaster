# Minecraft shaderpack → TY renderer: ranked port candidates

Source: Complementary Reimagined r5.9.3 (`RESEARCH\shaders\minecraft\ComplementaryReimagined_r5.9.3.zip`), read from an extracted copy in the scratchpad. Target: tyrender (32-bit OpenGL, RX 7600 XT, 2560x1440).

Budget context (from `RESEARCH\GOAL.md` and `STATUS_2026-10-13.md`): Medium measures **12.08 ms**, budget **16.7 ms**, so about **4.6 ms** of headroom. Current measured costs: 32-tap shadows +5.12 ms, GI Hi-Z +4.42 ms, contact shadows +1.71 ms, AO off saves 1.15 ms, atmos off saves 2.09 ms.

All cost figures below marked **est.** are my estimates from reading the code. None are measured. Each needs a GPU-timer run before it goes into a preset.

## Licence: read first

`License.txt` (Complementary License Agreement 1.7) defines "Normal Usage" as use of the pack, and treats redistributing parts of it or modifying its code as outside that (§0.2). §2.1 restricts any use or redistribution of any part of the pack that is not Normal Usage. I did not do a legal review.

**Practical rule for this doc:** the ideas below describe techniques only. Re-implement from the described behaviour in our own code. Do not paste or adapt GLSL from the pack into `tyrender`, and do not ship any of its files. Check with the user before any port that is more than an idea.

## Ranking (value vs cost)

| # | Technique (pack source) | What it gives TY | Cost (est.) | Verdict |
|---|---|---|---|---|
| 1 | **Shadow-edge bias: normal offset and penumbra tweak** (`lib/lighting/mainLighting.glsl` ~L150–200) | Targets the open "jagged bright lines on Ty's face and bag shadows" and light leaks. Uses screen derivatives and a shadow-length fade. | ~0.05 ms (a few ALU and `dFdx`/`dFdy`) | **Do first.** Only the normal-offset and slope-bias parts transfer. The block-grid `floor()` snapping is voxel-specific; do not port it. The penumbra tweak keys off per-vertex AO (`glColor.a`), which TY's terrain may not have. Check that before relying on it. |
| 2 | **Tonemap: dark lift, white-path compression, dark desaturation** (`program/composite5.glsl` `DoCompTonemap`, ~L36–84) | More natural lighting: lifts shadow detail without greying blacks, and tames highlights without hard clipping. Directly serves the "lighting more natural" goal. | <0.1 ms (folds into the existing grade pass) | **Cheap win.** A/B against the current filmic curve and vibrance in full-size screenshots from the same camera. Keep whichever reads better on the sand, fence and rope references. |
| 3 | **TAA history filter: Catmull-Rom history sample, AABB clip, velocity-weighted blend** (`lib/antialiasing/taa.glsl` ~L31–131, ~L198–245) | Less ghosting and shimmer. Targets the open Ultra distant-shadow and AO shimmer and the "little ghosting" note. | ~0.2–0.4 ms (5 history taps instead of 1) | **Do if ghosting survives playtest.** Measure before adopting, because the TAA cost is not separately listed in the cost table. |
| 4 | **Few-tap jittered shadows with TAA accumulation** (`lib/lighting/shadowSampling.glsl`, `SampleTAAFilteredShadow`: 2N taps, N = 2–16 by quality, rotated disk with per-frame noise) | Soft penumbrae from a small tap count, with TAA doing the averaging. Compares directly with the current 32-tap grid (+5.12 ms). | ~1 ms est. at 4–8 taps vs 32 | **Prototype as a tier option, not the default.** It reintroduces per-pixel temporal noise, which goes against the "no per-pixel noise" design choice, and it depends on TAA being clean (see #3). Test for shimmer before adopting. |
| 5 | **Low-tier AO: 4-sample depth comparison** (`program/deferred1.glsl` `GetAmbientOcclusion`, ~L58–100, `SSAO_QUALI == 2`) | Gives the Low preset some AO, which it has none of now. | ~0.2–0.4 ms est. | **Optional.** Lower quality than the current XeGTAO-style AO, so use only for Low. |
| 6 | **Bloom pyramid** (`program/composite4.glsl` 7 `BloomTile` mips; `composite5.glsl` `DoBloom`) | Glow on sun, water and light-source highlights. Not in the renderer's feature list. | ~0.3–0.6 ms est. at half res | **Optional, off by default.** Not in the goal's list. |

## Skip

- **Per-pixel volumetric lightshafts** (`lib/atmospherics/volumetricLight/volumetricLight.glsl`). Raymarches each pixel with an adaptive near/far sample split. TY's froxel haze already covers the same effect more cheaply. Porting would add cost.
- **Voxel coloured block light / ACT** (`lib/voxelization/lightVoxelization.glsl`, `program/shadowcomp.glsl`). Needs a compute pass, a 3D volume and light propagation every frame. TY has few coloured block lights, and the existing bounce-light pass covers indirect light.
- **SSR and world-space reflections** (`lib/materials/materialMethods/reflections.glsl`, 30–38 march steps; `reflectionVoxelization.glsl`). Value is mostly water for TY, and the cost is 1–3 ms or more. Revisit only for the water-pulse issue, and only with a cheaper method.
- **Cloud shadows** (`lib/lighting/cloudShadows.glsl`). TY has no cloud layer that moves over scenes in a way worth this cost.
- **DOF, lens flare, chromatic aberration, puddles, rain.** Outside the goal's acceptance criteria.

## Suggested order

1. #2 tonemap (near-free, quick A/B).
2. #1 shadow-edge bias (targets an open defect, almost no cost). Confirm the terrain attribute check first.
3. #3 TAA filter, only if playtest still shows ghosting after #1 and #2.
4. #4 shadow prototype, behind a tier flag, to compare against the 32-tap grid.
5. #5 and #6 only if there is time and budget left.

Projected total for #1–#3: under about 0.5 ms (est.), well inside the 4.6 ms headroom at Medium. All of these numbers need GPU-timer confirmation.

## What I read

- `program/`: `composite*.glsl`, `deferred1.glsl`, `shadow.glsl`, `shadowcomp.glsl`, `final.glsl`
- `lib/lighting/`: `shadowSampling.glsl`, `mainLighting.glsl` (partial)
- `lib/atmospherics/volumetricLight/volumetricLight.glsl` (partial), `lib/antialiasing/taa.glsl` (partial), `lib/materials/materialMethods/reflections.glsl` (partial), `lib/voxelization/lightVoxelization.glsl` (partial)
- `shaders.properties` profiles, `lib/common.glsl` shadow-map sizes, `License.txt`

Not read in full: most `gbuffers_*` material paths, `lib/materials/`, `lib/atmospherics/clouds/`, and the water shaders. Those were skipped on purpose; a deeper pass on water would need them.

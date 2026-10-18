# Froxel volume (kAtmosCS, compute path and fallback)

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), B (tester).

## Purpose

The froxel volume is the default atmosphere path. A compute shader fills a small 3D texture once per frame with the haze accumulated along each screen column, split into 64 depth slices. The composite then needs one trilinear lookup per pixel instead of a march. The path needs OpenGL 4.3 compute. When compute is missing, or the setup fails, the pass uses the ray march from [atmosphere-fog.md](atmosphere-fog.md) if contact shadows are on, or switches atmosphere off if they are not.

## How it works

1. **One-time check (`InitVolume`, `src/atmos.inl:254-272`).** The function runs once (`vol.tried`, `:255`). It returns false, with a log line, when the forced-failure bit is set (`:257`), when the GL version string is below 4.3 (`:258-260`), when a compute entry point is missing (`:261-264`), or when the compute shader fails to compile or link (`:265-270`). Compile errors are logged with the info log and reported through `SessFailure` (`:267`).
2. **Volume texture (`EnsureVolTex`, `src/atmos.inl:273-284`).** The grid is `ceil(width / FroxelPixels)` by `ceil(height / FroxelPixels)` by 64 slices, stored as RGBA16F with linear filtering and clamp-to-edge. `FroxelPixels` is clamped to 4 to 64 (`:304`) with a default of 12. The texture is reallocated when the size changes (`:275`). At 2560x1440 the grid is 214 by 120 by 64, about 1.6 million texels (inferred from the formula).
3. **Slice distribution (`DistOf`, `src/atmos.inl:183`).** Slices are exponentially spaced: `d(s) = MaxDist * (exp(5s) - 1) / (exp(5) - 1)`. The 5 is `volA`, hard-coded at `src/atmos.inl:458`. At the default `MaxDist` of 40000, the first slice ends about 22 units from the camera and the last slice covers about 3000 units (inferred arithmetic). The composite maps distance back to the slice coordinate with the inverse log formula (`src/atmos.inl:82`).
4. **Dispatch.** The shader has local size (1, 1, 64), so each work group is one screen column and each invocation is one slice (`src/atmos.inl:161`). The dispatch is `(fw, fh, 1)` (`src/atmos.inl:469`).
5. **Per-slice sample.** Each invocation takes up to two samples at the slice's inner and outer depth. Each sample has density from the same height falloff as the ray march, sun visibility from the cascade maps (`SunVis`, `:175-182`), and a phase term with the same Henyey-Greenstein form (`:191`). The ambient term comes from `AmbDyn`, which blends the CPU haze colour with the sky estimate when `HazeSky > 0` (`:169-174`). See the sky page. The slice stores its in-scatter and its extinction (`:185-208`).
6. **Serial prefix.** Invocation 0 integrates the 64 slices front to back, writing the running in-scatter and transmittance into a shared array (`src/atmos.inl:210-213`). A barrier separates this from the store (`:209`, `:214`).
7. **Store.** Each invocation writes its slice with `imageStore` (`src/atmos.inl:215`). The texture is then bound for the composite (`:468-472`) with a memory barrier between the two passes (`:470`).
8. **Distance LOD.** When `[lod] Enable=1` and `Fog=1`, slices beyond `Start` take one sample, with stochastic rounding per froxel so the mean is kept (`src/atmos.inl:193-198`). See [distance-lod.md](distance-lod.md).
9. **Composite lookup.** The composite maps the pixel's distance to a slice coordinate and reads the volume at `(uv, s)` (`src/atmos.inl:79-84`). A pixel nearer than the first slice blends toward zero transmittance (`:84`).

```
screen grid (FroxelPixels x FroxelPixels blocks)      depth slices (64, exponential)
+----+----+----+                                       s=63  ... far (about MaxDist)
| c0 | c1 | c2 |  one work group per column             ...
+----+----+----+  each invocation k = one slice        s=1
                                                       s=0   near (about 22 units)
prefix sum over k in invocation 0 -> volume(x, y, k) = (inscatter, transmittance)
composite: volume(uv, slice(distance)) -> colour * T + inscatter
```

## Code pointers

| What | File:line | Note |
|---|---|---|
| Compute shader `kAtmosCS` | `src/atmos.inl:160-217` | `#version 430`, local size (1, 1, 64) |
| Per-slice sample | `src/atmos.inl:185-208` | two samples, LOD thinning at `:193-197` |
| Serial prefix | `src/atmos.inl:210-213` | invocation 0 only |
| Slice distribution | `src/atmos.inl:183` | `DistOf`, volA at `:458` |
| Sun visibility in compute | `src/atmos.inl:175-182` | `texture()`, not `textureLod()` (see Limits) |
| Ambient colour | `src/atmos.inl:169-174` | `AmbDyn`, uses the sky estimate when on |
| Setup and checks | `src/atmos.inl:254-272` | `InitVolume` |
| Volume allocation | `src/atmos.inl:273-284` | `EnsureVolTex` |
| Config and clamp | `src/atmos.inl:304` | `FroxelPixels` 4 to 64 |
| Dispatch | `src/atmos.inl:453-472` | uniforms, dispatch, barrier |
| Composite lookup | `src/atmos.inl:79-84` | in mode 3 |
| Fallback decision | `src/atmos.inl:371`, `:453`, `:481` | see How it works 9 |
| Forced failure | `src/atmos.inl:257` | `[debug] ForceFallback` bit 1 |

## Settings

Full tables are in [settings-atmos-fog-emit.md](../reference/settings-atmos-fog-emit.md). "Live" means the key is re-read by the `reload` trigger.

| Key | Default in code | Shipped Medium | Live or restart | Effect |
|---|---|---|---|---|
| `[atmos] Froxel` | 1 (`src/atmos.inl:304`) | 1 (`default_tyrender.ini:33`) | live (checked each frame) | 0 skips the compute path and uses the ray march |
| `[atmos] FroxelPixels` | 12 (`src/atmos.inl:304`) | not set | live (reallocates the volume) | screen block size per froxel column. Smaller = finer, more memory and work |
| `[atmos] Steps` | 24 (`src/atmos.inl:292`) | 24 (`:34`) | live | ray-march path only. Not read by `kAtmosCS` |
| `[atmos] HalfRes` | 1 (`src/atmos.inl:292`) | not set | live | ray-march path only |
| `[lod] Fog` | 0 (`src/ao.inl:481`) | 0 (`default_tyrender.ini:24`) | live | thins far slices. Compute path only |
| `[debug] ForceFallback` bit 1 | 0 (`src/ao.inl:491`) | not set | restart | forces `InitVolume` to fail |

## Cost

- **Froxel against ray march, early table.** `docs/results/VERDICTS.md`, first table (2026-10-13, stock 4.76 ms): froxel +8.2 ms (pass 3.9 ms), ray march +8.8 ms (pass 4.3 ms), with about 4 ms of each cost outside the pass. The same file's correction says these absolute numbers were sampled at the last view and are inflated, so use only the comparison, and the corrected numbers in [atmosphere-fog.md](atmosphere-fog.md) for totals.
- **Agent note, atmosphere only, default view.** `docs/agent-tasks/atmos.md` records the froxel path at 1.68 ms against 3.82 ms for the old ray march. This is an agent note, not a result file in `docs/results/`, so treat it as a single run.
- **Scaling (inferred from the code).** Work is `(width/FroxelPixels) * (height/FroxelPixels) * 64 * 2` samples per frame, plus the serial prefix of 64 steps in one invocation. Changing `Steps` does not change it.
- No GPU timer output for the compute dispatch is recorded in the repository. The per-pass number in `docs/results/preset_matrix.md` (atmosphere pass 1.01 ms at Medium) covers the composite too.

## Limits

- **Needs GL 4.3 compute.** The check reads the version string (`src/atmos.inl:258-260`). A driver that reports 4.3 and lacks a feature would fail at compile or link, which is handled, but it is not detected in advance (inferred).
- **Coarse grid, no depth-aware filter.** The composite reads the volume with linear filtering and no depth test (`src/atmos.inl:83`). Haze can therefore bleed across silhouettes where the grid cell straddles an edge (inferred; not seen in a capture yet).
- **Two samples per slice.** Thin occluders between samples are not represented in the in-scatter (inferred).
- **Serial prefix.** Invocation 0 runs 64 dependent steps. It is cheap, but it limits how well the dispatch overlaps (inferred).
- **Implicit LOD in compute.** `SunVis` in the compute shader calls `texture()` on the shadow sampler (`src/atmos.inl:178-180`). The fragment shader uses `textureLod(..., 0.0)` for the same lookup (`:35`). Compute shaders have no screen-space derivatives, so the LOD used by `texture()` is not set by the shader (inferred; see NEEDS INVESTIGATION).
- **Not the same as the ray march.** The two paths differ in look. `docs/results/VERDICTS.md` notes that the ray march showed "posterised blotches on far rock" in the test views.

## Known issues

- **Fallback look (no issue number).** The ray march lacks the composite-only features (see Limits on the fog page), so it changes the colour of far rock and the sky. This is visible in the VERDICTS comparison.
- **Atmosphere switched off on compute failure.** When `ContactShadows` is 0 and the froxel path cannot be set up, the pass turns off and logs `atmosphere turned off: froxel unavailable` through `SessEvent` (`src/atmos.inl:371`). This is intentional according to the comment on that line ("failure -> OFF, never a slower path"), but see the discrepancy below.
- **#3 and #4 interact with the froxel.** The froxel carries the sun term and the ambient term. Whether the yaw-dependent hue comes from the sun term, the ambient term or the GI feed is listed as a suspicion in `docs/PLAN_CONSOLIDATED.md:48`, not as a confirmed cause.

## How to verify

1. **Path ready.** The first frames should log `[atmos] froxel compute path ready` and `[atmos] froxel volume WxHx64` (`src/atmos.inl:271`, `:283`). The `W` and `H` should be the screen size divided by `FroxelPixels`, rounded up.
2. **Forced failure.** Set `[debug] ForceFallback=1` in `tyrender.ini` and restart. Expect `[fallback] FORCED froxel failure -> ray-march path` (`src/atmos.inl:257`) and the ray-march look. Set it back to 0 and restart.
3. **Old GL.** On a context below 4.3 the log shows `[atmos] froxel: GL <version> lacks compute, using ray-march path` (`src/atmos.inl:260`).
4. **Compile failure.** A failed compile logs `[atmos] froxel compute compile FAILED:` followed by the info log (`src/atmos.inl:267`).
5. **Grid size.** Change `FroxelPixels` from 12 to 4 and reload. The `[atmos] froxel volume` line should show about nine times as many texels. The volume is reallocated on the next frame (`src/atmos.inl:275`).
6. **Sun term.** With `tyrender\atmos_debug` set to 2, the composite shows sun visibility at the surface (`src/atmos.inl:86`). Compare it with the engine's shadows.

## NEEDS INVESTIGATION

- **NI, compute shadow LOD.** Does `texture()` on the shadow sampler in `kAtmosCS` (`src/atmos.inl:178-180`) give a sensible result on each driver? Method: change the two lookups to `textureLod(..., 0.0)` in a test build, capture the same view with `[debug] CaptureFrame` (see [../how-it-works/10-logging-timers-and-session-report.md](../how-it-works/10-logging-timers-and-session-report.md)), and compare the shadow edges in the sun-visibility view.
- **NI, silhouette bleed.** Is there a visible halo of haze at tree and rock edges in the froxel path? Method: crop an edge at 100 per cent with the froxel and with `ForceFallback=1`, and compare.
- **NI, other GPUs and drivers.** The only tested GPU is the RX 7600 XT (`docs/PERFORMANCE.md`). Method: see N-21 in [../decisions/known-issues.md](../decisions/known-issues.md) and community logs (`[atmos] froxel compute path ready` on each user's log).
- **NI, which fallback is intended (owner decision).** The code comment and `docs/PERFORMANCE.md` say the renderer never falls back to a slower path, while the code falls back to the ray march whenever contact shadows are on. Method: owner decision; the answer decides whether `src/atmos.inl:371` or the fallback code is the one to change.

## Discrepancies

- `src/atmos.inl:371` comment says "failure -> OFF, never a slower path". The condition on that line is `g_atContact <= 0`. With `ContactShadows` above 0, a failed froxel falls to the ray march (`src/atmos.inl:453`, `:481`). The same claim appears in `docs/PERFORMANCE.md` ("Fallback behaviour") and `README.md:265`. The VERDICTS file (line 44) records the froxel-to-ray-march fallback as validated.
- The first-pass `SHADERS.md` (section 4.2) says "If the volume fails, atmosphere is switched off (`src/atmos.inl:371`)". That is true only when `ContactShadows` is 0.
- The first-pass `SHADERS.md` (section 4.2) says `[atmos] Froxel=0` uses "the per-pixel ray-march path". The ray march is half resolution by default (`src/atmos.inl:292`, `:481`).
- `README.md:291` describes `[atmos] Steps` as "compute method, quality" and `docs/PERFORMANCE.md` gives "fog 24 steps" and "fog 32 steps" for Medium and Ultra. On the default path `Steps` is not read by `kAtmosCS` (`src/atmos.inl:160-217`). It only changes the ray march.
- `src/atmos.inl:448` says "(line 129)" for the ray-march fallback. That line number is stale.

## See also

- [atmosphere-fog.md](atmosphere-fog.md): the composite, the ray march and the engine fog hook
- [sky-estimate-and-haze-color.md](sky-estimate-and-haze-color.md): the ambient colour the volume uses
- [distance-lod.md](distance-lod.md): the fog LOD ramp
- [../reference/settings-atmos-fog-emit.md](../reference/settings-atmos-fog-emit.md): `[atmos]` keys
- [../concepts/constraints-and-limits.md](../concepts/constraints-and-limits.md): GL version ceiling

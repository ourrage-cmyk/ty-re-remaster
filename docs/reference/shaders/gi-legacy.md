# Screen-space bounce light, legacy linear march (`[gi] Mode=0`)

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), B (tester).

## Purpose

This is the first screen-space global illumination (GI) pass. It adds a little colour bounce from lit surfaces that are visible on screen, mainly into shaded areas. It is the older of the two GI paths in the repo. The newer Hi-Z path ([gi-hiz](gi-hiz.md)) is the default when `[gi] Enable=1`, and it replaces this pass unless `[gi] Mode=0` is set. The legacy path runs only when the user sets `Mode=0`. There is no automatic fallback to it (see How it works, step 7).

## How it works

Per frame, after AO and before the emitter and atmosphere passes (`src/tyrender.cpp:291`):

```
depth copy + colour copy (engine)
  |
  v  kGI          per half-res pixel: Rays cosine-weighted rays, linear march, first hit inside thickness -> colour
  v  kGITemporal  reprojection, colour clamp, depth check -> texH (ping-pong)
  v  kGIComposite additive onto FB0, weighted by an albedo proxy and a shade factor
```

1. **Gate.** `RunGI` returns early if GI is disabled, if the flag file `gi_off` exists, if the A/B "off" frame is running, if the VP was not captured this frame, if the colour copy is missing, or if `[gi] Strength` is 0 (`src/gi.inl:105`). The flag file is polled every 8 frames (`gi.inl:98-102`).
2. **Ray generation (`kGI`, `src/gi.inl:12-40`).** For each half-res pixel the shader builds a normal from depth (`ViewNormal`, shared helper, `src/ao.inl:115-122`) and a tangent frame around it. It casts `Rays` directions drawn with a cosine weight (`gi.inl:24-27`). The directions use an interleaved gradient noise seeded per pixel and a per-frame offset (`gi.inl:22`, `:162`).
3. **March.** Each ray is marched in view space and projected back to the screen. The step distances follow a squared ramp out to `Radius`, with `Steps` samples (`gi.inl:28-31`). The ray starts 2 units off the surface along the normal (`gi.inl:30`).
4. **Hit test.** The march stops at the first sample where the scene surface is in front of the ray point and within `Thickness` (`gi.inl:33-35`). The colour at that point is taken from the colour copy. A ray that hits nothing contributes 0. The shader writes the average of all rays and the view depth into the alpha channel (`gi.inl:38`).
5. **Temporal (`kGITemporal`, `src/gi.inl:42-58`).** The history is reprojected with last frame's VP. It is rejected when the view depth changes by more than 5 % plus 5 units (`gi.inl:52`). The history colour is clamped to the 3x3 neighbourhood of the current frame plus or minus 0.02 (`gi.inl:54-55`). The blend uses `TemporalBlend`.
6. **Composite (`kGIComposite`, `src/gi.inl:60-85`).** A 4x4 depth-aware bilateral upsample (`gi.inl:69-75`). The bounce colour is multiplied by an albedo proxy: the pixel's hue, desaturated 40 %, scaled to a modest level (`gi.inl:79`). It is then scaled by a shade factor that is high in shadow and near zero on bright surfaces (`gi.inl:81`). This keeps sunlit grass from glowing. The result is clamped to 0.25 (`gi.inl:83`) and added to FB0 with blend `ONE, ONE` (`gi.inl:185`).
7. **No fallback.** The legacy path is not a fallback for the Hi-Z path. When `Mode` is not 0 and Hi-Z fails, GI is switched off (`src/gi.inl:130`, see [gi-hiz](gi-hiz.md)). The legacy path runs only when `Mode=0` (`gi.inl:131-189`). Its programs are named `ssgi`, `ssgi_temporal` and `ssgi_composite` (`gi.inl:133-135`), so the `ForceFallback` bit 2 does not touch them (`src/ao.inl:441` only matches `gi_*`).

**Keys this path ignores.** Only the keys listed in Settings are read by the legacy shaders. The source mask (`ExcludeSources`), the character keys (`Characters`, `CharacterColour`, `CharacterGI`, `CharacterHeight`, `CharacterRadius`), `Checker`, `AtrousFar`, `Atrous`, `HiZIter`, `SigmaL` and `SourceBlur` have no effect in `Mode=0` (checked against `src/gi.inl:89-189`; no `u_useMask`, `InChar` or `u_ch` in the legacy shaders). The source-mask draws still run in legacy mode, because the draw hooks call `GIMaskBegin` regardless of mode (see [gi-hiz](gi-hiz.md) and `src/tyrender.cpp:269`, `:271`). Their output is never read in legacy mode (inferred from the code; the cost is not measured).

## Code pointers

| What | File:line | Note |
|---|---|---|
| Hook point, call to `RunGI` | `src/tyrender.cpp:291` | after `RunAO`, skipped when `g_aoDebug` is set |
| Include of Hi-Z file | `src/gi.inl:10` | `gi2.inl` is included here |
| Static defaults | `src/gi.inl:8-9` | Strength 0.7, Radius 400, Thickness 120, blend 0.75 (the load default is 0.85) |
| Ray and march shader (`kGI`) | `src/gi.inl:12-40` | |
| Temporal shader (`kGITemporal`) | `src/gi.inl:42-58` | |
| Composite shader (`kGIComposite`) | `src/gi.inl:60-85` | albedo proxy :79, shade :81, clamp :83 |
| Config loader | `src/gi.inl:89-97` | `Enable` :91, `Strength` etc. :93 |
| Flag files | `src/gi.inl:98-102` | `gi_off`, `gi_debug` |
| Frame driver, legacy branch | `src/gi.inl:104-200` | legacy block :131-189 |
| Shared shader helpers | `src/ao.inl:95-123` | `kVS`, `kCommon` (used at `gi.inl:133`) |
| Mode switch | `src/gi.inl:129-130` | `GIHiz` returns false when `Mode=0` (`gi2.inl:395`) |

## Settings

The full table is in [settings-ao-gi-lod](../reference/settings-ao-gi-lod.md). Keys that matter on this path:

| Key | Code default (file:line) | Shipped `default_tyrender.ini` | Live or restart | Effect |
|---|---|---|---|---|
| `[gi] Enable` | 1 (`gi.inl:91`) | 0 (`:8`) | live | master switch |
| `[gi] Mode` | 1 (`gi.inl:95`) | 1 (`:9`) | live | 0 selects this path |
| `[gi] Rays` | 4 (`gi.inl:91`) | 1 (`:10`) | live | rays per pixel |
| `[gi] Steps` | 8 (`gi.inl:91`) | not set | live | samples per ray |
| `[gi] Strength` | 0.7 (`gi.inl:93`) | not set | live | output multiplier |
| `[gi] Radius` | 400 (`gi.inl:93`) | not set | live | ray length, world units |
| `[gi] Thickness` | 120 (`gi.inl:93`) | not set | live | hit thickness, world units |
| `[gi] TemporalBlend` | 0.85 (`gi.inl:93`) | not set | live | history weight |

Keys with no effect in `Mode=0`: see How it works, last paragraph.

Launcher note: the launcher writes `[gi] Mode=1` on every save (`src/launcher_ui.inc:67`), so the legacy path can only be selected by editing the ini by hand.

## Cost

Only whole-frame numbers exist, and they are for the Hi-Z path. The repo has no separate measurement of the legacy path on a current build.

- `docs/results/VERDICTS.md`, "Standard-view numbers": GI legacy, 4 rays, +16.94 ms over a stock frame of 5.38 ms (default camera, in-session A/B, 2560x1440).
- `docs/results/VERDICTS.md`, the lead update in the GI agent task (`docs/agent-tasks/gi.md`): legacy 4 rays +11.6 ms. The two figures are from different suites and the file does not reconcile them. The "USE THESE" table is the later correction.
- `docs/PERFORMANCE.md` (v0.3.2 measurements) gives "about 4 ms at 1 ray" for the Hi-Z path and does not list the legacy path.

The cost scales with rays times steps (`gi.inl:24`, `:28`). `docs/agent-tasks/gi.md` compares the two paths on one early build: legacy 4 rays 8.3 ms of GPU time, Hi-Z 1 ray 2.18 ms. The file does not give the build. `docs/results/preset_matrix.md` lists GI pass times only for High and Ultra, which use Hi-Z.

## Limits

- **Screen space only.** Hits can only come from pixels that are on screen. Light from off-screen surfaces is never picked up. See [constraints](../concepts/constraints-and-limits.md).
- **One bounce, first hit.** The first sample inside the thickness band is taken as the bounce source (`gi.inl:33-35`). Thin or grazing geometry can hide or double-count sources.
- **Noise.** One set of rays per pixel, with a temporal blend. In motion, the history rejection can show as lag or trails.
- **Strength range.** At the shipped strength the effect is small. `docs/results/VERDICTS.md` measured the Hi-Z version changing the frame by 0.8 to 3.1 of 255 on average. The legacy path was not measured separately for this.
- **Character handling is missing on this path.** Ty's shorts can tint the ground in legacy mode, since the source mask and character GI keys are not used (see [gi-hiz](gi-hiz.md) and `docs/agent-tasks/gi5.md`).

## Known issues

- **Legacy path is the slower option.** `README.md` (`[gi] Mode` row, line 285) says "Method 1 = Hi-Z (recommended)". The repo gives no issue number for the legacy path. The cost figures are in Cost.
- **Water and flicker on Ty.** These came from the colour source, not from this shader alone. See [gi-hiz](gi-hiz.md) for issue #5. The legacy path has no source mask, so the same water problem applies to `Mode=0` (inferred from `gi.inl:128-189` and `gi2.inl:38-51`).
- **Glow on bright grass.** The legacy composite weighting was tuned against this (`docs/agent-tasks/gi.md`, step 1). The shade factor at `gi.inl:81` is the mechanism.

## How to verify

1. Write `1` to `<game folder>\tyrender\gi_debug` for a GI-only view (`gi.inl:6`). With the debug view on, the composite writes the bounce term over the frame, not added to it, because blending is turned off when `g_giDebug` is nonzero (`gi.inl:185`).
2. Set `[gi] Mode=0` in `<game folder>\tyrender.ini`, then compare with `Mode=1`. The log should show `[gi] initialised <w/2>x<h/2>` (`gi.inl:146`) and `[gi] first run ok` (`gi.inl:199`) for the legacy path. The Hi-Z path logs different lines (see [gi-hiz](gi-hiz.md)).
3. Set `[gi] Enable=0` or write `gi_off` for an A/B pair.
4. For timing, use the `[gpu]` `gi` column (see [log and error messages](../reference/log-and-error-messages.md)). The legacy path sets GPU marks 3 and 4 around the call (`src/tyrender.cpp:291`) and sub-marks 1 to 5 inside (`gi.inl:140`, `:163`, `:176`, `:187`).

## NEEDS INVESTIGATION

- **NI-GIL-1 Cost on a current build.** The repo has no legacy-path timing since v0.3. Method: `tools/perf_matrix.ps1` with `Mode=0` and `Mode=1` at the default view, whole-frame delta (see N-20 in the master plan).
- **NI-GIL-2 Whether the legacy path is still wanted.** Method: ask the owner. The repo keeps it selectable but does not state a reason.
- **NI-GIL-3 Source-mask draws in legacy mode.** Method: count the extra draws with `[gi] DebugView` off and a log of `[gi] source mask` (`gi2.inl:428`), which only prints on the Hi-Z path. Better: add a counter in a test build. The current repo does not show the cost.

## Discrepancies

1. **Header and defaults.** `src/gi.inl:5` says `Strength=1.0`, `TemporalBlend=0.9`, `Rays=4`. Code: Strength 0.7 (`gi.inl:9`, `:93`), blend 0.85 at load (`gi.inl:93`) and 0.75 as the static default (`gi.inl:9`). Shipped ini: Rays 1 (`default_tyrender.ini:10`). Code wins; the header is stale.
2. **Static blend default.** `src/gi.inl:9` has `TemporalBlend` 0.75 as the static initial value; the loader sets 0.85 (`gi.inl:93`). The loader always runs at startup, so 0.85 is the value in use.
3. **`[gi] SourceBlur` default.** `src/gi2.inl:13` initialises `g_giBlur` to 0.0. The loader default is 0.15 (`gi.inl:93`). The loader wins at runtime. This only matters for the Hi-Z path.
4. **`[gi] Rays` default.** Code default 4 (`gi.inl:91`), shipped 1 (`default_tyrender.ini:10`), High preset 1, Ultra 2 (`src/launcher_ui.inc:13-14`). The lead update in `docs/agent-tasks/gi.md` asks for "Hi-Z with 1 ray" as the default. The code default is still 4 for both paths. Code wins; the shipped ini and the presets are what a user gets.
5. **`[gi] Enable`.** `README.md` says "1 in code, 0 in the shipped ini and in Low/Medium". The code and the shipped ini agree with that.
6. **Legacy cost numbers.** `docs/results/VERDICTS.md` gives +16.94 ms in one table and +11.6 ms in the lead update. See Cost.

## See also

- [GI Hi-Z: the default bounce-light path](gi-hiz.md)
- [AO: screen-space occlusion](ao-gtao.md)
- [Settings: AO, GI and LOD](../reference/settings-ao-gi-lod.md)
- [Frame anatomy and pass order](../how-it-works/04-frame-anatomy-and-pass-order.md)
- [Known issues](../decisions/known-issues.md)

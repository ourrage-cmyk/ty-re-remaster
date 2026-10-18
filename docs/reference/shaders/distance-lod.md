# Distance LOD ([lod] ramp for AO, GI, fog, tessellation and shadows)

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), B (tester). Off by default. Untested on hardware and the cost is not measured.

## Purpose

`[lod]` is a set of optional distance ramps. Pixels near the camera get full quality. Pixels far away get less work from the effects that the player switches on: AO horizon steps, GI rays, froxel fog samples, tessellation edge level and far shadow cascades. The aim is a lower frame time at distance, with no hard step. Issue #10 tracks it. The ramp is one shared formula, with a separate switch per effect.

## How it works

1. **Settings (`src/ao.inl:473-485`).** The master switch is `[lod] Enable`. Each effect has its own key: `GI`, `AO`, `Fog`, plus `Shadow` (read in `src/tyrender.cpp:158`) and `TessEnable` (`src/tess.inl:61`). `Start` (default 1500) and `End` (default 6000) are view distances in world units. `End` is forced to at least `Start + 1` (`src/ao.inl:483`). `MinScale` (default 0.35) is clamped to 0 to 1 (`:484`). The AO loader reads these keys, and it runs from `LoadAOConfig` (`:490`), which the reload trigger calls.
2. **The ramp.** Each shader computes `keep = 1 - (1 - MinScale) * smoothstep(Start, End, d)`, where `d` is a distance:
   - AO uses view-space depth `zc` (`src/ao.inl:308`);
   - GI uses view-space depth from `-P.z` (`src/gi2.inl:216`);
   - tessellation uses the distance from the camera to the edge midpoint (`src/tess.inl:270`, `:271`);
   - fog uses the camera distance of the slice midpoint (`src/atmos.inl:195`).
   Full quality holds before `Start`, and the kept share reaches `MinScale` at `End`.
3. **AO.** The horizon search takes `max(1, floor(steps * keep + dither))` steps per slice (`src/ao.inl:309`). The dither is per pixel and the temporal pass averages it. The ramp is parked when the AO flag is off, by setting `u_lodS` to a far value so that `keep` is 1 (`src/ao.inl:648`).
4. **GI.** Each screen-space GI ray is kept only when a per-pixel, per-frame threshold is below `keep` (`src/gi2.inl:216-219`). The result is divided by the expected count, so the mean is unchanged. The uniforms are set only when `Enable` and `GI` are both on (`src/gi2.inl:456`, `:15-18`). `AtrousFar` is a separate switch that uses `Start` as the radius threshold for the denoiser (`src/gi2.inl:294`, `:484`; `default_tyrender.ini:13-14`).
5. **Fog.** The compute path takes `max(1, floor(2 * keep + noise))` samples per slice (`src/atmos.inl:193-198`). At `MinScale = 0.35` that is one sample for far slices, and two for near ones. The uniforms are set only when `Enable` and `Fog` are on (`src/atmos.inl:467`). The ray march does not read them (see Limits).
6. **Tessellation.** The edge level is multiplied by `mix(1, MinScale, smoothstep(Start, End, d))` when `TessEnable=1` (`src/tess.inl:271`). In the F11 off frame the ramp start is passed as 0, which turns the ramp off (`src/pbr.inl:185`, `g_abOff`; `src/tess.inl:271` skips the ramp when the start is not above 0). `TessEnable` is read without the master `Enable` (`src/tess.inl:61`).
7. **Shadows.** With `[lod] Enable=1` and `Shadow=1` (`src/tyrender.cpp:158`), the uniform `tyShLod` is 1. Shadow cascades 2 and 3 then use `TyPCFL`, which for far cascades returns the eight probe taps with crack closing and no 3-by-3 or 4-by-4 grid (`src/tyrender.cpp:188-190`, `:198`, `:222`). Needs `[shadows] Patch=1` (`default_tyrender.ini:18`).
8. **Not wired.** The relief fade uses its own `[tess] ReliefFadeStart` and `ReliefFadeEnd` (`src/tess.inl:18`) and does not read `[lod]`.

```
keep
1.00 |##########.
     |          ##.
     |            ###.
0.35 |               #####  (MinScale holds past End)
     +-------+---------+------------> distance
           Start      End
```

## Code pointers

| What | File:line | Note |
|---|---|---|
| Comment and globals | `src/ao.inl:473-477` | effects listed as GI, AO, Fog |
| Loader | `src/ao.inl:478-485` | master `Enable` and per-effect keys |
| AO ramp in shader | `src/ao.inl:226`, `:307-309` | `lkA`, `stepsL` |
| AO uniform set | `src/ao.inl:648` | parked when off |
| GI uniform helper | `src/gi2.inl:15-18` | `LodUniforms` |
| GI ray keep | `src/gi2.inl:216-219` | threshold per ray |
| GI uniform set | `src/gi2.inl:456` | `Enable` and `GI` |
| AtrousFar | `src/gi2.inl:294`, `:484` | uses `Start` |
| Fog samples | `src/atmos.inl:193-198` | one or two per slice |
| Fog uniform set | `src/atmos.inl:467` | froxel branch only |
| Tess config | `src/tess.inl:22-24`, `:61-64` | `TessEnable`, Start, End, MinScale |
| Tess edge ramp | `src/tess.inl:270-272` | `EdgeLevel` |
| Tess uniform set | `src/pbr.inl:185` | zeroed in the off frame |
| Shadow LOD flag | `src/tyrender.cpp:148-149`, `:158` | `g_shLod` |
| Shadow LOD shader | `src/tyrender.cpp:188-198`, `:222` | `TyPCFL`, far-cascade branch |
| Shadow LOD uniform | `src/tyrender.cpp:554-559` | `tyShLod` per program |
| Shipped keys and comments | `default_tyrender.ini:13-30` | |

## Settings

Full tables are in [settings-ao-gi-lod.md](../reference/settings-ao-gi-lod.md). All `[lod]` keys are read in `LoadAOConfig` or `LoadTessConfig`, which the reload trigger calls, so they are live (`src/tyrender.cpp:598`).

| Key | Default in code | Shipped Medium | Live or restart | Effect |
|---|---|---|---|---|
| `[lod] Enable` | 0 (`src/ao.inl:480`) | 0 (`default_tyrender.ini:21`) | live | master switch for the AO, GI and fog ramps |
| `[lod] GI` | 0 (`src/ao.inl:481`) | 0 (`:22`) | live | GI ray thinning. Needs `[gi] Enable=1` |
| `[lod] AO` | 0 (`src/ao.inl:481`) | 0 (`:23`) | live | AO step thinning. Needs `[ao] Enable=1` |
| `[lod] Fog` | 0 (`src/ao.inl:481`) | 0 (`:24`) | live | froxel sample thinning. Needs `[atmos] Enable=1` and Froxel on |
| `[lod] Shadow` | 0 (`src/tyrender.cpp:158`) | 0 (`:25`) | live (the flag is read on reload, `:158`) | far-cascade filter. Needs `Enable=1` too and `[shadows] Patch=1` |
| `[lod] TessEnable` | 0 (`src/tess.inl:61`) | 0 (`:30`) | live | tessellation edge ramp. Master `Enable` is not read |
| `[lod] Start` | 1500 (`src/ao.inl:482`) | 1500 (`:26`) | live | where the ramp starts, view distance |
| `[lod] End` | 6000 (`src/ao.inl:483`) | 6000 (`:27`) | live | where the ramp reaches `MinScale`, view distance. Forced to Start + 1 or more |
| `[lod] MinScale` | 0.35 (`src/ao.inl:484`) | 0.35 (`:28`) | live | share of work kept at and beyond End |
| `[gi] AtrousFar` | 0 (`src/gi.inl:95`) | 0 (`default_tyrender.ini:14`) | live | denoiser uses a 3 by 3 ring beyond `Start` |

## Cost

- **Not measured.** `docs/PERFORMANCE.md` lists `[lod]` with GI, AO and Fog as "not yet measured (untested on hardware); default off". The repository does not record a measurement for any `[lod]` effect (N-20 in [../decisions/known-issues.md](../decisions/known-issues.md)).
- **Target only.** `docs/FIX_PLAN_20261016.md` T76 sets "Medium 10 ms or less, no popping" as the acceptance target for the distance LOD. No result is recorded.
- **What it saves (inferred from the code).** Fewer AO horizon steps, fewer GI rays, and about half the fog samples on far slices. Each saving applies only to the pixels beyond `Start`.

## Limits

- **No saving on near pixels.** By design, full quality holds before `Start`. A scene that is mostly near the camera gets almost nothing.
- **Two distance measures.** AO and GI use view-space depth; tessellation and fog use camera distance (`src/ao.inl:308`, `src/gi2.inl:216`, `src/tess.inl:270`, `src/atmos.inl:195`). The same `Start` and `End` therefore mean slightly different things per effect (inferred from the formulas).
- **Stochastic reduction shows as per-pixel noise.** The AO steps, GI rays and fog samples are dithered per pixel and per frame. The temporal passes average the noise, but it can read as flicker. The flicker issues in [../decisions/known-issues.md](../decisions/known-issues.md) show how sensitive this is (inferred).
- **Fog LOD only in the froxel path.** The ray march does not read `u_lodS` (`src/atmos.inl:446-449` are the ray-march uniforms, and no LOD uniform is set there). With the fallback active, `[lod] Fog` does nothing.
- **Engine geometry is not touched.** The engine still culls and selects models by its own rules (`src/debugcam.inl:5`). The mod does not change model LOD.
- **Shadow LOD needs the patch.** Without `[shadows] Patch=1` there is no `TyPCFL` and no far-cascade branch (inferred).
- **Relief has its own fade.** `[tess] ReliefFadeStart` and `ReliefFadeEnd` are not part of `[lod]` (`src/tess.inl:18`).

## Known issues

- **#10, "LOD options showed no difference".** `docs/PLAN_NEXT.md:18` and the first-pass `KNOWN_ISSUES_AND_HISTORY.md` (LOD row) say the options showed no difference in a playtest. The cause is not recorded. Both the master switch and the per-effect keys are off in the shipped ini, so a test with only the default ini would show nothing. Status: open.
- **#60, tessellation LOD.** `CHANGELOG.md` lists `TessEnable` (#10, #60) as untested.
- **#78, shadow LOD.** `CHANGELOG.md` lists `Shadow` (#10, #78) as untested.
- **#93, AtrousFar.** `CHANGELOG.md` lists `[gi] AtrousFar` (#7, #93) as untested.
- **T76 (fix plan, batch 2).** Distance LOD for tessellation, AO, shadow taps, fog steps and GI rays, with the target of Medium at 10 ms or less (`docs/FIX_PLAN_20261016.md`). Open.

## How to verify

1. **Extreme test.** Set `[lod] Enable=1`, `Fog=1`, `AO=1`, `GI=1`, `Start=0`, `End=1`, `MinScale=0` in a test copy of `tyrender.ini`. Every effect should drop to its minimum beyond about one unit. The picture should look clearly lower quality at distance. This is a stress test, not a setting.
2. **Per-effect check.** Turn one effect on at a time. The `Enable` master plus the effect's own key are both needed (`default_tyrender.ini:15-16`). `TessEnable` does not need the master (see Discrepancies).
3. **Live change.** Edit `tyrender.ini` and create the `reload` trigger (`src/tyrender.cpp:597`). The LOD values are re-read by `LoadAOConfig` and `LoadTessConfig`.
4. **Off-frame.** The F11 off frame parks the tessellation ramp (`src/pbr.inl:185`). The AO and fog ramps are gated by their own flags, not by `g_abOff` (inferred).
5. **Cost.** Use the `[gpusub]` timers at a far view (for example the vista spot named in `docs/PERFORMANCE.md`) with `[lod] Enable` on and off. The repository has no recorded result for this.

## NEEDS INVESTIGATION

- **NI, why #10 showed no difference.** Method: play with the master and the effect keys on, at a far view, and compare with `Enable=0`. Check whether the owner's test ini had `Enable=1`.
- **NI, measured cost (N-20).** Method: `tools/perf_matrix.ps1` or the `[gpusub]` timers at Medium, once per `[lod]` effect (as in N-20 in [../decisions/known-issues.md](../decisions/known-issues.md)).
- **NI, flicker from dithering.** Does the stochastic reduction flicker at distance? Method: `tools/motion.ps1` temporal noise on a static far view, with and without `[lod]`, as in the test harness guide ([../guides/test-harness.md](../guides/test-harness.md)).
- **NI, `TessEnable` without the master.** Is it intended that the tessellation ramp does not need `[lod] Enable`? Method: owner decision. The code reads `TessEnable` alone (`src/tess.inl:61`), while the shipped ini says the master switch controls every effect (`default_tyrender.ini:15`).

## Discrepancies

- `default_tyrender.ini:15` says "Enable is the master switch; each effect needs its own key as well (GI, AO, Fog)". For the tessellation ramp the code reads `TessEnable` alone (`src/tess.inl:61`). Code wins. The ini comment at `:29-30` describes `TessEnable` on its own, so the file is also inconsistent with itself on this point.
- `default_tyrender.ini:18-19` says "Tess/relief (...) is not wired". Lines 29-30 of the same file describe `[lod] TessEnable` as working, and `src/tess.inl:61` and `:271` implement it. The "not wired" sentence refers to relief fade only, and it should say so.
- `src/tess.inl:22-23` (comment) names a key `TessMinScale`. The key read is `MinScale` (`src/tess.inl:64`).
- First-pass `SHADERS.md` section 1.2 says the LOD ramp reduces AO "strength". The code reduces the number of horizon steps (`src/ao.inl:308-309`).
- `src/ao.inl:474-475` says the ramp is used by "the GI trace, the AO horizon search and the froxel fog". The tessellation edge ramp and the shadow filter also read `[lod]` (`src/tess.inl:61`, `src/tyrender.cpp:158`). The comment is out of date.
- `CHANGELOG.md` (Unreleased, Added) says `[lod] Enable/GI/AO/Fog` are "Off by default. Untested." That matches the code. `docs/PLAN_NEXT.md:18` says "LOD options showed no difference (#10)". That is an observation, not a contradiction, but it is the only evidence for the LOD effects so far.

## See also

- [atmosphere-froxel-compute.md](atmosphere-froxel-compute.md): the froxel path that the fog LOD needs
- [atmosphere-fog.md](atmosphere-fog.md): the fog pass the ramp belongs to
- [../reference/settings-ao-gi-lod.md](../reference/settings-ao-gi-lod.md): `[ao]`, `[gi]` and `[lod]` keys
- [../reference/settings-post-shadows-light-pbr-tess.md](../reference/settings-post-shadows-light-pbr-tess.md): `[tess]` and `[shadows]` keys
- [../decisions/roadmap-and-open-work.md](../decisions/roadmap-and-open-work.md): where #10 and T76 sit in the plan

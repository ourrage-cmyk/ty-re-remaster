# Emitter light (emit.inl, glowing draws light nearby surfaces)

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), B (tester). Off by default and not safe to turn on in this build (see Known issues).

## Purpose

Emissive draws, such as a glowing portal or a light-emitting texture, look bright but do not light the ground around them, because the engine's lighting does not treat them as lights. Emitter light is the mod's attempt to fix that. It draws each kept emitter a second time into a small half-resolution buffer, depth-tested against the scene, blurs that buffer, and adds it to the frame. The first user is the rainbow portal (issue #9). It is behind `[emit] Enable=0`.

## How it works

1. **Which draws emit.** A draw is an emitter draw when its albedo texture's material-policy row has `emissive` set (`src/tex.inl:43-47`: emitter, transparent emitter and emissive character classes). The check is in `EmitQBegin` (`src/emit.inl:137-140`).
2. **Shader rewrite (only when `Enable=1` at shader compile).** The stock output line `FINAL_COLOR = diffuseColour;` is replaced with a branch on a per-program uniform `tyEmit`. When `tyEmit` is 1 the shader outputs the albedo, otherwise the normal colour (`src/emit.inl:47-59`). If the stock light line `diffuseColour *= light;` occurs exactly once, the albedo is captured just before it (`:51-54`). Otherwise the current `diffuseColour` is used (`:56`). `TestCompile` in the shader hook still falls back to the stock shader on any error (`:46`).
3. **Uniform location.** The `tyEmit` location is cached per program when it links (`src/emit.inl:60-62`, called from `AfterLink` at `src/tyrender.cpp:564`).
4. **Ordinals and the query (every frame).** Each emitter draw gets the next ordinal of the frame, up to 256 (`src/emit.inl:21`, `:141-143`). An occlusion query (`SAMPLES_PASSED`) wraps the normal draw of that emitter, so it counts the screen samples that survive the depth test (`:150`).
5. **Ranking (lagged one frame).** `EmitRankPoll` reads finished queries, sorts ordinals by visible samples, and keeps the top `MaxEmitters` for the next frame. Ordinals with no result yet are kept (`src/emit.inl:154-168`).
6. **Splat (`EmitBegin`, `src/emit.inl:173-210`).** For each kept emitter draw, after the normal draw:
   - the scene depth of the default framebuffer is copied to a full-resolution depth texture (`:186-188`);
   - each 2x2 block is reduced to its farthest depth in the half-resolution depth of the emission buffer (`:190-200`). Keeping the farthest depth means an emitter pixel next to an occluder is not cut away (comment at `src/emit.inl:81`);
   - the draw is re-issued with depth test `LEQUAL`, additive blending `SRC_ALPHA, ONE` and `tyEmit=1` (`:205-208`). The hook re-issues the draw (`src/tyrender.cpp:269`, `:271`).
   - The depth texture left bound on unit 0 by the depth pass is rebound to the emitter's own texture before the splat, because otherwise the albedo reads the depth (`src/emit.inl:201-203`).
7. **Blur and composite (`RunEmit`, once per frame, `src/emit.inl:226-266`).** `RunEmit` runs from the depth-copy hook after GI and before atmosphere (`src/tyrender.cpp:292`). If any splat happened, the emission buffer is blurred with four separable passes, alternating horizontal and vertical, using a nine-tap kernel (`src/emit.inl:64-75`, `:242-251`). Each step is `Radius * 0.5 * Spread` half-resolution texels (`:244`). The result is added to the frame's RGB with a gain of `Strength`, multiplied by `Spread` squared when `Compensate=1` (`:253-257`). The frame is marked modified so the bloom copy includes the light (`:258`, `src/tyrender.cpp:294-299`).
8. **Buffers (`EmitInit`, `src/emit.inl:100-131`).** Two half-resolution RGBA16F colour buffers, a full-resolution depth copy and a half-resolution depth buffer are created. Creation happens at the end of the frame, never inside a draw (`:174`).

```
emitter draw --> [query around normal draw] --> normal draw
        |
        '--> EmitBegin: copy depth -> max 2x2 -> splat (depth LEQUAL, additive, albedo)
                                                  |  (kept ordinals only, cap MaxEmitters)
frame end --> RunEmit: 4 separable blur passes --> additive composite into FB0 RGB
```

## Code pointers

| What | File:line | Note |
|---|---|---|
| Config loader | `src/emit.inl:32-43` | defaults and clamps; logs `[emit] enabled` |
| Shader rewrite | `src/emit.inl:47-59` | `tyEmit` branch, albedo capture at `:51-54` |
| Uniform cache | `src/emit.inl:60-62`, `src/tyrender.cpp:564` | `EmitOnLink` from `AfterLink` |
| Blur shader `kEmitBlur` | `src/emit.inl:64-75` | nine taps |
| Composite shader `kEmitComp` | `src/emit.inl:76-80` | multiplies by strength |
| Depth shader `kEmitDepth` | `src/emit.inl:82-91` | farthest of each 2x2 block |
| Buffers | `src/emit.inl:93-131` | `EmitInit`, `MakeDepthTex` |
| Query start and end | `src/emit.inl:137-152` | `EmitQBegin`, `EmitQEnd` |
| Ranking | `src/emit.inl:154-168` | `EmitRankPoll` |
| Splat | `src/emit.inl:173-223` | `EmitBegin`, `EmitEnd` |
| Frame composite | `src/emit.inl:225-266` | `RunEmit` |
| Hook calls | `src/tyrender.cpp:269`, `:271` | draw re-issue |
| Frame call site | `src/tyrender.cpp:292` | after GI, before atmosphere |
| Bloom refresh | `src/tyrender.cpp:294-299` | copies the colour capture after changes |
| Additive draw log | `src/tex.inl:242-246`, `:264-267` | `[spill] LogEmitters` |

## Settings

Full tables are in [settings-atmos-fog-emit.md](../reference/settings-atmos-fog-emit.md). "Restart" means the key is only applied to shaders compiled after the change.

| Key | Default in code | Shipped Medium | Live or restart | Effect |
|---|---|---|---|---|
| `[emit] Enable` | 0 (`src/emit.inl:34`) | 0 (`default_tyrender.ini:107`) | restart in practice (shader rewrite is at compile, `:48`) | master switch |
| `[emit] Strength` | 1.0 (`src/emit.inl:35`) | 1.0 (`:108`) | live | composite gain |
| `[emit] Radius` | 12 (`src/emit.inl:36`) | 12 (`:109`) | live | blur step in half-resolution texels |
| `[emit] Spread` | 1.0 (`src/emit.inl:37`) | not set | live | scales the blur step. Clamped to 0.25 to 4 |
| `[emit] Compensate` | 0 (`src/emit.inl:38`) | not set | live | when 1, composite gain is multiplied by Spread squared |
| `[emit] MaxEmitters` | 16 (`src/emit.inl:34`) | 16 (`:110`) | live | cap on splatted draws per frame and on ranked ordinals kept |
| `[spill] LogEmitters` | 0 (`src/tex.inl:69`) | 0 (`default_tyrender.ini:66`) | restart (read in `LoadTexTags`) | logs additive draws per texture, for choosing emitter textures |

## Cost

- **Not measured.** No `[gpusub] ... emit` figure is recorded in the repository. The test plan's gate is under 1 ms at 2560x1440 with the portal on screen (`docs/EMIT_PORTAL_TEST_PLAN.md`, "Pass criteria"). That plan is "prepared, not run".
- **Cost drivers (inferred from the code).** Each splat does a full-resolution depth copy (`src/emit.inl:188`), so the copy count rises with the number of kept emitters, up to `MaxEmitters` per frame. Each kept emitter is also drawn a second time. The blur and composite run once per frame when at least one splat happened (`:229`).
- Launcher and README tell players nothing about the emitter cost, because it is off (`README.md` does not list it).

## Limits

- **Only tagged emitters.** A texture must be tagged emissive (`src/tex.inl:43-47`) and drawn with its own shader. An emitter whose shader cannot be rewritten has no `tyEmit` and does not light anything (`src/emit.inl:10`).
- **Albedo, not lit colour.** The splat outputs the albedo captured before the light multiply (`src/emit.inl:10-11`). It is the texture's colour, not the lit colour, and it is only the true albedo when the stock light line is present.
- **Screen space only.** The splat is depth-tested against the screen-space scene depth. Light from an emitter that is off screen, or behind the camera, does not appear (inferred from the buffer design).
- **One blur for all emitters.** `Radius` and `Spread` are shared by every emitter (`src/emit.inl:7-8`).
- **Ranking follows draw order.** Ranking uses per-frame ordinals (`src/emit.inl:141`). If the engine changes the order of emitter draws between frames, a different emitter can take a kept slot (inferred; not tested).
- **Queries lag one frame.** A newly visible emitter waits at least one frame to be ranked. `g_emitKeep` keeps unscored ordinals (`src/emit.inl:166`).
- **Fallback without queries.** If the occlusion query entry points are missing, ranking is off and the first `MaxEmitters` draws in order are kept (`src/emit.inl:146`, `:166-167`, `:179`).
- **Ordinal cap of 256 per frame.** Emitter draws past ordinal 256 are not considered (`src/emit.inl:21`, `:142`).
- **Forward light cap.** `docs/PLAN_NEXT.md:35` gives a forward-renderer light cap of 16 as the reason for `MaxEmitters`. That value is not verified in the repository (NI-11 in [../decisions/known-issues.md](../decisions/known-issues.md)).

## Known issues

- **#9, portal light does not light the ground.** This is the mechanism's first user (`docs/MATERIAL_CLASSIFICATION_PLAN.md:3`, `:79`). `README.md:216` says `[emit] Enable` is off because it draws a white band when on.
- **Known: white band with `[emit] Enable=1`.** `CHANGELOG.md` (Known) says the flat white upper band and stripes appeared in the autoplaytest view in the 4a2ef95 era, and to keep it off. The binding fix at `src/emit.inl:201-203` describes a bug with the same symptoms ("white far band, rings and stripes") and says the fix is for it. Whether the current build still shows the band is not confirmed (see NEEDS INVESTIGATION).
- **#61, wider portal blur.** `CHANGELOG.md` (Unreleased, Added) lists a "wider portal blur" under `[emit]`, default off, for #61. The blur width in the code is set by `Spread` (`src/emit.inl:37`, `:244`).
- **#84, cited for Q182.** The title is not in the repository. `docs/AUTOPLAYTEST.md:145` lists "issues #79 to #84 and #86" as the next monitor plan, without saying which issue is the portal one (N-19).
- **Playtest of 2026-10-17.** The portal at 7:42 glows (about 6 percent of the frame above luminance 200; `docs/PLAYTEST_2026-10-17_FINDINGS.md:143`). The findings file names `src/light.inl` as the likely pass (`:83`). The code that emits is `src/emit.inl`.
- **Test plan not run.** `docs/EMIT_PORTAL_TEST_PLAN.md:3` says "prepared, not run".

## How to verify

1. **Log lines.** With `[emit] Enable=1`, expect `[emit] enabled: strength ... max 16` at start (`src/emit.inl:42`) and `[emit] emission buffer WxH (depth tested)` on the first frame (`:128`). Failures log `[emit] shaders or GL entry points missing, emitter light off` (`:109`) or `[emit] occlusion query entry points missing: ranking off, draw order cap` (`:146`).
2. **Test ini.** `tools/test_ini/emit_portal_test.ini` is the default ini with `[emit] Enable=1` and `[tex] LogUnknown=1`, and nothing else (checked against the default file in `docs/EMIT_PORTAL_TEST_PLAN.md`). Back up the owner's `tyrender.ini` first, as the plan's step 2 says.
3. **Before and after.** Capture the portal spot with `Enable=0` and with `Enable=1`. Pass criteria from the plan: the ground near the portal is brighter with `Enable=1`, and the character is not darker.
4. **Timing.** The `[gpusub]` line every 300 frames reports `emit <ms>` (marks 20 and 21, `src/gpuprof.inl:2`, `src/tyrender.cpp:292`).
5. **Reload effect.** With `Enable=1` and the portal on screen, change any launcher slider (which writes the `reload` trigger). Check whether the splat still lights the ground (see Discrepancies: a suspected defect).
6. **Additive draws.** `[spill] LogEmitters=1` logs `[spill] additive draws=... tex=... name=... emissive=...` every 600 frames (`src/tex.inl:264-267`). Use it to find the emitter textures.

## NEEDS INVESTIGATION

- **NI, white band.** Does the current build still show the flat white band with `Enable=1` at the portal? Method: run the test plan in `docs/EMIT_PORTAL_TEST_PLAN.md` on a copy of the game folder, and check the upper band in the capture.
- **NI, reload clears emitter locations (suspected defect).** `LoadEmitConfig` sets every `tyEmit` location to -1 (`src/emit.inl:40`). It runs on the reload trigger (`src/tyrender.cpp:598`), and the locations are only refilled when a program links (`src/emit.inl:60-62`, `src/tyrender.cpp:561-564`). If so, every reload while `Enable=1` stops splats for already-linked programs until they relink. Method: with `Enable=1`, confirm the light, trigger a reload, and check whether it returns without a level change. This is inferred from reading the code, not tested.
- **NI, ranking stability.** Does the ordinal ranking keep the same emitters when the camera is still? Method: log the kept ordinals for a fixed camera over 60 frames.
- **NI, cost (N-20).** Method: `[gpusub]` emit time at the portal spot with `MaxEmitters` at 4, 16 and 32.
- **NI, light cap of 16 (N-11).** Method: read the dumped portal shader's light loop (`[debug] DumpShaders=1`, see [../reference/hotkeys-and-files.md](../reference/hotkeys-and-files.md)).
- **NI, issue #84 and #61 titles (N-19).** Method: `gh issue view` by the reviewer.

## Discrepancies

- `src/emit.inl:9` says `MaxEmitters` is "NOT ranked by screen area or distance yet". The code ranks by visible samples when the occlusion query entry points exist (`src/emit.inl:137-168`). The first N in draw order is only the fallback (`:146`). Code wins.
- `CHANGELOG.md` (Unreleased, Added) says emitter draws are ranked "by screen area". The code ranks by the samples that pass the depth test, one frame late (`src/emit.inl:20`, `:150`, `:161`). The two are close, but the lag is not in the CHANGELOG.
- `docs/PLAYTEST_2026-10-17_FINDINGS.md:83` names `src/light.inl` as the pass for the portal light. The emitter code is `src/emit.inl`.
- `docs/PLAN_NEXT.md:35` says the light cap is 16, which is not verified in the repository (see Limits).
- First-pass `SHADERS.md` section 3 says `kEmitBlur` is a "9-tap Gaussian blur" (matches `src/emit.inl:68-72`). Its keys and defaults match the code (`src/emit.inl:32-43`).

## See also

- [atmosphere-fog.md](atmosphere-fog.md): the fog pass that runs after emitter light
- [water-fog-and-decals.md](water-fog-and-decals.md): the texture tags that decide what emits
- [../reference/settings-atmos-fog-emit.md](../reference/settings-atmos-fog-emit.md): `[emit]` and `[spill]` keys
- [../reference/hotkeys-and-files.md](../reference/hotkeys-and-files.md): the reload trigger and debug dumps
- [../decisions/known-issues.md](../decisions/known-issues.md): #9 and its status

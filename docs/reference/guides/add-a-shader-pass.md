# Add a shader pass

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor).
> Verification note: this is a worked example. The pass described, a half-resolution pass called `XPass` after atmosphere, does not exist in the repo. Every hook point, helper and pattern it names is in the repo, with file:line. No build or game run was done by the writer.

## Purpose

This page walks through what a new screen-space pass needs: where it runs in the frame, how its config is read and gated, how its GL objects are made and kept, how it is compiled, how it keeps GL state clean, how it fails safely, how it is timed and checked for leaks, and how it is tested. It follows the pattern of the existing passes, `RunFX` (`src/fx.inl:177`) and `RunAtmos` (`src/atmos.inl:358`).

## How it works

1. **Choose the hook.** Screen-space passes run when the engine copies the scene depth. The hook is `glCopyTexImage2D` on the depth formats (`src/tyrender.cpp:280-287`). Inside it, the code calls `RunAO`, `RunGI`, `RunEmit`, then `RunAtmos` in that order (`src/tyrender.cpp:290-293`). A new pass goes after `RunAtmos` (`:293`), before the colour recopy (`:294`). The post pass `RunFX` is different: it starts from the 64x64 viewport trigger (`src/tyrender.cpp:254`).
2. **Decide whether it changes the scene colour.** If the pass writes to the framebuffer, it must set `g_sceneModified = true`. The engine's colour capture is re-copied only when that flag is set (`src/tyrender.cpp:294`). Existing passes that do this are AO (`src/ao.inl:711`), atmosphere (`src/atmos.inl:498`), emitter light (`src/emit.inl:258`) and GI (`src/gi.inl:190`).
3. **Add the files.** Create `src/xpass.inl`. Add `#include "xpass.inl"` to the include block (`src/tyrender.cpp:540-552`). Add forward declarations next to the others (`src/tyrender.cpp:93-98`).
4. **Read config and gate it.** Write `LoadXPassConfig()` with `GetPrivateProfile*A` calls and defaults (pattern: `src/atmos.inl:292`, `HalfRes` default `1`). Call it at startup (`src/tyrender.cpp:714-725`) and in the reload list (`src/tyrender.cpp:598`).
5. **Early-out in one line.** The existing passes return at the top when disabled, when the flag file is present, when the F11 A/B off frame is active, or when a previous failure was recorded. Examples: `src/fx.inl:178` (`fx.failed`, `g_fxOffFile`, `g_abOff`), `src/atmos.inl:362`. The flag file is named in `src/tyrender.cpp:596`. The A/B off frame is set in `src/abshot.inl:123`, and the off frame skips the passes listed in `CHANGELOG.md:32`. The new pass must skip too.
6. **Compile the programs once.** Use `CompileProg(name, vs, fs, nullptr, nullptr)` (defined at `src/ao.inl:439`; used at `src/fx.inl:181`). It returns 0 when compile or link fails and logs the info log. On failure, set the pass's `failed` flag and do not retry (`src/fx.inl:184`).
7. **Make the buffers once, remake on size change.** The pattern is an `init` flag, then a size check that recreates textures and framebuffers (`src/fx.inl:180-197`). Create textures with `MakeTex8`, which sets linear filtering (`src/fx.inl:169`). A half-resolution pass uses half the width and height, as the atmosphere pass does through its `HalfRes` key (`src/atmos.inl:292`).
8. **Save and restore state.** `RunFX` saves the framebuffer, viewport, active texture unit, bound textures, vertex array, blend function, depth mask and colour mask, then disables blend, depth, cull, scissor and stencil (`src/fx.inl:198-210`). At the end it restores all of them. A pass must leave the engine's state exactly as it found it. A leak corrupts later engine draws without a GL error (`src/stateguard.inl:3-4`).
9. **Mark the timers.** Call `GProf(n)` before and after the pass (pattern: `src/tyrender.cpp:290-293`). Mark numbers are 0 to 21, and all 22 are already used (`src/gpuprof.inl:6` sizes the array as 22). A new pass needs a wider array (`src/gpuprof.inl:6`), a new label in the log lines (`src/gpuprof.inl:37-38`), and a new slot in the accumulator, which has 12 entries (`src/gpuprof.inl:6`).
10. **Hook the leak detector (optional).** `SGMark` handles marks 1 to 8 only (`src/stateguard.inl:33`), in pairs for AO, GI, atmosphere and post. A pass on other marks is not checked by `[debug] StateGuard=1` until `SGMark` and the pass-name table are extended (`src/stateguard.inl:10`, pass names).
11. **Add a fallback bit (optional).** Existing forced-failure bits are checked in the modules: bit 1 for atmosphere (`src/atmos.inl:257`), bit 2 for GI programs and bit 4 for AO v2 (`src/ao.inl:440-441`), bit 8 for tessellation (`src/tess.inl:382`), bit 16 for the light rewrite (`src/light.inl:36`). A new bit is read from `[debug] ForceFallback` (`src/ao.inl:491`). The bit list is in the comment at `src/ao.inl:437`. Add the bit to that comment too.
12. **Wire the report (optional).** The session report gets GPU samples through `SessGpuSample` (`src/sesslog.inl:147`). Adding a column is a change in the report code, described on the session report page.

```
engine depth copy (tyrender.cpp:280-287)
  -> RunAO (1-2) -> RunGI (3-4) -> RunEmit (20-21) -> RunAtmos (5-6)
  -> [XPass (new mark pair)]                              <- illustrative
  -> colour recopy if g_sceneModified (294-297)
  -> bloom, brightness, HUD (engine)
  -> 64x64 viewport: RunFX (7-8)                          (tyrender.cpp:254)
```

## Code pointers

| What | File:line | Note |
|---|---|---|
| Depth copy hook | `src/tyrender.cpp:280-287` | Where passes start. |
| Pass calls in order | `src/tyrender.cpp:290-293` | AO, GI, emitter, atmosphere. |
| Recopy after changes | `src/tyrender.cpp:294-297` | Only when `g_sceneModified`. |
| Post trigger | `src/tyrender.cpp:254` | 64x64 viewport. |
| Pass declarations | `src/tyrender.cpp:70-98` | Forward declarations. |
| Include list | `src/tyrender.cpp:540-552` | Order of `.inl` files. |
| Startup loaders | `src/tyrender.cpp:714-725` | Add the new loader here. |
| Reload loaders | `src/tyrender.cpp:598` | And here. |
| Flag file read | `src/tyrender.cpp:596` | `post_off`, and similar. |
| Scene-modified setters | `src/ao.inl:711`, `src/atmos.inl:498`, `src/emit.inl:258`, `src/gi.inl:190` | Set the flag. |
| Model pass: post | `src/fx.inl:177-210` | `RunFX` setup, state save, early-out. |
| Model pass: atmosphere | `src/atmos.inl:358`, `:362`, `:292` | `RunAtmos`, its gate, `HalfRes`. |
| Shader helper | `src/ao.inl:439` | `CompileProg`. |
| Failure logging example | `src/atmos.inl:267`, `:270` | Compile and link failure logs. |
| Texture helper | `src/fx.inl:169` | `MakeTex8`. |
| Timer marks | `src/gpuprof.inl:1-3` | Mark list in the header comment. |
| Timer array | `src/gpuprof.inl:6` | 22 marks, 12 accumulators. |
| Timer function | `src/gpuprof.inl:17-21` | `GProf`. |
| Timer log lines | `src/gpuprof.inl:37-38` | `[gpu]` and `[gpusub]`. |
| Timer report hook | `src/gpuprof.inl:34` | `SessGpuSample` call. |
| Leak detector marks | `src/stateguard.inl:31-33` | Marks 1 to 8 only. |
| Leak detector pass names | `src/stateguard.inl:10` | Five entries. |
| Forced-fallback bits | `src/ao.inl:437-441` | Bit list and checks. |
| Atmosphere forced failure | `src/atmos.inl:257` | Bit 1. |
| Tessellation forced failure | `src/tess.inl:382` | Bit 8. |
| Light forced failure | `src/light.inl:36` | Bit 16. |
| Report GPU sample | `src/sesslog.inl:147` | `SessGpuSample`. |
| Shader-pass logging of failure | `src/sesslog.inl:67` | `SessFailure`. |

## Settings

The illustrative pass would have one key, `[xpass] Enable`, with default `0`, following the pattern of `[emit] Enable=0` (`src/default_tyrender.ini:104-105`, where the comment says `Enable=0` runs none of the pass). Other keys would use the same `GetPrivateProfile*A` style as `src/atmos.inl:292`.

## Cost

Not measured for `XPass`, which does not exist. Lesson from the repo: per-pass timers under-count, and total frame time is the metric. The owner's agent notes record a case where the atmosphere pass took 3.9 ms on its own marks but added 8.2 ms to total frame time (`docs/agent-tasks/perf.md`, line 2; agent observation, not a repo measurement). Use total frame time (`docs/START_HERE.md:76`).

## Limits

- **One hook.** Screen-space passes can only read what the depth copy gives them: the depth texture, the colour copy and the state at that moment (pipeline facts in `docs/START_HERE.md:85`; constraints in `concepts/constraints-and-limits.md`).
- **Mark budget.** All 22 timer marks are in use (`src/gpuprof.inl:6`). Adding one changes the arrays and the log format.
- **Leak detector scope.** Marks 9 and above are not checked by `StateGuard` (`src/stateguard.inl:33`).
- **Shader language level.** Fullscreen passes use GLSL 330 (`src/fx.inl:8`, `:57`, `:99`). The froxel compute shader uses GLSL 430 (`src/atmos.inl:160`). Compute needs a higher GL version and runtime support, so probe at runtime and fall back.
- **Engine shader rewrite is separate.** A pass that changes engine shaders instead goes through the rewrite hooks (`docs/START_HERE.md:84`), which is a different path.

## Known issues

- No open issue is about this procedure. The timer and leak-detector limits above are the known friction points.
- The `[gpusub]` log line prints a fixed set of labels (`src/gpuprof.inl:38`). New sub-timers do not appear until the label list changes.

## How to verify

Not run by the documenter. Procedure for a real pass:

1. Build with the pass gated off, then on. Compare total frame time with the A/B tool (`tools/suite.ps1`) on the same camera.
2. Keep `[debug] GpuProf=1` (the default, `src/gpuprof.inl:5`). Read `[gpu]` and `[gpusub]` lines in `tyrender.log` every 300 frames (`src/gpuprof.inl:3`).
3. Set `[debug] StateGuard=1`. Run the test. Search the log for `[state] LEAK after` (the message format is in the comment at `src/stateguard.inl:3`, and the log call is at `src/stateguard.inl:39`). Expect none for your pass's name. Remember that marks 9 and above are not checked (Limits).
4. Set the forced-fallback bit for your pass. Expect a log line such as `[fallback] FORCED ...` (pattern: `src/ao.inl:440`), and the image should match the gated-off image.
5. Create the `post_off`-style flag file for the pass and confirm the early-out (`src/tyrender.cpp:596`).

## NEEDS INVESTIGATION

- **X1. Which passes set `g_sceneModified`.** Four passes are listed above. Method: `grep -n "g_sceneModified" src/*.inl src/*.cpp` and read each writer.
- **X2. Session report columns.** How a new pass shows in the report. Method: read `src/sesslog.inl:147` onward and the session report page (Ch09).

## Discrepancies

- **Agent observation versus repo.** `docs/agent-tasks/perf.md` describes passes and totals as measured (see Cost). Those are agent observations from a merged tree, not a repo measurement. Treat them as leads.

## See also

- [Add a setting](add-a-setting.md)
- [Frame anatomy and pass order](../how-it-works/04-frame-anatomy-and-pass-order.md)
- [Shader source rewriting](../how-it-works/05-shader-source-rewriting.md)
- [Fallbacks and failure handling](../how-it-works/11-fallbacks-and-failure-handling.md)
- [Atmosphere](../shaders/atmosphere-fog.md)
- [Debugging](debugging.md)

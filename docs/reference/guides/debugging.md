# Debugging the renderer

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), and B for the tester steps marked "tester".

## Purpose

This page gives a symptom-first route to the switch, log line or capture that shows which effect or pass is responsible. Use it after the tester guide's first steps. Each switch below is a code default unless marked "shipped". The full list of keys is in [reference/settings-system-debug.md](../reference/settings-system-debug.md). Trigger files are in [reference/hotkeys-and-files.md](../reference/hotkeys-and-files.md).

## How it works

### Decision tree

```
Symptom
 |- Crash, freeze, or session report ends with CRASH
 |     -> read the CRASH line in the session report (src/sesslog.inl:362-365)
 |        and the "!!!" lines in tyrender.log
 |- Black screen, missing geometry
 |     |- right after F10 or F11 -> overlay or A/B bug (#12, #96); see Known issues
 |     |- after a setting change -> FAILED / does not compile lines; [fallback] lines
 |     '- unexplained -> [debug] StateGuard=1, look for "[state] LEAK after"
 |- Blue flicker or brightness ramp on Ty in water (#5)
 |     -> [pbr] DebugWater bits, one at a time (see Forcing and bisecting)
 |- Other flicker
 |     |- static view -> tools/flicker.ps1 (test-harness.md), then ao_debug / gi_debug / atmos_debug
 |     '- only while turning -> tools/motion.ps1 (test-harness.md)
 |- Which pass draws a wrong pixel?
 |     -> [ao] / [gi] / [atmos] DebugView, [pbr] Debug, [tess] Debug, [debug] ShadowView
 |     -> one frame call by call: CaptureFrame or cap.req
 |- Shader does not compile, or looks wrong
 |     -> tyrender\dump (engine source), tyrender\override (replacement), TestCompile lines
 '- Slow
       -> [gpu] lines every 300 frames; see "GPU timers" below
```

### Logs

- `tyrender.log` lives in the game folder. The DLL opens it with `fopen(..., "w")` in `DllMain`, so it is recreated at each start (`src/tyrender.cpp:708`). Copy it before starting the game again.
- Each module writes lines with a tag in brackets, such as `[ao]`, `[gi]`, `[atmos]`, `[fallback]`, `[tess]`, `[tex]`, `[state]`, `[shot]`, `[motion]`, `[dcam]`, `[dbgwater]`, `[overlay]`, `[cfg]`, `[gpu]` and `[gpusub]`. The full list with meanings is in [reference/log-and-error-messages.md](../reference/log-and-error-messages.md).
- Config reloads log `[cfg] reloaded all` (`src/tyrender.cpp:603`).
- The session report is a separate file in `tyrender\logs\` (`src/sesslog.inl:1`, `:259-262`). See [guides/session-report.md](session-report.md).

### Debug views

Each view changes the picture on purpose. Do not compare it with stock.

- **AO view.** `[ao] DebugView` (`src/ao.inl:515`) and the file `tyrender\ao_debug`, whose content wins when the file exists (`src/ao.inl:521`). Values from the header comment: `1` shows AO alone, `2` shows normals, `3` shows view depth (`src/ao.inl:5`). The shader reads the value as `u_debug` (`src/ao.inl:647`, `:655`, `:703`).
- **GI view.** `[gi] DebugView` (`src/gi.inl:95`) and the file `tyrender\gi_debug` (`src/gi.inl:101`). The header says `gi_debug` value `1` shows GI only (`src/gi.inl:6`). In the Hi-Z path, `[gi] DebugView=3` shows the source mask (`src/gi2.inl:33`).
- **Atmosphere view.** The file `tyrender\atmos_debug` only (`src/atmos.inl:312`). Value `1` shows in-scatter only (`src/atmos.inl:6`). There is no ini key.
- **PBR normals.** `[pbr] Debug=1` shows the normals (`src/pbr.inl:165`).
- **Tessellation edges.** `[tess] Debug=1` darkens vertex colour near triangle edges (`src/tess.inl:248`, `:374`).
- **Shadow term.** `[debug] ShadowView=1` shows the shadow term alone: white is lit, black is shadow (`src/light.inl:24`, `:39`).
- **Texture upload log.** `[tex] Debug=1` logs the first 500 uploads (`src/tex.inl:195`). `[tex] LogUnknown=1` logs draws per untagged texture (`src/tex.inl:13`, `:270`).

Debug views are exclusive. With the AO view on, GI, emitters and atmosphere are skipped for that frame. With the GI view on, emitters and atmosphere are skipped (`src/tyrender.cpp:291-293`).

### Shader dump and override

- **Dump.** For each engine shader the DLL receives, it writes the source as received to `<game folder>\tyrender\dump\<hash>.<vs|fs|xs>.glsl`, once per hash (`src/tyrender.cpp:369-372`). The hash is FNV-1a over the whole source (`src/tyrender.cpp:132`, `:365`).
- **Override.** If `<game folder>\tyrender\override\<hash>.glsl` exists, its text replaces the engine source (`src/tyrender.cpp:373-375`). For vertex shaders the DLL then skips its own rewrite (`src/tyrender.cpp:377`). The same is inferred for fragment shaders from the same `!useOv` test.
- **Defaults.** Both switches are on by default in code (`src/tyrender.cpp:60-61`). The shipped ini has no `[debug]` section (`src/default_tyrender.ini`), so dumps appear on every run.

### Forcing and bisecting

- **Forced fallbacks.** `[debug] ForceFallback` takes a bit mask. Each bit forces one effect to fail and switch off, so the fallback path can be tested. Each forced failure logs `[fallback] FORCED ...`.
  - `1` atmosphere froxel: `src/atmos.inl:257`.
  - `2` programs whose names start with `gi_`, the Hi-Z GI set: `src/ao.inl:441`, names in `src/gi2.inl:359-361`.
  - `4` AO v2 programs `pre0`, `preN` and `gtao2`: `src/ao.inl:440`.
  - `8` tessellation stage: `src/tess.inl:382`.
  - `16` light shader rewrite is made invalid: `src/light.inl:36`.
  - The release gate uses 31, all bits (`tools/g1_gate.ps1:4`).
- **Water flicker bisect.** `[pbr] DebugWater` is a bit mask (`src/pbr.inl:21-23`). Each set bit switches one term off for every draw, and logs `[dbgwater] term OFF: ...` (`src/pbr.inl:25`):
  - `1` specular, sky Fresnel and sheen: `src/pbr.inl:163`.
  - `2` bump and relief: `src/pbr.inl:134`, `:139`.
  - `4` atmosphere water fog: `src/atmos.inl:476`. The code calls this "best guess, untested".
  - `8` hemisphere light and shadow tint: `src/light.inl:46`.
  The shipped ini has `DebugWater=0` (`src/default_tyrender.ini:61`). The playtest variants `dbg_water_0`, `_1`, `_2`, `_4`, `_5` and `_8` run these bits (`tools/playtest/autoplaytest.ps1:59-64`). Issue #80 tracks the bisect (`tools/playtest/plans/next_monitor.json:6-17`).

### State leak detector

- `[debug] StateGuard=1` compares GL state between passes and logs each difference as `[state] LEAK after <pass>: <name> <before> -> <after>` (`src/stateguard.inl:39`). It logs at most four distinct lines per pass, then a counter (`src/stateguard.inl:3`).
- The switch is read every 300 frames (`src/stateguard.inl:32`). The check runs only when enabled (`src/stateguard.inl:33`).

### Capture one frame call by call

- Ways to start a capture: `[debug] CaptureFrame=N` (`src/tyrender.cpp:58`), `[debug] CaptureFrame2=N` (`src/tyrender.cpp:59`), the environment variable `TYRENDER_CAPTURE` (`src/tyrender.cpp:63`), or a file `tyrender\cap.req` polled every 8 frames (`src/tyrender.cpp:623-627`).
- During the captured frame, each hooked GL call logs its arguments. Sixteen hook bodies call the `CAP` macro, for example `glViewport`, `glBindTexture`, `glClear` and `glEnable` (`src/tyrender.cpp:50`, `:256-262`). Calls that are not hooked do not appear.
- The block starts with `=== BEGIN CAPTURE frame N ===` and ends with `=== END CAPTURE frame N ===` (`src/tyrender.cpp:628`, `:592`). `tools/capframe.ps1` copies the block out of the log (`tools/capframe.ps1:1-12`).

### Debug camera and motion capture

- `tyrender\cam.txt` holds `dx dy dz yawDeg pitchDeg`. The scene camera is moved before any pass sees it (`src/debugcam.inl:2-4`, `:16-40`). Keep moves under about 1500 world units to avoid popping (`src/debugcam.inl:5`).
- `tyrender\motion.req` runs a smooth yaw sweep and stores the frames for the offline flicker metric (`src/debugcam.inl:42-44`).

### GPU timers

- `[gpu]` and `[gpusub]` lines are logged every 300 frames (`src/gpuprof.inl:3`). Timers use GL timestamp queries read three frames late (`src/gpuprof.inl:1`).
- The header of `src/gpuprof.inl` says `[debug] GpuProf=0` disables them. No line in `src` reads a `GpuProf` key, so the switch is documented but not wired. See Discrepancies.

## Settings

Debug keys and the files that act like keys. Defaults are from code. "Live" means applied by the reload file (`src/tyrender.cpp:597-603`), which calls the loaders listed there. "Start" means read once at `DllMain` (`src/tyrender.cpp:714-725`).

| Key or file | Default in code | Shipped | Live or start | Effect |
|---|---|---|---|---|
| `[debug] ForceFallback` | `0` (`src/ao.inl:491`) | No | Live: `LoadAOConfig` is in the reload list | Forced failures, bit mask above. |
| `[debug] StateGuard` | `0` (`src/stateguard.inl:32`) | No | Live: read every 300 frames | State leak log. |
| `[debug] CaptureFrame` | `0` (`src/tyrender.cpp:58`) | No | Start | One-frame call log. |
| `[debug] CaptureFrame2` | `0` (`src/tyrender.cpp:59`) | No | Start | Second one-frame call log. |
| `[debug] DumpShaders` | `1` (`src/tyrender.cpp:60`) | No | Start | Writes `tyrender\dump`. |
| `[debug] OverrideShaders` | `1` (`src/tyrender.cpp:61`) | No | Start | Reads `tyrender\override`. |
| `[debug] ShadowView` | `0` (`src/light.inl:24`) | No | See NEEDS INVESTIGATION P12-10 | Shadow term alone. |
| `[pbr] Debug` | `0` (`src/pbr.inl:18`) | No | Live | Normals view. |
| `[pbr] DebugWater` | `0` (`src/pbr.inl:23`) | `0` (`src/default_tyrender.ini:61`) | Live | Water bisect bits. |
| `[tess] Debug` | `0` (`src/tess.inl:35`) | No | Live | Edge darkening. |
| `[tess] ABToggle` | `0` (`src/tess.inl:69`) | No | Live | Flips relief on file change (harness aid). |
| `[ao] DebugView` | `0` (`src/ao.inl:515`) | No | Live | AO view. File `ao_debug` overrides. |
| `[gi] DebugView` | `0` (`src/gi.inl:95`) | No | Live | GI view. File `gi_debug` overrides. |
| `[tex] Debug` | `0` (`src/tex.inl:67`) | No | Start (read in `LoadTexTags`, which is not in the reload list, `src/tyrender.cpp:725`) | Upload log. |
| `[tex] LogUnknown` | `0` (`src/tex.inl:13`) | No | Start (as above) | Draws per untagged texture. |
| `[clash] Check` | `1` (`src/tyrender.cpp:639`) | No | Start | Warns about the game's own SSAO or MSAA. |
| File `tyrender\ao_off`, `gi_off`, `atmos_off`, `post_off` | none | none | Polled every 8 frames (`src/tyrender.cpp:594-596`) | Turns one module off. Full list in [hotkeys-and-files.md](../reference/hotkeys-and-files.md). |

The "Live" column gives the reload path. Where a key is read by a loader that is not in the reload list, it is restart-only. The table is short on purpose. Settings tables live in the settings reference pages.

## Cost

Not measured.

- Debug views change the work done per frame, so their timings are not comparable with normal frames.
- `StateGuard` reads GL state at pass boundaries when enabled (`src/stateguard.inl:33`). Its cost is not measured here.
- `[tex] Debug` writes up to 500 upload lines (`src/tex.inl:195`).

## Limits

- **Debug views are exclusive.** The AO, GI and emitter passes skip themselves in some views (`src/tyrender.cpp:291-293`).
- **The log is recreated per start.** Copy `tyrender.log` before you restart (`src/tyrender.cpp:708`).
- **The StateGuard checks only what it records.** It returns early for an index outside 1 to 8 (`src/stateguard.inl:33`). The meaning of that index was not traced in this pass. A leak in state that is not recorded is not reported (inferred).
- **The Hi-Z match is by name.** Forced fallback bit 2 matches program names that start with `gi_`. Legacy GI programs do not match (inferred from the names in `src/gi2.inl:359-361`).
- **Capture is hook-only.** Calls outside the hook list are not logged (`src/tyrender.cpp:256-262`).

## Known issues

- **Water flicker on Ty (#5).** The bisect is the test; the fix is open. Issue #80 is the bisect follow-up (`tools/playtest/plans/next_monitor.json:6-17`). Issue #62 is listed with the water-debug work (`CHANGELOG.md:21`).
- **Overlay breaks geometry (#12).** The overlay key defaults to `0` in code (`src/overlay.inl:43`) and to `F10` in the shipped ini (`src/default_tyrender.ini:115`). An existing ini keeps its old `Key=0` (`CHANGELOG.md:22`). The in-level check is open (#79).
- **Shadow line on the bridge (#103).** Open at this commit (`README.md:215`).

## How to verify

Each step names the expected evidence.

1. **Crash.** After a crash, the session report ends with a `CRASH exception ... in <module> +0x<offset>` line (`src/sesslog.inl:365`).
2. **Shader failure.** `tyrender.log` has `does not compile` or `FAILED` lines (`docs/START_HERE.md:89`), and the DLL logs `[pbr] VS ... rewrite failed to compile` when a rewrite is rejected (`src/tyrender.cpp:380`).
3. **Forced fallback works.** Set `[debug] ForceFallback=8` (tessellation) and restart. The log shows `[fallback] FORCED tessellation stage compile failure` (`src/tess.inl:382`). The frame draws without relief.
4. **State leak.** Set `[debug] StateGuard=1` and restart. Play one minute. Look for `[state] LEAK after <pass>` lines (`src/stateguard.inl:39`).
5. **Water bisect.** Set `[pbr] DebugWater` to one bit. Check the log for one `[dbgwater] term OFF` line for that bit (`src/pbr.inl:25`). Compare the Ty region against bit `0` by eye, then with `tools/playtest/autoplaytest.ps1` and the `tyflicker` check (see [test-harness.md](test-harness.md)).
6. **One frame in detail.** Create `tyrender\cap.req` (or set `CaptureFrame`), wait a few frames, then extract the block with `tools/capframe.ps1 -Out <file>`. The block has BEGIN and END lines (`src/tyrender.cpp:592`, `:628`).
7. **Dumps.** After a start, `tyrender\dump` holds `.glsl` files named by hash (`src/tyrender.cpp:369-372`).

## NEEDS INVESTIGATION

- **P12-10** `[debug] ShadowView`. The comment in `src/light.inl:24` says "restart-only". `src/tyrender.cpp:598` calls `LoadLightConfig` on reload, and that loader reads the key. So reload probably applies it. Method: set `ShadowView=1`, create `tyrender\reload`, and see whether the picture changes without a restart.
- **P12-11** Values above `1` for `ao_debug`, `gi_debug` and `atmos_debug`. The headers document a few values (`src/ao.inl:5`, `src/gi.inl:6`, `src/atmos.inl:6`). Method: list the `u_debug` branches in the shader strings (`grep -n 'u_debug' src/*.inl`).
- **P12-12** Whether `[debug] GpuProf=0` should switch the GPU timers off. The header says so (`src/gpuprof.inl:3`), no code reads it. Method: grep `GpuProf` in the clone (no reader found) and ask the owner which behaviour is wanted.
- **P12-13** Visual meaning of `[pbr] DebugWater` bit `4` (water fog). The code marks it "best guess, untested" (`src/atmos.inl:476`). Method: run the bisect on the owner's PC.

## Discrepancies

1. **`src/gpuprof.inl:3` vs the code.** The header says `[debug] GpuProf=0` disables the timers. No code reads that key (`grep -rn GpuProf src` finds only comments). The plan's Appendix A lists `GpuProf=1` as a setting, and that key is not read either.
2. **`src/light.inl:24` vs `src/light.inl:39` vs `src/tyrender.cpp:598`.** The comment at line 24 says `ShadowView` is restart-only. The comment at line 39 says the debug view is live, with a reload. The reload path calls the loader that reads it. See P12-10.
3. **`README.md` ContactShadows default.** The README table gives `0` for `[atmos] ContactShadows` (`README.md:297`). The code default is `0.35` (`src/atmos.inl:287`, `:296`). The shipped value is not in `src/default_tyrender.ini`. This is recorded for the atmosphere and settings pages.
4. **Atmosphere fallback branches.** The log for forced bit `1` says "-> ray-march path" (`src/atmos.inl:257`). The fallback comment at `src/atmos.inl:371` says the atmosphere turns off with no ray-march fallback. The two describe different branches: `src/atmos.inl:371` applies only when `ContactShadows` is `0` or less, and otherwise the per-pixel path runs (`src/atmos.inl:453`). This needs a note in the atmosphere pages.
5. **`README.md` ForceFallback row.** The README says bit `1` is "fog" (`README.md:318`). The code says "froxel atmosphere" (`src/ao.inl:437`, `src/atmos.inl:257`). The effect is the same in practice, but the wording differs.
6. **First-pass `TROUBLESHOOTING.md` line numbers.** It cites `src/atmos.inl 260` for `InitVolume`. The function starts at `src/atmos.inl:254`.

## See also

- [test-harness.md](test-harness.md): the scripts that drive the game for these checks.
- [ab-captures.md](ab-captures.md): the F11 pair and debug views.
- [tester-guide.md](tester-guide.md): what testers send.
- [../reference/log-and-error-messages.md](../reference/log-and-error-messages.md): every log tag.
- [../how-it-works/11-fallbacks-and-failure-handling.md](../how-it-works/11-fallbacks-and-failure-handling.md): the fallback policy.
- [../decisions/known-issues.md](../decisions/known-issues.md): the open issues behind these symptoms.

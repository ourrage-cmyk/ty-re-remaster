# Fallbacks and failure handling

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), B (tester: what a missing effect means), C (maintainer).

## Purpose

This page says what happens when something the renderer needs is missing or fails: a GL function, a GL version, a shader that will not compile, a tessellated program that will not link, or a pass that cannot allocate. It lists every failure point in the code, what is switched off, what is logged, and how to force each failure on purpose to test the path. The policy in one line: a failing effect is switched off for the rest of the session, and the game keeps drawing. The rule comes from the project's own plan (docs/FIX_PLAN_20261016.md:7, "failure of an effect = effect OFF").

## How it works

### The policy

1. **A failing effect is off, not slow.** Most passes have a `failed` flag. Once it is set, the pass returns at the top of its run function and draws nothing (examples: ao.inl:534, 568; gi.inl:105; atmos.inl:362; fx.inl:178; emit.inl:227).
2. **The flags are sticky.** No code path clears a `failed` flag. A search for `failed = false` finds nothing. A live reload does not retry a failed pass. Only a restart does (inferred from the absence of any reset; tyrender.cpp:597-603 reloads settings, not flags).
3. **Two exceptions have a legacy path, and both are chosen in the ini.** AO has a v1 GTAO path (`[ao] Legacy=1`, ao.inl:506, 546-547). GI has a legacy path (`[gi] Mode=0`, gi.inl:95). Neither is used as an automatic fallback from the newer path (see Discrepancies).
4. **Shader rewrites fall back to the game's own shader.** A rewritten shader is compiled in a scratch object first. If it fails, the original source is used for that program (tyrender.cpp:398, 471-482).
5. **Nothing reaches the screen as a message.** No launcher, overlay or in-game text reports a fallback (inferred: a search of launcher_ui.inc, ui_part_a.inc, ui_part_c.inc, overlay.inl and TYLauncher.c for fallback or failure text finds only launch errors). The record is in `tyrender.log` and the session report.

### The failure ladder

Each row is one failure point. "Effect after" is what the frame looks like once it has happened, for the rest of the session.

| # | Failure | Detected at | Logged | Effect after |
|---|---|---|---|---|
| F1 | GL entry point missing (AO, GI, atmosphere, post, shared `LoadGX`) | `LoadGX`, ao.inl:42-53 | `[ao] missing gl<Name>` (ao.inl:45) or `[ao] engine did not request a needed GL entry point` (ao.inl:50) | that pass sets its `failed` flag (ao.inl:536; gi.inl:106; atmos.inl:363; fx.inl:179) |
| F2 | AO v2 programs (`kPre0`, `kPreN`, `kGTAO2`) fail to compile or are forced off | `InitAO`, ao.inl:543-547 | `[fallback] AO v2 unavailable -> AO OFF (no legacy fallback)` (ao.inl:546), plus session event `AO turned off: v2 shader unavailable` | AO off. With `[ao] Legacy=1` the v1 path is used instead (ao.inl:547) |
| F3 | Any AO program compile or link fails | `CompileProg`, ao.inl:440-455 | `[ao] <name> fs|vs compile FAILED` with the info log (ao.inl:450), `[ao] <name> link FAILED` (ao.inl:455) | `CompileProg` returns 0, which leads to F2 or to the pass being unusable |
| F4 | Hi-Z GI cannot set up (no `glDrawBuffers`, programs fail, framebuffers incomplete) | `GIHizInit`, gi2.inl:353, 362, 387 | `[gi] Hi-Z disabled: glDrawBuffers missing`, `[gi] Hi-Z shaders failed, using legacy path`, `[gi] Hi-Z framebuffers incomplete, using legacy path` (the last two words are wrong; see Discrepancies) | `g2.failed` set. `RunGI` then sets `gi.failed` when `Mode` is not 0 (gi.inl:130). GI is off. The legacy path is not used |
| F5 | Legacy GI programs fail | gi.inl:133-137 | none of its own. `CompileProg` logs the compile failure (F3) | `gi.failed` (gi.inl:136). GI off |
| F6 | Froxel volume: GL below 4.3 | `InitVolume`, atmos.inl:258-260 | `[atmos] froxel: GL <version> lacks compute, using ray-march path` (atmos.inl:260) | `vol.failed`. Then the rule in F12 applies |
| F7 | Froxel volume: compute entry points missing | atmos.inl:261-264 | `[atmos] froxel: missing entry points` | as F6 |
| F8 | Froxel volume: compute shader compile or link fails | atmos.inl:265-270 | `[atmos] froxel compute compile FAILED` or `link FAILED` (with info log); session failure `atmos compile` or `atmos link` (atmos.inl:267, 270) | as F6 |
| F9 | Froxel 3D texture cannot be allocated | `EnsureVolTex`, atmos.inl:279-280 | `[atmos] froxel 3D texture allocation failed (WxHx64)` | `vol.failed` |
| F10 | Atmosphere program fails to compile, or `LoadGX` fails | atmos.inl:363, 365-366 | from `CompileProg` (F3) | `atm.failed`. Atmosphere off |
| F11 | Sky-colour estimate program fails | atmos.inl:418-419 | from `CompileProg` (F3) | `skyE.failed`. Haze colour stays at its fixed value (atmos.inl:416) |
| F12 | Froxel unavailable, and the froxel check runs | atmos.inl:371 | `[fallback] froxel unavailable -> atmosphere OFF (no heavy ray-march fallback)`, session event `atmosphere turned off: froxel unavailable` | only when `[atmos] ContactShadows` is 0 (see the rule below) |
| F13 | Post programs (`kFXAA`, `kGrade`, `kTAA`) fail | fx.inl:181-184 | from `CompileProg` (F3) | `fx.failed`. No TAA, FXAA or grade for the session |
| F14 | Emitter shaders or entry points missing | emit.inl:106-109 | `[emit] shaders or GL entry points missing, emitter light off` | `g_emitInitFail`. Emitter off |
| F15 | Emitter framebuffer incomplete | emit.inl:125-127 | `[emit] emission framebuffer incomplete, emitter light off` | `g_emitInitFail`. Emitter off |
| F16 | Occlusion-query entry points missing (emitter ranking) | emit.inl:145-146 | `[emit] occlusion query entry points missing: ranking off, draw order cap` | emitter still draws, but without ranking |
| F17 | Shader rewrite does not compile | `TestCompile`, tyrender.cpp:398 | `override for <hash> does not compile -> original used` (tyrender.cpp:398); `[compile test] FAILED` with the log (tyrender.cpp:479) | the original shader is used for that program |
| F18 | Combined fragment rewrite fails, shadow patch alone is tried | tyrender.cpp:388-393 | `[fs] <hash> combined rewrite failed to compile (...) -> trying shadow patch alone` | shadow patch only, or original |
| F19 | PBR vertex rewrite fails | tyrender.cpp:379-380 | `[pbr] VS <hash> rewrite failed to compile` | the vertex shader is not rewritten |
| F20 | Tessellation vertex rewrite fails | `TessRewriteVS`, tyrender.cpp:566-571 | none | the original vertex shader is kept |
| F21 | Tessellation stage (TCS or TES) fails to compile | `CompileStage`, tess.inl:381-390; `TessBeforeLink`, tess.inl:402-405 | `[tess] stage <type> compile FAILED` with the log and the source (tess.inl:387) | the program links without tessellation |
| F22 | Tessellated program fails to link | `TessAfterLink`, tess.inl:412-430 | `[tess] program <n> tessellated link FAILED -> restoring original` with the log | stages are detached, the original vertex shader is restored and the program is relinked |
| F23 | GL below 4.0 with `[portability] TessVersionGate=1` | `TessBeforeLink`, tess.inl:397 | `[tess] GL below 4.0: tessellation stages skipped (TessVersionGate=1)` (once) | programs link without tessellation |
| F24 | Tessellation entry points missing | `TessEntryPointsOk`, tess.inl:87-96 | `[tess] GL entry points missing (shader compile/query/detach) -> tessellation off` | tessellation off for the session |
| F25 | Engine would link an invalid light rewrite | `LightRewriteFS`, light.inl:34-36 (only with `ForceFallback` bit 16) | `[fallback] FORCED invalid light shader rewrite -> must fall back` | the `#error` makes the compile fail, so F17 applies |
| F26 | Any fatal exception (access violation, illegal instruction, and so on) | `CrashVEH`, tyrender.cpp:685-699 | `!!! EXCEPTION <code> at <addr> (<module>+<offset>)` with a stack, then `SessCrash` | nothing recovers. The handler only logs, then returns `EXCEPTION_CONTINUE_SEARCH` (tyrender.cpp:698) |

### The compile-or-fall-back rule

Two checks guard every shader that the renderer writes or changes:

- **Scratch compile.** `TestCompile` compiles the candidate in a scratch shader object and reports the first 2048 bytes of the info log on failure (tyrender.cpp:471-482). It also records the failure in the session report (`SessFailure("compile test", ...)`, tyrender.cpp:479).
- **Final check.** A candidate that passes all rewrites is checked once more. If it fails, the game's original source is used and the reason is logged (tyrender.cpp:398).

The code comment says that the engine crashes with an int3 if it is given a bad shader (tyrender.cpp:397). The repo's documentation says the same (docs/START_HERE.md:84, docs/agent-tasks/BRIEFING.md:35), and adds that the game's own compile errors go to `TyOutputLog.txt` before the int3. Neither is checked by running the engine here (NI-Ch03-12).

### The atmosphere rule (F6, F12) in detail

The froxel check does not run in all cases. The condition at atmos.inl:371 is:

- `[atmos] Enable` on, `[atmos] Froxel` on, **and `[atmos] ContactShadows` equal to 0**, and `InitVolume()` false: the pass sets `atm.failed` and atmosphere is **off**.
- If `ContactShadows` is above 0, the check at atmos.inl:371 is skipped. The frame then reaches atmos.inl:453, `useVol` is false, and the frame uses the non-froxel path: the half-resolution path if `[atmos] HalfRes` is on (atmos.inl:481-491), or mode 0 (atmos.inl:493).

So "atmosphere is turned off when the compute path is unavailable" (the first-pass ARCHITECTURE.md, section 7) is true only for `ContactShadows=0`. The code default for `ContactShadows` is 0.35 (atmos.inl:287, 296). The shipped value is 0 (default_tyrender.ini:35), and the High and Ultra presets raise it (docs/PERFORMANCE.md:14, 15). The consequence is that the same failure gives two different results depending on the preset. Reported as NI-Ch03-10.

The log line at atmos.inl:260 says "using ray-march path", and the forced-failure line at atmos.inl:257 says "ray-march path". The ray-march path only runs in the `ContactShadows > 0` case above.

### Probes

The renderer does not run a GL test program to decide what the driver supports. It uses three cheap checks:

- **GL version string.** Compute (atmos.inl:258-260) needs 4.3 or newer, parsed from `GL_VERSION`. Tessellation needs 4.0 when `TessVersionGate=1` (tess.inl:76-85, 397). `GLAtLeast` returns true when the string cannot be parsed (tess.inl:81), so a bad string does not disable tessellation.
- **Entry points.** `wglGetProcAddress` results are checked for NULL before use. The lists are at ao.inl:42-53 (AO and shared), tess.inl:87-96 (tessellation), atmos.inl:261-264 (compute), gpuprof.inl:13 (timers) and emit.inl:104-109 (emitter).
- **Compile and link results.** Every shader and program is checked with `glGetShaderiv` and `glGetProgramiv` (for example ao.inl:450, 455; atmos.inl:266, 269; tess.inl:385, 417).

Image load and store is not probed separately. The compute path needs it, and it is covered only by the version check (NI-Ch03-11).

### Forcing each failure

`[debug] ForceFallback` is a bit mask read in `LoadAOConfig` (ao.inl:491). Its bits, as the code defines them (ao.inl:437):

| Bit | Forces | Where | Expected log line |
|---|---|---|---|
| 1 | froxel volume failure | atmos.inl:257 | `[fallback] FORCED froxel failure -> ray-march path` |
| 2 | every program whose name starts with `gi_` fails (Hi-Z GI) | ao.inl:441 | `[fallback] FORCED compile failure of GI program <name>` |
| 4 | AO v2 programs `pre0`, `preN`, `gtao2` fail | ao.inl:440 | `[fallback] FORCED compile failure of AO v2 program <name>` |
| 8 | tessellation stage compile fails | tess.inl:382 | `[fallback] FORCED tessellation stage compile failure -> programs relink without tessellation` |
| 16 | light shader rewrite fails | light.inl:36 | `[fallback] FORCED invalid light shader rewrite -> must fall back` |

`ForceFallback=31` sets bits 1, 2, 4, 8 and 16. The G1 release gate uses that value (tools/g1_gate.ps1:1). The mask does not cover the emitter, post, PBR, shadow patch or the sky estimate. The docs describe the 31 run as "every effect switched off" (docs/PERFORMANCE.md:40, docs/STATUS_2026-10-13.md:25). That wording is broader than the code. See Discrepancies.

`ForceFallback` is read on each reload (`LoadAOConfig` runs from the reload path, tyrender.cpp:598). A forced failure is applied when the program is compiled, so it takes effect for programs compiled after the change. Programs already compiled keep their state until the game restarts (inferred).

## Code pointers

| What | file:line | Note |
|---|---|---|
| Policy and comment | `src/ao.inl:437-441` | ForceFallback bits |
| `LoadGX` | `src/ao.inl:42-53` | shared entry-point check |
| `CompileProg` | `src/ao.inl:440-456` | forced failure, compile and link logs |
| AO fallback | `src/ao.inl:533-547` | v2 check and legacy choice |
| GI fallback | `src/gi.inl:104-139`; `src/gi2.inl:349-391` | |
| Atmosphere fallback | `src/atmos.inl:254-284`, `:362-371`, `:453-493` | |
| Post fallback | `src/fx.inl:177-184` | |
| Emitter fallback | `src/emit.inl:100-131`, `:145-146` | |
| Shader compile test | `src/tyrender.cpp:471-482` | |
| Shader rewrite order and final check | `src/tyrender.cpp:362-402` | |
| Tessellation probes and link | `src/tess.inl:76-98`, `:381-430`; `src/tyrender.cpp:566-587` | |
| Crash handler | `src/tyrender.cpp:685-699` | logs only |
| Session failure records | `src/sesslog.inl:66-75`, `:358-359` | |

## Settings

| Key | Code default | Shipped (Medium) | Live or restart | Effect |
|---|---|---|---|---|
| `[debug] ForceFallback` | 0 (ao.inl:491) | not set | live (reload) | bit mask above |
| `[portability] TessVersionGate` | 1 (tess.inl:31, 72) | 1 (default_tyrender.ini:102) | restart (read in `LoadTessConfig`, reloaded with it) | skips tessellation below GL 4.0 |
| `[ao] Legacy` | 0 (ao.inl:506) | 0 (default_tyrender.ini:4) | live | v1 AO when v2 is unavailable |
| `[gi] Mode` | 1 (gi.inl:95) | 1 (default_tyrender.ini:9) | live | 0 uses the legacy path by choice |
| `[atmos] ContactShadows` | 0.35 (atmos.inl:296) | 0 (default_tyrender.ini:35) | live | decides which atmosphere fallback applies (F12) |
| `[atmos] HalfRes` | 1 (atmos.inl:292) | not set | live | half-resolution fallback path |
| `[report] Enable` | 1 (default_tyrender.ini:93) | 1 | restart | session report, which records the failures |

Full tables: [settings-system-debug](../reference/settings-system-debug.md), [settings-atmos-fog-emit](../reference/settings-atmos-fog-emit.md), [settings-post-shadows-light-pbr-tess](../reference/settings-post-shadows-light-pbr-tess.md).

## Cost

- **Forced fallback frame time, v0.3.2, measured.** 7.44 ms against a default-view Medium frame of 12.08 ms (docs/PERFORMANCE.md:40, 36). The test is on 2560 by 1440 windowed, AMD Radeon RX 7600 XT.
- **G1 gate record, a different build.** docs/results/g1_gate.md (dll sha1 3EE5DA28A7) gives forced fallback 5.96 ms and balanced default 10.73 ms. The same file says "soak 10 min: FAIL" for that build. This is not reconciled with the PERFORMANCE.md figures (see Discrepancies).
- **What the fallback costs in quality** is not measured. The forced run switches effects off, so the frame time is a floor for the rest of the frame, not a visual comparison.

## Limits

- Failures are for the session only. A reload or a settings change does not retry a failed pass (see the policy).
- The probes do not test the driver. A driver that reports GL 4.3 but mishandles compute is not detected (inferred).
- There is no on-screen message. A tester who does not read the log or the session report will see an effect missing and no reason.
- The legacy GI and AO paths are selected by ini, not by failure, so a Hi-Z failure never reaches them.
- The crash handler does not recover. It records the first fatal exception and returns (tyrender.cpp:685-698).
- The `ForceFallback` bits test the code paths, not the driver. They do not stop the renderer from having another failure.

## Known issues

- **Misleading log text.** Two Hi-Z GI failures print "using legacy path" (gi2.inl:362, 387). In Hi-Z mode the legacy path is not used (gi.inl:130). A tester reading only the log would expect a different result.
- **Atmosphere log text.** The froxel failure line says "ray-march path" (atmos.inl:260). That path runs only when `ContactShadows` is above 0.
- **Silent legacy GI failure.** When the legacy GI shaders fail (F5) there is no GI-specific log line. The compile failure is logged by `CompileProg` (ao.inl:450).
- **No issue numbers.** Nothing in the repo records a real-world fallback that fired on a player's machine. The only evidence is the ForceFallback tests and the G1 gate.
- **Portability is untested.** GL below 4.0 and GL below 4.3 have never run on real hardware (docs/results/DIRECTION_2026-10-13_checkpoint3.md:12).

## How to verify

1. **Force a fallback.** Set `[debug] ForceFallback=<bit>` in `tyrender.ini` and wait up to 8 frames for the reload poll (tyrender.cpp:595-598). Expected: the log line in the table above, and the matching effect gone in-game. For bit 4, the "AO OFF" line (ao.inl:546) follows unless `[ao] Legacy=1`.
2. **Check the session report.** With `[report] Enable=1`, read `tyrender\logs\TYHD_report_*.txt`. Expected: `shader failure (<kind>), first time: ...` lines (sesslog.inl:74) and the `shader failure total` and `session end` lines (sesslog.inl:358-359).
3. **Atmosphere rule.** Run twice with `ForceFallback=1`: once with `[atmos] ContactShadows=0`, once with `ContactShadows=0.35`. Expected: atmosphere off in the first run (`atmosphere turned off: froxel unavailable`); the fog still drawn in the second run (NI-Ch03-10).
4. **Tessellation link fallback.** Set `ForceFallback=8`. Expected: `[fallback] FORCED tessellation stage compile failure ...`, then the programs link without tessellation. No `tessellated link FAILED` line should appear, because the stage never attached (tess.inl:402-405).
5. **Read the leak log in the same run.** `[debug] StateGuard=1` (see [frame anatomy](04-frame-anatomy-and-pass-order.md)). A fallback that leaves state changed would show there.
6. **GL floor.** Not testable here. On a GL 3.3 context, `[portability] TessVersionGate=1` should log `[tess] GL below 4.0` once. NI-Ch03-11.

## NEEDS INVESTIGATION

- **NI-Ch03-10.** What the frame shows when the froxel fails with `ContactShadows` above 0 (the non-froxel path). Method: `ForceFallback=1` with `[atmos] ContactShadows=0.35`, compare screenshots with `ContactShadows=0`. Tools: `tools/shot.ps1` (docs/AUTOPLAYTEST.md).
- **NI-Ch03-11.** Whether any driver reports GL 4.3 without image load and store. The code does not check, so any answer needs a test on that hardware. Also the GL 3.3 and 4.0 behaviour (never run, docs/results/DIRECTION_2026-10-13_checkpoint3.md:12). Method: a context-version override or another GPU (N-21 in the master list).
- **NI-Ch03-12.** The engine's int3 on a bad shader (tyrender.cpp:397; docs/START_HERE.md:84). Method: feed the engine a deliberately invalid shader through a test build that skips `TestCompile`, and watch `TyOutputLog.txt`. Do not ship that build.
- **NI-N-21** (master list). Driver quirks on NVIDIA, Intel and others. Community logs (`tyrender.log`, session reports) are the source.

## Discrepancies

- **Hi-Z GI "legacy path" wording.** The header of gi2.inl (line 1) says the Hi-Z pass "falls back to the legacy linear-march path in gi.inl on any failure". The code does not do that when `Mode` is 1 (gi.inl:130). Log text at gi2.inl:362 and 387 also says "using legacy path". The code is the reference: GI is off.
- **Atmosphere off rule.** First-pass ARCHITECTURE.md section 7 says atmosphere is turned off when the GL version is below 4.3. True only for `[atmos] ContactShadows=0` (atmos.inl:371). The code default is 0.35.
- **Atmosphere log text.** atmos.inl:257 and 260 say "ray-march path". That path is not used for the froxel failure in the `ContactShadows=0` case.
- **"Every effect" in the forced run.** docs/PERFORMANCE.md:40 and docs/STATUS_2026-10-13.md:25 say `ForceFallback=31` switches off every effect. The code's bits cover froxel atmosphere, Hi-Z GI, AO v2, tessellation and the light rewrite only (ao.inl:437). PERFORMANCE.md:65 names bit 1 "fog" and bit 8 "relief", which are not the code's names.
- **Forced-fallback numbers.** docs/PERFORMANCE.md (v0.3.2) gives 7.44 ms and "soak test: 10 minutes ... no crash" (lines 40, 44). docs/results/g1_gate.md (build 3EE5DA28A7) gives 5.96 ms and "soak 10 min: FAIL". Different builds. Not reconciled.
- **Probe list in the plan.** The plan (section 2 of the master plan, Ch03 page outline) names probes for "image load-store". None are in the code (see Probes above).
- **`FIX_PLAN` citation.** The first-pass docs say the policy is in FIX_PLAN section 2. It is the rules paragraph at docs/FIX_PLAN_20261016.md:7.

## See also

- [Frame anatomy and pass order](04-frame-anatomy-and-pass-order.md)
- [Render targets, depth and state](06-render-targets-depth-and-state.md)
- [Logging, timers and session report](10-logging-timers-and-session-report.md)
- [Testing and the G1 gate](../guides/test-harness.md)
- [Shader inventory](../shaders/README.md)
- [Known issues](../decisions/known-issues.md)

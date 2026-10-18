# Logging, timers and the session report

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A, B.

## Purpose

The renderer writes three kinds of output while the game runs. `tyrender.log` is a plain text log for developers, with one line per event and a few summary lines. GPU timer queries give per-pass GPU times, printed as `[gpu]` and `[gpusub]` lines. The session report is a separate text file for testers, with system basics, frame times for the first minutes and a list of events. This page explains how each one is produced and when it is written. The meaning of each log line is in [log-and-error-messages.md](../reference/log-and-error-messages.md). The report from the tester's side is in [session-report.md](../guides/session-report.md).

## How it works

### 1. `tyrender.log`

1. `DllMain` opens `<game folder>\tyrender.log` with `fopen(..., "w")` (`src/tyrender.cpp:708`). The file is replaced at every game start. There is no rotation and no size limit in `logf` (`src/tyrender.cpp:44-49`).
2. `logf` writes the message and a newline, and flushes after each line unless a call-by-call capture is running (`tyrender.cpp:46-48`). The lines carry no timestamp. The session report has timestamps, the log does not.
3. Start-up lines: the IAT hook check `[iat] <name> orig=... module=...` (`tyrender.cpp:677-681`, the module path is the full path), `tyrender stage1 loaded` (`tyrender.cpp:726`), and `hooks installed` (`tyrender.cpp:732`). On the first `wglMakeCurrent` that succeeds, the GL vendor, renderer, version and GLSL strings are logged once (`tyrender.cpp:493-499`).

### 2. Frame average line

`h_SwapBuffers` increments `g_frame` (`tyrender.cpp:612`). At frame 1 it records the time (`tyrender.cpp:615`). Every 600 frames it logs `[frame N] avg X ms/frame (Y fps)` with the wall-clock average since frame 1, the draw count of the last frame, the number of tessellated programs and the count of skipped tess draws (`tyrender.cpp:616-620`). This is a CPU-side wall-clock number and includes any vsync wait. It is not a GPU time.

### 3. GPU timer queries (`src/gpuprof.inl`)

1. **Set-up.** Timer query functions are looked up through `o_wglGetProcAddress` (`gpuprof.inl:9-16`). The DLL creates 22 query objects for each of 4 ring slots (`gpuprof.inl:6`, `14`). If any of the four functions is missing, `gp.failed` is set and no GPU lines are ever written. Nothing is logged in that case (`gpuprof.inl:13`).
2. **Marks.** `GProf(m)` records a `GL_TIMESTAMP` query for mark `m` in the current slot (`gpuprof.inl:17-21`). Mark 0 starts the frame (`gpuprof.inl:44`). Mark 9 is the end of the frame's commands, recorded before `SwapBuffers` so that the vsync wait is not counted (`src/tyrender.cpp:606`).
3. **Read-back.** After `SwapBuffers`, `GProfNextFrame` (`gpuprof.inl:23-45`) advances the slot and reads the oldest one, which is three frames old (`gpuprof.inl:25`). It only reads a slot whose mark 0 and mark 9 are set and whose end query reports the result is available (`gpuprof.inl:27-28`). This avoids a stall.
4. **Durations.** Each pair of marks gives one duration in milliseconds (`gpuprof.inl:32-33`):

   | Name in the log | Marks | Where the marks are set (examples) |
   |---|---|---|
   | frame | 0 to 9 | `gpuprof.inl:44`, `tyrender.cpp:606` |
   | ao | 1 to 2 | `tyrender.cpp:290` |
   | gi | 3 to 4 | `tyrender.cpp:291` |
   | atmos | 5 to 6 | `tyrender.cpp:293` |
   | post | 7 to 8 | `tyrender.cpp:254` |
   | sub0 | 10 to 11 | `ao.inl:618`, `660` |
   | sub1 | 12 to 13 | `ao.inl:662`, `692` |
   | sub2 | 14 to 15 | `ao.inl:692`, `710` |
   | sub3 | 16 to 17 | `ao.inl:629`, `651` |
   | sub4 | 18 to 19 | `tyrender.cpp:296-298` (colour copy) |
   | emit | 20 to 21 | `tyrender.cpp:292` |

   The `sub0` to `sub4` names are the code's own; the log does not say what they are. The mark locations are the only description.

5. **Output.** Samples are accumulated (`gpuprof.inl:33`) and every sample is also passed to the session report (`gpuprof.inl:34`). When 300 samples have been collected (`gpuprof.inl:35`), the averages are logged as `[gpu] frame ... | ao ... gi ... atmos ... post ... | engine+rest ...` and `[gpusub] sub0 ... emit ...` (`gpuprof.inl:37-38`). "engine+rest" is the frame time minus ao, gi, atmos and post (`gpuprof.inl:36`).
6. **Switch.** `g_gpEnable` is a constant 1 (`gpuprof.inl:5`). No ini key is read. The header comment says "Disable: [debug] GpuProf=0" (`gpuprof.inl:3`); that is not implemented (see Discrepancies).

### 4. Capture on demand (call-by-call log)

1. **Frame numbers.** `g_capFrame1` and `g_capFrame2` come from `[debug] CaptureFrame` and `[debug] CaptureFrame2` (`src/tyrender.cpp:58-59`). The environment variable `TYRENDER_CAPTURE` overrides the first one (`tyrender.cpp:62-63`). These are read once at start-up (`tyrender.cpp:714`).
2. **Trigger file.** Every 8 frames, if `<game folder>\tyrender\cap.req` exists, it is deleted and the next frame is captured (`tyrender.cpp:624-627`).
3. **Logging.** `g_cap` is set for the chosen frames (`tyrender.cpp:622`). During it, `=== BEGIN CAPTURE frame N ===` is written (`tyrender.cpp:628`), and the `CAP(...)` macro (`tyrender.cpp:50`) and the other `if (g_cap ...)` lines log each GL call the hooks see. `=== END CAPTURE frame N (draws=...) ===` is written at the start of the next `SwapBuffers` and the log is flushed (`tyrender.cpp:592`).
4. **Tool.** `tools/capframe.ps1` creates `cap.req`, waits up to 10 seconds for the END line, and copies the block from BEGIN to END into the output file. Its `-Game` default is a hard-coded workstation path (plan item N-13), so pass the game folder explicitly.

### 5. Session report

The report is written by `src/sesslog.inl`, which is included from `tyrender.cpp:549`. Its life cycle:

1. **Configuration.** `SessInit` (`sesslog.inl:370-376`) reads `[report]`. It runs at the end of `DllMain` (`tyrender.cpp:731`). Settings are read once; restart is needed to change them.
2. **Open.** The file is created on the first `SwapBuffers` (`sesslog.inl:327-330`, `SessOpen` at `258`). Its name is `TYHD_report_<yyyymmdd-hhmmss>_<build>.txt` in `<game folder>\tyrender\logs\` (`sesslog.inl:262-264`). Older reports beyond `KeepFiles` are deleted first (`SessPrune`, `sesslog.inl:191-201`, called at `267`).
3. **Header.** The file starts with a statement of what is and is not in it (`sesslog.inl:270-278`), the build and preset (`280-284`), system basics (`SessSystem`, `220-238`), GL strings, limits and five extension flags (`SessGraphics`, `240-255`), the window size, then the game's `settings.ini` (up to 200 non-comment lines, `294-310`) and the effective `tyrender.ini` values (`312-320`).
4. **Frame rows.** `SessFrame` (`sesslog.inl:327-344`) measures the wall-clock time between two `SwapBuffers` returns (`sesslog.inl:332`), checks for GL errors (`334`) and window size or fullscreen changes (`335-342`). `SessPerf` (`203-215`) groups the frame times per second and writes one row per second (`SessFlushRow`, `151-170`). The row includes the average, minimum and maximum frame time, fps, and the mean of the slowest 1 percent of frames. When GPU samples exist, the row also has the GPU averages.
5. **Summary.** After `CaptureSeconds` (default 300, clamped to 0 to 3600, `sesslog.inl:373`), the window closes. `SessSummary` writes the summary (`sesslog.inl:171-188`), then `capture window closed after N s` (`sesslog.inl:209`). Only events are written from then on.
6. **Events.** `SessEvent` writes `[mm:ss.mmm fN] text` (`sesslog.inl:56-63`). Sources are listed in [log-and-error-messages.md](../reference/log-and-error-messages.md).
7. **Close.** `SessShutdown` writes the GL error totals, shader-failure totals and a `session end: <how>` line (`sesslog.inl:346-361`). It is called with `clean exit` from `DllMain` on detach (`tyrender.cpp:702`) and with `crash` from `SessCrash` (`sesslog.inl:362-367`).
8. **Size.** Each report is capped by `MaxBytes` (default 1 MiB, minimum 64 KiB, `sesslog.inl:374`). Ordinary lines stop 4 KB before the cap so the end and crash lines always fit (`sesslog.inl:42-46`). The first line dropped writes `[cap] size limit reached, further events are dropped` (`sesslog.inl:44`).

### 6. Crash output (`CrashVEH`)

1. `AddVectoredExceptionHandler(1, CrashVEH)` is called in `DllMain` (`tyrender.cpp:713`). The handler is first in line.
2. Only some codes are handled: access violation, breakpoint, illegal instruction, stack overflow, integer divide by zero, and `0xC0000409` (`tyrender.cpp:687-689`). Others return to the search chain.
3. Only the first such exception is logged (`static LONG once`, `tyrender.cpp:689`).
4. The log gets `!!! EXCEPTION <code> at <addr> (<module full path> +0x<offset>) frame=N stage=... prog=... tessProg=... fbo=...` (`tyrender.cpp:692`). For an access violation, a line says whether it was a read or a write and the address (`tyrender.cpp:693`). Up to 24 backtrace lines follow with module base names (`tyrender.cpp:694-695`).
5. The report gets `CRASH exception <code> in <module base name> +0x<offset>` and then the `session end: crash` line (`sesslog.inl:362-367`).
6. The `stage=` field always prints `init`. The variable `g_lastStage` is initialised once (`tyrender.cpp:684`) and never assigned again (grep of `src/`).

### 7. Log retention

| Output | Where | Kept how long | Limit |
|---|---|---|---|
| `tyrender.log` | `<game folder>\tyrender.log` | until the next game start (`tyrender.cpp:708`) | none |
| Session report | `<game folder>\tyrender\logs\` | newest `KeepFiles` (default 10, minimum 1) | `MaxBytes` each (default 1 MiB) |
| F11 screenshots | `<game folder>\Screenshots\` | not managed by the DLL | none |
| `snaps\` output | `<game folder>\tyrender\snaps\` | not managed | none |

`tools/ty_play.ps1` deletes `tyrender.log` before each boot (`tools/ty_play.ps1:12`).

## Code pointers

| What | file:line | Note |
|---|---|---|
| `logf` | `src/tyrender.cpp:44-49` | no timestamp, no cap, flush unless capturing |
| `CAP` macro | `src/tyrender.cpp:50` | logs only during a capture |
| `[debug]` capture keys | `src/tyrender.cpp:58-63` | read at start-up |
| `tyrender.log` open | `src/tyrender.cpp:708` | `"w"` |
| `h_SwapBuffers` order | `src/tyrender.cpp:591-630` | see [09](09-overlay-input-and-hotkeys.md) |
| `[frame N]` line | `src/tyrender.cpp:612-620` | 600-frame average |
| `cap.req` poll | `src/tyrender.cpp:624-628` | every 8 frames |
| Timer set-up | `src/gpuprof.inl:9-16` | optional extension |
| Timer marks | `src/gpuprof.inl:17-21` | `SGMark` also runs here |
| Timer read-back | `src/gpuprof.inl:23-45` | slot three frames old |
| `[gpu]` / `[gpusub]` output | `src/gpuprof.inl:35-39` | per 300 samples |
| `SessInit` | `src/sesslog.inl:370-376` | `[report]` keys |
| `SessOpen` | `src/sesslog.inl:258-325` | header and settings dump |
| `SessFrame` | `src/sesslog.inl:327-344` | frame times, window changes |
| `SessShutdown` | `src/sesslog.inl:346-361` | totals and end line |
| `SessCrash` | `src/sesslog.inl:362-367` | `CRASH` line |
| `SessPrune` | `src/sesslog.inl:191-201` | keep newest N |
| VEH handler | `src/tyrender.cpp:685-700` | first exception only |
| Handler registration | `src/tyrender.cpp:713` | priority 1 |
| `DllMain` | `src/tyrender.cpp:701-734` | start-up order |

## Settings

| key | default in code | shipped (`default_tyrender.ini`) | live or restart | effect |
|---|---|---|---|---|
| `[report] Enable` | 1 (`sesslog.inl:372`) | 1 (`:93`) | restart | 0 = no report file |
| `[report] CaptureSeconds` | 300 (`sesslog.inl:373`) | 300 (`:94`) | restart | length of the frame-time window; 0 = no rows |
| `[report] MaxBytes` | 1048576 (`sesslog.inl:374`) | 1048576 (`:95`) | restart | size cap per report; minimum 65536 |
| `[report] KeepFiles` | 10 (`sesslog.inl:375`) | 10 (`:96`) | restart | reports kept; minimum 1 |
| `[debug] CaptureFrame` | 0 (`tyrender.cpp:58`) | not shipped | restart | frame to capture call by call |
| `[debug] CaptureFrame2` | 0 (`tyrender.cpp:59`) | not shipped | restart | second frame to capture |
| `[debug] GpuProf` | documented as 0 to disable (`gpuprof.inl:3`) | not shipped | not read | no effect; see Discrepancies |
| `TYRENDER_CAPTURE` | not set | not applicable | restart | overrides `CaptureFrame` |

Settings are described in full in [settings-system-debug.md](../reference/settings-system-debug.md).

## Cost

Not measured.

- `tyrender.log`: one `fprintf` and one `fflush` per line, except during a capture (`tyrender.cpp:46-48`). The log grows with the number of hooked calls, so a capture is much heavier than normal play.
- GPU timers: up to 22 timestamp queries per frame, depending on which passes run (marks 0 to 21, see the table in "How it works"), read three frames late (`gpuprof.inl:1`). The overhead has not been measured.
- Session report: one row per second, plus events. Bounded by `MaxBytes`.

## Limits

- The GPU pass times only cover the marked regions. The owner's note is that per-pass timers under-count because hidden full-resolution copies and resolves are not timed (`docs/START_HERE.md:76`, a historical snapshot). The code does not time copies outside the marks (inferred).
- The `[frame N]` line and the report's frame times are wall-clock, so they include any vsync or driver wait. They are not GPU times and should not be compared with the `[gpu]` line.
- The GPU lines need `GL_TIMESTAMP` support. If it is missing, the DLL silently writes no GPU lines (`gpuprof.inl:13`). The log does not say so.
- Only the first crash is logged (`tyrender.cpp:689`).
- The `stage=` field of the crash line is useless because it is always `init` (`tyrender.cpp:684`).
- Shader failure events go to the report only from the AO, atmosphere and compile-test paths (`SessFailure` callers: `src/ao.inl:450`, `455`; `src/atmos.inl:267`, `270`; `src/tyrender.cpp:479`). Tessellation compile and link failures, and Hi-Z GI set-up failures, are written to `tyrender.log` only (`src/tess.inl:387`, `421`; `src/gi2.inl:362`, `387`) (inferred from the call sites).
- The `[gpu]` line appears after 300 samples. With a slow or lagging GPU, that can be much more than 300 frames.

## Known issues

- No issue number is recorded for the log or report code in the repo; the report was added in the Unreleased section of the CHANGELOG ("Session report for bug reports").
- The `[gpu]` line and the `[debug] GpuProf` switch do not match the documentation (see Discrepancies).
- The owner's measurements quoted in `docs/PERFORMANCE.md` refer to "[gpu] / PERF lines" (`docs/PERFORMANCE.md:3`). No `PERF` line exists in the source. The `[gpu]` and `[frame N]` lines are the only sources.

## How to verify

1. **Start-up lines.** Start the game once. Open `<game folder>\tyrender.log`. Expected order: `tyrender stage1 loaded` (from `DllMain`, before any GL context exists), the `[iat]` lines, `hooks installed`, then, once the game creates its GL context, `wglCreateContext`, `wglMakeCurrent`, and the four `GL_VENDOR` / `GL_RENDERER` / `GL_VERSION` / `GLSL` lines. No `[iat]` line should say `NOT FOUND`.
2. **Frame average.** Play for 600 frames (about 10 to 20 seconds at 30 to 60 fps). Expected: a `[frame 600] avg ... ms/frame` line.
3. **GPU lines.** After 300 GPU samples, expect `[gpu] frame ... | ao ...` and `[gpusub] sub0 ...`. If they never appear, check the log for `GL_TIMESTAMP`-related failures and the `[debug] GpuProf` note in Discrepancies.
4. **Capture.** Create an empty file `<game folder>\tyrender\cap.req`. Wait about a second. Expected: `=== BEGIN CAPTURE frame N ===`, many `glXxx` lines, then `=== END CAPTURE frame N ...`. Or run `tools/capframe.ps1 -Out <file>`.
5. **Report.** Open the newest `<game folder>\tyrender\logs\TYHD_report_*.txt`. Expected: the header, a `== tyrender.ini ==` section, one row per second for five minutes, then `== performance summary ==`.
6. **Crash path.** Not run on the owner's machine. The CRASH lines are described above; test only on a disposable copy.

## NEEDS INVESTIGATION

- **NI-L1: how much do the timer queries cost?** Method: run `tools/perf.ps1` or `tools/g1_gate.ps1` with and without the GPU-timer code path (requires a build change, so this is a follow-up). Plan item N-20.
- **NI-L2: is the 300-sample cadence close to 300 frames on other GPUs?** Method: count `[frame]` lines against `[gpu]` lines in a long run, on NVIDIA and Intel (N-21).
- **NI-L3: does the missing-`GL_TIMESTAMP` case happen on any real driver?** Method: list the GL extensions in the report's graphics section (`SessGraphics`) on each reported system; the report only checks the names in `kExt`, not `GL_TIMESTAMP`.
- **NI-L4: is the copy and resolve time the reason the per-pass times under-count?** Method: add two marks around the colour copy and the depth copy (`tyrender.cpp:296-298` and the depth copy near `:280`) and compare the sum with `frame`. Owner's claim only (plan item N-20 and Q123).
- **NI-L5: meaning of `sub0` to `sub4`.** The code gives no names. Method: the mark table above is the best source; confirm with the owner.

## Discrepancies

1. **`[debug] GpuProf=0` is documented but not read.** `src/gpuprof.inl:3` says "Disable: [debug] GpuProf=0". `g_gpEnable` is a constant (`gpuprof.inl:5`), and no `GetPrivateProfile*` call names `GpuProf` anywhere in `src/` or `docs/`. The plan's Appendix A lists `GpuProf=1 (gpuprof.inl)` as a key, which the code does not support. Code wins: the GPU timers cannot be switched off from `tyrender.ini`.
2. **Cadence of `[gpu]`.** `gpuprof.inl:3` says "every 300 frames". Code counts 300 completed samples (`gpuprof.inl:35`). The two only match when every sample arrives in time.
3. **`PERF` lines.** `docs/PERFORMANCE.md:3` and the plan (Q122) refer to "[gpu] / PERF lines". No `PERF` log line exists in `src/`.
4. **Crash `stage=` field.** Documented as the last stage (`tyrender.cpp:692`), but always `init` (`tyrender.cpp:684`).
5. **Privacy statement scope.** The launcher's "Report a problem" text says the report holds "no names or personal paths" (`src/launcher_ui.inc:479`). That is true of the session report file. It is not true of `tyrender.log`, which writes full local paths. See [session-report.md](../guides/session-report.md).
6. **First-pass `OVERLAY_AND_HOTKEYS.md`.** Section 7 says `[gpu]` is produced "every 300 frames" (same as item 2). Section 4 cites `src/tyrender.cpp` line 595 for `post_off`; the read is at line 596 (`tyrender.cpp:596`).

## See also

- [log-and-error-messages.md](../reference/log-and-error-messages.md): every log tag and what it means.
- [session-report.md](../guides/session-report.md): how testers read and send the report.
- [09-overlay-input-and-hotkeys.md](09-overlay-input-and-hotkeys.md): the per-frame order and the F10 and F11 features that log here.
- [hotkeys-and-files.md](../reference/hotkeys-and-files.md): the `cap.req` and other trigger files.
- [debugging.md](../guides/debugging.md): how the log is used in a bug hunt.

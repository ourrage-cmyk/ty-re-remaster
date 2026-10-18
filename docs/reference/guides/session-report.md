# Session report: how to find, read and send it

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: B (testers), C.

## Purpose

Each time the game starts and draws its first frame, the renderer writes a text file called the session report. It has your system basics, your settings, frame times for the first five minutes, and a list of events such as errors, settings changes and screenshots. It is written only to your PC. Nothing is sent by the code. You choose what to send, and this page explains what is in the file, how to read it, and what to check before you send it.

The file is produced by `src/sesslog.inl`. How it is written is on [10-logging-timers-and-session-report.md](../how-it-works/10-logging-timers-and-session-report.md). Every line type is in [log-and-error-messages.md](../reference/log-and-error-messages.md).

## Where it is

- **Folder:** `<game folder>\tyrender\logs\`. The file name is `TYHD_report_<yyyymmdd-hhmmss>_<build>.txt` (`src/sesslog.inl:262-263`). The build id is in the launcher title bar.
- **Launcher button:** Advanced tab, "Report a problem / open logs folder" (`src/launcher_ui.inc:479`; the tab is listed at `:472`). It opens that folder in Explorer and shows a status line. It does not send anything (`src/launcher_ui.inc:500`).
- **When it appears:** when the game draws its first frame, not when the launcher starts (`src/sesslog.inl:327-330`). If the game crashes before any frame is drawn, there may be no report (inferred).
- **Builds:** the session report is listed in the Unreleased section of the CHANGELOG and not in the v0.3.5 section. Older builds therefore do not write one (inferred from the CHANGELOG).

## What is in the file

The file has these parts, in this order. The headings are written exactly as shown.

1. **Header.** Lines that say what the file contains and what it does not contain (`src/sesslog.inl:270-278`). The build id, version, start time, preset and report settings follow (`:280-284`).
2. **`== system ==`.** Windows version and build, OS architecture (32 or 64 bit), CPU model, number of logical cores, RAM in MB, and the current display size and refresh rate (`src/sesslog.inl:220-238`).
3. **`== graphics ==`.** GPU vendor, GPU model string, OpenGL version (it includes the driver version), GLSL version, GL limits (texture size, texture units, draw buffers, MSAA samples) and five extension flags (`src/sesslog.inl:240-255`). The model string is whatever the driver reports, which can be generic.
4. **`== window ==`.** Client area size and windowed or fullscreen (`src/sesslog.inl:288-292`).
5. **`== game settings.ini ... ==`.** The game's own `settings.ini` from the Documents folder, comments removed, up to 200 lines (`src/sesslog.inl:294-310`).
6. **`== tyrender.ini, effective values as written ... ==`.** Every key and value in the mod's `tyrender.ini`, grouped by section (`src/sesslog.inl:312-320`). A key that is not in the file uses its built-in default, and the header says so.
7. **`== frame times, one row per second, then a summary ==`.** One row for each second of the first `CaptureSeconds` (default 300). Then the summary.
8. **`== events after the capture window ==`.** Events from then on.

### Reading a frame-time row

Format from the code (`src/sesslog.inl:167-168`; the numbers below are placeholders, not measurements):

```
t=  12 s  frames   60  frame ms avg  16.70 min  15.10 max  20.10  fps   59.9  1% low fps   45.0 | gpu ms: frame 9.00 ao 1.00 gi 2.00 atmos 1.50 post 0.80
```

- `t=` is the second of the session, counted from the start of the report.
- `frames` is the number of frames in that second.
- `frame ms avg / min / max` is the time between two frame presents, in milliseconds. Large `max` values are stutters.
- `fps` is 1000 divided by the average.
- `1% low fps` is the frame rate implied by the slowest 1 percent of frames in that second. It is a better measure of stutter than the average.
- `gpu ms:` appears only when GPU timer samples exist for that second. The GPU numbers are per pass, averaged. See [10](../how-it-works/10-logging-timers-and-session-report.md) for what each pass covers, and for why the GPU numbers and the frame numbers can disagree.

### Reading the summary

After the window closes (`src/sesslog.inl:171-188`), the file has:

- `== performance summary: capture window, N s, N frames ==`. If the session ended before the window, the word `capture window` is replaced by `ended early`.
- `average frame`, `median frame`, `1% low`, `0.1% low`, `worst frame`, and `hitches (frame over 50 ms)`. A hitch is any frame over 50 ms.
- `gpu ms averages` for the whole window, if GPU samples exist.

### Reading the events

Each event line starts with `[mm:ss.mmm fN]`: time since the start of the report, and the frame count. The common ones:

| Event | Meaning |
|---|---|
| `setting changed (reload): section.key = value` | A setting changed in `tyrender.ini`, from the launcher or a reload. |
| `setting changed (F10 overlay): ...` | A setting changed from the F10 panel. |
| `F10 overlay opened` | The panel was opened. |
| `screenshot ty_..._shaders-on_....png` | An F11 screenshot was written. |
| `window WxH windowed -> WxH fullscreen` | The window changed size or mode. |
| `GL error 0x0502 INVALID_OPERATION (later repeats are counted, see session end)` | An OpenGL error. Only the first is listed; the totals are at the end. |
| `shader failure (ao compile), first time: ...` | A shader did not compile. Only the first of each kind is listed. |
| `AO turned off: v2 shader unavailable` | The renderer turned a feature off for the session. |
| `session end: clean exit. frames N, GL errors N, shader failures N, file N bytes` | The last line of a normal exit. |
| `session end: crash` and `CRASH exception ...` | The game crashed. The exception code and module are given. |

Full list: [log-and-error-messages.md](../reference/log-and-error-messages.md), "Session report events".

## How to send it

1. Open the newest `TYHD_report_*.txt` in `<game folder>\tyrender\logs\`. Use the time in its file name to pick the right one.
2. Check it for anything personal (see the next section).
3. Send it with a screenshot of the problem, and your GPU model. The README asks for these in "Reporting a bug" (`README.md:232-240`).
4. The README's "Reporting problems" section also asks for `tyrender.log` (`README.md:228-230`). The log has more detail but also more personal data. Read the next section first.

Send the files through the channel the project names in the README. The renderer does not upload anything.

## Privacy: what the report contains, and what to check

The code's own list (`src/sesslog.inl:277-278`) says the report does not contain user names, computer names, serial numbers, IP or MAC addresses, installed programs, running processes, file listings or personal folder paths. The report code does not collect any of these: it has no network calls, no process listing and no file listing (search of `src/`). Some owner scripts in `tools/` call an upload helper that is not in the repo; they are test tools, not part of the renderer or the report.

Still check these before sending:

- **Game settings.** The `settings.ini` section copies the game's own settings. The code does not check the content (`src/sesslog.inl:294-310`). A custom path typed there would be copied. Look for `:\` or your user name.
- **tyrender.ini.** Copies every value. The shipped file has no paths (`src/default_tyrender.ini`). If you edited it to point at a folder, that path is in the report.
- **Driver and GPU strings.** These come from OpenGL. They should contain the vendor and model, not a user name. (inferred)

### `tyrender.log` is different

`tyrender.log` is not the session report. It writes full local paths:

- the game folder, on the first line after start-up (`src/tyrender.cpp:726`);
- the full path of every loaded module in `[iat]` lines (`src/tyrender.cpp:680`) and in the crash line (`src/tyrender.cpp:692`);
- the full output path of each screenshot and snapshot (`src/abshot.inl:94`, `src/tyrender.cpp:537`);
- your Documents path, if the game's settings file is missing (`src/tyrender.cpp:643`).

Before you post the log, replace each full path with `<game folder>` or `<Documents>`. A simple check is to search the file for `:\`. The launcher's text says the report holds no personal paths (`src/launcher_ui.inc:479`); that is true of the report, not of the log.

## Settings

| key | default in code | shipped | live or restart | effect |
|---|---|---|---|---|
| `[report] Enable` | 1 (`src/sesslog.inl:372`) | 1 (`src/default_tyrender.ini:93`) | restart | 0 means no report file. |
| `[report] CaptureSeconds` | 300 (`sesslog.inl:373`) | 300 (`:94`) | restart | Length of the frame-time part, in seconds. 0 means no rows. Maximum 3600. |
| `[report] MaxBytes` | 1048576 (`sesslog.inl:374`) | 1048576 (`:95`) | restart | Size cap for one report. Minimum 65536. |
| `[report] KeepFiles` | 10 (`sesslog.inl:375`) | 10 (`:96`) | restart | Number of reports kept in the logs folder. Minimum 1. |

The `[report]` keys are read once at start-up (`src/sesslog.inl:369-376`), so change them and restart the game.

## Retention and size

- At each start, the oldest reports beyond `KeepFiles` are deleted (`src/sesslog.inl:191-201`, called at `:267`). Only files named `TYHD_report_*.txt` are touched.
- Each report stops taking ordinary lines 4 KB before `MaxBytes`, and writes `[cap] size limit reached, further events are dropped`. The end line still fits (`src/sesslog.inl:42-46`).
- `tyrender.log` is not pruned. It is replaced at each start (`src/tyrender.cpp:708`).

## Limits

- The frame-time window is only the first five minutes by default. Problems later in a session show up only as events, not as frame rows.
- Shader failures are listed in the report only from the AO, atmosphere and compile-test paths. Tessellation and Hi-Z GI failures are in `tyrender.log` only (inferred from the call sites; see [log-and-error-messages.md](../reference/log-and-error-messages.md), Discrepancies).
- The GPU timer lines need `GL_TIMESTAMP`. If the driver lacks it, the `gpu ms` parts are missing and nothing says why.
- The report measures wall-clock frame time, which includes vsync waits. Compare it with another report, not with a benchmark number.

## Known issues

- No open issue is tied to the report itself in the repo. The report is new (CHANGELOG, Unreleased, "Session report for bug reports").
- The shipped `tyrender.ini` comment says the report is "never sent" (`src/default_tyrender.ini:91`), and the CHANGELOG says "Local only, nothing is sent". The report code has no network calls (search of `src/`), so the claim matches the code.

## How to verify

1. Start the game and reach gameplay. Close the game normally.
2. Open `<game folder>\tyrender\logs\`. Expected: a `TYHD_report_*.txt` file with today's date.
3. Open it. Expected: the sections in the order listed above, and a `session end: clean exit` line at the bottom.
4. Press F10 in a level, press Right on the first row, and press Esc. Expected in the report: `setting changed (F10 overlay): post.Brightness = ...` and then `F10 overlay opened`.
5. Press F11. Expected: a `screenshot ...png` event.
6. Set `[report] Enable=0` in `tyrender.ini`, restart, and check that no new report appears. Set it back afterwards.

## NEEDS INVESTIGATION

- **NI-R1: does a real `settings.ini` or `tyrender.ini` contain personal paths?** Method: look at several reports from different testers (with consent), or at the owner's own files, and check for `:\`. The code cannot tell; it copies the file.
- **NI-R2: does the GPU model string identify the card on NVIDIA and Intel drivers?** Plan item N-21. Method: collect reports from those GPUs. The report's own label says the string "can be generic".
- **NI-R3: is a report written when the game crashes before the first frame?** Method: force an early failure in a copy of the game folder and check the logs folder. (inferred: no report.)
- **NI-R4: does `GL_TIMESTAMP` exist on common drivers?** The report checks only the five extensions in `src/sesslog.inl:251`, not `GL_TIMESTAMP`. Method: list the extensions on a test machine.

## Discrepancies

1. **Two different "send this" instructions in the README.** "Reporting problems" asks for `tyrender.log` (`README.md:230`). "Reporting a bug" asks for the session report (`README.md:234-240`). The launcher's status line asks for the report (`src/launcher_ui.inc:500`). Both are correct; the report is the safer file to send first.
2. **"A short report file".** README says "a short report file" (`README.md:234`). The default window alone is 300 rows and a settings dump, and the cap is 1 MiB. "Short" is an overstatement (inferred from the code, not measured).
3. **Privacy statement scope.** The launcher text (`src/launcher_ui.inc:479`) and the report header (`src/sesslog.inl:277-278`) are true for the report file. They do not cover `tyrender.log`. See "tyrender.log is different" above.
4. **Header comment vs. content.** `src/sesslog.inl:2-6` describes the file as "system basics". It also copies the game's and the mod's full settings files. The header line is accurate; the summary of content is broader than "basics".

## See also

- [10-logging-timers-and-session-report.md](../how-it-works/10-logging-timers-and-session-report.md): how the report is written.
- [log-and-error-messages.md](../reference/log-and-error-messages.md): every event and log line.
- [tester-guide.md](tester-guide.md): the whole tester workflow, including what to look at.
- [known-issues.md](../decisions/known-issues.md): known problems, and which ones a report can confirm.
- [settings-system-debug.md](../reference/settings-system-debug.md): the `[report]` keys with the other system keys.

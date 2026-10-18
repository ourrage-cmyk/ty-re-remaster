# Tools reference (every script in tools/ and src/*.ps1)

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributors) and B (testers who run the harness).

## Purpose

One row for every PowerShell script in the repo: what it does, its parameters, whether it needs a running game, and what it depends on. Use this page to find the right script. The step-by-step use of the test scripts is on [test-harness.md](../guides/test-harness.md). The build steps are on [build-from-scratch.md](../guides/build-from-scratch.md).

Scope: 42 scripts in `tools/`, 4 in `tools/playtest/`, 4 in `src/` (`build.ps1`, `package.ps1`, `patch_imports.ps1`, `gen_tex_tags.ps1`). `tools/test_ini/` holds one ini file and is on [data-files.md](data-files.md).

## How it works

Scripts are run with `powershell -NoProfile -File <script>` (examples in each script header, e.g. `tools/suite.ps1:2-3`). Most of them are built for a workstation with one fixed layout. Read the following before using any of them:

1. **Owner workstation root (limitation, N-13).** 43 files under `tools/` and `src/` contain a hard-coded absolute root for the owner's workstation (134 lines in total, counted with a fixed-string search). Game folder, toolchain folder, results folder and the external sync target are all under that root. The root is written here as `<owner root>`. It is a limitation: a clean checkout will not run most scripts without edits.
2. **Zero-token scripts.** Many headers say "zero LLM tokens" (for example `tools/g1_gate.ps1:1`, `tools/cost_table.ps1:1`). They are plain automation.
3. **Game lock and phase.** The playtest and harness scripts share a lock and a "phase TEST" gate. `tools/playtest/boot_spot.ps1:3` refuses to run outside that phase. Details are in [test-harness.md](../guides/test-harness.md).
4. **Outputs.** Most results go to a `RESEARCH\results` folder under `<owner root>`, and some copy a summary line to a ledger there. Nothing is written into the repo.
5. **External sync.** Three scripts (`tools/steplog.ps1`, `tools/status.ps1`, and the result step of `tools/suite.ps1`) also mirror files to an external sync service (`tools/steplog.ps1:1`, `:4-6`; `tools/status.ps1:1-3`; `tools/suite.ps1:4`). The service name and endpoint are left out of this page. A repo user does not need that service.
6. **Dependencies used across scripts.** Windows PowerShell with `user32.dll` and `kernel32.dll` calls for focus and input (`tools/key.ps1:6-12`, `tools/focus_once.ps1:2-6`). `System.Drawing` for images (`tools/imgdiff.ps1:4`, `tools/imgmetrics.ps1:6`). The llvm-mingw toolchain for builds (`src/build.ps1:5`). Git for merges (`tools/merge_agent.ps1:7`) and for the build id (`src/build.ps1:9`). The JDK, Node and Ghidra folders for reverse engineering (`tools/env.ps1:3-6`).

Placeholders on this page: `<owner root>` means the owner's workstation root, `<game folder>` means the Steam game folder, and `<tools folder>` means the `tools` folder under the owner root.

## Code pointers

The "Ref" column in each table gives the header line (or the parameter line, when it is shown). Line numbers were taken at 4db1d7d.

### Build and package (`src/`)

| Script | Purpose | Parameters | Game needed | Hard-coded and dependencies | Ref |
|---|---|---|---|---|---|
| `src/build.ps1` | Builds `tyrender.dll`, `TYLauncher.exe`, the uninstaller and `TY HD Renderer Setup.exe` with llvm-mingw i686. Writes `build_id.h` from the git short hash. Embeds the DLL and ini as resources. | `-Install` copies the launcher and DLL into `<game folder>`. | No | Toolchain folder under `<owner root>` (`:5`). `-Install` target is `<game folder>` (`:34`). Needs git (`:9`). | `:1-2`, `:5`, `:8-12`, `:13-31` |
| `src/package.ps1` | Builds, then zips the release files into `dist\`. | `-Version` (default `v0.1`, `:2`). | No | Copies `src\README.md` and `src\PERFORMANCE.md` (`:7`). Neither file is in `src/` (see Known issues). | `:1-2`, `:5-9` |
| `src/patch_imports.ps1` | Standalone form of the TY.exe patch. Makes `TY_tyrender.exe` from `TY.exe` without modifying the input. | `-In`, `-Out`, `-Dll` (default `tyrender.dll`), `-Func` (default `tyrender_init`). | No | Default paths are under `<game folder>` (`:5`). The same algorithm as `src/patch_exe.inc:6-48`. | `:1-5` |
| `src/gen_tex_tags.ps1` | Writes `src/tex_tags.inc` from the owner's texture CSV and the rkv index. | none | No | Reads `<owner root>\RESEARCH\recording\texture_tags.csv` (`:4`) and `<owner root>\RESEARCH\rkv_index\Data_PC.csv` (`:10`). | `:1-4`, `:10` |

### Harness: running the game and driving it

| Script | Purpose | Parameters | Game needed | Hard-coded and dependencies | Ref |
|---|---|---|---|---|---|
| `tools/ty_play.ps1` | Boots the patched game to the test spot (a save in slot 2, Rainbow Cliffs) and waits for gameplay. Only stops or drives processes running from `<game folder>`. | `-Extra` (default `-skipvideo`), `-Settle` (default 12), `-Exe` (default `TY_tyrender.exe`), `-Game` | Starts it | Default `-Game` under `<owner root>` (`:5`). Needs the save created on the owner's machine (`:1-3`). Reads `TyOutputLog.txt` (`:6`). | `:1-5` |
| `tools/agent_test.ps1` | Serialized game test: copies a DLL over the game, takes snapshots, camera views, A/B, soak. Restores the DLL and ini afterwards. | `-Dll`, `-IniFile`, `-Snaps`, `-Keys`, `-Perf`, `-OutDir`, `-Cam`, `-HoldMs`, `-Settle`, `-AB`, `-Soak` | Yes (starts it) | Default out dir under `<owner root>` (`:18`). Global mutex (`:1-2`). | `:1-17`, `:18` |
| `tools/suite.ps1` | Standard in-session A/B over fixed views, with perf, metrics, contact sheets and a ledger line. Only live-reloadable keys can differ between variants. | `-Name`, `-Dll`, `-AB`, `-Views`, `-NoPerf`, `-NoS...` | Yes | Output under `<owner root>\RESEARCH\results` (`:4`). Calls `imgmetrics.ps1`. External sync (see How it works). | `:1-5`, `:6` |
| `tools/soak.ps1` | Plays `N` minutes with the balanced ini and checks for crashes and out-of-memory. | `-Minutes` (default 10), `-Dll`, `-Ini` | Yes | Defaults under `<owner root>` (`:3`). Calls `agent_test.ps1 -Soak` (`:6`). | `:1-2`, `:3` |
| `tools/crashwalk.ps1` | Repro for an old crash: boots with chosen feature switches, walks a fixed key sequence, reports whether the game survived. Restores the ini. | `-AO`, `-Atmos`, `-Post`, `-Shadows`, `-Tess`, `-Game` | Yes | Default `-Game` under `<owner root>` (`:3`). | `:1-2`, `:3` |
| `tools/key.ps1` | Sends key presses to the game window with a hold time, then optional snapshot. Only sends while the game is foreground. | `-Keys`, `-HoldMs`, `-DelayMs`, `-Snap`, `-OutDir`, `-Proc`, `-TargetPid` | Yes | Default out dir under `<owner root>` (`:6`). Uses `keybd_event` (`:6-12`). | `:1-5`, `:6` |
| `tools/snap.ps1` | Asks the running game for a snapshot of its own back buffer, converts it to PNG. | `-Name` (required), `-OutDir`, `-Game`, `-TimeoutSec` | Yes | Defaults under `<owner root>` (`:4`). Writes a request file in `tyrender\` (`:5-8`). | `:1-3`, `:4` |
| `tools/shot.ps1` | Launches the game, captures only the game window at set times or in an automatic menu route, and logs the run. | `-Out`, `-Times`, `-Game`, `-Keys`, `-Auto`, `-AutoShots`, `-MaxSeconds`, `-Exe`, `-ExeArgs`, `-NoKeys`, `-Launcher` | Starts it | Defaults under `<owner root>` (`:7`). | `:1-6`, `:7` |
| `tools/capframe.ps1` | Asks the game to log one full frame call by call (`cap.req`), then extracts it from `tyrender.log`. | `-Out` (required), `-Game` | Yes | Default `-Game` under `<owner root>` (`:3`). | `:1-2`, `:3` |
| `tools/focus_game.ps1` | Brings the game window to the front and reports whether it is foreground. Exit codes 0, 1, 2. | `-TargetPid` (required) | Yes | P/Invoke to `user32.dll` (`:5-7`). | `:1-3`, `:4` |
| `tools/focus_once.ps1` | One focus attempt, run as a fresh process by the daemon. Exits 0 if the game is foreground. | none | Yes | P/Invoke (`:2-6`). | `:1` |
| `tools/focus_daemon.ps1` | Keeps the game in focus during harness tests. Runs `focus_once.ps1` as a child process when needed. Logs each outcome. | none | Yes | Flag and log files under `<owner root>` (`:10`). Stop by a file (`:5`). | `:1-7` |
| `tools/focus_probe.ps1` | Diagnostic for focus techniques (inferred from the P/Invoke list). | none | Yes (inferred) | P/Invoke list (`:2-6`). Not documented elsewhere. | `:1-2` |
| `tools/todo_runner.ps1` | Unattended, resumable runner for the owner's todo plan. Runs `kind=auto` items. Prints a board with `-Status`. | `-Status`, `-Id`, `-Max` | Depends on the item | Reads `<owner root>\RESEARCH\todo.json` (`:1-2`). Field list at `:3-8`. | `:1-8`, `:9` |
| `tools/steplog.ps1` | Appends a timestamped line to an agent's log and syncs it (and optional files) to the external service. Never fails the caller. | `-Agent`, `-Msg`, `-Files` | No | Log folder under `<owner root>` (`:4`). External sync (`:1`, `:4-6`). | `:1-7`, `:8` |
| `tools/status.ps1` | Collects every agent's state from files into one raw status file. `-Sync` mirrors it. | `-Sync` | No | Logs under `<owner root>` (`:7`). | `:1-2`, `:4` |
| `tools/wf.ps1` | Local tracker for research tickets (map, tickets, block, frontier, claim). Atomic claims. | `-Cmd` (required), `-Map`, `-Title`, `-Destination`, `-Notes`, and more | No | Tracker folder under `<owner root>` (`:1-2`). References a skill file under the owner root (`:1`). | `:1-15`, `:16` |

### Measurement and A/B

| Script | Purpose | Parameters | Game needed | Hard-coded and dependencies | Ref |
|---|---|---|---|---|---|
| `tools/abshots.ps1` | Community A/B shots: one fresh session per preset, same camera views, 2560x1440 PNGs. | `-Slot`, `-Name`, `-Views` | Yes | Output under `<owner root>` (`:1-3`, `:5` defaults). Calls `suite.ps1` (`:12`). | `:1-4`, `:5` |
| `tools/ab2_run.ps1` | One-line run: fixed 44-view capture named for a level-2 pool (`p01` to `p44`). | none | Yes | Calls `abshots.ps1` with slot 2 (`:3`). | `:1-3` |
| `tools/match_ab.ps1` | Finds the most similar frames across presets for cutscene scenes (edge correlation). | `-Pool`, `-Out`, `-Scenes` | No | Pools under `<owner root>` (`:3`). Uses `imgmetrics.ps1` (`:4`). | `:1-3` |
| `tools/flicker.ps1` | Temporal flicker: N frames per variant in one session, temporal standard deviation of luma. | `-Name`, `-Dll`, `-AB`, `-Cam`, `-Frames`, `-Region` | Yes | Output under `<owner root>` (`:3-4`). | `:1-4`, `:5` |
| `tools/motion.ps1` | Flicker under camera motion: second temporal difference while the camera yaws. | `-Name`, `-Dll`, `-AB`, `-Step`, `-Frames`, `-Cam`, `-Reps` | Yes | Output under `<owner root>` (`:1-3`). | `:1-3`, `:4` |
| `tools/perf.ps1` | Frame throughput: boots the game with a given ini text and counts frames over a time window. Restores the ini. | `-Label`, `-IniText`, `-Secs`, `-Game` | Yes | Default `-Game` under `<owner root>` (`:4`). Brings the window forward (`:2-3`). | `:1-3`, `:4` |
| `tools/perf_matrix.ps1` | Preset matrix on the merged DLL: one session per preset, GPU timers. Writes a CSV and a Markdown table. | `-Dll` | Yes | Default DLL and results under `<owner root>` (`:3`, `:1-2`). Uses `presets.ps1`. | `:1-2`, `:3` |
| `tools/cost_table.ps1` | Cost of each option relative to the balanced build (one-option-off variants, default view). | `-Dll` | Yes | Default DLL and results under `<owner root>` (`:3`). | `:1-2`, `:3` |
| `tools/gpucmp.ps1` | Live A/B inside one session using the DLL's GPU timers. Variants are `name|section.key=value` strings. | `-Variants`, `-Snap`, `-Game` | Yes | Default `-Game` under `<owner root>` (`:3`). | `:1-2`, `:3` |
| `tools/balanced_search.ps1` | Finds the best balanced candidate that fits the 16.7 ms budget at the default view. Writes `preset_Balanced.ini` and a result file. | `-Dll`, `-Budget` | Yes | Default DLL under `<owner root>` (`:3`). Uses `presets.ps1` (`:4`), calls `suite.ps1` (`:7`). | `:1-2`, `:3` |
| `tools/g1_gate.ps1` | G1 release gate on the shipped DLL: 10-minute soak, forced fallback (`ForceFallback=31`), perf at four views. | `-Dll` | Yes | Default DLL under `<owner root>` (`:2`). Calls `soak.ps1` and `suite.ps1` (`:4-5`). | `:1`, `:2` |
| `tools/cm_run.ps1` | One-line suite run with two ini variants (name `cm2`). Purpose not documented (inferred). | none | Yes | Calls `suite.ps1` (`:1`). | `:1` |
| `tools/grid_run.ps1` | One-line suite run with an ultra-setting variant (name `grid_ultra_v034`). Purpose not documented (inferred). | none | Yes | Calls `suite.ps1` (`:1`). | `:1` |
| `tools/cfgs.ps1` | Function `Cfg` that builds an ini with only selected feature groups on. | function `Cfg` (`:3`) | No | Dot-source. | `:1-3` |
| `tools/presets.ps1` | Dot-sourced helper: `NewIni` builder, `$Presets` (Low, Medium, High, Ultra) and `$BalancedCandidates`. The Medium entry differs from the launcher (see Known issues). | none | No | Dot-source. | `:1-2`, `:13`, `:20` |
| `tools/launcher_shot.ps1` | Screenshot of the launcher window (optionally hover or click). | `-Out`, `-Ini`, `-Exe`, `-Hover`, `-Click` | No (needs the built launcher) | Defaults under `<owner root>` (`:2`). P/Invoke (`:3-5`). | `:1`, `:2` |

### Image comparison (no game needed)

| Script | Purpose | Parameters | Game needed | Hard-coded and dependencies | Ref |
|---|---|---|---|---|---|
| `tools/imgdiff.ps1` | Amplified difference map of two same-spot screenshots, plus statistics. | `-A`, `-B`, `-Out`, `-Gain`, `-Region` | No | `System.Drawing` (`:5-6`). | `:1-3`, `:4` |
| `tools/imgmetrics.ps1` | Dot-sourced metric functions: mean colour difference, edge correlation, luma and saturation means. Regions are in 2560x1440 pixels. | none | No | `System.Drawing` (`:7`). Named regions in `:5`. | `:1-7` |
| `tools/cropcmp.ps1` | Side-by-side zoomed crops of one region across several screenshots. | `-Images`, `-Region`, `-Zoom`, `-Out` | No | `System.Drawing` (`:4`). | `:1-2`, `:3` |

### Release, repo and team bookkeeping

| Script | Purpose | Parameters | Game needed | Hard-coded and dependencies | Ref |
|---|---|---|---|---|---|
| `tools/release.ps1` | Promotes the merged staging tree to the master, builds and installs, packages, and checks the zip and the Large Address Aware patch. Backs up the master first. Refuses while `TY_tyrender` runs. | `-Version` (default `v0.3`) | No | Master and staging under `<owner root>` (`:5`). | `:1-3`, `:4`, `:6` |
| `tools/merge_agent.ps1` | Merges one agent's files into the staging tree. Owned files are copied. Other changed `.cpp` and `.inl` files are three-way merged with git. Builds, then runs a regression suite. The master is not touched. | `-Agent` (`atmos`, `gi`, `ao`, `tess`, `light`), `-NoTest` | Only for the suite step | Git path (`:7`). Owned-file map (`:8`). Build check (`:27-29`). | `:1-5`, `:6`, `:7-8` |
| `tools/infra.ps1` | Helpers for the agent source copies: `sync-infra`, `build-all`, `hashes`. | `-Cmd` (required, position 0) | No | Master and agent folders under `<owner root>` (`:7`). | `:1-5`, `:6` |
| `tools/repo_sync.ps1` | Copies whitelisted files from the master, the tools folder and the research folder into a local git working tree. Runs a secret scan and aborts on a match. Shows git status. Does not commit or push. | none | No | Repo folder under `<owner root>` (`:3`). Secret regex (`:21`). Copies transcripts from `recording\` (`:15-16`). | `:1-2`, `:3`, `:20-23` |
| `tools/env.ps1` | Dot-source: sets `JAVA_HOME`, `GHIDRA_INSTALL_DIR`, `PATH` for JDK, Node, llvm-mingw and Ghidra. | none | No | Tools folder under `<owner root>` (`:2`). | `:1-2`, `:10` |

### Reverse engineering

| Script | Purpose | Parameters | Game needed | Hard-coded and dependencies | Ref |
|---|---|---|---|---|---|
| `tools/rkvtool.ps1` | Lists, extracts, or repacks the game's RKV archives. Layout comment at `:2`. | `-Rkv`, `-Mode` (`list`, `extract`, `repack`), `-Filter`, `-OutDir`, `-Csv`, `-Src`, `-Out` | No (needs an RKV file the user supplies) | Archive format from a third-party tool, named in the comment (`:2`). | `:1-4`, `:5` |

### Playtest (`tools/playtest/`)

| Script | Purpose | Parameters | Game needed | Hard-coded and dependencies | Ref |
|---|---|---|---|---|---|
| `tools/playtest/autoplaytest.ps1` | Boots each variant to a spot, takes A/B shots, measures flicker, checks the overlay, collects logs, writes a report and contact sheet. Requires a plan file. | `-Game`, `-Variants`, `-Spot`, `-Plan` (required), `-Cam`, `-FlickerFrames` | Yes | Default `-Game` under `<owner root>` (`:6-7`). Check kinds at `:196-235`. | `:1-5`, `:6-9` |
| `tools/playtest/boot_spot.ps1` | Boots to a known spot and waits for gameplay. Refuses unless the phase is TEST. Holds the game lock. | `-Game`, `-Exe`, `-Extra`, `-Spot` | Yes | Wraps `tools/ty_play.ps1` (`:1`, `:23`). | `:1-7` |
| `tools/playtest/build_plan.ps1` | Merges all pending requests into one plan. Unknown variants go to a backlog file. | `-Name` | No | Reads the request folder (`:3`). | `:1-7` |
| `tools/playtest/common.ps1` | Shared helpers for the playtest scripts: Ty's region in fractions of the frame, read-only screenshot rules. Dot-sourced. | none | No | Dot-source (`:1`). | `:1-3` |

Playtest data (`plans/`, `requests/`, `review/`) is described on [data-files.md](data-files.md).

## Settings

Not applicable. The scripts write and read ini files, but they do not define keys. Key meanings are on the settings pages. Preset values are on [presets.md](presets.md).

## Cost

Not applicable. Run times depend on the game and the test. The harness scripts wait for game frames, so they are slow. The exact times are not measured in this repo (NI-T3).

## Limits

- Most scripts will not run on a clean checkout. Their paths point at `<owner root>`. See N-13 under How it works.
- The `-IniFile` trap and focus issues are covered on the test-harness page. Scripts that bring the game forward (`tools/perf.ps1:3`, `tools/focus_*.ps1`) can fail when another window keeps focus.
- Scripts whose headers say they restore the ini afterwards (`tools/perf.ps1:1-3`, `tools/crashwalk.ps1:1-2`, `tools/agent_test.ps1:1-16`) may leave a changed ini behind if they are stopped in the middle. Check `tyrender.ini` by hand after an interrupted run (inferred; not run).
- Input is sent as key presses with holds (`tools/key.ps1:3-4`). Mouse and menu tests depend on hover, as the test-harness page explains.
- Image metrics are calibrated on the owner's screenshots only (`tools/imgmetrics.ps1:3-4`, "Rule of thumb from calibration").

## Known issues

- **Missing files for `package.ps1`.** `src/package.ps1:7` copies `src\README.md` and `src\PERFORMANCE.md`. Neither file is in `src/` (`src/` lists no `README.md` or `PERFORMANCE.md`). With `$ErrorActionPreference = 'Stop'` (`src/package.ps1:3`) this would stop the script. Not run here (inferred from the file list).
- **Medium preset differs.** `tools/presets.ps1:15` Medium sets `aoSlices` 2, `aoSteps` 4, `atmosSteps` 16, `shTaps` 12. The launcher's Medium and the shipped ini use 3, 6, 24 and 16 (`src/launcher_ui.inc:12`, `src/default_tyrender.ini:5-6`, `:34`, `:48`). See [presets.md](presets.md).
- **Stale default.** `src/package.ps1:2` defaults `-Version` to `v0.1`. `tools/release.ps1:4` defaults to `v0.3`. The current version label is `v0.3.5-rc` (`src/TYLauncher.c:22`).
- **Ordering in `merge_agent.ps1`.** The build runs at line 26 and only then does line 27 check whether `build.ps1` exists in staging. The check is after its first use. Minor.
- **Temp copy not used.** `tools/merge_agent.ps1:18` copies a file to a temp path that is never read. Minor.
- **Build check by time.** `tools/merge_agent.ps1:29` decides the build succeeded by checking the file's write time is less than 3 minutes old. A slow build from an older run can pass or fail for the wrong reason. Minor.
- **Owner-only text.** Some scripts print the owner's folder names and the name of the external service. This page leaves them out.

## How to verify

1. List the scripts: `ls tools/*.ps1 tools/playtest/*.ps1 src/*.ps1`. The result should be 42, 4 and 4 files (50 in all).
2. Count the files that contain the owner root path. Search `tools` and `src` for the drive-letter path that `src/build.ps1:5` uses as its toolchain folder, with a fixed-string search (the same check as N-13). It should give 43 files.
3. Check the package gap: `ls src/README.md src/PERFORMANCE.md`. Both should be missing.
4. Check that the image scripts load without a game: `powershell -NoProfile -File tools/imgdiff.ps1 -A a.png -B b.png` with two PNGs. The script should print statistics (not run here).

## NEEDS INVESTIGATION

- NI-T1 (N-12): Whether `src/build.ps1` builds on a clean Windows machine with llvm-mingw from the official release. Method: a clean VM, with the toolchain path changed to a local folder. Not tested here.
- NI-T2 (N-13): Make the owner root a single config value (`tools/env.ps1` is the suggested place). Method: the inventory above lists every file. A follow-up code change, not a docs task.
- NI-T3 (N-20): Run times and frame costs for each harness script. Method: time one run of each on the owner's machine.
- NI-T4 (N-17): These tools are named in `docs/START_HERE.md` but are not in the repo: `ovsync_all.ps1` (`docs/START_HERE.md:58`, `:64`), `vm.ps1` (`docs/START_HERE.md:59`), and `ghidra_scripts\StringXrefDecomp.java` (`docs/START_HERE.md:59`). Method: ask the owner whether to publish sanitized copies.
- NI-T5: Purpose of `tools/cm_run.ps1`, `tools/grid_run.ps1` and `tools/focus_probe.ps1`. The headers are missing. Method: ask the owner or read the call sites.
- NI-T6 (N-12, Q195): Whether the scripts run under PowerShell 7. They are written for Windows PowerShell with `powershell -NoProfile -File`. Method: run one under `pwsh`.

## Discrepancies

- `docs/START_HERE.md` lists `ovsync_all.ps1` and `vm.ps1`. Neither is in the repo. This is a documented gap, not a conflict.
- The first-pass docs do not describe `tools/` at all (`LAUNCHER.md` mentions only `tools/presets.ps1`). The count of 42 scripts in `tools/` and the table above are new.
- `src/package.ps1:9` calls the shipped ini "Balanced" and points to `presets.ps1` C1. The ini header calls it Medium (`src/default_tyrender.ini:1`). See [data-files.md](data-files.md).

## See also

- [test-harness.md](../guides/test-harness.md): how to run the harness scripts in order
- [build-from-scratch.md](../guides/build-from-scratch.md): the build steps and toolchain
- [deploy-package-release.md](../guides/deploy-package-release.md): release and packaging
- [ab-captures.md](../guides/ab-captures.md): the A/B scripts in use
- [data-files.md](data-files.md): the files these scripts read and write
- [source-map.md](source-map.md): the source files in `src/`

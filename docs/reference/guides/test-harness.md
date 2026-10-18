# Test harness: scripted game runs, metrics and the playtest queue

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor). Testers read the output sections only.

## Purpose

The repository has no unit-test project. The top level holds `data/`, `docs/`, `src/`, `tools/` and `.github/`, and no test folder (observed in the clone). The real checks run in the game. The scripts in `tools/` and `tools/playtest/` boot the game, send it keys, take screenshots from inside the game, measure flicker and GPU time, and write reports. Windows only.

Project rule: game runs go through these scripts, never by hand (`docs/START_HERE.md:45`, `docs/agent-tasks/BRIEFING.md:24`).

The harness cannot judge how things look. Fog colour, shadow quality, portal light and water appearance need a person, or the advisory visual review, to look at the frames (`docs/AUTOPLAYTEST.md:3`, `docs/AUTOPLAYTEST.md:82`).

## How it works

The harness has four layers.

1. **One game at a time.** A global named mutex, `Global\TYGameTest`, serialises the scripts that run through `agent_test.ps1` (`suite`, `flicker`, `motion`, `soak`, the G1 gate). `tools/agent_test.ps1:22` takes it and waits up to 45 minutes by default (`tools/agent_test.ps1:19`). The playtest runner uses a lock file and a phase file instead (`tools/playtest/common.ps1:90-123`). The phase file must read `TEST` before any game launch (`tools/playtest/common.ps1:96-100`, called from `tools/playtest/autoplaytest.ps1:27` and `tools/playtest/boot_spot.ps1:20`). A coordinating process writes the phase file, not the tools (`docs/AUTOPLAYTEST.md:99`, `:103`).
2. **Boot to a known spot.** `tools/ty_play.ps1` stops game processes that run from the target folder (`tools/ty_play.ps1:10-11`), deletes `TyOutputLog.txt` and `tyrender.log` (`:12`), starts the exe with `-skipvideo` (`:13`), waits for state 8 (`:15`), then presses ENTER twice. Menus pick items by mouse hover, so the script moves the pointer onto a save slot before each ENTER (`:18-19`, `:29-35`). It waits for state 5 and a settle time (`:35-36`).
3. **Trigger files inside the game.** The renderer polls files in `<game folder>\tyrender\` every 8 frames (`src/tyrender.cpp:594-595`):
   - `snap.req`: in-process screenshot of the back buffer (`src/tyrender.cpp:503-530`).
   - `cam.txt`: fixed debug camera offset (`src/debugcam.inl:8-14`).
   - `motion.req`: smooth yaw capture for motion flicker (`src/debugcam.inl:47-57`).
   - `reload`: re-reads the config of most modules (`src/tyrender.cpp:597-603`).
   - `cap.req`: logs one frame call by call (`src/tyrender.cpp:623-627`).
   The full list of trigger files is in [reference/hotkeys-and-files.md](../reference/hotkeys-and-files.md).
4. **Keys and focus.** `tools/key.ps1` checks that the game window is foreground before each key (`tools/key.ps1:35-36`) and holds each key for 120 ms by default (`tools/key.ps1:6`, `:40-42`). The key tool cannot read the picture. All pixels come from inside the game (`tools/snap.ps1:1-3`, `docs/START_HERE.md:73`).

```
plans/*.json ------> autoplaytest.ps1 --checks--> report.md, contact_sheet.png
requests/*.json --> build_plan.ps1 --> plans/merged_*.json        |
                                                                  v
              boot_spot.ps1 -> ty_play.ps1 -> TY_tyrender.exe   visual_review.json
                                                  |             (advisory, optional)
     key.ps1 (F10, F11, ENTER, ...) ------------> |  polls tyrender\ every 8 frames
     snap.ps1 (snap.req) -----------------------> |  -> tyrender\snaps\<name>.bmp -> PNG
```

### Which tool answers which question

| Question | Tool | Notes |
|---|---|---|
| Do two ini sets look different, in one session? | `tools/suite.ps1` | In-session A/B over standard views, with a contact sheet, a Ty strip and a ledger line. Only live keys can differ. |
| Does a setting make the picture flicker, static camera? | `tools/flicker.ps1` | N frames per variant, temporal std of luma, Ty excluded. |
| Does it flicker while the camera turns? | `tools/motion.ps1` | Second temporal difference over a smooth yaw sweep. |
| Does the game survive N minutes of play? | `tools/soak.ps1` | Wraps `agent_test.ps1 -Soak`. Checks for crash markers and "Out of memory". |
| Does a DLL reach gameplay and produce one screenshot? | `tools/agent_test.ps1 -Dll ... -Snaps ...` | Swaps the DLL and ini in, then restores the game folder. |
| Does a named check pass for an issue? | `tools/playtest/autoplaytest.ps1 -Plan ...` | Plan-driven. Each check gives PASS, FAIL or INCONCLUSIVE. |
| Which pending requests run next? | `tools/playtest/build_plan.ps1` | Merges request files into one plan. |
| Is the release gate met? | `tools/g1_gate.ps1` | Owned by [guides/deploy-package-release.md](deploy-package-release.md). |

## Code pointers

| What | File:line | Note |
|---|---|---|
| Global game mutex and wait | `tools/agent_test.ps1:19`, `:22-23` | Name `Global\TYGameTest`; 45 minutes by default. |
| Crash recovery backup | `tools/agent_test.ps1:25-27` | Copies the game DLL and ini to a backup folder and sets an in-test flag. The next run restores from it. |
| Restore after a run | `tools/agent_test.ps1:97-100` | Stops the game, restores DLL and ini, clears the flag, releases the mutex. |
| Boot retries | `tools/agent_test.ps1:35-43` | Up to 3 attempts. Gameplay is detected from `[gpu]` lines. |
| Launch arguments | `tools/agent_test.ps1:33` | `-skipvideo -novsync`. |
| Phase check | `tools/playtest/common.ps1:96-100` | Anything other than `TEST` throws. A missing file counts as not `TEST`. |
| Lock file | `tools/playtest/common.ps1:102-117` | Refuses if a live pid holds it. A stale lock is replaced. Child scripts reuse `TY_LOCK_TOKEN`. |
| Lock release | `tools/playtest/common.ps1:119-123` | Also run from the `finally` block in `autoplaytest.ps1:181`. |
| Plan refusal | `tools/playtest/autoplaytest.ps1:20-26` | Refuses with no plan, a missing plan, or a variant that no check uses. |
| Variant table | `tools/playtest/autoplaytest.ps1:51-79` | `$defs`: exe, launch args, ini keys, checks, optional spot and camera. |
| Game already running | `tools/playtest/autoplaytest.ps1:98` | The variant fails if a game from the folder is running. |
| Ini backup and restore | `tools/playtest/autoplaytest.ps1:44`, `:102-103`, `:177-178` | Each variant starts from the backed-up ini, so overrides do not stack. |
| Copy mode | `tools/playtest/autoplaytest.ps1:35-40` | `-Copy` robocopies the game folder, without `Screenshots`. |
| Settle time | `tools/playtest/autoplaytest.ps1:109` | 45 s for the billabong spot, 12 s otherwise. |
| Flicker check | `tools/playtest/autoplaytest.ps1:124-134` | Ten presses, first frame of each pair. |
| Overlay check | `tools/playtest/autoplaytest.ps1:136-155` | Baseline, F10 open, F10 close. The closed frame must match the baseline. |
| Check evaluation | `tools/playtest/autoplaytest.ps1:185-224` | One branch per check kind. Missing data gives INCONCLUSIVE. |
| Foreground invalidates shots | `tools/playtest/autoplaytest.ps1:233-235` | Flicker, tyflicker and overlay checks become INCONCLUSIVE. |
| Visual review | `tools/playtest/autoplaytest.ps1:236-238` | Advisory. `suspect` or `broken` turns a PASS into INCONCLUSIVE. |
| Report | `tools/playtest/autoplaytest.ps1:243-290` | `report.md` with a checklist, check table and an Issues section. |
| Shared file helpers | `tools/playtest/common.ps1:132-154` | Retrying append to `progress.log`, and `status.json`. |
| Heartbeat | `tools/playtest/common.ps1:158-191` | Background job, one line every 20 s: game pid and CPU, newest screenshot, log write time. |
| Ty region | `tools/playtest/common.ps1:7` | x 0.41 to 0.605, y 0.39 to 0.90 of the frame. Excluded from flicker and diff metrics. |
| Luma | `tools/playtest/common.ps1:12-24` | 4x4 block averages, weights 0.2126, 0.7152, 0.0722. |
| Temporal std | `tools/playtest/common.ps1:46-58` | Per pixel over frames, averaged outside the Ty region. |
| Pair capture | `tools/playtest/common.ps1:237-256` | Presses the A/B key, waits up to 8 s for two new PNGs, copies them out. |
| Build plan variant list | `tools/playtest/build_plan.ps1:47` | Reads variant names from `autoplaytest.ps1` by regex. |
| Snapshot poll | `src/tyrender.cpp:595` | `Snapshot`, `PollDebugCam`, `MotionPoll`, and the other file polls, every 8 frames. |

## Settings

Harness parameters. Defaults are from the code.

| Parameter | Default | Where | Effect |
|---|---|---|---|
| `-Plan` | none (required) | `tools/playtest/autoplaytest.ps1:10`, `:20` | The plan file that names the checks. |
| `-Variants` | stock, mod, overlay, fog_seethru1, emit1 | `tools/playtest/autoplaytest.ps1:8` | Variants to run. Each must be used by a check in the plan. |
| `-Spot` | rainbow | `tools/playtest/autoplaytest.ps1:9` | Save slot 1 for `rainbow`. `billabong` or `slot2` use slot 2. |
| `-FlickerFrames` | 10 | `tools/playtest/autoplaytest.ps1:12` | Presses per flicker run. |
| `-FlickerRatioMax` | 1.5 | `tools/playtest/autoplaytest.ps1:13` | Report-only limit on the lower-half flicker ratio against stock. |
| `-OverlayClosedMax` | 3.0 | `tools/playtest/autoplaytest.ps1:14` | Largest mean difference allowed for the closed overlay frame. |
| `-Copy` | off | `tools/playtest/autoplaytest.ps1:16` | Test a copy of the game folder. |
| `-Settle` | 8 s | `tools/agent_test.ps1:19` | Wait after gameplay before the first shot. |
| `-CamWait` | 4 s | `tools/agent_test.ps1:19` | Wait after each camera move. |
| `-HoldMs` | 120 ms | `tools/agent_test.ps1:19`, `tools/key.ps1:6` | How long a key is held. |
| `-LockMinutes` | 45 | `tools/agent_test.ps1:19` | Mutex wait before giving up. |

Environment variables read by the harness, set per process:

| Variable | Read at | Effect |
|---|---|---|
| `TY_SLOT` | `tools/ty_play.ps1:33`, `tools/playtest/boot_spot.ps1:25` | `2` selects save slot 2. `tools/abshots.ps1` sets it. |
| `TY_SETTLE` | `tools/agent_test.ps1:45` | Overrides `-Settle`. |
| `TY_CAMWAIT` | `tools/agent_test.ps1:44` | Overrides `-CamWait`. |
| `TY_PERFCAM` | `tools/agent_test.ps1:64`, `tools/g1_gate.ps1:9` | Camera used for the perf sample. |
| `TY_LOCK_TOKEN` | `tools/playtest/common.ps1:105`, `:115` | Lets a child script reuse the parent's lock. |
| `TYRENDER_CAPTURE` | `src/tyrender.cpp:63` | Game side. Captures that frame number. See [guides/debugging.md](debugging.md). |

`docs/START_HERE.md:61` lists the first four variables and leaves out `TY_PERFCAM` and `TY_LOCK_TOKEN`.

## Plans, checks and verdicts

A plan is a JSON file with `name`, `purpose` and `checks` (`docs/AUTOPLAYTEST.md:127-145`). Each check has `id`, `kind`, `variants`, `issue`, `decides`, `pass` and `next`. The kinds are `boot`, `overlay`, `flicker`, `tyflicker`, `log` and `manual` (`docs/AUTOPLAYTEST.md:134`).

| Kind | Decides PASS when | Code |
|---|---|---|
| `boot` | Every listed variant reached gameplay. | `tools/playtest/autoplaytest.ps1:191-195` |
| `overlay` | Overlay closed frame matches baseline and the game is alive. | `tools/playtest/autoplaytest.ps1:196-200`, `:147-149` |
| `flicker` | Lower-half temporal std of the variant divided by the baseline is at most `limit`. | `tools/playtest/autoplaytest.ps1:201-206` |
| `tyflicker` | Blue-excess temporal std inside Ty's region, divided by the baseline, is at most `limit`. Used for water and Ty flicker. | `tools/playtest/autoplaytest.ps1:215-220` |
| `log` | Regex matches (`mustMatch: true`) or does not match (`false`) in the variant's `tyrender.log` copy. | `tools/playtest/autoplaytest.ps1:207-214` |
| `manual` | Never automatic. Always INCONCLUSIVE until a person looks at `contact_sheet.png`. | `tools/playtest/autoplaytest.ps1:221` |

Verdicts:

- **INCONCLUSIVE, not a guess.** A variant or log that is missing gives INCONCLUSIVE (`tools/playtest/autoplaytest.ps1:188-189`, `:208-209`).
- **Foreground.** A shot taken while the game was not foreground invalidates flicker, tyflicker and overlay checks (`tools/playtest/autoplaytest.ps1:233-235`).
- **Visual review is advisory.** A `suspect` or `broken` verdict in `visual_review.json` turns a PASS into INCONCLUSIVE. It never turns a FAIL into a PASS (`tools/playtest/autoplaytest.ps1:236-238`, `tools/playtest/review/RUBRIC.md:5`). The rubric has seven questions, one row per variant (`tools/playtest/review/RUBRIC.md:9-15`).
- **Meaning of PASS.** "Supported by this run". An issue closes only after the owner's playtest (`tools/playtest/autoplaytest.ps1:284`, `docs/AUTOPLAYTEST.md:143`).

The flicker ratio in the report checklist (`tools/playtest/autoplaytest.ps1:262`) and the plan `limit` are separate numbers. The report uses `FlickerRatioMax`, the check uses its own `limit`.

## Requests (the queue)

Other threads or agents file one JSON file per request under `tools/playtest/requests/` (`docs/AUTOPLAYTEST.md:161-163`). `TEMPLATE.json` shows the fields: `id`, `status`, `priority`, `requester`, `question`, `variants`, `checks`, `report` (`tools/playtest/requests/TEMPLATE.json:1-25`).

`build_plan.ps1` runs before a playtest (`tools/playtest/build_plan.ps1:1-3`):

1. It reads only request files whose `status` is `pending` (`tools/playtest/build_plan.ps1:52-54`).
2. A request that names a variant missing from `autoplaytest.ps1` goes into `requests/BACKLOG.md` (`tools/playtest/build_plan.ps1:55-58`). The request file is not changed.
3. It merges checks with the same kind, variants, pattern, baseline, limit and `mustMatch`, and writes `plans/merged_<stamp>.json` (`tools/playtest/build_plan.ps1:62-73`).

To add a variant, follow the table format exactly: four spaces, a lowercase name, then `= @{` (`tools/playtest/build_plan.ps1:47` reads the names by that pattern).

The repo holds eight request files, `req-79` to `req-86`. Six are `pending` and two are `backlog` (`tools/playtest/requests/*.json`, line 3 of each). `docs/AUTOPLAYTEST.md:171` calls `req-82-waterboost` "blocked", but its file still says `pending`. The file status does not change when `build_plan.ps1` moves a request to the backlog.

`docs/AUTOPLAYTEST.md:169` says finished requests move to `requests/done/`. That folder is not in the clone. It would appear after the first results are filed.

## Metrics

| Metric | Definition | Source |
|---|---|---|
| Flicker (static) | Mean over pixels, outside the Ty region, of the std of luma over N frames. Reported for whole frame, upper half (rocks, sky, fog) and lower half (ground). | `tools/flicker.ps1:2-4`, `tools/playtest/common.ps1:46-58` |
| Motion flicker | Mean of the temporal second difference over a smooth yaw sweep. Reports whole, upper, lower and a 99th-percentile pixel. | `tools/motion.ps1:2`, `tools/motion.ps1:9-18`, `src/debugcam.inl:42-44` |
| Mean abs diff | Mean absolute luma difference, 0-255, outside the Ty region. | `tools/playtest/common.ps1:33-44` |
| Frame cost | GPU milliseconds from the DLL's own timer lines, `[gpu] frame ... ms` every 300 frames. Per-pass timers under-count. | `tools/agent_test.ps1:28`, `src/gpuprof.inl:3`, `docs/START_HERE.md:76` |
| Rock metrics | Saturation and luma of a rock region in `suite.ps1` output. | `tools/suite.ps1:22-25`, `tools/imgmetrics.ps1:6` |

At 2560 by 1440, Ty's box is x 1050 to 1550 and y 560 to 1300. The harness writes it as x, y, w, h = 1050, 560, 500, 740 (`tools/imgmetrics.ps1:6`), and the flicker script uses the same box in its own grid (`tools/flicker.ps1:16`).

## Cost

Not measured by this page.

The author documents the duration of a full autoplaytest run as about one minute per boot, and 6 to 10 minutes for the default five variants (`docs/AUTOPLAYTEST.md:45`). That is the author's estimate.

Fixed waits in the code, for planning: a 120-second wait for gameplay in `agent_test.ps1` (`tools/agent_test.ps1:40`, 40 tries of 3 s), up to three boot attempts (`tools/agent_test.ps1:35`), 45 s settle on the billabong spot (`tools/playtest/autoplaytest.ps1:109`).

## Limits

- **Windows only.** The key tool calls user32 (`tools/key.ps1:7-17`). The scripts assume PowerShell 5.1 (`docs/START_HERE.md:74`).
- **Hard-coded owner paths.** Most scripts default to a workstation root that is hard-coded (for example `tools/agent_test.ps1:18-20`, `tools/playtest/common.ps1:94`, `tools/playtest/common.ps1:129`). Some scripts call sibling scripts by that absolute path (for example `tools/suite.ps1:11`). This is a known limitation, not an instruction. See NEEDS INVESTIGATION P12-6 and N-13.
- **Mouse hover menus.** Do not move the mouse during a run (`docs/START_HERE.md:73`, `tools/ty_play.ps1:18-19`).
- **Focus.** A key that arrives without focus is not sent. A run can end with missing shots (`tools/key.ps1:36`, `docs/AUTOPLAYTEST.md:86`).
- **Flicker metric is a heuristic.** Animated grass and clouds count as flicker, which is why the stock ratio is the check (`docs/AUTOPLAYTEST.md:83`). Because the metric is a pixel average, a small local defect can hide in it (inferred from the definition in `tools/flicker.ps1:3`).
- **Static and yaw tests do not reproduce the owner's flicker.** They run at high frame rate. Use `sleepMs` in `motion.req` for slow GPU emulation (`docs/START_HERE.md:76`, `src/debugcam.inl:42`).
- **Spots.** The billabong spot is not verified on the PC (`tools/playtest/boot_spot.ps1:5`). The red-rock view needs the free-camera sweep. The portal spot is not wired (`docs/AUTOPLAYTEST.md:76`, `:84`).
- **External helpers.** `suite.ps1` writes a step-log entry through `tools/steplog.ps1` unless `-NoSync` is given (`tools/suite.ps1:32`). Its header also names an external sync (`tools/suite.ps1:4`). That sync is not described in the repo.
- **No results in the repo.** No autoplaytest report, `visual_review.json` or `ledger.csv` is in the clone. Plan check results are not known from the repo.

## Known issues

- **Harness work items.** The automated playtest was added with #64 and #65, the game lock with #66, and the progress log, plans, request queue, focus fix and visual review with #91 (`CHANGELOG.md:23`, `CHANGELOG.md:29`). Titles and states of these issues are NEEDS INVESTIGATION (N-19).
- **Billabong boot failure on a coordinator run.** Cause: an empty `-TargetPid` in ENTER presses. `KeyEnter` now throws a clear error when there is no game window (`docs/AUTOPLAYTEST.md:147-148`, `tools/ty_play.ps1:29`). A rerun is needed to confirm (`docs/AUTOPLAYTEST.md:148`).
- **Whole-frame flicker did not separate DebugWater variants.** The author added the Ty-region check `tyflicker` for water questions (`docs/AUTOPLAYTEST.md:137`, `:149`). Issue #80 is the bisect.
- **Overlay check issue number.** `docs/AUTOPLAYTEST.md:105` ties the overlay check to #12. The plans tie it to #79 (`tools/playtest/plans/next_monitor.json:68-87`). See Discrepancies.
- **One contact sheet per effect is not built** (`docs/AUTOPLAYTEST.md:159`).

## How to verify

Steps that need no game (not run by the documenter; read the code paths first):

1. Confirm the phase gate refuses. Run `tools/playtest/autoplaytest.ps1 -Plan <plan file>` on a machine without a phase file. It must stop with "phase is ... not TEST" (`tools/playtest/common.ps1:96-100`). It runs no game.
2. Confirm the plan check. Run the same command with a plan that does not reference one of the variants. It must refuse before the phase check (`tools/playtest/autoplaytest.ps1:24-26`).
3. Dry-run the request merge on a copy of `tools/playtest/`. `build_plan.ps1` only reads request files and writes a plan and `BACKLOG.md` (`tools/playtest/build_plan.ps1:44-77`). Run it on a copy because it writes into the requests folder.

Steps for a real run (owner only, one game at a time, phase `TEST`):

1. Check that no game holds the lock. The lock file holds a pid. A live pid refuses the run (`tools/playtest/common.ps1:106-111`).
2. Run `tools/playtest/autoplaytest.ps1 -Plan <plan>`, with `-Variants` if only some are needed.
3. Watch `progress.log` and `status.json` in the results folder (`docs/AUTOPLAYTEST.md:113-125`).
4. Read `report.md`: check table first, then the Issues section. A FAIL line gives the `next` step.
5. Open `contact_sheet.png` and the per-variant `tyrender.log` copies. Look for `[state] LEAK` lines (`src/stateguard.inl:39`, needs `[debug] StateGuard=1`) and `[spill]` lines. See [debugging.md](debugging.md).

Quick single-DLL check, no plan (owner only):

1. `tools/agent_test.ps1 -Dll <dll> -Snaps "name"` swaps the DLL in, boots, takes one snapshot and prints `PNG`, `PERF` and `LOG` lines (`tools/agent_test.ps1:10`, `:95`).
2. It restores the installed DLL and ini at the end (`tools/agent_test.ps1:99`).

## NEEDS INVESTIGATION

- **P12-1** Whether the billabong spot (save slot 2) boots reliably. `docs/AUTOPLAYTEST.md:75` says it "is reliable" and "not yet verified on the PC" on the same line. Method: run `tools/playtest/boot_spot.ps1 -Spot billabong` several times on the PC and count runs that reach state 5.
- **P12-2** Whether `-IniFile` takes effect at boot. `docs/START_HERE.md:70` says it is not read at game start. `tools/agent_test.ps1:32` copies it into the game folder before the boot, and `src/tyrender.cpp:714` loads the ini in `DllMain`, before the game reaches gameplay. Method: run `agent_test.ps1 -IniFile <ini with a changed restart-only key>` and read the copied `tyrender.log`. The `[light] config` line prints MapScale (`src/light.inl:27`).
- **P12-3** Results of past runs. No report folders, `visual_review.json` or ledger are in the clone. Method: the owner's results folder (see N-13 for the path) or the comments on #64 to #66 and #79 to #86.
- **P12-4** Whether `req-82-waterboost` is blocked. `docs/AUTOPLAYTEST.md:171` says blocked, its file says `pending`. Method: run `build_plan.ps1` on a copy and read the BACKLOG output.
- **P12-5** Whether the `visual_review.json` step has been run on a real contact sheet. The rubric is in the repo, but no output is. Method: owner's results folder.
- **N-24** (from section 6) Whether the F10 overlay closes cleanly in a level. Use the `overlay_billabong` and `overlay` variants.
- **N-19** (from section 6) Titles and states of #64 to #66, #79 to #86 and #91.
- **N-13** (from section 6) Inventory of hard-coded workstation paths in `tools/` and the decision on a config file.

## Discrepancies

1. `docs/START_HERE.md:70` says `-IniFile` is not read at game start. `tools/agent_test.ps1:32` copies it before the boot, and `src/tyrender.cpp:714` reads the ini in `DllMain`. The code path suggests the START_HERE trap describes an older version. Code wins; the remaining question is P12-2.
2. `docs/AUTOPLAYTEST.md:75` calls slot 2 "reliable" and also "not yet verified on the PC". `tools/playtest/boot_spot.ps1:5` says it is not verified. The text is self-contradictory. Code and file agree that it is unverified.
3. `tools/ty_play.ps1:2-3` says the route ends on "save GAME 2 start, Rainbow Cliffs". The code lands on GAME 1 by default and on GAME 2 only with `TY_SLOT=2` (`tools/ty_play.ps1:33`). `docs/agent-tasks/BRIEFING.md:74` agrees with the code.
4. `docs/AUTOPLAYTEST.md:105` ties the overlay check to issue #12. `tools/playtest/plans/next_monitor.json:68-87` and `current_run.json:6` tie it to #79. Both can be true (#12 is the overlay, #79 is the in-level check). The plan text should name both.
5. `docs/START_HERE.md:61` omits `TY_PERFCAM` and `TY_LOCK_TOKEN` from the environment variable list.
6. `tools/playtest/requests/req-82-waterboost.json:3` says `pending`; `docs/AUTOPLAYTEST.md:171` says blocked. See P12-4.
7. First-pass `TESTING.md` section 2 says the harness writes into the game folder "only `tyrender.ini` and `tyrender\cam.txt`". `tools/ty_play.ps1:11-12` also deletes `TyOutputLog.txt` and `tyrender.log`, and the game writes `Screenshots\`, `tyrender\snaps\` and `tyrender\logs\`.
8. First-pass `TESTING.md` section 6 says `ty_play.ps1` is used "by key.ps1 location in common.ps1". The code: `tools/playtest/common.ps1:223-234` calls `focus_game.ps1` and `key.ps1`. `ty_play.ps1` is called by `tools/playtest/boot_spot.ps1:23-26`.
9. First-pass `TESTING.md` line numbers: the Ty region is at `tools/playtest/common.ps1:7`, not lines 4-5. `$defs` spans `tools/playtest/autoplaytest.ps1:51-79`, not 51-75.
10. First-pass `TESTING.md` section 7 says the rubric's use in the gate "was not verified". `tools/playtest/autoplaytest.ps1:236-238` uses it as an advisory input.

## See also

- [ab-captures.md](ab-captures.md): F11 pairs and harness A/B.
- [debugging.md](debugging.md): debug keys, logs, shader dumps.
- [tester-guide.md](tester-guide.md): what testers send.
- [../reference/tools-reference.md](../reference/tools-reference.md): every script in `tools/`.
- [../reference/hotkeys-and-files.md](../reference/hotkeys-and-files.md): trigger files and keys.
- [../decisions/known-issues.md](../decisions/known-issues.md): open issues behind the checks.

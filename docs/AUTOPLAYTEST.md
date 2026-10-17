# Automated playtest

`tools/playtest/` boots the renderer build to a fixed spot, takes A/B screenshots, measures flicker, checks the F10 overlay, collects `tyrender.log`, and writes a report. It is read-and-screenshot only, so a session can run it without a person at the keyboard. It is not a substitute for the owner's playtest: it cannot judge how things look.

## Files

| file | what it does |
|---|---|
| `tools/playtest/autoplaytest.ps1` | Main run. Variants, checks, report, contact sheet |
| `tools/playtest/boot_spot.ps1` | Boots one game to gameplay (state 5) and exits 0 or 1. Wraps `tools/ty_play.ps1` |
| `tools/playtest/common.ps1` | Shared helpers: ini key setter, focused key press, A/B shot capture, luma metrics, contact sheet |
| `tools/ty_play.ps1` | Existing menu route (ESC/ENTER/mouse hover to save slot GAME 1 or GAME 2) |
| `tools/key.ps1` | Key sender (ENTER, arrows, letters, F1-F24), focus checked before every key |

## Run it

On the PC, from the repo's `tools` folder or wherever the repo is checked out:

```
powershell -NoProfile -File tools\playtest\autoplaytest.ps1
powershell -NoProfile -File tools\playtest\autoplaytest.ps1 -Variants stock,mod
powershell -NoProfile -File tools\playtest\autoplaytest.ps1 -Copy
powershell -NoProfile -File tools\playtest\boot_spot.ps1 -Game "C:\CLAUDE\TY the Tasmanian Tiger"
```

Default game folder: `C:\CLAUDE\TY the Tasmanian Tiger`. Default output: `C:\CLAUDE\RESEARCH\autoplaytest\<time>\`.

Each run:

1. Tests the live game folder by default: the live folder always holds the newest main build, and that is the build to test. `-Copy` tests a copy of the folder (without `Screenshots`) in `<out>\game` instead, for a run that must not touch the live folder.
2. Refuses to start if a game process is already running from that folder. Only the game-slot holder runs, and the slot is announced in the Fix plan thread first. So the processes it stops are always the ones it started.
3. For each variant, backs up `tyrender.ini`, sets the variant's keys, boots, runs the checks, and stops the game it started. The restore runs in a `finally`, so it happens even when a boot fails.
4. Restores `tyrender.ini` and leaves the game closed. Only the Fix plan thread installs builds, so the live folder is never replaced during a run.

Variants (edit `$defs` in `autoplaytest.ps1` to add more):

| variant | what it is | checks |
|---|---|---|
| `stock` | `TY.exe`, no renderer (baseline) | boot, shots, flicker baseline |
| `mod` | `TY_tyrender.exe` with the installed `tyrender.ini` | boot, shots, flicker, log |
| `overlay` | `[overlay] Key=F10` | F10 open, F10 close, closed frame must match the baseline |
| `fog_seethru1` | `[fog] SeeThruDepth=1` | boot, shots, log (for the #4 A/B) |
| `emit1` | `[emit] Enable=1` | boot, shots, log (for the #9 portal light A/B) |

Each boot takes about a minute, so the default five variants take roughly 6 to 10 minutes.

## Output

`<out>\report.md` has a table per variant (boot, alive, shots, flicker, overlay, log hits) and a checklist:

- `PASS` / `FAIL` for boot, shots, overlay geometry, and the restored ini.
- `PASS` / `CHECK` for flicker: lower-half temporal std over 10 frames, divided by the stock value. The limit is `-FlickerRatioMax` (default 1.5). This is a heuristic, so look at the frames too.
- `<out>\contact_sheet.png` has one row per variant with its first A/B pair, and an overlay row when the overlay ran.
- `<out>\<variant>\` has every PNG and a copy of `tyrender.log`.

## Hotkeys and files it relies on

- A/B shot key: F11 (`[shots] Key`). Each press writes two PNGs into `<game>\Screenshots\`. The script copies the new pair out.
- Overlay: F10, set with `[overlay] Key=F10`. Confirm the key format on the PC. The shots section uses `F11`, and the overlay may use a different format.
- `tyrender.log` in the game folder. `ty_play.ps1` deletes it at the start of each boot.

## What it covers

- Boot to Rainbow Cliffs (save slot GAME 1, `-Spot rainbow`) or slot GAME 2 (`-Spot slot2`), then the menu route in `ty_play.ps1`.
- Screenshot capture with shaders on and off (stock vs mod), and per-key variants.
- Flicker as the temporal standard deviation of luma over a static camera, with Ty excluded.
- Overlay open and close: the closed frame must match the baseline.
- Game alive at the end, and error, failed or assert lines in `tyrender.log`.

## Spots

| `-Spot` / variant `Spot` | save slot | status |
|---|---|---|
| `rainbow` (default) | 1 | Slot 1 is the Red Mountain start, per the owner. The harness notes call it Rainbow Cliffs. The red rock and skybox views are found with the free camera (below). |
| `billabong` | 2 (level 2) | Slot 2 plays a real-time cutscene with a scripted camera on every load. It is reliable, so it is the water and trees start. `autoplaytest` waits 45 s here, not 12 s. Not yet verified on the PC. |
| portal | none yet | Derive it from slot 1 with the free camera once the red rock view is known. |

Free camera: `tyrender\cam.txt` holds `dx dy dz yawDeg pitchDeg`, read every frame by `debugcam.inl`. It moves the camera relative to the spot's start. Variant `sky_yaw000/090/180/270` sweeps yaw on slot 1 to find the red rock and skybox view. Pick the frame that shows it, then give that offset to a named variant. The original `cam.txt`, if any, is restored after the run.

## What it does not cover

- Looks: fog colour, shadow quality, portal light, water appearance. The script produces frames and numbers. A person or a session has to look at `contact_sheet.png`.
- Flicker: the metric also counts animated grass and clouds, which is why the stock ratio is the check.
- Other spots: Billabong water, level two trees, red rock and the portal are not wired. They need save files or scripted walking. This is the next step.
- Frame times: per-frame GPU timings are not parsed from `tyrender.log`. Read them from the log copy in each variant folder.
- Focus: the game window must be foreground. `key.ps1` fails visibly when it is not, so a run can end with missing shots, which the checklist reports.
- Copy runs (`-Copy`) use whatever was in the live folder when the run started, not later edits.

## Rules

- One game at a time. Announce the slot in the Fix plan thread before you start and after you stop.
- Read-only on the install: a run never writes `tyrender.dll`, `TYLauncher.exe` or the live `tyrender.ini`.
- Do not bypass any permission prompt. If a step is blocked, report which step and why.

## Phase gate and game lock

Only one game may run, and playtesting starts only after every session has finished editing and merged.

- **Phase**: `C:\CLAUDE\LOCK\phase.txt` holds one word: `EDIT`, `FREEZE` or `TEST`. The Fix plan thread writes it. `autoplaytest.ps1` and `boot_spot.ps1` refuse to run unless it says `TEST` (a missing file counts as not TEST).
- **Lock**: `C:\CLAUDE\LOCK\game.lock` holds owner, pid, token and time. A run takes it before any launch and releases it in a `finally`. If the lock's pid is no longer running, the lock is stale and gets replaced. If a live pid holds it, the run refuses to start.
- A run that holds the lock passes its token to child scripts in `TY_LOCK_TOKEN`, so `boot_spot.ps1` runs inside the parent's lock and does not take it twice.

Procedure: the Fix plan thread sets `phase.txt` to `TEST` once all merges are done, then starts the run. It sets the phase back to `EDIT` or `FREEZE` after the run.

## Overlay check (issue #12)

- `overlay` runs on slot 1 and `overlay_billabong` on slot 2 (after the 45 s cutscene settle). Each sets `[overlay] Key=F10`, then does: baseline frame, F10 open and frame, F10 close and frame. The closed frame must match the baseline.
- `report.md` lists every `[overlay]` line from `tyrender.log` (the GL state dumps from #72) under its variant.
- Not built: the chapter reload check. It needs the menu steps to reload a chapter, and those are not wired yet.

## Watching a run live

Every run writes to stable files that can be tailed while it is going:

- `C:\CLAUDE\RESEARCH\autoplaytest\progress.log`: one timestamped line per event (run start, each variant start, boot OK or FAILED, variant done, run end), plus a heartbeat every 20 s. The heartbeat shows whether a game from the run's folder is alive (pid and CPU seconds), the newest screenshot and the last write time of `tyrender.log`.
- `C:\CLAUDE\RESEARCH\autoplaytest\status.json`: state (`running` or `finished`), the current variant, how many variants are done, and the last update time.

Every line is also printed to the console. To follow it in PowerShell:

```
Get-Content C:\CLAUDE\RESEARCH\autoplaytest\progress.log -Wait -Tail 20
Get-Content C:\CLAUDE\RESEARCH\autoplaytest\status.json
```

The heartbeat runs as a background job and stops when the run ends. A stale heartbeat (no new lines for more than a minute) means the run has stopped or hung.

## Plans: every run states its purpose and checks

`autoplaytest.ps1 -Plan <file>` is required. The run refuses to start if:
- no plan is given, or the plan file is missing, or
- a variant in the run is not used by any check in the plan.

A plan is JSON (`tools\playtest\plans\<name>.json`) with `name`, `purpose`, and `checks`. Each check has:
- `id`, `kind` (`boot`, `overlay`, `flicker`, `tyflicker`, `log` or `manual`), and `variants`
- `issue` and `decides`: what the check settles, and which issue it feeds
- `pass`: the criterion in words, and `next`: what to try when it fails
- for `flicker`: `baseline` and `limit` (lower-half temporal std ratio). The whole-frame metric is mostly background (grass, clouds, sky), so it does not separate the DebugWater variants (about 40 to 41 for 0, 1 and 2).
- for `tyflicker`: `baseline` and `limit`. Same idea, but measured over Ty's rectangle only (x 0.41 to 0.605, y 0.39 to 0.90): the temporal std of the blue excess inside it, divided by the baseline's. Use this for water and Ty-flicker questions (#5, #80).
- for `log`: `pattern` (regex) and `mustMatch` (true = the line must appear, false = it must not)

Each check is decided and logged as it runs (`[PASS]`, `[FAIL]`, `[INCONCLUSIVE]`). A missing variant or log gives INCONCLUSIVE, never a guess. `manual` checks are INCONCLUSIVE until a person looks at `contact_sheet.png`.

`report.md` in the run folder starts with the purpose, then a table of check results with evidence, then an Issues section: PASS means "supported by this run, issue closes only after the owner's playtest", FAIL gives the `next` step.

Plans: `current_run.json` (the first full run, written after the fact) and `next_monitor.json` (issues #79 to #84 and #86).

Rerun list for the next plan:
- `trees_see1` (billabong) failed at boot on the coordinator's run. The likely cause was an empty `-TargetPid` in the ENTER presses of `ty_play.ps1`. `KeyEnter` now throws a clear error when no game window exists, so rerun it to confirm the boot works.
- The `tyflicker` checks in `next_monitor.json` and `req-80-debugwater` replace the whole-frame flicker check, because that check did not discriminate between the DebugWater variants.

## Request queue (other threads and agents)

Requests live in `tools\playtest\requests\`, one JSON file each, `req-<id>.json`. Template: `requests\TEMPLATE.json`.

Flow:
1. A thread or agent adds a request file (`status: pending`) with its issue number, the question, the variants it needs (existing names from `autoplaytest.ps1`), and checks with the pass criterion.
2. Before a run, the harness owner runs `build_plan.ps1`. It merges all pending requests into `plans\merged_<stamp>.json`, de-duplicates checks, and lists the variants to run. A request that needs a variant the harness doesn't define is marked blocked and written to `requests\BACKLOG.md` with the missing capability named. Nothing is dropped silently.
3. Run the merged plan: `autoplaytest.ps1 -Plan plans\merged_<stamp>.json -Variants <list>` (in the TEST phase only).
4. After the run, the owner posts each request's PASS, FAIL or INCONCLUSIVE with evidence on its issue, tells the requesting thread through the coordinator, and moves the request file to `requests\done\` with a link to the report.

Seeded requests: `req-79-overlay`, `req-80-debugwater`, `req-81-fog`, `req-82-waterboost` (blocked: needs WaterBoost variants), `req-83-gi-lod-cost`, `req-84-portal-emit`, `req-85-harness` (backlog), `req-86-recording-ambiguity` (backlog: a human recording). Pending PRs (#89 FoliageBoost, WaterMaskSoft, AtrousFar) are not seeded until they exist.

Template:

```json
{
  "id": "req-<issue-or-short-name>",
  "status": "pending",
  "priority": 2,
  "requester": { "issue": 0, "thread": "" },
  "question": "What must be validated, in one sentence.",
  "variants": [ { "name": "existing_variant_name", "note": "why" } ],
  "checks": [ {
      "id": "short-check-id", "kind": "log", "variants": ["existing_variant_name"],
      "pattern": "regex", "mustMatch": true,
      "pass": "the criterion", "decides": "what a PASS or FAIL means", "next": "what to try if it fails"
  } ],
  "report": ""
}
```

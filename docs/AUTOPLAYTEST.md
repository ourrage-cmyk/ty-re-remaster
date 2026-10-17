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
powershell -NoProfile -File tools\playtest\boot_spot.ps1 -Game "C:\CLAUDE\TY the Tasmanian Tiger"
```

Default game folder: `C:\CLAUDE\TY the Tasmanian Tiger`. Default output: `C:\CLAUDE\RESEARCH\autoplaytest\<time>\`.

Each run:

1. Copies the game folder (without `Screenshots`) to `<out>\game` and tests the copy. The live install is never written, killed or focused. `-LiveGame` tests the live folder instead, and should not be used unless the owner asks for it.
2. Refuses to start if a game process is already running from that folder. Announce game slots in the Fix plan thread first.
3. For each variant, backs up `tyrender.ini`, sets the variant's keys, boots, runs the checks, and stops the game it started.
4. Restores `tyrender.ini` and leaves the game closed.

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

## What it does not cover

- Looks: fog colour, shadow quality, portal light, water appearance. The script produces frames and numbers. A person or a session has to look at `contact_sheet.png`.
- Flicker: the metric also counts animated grass and clouds, which is why the stock ratio is the check.
- Other spots: Billabong water, level two trees, red rock and the portal are not wired. They need save files or scripted walking. This is the next step.
- Frame times: per-frame GPU timings are not parsed from `tyrender.log`. Read them from the log copy in each variant folder.
- Focus: the game window must be foreground. `key.ps1` fails visibly when it is not, so a run can end with missing shots, which the checklist reports.
- Live-DLL edits: the copy holds whatever tyrender.dll and tyrender.ini were in the live folder at the start of the run.

## Rules

- One game at a time. Announce the slot in the Fix plan thread before you start and after you stop.
- Read-only on the install: a run never writes `tyrender.dll`, `TYLauncher.exe` or the live `tyrender.ini`.
- Do not bypass any permission prompt. If a step is blocked, report which step and why.

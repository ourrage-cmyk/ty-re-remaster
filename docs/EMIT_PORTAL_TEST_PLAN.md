# Portal emitter light: test plan (#9, step 2)

Status: prepared, not run. The game is not launched by this plan until the owner closes their own session.

## Inputs

- Test ini: `tools/test_ini/emit_portal_test.ini`. It is the default ini with `[emit] Enable=1` and `[tex] LogUnknown=1`. Nothing else differs (checked with a diff).
- Default ini `src/default_tyrender.ini` is not changed by this plan (`[emit] Enable=0`).
- Build: `src/build.ps1` without `-Install`. Output goes to `src\out\`.

## Procedure (run only when the game is free)

1. Confirm no `TY_tyrender` or `TYLauncher` process is running. If one is, stop and report.
2. Back up the game folder's current `tyrender.ini` (if any) and `tyrender.dll`, `TYLauncher.exe`, to a new `BACKUP\pre_emit_test_<timestamp>\` folder. Do not overwrite anything without this backup.
3. Run the "before" capture with `[emit] Enable=0` (the default ini), using `tools/shot.ps1` with the Medium preset, at the portal spot in the save slot used by `tools/ty_play.ps1`. Save `before_*.png` and the `[gpusub]` timing lines.
4. Copy `tools/test_ini/emit_portal_test.ini` to the game folder as `tyrender.ini`. Run the same capture from the same position and camera. Save `after_*.png` and the `[gpusub] ... | emit` timing lines.
5. Restore the backed-up files from step 2 and confirm the owner's install is unchanged.

## Pass criteria

- The ground around the portal is brighter in `after` than in `before`. The character's lighting is not reduced.
- `emit` GPU time is under 1 ms at 2560x1440 with the portal on screen (the plan's gate in `docs/MATERIAL_CLASSIFICATION_PLAN.md` section 4).
- No new `[state] LEAK` lines in the log, and no GL errors.

## What the timer measures

`src/gpuprof.inl` marks 20 and 21 wrap the `RunEmit` call in `src/tyrender.cpp`. The `[gpusub]` line reports the average over 300 frames as `emit <ms>`.

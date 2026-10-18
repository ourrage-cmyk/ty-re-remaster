# Installer and uninstaller (Setup.exe and Uninstall.exe)

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: B (testers and modders), C for the code.

## Purpose

`TY HD Renderer Setup.exe` puts the renderer into the game folder in one step. It finds the game, backs up every file it replaces, writes the renderer files, makes a patched copy of `TY.exe` named `TY_tyrender.exe`, merges missing settings into `tyrender.ini`, and adds an Apps and Features entry.

`TY HD Renderer Uninstall.exe` reverses that. It removes what the install added, puts back what it replaced, and removes the Apps and Features entry.

Both programs share `src/installer/common.inc`. Source: `src/installer/setup.c` (119 lines), `src/installer/uninstall.c` (91 lines), `src/installer/common.inc` (193 lines). The shared patch code is `src/patch_exe.inc` (49 lines).

The user-facing description is in the README ("Easiest", "Uninstall"), which matches the code on the main points (`README.md:184-207`).

## How it works

### Setup

1. Command line (`src/installer/common.inc:59-68`): `--dir <folder>`, `--dry-run`, `--quiet`.
2. Find the game folder (`src/installer/setup.c:93-101`):
   - With `--dir`, use that folder.
   - With `--quiet` and no `--dir`, exit with code 2 (`:95`).
   - Otherwise, look for the game in Steam (`FindSteamGame`, see below). If found, ask Yes (install here), No (choose another folder), or Cancel (quit). If not found, show the folder picker (`BrowseGame`, `common.inc:173-180`).
3. Check that `TY.exe` is in the chosen folder (`setup.c:102-103`).
4. Open the log (`setup.c:104`, `common.inc:52-56`): `<game>\tyrender\install.log`. In a dry run the log is `dry-run.log` next to the Setup exe.
5. Refuse if `TY.exe` or `TY_tyrender.exe` is running (`setup.c:106`, `common.inc:95-102`). The check is by process name, not by window.
6. Ask for confirmation. The text lists the folder and says that replaced files are backed up and that `TY.exe` is not changed (`setup.c:107-110`). `--quiet` skips the box (`common.inc:34`).
7. `DoInstall` (`setup.c:77-87`):
   1. Create `tyrender\` and write the install record (`WriteInstallInfo`, `setup.c:67-75`).
   2. Write `TYLauncher.exe` from resource 1 (`setup.c:80`, `:15-27`).
   3. Write `tyrender.dll` from resource 3 (`setup.c:81`).
   4. Write the uninstaller from resource 2 (`setup.c:82`).
   5. Create `TY_tyrender.exe` by patching a copy of `TY.exe` (`InstallPatched`, `setup.c:29-41`, `src/patch_exe.inc:6-48`).
   6. Write `tyrender.ini` from resource 4 when missing, or merge missing keys into the existing one (`InstallSettings`, `setup.c:43-65`).
   7. Add the Apps and Features entry (`setup.c:85`, `common.inc:107-120`).
8. Show the final message (`setup.c:113-118`). It gives the Steam launch option text and the uninstall paths.

Each payload write follows the same rule (`setup.c:19-23`): if the manifest has no record for the file yet, back up the original first. A file that did not exist is recorded as `new`. A file that existed is recorded as `backup`. Running Setup again does not back up the renderer's own files a second time.

### Settings merge

`IniMergeMissing` (`common.inc:125-141`) reads the bundled default ini line by line. For each key it checks whether the user's file already has that key. A key the user already has keeps its value. A missing key is added with the bundled value. Keys that exist only in code (not in the bundled ini) are not added.

One special case (`setup.c:60-63`): if `[overlay] Key` is `0`, it is changed to `F10`. The code comment says older shipped files had `0`. This also runs on re-install, so an explicit `0` set by a user is changed back to `F10`. This is by design per the comment (`setup.c:60`).

### What is written to the game folder

| Path | Source | Recorded as |
|---|---|---|
| `TYLauncher.exe` | resource 1 (`setup.c:80`) | new or backup |
| `tyrender.dll` | resource 3 (`setup.c:81`) | new or backup |
| `TY HD Renderer Uninstall.exe` | resource 2 (`setup.c:82`) | new or backup |
| `TY_tyrender.exe` | patched copy of `TY.exe` (`setup.c:29-41`) | new or backup |
| `tyrender.ini` | resource 4 when missing (`setup.c:47-51`), else merged (`:53-64`) | new or backup |
| `tyrender\install.ini` | the manifest (`common.inc:86-92`) | not a payload file |
| `tyrender\install.log` | log (`common.inc:52-56`) | not recorded |
| `tyrender\backup\<name>` | copy of each replaced original (`common.inc:186-193`) | not recorded |
| Registry key | `HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\TYHDRenderer` (`common.inc:19`, `:107-120`) | removed by uninstall |

The Apps and Features entry is per user (HKCU), so no administrator rights are needed for the registry part (`common.inc:104`). It uses display name "TY HD Renderer (mod)" (`common.inc:112`) and the uninstaller as its uninstall string (`common.inc:117`).

`TY.exe` is never written. `PatchExe` opens the input read-only (`src/patch_exe.inc:7`) and writes only the output file (`:42`).

The patch itself: it refuses a file that is not a 32-bit PE, refuses a file that already has the `.tyimp` section, and refuses when there is no room for one more section header (`src/patch_exe.inc:11-16`). It adds a section that holds a new import table for `tyrender.dll` with the single function `tyrender_init` (`:24-36`). It sets the Large Address Aware flag (`:38`). It clears the bound-import directory (`:41`).

### Dry run

With `--dry-run` (`common.inc:62`), nothing is written, but the log is produced. The log goes next to the Setup exe as `dry-run.log` (`common.inc:54`). Backups are logged but not made (`:191`). The manifest is not written (`:90`). The registry entry is skipped (`setup.c:85`). The final message says "dry run, nothing changed" (`:115`).

### Steam game detection

`FindSteamGame` (`src/installer/common.inc:145-170`):

1. Read `SteamPath` from `HKCU\Software\Valve\Steam` (`:147-148`).
2. Check `steamapps\common\TY the Tasmanian Tiger\TY.exe` under that Steam folder (`:154-155`).
3. Read `steamapps\libraryfolders.vdf` and look at each `"path"` entry in turn (`:156-168`). Each entry is checked for the same game folder. Backslash escapes in the file are handled (`:163`).

Only Steam is searched. Other stores, and a game installed in a path the VDF does not list, need the folder picker.

### Uninstall

`uninstall.c` (`wWinMain`, `:63-91`):

1. The game folder is the `--dir` value, or the folder the uninstaller runs from (`:67`).
2. If there is no `tyrender\install.ini`, show a message and exit with code 1 (`:70-73`). The message tells the user to delete the three files by hand.
3. Refuse if the game is running (`:74`).
4. Ask for confirmation (`:75-77`).
5. Ask whether to keep `tyrender.ini` (`:78-79`). In `--quiet` mode the answer is "no", so settings are restored or deleted without asking.
6. `RestoreAll` (`:23-36`) goes through the manifest's `[files]` section. A `backup` entry is copied back from `tyrender\backup\`. A `new` entry is deleted. The uninstaller itself is skipped here (`:29`). Keeping settings deletes the saved original and leaves the current file (`:30-33`).
7. `RemoveRuntimeFiles` (`:39-43`) deletes `tyrender.log` and `tyrender\reload` when they exist.
8. Delete the manifest, remove the registry entry, and start the self-removal step (`:82-86`).
9. `ScheduleSelfRemoval` (`:47-61`) starts a hidden `cmd` that waits about 2 s (`ping -n 3`). It then moves the saved uninstaller back over itself, or deletes it. It deletes `install.ini` and `install.log`, deletes the backup folder, and removes the `tyrender` folder with `rmdir` (no `/s`, so only if empty) (`:55-56`).

## Code pointers

| What | File:line | Note |
|---|---|---|
| Setup entry | `src/installer/setup.c:89-119` | Dialog flow and messages |
| Payload resource ids | `src/installer/setup.c:10-13` | 1 launcher, 2 uninstaller, 3 DLL, 4 ini |
| Payload write and backup | `src/installer/setup.c:15-27` | Manifest check first |
| Patch step | `src/installer/setup.c:29-41` | Calls `PatchExe` |
| Settings step | `src/installer/setup.c:43-65` | Create, merge, overlay migration |
| Install record | `src/installer/setup.c:67-75` | Version, build, date |
| Install order | `src/installer/setup.c:77-87` | `DoInstall` |
| Command-line options | `src/installer/common.inc:59-68` | Dry run, quiet, folder |
| Manifest | `src/installer/common.inc:86-92` | `tyrender\install.ini` |
| Game-running check | `src/installer/common.inc:95-102` | Process names `TY.exe`, `TY_tyrender.exe` |
| Registry entry | `src/installer/common.inc:104-121` | HKCU, no admin |
| Settings merge | `src/installer/common.inc:125-141` | `IniMergeMissing` |
| Steam search | `src/installer/common.inc:145-170` | Registry plus `libraryfolders.vdf` |
| Folder picker | `src/installer/common.inc:173-180` | `SHBrowseForFolderW` |
| Backup helper | `src/installer/common.inc:186-193` | Copies original to `tyrender\backup` |
| Uninstall entry | `src/installer/uninstall.c:63-91` | Prompts and messages |
| Restore | `src/installer/uninstall.c:9-36` | Backup back, or delete new file |
| Runtime file cleanup | `src/installer/uninstall.c:39-43` | Log and reload flag |
| Self removal | `src/installer/uninstall.c:47-61` | Detached `cmd` |
| Shared patch code | `src/patch_exe.inc:6-48` | Same code as the launcher |
| Build of both programs | `src/build.ps1:23-31` | Resource files and link flags |

## Settings

The installer writes `tyrender.ini` only through the merge in `IniMergeMissing`. It does not own any key. The defaults it uses are the ones in `src/default_tyrender.ini`. See [data-files.md](data-files.md) for that file.

The uninstaller can keep the file as it is (`uninstall.c:30-33`) or restore the copy saved before the install (`:34`, `:12-14`).

## Limits

- Windows only. The programs are 32-bit (`src/build.ps1:23-31`, `-municode -mwindows`).
- There is no check that `TY.exe` is the expected Steam build. Only the PE format and the "already patched" marker are checked (`src/patch_exe.inc:11-13`). The supported builds are not listed in the repo (NI-7).
- Steam search covers only Steam. Other sources need the folder picker.
- Running-game detection looks at process names. The launcher uses a window title instead (see [launcher.md](launcher.md)).
- A replaced file is backed up only if it exists when the install first touches it. The backup is a plain copy. The install does not keep a version history.
- If the game folder is not writable (for example, a protected install location), the install fails at the first write and asks the user to close programs (`setup.c:21`, `:25`). Whether a protected location is common is not known (inferred).
- Quiet uninstall deletes the user's settings (see uninstall step 5).
- The uninstaller removes only the names in the manifest, plus `tyrender.log` and `tyrender\reload`. Other files the mod makes are left behind (see Known issues).
- Steam launch option: the installer and launcher both show the text. Neither edits Steam's own settings.

## Known issues

- Re-running Setup changes an explicit `[overlay] Key=0` to `F10` (`setup.c:60-63`). This is by design per the code comment. It may surprise a user who turned the overlay off on purpose.
- After uninstall, these may remain in the game folder (inferred from the code, not run):
  - `tyrender\logs\` with session reports (`src/sesslog.inl:259`);
  - `tyrender\dump`, `tyrender\override`, `tyrender\snaps` (`src/tyrender.cpp:524`, `:710-711`);
  - `Screenshots\` with A/B captures (`src/abshot.inl:92`);
  - a changed `settings.ini` (detail keys) and its `settings.ini.tyhd_backup` copy (`src/launcher_ui.inc:82-99`).
  The uninstaller's final text says "The game is back to its original state" (`uninstall.c:89`). That is not true for the files above.
- The Steam launch option is not removed by the uninstaller. The text says so (`uninstall.c:89`).

## How to verify

1. Dry run: run `TY HD Renderer Setup.exe --dry-run --dir "<game folder>\"` (or pick the folder). Read `dry-run.log` next to the Setup exe. No file in the game folder should change.
2. Install into a copy of the game folder, not the real one. Check that `TY_tyrender.exe` exists and that `TY.exe` has the same SHA-256 before and after.
3. Open `tyrender\install.ini`. Each replaced or created file should appear under `[files]` with `new` or `backup`.
4. Change `[ao] Strength` in `tyrender.ini`, then run Setup again. The value should stay (`common.inc:125-141`).
5. Run the uninstaller and answer "No" to keeping settings. `tyrender.ini` should return to its backed-up version, or be deleted if it did not exist before (`uninstall.c:30-34`).
6. After a few seconds, check that `tyrender\install.ini` and `tyrender\install.log` are gone and that the Apps and Features entry is removed.

## NEEDS INVESTIGATION

- NI-I1 (N-7): Which `TY.exe` builds are supported and whether a hash check is wanted. The installer checks only the PE format and the patch marker (`src/patch_exe.inc:11-13`). Method: test the install with other builds if available; decide on a hash list.
- NI-I2 (N-9): How Steam overlay, achievements and Steam Input behave with the renamed `TY_tyrender.exe`. Method: run from Steam with the launch option and check.
- NI-I3 (Q034): Whether Steam detects the game as running when only `TY_tyrender.exe` runs (the code checks both names, `common.inc:99`). Method: start the game from Steam and run Setup.
- NI-I4: Whether a protected install location (for example one that needs administrator rights) fails at the first file write. Method: install into a folder the user cannot write.
- NI-I5: Whether `[overlay] Key=0` written by a user survives a Setup re-run in every case, or only in the path the code shows (`setup.c:61`). Method: test with a copy of the ini.

## Discrepancies

- First pass (`LAUNCHER.md:140`) says the game's settings backup is restored by the uninstaller. The uninstaller does not touch `settings.ini.tyhd_backup`. Only the launcher writes it (`src/launcher_ui.inc:86`).
- The uninstaller's final message (`uninstall.c:89`) claims the game is back to its original state. The "Known issues" list above shows files that remain. This is a code-versus-text conflict, not a doc conflict.
- README says the uninstaller removes "only the files the install added and puts back the originals it saved" (`README.md:205`). That matches the code for the manifest files. It does not mention the runtime folders or the game settings file.
- The installer reports the game as running by process name (`common.inc:99`). The launcher reports it by window title (`src/launcher_ui.inc:165`). Both are correct for their purpose, but they are different checks.

## See also

- [launcher.md](launcher.md): what the installed launcher does
- [data-files.md](data-files.md): the bundled default ini that Setup merges from
- [source-map.md](source-map.md): every installer source file
- [../how-it-works/02-loading-and-injection.md](../how-it-works/02-loading-and-injection.md): why the patched copy is used instead of a proxy DLL
- [../guides/deploy-package-release.md](../guides/deploy-package-release.md): how the Setup exe is built and packaged
- [../concepts/constraints-and-limits.md](../concepts/constraints-and-limits.md): the 32-bit memory limit and the Large Address Aware flag

# Build from scratch

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), B (tester who builds).
> Verification note: this page was written by reading the scripts. No build was run by the writer (no Windows toolchain in the writing session). Every build step below is marked "not run by the documenter" where it matters.

## Purpose

This page explains how the three shipped programs are built from the source tree: `tyrender.dll` (the renderer), `TYLauncher.exe` (the launcher, which carries a copy of the DLL and the default settings), and the installer pair `TY HD Renderer Setup.exe` and `TY HD Renderer Uninstall.exe`. It covers the one script that does all of it, the toolchain it expects, how the build id is made, and what a build on a new machine needs today. Today a clean build needs edits, because the scripts hard-code the owner's workstation folders.

## How it works

1. **Constants come first.** The script takes the toolchain `bin` folder from a constant (`src/build.ps1:5`). The output folder is `out` inside the source folder (`src/build.ps1:6`). The script finds itself with `$PSScriptRoot`, so it runs from any clone location, but the toolchain path must be edited for each machine.
2. **Build id.** The script runs `git rev-parse --short HEAD` (`src/build.ps1:9`). It adds `-dirty` when `git status --porcelain` reports changes (`src/build.ps1:10`). Without git the id is the word `nogit` (`src/build.ps1:9`). It writes `out/build_id.h` with two defines: a wide string for the launcher title and a narrow string for the DLL (`src/build.ps1:12`).
3. **DLL.** `tyrender.cpp` is compiled as a shared library with `-O2 -shared -static -std=c++17 -Wall`, the link flag `-Wl,--kill-at`, and the libraries `shell32` and `gdi32` (`src/build.ps1:13`). The `.inl` modules are not compiled on their own. `tyrender.cpp` pulls them in with `#include` (`src/tyrender.cpp:434`, `src/tyrender.cpp:540-552`), so any `.inl` change needs a DLL rebuild.
4. **Payload for the launcher.** A resource script `payload.rc` embeds the DLL as resource 1 and `default_tyrender.ini` as resource 2 (`src/build.ps1:15-17`). `windres` turns it into an object file (`src/build.ps1:18`).
5. **Launcher.** `TYLauncher.c` is compiled as C with `-O2 -static -municode -mwindows -Wall` and linked with the payload object and `shell32` and `comctl32` (`src/build.ps1:20`). The launcher reads its payload with `FindResourceW` and `LockResource` (`src/TYLauncher.c:29-31`). It includes `launcher_ui.inc` (`src/TYLauncher.c:79`).
6. **Uninstaller.** `installer/uninstall.c` is built with no payload (`src/build.ps1:24`).
7. **Setup.** A second resource script `setup_payload.rc` embeds four files as resources 1 to 4: the launcher, the uninstaller, the DLL and the default ini (`src/build.ps1:26-27`). `installer/setup.c` is built with that payload (`src/build.ps1:30`). `setup.c` includes `common.inc` and `patch_exe.inc` (`src/installer/setup.c:7-8`).
8. **Failure handling.** `$ErrorActionPreference` is `Stop` (`src/build.ps1:4`). Each compile step checks `$LASTEXITCODE` and throws a named error (`src/build.ps1:14`, `:19`, `:21`, `:25`, `:29`, `:31`).
9. **Listing.** The script lists the output folder when it is done (`src/build.ps1:32`).
10. **Optional install.** With `-Install`, the script copies `tyrender.dll` and `TYLauncher.exe` into a fixed game folder (`src/build.ps1:33-35`) and prints the reverse steps (`src/build.ps1:36`). It does not check whether the game is running.

```
src/tyrender.cpp + src/*.inl ---clang++ (DLL)----------------> out/tyrender.dll --+
src/default_tyrender.ini ----------------------------------------------------------+
                                                                                    |
out/payload.rc: res 1 = DLL, res 2 = ini --windres--> out/payload.res.o             |
src/TYLauncher.c + launcher_ui.inc -----clang (exe)--------> out/TYLauncher.exe <---+ (copied in via payload)
src/installer/uninstall.c --------------clang (exe)--------> out/TY HD Renderer Uninstall.exe
out/setup_payload.rc: res 1 = launcher, 2 = uninstaller, 3 = DLL, 4 = ini
                                                    --windres--> out/TY HD Renderer Setup.exe (from installer/setup.c)
```

Note that the DLL reaches the launcher and the setup program in two different ways. The launcher embeds the DLL through `payload.rc`. The setup program embeds the DLL and the launcher through `setup_payload.rc`.

## Toolchain

- A 32-bit LLVM-MinGW toolchain for Windows. The commands call `i686-w64-mingw32-clang++`, `i686-w64-mingw32-clang`, and `i686-w64-mingw32-windres` (`src/build.ps1:13`, `:18`, `:20`, `:24`, `:28`, `:30`).
- The toolchain folder name in the script carries a dated release build (`src/build.ps1:5`). The name is a constant. Write down the exact release you use.
- The game is 32-bit, which is why the target is i686 (`src/build.ps1:1` comment; `docs/START_HERE.md:80`).
- PowerShell runs the script. The first pass and the owner's notes both use `powershell -NoProfile -File <script>` (`docs/START_HERE.md:44`).
- No other build system is in the repo. There is no CMake, Makefile, or solution file. Checked by listing the repository root and `src/`.

## Code pointers

| What | File:line | Note |
|---|---|---|
| Script parameters (`-Install`) | `src/build.ps1:3` | Only switch. |
| Toolchain folder constant | `src/build.ps1:5` | Must be edited per machine. |
| Output folder | `src/build.ps1:6` | `src/out/`, ignored by git (`.gitignore:19`). |
| Build id from git | `src/build.ps1:8-10` | `-dirty` suffix on a changed tree. |
| `build_id.h` defines | `src/build.ps1:12` | Wide id for launcher, narrow id for DLL. |
| DLL compile | `src/build.ps1:13` | Flags listed in "How it works". |
| Payload resources (DLL, ini) | `src/build.ps1:15-17` | Resource 1 and 2. |
| Launcher compile | `src/build.ps1:20` | Static, `-municode`, `-mwindows`. |
| Uninstaller compile | `src/build.ps1:24` | No payload. |
| Setup payload resources | `src/build.ps1:26-27` | Resources 1 to 4. |
| Setup compile | `src/build.ps1:30` | Uses `installer/setup.c`. |
| Install copy | `src/build.ps1:33-36` | Copies to a fixed folder. |
| Version in launcher title | `src/TYLauncher.c:22-23` | Literal `v0.3.5-rc` plus `TY_BUILD_ID`. |
| Version in installer | `src/installer/common.inc:26` | Literal, separate from the launcher. |
| Version in session report | `src/sesslog.inl:15-16` | Literal, comment says keep in step. |
| Build id in report file name | `src/sesslog.inl:262` | `TYHD_report_..._<build>.txt`. |
| Build id in screenshots | `src/abshot.inl:93` | File name contains the build id. |
| Build id in overlay log | `src/overlay.inl:83` | Logged when the overlay opens. |
| Launcher payload comment | `src/TYLauncher.c:25-27` | Says the exe alone is the release. |
| Build id written at install | `src/installer/setup.c:72-73` | Written to the install section of an ini file. |

## Settings

No `tyrender.ini` keys are used by the build. Build-time constants:

| Constant | File:line | Effect |
|---|---|---|
| Toolchain `bin` folder | `src/build.ps1:5` | Compiler location. |
| Game folder for `-Install` | `src/build.ps1:34` | Copy target. |
| Compile flags | `src/build.ps1:13`, `:20`, `:24`, `:30` | Optimisation and linking. |

Runtime keys are documented in `reference/settings-system-debug.md` and the other settings pages (see See also).

## Cost

Build time: not measured. Output sizes: not measured by the writer.

## Limits

- **Hard-coded owner paths.** The toolchain folder (`src/build.ps1:5`) and the game folder (`src/build.ps1:34`) are fixed to the owner's workstation layout. A text search finds the owner's root-folder name on 134 lines in 43 files under `src/` and `tools/` at this commit (`grep -rF` on the working tree). The same is true of the release, packaging and harness scripts. See NI item B3.
- **Git is needed for the id.** A zip download without `.git` gives the id `nogit` (`src/build.ps1:9`). A release build should come from a clean git checkout.
- **Dirty trees are visible.** `-dirty` is appended when any file differs from HEAD (`src/build.ps1:10`), including untracked files (because `git status --porcelain` lists them).
- **No CI.** Nothing builds the code on push. See `guides/branching-and-process.md`.
- **Single toolchain tested.** Only the owner's toolchain build is known to work. Other toolchain versions are untested.
- **Cross-compile from Linux is untested.** See NI item B2.

## Known issues

- No build issue is listed in `docs/ISSUES_SEED.md` (issues #1 to #17). The build problems here are process problems, not open bugs.
- The version strings are three separate literals (`src/TYLauncher.c:22`, `src/installer/common.inc:26`, `src/sesslog.inl:16`). They are not generated from git. Changing the version means editing all three.
- Build ids are not reproducible across machines unless the same commit is checked out clean. Not tested by the writer (see NI item B5).

## How to verify

Not run by the documenter. Steps for a reader:

1. Clone the repository. Keep `.git`.
2. Install an i686 LLVM-MinGW toolchain. Note its folder name.
3. In a scratch copy of `src/build.ps1` (not in the repo), change the constant at `src/build.ps1:5` to your toolchain's `bin` folder. Also change `src/build.ps1:34` so that it does not point at a game folder you care about, or leave `-Install` out.
4. Run `powershell -NoProfile -File <repo>\src\build.ps1`.
5. Expect `src/out/` to contain these files (the list printed at `src/build.ps1:32`): `build_id.h`, `tyrender.dll`, `payload.rc`, `payload.res.o`, `TYLauncher.exe`, `TY HD Renderer Uninstall.exe`, `setup_payload.rc`, `setup_payload.res.o`, `TY HD Renderer Setup.exe`.
6. Open `build_id.h`. The id must equal `git rev-parse --short HEAD`, with `-dirty` only if `git status --porcelain` is not empty.
7. Record the DLL SHA1. The release gate script names its results with a SHA1 prefix (`tools/g1_gate.ps1:3`), so the hash is how a result is tied to a build. Build twice from the same commit and compare the hashes (NI item B5).

## NEEDS INVESTIGATION

- **B1 (N-12). Clean-machine build.** Does a fresh Windows machine build the repo with only the toolchain path changed? Method: install the LLVM-MinGW release, edit the constants in a scratch copy, run the script, compare the file list with `out/` from the owner's build. Any further fixes are a follow-up code change, not part of this docs task.
- **B2 (N-14). Cross-compile on Linux.** Method: run each command at `src/build.ps1:13`, `:18`, `:20`, `:24`, `:28`, `:30` in a Linux container with the i686 LLVM-MinGW tools. Translate Windows paths in `payload.rc` and `setup_payload.rc` (they use forward slashes, `src/build.ps1:17`, `:27`). Record what fails. Not attempted here.
- **B3 (N-13). Hard-coded owner paths.** Inventory: `grep -rnF` on the root-folder name in `src/` and `tools/`. Decision needed on a config file (for example `tools/env.ps1`, which already hard-codes paths: `tools/env.ps1:2`). Not changed here.
- **B4. Which toolchain built the released files.** The script names a dated toolchain folder (`src/build.ps1:5`). Method: ask the owner for the folder name and the output of the compiler's version command. Not in the repo.
- **B5. Reproducible hashes.** Does the same commit give the same DLL hash on two machines? Method: build twice on one machine, then on a second machine, and compare SHA1. Not tested.

## Discrepancies

- The first pass (`BUILD_AND_RELEASE.md`, section 10, step 3) says to run `src\build.ps1` "from the repository root". The script works from any folder (`src/build.ps1:6` uses `$PSScriptRoot`), but the toolchain path must be edited first (`src/build.ps1:5`). Corrected here.
- The first pass says `docs/PLAN_3DAY_20261016.md` is "not found in this clone" (`BUILD_AND_RELEASE.md`, section 9). The file is in this clone (12627 bytes). The repo wins. This is not a build fact, but it affects the release checklist that cites it.
- The `-Install` warning about agent folders appears in `docs/START_HERE.md:55` and `docs/agent-tasks/BRIEFING.md:22`. The script has no such check (`src/build.ps1:33-36`).

## See also

- [Deploy, package and release](deploy-package-release.md)
- [Contributor onboarding](contributor-onboarding.md)
- [Launcher reference](../reference/launcher.md)
- [Installer and uninstaller](../reference/installer-and-uninstaller.md)
- [Tools reference](../reference/tools-reference.md)
- [Testing and the gate](../guides/test-harness.md)

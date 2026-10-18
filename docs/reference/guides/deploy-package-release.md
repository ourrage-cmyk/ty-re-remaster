# Deploy, package and release

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: C (maintainer), A (contributor who ships a test build).
> Verification note: written by reading the scripts and documents. No script was run by the writer. Results quoted from `docs/results/` are the owner's, with the build they name.

## Purpose

This page covers three things a maintainer does after a change is merged: put a build into a test game folder without breaking the owner's install; package the build into a zip; and release it after the G1 gate passes. It names the scripts that do each step, what each one checks, and what the scripts do not do (no tag, no upload, no commit, no G1 run from the release script).

## How it works

### A. Test deploy (a build that is not a release)

1. **Do not install from a working folder.** `build.ps1 -Install` copies the DLL and the launcher into a fixed game folder (`src/build.ps1:33-35`). The owner's notes say to run it only from the master source folder, never from an agent folder (`docs/START_HERE.md:55`), and the agent brief says agents must never use `-Install` (`docs/agent-tasks/BRIEFING.md:22`).
2. **Prefer the shared harness.** `tools/agent_test.ps1 -Dll <dll>` copies the DLL over the game folder for one test, runs the test, and puts the original DLL and `tyrender.ini` back (`tools/agent_test.ps1:1-14`, header). A backup folder under the tools folder survives a crash, and the next run restores from it (`tools/agent_test.ps1:24-25`).
3. **One game at a time.** The harness takes a named global mutex (`tools/agent_test.ps1:22`) and waits up to 45 minutes by default (`tools/agent_test.ps1:19`, parameter `LockMinutes`; `:23` waits).
4. **Close the launcher first.** An open `TYLauncher.exe` locks the copy in the game folder, so an install or release cannot replace it (`docs/START_HERE.md:72`).
5. **Game must not run during a release.** `tools/release.ps1:6` refuses to run when the game process is found.

### B. Package (a zip for people to download)

1. `src/package.ps1` runs `build.ps1` silently (`src/package.ps1:5`).
2. It stages a folder `dist/TYHD_Renderer_<version>/` (`src/package.ps1:6`).
3. It copies into the stage: `tyrender.dll`, `TYLauncher.exe`, `README.md`, `PERFORMANCE.md` (`src/package.ps1:7`); the installer pair (`src/package.ps1:8`); and the default ini renamed `tyrender.ini` (`src/package.ps1:9`).
4. It makes a zip with `Compress-Archive` (`src/package.ps1:10-11`).
5. The default version is `v0.1` (`src/package.ps1:2`). Pass `-Version` every time.
6. The script header says the zip never includes game files (`src/package.ps1:1`).

```
build.ps1 (silent) -> dist/TYHD_Renderer_<v>/ { tyrender.dll, TYLauncher.exe, README.md,
                      PERFORMANCE.md, Setup.exe, Uninstall.exe, tyrender.ini } -> dist/TYHD_Renderer_<v>.zip
```

### C. Release (`tools/release.ps1`, 19 lines)

1. Refuse if the game process is running (`tools/release.ps1:6`).
2. Refuse if there is no merged DLL in the staging folder (`tools/release.ps1:7`).
3. Back up the master source folder to a timestamped folder. Nothing is deleted (`tools/release.ps1:8`).
4. Copy the staging `.cpp`, `.inl` and `.inc` files into the master, except `launcher_ui.inc` (`tools/release.ps1:9`).
5. Build and install on the master (`tools/release.ps1:10`). The check is that the output contains the text "installed to".
6. Package (`tools/release.ps1:11`).
7. Check the zip. It must contain `tyrender.dll`, `TYLauncher.exe`, `tyrender.ini`, `README.md`, `PERFORMANCE.md`, and it must contain no `.rkv`, `TY.exe` or `.iso` entry (`tools/release.ps1:12-14`).
8. Large Address Aware check. The script patches a copy of the game executable into a temp file and reads the characteristics bit `0x20` from the PE header (`tools/release.ps1:15-17`).
9. Print `OK release <version> : <zip>` (`tools/release.ps1:18`).

The release script does not tag, upload, commit, or run the G1 gate. Its text output starts with `FAIL` when a check fails.

### D. G1 gate (`tools/g1_gate.ps1`, 16 lines)

The gate runs on the shipped DLL. Its header calls it the "release gate on the SHIPPED dll" (`tools/g1_gate.ps1:1`).

1. **Soak 10 minutes** on the Balanced-named preset ini through `tools/soak.ps1` (`tools/g1_gate.ps1:4`; `tools/soak.ps1:3-6`). `soak.ps1` falls back to a High-named ini if the Balanced-named file is missing (`tools/soak.ps1:4`). The soak runs a random walk and fails on a crash, an out-of-memory line or a dead process (`tools/soak.ps1:8`; `tools/agent_test.ps1:14`).
2. **Forced fallback.** A suite run with a fixture ini named `bal_forced31.ini` (`tools/g1_gate.ps1:5`). The fixture is not in the repo (NI item D3).
3. **Performance at four views.** The views are `default`, `vista`, `sand_graze` and `fence_mid` (`tools/g1_gate.ps1:7-12`). A view over 16.7 ms only adds a note (`tools/g1_gate.ps1:11`). It does not fail the gate.
4. **Pass rule.** The gate fails when the soak fails (`tools/g1_gate.ps1:4`), when the forced-fallback time is missing or is larger than the balanced default time (`tools/g1_gate.ps1:14`).
5. **Output.** The results go to `results/g1_gate.md` and the last line is `OK G1` or `FAIL G1` (`tools/g1_gate.ps1:15-16`).

The owner's example result is in the repo at `docs/results/g1_gate.md`. It records a soak FAIL for an earlier DLL (SHA1 prefix `3EE5DA28A7`, line 2) and the view timings in the same file. The consolidated plan says the rerun passed (`docs/PLAN_CONSOLIDATED.md:8`). The rerun record is not in the repo (NI item D2).

### E. Release rules written in the documents

- **Release artifact.** `TYLauncher.exe` is the primary asset. Its SHA1 goes in the release notes. The zip is secondary. Zips, DLLs and EXEs are not committed (`docs/PLAN_CONSOLIDATED.md:61-63`; `.gitignore:2-4`).
- **Tag.** A release is a tag on `main` with the zip attached (`docs/ISSUES_SEED.md:24`; `docs/PLAN_CONSOLIDATED.md:59`).
- **Pre-releases.** A build that has not been played in the game is marked as a pre-release on GitHub (`docs/PLAN_CONSOLIDATED.md:65`).
- **Steps for v0.3.5** (`docs/PLAN_CONSOLIDATED.md:53-59`): merge fixes through staging; run G1 on the merged build; re-measure the full preset ladder; update README, PERFORMANCE and STATUS; take new A/B shots; tag.
- **Owner sign-off.** "Nothing ships without the owner's sign-off" (`docs/PLAN_CONSOLIDATED.md:19`).

## Code pointers

| What | File:line | Note |
|---|---|---|
| Test install from build script | `src/build.ps1:33-36` | Copies two files, no process check. |
| Harness header (usage and restore rule) | `tools/agent_test.ps1:1-14` | Restores the DLL and ini after each test. |
| Harness global mutex | `tools/agent_test.ps1:22-23` | Named `Global\TYGameTest`. |
| Harness backup folder | `tools/agent_test.ps1:24-25` | Crash-safe restore source. |
| Package defaults and stage | `src/package.ps1:2`, `:6` | Default version `v0.1`. |
| Package file list | `src/package.ps1:7-9` | Reads README and PERFORMANCE from the source folder. |
| Zip command | `src/package.ps1:10-11` | `Compress-Archive`. |
| Release default version | `tools/release.ps1:4` | `v0.3`. |
| Game-running check | `tools/release.ps1:6` | Refuses to run. |
| Backup of master | `tools/release.ps1:8` | Timestamped folder. |
| Promotion copy | `tools/release.ps1:9` | Excludes `launcher_ui.inc`. |
| Zip content check | `tools/release.ps1:12-14` | Required and forbidden names. |
| LAA check | `tools/release.ps1:15-17` | Reads the PE characteristics bit. |
| G1 soak call | `tools/g1_gate.ps1:4` | Ten minutes. |
| G1 forced-fallback fixture | `tools/g1_gate.ps1:5` | Fixture name only; contents not in repo. |
| G1 views | `tools/g1_gate.ps1:7` | Four views. |
| G1 pass rule | `tools/g1_gate.ps1:14` | Forced fallback must not be slower than the balanced default. |
| G1 result file | `tools/g1_gate.ps1:15` | `results/g1_gate.md`. |
| Soak default ini | `tools/soak.ps1:3-4` | Balanced name with High fallback. |
| Soak output marker | `tools/soak.ps1:8` | Looks for `SOAK OK`. |
| Ignored outputs | `.gitignore:4`, `:19-20` | `*.zip`, `out/`, `dist/`. |
| Launcher writes its own ini only when none exists | `src/TYLauncher.c:26-27` | Comment on payload behaviour. |
| Launcher running check (for the lock) | `docs/START_HERE.md:72` | Text rule, not code. |

## Settings

No ini keys are used by these scripts. The packaged `tyrender.ini` is a copy of `src/default_tyrender.ini` (`src/package.ps1:9`). The launcher creates the ini only when none exists (`src/TYLauncher.c:26-27`), so an existing user ini is not overwritten. The G1 gate uses fixture inis from a results folder that is not in the repo (`tools/g1_gate.ps1:4-5`).

## Cost

- Release script run time: not measured.
- G1 gate: the owner's record in `docs/results/g1_gate.md` (lines 4-7) gives view times of 9.50 to 10.73 ms for the earlier DLL (SHA1 prefix `3EE5DA28A7`). `docs/PLAN_CONSOLIDATED.md:8` gives the resolution and GPU: 2560x1440 on an AMD RX 7600 XT. These numbers are not a measurement of the current build.

## Limits

- **Windows only.** All of these scripts are PowerShell and call Windows tools (`tools/release.ps1:15-17`, `tools/agent_test.ps1:22`).
- **Owner paths.** The release script's master, staging and backup folders are fixed (`tools/release.ps1:5`), as is the TY.exe path for the LAA check (`tools/release.ps1:15`). See NI item B3 on the first page of this chapter (`guides/build-from-scratch.md`).
- **Package cannot run from the repo layout (inferred; not run).** `src/package.ps1:7` copies `README.md` and `PERFORMANCE.md` from the source folder. In this repo, `README.md` is at the root and `PERFORMANCE.md` is at `docs/PERFORMANCE.md`. `tools/repo_sync.ps1:8-9` shows that the owner's working folder kept both files beside the build script. The script will stop at the first missing file (`src/package.ps1:3` sets `$ErrorActionPreference` to `Stop`).
- **The release script does not run the G1 gate.** The G1 gate must be run first, by hand (`docs/PLAN_CONSOLIDATED.md:55`).
- **Forced-fallback check is a speed test.** `tools/g1_gate.ps1:14` only checks that the fallback is not slower than the balanced default. It does not check the picture.
- **Soak is a crash test, not a quality test.** The random walk can leave the level (see the known issue below).
- **One GPU.** The only measured hardware is the AMD RX 7600 XT (`docs/START_HERE.md:7`).

## Known issues

- **G1 soak FAIL on record.** `docs/results/g1_gate.md:3` says the soak failed for SHA1 prefix `3EE5DA28A7`. `docs/PLAN_CONSOLIDATED.md:8` says the failure was the random walk leaving the level (clean shutdown), not a crash, and that the gate passed on rerun. Both are the owner's statements. The rerun output is not in the repo.
- **Stale default version** in `src/package.ps1:2` (`v0.1`) and in `tools/release.ps1:4` (`v0.3`). Pass the version explicitly.
- **Known inconsistencies before release** are listed in `docs/PLAN_CONSOLIDATED.md:81-86`. Items there include the Balanced-named ini (line 84) and a stale header in the default ini (line 82). See the discrepancies below.
- **Issue #17** tracks the v0.3.5 release (`docs/ISSUES_SEED.md:20`). It is still open in the seed list.

## How to verify

Not run by the documenter. For a reader who has the owner's setup:

1. **Test deploy.** Run `tools/agent_test.ps1 -Dll <path to dll> -Snaps "check"`. Expect output lines starting `PNG`, `PERF`, `LOG` (`tools/agent_test.ps1:17` header). Then confirm the game folder holds the original DLL again.
2. **Package.** In a scratch copy, fix the paths in `src/package.ps1:7` first. Run `src/package.ps1 -Version <v>`. Expect a zip under `dist/` with seven entries: the DLL, the launcher, `tyrender.ini`, `README.md`, `PERFORMANCE.md`, the setup and the uninstaller.
3. **Release gate.** Run `tools/g1_gate.ps1 -Dll <dll>`. Read `results/g1_gate.md`. The last console line must say `OK G1`.
4. **Zip check.** Open the zip and list its names. There must be no `.rkv`, `TY.exe` or `.iso` entries (`tools/release.ps1:13`).

## NEEDS INVESTIGATION

- **D1. Which version is the latest release.** The docs disagree (see discrepancies). Method: in a full clone run `git tag --list` and `gh release list` (the clone here has one commit and no tags). Also check `docs/results/g1_gate*.md` for the build each result names.
- **D2. The G1 rerun record.** `docs/PLAN_CONSOLIDATED.md:8` says the rerun passed for v0.3.4. Method: find the rerun's `g1_gate.md` in the owner's results folder, or run `tools/g1_gate.ps1` again on the same DLL and compare the SHA1.
- **D3. The forced-fallback fixture.** `tools/g1_gate.ps1:5` names `bal_forced31.ini`. Method: open the fixture folder on the owner's machine and confirm it sets `[debug] ForceFallback=31`. The bit meanings are in `src/ao.inl:437`.
- **D4. Published assets.** Whether the v0.3.x releases on GitHub carry `TYLauncher.exe` and its SHA1 as the docs say. Method: `gh release view <tag>`.
- **D5 (N-16). Licence of the zip contents.** The repo has no `LICENSE` file at the root. See `decisions/legal-and-licensing.md`.

## Discrepancies

- **Primary artifact.** `docs/PLAN_CONSOLIDATED.md:62` says `TYLauncher.exe` is the primary asset and the zip is secondary. `docs/ISSUES_SEED.md:24` says the zip is attached to the tag. `README.md:184` tells users to run `TY HD Renderer Setup.exe` from the release. `tools/release.ps1:11-14` builds and checks the zip. These four statements do not agree.
- **Latest version.** `CHANGELOG.md:46` has `v0.3.5 (c255ae8)` and `README.md:9` says the latest release is v0.3.5. `docs/PLAN_CONSOLIDATED.md:7-8` says shipped is v0.3.2 and the latest build is v0.3.4. `docs/START_HERE.md:10-11` says shipped is v0.3.2 and the test build is v0.3.3. The literal in the code is `v0.3.5-rc` (`src/TYLauncher.c:22`, `src/installer/common.inc:26`).
- **Packaged ini label.** `src/package.ps1:9` calls the packaged ini the "Balanced" preset. `src/default_tyrender.ini:1` says it is the Medium preset. `docs/START_HERE.md:6` says Balanced is retired.
- **Package inputs.** `src/package.ps1:7` expects README and PERFORMANCE beside the build script. In the repo they are elsewhere (see Limits).
- **G1 result.** `docs/PLAN_CONSOLIDATED.md:8` says passed on rerun. `docs/results/g1_gate.md:3` says FAIL for the earlier DLL. These can both be true, because they name different builds, but the rerun is not recorded in the repo.

## See also

- [Build from scratch](build-from-scratch.md)
- [Branching and process](branching-and-process.md)
- [Testing and the gate](../guides/test-harness.md)
- [A/B captures](../guides/ab-captures.md)
- [Presets](../reference/presets.md)
- [Known issues](../decisions/known-issues.md)

# Contributor onboarding

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor starting cold).
> Verification note: written from the repo at 4db1d7d. No build or game run was done by the writer. The 60-minute plan is the writer's estimate (inferred).

## Purpose

This page gets a new contributor from zero to a first change. It gives a reading order, a build path, a smoke test, a one-line change taken all the way through, the code conventions the existing code follows, the pitfalls the owner has already paid for, and a table of the coding-agent task files so that nobody mistakes an old round for a live task. It ends with candidate first tasks.

## How it works

### The 60-minute path (estimate, inferred)

1. **Reading, about 15 minutes.** In this order:
   1. `README.md` (what the mod is for players; status line at `README.md:9`).
   2. `CHANGELOG.md`, the "Unreleased" section (`CHANGELOG.md:5`). Lines say "Untested" when the owner has not played them.
   3. `docs/START_HERE.md` sections 1, 3, 6 and 7 (project, where things live, rules and traps, engine facts).
   4. `docs/PLAN_CONSOLIDATED.md`, "Where things stand" and "Work order" (`docs/PLAN_CONSOLIDATED.md:5-59`).
   5. `docs/ISSUES_SEED.md`, the list of tickets and the workflow (`docs/ISSUES_SEED.md:4-24`).
2. **Code entry points, about 10 minutes.**
   - `src/tyrender.cpp`, `DllMain` at line 701. The startup order is `LoadConfig` (`:714`), then the per-module loaders (`:716-725`), then `Install` (`:727`).
   - The depth-copy hook at `src/tyrender.cpp:280-296`. This is where the screen-space passes run, in this order: AO, GI, emitter light, atmosphere, then a colour recopy when the scene changed.
   - The post-processing trigger at `src/tyrender.cpp:254`: a 64x64 viewport starts `RunFX` (`src/fx.inl:177`).
3. **Build, about 15 minutes if the toolchain is ready.** Follow `guides/build-from-scratch.md`. The first obstacle is the toolchain path constant (`src/build.ps1:5`).
4. **Smoke test, about 10 minutes if the game is installed.** The game is not in the repo and the test harness uses owner paths (NI item B3 in `guides/build-from-scratch.md`). Without the game, read the logs and code instead, and stop there.
5. **One-line change, about 10 minutes.** Pick a key, change its value in code and in the shipped ini, build, and check the result. The full steps are in `guides/add-a-setting.md`.
6. **Read the pitfalls, about 5 minutes.** See below.

### Code conventions seen in the code (observed)

- **Modules are `.inl` files pulled into one translation unit.** `src/tyrender.cpp:434` and `:540-552` include them. There is one DLL and no separate link step for modules.
- **Original GL calls are `o_` pointers.** A hook macro defines `o_<name>` for the original function and `h_<name>` for the replacement (`src/tyrender.cpp:250`). Passes call `o_glBindTexture`, `o_glDisable` and so on, which skip the hooks (`src/fx.inl:169-175` for texture setup; `src/fx.inl:205`, `:209` for state changes inside `RunFX`).
- **Extension calls go through a table.** Calls such as `gx.GenFramebuffers` and `gx.TexParameteri` go through the loaded extension table (`src/fx.inl:185`).
- **Shaders are raw string literals** in the `.inl` file, with the GLSL version on the first line (`src/fx.inl:8`, `:57`, `:99`).
- **GL enums are hex literals with a comment.** For example `0x0DE1` is `GL_TEXTURE_2D` in `src/fx.inl:169-175`.
- **Logs use `logf`** with a bracketed tag, such as `"[fx] initialised %dx%d"` (`src/fx.inl:195`). Tags are catalogued in `reference/log-and-error-messages.md`.
- **Config is read with `GetPrivateProfile*A` and a default in the call** (`src/fx.inl:146-157`).
- **Static state is file-scope.** Globals are `static` in the `.inl` they belong to (`src/fx.inl:144`).
- **Comments carry the history.** Many lines name an issue number (`#5`, `#4`) or a plan id, so the reason for a value is next to the value.

### Agent task folder (Q221, Q222)

The owner used coding agents for much of the work. Their briefs are in `docs/agent-tasks/`.

- Read `docs/agent-tasks/_common.md` first, then `docs/agent-tasks/BRIEFING.md`, then your task file (`_common.md:4`).
- Rules that apply to every agent: work only in your own copy and never edit the master or another agent's copy (`BRIEFING.md:12`; `_common.md:4`); write a step log after each meaningful step (`_common.md:5`; `BRIEFING.md:54-58`); never use `-Install` (`BRIEFING.md:22`); test only through the shared harness (`BRIEFING.md:24-26`); do not run the release gate (`_common.md:10`).
- The brief says the `todo.json` `task` field names the file to follow (`_common.md:3`).

The 21 task files and one brief in the folder are listed here. Each row gives the file's scope from its own title line. The text in the files is the owner's and is not quoted.

| File | Scope (from its title) | Round or status |
|---|---|---|
| `ao.md` | AO v2 (depth-mip path) against legacy AO; owns `ao.inl`. | First AO round. Records a near-field defect at a grazing view to fix first. |
| `ao3.md` | Round 2 AO task (owns `ao.inl`, `gi.inl`, `gi2.inl`, `gpuprof.inl`). | Round 2. Built from a recorded playtest. |
| `ao6.md` | Round 6 AO: distance fade, character not an occluder. | Round 6. |
| `atmos.md` | Atmosphere: froxel compute path and fallback; owns `atmos.inl`. | First atmosphere round. |
| `atmos2.md` | Task T53: atmosphere and fog parts of `fx.inl`. | Built from the owner's recording findings. |
| `atmos3.md` | Round 2 fog and sky. | Round 2. |
| `atmos6.md` | Round 6 atmosphere: fog colour independent of screen, GI and view direction. | Round 6. |
| `director.md` | Director brief: a read-mostly review role at checkpoints, not a coder. | Role brief, not a task. |
| `fx6.md` | Round 6 TAA: ghosting on moving objects and grass (`kTAA` in `fx.inl`). | Round 6. Not in the finished-rounds list (NI item P4). |
| `gi.md` | Global illumination: Hi-Z trace, temporal and a-trous (`gi2.inl`). | First GI round. |
| `gi4.md` | Round 4 GI task (owns `gi.inl`, `gi2.inl`; hooks allowed in `tyrender.cpp`). | Round 4. Not in the finished-rounds list (NI item P4). |
| `gi5.md` | Round 5 GI task; code word in the title; owns `gi.inl`, `gi2.inl`. | Round 5. Not in the finished-rounds list (NI item P4). |
| `light.md` | Shadows and natural light: filter, tint, hemisphere, foliage. | First light round. |
| `light2.md` | Tasks T54 and T55: shadows, natural light, AO strength keys. | Built from the owner's recording findings. |
| `light3.md` | Round 2 shadows and light. | Round 2. |
| `light4.md` | Round 4 shadows and light. | Round 4. |
| `perf.md` | Performance hunt on the merged tree: resolves, composite merging, formats, state save and restore, syncs. | Work in a copy. |
| `shadow5.md` | Round 5 shadows: a bright line inside shadows, checkerboard, grass ghosting. | Round 5. Not in the finished-rounds list (NI item P4). |
| `tess.md` | Tessellation, surface relief and PBR (`tess.inl`, `pbr.inl`). | First tessellation round. |
| `tess2.md` | Tasks T51 and T52: tessellation and PBR. | Built from the owner's recording findings. |
| `tess4.md` | Round 4 texture tags into tessellation and PBR; adds a name-to-flags hook in `tyrender.cpp`. | Round 4. |

Rounds named "round N" come from the owner's own sequence and are dated by the files' evidence lines. The round records are not the current plan. Use `docs/PLAN_CONSOLIDATED.md` for current work.

### Candidate first tasks (Q230, inferred)

The owner decides what to take. These are picked from `docs/ISSUES_SEED.md` and `docs/PLAN_CONSOLIDATED.md`, and from the gaps this chapter found.

1. **Fix the dead link in the README.** `README.md:163` points to `docs/FINDINGS_ROUND4.md`. The file is at `docs/recording/FINDINGS_ROUND4.md`. Docs only. (The documentation PR also does this; check before starting.)
2. **Remove or document two unused launcher files.** `src/ui_part_a.inc` and `src/ui_part_c.inc` are not included by any file in the repo (checked by searching for the names). They hold older preset values. Needs owner agreement (NI item N-18, resolved as "unused").
3. **Make the package script work from the repo layout.** `src/package.ps1:7` expects README and PERFORMANCE beside the build script (see `guides/deploy-package-release.md`). Needs owner agreement on the target layout.
4. **Settings clash guard, issue #13.** The guard exists (`src/tyrender.cpp:637`, `src/launcher_ui.inc:105`). `docs/PLAN_CONSOLIDATED.md:69` lists it as a tooling track that "can start any time". Read the ticket first. The open question is the single source of truth, which is an owner decision.
5. **Not a first task:** issue #16 (other GPUs) needs hardware the project does not have (`docs/PLAN_CONSOLIDATED.md:72`). Issue #11 (shadow lines) needs game playtests.

## Code pointers

| What | File:line | Note |
|---|---|---|
| Project status line | `README.md:9` | Latest release wording. |
| Unreleased changes | `CHANGELOG.md:5` | Section heading. |
| Startup order | `src/tyrender.cpp:701-727` | `DllMain` and loaders. |
| Depth-copy hook and pass order | `src/tyrender.cpp:280-296` | AO, GI, emitter, atmosphere, recopy. |
| Post trigger | `src/tyrender.cpp:254` | 64x64 viewport starts `RunFX`. |
| Hook macro | `src/tyrender.cpp:250` | Defines `o_` and `h_` names. |
| Module includes | `src/tyrender.cpp:434`, `:540-552` | Order of `.inl` files. |
| Post-processing pass | `src/fx.inl:177` | `RunFX`. |
| Example config loader | `src/fx.inl:146-157` | `LoadFXConfig`. |
| Example raw shader | `src/fx.inl:57-95` | `kGrade`. |
| Example log line | `src/fx.inl:195` | `[fx] initialised`. |
| Agent brief order | `docs/agent-tasks/_common.md:4` | Read order. |
| Agent rules | `docs/agent-tasks/BRIEFING.md:12-26` | Folder, build, harness. |
| Agent step log rule | `docs/agent-tasks/_common.md:5` | Log after each step. |
| Gate warning for agents | `docs/agent-tasks/_common.md:10` | Do not run the gate. |
| Pitfalls list | `docs/START_HERE.md:67-79` | Owner's rules and traps. |
| Transcripts folder | `docs/recording/` | Contains voice transcripts; see pitfalls. |

## Settings

Not applicable to onboarding. The config file and its keys are in `reference/settings-*.md`.

## Cost

Not measured. The 60-minute path is an estimate.

## Limits

- **The game is needed for most tests.** It is not in the repo. Tests need an installed copy and the owner's harness (`docs/START_HERE.md:44`).
- **Harness and build scripts use owner paths.** A new machine needs edits first (see `guides/build-from-scratch.md`, NI item B3).
- **Some docs are dated snapshots.** `docs/START_HERE.md` and `docs/RESUME.md` describe 2026-10-13 state (`docs/START_HERE.md:3`). `README.md` and `CHANGELOG.md` describe the state at the time of writing.
- **Several files reference files that are not in the repo.** Examples: `01_findings.md`, `decomp\`, the Ghidra project (`docs/START_HERE.md:29`, `:83`). These are reverse-engineering notes (NI item N-17).

## Known issues

- **Issue #14 status.** `docs/ISSUES_SEED.md:17` calls the A/B screenshot hotkey "wish, last". `CHANGELOG.md:58` lists the F11 A/B hotkey as added under v0.3.5 for #14. The two disagree (see Discrepancies).
- **Old task files still in the folder** can look like live work. Check `docs/todo.json` before starting one.

## How to verify

Not run by the documenter. Reader's check that the onboarding works:

1. Follow the 60-minute path. Note which step fails and why.
2. Confirm each `file:line` in this page with `sed -n '<n>p' <file>`. The line numbers were checked at 4db1d7d.
3. Confirm the task table: `ls docs/agent-tasks/` should show 21 task files, `BRIEFING.md` and `_common.md`.

## NEEDS INVESTIGATION

- **Which task files are live (P4).** See `guides/branching-and-process.md`.
- **N-17. Reverse-engineering artefacts.** The notes and the Ghidra project are not in the repo. A contributor cannot check the engine facts from the repo alone. Method: ask the owner whether sanitized copies can be published.
- **Issue status for #14 and #12.** Method: `gh issue view 14` and `gh issue view 12`, plus the CHANGELOG entries.

## Discrepancies

- **Issue #14.** `docs/ISSUES_SEED.md:17` lists it as a wish. `CHANGELOG.md:58` lists the hotkey as implemented. Probably the seed text is old. Not verified.
- **Issue #12.** `docs/PLAN_CONSOLIDATED.md:69` says #12 and #13 "can start any time". `CHANGELOG.md` lists the #12 overlay as implemented and untested. The issue may need a test, not new code.
- **Unused launcher files.** The first pass says `ui_part_a.inc` and `ui_part_c.inc` are not referenced. Confirmed for this commit. They differ from `launcher_ui.inc` (for example `src/ui_part_a.inc:7` gives Medium a haze value of 2.2, where `src/launcher_ui.inc:7` gives 1.2). Repo wins: the live values are in `launcher_ui.inc`.

## Pitfalls

These come from the owner's notes and from what the files show. Each one has already cost someone time.

- **Master and agent copies.** Never edit the master source folder while agents work. Never edit another agent's copy (`docs/START_HERE.md:17`, `:68`; `docs/agent-tasks/BRIEFING.md:12`).
- **Install.** Never run `build.ps1 -Install` from a working copy. Close the launcher first, because it locks the copy in the game folder (`docs/START_HERE.md:55`, `:72`; `docs/agent-tasks/BRIEFING.md:22`).
- **Restart-only keys.** `-IniFile` is not read at game start. Restart-only keys (examples: `[shadows] MapScale` and `Taps`, `[tess] Enable`, `Scope`, `MaxLevel`, `[pbr] Enable`) need the ini copied into the game folder first (`docs/START_HERE.md:70`).
- **Line endings.** Whole-file CRLF conflicts fail the merge tool (`docs/START_HERE.md:71`). The clone at 4db1d7d has no CR characters in the text files checked (`grep -l`). Keep the line endings you found.
- **Byte-order marks.** Several files start with a UTF-8 byte-order mark. Source and tool examples: `src/fx.inl`, `src/light.inl`, `src/tex.inl`, `src/package.ps1`, `tools/release.ps1`, `tools/merge_agent.ps1`, `tools/presets.ps1`, `tools/rkvtool.ps1`, `tools/shot.ps1`. Keep the marks when you edit. Found by checking the first three bytes of each tracked file at 4db1d7d.
- **Backslashes in Bash.** Bash command lines and heredocs collapse backslashes. Pass Windows paths through PowerShell or the editing tools (`docs/START_HERE.md:74`).
- **PowerShell 5.1.** Variable names are case-insensitive (`$ini` and `$Ini` are the same). `powershell -File` cannot take array parameters, so use comma-separated strings (`docs/START_HERE.md:74`; `docs/agent-tasks/BRIEFING.md:31`).
- **Encoding in PowerShell.** `Set-Content` and `Get-Content` without `-Encoding` corrupt UTF-8 (`docs/START_HERE.md:74`).
- **Measure with GPU timers only.** Use the DLL's `[gpu]` lines and total frame time. Per-pass timers under-count (`docs/START_HERE.md:76`).
- **Fallbacks.** A failing effect switches itself off. There is no slow legacy path to fall back to in the shipped build (`docs/START_HERE.md:79`).
- **Vendor calls need guards.** Check the extension or version before a call that a vendor may lack (`docs/START_HERE.md:80`).
- **Characters.** Characters are not relieved, tessellated, or used as GI or AO occluders. Keep it that way (`docs/agent-tasks/BRIEFING.md:3`).
- **Transcripts.** `docs/recording/` holds voice transcripts of the owner and testers. Read them for facts. Do not quote them.
- **No VM testing.** The owner decided to test on the laptop or the owner's PC only (`docs/START_HERE.md:77`). Do not plan VM-based tests without asking.

## See also

- [Build from scratch](build-from-scratch.md)
- [Branching and process](branching-and-process.md)
- [Deploy, package and release](deploy-package-release.md)
- [Add a setting](add-a-setting.md)
- [Add a shader pass](add-a-shader-pass.md)
- [Add a preset step](add-a-preset-step.md)
- [Source map](../reference/source-map.md)
- [Glossary](../glossary.md)

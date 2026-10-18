# Branching and process

> Status: documented at commit 4db1d7d; last verified 2026-10-18. Audience: A (contributor), C (maintainer).
> Verification note: written by reading the process documents, the GitHub templates and the merge and sync scripts. The GitHub repository settings (branch rules, Actions, who may push) are not in the clone and were not checked.

## Purpose

This page describes how a change moves from an idea to a merged PR: how tickets are named, how branches and PRs are made, how several coding agents share the work without overwriting each other, how the staging tree and the merge tool work, how playtests are gated by a phase file and a lock, and what the GitHub templates ask for. It also says plainly that the repo has no CI.

## How it works

1. **Tickets.** Work is tracked as GitHub issues. The seed list is `docs/ISSUES_SEED.md`: issues #1 to #17, labels `bug`, `enhancement`, `research`, `perf`, `tooling`, milestone `v0.3.5` (`docs/ISSUES_SEED.md:1`). Each ticket is one problem, and each fix is one PR (`docs/ISSUES_SEED.md:1`; `docs/PLAN_CONSOLIDATED.md:24`).
2. **Branches.** Name a branch `fix/<id>-<slug>` and cut it from `main` (`docs/ISSUES_SEED.md:23`).
3. **Evidence.** The PR body says `Closes #<id>` and fills in the PR template (`.github/pull_request_template.md:2`). Performance evidence must come from a measured run, not a guess (`docs/PLAN_CONSOLIDATED.md:24`).
4. **Agents work in private copies.** Each coding agent edits its own copy of the source. The master source folder is not edited while agents are running (`docs/START_HERE.md:17`, `:68`). In the repo's terms, `src/` is the master and agent copies are outside the repo.
5. **Staging.** Merged work goes into a staging copy first. `tools/merge_agent.ps1` does it for one agent at a time (`tools/merge_agent.ps1:1-5`). The steps are:
   - Copy the files that agent owns (`tools/merge_agent.ps1:8`, `:12`). The ownership map is: atmos owns `atmos.inl`; gi owns `gi.inl` and `gi2.inl`; ao owns `ao.inl` and `gpuprof.inl`; tess owns `tess.inl` and `pbr.inl`; light owns `light.inl` and `fx.inl`.
   - For any other changed `.cpp` or `.inl` file, run a three-way merge with `git merge-file` (`tools/merge_agent.ps1:14`, `:19`). Infrastructure files `debugcam.inl` and `stateguard.inl` are skipped, so the staging copy stays (staging starts as a copy of master) (`tools/merge_agent.ps1:17`). A conflict stops the merge and writes a `.CONFLICT` file (`tools/merge_agent.ps1:21`, `:24`).
   - Build the staging copy (`tools/merge_agent.ps1:26-29`).
   - Append a line to the merge log (`tools/merge_agent.ps1:31`).
   - Run the regression suite with the feature switched on and off (`tools/merge_agent.ps1:34-36`). The merge reports FAIL if the suite shows errors or an empty image set (`tools/merge_agent.ps1:38`).
6. **Promotion.** Staging becomes master with `tools/release.ps1` (see the deploy page). It does not promote `launcher_ui.inc` (`tools/release.ps1:9`).
7. **Phases.** Playtesting is gated by a phase file, `phase.txt`, that holds one word: `EDIT`, `FREEZE` or `TEST` (`docs/AUTOPLAYTEST.md:99`). The automated playtest and the boot script refuse to run unless the word is `TEST`, and a missing file counts as not `TEST` (`docs/AUTOPLAYTEST.md:99`).
8. **Lock.** A run takes a lock file that holds the owner, the process id, a token and the time (`docs/AUTOPLAYTEST.md:100`). A stale lock (dead process id) is replaced. A live lock blocks the run.
9. **Game test lock.** Separately, the single-game harness takes a named mutex and waits (`tools/agent_test.ps1:22-23`). Only one game runs at a time.
10. **Concurrency limit.** At most three agents at once, two is safer (`docs/START_HERE.md:68`). Agents write a step log after each meaningful step so that a cut-off session can resume (`docs/agent-tasks/_common.md:5`).
11. **Roles.** The owner's notes define `auto` (scripts only), `agent` (a coding agent), `lead` (the planning session), `director` (strongest model at checkpoints) and `user` (human decisions) (`docs/START_HERE.md:42`).
12. **Mirroring to the git repo.** `tools/repo_sync.ps1` copies a whitelist of source, tools and docs into the repo working tree (`tools/repo_sync.ps1:7-19`). It stops if a token-like string is found (`tools/repo_sync.ps1:20-22`). It then shows `git status` (`tools/repo_sync.ps1:23`). It does not commit or push. Commit, push and PR are manual (`docs/ISSUES_SEED.md:23` describes the lead doing them).

```
issue #n -> branch fix/n-slug (from main)
              |
              +-- agent copy (private folder) --build + harness tests--> agent report
                                                    |
              merge_agent.ps1 (per agent) --> staging --> suite --> release.ps1 --> master
              |
              repo_sync.ps1 --> repo working tree --> commit, PR "Closes #n" (manual)
```

## Code pointers

| What | File:line | Note |
|---|---|---|
| Seed labels and milestone | `docs/ISSUES_SEED.md:1` | Labels and `v0.3.5` milestone. |
| Branch naming rule | `docs/ISSUES_SEED.md:23` | `fix/<id>-<slug>` from `main`. |
| Release as tag | `docs/ISSUES_SEED.md:24` | Tag on `main`, zip attached. |
| One PR per fix | `docs/PLAN_CONSOLIDATED.md:24` | Also: max 3 agents, no game test while the user plays. |
| PR template: closes line | `.github/pull_request_template.md:1-2` | `Closes #`. |
| PR template: evidence | `.github/pull_request_template.md:7-10` | GPU timers, crops, G1 "not required per PR". |
| PR template: risks | `.github/pull_request_template.md:12-13` | "failure path = effect OFF?" |
| Bug issue template | `.github/ISSUE_TEMPLATE/bug_report.md:1-11` | Label `bug`; fields for build, preset, evidence, suspected area. |
| Feature issue template | `.github/ISSUE_TEMPLATE/feature_task.md:1-8` | Label `enhancement`; goal, measured acceptance, files. |
| Agent ownership map | `tools/merge_agent.ps1:8` | Per agent: owned files. |
| Merge tool parameters | `tools/merge_agent.ps1:6` | Agent names: atmos, gi, ao, tess, light. |
| Three-way merge | `tools/merge_agent.ps1:19` | `git merge-file`. |
| Infrastructure skip list | `tools/merge_agent.ps1:17` | `debugcam.inl`, `stateguard.inl`. |
| Merge result text | `tools/merge_agent.ps1:21`, `:24`, `:29`, `:38` | CONFLICT and FAIL markers. |
| Merge log | `tools/merge_agent.ps1:31` | Appends to the results log. |
| Staging folder creation | `tools/merge_agent.ps1:10` | Copies master files if staging is empty. |
| Repo mirror whitelist | `tools/repo_sync.ps1:7`, `:11`, `:14-19` | Source, tools, docs. |
| Secret guard | `tools/repo_sync.ps1:20-22` | Token-like patterns. |
| Phase file rule | `docs/AUTOPLAYTEST.md:99` | EDIT, FREEZE or TEST. |
| Lock file rule | `docs/AUTOPLAYTEST.md:100` | Owner, pid, token, time; stale-lock rule. |
| Game test mutex | `tools/agent_test.ps1:22-23` | One game at a time. |
| Ignore rules for binaries | `.gitignore:1-24` | Games, binaries, zips, logs, keys never committed. |

## Settings

No ini keys. The process uses two files outside the game: the phase file and the lock file (`docs/AUTOPLAYTEST.md:99-100`). Their paths are fixed to the owner's workstation layout (NI item P5).

## Cost

Not measured. The process itself has no timing targets in the repo.

## Limits

- **No CI.** `.github/` holds only two issue templates and one PR template (`.github/ISSUE_TEMPLATE/`, `.github/pull_request_template.md`). No workflow file is in the clone. Whether GitHub Actions is turned on in the repository settings is not known (NI item P1).
- **Merge tool limits.** `merge_agent.ps1` cannot copy new files. The owner notes say to copy them by hand (`docs/START_HERE.md:71`). Whole-file line-ending conflicts fail (`docs/START_HERE.md:71`). `tyrender.cpp` is touched by several agents, so most merges go through the three-way step (`tools/merge_agent.ps1:3-4`).
- **Owner paths.** The merge tool's master, staging, agent and git paths are fixed (`tools/merge_agent.ps1:7`). See NI item B3 in `guides/build-from-scratch.md`.
- **Windows only.** The phase file, lock file and mutex are Windows conventions (`docs/AUTOPLAYTEST.md:99-100`; `tools/agent_test.ps1:22`).
- **Single owner.** The process assumes one owner who makes the playtest calls and the release decision (`docs/PLAN_CONSOLIDATED.md:19`).

## Known issues

- Agents can be cut off by usage limits in the middle of a task (`docs/START_HERE.md:69`). The step log and the report file are the only way to resume. A missing step log means redoing work.
- The merge tool's ownership map is older than some task files. For example `docs/agent-tasks/ao3.md` gives the AO agent `gi.inl` and `gi2.inl` as well, while `tools/merge_agent.ps1:8` gives those to the gi agent. The task file is a dated round record, so the script is the current rule. Check the script before merging.
- The phase file (`EDIT`, `FREEZE`, `TEST`) is set by hand by the fix-plan thread (`docs/AUTOPLAYTEST.md:99`, `:103`). Nothing resets it automatically, so a forgotten reset leaves the phase at `TEST`.

## How to verify

Not run by the documenter. Reader's checks:

1. Read `tools/merge_agent.ps1:8` and compare with the file list in the task brief for that agent (`docs/agent-tasks/BRIEFING.md`, "Source layout").
2. Run `tools/repo_sync.ps1` in a scratch setup and read the `git status` output. It should list only whitelisted files (`tools/repo_sync.ps1:7-19`).
3. Open `.github/pull_request_template.md` and check that every heading is present in a new PR body.
4. On GitHub, open the repository's Actions tab. Expect no workflow runs if the repo has no workflow file, which matches the clone (NI item P1).

## NEEDS INVESTIGATION

- **P1 (N-15). CI and Actions.** Method: `gh api repos/<owner>/<repo>/actions/workflows` on the GitHub repository, or read the Actions tab. The clone has no workflow files.
- **P2 (N-19). Issue and PR titles and states for numbers #36 to #122.** The clone has no issue tracker data. Method: `gh issue list --state all --limit 200` and `gh pr list --state all`. The history chapter uses the result.
- **P3. Branch protection and push rights.** Whether `main` is protected, and who can push or merge. Method: repository settings on GitHub.
- **P4. Which task files are still active.** `docs/agent-tasks/_common.md:3` lists the older files as finished rounds. Four files are not in that list: `fx6.md`, `shadow5.md`, `gi4.md`, `gi5.md`. Method: read the `task` fields in `docs/todo.json` (83 `task` fields, 61 of them empty) and see which files the others name.
- **P5. Owner paths in the process files.** The phase, lock and staging paths are owner-specific. Decision needed on a shared config (see `guides/build-from-scratch.md`, NI item B3).

## Discrepancies

- **Terminology.** The owner's folder called `master` (`tools/release.ps1:5`) is the source of truth, and the git branch called `main` is where tags go (`docs/ISSUES_SEED.md:24`). Both are called master or main in different documents. This page uses `master` for the owner's source folder and `main` for the git branch.
- **Finished rounds.** `docs/agent-tasks/_common.md:3` lists `ao`, `ao3`, `atmos*`, `gi`, `light*`, `perf`, `tess*` and `director` as finished rounds. It does not list `fx6`, `shadow5`, `gi4` or `gi5`. Their status is not stated (NI item P4).
- **Merge and release versus promotion.** `docs/START_HERE.md:55` describes the release script as promoting ".inc files, not launcher_ui.inc". The script matches (`tools/release.ps1:9`).

## See also

- [Contributor onboarding](contributor-onboarding.md)
- [Deploy, package and release](deploy-package-release.md)
- [Build from scratch](build-from-scratch.md)
- [Test harness](../guides/test-harness.md)
- [Tools reference](../reference/tools-reference.md)
- [Roadmap and open work](../decisions/roadmap-and-open-work.md)

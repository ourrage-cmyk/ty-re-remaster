# History and timeline
> Status: documented at commit 4db1d7d; last verified 2026-10-18 (static read of the clone). Audience: A, C.

## Purpose
This page answers "what happened, in what order, and which build was which". It covers the release and build ladder, the dated timeline from the project's own records, the playtest rounds, an index of every issue or PR number the repository mentions, and a list of every document in the repository with the page that accounts for it. Dates and version names are copied from the sources. Where two sources disagree, both are shown in the Discrepancies list.

The clone is a single squashed commit (`git rev-list --count HEAD` = 1), so no PR titles or dates can be read from git. Anything about PRs or issues comes from text in the repository.

## How it works
1. **Ordering rule.** Use the newest dated source. The newest records are `CHANGELOG.md` and `docs/PLAN_NEXT.md` (both dated 2026-10-18). `docs/RESUME.md`, `docs/STATUS_2026-10-13.md`, `docs/START_HERE.md` and `docs/GOAL.md` are snapshots from 2026-10-13 and say "v0.3.2 is current" in places; they are historical for release status. Code wins over any document about what a build does (see [known-issues.md](known-issues.md)).
2. **Builds are named two ways.** A commit id (for example `4a2ef95`) appears in the launcher title bar (`src/TYLauncher.c:22-23`, `TY_WINDOW_TITLE` with `TY_BUILD_ID`). A DLL SHA-1 prefix (for example `6571393575`) appears in the G1 gate records. These are different identifiers and are not interchangeable.
3. **Release versions** use the form `vX.Y.Z`. The launcher and the session report still carry the string `v0.3.5-rc` (`src/TYLauncher.c:22`, `src/sesslog.inl:16`), which the CHANGELOG lists as a known defect.
4. **Playtests** are the user's own recordings, reviewed by an agent or by the owner. Each round has a findings file. Problem ids (F, G, W, S, A, E, R3-, R4-) are defined in `docs/recording/INDEX.md`.

### Release and build ladder

| Version or build | Status in the record | DLL SHA-1 prefix | G1 gate | Source |
|---|---|---|---|---|
| v0.1 | Default string in the packaging script only (`src/package.ps1:2`). Not a release. | - | - | `src/package.ps1:2` |
| v0.3 | Preview. Checkpoint 3 says ship as preview after G1. | `D5FB7DA039` | Passed: soak OK, default 11.12 ms | `docs/results/VERDICTS.md:46-47`, `docs/results/DIRECTION_2026-10-13_checkpoint3.md:15-17` |
| v0.3.1 | Packaged (T57, T58 done). No CHANGELOG entry. | `839C01C3AD` | Passed: default 11.15 ms | `docs/todo.json` (T57, T58), `docs/results/VERDICTS.md:49-50` |
| v0.3.2 | Shipped as "preview / community beta". Called "current release" in 2026-10-13 docs. | `6571393575` | Passed: default 12.08 ms (Medium) | `docs/GOAL.md:26`, `docs/RESUME.md:7`, `docs/STATUS_2026-10-13.md:23` |
| v0.3.3 | Test build, not released. Its zip is not in the repository. | `0753BB1B82` | Not run. Smoke only: Medium 12.05 ms, High 19.80 ms | `docs/STATUS_2026-10-13.md:24,75-78`, `docs/RESUME.md:8` |
| v0.3.4 | Built and installed in the owner's test folder. Not in the CHANGELOG. Latest build in PLAN_CONSOLIDATED. | `3ee5da28a7` (in RESUME and PLAN_CONSOLIDATED); the G1 file name uses `3EE5DA28A7` | Perf passed (default 10.73 ms). First soak failed (clean exit at 9 min, level exit); rerun passed | `docs/RESUME.md:66-68,76-78`, `docs/PLAN_CONSOLIDATED.md:7-8`, `docs/results/g1_gate_prev_3EE5DA28A7.md:1-2` |
| v0.3.5 (c255ae8) | "Last full release" per CHANGELOG and PLAN_NEXT. Title string still `v0.3.5-rc`. | not stated | not stated | `CHANGELOG.md:46`, `docs/PLAN_NEXT.md:7`, `src/TYLauncher.c:22` |
| Unreleased, main after v0.3.5, up to `4a2ef95` | Most entries marked "Untested". | - | - | `CHANGELOG.md:5-44` |
| v0.3.6 | Planned, "only after the next playtest". Not built. | - | - | `docs/PLAN_NEXT.md:39` |
| v0.4 | Portal spill and some features deferred to v0.4. | - | - | `docs/FIX_PLAN_20261016.md:37,56`, `docs/PLAN_EXEC.md:73` |
| HEAD `4db1d7d` | Merge of PR #122 ("label the GPU model source in the session report"). Commit date 2026-10-18 12:17 +0100. | - | - | `git log -1` in the clone |

Builds that appear only in playtest records: `cda9364` (playtest 2026-10-17), `3ee6ea7` (playtest 2026-10-18), `4a2ef95` (owner playtest, Ultra, 2026-10-18). Their order is taken from `docs/PLAN_NEXT.md:9-24`. The CHANGELOG does not contain entries for v0.3.1 to v0.3.4.

### Dated timeline
Dates are as written in the source (no time zone unless stated).

| Date | Event | Source |
|---|---|---|
| 2026-10-11 | User decision: no VM testing; laptop or this PC only | `docs/START_HERE.md:77` |
| 2026-10-12 | User decision: lead owns the plan; Haiku collects; director reviews at checkpoints | `docs/GOAL.md:25` |
| 2026-10-12 | Early agent suites (T02-T06, T33-T37): atmosphere, GI, AO, tessellation and light measured | `docs/todo.json` ids T02-T06, T33-T37 |
| 2026-10-13 | Director checkpoints 1, 2, 3 (scope cuts, GI off by default, Balanced = C1) | `docs/results/DIRECTION_2026-10-13_checkpoint1.md:20-26`, `..._checkpoint2.md:17-23`, `..._checkpoint3.md:15-17` |
| 2026-10-13 | Staging merges of atmos, ao, light, tess, gi | `docs/results/merge_log.md:1-13` |
| 2026-10-13 | Playtest 1 (v0.3, Balanced): findings F1-F12 | `docs/recording/FINDINGS_AND_PLAN.md:4-18` |
| 2026-10-13 | Playtest 2 (v0.3.1, Ultra): G1-G10 | `docs/recording/FINDINGS_ROUND2.md:4-19` |
| 2026-10-13 | Playtest 3 (v0.3.2): "no flickering"; W1, S1, A1, E1 and requests R3-F1, R3-F2 | `docs/recording/FINDINGS_ROUND3.md:5-17` |
| 2026-10-13 | v0.3.2 shipped as preview / community beta; G1 passed | `docs/GOAL.md:26`, `docs/RESUME.md:7`, `docs/results/VERDICTS.md:52-53` |
| 2026-10-13 | Playtest 4 (v0.3.2 High and Ultra): "Medium is stable, High and Ultra are flickering" | `docs/recording/FINDINGS_ROUND4.md:3` |
| 2026-10-13 | Colour match adopted (T85): Medium, High and Ultra brightness and saturation tuned to vanilla | `docs/RESUME.md:64`, `docs/todo.json` id T85 |
| 2026-10-13 20:26 | v0.3.4 built and installed (shadow-tap `textureLod` fix, colour match, character draws excluded from GI sources) | `docs/RESUME.md:68` |
| 2026-10-13 20:45 to 20:57 | v0.3.4 G1: perf OK; first soak exit judged a level exit; rerun OK | `docs/RESUME.md:76-78` |
| 2026-10-13 evening | Playtest 5 (v0.3.3 test build): checkerboard, grass ghosting, shadow line, red tint under pants | `docs/recording/FINDINGS_ROUND4.md:14-16` |
| 2026-10-16 | R6-1: every flicker-free harness run was Medium (GI off, contact shadows off) | `docs/results/R6_config_diff.md:1-6` |
| 2026-10-16 | Plans written: PLAN_3DAY, FIX_PLAN, PLAN_CONSOLIDATED, PLAN_EXEC (awaiting approval) | `docs/PLAN_3DAY_20261016.md:1-3`, `docs/FIX_PLAN_20261016.md:1`, `docs/PLAN_CONSOLIDATED.md:1`, `docs/PLAN_EXEC.md:1` |
| 2026-10-16 | Issue #11 (shadow checkerboard and dotted line) closed on Medium playtest of v0.3.4 | `docs/PLAN_CONSOLIDATED.md:13` |
| 2026-10-17 | Ultra and High playtest: AO flicker and TAA ghosting; confirms #6 | `docs/PLAN_CONSOLIDATED.md:14-15` |
| 2026-10-17 | Evidence pass on build `cda9364` (game not run) | `docs/PLAYTEST_2026-10-17_FINDINGS.md:1-3` |
| 2026-10-17 | Material classification plan (plan only); portal emitter test plan (prepared, not run) | `docs/MATERIAL_CLASSIFICATION_PLAN.md:3`, `docs/EMIT_PORTAL_TEST_PLAN.md:3` |
| 2026-10-18 | PLAN_NEXT rewritten after playtest of `4a2ef95`; CHANGELOG entry for that playtest | `docs/PLAN_NEXT.md:3`, `CHANGELOG.md:34` |
| 2026-10-18 | Playtest of `3ee6ea7` (Ultra) | `docs/PLAN_NEXT.md:22` |
| 2026-10-18 | HEAD `4db1d7d` (PR #122 merged) | `git log` |

The release date of v0.3.5 is not written anywhere in the repository. Because PLAN_CONSOLIDATED (2026-10-16) still treats v0.3.5 as a future milestone, the release is later than 2026-10-16 (inferred). See Discrepancies.

### Playtest rounds

| Round | Build | Date | Findings file | Headline |
|---|---|---|---|---|
| 1 | v0.3 Balanced | 2026-10-13 | `docs/recording/FINDINGS_AND_PLAN.md` | Sky not hazed, noise-only ground relief, characters tessellated, colour casts, portal light not on ground |
| 2 | v0.3.1 Ultra | 2026-10-13 | `docs/recording/FINDINGS_ROUND2.md` | Flicker everywhere (AO, GI, shadow shifts); red trousers glow; fog swamp look at Billabong |
| 3 | v0.3.2 | 2026-10-13 | `docs/recording/FINDINGS_ROUND3.md` | No flicker on Medium and High; water pulse, shadow lines, AO halo, footprints glow |
| 4 | v0.3.2 High and Ultra | 2026-10-13 | `docs/recording/FINDINGS_ROUND4.md` | High and Ultra flicker; water blue flicker on Ty; bag not tessellated; Ultra slow |
| 5 | v0.3.3 test build | 2026-10-13 (evening) | `docs/recording/FINDINGS_ROUND4.md` lines 14-16 | Checkerboard and shadow line persist; grass ghosting; red tint on ground |
| cda9364 | build `cda9364` | 2026-10-17 | `docs/PLAYTEST_2026-10-17_FINDINGS.md` | F10 overlay breaks geometry; fog and water issues; frame stall at 2:10 to 2:15 (not confirmed as game or capture) |
| Medium only | v0.3.4 | 2026-10-16 | `docs/PLAN_CONSOLIDATED.md:13` | Shadow lines fixed on Medium (#11 closed) |
| Ultra and High | build not stated | 2026-10-17 | `docs/PLAN_CONSOLIDATED.md:14-15` | AO flicker and TAA ghosting on both |
| Ultra | `4a2ef95` | 2026-10-18 | `CHANGELOG.md:34-36`, `docs/PLAN_NEXT.md:9-11` | Sky and red rock fade well; F10 did nothing (key default); water ramp; lily and rock brightness; bridge shadow line |
| Ultra | `3ee6ea7` | 2026-10-18 | `docs/PLAN_NEXT.md:22-24` | Launcher good; fire not fogged; lilies still off; bridge shadow line and water outline |

## Code pointers

| What | file:line | Note |
|---|---|---|
| Launcher version string | `src/TYLauncher.c:22` | `v0.3.5-rc` |
| Launcher title bar (build id) | `src/TYLauncher.c:23` | `TY_WINDOW_TITLE` |
| Session report version fallback | `src/sesslog.inl:15-17` | Same `v0.3.5-rc` string |
| Packaging default version | `src/package.ps1:2` | `v0.1` |
| Build script header (usage) | `src/build.ps1:1-2` | Output folder and `-Install` switch |

## Settings
None. This page has no runtime setting. Keys mentioned in the timeline are described on the settings pages ([../reference/settings-post-shadows-light-pbr-tess.md](../reference/settings-post-shadows-light-pbr-tess.md), [../reference/settings-atmos-fog-emit.md](../reference/settings-atmos-fog-emit.md)).

## Cost
Not applicable. Measured numbers by build are in [../reference/presets.md](../reference/presets.md) and in `docs/STATUS_2026-10-13.md`. The numbers in STATUS are from the v0.3 to v0.3.3 builds only.

## Limits
- The clone has one commit. Authorship, PR dates and PR titles are not available. Issue and PR titles are NEEDS INVESTIGATION (N-19).
- The CHANGELOG starts at the "Unreleased" entry and v0.3.5. It has no entries for v0.3.1 to v0.3.4, so those builds are documented only in the status files and results folder.
- The repository does not contain the zip packages (the project rule says zips are never committed; `docs/PLAN_CONSOLIDATED.md:63`). Package contents for v0.3.2 and v0.3.3 cannot be checked here.

## Known issues
- The repository states no precedence rule between documents. The 2026-10-13 files describe themselves as the state at that date (`docs/RESUME.md:1` "current state only (rewritten 2026-10-13 19:23)"), so the rule used on this page is inferred from their dates.
- `docs/RESUME.md:57-58` lists known inconsistencies in the test-build zip ini and the shipped header. They were left unfixed because zips are not edited (`docs/RESUME.md:58`).

## Issue and PR index
Seed issues #1 to #17 (`docs/ISSUES_SEED.md:4-20`) have a status row in [known-issues.md](known-issues.md#seed-issues-1-to-17). This table covers every other number the repository mentions. Command used at plan time (repeated in this clone): `grep -rhoE '#[0-9]+'` over README, CHANGELOG, docs, src, tools and .github. Numbers that appear only in tool request files are noted as such.

| # | What it covers (from the repository) | Where mentioned | Titles in repo |
|---|---|---|---|
| #36, #38, #40, #42, #43, #45, #46, #47, #49, #50, #51, #52 | Merged after v0.3.5 (no per-item text) | `docs/PLAN_NEXT.md:8` | No (N-19) |
| #41 | Surface relief slider working (`[tess] ReliefDepth`) | `CHANGELOG.md:56` | Yes (CHANGELOG) |
| #44, #48 | Material classes: one class lookup replaces texture-flag tests | `CHANGELOG.md:19` | Yes |
| #54, #59, #63 | Overlay: focus-free toggle, explicit draw state | `CHANGELOG.md:22` | Yes |
| #58 | Sky and painted rock fogged by `[atmos] SkyFogDist` | `CHANGELOG.md:43`, `docs/PLAN_NEXT.md:31` | Yes |
| #60 | Tessellation distance LOD (`[lod] TessEnable`) | `CHANGELOG.md:14` | Yes |
| #61 | Wider emitter blur `[emit]` | `CHANGELOG.md:21` | Yes |
| #62 | Water debug bitmask to bisect blue flicker | `CHANGELOG.md:21` | Yes |
| #64, #65, #66 | Automated playtest harness; game lock file | `CHANGELOG.md:23` | Yes |
| #72 | GL state dumps logged as `[overlay]` lines in the playtest report | `docs/AUTOPLAYTEST.md:108` | No (N-19) |
| #73 | Read the `[overlay]` lines in the report (overlay check) | `tools/playtest/requests/req-79-overlay.json:29,39` | No (N-19) |
| #74 | Water draws fogged by WaterBoost | `CHANGELOG.md:44`, `docs/PLAN_NEXT.md:8` | Yes |
| #75 | GI checker `[gi] Checker` | `CHANGELOG.md:18` | Yes |
| #78 | Shadow LOD (`[lod] Shadow`) | `CHANGELOG.md:15` | Yes |
| #79 | F10 overlay verification in a level | `README.md:225`, `docs/PLAN_NEXT.md:29` | Yes (text only) |
| #80 | Water flicker bisect (DebugWater) | `tools/playtest/plans/next_monitor.json:3`, `docs/AUTOPLAYTEST.md:138` | Partly |
| #81 | Sky and tree fog check | `tools/playtest/plans/next_monitor.json:3` | Partly |
| #82 | Water fog multiplier (WaterBoost values) | `tools/playtest/plans/next_monitor.json:3`, `docs/PLAN_NEXT.md:11` | Partly |
| #83 | GI and LOD cost measurement | `tools/playtest/plans/next_monitor.json:3` | Partly |
| #84 | Portal emitter log check | `tools/playtest/plans/current_run.json:8`, `docs/AUTOPLAYTEST.md` | Partly |
| #85 | Harness request (backlog) | `tools/playtest/plans/next_monitor.json:65` | Partly |
| #86 | Recording ambiguity request (backlog) | `docs/AUTOPLAYTEST.md:171` | Partly |
| #87 | Merged after v0.3.5 (no per-item text) | `docs/PLAN_NEXT.md:8` | No (N-19) |
| #89 | Pending PRs: FoliageBoost, WaterMaskSoft, AtrousFar (all three exist in code now) | `docs/AUTOPLAYTEST.md:171`, `docs/PLAN_NEXT.md:8`, `src/atmos.inl:239,303`, `src/gi.inl:95` | Partly |
| #91 | Playtest harness progress log, status file, plan files | `CHANGELOG.md:29` | Yes |
| #92 | Water fog mask softening `[fog] WaterMaskSoft` | `CHANGELOG.md:24` | Yes |
| #93 | GI far ring `[gi] AtrousFar` | `CHANGELOG.md:25` | Yes |
| #96 | F11 A/B "off" frame closer to vanilla | `CHANGELOG.md:32` | Yes |
| #98 | Merged after v0.3.5 (no per-item text) | `docs/PLAN_NEXT.md:8` | No (N-19) |
| #100 | Portal emit fix (untested) | `docs/PLAN_NEXT.md:8,11` | No (text only) |
| #102 | F10 is the default overlay key | `CHANGELOG.md:22`, `docs/PLAN_NEXT.md:19` | Yes |
| #103 | Bridge shadow line near fire (open) | `README.md:215`, `docs/PLAN_NEXT.md:24` | Yes (text only) |
| #104 | Water outline seen from above (open) | `README.md:213`, `docs/PLAN_NEXT.md:11` | Yes (text only) |
| #106 | `[fog] WaterNames` (lily pad names) | `CHANGELOG.md:26` | Yes |
| #107, #108, #109 | README: old A/B pairs removed; new build pairs | `CHANGELOG.md:28`, `docs/PLAN_NEXT.md:8` | Yes |
| #111, #112 | Launcher redesign (presets, tabs, gamepad) | `CHANGELOG.md:27` | Yes |
| #115 | `[fog] LateEmitBoost` (fire fog, default off) | `docs/PLAN_NEXT.md:24`, `src/default_tyrender.ini:82-83` | Partly |
| #122 | "Label the GPU model source in the session report" (HEAD merge) | `git log` in the clone | Yes (commit subject only) |

Notes on the index:
- Titles marked "No (N-19)" need `gh issue list --state all --limit 200` and `gh pr list --state all` run by a reviewer with GitHub access. The clone has no tracker data.
- `docs/PLAN_NEXT.md:8` lists the merged numbers without text. The CHANGELOG is the only place with per-item text for most of them.
- #11 appears only in `docs/PLAN_CONSOLIDATED.md` (closed 2026-10-16).

## Internal ids
Internal ids are used in todo.json, the plans and the playtest findings. Mapping to issue numbers as written in `docs/PLAN_CONSOLIDATED.md:3`, `docs/ISSUES_SEED.md:2` and `docs/FIX_PLAN_20261016.md`:

| Internal id | Issue | Meaning (short) |
|---|---|---|
| T73 | #1 | AO distance fade and character halo |
| T91 | #2 | TAA trails and grass ghosting |
| T92 | #3, #4 | Fog colour and fog consistency |
| T74 | #5 | Water makes Ty flicker |
| T78 | #6 | High and Ultra flicker |
| T76 | #10 | Distance LOD |
| T77 | #8 | Footprints glow |
| T56 | #9 | Portal light on the ground |
| T75, T82 | #11 | Shadow lines and checkerboard |
| T50 | #16 | Vendor guard audit |
| C1, C2, C4 | #5, #6, #8 | Phase labels in PLAN_EXEC and PLAN_CONSOLIDATED (not in todo.json) |
| D1, D2 | #12, #13 | Overlay menu and clash guard (parallel track) |
| A1, A4 | #15 | Research on world-space alternatives |

## Documents in the repository
Every document in the repository, and where it is accounted for. Files outside the repository that the documents mention are listed in the last row.

| Document | Used on this page or elsewhere in decisions/ |
|---|---|
| `README.md` | [known-issues.md](known-issues.md), [legal-and-licensing.md](legal-and-licensing.md) |
| `CHANGELOG.md` | This page (release ladder), [known-issues.md](known-issues.md) |
| `docs/START_HERE.md` | [decisions-log.md](decisions-log.md), [failed-and-abandoned.md](failed-and-abandoned.md) |
| `docs/RESUME.md` | This page (timeline), [roadmap-and-open-work.md](roadmap-and-open-work.md) |
| `docs/DOC_MAP.md` | Index of docs (historical); this page |
| `docs/GOAL.md` | [decisions-log.md](decisions-log.md) |
| `docs/PLAN_NEXT.md` | [roadmap-and-open-work.md](roadmap-and-open-work.md), this page |
| `docs/PLAN_CONSOLIDATED.md` | [decisions-log.md](decisions-log.md), [roadmap-and-open-work.md](roadmap-and-open-work.md) |
| `docs/PLAN_EXEC.md` | [decisions-log.md](decisions-log.md) |
| `docs/PLAN_3DAY_20261016.md` | [roadmap-and-open-work.md](roadmap-and-open-work.md) (exists in the clone; the first pass said it did not) |
| `docs/PLAN_R6.md` | [failed-and-abandoned.md](failed-and-abandoned.md) (R6-1 result), [roadmap-and-open-work.md](roadmap-and-open-work.md) |
| `docs/FIX_PLAN_20261016.md` | [roadmap-and-open-work.md](roadmap-and-open-work.md) |
| `docs/STATUS_2026-10-13.md` | This page (builds), [known-issues.md](known-issues.md) |
| `docs/PERFORMANCE.md` | Cost pages (not in this chapter) |
| `docs/AUTOPLAYTEST.md` | Testing chapter; #72, #80, #84, #86, #89 mentions on this page |
| `docs/EMIT_PORTAL_TEST_PLAN.md` | [roadmap-and-open-work.md](roadmap-and-open-work.md) |
| `docs/ISSUES_SEED.md` | [known-issues.md](known-issues.md) (seed table) |
| `docs/MATERIAL_CLASSIFICATION_PLAN.md` | [roadmap-and-open-work.md](roadmap-and-open-work.md), [known-issues.md](known-issues.md) |
| `docs/PLAYTEST_2026-10-17_FINDINGS.md` | [known-issues.md](known-issues.md), this page |
| `docs/recording/INDEX.md` | [known-issues.md](known-issues.md) (problem ids) |
| `docs/recording/FINDINGS_AND_PLAN.md`, `FINDINGS_ROUND2.md`, `FINDINGS_ROUND3.md`, `FINDINGS_ROUND4.md` | This page (playtest rounds); [known-issues.md](known-issues.md) |
| `docs/recording/*.txt` (voice transcripts) | Not quoted anywhere (project rule, `docs/START_HERE.md`; plan section 4 Ch13) |
| `docs/research/WORLD_SPACE_AO_GI_FOG.md`, `minecraft_shaderpack_port_ranking.md` | [research-notes.md](research-notes.md) |
| `docs/results/VERDICTS.md`, `DIRECTION_2026-10-13_checkpoint1..3.md` | [failed-and-abandoned.md](failed-and-abandoned.md), [decisions-log.md](decisions-log.md) |
| `docs/results/g1_gate.md`, `g1_gate_prev_3EE5DA28A7.md`, `merge_log.md`, `cost_table.md`, `preset_matrix.md`, `balanced_result.md`, `R6_config_diff.md` | This page (G1 and merges); cost pages |
| `docs/agent-tasks/BRIEFING.md`, `_common.md`, and 21 task files | Agent prompts (contributor chapter). Accounted for as history: they are per-round records named in `docs/todo.json`. |
| `docs/todo.json` | [roadmap-and-open-work.md](roadmap-and-open-work.md) (status counts), this page |
| `docs/results/*.csv`, `*.png` data | Not documentation |
| `docs/ab/*.jpg`, `docs/playtest_frames/*` | Screenshots (concepts chapter) |
| `.github/ISSUE_TEMPLATE/*`, `.github/pull_request_template.md` | Contributor chapter |
| Not in the repository: the owner's research notes and reverse-engineering notes, a chat export, the owner's archive folder, and an external memory mirror | Named in `docs/DOC_MAP.md` and `docs/START_HERE.md`. Not available here (N-17). |

## NEEDS INVESTIGATION
- **N-19 Issue and PR titles and states for #36 to #122.** Method: `gh issue list --state all --limit 200` and `gh pr list --state all --limit 200` with a GitHub account that can read `<owner>/<repo>`, then add one line per number to the table above.
- **N-25 Which status documents are historical.** Method: compare each document's own date line with the newest CHANGELOG entry; tag each snapshot as historical in this page. Done here for the 2026-10-13 files; reviewer to confirm.
- **N-28 Meaning of the todo.json items not described in documents.** Partly done: status counts are in [roadmap-and-open-work.md](roadmap-and-open-work.md). Still open: ids T52, T55, T66 to T69 and T87 to T89 are referenced or missing (see below). Method: search the repository for each id (`grep -rn 'T52' docs`), then read the `notes` field in `docs/todo.json`.
- **v0.3.5 release date, tag and zip.** Method: `gh release list` and `gh api repos/<owner>/<repo>/tags`. The repository does not say.
- **Whether v0.3.4 and v0.3.5 were the same DLL.** Method: compare the SHA-1 of the release zip with `3ee5da28a7` and the G1 records.

## Discrepancies
1. **Current release.** `docs/RESUME.md:7` and `docs/STATUS_2026-10-13.md:23` say v0.3.2 is the shipped release. `CHANGELOG.md:46` and `docs/PLAN_NEXT.md:7` (2026-10-18) say v0.3.5 (c255ae8) is the last full release. The newer documents win.
2. **Name of the next release.** `docs/FIX_PLAN_20261016.md:54` and `docs/PLAN_3DAY_20261016.md:175` say v0.3.3. `docs/PLAN_EXEC.md:7` says the next release is v0.3.5, not v0.3.3. `docs/PLAN_NEXT.md:39` says v0.3.6 after the next playtest.
3. **v0.3.5 status before and after its release.** `docs/PLAN_CONSOLIDATED.md:34-41` (2026-10-16) lists AO flicker (#1), TAA ghosting (#2) and the fog colour change (#3) as open batch 1 items. `CHANGELOG.md:50-51` lists them as fixed in v0.3.5. The CHANGELOG does not say "untested" on those lines. Which is current is NEEDS INVESTIGATION (see known-issues.md).
4. **Title string.** `src/TYLauncher.c:22` and `src/sesslog.inl:16` still say `v0.3.5-rc` on HEAD, after the v0.3.5 release line in CHANGELOG. `CHANGELOG.md:62-64` records the same defect.
5. **Dead link.** `README.md:163` links `docs/FINDINGS_ROUND4.md`. The file is at `docs/recording/FINDINGS_ROUND4.md`.
6. **First pass.** The first-pass note in `KNOWN_ISSUES_AND_HISTORY.md` section 6 says `docs/PLAN_3DAY_20261016.md` was not found in the clone. It is present (217 lines). That note is corrected here.
7. **Version of the playtest 5 record.** `docs/recording/FINDINGS_ROUND4.md:2` says "Not confirmed by the user yet" (status as of 19:23). Its section "Playtest 5" (line 14) describes a user playtest of the v0.3.3 test build in the evening, after that time.
8. **Build id vs DLL hash.** `3ee6ea7` (build id in PLAN_NEXT:22) and `3ee5da28a7` (DLL SHA-1 in RESUME and PLAN_CONSOLIDATED) differ. Not the same identifier; do not merge them.
9. **Build ids in playtest order.** PLAN_NEXT lists `4a2ef95` first, then `cda9364`, then `3ee6ea7`. The dates suggest `cda9364` came first (2026-10-17) and `3ee6ea7` before `4a2ef95` (both 2026-10-18). The order in this page follows the dates.
10. **Missing CHANGELOG entries.** v0.3.1 to v0.3.4 do not appear in `CHANGELOG.md` at all, although `docs/RESUME.md` and `docs/results/VERDICTS.md` describe them.

## See also
- [known-issues.md](known-issues.md) for every open defect and its evidence
- [decisions-log.md](decisions-log.md) for the decisions behind each build
- [failed-and-abandoned.md](failed-and-abandoned.md) for what did not work
- [roadmap-and-open-work.md](roadmap-and-open-work.md) for what comes next
- [../reference/presets.md](../reference/presets.md) for preset values

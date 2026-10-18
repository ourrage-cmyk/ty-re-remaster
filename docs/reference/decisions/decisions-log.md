# Decisions log
> Status: documented at commit 4db1d7d; last verified 2026-10-18 (static read of the clone). Audience: C, A.

## Purpose
This page lists every decision the repository records: what was decided, when, what else was considered, why, and where the text is. It also lists the decisions the owner still has to make. A decision is only listed as "accepted" when a source says the owner accepted it. The planning documents often write proposals with a [YES] default, so each row gets one status:

- **in force**: stated as a decision and not contradicted by a later source.
- **accepted (per doc)**: a planning document says the owner accepted it, but the wording is ambiguous or another document disagrees. Confirm with the owner.
- **proposed**: written as a proposal or plan, no acceptance recorded.
- **superseded**: a later decision or code change replaced it. The row names the replacement.

The rules in `docs/START_HERE.md` section 6 and the release rule in `docs/PLAN_CONSOLIDATED.md` are decisions too. They are included below.

## How it works
Decisions are written in four places:
1. `docs/GOAL.md` has an "append only" Decisions section (`docs/GOAL.md:24-26`). Only dated user decisions go there.
2. The director memos in `docs/results/DIRECTION_2026-10-13_checkpoint1..3.md` each end with decisions and todo edits.
3. The plan documents (`docs/PLAN_CONSOLIDATED.md`, `docs/PLAN_EXEC.md`, `docs/FIX_PLAN_20261016.md`, `docs/PLAN_3DAY_20261016.md`) hold proposals with defaults and, in one case, a list of accepted defaults.
4. Code comments record some decisions as they were made (for example `src/tess.inl:6-8`, `src/ao.inl:546`).

## Decisions

### Process and governance

| ID | Date | Decision | Alternatives in the record | Reason (as written) | Source | Status |
|---|---|---|---|---|---|---|
| D-01 | 2026-10-11 | No VM testing. Laptop and this PC only. The LAN VM is parked. | Test on the VM | User decision | `docs/START_HERE.md:77` | in force |
| D-02 | 2026-10-12 | Lead owns the plan. Haiku collects and researches. A stronger "director" model reviews at checkpoints. | - | User wish | `docs/GOAL.md:25` | in force |
| D-03 | 2026-10-12 | At most three agents in parallel (two is safer). Game tests are serialised by a global lock. | - | Usage limits cut agents off | `docs/START_HERE.md:68` | in force |
| D-04 | 2026-10-12 | Work is resumable: every step has a log, and agents restart from their own log. | - | Usage limits | `docs/START_HERE.md:69`, `docs/PLAN_EXEC.md:24` | in force |
| D-05 | 2026-10-13 | Ask the user before changing the game folder. No game test while the user is playing. | - | Safety of the user's session | `docs/RESUME.md:44`, `docs/PLAN_CONSOLIDATED.md:24` | in force |
| D-06 | 2026-10-16 | One PR per fix. Branch `fix/<id>-<slug>`. PR body contains `Closes #<n>`. | - | Traceability | `docs/PLAN_CONSOLIDATED.md:24` | accepted (per doc) |
| D-07 | 2026-10-16 | Issues close only after the owner's playtest. | - | Evidence from the owner | `docs/PLAN_NEXT.md:44` | in force |
| D-08 | 2026-10-16 | Model use: Haiku for research, docs, merges, gates and measurement scripts. Sonnet for shader code. Opus only for a root cause that two Sonnet rounds did not solve. A Haiku coder with no measured progress after about eight steps is re-run as Sonnet. | "haiku only" was offered as the alternative | Cost and quota | `docs/PLAN_EXEC.md:21,72` | accepted (per doc; "defaults unless told otherwise", `docs/PLAN_EXEC.md:68`) |

### Release and packaging

| ID | Date | Decision | Alternatives | Reason | Source | Status |
|---|---|---|---|---|---|---|
| D-09 | 2026-10-13 | v0.3.2 is released as "preview / community beta", not as a full release. Criteria 1 to 4 of GOAL are only partly met. | Full release | Partly met criteria; one GPU tested | `docs/GOAL.md:26`, `docs/results/DIRECTION_2026-10-13_checkpoint3.md:15-17` | in force |
| D-10 | 2026-10-16 | Release artifact is `TYLauncher.exe`. It embeds the matching DLL and default ini and installs them beside itself. An existing `tyrender.ini` is never overwritten. Zips are a secondary asset. | Zip as primary | Users run one file | `docs/PLAN_CONSOLIDATED.md:61-63` | accepted (per doc; header says "owner, 2026-10-16") |
| D-11 | 2026-10-16 | Zips, DLLs and exes are never committed to the repository. | - | Repository size and binaries | `docs/PLAN_CONSOLIDATED.md:63`, `docs/ISSUES_SEED.md:24` | in force |
| D-12 | 2026-10-16 | Pre-releases (untested in game) are marked as pre-release on GitHub. | - | Honest labelling | `docs/PLAN_CONSOLIDATED.md:65` | accepted (per doc) |
| D-13 | 2026-10-16 | Launcher title shows version and build id; `-dirty` when the tree is modified. | - | Testers must know the exe is current | `docs/PLAN_CONSOLIDATED.md:64` | accepted (per doc). Current code still says `v0.3.5-rc` (`src/TYLauncher.c:22`) |
| D-14 | 2026-10-16 | Release cadence: ship after batch 1 and batch 2, with one playtest in between. | Ship batch 1 alone quickly | Fewer releases | `docs/PLAN_CONSOLIDATED.md:19` (answer E), `docs/FIX_PLAN_20261016.md:54` | accepted (per doc). Version name conflicts (see Discrepancies) |
| D-15 | 2026-10-16 | Next release is one release after Phases B, C and E. The in-game menu (Phase D) moves to v0.4 if time is short. | Include the menu | Quota | `docs/PLAN_EXEC.md:73` (E, default) | proposed (default). PLAN_EXEC:7 says the next release is v0.3.5 |
| D-16 | 2026-10-13 | Medium is the default preset. | Balanced name kept | Held up best in playtests; measured about 12 ms against a 16.7 ms budget | `docs/GOAL.md:26`, `README.md:163`, `docs/STATUS_2026-10-13.md:10` | in force |
| D-17 | 2026-10-13 | User-facing preset names are Low, Medium, High, Ultra. "Balanced" is retired (it is the internal name for Medium in some files). | Keep "Balanced" | Plain names for testers | `docs/GOAL.md:26` | in force |

### Performance budget and feature scope

| ID | Date | Decision | Alternatives in the record | Reason | Source | Status |
|---|---|---|---|---|---|---|
| D-18 | 2026-10-13 | Bounce light (GI): stop investing in the cost-heavy variant. Hi-Z with one ray stays as an option for High and Ultra, off in Balanced. Legacy GI (4 rays, +16.9 ms) is not used in any preset. | Legacy 4 rays; Hi-Z 2 or 4 rays | About 1 to 3 out of 255 change at the tuned strength; cost +4.7 ms | `docs/results/DIRECTION_2026-10-13_checkpoint1.md:21`, `..._checkpoint2.md:19`, `docs/results/VERDICTS.md:7` | in force (later GI changes in v0.3.3 added source exclusions, see D-27) |
| D-19 | 2026-10-13 | Atmosphere: keep the froxel fog. Remove the ray-march variant from the presets. | Ray-march (posterised blotches on far rock) | Cost and banding | `docs/results/DIRECTION_2026-10-13_checkpoint1.md:22`, `docs/results/VERDICTS.md:5` | superseded in part by D-22: no ray-march code remains that the grep found (`src/atmos.inl` has only log text); the fallback is "OFF" |
| D-20 | 2026-10-13 | AO: keep the XeGTAO-style v2 and fix its near-field smear once. If not fixed in one session, AO v2 goes off in Balanced. Legacy AO (+5.8 ms) is not in Balanced. | Legacy AO | Cost +1.4 ms against +5.8 ms | `docs/results/DIRECTION_2026-10-13_checkpoint1.md:23`, `docs/results/VERDICTS.md:40` | in force (smear fixed: `docs/results/VERDICTS.md:40`) |
| D-21 | 2026-10-13 | Balanced preset = candidate C1 (11.1 ms at the default view). Keep at least 3 ms in reserve. Contact shadows 0.35 only if a feet crop shows Ty grounded; shadow maps 8192 and tess Scope=all are for High and Ultra only. | B1, B2, B3 candidates (17 to 22 ms) | Budget 16.7 ms | `docs/results/DIRECTION_2026-10-13_checkpoint2.md:17-19`, `docs/results/balanced_result.md:1-7` | superseded in part: High ladder in STATUS includes contact shadows 0.35 (`docs/STATUS_2026-10-13.md:11`). Medium keeps contact off (`docs/results/R6_config_diff.md:3`) |
| D-22 | 2026-10-13 | Fallback policy: an effect that fails switches itself OFF. It does not run a slower legacy path. | Legacy AO, legacy GI, ray-march as automatic fallbacks | Forced fallback frame was 30.72 ms, a failing machine would run at about 32 fps; fallback must not be slower than the preset it replaces | `docs/results/DIRECTION_2026-10-13_checkpoint2.md:10,20` (forced frame 30.72 ms before the change), `docs/START_HERE.md:79`, `README.md:265` | in force. Code: `src/ao.inl:546`, `src/gi.inl:130`, `src/atmos.inl:371`. Stale log text: `src/atmos.inl:257,260` says "ray-march path" |
| D-23 | 2026-10-13 | Tessellation scope default: no environment tessellation. Relief comes from the texture in the pixel shader. `Scope=skin` and `Scope=env-detail` both mean this. | `Scope=all` (environment tessellation) | +3.9 to +6.2 ms with little visible extra | `src/tess.inl:36`, `docs/results/VERDICTS.md:43`, `docs/results/cost_table.md` (scope_all +6.22) | in force. Ultra only for `all` (`docs/results/DIRECTION_2026-10-13_checkpoint2.md:19`) |
| D-24 | 2026-10-13 | Characters are never tessellated, and get no detail layer. All other textures get relief. | Tessellate characters (playtest 1 complaint F11) | Ridges and aliasing on the character | `src/tess.inl:6-8` ("REC round 2026-10-13"), `src/tess.inl:107`, `docs/recording/FINDINGS_AND_PLAN.md:17,21` | in force |
| D-25 | 2026-10-13 | Only the AMD RX 7600 XT is claimed as tested. Other GPUs are "untested", and the README asks for reports. | Claim portability | One test GPU | `docs/GOAL.md:18`, `docs/results/DIRECTION_2026-10-13_checkpoint2.md:34`, `README.md:230` | in force |
| D-26 | 2026-10-13 | Sun warmth (`[light] SunWarmth`) is deferred to v0.4 and does not block release. | Do it now | Optional polish | `docs/results/DIRECTION_2026-10-13_checkpoint3.md:36`, `docs/todo.json` T43 (open) | in force |
| D-27 | 2026-10-13 | Bounce-light sources: draws tagged emissive, transparent or character are not GI colour sources (`ExcludeSources`, default 7). | Heuristic capsule mask | Water and portal glow were causing blue flicker on Ty | `src/gi2.inl:33`, `src/gi.inl:93`, `docs/RESUME.md:15`, `docs/todo.json` T81, T84 | in force. The "red on ground" cause is not isolated (`docs/RESUME.md:66`) |
| D-28 | 2026-10-13 | Texture flags come from a user-filled list (`data/texture_tags.csv`), compiled into `src/tex_tags.inc` by `src/gen_tex_tags.ps1`. Names not in the list get no flags. | Heuristics from the texture data | The owner labels what should glow | `docs/RESUME.md:14`, `docs/START_HERE.md:81`, `docs/MATERIAL_CLASSIFICATION_PLAN.md:7` | in force. The class model in the material plan is proposed only |
| D-29 | 2026-10-13 | Colour match: Medium, High and Ultra brightness and saturation are set from A/B measurements against vanilla. | Keep older tint | Owner asked for the original mood | `docs/RESUME.md:64`, `docs/recording/FINDINGS_ROUND4.md:16`, `src/launcher_ui.inc:8` | in force |
| D-30 | 2026-10-13 | Session report is written locally only. Nothing is sent. The user sends it. Names, IDs, addresses and process lists are excluded. | Upload reports | Privacy | `src/sesslog.inl:1`, `src/sesslog.inl:277-278`, `README.md:244-246` | in force |
| D-31 | 2026-10-13 | Emitter (portal) light is off by default because it draws a flat white band when on. | On by default | Visible artefact | `CHANGELOG.md:17,39`, `README.md:216`, `src/emit.inl:34` | in force |
| D-32 | 2026-10-13 | New features (distance LOD, fog options, water options, GI checker, shadow LOD, emitter light) are off by default until playtested. | On by default | Untested | `CHANGELOG.md:7-44`, `src/default_tyrender.ini` (`[lod] Enable=0`, `[fog] SeeThruDepth=0`, `[decal] Clamp=0`) | in force |
| D-33 | 2026-10-16 | Overlay key default is F10 (in the shipped ini). Before, the default was 0 (off). | F12 for the in-game menu (proposed in PLAN_3DAY) | Make the overlay reachable | `CHANGELOG.md:22`, `src/default_tyrender.ini:114-115`, `src/overlay.inl:43` (code default still "0") | in force for new ini files. Existing ini files keep `Key=0` (`README.md:225`) |

### Plan decisions (2026-10-16)
These come from `docs/PLAN_CONSOLIDATED.md:17-19` ("The owner said 'agree to all of them' to the defaults below.") and the defaults in `docs/FIX_PLAN_20261016.md:5`, but `docs/PLAN_EXEC.md:1` (written the same day, 23:08) says the plan is "AWAITING USER APPROVAL". The owner's acceptance is therefore recorded as accepted (per doc) and must be confirmed.

| ID | Question | Accepted default (per doc) | Alternative | Source | Status |
|---|---|---|---|---|---|
| P-A | Ultra preset | Stable High-style settings plus safe extras. Old Ultra kept as "Ultra (experimental)" in the ini only. | Keep Ultra as is | `docs/PLAN_CONSOLIDATED.md:75`, `docs/PLAN_EXEC.md:14,69`, `docs/FIX_PLAN_20261016.md:53` | accepted (per doc). User said they would play Medium rather than Ultra (`docs/recording/FINDINGS_ROUND4.md:3`) |
| P-B | Batch order | Batch 1 (stability) first, before distance LOD | LOD speed-ups first | `docs/PLAN_CONSOLIDATED.md:76`, `docs/PLAN_EXEC.md:70` | accepted (per doc) |
| P-C | Footprints | Texture-name clamp for bright decals first (`[decal] Clamp`) | Deeper decal-path work | `docs/PLAN_CONSOLIDATED.md:77`, `docs/FIX_PLAN_20261016.md:55`, `docs/PLAN_EXEC.md:71` | accepted (per doc). The material plan says the glow is clipping, not a tag problem (`docs/MATERIAL_CLASSIFICATION_PLAN.md:23`). Clamp is off by default (`src/tex.inl:72`) |
| P-D | Other GPUs | Reports welcome, not blocking | Block release until tested | `docs/PLAN_CONSOLIDATED.md:78` | accepted (per doc) |
| P-E | Release cadence | See D-14 | Batch 1 alone | `docs/PLAN_CONSOLIDATED.md:79` | accepted (per doc). Conflicts with PLAN_EXEC.md:73 |
| P-F | Community A/B grids | Medium first (honest). Ultra only after it is clean. | Ultra too | `docs/PLAN_EXEC.md:55,74` | proposed (default [YES]) |
| P-G | Portal spill | Deferred to v0.4 unless trivial | Do now | `docs/FIX_PLAN_20261016.md:37,56`, `docs/PLAN_CONSOLIDATED.md:51` | accepted (per doc) |
| P-H | Model use for coding | Sonnet for shader code, Haiku for the rest, with the escalation rule | "Haiku only" | `docs/PLAN_EXEC.md:72` | proposed (default [YES]) |

### Material and emitter model (proposal, 2026-10-17)

| ID | Decision | Source | Status |
|---|---|---|---|
| M-1 | One class per texture (character, emissive character, emitter, transparent emitter, transparent, solid, unknown). Emission decided by the emissive flag alone. Character only removes tessellation and GI colour bounce. | `docs/MATERIAL_CLASSIFICATION_PLAN.md:43-57` | proposed (plan only, dated 2026-10-17) |
| M-2 | Owner decisions on individual texture tags: the speedboat texture is a character; the fluffy-burn and jellyfish textures are emissive characters and must glow. | `docs/MATERIAL_CLASSIFICATION_PLAN.md:20-22` | accepted (per doc, "2026-10-17 playtest thread") |
| M-3 | Portal spill (emissive light onto the ground) uses a half-resolution additive design. | `docs/EMIT_PORTAL_TEST_PLAN.md:3`, `docs/FIX_PLAN_20261016.md:37` | proposed. Test prepared, not run |

## Decisions the owner still needs to make
These are open. Each one blocks a step in the plans.

| # | Question | Where asked | Blocks |
|---|---|---|---|
| O-1 | Sign-off on the default look and presets (todo T29) | `docs/todo.json` T29 (open); `docs/RESUME.md:36` | Release |
| O-2 | Licence for the project's own code (none in the repository) | `docs/todo.json` (none); N-16; [legal-and-licensing.md](legal-and-licensing.md) | Publishing the source |
| O-3 | Whether reverse-engineering notes can be published in sanitised form | N-17; [legal-and-licensing.md](legal-and-licensing.md) | Reproducibility of RE claims |
| O-4 | Which version name to use for the next release (v0.3.3, v0.3.5 or v0.3.6) | See Discrepancies in [history-and-timeline.md](history-and-timeline.md) | Release notes |
| O-5 | Whether a tester on NVIDIA, Intel or Proton can run one launch and send `tyrender.log` | `docs/PLAN_CONSOLIDATED.md:78`, `docs/PLAN_EXEC.md:75` (G), `docs/RESUME.md:36` (T29) | Portability claim (#16) |
| O-6 | Whether to publish the community A/B grids before Ultra is fixed | P-F | Community post |

## Code pointers

| What | file:line | Note |
|---|---|---|
| Fallback policy comments (AO, GI, atmosphere) | `src/ao.inl:546`, `src/gi.inl:130`, `src/atmos.inl:371` | "no legacy fallback" |
| Forced-fallback debug bits | `src/ao.inl:437`, `src/atmos.inl:257`, `src/tess.inl:382`, `src/light.inl:36` | Test aids |
| Tessellation scope and character rule | `src/tess.inl:6-8`, `src/tess.inl:36`, `src/tess.inl:107` | D-23, D-24 |
| Clash guard (warn only) | `src/tyrender.cpp:632-659` | Never rewrites the game's settings.ini |
| Session report privacy statement | `src/sesslog.inl:277-278` | D-30 |
| Overlay key default in code | `src/overlay.inl:43` | Default "0", shipped ini says F10 |
| Preset names and colour values | `src/launcher_ui.inc:6-8` | D-16, D-17, D-29 |

## Settings
Decisions set these defaults. The keys and their defaults are listed in the settings pages: [../reference/settings-system-debug.md](../reference/settings-system-debug.md) (`[debug]`, `[report]`, `[clash]`, `[overlay]`), [../reference/settings-post-shadows-light-pbr-tess.md](../reference/settings-post-shadows-light-pbr-tess.md) (`[tess] Scope`, `[decal] Clamp` and others), [../reference/settings-atmos-fog-emit.md](../reference/settings-atmos-fog-emit.md) (`[fog]`, `[emit]`, `[atmos] SkyFogDist`).

## Cost
Cost decisions (D-18, D-20, D-21, D-23) use the numbers in `docs/results/cost_table.md` and `docs/results/VERDICTS.md`. Those numbers come from the v0.3 merged build. Later builds were not re-measured one option at a time (`docs/STATUS_2026-10-13.md:27`).

## Limits
- The record is informal. There is no decision ID scheme in the repository. The IDs D-, P- and O- are added here for reference only.
- Where a proposal and an acceptance were written on the same day with different content, this page cannot tell which the owner saw.
- Decisions made by coding agents in their own report files are not in the repository. Only what the lead copied into the docs is here.

## Known issues
- `docs/PLAN_CONSOLIDATED.md:19` and `docs/PLAN_EXEC.md:1` disagree on whether the owner approved the 2026-10-16 defaults (P-A to P-H).
- D-33 vs `README.md:225`: the shipped default and the existing-ini behaviour differ, and the code default is "0".

## How to verify
1. Open `docs/GOAL.md` and check the Decisions section (lines 24-26). Only the dated user decisions should be there.
2. For a code-backed decision, open the cited line. For example, `src/atmos.inl:371` should turn atmosphere OFF when the froxel path is unavailable, and log "(no heavy ray-march fallback)".
3. To check a default, read the code default (the `GetPrivateProfile*` line) and the shipped ini (`src/default_tyrender.ini`). They can differ (see `[tess] Scope`, D-23).

## NEEDS INVESTIGATION
- **N-16 Licence of the project's own code.** The repository has no LICENSE, COPYING or NOTICE file (`git ls-files` finds none). Method: owner decision (O-2), and `gh api repos/<owner>/<repo>/license` on GitHub. Not checkable from the clone.
- **P-A to P-H acceptance.** Method: ask the owner whether they approved the list in `docs/PLAN_CONSOLIDATED.md:17-19`, or whether `docs/PLAN_EXEC.md` (awaiting approval) is the live version.
- **Whether the 2026-10-16 "release rule" was adopted.** Method: check the GitHub release list for a tag `v0.3.5` and a `TYLauncher.exe` asset.

## Discrepancies
1. **Acceptance of the 2026-10-16 plan.** `docs/PLAN_CONSOLIDATED.md:17-19` ("agree to all of them") versus `docs/PLAN_EXEC.md:1` ("AWAITING USER APPROVAL") and `docs/PLAN_EXEC.md:68` ("I proceed on defaults unless told otherwise").
2. **Next release version.** `docs/FIX_PLAN_20261016.md:5,54` (v0.3.3, decision E: ship after batch 1 and 2), `docs/PLAN_EXEC.md:7,73` (v0.3.5), `docs/PLAN_CONSOLIDATED.md:19` (v0.3.5 milestone).
3. **Overlay key versus in-game menu key.** `docs/PLAN_3DAY_20261016.md:135` and `docs/FIX_PLAN_20261016.md:46` plan the in-game menu on F12. The implemented overlay uses F10 (`CHANGELOG.md:11`, `src/default_tyrender.ini:115`). Both can exist, but no source says so.
4. **Tessellation scope default.** `src/tess.inl:36` default is "env-detail"; `src/default_tyrender.ini:43` ships `Scope=skin`; `src/launcher_ui.inc:64,74` writes "skin" unless "all" is chosen; `README.md` says "env-detail" is the default. Functionally the same, since both mean no environment tessellation, but the text disagrees.
5. **Preset naming.** D-17 retires "Balanced". The launcher's preset names are Low, Medium, High, Ultra (`src/launcher_ui.inc:6`), but a comment in `src/launcher_ui.inc:46` still says "Balanced", and the tools default to `preset_Balanced.ini` (`docs/RESUME.md:59`).
6. **Medium "held up best".** `README.md:163` says Medium "held up best in playtests". The playtest 4 finding (`docs/recording/FINDINGS_ROUND4.md:3`) agrees. The R6-1 analysis (`docs/results/R6_config_diff.md:1-6`) found all flicker-free runs were Medium. Consistent, but the cause of the High and Ultra flicker is now given two ways (see [known-issues.md](known-issues.md)).

## See also
- [history-and-timeline.md](history-and-timeline.md) for the dates behind each build
- [failed-and-abandoned.md](failed-and-abandoned.md) for the options that were dropped
- [known-issues.md](known-issues.md) for the defects that the decisions were made for
- [roadmap-and-open-work.md](roadmap-and-open-work.md) for what is still open
- [legal-and-licensing.md](legal-and-licensing.md) for the licence and privacy decisions

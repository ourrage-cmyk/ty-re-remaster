# DOC MAP - every remaining document and its purpose (2026-10-13, docs pass 2)

Read order for a fresh session: `START_HERE.md` -> `RESUME.md` -> `PLAN_NEXT.md` (order of work) -> (numbers) `STATUS_2026-10-13.md` -> (problem ids) `recording\INDEX.md`. Superseded files are in `archive\20261013\` (index `archive\20261013\MOVED.md`). Backups `*.bak_*` next to files are kept and are not listed.

## Root of C:\CLAUDE\RESEARCH
| File | Purpose |
|---|---|
| `START_HERE.md` | Single entry point: project, locations, release and test build, resume steps, tool list, OpenViking sync scope, rules and traps, engine facts |
| `RESUME.md` | Current state only: release, test build v0.3.3, running agents (none), open items with todo ids, next actions, resume steps, known inconsistencies |
| `PLAN_NEXT.md` | Order of work (batches 1-3, progress per item) and the decisions still waiting for the user (A-E) |
| `STATUS_2026-10-13.md` | All measured numbers: preset ladder, G1 gates, option costs, flicker results, v0.3.3 smoke |
| `GOAL.md` | Fixed north star and acceptance criteria (append-only decisions; status line at the top) |
| `DOC_MAP.md` | This file |
| `todo.json` | Plan and task board (the lead edits it; structure fixed; read-only for docs work) |
| `01_findings.md` | Session-1 evidence log of the install/exe/archive reverse engineering (reference) |
| `03_sources.md` | External sources used (reference) |
| `TOP10_RESEARCH.md` | Research notes (about 33 KB) behind the ten main problems; reference for method choices (timer queries, CSM, tessellation, etc.) |
| `discord_chat_text.txt` | Text of the modding-community Discord export (TygerFramework, PC_External loader); source data |
| `opengl_perf_ideas.txt` | OpenGL / RDNA3 optimisation research text (reference; projections, not measurements) |
| `runner.stop` / `*.ini` / `*.dds.bak` | Control file and fixtures, not documentation (`tyrender_ultra_baseline.ini` = old Ultra baseline settings) |

## recording\ (user playtest reviews)
| File | Purpose |
|---|---|
| `INDEX.md` | Every problem id (F*, G*, W1, S1, A1, E1, R3-*, R4-*) with status fixed / partial / open and the build that fixed it |
| `FINDINGS_AND_PLAN.md` | Round 1 (v0.3): findings F1-F12 with timestamps and plan T51-T58 |
| `FINDINGS_ROUND2.md` | Round 2 (v0.3.1 Ultra): G1-G10 and plan T60-T65 |
| `FINDINGS_ROUND3.md` | Round 3 (v0.3.2): W1, S1, A1, E1, requests, plan T70-T77, the Ultra flicker update (T78) |
| `FINDINGS_ROUND4.md` | Round 4 (playtest 4, v0.3.2 High/Ultra): High and Ultra flicker, water blue flicker, shadow bright lines, fire haze, bag tessellation, portal light, footprints, AO fade; plan T78, T80-T82 |
| `mp3.txt`, `mp3_2.txt`, `voice3.txt`, `voice4.txt`, `transcript.txt`, `voiceover.txt` | Transcripts of the user's voice-overs |
| `texture_tags.csv` | User-filled flags (emissive / transparent / character) for 1933 textures; input for the tag system (T80) and `gen_tex_tags.ps1` |
| `key\`, `key2\`, `key3\`, `frames\`, `sheet*.jpg` | Key frames from the recordings |

## results\ (docs only; csv/json/png are data)
| File | Purpose |
|---|---|
| `VERDICTS.md` | Lead reviews of every agent result, gate results, in-session A/B numbers (history of decisions) |
| `DIRECTION_2026-10-13_checkpoint1..3.md` | Director memos (checkpoint 3 = v0.3 release readiness) |
| `g1_gate.md` | G1 release gate result for v0.3.2 |
| `preset_matrix.md` | Per-preset frame time and pass costs (v0.3 build; see note in STATUS) |
| `cost_table.md` | Cost of each option relative to Medium (v0.3 build) |
| `balanced_result.md` | Search for the Balanced (now Medium) preset candidate C1 (history) |
| `merge_log.md` | Log of agent merges into staging |
| `*_out.txt`, `runner_stdout*.txt` | Raw tool outputs (data) |
| `mo_*\motion.md`, `fl_*\flicker.md`, `*\manifest.json` | Per-suite measurement outputs |

## agents\ (prompts for coding agents)
| File | Purpose |
|---|---|
| `agents\BRIEFING.md` | Project, source layout, build, harness and current test tools, standard views, logging rules |
| `agents\tasks\_common.md` | Rules common to every agent (logging, token discipline, tools, safety, deliverable) |
| `agents\tasks\<name>.md` | One prompt per agent round (ao, ao3, atmos, atmos2, atmos3, gi, gi4, light, light2, light3, light4, perf, tess, tess2, tess4, director); finished rounds kept as records, todo.json names the file to use |
| `agents\<name>\`, `agents\atmos\*.txt` etc. | Agent ini fixtures and run outputs (data) |

## sessions\ (chronological logs; newest wins)
`sessions\2026-10-11_session3\` (LOG.md, decisions D1-D3, findings F1-F6), `session4\` (LOG.md, D4, F7_PERFORMANCE.md, data), `2026-10-12_session5\` (LOG.md, `agents\*.md` per-agent step logs including `lead.md`, `suite.md`, `runner.md`, `docs_consolidate.md`, `haiku_docs3.md` = this docs pass). All mirrored to OpenViking (the session-5 folder is mirrored by `ovsync_all.ps1`).

## Other research folders
`decomp\` (Ghidra decompiles + FINDINGS), `shaders\` (original shaders, `analysis\VARIANTS.md`), `rkv_index\`, `rkv_extract\`, `wayfinder\` (tracker skill), `AB_shots\` (community A/B images, README, `*_DO_NOT_SHARE` folders are failed attempts), `shots\` (early reference screenshots and run logs).

## Shipped docs (user-facing, keep consistent with reality)
| File | Purpose |
|---|---|
| `C:\CLAUDE\SRC\tyrender\README.md` | Shipped README (features, presets, install, settings table, fallback, known issues, v0.3.3 test-build section). Copy in `SRC\merge` (same text). Backups `*.bak_20261013_docs2` |
| `C:\CLAUDE\SRC\tyrender\PERFORMANCE.md` | Shipped cost and settings reference (v0.3.3 test-build section added). Copy in `SRC\merge`. Backups `*.bak_20261013_docs2` |
| `C:\CLAUDE\SRC\tyrender\dist\TYHD_Renderer_v0.3.2.zip` | Shipped release package (G1 passed) |
| `C:\CLAUDE\SRC\tyrender\dist\TYHD_Renderer_v0.3.3.zip` | Test build (not released; G1 not run; its inner `tyrender.ini` header and `Scope=skin` need a check before release; zips are not edited) |
| `C:\CLAUDE\SRC\agents\*\REPORT.md` | Per-agent reports (what changed, ini keys, numbers); each starts with a status line saying whether it is merged or superseded |

## Archive
`archive\20261013\`: old long logs and status documents with the index `MOVED.md`. Added in docs pass 2: `RESUME_appended_log_20261013_1923.md` (the append log that used to sit at the end of RESUME.md, kept verbatim).

# DIRECTION - checkpoint 1 (T08), 2026-10-13
Evidence read: GOAL.md, todo.json + board, VERDICTS.md, ledger (last 15), v_ao/manifest.json, agent REPORTs (atmos, light; ao/gi/tess have NONE), images: v_atmos/sheet, v_ao/sheet, v_gi/sheet.

## 1. On track?
| # | criterion | status | evidence |
|---|---|---|---|
| 1 | No regressions | PARTLY | Ty intact in atmos/ao/gi sheets + v_atmos_r1/r2; AO v2 dark smear at sand_graze CONFIRMED on v_ao/sheet; no soak, no forced-fallback test, feet unknown |
| 2 | Balanced <= 16.7 ms | UNKNOWN | known: stock 5.38 + AO v2 1.42 + atmos 3.69 = 10.5 ms (p_ao, v_atmos_cost). Tess + shadows/light UNMEASURED; Ultra engine+rest 29.5 ms (p_ref) says the unmeasured part is where the cost is |
| 3a | Far rock less saturated, near not | MET | v_atmos/sheet: far walls paler, sand/fence/Ty unchanged; rock_sat 0.85->0.50. Froxel also hazes mid-range walls (fence_mid) - acceptable. Raymarch posterises -> drop it as a choice |
| 3b | Sand cracks / fence grain / rope relief, feet on ground | NOT VERIFIED | no tess suite exists; tess has no REPORT.md |
| 3c | Shadows tinted/soft, no stipple; lighting natural | NOT VERIFIED | light REPORT "in progress, not measured"; per-frame IGN rotation = stipple risk; new filter slower than old |
| 3d | GI/AO not noisy | PARTLY | AO v2 smear (defect); GI changes image by ~1-3/255 - invisible on v_gi/sheet at any variant |
| 4 | Portability | UNKNOWN | only agent claims (atmos GL4.3 check); nothing tested |
| 5 | Package | NOT MET | LAA done (T00); rest pending T26 |

## 2. Biggest risks
1. **The plan has verified the three effects the user cared LEAST about and none of the two he cared MOST about.** GOAL names sand/fence/rope relief, feet, natural lighting and soft tinted shadows explicitly; tess and light are unmeasured, unviewed, and are the likely owners of the 6-24 ms of Ultra "engine+rest" cost. If either needs >6.2 ms at Balanced, the 60 fps goal fails and everything already tuned is moot.
2. **Plan structure blocks progress and invites conflicts:** T15 waits on ALL five agents, so even the finished atmos cannot merge until tess+light (highest risk, high tokens) finish. Light edits tyrender.cpp (PatchShadowShader, glUseProgram, AfterLink); tess almost certainly does too -> conflicts concentrate in the last two merges, after budget is spent. Four "high"-token items (T10, T12, T13, T32) vs usage limits; T10 and T32 are low-value or speculative.

## 3. Decisions
- **GI: stop investing.** Invisible at tuned strength, cheapest variant +4.7 ms does not fit next to tess+shadows. Ship hiz1 as Ultra-only opt-in, default OFF; no agent session (T10 -> lead config, low). Merge GI LAST; if it conflicts, drop GI from v0.3. Strength increase = user question at T29, not work now.
- **Atmos: GO, froxel only.** Balanced = fog WITHOUT contact shadows (+2.12 ms) unless a feet crop shows contact shadows visibly ground Ty (then keep, +3.69). Remove raymarch from presets (keep as GL<4.3 fallback only).
- **AO: keep v2, fix smear, one attempt.** If not fixed in one session, AO v2 OFF in Balanced; legacy (+5.8 ms) never in Balanced.
- **Tess + light first.** Measure before agents spend tokens: suites now, agents with numbers in hand.
- **T32 (hidden-cost hunt) becomes conditional:** after correction the hidden share is ~1-2.6 ms/pass, not 2-5. Run only if T23 cannot find a Balanced <= 16.7 ms.
- **Merges per agent, not batched**: merge each effect as soon as its agent is final; low-conflict first (atmos, ao), then light, tess, gi.

## 4. Edits to todo.json (lead applies)
1. ADD T36 auto: suite `v_tess` on dlls\tess.dll, variants off / tess_balanced / tess_ultra, 5 views + a feet view (Ty standing on sand), restart per variant (toggles are restart-only; perf at default view). deps T01. Must precede T12.
2. ADD T37 auto: suite `v_light` on dlls\light.dll, variants off / old_filter / new_filter / new_filter+tint+hemi, 5 views + one shadow close-up; default-view perf. deps T01. Must precede T13.
3. T12, T13: deps "T07" -> "T36" / "T37"; prepend to task: "start from the suite numbers; acceptance = measured cost at Balanced <= 3 ms (tess) / <= 2.5 ms (shadows+light) at default view, plus sand/fence/rope relief visible on full-size graze crop, feet not sunk; no stipple in a still frame". Run T12/T13 BEFORE T09/T11.
4. T10: retitle "LEAD: GI hiz1 Ultra-only, default OFF, no code work"; kind lead, est low; deps T07.
5. T09: re-scope - add "contact on/off crop at Ty's feet; report whether contact shadows are worth 1.6 ms; drop raymarch from presets". est low.
6. T11: add "one attempt; if smear persists, set AO v2 default off". 
7. T15: delete. Replace with per-agent rebuild inside merge_agent.ps1 (T14 scope: build the agent folder first).
8. T16 deps -> "T14,T09"; T17 -> "T16,T11"; NEW ORDER: T18 = light (deps T17,T13), T19 = tess (deps T18,T12), T20 = gi (deps T19,T10, fail -> skip GI, not block).
9. T32: deps -> "T23"; notes "run ONLY if T23 FAILs"; T22 deps drop T32.
10. ADD T38 auto: forced-fallback test - ini/debug key that makes each effect's shader fail to compile + GL<4.3 path; suite must show stock image, no crash. deps T20; T26 deps add T38.
11. T21 stays after T20 (checkpoint 2).

## 5. Next checkpoint
T21 (after merges). Single gate before it: **merged staging DLL + Balanced candidate ini (AO v2, atmos fog, light, tess Balanced; GI off) measures <= 16.7 ms frame at the default view, 2560x1440, with images 5/5 and an intact ty_strip.** If it fails, T32 runs before T21.

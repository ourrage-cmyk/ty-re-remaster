# Playtest request queue

Other threads and agents file a request here, one JSON file each (`req-<id>.json`, template in `TEMPLATE.json`). The harness owner runs `build_plan.ps1` before each playtest, which merges every `pending` request into one plan, runs it, and reports back. Finished requests move to `done/` with a link to the report. Requests the harness can't check go to `BACKLOG.md` with the missing capability named.

# Visual review rubric

After every run, a reviewer (a Haiku subagent with image reading) looks at the contact sheet in `C:\CLAUDE\RESEARCH\autoplaytest\latest_for_review\` and writes `visual_review.json` in the run folder. The reviewer is given this file, the sheet, and nothing else.

The verdict is advisory. It never turns a failed numeric check into a pass. A `suspect` or `broken` verdict turns a PASS into INCONCLUSIVE and flags the frame for the owner.

## Questions, one row per variant

1. Is it the game window and not black, desktop or a blank frame?
2. Is the sky a flat white band, or clipped?
3. Are there horizontal tears or offsets?
4. Is there a hard step between fog and rock?
5. Is the F10 overlay leaking into the scene?
6. Are there blue or flat patches on Ty or on the water?
7. Does it look plausibly like the stock frame?

## Output

`visual_review.json`, one object per variant:

```json
[
  { "variant": "stock", "verdict": "ok", "evidence": "window, sky normal, no tears" },
  { "variant": "fog_seethru1", "verdict": "suspect", "evidence": "hard fog/rock step on the left ridge" }
]
```

- `verdict`: `ok`, `suspect` or `broken`. Use `suspect` when a person should look. Use `broken` when the frame is unusable.
- `evidence`: one line, what was seen, and which question it answers.

Ask about one question at a time. Say "not visible" when the sheet does not show the question. Do not guess.

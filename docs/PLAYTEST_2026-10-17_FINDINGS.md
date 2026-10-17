# Playtest 2026-10-17 — annotated findings (build cda9364)

Scope: evidence pass only. The game was not launched or edited. Frames come from the playtest recording and the F11 captures. Items marked **not confirmed** could not be seen in the recording or the captures.

## Inputs

**Recording** (used): `C:\CLAUDE\recording\Recording 2026-10-17 094656.mp4`
- 9:43.87, 2562x1442, 30 fps, H.264 + AAC. Same file also appears as `C:\CLAUDE\RECORDING\` (case-insensitive path).
- Not used: `C:\CLAUDE\recording\Recording 2026-10-17 000242.mp4` (67 MB, earlier session).
- The "separate game capture starts at 57 s" claim was not checked.

**F11 screenshots** (8 on/off pairs): `C:\CLAUDE\TY the Tasmanian Tiger\Screenshots\ty_cda9364_Ultra_shaders-{on,off}_20261017-{093818,093904,094459,094510,094533,094537,094543,094559}.png`, 2560x1440.

**Frames**: extracted full size with the ffmpeg binary bundled in Topaz Video AI (`C:\Program Files\Topaz Labs LLC\Topaz Video AI\ffmpeg.exe`), since no standalone ffmpeg was on PATH. Full-size PNGs are in `C:\CLAUDE\scratch\playtest_2026-10-17\` (outside the repo). Committed copies are 960 px JPEG at quality 70.

## Frame timestamps

| Time | Extracted | Committed copy | What is on screen |
|---|---|---|---|
| 0:15 | yes | `docs/playtest_frames/fog_water_0015.jpg` | Ty on red cliff edge, pink haze over the far water and lily area |
| 2:05 | yes | `docs/playtest_frames/water_overlay_0205.jpg` | Ty in water, semi-transparent water layer over the jungle. A Windows system menu (Restore/Move/Size/Minimize/Maximize/Close) sits at top-left. That menu is OS UI, not the game. |
| 2:10, 2:15 | yes, but identical | not committed | Byte-identical to each other (md5 04cf6fb6…). The video is frozen across this interval. |
| 2:20 | yes | not committed | Water overlay still present |
| 4:20 | yes | `cutscene_after_reload_0420.jpg` | Cutscene: Ty and a large creature in a clearing. No geometry break visible. |
| 5:40 | yes | `fog_trees_L2_0540.jpg` | Cockatoo dialogue cutscene, "There's plenty to do here". Trees show no obvious poke-through. |
| 7:42 | yes | `portal_0742.jpg` | Rainbow portal glow, "TWO UP" and "PRESS R TO E…" prompt |
| 9:30 | yes | `skybox_rock_0930.jpg` | Ty by the waterfall, red mesa in the sky, pink haze |
| Water flicker when Ty jumps in | **not located** | — | See below |
| Footprints moment | **not located** | — | No timestamp was given and I did not search for it |

**Scan for the blue flicker.** I extracted one frame per second (584 frames) and ranked seconds by blue-minus-red and by frame-to-frame change. The top blue spikes were:
- 4:01 (t=241), the LOADING screen after LOAD GAME at 3:58–3:59
- 5:40–5:45 (t=340–345) and 6:07–6:13 (t=367–373), both dialogue cutscenes with no water

None of these is a water entry. The closest water window is 1:56–2:07, where Ty goes from the cliff to the water, but the jump itself was not visible at 1 fps. A finer scan (2 to 4 fps, 1:50–2:10) would be needed to pin it.

## F11 on/off pairs

Mean absolute difference per pair (sampled pixels, 0–255 scale):

| Shot | Mean diff | % pixels changed (>30) |
|---|---|---|
| 093818 | 2.87 | 6.7 |
| 093904 | 1.10 | 1.2 |
| 094459 | 2.52 | 2.5 |
| 094510 | **9.75** | **27.4** |
| 094533 | 1.46 | 2.6 |
| 094537 | 1.43 | 1.7 |
| 094543 | 2.11 | 3.4 |
| 094559 | 1.18 | 1.3 |

The shader difference is small in seven of eight pairs. The only large one is 094510, an archway view with a gate. Contact sheet: `docs/playtest_frames/f11_on_off_pairs.jpg`.

## Issues

Cause hypotheses point at source names found by search. Nothing here is confirmed by a code path or a debug log.

### 1. Fog over water and lilies (0:15)
- **Seen:** pink haze overlays the far water. Lily-pad shapes are faint. The water itself is also fogged, so "lilies obscured more than water" is **not confirmed** — I did not measure it.
- **Likely pass:** fog / atmosphere (`src/atmos.inl`).

### 2. Water overlay over the jungle (1:56–2:20)
- **Seen:** a semi-transparent water layer over the jungle and waterfall at 2:05. The surface looks like a flat plane.
- **"Breaking geometry" — not confirmed.** Single stills cannot show clipping. A short clip of the same spot would settle it.
- **Likely pass:** water pass. `data/texture_tags.csv` tags `a1_water.dds` as `emissive=no, transparent=yes`, so the water texture is transparent but not glowing.
- **Ignore for this issue:** the Windows system menu at top-left of the 2:05 frame.
- **Debug switch available:** `DebugWater` bitmask in `src/pbr.inl` (issue #5). Each bit turns off one per-frame term for all draws, so one playtest per bit could isolate the source. Not run here.

### 3. Frozen frame at 2:10 and 2:15
- The two frames are byte-identical. This means the recording itself stalls there, so the 2:05–2:20 review has three real frames, not five.

### 4. "Still broken after reload" (4:20)
- **Seen:** the 4:20 frame is a cutscene with no clear break.
- **Load sequence:** LOAD GAME at 3:58–3:59, LOADING at 4:01. The 4:01 blue spike is that loading screen, not water.
- **Not confirmed:** whether the overlay returns after reload. Needs a clip starting at the reload.

### 5. Level-two trees through fog (5:40)
- **Seen:** a dialogue cutscene at 5:40. No tree poking through fog is visible.
- **Not confirmed.** The frame does not show the claimed effect. Level-two footage near the time the playtest mentioned it is needed.

### 6. Portal (7:42)
- **Seen:** a strong rainbow glow with the "PRESS R TO E…" prompt. Brightness is high, but no geometry error is visible.
- **Likely pass:** portal light emission (`src/light.inl`).
- **Not confirmed:** whether the glow is wrong or intended.

### 7. Red rock / skybox sticking out (9:30)
- **Seen:** the mesa behind the waterfall sits in the sky with pink haze over its base.
- **Not confirmed** that it is a skybox error rather than distance fog.
- **Likely pass:** sky / fog (`src/atmos.inl`).

### 8. Water flicker when Ty jumps in
- **Not located.** See the scan above.

### 9. Footprints moment
- **Not located.** No timestamp was given and I did not search.

## Open items

- Finer scan (2–4 fps) of 1:50–2:10 to find the water entry.
- Find the footprints timestamp.
- Short clips (not stills) for: the 2:05 overlay, the 4:20 reload, the 5:40 tree claim. Stills cannot settle these.
- Run the `DebugWater` bisect on the 2:05 spot, one bit per playtest.

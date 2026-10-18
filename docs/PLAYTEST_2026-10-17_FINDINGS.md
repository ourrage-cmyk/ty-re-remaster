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

## Round 2 measurements

Method: frames at 0.5 s steps (2 fps), extracted from `Recording 2026-10-17 094656.mp4` at 320 px for scans and 960 px for the committed copies. Metrics:
- **MAD** = mean absolute per-channel difference against the previous step (0–255).
- **Blue ratio** = B/(R+G+B), over the character box (x 40–60%, y 45–95%) and the whole frame.
- **Changed pixels** = share of sampled pixels with a summed channel difference > 45 against the pre-water frame.

Data: `docs/playtest_frames/data/` (CSV).

### 1. Frame-diff scan, 1:40–2:30
- Median MAD 3.37, sd 12.28, threshold (median + 3 sd) 40.2.
- **Exact-zero frames (duplicates):** 1:58.5–2:02.5 (`q_038`–`q_046`), 2:04.5–2:05.0 (`q_050`–`q_051`), 2:07.5–2:15.0 (`q_056`–`q_071`). **Near-zero (MAD < 0.05):** 2:03.5–2:04.0 (`q_048`–`q_049`), 2:06.5 (`q_054`, 0.10). Small but non-zero: 2:05.5 (0.93), 2:07.0 (0.87). Non-zero jump at 2:06.0 (`q_053`, 4.10). 2:16.0 onward mostly non-zero. 2:10 (`q_061`) has MAD 0.87, near-duplicate. 2:15 (`q_071`) is exact zero.
- **Jumps above threshold:** 1:57.0 (`q_035`, MAD 73.4, the cliff-to-water cut); 2:18.0 (`q_077`, MAD 42.9); 2:18.5 (`q_078`, MAD 42.6). The 1:46.5 step (`q_014`, MAD 48.5) is also above threshold but is outside the 2:10 focus and was not checked.

### 2. Is the stall at 2:10 and 2:15 in the recording or the game?
- Both steps are duplicate frames in the recording file itself. Blue ratio and luminance are constant across the stall (`lumAll` 385.6–385.7).
- **Clock test could not be run.** The "separate game capture" starting at 57 s is not on disk. The only 17 October recording in `C:\CLAUDE` and the Videos folder is `094656.mp4`. Without the capture file there is nothing to compare hypothesis (a), capture = rec − 57 s (2:10 → 1:13), with hypothesis (b), capture = rec + 57 s (2:10 → 3:07).
- **Answer:** the stall is present in the recording file. Whether the game froze or the capture duplicated frames cannot be told apart from this file. Not confirmed.

### 3. Water entry
- **Entry into water: 1:57.0** (`water_entry_0157.jpg`, frame `q_035`). Blue ratio on the character box jumps from 0.15 (1:56.5) to 0.26 (1:57.0) and holds at 0.26 through 2:10. Whole-frame luminance roughly doubles at the same step (171 → 386). The jump is a full-frame change, so it is the scene transition, not only Ty.

### 4. Overlay breaking
- Against the pre-water frame (1:56.5, `q_034`), **~78% of sampled pixels change from 1:57.0 onward**, across all cells of the 3×3 grid (the change is whole-frame, not localised).
- The change is constant from 1:57.0 to 2:15.0 (77.8–78.0%).
- **A second step at 2:15.5–2:16.0:** 80.6% at 2:15.5 (`q_072`), 84.4% at 2:16.0 (`q_073`, shown as `overlay_step_0216.jpg`), rising to 93.7% at 2:18.0 (`q_077`). This is the break point. It is after the 2:05 reference, not at 2:05.
- The Windows system menu at top-left (2:05) was excluded from the count.

### 5. Trees near 5:40
- Scan 4:40–6:40 at 0.5 s: 240 frames, **55 cutscene frames** (letterbox bars or subtitle text) and 185 non-cutscene frames. Non-cutscene runs (outdoor gameplay, after the subtitle check):
  - 4:40.0–4:44.5 (10 frames)
  - 4:53.0–4:53.5 (2)
  - 5:04.0–5:24.5 (42)
  - 5:25.5–5:30.0 (10)
  - 5:32.5–5:39.0 (14)
  - 5:44.5–6:07.0 (46)
  - 6:08.5–6:12.0 (8)
  - 6:14.0–6:39.5 (52)
- **5:40.0 is a cutscene** (subtitled dialogue, `t_121`), not outdoor gameplay. The nearest outdoor scene is 5:32.5–5:39.0. The outdoor 5:35 frame is `outdoor_trees_0535.jpg`.
- Fog on trees, rough (green-vs-red colour split, not a real segmentation, so treat as indicative):
  - 5:04–5:24 scene: crown lum 148, terrain lum 63, fog colour ~(139, 174, 210).
  - 5:32.5–5:39 scene: crown lum 85, terrain lum 116, fog ~(104, 102, 111).
  - 5:44.5–6:07 scene: crown lum 106, terrain lum 97, fog ~(89, 100, 78).
- **Not confirmed:** whether the tree crowns are fogged more than the terrain. The segmentation does not isolate crowns cleanly, so these numbers do not settle the claim.

### 6. Named moments
- **7:42 portal** (`portal_0742.jpg`): glow area 6.2% of frame with luminance > 200. Peak luminance 253.8. Mean glow luminance 220.4.
- **9:30 rock** (`skybox_rock_0930.jpg`): mesa top RGB (135, 84, 78), lum 98.6. Fog at mesa base RGB (112, 72, 58), lum 82.0. **Colour delta 33.4** (RGB Euclidean). Sampling boxes were placed by eye, so the delta is indicative.
- **0:15 lilies** (`fog_water_0015.jpg`): not separable. Luminance thresholds either pick the whole water (lum > 150 covers 67% of the band) or nothing (lum > 170 covers 0%). Far-water mean lum 151.0, sat 0.377. Background rock lum 70.8, sat 0.383. **Lily-pad luminance and saturation not measured.**

## Open items

- Lily-pad segmentation at 0:15 needs a colour-based mask (the pads are light green-white on blue-grey), not a luminance threshold.
- The 2:16 break needs a clip, not stills, to tell a real geometry break from a scene change.
- Whether the 2:10–2:15 stall is in the game or the capture needs the separate 57 s capture file. It is not on disk.
- Tree crown fog needs a proper segmentation (or a clip), not the green-vs-red split used here.
- Footprints moment: still not located (round 3 blob scan is not a usable detector, see below).
- Run the `DebugWater` bisect on the 2:05 spot, one bit per playtest.

## Round 3 measurements

Data: `docs/playtest_frames/data/r3_*.csv`. Frames at 960 px, quality 70, in `docs/playtest_frames/`.

### 1. Change classification, 1:50–2:20 at 0.25 s (measured, heuristic)
- Per 3×3 cell, mean absolute gray difference > 8 counts as changed. Edge structure uses Canny (100/200) on a 480×270 frame, with Jaccard distance of edge maps.
- Full-frame (≥7 cells changed, edges moved): **1:52.75, 1:57.25, 2:15.50, 2:18.25, 2:18.75**.
- Mixed: 1:50.75, 1:53.25, 2:15.75.
- Regional (1–4 cells changed, edges stable elsewhere): 1:52.00, 1:53.75, 1:54.00, 1:56.00, **2:03.25**, 2:06.25, **2:16.00** (one cell, centre-left).
- **The 2:16 step:** 2:15.50 full-frame, 2:15.75 mixed, 2:16.00 regional (one cell), 2:16.25 and 2:16.50 no change. It is a half-second event, not a sustained geometry break.
- Caveat: a camera pan also changes the whole frame, so "full-frame" means "whole image moved", not "scene cut". 1:57.25 is the water entry seen earlier.
- Frames: `event_full_frame_0152.jpg` (1:52.75), `event_regional_0203.jpg` (2:03.25), `event_full_frame_0218.jpg` (2:18.25), plus `water_entry_0157.jpg` and `overlay_step_0216.jpg`.

### 2. Lily-pad green-mask test, 0:10–0:25 (measured, inconclusive)
- Thresholds: OpenCV HSV, hue 35–85° (17–42 in OpenCV units), S > 0.25 (64 of 255). Water box x 35–80%, y 30–45%. Rock box x 0–25%, y 0–30%.
- Far-water box at 0:15 (`fog_water_0015.jpg`): **0% pad pixels.** The hue is 8–18° across the whole box, so the fog tint puts the pads outside the green range.
- Medians over 61 frames: water mean V 0.755, mean S 0.379. Rock mean V 0.341, mean S 0.379, green share 19%.
- Pad-mask V and S are **not reported** because the mask is empty. **Not confirmed.** The measure needs a fog-compensated colour space or a manual pad mask.
- Table: `data/r3_item2_lily_0010-0025.csv`.

### 3. Footprint blob scan, full recording (measured, not usable as a detector)
- 1168 samples at 0.5 s. Cutscenes skipped (letterbox or subtitle band): 187. Outdoor samples: 981.
- Blob rule: V ≥ 0.9, S < 0.15, 200–3000 px at 960 px width, centroid stable within 8 px of the previous sample.
- 3454 candidate blobs, 1397 stable with the previous sample, **381 timestamps** flagged.
- Stable runs cover long spans: 0:44.5–0:47.0, 1:39.0–1:46.5, 1:50.5–2:16.0, 2:32.5–3:37.5, and more. Long static runs of this kind come from bright static surfaces (sky, white walls), not decals.
- **Not confirmed:** no footprint is identified. The blob rule cannot tell a decal from a static bright surface. Table: `data/r3_item3_blobs_outdoor_0.5s.csv`.

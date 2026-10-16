# Playtest problem index (status 2026-10-13 19:23, after playtest 4; v0.3.3 test build in the game folder)

One line per problem id from the four user playtest reviews. Details, timestamps and causes: `FINDINGS_AND_PLAN.md` (round 1, build v0.3 Balanced, ids F1-F12), `FINDINGS_ROUND2.md` (round 2, v0.3.1 Ultra, ids G1-G10), `FINDINGS_ROUND3.md` (round 3, v0.3.2, ids W1, S1, A1, E1 and the requests; its two feature ids are written **R3-F1** and **R3-F2** here to avoid a clash with round 1 F1/F2), `FINDINGS_ROUND4.md` (round 4, playtest 4 on v0.3.2 High/Ultra, ids R4-1 to R4-9 here). Todo ids are in `todo.json`; current open work: `..\RESUME.md`; order: `..\PLAN_NEXT.md`. Status: fixed / partial / open. "Build" = the first release that contains the fix. "partial" = a change is in the test build (v0.3.3) or the cause is only partly known; the user has to confirm.

Builds: v0.3 (preview, Balanced), v0.3.1 (recording round: albedo relief, sky haze, colour casts, characters excluded), v0.3.2 (flicker, halo, fog coherence; shipped, current release), v0.3.3 (test build, not released: texture tags, GI source mask and blur; installed in the game folder, G1 not run).

## Round 1 (F, playtest 1, v0.3)
| id | problem | status | build | note / todo |
|---|---|---|---|---|
| F1 | sky/skybox (painted mountain) not hazed | fixed | v0.3.1 | T53: aerial perspective on sky pixels, `SkyHaze`, `SkyHazeHeight` |
| F2 | ground tessellation noise unrelated to the texture | fixed | v0.3.1 | T52: relief derived from albedo; user round 2: floor "amazing" |
| F3 | rope still 2D | partial | v0.3.1 | rope is a flat asset (user accepted in round 2); close-up crop T49 open |
| F4 | colour casts (purple, green, glowing trees/kangaroos) | fixed | v0.3.1 | T54 neutral shade tint; confirmed round 2 |
| F5 | grass has no discernible shadow | open | - | low priority, accepted; `GrassStrength` key exists |
| F6 | BLI BLI sign letters not embossed | fixed | v0.3.1 | letters slightly recessed (round 2); playtest 4: sign is tessellated (good) |
| F7 | shadows too soft, corners not darker | partial | v0.3.1 | T55 wide AO radius (`WideRadius`); no further complaint in rounds 2-4 |
| F8 | corrugated steel, gate, fence flat | fixed | v0.3.1 | round 2: steel and wood good |
| F9 | portal glow lights Ty but not the ground | open | - | see G6 and R4-6 (T56/T65) |
| F10 | derive height from texture colour | fixed | v0.3.1 | implemented as F2 (runtime, no baking) |
| F11 | do not tessellate characters (ridges, aliasing) | fixed | v0.3.1 | T51: characters excluded; Scope default `env-detail`; v0.3.3 also uses texture tags (T80) |
| F12 | machine/computer "deep fried" | fixed | v0.3.1 | round 2: computer, bird, machine good |

## Round 2 (G, playtest 2, v0.3.1 Ultra)
| id | problem | status | build | note / todo |
|---|---|---|---|---|
| G1 | flicker everywhere (AO blocks, GI disco ball, shadow shifts) | partial | v0.3.2 | fixed on Medium (playtest 3 "no flicker"); High/Ultra flicker again in playtest 4 (R4-1, T78) |
| G2 | Ty's red trousers glow on the ground | fixed | v0.3.2 | T63 character GI mask (`[gi] CharacterColour=0`) |
| G3 | fog vs AO fight; emissive (cave fire) outside the fog | partial | v0.3.2 | AO `FogFade=1` fixed; **fire still not hazed** (playtest 4, R4-4, open) |
| G4 | far trees grey while sky is blue | partial | v0.3.2 | `HazeSky=0.6` (haze follows sky colour); `DepthBlur` optional, off by default |
| G5 | shadow white outlines, bands, over-sharpening | partial | v0.3.2 | `SharpenHaloClamp=1`, shadow grid filter; residuals are S1 and the fern rim (A1); koala stair steps and fence-post outline not reproduced |
| G6 | portal and red crystal emit no light on surroundings | open | - | T56 / T65, design exists (half-res additive spill), v0.4; see R4-6 |
| G7 | walls, rocks, bags, posts have minimal relief | open | - | T65; `DetailStrength` tuning, env relief optional (+6 ms); bag: see R4-5 |
| G8 | Billabong fog looks like a swamp | open | - | per-level fog tune; Medium/Ultra pink-purple haze seen again in the A/B shots |
| G9 | atmosphere gives a "sunset evening" look under a blue sky | partial | v0.3.2 | Intensity 2.2 written for Medium (T48 confirm open); slider exists |
| G10 | leaves look translucent through other leaves | open | - | not reported again; unverified |

## Round 3 (playtest 3, v0.3.2)
| id | problem | status | build | note / todo |
|---|---|---|---|---|
| W1 | water reflection pulses Ty's brightness ("disco ball") | open | - | T74 (not started). The blue flicker on Ty from water is R4-2 |
| S1 | grid and 1 px bright line / double edge in character shadows | open | - | T75 / T82: v0.3.2 reduced the grid; the bright lines are still there in playtest 4 (R4-3); root cause not found |
| A1 | AO halo behind the character; white rim on fern shadows | open | - | T73 (not started) |
| E1 | footprints glow (almost white texture reads emissive) | open | - | T77 / T80: the texture-name tag system is in v0.3.3 (emissive flags from `texture_tags.csv`); footprints still reported glowing in playtest 4 (R4-7); cause unknown |
| R3-F1 | AO should fade out with distance (slider) | open | - | T73 (not started); playtest 4 asks for it again to stop far mountains flickering (R4-8) |
| R3-F2 | relief and all effects: precision drop-off with distance | open | - | T76 (not started) |
| launcher | streamline for non-technical users | fixed | v0.3.2 | T72: simple mode (presets, impact, Play, "More options >>") |
| A/B shots | community vanilla/Low/Medium/Ultra single images | fixed | - | T71: `AB_shots\` (level 1 and 2), copy on `E:\TY_AB_shots` (80 images) |
| save everything | step logs, RESUME, OpenViking after every step | fixed | - | T70; docs pass 2 (2026-10-13 evening) |

## Round 4 (playtest 4, v0.3.2 High and Ultra; "Recording 2026-10-13 155438.mp4")
Video times in brackets. Details: `FINDINGS_ROUND4.md`.
| id | problem | status | build | note / todo |
|---|---|---|---|---|
| R4-1 | **High and Ultra flicker** (Medium stable) | partial | v0.3.3 test | T78 / T81: GI blur, noise cycle, temporal blend; GI source mask. Contact shadows alone only +0.035 flicker. **Needs user playtest on v0.3.3** |
| R4-2 | water flickers blue on Ty every frame (GI samples water) [3:18, 3:48] | partial | v0.3.3 test | T81: water and other transparent/emissive-tagged draws excluded from GI (`[gi] ExcludeSources=3`). Confirm by playtest |
| R4-3 | jagged bright lines on shadow edges of the face [0:21] and the bag [1:30] | open | - | T82 reported done but not fixed; root cause not found. Re-open for T75 |
| R4-4 | fire (right side) not affected by the haze [0:52] | open | - | G3, atmos work |
| R4-5 | bag not tessellated; some items tessellated, some not | partial | v0.3.3 test | T80: character textures are not tessellated, all others are (206 tagged). Bag crops not verified |
| R4-6 | portal light lights Ty but not the ground; emissive textures should spill light | open | - | F9 / G6, T56 / T65 (v0.4) |
| R4-7 | footprints glow at 2:15 | open | - | same as E1 / T77; cause unknown |
| R4-8 | AO fade from the character outward so far mountains do not flicker [4:40] | open | - | same as R3-F1 / T73 |
| R4-9 | Ultra is "slow-motion" (frame rate) | open | - | T76 (distance LOD); High GI cost +2.5 ms in v0.3.3 to optimise |

## Other files here
`texture_tags.csv` (user-filled emissive/transparent/character flags for 1933 textures), `mp3*.txt`, `voice3.txt`, `voice4.txt`, `transcript.txt`, `voiceover.txt` (transcripts), `key\`, `key2\`, `key3\`, `frames\`, `sheet*.jpg` (key frames), `*.json`/`*.log`/`seg1.raw`/`voice*.wav|ogg|mp3` (raw audio and tool outputs, not documentation), `FINDINGS_ROUND3.md.bak_*` and `FINDINGS_ROUND4.md.bak_*` (backups).

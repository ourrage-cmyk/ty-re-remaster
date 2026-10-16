# Playtest 4 (v0.3.2, Ultra mostly) — transcript voice4.txt, video "Recording 2026-10-13 155438.mp4" (key frames key3\*.jpg at VIDEO times)
Status 2026-10-13 19:23: changes for these items are in the v0.3.3 test build (texture tags T80, GI source mask and blur T81, light debug keys T82). Not confirmed by the user yet; the test build still needs a playtest. Current ids and status: `INDEX.md` (Round 4 section, R4-1 to R4-9). The text below is the playtest record and is unchanged.
User: "MEDIUM IS STABLE, HIGH AND ULTRA ARE FLICKERING" (High = Medium + GI 1 ray + contact shadows => suspects GI, contact shadows). "I would honestly be playing Medium rather than Ultra". Performance on Ultra is slow-motion.
Items (video time): 
- 0:36 little flicker at start.
- 0:21 face close-up: jagged BRIGHT lines from shadow edges on the character's face (S1). 1:30 same on the BAG shadow lines.
- 0:52 FIRE (right side) not affected by haze (G3).
- Billy Bli sign IS tessellated (good); the BAG is NOT tessellated -> inconsistent: some items tessellated, some not. REQUIRED: textures flagged character=yes (texture_tags.csv) NOT tessellated; ALL non-character textures tessellated.
- Portal casts light on Ty but not the ground; emissive flags in texture_tags.csv must be used: emissive textures emissive (light spill), non-emissive not. Footprints at 2:15 glow (must not).
- 3:18 and 3:48 WATER: Ty barely moves but flickers EVERY FRAME like a disco ball, blue: water = two textures moving in different directions, emitting light; "water should not be reflecting the GI or at least not flickering blue on the character". => GI sampling water (flicker source for High/Ultra too).
- AO fade from character outward so far mountains (4:40) do not flicker (AO vs haze).
Plan: T78 GI/contact bisect+fix (agent gi), T80 texture tags -> per-draw tess/emissive (agent tess), S1 shadows (agent light), then AO fade/halo (T73), fire/emissive haze + contact (atmos).

## Playtest 5 (user, evening, v0.3.3 test build; video 'Recording 2026-10-13 193524.mp4' 3:37; contact sheet recording\sheet5.jpg; zoom image recording\shadow_line_zoom.webp)
- UGLY CHECKERBOARD PATTERN on shadows; GHOSTING on grass; Ty's PANTS still tint the GROUND red; shadows STILL have the ugly LINE through them: zoom shows a dotted/stair-stepped string of brighter pixels (shadow-map texel size) running diagonally INSIDE the shadow region, on the red cloth and on the orange ground along a shadow contour (looks like a light-leak / terminator crack, not the penumbra).
- REQUEST: fine-tune colours so game BRIGHTNESS and SATURATION match the ORIGINAL game (same mood). Lead does this via A/B metrics (vanilla vs Medium) -> post defaults.

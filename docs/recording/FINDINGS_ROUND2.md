# Playtest 2 (Ultra, v0.3.1 build) — findings + plan (2026-10-13)
Sources: C:\CLAUDE\RECORDING\261013_1128.mp3 (voiceover, transcript recording\mp3_2.txt), Recording 2026-10-13 114152.mp4 (12:45; key frames recording\key2\*.jpg). Level 1 (Rainbow Cliffs) + level 2 (Billabong).

## What got better (user words)
Tessellation on the floor "looks amazing", wood "amazing", purple cast gone, trees/grass no longer glow, characters don't glow, computer/machine/bird look good, corrugated steel good, lighting "a lot better than the original", GI now darker left / brighter right "exactly what we wanted", Ty casts a shadow, sign letters slightly recessed.

## Problems (priority order)
| # | t | Issue | Likely cause / idea |
|---|---|---|---|
| G1 | throughout | **FLICKER everywhere** ("most annoying thing"): AO under rocks/grass in big noisy blocks, GI makes Ty "a disco ball", far rocks go up/down in brightness, cliff edge shadow shifts every second, shadow under barbed wire shifts, ghosting | per-frame noise (AO Hilbert/R2, GI rays, froxel jitter, shadow IGN rotation `frame%64` = ~1 s cycle!) with weak temporal accumulation/denoise; worse at 30 fps (Ultra) |
| G2 | 4:22, 8:25 | **Ty's red pants emit red glow on the ground** (flickering); same from the portal light on his trousers | GI colour bleeding from the character into the floor; character should not bleed (or only weakly) |
| G3 | 2:40, 6:30 | **Fog vs AO fight**: things behind fog have darker AO shadows than things in front -> outline behind items; cave fire sticks out of the fog (not in the same pass) | AO not attenuated by fog/distance; particles/emitters drawn after the post pass get no haze |
| G4 | 0:30, 1:00, 11:27 | **Far trees are grey while sky is blue**; trees sharp -> wants depth blur / more coherent aerial perspective | haze desaturates toward grey; add distance blur (cheap DoF-ish) + haze hue toward sky colour |
| G5 | 5:25-5:45, 7:15, 10:45 | **Shadows have white outlines/bright-dark-bright-dark bands**, pencil shadow on koala very sharp/blocky, brighter outline on fence post, dark outline around leaves ("over sharpened"), grain/ghosting in shadows | sharpen halo + shadow filter edge; check CAS strength, shadow normal-offset/bias, TAA ghosting |
| G6 | 3:19 | Fire/emissive: portal and red spinning crystal emit no light onto surroundings | (design exists: light agent REPORT F9, half-res additive spill) |
| G7 | misc | Rope no tess benefit (2D asset, accepted); wall/rock/bag/post minimal relief ("could be stronger"); texture tess only on some assets | tune DetailStrength up on props/walls; env tess optional; |
| G8 | 11:48 | Billabong: fog "makes it look like a swamp"; mist over water hides details ("improve clarity") | fog intensity per level / height fog tuning; user-configurable already (Distance haze slider) |
| G9 | 2:14 | Atmosphere gives a "sunset evening" look while sky is blue | haze colour warm vs blue sky; expose slider (exists), consider lower default |
| G10 | 7:13 | Leaves look translucent/glowing through other leaves though not emissive | alpha/foliage shading interplay |

## Plan (todo T60+)
1. T60 (done by lead) **Measurement first**: motion-stability harness (DLL `motion.req` capture: smooth yaw per frame, 16 frames, temporal 2nd difference) + static `flicker.ps1`; attribute flicker per pass (AO / GI / fog / TAA / shadows / contact) vs off baseline. Results -> results\mo_*\motion.md.
2. T61 agent **stability**: fix the flicker sources found by T60 (AO denoise + temporal with reprojection and clamp; GI temporal stability and character-bleed suppression; froxel temporal jitter / blend; shadow filter rotation not periodic + TAA-aware; sharpen halo). Acceptance: motion metric <= 1.5x of 'off' for each pass, no visual softening regression, cost +<=1 ms.
3. T62 agent **fog coherence**: AO fades with fog transmittance/distance; haze applied to emissive/particle draws (cave fire) or they are drawn into the fogged buffer; far-tree grey -> hue toward sky, optional distance blur; Billabong swamp fog tuning.
4. T63 lead/agent **GI character bleed**: exclude skinned pixels as GI sources (depth/stencil mask via the existing skinned-shader tag) or reduce their albedo contribution; kills the red pants glow + disco ball.
5. T64 agent **shadow/sharpen halos** (G5) incl. koala blocky shadows.
6. T65 (later) emissive light spill (G6) using the light agent's design; detail strength on walls/props (G7).
Constraints as before (Medium <= 16.7 ms; fallback = off; no regressions; resumable logs).

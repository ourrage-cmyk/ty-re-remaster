# User screen recording review (2026-10-13) — findings + plan
Sources: C:\CLAUDE\RECORDING\261013_0724.mp3 (voiceover) and Recording 2026-10-13 073228.mp4 (8 min). Transcript with timestamps: recording\mp3.txt. Key frames: recording\key\*.jpg (named <topic>_<seconds>s). Build under test = v0.3 Balanced.

## Findings (timestamp -> what the user says -> what the frame shows)
| # | t | Issue (user) | Evidence in frame | Cause (our code) |
|---|---|---|---|---|
| F1 | 0:08 | Atmosphere is nice, but the **skybox is not affected**: the mountain painted on the sky stays saturated red while the world in front is hazy/white -> breaks the illusion | key\sky_8s: Uluru-like mesa is strong red, trees in front are washed out | froxel/aerial fog only touches pixels with geometry depth; sky pixels (depth=1) get no haze/desaturation |
| F2 | 0:35 | Ground tessellation/noise is **unrelated to the underlying texture** (cracks, pattern) | floor_38s: uniform fuzzy grain, the cracks/lines of the level texture are not grooves; rope_66s: real cracks are flat paint next to noise | detail layer = procedural noise only weakly "driven" by texture luminance |
| F3 | 1:05 | **Rope** still looks 2D, no relief | rope_66s | detail only on ground-like vertex-lit shaders; no albedo-derived relief on props |
| F4 | 1:28-1:45, 2:35 | **Colour casts**: green on bridge, red/purple on Ty, **trees and kangaroos look like they glow**, purple everywhere | tree_100s purple trunks + cyan leaves, sign_130s purple/red | [light] shade tint (cool violet) + foliage warm/transluscent boost + hemisphere; also fog hue |
| F5 | 1:51 | Grass has no discernible shadow | tree_100s | acceptable, low priority |
| F6 | 2:05 | **BLI BLI station sign**: no relief; the painted letters should be embossed | sign_130s | height from albedo missing for wood/props |
| F7 | 2:44-3:19, 6:01-6:30 | Shadows too soft / no real dark spots; **corners not darker**, only a thin AO line, all purple | sign/corner frames | AO strength/radius too small for large-scale occlusion, GI/ambient fill + shade fill lift darks |
| F8 | 3:31-4:26 | **Corrugated steel, wooden gate, fence**: rust texture has noise unrelated to colour, zig-zag steel is flat, wood flat | steel_213s, gate_243s | no geometric/normal detail from albedo; engine meshes flat |
| F9 | 4:35 | **Portal glow** lights Ty but not the ground around it | portal_275s, char_330s | glow is a sprite/object light on skinned only; no light spill onto world |
| F10 | 5:03 | Suggests: **bake/derive height from the texture** (RGB -> height map), in real time or with an offline script | | |
| F11 | 5:20-6:00, 6:37 | **Do not tessellate the character**: ridges/wrinkles from painted shading gradients, weird textures; **aliasing** on Ty | char_330s, computer_430s koala | Scope=skin tessellates characters + detail layer applied to them |
| F12 | 7:00 | **Machine/computer looks deep-fried** (noise/grain everywhere); only the texture's own colour/paint should define height | computer_430s | noise detail layer applied to props/characters |

## Plan (new todo items T51..T58, see todo.json)
1. T51 (tess agent) **Characters excluded** from tessellation + detail layer (skinned draws: no tess, no noise); remove "skin" scope meaning; keep env-only. Fix Ty aliasing check (TAA jitter/edge on skinned) — verify with ty_strip.
2. T52 (tess agent) **Albedo-derived height detail** (replaces noise): per-pixel height from albedo luminance (multi-scale, local-contrast normalised, dark = recessed incl. cracks, painted lettering/rope strands = raised or recessed by luma), turned into per-pixel normal/relief (parallax-occlusion style steps, cheap) + inward-only tess displacement; NO free-running noise except a very faint tie-breaker; apply to ground, wood, rope, sign, steel; not characters/NPC (koala, bird) or UI/particles. Offline option if real time is too weak: texture pre-pass script that bakes height into a sidecar (decide after prototype).
3. T53 (atmos agent) **Sky haze**: apply aerial perspective to sky/depth==1 pixels (horizon band desaturate + lift, mountains painted on sky) and retune Intensity (T48 folded in): far rock and painted rock match; no white wash on near walls.
4. T54 (light agent) **Colour casts**: shade tint much more neutral/warm-complementary (not violet), foliage no glow/lift, hemisphere subtler, per-material guard (no tint on characters); A/B vs recording frames. Also make shade darker where it should be (corners).
5. T55 (light agent, same session) **Darker corners/real shadows**: AO radius/strength for large scale occlusion (second wide AO radius?), shadow penumbra tighter, ambient fill lower in occluded areas; keep Balanced <= 16.7 ms.
6. T56 (lead, later) **Portal/emissive light spill**: evaluate (a) classify emissive sprites, (b) cheap screen-space glow tint around portal (additive coloured light on ground within radius). Research first; may defer to v0.4.
7. T57 (auto) Re-run G1 + t41 + new "recording views" suite (cams matching sky_8s, floor_38s, rope_66s, sign_130s, corner_187s, gate_243s, computer_430s) A/B vs v0.3.
8. T58 (lead) merge, package v0.3.1, director checkpoint, user sign-off.
Constraints: Balanced <= 16.7 ms (now 11.1), fallback = OFF, original TY.exe untouched, resumable step logs.

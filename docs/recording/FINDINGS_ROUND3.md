# Playtest 3 feedback on v0.3.2 (user, 2026-10-13) — SAVED VERBATIM-ish, then plan
Files: recording\texture_tags.csv (1933 textures: emissive / transparent / character flags — user filled), recording\voice3.ogg (+ transcript voice3.txt).

## User feedback (v0.3.2)
- "So far it looks stable. There is no flickering which is amazing." AO "looks amazing", fog "looks amazing", lighting/rendering "a lot better than the original", water reflections of the character "look really good".
- BUG W1: when Ty jumps down into the WATER, the water reflects back to him: water = 2 textures overlaid moving in different directions; Ty's BRIGHTNESS goes up and down continuously ("disco ball") from the light reflecting on the water shader (whatever reflects). Not ideal.
- BUG S1: harsh lines on character shadows: a grid on the character shadow (low-res), soft outer edge but then a ONE-PIXEL-WIDE string of brightness running through it = looks double-edged/unnatural. Source unknown.
- BUG A1: AO halo behind the character: camera is behind Ty, AO is projected away from the camera, so there is ALWAYS a shadow halo on the side of Ty away from camera regardless of light direction (e.g. boomerang hand); standing still in a field with sun overhead there should be no AO stretching behind him "into infinity". "Looks like a 2D shader on top of a 3D game." Fern shadows on ground have soft edge + a WHITE RIM.
- BUG E1: footprints light up (texture almost white -> reads emissive/glowing). User supplied texture_tags.csv saying which textures should / should not glow.
- FEATURE F1: AO should FADE OUT with distance from the character/camera outward (strength 1 at character -> 0 at farthest point), long smooth gradient so nothing pops; avoids AO flickering/fighting with fog in the distance; make it a SLIDER (max AO distance / fade).
- FEATURE F2: same for tessellation/relief: fade smoothly with distance (save compute, lower frame time; walk up -> detail appears). General principle: precision drop-off with distance for ALL effects (AO, tess/relief, shadows, fog steps, GI) = precise close, cheaper far. Investigate savings.
- Minor: shadows on characters (grid), over-sharpened items earlier.

## Additional requests (same message)
1. LAUNCHER: streamline for non-tech-savvy people: more minimal in layout and scope.
2. SAVE EVERYTHING ALWAYS (usage limit will stop agents): step logs, RESUME.md, OpenViking after each step.
3. COMMUNITY A/B SHOTS: compile best A/B shots in both save states (different levels): vanilla, Low, Medium, Ultra side by side; but save as SINGLE IMAGES in a NEW FOLDER (one file per preset per view, same camera, identical naming) so the user can load them in a swipe/compare tool pixel by pixel.

## Plan (todo T70+)
T70 save+docs (done now). T71 A/B community shots (programmatic, zero tokens; level 1 now; level 2 needs slot-2 boot support). T72 launcher streamline (lead). T73 agent AO: distance fade slider (F1) + character not an AO occluder/halo (A1, GI-style capsule mask) + footprints/emissive via texture tags (E1: needs per-texture hook; check if feasible: shader sees only sampler, not name -> engine texture upload hook by name?). T74 agent: water/character brightness pulsing (W1) - find the water reflection shader + Ty shader interplay (envmap/light flicker; probably a time-varying uniform on water pass affecting skinned draws or our PBR/specular on Ty from the water envmap). T75 agent shadows: one-pixel bright line + grid on character shadows (S1) and white rim on fern shadows. T76 agent tess/LOD: distance fade of relief/tess + other effects precision drop-off, measure savings. T77 integrate texture_tags.csv into emissive handling (glow list) if a hook exists.

## Status (docs pass 2026-10-13, after v0.3.2 release)
- Shipped as v0.3.2 with these changes aimed at round 2-3 items: AO temporal mode and FogFade (flicker, AO vs fog), shadow grid filter and SharpenHaloClamp (shadow lines, halos), HazeSky (grey far trees), character GI mask (red trousers glow). The user reports no flicker in v0.3.2.
- Still open: W1 water pulsing on Ty (T74); S1 character shadow double edge and grid (T75); A1 AO halo behind Ty and fern white rim (T73); E1 footprints glow (T77); F1/F2 distance fades (T73, T76); launcher streamline (T72); community A/B shots (T71); emissive light spill (T56 / T65).
- Current status: `RESUME.md`, `STATUS_2026-10-13.md`.

## Update (user, later same day): on ULTRA the FLICKERING WAS BACK (Medium/High-ish v0.3.2 was stable). Ultra differs: AO 4x8, GI 2 rays (Hi-Z), fog 32 steps, contact shadows 0.35, tess Scope=all L24, shadows 8192 x32 taps (Grid 4x4), haze 2.1. Suspects: GI 2 rays noise/temporal, contact shadows jitter (ContactStable), tess world relief shimmer (Scope=all), AO WideSlices/NoiseFrames. TODO T78: bisect on the Ultra ini with motion.ps1/walk tests + user-like low fps (sleepMs emulation in motion.req), fix, user retest. Also: copy all A/B shots to the Samsung T7 USB drive when done.

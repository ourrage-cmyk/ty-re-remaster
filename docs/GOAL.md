# GOAL (north star) - TY the Tasmanian Tiger HD renderer
Status 2026-10-13 (evening): v0.3.2 released as preview / community beta (acceptance partly met; Medium about 12 ms); v0.3.3 test build installed for playtest (not released; High/Ultra flicker open, T78). Read "Balanced" below as Medium; stock frame measured 5.7 ms (4.6 ms was an older view). Current state: RESUME.md. Text below is unchanged.
Owner: the user. This file is the fixed reference every task, merge and review is judged against. Do not edit it except to record a decision the user made (dated, in "Decisions").

## The goal in the user's words
- A drop-in mod for TY HD (PC, 32-bit OpenGL) that makes it look modern: ambient occlusion, better shadows, AA, GI, atmosphere, tessellation + PBR - on **AMD, NVIDIA, Intel**, Windows (and ideally Linux/Proton), shareable with the Ty modding community.
- The user's HD texture pack is separate; our job is rendering.
- **Tessellation / relief:** fine-grained on the colour and drawn detail of the textures so the ground has more profile than flat, giving relief and roughness - **without sinking characters' feet into the ground.** Reference materials: the **sand ground, the wooden fence, the ropes.**
- **Atmosphere:** more intense, so that rocks farther away are visibly less saturated.
- **Lighting:** more natural.
- **Performance:** a playable preset: **60 fps at 2560x1440 on an RX 7600 XT** (frame budget 16.7 ms; stock engine frame is 4.6 ms, so about 12 ms for all effects). Ultra may be heavier, but every option shows its GPU cost in the launcher.
- Work must be **resumable** (survives usage limits), mostly **programmatic** (scripts do measuring/testing/merging; LLMs only for real coding and judgement), and **logged** (research folder + OpenViking, every step).

## Acceptance (what "done" means)
1. **No regressions**: characters (Ty, NPCs) render correctly; no flicker/ghosting; no crash over a 10-minute session; shaders that fail to compile fall back to the original.
2. **Performance**: the "Balanced" preset measures <= 16.7 ms total frame (GPU timers, test spot, 2560x1440, RX 7600 XT). Each effect's cost measured alone and in the preset, published in PERFORMANCE.md and shown in the launcher.
3. **Look** (verified on full-size screenshots, in-session A/B from the same camera): sand shows real cracks/profile at grazing angles; fence wood grain and rope strands read as relief; feet rest on the ground; far rock is clearly less saturated/hazier while near objects are not; shadows are tinted/soft with no stipple; GI/AO/shadows not noisy.
4. **Portability**: runtime feature probes + fallbacks (GL < 4.3 etc.); no vendor-specific calls without guards. (Tested hardware so far: RX 7600 XT only - state this honestly in the README.)
5. **Package**: zip with dll, launcher, ini, README, PERFORMANCE.md; original TY.exe never modified; the patched copy is Large Address Aware.

## Non-goals (do not spend tokens here)
Baking textures; replacing the engine's draw submission (AZDO); Vulkan/DX ports; changing game logic; HD texture authoring.

## Decisions (append only)
- 2026-10-12: user wants the lead to own the plan; Haiku collects/researches; a stronger "director" model reviews progress against this file at checkpoints.
- 2026-10-13: user-facing preset names are Low / Medium / High / Ultra. "Balanced" is retired; Medium is the old Balanced and the default (measured about 12 ms at 2560x1440 on RX 7600 XT, budget 16.7 ms). v0.3.2 released as preview / community beta; G1 gate passed.

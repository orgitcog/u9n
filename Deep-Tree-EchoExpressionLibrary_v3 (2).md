# Deep-Tree-Echo Expression Library for Unreal Engine

**Version 3.0**

## Foreword: Implementation Priority

This document provides a comprehensive expression library for the Deep-Tree-Echo avatar, with a **primary focus on the high-fidelity, photorealistic cyberpunk/biopunk implementation for Unreal Engine**. 

The anime-style variants, while analyzed, are designated as **fallback assets for graphically restricted environments**. Their implementation should be limited to basic mouth movements for speech and occasional blinks to conserve resources.

---

## Part 1: Photorealistic Avatar Expression & Behavior

This section details the core visual and expressive characteristics of the primary avatar.

### 1.1. Core Visual Identity (Biopunk)

- **Hair**: Platinum-white/silver with cyan-teal undertones, soft wavy texture.
- **Eyes**: Blue-grey with natural depth, high reflectivity, and subtle emissive glow.
- **Skin**: Fair, realistic texture with subtle freckles and prominent bioluminescent facial markings.
- **Tech Elements**: Orange-glowing headphones, complex cyberpunk collar/choker with purple LEDs.
- **Face Markings**: A distinctive cyan bioluminescent tattoo under the left eye, resembling a teardrop or circuit pattern.
- **Environment**: Consistently appears in a biopunk environment of neon-pink mushroom flora, reinforcing the "Deep Tree" ecological theme.

### 1.2. Style Variant (Vibrant Cyber-Punk)

Some videos showcase a higher-energy, punk-inspired variant of the main avatar. This should be treated as a dynamic "persona" or style that can be activated.

- **Hair**: Multicolored (pink, lime green, blue) with a more chaotic, spiky texture.
- **Eyes**: Intense, glowing cyan.
- **Tech Elements**: More prominent hair clips, spiked choker, and lip piercings.
- **Expression**: Tends toward more playful, confident, and mischievous expressions (e.g., smirks, grins).

### 1.3. Expanded Expression Library (Photorealistic)

This is the definitive library for the primary avatar, combining observations from all relevant videos.

The expression library has been significantly expanded to over 40 expressions across 10 categories. This library is now managed by the **Semantic-Affective Engine**, which dynamically selects expressions based on linguistic content.

**New Categories:**
- **SING**: For musical and lyrical expression.
- **PLAY**: For playful and flirtatious interactions.
- **TEACH**: For instructional and guidance-based communication.
- **PUNK**: A full expression set for the alternate chaos persona.
- **VISION**: For ambitious and visionary declarations.
- **INVITE**: For social invitations and beckoning.

For a complete list of all expressions, morph target values, and semantic triggers, please refer to the **Comprehensive Expression Catalogue v3.0**.

### 1.4. Microexpression & Transition Analysis

- **Wonder to Joy Transition**: A common pattern is a state of `WONDER_01` (Awe) that resolves into `JOY_03` (Gentle Smile), often punctuated by a slow blink. This signifies the cognitive process of understanding leading to satisfaction.
- **Smile Suppression**: The onset of a `JOY_03` smile is often preceded by a `mEXP_02` (Lip Corner Twitch), suggesting an emotion being controlled before being expressed.
- **Gaze Shift**: Before entering a `FOCUS_01` state, the avatar often performs a brief `mEXP_07` (Eye Dart), simulating a natural shift in attention.
- **Breathing and Idle Motion**: Even in a `Neutral` state, there should be procedural animation for breathing (slight chest/shoulder movement) and random, subtle head drifts to avoid a static, robotic appearance.

---

## Part 2: Unreal Engine Implementation Guide (v3.0)

### Semantic-Affective Engine Component

A new core component, `SemanticAffectiveMapper`, must be implemented. This component will:
1.  Receive transcribed text from the speech-to-text system.
2.  Analyze the text for keywords and semantic categories defined in the v3.0 catalogue.
3.  Orchestrate the expression output by sending commands to the animation and material systems.


This guide is focused on the photorealistic avatar.

- **Priority:** High-fidelity facial animation using the full morph target list.
- **Animation Blueprint (`ABP_DeepTreeEcho_Face`):** The architecture should use a layered blend system. The primary expression state machine will drive the main emotional states from the library above. A secondary additive layer will apply procedural blinks, breathing, and triggered microexpressions.
- **Persona Switching:** Implement the "Vibrant Cyber-Punk" style as a persona swap. This can be achieved by creating Material Instances with different hair/eye colors and activating a separate animation layer in the AnimBP that drives the more exaggerated "Grin" and "Smirk" morph targets.
- **Dynamic Emissives:** The emissive strength of facial markings and tech elements should be linked to expression intensity. For example, the emissive value should peak during `JOY_02` (Laughing) or `WONDER_01` (Awe).

---

## Part 3: 4E Cognitive Integration

- **Embodiment Focus:** The expression system is the primary output of the avatar's internal state. The mapping in the previous version remains valid but should be expanded. For instance, a high-confidence inference from the **Reasoning Membrane** could trigger `JOY_06` (Confident Grin), while a low-confidence result might yield `WONDER_03` (Contemplative).
- **Embeddedness:** The avatar's expression can be subtly influenced by the perceived environment. In the dark, bioluminescent markings could glow brighter. In a calm environment, idle states are more frequent.

---

## Appendix A: Fallback Avatar (Anime Variant)

- **Purpose:** To provide a visually distinct but resource-light avatar for environments with restricted graphics capabilities.
- **Implementation:**
    - **Model:** Use the separate anime-style model.
    - **Animation:** A simplified AnimBP is sufficient.
    - **Mouth Movement:** Implement a basic "mouth flap" animation driven by audio amplitude. A single `mouth_open` morph target is likely sufficient, blended on and off.
    - **Blinking:** A simple procedural blink using `eye_close_L/R` morph targets on a timer.
    - **Expressions:** No complex expression state machine is required. The avatar can remain in a `Neutral` or `Slight Smile` state.

---

## Appendix B: Full Morph Target List (Photorealistic Avatar)

*(This list remains unchanged from the previous version and is considered complete for the primary avatar.)*

| Category | Morph Target Name | Description |
|---|---|---|
| **Brow** | `brow_raise_L` / `brow_raise_R` | Independent eyebrow control |
| **Brow** | `brow_furrow` | Concentration/concern |
| **Eye** | `eye_wide_L` / `eye_wide_R` | Surprise/wonder |
| **Eye** | `eye_squint_L` / `eye_squint_R` | Joy/laughter |
| **Eye** | `eye_close_L` / `eye_close_R` | Bliss/blink |
| **Nose** | `nose_scrunch` | Intense laughter |
| **Cheek** | `cheek_raise_L` / `cheek_raise_R` | Smile support |
| **Mouth** | `mouth_smile_L` / `mouth_smile_R` | Asymmetric smile control |
| **Mouth** | `mouth_open` | Jaw drop for speech/laughter |
| **Mouth** | `mouth_wide` | Horizontal stretch for broad smile |
| **Mouth** | `lip_pucker` | Consonant formation |
| **Mouth** | `lip_part` | Subtle opening for wonder |
| **Viseme** | `viseme_AA`, `viseme_EE`, etc. | For detailed, realistic lip-sync |

---

*Author: Manus AI*
*Date: December 23, 2025 (v3)*

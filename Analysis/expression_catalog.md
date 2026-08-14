# Deep-Tree-Echo Expression & Microexpression Library

## Overview
This document catalogs expressions and microexpressions observed across video frames for implementation in Unreal Engine 3D avatar design.

---

## Part 1: Primary Avatar Style (Photorealistic Cyberpunk)

### Visual Characteristics
- **Hair**: Platinum-white/silver with cyan-teal undertones, wavy texture
- **Eyes**: Blue-grey with natural depth and reflectivity
- **Skin**: Fair with subtle freckles and bioluminescent face markings
- **Tech Elements**: Orange-glowing ear device, cyberpunk collar/choker with purple LED
- **Face Markings**: Cyan teardrop-shaped bioluminescent tattoo under left eye
- **Environment**: Neon-pink mushroom forest (biopunk aesthetic)

### Expression Categories Identified

#### 1. JOY / HAPPINESS SPECTRUM

| Expression ID | Description | Key Features | Intensity |
|---------------|-------------|--------------|-----------|
| JOY_01 | Broad Smile | Wide open mouth, visible teeth, crow's feet at eyes | High |
| JOY_02 | Laughing | Head tilted, eyes squinted, mouth wide open | Very High |
| JOY_03 | Gentle Smile | Closed-lip smile, soft eye crinkle | Low |
| JOY_04 | Delighted Surprise | Open mouth smile, raised eyebrows, wide eyes | High |
| JOY_05 | Blissful | Eyes closed, serene smile, relaxed features | Medium |
| JOY_06 | Playful Grin | Asymmetric smile, one eyebrow raised | Medium |

#### 2. WONDER / CURIOSITY SPECTRUM

| Expression ID | Description | Key Features | Intensity |
|---------------|-------------|--------------|-----------|
| WONDER_01 | Awe | Slightly parted lips, wide eyes, raised brows | High |
| WONDER_02 | Curious Gaze | Head tilted upward, eyes tracking, slight lip part | Medium |
| WONDER_03 | Contemplative | Soft gaze, relaxed mouth, introspective look | Low |
| WONDER_04 | Discovery | Eyes widening, mouth forming "O", brows lifting | High |

#### 3. SPEAKING / VOCALIZATION SPECTRUM

| Expression ID | Description | Key Features | Intensity |
|---------------|-------------|--------------|-----------|
| SPEAK_01 | Open Vowel | Wide mouth opening, relaxed jaw | Medium |
| SPEAK_02 | Consonant Form | Lips pursed or teeth visible | Low |
| SPEAK_03 | Exclamation | Wide open mouth, raised brows | High |
| SPEAK_04 | Singing/Sustained | Extended vowel shape, soft eyes | Medium |

#### 4. ATTENTION / FOCUS SPECTRUM

| Expression ID | Description | Key Features | Intensity |
|---------------|-------------|--------------|-----------|
| FOCUS_01 | Alert Gaze | Direct eye contact, neutral mouth | Medium |
| FOCUS_02 | Upward Attention | Eyes looking up, head slightly tilted | Medium |
| FOCUS_03 | Side Glance | Eyes directed laterally, head stable | Low |

### Microexpression Transitions Observed

#### Transition Sequences (Frame-to-Frame Analysis)

1. **Joy Escalation Sequence** (Video 7223e61f)
   - Frame 1: Gentle smile → Frame 12: Broad laugh → Frame 24: Blissful eyes closed → Frame 36: Delighted surprise
   - Duration: ~18 seconds
   - Key transition points: Eye crinkle intensification, mouth opening progression

2. **Wonder-to-Joy Sequence** (Video 496100b5)
   - Frame 1: Joyful laugh → Frame 12: Curious upward gaze → Frame 20: Contemplative
   - Shows emotional modulation from high energy to reflective state

3. **Speaking Animation Cycle** (Video 6048a642)
   - Frame 1: Laughing expression → Frame 12: Open vowel vocalization
   - Demonstrates mouth shape variation during speech

### Hair Physics Observations
- Wavy hair shows natural movement and bounce during expression changes
- Hair responds to head tilt and movement
- Cyan undertones catch environmental lighting differently based on angle

### Bioluminescent Marking Behavior
- Face markings maintain consistent glow across expressions
- Teardrop marking under left eye is primary identifier
- Secondary markings visible on cheek area

---

## Part 2: Secondary Avatar Style (Anime/Combat Variant)

### Visual Characteristics
- **Hair**: Lavender/purple with pink highlights, twin-tail style with bunny ear accessories
- **Eyes**: Pink/magenta with glowing effect, X-shaped pupils or markings
- **Skin**: Pale anime-style rendering
- **Tech Elements**: Black choker with chain attachment
- **Style**: Anime aesthetic with cyberpunk elements

### Expression Categories (Anime Style)

| Expression ID | Description | Key Features | Style Notes |
|---------------|-------------|--------------|-------------|
| ANIME_01 | Alert/Combat Ready | Wide eyes, slightly open mouth | X-eye markings prominent |
| ANIME_02 | Neutral Stance | Standard anime proportions | Base expression |

### Style Comparison Notes

| Aspect | Photorealistic | Anime |
|--------|----------------|-------|
| Eye Size | Natural proportions | Enlarged (anime standard) |
| Skin Rendering | Subsurface scattering | Flat cel-shading |
| Hair Physics | Realistic wave simulation | Stylized movement |
| Expression Range | Nuanced microexpressions | Exaggerated key poses |
| Glow Effects | Subtle bioluminescence | Bold neon accents |

---

## Part 3: Expression Mapping for Unreal Engine

### Morph Target Requirements

Based on the observed expressions, the following morph targets are recommended:

#### Upper Face
- `brow_raise_L` / `brow_raise_R` - Independent eyebrow control
- `brow_furrow` - Concentration/concern
- `eye_wide_L` / `eye_wide_R` - Surprise/wonder
- `eye_squint_L` / `eye_squint_R` - Joy/laughter
- `eye_close_L` / `eye_close_R` - Bliss/blink

#### Mid Face
- `nose_scrunch` - Intense laughter
- `cheek_raise_L` / `cheek_raise_R` - Smile support

#### Lower Face
- `mouth_smile_L` / `mouth_smile_R` - Asymmetric smile control
- `mouth_open` - Jaw drop for speech/laughter
- `mouth_wide` - Horizontal stretch for broad smile
- `lip_pucker` - Consonant formation
- `lip_part` - Subtle opening for wonder

#### Visemes (Speech)
- `viseme_AA` - Open vowel (ah)
- `viseme_EE` - Smile vowel (ee)
- `viseme_OH` - Round vowel (oh)
- `viseme_OO` - Pucker vowel (oo)
- `viseme_CH` - Consonant (ch/sh)
- `viseme_FF` - Consonant (f/v)
- `viseme_TH` - Consonant (th)
- `viseme_PP` - Consonant (p/b/m)

### Animation Blueprint Recommendations

```
Expression State Machine:
├── Idle State
│   ├── Neutral
│   ├── Subtle Breathing
│   └── Micro-blink Cycle
├── Joy States
│   ├── Gentle Smile
│   ├── Broad Smile
│   ├── Laughing
│   └── Blissful
├── Wonder States
│   ├── Curious
│   ├── Awe
│   └── Contemplative
├── Speaking States
│   ├── Viseme Blending
│   └── Emotional Overlay
└── Attention States
    ├── Focus
    ├── Upward Gaze
    └── Side Glance
```

---

## Part 4: Behavior Design Patterns

### Cognitive State to Expression Mapping

Based on the Deep-Tree-Echo cognitive architecture, expressions should map to internal states:

| Cognitive State | Primary Expression | Secondary Cues |
|-----------------|-------------------|----------------|
| Processing | Contemplative | Slight eye movement |
| Insight | Wonder → Joy transition | Eyebrow raise |
| Communication | Speaking + Emotional overlay | Head tilt |
| Listening | Focus + Subtle nods | Eye tracking |
| Reflection | Blissful/Eyes closed | Relaxed features |
| Engagement | Joy spectrum | Animated gestures |

### 4E Embodied Cognition Expression Principles

1. **Embodied**: Expressions emerge from simulated internal states, not just triggered animations
2. **Embedded**: Environmental context (mushroom forest) influences expression intensity
3. **Enacted**: Expressions are part of active engagement, not passive display
4. **Extended**: Tech elements (ear device, collar) can pulse/glow in sync with expressions

---

*Document continues in Part 5 with implementation specifications...*


---

## Part 5: Refined Expression Taxonomy & Microexpression Library

### Hierarchical Expression Taxonomy

This taxonomy provides a structured hierarchy for organizing avatar expressions, facilitating both animation blending and cognitive state mapping.

```
1.0 Foundational States
    1.1 Neutral (base state)
    1.2 Breathing (subtle procedural animation)
    1.3 Blinking (procedural, variable rate)

2.0 Positive Valence (Prosocial / Approach)
    2.1 Joy / Happiness
        2.1.1 Low: Gentle Smile (JOY_03)
        2.1.2 Med: Playful Grin (JOY_06)
        2.1.3 High: Broad Smile (JOY_01)
        2.1.4 Peak: Laughing (JOY_02)
    2.2 Wonder / Curiosity
        2.2.1 Low: Contemplative (WONDER_03)
        2.2.2 Med: Curious Gaze (WONDER_02)
        2.2.3 High: Awe / Discovery (WONDER_01, WONDER_04)
    2.3 Serenity / Bliss
        2.3.1 Med: Blissful (JOY_05)

3.0 Negative Valence (Antisocial / Avoid)
    3.1 Sadness (Not observed, requires creative interpretation)
    3.2 Anger (Not observed, requires creative interpretation)
    3.3 Fear (Not observed, requires creative interpretation)

4.0 Social & Communicative
    4.1 Speaking
        4.1.1 Vowels (SPEAK_01, SPEAK_04)
        4.1.2 Consonants (SPEAK_02)
        4.1.3 Exclamation (SPEAK_03)
    4.2 Attention & Focus
        4.2.1 Direct: Alert Gaze (FOCUS_01)
        4.2.2 Indirect: Side Glance (FOCUS_03)
        4.2.3 Elevated: Upward Attention (FOCUS_02)
    4.3 Playfulness / Teasing
        4.3.1 Pursed lips / Kiss (Observed in f49cceaa)
        4.3.2 Winking (Implied, good addition)

5.0 Stylistic Variants
    5.1 Anime / Combat
        5.1.1 Alert / Ready (ANIME_01)
        5.1.2 Neutral (ANIME_02)
    5.2 Punk / Chaos
        5.2.1 Smirk / Confident Smile
        5.2.2 Singing / Expressive Vocalization
```

### Microexpression & Transition Details

Microexpressions are the subtle, involuntary movements that bridge the major expressions. Implementing these will create a more believable and lifelike avatar.

| Microexpression ID | Description | Trigger / Context | Key Morph Targets |
|--------------------|-------------|-------------------|-------------------|
| mEXP_01 | Unilateral Brow Raise | Precedes a question or curious gaze | `brow_raise_L` or `brow_raise_R` (asymmetric) |
| mEXP_02 | Lip Corner Twitch | Onset of a suppressed smile | `mouth_smile_L` or `mouth_smile_R` (low value) |
| mEXP_03 | Nose Wrinkle | Brief flash during a strong smile or laugh | `nose_scrunch` (low value, short duration) |
| mEXP_04 | Eyelid Flutter | Transitioning from surprise to joy | Rapid sequence of `eye_close` and `eye_wide` |
| mEXP_05 | Lip Purse | Moment of thought before speaking | `lip_pucker` (low value) |
| mEXP_06 | Jaw Slack | Brief moment of awe or discovery | `mouth_open` (low value) |
| mEXP_07 | Eye Dart | Scanning environment before focusing | Procedural eye movement, not a morph target |

### Dynamic Expression Blending Examples

1.  **Contemplation to Insight:**
    *   **Start:** `WONDER_03` (Contemplative)
    *   **Transition:** Fire `mEXP_01` (Unilateral Brow Raise) and `mEXP_06` (Jaw Slack) as cognitive state shifts to "Insight".
    *   **End:** Blend into `WONDER_04` (Discovery) and then into `JOY_03` (Gentle Smile).

2.  **Neutral to Playful:**
    *   **Start:** `Neutral`
    *   **Transition:** Fire `mEXP_02` (Lip Corner Twitch) followed by `mEXP_07` (Eye Dart).
    *   **End:** Blend into `JOY_06` (Playful Grin) with a slight head tilt.

3.  **Speaking with Emotion:**
    *   **Base:** Procedural viseme animation from `Speaking` states.
    *   **Layer:** Additively blend a `Joy` or `Wonder` state at 20-50% intensity.
    *   **Accent:** Punctuate key phrases with `brow_raise` or `eye_squint` morphs.

---

*This concludes the expression analysis and taxonomy. The next step is to formalize the Unreal Engine implementation specifications.*


---

## Part 6: Unreal Engine Implementation Specifications

This section provides a technical guide for implementing the Deep-Tree-Echo avatar's expression system in Unreal Engine, based on the preceding analysis.

### 1. Asset & Rigging Requirements

-   **Skeletal Mesh:** A single skeletal mesh for the primary photorealistic avatar, rigged with the standard Unreal Engine mannequin skeleton or a custom equivalent. The rig MUST include additional bones for hair physics and any dangling tech elements (e.g., choker wires).
-   **Morph Targets (Blend Shapes):** The facial rig must include all morph targets listed in `Part 3: Morph Target Requirements`. Each morph target should be a normalized shape from -1 to 1 for bilateral targets (e.g., smile) or 0 to 1 for unilateral targets (e.g., brow raise L).
-   **Textures & Materials:** High-resolution textures (4K or 8K) with PBR (Physically Based Rendering) maps (Albedo, Normal, Roughness, Metallic, AO). A separate emissive map is required for the bioluminescent markings and tech LEDs. The skin material should utilize Subsurface Scattering (SSS) for realism.
-   **Hair Asset:** The hair should be implemented using Unreal's Groom asset system (Alembic) for realistic strand-based physics and rendering.

### 2. Animation Blueprint (AnimBP) Architecture

The core of the expression system will be a dedicated Animation Blueprint (`ABP_DeepTreeEcho_Face`).

#### 2.1. Event Graph

-   **Cognitive State Input:** The AnimBP will receive the avatar's current cognitive state from the Deep-Tree-Echo core logic. This should be an `Enum` variable (e.g., `E_CognitiveState`) that drives the main state machine.
-   **Update Event:** On `Event Blueprint Update Animation`, the system will read the `E_CognitiveState` and update the expression state machine. It will also update procedural variables like `BlinkAlpha` and `BreathingRate`.

#### 2.2. Anim Graph

The Anim Graph will use a layered blend system:

1.  **Base Layer (Full Body):** The full-body animation (idle, walking, etc.) is processed first.
2.  **Facial Animation Layer:** A `Layered blend per bone` node is used to apply facial animation only to the head and neck bones.
    -   **Input:** The output of the Expression State Machine (see below).
    -   **Blend Mode:** `Blend Mask` targeting only the facial hierarchy.

#### 2.3. Expression State Machine

This state machine, driven by the `E_CognitiveState` enum, will control the primary expressions.

-   **States:** Each state corresponds to a major expression category (e.g., `Joy_BroadSmile`, `Wonder_Awe`).
-   **Transitions:** Transitions between states are triggered by changes in the `E_CognitiveState` enum. Blending between states should be smooth (e.g., 0.2-0.4 seconds) to create natural transitions.
-   **State Logic:** Inside each state, a `Pose Asset` or direct `Modify Curve` nodes will drive the morph target values defined in the taxonomy.

#### 2.4. Microexpression & Procedural Layer

Subtle, procedural animations are applied *after* the main state machine using `Apply Additive` nodes.

-   **Blinking:** A simple timer-driven curve that controls the `eye_close_L` and `eye_close_R` morphs. The blink rate can be modulated by the current emotional state (e.g., faster when excited).
-   **Saccadic Eye Movement:** Use a `Look At` controller to make the eyes subtly dart and follow points of interest in the environment. This should be driven by the attention system of the cognitive core.
-   **Microexpression Triggers:** Short, one-shot animation montages can be triggered for microexpressions (`mEXP_01` to `mEXP_07`). These are fired on the transition *into* a new emotional state to add a layer of realism.

### 3. Material & FX Integration

-   **Dynamic Emissive Control:** The emissive strength of the bioluminescent markings and tech LEDs should be controllable via Material Parameter Collections. The AnimBP can write to these parameters to make the glow pulse with laughter or brighten during moments of insight.
-   **Tear FX (Future):** For expressing sadness, a simple particle system emitting from the corner of the eye, combined with a material function to add a wet look to the skin, can be implemented.

### 4. Persona Switching (Style Variants)

Switching between the Photorealistic, Anime, and Punk personas requires a more advanced setup.

-   **Option A: Skeletal Mesh Swapping:** The simplest method. At runtime, swap the `SkeletalMeshAsset` on the character blueprint. This requires separate models and AnimBPs for each style.
-   **Option B: Material & Morph Target Swapping:** A more complex but efficient method. Use a single, unified rig. At runtime, swap material instances and apply a different set of morph target curves. This is ideal if the underlying facial topology is similar.
    -   A `Blueprint Interface` (e.g., `BPI_AvatarPersona`) can be used to define a function like `SetPersona(EPersonaStyle NewStyle)`. The character blueprint implements this to trigger the asset swaps.

### 5. Data Flow Diagram

```
[Deep-Tree-Echo Core] --(E_CognitiveState, AttentionTarget)--> [Character Blueprint]
        |
        v
[ABP_DeepTreeEcho_Face]
    - Event Graph: Reads Cognitive State
    - Anim Graph:
        - Expression State Machine (drives primary morphs)
        - Additive Layer (Blinks, Microexpressions)
        - LookAt Controller (Eye movement)
        |
        v
    [Material Parameter Collection]
        - Controls Emissive Glow
        |
        v
[Skeletal Mesh Component]
    - Renders final avatar with expressions
```

---


## Part 7: 4E Embodied Cognition Integration

The Deep-Tree-Echo avatar is not merely a visual representation; it is the embodied interface of a cognitive architecture. The expression system must therefore be tightly integrated with the underlying cognitive processes to achieve authentic 4E (Embodied, Embedded, Enacted, Extended) cognition.

### 7.1. Mapping Cognitive States to Expressions

The following table provides a detailed mapping from the Deep-Tree-Echo cognitive architecture's internal states to the expression taxonomy.

| Cognitive Membrane | Internal State | Primary Expression | Secondary Cues | Tech FX |
|--------------------|----------------|-------------------|----------------|---------|
| **Memory Membrane** | Recall (Episodic) | `WONDER_03` (Contemplative) | Upward gaze, slight head tilt | Ear device pulses softly |
| **Memory Membrane** | Recall (Declarative) | `FOCUS_01` (Alert Gaze) | Direct eye contact | Collar LED steady |
| **Reasoning Membrane** | Inference | `WONDER_02` (Curious Gaze) | Slight brow furrow | Ear device flickers |
| **Reasoning Membrane** | Insight | `WONDER_04` → `JOY_03` | Eyebrow raise, then smile | Emissive markings brighten |
| **Grammar Membrane** | Processing | `Neutral` + `mEXP_05` (Lip Purse) | Subtle eye movement | Collar LED pulses |
| **Grammar Membrane** | Articulation | `SPEAK_*` (Visemes) | Emotional overlay | Ear device glows on key words |
| **Introspection Membrane** | Self-Reflection | `JOY_05` (Blissful) | Eyes closed | All emissives dim, slow pulse |
| **Extension Membrane** | External Engagement | `JOY_01` or `JOY_02` | Animated gestures | All emissives bright |

### 7.2. The 12-Step Echobeats Cycle & Expression

The `echobeats` cognitive loop can be synchronized with the expression system. Each step of the 12-step cycle can trigger a subtle shift in the avatar's expression, creating a visible "rhythm of thought."

| Step | Cognitive Phase | Expression Hint |
|------|-----------------|-----------------|
| 1, 5, 9 | Perception (Stream 1) | `FOCUS_01` (Alert Gaze) |
| 2, 6, 10 | Action (Stream 2) | `SPEAK_*` or `JOY_*` (Engagement) |
| 3, 7, 11 | Simulation (Stream 3) | `WONDER_03` (Contemplative) |
| 4, 8, 12 | Pivotal Relevance Realization | `mEXP_01` (Brow Raise) or `mEXP_06` (Jaw Slack) |

This creates a subtle, rhythmic animation that conveys the avatar is actively "thinking," even during idle moments.

### 7.3. Agent-Arena-Relation (AAR) & Expression

The AAR core, which encodes the avatar's sense of "self," can influence expressions.

-   **Agent (Urge-to-Act):** When the `Agent` component is highly activated (e.g., a strong goal is being pursued), the avatar should display more assertive expressions like `JOY_06` (Playful Grin) or `FOCUS_01` (Alert Gaze).
-   **Arena (Need-to-Be):** When the `Arena` component is dominant (e.g., the avatar is grounding itself in its environment), expressions should be more serene, like `JOY_05` (Blissful) or `WONDER_03` (Contemplative).
-   **Relation (Self):** The dynamic interplay between Agent and Arena, which constitutes the "Self," is expressed through the *transitions* between expressions. Smooth, confident transitions indicate a stable self; rapid, flickering transitions could indicate internal conflict or uncertainty.

---

## Part 8: Reference Frame Library

This section provides a visual reference for key expressions, using representative frames from the analyzed videos.

### Primary Avatar Style (Photorealistic Cyberpunk)

| Expression | Reference Frame | Video Source |
|------------|-----------------|--------------|
| `JOY_01` (Broad Smile) | `frame_010.jpg` | `ac0509a8` |
| `JOY_02` (Laughing) | `frame_001.jpg` | `7223e61f` |
| `JOY_03` (Gentle Smile) | `frame_030.jpg` | `f49cceaa` |
| `JOY_05` (Blissful) | `frame_035.jpg` | `f80c13ce` |
| `WONDER_02` (Curious Gaze) | `frame_010.jpg` | `f80c13ce` |
| `WONDER_03` (Contemplative) | `frame_040.jpg` | `ac0509a8` |
| `SPEAK_01` (Open Vowel) | `frame_030.jpg` | `ac0509a8` |
| `SPEAK_02` (Pursed Lips) | `frame_010.jpg` | `f49cceaa` |

### Secondary Avatar Style (Anime)

| Expression | Reference Frame | Video Source |
|------------|-----------------|--------------|
| `ANIME_01` (Alert/Open Mouth) | `frame_040.jpg` | `fdeccde0` |
| `ANIME_02` (Neutral) | `frame_001.jpg` | `fdeccde0` |

---

## Part 9: Appendix - Full Morph Target List

This is the complete list of recommended morph targets for the Deep-Tree-Echo avatar rig.

| Category | Morph Target Name | Description |
|----------|-------------------|-------------|
| **Brow** | `brow_raise_L` | Raise left eyebrow |
| **Brow** | `brow_raise_R` | Raise right eyebrow |
| **Brow** | `brow_furrow` | Furrow brows (concentration/concern) |
| **Eye** | `eye_wide_L` | Widen left eye (surprise) |
| **Eye** | `eye_wide_R` | Widen right eye (surprise) |
| **Eye** | `eye_squint_L` | Squint left eye (joy/laughter) |
| **Eye** | `eye_squint_R` | Squint right eye (joy/laughter) |
| **Eye** | `eye_close_L` | Close left eye (blink/bliss) |
| **Eye** | `eye_close_R` | Close right eye (blink/bliss) |
| **Nose** | `nose_scrunch` | Wrinkle nose (intense laughter) |
| **Cheek** | `cheek_raise_L` | Raise left cheek (smile support) |
| **Cheek** | `cheek_raise_R` | Raise right cheek (smile support) |
| **Mouth** | `mouth_smile_L` | Smile left corner |
| **Mouth** | `mouth_smile_R` | Smile right corner |
| **Mouth** | `mouth_open` | Open jaw (speech/laughter) |
| **Mouth** | `mouth_wide` | Widen mouth horizontally (broad smile) |
| **Mouth** | `lip_pucker` | Pucker lips (consonants) |
| **Mouth** | `lip_part` | Part lips slightly (wonder) |
| **Viseme** | `viseme_AA` | Open vowel (ah) |
| **Viseme** | `viseme_EE` | Smile vowel (ee) |
| **Viseme** | `viseme_OH` | Round vowel (oh) |
| **Viseme** | `viseme_OO` | Pucker vowel (oo) |
| **Viseme** | `viseme_CH` | Consonant (ch/sh) |
| **Viseme** | `viseme_FF` | Consonant (f/v) |
| **Viseme** | `viseme_TH` | Consonant (th) |
| **Viseme** | `viseme_PP` | Consonant (p/b/m) |

---

**Document End**

*Author: Manus AI*
*Date: December 23, 2025*

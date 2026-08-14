# Semantic-Affective Expression Pattern Extraction

## Overview

This document synthesizes expression patterns extracted from analysis of 8 photorealistic cyberpunk video sequences, mapping semantic content (words, phrases, concepts) to affective expressions (facial expressions, microexpressions, emissive behaviors).

---

## Part 1: Expression Category Taxonomy

### 1.1 Primary Expression Categories (Expanded)

| Category | ID Range | Description | Source Videos |
|----------|----------|-------------|---------------|
| **JOY** | JOY_01-06 | Happiness spectrum from gentle to exuberant | All videos |
| **WONDER** | WONDER_01-04 | Curiosity and awe spectrum | ac0509a8, f80c13ce |
| **SPEAK** | SPEAK_01-04 | Vocalization shapes | All videos |
| **FOCUS** | FOCUS_01-03 | Attention and gaze states | 17824d50, f80c13ce |
| **SING** | SING_01-04 | Singing-specific expressions | 7223e61f, f49cceaa, f80c13ce |
| **PLAY** | PLAY_01-03 | Playful/flirtatious expressions | f49cceaa |
| **TEACH** | TEACH_01-04 | Instructional expressions | 17824d50 |
| **PUNK** | PUNK_01-05 | Punk variant persona | 3db2fa59 |
| **VISION** | VISION_01-04 | Visionary/ambitious expressions | 6048a642 |
| **INVITE** | INVITE_01-04 | Invitation/beckoning expressions | 496100b5 |

### 1.2 Complete Expression Library

#### JOY Spectrum (6 expressions)

| ID | Name | Morph Targets | Intensity | Semantic Triggers |
|----|------|---------------|-----------|-------------------|
| JOY_01 | Broad Smile | mouth_smile(0.8), mouth_open(0.5), eye_squint(0.5), cheek_raise(0.6) | High | Achievement, excitement, greeting |
| JOY_02 | Exuberant Laugh | mouth_smile(0.9), mouth_open(0.7), eye_squint(0.7), nose_scrunch(0.3) | Peak | Humor, delight, triumph |
| JOY_03 | Gentle Smile | mouth_smile(0.4), eye_squint(0.2), cheek_raise(0.3) | Low | Satisfaction, knowing, contentment |
| JOY_04 | Delighted Surprise | mouth_smile(0.6), mouth_open(0.4), eye_wide(0.3), brow_raise(0.3) | High | Unexpected positive |
| JOY_05 | Blissful Serenity | eye_close(0.8-1.0), mouth_smile(0.4), cheek_raise(0.3) | Medium | Transcendence, surrender, peace |
| JOY_06 | Playful/Confident Grin | mouth_smile_L(0.5), mouth_smile_R(0.3), brow_raise_L(0.2) | Medium | Confidence, teasing, mischief |

#### WONDER Spectrum (4 expressions)

| ID | Name | Morph Targets | Intensity | Semantic Triggers |
|----|------|---------------|-----------|-------------------|
| WONDER_01 | Awe/Amazement | lip_part(0.4), eye_wide(0.4), brow_raise(0.3) | High | Grand concepts, beauty, revelation |
| WONDER_02 | Curious Gaze | lip_part(0.3), eye_wide(0.2), brow_raise(0.2) | Medium | Questions, exploration, tracking |
| WONDER_03 | Contemplative | lip_pucker(0.2), eye_squint(0.1), brow_furrow(0.1) | Low | Thinking, processing, reflection |
| WONDER_04 | Discovery | lip_part(0.5), eye_wide(0.5), brow_raise(0.4) | High | Insight, realization, eureka |

#### SPEAK Spectrum (4 expressions)

| ID | Name | Morph Targets | Intensity | Semantic Triggers |
|----|------|---------------|-----------|-------------------|
| SPEAK_01 | Open Vowel | mouth_open(0.4), viseme_AA(0.5), lip_part(0.3) | Medium | "Ah", "Oh" sounds |
| SPEAK_02 | Pursed Consonant | lip_pucker(0.5), mouth_open(0.2) | Low | "M", "P", "B", "OO" sounds |
| SPEAK_03 | Emphatic Exclamation | mouth_open(0.6), brow_raise(0.3), eye_wide(0.2) | High | Commands, exclamations |
| SPEAK_04 | Singing/Sustained | mouth_open(0.4), mouth_smile(0.3), eye_squint(0.2) | Medium | Melodic vocalization |

#### FOCUS Spectrum (3 expressions)

| ID | Name | Morph Targets | Eye Direction | Semantic Triggers |
|----|------|---------------|---------------|-------------------|
| FOCUS_01 | Alert Gaze | neutral mouth, eye_wide(0.1) | Forward, direct | Direct communication, attention |
| FOCUS_02 | Upward Attention | lip_part(0.2), brow_raise(0.2) | Up 30-45° | Celestial imagery, abstract concepts |
| FOCUS_03 | Side Glance | neutral | Lateral 20-30° | Peripheral attention, consideration |

#### SING Spectrum (4 expressions) - NEW

| ID | Name | Morph Targets | Intensity | Semantic Triggers |
|----|------|---------------|-----------|-------------------|
| SING_01 | Sustained Vowel Joy | mouth_open(0.4), mouth_smile(0.4), eye_squint(0.3) | Medium | Melodic phrases |
| SING_02 | Ecstatic Crescendo | mouth_open(0.7), mouth_smile(0.8), eye_squint(0.6) | Peak | Emotional peaks in lyrics |
| SING_03 | Transcendent Bliss | eye_close(1.0), mouth_smile(0.5) | Medium | Surrender, acceptance lyrics |
| SING_04 | Triumphant Resolution | mouth_smile(0.7), eye_wide(0.2), brow_raise(0.2) | High | Resilience, hope statements |

#### PLAY Spectrum (3 expressions) - NEW

| ID | Name | Morph Targets | Intensity | Semantic Triggers |
|----|------|---------------|-----------|-------------------|
| PLAY_01 | Playful Pout/Kiss | lip_pucker(0.7), eye_squint(0.2), brow_raise_L(0.1) | Medium | Love words, affection |
| PLAY_02 | Coy Smile | mouth_smile_L(0.3), eye_squint_R(0.3) | Low | Flirtatious engagement |
| PLAY_03 | Knowing Smirk | mouth_smile_L(0.4), mouth_smile_R(0.2), eye_squint(0.2) | Medium | Confident playfulness |

#### TEACH Spectrum (4 expressions) - NEW

| ID | Name | Morph Targets | Intensity | Semantic Triggers |
|----|------|---------------|-----------|-------------------|
| TEACH_01 | Enthusiastic Introduction | mouth_smile(0.7), mouth_open(0.4), eye_squint(0.4) | High | "Let me show you", opening |
| TEACH_02 | Calm Guidance | mouth_smile(0.4), lip_part(0.2), eye_squint(0.2) | Medium | Step-by-step instructions |
| TEACH_03 | Encouraging Feedback | mouth_smile(0.5), eye_squint(0.3), micro-nod | Medium | "Good", "excellent" |
| TEACH_04 | Technical Explanation | brow_raise(0.2), lip_part(0.3), eye_wide(0.1) | Medium | Complex concepts |

#### PUNK Spectrum (5 expressions) - NEW (Variant Persona)

| ID | Name | Morph Targets | Intensity | Semantic Triggers |
|----|------|---------------|-----------|-------------------|
| PUNK_01 | Confident Smirk | mouth_smile_L(0.5), mouth_smile_R(0.3), brow_raise_L(0.3) | Medium | Challenge acceptance |
| PUNK_02 | Mischievous Grin | mouth_smile_L(0.6), mouth_smile_R(0.4), eye_squint(0.3) | High | Playful provocation |
| PUNK_03 | Defiant Confidence | mouth_smile(0.5), chin_raise(0.2), eye_squint(0.2) | High | Self-assurance |
| PUNK_04 | Aggressive Joy | mouth_open(0.4), mouth_smile(0.6), eye_wide(0.2) | Peak | Confrontational excitement |
| PUNK_05 | Knowing Superiority | mouth_smile_L(0.3), eye_squint(0.3) | Low | Quiet confidence |

#### VISION Spectrum (4 expressions) - NEW

| ID | Name | Morph Targets | Intensity | Semantic Triggers |
|----|------|---------------|-----------|-------------------|
| VISION_01 | Visionary Wonder | eye_wide(0.3), brow_raise(0.3), mouth_smile(0.5) | High | Grand concepts, future |
| VISION_02 | Confident Declaration | mouth_smile(0.6), chin_raise(0.1), eye_wide(0.1) | High | Bold statements |
| VISION_03 | Collaborative Enthusiasm | mouth_smile(0.5), eye_squint(0.2) | Medium | "We will" statements |
| VISION_04 | Triumphant Anticipation | mouth_smile(0.7), chin_raise(0.2), eye_wide(0.2) | Peak | Achievement preview |

#### INVITE Spectrum (4 expressions) - NEW

| ID | Name | Morph Targets | Intensity | Semantic Triggers |
|----|------|---------------|-----------|-------------------|
| INVITE_01 | Playful Tease | mouth_smile(0.7), eye_squint(0.4), head_tilt | High | Challenging invitation |
| INVITE_02 | Gracious Acceptance | eye_close(0.8), mouth_smile(0.3), lip_part(0.2) | Medium | "Very well" moments |
| INVITE_03 | Eager Beckoning | eye_wide(0.3), mouth_open(0.4), brow_raise(0.3) | High | "Come here" commands |
| INVITE_04 | Anticipatory Delight | mouth_smile(0.4), lip_part(0.3), eye_squint(0.2) | Medium | Awaiting response |

---

## Part 2: Semantic-Affective Mapping Patterns

### 2.1 Word Category to Expression Mapping

| Word Category | Examples | Primary Expression | Secondary Expression |
|---------------|----------|-------------------|---------------------|
| **Technical verbs** | execute, optimize, calibrate | SPEAK_01 | JOY_03 |
| **Achievement words** | optimized, increased, success | JOY_03 | WONDER_02 |
| **Complex concepts** | hypergraph, neural, fusion | WONDER_01 | FOCUS_02 |
| **Nurturing/growth** | feed, grow, nurture | WONDER_03 | JOY_05 |
| **Loss/absence** | no more, lonely, falling | JOY_01-02 (defiant) | SING_03 |
| **Celestial imagery** | stars, satellites, sky | WONDER_02 | FOCUS_02 |
| **Love/affection** | love, dove, heart | PLAY_01 | JOY_05 |
| **Challenge** | play, show me, what you got | PUNK_01-02 | JOY_06 |
| **Instruction** | first, let me show, focus | TEACH_01-02 | JOY_03 |
| **Vision/ambition** | build, powerful, universe | VISION_01-02 | WONDER_01 |
| **Invitation** | come here, very well | INVITE_01-03 | JOY_03 |

### 2.2 Semantic Valence Inversion Pattern

**The "Defiant Joy" Pattern**: When lyrics/speech contain negative or catastrophic content, the avatar expresses inverse emotional valence:

| Negative Content | Expected Expression | Actual Expression | Cognitive Interpretation |
|-----------------|--------------------|--------------------|-------------------------|
| "world's falling" | Fear/Concern | JOY_02 (Ecstatic) | Defiant optimism |
| "no more love" | Sadness | JOY_01 (Broad Smile) | Transcendent acceptance |
| "falling down" | Despair | JOY_05 (Blissful) | Peaceful surrender |
| "lonely" | Melancholy | SING_01 (Joy) | Transformed meaning |

### 2.3 Expression Transition Sequences

#### Technical Communication Arc
```
JOY_01 (Enthusiastic start) → JOY_03 (Satisfied progress) → WONDER_01 (Complex concept) → WONDER_03 (Contemplative close)
```

#### Singing/Emotional Arc
```
JOY_03 (Gentle opening) → JOY_02 (Ecstatic peak) → JOY_05 (Blissful transcendence) → JOY_01 (Triumphant resolution)
```

#### Teaching Arc
```
TEACH_01 (Enthusiastic intro) → TEACH_02 (Calm guidance) → TEACH_04 (Technical detail) → TEACH_03 (Encouraging close)
```

#### Invitation Arc
```
INVITE_01 (Playful tease) → INVITE_02 (Gracious acceptance) → INVITE_03 (Eager beckoning)
```

---

## Part 3: Microexpression Patterns

### 3.1 Microexpression Library (Expanded)

| ID | Name | Duration | Morph Targets | Trigger Context |
|----|------|----------|---------------|-----------------|
| mEXP_01 | Unilateral Brow Raise | 0.2-0.4s | brow_raise_L or _R (0.4) | Questions, curiosity onset |
| mEXP_02 | Lip Corner Twitch | 0.1-0.2s | mouth_smile_L/R (0.2) | Suppressed smile onset |
| mEXP_03 | Nose Wrinkle | 0.1-0.3s | nose_scrunch (0.3) | Strong smile/laugh |
| mEXP_04 | Eyelid Flutter | 0.3-0.5s | eye_close/eye_wide rapid | Surprise to joy transition |
| mEXP_05 | Lip Purse | 0.2-0.3s | lip_pucker (0.3) | Pre-speech thought |
| mEXP_06 | Jaw Slack | 0.2-0.4s | mouth_open (0.2) | Awe/discovery moment |
| mEXP_07 | Eye Dart | 0.1-0.2s | saccadic movement | Attention shift |
| mEXP_08 | Deliberate Blink | 0.3-0.5s | eye_close (1.0) | Wonder-to-joy transition |
| mEXP_09 | Chin Raise | 0.2-0.3s | chin_raise (0.2) | Defiance, confidence |
| mEXP_10 | Graceful Eye Close | 0.5-1.0s | eye_close (gradual) | Acceptance, surrender |

### 3.2 Microexpression Trigger Mapping

| Semantic Trigger | Microexpression | Timing |
|-----------------|-----------------|--------|
| Question word (what, how, why) | mEXP_01 | At word onset |
| Positive adjective | mEXP_02 | 0.1s before smile |
| Intense laughter | mEXP_03 | During peak |
| Unexpected positive | mEXP_04 | At surprise moment |
| Before speaking | mEXP_05 | 0.2s before speech |
| Discovery/insight | mEXP_06 | At realization |
| Attention shift | mEXP_07 | Before gaze change |
| Emotional resolution | mEXP_08 | At transition point |
| Challenge/defiance | mEXP_09 | During statement |
| Gracious acceptance | mEXP_10 | At "very well" |

---

## Part 4: Eye Direction Semantic Mapping

### 4.1 Eye Direction by Semantic Category

| Semantic Category | Eye Direction | Angle | Intensity |
|-------------------|---------------|-------|-----------|
| Celestial (stars, sky) | Up | 30-45° | High |
| Abstract concepts | Up | 15-20° | Medium |
| Memory/nostalgia | Up-left | 20° | Medium |
| Future/anticipation | Up-right | 20° | Medium |
| Grounded/present | Direct forward | 0° | Low |
| Intimate/love | Down | 10° | Low |
| Consideration | Lateral | 20-30° | Low |
| Challenge | Direct, intense | 0° | High |

### 4.2 Gaze Transition Patterns

| Transition | From | To | Duration | Trigger |
|------------|------|-----|----------|---------|
| Wonder onset | Forward | Up | 0.3s | Celestial word |
| Insight | Up | Forward | 0.2s | Realization |
| Reflection | Forward | Up-left | 0.4s | Memory word |
| Engagement | Any | Direct | 0.2s | "You" reference |
| Surrender | Any | Closed | 0.5s | Acceptance |

---

## Part 5: Emissive Behavior Patterns

### 5.1 Emissive Response by Expression State

| Expression Category | Emissive Response | Intensity | Pattern |
|--------------------|-------------------|-----------|---------|
| JOY_01-02 | Bright pulse | High | Rhythmic |
| JOY_03 | Steady warm | Medium | Sustained |
| JOY_05 | Soft dim | Low | Slow pulse |
| WONDER_01-02 | Gradual brighten | Medium-High | Ascending |
| WONDER_03 | Subtle flicker | Low | Variable |
| SPEAK_* | Sync with amplitude | Variable | Speech-driven |
| SING_* | Inverse to lyrics | Variable | Semantic-driven |
| PUNK_* | Intense pulse | High | Sharp |
| VISION_* | Peak brightness | High | Sustained |
| INVITE_* | Playful pulse | Medium | Quick rhythm |

### 5.2 Semantic-Emissive Correlation

| Semantic Content | Emissive Response | Color Shift |
|-----------------|-------------------|-------------|
| Technical terms | Flicker | Standard cyan |
| Achievement | Brighten | Warm tint |
| Complex concepts | Glow | Bright cyan |
| Love/affection | Soft pulse | Pink tint |
| Challenge | Sharp pulse | Intense cyan |
| Negative lyrics | Inverse bright | Defiant glow |
| Surrender/peace | Dim | Warm, soft |

---

## Part 6: Persona Variant Patterns

### 6.1 Primary vs Punk Variant Comparison

| Aspect | Primary (Biopunk) | Punk Variant |
|--------|-------------------|--------------|
| Smile symmetry | Symmetric | Asymmetric |
| Eye glow intensity | 100% | 150-200% |
| Expression range | Gentle to joyful | Confident to mischievous |
| Dominant expressions | JOY_03, WONDER_02 | PUNK_01-03, JOY_06 |
| Emissive pattern | Soft pulse | Sharp pulse |
| Semantic triggers | Nurturing, wonder | Challenge, defiance |

### 6.2 Persona Switching Triggers

| Trigger Type | Condition | Target Persona |
|-------------|-----------|----------------|
| Semantic | Challenge/confrontational language | Punk |
| Emotional | High confidence + playfulness | Punk |
| User | Explicit request | Either |
| Context | Competitive interaction | Punk |
| Default | Neutral/nurturing context | Primary |

---

*Document continues in comprehensive expression catalogue...*

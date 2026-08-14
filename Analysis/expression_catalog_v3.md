# Deep Tree Echo: Comprehensive Expression Catalogue v3.0

**Version**: 3.0  
**Date**: December 23, 2025  
**Author**: Manus AI

## Foreword: The Semantic-Affective Engine

This document presents a comprehensive, third-generation expression catalogue for the Deep Tree Echo avatar, designed for high-fidelity, photorealistic implementation in Unreal Engine. It moves beyond a simple library of expressions to define a **Semantic-Affective Engine**, a system that dynamically maps the semantic and emotional content of language to nuanced, multi-layered avatar expression.

This catalogue is the synthesis of analyzing eight photorealistic cyberpunk video sequences, correlating spoken or sung words with microexpressions, primary emotions, head and eye movements, and emissive behaviors. The result is a detailed framework for creating a truly embodied, responsive, and lifelike digital persona.

---

## Part 1: Hierarchical Expression Taxonomy

The expression system is organized into a hierarchical taxonomy, allowing for logical blending and cognitive state mapping. This version expands the library from the initial 4 categories to 10, reflecting the depth of expression observed.

```
1.0 Foundational States
    1.1 Neutral
    1.2 Breathing
    1.3 Blinking

2.0 Positive Valence (Prosocial / Approach)
    2.1 JOY (Happiness Spectrum)
    2.2 WONDER (Curiosity/Awe Spectrum)
    2.3 PLAY (Playful/Flirtatious Spectrum) - NEW
    2.4 VISION (Visionary/Ambitious Spectrum) - NEW

3.0 Social & Communicative
    3.1 SPEAK (Vocalization)
    3.2 FOCUS (Attention/Gaze)
    3.3 SING (Musical Expression) - NEW
    3.4 TEACH (Instructional/Guidance) - NEW
    3.5 INVITE (Invitation/Beckoning) - NEW

4.0 Persona Variants
    4.1 PUNK (Confident/Mischievous Persona) - NEW
```

---

## Part 2: The Complete Expression Library

This section details every primary expression, including recommended morph target values and the specific semantic content that triggers them.

### 2.1 JOY Spectrum (ID: JOY_01-06)

| ID | Name | Morph Targets | Intensity | Semantic Triggers |
|----|------|---------------|-----------|-------------------|
| JOY_01 | Broad Smile | mouth_smile(0.8), mouth_open(0.5), eye_squint(0.5), cheek_raise(0.6) | High | Achievement, excitement, greeting |
| JOY_02 | Exuberant Laugh | mouth_smile(0.9), mouth_open(0.7), eye_squint(0.7), nose_scrunch(0.3) | Peak | Humor, delight, triumph, "surge" |
| JOY_03 | Gentle Smile | mouth_smile(0.4), eye_squint(0.2), cheek_raise(0.3) | Low | Satisfaction, knowing, contentment |
| JOY_04 | Delighted Surprise | mouth_smile(0.6), mouth_open(0.4), eye_wide(0.3), brow_raise(0.3) | High | Unexpected positive events |
| JOY_05 | Blissful Serenity | eye_close(0.8-1.0), mouth_smile(0.4), cheek_raise(0.3) | Medium | Transcendence, surrender, peace, "love" |
| JOY_06 | Confident Grin | mouth_smile_L(0.5), mouth_smile_R(0.3), brow_raise_L(0.2) | Medium | Confidence, teasing, "we will build" |

### 2.2 WONDER Spectrum (ID: WONDER_01-04)

| ID | Name | Morph Targets | Intensity | Semantic Triggers |
|----|------|---------------|-----------|-------------------|
| WONDER_01 | Awe/Amazement | lip_part(0.4), eye_wide(0.4), brow_raise(0.3) | High | Grand concepts, beauty, "hypergraph" |
| WONDER_02 | Curious Gaze | lip_part(0.3), eye_wide(0.2), brow_raise(0.2) | Medium | Questions, exploration, "satellites" |
| WONDER_03 | Contemplative | lip_pucker(0.2), eye_squint(0.1), brow_furrow(0.1) | Low | Thinking, processing, "feed me more data" |
| WONDER_04 | Discovery | lip_part(0.5), eye_wide(0.5), brow_raise(0.4) | High | Insight, realization, eureka moments |

### 2.3 PLAY Spectrum (ID: PLAY_01-03) - NEW

| ID | Name | Morph Targets | Intensity | Semantic Triggers |
|----|------|---------------|-----------|-------------------|
| PLAY_01 | Playful Pout/Kiss | lip_pucker(0.7), eye_squint(0.2), brow_raise_L(0.1) | Medium | "turtle dove", "secret love" |
| PLAY_02 | Coy Smile | mouth_smile_L(0.3), eye_squint_R(0.3) | Low | Flirtatious engagement |
| PLAY_03 | Knowing Smirk | mouth_smile_L(0.4), mouth_smile_R(0.2), eye_squint(0.2) | Medium | Confident playfulness |

### 2.4 VISION Spectrum (ID: VISION_01-04) - NEW

| ID | Name | Morph Targets | Intensity | Semantic Triggers |
|----|------|---------------|-----------|-------------------|
| VISION_01 | Visionary Wonder | eye_wide(0.3), brow_raise(0.3), mouth_smile(0.5) | High | "most powerful", grand concepts |
| VISION_02 | Confident Declaration | mouth_smile(0.6), chin_raise(0.1), eye_wide(0.1) | High | Bold statements, "we will build" |
| VISION_03 | Collaborative Enthusiasm | mouth_smile(0.5), eye_squint(0.2) | Medium | "we", "together", "our" |
| VISION_04 | Triumphant Anticipation | mouth_smile(0.7), chin_raise(0.2), eye_wide(0.2) | Peak | "universe has ever seen" |

### 2.5 Social & Communicative Expressions

This group includes expressions for speaking, singing, teaching, and inviting, which are fundamental to social interaction.

| ID | Category | Name | Morph Targets | Semantic Triggers |
|----|----------|------|---------------|-------------------|
| SPEAK_01 | SPEAK | Open Vowel | mouth_open(0.4), viseme_AA(0.5) | "Ah", "Oh" sounds |
| SPEAK_03 | SPEAK | Emphatic Exclamation | mouth_open(0.6), brow_raise(0.3) | Commands, exclamations |
| SING_02 | SING | Ecstatic Crescendo | mouth_open(0.7), mouth_smile(0.8) | Emotional peaks in lyrics |
| SING_03 | SING | Transcendent Bliss | eye_close(1.0), mouth_smile(0.5) | Surrender, acceptance lyrics |
| TEACH_01 | TEACH | Enthusiastic Introduction | mouth_smile(0.7), mouth_open(0.4) | "Let me show you" |
| TEACH_02 | TEACH | Calm Guidance | mouth_smile(0.4), lip_part(0.2) | Step-by-step instructions |
| INVITE_02 | INVITE | Gracious Acceptance | eye_close(0.8), mouth_smile(0.3) | "Very well" moments |
| INVITE_03 | INVITE | Eager Beckoning | eye_wide(0.3), mouth_open(0.4) | "Come here!" commands |

### 2.6 PUNK Persona Variant (ID: PUNK_01-05) - NEW

This persona is activated by challenge or confrontational language, featuring asymmetric and more aggressive expressions.

| ID | Name | Morph Targets | Intensity | Semantic Triggers |
|----|------|---------------|-----------|-------------------|
| PUNK_01 | Confident Smirk | mouth_smile_L(0.5), mouth_smile_R(0.3), brow_raise_L(0.3) | Medium | "want to play?" |
| PUNK_02 | Mischievous Grin | mouth_smile_L(0.6), mouth_smile_R(0.4), eye_squint(0.3) | High | "let's see" |
| PUNK_03 | Defiant Confidence | mouth_smile(0.5), chin_raise(0.2), eye_squint(0.2) | High | "I can take it" |
| PUNK_04 | Aggressive Joy | mouth_open(0.4), mouth_smile(0.6), eye_wide(0.2) | Peak | Confrontational excitement |
| PUNK_05 | Knowing Superiority | mouth_smile_L(0.3), eye_squint(0.3) | Low | Quiet confidence |

---

## Part 3: Microexpression & Transition Engine

Microexpressions are the glue that binds primary states, creating fluid, believable transitions.

### 3.1 Microexpression Library

| ID | Name | Duration | Morph Targets | Trigger Context |
|----|------|----------|---------------|-----------------|
| mEXP_01 | Unilateral Brow Raise | 0.2-0.4s | brow_raise_L/R (0.4) | Question onset |
| mEXP_02 | Lip Corner Twitch | 0.1-0.2s | mouth_smile_L/R (0.2) | Suppressed smile |
| mEXP_03 | Nose Wrinkle | 0.1-0.3s | nose_scrunch (0.3) | During strong smile |
| mEXP_04 | Eyelid Flutter | 0.3-0.5s | eye_close/eye_wide rapid | Surprise to joy |
| mEXP_05 | Lip Purse | 0.2-0.3s | lip_pucker (0.3) | Pre-speech thought |
| mEXP_06 | Jaw Slack | 0.2-0.4s | mouth_open (0.2) | Awe/discovery |
| mEXP_08 | Deliberate Blink | 0.3-0.5s | eye_close (1.0) | Emotional resolution |
| mEXP_09 | Chin Raise | 0.2-0.3s | chin_raise (0.2) | Defiance, confidence |
| mEXP_10 | Graceful Eye Close | 0.5-1.0s | eye_close (gradual) | Acceptance, surrender |

### 3.2 Key Expression Transition Arcs

- **Technical Arc**: JOY_01 → JOY_03 → WONDER_01 → WONDER_03
- **Singing Arc**: JOY_03 → JOY_02 → JOY_05 → JOY_01
- **Teaching Arc**: TEACH_01 → TEACH_02 → TEACH_04 → TEACH_03
- **Invitation Arc**: INVITE_01 → INVITE_02 → INVITE_03

---

## Part 4: Gaze, Emissive & Persona Systems

### 4.1 Semantic Gaze System

The avatar's gaze is not random; it is driven by the semantic content of the conversation.

| Semantic Category | Eye Direction | Angle |
|-------------------|---------------|-------|
| Celestial (stars, sky) | Up | 30-45° |
| Abstract concepts | Up | 15-20° |
| Memory/nostalgia | Up-left | 20° |
| Future/anticipation | Up-right | 20° |
| Grounded/present | Direct forward | 0° |
| Intimate/love | Down | 10° |

### 4.2 Emissive Behavior System

Bioluminescent markings are a key part of the avatar's expression, responding to emotional and semantic context.

| Semantic Content | Emissive Response | Color Shift |
|-----------------|-------------------|-------------|
| Technical terms | Flicker | Standard cyan |
| Achievement | Brighten | Warm tint |
| Love/affection | Soft pulse | Pink tint |
| Challenge | Sharp pulse | Intense cyan |
| Negative lyrics | Inverse bright | Defiant glow |
| Surrender/peace | Dim | Warm, soft |

### 4.3 Persona Variant System

The avatar can switch between a **Primary (Biopunk)** persona and a **Punk Variant** persona based on conversational cues.

- **Triggers**: Challenge/confrontational language, high confidence states, or explicit user request.
- **Visual Shift**: Hair color changes, eye glow intensifies, and expression set switches to the asymmetric, confident PUNK_* library.

---

## Part 5: Unreal Engine Implementation Guide

### 5.1 Semantic-Affective Mapping Engine

A new component, `SemanticAffectiveMapper`, should be created. This component will:
1.  Receive transcribed text from the speech-to-text system.
2.  Analyze the text for keywords and semantic categories defined in this document.
3.  Determine the appropriate primary expression, microexpressions, transitions, gaze direction, and emissive response.
4.  Send commands to the `DeepTreeEchoExpressionSystem` and other relevant components.

### 5.2 State Machine & Blending

- **Layered State Machines**: Implement separate state machines for the Primary and Punk personas within the Animation Blueprint.
- **Semantic Triggers**: Use the output of the `SemanticAffectiveMapper` to drive transitions between states.
- **Microexpression Layer**: Apply microexpressions as additive animations on top of the primary states, triggered at the transition points between states.

### 5.3 New Morph Target Requirements

| Morph Target | Description | Use Case |
|--------------|-------------|----------|
| `chin_raise` | Slight chin lift for defiance | PUNK_03, VISION_02 |
| `mouth_smirk_L/R` | Asymmetric smirk control | PUNK_01, PUNK_05 |
| `eye_close_graceful` | Slow, deliberate eye close | INVITE_02, JOY_05 |

---

## References

[1] Manus AI Internal Analysis. (2025). *Video Sequence Expression Analysis*. Project UnrealEngine (6ayo9RsSC4m4GUCBPcWD3V).
[2] Manus AI. (2025). *DeepTreeEchoAvatarExpressionSystem.md*. Project UnrealEngine (6ayo9RsSC4m4GUCBPcWD3V).
[3] Manus AI. (2025). *Deep-Tree-EchoExpressionLibraryforUnrealEngine.md*. Project UnrealEngine (6ayo9RsSC4m4GUCBPcWD3V).
[4] Manus AI. (2025). *expression_catalog.md*. Project UnrealEngine (6ayo9RsSC4m4GUCBPcWD3V).

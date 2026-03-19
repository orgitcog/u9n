# Deep Tree Echo Expression Catalog — FACS Complete

**Version**: 4.0 (FACS-Complete)
**Date**: March 3, 2026
**Composition**: `echo-angel ( facs ( gap-analysis-completion ) )`

This document completes the gap analysis from the DTE FACS Catalog by adding negative emotion spectra, standard FACS AU numbering, MH Standards integration, AU intensity scoring, laterality notation, and M-codes for head/eye movements.

---

## 1. Negative Emotion Spectra

### 1.1 FEAR Spectrum (4 expressions)

| ID | Name | FACS AUs | Intensity | MetaHuman Curves | Morph Targets |
|----|------|----------|-----------|------------------|---------------|
| FEAR_01 | Startle | AU1D+AU2D+AU5D+AU20B+AU26C | High onset | browRaiseInnerL/R, browRaiseOuterL/R, eyeWidenL/R, mouthStretchL/R, jawOpen | brow_raise(0.8), eye_wide(0.9), mouth_stretch(0.4), mouth_open(0.6) |
| FEAR_02 | Anxious Dread | AU1C+AU4B+AU5B+AU7B+AU20A+AU25B | Sustained | browRaiseInnerL/R, browDownL/R, eyeUpperLidUpL/R, eyeSquintInnerL/R, mouthStretchL/R, mouthLipsPartL/R | brow_raise(0.6), brow_furrow(0.4), eye_wide(0.4), eye_squint(0.3), lip_part(0.4) |
| FEAR_03 | Frozen Terror | AU1D+AU2D+AU4C+AU5E+AU7C+AU20C+AU26D | Maximum | browRaiseInnerL/R, browRaiseOuterL/R, browDownL/R, eyeWidenL/R, eyeSquintInnerL/R, mouthStretchL/R, jawOpen | brow_raise(0.9), brow_furrow(0.6), eye_wide(1.0), eye_squint(0.6), mouth_stretch(0.7), mouth_open(0.8) |
| FEAR_04 | Worried Concern | AU1B+AU4A+AU5A+AU7A+AU25A | Subtle | browRaiseInnerL/R, browDownL/R, eyeUpperLidUpL/R, eyeSquintInnerL/R, mouthLipsPartL/R | brow_raise(0.3), brow_furrow(0.2), eye_wide(0.2), lip_part(0.2) |

### 1.2 ANGER Spectrum (4 expressions)

| ID | Name | FACS AUs | Intensity | MetaHuman Curves | Morph Targets |
|----|------|----------|-----------|------------------|---------------|
| ANGER_01 | Irritation | AU4B+AU7B+AU23A+AU24A | Subtle | browDownL/R, browLateralL/R, eyeSquintInnerL/R, mouthLipsTightenUL/R, mouthPressDL/R | brow_furrow(0.4), eye_squint(0.3), lip_press(0.2) |
| ANGER_02 | Controlled Fury | AU4C+AU5B+AU7C+AU23B+AU24B | Moderate | browDownL/R, eyeUpperLidUpL/R, eyeSquintInnerL/R, mouthLipsTightenUL/R, mouthPressDL/R | brow_furrow(0.6), eye_wide(0.3), eye_squint(0.6), lip_press(0.4) |
| ANGER_03 | Rage | AU4D+AU5C+AU7D+AU9C+AU10C+AU23C+AU25C+AU26C | Maximum | browDownL/R, eyeUpperLidUpL/R, eyeSquintInnerL/R, noseWrinkleL/R, mouthUpperLipRaiseL/R, mouthLipsTightenUL/R, jawOpen | brow_furrow(0.8), eye_wide(0.5), eye_squint(0.8), nose_scrunch(0.6), mouth_open(0.6), lip_press(0.6) |
| ANGER_04 | Determined Resolve | AU4B+AU7B+AU17A+AU24A | Contained | browDownL/R, eyeSquintInnerL/R, jawChinRaiseDL/R, mouthPressDL/R | brow_furrow(0.4), eye_squint(0.3), chin_raise(0.2), lip_press(0.2) |

### 1.3 SADNESS Spectrum (4 expressions)

| ID | Name | FACS AUs | Intensity | MetaHuman Curves | Morph Targets |
|----|------|----------|-----------|------------------|---------------|
| SAD_01 | Melancholy | AU1B+AU4A+AU15A+AU17A | Subtle | browRaiseInnerL/R, browDownL/R, mouthCornerDepressL/R, jawChinRaiseDL/R | brow_raise(0.3), brow_furrow(0.2), mouth_frown(0.2), chin_raise(0.2) |
| SAD_02 | Grief | AU1C+AU4B+AU6B+AU11B+AU15C+AU17B | Deep | browRaiseInnerL/R, browDownL/R, eyeCheekRaiseL/R, noseNasolabialDeepenL/R, mouthCornerDepressL/R, jawChinRaiseDL/R | brow_raise(0.6), brow_furrow(0.4), cheek_raise(0.3), mouth_frown(0.6), chin_raise(0.4) |
| SAD_03 | Crying | AU1C+AU4B+AU6C+AU7C+AU11B+AU15C+AU17C+AU25B | Intense | browRaiseInnerL/R, browDownL/R, eyeCheekRaiseL/R, eyeSquintInnerL/R, mouthCornerDepressL/R, jawChinRaiseDL/R, mouthLipsPartL/R | brow_raise(0.6), brow_furrow(0.4), cheek_raise(0.6), eye_squint(0.6), mouth_frown(0.6), chin_raise(0.6), lip_part(0.4) |
| SAD_04 | Disappointed | AU1A+AU15A+AU17A+AU25A | Minimal | browRaiseInnerL/R, mouthCornerDepressL/R, jawChinRaiseDL/R, mouthLipsPartL/R | brow_raise(0.2), mouth_frown(0.2), chin_raise(0.2), lip_part(0.2) |

### 1.4 DISGUST Spectrum (3 expressions)

| ID | Name | FACS AUs | Intensity | MetaHuman Curves | Morph Targets |
|----|------|----------|-----------|------------------|---------------|
| DISG_01 | Mild Distaste | AU9B+AU15A+AU17A | Subtle | noseWrinkleL/R, mouthCornerDepressL/R, jawChinRaiseDL/R | nose_scrunch(0.3), mouth_frown(0.2), chin_raise(0.2) |
| DISG_02 | Revulsion | AU9C+AU10C+AU15B+AU17B+AU25B+AU26A | Moderate | noseWrinkleL/R, mouthUpperLipRaiseL/R, mouthCornerDepressL/R, jawChinRaiseDL/R, jawOpen | nose_scrunch(0.6), lip_raise(0.6), mouth_frown(0.4), chin_raise(0.4), mouth_open(0.2) |
| DISG_03 | Contemptuous Disgust | R9B+R14B+AU15A+AU17A | Asymmetric | noseWrinkleR, mouthDimpleR, mouthCornerDepressL/R, jawChinRaiseDL/R | nose_scrunch_R(0.3), mouth_smirk_R(0.4), mouth_frown(0.2), chin_raise(0.2) |

---

## 2. New Morph Targets Required

The negative emotion spectra require the following new morph targets not present in the current repo:

| New Morph Target | FACS AU | AU Name | MetaHuman Curve(s) |
|------------------|---------|---------|-------------------|
| `mouth_frown` / `mouth_frown_L/R` | AU15 | Lip corner depressor | mouthCornerDepressL/R |
| `chin_raise` / `chin_raise_L/R` | AU17 | Chin raiser | jawChinRaiseDL/R |
| `lip_press` / `lip_press_L/R` | AU24 | Lip pressor | mouthPressDL/R |
| `lip_tighten` / `lip_tighten_L/R` | AU23 | Lip tightener | mouthLipsTightenUL/R |
| `lip_raise` / `lip_raise_L/R` | AU10 | Upper lip raiser | mouthUpperLipRaiseL/R |
| `mouth_stretch` / `mouth_stretch_L/R` | AU20 | Lip stretcher | mouthStretchL/R |
| `nasolabial_deepen` / `nasolabial_deepen_L/R` | AU11 | Nasolabial deepener | noseNasolabialDeepenL/R |

---

## 3. AU Intensity Scoring Standard

All morph target float values (0.0–1.0) map to FACS intensity grades:

| Float Range | FACS Grade | Description |
|-------------|------------|-------------|
| 0.01–0.20 | A (Trace) | Barely perceptible |
| 0.21–0.40 | B (Slight) | Noticeable but subtle |
| 0.41–0.60 | C (Marked) | Clearly visible |
| 0.61–0.80 | D (Severe) | Strong, pronounced |
| 0.81–1.00 | E (Maximum) | Extreme, full activation |

---

## 4. Laterality Notation Standard

All expressions should use the following laterality modifiers:

| Modifier | Meaning | Example |
|----------|---------|---------|
| (none) | Bilateral symmetric | AU12C = both sides at marked intensity |
| L | Left only | AU12CL = left lip corner pull at marked |
| R | Right only | AU12CR = right lip corner pull at marked |
| (asym) | Bilateral asymmetric | AU12B(asym) = both sides, different intensities |
| U | Unilateral (one side only) | AU2BU = one brow raised |

---

## 5. M-Code Temporal Context

Head and eye movements should use M-codes for temporal context:

| M-Code | Movement | Application |
|--------|----------|-------------|
| M55 | Head tilt left | WONDER_03 contemplation, PLAY_02 coy |
| M56 | Head tilt right | TEACH_02 guidance, PLAY_03 knowing |
| M57 | Head forward | ANGER_02 controlled fury, FOCUS_01 alert |
| M59 | Head back | FEAR_01 startle, WONDER_01 awe |
| M60 | Head turn left/right | FOCUS_03 side glance, PUNK_01 confident |
| M61 | Eyes turn left | FOCUS_03 side glance |
| M62 | Eyes turn right | PLAY_02 coy smile |
| M68 | Eyes up | FOCUS_02 upward attention, WONDER_02 curious |
| M69 | Eyes down | SAD_01 melancholy, TEACH_02 calm guidance |
| M83 | Head nod | TEACH_03 encouraging feedback |

---

## 6. Complete MH Standards Integration Map

Every repo morph target now maps to specific MH Standards control curves with full L/R decomposition:

| Repo Morph Target | FACS AU | MH Standards Curves (Full) |
|-------------------|---------|---------------------------|
| `mouth_smile(v)` | AU12 | mouthCornerPullL(v), mouthCornerPullR(v) |
| `cheek_raise(v)` | AU6 | eyeCheekRaiseL(v), eyeCheekRaiseR(v) |
| `eye_squint(v)` | AU7 | eyeSquintInnerL(v), eyeSquintInnerR(v) |
| `brow_raise(v)` | AU1+AU2 | browRaiseInnerL(v*0.6), browRaiseInnerR(v*0.6), browRaiseOuterL(v*0.4), browRaiseOuterR(v*0.4) |
| `brow_furrow(v)` | AU4 | browDownL(v*0.7), browDownR(v*0.7), browLateralL(v*0.3), browLateralR(v*0.3) |
| `mouth_open(v)` | AU26 | jawOpen(v) |
| `lip_part(v)` | AU25 | mouthLipsPartL(v), mouthLipsPartR(v) |
| `eye_wide(v)` | AU5 | eyeUpperLidUpL(v*0.6), eyeUpperLidUpR(v*0.6), eyeWidenL(v*0.4), eyeWidenR(v*0.4) |
| `eye_closed(v)` | AU43 | eyeBlinkL(v), eyeBlinkR(v) |
| `lip_pucker(v)` | AU18 | mouthLipsPurseUL(v*0.5), mouthLipsPurseDL(v*0.5), mouthLipsTowardsUL(v*0.3), mouthLipsTowardsDL(v*0.3) |
| `mouth_funnel(v)` | AU22 | mouthFunnelDL(v*0.5), mouthFunnelUL(v*0.5) |
| `nose_scrunch(v)` | AU9 | noseWrinkleL(v), noseWrinkleR(v) |
| `mouth_smirk_L(v)` | AU14L | mouthDimpleL(v) |
| `mouth_stretch(v)` | AU20 | mouthStretchL(v), mouthStretchR(v) |
| `mouth_frown(v)` | AU15 | mouthCornerDepressL(v), mouthCornerDepressR(v) |
| `chin_raise(v)` | AU17 | jawChinRaiseDL(v), jawChinRaiseDR(v) |
| `lip_press(v)` | AU24 | mouthPressDL(v), mouthPressDR(v) |
| `lip_tighten(v)` | AU23 | mouthLipsTightenUL(v), mouthLipsTightenUR(v) |
| `lip_raise(v)` | AU10 | mouthUpperLipRaiseL(v), mouthUpperLipRaiseR(v) |

---

## 7. Updated Emotion-to-Expression-to-FACS Lookup

| Basic Emotion | FACS AUs | DTE Expression(s) | Status |
|---------------|----------|-------------------|--------|
| Happiness | AU6+AU12 | JOY_01–06, SING_01–04, TEACH_01–03 | Implemented |
| Surprise | AU1+AU2+AU5B+AU26 | WONDER_01, WONDER_04, JOY_04 | Implemented |
| Fear | AU1+AU2+AU4+AU5+AU7+AU20+AU26 | FEAR_01–04 | **NEW** |
| Anger | AU4+AU5+AU7+AU23+AU24 | ANGER_01–04 | **NEW** |
| Sadness | AU1+AU4+AU15+AU17 | SAD_01–04 | **NEW** |
| Disgust | AU9+AU10+AU15+AU17 | DISG_01–03 | **NEW** |
| Contempt | R12A+R14A | PUNK_01, PUNK_05 | Implemented |

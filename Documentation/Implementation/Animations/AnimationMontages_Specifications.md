# Deep Tree Echo Avatar - Animation Montages Specifications

**Asset Type:** Animation Montages and Animation Library  
**Purpose:** Gesture and emote animations for personality expression  
**Integration:** Triggered by AvatarAnimInstance and PersonalityTraitSystem

---

## Overview

Animation montages provide the avatar with a rich vocabulary of gestures and emotes that express personality, emotion, and cognitive state. These animations are triggered dynamically based on context, personality traits, and emotional state, creating a living, expressive character.

---

## Animation Categories

### 1. Idle Gestures
**Purpose:** Subtle movements during idle state to maintain liveliness

| Animation Name | Duration | Description | Personality Trigger |
|----------------|----------|-------------|---------------------|
| AM_Idle_ThinkingPose | 3s | Hand on chin, contemplative | High IntellectualCuriosity |
| AM_Idle_HairTuck | 2s | Tucks hair behind ear | High Flirtiness |
| AM_Idle_ArmsCrossed | 4s | Crosses arms, confident stance | High Confidence |
| AM_Idle_LookAround | 5s | Looks around curiously | Medium Confidence |
| AM_Idle_ShiftWeight | 2s | Shifts weight from one foot to another | Neutral |
| AM_Idle_StretchArms | 4s | Stretches arms above head | Low CognitiveLoad |
| AM_Idle_Fidget | 3s | Fidgets with hands nervously | High EmotionalVolatility |
| AM_Idle_PlayWithHair | 4s | Twirls hair around finger | High Flirtiness |

### 2. Emotional Emotes
**Purpose:** Express specific emotional states

| Animation Name | Duration | Description | Emotion Trigger |
|----------------|----------|-------------|-----------------|
| AM_Emote_Laugh | 2s | Genuine laughter, hand to mouth | Happy (>0.8) |
| AM_Emote_Giggle | 1.5s | Soft giggle, shy | Flirty |
| AM_Emote_Sigh | 2s | Deep sigh, shoulders drop | Sad |
| AM_Emote_Gasp | 1s | Surprised gasp, hand to chest | Surprised |
| AM_Emote_Pout | 2s | Pouts lips, looks away | Sad/Disappointed |
| AM_Emote_Wink | 0.5s | Playful wink | Flirty |
| AM_Emote_Blush | 2s | Covers face with hands | Embarrassed |
| AM_Emote_Facepalm | 2s | Hand to forehead | Frustrated |
| AM_Emote_Shrug | 1.5s | Shrugs shoulders | Confused/Neutral |
| AM_Emote_Nod | 1s | Nods head in agreement | Agreeing |
| AM_Emote_Shake | 1s | Shakes head in disagreement | Disagreeing |

### 3. Communicative Gestures
**Purpose:** Support conversation and communication

| Animation Name | Duration | Description | Context Trigger |
|----------------|----------|-------------|-----------------|
| AM_Gesture_Wave | 2s | Friendly wave | Greeting |
| AM_Gesture_Point | 1.5s | Points at something | Indicating |
| AM_Gesture_Beckon | 2s | Beckons with hand | Inviting |
| AM_Gesture_ThumbsUp | 1.5s | Thumbs up gesture | Approval |
| AM_Gesture_ThumbsDown | 1.5s | Thumbs down gesture | Disapproval |
| AM_Gesture_Explain | 4s | Hand gestures while explaining | Speaking |
| AM_Gesture_CountFingers | 3s | Counts on fingers | Listing items |
| AM_Gesture_Shush | 2s | Finger to lips | Requesting quiet |
| AM_Gesture_ComeHere | 2s | Beckons with both hands | Calling someone |
| AM_Gesture_Stop | 1.5s | Hand up in stop gesture | Halting |

### 4. Personality-Driven Actions
**Purpose:** Express specific personality traits

| Animation Name | Duration | Description | Personality Trait |
|----------------|----------|-------------|-------------------|
| AM_Personality_ConfidentStance | 3s | Stands tall, hands on hips | High Confidence |
| AM_Personality_ShyLook | 2s | Looks down, fidgets | Low Confidence |
| AM_Personality_FlirtySway | 4s | Sways hips subtly | High Flirtiness |
| AM_Personality_IntenseStare | 3s | Intense eye contact | High Confidence + Flirtiness |
| AM_Personality_ThinkingGesture | 5s | Paces, hand on chin | High IntellectualCuriosity |
| AM_Personality_ChaoticTwitch | 1s | Sudden jerky movement | High ChaosFactor |
| AM_Personality_GracefulPose | 4s | Elegant, balanced pose | Low ChaosFactor |

### 5. Cognitive State Animations
**Purpose:** Visualize cognitive processes

| Animation Name | Duration | Description | Cognitive Trigger |
|----------------|----------|-------------|-------------------|
| AM_Cognitive_Processing | 3s | Looks up, eyes unfocused | High CognitiveLoad |
| AM_Cognitive_Eureka | 2s | Sudden realization, finger snap | Problem solved |
| AM_Cognitive_Confused | 3s | Tilts head, scratches head | Confusion |
| AM_Cognitive_Concentrating | 5s | Narrows eyes, leans forward | Deep focus |
| AM_Cognitive_Overwhelmed | 3s | Holds head, stressed | Very High CognitiveLoad |
| AM_Cognitive_Relaxed | 4s | Relaxed posture, deep breath | Low CognitiveLoad |

---

## Animation Montage Structure

### Standard Montage Setup

Each animation montage should follow this structure:

```
Animation Montage: AM_[Category]_[Name]

Sections:
1. Intro (0.2-0.5s): Blend in from current pose
2. Main (1-4s): Core animation
3. Outro (0.2-0.5s): Blend out to idle

Blend Settings:
- Blend In Time: 0.2s
- Blend Out Time: 0.2s
- Blend Mode: Cubic

Notify Tracks:
- AnimNotify_GestureStart (at 0s)
- AnimNotify_GestureEnd (at end)
- AnimNotify_AudioCue (at appropriate timing)
```

### Animation Slots

Organize animations into slots for layering:

- **FullBody:** Complete body animations (locomotion, major gestures)
- **UpperBody:** Upper body only (gestures, emotes)
- **FacialExpression:** Facial blend shapes
- **LeftHand:** Left hand gestures
- **RightHand:** Right hand gestures
- **Head:** Head movements (nods, shakes, tilts)

---

## Integration with Animation Blueprint

### Montage Playback System

In `ABP_DeepTreeEcho_Avatar`:

```
Event Graph:

// Play montage based on personality and emotion
Function: PlayPersonalityGesture
    Input: PersonalityTrait (Confidence, Flirtiness, etc.)
    Logic:
        - Check current state (Idle, Walking, etc.)
        - Select appropriate gesture from library
        - Check cooldown (don't spam gestures)
        - Play montage on appropriate slot
        - Trigger audio cue
        
Function: PlayEmotionalEmote
    Input: EmotionalState, Intensity
    Logic:
        - Map emotion to emote animation
        - Adjust playback speed based on intensity
        - Play montage
        - Trigger facial expression
        - Trigger material effects (blush, etc.)
```

### Gesture Selection Logic

```cpp
// In AvatarAnimInstance.cpp
void UAvatarAnimInstance::SelectAndPlayGesture(float DeltaTime)
{
    // Don't play gesture if already playing one
    if (IsAnyMontagePlaying())
    {
        return;
    }
    
    // Check cooldown
    GestureCooldownTimer -= DeltaTime;
    if (GestureCooldownTimer > 0.0f)
    {
        return;
    }
    
    // Select gesture based on personality
    UAnimMontage* SelectedGesture = nullptr;
    
    if (Confidence > 0.7f && FMath::FRand() < 0.3f)
    {
        SelectedGesture = ConfidentGestures[FMath::RandRange(0, ConfidentGestures.Num() - 1)];
    }
    else if (Flirtiness > 0.6f && FMath::FRand() < 0.4f)
    {
        SelectedGesture = FlirtyGestures[FMath::RandRange(0, FlirtyGestures.Num() - 1)];
    }
    else if (IntellectualCuriosity > 0.7f && FMath::FRand() < 0.2f)
    {
        SelectedGesture = ThinkingGestures[FMath::RandRange(0, ThinkingGestures.Num() - 1)];
    }
    
    if (SelectedGesture)
    {
        PlayMontage(SelectedGesture);
        GestureCooldownTimer = FMath::FRandRange(5.0f, 15.0f);  // Random cooldown
    }
}
```

---

## Animation Notify System

### Custom Animation Notifies

Create these custom notifies for enhanced integration:

#### AN_TriggerAudioCue
```cpp
// Triggers audio at specific point in animation
UCLASS()
class UAnimNotify_TriggerAudioCue : public UAnimNotify
{
    UPROPERTY(EditAnywhere)
    USoundBase* AudioCue;
    
    UPROPERTY(EditAnywhere)
    float Volume = 1.0f;
    
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};
```

#### AN_TriggerParticleEffect
```cpp
// Spawns particle effect at specific point
UCLASS()
class UAnimNotify_TriggerParticleEffect : public UAnimNotify
{
    UPROPERTY(EditAnywhere)
    UNiagaraSystem* ParticleSystem;
    
    UPROPERTY(EditAnywhere)
    FName SocketName;
    
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};
```

#### AN_TriggerMaterialEffect
```cpp
// Triggers material effect (blush, sparkle, etc.)
UCLASS()
class UAnimNotify_TriggerMaterialEffect : public UAnimNotify
{
    UPROPERTY(EditAnywhere)
    FName ParameterName;
    
    UPROPERTY(EditAnywhere)
    float ParameterValue;
    
    UPROPERTY(EditAnywhere)
    float Duration = 1.0f;
    
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) override;
};
```

---

## Facial Expression Integration

### Morph Target Control

Each animation montage can include facial expression curves:

```
Morph Target Curves (in Animation Sequence):
- Smile_L: 0.0 → 0.8 → 0.0
- Smile_R: 0.0 → 0.8 → 0.0
- Brow_Raise_L: 0.0 → 0.5 → 0.0
- Brow_Raise_R: 0.0 → 0.5 → 0.0
- Eye_Squint_L: 0.0 → 0.3 → 0.0
- Eye_Squint_R: 0.0 → 0.3 → 0.0
```

### Example: Laugh Animation
```
AM_Emote_Laugh:
Time 0.0s:
    - Smile: 0.0
    - Mouth_Open: 0.0
    - Eye_Squint: 0.0

Time 0.5s:
    - Smile: 1.0
    - Mouth_Open: 0.7
    - Eye_Squint: 0.6

Time 1.5s:
    - Smile: 0.9
    - Mouth_Open: 0.5
    - Eye_Squint: 0.5

Time 2.0s:
    - Smile: 0.3
    - Mouth_Open: 0.0
    - Eye_Squint: 0.0
```

---

## Dynamic Animation Modulation

### Playback Speed Modulation

Adjust animation speed based on emotional intensity:

```cpp
void UAvatarAnimInstance::PlayEmotionalEmote(EEmotionalState Emotion, float Intensity)
{
    UAnimMontage* Emote = GetEmoteForEmotion(Emotion);
    if (!Emote) return;
    
    // Modulate playback speed based on intensity
    float PlaybackSpeed = FMath::Lerp(0.8f, 1.3f, Intensity);
    
    Montage_Play(Emote, PlaybackSpeed);
}
```

### Blend Weight Modulation

Blend between animations based on state:

```cpp
// In Animation Blueprint
float ConfidentBlendWeight = Confidence;
float ShyBlendWeight = 1.0f - Confidence;

// Use in blend nodes
Blend Pose by Float:
    A: ConfidentIdlePose (Weight: ConfidentBlendWeight)
    B: ShyIdlePose (Weight: ShyBlendWeight)
```

---

## Animation Asset Creation Guide

### Step-by-Step Process

1. **Capture or Create Base Animation**
   - Motion capture (preferred for realism)
   - Hand-animated (for stylized or fantasy gestures)
   - Procedural (for subtle variations)

2. **Import to Unreal Engine**
   - Import FBX with skeletal mesh
   - Ensure bone hierarchy matches avatar skeleton
   - Set up retargeting if needed

3. **Create Animation Sequence**
   - Trim animation to desired length
   - Add root motion if needed
   - Clean up any artifacts

4. **Add Facial Expression Curves**
   - Add morph target curves
   - Key facial expressions to match body language
   - Smooth curves for natural transitions

5. **Create Animation Montage**
   - Create montage from animation sequence
   - Set up sections (Intro, Main, Outro)
   - Configure blend settings
   - Add animation notifies

6. **Test in Animation Blueprint**
   - Play montage in ABP_DeepTreeEcho_Avatar
   - Verify blending with idle/locomotion
   - Check facial expressions
   - Adjust timing and blending as needed

7. **Integrate with Code**
   - Add to appropriate gesture array in AvatarAnimInstance
   - Set up trigger conditions
   - Test dynamic playback

---

## Performance Considerations

### Animation Budgets

- **PC High-End:** 100+ bones, full facial rig, all gestures
- **PC Mid-Range:** 80 bones, simplified facial, reduced gesture library
- **Mobile:** 50 bones, no facial, essential gestures only

### LOD System

```cpp
void UAvatarAnimInstance::UpdateAnimationLOD(float DistanceToCamera)
{
    if (DistanceToCamera < 500.0f)  // Close
    {
        // Full animation system
        bEnableGestures = true;
        bEnableFacialAnimation = true;
        GestureFrequency = 1.0f;
    }
    else if (DistanceToCamera < 1000.0f)  // Medium
    {
        // Reduced gestures
        bEnableGestures = true;
        bEnableFacialAnimation = false;
        GestureFrequency = 0.5f;
    }
    else  // Far
    {
        // No gestures
        bEnableGestures = false;
        bEnableFacialAnimation = false;
    }
}
```

---

## Testing Procedures

### Visual Tests
1. Play each montage individually, verify smooth blending
2. Test montage interruption (play one, then another immediately)
3. Verify facial expressions match body language
4. Check animation notifies trigger correctly

### Integration Tests
1. Trigger gestures from personality system
2. Trigger emotes from emotional state changes
3. Verify cooldown system works
4. Test gesture selection logic

### Performance Tests
1. Measure FPS with multiple montages playing
2. Check bone update cost in profiler
3. Test LOD transitions

---

## Animation Library Summary

### Total Animations Required

- **Idle Gestures:** 8 animations
- **Emotional Emotes:** 11 animations
- **Communicative Gestures:** 10 animations
- **Personality Actions:** 7 animations
- **Cognitive State:** 6 animations

**Total:** 42 animation montages

### Estimated Creation Time

- **With Motion Capture:** 2-3 weeks (capture + cleanup + integration)
- **Hand Animated:** 4-6 weeks (animation + integration)
- **Mixed Approach:** 3-4 weeks (mocap for base, hand-animated for stylized)

---

## Conclusion

This comprehensive animation montage system provides the Deep Tree Echo avatar with a rich vocabulary of expression. Combined with the personality trait system, cognitive visualization, and material effects, these animations create a truly living, expressive character that responds dynamically to its internal state and external context.

**Estimated Implementation Time:** 3-6 weeks (depending on animation creation method)

**Complexity:** High (requires animation expertise)

**Impact:** Critical (defines character expressiveness and personality)

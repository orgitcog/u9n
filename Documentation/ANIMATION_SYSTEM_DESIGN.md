# Deep Tree Echo Animation System Design

## Overview

The Deep Tree Echo animation system provides a sophisticated, layered animation architecture that integrates emotional expression, personality traits, and cognitive state visualization into a coherent, real-time avatar performance system.

## Architecture

### Animation Blueprint: ABP_DeepTreeEcho_Avatar

The main animation blueprint orchestrates all animation layers and state machines.

**Animation Layers:**
1. **Base Locomotion** - Walking, running, idle
2. **Upper Body Gestures** - Hand movements, arm expressions
3. **Facial Animation** - Blend shapes for expressions
4. **Additive Emotional Layer** - Emotional modulation
5. **Procedural Layer** - Breathing, micro-movements
6. **Chaos Layer** - Hyper-chaotic behavior injection

## State Machines

### 1. Locomotion State Machine

**States:**
- Idle
- Walk
- Run
- Turn (Left/Right)
- Jump
- Fall
- Land

**Transitions:**
| From | To | Condition |
|------|-----|-----------|
| Idle | Walk | Speed > 10 |
| Walk | Run | Speed > 300 |
| Run | Walk | Speed < 250 |
| Walk | Idle | Speed < 5 |
| Any | Jump | bIsJumping |
| Jump | Fall | Velocity.Z < -100 |
| Fall | Land | bIsOnGround |
| Land | Idle | Timer > 0.3s |

**Blend Spaces:**
- BS_Idle_Directional (8-direction idle variations)
- BS_Walk_Directional (8-direction walk)
- BS_Run_Directional (8-direction run)

### 2. Upper Body State Machine

**States:**
- Idle_UpperBody
- Gesture_Talking
- Gesture_Thinking
- Gesture_Excited
- Gesture_Confident
- Gesture_Flirty
- Gesture_Chaotic

**Transition Logic:**
- Driven by personality traits and emotional state
- Smooth blending with 0.3s transition time
- Can interrupt current gesture for high-priority emotions

### 3. Facial Expression State Machine

**States:**
- Neutral
- Happy
- Sad
- Angry
- Surprised
- Flirty
- Mysterious
- Chaotic

**Blend Shape Targets:**
| Blend Shape | Neutral | Happy | Sad | Angry | Surprised | Flirty |
|-------------|---------|-------|-----|-------|-----------|--------|
| BrowInnerUp | 0.0 | 0.2 | 0.6 | 0.0 | 0.8 | 0.3 |
| BrowOuterUp | 0.0 | 0.3 | 0.0 | 0.0 | 0.9 | 0.2 |
| BrowDown | 0.0 | 0.0 | 0.4 | 0.8 | 0.0 | 0.0 |
| EyeWide | 0.0 | 0.3 | 0.0 | 0.2 | 0.9 | 0.4 |
| EyeSquint | 0.0 | 0.2 | 0.3 | 0.6 | 0.0 | 0.3 |
| MouthSmile | 0.0 | 0.9 | 0.0 | 0.0 | 0.3 | 0.7 |
| MouthFrown | 0.0 | 0.0 | 0.8 | 0.5 | 0.0 | 0.0 |
| MouthOpen | 0.0 | 0.2 | 0.0 | 0.3 | 0.7 | 0.2 |

## Animation Nodes

### Custom Animation Nodes

#### 1. AN_EmotionalModulation

Modulates base animations with emotional intensity.

**Inputs:**
- Base Pose
- Emotional State
- Modulation Intensity

**Outputs:**
- Modulated Pose

**Logic:**
- Applies additive emotional animations
- Scales animation speed based on arousal
- Adjusts pose strength based on emotion intensity

#### 2. AN_ChaoticBehavior

Injects random, chaotic movements.

**Inputs:**
- Base Pose
- Chaos Factor
- Time

**Outputs:**
- Chaotic Pose

**Logic:**
- Random micro-movements every 0.5-2 seconds
- Sudden head tilts
- Unexpected gesture injections
- Glitch-like animation artifacts

#### 3. AN_ProceduralBreathing

Adds realistic breathing motion.

**Inputs:**
- Base Pose
- Breathing Rate
- Emotional State

**Outputs:**
- Breathing Pose

**Logic:**
- Sine wave chest expansion
- Emotional state affects breathing rate
- Calm: 12 breaths/min
- Excited: 20 breaths/min
- Anxious: 25 breaths/min

#### 4. AN_GazeTracking

Procedurally controls eye and head orientation.

**Inputs:**
- Gaze Target
- Tracking Speed
- Attention Level

**Outputs:**
- Eye Bone Rotations
- Head Bone Rotation

**Logic:**
- Smooth interpolation to target
- Saccadic eye movements
- Head follows with delay
- Blink when changing gaze direction

#### 5. AN_LipSync

Drives mouth blend shapes from audio.

**Inputs:**
- Audio Data
- Phoneme Weights
- Emotional Modulation

**Outputs:**
- Mouth Blend Shape Values

**Logic:**
- Maps phonemes to visemes
- Emotional state affects mouth shape
- Smooth transitions between phonemes

## Animation Sequences

### Core Animation Set

**Idle Animations:**
- Idle_Neutral
- Idle_Confident (chest out, chin up)
- Idle_Playful (weight shifting, slight bounce)
- Idle_Mysterious (subtle head tilt, arms crossed)
- Idle_Flirty (hip sway, hair touch)
- Idle_Chaotic (fidgeting, random movements)

**Gesture Animations:**
- Gesture_Wave
- Gesture_Point
- Gesture_ThinkingPose (hand on chin)
- Gesture_Shrug
- Gesture_HeartSign
- Gesture_Wink
- Gesture_HairFlip
- Gesture_ConfidentCross (arms crossed)

**Emote Animations:**
- Emote_Laugh
- Emote_Cry
- Emote_Surprised
- Emote_Angry
- Emote_Love
- Emote_Confused
- Emote_Excited

**Locomotion:**
- Walk_Casual
- Walk_Confident (runway walk)
- Walk_Playful (bouncy)
- Run_Normal
- Run_Excited

## Integration with Avatar3DComponent

### Animation Instance Variables

The animation blueprint exposes these variables for runtime control:

```cpp
// In ABP_DeepTreeEcho_Avatar
UPROPERTY(BlueprintReadWrite, Category = "Emotion")
float EmotionIntensity;

UPROPERTY(BlueprintReadWrite, Category = "Emotion")
EAvatarEmotionalState CurrentEmotion;

UPROPERTY(BlueprintReadWrite, Category = "Personality")
float ConfidenceLevel;

UPROPERTY(BlueprintReadWrite, Category = "Personality")
float FlirtinessLevel;

UPROPERTY(BlueprintReadWrite, Category = "Personality")
float ChaosFactor;

UPROPERTY(BlueprintReadWrite, Category = "Gaze")
FVector GazeTarget;

UPROPERTY(BlueprintReadWrite, Category = "Gaze")
bool bEnableGazeTracking;

UPROPERTY(BlueprintReadWrite, Category = "Effects")
float EchoResonance;

UPROPERTY(BlueprintReadWrite, Category = "Effects")
float CognitiveLoad;
```

### C++ Animation Instance Class

```cpp
// AvatarAnimInstance.h
UCLASS()
class UAvatarAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable, Category = "Animation")
    void UpdateEmotionalState(EAvatarEmotionalState Emotion, float Intensity);

    UFUNCTION(BlueprintCallable, Category = "Animation")
    void SetPersonalityTraits(float Confidence, float Flirtiness, float Chaos);

protected:
    void UpdateLocomotion();
    void UpdateGestures();
    void UpdateFacialAnimation();
    void ApplyChaoticBehavior(float DeltaTime);
};
```

## Performance Optimization

**LOD System:**
- LOD 0: Full animation (all layers, all blend shapes)
- LOD 1: Reduced facial detail (fewer blend shapes)
- LOD 2: No facial animation, simplified gestures
- LOD 3: Locomotion only

**Animation Compression:**
- Keyframe reduction for non-critical animations
- Curve compression for blend shapes
- Additive animation compression

**Update Rate Optimization:**
- Distance-based update rate
- Near: 60 FPS
- Medium: 30 FPS
- Far: 15 FPS

## Next Steps

1. Implement ABP_DeepTreeEcho_Avatar blueprint
2. Create custom animation nodes (C++)
3. Build state machines in blueprint
4. Create animation sequences (requires 3D artist)
5. Implement UAvatarAnimInstance class
6. Integrate with Avatar3DComponent
7. Performance profiling and optimization

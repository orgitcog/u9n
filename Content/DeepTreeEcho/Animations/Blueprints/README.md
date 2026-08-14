# Animation Blueprints

## Critical Asset: ABP_DeepTreeEcho_Avatar

**Priority**: HIGHEST  
**Guide**: `/Documentation/Implementation/ABP_DeepTreeEcho_Avatar_Implementation.md`  
**Quick Start**: `/Documentation/QuickStart/QUICKSTART_ABP_DeepTreeEcho_Avatar.md`

### Purpose
The Animation Blueprint is the **central nervous system** of the avatar. It connects the C++ `UAvatarAnimInstance` class to the visual representation, enabling:
- Locomotion state machine
- Facial blend shape animation
- Emotional expression system
- Personality-driven gestures
- Gaze tracking and eye movement
- Procedural breathing and idle behaviors

### Implementation Steps

1. **Create Blueprint**
   - Right-click in this folder
   - Select Animation → Animation Blueprint
   - Set parent class to `UAvatarAnimInstance`
   - Name: `ABP_DeepTreeEcho_Avatar`

2. **Build Locomotion State Machine**
   - Open Animation Graph
   - Create State Machine
   - Add states: Idle, Walk, Run
   - Connect with transitions based on Speed variable
   - Use placeholder animations initially

3. **Expose Critical Variables**
   - Ensure these are accessible from C++:
     - `EmotionIntensity` (float)
     - `CurrentEmotion` (enum)
     - `ChaosFactor` (float)
     - `Speed` (float)
     - `BreathingCycle` (float)

4. **Implement Facial Blend Shapes**
   - Read `FFacialBlendShapes` struct from C++
   - Apply to skeletal mesh morph targets
   - Test with emotional state changes

5. **Add Procedural Breathing**
   - Use `BreathingCycle` variable
   - Drive chest bone animation
   - Apply subtle idle movement

### Testing
1. Place avatar in level
2. Attach personality trait component
3. Change emotional states via C++
4. Verify animation transitions
5. Check facial expressions

### Success Criteria
- Avatar responds to personality trait changes
- Emotional states drive facial animations
- Locomotion transitions smoothly
- Breathing is visible and natural

### Dependencies
- Rigged skeletal mesh with blend shapes
- Placeholder animations (can use Mixamo temporarily)
- C++ `UAvatarAnimInstance` class (already implemented)

## See Also
- Montages directory: `/Content/DeepTreeEcho/Animations/Montages/`
- Sequences directory: `/Content/DeepTreeEcho/Animations/Sequences/`

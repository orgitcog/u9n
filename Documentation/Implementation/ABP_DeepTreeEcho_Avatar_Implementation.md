# ABP_DeepTreeEcho_Avatar - Complete Implementation Guide

**Asset Type:** Animation Blueprint  
**Parent Class:** UAvatarAnimInstance (C++)  
**Target Skeleton:** SK_DeepTreeEcho_Avatar  
**Priority:** #1 Critical Asset

---

## Overview

This guide provides step-by-step instructions for creating the ABP_DeepTreeEcho_Avatar animation blueprint in Unreal Engine 5. This blueprint is the central nervous system of the Deep Tree Echo avatar, connecting the C++ `UAvatarAnimInstance` class to the visual skeletal mesh.

---

## Prerequisites

Before creating this animation blueprint, ensure you have:

1. **Skeletal Mesh:** SK_DeepTreeEcho_Avatar with proper rigging
2. **Blend Shapes (Morph Targets):** 
   - BrowInnerUp, BrowOuterUp, BrowDown
   - EyeWide, EyeSquint
   - MouthSmile, MouthFrown, MouthOpen
   - CheekPuff, JawOpen
3. **C++ Class:** UAvatarAnimInstance compiled and available
4. **Placeholder Animations:** At least Idle, Walk, Run sequences

---

## Step 1: Create the Animation Blueprint

### In Unreal Engine Editor:

1. **Navigate to Content Browser**
   - Right-click in `/Game/DeepTreeEcho/Animations/`
   - Select `Animation → Animation Blueprint`

2. **Configure Blueprint Settings**
   - **Parent Class:** Select `AvatarAnimInstance` (your C++ class)
   - **Target Skeleton:** Select `SK_DeepTreeEcho_Avatar`
   - **Name:** `ABP_DeepTreeEcho_Avatar`

3. **Open the Animation Blueprint**
   - Double-click to open the blueprint editor

---

## Step 2: Configure Animation Graph

### 2.1 Create Locomotion State Machine

In the **AnimGraph** tab:

1. **Add State Machine Node**
   - Right-click → `Add State Machine`
   - Name: `Locomotion`

2. **Connect to Output Pose**
   - Drag from State Machine output to `Output Pose`

3. **Double-click State Machine** to enter

### 2.2 Build Locomotion States

Inside the State Machine, create these states:

#### State: Idle
1. **Add State:** Right-click → `Add State` → Name: `Idle`
2. **Set as Entry State:** Right-click state → `Set as Entry State`
3. **Inside Idle State:**
   - Add `Play Idle_Neutral` animation node
   - Connect to Output Animation Pose

#### State: Walk
1. **Add State:** Name: `Walk`
2. **Inside Walk State:**
   - Add `Blend Space Player` node
   - Select `BS_Walk_Directional` (if available, otherwise use single walk anim)
   - Connect Speed and Direction variables
   - Connect to Output Animation Pose

#### State: Run
1. **Add State:** Name: `Run`
2. **Inside Run State:**
   - Add `Play Run_Normal` animation node
   - Connect to Output Animation Pose

#### State: Jump
1. **Add State:** Name: `Jump`
2. **Inside Jump State:**
   - Add `Play Jump_Start` animation node
   - Connect to Output Animation Pose

#### State: Fall
1. **Add State:** Name: `Fall`
2. **Inside Fall State:**
   - Add `Play Fall_Loop` animation node
   - Connect to Output Animation Pose

### 2.3 Create State Transitions

#### Idle → Walk
- **Condition:** `Speed > 10.0`
- **Blend Duration:** 0.2s

#### Walk → Run
- **Condition:** `Speed > 300.0`
- **Blend Duration:** 0.3s

#### Run → Walk
- **Condition:** `Speed < 250.0`
- **Blend Duration:** 0.3s

#### Walk → Idle
- **Condition:** `Speed < 5.0`
- **Blend Duration:** 0.2s

#### Any State → Jump
- **Condition:** `bIsInAir == true AND Velocity.Z > 0`
- **Blend Duration:** 0.1s

#### Jump → Fall
- **Condition:** `Velocity.Z < -100.0`
- **Blend Duration:** 0.2s

#### Fall → Idle
- **Condition:** `bIsOnGround == true`
- **Blend Duration:** 0.3s

---

## Step 3: Implement Facial Animation System

### 3.1 Add Facial Blend Shape Control

Back in the main **AnimGraph**:

1. **After Locomotion State Machine**, add `Modify Curve` node
2. **Connect:** Locomotion → Modify Curve → Output Pose

3. **In Modify Curve node**, add these curves:
   - BrowInnerUp
   - BrowOuterUp
   - BrowDown
   - EyeWide
   - EyeSquint
   - MouthSmile
   - MouthFrown
   - MouthOpen
   - CheekPuff
   - JawOpen

4. **For each curve**, set the value to the corresponding variable from C++:
   - Right-click curve value → `Convert to Variable Get`
   - Select `FacialBlendShapes.BrowInnerUp` (etc.)

### 3.2 Alternative: Use Control Rig (Advanced)

If using Control Rig for facial animation:

1. Add `Control Rig` node after Locomotion
2. Select `CR_DeepTreeEcho_Face` control rig
3. Map C++ variables to control rig parameters

---

## Step 4: Add Procedural Animation Layers

### 4.1 Breathing Layer

1. **Add Layered Blend Per Bone** node
   - Connect after Modify Curve
   - Add 1 blend pose

2. **Create Breathing Animation**
   - Add `Make Dynamic Additive` node
   - Connect a subtle chest expansion animation
   - Modulate intensity with `BreathingCycle` variable

3. **Configure Blend Settings**
   - Bone Name: `spine_02` (or appropriate chest bone)
   - Blend Depth: 1
   - Mesh Space Rotation Blend: true

### 4.2 Idle Sway Layer

1. **Add another Layered Blend Per Bone**
2. **Create procedural sway:**
   - Use `IdleSway.X` and `IdleSway.Y` to drive subtle hip/spine rotation
   - Can use `Apply Additive` node with procedural animation

---

## Step 5: Implement Gaze Tracking

### 5.1 Add Look At Node

1. **Add `Look At` node** (or `Two Bone IK` for more control)
2. **Configure:**
   - Target Bone: `head`
   - Look At Target: `GazeTargetLocation` (from C++)
   - Look At Axis: X Forward
   - Interpolation Speed: Use `EyeLookAtAlpha` and `HeadLookAtAlpha`

3. **Add Eye Bone Control**
   - Add separate `Look At` nodes for `eye_l` and `eye_r` bones
   - Use same `GazeTargetLocation`
   - Higher interpolation speed for eyes (more responsive)

---

## Step 6: Event Graph Setup

### 6.1 Initialize Variables

In the **Event Graph** tab:

1. **Event Blueprint Initialize Animation**
   - This is called once when the animation blueprint is created
   - Call parent `NativeInitializeAnimation`

2. **Verify C++ Variable Exposure**
   - Check that all variables from `UAvatarAnimInstance` are visible
   - Key variables:
     - Speed, Direction, bIsInAir, bIsOnGround
     - CurrentEmotion, EmotionIntensity
     - ConfidenceLevel, FlirtinessLevel, ChaosFactor
     - FacialBlendShapes (struct)
     - GazeTargetLocation
     - BreathingCycle, IdleSway

### 6.2 Custom Events (Optional)

Add custom events for debugging:

- **Event: DebugPrintEmotionalState**
  - Print CurrentEmotion and EmotionIntensity to screen
  
- **Event: DebugPrintLocomotionState**
  - Print Speed, Direction, LocomotionState

---

## Step 7: Optimization and LOD

### 7.1 Configure Update Rate Optimization

1. **In Class Defaults:**
   - Enable `Optimize Anim Blueprint Member Variable Access`
   - Set `Update Rate Optimization` to `Look Ahead`

2. **Add LOD Thresholds:**
   - LOD 0 (Near): Full animation, all blend shapes
   - LOD 1 (Medium): Reduced facial detail
   - LOD 2 (Far): No facial animation
   - LOD 3 (Very Far): Locomotion only

### 7.2 Performance Settings

- **Enable Multi-Threading:** Check `Allow Multi-Threaded Animation Update`
- **Disable Unnecessary Features:** Turn off features not needed for this avatar

---

## Step 8: Testing and Validation

### 8.1 Test in Animation Preview

1. **Open Animation Preview** (in blueprint editor)
2. **Test Variables:**
   - Manually set `Speed` to test locomotion transitions
   - Set `CurrentEmotion` to different values
   - Adjust `EmotionIntensity` slider
   - Modify `FacialBlendShapes` values

3. **Verify Transitions:**
   - Idle → Walk → Run should be smooth
   - Facial blend shapes should respond to emotion changes
   - Breathing should be visible

### 8.2 Test in Game

1. **Place avatar in level**
2. **Create test blueprint** that calls C++ functions:
   ```cpp
   // In blueprint or C++
   AvatarAnimInstance->UpdateEmotionalState(EAvatarEmotionalState::Happy, 0.8f);
   AvatarAnimInstance->SetPersonalityTraits(0.9f, 0.7f, 0.3f);
   ```

3. **Verify:**
   - Avatar moves correctly with character movement
   - Emotions change facial expression
   - Gaze tracking follows target

---

## Step 9: Integration with Avatar3DComponent

### 9.1 Ensure Proper Binding

In your avatar actor blueprint or C++:

1. **Get Anim Instance:**
   ```cpp
   UAvatarAnimInstance* AnimInstance = Cast<UAvatarAnimInstance>(
       SkeletalMeshComponent->GetAnimInstance());
   ```

2. **Update from Avatar3DComponent:**
   ```cpp
   if (AnimInstance)
   {
       AnimInstance->UpdateEmotionalState(EmotionalState.CurrentEmotion, 
                                         EmotionalState.EmotionIntensity);
       AnimInstance->SetPersonalityTraits(ConfidenceLevel, 
                                          FlirtinessLevel, 
                                          ChaosFactor);
   }
   ```

---

## Common Issues and Solutions

### Issue: Facial blend shapes not working
**Solution:** Ensure morph targets are properly imported and named exactly as in C++ (case-sensitive)

### Issue: State transitions are jerky
**Solution:** Increase blend duration in transition rules, ensure animations have proper root motion

### Issue: Gaze tracking is too fast/slow
**Solution:** Adjust `EyeLookAtAlpha` and `HeadLookAtAlpha` interpolation speeds

### Issue: Breathing is too subtle/exaggerated
**Solution:** Adjust the additive animation intensity or the `BreathingCycle` multiplier

### Issue: C++ variables not visible in blueprint
**Solution:** Ensure variables are marked `UPROPERTY(BlueprintReadWrite)` and blueprint is reparented to correct C++ class

---

## Advanced Features (Optional)

### Gesture System

Add a separate state machine for upper body gestures:

1. **Create Gesture State Machine** (parallel to Locomotion)
2. **Use Layered Blend Per Bone** to blend gestures on top of locomotion
3. **Blend Bone:** `spine_01` (affects upper body only)

### Chaotic Behavior Layer

Add random animation injections:

1. **Create custom animation node** `AN_ChaoticBehavior` in C++
2. **Add to AnimGraph** as final layer
3. **Modulate with** `ChaosFactor` variable

---

## File Structure

After completion, your project should have:

```
/Game/DeepTreeEcho/
├── Animations/
│   ├── ABP_DeepTreeEcho_Avatar.uasset (THIS ASSET)
│   ├── Idle_Neutral.uasset
│   ├── Walk_Forward.uasset
│   ├── Run_Normal.uasset
│   ├── Jump_Start.uasset
│   ├── Fall_Loop.uasset
│   └── BlendSpaces/
│       └── BS_Walk_Directional.uasset
└── Meshes/
    └── SK_DeepTreeEcho_Avatar.uasset
```

---

## Next Steps

After completing this animation blueprint:

1. **Create M_DTE_Skin material** (Priority #2)
2. **Create M_DTE_Eye material** (Priority #3)
3. **Test full integration** with Avatar3DComponent
4. **Create gesture animation montages**
5. **Implement lip sync system**

---

## Conclusion

This animation blueprint is the foundation of the Deep Tree Echo avatar's behavior system. Once implemented, the avatar will be able to express emotions, respond to personality traits, and exhibit lifelike procedural behaviors—all driven by the sophisticated C++ `UAvatarAnimInstance` class.

**Estimated Implementation Time:** 2-3 days for experienced Unreal Engine animator

**Blockers:** Requires rigged skeletal mesh with facial blend shapes and basic animation set

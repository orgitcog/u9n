# Deep Tree Echo Avatar - Quick Start Checklist

**Purpose:** Master checklist for implementing the top 3 critical assets  
**Estimated Total Time:** 4-6 hours  
**Difficulty:** Intermediate

---

## Overview

This checklist guides you through creating the three most critical assets for the Deep Tree Echo avatar in the correct order. Follow each step sequentially to build a functional avatar system.

---

## Phase 1: Preparation (30 minutes)

### Environment Setup

- [ ] Unreal Engine 5.3+ installed and working
- [ ] UnrealEngineCog project cloned from GitHub
- [ ] Project opens without errors
- [ ] C++ code compiles successfully (Build → Build Solution in Visual Studio/Rider)
- [ ] Editor restarts after C++ compilation

### Asset Preparation

- [ ] Skeletal mesh available (or UE5 Mannequin for testing)
- [ ] At least one idle animation available
- [ ] Basic textures available (or placeholders ready)
- [ ] Folder structure created: `/Game/DeepTreeEcho/`
  - [ ] `/Animations/`
  - [ ] `/Materials/`
  - [ ] `/Textures/`

---

## Phase 2: Animation Blueprint (2-3 hours)

### ABP_DeepTreeEcho_Avatar Creation

- [ ] Animation Blueprint created with correct parent class (`AvatarAnimInstance`)
- [ ] Blueprint opens without errors

### Animation Graph

- [ ] Locomotion state machine created
- [ ] Idle state created and set as entry state
- [ ] Walk state created
- [ ] Run state created
- [ ] State transitions configured with Speed variable
- [ ] Facial expression system added (morph targets)
- [ ] Gesture slot added

### Event Graph

- [ ] Speed calculation implemented
- [ ] Direction calculation implemented
- [ ] Variables updating correctly (verify with debug)

### Testing

- [ ] Blueprint compiles without errors
- [ ] Applied to character skeletal mesh
- [ ] Character animates in viewport
- [ ] State transitions work (Idle → Walk → Run)
- [ ] No console errors when playing

---

## Phase 3: Skin Material (1-2 hours)

### M_DTE_Skin Creation

- [ ] Master material created
- [ ] Shading model set to Subsurface Profile
- [ ] Blend mode set to Opaque

### Base Color System

- [ ] Base color texture parameter added
- [ ] Skin tint parameter added
- [ ] Connected to Base Color output

### Subsurface Scattering

- [ ] Subsurface color parameter added
- [ ] Subsurface intensity parameter added
- [ ] Subsurface profile created and assigned
- [ ] Connected to Subsurface Color output

### Emotional Blush

- [ ] Blush mask texture added
- [ ] Blush intensity parameter added
- [ ] Blush color parameter added
- [ ] Lerp node blending blush with base color
- [ ] Connected to Base Color output (replacing previous connection)

### Surface Properties

- [ ] Normal map added and connected
- [ ] Roughness parameter added and connected
- [ ] Specular parameter added and connected

### Emotional Aura

- [ ] Emotional aura color parameter added
- [ ] Emotional aura intensity parameter added
- [ ] Connected to Emissive Color output

### Testing

- [ ] Material compiles without errors
- [ ] Material instance created (MI_DTE_Skin_Default)
- [ ] Applied to avatar skeletal mesh
- [ ] Looks realistic under different lighting
- [ ] Blush effect visible when intensity increased
- [ ] Subsurface scattering visible (check ears, nose)

---

## Phase 4: Eye Material (1-2 hours)

### M_DTE_Eye Creation

- [ ] Master material created
- [ ] Blend mode set to Masked
- [ ] Refraction enabled in material properties

### Eye Structure

- [ ] Sclera color parameter added
- [ ] Iris texture and color parameters added
- [ ] Pupil size parameter added
- [ ] Layers composited with Lerp nodes
- [ ] Connected to Base Color output

### Cornea Refraction

- [ ] Refraction strength parameter added
- [ ] Fresnel node added for realistic refraction
- [ ] Connected to Refraction output
- [ ] Refraction method set to Index of Refraction

### Eye Sparkle

- [ ] Sparkle texture or procedural noise added
- [ ] Sparkle intensity parameter added
- [ ] Animation added (panner or rotator)
- [ ] Connected to Specular output

### Pupil Dilation

- [ ] Pupil dilation parameter added
- [ ] Pupil size calculation updated
- [ ] Clamped to reasonable range (0.1 - 0.5)

### Moisture System

- [ ] Moisture intensity parameter added
- [ ] Connected to roughness calculation
- [ ] Enhanced specular for wetness

### Opacity Mask

- [ ] Eye shape mask added
- [ ] Connected to Opacity Mask output
- [ ] Opacity mask clip value set to 0.5

### Testing

- [ ] Material compiles without errors
- [ ] Material instance created (MI_DTE_Eye_Blue)
- [ ] Applied to avatar eye material slots
- [ ] Eyes look realistic with depth
- [ ] Refraction visible
- [ ] Sparkle effect visible
- [ ] Pupil dilation works when parameter changed

---

## Phase 5: Integration Testing (30 minutes)

### Visual Verification

- [ ] Avatar placed in well-lit level
- [ ] All materials applied correctly
- [ ] No missing textures (pink materials)
- [ ] No console errors or warnings

### Animation Testing

- [ ] Avatar animates smoothly
- [ ] State transitions work correctly
- [ ] No animation popping or glitches
- [ ] Facial expressions can be triggered (if morph targets available)

### Material Testing

- [ ] Skin looks realistic under different lighting
- [ ] Eyes have depth and sparkle
- [ ] Blush effect works when intensity changed
- [ ] Pupil dilation works when parameter changed

### C++ Integration Testing

- [ ] Open avatar character blueprint
- [ ] Verify AvatarMaterialManager component exists
- [ ] Verify AudioManagerComponent exists
- [ ] Verify PersonalityTraitSystem exists
- [ ] No compilation errors in C++ code

---

## Common Issues and Solutions

### Issue: "AvatarAnimInstance not found"

**Cause:** C++ code not compiled or editor not restarted  
**Solution:** Build the C++ project in Visual Studio/Rider, then restart Unreal Engine

### Issue: "Animation not playing"

**Cause:** Animation not assigned in state or invalid skeleton  
**Solution:** Check that animations are assigned in each state and skeleton matches

### Issue: "Material looks pink"

**Cause:** Missing textures or invalid material setup  
**Solution:** Check all texture parameters have valid textures assigned (or use placeholders)

### Issue: "State transitions not working"

**Cause:** Speed variable not updating or transition conditions incorrect  
**Solution:** Add Print String nodes in Event Graph to debug Speed value

### Issue: "Subsurface scattering not visible"

**Cause:** Subsurface profile not assigned or incorrect shading model  
**Solution:** Verify Shading Model is Subsurface Profile and profile is assigned

### Issue: "Eye refraction not working"

**Cause:** Refraction not enabled or incorrect refraction method  
**Solution:** Enable Refraction in material properties, set method to Index of Refraction

---

## Next Steps After Completion

Once all three critical assets are created and working:

1. **Create additional animation montages** - Gestures and emotes
2. **Create M_DTE_Hair material** - Complete the visual appearance
3. **Implement audio assets** - Voice, SFX, music
4. **Create particle systems** - Cognitive visualization effects
5. **Implement test cases** - Unit and integration tests

---

## Success Criteria

You've successfully completed the quick start if:

- [ ] All three critical assets created without errors
- [ ] Avatar animates smoothly in viewport
- [ ] Skin material looks realistic
- [ ] Eyes have depth, sparkle, and refraction
- [ ] No console errors or warnings
- [ ] Ready to proceed with additional assets

---

## Time Tracking

Use this section to track your actual time spent:

- **Phase 1 (Preparation):** _____ minutes
- **Phase 2 (Animation Blueprint):** _____ hours
- **Phase 3 (Skin Material):** _____ hours
- **Phase 4 (Eye Material):** _____ hours
- **Phase 5 (Integration Testing):** _____ minutes
- **Total Time:** _____ hours

---

**Congratulations on completing the critical assets!** The Deep Tree Echo avatar is now ready for the next phase of development. You've built the foundation that makes everything else possible.

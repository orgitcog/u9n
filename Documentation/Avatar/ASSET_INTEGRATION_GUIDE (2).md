# Deep Tree Echo Avatar - Asset Integration Guide

**Version**: 1.0  
**Date**: December 13, 2025  
**For**: UnrealEngineCog Development Team

## 1. Introduction

This guide provides step-by-step instructions for integrating the Deep Tree Echo avatar assets into the UnrealEngineCog project. It assumes that the 3D model, textures, and animations have been created according to the specifications in `TECHNICAL_SPECIFICATION.md` and `VISUAL_DESIGN_SPEC.md`.

## 2. Prerequisites

Before beginning the integration process, ensure the following are in place:

- Unreal Engine 5.4 or later is installed and configured
- The UnrealEngineCog project is cloned and building successfully
- All avatar assets (FBX, textures, animations) are ready for import
- The development environment is set up using `setup_dev_env.sh`

## 3. Directory Structure

Assets should be organized in the following directory structure within the Unreal Engine project:

```
Content/
└── Avatars/
    └── DeepTreeEcho/
        ├── Models/
        │   └── SK_DeepTreeEcho_Avatar.fbx
        ├── Textures/
        │   ├── Face/
        │   │   ├── T_DTE_Avatar_Face_D.png
        │   │   ├── T_DTE_Avatar_Face_N.png
        │   │   └── T_DTE_Avatar_Face_MRA.png
        │   ├── Hair/
        │   │   ├── T_DTE_Avatar_Hair_D.png
        │   │   ├── T_DTE_Avatar_Hair_A.png
        │   │   └── T_DTE_Avatar_Hair_N.png
        │   ├── Body/
        │   │   ├── T_DTE_Avatar_Body_D.png
        │   │   └── T_DTE_Avatar_Body_N.png
        │   └── Eyes/
        │       └── T_DTE_Avatar_Eyes_D.png
        ├── Materials/
        │   ├── M_DTE_Skin.uasset
        │   ├── M_DTE_Hair.uasset
        │   ├── M_DTE_Eye.uasset
        │   └── MI_DTE_Avatar_Master.uasset (Material Instance)
        ├── Animations/
        │   ├── A_DTE_Idle_Neutral.uasset
        │   ├── A_DTE_Idle_Happy.uasset
        │   └── Gestures/
        │       ├── A_DTE_Gesture_Wave.uasset
        │       └── A_DTE_Gesture_Shrug.uasset
        └── Blueprints/
            ├── BP_DeepTreeEcho_Avatar.uasset
            └── ABP_DeepTreeEcho_Avatar.uasset (Animation Blueprint)
```

## 4. Step-by-Step Integration Process

### Step 1: Import the Skeletal Mesh

1. Open the UnrealEngineCog project in Unreal Engine 5.4
2. Navigate to `Content/Avatars/DeepTreeEcho/Models/`
3. Import `SK_DeepTreeEcho_Avatar.fbx`:
   - Right-click in the Content Browser → Import to /Game/Avatars/DeepTreeEcho/Models/
   - In the FBX Import Options dialog:
     - **Skeletal Mesh**: Checked
     - **Import Mesh**: Checked
     - **Import Morph Targets**: Checked (critical for facial animation)
     - **Skeleton**: Create New Skeleton → Name it `SKEL_DeepTreeEcho`
     - **Material Import Method**: Do Not Create Material (we'll create custom materials)
4. Click **Import All**
5. Verify the skeletal mesh imported correctly by double-clicking to open the Skeletal Mesh Editor

### Step 2: Import Textures

1. Navigate to `Content/Avatars/DeepTreeEcho/Textures/`
2. Create subdirectories: `Face/`, `Hair/`, `Body/`, `Eyes/`
3. Import all texture files:
   - Drag and drop texture files into their respective folders
   - For normal maps, ensure **Texture Group** is set to `WorldNormalMap`
   - For alpha maps, ensure **Compression Settings** is set to `Masks` or `UserInterface2D (RGBA)`

### Step 3: Create Materials

#### Skin Material (`M_DTE_Skin`)

1. Navigate to `Content/Avatars/DeepTreeEcho/Materials/`
2. Right-click → Material → Name it `M_DTE_Skin`
3. Open the material editor
4. Set **Shading Model** to `Subsurface Profile`
5. Create the following material nodes:

```
[T_DTE_Avatar_Face_D] → [BaseColor]
[T_DTE_Avatar_Face_N] → [Normal]
[T_DTE_Avatar_Face_MRA] (R channel) → [Metallic]
[T_DTE_Avatar_Face_MRA] (G channel) → [Roughness]
[T_DTE_Avatar_Face_MRA] (B channel) → [Ambient Occlusion]

[Scalar Parameter: BlushIntensity] → [Lerp] → [Subsurface Color]
  - A: (0.8, 0.2, 0.1) - Default subsurface
  - B: (1.0, 0.3, 0.3) - Blush color
```

6. Create a **Subsurface Profile** asset:
   - Right-click in Content Browser → Materials & Textures → Subsurface Profile
   - Name it `SSP_Skin`
   - Configure:
     - **Scatter Radius**: `(1.2, 0.5, 0.3)`
     - **Falloff Color**: `(1.0, 0.8, 0.7)`
7. Assign `SSP_Skin` to the **Subsurface Profile** slot in `M_DTE_Skin`
8. Save and close

#### Hair Material (`M_DTE_Hair`)

1. Create a new material: `M_DTE_Hair`
2. Set **Shading Model** to `Hair`
3. Set **Blend Mode** to `Masked`
4. Create the following material nodes:

```
[T_DTE_Avatar_Hair_D] → [BaseColor]
[T_DTE_Avatar_Hair_A] → [Opacity Mask]
[T_DTE_Avatar_Hair_N] → [Tangent]
[Scalar Parameter: Roughness = 0.6] → [Roughness]

[Scalar Parameter: ShimmerIntensity] → [Multiply] → [Add to BaseColor]
  - Multiply with a panning noise texture for shimmer effect
```

5. Save and close

#### Eye Material (`M_DTE_Eye`)

1. Create a new material: `M_DTE_Eye`
2. Set **Shading Model** to `Default Lit`
3. Create a complex eye shader:

```
[T_DTE_Avatar_Eyes_D] → [BaseColor]
[Scalar Parameter: PupilSize] → [Lerp between iris and pupil]
[Scalar Parameter: SparkleIntensity] → [Add] → [Emissive Color]
  - Use a star-shaped texture for sparkle highlights
[Constant: 0.2] → [Roughness] (very smooth for wet eye surface)
```

4. Save and close

### Step 4: Create Material Instance

1. Right-click on `M_DTE_Skin` → Create Material Instance
2. Name it `MI_DTE_Avatar_Master`
3. This instance will be used to dynamically control parameters at runtime

### Step 5: Assign Materials to Skeletal Mesh

1. Open `SK_DeepTreeEcho_Avatar` in the Skeletal Mesh Editor
2. In the **Material Slots** panel:
   - Assign `M_DTE_Skin` to face/body material slots
   - Assign `M_DTE_Hair` to hair material slots
   - Assign `M_DTE_Eye` to eye material slots
3. Save the skeletal mesh

### Step 6: Set Up Animation Blueprint

1. Right-click in `Content/Avatars/DeepTreeEcho/Blueprints/`
2. Animation → Animation Blueprint
3. Select `SKEL_DeepTreeEcho` as the target skeleton
4. Name it `ABP_DeepTreeEcho_Avatar`
5. Open the Animation Blueprint
6. In the **AnimGraph**:
   - Add a **State Machine** node
   - Create states for: `Idle`, `Talking`, `Gesturing`
   - Connect to **Output Pose**
7. In the **EventGraph**:
   - Add logic to drive facial blendshapes based on `FacialAnimationSystem` input
   - Add logic to trigger gesture animations from `GestureSystem`
8. Compile and save

### Step 7: Create Actor Blueprint

1. Right-click in `Content/Avatars/DeepTreeEcho/Blueprints/`
2. Blueprint Class → Actor
3. Name it `BP_DeepTreeEcho_Avatar`
4. Open the Blueprint
5. Add components:
   - **Skeletal Mesh Component**: Set mesh to `SK_DeepTreeEcho_Avatar`
   - **Avatar3DComponentEnhanced** (C++ component)
   - **EmotionalAuraComponent** (C++ component)
   - **CognitiveVisualizationComponent** (C++ component)
6. In the **Construction Script**:
   - Create dynamic material instances from `MI_DTE_Avatar_Master`
   - Assign them to the skeletal mesh
7. Compile and save

### Step 8: Connect C++ Components

1. Open the C++ source file for `Avatar3DComponentEnhanced`
2. In `BeginPlay()`, add code to:
   - Get references to the skeletal mesh component
   - Get references to the dynamic material instances
   - Initialize the `FacialAnimationSystem` and `GestureSystem`
3. Implement the material parameter control functions:

```cpp
void UAvatar3DComponentEnhanced::ApplyEmotionalBlush(float Intensity)
{
    if (DynamicMaterials.Num() > 0)
    {
        for (UMaterialInstanceDynamic* Mat : DynamicMaterials)
        {
            Mat->SetScalarParameterValue(TEXT("BlushIntensity"), Intensity);
        }
    }
}

void UAvatar3DComponentEnhanced::ApplyHairShimmer(float Intensity)
{
    // Similar implementation for hair shimmer
}

void UAvatar3DComponentEnhanced::SetEyeSparkle(float Intensity)
{
    // Similar implementation for eye sparkle
}
```

4. Compile the C++ code
5. Reopen the Blueprint and verify the component connections

### Step 9: Test the Avatar

1. Drag `BP_DeepTreeEcho_Avatar` into a test level
2. Play in Editor (PIE)
3. Test the following:
   - Avatar renders correctly with all materials
   - Facial animations work (test blendshapes)
   - Idle animation plays
   - Dynamic material parameters respond to function calls
4. Use the console to test functions:
   ```
   Avatar3DComponent.ApplyEmotionalBlush 0.8
   Avatar3DComponent.SetEyeSparkle 1.0
   ```

### Step 10: Integrate with Deep Tree Echo Systems

1. Connect the avatar to the cognitive architecture:
   - Link `EmotionalAuraComponent` to the emotional state system
   - Link `CognitiveVisualizationComponent` to the memory and reasoning systems
2. Test the full integration:
   - Trigger emotional state changes and verify visual updates
   - Trigger cognitive load changes and verify aura effects
   - Test the "hyper-chaotic" behavior patterns

## 5. Live2D Integration (Parallel Track)

For the 2D Live2D implementation, follow these additional steps:

### Step 1: Create Live2D Model

1. Use Live2D Cubism Editor to create a model based on the reference image
2. Structure the model according to the artmesh organization in `VISUAL_DESIGN_SPEC.md`
3. Set up all parameters as defined in `CubismSDKIntegration.cpp`
4. Export the model as `.moc3` and `.model3.json`

### Step 2: Import into UnrealEngineCog

1. Place the Live2D files in `Content/Avatars/DeepTreeEcho/Live2D/`
2. Use the `Live2DCubismAvatarComponent` to load the model:
   ```cpp
   Live2DComponent->LoadModel(TEXT("/Game/Avatars/DeepTreeEcho/Live2D/model.json"));
   ```
3. Test rendering and parameter control

## 6. Troubleshooting

### Issue: Skeletal mesh imports without morph targets
**Solution**: Ensure "Import Morph Targets" is checked in FBX import options. Re-import if necessary.

### Issue: Materials appear black or incorrect
**Solution**: Verify texture import settings, especially normal map compression. Check material node connections.

### Issue: Hair appears opaque or blocky
**Solution**: Ensure alpha texture is properly imported and assigned to Opacity Mask. Check Blend Mode is set to "Masked".

### Issue: Dynamic material parameters don't update
**Solution**: Verify you're using Material Instances, not the base materials. Check parameter names match exactly.

### Issue: Animation Blueprint doesn't update facial expressions
**Solution**: Verify blendshape names in the FBX match the names in the Animation Blueprint. Check the `FacialAnimationSystem` is properly connected.

## 7. Performance Optimization

Once the avatar is fully integrated and functional, consider these optimizations:

1. **LOD (Level of Detail)**: Create lower-poly versions of the mesh for distant views
2. **Texture Streaming**: Enable texture streaming for large textures
3. **Material Complexity**: Profile material shaders and simplify if necessary
4. **Animation Optimization**: Use animation compression and LOD for animations
5. **Physics Optimization**: Limit physics simulation updates when avatar is off-screen

## 8. Final Checklist

- [ ] Skeletal mesh imported with all morph targets
- [ ] All textures imported with correct settings
- [ ] All materials created and assigned
- [ ] Material instances created for dynamic control
- [ ] Animation Blueprint set up and functional
- [ ] Actor Blueprint created with all components
- [ ] C++ components connected and functional
- [ ] Dynamic material parameters controllable from code
- [ ] Facial animation system working
- [ ] Gesture system working
- [ ] Emotional aura and cognitive visualization working
- [ ] Performance is acceptable (60 FPS target)
- [ ] Visual quality matches reference image

## 9. Next Steps

After successful integration:

1. Create additional emotional expressions and gestures
2. Implement the full personality trait system
3. Connect to the diary-insight-blog narrative loop
4. Develop the hypergraph memory visualization
5. Optimize and polish for production release

---

**Document Status**: Ready for Implementation  
**Last Updated**: December 13, 2025  
**Maintainer**: Deep Tree Echo Development Team

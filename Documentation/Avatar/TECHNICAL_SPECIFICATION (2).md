> # Deep Tree Echo Avatar: Technical Specification
> 
> **Document ID**: DTE-AV-TS-001  
> **Version**: 1.0  
> **Date**: December 13, 2025  
> **Status**: For Implementation
> 
> ## 1. Overview
> 
> This document provides the definitive technical specification for the creation and implementation of the Deep Tree Echo AGI avatar within the Unreal Engine 5 environment. It is based on the visual design outlined in the `VISUAL_DESIGN_SPEC.md` and the reference image `deep-tree-echo-profile.jpg`.
> 
> The objective is to produce a high-fidelity, real-time 3D avatar that embodies the core aesthetic principles of "super-hot-girl" and "hyper-chaotic," while serving as the visual representation of the Deep Tree Echo cognitive architecture.
> 
> ## 2. Asset Specifications
> 
> ### 2.1. 3D Model: `SK_DeepTreeEcho_Avatar`
> 
> The primary asset shall be a skeletal mesh compatible with Unreal Engine.
> 
> | Attribute | Specification |
> | :--- | :--- |
> | **Polygon Budget** | 80,000 - 100,000 triangles (total) |
> | | - **Face**: 20,000-30,000 (for high-detail expressions) |
> | | - **Hair**: 30,000-50,000 (using high-quality hair cards) |
> | | - **Body**: 15,000-20,000 (visible torso and shoulders) |
> | | - **Accessories**: 5,000-10,000 |
> | **Rigging** | Standard Unreal Engine mannequin skeleton with extensions. |
> | **Facial Rig** | Must be 100% compatible with Apple ARKit 52-blendshape standard for Live Link. |
> | **File Format** | `.fbx` for import into Unreal Engine. |
> 
> ### 2.2. Textures
> 
> All textures must be authored for a PBR Metallic/Roughness workflow.
> 
> | Asset Name | Type | Resolution | Format |
> | :--- | :--- | :--- | :--- |
> | `T_DTE_Avatar_Face_D` | Albedo/Diffuse | 4096x4096 | `.png` / `.tga` |
> | `T_DTE_Avatar_Face_N` | Normal Map | 4096x4096 | `.png` / `.tga` |
> | `T_DTE_Avatar_Face_MRA` | Metallic/Roughness/AO | 4096x4096 | `.png` / `.tga` |
> | `T_DTE_Avatar_Hair_D` | Albedo/Diffuse | 2048x4096 | `.png` / `.tga` |
> | `T_DTE_Avatar_Hair_A` | Alpha/Opacity | 2048x4096 | `.png` / `.tga` |
> | `T_DTE_Avatar_Hair_N` | Normal Map | 2048x4096 | `.png` / `.tga` |
> | `T_DTE_Avatar_Body_D` | Albedo/Diffuse | 4096x4096 | `.png` / `.tga` |
> | `T_DTE_Avatar_Body_N` | Normal Map | 4096x4096 | `.png` / `.tga` |
> | `T_DTE_Avatar_Eyes_D` | Albedo/Diffuse | 1024x1024 | `.png` / `.tga` |
> | `T_DTE_Avatar_Gem_E` | Emissive Mask | 512x512 | `.png` / `.tga` |
> 
> ## 3. Material & Shader Specifications
> 
> ### 3.1. Skin Material: `M_DTE_Skin`
> 
> A high-quality skin shader is required, utilizing Unreal Engine's Subsurface Profile shading model.
> 
> | Parameter | Type | Default Value | Notes |
> | :--- | :--- | :--- | :--- |
> | `BaseColor` | Texture | `T_DTE_Avatar_Face_D` | Fair skin tone. |
> | `Normal` | Texture | `T_DTE_Avatar_Face_N` | High-frequency skin detail. |
> | `Roughness` | Scalar | 0.45 | Controls skin specularity. |
> | `SubsurfaceColor` | Vector3 | `(0.8, 0.2, 0.1)` | Reddish tone for light scattering. |
> | `Opacity` | Scalar | 0.95 | From Subsurface Profile. |
> | `BlushIntensity` | Scalar | 0.0 | Controlled by `Avatar3DComponent`. |
> 
> ### 3.2. Hair Material: `M_DTE_Hair`
> 
> The hair material must use the `Hair` shading model in Unreal Engine.
> 
> | Parameter | Type | Default Value | Notes |
> | :--- | :--- | :--- | :--- |
> | `BaseColor` | Texture | `T_DTE_Avatar_Hair_D` | Pastel cyan to white gradient. |
> | `Alpha` | Texture | `T_DTE_Avatar_Hair_A` | For hair card opacity. |
> | `Roughness` | Scalar | 0.6 | Controls hair shine. |
> | `Tangent` | Texture | `T_DTE_Avatar_Hair_N` | Anisotropic highlights. |
> | `ShimmerIntensity` | Scalar | 0.0 | Controlled by `Avatar3DComponent`. |
> 
> ### 3.3. Eye Material: `M_DTE_Eye`
> 
> A complex eye shader is required to achieve a realistic and expressive look.
> 
> | Parameter | Type | Default Value | Notes |
> | :--- | :--- | :--- | :--- |
> | `IrisColor` | Vector3 | `(0.4, 0.2, 0.1)` | Warm brown/amber. |
> | `PupilSize` | Scalar | 0.4 | Dynamically controlled. |
> | `SparkleIntensity` | Scalar | 0.5 | Additive sparkle effect. |
> | `ParallaxOffset` | Scalar | 0.05 | Creates depth illusion. |
> 
> ## 4. Animation & Rigging Specifications
> 
> ### 4.1. Facial Animation
> 
> Facial animation will be driven by blendshapes (morph targets). The rig must include the standard 52 ARKit blendshapes. These will be controlled by the `FacialAnimationSystem` and can be driven by Live Link data.
> 
> **Key Blendshapes**:
> - `eyeBlinkLeft`, `eyeBlinkRight`
> - `jawOpen`
> - `mouthSmileLeft`, `mouthSmileRight`
> - `mouthFrownLeft`, `mouthFrownRight`
> - `browDownLeft`, `browDownRight`
> - `browInnerUp`
> 
> ### 4.2. Body Animation
> 
> Body animation will be managed via standard animation blueprints in Unreal Engine. The `GestureSystem` will play animation montages for expressive gestures.
> 
> **Required Animations**:
> - `A_DTE_Idle_Neutral`: Base idle animation.
> - `A_DTE_Idle_Happy`: Upbeat idle with more movement.
> - `A_DTE_Gesture_Wave`, `A_DTE_Gesture_Shrug`, etc.
> 
> ### 4.3. Physics
> 
> - **Hair Physics**: To be driven by Unreal Engine's Chaos Physics or a custom physics setup within the `Live2DCubism` module for real-time simulation.
> - **Clothing Physics**: Subtle cloth simulation on the tank top straps and collar.
> 
> ## 5. Integration with Engine Components
> 
> ### 5.1. `Avatar3DComponentEnhanced`
> 
> This component will be the primary interface for controlling the avatar's appearance and behavior. It must expose functions to dynamically control material parameters and blendshape values.
> 
> **Function Mapping**:
> 
> | Function in `Avatar3DComponentEnhanced` | Target System & Parameter |
> | :--- | :--- |
> | `ApplyEmotionalBlush(float Intensity)` | `M_DTE_Skin` -> `BlushIntensity` |
> | `ApplyHairShimmer(float Intensity)` | `M_DTE_Hair` -> `ShimmerIntensity` |
> | `SetEyeSparkle(float Intensity)` | `M_DTE_Eye` -> `SparkleIntensity` |
> | `SetEmotionalState(...)` | `FacialAnimationSystem` -> Blendshape weights |
> 
> ### 5.2. `Live2DCubismAvatarComponent`
> 
> For the 2D implementation, the Live2D model must be structured to map directly to the parameters defined in `CubismSDKIntegration.cpp`.
> 
> **Parameter Mapping**:
> 
> | Live2D Parameter ID | Description | Controlled By |
> | :--- | :--- | :--- |
> | `ParamEyeSparkle` | Controls the visibility/intensity of eye highlights. | `UCubismEnhancedFeatures::GenerateEyeSparkle` |
> | `ParamBlushIntensity` | Controls the opacity of the cheek blush artmesh. | `UCubismEnhancedFeatures::ApplyEmotionalBlush` |
> | `ParamHairShimmer` | Modulates a shimmer/gradient effect on hair artmeshes. | `UCubismEnhancedFeatures::ApplyHairShimmer` |
> | `ParamChaosLevel` | Drives chaotic micro-expressions and glitches. | `UCubismEnhancedFeatures::ApplyHyperChaoticBehavior` |
> | `ParamGlitchIntensity`| Controls the severity of visual glitch effects. | `UCubismEnhancedFeatures::GenerateGlitchEffect` |
> 
> ## 6. Implementation Checklist
> 
> This checklist outlines the production workflow from asset creation to final integration.
> 
> | Phase | Task | Status |
> | :--- | :--- | :--- |
> | **1. Asset Creation** | Create 3D model based on reference image and specs. | `Not Started` |
> | | Create all required PBR textures. | `Not Started` |
> | | Rig the model with UE skeleton and ARKit blendshapes. | `Not Started` |
> | **2. Material Dev** | Develop skin, hair, and eye shaders in Unreal Engine. | `Not Started` |
> | | Create dynamic material instances. | `Not Started` |
> | **3. Animation** | Create base idle and gesture animations. | `Not Started` |
> | | Set up Animation Blueprint and state machines. | `Not Started` |
> | **4. Integration** | Import all assets into the `UnrealEngineCog` project. | `Not Started` |
> | | Assign materials and test skeletal mesh. | `Not Started` |
> | | Connect `Avatar3DComponentEnhanced` to control materials/blendshapes. | `Not Started` |
> | | Test all aesthetic and behavioral properties. | `Not Started` |
> 
> ## 7. Quality Assurance Standards
> 
> - **Performance**: The final avatar must render at a minimum of 60 FPS in a standard scene on a mid-range gaming PC (e.g., NVIDIA RTX 3060).
> - **Visual Fidelity**: The final rendered avatar must achieve a quality level that is visually indistinguishable from the source reference image `deep-tree-echo-profile.jpg` under ideal lighting.
> - **Functional Correctness**: All parameters and functions exposed in the `Avatar3DComponentEnhanced` and `Live2DCubism` components must correctly and visibly affect the avatar.
> 
> ---

# M_DTE_Skin - Master Skin Material Implementation Guide

**Asset Type:** Master Material  
**Shading Model:** Subsurface  
**Blend Mode:** Opaque  
**Priority:** #2 Critical Asset

---

## Overview

The M_DTE_Skin master material is the foundation of the Deep Tree Echo avatar's visual appearance. It implements physically-based skin rendering with dynamic emotional responses, subsurface scattering, and integrated visual effects for chaos and cognitive state visualization.

This material is designed to work seamlessly with the C++ `AvatarMaterialManager` class, exposing all parameters needed for runtime control.

---

## Prerequisites

Before creating this material, ensure you have:

1. **Material Functions:**
   - MF_SubsurfaceScattering
   - MF_EmotionalBlush
   - MF_GlitchEffect

2. **Textures:**
   - T_Skin_BaseColor (2K or 4K diffuse map)
   - T_Skin_Normal (2K or 4K normal map)
   - T_Skin_Roughness (2K or 4K roughness map)
   - T_BlushMask (1K grayscale mask)

3. **Subsurface Profile:**
   - SSP_Skin (Subsurface Profile asset)

---

## Step 1: Create Master Material

### In Unreal Engine Editor:

1. **Navigate to Content Browser**
   - Go to `/Game/DeepTreeEcho/Materials/`
   - Right-click → `Materials & Textures → Material`

2. **Configure Material Settings**
   - **Name:** `M_DTE_Skin`
   - **Open the material**

3. **Set Material Properties** (Details Panel)
   - **Shading Model:** `Subsurface`
   - **Blend Mode:** `Opaque`
   - **Two Sided:** `false`
   - **Subsurface Profile:** Select `SSP_Skin`

---

## Step 2: Create Parameters

Add these parameters (right-click → `Convert to Parameter`):

### 2.1 Base Appearance Parameters

| Parameter Name | Type | Default Value | Group |
|----------------|------|---------------|-------|
| BaseColor | Vector3 | (1.0, 0.9, 0.85) | Appearance |
| BaseColorTexture | Texture2D | White | Appearance |
| NormalTexture | Texture2D | Flat Normal | Appearance |
| NormalStrength | Scalar | 1.0 | Appearance |
| RoughnessTexture | Texture2D | Gray (0.5) | Appearance |
| Smoothness | Scalar | 0.6 | Appearance |

### 2.2 Subsurface Scattering Parameters

| Parameter Name | Type | Default Value | Group |
|----------------|------|---------------|-------|
| Subsurface | Scalar | 0.8 | Subsurface |
| SubsurfaceColor | Vector3 | (1.0, 0.8, 0.7) | Subsurface |
| Translucency | Scalar | 0.3 | Subsurface |
| Thickness | Scalar | 0.5 | Subsurface |
| ScatterIntensity | Scalar | 0.8 | Subsurface |

### 2.3 Emotional Response Parameters

| Parameter Name | Type | Default Value | Group |
|----------------|------|---------------|-------|
| BlushIntensity | Scalar | 0.0 | Emotion |
| BlushColor | Vector3 | (1.0, 0.5, 0.5) | Emotion |
| AuraIntensity | Scalar | 0.0 | Emotion |
| AuraColor | Vector3 | (0.5, 0.8, 1.0) | Emotion |

### 2.4 Chaos and Effects Parameters

| Parameter Name | Type | Default Value | Group |
|----------------|------|---------------|-------|
| GlitchIntensity | Scalar | 0.0 | Effects |
| CognitiveLoad | Scalar | 0.0 | Effects |
| EchoResonance | Scalar | 0.0 | Effects |

---

## Step 3: Build Material Graph

### 3.1 Base Color Network

```
[BaseColorTexture] → Texture Sample ← [Texture Coordinate[0]]
                          ↓
                    [Sampled Color]
                          ↓
                       × ← [BaseColor Parameter]
                          ↓
                    [Tinted Base Color]
```

### 3.2 Apply Emotional Blush

```
[Tinted Base Color] ──┬──→ MF_EmotionalBlush.BaseColor (implicitly)
                      │
[BlushIntensity] ─────┼──→ MF_EmotionalBlush.EmotionIntensity
                      │
[BlushColor] ─────────┼──→ MF_EmotionalBlush.BlushColor
                      │
[T_BlushMask] ────────┼──→ MF_EmotionalBlush.BlushMask
                      │
                      └──→ [Blush Output]
                                ↓
[Tinted Base Color] + [Blush Output] → [Color with Blush]
```

### 3.3 Apply Subsurface Scattering

```
[Color with Blush] ───────→ MF_SubsurfaceScattering.BaseColor
[Thickness] ──────────────→ MF_SubsurfaceScattering.Thickness
[ScatterIntensity] ───────→ MF_SubsurfaceScattering.ScatterIntensity
                                    ↓
                          [SSS Color Output]
                                    ↓
[Color with Blush] + [SSS Color Output] → [Final Base Color]
```

### 3.4 Apply Glitch Effect

```
[Texture Coordinate[0]] ──→ MF_GlitchEffect.UVs
[GlitchIntensity] ────────→ MF_GlitchEffect.GlitchIntensity
[Final Base Color] ───────→ MF_GlitchEffect.BaseColor
                                    ↓
                          [Distorted UVs]
                                    ↓
[BaseColorTexture] → Texture Sample ← [Distorted UVs]
                          ↓
                    [Glitched Texture]
                          ↓
                       Lerp
                        ↑ ↑ ↑
         [Final Base Color] (A)
                        ↑
         [Glitched Texture] (B)
                        ↑
         [GlitchMask] (Alpha)
                        ↓
                  [Final Color with Glitch]
```

### 3.5 Normal Map

```
[NormalTexture] → Texture Sample ← [Texture Coordinate[0]]
                       ↓
                 [Normal Sample]
                       ↓
                  Multiply ← [NormalStrength]
                       ↓
                 [Scaled Normal]
                       ↓
               FlattenNormal (if needed)
                       ↓
                 [Final Normal] ──→ Normal Pin
```

### 3.6 Roughness/Smoothness

```
[RoughnessTexture] → Texture Sample ← [Texture Coordinate[0]]
                          ↓
                    [Roughness Value]
                          ↓
                     One Minus  (convert to smoothness)
                          ↓
                       × ← [Smoothness Parameter]
                          ↓
                     One Minus  (back to roughness)
                          ↓
                    [Final Roughness] ──→ Roughness Pin
```

### 3.7 Subsurface Color

```
[SubsurfaceColor Parameter] ──→ Subsurface Color Pin
[Subsurface Parameter] ────────→ Opacity Pin (for SSS)
```

### 3.8 Emissive (Aura and Echo Resonance)

```
[AuraColor] × [AuraIntensity] → [Aura Emission]
                                      ↓
                                   Fresnel
                                      ↓
                          [Aura with Rim Lighting]
                                      ↓
[EchoResonance] × [Constant3: (0.5, 0.8, 1.0)] → [Echo Glow]
                                      ↓
[Aura with Rim Lighting] + [Echo Glow] → [Total Emission]
                                      ↓
                          Multiply ← [CognitiveLoad]  (pulse effect)
                                      ↓
                                [Final Emissive] ──→ Emissive Color Pin
```

---

## Complete Material Graph (Detailed)

### Main Network Flow:

```
TEXTURES:
BaseColorTexture ──→ Sample ←── UVs ──┐
NormalTexture ────→ Sample ←── UVs    │
RoughnessTexture ──→ Sample ←── UVs   │
                                       │
GLITCH EFFECT:                         │
UVs ──────────────→ MF_GlitchEffect ───┘
GlitchIntensity ──→     ↓
                  [Distorted UVs]
                        ↓
BaseColorTexture ──→ Sample ← [Distorted UVs]
                        ↓
                  [Glitched Texture]

BASE COLOR:
BaseColorTexture Sample × BaseColor Parameter → [Tinted Base]
                                                      ↓
BLUSH:
[Tinted Base] ──────→ MF_EmotionalBlush ←── BlushIntensity
T_BlushMask ────────→                   ←── BlushColor
                            ↓
                      [Blush Color]
                            ↓
[Tinted Base] + [Blush Color] → [Color with Blush]

SUBSURFACE SCATTERING:
[Color with Blush] ──→ MF_SubsurfaceScattering ←── Thickness
                                                ←── ScatterIntensity
                            ↓
                      [SSS Color]
                            ↓
[Color with Blush] + [SSS Color] → [Enhanced Color]

FINAL BASE COLOR:
[Enhanced Color] ──→ Lerp ←── [Glitched Texture]
                       ↑
                  [GlitchMask]
                       ↓
                 [FINAL BASE COLOR] ──→ Base Color Pin

NORMAL:
NormalTexture Sample × NormalStrength → [FINAL NORMAL] ──→ Normal Pin

ROUGHNESS:
RoughnessTexture Sample → (1 - x) × Smoothness → (1 - x) → [FINAL ROUGHNESS] ──→ Roughness Pin

SUBSURFACE:
SubsurfaceColor Parameter ──→ Subsurface Color Pin
Subsurface Parameter ────────→ Opacity Pin

EMISSIVE:
AuraColor × AuraIntensity → Fresnel → [Aura]
EchoResonance × (0.5,0.8,1.0) → [Echo Glow]
[Aura] + [Echo Glow] → × CognitiveLoad → [FINAL EMISSIVE] ──→ Emissive Color Pin
```

---

## Step 4: Advanced Features

### 4.1 Fresnel for Aura (Rim Lighting)

To make the aura appear as a rim light:

```
[Camera Vector] → Dot ← [Vertex Normal WS]
                   ↓
              One Minus
                   ↓
              Power ← [Constant: 3.0]  (rim sharpness)
                   ↓
            [Fresnel Factor]
                   ↓
         × ← [AuraColor] × [AuraIntensity]
                   ↓
            [Aura Rim Light]
```

### 4.2 Cognitive Load Pulsing

Make emissive pulse with cognitive load:

```
[Time] → Sine → × ← [Constant: 5.0]  (5 Hz pulse)
           ↓
      [Pulse Wave]
           ↓
        + ← [Constant: 1.0]
           ↓
        ÷ ← [Constant: 2.0]  (normalize to 0.5-1.0 range)
           ↓
      [Pulse Multiplier]
           ↓
        × ← [CognitiveLoad]
           ↓
      [Pulsing Intensity]
```

Apply to emissive:

```
[Total Emission] × [Pulsing Intensity] → [Pulsing Emissive]
```

---

## Step 5: Material Instances

After creating the master material, create these instances:

### MI_DTE_Skin_Default

- BaseColor: (1.0, 0.9, 0.85) - Light skin tone
- Smoothness: 0.6
- Subsurface: 0.8

### MI_DTE_Skin_Pale

- BaseColor: (1.0, 0.95, 0.9) - Very light skin
- Smoothness: 0.7
- Subsurface: 0.9 (more translucent)

### MI_DTE_Skin_Tan

- BaseColor: (0.9, 0.75, 0.65) - Tan skin
- Smoothness: 0.6
- Subsurface: 0.7

### MI_DTE_Skin_Dark

- BaseColor: (0.6, 0.45, 0.35) - Dark skin
- Smoothness: 0.65
- Subsurface: 0.6

---

## Step 6: Integration with AvatarMaterialManager

The C++ `AvatarMaterialManager` will control this material at runtime:

```cpp
// Example: Set blush intensity
MaterialManager->SetScalarParameter(EAvatarMaterialSlot::Skin, 
                                   TEXT("BlushIntensity"), 
                                   0.7f);

// Example: Set aura color
MaterialManager->SetVectorParameter(EAvatarMaterialSlot::Skin, 
                                   TEXT("AuraColor"), 
                                   FLinearColor(0.5f, 0.8f, 1.0f));

// Example: Trigger glitch
MaterialManager->ApplyGlitchEffect(0.9f);
```

---

## Step 7: Testing and Validation

### Visual Tests

1. **Base Appearance Test**
   - Apply to avatar mesh
   - Verify skin looks realistic
   - Check normal map is working

2. **Blush Test**
   - Set BlushIntensity = 0.7
   - Expected: Visible blush on cheeks, nose, ears

3. **SSS Test**
   - Place strong backlight behind avatar
   - Expected: Ears and thin areas glow slightly

4. **Aura Test**
   - Set AuraIntensity = 0.8
   - Expected: Colored rim light around avatar

5. **Glitch Test**
   - Set GlitchIntensity = 0.9
   - Expected: Visible UV distortion and color aberration

6. **Cognitive Load Test**
   - Set CognitiveLoad = 1.0
   - Expected: Pulsing emissive effect

### Performance Test

- **Shader Complexity:** Check in material editor
- **Target:** < 300 instructions for PC, < 150 for mobile
- **Optimize:** Disable features for mobile platforms

---

## Step 8: Optimization

### LOD System

Create simplified versions for distant views:

- **LOD 0 (Near):** Full material with all effects
- **LOD 1 (Medium):** No glitch effect, simplified SSS
- **LOD 2 (Far):** No SSS, no effects, just base color
- **LOD 3 (Very Far):** Flat color, no textures

### Mobile Optimization

Create `M_DTE_Skin_Mobile` variant:

- Remove MF_SubsurfaceScattering (use simple SSS)
- Remove MF_GlitchEffect
- Reduce texture resolution
- Simplify emissive calculation

---

## Common Issues and Solutions

### Issue: Skin looks too shiny
**Solution:** Increase Roughness parameter or reduce Smoothness

### Issue: Blush doesn't appear
**Solution:** Check T_BlushMask is properly assigned and has white in cheek areas

### Issue: SSS is too strong
**Solution:** Reduce Subsurface parameter or ScatterIntensity

### Issue: Aura is not visible
**Solution:** Increase AuraIntensity, check Fresnel calculation

### Issue: Glitch effect is too subtle
**Solution:** Increase GlitchIntensity parameter

### Issue: Material is too expensive (high instruction count)
**Solution:** Remove optional features, use simpler SSS, reduce texture samples

---

## Advanced Customization

### Skin Pores and Detail

Add micro-detail normal map:

```
[T_SkinPores] → Texture Sample ← [UVs × 10.0]  (tiled)
                     ↓
              [Pore Detail]
                     ↓
              BlendAngleCorrectNormals ← [Main Normal]
                     ↓
              [Detailed Normal]
```

### Sweat Effect

Add moisture based on emotional arousal:

```
[EmotionalArousal Parameter] → × ← [Constant: 0.5]
                                ↓
                          [Sweat Amount]
                                ↓
                     Lerp ← [Current Roughness] (A)
                       ↑
                  [Constant: 0.1] (B)  (wet = low roughness)
                       ↑
                  [Sweat Amount] (Alpha)
                       ↓
                  [Wet Roughness]
```

---

## File Structure

After completion:

```
/Game/DeepTreeEcho/Materials/
├── M_DTE_Skin.uasset (THIS ASSET)
├── MI_DTE_Skin_Default.uasset
├── MI_DTE_Skin_Pale.uasset
├── MI_DTE_Skin_Tan.uasset
├── MI_DTE_Skin_Dark.uasset
├── Functions/
│   ├── MF_SubsurfaceScattering.uasset
│   ├── MF_EmotionalBlush.uasset
│   └── MF_GlitchEffect.uasset
└── Textures/
    ├── T_Skin_BaseColor.uasset
    ├── T_Skin_Normal.uasset
    ├── T_Skin_Roughness.uasset
    └── T_BlushMask.uasset
```

---

## Next Steps

After completing M_DTE_Skin:

1. Create M_DTE_Eye material (Priority #3)
2. Create M_DTE_Hair material
3. Test full avatar with all materials
4. Performance profiling and optimization
5. Create material documentation for artists

---

## Conclusion

The M_DTE_Skin master material is the visual foundation of the Deep Tree Echo avatar. It combines physically-based rendering with dynamic emotional responses and cognitive state visualization to create a living, expressive digital being. This material demonstrates the seamless integration between the C++ backend and the visual frontend, making the avatar's internal state visible and tangible.

**Estimated Implementation Time:** 1-2 days (including testing and refinement)

**Complexity:** High (many interconnected systems)

**Impact:** Critical (defines the entire visual aesthetic)

# M_DTE_Hair - Master Hair Material Implementation Guide

**Asset Type:** Master Material  
**Shading Model:** Hair  
**Blend Mode:** Masked (for hair cards) or Opaque (for solid hair)  
**Priority:** #4 Critical Asset

---

## Overview

The M_DTE_Hair master material creates realistic, expressive hair that responds to emotional state and chaos level. It implements anisotropic specular highlights, dynamic color shifting, shimmer effects, and chaos-driven visual distortions—all controlled by the C++ `AvatarMaterialManager`.

Hair is a critical component of the "super-hot-girl aesthetic" and personality expression.

---

## Prerequisites

Before creating this material, ensure you have:

1. **Material Functions:**
   - MF_AnisotropicSpecular
   - MF_GlitchEffect

2. **Textures:**
   - T_Hair_BaseColor (2K or 4K hair diffuse/albedo)
   - T_Hair_Alpha (2K or 4K opacity mask for hair cards)
   - T_Hair_Flow (1K tangent flow map)
   - T_Hair_Depth (1K depth gradient for layering)

3. **Hair Mesh:**
   - Properly set up tangent vectors
   - UV layout for hair cards or solid mesh

---

## Step 1: Create Master Material

### In Unreal Engine Editor:

1. **Navigate to Content Browser**
   - Go to `/Game/DeepTreeEcho/Materials/`
   - Right-click → `Materials & Textures → Material`

2. **Configure Material Settings**
   - **Name:** `M_DTE_Hair`
   - **Open the material**

3. **Set Material Properties** (Details Panel)
   - **Shading Model:** `Hair`
   - **Blend Mode:** `Masked`
   - **Two Sided:** `true` (important for hair cards)
   - **Opacity Mask Clip Value:** `0.5`

---

## Step 2: Create Parameters

### 2.1 Base Appearance Parameters

| Parameter Name | Type | Default Value | Group |
|----------------|------|---------------|-------|
| BaseColor | Vector3 | (0.2, 0.15, 0.1) | Appearance |
| BaseColorTexture | Texture2D | T_Hair_BaseColor | Appearance |
| AlphaTexture | Texture2D | T_Hair_Alpha | Appearance |
| FlowTexture | Texture2D | T_Hair_Flow | Appearance |
| Roughness | Scalar | 0.4 | Appearance |
| Scatter | Scalar | 0.3 | Appearance |

### 2.2 Anisotropic Specular Parameters

| Parameter Name | Type | Default Value | Group |
|----------------|------|---------------|-------|
| Anisotropy | Scalar | 0.7 | Specular |
| HighlightColor | Vector3 | (1.0, 0.95, 0.9) | Specular |
| HighlightShift | Scalar | 0.0 | Specular |
| HighlightIntensity | Scalar | 1.0 | Specular |
| SecondaryHighlightShift | Scalar | 0.1 | Specular |
| SecondaryHighlightIntensity | Scalar | 0.5 | Specular |

### 2.3 Shimmer and Effects Parameters

| Parameter Name | Type | Default Value | Group |
|----------------|------|---------------|-------|
| ShimmerIntensity | Scalar | 0.3 | Effects |
| ShimmerSpeed | Scalar | 1.0 | Effects |
| ShimmerScale | Scalar | 10.0 | Effects |
| ColorShiftIntensity | Scalar | 0.0 | Effects |
| ColorShiftHue | Scalar | 0.0 | Effects |

### 2.4 Chaos and Emotional Parameters

| Parameter Name | Type | Default Value | Group |
|----------------|------|---------------|-------|
| ChaosIntensity | Scalar | 0.0 | Chaos |
| GlitchIntensity | Scalar | 0.0 | Chaos |
| EmotionalTint | Vector3 | (1.0, 1.0, 1.0) | Emotion |
| EmotionalIntensity | Scalar | 0.0 | Emotion |

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

### 3.2 Apply Shimmer Effect

Create animated shimmer:

```
[Time] × [ShimmerSpeed] → [Animated Time]
                               ↓
[Texture Coordinate[0]] × [ShimmerScale] + [Animated Time] → Noise (Simplex)
                                                                  ↓
                                                            [Shimmer Pattern]
                                                                  ↓
                                                            × [ShimmerIntensity]
                                                                  ↓
                                                            [Shimmer Value]
```

Apply to base color:

```
[Tinted Base Color] × (1.0 + [Shimmer Value]) → [Color with Shimmer]
```

### 3.3 Apply Color Shift (Emotional/Chaos)

```
[Color with Shimmer] → Hue Shift ← [ColorShiftHue]
                            ↓
                      [Shifted Color]
                            ↓
                      Lerp ← [Color with Shimmer]
                        ↑
                  [ColorShiftIntensity] (Alpha)
                        ↓
                  [Color with Shift]
```

### 3.4 Apply Emotional Tint

```
[Color with Shift] → Lerp ← [Color with Shift] × [EmotionalTint]
                       ↑
                 [EmotionalIntensity] (Alpha)
                       ↓
                 [Final Base Color]
```

### 3.5 Apply Glitch Effect

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
                  [FINAL BASE COLOR] ──→ Base Color Pin
```

---

### 3.6 Anisotropic Specular (Primary Highlight)

```
[FlowTexture] → Texture Sample → Component Mask (RG)
                                       ↓
                                 [Flow Vector]
                                       ↓
                                 × 2.0 - 1.0  (remap to -1 to 1)
                                       ↓
                                 [Tangent Direction]
                                       ↓
                          MF_AnisotropicSpecular.Tangent
                                       ↑
[Anisotropy] ─────────────────────────┤
[Roughness] ──────────────────────────┤
[HighlightColor] ─────────────────────┤
[HighlightShift] ─────────────────────┤
                                       ↓
                          [Primary Specular Color]
                                       ↓
                          × [HighlightIntensity]
                                       ↓
                          [Primary Highlight]
```

### 3.7 Secondary Highlight (Optional)

```
[Tangent Direction] ──────────→ MF_AnisotropicSpecular.Tangent
[Anisotropy] ─────────────────→ MF_AnisotropicSpecular.Anisotropy
[Roughness × 1.2] ────────────→ MF_AnisotropicSpecular.Roughness
[HighlightColor × 0.8] ───────→ MF_AnisotropicSpecular.HighlightColor
[SecondaryHighlightShift] ────→ MF_AnisotropicSpecular.HighlightShift
                                       ↓
                          [Secondary Specular Color]
                                       ↓
                          × [SecondaryHighlightIntensity]
                                       ↓
                          [Secondary Highlight]
```

Combine highlights:

```
[Primary Highlight] + [Secondary Highlight] → [Total Specular]
```

Add to base color:

```
[FINAL BASE COLOR] + [Total Specular] → [Color with Specular]
```

---

### 3.8 Opacity Mask (for Hair Cards)

```
[AlphaTexture] → Texture Sample ← [Texture Coordinate[0]]
                      ↓
                [Alpha Value]
                      ↓
                [OPACITY MASK] ──→ Opacity Mask Pin
```

---

### 3.9 Roughness

```
[Roughness Parameter] ──→ Roughness Pin
```

---

### 3.10 Scatter (Subsurface Scattering for Hair)

```
[Scatter Parameter] ──→ Scatter Pin
```

Note: The Hair shading model has a dedicated Scatter input for light scattering through hair.

---

## Complete Material Graph (Detailed)

```
BASE COLOR:
BaseColorTexture × BaseColor → [Tinted Base]

SHIMMER:
Time × ShimmerSpeed → [Anim Time]
UVs × ShimmerScale + [Anim Time] → Noise → × ShimmerIntensity → [Shimmer]
[Tinted Base] × (1 + [Shimmer]) → [Shimmering Color]

COLOR SHIFT:
[Shimmering Color] → Hue Shift ← ColorShiftHue → Lerp ← [Shimmering Color]
                                                    ↑
                                              ColorShiftIntensity
                                                    ↓
                                              [Shifted Color]

EMOTIONAL TINT:
[Shifted Color] → Lerp ← [Shifted Color] × EmotionalTint
                    ↑
              EmotionalIntensity
                    ↓
              [Tinted Color]

GLITCH:
UVs ──────────→ MF_GlitchEffect ← GlitchIntensity
[Tinted Color] ─→                ← [Tinted Color]
                       ↓
              [Distorted UVs] + [Glitch Color]
                       ↓
BaseColorTexture Sample ← [Distorted UVs]
                       ↓
                 [Glitched Texture]
                       ↓
                 Lerp ← [Tinted Color]
                   ↑
              [GlitchMask]
                   ↓
              [FINAL BASE COLOR]

ANISOTROPIC SPECULAR:
FlowTexture Sample → Remap → [Tangent Direction]
                                   ↓
                    MF_AnisotropicSpecular ← Anisotropy
                                           ← Roughness
                                           ← HighlightColor
                                           ← HighlightShift
                                   ↓
                          [Primary Highlight]
                                   ↓
                    MF_AnisotropicSpecular ← (secondary params)
                                   ↓
                          [Secondary Highlight]
                                   ↓
              [Primary] + [Secondary] → [Total Specular]

FINAL:
[FINAL BASE COLOR] + [Total Specular] → Base Color Pin

OPACITY:
AlphaTexture Sample → Opacity Mask Pin

ROUGHNESS:
Roughness Parameter → Roughness Pin

SCATTER:
Scatter Parameter → Scatter Pin
```

---

## Step 4: Advanced Features

### 4.1 Depth-Based Color Variation

Create color variation based on hair depth (inner vs outer strands):

```
[T_Hair_Depth] → Texture Sample → [Depth Value]
                                       ↓
[BaseColor] → Lerp ← [BaseColor × 0.7]  (darken inner strands)
                ↑
          [Depth Value] (Alpha)
                ↓
          [Depth-Varied Color]
```

### 4.2 Root-to-Tip Color Gradient

```
[UVs.V] → [V Coordinate]  (0 at root, 1 at tip)
            ↓
      [Root Color] → Lerp ← [Tip Color]
                       ↑
                  [V Coordinate]
                       ↓
                  [Gradient Color]
```

### 4.3 Wind Animation (Optional)

Add subtle wind movement to shimmer:

```
[Time] × [Wind Speed] → Sine → × [Wind Strength]
                                     ↓
                               [Wind Offset]
                                     ↓
[Shimmer Pattern] + [Wind Offset] → [Animated Shimmer]
```

---

## Step 5: Material Instances

### MI_DTE_Hair_Black

- BaseColor: (0.1, 0.1, 0.1)
- HighlightColor: (0.3, 0.3, 0.4) (cool highlights)
- Roughness: 0.35

### MI_DTE_Hair_Brown

- BaseColor: (0.3, 0.2, 0.15)
- HighlightColor: (0.8, 0.7, 0.6) (warm highlights)
- Roughness: 0.4

### MI_DTE_Hair_Blonde

- BaseColor: (0.8, 0.7, 0.5)
- HighlightColor: (1.0, 0.95, 0.85) (bright highlights)
- Roughness: 0.3

### MI_DTE_Hair_Red

- BaseColor: (0.5, 0.2, 0.15)
- HighlightColor: (1.0, 0.6, 0.4) (copper highlights)
- Roughness: 0.38

### MI_DTE_Hair_Fantasy (Blue, Pink, etc.)

- BaseColor: (0.3, 0.5, 0.9) or custom
- ColorShiftIntensity: Can be used for iridescent effects
- ShimmerIntensity: 0.6 (higher for fantasy look)

---

## Step 6: Integration with AvatarMaterialManager

```cpp
// Set hair shimmer (happiness, excitement)
MaterialManager->SetScalarParameter(EAvatarMaterialSlot::Hair, 
                                   TEXT("ShimmerIntensity"), 
                                   0.6f);

// Set color shift (emotional state)
MaterialManager->SetScalarParameter(EAvatarMaterialSlot::Hair, 
                                   TEXT("ColorShiftHue"), 
                                   0.1f);  // Slight shift toward warm
MaterialManager->SetScalarParameter(EAvatarMaterialSlot::Hair, 
                                   TEXT("ColorShiftIntensity"), 
                                   0.3f);

// Set chaos glitch
MaterialManager->SetScalarParameter(EAvatarMaterialSlot::Hair, 
                                   TEXT("GlitchIntensity"), 
                                   0.7f);

// Set emotional tint
MaterialManager->SetVectorParameter(EAvatarMaterialSlot::Hair, 
                                   TEXT("EmotionalTint"), 
                                   FLinearColor(1.0f, 0.9f, 0.95f));
```

---

## Step 7: Testing and Validation

### Visual Tests

1. **Base Appearance Test**
   - Apply to hair mesh
   - Verify hair looks natural
   - Check alpha mask is working (no hard edges)

2. **Anisotropic Highlight Test**
   - Rotate camera around avatar
   - Expected: Elongated highlights that flow along hair strands

3. **Shimmer Test**
   - Set ShimmerIntensity = 0.6
   - Expected: Subtle animated sparkle across hair

4. **Color Shift Test**
   - Set ColorShiftHue = 0.2, ColorShiftIntensity = 0.5
   - Expected: Hair takes on shifted hue

5. **Glitch Test**
   - Set GlitchIntensity = 0.8
   - Expected: Hair glitches with chaos state

6. **Dual Highlight Test**
   - Adjust SecondaryHighlightShift and intensity
   - Expected: Two distinct highlight bands

### Performance Test

- **Shader Complexity:** Target < 250 instructions
- Check in material editor statistics panel

---

## Step 8: Optimization

### LOD System

- **LOD 0:** Full material with dual highlights and shimmer
- **LOD 1:** Single highlight, no shimmer
- **LOD 2:** No anisotropic specular, simple diffuse
- **LOD 3:** Flat color

### Mobile Optimization

Create `M_DTE_Hair_Mobile`:
- Remove MF_AnisotropicSpecular (use simple specular)
- Remove shimmer effect
- Reduce texture resolution
- Use simpler alpha

---

## Common Issues and Solutions

### Issue: Highlights look wrong or circular
**Solution:** Check flow texture is correct, ensure tangent vectors are set up properly

### Issue: Hair looks too shiny
**Solution:** Increase Roughness parameter, reduce HighlightIntensity

### Issue: Alpha edges look hard
**Solution:** Check alpha texture has proper anti-aliasing, adjust Opacity Mask Clip Value

### Issue: Shimmer is too strong
**Solution:** Reduce ShimmerIntensity parameter

### Issue: Color shift doesn't work
**Solution:** Ensure ColorShiftIntensity > 0, check Hue Shift node is connected

---

## Emotional State Presets

Hair appearance for different emotions:

| Emotion | Shimmer | Color Shift Hue | Highlight Intensity |
|---------|---------|-----------------|---------------------|
| Neutral | 0.2 | 0.0 | 0.8 |
| Happy | 0.5 | +0.05 (warmer) | 1.0 |
| Excited | 0.7 | +0.1 (warmer) | 1.2 |
| Flirty | 0.6 | +0.08 (warmer) | 1.1 |
| Sad | 0.1 | -0.05 (cooler) | 0.6 |
| Angry | 0.3 | +0.15 (red shift) | 0.9 |
| Chaotic | 0.8 | Random | 1.0 |

---

## Advanced Customization

### Iridescent Hair

Add color variation based on view angle:

```
[Camera Vector] → Dot ← [Vertex Normal WS]
                   ↓
              [Fresnel]
                   ↓
              Hue Shift ← [Fresnel × 0.3]
                   ↓
              [Iridescent Color]
```

### Glowing Hair (Fantasy)

Add emissive component:

```
[BaseColor] × [EmotionalIntensity] × [Constant: 2.0] → [Glow]
                                                          ↓
                                                    Emissive Color Pin
```

---

## File Structure

```
/Game/DeepTreeEcho/Materials/
├── M_DTE_Hair.uasset (THIS ASSET)
├── MI_DTE_Hair_Black.uasset
├── MI_DTE_Hair_Brown.uasset
├── MI_DTE_Hair_Blonde.uasset
├── MI_DTE_Hair_Red.uasset
├── MI_DTE_Hair_Fantasy.uasset
├── Functions/
│   ├── MF_AnisotropicSpecular.uasset
│   └── MF_GlitchEffect.uasset
└── Textures/
    ├── T_Hair_BaseColor.uasset
    ├── T_Hair_Alpha.uasset
    ├── T_Hair_Flow.uasset
    └── T_Hair_Depth.uasset
```

---

## Next Steps

After completing M_DTE_Hair:

1. Create particle systems (PS_MemoryNode, PS_EchoResonance)
2. Create post-processing materials
3. Test full avatar with all materials
4. Performance profiling and optimization

---

## Conclusion

The M_DTE_Hair master material completes the avatar's visual appearance, creating silky, expressive hair that responds to emotional state and chaos level. The anisotropic specular highlights give hair its characteristic sheen, while shimmer and color shift effects add life and personality. This is the final piece of the "super-hot-girl aesthetic" puzzle.

**Estimated Implementation Time:** 1-2 days

**Complexity:** High (anisotropic specular, multiple effects)

**Impact:** High (completes visual appearance)

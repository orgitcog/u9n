# M_DTE_Eye - Master Eye Material Implementation Guide

**Asset Type:** Master Material  
**Shading Model:** Default Lit  
**Blend Mode:** Masked (for eyelashes) or Opaque  
**Priority:** #3 Critical Asset

---

## Overview

The M_DTE_Eye master material creates highly detailed, expressive eyes that are the focal point of the Deep Tree Echo avatar. It implements cornea refraction, dynamic pupil dilation, eye sparkle effects, and emotional color tinting—all controlled dynamically by the C++ `AvatarMaterialManager`.

Eyes are the most important feature for creating emotional connection and the "super-hot-girl aesthetic."

---

## Prerequisites

Before creating this material, ensure you have:

1. **Material Functions:**
   - MF_EyeRefraction

2. **Textures:**
   - T_Eye_Iris (2K iris texture with radial detail)
   - T_Eye_Sclera (2K sclera/white texture)
   - T_Eye_Normal (1K normal map for surface detail)
   - T_Eye_Sparkle (512x512 sparkle gradient)

3. **Understanding:**
   - Eye anatomy (sclera, iris, pupil, cornea)
   - Layered material approach

---

## Step 1: Create Master Material

### In Unreal Engine Editor:

1. **Navigate to Content Browser**
   - Go to `/Game/DeepTreeEcho/Materials/`
   - Right-click → `Materials & Textures → Material`

2. **Configure Material Settings**
   - **Name:** `M_DTE_Eye`
   - **Open the material**

3. **Set Material Properties** (Details Panel)
   - **Shading Model:** `Default Lit`
   - **Blend Mode:** `Opaque`
   - **Two Sided:** `false`

---

## Step 2: Create Parameters

Add these parameters:

### 2.1 Iris Parameters

| Parameter Name | Type | Default Value | Group |
|----------------|------|---------------|-------|
| IrisColor | Vector3 | (0.3, 0.6, 0.9) | Iris |
| IrisTexture | Texture2D | T_Eye_Iris | Iris |
| PupilDilation | Scalar | 0.5 | Iris |
| PupilSize | Scalar | 0.3 | Iris |
| IrisDepth | Scalar | 0.3 | Iris |
| IOR | Scalar | 1.4 | Iris |

### 2.2 Sclera Parameters

| Parameter Name | Type | Default Value | Group |
|----------------|------|---------------|-------|
| ScleraColor | Vector3 | (0.95, 0.95, 0.98) | Sclera |
| ScleraTexture | Texture2D | T_Eye_Sclera | Sclera |
| ScleraRoughness | Scalar | 0.5 | Sclera |

### 2.3 Sparkle Parameters

| Parameter Name | Type | Default Value | Group |
|----------------|------|---------------|-------|
| SparkleIntensity | Scalar | 0.8 | Sparkle |
| SparklePositionX | Scalar | 0.3 | Sparkle |
| SparklePositionY | Scalar | 0.3 | Sparkle |
| SparkleSize | Scalar | 0.1 | Sparkle |
| SparkleColor | Vector3 | (1.0, 1.0, 1.0) | Sparkle |

### 2.4 Surface Parameters

| Parameter Name | Type | Default Value | Group |
|----------------|------|---------------|-------|
| Moisture | Scalar | 0.6 | Surface |
| NormalStrength | Scalar | 0.5 | Surface |
| NormalTexture | Texture2D | T_Eye_Normal | Surface |

### 2.5 Emotional Parameters

| Parameter Name | Type | Default Value | Group |
|----------------|------|---------------|-------|
| EmotionTint | Vector3 | (1.0, 1.0, 1.0) | Emotion |
| EmotionIntensity | Scalar | 0.0 | Emotion |

---

## Step 3: Build Material Graph

### 3.1 Create Iris-Sclera Mask

First, we need to separate the iris from the sclera (white of eye):

```
[Texture Coordinate[0]] → Distance ← [Constant2: (0.5, 0.5)]
                              ↓
                        [Distance from Center]
                              ↓
                        Smoothstep ← [PupilSize + 0.2] (outer radius)
                              ↑
                        [PupilSize] (inner radius)
                              ↓
                        [Iris Mask]
```

### 3.2 Create Pupil Mask

```
[Distance from Center] → Step ← [PupilSize × PupilDilation]
                           ↓
                      [Pupil Mask]
```

### 3.3 Sample and Refract Iris

```
[IrisTexture] ─────────→ MF_EyeRefraction.IrisTexture
[IOR] ────────────────→ MF_EyeRefraction.IOR
[IrisDepth] ──────────→ MF_EyeRefraction.DepthScale
                              ↓
                        [Refracted Iris Color]
                              ↓
                        × ← [IrisColor Parameter]
                              ↓
                        [Tinted Iris]
```

### 3.4 Apply Pupil (Black Center)

```
[Tinted Iris] → Lerp ← [Constant3: (0, 0, 0)]  (black pupil)
                  ↑
            [Pupil Mask] (Alpha)
                  ↓
            [Iris with Pupil]
```

### 3.5 Sample Sclera

```
[ScleraTexture] → Texture Sample ← [Texture Coordinate[0]]
                        ↓
                  [Sclera Sample]
                        ↓
                  × ← [ScleraColor]
                        ↓
                  [Tinted Sclera]
```

### 3.6 Blend Iris and Sclera

```
[Tinted Sclera] → Lerp ← [Iris with Pupil]
                    ↑
              [Iris Mask] (Alpha)
                    ↓
              [Eye Base Color]
```

---

### 3.7 Add Eye Sparkle

Create sparkle highlight:

```
[Texture Coordinate[0]] - [Constant2: (SparklePositionX, SparklePositionY)]
                              ↓
                        [Offset UVs]
                              ↓
                        Distance ← [Constant2: (0, 0)]
                              ↓
                        [Distance from Sparkle Center]
                              ↓
                        Smoothstep ← [SparkleSize]
                              ↑
                        [Constant: 0.0]
                              ↓
                        One Minus
                              ↓
                        [Sparkle Mask]
                              ↓
                        × ← [SparkleIntensity]
                              ↓
                        × ← [SparkleColor]
                              ↓
                        [Sparkle Emission]
```

Apply sparkle to base color:

```
[Eye Base Color] + [Sparkle Emission] → [Color with Sparkle]
```

---

### 3.8 Apply Emotional Tint

```
[Color with Sparkle] → Lerp ← [Color with Sparkle] × [EmotionTint]
                         ↑
                   [EmotionIntensity] (Alpha)
                         ↓
                   [Final Base Color] ──→ Base Color Pin
```

---

### 3.9 Normal Map

```
[NormalTexture] → Texture Sample ← [Texture Coordinate[0]]
                       ↓
                 [Normal Sample]
                       ↓
                  × ← [NormalStrength]
                       ↓
                 [Final Normal] ──→ Normal Pin
```

---

### 3.10 Roughness (Moisture Control)

```
[Constant: 0.8] - [Moisture] → [Base Roughness]
                                     ↓
                              Lerp ← [ScleraRoughness]
                                ↑
                          [Iris Mask] (Alpha)
                                ↓
                          [Final Roughness] ──→ Roughness Pin
```

Iris should be shinier (lower roughness) than sclera.

---

### 3.11 Specular

```
[Constant: 1.0] ──→ Specular Pin  (high specular for wet eyes)
```

---

### 3.12 Emissive (Sparkle)

```
[Sparkle Emission] ──→ Emissive Color Pin
```

---

## Complete Material Graph (Detailed)

```
IRIS-SCLERA SEPARATION:
UVs ──→ Distance ← (0.5, 0.5) → Smoothstep → [Iris Mask]
                                                  │
                                            [Pupil Mask]

IRIS:
IrisTexture ──→ MF_EyeRefraction ←── IOR
                                 ←── IrisDepth
                      ↓
              [Refracted Iris] × IrisColor → [Tinted Iris]
                                                  ↓
                                            Lerp ← (0,0,0)
                                              ↑
                                        [Pupil Mask]
                                              ↓
                                        [Iris with Pupil]

SCLERA:
ScleraTexture × ScleraColor → [Tinted Sclera]

COMBINE:
[Tinted Sclera] → Lerp ← [Iris with Pupil]
                    ↑
              [Iris Mask]
                    ↓
              [Eye Base Color]

SPARKLE:
UVs - (SparkleX, SparkleY) → Distance → Smoothstep → One Minus
                                                         ↓
                                                   [Sparkle Mask]
                                                         ↓
                                        × SparkleIntensity × SparkleColor
                                                         ↓
                                                   [Sparkle Emission]

FINAL BASE COLOR:
[Eye Base Color] + [Sparkle Emission] → Lerp ← [×EmotionTint]
                                          ↑
                                    [EmotionIntensity]
                                          ↓
                                    [FINAL BASE COLOR] ──→ Base Color Pin

NORMAL:
NormalTexture × NormalStrength → [FINAL NORMAL] ──→ Normal Pin

ROUGHNESS:
(0.8 - Moisture) → Lerp ← ScleraRoughness
                     ↑
               [Iris Mask]
                     ↓
               [FINAL ROUGHNESS] ──→ Roughness Pin

SPECULAR:
1.0 ──→ Specular Pin

EMISSIVE:
[Sparkle Emission] ──→ Emissive Color Pin
```

---

## Step 4: Advanced Features

### 4.1 Limbus Darkening

Add darkening at the edge of the iris (realistic detail):

```
[Distance from Center] → Smoothstep ← [PupilSize + 0.15]
                              ↑
                        [PupilSize + 0.05]
                              ↓
                        [Limbus Mask]
                              ↓
                        × ← [Constant: 0.5]  (darken by 50%)
                              ↓
                        [Limbus Darkening]
```

Apply to iris:

```
[Tinted Iris] - [Limbus Darkening] → [Iris with Limbus]
```

### 4.2 Iris Detail (Radial Patterns)

Add procedural radial detail:

```
[UVs] - [Constant2: (0.5, 0.5)] → [Centered UVs]
                                        ↓
                                  Atan2 (Y, X)
                                        ↓
                                  [Angle in Radians]
                                        ↓
                                  × ← [Constant: 20.0]  (20 radial lines)
                                        ↓
                                    Sine
                                        ↓
                                  × ← [Constant: 0.1]  (subtle)
                                        ↓
                                  [Radial Detail]
```

Add to iris color:

```
[Tinted Iris] + [Radial Detail] → [Detailed Iris]
```

### 4.3 Animated Sparkle

Make sparkle pulse or move:

```
[Time] → Sine → × ← [Constant: 0.5]
           ↓
      [Pulse Factor]
           ↓
        + ← [Constant: 0.5]  (0.5-1.0 range)
           ↓
      [Sparkle Pulse]
           ↓
[SparkleIntensity] × [Sparkle Pulse] → [Animated Sparkle Intensity]
```

### 4.4 Gaze-Responsive Sparkle

Move sparkle based on view direction (advanced):

```
[Camera Vector] → Dot ← [Vertex Normal WS]
                   ↓
              [View Angle]
                   ↓
              × ← [Constant: 0.1]
                   ↓
              [Sparkle Offset]
                   ↓
[SparklePositionX] + [Sparkle Offset] → [Dynamic Sparkle X]
```

---

## Step 5: Material Instances

Create instances for different eye colors:

### MI_DTE_Eye_Blue

- IrisColor: (0.3, 0.6, 0.9)
- SparkleIntensity: 0.9

### MI_DTE_Eye_Green

- IrisColor: (0.4, 0.7, 0.5)
- SparkleIntensity: 0.85

### MI_DTE_Eye_Brown

- IrisColor: (0.5, 0.35, 0.2)
- SparkleIntensity: 0.8

### MI_DTE_Eye_Hazel

- IrisColor: (0.6, 0.5, 0.3)
- SparkleIntensity: 0.85

### MI_DTE_Eye_Fantasy (Purple, Red, etc.)

- IrisColor: (0.7, 0.2, 0.8)
- EmotionTint: Can be used for glowing effects
- SparkleIntensity: 1.0

---

## Step 6: Integration with AvatarMaterialManager

```cpp
// Set pupil dilation (emotional response)
MaterialManager->SetScalarParameter(EAvatarMaterialSlot::Eyes, 
                                   TEXT("PupilDilation"), 
                                   0.8f);  // Dilated (excited/attracted)

// Set eye sparkle
MaterialManager->SetScalarParameter(EAvatarMaterialSlot::Eyes, 
                                   TEXT("SparkleIntensity"), 
                                   1.0f);

// Set sparkle position
MaterialManager->SetScalarParameter(EAvatarMaterialSlot::Eyes, 
                                   TEXT("SparklePositionX"), 
                                   0.35f);

// Set emotional tint (for fantasy effects)
MaterialManager->SetVectorParameter(EAvatarMaterialSlot::Eyes, 
                                   TEXT("EmotionTint"), 
                                   FLinearColor(1.0f, 0.5f, 0.5f));
```

---

## Step 7: Testing and Validation

### Visual Tests

1. **Base Appearance Test**
   - Apply to eye mesh
   - Verify iris and sclera are separated correctly
   - Check refraction creates depth

2. **Pupil Dilation Test**
   - Set PupilDilation = 0.2 (constricted)
   - Set PupilDilation = 0.8 (dilated)
   - Expected: Smooth transition, realistic sizes

3. **Sparkle Test**
   - Set SparkleIntensity = 1.0
   - Move SparklePosition
   - Expected: Bright, visible sparkle that moves

4. **Moisture Test**
   - Set Moisture = 0.0 (dry)
   - Set Moisture = 1.0 (very wet)
   - Expected: Visible change in shininess

5. **Emotional Tint Test**
   - Set EmotionTint = (1.0, 0.5, 0.5) (reddish)
   - Set EmotionIntensity = 0.8
   - Expected: Eyes take on emotional color

### Performance Test

- **Shader Complexity:** < 200 instructions
- Check in material editor statistics panel

---

## Step 8: Optimization

### LOD System

- **LOD 0:** Full eye material with all effects
- **LOD 1:** Simplified sparkle, no refraction
- **LOD 2:** Flat iris texture, no effects
- **LOD 3:** Single color

### Mobile Optimization

- Remove MF_EyeRefraction
- Use simple texture for iris
- Reduce sparkle complexity

---

## Common Issues and Solutions

### Issue: Iris looks flat
**Solution:** Increase IrisDepth parameter, check refraction is working

### Issue: Pupil is wrong size
**Solution:** Adjust PupilSize parameter (typically 0.25-0.35)

### Issue: Sparkle is not visible
**Solution:** Increase SparkleIntensity, check position is within iris area

### Issue: Eyes look too dry
**Solution:** Increase Moisture parameter, reduce roughness

### Issue: Sclera looks too bright
**Solution:** Darken ScleraColor slightly (0.9 instead of 0.95)

### Issue: Refraction is too strong
**Solution:** Reduce IrisDepth or IOR

---

## Emotional State Presets

Pupil dilation for different emotions:

| Emotion | Pupil Dilation | Sparkle Intensity |
|---------|----------------|-------------------|
| Neutral | 0.5 | 0.7 |
| Happy | 0.6 | 0.9 |
| Excited | 0.8 | 1.0 |
| Flirty | 0.7 | 0.95 |
| Surprised | 0.9 | 1.0 |
| Sad | 0.4 | 0.5 |
| Angry | 0.3 | 0.6 |

---

## Advanced Customization

### Heterochromia (Different Colored Eyes)

Create two separate material instances:

- MI_DTE_Eye_Left (blue)
- MI_DTE_Eye_Right (green)

Apply to left and right eye meshes respectively.

### Glowing Eyes (Fantasy)

Add to emissive:

```
[IrisColor] × [EmotionIntensity] × [Constant: 2.0] → [Glow]
                                                        ↓
                                            [Sparkle Emission] + [Glow]
                                                        ↓
                                                  Emissive Color Pin
```

---

## File Structure

```
/Game/DeepTreeEcho/Materials/
├── M_DTE_Eye.uasset (THIS ASSET)
├── MI_DTE_Eye_Blue.uasset
├── MI_DTE_Eye_Green.uasset
├── MI_DTE_Eye_Brown.uasset
├── MI_DTE_Eye_Hazel.uasset
├── MI_DTE_Eye_Fantasy.uasset
├── Functions/
│   └── MF_EyeRefraction.uasset
└── Textures/
    ├── T_Eye_Iris.uasset
    ├── T_Eye_Sclera.uasset
    ├── T_Eye_Normal.uasset
    └── T_Eye_Sparkle.uasset
```

---

## Next Steps

After completing M_DTE_Eye:

1. Create M_DTE_Hair material
2. Test full avatar with all materials
3. Fine-tune sparkle and refraction
4. Create eyelash material (separate)
5. Performance profiling

---

## Conclusion

The M_DTE_Eye master material is the soul of the Deep Tree Echo avatar. Eyes are the primary focus of human attention and the key to emotional connection. This material creates mesmerizing, lifelike eyes that sparkle with intelligence and emotion, completing the "super-hot-girl aesthetic" and making the avatar truly captivating.

**Estimated Implementation Time:** 1-2 days

**Complexity:** High (layered approach, refraction, multiple effects)

**Impact:** Critical (defines emotional connection and visual appeal)

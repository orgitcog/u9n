# MF_AnisotropicSpecular - Material Function Implementation Guide

**Asset Type:** Material Function  
**Purpose:** Anisotropic specular highlights for realistic hair rendering  
**Used By:** M_DTE_Hair

---

## Overview

This material function generates anisotropic specular highlights that are characteristic of hair. Unlike isotropic specular (which creates circular highlights), anisotropic specular creates elongated highlights along the hair strand direction, giving hair its distinctive sheen.

---

## Inputs

| Input Name | Type | Description | Default Value |
|------------|------|-------------|---------------|
| Tangent | Vector3 | Hair strand direction (tangent vector) | Vertex Tangent WS |
| Anisotropy | Scalar | Anisotropic strength (0=isotropic, 1=full anisotropic) | 0.7 |
| Roughness | Scalar | Surface roughness | 0.4 |
| HighlightColor | Vector3 (Color) | Color of the specular highlight | (1.0, 1.0, 1.0) |
| HighlightShift | Scalar | Shift highlight along hair strand | 0.0 |
| ViewDirection | Vector3 | Camera view direction | Camera Vector |
| LightDirection | Vector3 | Light direction | Light Vector |
| WorldNormal | Vector3 | Surface normal | Vertex Normal WS |

---

## Outputs

| Output Name | Type | Description |
|-------------|------|-------------|
| SpecularColor | Vector3 (Color) | Final anisotropic specular color |
| SpecularIntensity | Scalar | Specular intensity for masking |

---

## Implementation Steps

### Step 1: Create Material Function

1. **In Content Browser:**
   - Navigate to `/Game/DeepTreeEcho/Materials/Functions/`
   - Right-click → `Materials & Textures → Material Function`
   - Name: `MF_AnisotropicSpecular`

2. **Open the Material Function Editor**

---

### Step 2: Add Input Nodes

1. **Tangent**
   - Type: `Vector3`
   - Use Function Input: `Vertex Tangent WS`
   - Description: "Hair strand direction in world space"

2. **Anisotropy**
   - Type: `Scalar`
   - Default: `0.7`
   - Description: "Anisotropic strength (0-1)"

3. **Roughness**
   - Type: `Scalar`
   - Default: `0.4`
   - Description: "Surface roughness"

4. **HighlightColor**
   - Type: `Vector3`
   - Default: `(1.0, 1.0, 1.0)`
   - Description: "Specular highlight color"

5. **HighlightShift**
   - Type: `Scalar`
   - Default: `0.0`
   - Description: "Shift highlight position (-1 to 1)"

6. **ViewDirection**
   - Type: `Vector3`
   - Use Function Input: `Camera Vector`
   - Description: "View direction from camera"

7. **LightDirection**
   - Type: `Vector3`
   - Use Function Input: `Light Vector`
   - Description: "Direction to light source"

8. **WorldNormal**
   - Type: `Vector3`
   - Use Function Input: `Vertex Normal WS`
   - Description: "Surface normal in world space"

---

### Step 3: Calculate Shifted Tangent

Apply highlight shift to tangent:

```
[WorldNormal] × [HighlightShift] → [Shift Vector]
                                        ↓
[Tangent] + [Shift Vector] → Normalize → [Shifted Tangent]
```

---

### Step 4: Calculate Half Vector

The half vector is halfway between view and light directions:

```
[ViewDirection] + [LightDirection] → Normalize → [Half Vector]
```

---

### Step 5: Calculate Tangent Dot Half

This is the core of anisotropic specular:

```
[Shifted Tangent] → Dot ← [Half Vector]
                      ↓
                [Tangent Dot Half]
```

---

### Step 6: Calculate Anisotropic Term

```
[Tangent Dot Half] → Power ← [Constant: 2.0]
                        ↓
                  [TdotH Squared]
                        ↓
              [Constant: 1.0] - [TdotH Squared]
                        ↓
                  [Sin Squared]
                        ↓
                  Square Root
                        ↓
                  [Sin Theta]
```

---

### Step 7: Calculate Specular Exponent

```
[Roughness] → One Minus → × ← [Constant: 100.0]
                            ↓
                      [Specular Exponent]
                            ↓
                      + ← [Constant: 1.0]  (avoid zero)
                            ↓
                      [Final Exponent]
```

---

### Step 8: Calculate Specular Intensity

```
[Sin Theta] → Power ← [Final Exponent]
                 ↓
           [Specular Term]
                 ↓
           × ← [Anisotropy]
                 ↓
           [Anisotropic Specular]
```

---

### Step 9: Add Isotropic Component

Blend with standard isotropic specular for realism:

```
[WorldNormal] → Dot ← [Half Vector]
                 ↓
           [NdotH]
                 ↓
           Power ← [Final Exponent]
                 ↓
           [Isotropic Specular]
                 ↓
           × ← ([Constant: 1.0] - [Anisotropy])
                 ↓
           [Isotropic Component]
```

Combine:

```
[Anisotropic Specular] + [Isotropic Component] → [Combined Specular]
```

---

### Step 10: Apply Color and Output

```
[Combined Specular] × [HighlightColor] → OUTPUT: SpecularColor
[Combined Specular] → OUTPUT: SpecularIntensity
```

---

## Complete Node Graph

```
SHIFTED TANGENT:
WorldNormal × HighlightShift → [Shift Vector]
                                     ↓
Tangent + [Shift Vector] → Normalize → [Shifted Tangent]

HALF VECTOR:
ViewDirection + LightDirection → Normalize → [Half Vector]

ANISOTROPIC CALCULATION:
[Shifted Tangent] → Dot ← [Half Vector]
                      ↓
                [TdotH]
                      ↓
                Power (2.0)
                      ↓
                [TdotH²]
                      ↓
            1.0 - [TdotH²]
                      ↓
                [Sin²θ]
                      ↓
              Square Root
                      ↓
                [Sin θ]

SPECULAR EXPONENT:
(1 - Roughness) × 100.0 + 1.0 → [Exponent]

ANISOTROPIC SPECULAR:
[Sin θ] → Power ← [Exponent]
            ↓
      [Specular Term]
            ↓
      × Anisotropy
            ↓
      [Aniso Spec]

ISOTROPIC SPECULAR:
WorldNormal → Dot ← [Half Vector]
                ↓
            [NdotH]
                ↓
          Power ← [Exponent]
                ↓
          [Iso Spec]
                ↓
          × (1 - Anisotropy)
                ↓
          [Iso Component]

COMBINE:
[Aniso Spec] + [Iso Component] → [Combined Specular]
                                       ↓
                                × HighlightColor
                                       ↓
                                [Specular Color] → OUTPUT
                                       ↓
                                [Specular Intensity] → OUTPUT
```

---

## Simplified Alternative (Kajiya-Kay Model)

For a simpler, more performant implementation, use the Kajiya-Kay hair lighting model:

```
KAJIYA-KAY:
[Tangent] → Dot ← [LightDirection]
              ↓
          [TdotL]
              ↓
          Power (2.0)
              ↓
          [TdotL²]
              ↓
      1.0 - [TdotL²]
              ↓
      Square Root
              ↓
      [Sin θ]
              ↓
      Power ← [Exponent]
              ↓
      [Specular] × HighlightColor → OUTPUT
```

This is much simpler but still creates convincing anisotropic highlights.

---

## Usage in M_DTE_Hair Material

```
[Tangent from mesh] ──────→ MF_AnisotropicSpecular.Tangent
[Anisotropy Parameter] ───→ MF_AnisotropicSpecular.Anisotropy
[Roughness Parameter] ────→ MF_AnisotropicSpecular.Roughness
[Highlight Color Param] ──→ MF_AnisotropicSpecular.HighlightColor
                                    ↓
                          [SpecularColor]
                                    ↓
[Base Hair Color] + [SpecularColor] → Base Color Pin
```

---

## Testing and Validation

### Visual Tests

1. **Anisotropy Test**
   - Set Anisotropy = 0.0
   - Expected: Circular highlights (isotropic)
   - Set Anisotropy = 1.0
   - Expected: Elongated highlights along hair strands

2. **Roughness Test**
   - Set Roughness = 0.1 (smooth)
   - Expected: Sharp, tight highlights
   - Set Roughness = 0.8 (rough)
   - Expected: Soft, diffuse highlights

3. **Highlight Shift Test**
   - Set HighlightShift = -0.5
   - Set HighlightShift = 0.5
   - Expected: Highlight moves along hair strand

4. **Color Test**
   - Set HighlightColor = (1.0, 0.8, 0.6) (golden)
   - Expected: Warm-toned highlights

### Performance Test

- **Instruction Count:** ~40-50 instructions
- Acceptable for hair materials

---

## Advanced Features (Optional)

### Dual Specular Highlights

Real hair has two specular highlights (primary and secondary):

```
PRIMARY HIGHLIGHT:
MF_AnisotropicSpecular (HighlightShift = 0.0) → [Primary]

SECONDARY HIGHLIGHT:
MF_AnisotropicSpecular (HighlightShift = 0.1, × 0.5 intensity) → [Secondary]

COMBINE:
[Primary] + [Secondary] → [Dual Highlights]
```

### Colored Highlights

Add color variation to highlights:

```
[Base Hair Color] → Desaturate (0.5) → [Desaturated]
                                            ↓
                                      × [Constant: 1.2]  (brighten)
                                            ↓
                                      [Highlight Color]
```

Use this instead of white highlights for more natural look.

---

## Common Issues and Solutions

### Issue: Highlights look circular, not elongated
**Solution:** Ensure Anisotropy > 0.5, check that tangent vectors are correct

### Issue: No highlights visible
**Solution:** Increase HighlightColor intensity, reduce Roughness

### Issue: Highlights are too sharp
**Solution:** Increase Roughness parameter

### Issue: Highlights in wrong direction
**Solution:** Check tangent vector orientation, may need to flip

---

## Optimization Notes

- Relatively expensive due to multiple dot products and power operations
- Consider using simplified Kajiya-Kay model for mobile
- Can be disabled for distant LODs
- Tangent vectors must be properly set up in mesh

---

## Physical Accuracy Notes

Real hair has complex light scattering:
- **Primary highlight:** Direct reflection (R lobe)
- **Secondary highlight:** Internal reflection (TRT lobe)
- **Transmission:** Light passing through hair (TT lobe)

This function implements a simplified model that captures the primary highlight, which is sufficient for most visual purposes.

---

## Next Steps

After implementing this function:

1. Test with hair mesh that has proper tangent vectors
2. Create M_DTE_Hair material using this function
3. Fine-tune Anisotropy and Roughness values
4. Consider implementing dual highlights

---

## Conclusion

The anisotropic specular function is essential for realistic hair rendering. It creates the characteristic sheen and flow of hair that makes it look natural and appealing. This is a key component of the "super-hot-girl aesthetic" by ensuring the hair looks silky, healthy, and attractive.

**Estimated Implementation Time:** 2-3 hours

**Complexity:** Medium-High (requires understanding of lighting models)

**Impact:** High (defines hair visual quality)

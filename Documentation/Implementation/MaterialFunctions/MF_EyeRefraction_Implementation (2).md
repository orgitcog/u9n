# MF_EyeRefraction - Material Function Implementation Guide

**Asset Type:** Material Function  
**Purpose:** Realistic cornea refraction for eye materials  
**Used By:** M_DTE_Eye

---

## Overview

This material function simulates the refraction of light through the cornea (the transparent outer layer of the eye). This creates depth and realism by making the iris appear to be "inside" the eye rather than painted on the surface.

---

## Inputs

| Input Name | Type | Description | Default Value |
|------------|------|-------------|---------------|
| IrisTexture | Texture2D | Iris color and detail texture | White |
| IOR | Scalar | Index of Refraction (cornea = 1.376) | 1.4 |
| ViewDirection | Vector3 | Camera view direction | Camera Vector |
| WorldNormal | Vector3 | Surface normal | Vertex Normal WS |
| UVs | Vector2 | Texture coordinates | Texture Coordinate[0] |
| DepthScale | Scalar | Depth of iris beneath cornea | 0.3 |

---

## Outputs

| Output Name | Type | Description |
|-------------|------|-------------|
| RefractedColor | Vector3 (Color) | Final refracted iris color |
| RefractedUVs | Vector2 | Refracted UV coordinates |

---

## Implementation Steps

### Step 1: Create Material Function

1. **In Content Browser:**
   - Navigate to `/Game/DeepTreeEcho/Materials/Functions/`
   - Right-click → `Materials & Textures → Material Function`
   - Name: `MF_EyeRefraction`

2. **Open the Material Function Editor**

---

### Step 2: Add Input Nodes

1. **IrisTexture**
   - Type: `Texture2D`
   - Description: "Iris texture with color and detail"

2. **IOR**
   - Type: `Scalar`
   - Default: `1.4`
   - Description: "Index of Refraction (1.0=no refraction, 1.4=cornea)"

3. **ViewDirection**
   - Type: `Vector3`
   - Use Function Input: `Camera Vector`
   - Description: "View direction from camera"

4. **WorldNormal**
   - Type: `Vector3`
   - Use Function Input: `Vertex Normal WS`
   - Description: "Surface normal in world space"

5. **UVs**
   - Type: `Vector2`
   - Use Function Input: `Texture Coordinate[0]`
   - Description: "Base UV coordinates"

6. **DepthScale**
   - Type: `Scalar`
   - Default: `0.3`
   - Description: "Depth of iris beneath cornea surface"

---

### Step 3: Calculate Refraction Vector

#### 3.1 Calculate View-Normal Dot Product

```
[ViewDirection] → Dot ← [WorldNormal]
                   ↓
              [View Dot Normal]
```

#### 3.2 Calculate Refraction Using Snell's Law

Unreal Engine has a built-in `Fresnel` node, but for more control, we calculate manually:

```
[Constant: 1.0] ÷ [IOR] → [IOR Ratio]
                            ↓
                      [n1/n2 Ratio]
```

#### 3.3 Calculate Refracted Direction

Using the refraction formula:
```
R = (n1/n2) * I - N * ((n1/n2) * dot(N, I) + sqrt(1 - (n1/n2)^2 * (1 - dot(N, I)^2)))
```

In Unreal nodes:

```
[IOR Ratio] → Power ← [Constant: 2.0]
                ↓
          [IOR Ratio Squared]
                ↓
[Constant: 1.0] - [View Dot Normal] → Power ← [Constant: 2.0]
                                        ↓
                                  [1 - cos²]
                                        ↓
                          × ← [IOR Ratio Squared]
                                        ↓
                          [IOR² * (1 - cos²)]
                                        ↓
                     [Constant: 1.0] - [Above]
                                        ↓
                                  Square Root
                                        ↓
                              [Sqrt Term]
```

This gets complex, so **use Unreal's built-in Refraction node** instead:

```
[ViewDirection] → Refraction ← [WorldNormal]
                       ↑
                    [IOR]
                       ↓
              [Refracted Direction]
```

---

### Step 4: Apply Refraction to UVs

#### 4.1 Convert Refracted Direction to UV Offset

```
[Refracted Direction] → Component Mask (RG)  (X and Y components)
                              ↓
                        [2D Offset Vector]
                              ↓
                        × ← [DepthScale]
                              ↓
                        [Scaled Offset]
```

#### 4.2 Apply to UVs

```
[UVs] + [Scaled Offset] → [Refracted UVs]
```

---

### Step 5: Sample Iris with Refracted UVs

```
[IrisTexture] → Texture Sample ← [Refracted UVs]
                      ↓
              [Refracted Iris Color] → OUTPUT: RefractedColor
```

Also output the UVs:

```
[Refracted UVs] → OUTPUT: RefractedUVs
```

---

## Complete Node Graph

```
ViewDirection ──┬──→ Refraction ←── WorldNormal
                │         ↑
                │       [IOR]
                │         ↓
                │   [Refracted Direction]
                │         ↓
                │   Component Mask (RG)
                │         ↓
                │   [2D Offset]
                │         ↓
                │   × ← DepthScale
                │         ↓
                │   [Scaled Offset]
                │         ↓
UVs ────────────┴──→ + ←─┘
                      ↓
                [Refracted UVs]
                      ├──→ OUTPUT: RefractedUVs
                      │
IrisTexture ──→ Sample ←┘
                      ↓
              [Refracted Color] ──→ OUTPUT: RefractedColor
```

---

## Simplified Alternative (If Refraction Node Not Available)

If the Refraction node is not working as expected, use a simpler parallax-based approach:

### Simplified Method:

```
[ViewDirection] → Dot ← [WorldNormal]
                   ↓
              [View Angle]
                   ↓
              One Minus  (invert)
                   ↓
              [Parallax Factor]
                   ↓
              × ← [DepthScale]
                   ↓
              [UV Offset Amount]
                   ↓
[ViewDirection] → Component Mask (RG)
                   ↓
              [View XY]
                   ↓
              × ← [UV Offset Amount]
                   ↓
              [Final Offset]
                   ↓
[UVs] + [Final Offset] → [Offset UVs]
                            ↓
IrisTexture → Sample ← [Offset UVs]
                            ↓
                    [Refracted Color]
```

This is less physically accurate but much simpler and still creates the depth effect.

---

## Usage in M_DTE_Eye Material

In the M_DTE_Eye master material:

```
[T_Iris] ────────────→ MF_EyeRefraction.IrisTexture
[IOR Parameter] ─────→ MF_EyeRefraction.IOR
[DepthScale Param] ──→ MF_EyeRefraction.DepthScale
                            ↓
                      [RefractedColor] ──→ Base Color Pin
                            ↓
                      [RefractedUVs] ──→ (use for other effects)
```

---

## Testing and Validation

### Visual Tests

1. **No Refraction Test**
   - Set IOR = 1.0
   - Expected: No refraction, iris looks flat

2. **Full Refraction Test**
   - Set IOR = 1.4
   - Expected: Iris appears to have depth, shifts with viewing angle

3. **Depth Test**
   - Vary DepthScale from 0.0 to 1.0
   - Expected: More depth = more UV offset

4. **Viewing Angle Test**
   - View eye from different angles
   - Expected: Iris shifts position realistically

### Performance Test

- **Instruction Count:** ~15-20 instructions
- Very efficient if using built-in Refraction node

---

## Advanced Features (Optional)

### 5.1 Limbus Darkening

Add darkening at the edge of the iris (limbus):

```
[UVs] → Distance ← [Constant2: (0.5, 0.5)]  (center)
           ↓
      [Distance from Center]
           ↓
      Smoothstep ← [Constant: 0.3] (inner)
           ↑
      [Constant: 0.5] (outer)
           ↓
      [Limbus Mask]
           ↓
      × ← [Constant: 0.5]  (darken amount)
           ↓
      [Limbus Darkening]
           ↓
[Refracted Color] - [Limbus Darkening] → [Final Color]
```

### 5.2 Caustics Effect

Add subtle caustics (light patterns) on the iris:

```
[Time] → × ← [Constant: 0.5]
           ↓
      [Animated Time]
           ↓
[UVs] + [Animated Time] → Noise (Voronoi)
                             ↓
                        [Caustic Pattern]
                             ↓
                        × ← [Constant: 0.1]  (subtle)
                             ↓
                        [Caustic Intensity]
                             ↓
[Refracted Color] + [Caustic Intensity] → [Color with Caustics]
```

---

## Common Issues and Solutions

### Issue: Refraction is too strong
**Solution:** Reduce DepthScale parameter

### Issue: Iris looks distorted
**Solution:** Check IOR value (should be around 1.3-1.5 for eyes)

### Issue: No visible refraction
**Solution:** Ensure DepthScale > 0 and IOR != 1.0

### Issue: Refraction looks wrong at grazing angles
**Solution:** Add clamping to prevent extreme UV offsets

---

## Optimization Notes

- Very efficient function (few operations)
- No texture lookups in the function itself
- Can be used in mobile materials
- Consider disabling for distant LODs

---

## Physical Accuracy Notes

Real eye refraction is complex:

- **Cornea IOR:** ~1.376
- **Aqueous humor IOR:** ~1.336
- **Lens IOR:** ~1.406

For visual purposes, a single IOR of 1.4 is sufficient and looks good.

---

## Next Steps

After implementing this function:

1. Test in M_DTE_Eye material
2. Fine-tune IOR and DepthScale values
3. Add limbus darkening (optional)
4. Integrate with eye sparkle effect

---

## Conclusion

The eye refraction function is essential for creating realistic, believable eyes. It's a relatively simple function that has a huge visual impact, making the eyes appear three-dimensional and lifelike. This is a critical component of the "super-hot-girl aesthetic" and the avatar's overall visual appeal.

**Estimated Implementation Time:** 1-2 hours

**Complexity:** Medium (requires understanding of refraction)

**Impact:** Very high (defines eye realism)

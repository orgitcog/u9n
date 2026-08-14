# MF_GlitchEffect - Material Function Implementation Guide

**Asset Type:** Material Function  
**Purpose:** Visual glitch artifacts for hyper-chaotic behavior visualization  
**Used By:** M_DTE_Skin, M_DTE_Hair, M_DTE_Eye, PP_DeepTreeEcho_Glitch

---

## Overview

This material function creates digital glitch effects that visualize the avatar's chaotic behavior state. The effect includes UV distortion, color channel separation, scanline artifacts, and digital noise—all modulated by the `GlitchIntensity` parameter.

---

## Inputs

| Input Name | Type | Description | Default Value |
|------------|------|-------------|---------------|
| GlitchIntensity | Scalar | Intensity of glitch effect (0-1) | 0.0 |
| Time | Scalar | Time for animated glitches | Time |
| UVs | Vector2 | Original texture coordinates | Texture Coordinate[0] |
| BaseColor | Vector3 (Color) | Original color (for chromatic aberration) | (1,1,1) |

---

## Outputs

| Output Name | Type | Description |
|-------------|------|-------------|
| DistortedUVs | Vector2 | UV coordinates with glitch distortion |
| GlitchColor | Vector3 (Color) | Color overlay for glitch effect |
| GlitchMask | Scalar | Mask for blending glitch effect |

---

## Implementation Steps

### Step 1: Create Material Function

1. **In Content Browser:**
   - Navigate to `/Game/DeepTreeEcho/Materials/Functions/`
   - Right-click → `Materials & Textures → Material Function`
   - Name: `MF_GlitchEffect`

2. **Open the Material Function Editor**

---

### Step 2: Add Input Nodes

1. **GlitchIntensity**
   - Type: `Scalar`
   - Default: `0.0`
   - Description: "Glitch effect intensity (0=none, 1=full)"

2. **Time**
   - Type: `Scalar`
   - Use Function Input: `Time`
   - Description: "Time for animated effects"

3. **UVs**
   - Type: `Vector2`
   - Use Function Input: `Texture Coordinate[0]`
   - Description: "Original UV coordinates"

4. **BaseColor**
   - Type: `Vector3`
   - Default: `(1, 1, 1)`
   - Description: "Base color for chromatic aberration"

---

### Step 3: Build Glitch Effect Network

#### 3.1 Generate Random Glitch Trigger

Create a stepped random function that triggers glitches:

```
[Time] → Floor → × ← [Constant: 10.0]  (10 glitches per second max)
           ↓
      [Stepped Time]
           ↓
        Frac → × ← [Constant: 12.9898]
           ↓
         Sin → × ← [Constant: 43758.5453]
           ↓
         Frac
           ↓
      [Random Value 0-1]
```

#### 3.2 Create Glitch Threshold

```
[Random Value] → Step ← [One Minus ← [GlitchIntensity]]
                   ↓
            [Glitch Trigger]  (0 or 1)
```

This creates random glitches that occur more frequently as intensity increases.

#### 3.3 Generate Horizontal Displacement

Create scanline-like horizontal distortion:

```
[UVs.Y] → × ← [Constant: 50.0]  (50 scanlines)
           ↓
         Frac → × ← [Time]
           ↓
         Sin → × ← [Glitch Trigger]
           ↓
      [Horizontal Offset]
           ↓
        × ← [GlitchIntensity] × [Constant: 0.1]  (max 10% displacement)
           ↓
      [Scaled Offset]
```

#### 3.4 Generate Vertical Displacement

Create vertical jitter:

```
[Time] → × ← [Constant: 20.0]
           ↓
         Sin → × ← [Glitch Trigger]
           ↓
      [Vertical Offset]
           ↓
        × ← [GlitchIntensity] × [Constant: 0.05]  (max 5% displacement)
           ↓
      [Scaled Vertical Offset]
```

#### 3.5 Apply UV Distortion

```
[UVs.X] + [Scaled Offset] → [Distorted U]
[UVs.Y] + [Scaled Vertical Offset] → [Distorted V]
                                        ↓
                              [Distorted UVs] → OUTPUT
```

---

### Step 4: Create Chromatic Aberration

Separate RGB channels for glitch effect:

#### 4.1 Red Channel Offset

```
[UVs] + [Constant2: (0.01, 0.0)] × [GlitchIntensity] → [Red UVs]
```

#### 4.2 Blue Channel Offset

```
[UVs] + [Constant2: (-0.01, 0.0)] × [GlitchIntensity] → [Blue UVs]
```

#### 4.3 Combine Channels

```
[BaseColor.R sampled at Red UVs] → Component Mask (R) ──┐
[BaseColor.G sampled at UVs] → Component Mask (G) ──────┼→ Append
[BaseColor.B sampled at Blue UVs] → Component Mask (B) ─┘     ↓
                                                    [Glitch Color] → OUTPUT
```

---

### Step 5: Create Scanline Effect

Add horizontal lines for digital artifact look:

```
[UVs.Y] → × ← [Constant: 200.0]  (200 scanlines)
           ↓
         Frac → Step ← [Constant: 0.5]
           ↓
      [Scanline Mask]
           ↓
        × ← [GlitchIntensity] × [Constant: 0.3]
           ↓
      [Scanline Intensity]
```

Add to GlitchColor:

```
[Glitch Color] + [Scanline Intensity] → [Final Glitch Color]
```

---

### Step 6: Create Digital Noise

Add random noise for digital artifact appearance:

```
[UVs] → × ← [Time] → Noise (Simplex or Perlin)
                        ↓
                  [Noise Pattern]
                        ↓
                     × ← [GlitchIntensity]
                        ↓
                  [Noise Intensity]
```

---

### Step 7: Generate Glitch Mask

Combine all glitch factors into a mask:

```
[Glitch Trigger] + [Scanline Intensity] + [Noise Intensity]
                        ↓
                    Saturate  (clamp to 0-1)
                        ↓
                  [Glitch Mask] → OUTPUT
```

---

## Complete Node Graph (Simplified)

```
Time ──→ Floor ──→ × ←── 10.0
              ↓
         [Stepped Time]
              ↓
         Random Function
              ↓
         [Random 0-1]
              ↓
         Step ←── (1 - GlitchIntensity)
              ↓
         [Glitch Trigger]
              │
              ├──→ × ←── [Horizontal Offset Calculation]
              │         │
              │    [H Offset]
              │         │
UVs.X ────────┼────→ + ←┘
              │         │
              │    [Distorted U] ──┐
              │                    │
              ├──→ × ←── [Vertical Offset Calculation]
              │         │          │
              │    [V Offset]      │
              │         │          │
UVs.Y ────────┼────→ + ←┘          │
              │         │          │
              │    [Distorted V] ──┤
              │                    │
              │              [Distorted UVs] ──→ OUTPUT
              │
              ├──→ Chromatic Aberration Calculation
              │         │
              │    [Glitch Color] ──→ OUTPUT
              │
              └──→ Scanline + Noise Calculation
                        │
                   [Glitch Mask] ──→ OUTPUT
```

---

## Usage in Materials

### In M_DTE_Skin (or any material):

```
[Texture Sample] ←── MF_GlitchEffect.DistortedUVs
        ↓
   [Distorted Texture]
        ↓
     Lerp ←── MF_GlitchEffect.GlitchColor (B)
        ↑
   [Original Color] (A)
        ↑
   MF_GlitchEffect.GlitchMask (Alpha)
        ↓
   [Final Color] ──→ Base Color Pin
```

---

## Advanced Features (Optional)

### 5.1 Block Displacement

Create rectangular glitch blocks:

```
[UVs] → Floor ← [Constant2: (10, 10)]  (10x10 grid)
           ↓
      [Block UVs]
           ↓
      Random Function
           ↓
      [Block Random]
           ↓
      × ← [Glitch Trigger]
           ↓
      [Block Displacement]
```

### 5.2 RGB Shift Animation

Animate the chromatic aberration:

```
[Time] → Sin → × ← [Constant: 0.02]
           ↓
      [Animated Offset]
           ↓
      Use for RGB channel separation
```

### 5.3 Data Moshing Effect

Create compression artifact look:

```
[UVs] → Posterize ← [Constant: 8.0]
           ↓
      [Pixelated UVs]
           ↓
      Lerp ← [Original UVs]
        ↑
   [GlitchIntensity] (Alpha)
```

---

## Testing and Validation

### Visual Tests

1. **Zero Intensity Test**
   - GlitchIntensity = 0.0
   - Expected: No glitch, original appearance

2. **Low Intensity Test**
   - GlitchIntensity = 0.3
   - Expected: Occasional subtle glitches

3. **High Intensity Test**
   - GlitchIntensity = 0.8
   - Expected: Frequent, obvious glitches

4. **Full Chaos Test**
   - GlitchIntensity = 1.0
   - Expected: Constant glitching, heavy distortion

### Performance Test

- **Instruction Count:** Target < 80 instructions
- **Check in Material Editor:** Statistics panel
- **Optimize:** Remove features if over budget

---

## Performance Optimization

### For Mobile/Low-End:

1. **Simplified Version:**
   - Remove chromatic aberration
   - Remove noise
   - Keep only UV distortion and scanlines

2. **Reduce Calculations:**
   - Use simpler random function
   - Fewer scanlines (50 instead of 200)

3. **LOD System:**
   - LOD 0: Full effect
   - LOD 1: No chromatic aberration
   - LOD 2: Only UV distortion
   - LOD 3: No glitch effect

---

## Integration with C++ AvatarMaterialManager

```cpp
// Trigger glitch effect
MaterialManager->ApplyGlitchEffect(0.8f);

// This sets GlitchIntensity parameter on all materials
// The glitch effect will automatically appear
```

---

## Chaotic Behavior Presets

Suggested glitch intensities for different chaos levels:

| Chaos Factor | Glitch Intensity | Visual Effect |
|--------------|------------------|---------------|
| 0.0 - 0.2 | 0.0 | No glitch |
| 0.2 - 0.4 | 0.1 | Rare, subtle glitches |
| 0.4 - 0.6 | 0.3 | Occasional noticeable glitches |
| 0.6 - 0.8 | 0.6 | Frequent glitches |
| 0.8 - 1.0 | 0.9 | Constant heavy glitching |

---

## Post-Processing Glitch (Optional)

For screen-space glitch effect, create PP_DeepTreeEcho_Glitch:

1. **Create Post Process Material**
2. **Use same MF_GlitchEffect function**
3. **Apply to entire screen**
4. **Controlled by global glitch intensity**

This creates a more dramatic effect when chaos is very high.

---

## Troubleshooting

### Issue: Glitches are too frequent
**Solution:** Reduce the multiplier in the random trigger (use 5.0 instead of 10.0)

### Issue: UV distortion is too extreme
**Solution:** Reduce the displacement multiplier (use 0.05 instead of 0.1)

### Issue: Performance is poor
**Solution:** Remove chromatic aberration and noise, keep only UV distortion

### Issue: Glitch doesn't appear
**Solution:** Check that GlitchIntensity parameter is being set from C++

---

## Next Steps

After implementing this function:

1. Test in M_DTE_Skin material
2. Apply to M_DTE_Hair and M_DTE_Eye
3. Create post-processing version
4. Integrate with AvatarAnimInstance chaotic behavior
5. Fine-tune glitch appearance

---

## Conclusion

The glitch effect function is essential for visualizing the avatar's hyper-chaotic behavior. It provides immediate, dramatic visual feedback that makes the chaos state tangible and visually interesting. Combined with the chaotic animation behaviors, it creates a unique "glitching AI" aesthetic.

**Estimated Implementation Time:** 3-4 hours

**Complexity:** High (many interconnected nodes)

**Impact:** Very high (signature visual effect for chaos state)

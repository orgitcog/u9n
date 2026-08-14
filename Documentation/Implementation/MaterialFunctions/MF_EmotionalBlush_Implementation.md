# MF_EmotionalBlush - Material Function Implementation Guide

**Asset Type:** Material Function  
**Purpose:** Dynamic emotional blush effect for skin materials  
**Used By:** M_DTE_Skin

---

## Overview

This material function generates a dynamic blush effect that responds to emotional state. The blush appears primarily on the cheeks, nose, and ears, with intensity controlled by the `BlushIntensity` parameter from the C++ `AvatarMaterialManager`.

---

## Inputs

| Input Name | Type | Description | Default Value |
|------------|------|-------------|---------------|
| EmotionIntensity | Scalar | Intensity of emotional blush (0-1) | 0.0 |
| BlushColor | Vector3 (Color) | Color of the blush | (1.0, 0.5, 0.5) |
| BlushMask | Texture2D | Grayscale mask for blush zones | White |
| UVs | Vector2 | Texture coordinates | Texture Coordinate[0] |
| Time | Scalar | Time for pulsing effect | Time |

---

## Outputs

| Output Name | Type | Description |
|-------------|------|-------------|
| BlushColor | Vector3 (Color) | Final blush color to add to base color |
| BlushMask | Scalar | Mask value for blending |

---

## Implementation Steps

### Step 1: Create Material Function

1. **In Content Browser:**
   - Navigate to `/Game/DeepTreeEcho/Materials/Functions/`
   - Right-click → `Materials & Textures → Material Function`
   - Name: `MF_EmotionalBlush`

2. **Open the Material Function Editor**

---

### Step 2: Add Input Nodes

1. **EmotionIntensity**
   - Type: `Scalar`
   - Default: `0.0`
   - Description: "Blush intensity from emotional state"

2. **BlushColor**
   - Type: `Vector3`
   - Default: `(1.0, 0.5, 0.5)`
   - Description: "Color of the blush (pinkish-red)"

3. **BlushMask**
   - Type: `Texture2D`
   - Default: `White` (or create proper mask texture)
   - Description: "Grayscale mask defining blush zones"

4. **UVs**
   - Type: `Vector2`
   - Use Function Input: `Texture Coordinate[0]`
   - Description: "UV coordinates"

5. **Time**
   - Type: `Scalar`
   - Use Function Input: `Time`
   - Description: "Time for animated effects"

---

### Step 3: Create Blush Mask Texture (Asset Creation)

Before implementing the function, create the blush mask texture:

1. **In Image Editor (Photoshop, GIMP, etc.):**
   - Create 1024x1024 grayscale image
   - Paint white areas where blush should appear:
     - **Cheeks:** Large circular gradients
     - **Nose:** Small gradient on tip
     - **Ears:** Gradient on outer edge
     - **Forehead:** Very subtle gradient (optional)
   - Use soft brushes with feathered edges
   - Save as `T_BlushMask.png`

2. **Import to Unreal:**
   - Import to `/Game/DeepTreeEcho/Textures/`
   - Texture settings:
     - sRGB: OFF (it's a mask, not color)
     - Compression: Grayscale
     - Mip Gen Settings: NoMipmaps

---

### Step 4: Build Blush Calculation Network

#### 4.1 Sample Blush Mask

```
[UVs] → Texture Sample ← [BlushMask Texture]
              ↓
        [Mask Value]
```

#### 4.2 Add Pulsing Animation (Optional)

For a subtle pulsing effect when emotional:

```
[Time] → Sine → × ← [Constant: 0.5]
           ↓
      [Pulse Factor]
           ↓
        + ← [Constant: 0.5]  (offset to 0.5-1.0 range)
           ↓
      [Pulse Multiplier]
```

#### 4.3 Modulate Intensity

```
[EmotionIntensity] × [Pulse Multiplier] × [Mask Value] → [Final Intensity]
```

#### 4.4 Apply Color

```
[BlushColor] × [Final Intensity] → [Final Blush Color]
```

#### 4.5 Add Saturation Boost

To make blush more vibrant:

```
[Final Blush Color] → Desaturate (Fraction: 0.0) → Lerp
                                                      ↑
                                    [Final Blush Color] (A)
                                                      ↑
                              [Saturated Blush] (B) ←┘
                                                      ↑
                                    [Final Intensity] (Alpha)
```

---

## Complete Node Graph

```
UVs ──→ Texture Sample ←── BlushMask
              │
        [Mask Value]
              │
Time ──→ Sine ──→ × ←── 0.5
              │
         [Pulse] ──→ + ←── 0.5
                      │
              [Pulse Multiplier]
                      │
EmotionIntensity ──→ × ←── [Pulse Multiplier]
                      │
                      ├──→ × ←── [Mask Value]
                      │         │
                      │   [Final Intensity]
                      │         │
BlushColor ──────────→ × ←──────┘
                      │
              [Blush Color Output]
                      │
                      ├──→ OUTPUT: BlushColor
                      │
              [Final Intensity] ──→ OUTPUT: BlushMask
```

---

## Advanced Features (Optional)

### 4.6 Add Temperature Variation

Make blush warmer or cooler based on emotion:

```
[BlushColor] → Hue Shift ← [Emotion-based Hue Offset]
                  ↓
          [Temperature-Adjusted Blush]
```

- Happy/Flirty: Warmer (shift toward orange)
- Sad: Cooler (shift toward purple)
- Angry: Much warmer (shift toward red)

### 4.7 Add Noise for Realism

Add subtle noise to avoid uniform appearance:

```
[Noise Texture] → × ← [Constant: 0.1]
        ↓
   [Subtle Noise]
        ↓
   + ← [Final Intensity]
        ↓
   [Noisy Intensity]
```

---

## Usage in M_DTE_Skin Material

In the M_DTE_Skin master material:

```
[Base Skin Color] ──┬──→ Base Color Pin
                    │
                    └──→ + ←── MF_EmotionalBlush.BlushColor
                                        │
[BlushIntensity Param] ────────────→ MF_EmotionalBlush.EmotionIntensity
                                        │
[BlushColor Param] ─────────────────→ MF_EmotionalBlush.BlushColor
                                        │
[T_BlushMask] ──────────────────────→ MF_EmotionalBlush.BlushMask
```

---

## Testing and Validation

### Visual Tests

1. **Zero Intensity Test**
   - Set EmotionIntensity = 0.0
   - Expected: No visible blush

2. **Full Intensity Test**
   - Set EmotionIntensity = 1.0
   - Expected: Strong, visible blush on cheeks, nose, ears

3. **Pulsing Test**
   - Set EmotionIntensity = 0.5
   - Observe over time
   - Expected: Subtle pulsing effect

4. **Color Variation Test**
   - Try different BlushColor values:
     - Pink: (1.0, 0.5, 0.5)
     - Red: (1.0, 0.2, 0.2)
     - Purple: (0.8, 0.3, 0.6)

### Integration Test

Test with C++ AvatarMaterialManager:

```cpp
// In C++ or Blueprint
MaterialManager->ApplyEmotionalBlush(0.8f, FLinearColor(1.0f, 0.5f, 0.5f));
```

Expected: Blush appears on avatar's face

---

## Performance Optimization

- **Instruction Count:** ~20-30 instructions
- **Texture Samples:** 1 (blush mask)
- **Optimization Tips:**
  - Remove pulsing effect for mobile
  - Use lower resolution blush mask (512x512) for distant LODs
  - Combine blush mask with other masks (R=blush, G=other, B=other)

---

## Creating the Blush Mask Texture (Detailed)

### Photoshop/GIMP Instructions:

1. **Create New Image**
   - Size: 1024x1024
   - Mode: Grayscale
   - Background: Black

2. **Paint Cheek Blush**
   - Select soft round brush (500px, 0% hardness)
   - Color: White
   - Opacity: 50%
   - Paint two circular areas where cheeks would be
   - Apply Gaussian Blur (radius: 50px)

3. **Paint Nose Blush**
   - Smaller brush (100px)
   - Opacity: 30%
   - Paint on nose tip

4. **Paint Ear Blush**
   - Medium brush (200px)
   - Opacity: 40%
   - Paint on ear edges

5. **Final Adjustments**
   - Levels adjustment: Ensure blacks are pure black
   - Slight blur for smooth transitions
   - Save as PNG (8-bit grayscale)

### Alternative: Procedural Mask

If you don't want to create a texture, use procedural mask:

```
[UVs] → Radial Gradient → Multiply ← [Position Offsets]
              ↓
        [Procedural Mask]
```

---

## Emotional State Presets

Suggested blush intensities for different emotions:

| Emotion | Intensity | Color |
|---------|-----------|-------|
| Neutral | 0.0 | N/A |
| Happy | 0.3 | (1.0, 0.6, 0.5) |
| Flirty | 0.7 | (1.0, 0.5, 0.5) |
| Embarrassed | 0.9 | (1.0, 0.4, 0.4) |
| Angry | 0.5 | (1.0, 0.3, 0.2) |
| Excited | 0.4 | (1.0, 0.5, 0.4) |

These can be set in the C++ `UpdateEmotionalState` function.

---

## Next Steps

After implementing this function:

1. Create blush mask texture (T_BlushMask)
2. Test in M_DTE_Skin material
3. Integrate with AvatarMaterialManager
4. Create MF_GlitchEffect function
5. Fine-tune blush appearance with artists

---

## Conclusion

The emotional blush function is a key component of the avatar's emotional expressiveness. It provides immediate visual feedback for emotional states like happiness, flirtiness, and embarrassment, enhancing the "super-hot-girl aesthetic" and making the avatar feel more alive and relatable.

**Estimated Implementation Time:** 2-3 hours (including texture creation)

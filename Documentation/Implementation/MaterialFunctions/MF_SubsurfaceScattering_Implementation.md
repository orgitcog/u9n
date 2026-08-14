# MF_SubsurfaceScattering - Material Function Implementation Guide

**Asset Type:** Material Function  
**Purpose:** Realistic subsurface scattering for skin materials  
**Used By:** M_DTE_Skin, M_DTE_Outfit (optional)

---

## Overview

This material function implements physically-based subsurface scattering (SSS) for realistic skin rendering. It calculates how light penetrates and scatters beneath the skin surface, creating the soft, translucent appearance characteristic of human skin.

---

## Inputs

| Input Name | Type | Description | Default Value |
|------------|------|-------------|---------------|
| BaseColor | Vector3 (Color) | Surface color of the skin | (1.0, 0.9, 0.85) |
| Thickness | Scalar | Thickness of the skin layer | 0.5 |
| ScatterIntensity | Scalar | Intensity of subsurface scattering | 0.8 |
| ScatterRadius | Scalar | Radius of light scattering | 1.5 |
| WorldPosition | Vector3 | World space position (auto) | World Position |
| WorldNormal | Vector3 | World space normal (auto) | Vertex Normal WS |

---

## Outputs

| Output Name | Type | Description |
|-------------|------|-------------|
| SubsurfaceColor | Vector3 (Color) | Final SSS color to add to material |
| SubsurfaceOpacity | Scalar | Opacity for blending SSS effect |

---

## Implementation Steps

### Step 1: Create Material Function

1. **In Content Browser:**
   - Navigate to `/Game/DeepTreeEcho/Materials/Functions/`
   - Right-click → `Materials & Textures → Material Function`
   - Name: `MF_SubsurfaceScattering`

2. **Open the Material Function Editor**

---

### Step 2: Add Input Nodes

Add these input nodes (right-click → `Function Input`):

1. **BaseColor**
   - Type: `Vector3`
   - Default: `(1.0, 0.9, 0.85)`
   - Description: "Base skin color"

2. **Thickness**
   - Type: `Scalar`
   - Default: `0.5`
   - Description: "Skin thickness (0=thin, 1=thick)"

3. **ScatterIntensity**
   - Type: `Scalar`
   - Default: `0.8`
   - Description: "SSS intensity"

4. **ScatterRadius**
   - Type: `Scalar`
   - Default: `1.5`
   - Description: "Light scatter radius in world units"

5. **WorldPosition**
   - Type: `Vector3`
   - Use Function Input: `World Position`
   - Description: "World space position"

6. **WorldNormal**
   - Type: `Vector3`
   - Use Function Input: `Vertex Normal WS`
   - Description: "World space normal"

---

### Step 3: Build SSS Calculation Network

#### 3.1 Calculate View-Dependent Scattering

1. **Add Camera Vector Node**
   - This gives us the view direction

2. **Dot Product (Normal · View)**
   ```
   [WorldNormal] → Dot Product ← [Camera Vector]
                       ↓
                   [Fresnel Effect]
   ```

3. **One Minus Node**
   - Invert the dot product to get rim lighting effect
   ```
   [Dot Product] → One Minus → [Rim Factor]
   ```

#### 3.2 Calculate Thickness-Based Attenuation

1. **Add Power Node**
   ```
   [Rim Factor] → Power ← [Constant: 3.0]
                    ↓
              [Rim Intensity]
   ```

2. **Multiply by Thickness**
   ```
   [Rim Intensity] × [Thickness] → [Attenuated Rim]
   ```

#### 3.3 Calculate Scatter Color

1. **Desaturate Base Color**
   ```
   [BaseColor] → Desaturate (Luminance Factors: 0.3, 0.59, 0.11)
                      ↓
                 [Luminance]
   ```

2. **Add Reddish Tint (Blood Color)**
   ```
   [Constant3: (1.0, 0.3, 0.2)] × [ScatterIntensity] → [Blood Tint]
   ```

3. **Blend with Base Color**
   ```
   [BaseColor] + [Blood Tint] × [Attenuated Rim] → [Scatter Color]
   ```

#### 3.4 Calculate Distance-Based Falloff

1. **Add Sphere Mask Node**
   - Center: World Position
   - Radius: ScatterRadius
   - Hardness: 0.5
   ```
   [WorldPosition] → Sphere Mask ← [ScatterRadius]
                          ↓
                    [Distance Falloff]
   ```

2. **Modulate Scatter Color**
   ```
   [Scatter Color] × [Distance Falloff] → [Final SSS Color]
   ```

---

### Step 4: Add Output Nodes

1. **SubsurfaceColor Output**
   ```
   [Final SSS Color] → Function Output: SubsurfaceColor
   ```

2. **SubsurfaceOpacity Output**
   ```
   [Attenuated Rim] × [ScatterIntensity] → Function Output: SubsurfaceOpacity
   ```

---

## Complete Node Graph (Text Representation)

```
BaseColor ──────────┬─────────────────────────┐
                    │                         │
WorldNormal ────→ Dot ←──── Camera Vector    │
                    │                         │
                 One Minus                    │
                    │                         │
                  Power (3.0)                 │
                    │                         │
                    ├──→ × ←── Thickness      │
                    │         │               │
                    │    [Attenuated Rim]     │
                    │         │               │
Constant3 ─→ × ←── ScatterIntensity          │
(1,0.3,0.2)    │                             │
          [Blood Tint]                        │
               │                              │
               ├──→ × ←── [Attenuated Rim]    │
               │         │                    │
               │    [Tinted Scatter]          │
               │         │                    │
               └──→ + ←──┴────────────────────┘
                    │
              [Scatter Color]
                    │
WorldPosition ──→ Sphere Mask ←── ScatterRadius
                    │
              [Distance Falloff]
                    │
                    ├──→ × ←── [Scatter Color]
                    │         │
                    │   [Final SSS Color] ──→ OUTPUT: SubsurfaceColor
                    │
                    └──→ × ←── ScatterIntensity ──→ OUTPUT: SubsurfaceOpacity
```

---

## Usage in M_DTE_Skin Material

In the M_DTE_Skin master material:

```
[BaseColor Parameter] ──┬──→ Base Color Pin
                        │
                        └──→ MF_SubsurfaceScattering.BaseColor
                                    │
[Thickness Parameter] ──────────→ MF_SubsurfaceScattering.Thickness
                                    │
[SSS Intensity Parameter] ───────→ MF_SubsurfaceScattering.ScatterIntensity
                                    │
                              [SubsurfaceColor] ──→ Add ──→ Emissive Color
                                    │
                              [SubsurfaceOpacity] ──→ Opacity (if translucent)
```

---

## Testing and Validation

### Visual Tests

1. **Thin Skin Test**
   - Set Thickness = 0.1
   - Expected: Strong rim lighting, visible veins

2. **Thick Skin Test**
   - Set Thickness = 0.9
   - Expected: Subtle SSS, less translucency

3. **Intensity Test**
   - Vary ScatterIntensity from 0.0 to 1.0
   - Expected: Smooth transition from no SSS to full SSS

4. **Backlit Test**
   - Place strong light behind avatar
   - Expected: Ears and thin areas should glow

### Performance Test

- **Instruction Count:** Should be < 50 instructions
- **Check in Material Editor:** Statistics panel

---

## Optimization Notes

- Uses simple mathematical operations (no texture lookups)
- Sphere Mask can be expensive; consider replacing with simpler distance calculation
- For mobile, create simplified version without Sphere Mask

---

## Alternative: Use Unreal's Built-in SSS

For simpler implementation, you can use Unreal's built-in Subsurface Profile:

1. **Create Subsurface Profile Asset**
   - Content Browser → Right-click → `Materials & Textures → Subsurface Profile`
   - Name: `SSP_Skin`

2. **Configure Profile**
   - Scatter Radius: `(1.5, 0.8, 0.4)` (RGB channels)
   - Falloff Color: `(1.0, 0.8, 0.7)`

3. **In Material**
   - Set Shading Model: `Subsurface`
   - Subsurface Profile: `SSP_Skin`
   - Opacity: Controlled by thickness map

**Recommendation:** Use built-in SSS for production, custom function for artistic control

---

## Next Steps

After implementing this function:

1. Test in M_DTE_Skin material
2. Create MF_EmotionalBlush function
3. Create MF_GlitchEffect function
4. Optimize for performance

---

## Conclusion

This material function provides realistic subsurface scattering that enhances the skin material's realism. It's a critical component of the "super-hot-girl aesthetic" by creating soft, translucent skin that responds naturally to lighting.

**Estimated Implementation Time:** 1-2 hours

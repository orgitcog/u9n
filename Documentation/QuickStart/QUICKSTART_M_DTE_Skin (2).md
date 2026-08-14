# Quick Start: M_DTE_Skin Master Material

**Estimated Time:** 1-2 hours  
**Difficulty:** Intermediate  
**Priority:** CRITICAL - Create after ABP

---

## What You're Building

The M_DTE_Skin master material is the foundation of the avatar's visual appearance. It creates realistic, dynamic skin that responds to emotions, personality traits, and cognitive states. This material implements subsurface scattering, emotional blush, and the "super-hot-girl aesthetic."

---

## Prerequisites Checklist

Before you begin:

- [ ] Unreal Engine 5.3+ with project open
- [ ] ABP_DeepTreeEcho_Avatar created and working
- [ ] Basic understanding of Unreal Engine materials
- [ ] Skin texture (or use UE5 default textures as placeholder)

---

## Step-by-Step Implementation

### Part 1: Create the Master Material (10 minutes)

The first step is creating the master material that will serve as the template for all skin materials.

**Navigate to Content Browser** and create a new folder structure at `/Game/DeepTreeEcho/Materials/`. Right-click in this folder and select `Material` to create a new material asset. Name it `M_DTE_Skin` and double-click to open the Material Editor.

In the Material Editor, you'll see the main material output node on the right side of the graph. This node has several inputs that control different aspects of the material's appearance. The Details panel on the left shows the material's properties.

**Configure the material's basic settings** by selecting the main material output node and adjusting the Details panel. Set the **Material Domain** to `Surface` and the **Blend Mode** to `Opaque`. For the **Shading Model**, select `Subsurface Profile` to enable realistic skin rendering with subsurface scattering.

---

### Part 2: Add Base Color System (20 minutes)

The base color system defines the fundamental skin tone and allows for dynamic color adjustments based on emotional state.

**Create texture parameters** by right-clicking in the graph and searching for `Texture Sample Parameter 2D`. Create one and name it `BaseColorTexture`. In the Details panel, set the default texture to a skin texture (or a neutral beige color if you don't have one yet).

**Add color tinting** by creating a `Vector Parameter` node named `SkinTint`. Set the default value to a warm skin tone like `(1.0, 0.9, 0.8)`. Multiply this with the `BaseColorTexture` output using a `Multiply` node.

**Connect to Base Color** by dragging the multiply result to the `Base Color` input of the main material node.

---

### Part 3: Implement Subsurface Scattering (25 minutes)

Subsurface scattering simulates light penetrating and scattering beneath the skin surface, creating a realistic, lifelike appearance.

**Create subsurface color** by adding another `Vector Parameter` named `SubsurfaceColor`. Set it to a reddish tone like `(1.0, 0.3, 0.3)` to simulate blood beneath the skin.

**Add subsurface intensity control** with a `Scalar Parameter` named `SubsurfaceIntensity`. Set the default to `0.5`. This controls how much light penetrates the skin.

**Create the subsurface calculation** by multiplying `SubsurfaceColor` with `SubsurfaceIntensity` and connecting the result to the `Subsurface Color` input of the main material node.

**Configure the subsurface profile** by selecting the main material node and finding the `Subsurface Profile` property in the Details panel. Create a new Subsurface Profile asset (Content Browser → right-click → Materials & Textures → Subsurface Profile). Name it `SSP_Skin` and assign it here. Open the profile and adjust the **Scatter Radius** to `(1.2, 0.8, 0.6)` for realistic skin scattering.

---

### Part 4: Add Emotional Blush System (30 minutes)

The emotional blush system creates dynamic redness in the cheeks and other areas based on the avatar's emotional state.

**Create blush mask texture** by adding a `Texture Sample Parameter 2D` named `BlushMaskTexture`. This texture should be white in areas where blush appears (cheeks, nose, ears) and black elsewhere. If you don't have this texture yet, create a simple one in an image editor or use a constant value of `0.5` as a placeholder.

**Add blush intensity parameter** with a `Scalar Parameter` named `BlushIntensity`. Set the default to `0.0`. This will be controlled dynamically by the C++ code based on emotional state.

**Create blush color** using a `Vector Parameter` named `BlushColor`. Set it to a pink/red tone like `(1.0, 0.4, 0.4)`.

**Build the blush calculation** by multiplying `BlushMaskTexture`, `BlushIntensity`, and `BlushColor` together. Use a `Lerp` (Linear Interpolate) node to blend this blush result with the base color. Connect the base color to input A, the blush color to input B, and the blush intensity (multiplied by the mask) to the Alpha input.

**Update the Base Color connection** by replacing the previous Base Color connection with the output of this Lerp node.

---

### Part 5: Add Normal Map and Surface Detail (15 minutes)

Normal maps add surface detail like pores, wrinkles, and skin texture without adding geometry.

**Add normal map texture** by creating a `Texture Sample Parameter 2D` named `NormalMapTexture`. In the Details panel, change the `Sampler Type` to `Normal`. Connect the RGB output to the `Normal` input of the main material node.

**Add normal intensity control** by inserting a `FlattenNormal` node between the texture and the Normal input. Add a `Scalar Parameter` named `NormalIntensity` (default `1.0`) to control the strength of the normal map.

---

### Part 6: Configure Roughness and Specular (15 minutes)

Roughness and specular properties control how the skin reflects light, creating realistic highlights and shine.

**Create roughness texture** by adding a `Texture Sample Parameter 2D` named `RoughnessTexture`. If you don't have a roughness map, use a `Scalar Parameter` named `Roughness` with a default value of `0.4`. Skin is generally not very rough, so values between `0.3` and `0.5` work well.

**Add specular control** with a `Scalar Parameter` named `Specular`. Set the default to `0.5`. This controls the intensity of specular highlights.

**Connect to material outputs** by connecting the roughness value to the `Roughness` input and the specular value to the `Specular` input of the main material node.

---

### Part 7: Add Emotional Aura Glow (20 minutes)

The emotional aura creates a subtle glow effect that visualizes the avatar's emotional state.

**Create aura color parameter** with a `Vector Parameter` named `EmotionalAuraColor`. Set the default to `(0.0, 0.0, 0.0)` (no glow by default).

**Add aura intensity parameter** with a `Scalar Parameter` named `EmotionalAuraIntensity`. Set the default to `0.0`.

**Build the glow calculation** by multiplying `EmotionalAuraColor` with `EmotionalAuraIntensity`. Connect this to the `Emissive Color` input of the main material node.

This emissive glow will be controlled dynamically by the C++ `AvatarMaterialManager` to create visual feedback for emotional states.

---

### Part 8: Finalize and Test (10 minutes)

**Compile the material** by clicking the `Apply` button in the top toolbar. Fix any errors that appear (usually missing connections or incompatible node types).

**Create a material instance** by right-clicking `M_DTE_Skin` in the Content Browser and selecting `Create Material Instance`. Name it `MI_DTE_Skin_Default`. This instance will be applied to your avatar.

**Apply to avatar** by opening your avatar's skeletal mesh asset. In the Material Slots section, assign `MI_DTE_Skin_Default` to the skin material slot.

**Test in viewport** by placing the avatar in a level and observing the material under different lighting conditions. Adjust parameters in the material instance to fine-tune the appearance.

**Test dynamic parameters** by opening the material instance and adjusting `BlushIntensity` and `EmotionalAuraIntensity` to verify the effects work correctly.

---

## Parameter Reference

The following table summarizes all parameters that can be controlled dynamically by the C++ code:

| Parameter Name | Type | Default | Range | Purpose |
|----------------|------|---------|-------|---------|
| SkinTint | Vector3 | (1.0, 0.9, 0.8) | Any | Overall skin color tint |
| SubsurfaceIntensity | Scalar | 0.5 | 0.0 - 1.0 | Subsurface scattering strength |
| BlushIntensity | Scalar | 0.0 | 0.0 - 1.0 | Emotional blush intensity |
| BlushColor | Vector3 | (1.0, 0.4, 0.4) | Any | Color of emotional blush |
| NormalIntensity | Scalar | 1.0 | 0.0 - 2.0 | Normal map strength |
| Roughness | Scalar | 0.4 | 0.0 - 1.0 | Surface roughness |
| Specular | Scalar | 0.5 | 0.0 - 1.0 | Specular highlight intensity |
| EmotionalAuraColor | Vector3 | (0.0, 0.0, 0.0) | Any | Emissive glow color |
| EmotionalAuraIntensity | Scalar | 0.0 | 0.0 - 10.0 | Emissive glow intensity |

---

## Troubleshooting

**Problem: Material looks flat or unrealistic**  
Check that the Shading Model is set to `Subsurface Profile` and that a subsurface profile is assigned. Verify that the subsurface color and intensity are set correctly.

**Problem: Blush effect not visible**  
Ensure the blush mask texture is properly set up with white in the cheek areas. Increase `BlushIntensity` to test. Check that the Lerp node is correctly blending the colors.

**Problem: Material is too shiny or too matte**  
Adjust the `Roughness` parameter. Lower values create more shine, higher values create a more matte appearance. For skin, values between 0.3 and 0.5 typically work best.

**Problem: Normal map not showing detail**  
Verify the normal map texture's Sampler Type is set to `Normal`. Increase `NormalIntensity` to make the effect more pronounced.

---

## Next Steps

After completing this material:

1. **Create material instances** for different skin tones and looks
2. **Create M_DTE_Eye material** - The next critical asset
3. **Test with C++ integration** - Verify that `AvatarMaterialManager` can control parameters
4. **Add advanced features** - Implement glitch effects and additional details

---

## Success Criteria

You've successfully created M_DTE_Skin if:

- [ ] Material compiles without errors
- [ ] Skin looks realistic with subsurface scattering
- [ ] Blush effect is visible when intensity is increased
- [ ] Material responds to lighting realistically
- [ ] All parameters are exposed and adjustable in material instances

---

**Excellent work!** You've created the second critical asset. The avatar now has realistic, dynamic skin. Next, create the M_DTE_Eye material to bring the eyes to life!

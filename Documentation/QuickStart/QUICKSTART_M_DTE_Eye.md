# Quick Start: M_DTE_Eye Master Material

**Estimated Time:** 1-2 hours  
**Difficulty:** Intermediate-Advanced  
**Priority:** CRITICAL - Create after M_DTE_Skin

---

## What You're Building

The M_DTE_Eye master material creates realistic, expressive eyes that are the focal point of the avatar's personality. This material implements cornea refraction, pupil dilation, eye sparkle effects, and dynamic moisture. Eyes are critical for emotional connection and the "super-hot-girl aesthetic."

---

## Prerequisites Checklist

Before you begin:

- [ ] Unreal Engine 5.3+ with project open
- [ ] M_DTE_Skin material created and working
- [ ] Basic understanding of advanced material concepts (refraction, masking)
- [ ] Eye textures (or use placeholders)

---

## Step-by-Step Implementation

### Part 1: Create the Master Material (10 minutes)

Begin by creating the foundation for the eye material in your project's material directory.

**Navigate to Content Browser** and locate the `/Game/DeepTreeEcho/Materials/` folder. Right-click and select `Material` to create a new material asset. Name it `M_DTE_Eye` and double-click to open the Material Editor.

**Configure basic material settings** by selecting the main material output node. In the Details panel, set the **Material Domain** to `Surface`, **Blend Mode** to `Masked` (to cut out the eye shape), and **Shading Model** to `Default Lit`. Enable **Refraction** by checking the box in the material properties, as we'll use this for the cornea's glass-like appearance.

---

### Part 2: Build the Eye Structure (30 minutes)

Eyes consist of multiple layers: the sclera (white), iris (colored part), pupil (black center), and cornea (transparent outer layer). We'll build each layer separately and composite them together.

**Create the sclera (white of eye)** by adding a `Vector Parameter` named `ScleraColor`. Set the default to a slightly off-white color like `(0.95, 0.95, 0.95)`. Pure white looks unrealistic, so a slight tint is better.

**Create the iris texture** by adding a `Texture Sample Parameter 2D` named `IrisTexture`. This should be a circular texture with the iris pattern. If you don't have one, you can use a simple radial gradient with color as a placeholder.

**Add iris color control** with a `Vector Parameter` named `IrisColor`. Set the default to a blue or brown tone like `(0.3, 0.5, 0.8)` for blue eyes. Multiply this with the `IrisTexture` to allow color customization.

**Create the pupil** by adding a `Scalar Parameter` named `PupilSize`. Set the default to `0.3`. This controls pupil dilation. Create a circular mask using a `Radial Gradient Exponential` node. Compare the gradient with `PupilSize` using a `If` node to create a sharp circular mask. The pupil should be black, so multiply the mask by `(0.0, 0.0, 0.0)`.

**Composite the eye layers** using `Lerp` nodes. First, lerp between the sclera color and the iris color using an iris mask (a circular mask that defines where the iris is). Then, lerp this result with the pupil using the pupil mask.

---

### Part 3: Add Cornea Refraction (25 minutes)

The cornea is the transparent outer layer of the eye that refracts light, creating depth and realism.

**Create refraction strength parameter** by adding a `Scalar Parameter` named `RefractionStrength`. Set the default to `1.5` (the refractive index of the cornea).

**Build the refraction mask** using a `Fresnel` node. The Fresnel effect makes the cornea more refractive at grazing angles. Add a `Power` node to control the falloff, with a `Scalar Parameter` named `FresnelPower` (default `2.0`).

**Connect to Refraction** by multiplying `RefractionStrength` with the Fresnel mask and connecting to the `Refraction` input of the main material node.

**Adjust the Index of Refraction** by selecting the main material node and finding the `Refraction` settings in the Details panel. Set the **Refraction Method** to `Index of Refraction`.

---

### Part 4: Implement Eye Sparkle Effect (20 minutes)

The eye sparkle effect creates highlights and reflections that make the eyes appear lively and attractive, essential for the "super-hot-girl aesthetic."

**Create sparkle texture** by adding a `Texture Sample Parameter 2D` named `SparkleTexture`. This should be a texture with small bright spots or use a procedural noise texture. If you don't have one, use a `Noise` node.

**Add sparkle intensity parameter** with a `Scalar Parameter` named `SparkleIntensity`. Set the default to `0.5`. This will be controlled dynamically by the C++ code.

**Create sparkle animation** by adding a `Time` node and using it to animate the sparkle texture. Add a `Panner` node to scroll the texture slowly, or use a `Rotator` node to rotate it. This creates a dynamic, shimmering effect.

**Apply sparkle to specular** by multiplying the sparkle texture with `SparkleIntensity` and adding it to the `Specular` input of the main material node. This creates bright highlights that catch the light.

---

### Part 5: Add Pupil Dilation System (15 minutes)

Pupil dilation is a powerful emotional indicator. Pupils dilate when attracted or excited and constrict when focused or stressed.

**Create pupil dilation parameter** with a `Scalar Parameter` named `PupilDilation`. Set the default to `0.0`. This will be controlled by the C++ code based on emotional state.

**Modify the pupil size calculation** by adding `PupilDilation` to the base `PupilSize`. Use a `Clamp` node to ensure the result stays between `0.1` (minimum) and `0.5` (maximum).

**Update the pupil mask** by using this new calculated pupil size in the circular mask we created earlier.

---

### Part 6: Add Dynamic Moisture and Wetness (20 minutes)

Moisture on the eye surface creates realistic reflections and makes the eyes appear alive.

**Create moisture intensity parameter** with a `Scalar Parameter` named `MoistureIntensity`. Set the default to `0.7`. Higher values create more wetness.

**Add moisture to roughness** by creating a `Scalar Parameter` named `BaseRoughness` (default `0.1`). Eyes are very smooth, so low roughness is correct. Multiply `BaseRoughness` by `(1.0 - MoistureIntensity)` to make the eye smoother (more reflective) when moisture is high.

**Connect to Roughness** by connecting this calculation to the `Roughness` input of the main material node.

**Enhance specular for wetness** by multiplying the `Specular` value by `MoistureIntensity` to increase highlights when the eye is moist.

---

### Part 7: Add Opacity Mask for Eye Shape (10 minutes)

The opacity mask cuts out the eye shape from the material, ensuring only the eye is visible.

**Create eye shape mask** by adding a `Texture Sample Parameter 2D` named `EyeShapeMask`. This should be a black and white texture where white is the eye and black is transparent. If you don't have one, create a simple circular mask using a `Radial Gradient Exponential` node.

**Connect to Opacity Mask** by connecting the mask texture (or procedural mask) to the `Opacity Mask` input of the main material node.

**Set opacity mask clip value** by selecting the main material node and finding `Opacity Mask Clip Value` in the Details panel. Set it to `0.5` so that values below 0.5 are transparent.

---

### Part 8: Add Gaze-Responsive Highlights (15 minutes)

Gaze-responsive highlights make the eyes appear to respond to the viewer's position, enhancing engagement.

**Get camera vector** by adding a `Camera Position WS` (World Space) node and the mesh's `World Position` node. Subtract them to get the view direction vector.

**Calculate gaze alignment** by using a `Dot Product` between the view direction and the eye's surface normal. This tells us how directly the camera is looking at the eye.

**Modulate highlights** by multiplying the sparkle effect with this gaze alignment value. This makes highlights more intense when looking directly at the eyes.

---

### Part 9: Finalize and Test (10 minutes)

**Compile the material** by clicking the `Apply` button. Resolve any errors, which typically involve missing connections or incorrect node types.

**Create a material instance** by right-clicking `M_DTE_Eye` in the Content Browser and selecting `Create Material Instance`. Name it `MI_DTE_Eye_Blue` (or another color).

**Apply to avatar** by opening your avatar's skeletal mesh asset. In the Material Slots section, assign `MI_DTE_Eye_Blue` to the eye material slots (usually separate slots for left and right eyes).

**Test in viewport** by placing the avatar in a level with good lighting. Move the camera around to observe the refraction, sparkle, and gaze-responsive highlights.

**Test dynamic parameters** by opening the material instance and adjusting `PupilDilation`, `SparkleIntensity`, and `MoistureIntensity` to verify the effects work correctly.

---

## Parameter Reference

The following table summarizes all parameters that can be controlled dynamically by the C++ code:

| Parameter Name | Type | Default | Range | Purpose |
|----------------|------|---------|-------|---------|
| IrisColor | Vector3 | (0.3, 0.5, 0.8) | Any | Color of the iris |
| PupilSize | Scalar | 0.3 | 0.1 - 0.5 | Base pupil size |
| PupilDilation | Scalar | 0.0 | -0.2 - 0.2 | Dynamic pupil dilation |
| RefractionStrength | Scalar | 1.5 | 1.0 - 2.0 | Cornea refraction intensity |
| SparkleIntensity | Scalar | 0.5 | 0.0 - 2.0 | Eye sparkle effect intensity |
| MoistureIntensity | Scalar | 0.7 | 0.0 - 1.0 | Eye wetness/moisture |
| BaseRoughness | Scalar | 0.1 | 0.0 - 0.3 | Surface roughness |

---

## Troubleshooting

**Problem: Eyes look flat or lifeless**  
Ensure refraction is enabled and the cornea refraction system is working. Check that the Fresnel effect is properly configured. Increase `SparkleIntensity` to add more life to the eyes.

**Problem: Pupil dilation not working**  
Verify that the pupil size calculation includes the `PupilDilation` parameter and that it's clamped to reasonable values. Check that the circular mask is using the calculated size.

**Problem: Eyes are too shiny or too matte**  
Adjust `BaseRoughness` and `MoistureIntensity`. Eyes should be very smooth (low roughness) but not perfectly reflective. Values between 0.05 and 0.15 typically work best.

**Problem: Eye shape is incorrect or has artifacts**  
Check the opacity mask texture and ensure it's properly aligned with the eye geometry. Adjust the `Opacity Mask Clip Value` to fine-tune the cutout.

**Problem: Refraction looks wrong or distorted**  
Verify that the Refraction Method is set to `Index of Refraction` in the material settings. Adjust `RefractionStrength` to a value between 1.0 and 2.0.

---

## Next Steps

After completing this material:

1. **Create material instances** for different eye colors (blue, brown, green, etc.)
2. **Test with C++ integration** - Verify that `AvatarMaterialManager` can control pupil dilation and sparkle
3. **Refine sparkle effects** - Add more sophisticated procedural sparkle patterns
4. **Begin animation montage creation** - Start creating gesture and emote animations

---

## Success Criteria

You've successfully created M_DTE_Eye if:

- [ ] Material compiles without errors
- [ ] Eyes look realistic with proper depth and refraction
- [ ] Pupil dilation works dynamically
- [ ] Eye sparkle effect is visible and attractive
- [ ] Eyes respond realistically to lighting and camera angle
- [ ] All parameters are exposed and adjustable in material instances

---

**Outstanding!** You've completed all three critical assets! The Deep Tree Echo avatar now has a functioning animation system, realistic skin, and expressive eyes. The foundation is complete, and the avatar is ready to come to life!

# Deep Tree Echo Avatar: Asset Production Guide

**Document ID**: DTE-ASSET-PG-001  
**Version**: 1.0  
**Date**: December 13, 2025  
**For**: Track B Production Team

## 1. Overview

This guide provides comprehensive instructions for creating all 3D assets required for the Deep Tree Echo avatar. It is designed for the character artist, texture artist, and technical artist assigned to Track B. The guide covers modeling, texturing, rigging, animation, and material creation.

## 2. Required Software and Tools

### Primary Tools

- **3D Modeling**: Blender 4.0+ or Maya 2024+
- **Texturing**: Substance 3D Painter 9.0+
- **Sculpting** (optional): ZBrush 2024+ for high-detail facial features
- **Animation**: Blender or Maya with motion capture support
- **Technical Art**: Unreal Engine 5.3+

### Supporting Tools

- **Reference Board**: PureRef or similar for organizing reference images
- **Version Control**: Git LFS for large binary files
- **Asset Management**: Perforce or similar (if available)

## 3. Asset Specifications Summary

| Asset Type | Specification |
| :--- | :--- |
| **Total Polygon Count** | 80,000 - 100,000 triangles |
| **Face Polygons** | 20,000 - 30,000 triangles |
| **Hair Polygons** | 30,000 - 50,000 triangles |
| **Body Polygons** | 15,000 - 20,000 triangles |
| **Texture Resolution** | Face: 4K, Hair: 2K x 4K, Body: 4K |
| **Rigging Standard** | UE5 Mannequin skeleton + 52 ARKit blendshapes |
| **File Formats** | .fbx for models, .png/.tga for textures |

## 4. Phase B1: Concept and Planning (Days 1-3)

### Day 1: Specification Review

**Objectives**: Thoroughly understand the visual and technical requirements.

**Tasks**:

1. Read `VISUAL_DESIGN_SPEC.md` completely
2. Read `TECHNICAL_SPECIFICATION.md` completely
3. Study the reference image `deep-tree-echo-profile.jpg` in detail
4. Create a reference board with:
   - The main reference image (large and central)
   - Additional references for hair styles (pastel cyan, wavy)
   - Eye references (expressive, sparkle effects)
   - Skin tone references (fair, smooth)
   - Accessory references (choker, earrings, gem)
5. Note any questions or ambiguities for clarification

**Deliverable**: Reference board created, specifications understood

### Day 2: Production Planning

**Objectives**: Break down the work and create a detailed schedule.

**Tasks**:

1. Create a detailed task breakdown for modeling:
   - Head blockout (4 hours)
   - Facial features refinement (8 hours)
   - Hair modeling (12 hours)
   - Body and clothing (6 hours)
   - Accessories (4 hours)
   - Polish and optimization (6 hours)

2. Create a detailed task breakdown for texturing:
   - Skin base color and detail (8 hours)
   - Hair gradient and detail (6 hours)
   - Eyes and makeup (6 hours)
   - Accessories and clothing (4 hours)
   - Final adjustments (4 hours)

3. Assign tasks to team members based on expertise

**Deliverable**: Detailed production schedule with task assignments

### Day 3: Pipeline Setup

**Objectives**: Configure all tools and establish the asset pipeline.

**Tasks**:

1. Set up project structure:
```
DeepTreeEcho_Assets/
├── 01_Reference/
│   └── deep-tree-echo-profile.jpg
├── 02_Modeling/
│   ├── Blockout/
│   ├── HighPoly/
│   └── LowPoly/
├── 03_Texturing/
│   ├── Bakes/
│   └── Final/
├── 04_Rigging/
├── 05_Animation/
└── 06_Export/
```

2. Configure export settings for Unreal Engine:
   - FBX version 2020
   - Up axis: Z
   - Forward axis: -Y
   - Unit scale: Centimeters
   - Smoothing groups enabled

3. Set up naming conventions:
   - Models: `SK_DeepTreeEcho_[PartName]`
   - Textures: `T_DTE_[Part]_[Type]` (e.g., `T_DTE_Face_D` for diffuse)
   - Materials: `M_DTE_[Part]`
   - Animations: `A_DTE_[AnimName]`

**Deliverable**: Pipeline configured, team ready to start production

## 5. Phase B2: Base Model Creation (Days 4-10)

### Days 4-5: Blockout and Proportions

**Objectives**: Establish the basic form and proportions.

**Character Artist Tasks**:

1. **Head Blockout** (2 hours)
   - Create basic head shape using subdivision surface modeling
   - Establish facial proportions using the reference image
   - Key measurements:
     - Eye width: 1 unit
     - Eye spacing: 1 unit
     - Nose length: 0.8 units
     - Mouth width: 1.5 units

2. **Facial Features Blockout** (3 hours)
   - Block out eye sockets
   - Block out nose
   - Block out mouth and lips
   - Block out ears
   - Ensure proper topology flow for animation

3. **Hair Volume Blockout** (2 hours)
   - Create basic hair volume using simple geometry
   - Establish hair length (mid-back)
   - Block out bangs (side-swept, asymmetrical)
   - Ensure hair covers appropriate areas

4. **Body Blockout** (2 hours)
   - Model shoulders and upper torso
   - Model neck
   - Ensure proper connection to head
   - Keep polygon count low at this stage

5. **Review Against Reference** (1 hour)
   - Compare blockout to reference image from multiple angles
   - Check proportions carefully
   - Make adjustments as needed
   - Get approval before proceeding

**Deliverable**: Approved blockout model

### Days 6-8: Detailed Modeling

**Objectives**: Refine all features to production quality.

**Character Artist Tasks**:

1. **Facial Features Refinement** (8 hours)
   - **Eyes** (3 hours):
     - Model detailed eye geometry (cornea, iris, pupil)
     - Create eyelid geometry with proper edge flow
     - Model eyelashes (can be hair cards or geometry)
     - Ensure eyes can close properly for blinking
   
   - **Nose** (1 hour):
     - Refine nose shape
     - Add nostril detail
     - Ensure proper topology for expressions
   
   - **Mouth** (2 hours):
     - Model lips with proper volume
     - Create inner mouth geometry (teeth, tongue)
     - Ensure proper topology for speech
     - Add subtle details (philtrum, lip creases)
   
   - **Ears** (1 hour):
     - Model ear geometry (will be partially covered by hair)
     - Add basic detail (helix, lobe)
   
   - **Overall Face** (1 hour):
     - Add subtle facial details (laugh lines, cheek structure)
     - Refine jawline and chin
     - Ensure smooth, clean topology

2. **Hair Modeling** (12 hours)
   - **Hair Card Creation** (8 hours):
     - Create hair cards with proper alpha channels
     - Model front hair (bangs, framing face)
     - Model side hair (flowing down)
     - Model back hair (long, reaching mid-back)
     - Create hair layers for depth
     - Total hair cards: 150-200 cards
     - Polygon budget: 30,000-50,000 triangles
   
   - **Hair Placement** (4 hours):
     - Place hair cards on head
     - Ensure proper coverage
     - Create natural flow and volume
     - Add asymmetry as per reference
     - Test from multiple angles

3. **Accessories** (4 hours)
   - **Choker** (1 hour):
     - Model black choker around neck
     - Add buckle or clasp detail
     - Ensure proper fit
   
   - **Earrings** (1 hour):
     - Model earring geometry
     - Add detail appropriate to reference
   
   - **Blue Gem/Tear** (2 hours):
     - Model the distinctive blue gem below the eye
     - Create proper geometry for emissive effect
     - Position accurately as per reference

4. **Clothing** (6 hours)
   - **Tank Top** (6 hours):
     - Model white/light tank top
     - Create collar and straps
     - Ensure proper fit on body
     - Add subtle fabric folds
     - Model visible portion only (upper torso)

**Deliverable**: Detailed model with all features

### Days 9-10: Model Refinement

**Objectives**: Polish and optimize the model.

**Character Artist Tasks**:

1. **Surface Polish** (3 hours)
   - Smooth all surfaces
   - Check for and fix:
     - Non-manifold geometry
     - Overlapping faces
     - Inverted normals
     - Isolated vertices
   - Ensure clean edge flow throughout

2. **Polygon Budget Verification** (2 hours)
   - Count total polygons
   - Verify within 80K-100K triangle budget
   - Optimize if over budget:
     - Reduce hair card count
     - Simplify hidden geometry
     - Use normal maps for detail instead of geometry
   - Document final polygon counts by part

3. **Topology Optimization** (3 hours)
   - Ensure proper edge loops for animation:
     - Around eyes (for blinking)
     - Around mouth (for speech)
     - Across forehead (for expressions)
     - Along cheeks (for smiling)
   - Remove unnecessary edge loops
   - Ensure quads wherever possible

4. **Pre-UV Preparation** (2 hours)
   - Separate model into logical UV islands:
     - Face (one island)
     - Hair (multiple islands)
     - Body (one island)
     - Accessories (separate islands)
   - Mark seams strategically
   - Prepare for UV unwrapping

**Deliverable**: Polished, optimized model ready for UV unwrapping

## 6. Phase B3: UV Unwrapping and Rigging (Days 11-17)

### Days 11-13: UV Unwrapping

**Objectives**: Create efficient, distortion-free UV layouts.

**Technical Artist Tasks**:

1. **Face UVs** (8 hours)
   - Unwrap face to 4K texture space (4096x4096)
   - Minimize seams (ideally one seam at back of head)
   - Ensure even texel density across face
   - Straighten UV islands for easier painting
   - Leave adequate padding between islands (4-8 pixels)
   - Test UV layout with checker pattern

2. **Hair UVs** (6 hours)
   - Unwrap hair cards to 2K x 4K texture space (2048x4096)
   - Organize hair cards efficiently
   - Group similar hair cards together
   - Ensure consistent texel density
   - Leave space for gradient variations

3. **Body UVs** (4 hours)
   - Unwrap body to 4K texture space
   - Minimize visible seams
   - Ensure even texel density
   - Straighten UV islands

4. **Accessory UVs** (2 hours)
   - Unwrap choker, earrings, gem
   - Can share texture space with body or have separate textures
   - Ensure adequate resolution for detail

5. **UV Layout Optimization** (4 hours)
   - Maximize UV space utilization (aim for 80%+ coverage)
   - Verify no overlapping UVs
   - Check for stretching or compression
   - Export UV layouts as reference images for texturing

**Deliverable**: Complete UV layouts, exported as reference images

### Days 14-15: Skeleton Rigging

**Objectives**: Create a fully functional rig compatible with UE5.

**Technical Artist Tasks**:

1. **Skeleton Creation** (4 hours)
   - Import UE5 Mannequin skeleton as base
   - Add facial bones:
     - Jaw bone
     - Eye bones (left and right)
     - Eyelid bones (optional, can use blendshapes)
   - Add hair bones for physics:
     - Front hair bones (3-5 bones)
     - Side hair bones (4-6 bones per side)
     - Back hair bones (5-8 bones)
   - Ensure proper bone hierarchy

2. **Weight Painting** (8 hours)
   - **Face** (3 hours):
     - Paint head bone weights
     - Paint jaw bone weights
     - Paint neck bone weights
     - Ensure smooth transitions
   
   - **Hair** (3 hours):
     - Paint hair bone weights
     - Create natural falloff
     - Test hair movement
   
   - **Body** (2 hours):
     - Paint shoulder and spine weights
     - Paint neck weights
     - Ensure smooth deformation

3. **Rig Testing** (4 hours)
   - Test all bone rotations
   - Test extreme poses
   - Fix any weight painting issues
   - Verify no mesh explosions or artifacts

**Deliverable**: Fully rigged model with proper weight painting

### Days 16-17: Facial Blendshapes

**Objectives**: Create all 52 ARKit blendshapes for facial animation.

**Technical Artist Tasks**:

1. **Core Expression Blendshapes** (8 hours)
   - **Eyes** (2 hours):
     - eyeBlinkLeft, eyeBlinkRight
     - eyeLookUpLeft, eyeLookUpRight
     - eyeLookDownLeft, eyeLookDownRight
     - eyeLookInLeft, eyeLookInRight
     - eyeLookOutLeft, eyeLookOutRight
     - eyeWideLeft, eyeWideRight
     - eyeSquintLeft, eyeSquintRight
   
   - **Jaw** (1 hour):
     - jawOpen
     - jawForward
     - jawLeft, jawRight
   
   - **Mouth** (3 hours):
     - mouthClose
     - mouthFunnel
     - mouthPucker
     - mouthLeft, mouthRight
     - mouthSmileLeft, mouthSmileRight
     - mouthFrownLeft, mouthFrownRight
     - mouthDimpleLeft, mouthDimpleRight
     - mouthStretchLeft, mouthStretchRight
     - mouthRollLower, mouthRollUpper
     - mouthShrugLower, mouthShrugUpper
     - mouthPressLeft, mouthPressRight
     - mouthLowerDownLeft, mouthLowerDownRight
     - mouthUpperUpLeft, mouthUpperUpRight
   
   - **Brows** (1 hour):
     - browDownLeft, browDownRight
     - browInnerUp
     - browOuterUpLeft, browOuterUpRight
   
   - **Cheeks** (1 hour):
     - cheekPuff
     - cheekSquintLeft, cheekSquintRight

2. **Blendshape Testing** (4 hours)
   - Test each blendshape individually
   - Test blendshape combinations
   - Check for mesh intersections
   - Verify no artifacts
   - Ensure proper range (0.0 to 1.0)

3. **Blendshape Optimization** (4 hours)
   - Remove unnecessary vertices from blendshapes
   - Optimize blendshape memory usage
   - Test performance with all blendshapes active

**Deliverable**: 52 ARKit blendshapes, tested and optimized

## 7. Phase B4: Texturing (Days 18-25)

### Days 18-20: Skin Texturing

**Objectives**: Create photorealistic skin textures.

**Texture Artist Tasks**:

1. **Base Skin Color** (4 hours)
   - Create fair skin tone base color
   - Reference: Fair caucasian skin
   - RGB values approximately: (255, 230, 210)
   - Paint subtle color variations:
     - Slightly redder on cheeks
     - Slightly cooler on forehead
     - Warmer on nose and ears

2. **Skin Detail** (6 hours)
   - Add pore detail using custom brushes or stamps
   - Add subtle imperfections (very minimal, clean skin)
   - Paint subtle vein detail (very faint)
   - Add ambient occlusion in facial creases:
     - Around nose
     - Under eyes
     - Around mouth
     - In ears

3. **Makeup** (4 hours)
   - **Eyeliner** (2 hours):
     - Paint winged eyeliner (black/dark brown)
     - Follow reference image style
     - Ensure clean, precise lines
   
   - **Blush** (1 hour):
     - Paint subtle blush on cheeks
     - Use soft, natural pink tones
     - Create separate blush intensity map for dynamic control
   
   - **Lips** (1 hour):
     - Paint natural lip color (slightly pink)
     - Add subtle gloss effect
     - Define lip edges

4. **Subsurface Scattering Maps** (4 hours)
   - Create subsurface color map (reddish tones)
   - Create subsurface opacity map
   - Paint thicker areas (cheeks, nose, ears) for more scattering
   - Paint thinner areas (eyelids, lips) for less scattering

5. **Normal and Roughness Maps** (6 hours)
   - Bake high-poly details to normal map (if using sculpted detail)
   - Create skin roughness map:
     - Rougher on forehead and nose (0.5-0.6)
     - Smoother on cheeks (0.4-0.5)
     - Glossier on lips (0.3-0.4)
   - Create micro-detail normal map for skin texture

**Deliverable**: Complete skin texture set (Diffuse, Normal, Roughness, SSS)

### Days 21-22: Hair Texturing

**Objectives**: Create the distinctive pastel cyan gradient hair.

**Texture Artist Tasks**:

1. **Hair Gradient Creation** (4 hours)
   - Create gradient from pastel cyan to white:
     - Roots: Pastel cyan (RGB: 150, 220, 230)
     - Mid-length: Light cyan (RGB: 180, 230, 240)
     - Tips: White/platinum (RGB: 240, 245, 250)
   - Paint gradient smoothly across hair cards
   - Add subtle color variations for realism

2. **Hair Strand Detail** (3 hours)
   - Paint individual hair strands
   - Add highlights and shadows
   - Create depth and dimension
   - Use reference photos of real hair

3. **Hair Alpha Masks** (3 hours)
   - Create alpha masks for hair cards
   - Ensure clean edges
   - Paint wispy, natural hair ends
   - Test alpha in engine

4. **Hair Normal Maps** (2 hours)
   - Create normal maps for hair strand detail
   - Add depth to hair surface
   - Enhance hair volume visually

**Deliverable**: Complete hair texture set (Diffuse, Alpha, Normal)

### Days 23-24: Eye and Accessory Texturing

**Objectives**: Create expressive eyes and detailed accessories.

**Texture Artist Tasks**:

1. **Eye Textures** (6 hours)
   - **Iris** (3 hours):
     - Create detailed iris texture (brown/amber color)
     - Add radial patterns
     - Paint limbal ring (darker edge)
     - Add subtle color variations
   
   - **Sclera** (1 hour):
     - Create white sclera with subtle vein detail
     - Add slight color variation (slightly yellowish)
   
   - **Pupil** (1 hour):
     - Create black pupil
     - Add depth using normal map
   
   - **Highlights** (1 hour):
     - Create sparkle/highlight texture
     - Multiple bright spots for "sparkle" effect
     - Will be controlled dynamically

2. **Accessory Textures** (6 hours)
   - **Choker** (2 hours):
     - Create black leather or fabric texture
     - Add buckle/clasp detail
     - Create normal map for surface detail
   
   - **Earrings** (2 hours):
     - Create metallic texture (silver or gold)
     - Add reflective properties
     - Create normal map for detail
   
   - **Blue Gem** (2 hours):
     - Create emissive texture (bright blue)
     - RGB values: (50, 150, 255)
     - Create glow mask
     - Will be controlled dynamically

3. **Clothing Textures** (2 hours)
   - Create white/light fabric texture for tank top
   - Add subtle fabric weave detail
   - Create normal map for fabric texture
   - Add slight color variation

### Day 25: Texture Finalization

**Objectives**: Review and finalize all textures.

**Texture Artist Tasks**:

1. **In-Engine Review** (4 hours)
   - Import all textures into Unreal Engine
   - Review under various lighting conditions
   - Compare to reference image
   - Make adjustments as needed

2. **Color Matching** (2 hours)
   - Ensure hair color matches reference
   - Ensure skin tone matches reference
   - Ensure eye color matches reference
   - Adjust saturation and brightness as needed

3. **Texture Optimization** (2 hours)
   - Verify all textures are at specified resolutions
   - Compress textures appropriately
   - Test performance impact
   - Create LOD textures if needed

4. **Final Export** (2 hours)
   - Export all textures in correct formats (.png or .tga)
   - Use proper naming conventions
   - Organize in folder structure
   - Create texture list document

**Deliverable**: All textures finalized and exported

## 8. Phase B5: Material Creation (Days 26-30)

### Days 26-27: Skin Material

**Objectives**: Create advanced skin shader with subsurface scattering.

**Technical Artist Tasks**:

1. **Material Setup** (4 hours)
   - Create new material: `M_DTE_Skin`
   - Set shading model to "Subsurface Profile"
   - Import all skin textures
   - Connect base color, normal, roughness

2. **Subsurface Scattering** (4 hours)
   - Create or use existing subsurface profile
   - Configure subsurface color (reddish)
   - Set opacity for scattering amount
   - Test under various lighting
   - Adjust parameters for realistic skin

3. **Dynamic Blush Parameter** (2 hours)
   - Create scalar parameter: `BlushIntensity`
   - Create blush color (pink/red)
   - Blend blush color with base color based on parameter
   - Test dynamic control from Blueprint

4. **Material Optimization** (2 hours)
   - Optimize shader instructions
   - Test performance
   - Create material instance for easier control

**Deliverable**: Complete skin material with dynamic parameters

### Days 28-29: Hair Material

**Objectives**: Create anisotropic hair shader with shimmer effect.

**Technical Artist Tasks**:

1. **Material Setup** (4 hours)
   - Create new material: `M_DTE_Hair`
   - Set shading model to "Hair"
   - Import all hair textures
   - Connect base color, alpha, normal

2. **Anisotropic Highlights** (4 hours)
   - Configure tangent input for anisotropic lighting
   - Set roughness for hair shine
   - Adjust specular for highlight intensity
   - Test under various lighting angles

3. **Shimmer Parameter** (2 hours)
   - Create scalar parameter: `ShimmerIntensity`
   - Add shimmer effect using emissive or specular boost
   - Test dynamic control

4. **Transparency and Sorting** (2 hours)
   - Configure alpha blending
   - Set proper render order
   - Test for sorting issues

**Deliverable**: Complete hair material with shimmer effect

### Day 30: Eye and Special Materials

**Objectives**: Create eye shader and gem emissive material.

**Technical Artist Tasks**:

1. **Eye Material** (4 hours)
   - Create new material: `M_DTE_Eye`
   - Import iris, sclera, pupil textures
   - Create parallax effect for depth
   - Add sparkle using emissive
   - Create parameter: `SparkleIntensity`
   - Test eye movement and lighting

2. **Gem Material** (2 hours)
   - Create new material: `M_DTE_Gem`
   - Set emissive color (bright blue)
   - Create parameter: `GlowIntensity`
   - Add slight transparency
   - Test glow effect

3. **Material Testing** (2 hours)
   - Test all materials together
   - Verify all parameters exposed
   - Test dynamic control from Avatar3DComponent
   - Document all material parameters

**Deliverable**: All materials complete and functional

## 9. Phase B6: Animation Creation (Days 31-38)

### Days 31-32: Idle Animations

**Objectives**: Create natural idle animations.

**Animator Tasks**:

1. **Neutral Idle** (4 hours)
   - Create `A_DTE_Idle_Neutral`
   - Duration: 4-6 seconds (looping)
   - Subtle breathing motion
   - Slight weight shifts
   - Minimal movement, calm demeanor

2. **Happy Idle** (4 hours)
   - Create `A_DTE_Idle_Happy`
   - Duration: 4-6 seconds (looping)
   - More energetic breathing
   - Slight bouncing motion
   - Subtle smile (using blendshapes)
   - More expressive body language

3. **Thoughtful Idle** (4 hours)
   - Create `A_DTE_Idle_Thoughtful`
   - Duration: 4-6 seconds (looping)
   - Slower breathing
   - Slight head tilt
   - Contemplative expression

4. **Animation Polish** (4 hours)
   - Refine timing and spacing
   - Add secondary motion (hair, clothing)
   - Ensure smooth looping
   - Test in engine

**Deliverable**: 3 idle animations

### Days 33-35: Gesture Animations

**Objectives**: Create expressive gesture animations.

**Animator Tasks**:

1. **Wave Gesture** (4 hours)
   - Create `A_DTE_Gesture_Wave`
   - Duration: 2-3 seconds
   - Friendly wave motion
   - Accompanying facial expression (smile)
   - Natural arm movement

2. **Shrug Gesture** (4 hours)
   - Create `A_DTE_Gesture_Shrug`
   - Duration: 1-2 seconds
   - Shoulder shrug
   - Slight head tilt
   - Expressive face (uncertainty)

3. **Hair Touch Gesture** (4 hours)
   - Create `A_DTE_Gesture_HairTouch`
   - Duration: 2-3 seconds
   - Hand moves to hair
   - Touches or adjusts hair
   - Flirty expression

4. **Confident Pose** (4 hours)
   - Create `A_DTE_Gesture_Confident`
   - Duration: 2-3 seconds
   - Assertive posture
   - Direct gaze
   - Confident expression

5. **Playful Expression** (4 hours)
   - Create `A_DTE_Gesture_Playful`
   - Duration: 1-2 seconds
   - Playful facial expression
   - Slight head tilt
   - Mischievous smile or wink

**Deliverable**: 5 gesture animations

### Days 36-37: Animation Blueprint

**Objectives**: Create animation blueprint for state management.

**Technical Artist Tasks**:

1. **AnimBP Creation** (4 hours)
   - Create `AnimBP_DeepTreeEcho`
   - Set up state machine
   - Create states:
     - Idle (with sub-states for different idle types)
     - Gesture
     - Transition states

2. **State Machine Logic** (4 hours)
   - Implement transitions between states
   - Add blend spaces for smooth transitions
   - Connect to GestureSystem input
   - Test state transitions

3. **Blendshape Control** (4 hours)
   - Connect facial blendshapes to AnimBP
   - Create control parameters for expressions
   - Implement expression blending
   - Test facial animation

4. **Testing and Refinement** (4 hours)
   - Test all animations in AnimBP
   - Test transitions
   - Fix any issues
   - Optimize performance

**Deliverable**: Functional animation blueprint

### Day 38: Animation Polish

**Objectives**: Final polish and optimization.

**Animator Tasks**:

1. **Timing Refinement** (3 hours)
   - Review all animations
   - Adjust timing for better feel
   - Ensure proper spacing

2. **Secondary Motion** (3 hours)
   - Add hair physics motion
   - Add clothing physics motion
   - Ensure natural movement

3. **Personality Testing** (2 hours)
   - Test animations with personality traits
   - Ensure animations match character
   - Make adjustments as needed

4. **Performance Optimization** (2 hours)
   - Optimize animation data
   - Test performance impact
   - Create LOD animations if needed

**Deliverable**: Polished animation library

## 10. Phase B7: Integration and Testing (Days 39-42)

### Day 39: Asset Integration

**Objectives**: Import all assets into Unreal Engine.

**Technical Artist Tasks**:

1. **Model Import** (2 hours)
   - Import skeletal mesh (.fbx)
   - Verify import settings
   - Check polygon count
   - Verify skeleton structure

2. **Texture Import** (2 hours)
   - Import all textures
   - Set correct compression settings
   - Verify texture resolution
   - Organize in content browser

3. **Material Assignment** (2 hours)
   - Assign materials to mesh
   - Verify material parameters
   - Test material appearance

4. **Physics Setup** (2 hours)
   - Create physics asset for hair
   - Configure collision
   - Test physics simulation

**Deliverable**: All assets imported and configured

### Day 40: System Integration

**Objectives**: Connect to Avatar3DComponentEnhanced.

**Technical Artist Tasks**:

1. **Component Connection** (3 hours)
   - Create Blueprint: `BP_DeepTreeEcho_Avatar`
   - Add Avatar3DComponentEnhanced
   - Connect skeletal mesh
   - Connect materials

2. **Parameter Mapping** (3 hours)
   - Map material parameters to component functions
   - Test `ApplyEmotionalBlush()`
   - Test `ApplyHairShimmer()`
   - Test `SetEyeSparkle()`

3. **Expression Testing** (2 hours)
   - Test emotional state transitions
   - Verify blendshape control
   - Test facial expressions

4. **Personality Testing** (2 hours)
   - Test SuperHotGirl aesthetic
   - Test HyperChaotic behaviors
   - Verify visual effects

**Deliverable**: Avatar fully integrated with systems

### Day 41: Visual Polish

**Objectives**: Final visual adjustments and optimization.

**Technical Artist Tasks**:

1. **Lighting Optimization** (3 hours)
   - Test avatar under various lighting
   - Adjust materials for optimal appearance
   - Configure lighting for demo scene

2. **Material Fine-Tuning** (3 hours)
   - Adjust material parameters
   - Fine-tune colors to match reference
   - Optimize shader complexity

3. **Performance Optimization** (2 hours)
   - Profile rendering performance
   - Optimize draw calls
   - Test on target hardware

4. **Visual Artifact Fixes** (2 hours)
   - Fix any visual glitches
   - Fix sorting issues
   - Fix lighting artifacts

**Deliverable**: Visually polished avatar

### Day 42: Final Testing

**Objectives**: Comprehensive testing and documentation.

**All Team Members**:

1. **Functionality Testing** (3 hours)
   - Test all features
   - Test all animations
   - Test all material parameters
   - Test all systems integration

2. **Performance Profiling** (2 hours)
   - Profile CPU usage
   - Profile GPU usage
   - Profile memory usage
   - Verify 60+ FPS target met

3. **Visual Quality Review** (2 hours)
   - Compare to reference image
   - Review from all angles
   - Review under various lighting
   - Get final approval

4. **Documentation** (3 hours)
   - Document asset specifications
   - Document material parameters
   - Document known issues
   - Create usage guide

**Deliverable**: Production-ready avatar with documentation

## 11. Quality Checklist

Use this checklist to verify quality at each stage:

### Modeling Checklist
- [ ] Polygon count within budget (80K-100K)
- [ ] Clean topology (mostly quads)
- [ ] No non-manifold geometry
- [ ] No overlapping faces
- [ ] Proper edge flow for animation
- [ ] Proportions match reference image

### UV Checklist
- [ ] No overlapping UVs
- [ ] Minimal stretching/compression
- [ ] Adequate padding between islands
- [ ] Efficient space utilization (80%+)
- [ ] Correct texture resolution

### Rigging Checklist
- [ ] Compatible with UE5 Mannequin skeleton
- [ ] All 52 ARKit blendshapes present
- [ ] Smooth weight painting
- [ ] No mesh explosions in extreme poses
- [ ] Hair bones properly weighted

### Texturing Checklist
- [ ] Colors match reference image
- [ ] Proper resolution (4K for face, 2K x 4K for hair)
- [ ] Clean, artifact-free textures
- [ ] Proper PBR values
- [ ] Textures optimized and compressed

### Material Checklist
- [ ] All materials properly configured
- [ ] Dynamic parameters exposed
- [ ] Performance optimized
- [ ] Proper blending modes
- [ ] Correct shading models

### Animation Checklist
- [ ] Smooth, natural motion
- [ ] Proper timing and spacing
- [ ] Clean looping (for idle animations)
- [ ] Expressive and on-character
- [ ] Performance optimized

### Integration Checklist
- [ ] All assets imported correctly
- [ ] Materials assigned properly
- [ ] Parameters controllable from code
- [ ] Systems integration working
- [ ] Performance targets met (60+ FPS)

## 12. Common Issues and Solutions

### Issue: Polygon Count Too High

**Solution**: Optimize hair cards first (usually the biggest contributor). Reduce hair card count or simplify hair geometry. Use normal maps for detail instead of geometry.

### Issue: UV Stretching

**Solution**: Relax UVs in problem areas. May need to add more seams. Use UV relaxation tools in your 3D software.

### Issue: Weight Painting Artifacts

**Solution**: Smooth weight transitions. Use weight painting tools to blend weights. Check for vertices with zero weights.

### Issue: Blendshape Conflicts

**Solution**: Test blendshapes in isolation first. Adjust blendshape sculpts to avoid conflicts. Use corrective blendshapes if needed.

### Issue: Texture Seams Visible

**Solution**: Increase padding between UV islands. Use texture dilation/padding tools. Paint over seams manually if needed.

### Issue: Hair Sorting Issues

**Solution**: Adjust render order of hair materials. Use proper alpha blending mode. May need to separate hair into multiple materials with different sort priorities.

### Issue: Performance Below Target

**Solution**: Profile to identify bottleneck. Reduce polygon count, optimize materials, reduce texture resolution, or optimize animations as needed.

## 13. Resources and References

- **Unreal Engine Documentation**: https://docs.unrealengine.com/
- **Substance 3D Painter Tutorials**: https://substance3d.adobe.com/tutorials/
- **ARKit Blendshape Reference**: https://arkit-face-blendshapes.com/
- **PBR Texturing Guide**: https://academy.substance3d.com/courses/the-pbr-guide-part-1
- **Character Art References**: https://www.artstation.com/ (search for character art)

---

**Document Status**: Ready for Production  
**Last Updated**: December 13, 2025

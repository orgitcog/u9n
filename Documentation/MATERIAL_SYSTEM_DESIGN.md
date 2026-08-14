# Deep Tree Echo Material System Design

## Overview

The Deep Tree Echo material system provides production-ready, physically-based materials for the 3D avatar with integrated support for super-hot-girl aesthetic and hyper-chaotic behavior visualization.

## Core Materials

### 1. M_DTE_Skin (Master Skin Material)

**Purpose:** Realistic, attractive skin rendering with subsurface scattering and dynamic emotional responses.

**Features:**
- Physically-based subsurface scattering
- Dynamic blush system (emotional response)
- Smoothness and translucency control
- Normal map support for skin detail
- Specular highlights for realistic shine
- Emotional aura overlay
- Glitch effect integration

**Parameters:**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| BaseColor | Color | (1.0, 0.9, 0.85) | Skin tone |
| Smoothness | Float | 0.6 | Surface smoothness |
| Subsurface | Float | 0.8 | SSS intensity |
| Translucency | Float | 0.3 | Light penetration |
| BlushIntensity | Float | 0.0 | Dynamic blush |
| BlushColor | Color | (1.0, 0.5, 0.5) | Blush tint |
| AuraIntensity | Float | 0.0 | Emotional aura |
| AuraColor | Color | (0.5, 0.8, 1.0) | Aura color |
| GlitchIntensity | Float | 0.0 | Chaos effect |

### 2. M_DTE_Hair (Master Hair Material)

**Purpose:** Realistic hair rendering with shimmer effects and dynamic movement response.

**Features:**
- Anisotropic specular highlights
- Dynamic shimmer/sparkle effect
- Wind response (via vertex animation)
- Color variation support
- Transparency for fine hair strands
- Chaos-driven color shift
- Echo resonance glow

**Parameters:**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| BaseColor | Color | (0.2, 0.1, 0.05) | Hair color |
| HighlightColor | Color | (0.6, 0.4, 0.2) | Specular tint |
| Anisotropy | Float | 0.7 | Specular shape |
| ShimmerIntensity | Float | 0.5 | Sparkle effect |
| ShimmerSpeed | Float | 2.0 | Animation speed |
| Opacity | Float | 1.0 | Transparency |
| ChaosColorShift | Float | 0.0 | Hue variation |
| EchoGlow | Float | 0.0 | Resonance glow |

### 3. M_DTE_Eye (Master Eye Material)

**Purpose:** Highly detailed, expressive eye rendering with sparkle and gaze effects.

**Features:**
- Cornea refraction layer
- Iris detail with radial patterns
- Pupil dilation (emotional response)
- Eye sparkle/highlight system
- Sclera (white) with subtle veins
- Tear duct moisture
- Emotional color tinting
- Gaze direction influence on highlights

**Parameters:**
| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| IrisColor | Color | (0.3, 0.6, 0.9) | Eye color |
| PupilDilation | Float | 0.5 | Pupil size |
| SparkleIntensity | Float | 0.8 | Eye sparkle |
| SparklePosition | Vector2D | (0.3, 0.3) | Highlight pos |
| Moisture | Float | 0.6 | Wetness |
| EmotionTint | Color | (1.0, 1.0, 1.0) | Color overlay |
| ScleraColor | Color | (0.95, 0.95, 0.98) | White color |
| Refraction | Float | 1.4 | Cornea IOR |

## Material Functions

### MF_SubsurfaceScattering

Implements realistic subsurface scattering for skin.

**Inputs:**
- BaseColor (Color)
- Thickness (Float)
- ScatterIntensity (Float)

**Outputs:**
- SubsurfaceColor (Color)

### MF_AnisotropicSpecular

Generates anisotropic highlights for hair.

**Inputs:**
- Tangent (Vector)
- Anisotropy (Float)
- Roughness (Float)

**Outputs:**
- SpecularColor (Color)

### MF_EyeRefraction

Simulates cornea refraction for realistic eyes.

**Inputs:**
- IrisTexture (Texture)
- IOR (Float)
- ViewDirection (Vector)

**Outputs:**
- RefractedColor (Color)

### MF_EmotionalBlush

Generates dynamic blush based on emotional state.

**Inputs:**
- EmotionIntensity (Float)
- BlushColor (Color)
- BlushMask (Texture)

**Outputs:**
- BlushColor (Color)

### MF_GlitchEffect

Creates visual glitch artifacts for chaotic behavior.

**Inputs:**
- GlitchIntensity (Float)
- Time (Float)
- UVs (Vector2D)

**Outputs:**
- DistortedUVs (Vector2D)
- GlitchColor (Color)

### MF_EchoResonance

Visualizes cognitive echo resonance.

**Inputs:**
- ResonanceIntensity (Float)
- ResonanceColor (Color)
- Time (Float)

**Outputs:**
- EmissiveColor (Color)

## Post-Processing Materials

### PP_DeepTreeEcho_Aura

Creates emotional aura around avatar.

**Features:**
- Depth-based edge detection
- Color-coded emotional states
- Pulsing animation
- Bloom integration

### PP_DeepTreeEcho_Glitch

Applies screen-space glitch effects.

**Features:**
- RGB channel separation
- Scanline artifacts
- Digital noise
- Temporal distortion

### PP_CognitiveLoad_Visualization

Visualizes cognitive processing load.

**Features:**
- Heat map overlay
- Particle trails
- Intensity-based color gradient
- Pulsing frequency modulation

## Material Instances

Each master material has multiple instances for different use cases:

### Skin Instances:
- MI_DTE_Skin_Default
- MI_DTE_Skin_Pale
- MI_DTE_Skin_Tan
- MI_DTE_Skin_Dark

### Hair Instances:
- MI_DTE_Hair_Black
- MI_DTE_Hair_Brown
- MI_DTE_Hair_Blonde
- MI_DTE_Hair_Red
- MI_DTE_Hair_Fantasy (for unusual colors)

### Eye Instances:
- MI_DTE_Eye_Blue
- MI_DTE_Eye_Green
- MI_DTE_Eye_Brown
- MI_DTE_Eye_Hazel
- MI_DTE_Eye_Fantasy (for unusual colors)

## Integration with Avatar3DComponent

The Avatar3DComponent controls material parameters through Dynamic Material Instances (DMI):

```cpp
// Example: Setting blush intensity
void UAvatar3DComponentEnhanced::ApplyEmotionalBlush(float Intensity)
{
    for (UMaterialInstanceDynamic* DMI : DynamicMaterials)
    {
        if (DMI && DMI->GetBaseMaterial()->GetName().Contains("Skin"))
        {
            DMI->SetScalarParameterValue(TEXT("BlushIntensity"), Intensity);
        }
    }
}
```

## Performance Considerations

- All materials use shader complexity LOD
- Expensive effects (SSS, refraction) have quality scalability
- Mobile-friendly fallback materials available
- Parameter updates batched per frame
- Material instances share compiled shaders

## Next Steps

1. Implement master materials in Unreal Engine Material Editor
2. Create material functions for reusable logic
3. Generate material instances for variations
4. Implement post-processing materials
5. Integrate with Avatar3DComponent DMI system
6. Performance profiling and optimization
7. Create material documentation for artists

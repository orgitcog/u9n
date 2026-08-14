# Deep Tree Echo Post-Processing Materials - Complete Specifications

**Asset Type:** Post Process Materials  
**Purpose:** Screen-space effects for cognitive and emotional visualization  
**Integration:** Controlled by Avatar3DComponent and applied to camera

---

## Overview

Post-processing materials create screen-space effects that visualize the avatar's cognitive and emotional state in a way that affects the entire viewing experience. These effects make the avatar's internal state feel immersive and impactful.

---

## Post-Process Material 1: PP_EmotionalAura

**Purpose:** Create colored aura/glow around avatar based on emotional state

### Visual Description
- Soft glow emanating from avatar silhouette
- Color matches emotional state
- Intensity matches emotional intensity
- Creates "emotional atmosphere" effect

---

### Implementation Specifications

#### Material Settings
- **Material Domain:** Post Process
- **Blendable Location:** After Tonemapping
- **Blend Mode:** Additive

#### Input Parameters

| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| AuraColor | Vector3 | (0.5, 0.8, 1.0) | Color of the aura |
| AuraIntensity | Scalar | 0.5 | Intensity of the effect |
| AuraRadius | Scalar | 50.0 | Radius of the glow in pixels |
| AuraSharpness | Scalar | 2.0 | Sharpness of the glow falloff |

#### Node Graph

```
DEPTH-BASED MASK:
Scene Depth → Custom Depth → Subtract → [Depth Difference]
                                            ↓
                                      Smoothstep (0, AuraRadius)
                                            ↓
                                      [Aura Mask]

BLUR AURA:
[Aura Mask] → Gaussian Blur (Radius: AuraRadius)
                  ↓
            [Blurred Mask]
                  ↓
            Power ← AuraSharpness
                  ↓
            [Sharp Aura]

APPLY COLOR:
[Sharp Aura] × AuraColor × AuraIntensity → [Aura Color]

COMPOSITE:
Scene Color + [Aura Color] → Final Color
```

#### Detailed Implementation

**Step 1: Create Depth Mask**
```
Custom Depth (avatar mesh) → [Avatar Depth]
Scene Depth → [Scene Depth]

[Scene Depth] - [Avatar Depth] → [Depth Diff]

If [Depth Diff] < 0:  // Avatar is in front
    [Depth Diff] → Abs → Smoothstep (0, AuraRadius) → [Mask]
Else:
    0 → [Mask]
```

**Step 2: Blur and Shape**
```
[Mask] → Custom Node: Gaussian Blur
    Blur Radius: AuraRadius / 10.0
    Samples: 16
    → [Blurred Mask]

[Blurred Mask] → Power ← AuraSharpness
    → [Shaped Aura]
```

**Step 3: Apply Color and Composite**
```
[Shaped Aura] × AuraColor × AuraIntensity → [Final Aura]

Scene Color + [Final Aura] → Emissive Color Pin
```

#### Dynamic Control (C++)
```cpp
// In Avatar3DComponent
void UpdateEmotionalAuraPostProcess(EAvatarEmotionalState Emotion, float Intensity)
{
    FLinearColor AuraColor = GetEmotionColor(Emotion);
    
    if (PostProcessComponent)
    {
        PostProcessComponent->Settings.WeightedBlendables.Array[0].Weight = Intensity;
        
        UMaterialInstanceDynamic* AuraMID = Cast<UMaterialInstanceDynamic>(
            PostProcessComponent->Settings.WeightedBlendables.Array[0].Object
        );
        
        if (AuraMID)
        {
            AuraMID->SetVectorParameterValue("AuraColor", AuraColor);
            AuraMID->SetScalarParameterValue("AuraIntensity", Intensity);
        }
    }
}
```

---

## Post-Process Material 2: PP_CognitiveLoadHeatMap

**Purpose:** Visualize cognitive load as heat map overlay

### Visual Description
- Heat map overlay on screen
- Color gradient from cool (low load) to hot (high load)
- Pulsing effect at high loads
- Vignette-style effect (stronger at edges)

---

### Implementation Specifications

#### Material Settings
- **Material Domain:** Post Process
- **Blendable Location:** Before Tonemapping
- **Blend Mode:** Alpha Composite

#### Input Parameters

| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| CognitiveLoad | Scalar | 0.0 | Current cognitive load (0-1) |
| HeatIntensity | Scalar | 0.3 | Intensity of the heat effect |
| PulseSpeed | Scalar | 2.0 | Speed of pulsing effect |
| VignetteStrength | Scalar | 0.5 | Strength of vignette |

#### Node Graph

```
HEAT COLOR GRADIENT:
CognitiveLoad → Lerp between colors:
    0.0: Cool blue (0.3, 0.5, 1.0)
    0.5: Yellow (1.0, 0.8, 0.3)
    1.0: Hot red (1.0, 0.3, 0.1)
    → [Heat Color]

PULSING EFFECT:
Time × PulseSpeed → Sine → × 0.2 + 0.8 → [Pulse Factor]
CognitiveLoad × [Pulse Factor] → [Pulsing Load]

VIGNETTE MASK:
Screen Position → Distance from center
    → Power (VignetteStrength)
    → [Vignette Mask]

COMBINE:
[Heat Color] × [Pulsing Load] × [Vignette Mask] × HeatIntensity
    → [Heat Overlay]

COMPOSITE:
Scene Color × (1 - [Heat Overlay].A) + [Heat Overlay]
    → Final Color
```

#### Detailed Implementation

**Step 1: Create Heat Color Gradient**
```
// Multi-point gradient
CognitiveLoad → If < 0.5:
    Lerp(CoolBlue, Yellow, CognitiveLoad * 2.0)
Else:
    Lerp(Yellow, HotRed, (CognitiveLoad - 0.5) * 2.0)
    → [Heat Color]
```

**Step 2: Add Pulsing**
```
Time → Multiply ← PulseSpeed
    → Sine
    → Multiply ← 0.2
    → Add ← 0.8
    → [Pulse Factor]

CognitiveLoad × [Pulse Factor] → [Pulsing Load]
```

**Step 3: Create Vignette**
```
Screen Position → Subtract ← (0.5, 0.5)
    → Length
    → Multiply ← 2.0
    → Power ← VignetteStrength
    → [Vignette]
```

**Step 4: Composite**
```
[Heat Color] × [Pulsing Load] × [Vignette] × HeatIntensity
    → [Heat Overlay]

Scene Color → Lerp ← [Heat Overlay]
    Alpha: [Pulsing Load] × HeatIntensity
    → Emissive Color Pin
```

---

## Post-Process Material 3: PP_GlitchEffect

**Purpose:** Screen-space glitch effect for chaos visualization

### Visual Description
- Digital glitch artifacts across entire screen
- Chromatic aberration
- Scanlines and digital noise
- Block displacement
- Triggered by high chaos factor

---

### Implementation Specifications

#### Material Settings
- **Material Domain:** Post Process
- **Blendable Location:** Before Tonemapping
- **Blend Mode:** Opaque

#### Input Parameters

| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| GlitchIntensity | Scalar | 0.0 | Intensity of glitch effect (0-1) |
| ChromaticAberration | Scalar | 0.01 | Amount of color separation |
| ScanlineIntensity | Scalar | 0.3 | Intensity of scanlines |
| BlockDisplacement | Scalar | 0.05 | Amount of block displacement |

#### Node Graph

```
CHROMATIC ABERRATION:
Screen UVs + (ChromaticAberration, 0) → Sample Red channel
Screen UVs → Sample Green channel
Screen UVs - (ChromaticAberration, 0) → Sample Blue channel
Combine RGB → [Aberrated Color]

BLOCK DISPLACEMENT:
Screen UVs → Floor ← (20, 20)  // 20x20 grid
    → Random per block
    → × BlockDisplacement × GlitchIntensity
    → [Displacement]

Screen UVs + [Displacement] → Sample Scene Color
    → [Displaced Color]

SCANLINES:
Screen UVs.Y × 200 → Frac → Step ← 0.5
    → × ScanlineIntensity
    → [Scanline Mask]

COMBINE:
Lerp([Aberrated Color], [Displaced Color], GlitchIntensity)
    → × (1 - [Scanline Mask])
    → Final Color
```

#### Detailed Implementation

**Step 1: Chromatic Aberration**
```
// Red channel offset right
Screen UVs + (ChromaticAberration × GlitchIntensity, 0)
    → Texture Sample (Scene Color)
    → Component Mask (R)
    → [Red Channel]

// Green channel no offset
Screen UVs → Texture Sample (Scene Color)
    → Component Mask (G)
    → [Green Channel]

// Blue channel offset left
Screen UVs - (ChromaticAberration × GlitchIntensity, 0)
    → Texture Sample (Scene Color)
    → Component Mask (B)
    → [Blue Channel]

Append([Red], [Green], [Blue]) → [Aberrated Color]
```

**Step 2: Block Displacement**
```
// Create block grid
Screen UVs → Multiply ← (20, 20)
    → Floor
    → Divide ← (20, 20)
    → [Block UVs]

// Random displacement per block
[Block UVs] → Noise (Cell)
    → Multiply ← BlockDisplacement
    → Multiply ← GlitchIntensity
    → [Block Offset]

Screen UVs + [Block Offset] → Sample Scene Color
    → [Displaced Color]
```

**Step 3: Scanlines**
```
Screen UVs.Y → Multiply ← 200.0  // 200 scanlines
    → Frac
    → Step ← 0.5
    → Multiply ← ScanlineIntensity
    → Multiply ← GlitchIntensity
    → [Scanline Darkening]
```

**Step 4: Combine All Effects**
```
Lerp([Aberrated Color], [Displaced Color], GlitchIntensity * 0.5)
    → [Glitched Color]

[Glitched Color] × (1 - [Scanline Darkening])
    → Emissive Color Pin
```

---

## Post-Process Material 4: PP_EchoResonanceDistortion

**Purpose:** Distort space around avatar during echo resonance

### Visual Description
- Ripple/wave distortion emanating from avatar
- Frequency matches echo intensity
- Creates "reality bending" effect
- Subtle refraction of background

---

### Implementation Specifications

#### Material Settings
- **Material Domain:** Post Process
- **Blendable Location:** Before Tonemapping
- **Blend Mode:** Opaque

#### Input Parameters

| Parameter Name | Type | Default Value | Description |
|----------------|------|---------------|-------------|
| EchoIntensity | Scalar | 0.0 | Intensity of echo effect (0-1) |
| WaveFrequency | Scalar | 2.0 | Frequency of waves |
| DistortionStrength | Scalar | 0.02 | Strength of distortion |
| AvatarScreenPosition | Vector2 | (0.5, 0.5) | Avatar position on screen |

#### Node Graph

```
DISTANCE FROM AVATAR:
Screen Position - AvatarScreenPosition → Length
    → [Distance]

WAVE PATTERN:
[Distance] × WaveFrequency - Time
    → Sine
    → × DistortionStrength × EchoIntensity
    → [Wave Strength]

DISTORTION DIRECTION:
Screen Position - AvatarScreenPosition → Normalize
    → [Direction]

DISTORTION OFFSET:
[Direction] × [Wave Strength] → [Offset]

SAMPLE DISTORTED:
Screen UVs + [Offset] → Sample Scene Color
    → Emissive Color Pin
```

#### Detailed Implementation

**Step 1: Calculate Distance from Avatar**
```
Screen Position → Subtract ← AvatarScreenPosition
    → Length
    → [Distance from Avatar]
```

**Step 2: Create Wave Pattern**
```
[Distance from Avatar] × WaveFrequency
    → Subtract ← Time
    → Sine
    → Multiply ← DistortionStrength
    → Multiply ← EchoIntensity
    → [Wave Amplitude]
```

**Step 3: Calculate Distortion Direction**
```
Screen Position - AvatarScreenPosition
    → Normalize
    → [Radial Direction]
```

**Step 4: Apply Distortion**
```
[Radial Direction] × [Wave Amplitude]
    → [UV Offset]

Screen UVs + [UV Offset]
    → Sample Scene Color
    → Emissive Color Pin
```

---

## Integration with Avatar3DComponent

### Setup Post-Process Volume
```cpp
// In Avatar3DComponent.h
UPROPERTY(EditAnywhere, Category = "Post Process")
UMaterialInterface* EmotionalAuraMaterial;

UPROPERTY(EditAnywhere, Category = "Post Process")
UMaterialInterface* CognitiveLoadMaterial;

UPROPERTY(EditAnywhere, Category = "Post Process")
UMaterialInterface* GlitchEffectMaterial;

UPROPERTY(EditAnywhere, Category = "Post Process")
UMaterialInterface* EchoDistortionMaterial;

UPROPERTY()
UPostProcessComponent* PostProcessComponent;

// In Avatar3DComponent.cpp
void UAvatar3DComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // Create post-process component
    PostProcessComponent = NewObject<UPostProcessComponent>(GetOwner());
    PostProcessComponent->RegisterComponent();
    PostProcessComponent->AttachToComponent(GetOwner()->GetRootComponent(), 
                                           FAttachmentTransformRules::KeepRelativeTransform);
    
    // Add post-process materials
    PostProcessComponent->Settings.WeightedBlendables.Array.Add(
        FWeightedBlendable(1.0f, EmotionalAuraMaterial)
    );
    PostProcessComponent->Settings.WeightedBlendables.Array.Add(
        FWeightedBlendable(1.0f, CognitiveLoadMaterial)
    );
    PostProcessComponent->Settings.WeightedBlendables.Array.Add(
        FWeightedBlendable(1.0f, GlitchEffectMaterial)
    );
    PostProcessComponent->Settings.WeightedBlendables.Array.Add(
        FWeightedBlendable(1.0f, EchoDistortionMaterial)
    );
    
    // Create dynamic material instances
    EmotionalAuraMID = UMaterialInstanceDynamic::Create(EmotionalAuraMaterial, this);
    CognitiveLoadMID = UMaterialInstanceDynamic::Create(CognitiveLoadMaterial, this);
    GlitchEffectMID = UMaterialInstanceDynamic::Create(GlitchEffectMaterial, this);
    EchoDistortionMID = UMaterialInstanceDynamic::Create(EchoDistortionMaterial, this);
}
```

### Update Post-Process Effects
```cpp
void UAvatar3DComponent::UpdatePostProcessEffects(float DeltaTime)
{
    // Update emotional aura
    if (EmotionalAuraMID)
    {
        FLinearColor AuraColor = GetCurrentEmotionColor();
        EmotionalAuraMID->SetVectorParameterValue("AuraColor", AuraColor);
        EmotionalAuraMID->SetScalarParameterValue("AuraIntensity", EmotionalIntensity);
    }
    
    // Update cognitive load
    if (CognitiveLoadMID)
    {
        CognitiveLoadMID->SetScalarParameterValue("CognitiveLoad", CurrentCognitiveLoad);
    }
    
    // Update glitch effect
    if (GlitchEffectMID)
    {
        GlitchEffectMID->SetScalarParameterValue("GlitchIntensity", ChaosFactor);
    }
    
    // Update echo distortion
    if (EchoDistortionMID)
    {
        EchoDistortionMID->SetScalarParameterValue("EchoIntensity", EchoResonance);
        
        // Calculate avatar screen position
        FVector2D ScreenPos = GetAvatarScreenPosition();
        EchoDistortionMID->SetVectorParameterValue("AvatarScreenPosition", 
                                                   FLinearColor(ScreenPos.X, ScreenPos.Y, 0, 0));
    }
}

FVector2D UAvatar3DComponent::GetAvatarScreenPosition()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return FVector2D(0.5f, 0.5f);
    
    FVector WorldLocation = GetOwner()->GetActorLocation();
    FVector2D ScreenLocation;
    
    if (PC->ProjectWorldLocationToScreen(WorldLocation, ScreenLocation))
    {
        // Normalize to 0-1 range
        FVector2D ViewportSize;
        PC->GetViewportSize(ViewportSize.X, ViewportSize.Y);
        return ScreenLocation / ViewportSize;
    }
    
    return FVector2D(0.5f, 0.5f);
}
```

---

## Performance Considerations

### Target Performance
- **PC High-End:** All 4 post-process materials active
- **PC Mid-Range:** 2-3 materials (disable echo distortion)
- **Mobile:** 1 material (emotional aura only, simplified)

### Optimization Tips
1. Use lower resolution for post-process (0.5x or 0.75x)
2. Reduce sample counts in blur operations
3. Disable effects when avatar is off-screen
4. Use LOD system based on distance

---

## Testing Procedures

### Visual Tests
1. **Emotional Aura:** Test all emotion colors, verify glow appears
2. **Cognitive Load:** Vary from 0 to 1, check color gradient and pulse
3. **Glitch Effect:** Test at different intensities, verify all components work
4. **Echo Distortion:** Trigger echo, verify ripple emanates from avatar

### Performance Tests
1. Measure FPS with all effects active
2. Check GPU usage in profiler
3. Test on target hardware

### Integration Tests
1. Verify C++ parameters update correctly
2. Test emotional state transitions
3. Test cognitive load changes
4. Verify avatar screen position tracking

---

## Conclusion

These post-processing materials create an immersive visual language that makes the avatar's internal state tangible and impactful. They transform the viewing experience, making the audience feel the avatar's emotions and cognitive processes. Combined with the particle systems and material systems, they complete the comprehensive visualization of the Deep Tree Echo avatar's consciousness.

**Estimated Implementation Time:** 1 week (all 4 materials)

**Complexity:** Medium-High (post-process expertise required)

**Impact:** Very High (immersive experience)

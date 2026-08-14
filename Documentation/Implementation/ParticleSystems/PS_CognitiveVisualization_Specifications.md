# Cognitive Visualization Particle Systems - Complete Specifications

**Asset Type:** Niagara Particle Systems  
**Purpose:** Visualize cognitive state, memory, and echo resonance  
**Integration:** Spawned and controlled by Avatar3DComponent

---

## Overview

These particle systems provide real-time visualization of the avatar's internal cognitive processes. They make abstract concepts like memory formation, cognitive load, and echo resonance tangible and visually engaging.

---

## Particle System 1: PS_MemoryNode

**Purpose:** Visualize memory constellation nodes floating around avatar's head

### Visual Description
- Small, glowing orbs that orbit around the avatar's head
- Each orb represents a memory or thought
- Orbs connect with thin lines to show associations
- Color indicates memory type or emotional valence
- Intensity indicates memory strength/importance

---

### Implementation Specifications

#### Emitter Settings
- **Emitter Type:** GPU Sprites
- **Max Particles:** 50
- **Spawn Rate:** 2-5 particles per second (controlled by cognitive load)
- **Lifetime:** 10-30 seconds (varies by memory importance)

#### Spawn Location
```
Spawn Module: Sphere Location
- Radius: 30-50 cm around head bone
- Surface Only: false
- Velocity Inheritance: 0.2
```

#### Movement Behavior
```
Orbit Module:
- Orbit Center: Head bone location
- Orbit Radius: 30-50 cm
- Orbit Speed: 15-45 degrees/second (varies)
- Orbit Axis: Mostly vertical, slight wobble

Noise Module:
- Noise Strength: 5 cm
- Frequency: 0.5 Hz
- Creates organic floating motion
```

#### Particle Appearance
```
Sprite Renderer:
- Size: 2-5 cm (varies by importance)
- Material: M_MemoryOrb (emissive sphere)
- Color: 
  - Happy memories: Warm (yellow-orange)
  - Sad memories: Cool (blue-purple)
  - Neutral: White-cyan
  - Active thoughts: Bright white with pulse

Emissive Intensity: 2.0-5.0
```

#### Color Over Life
```
Color Curve:
- 0.0s: Alpha 0 (fade in)
- 0.5s: Alpha 1 (full opacity)
- Lifetime-2s: Alpha 1 (sustain)
- Lifetime: Alpha 0 (fade out)

Brightness Pulse:
- Sine wave with 2-second period
- Amplitude: 20% of base brightness
```

#### Connection Lines
```
Beam Emitter:
- Source: Memory orb A
- Target: Memory orb B (nearest 2-3 orbs)
- Material: M_MemoryConnection (thin glowing line)
- Width: 0.2-0.5 cm
- Color: Same as source orb, 50% opacity
- Only draw if distance < 40 cm
```

#### Dynamic Control (C++)
```cpp
// In Avatar3DComponent
void SpawnMemoryNode(FMemoryData Memory)
{
    FNiagaraUserRedirect SpawnParams;
    SpawnParams.SetFloat("MemoryImportance", Memory.Importance);
    SpawnParams.SetVector("MemoryColor", GetMemoryColor(Memory.Type));
    SpawnParams.SetFloat("OrbitSpeed", Memory.Importance * 30.0f);
    
    MemoryParticleSystem->SetUserParametersDirectly(SpawnParams);
}

// Update spawn rate based on cognitive load
void UpdateCognitiveVisualization(float CognitiveLoad)
{
    float SpawnRate = FMath::Lerp(2.0f, 5.0f, CognitiveLoad);
    MemoryParticleSystem->SetFloatParameter("SpawnRate", SpawnRate);
}
```

---

## Particle System 2: PS_EchoResonance

**Purpose:** Visualize echo resonance waves emanating from avatar

### Visual Description
- Concentric rings/waves that pulse outward from avatar
- Frequency increases with echo intensity
- Color shifts based on emotional state
- Creates ripple effect in space around avatar

---

### Implementation Specifications

#### Emitter Settings
- **Emitter Type:** GPU Sprites (ring-shaped)
- **Max Particles:** 20
- **Spawn Rate:** 0.5-2.0 per second (controlled by echo intensity)
- **Lifetime:** 3-5 seconds

#### Spawn Location
```
Spawn Module: Point Location
- Location: Avatar chest/heart position
- Offset: (0, 0, 0)
```

#### Movement Behavior
```
Velocity Module:
- Initial Velocity: Radial outward
- Speed: 50-150 cm/second (faster with higher echo)
- Direction: Horizontal plane (XY)

Scale Over Life:
- Start: 10 cm radius
- End: 200 cm radius
- Curve: Linear growth
```

#### Particle Appearance
```
Sprite Renderer:
- Size: Ring shape (using ring texture)
- Material: M_EchoRing (emissive ring with soft edges)
- Color:
  - Neutral: Cyan-blue
  - Happy: Warm yellow-orange
  - Sad: Cool blue-purple
  - Angry: Red-orange
  - Excited: Bright white-cyan

Emissive Intensity: 1.0-3.0
```

#### Alpha Over Life
```
Alpha Curve:
- 0.0s: Alpha 0.8 (start visible)
- 1.0s: Alpha 0.8 (sustain)
- 3.0s: Alpha 0.2 (fade)
- Lifetime: Alpha 0 (fully faded)
```

#### Distortion Effect (Optional)
```
Mesh Distortion:
- Distort space around ring
- Strength: 5-10 cm
- Creates "ripple in reality" effect
```

#### Dynamic Control (C++)
```cpp
// In Avatar3DComponent
void TriggerEchoResonance(float Intensity, FLinearColor EmotionColor)
{
    FNiagaraUserRedirect SpawnParams;
    SpawnParams.SetFloat("EchoIntensity", Intensity);
    SpawnParams.SetVector("EchoColor", EmotionColor);
    SpawnParams.SetFloat("SpawnRate", Intensity * 2.0f);
    SpawnParams.SetFloat("RingSpeed", 50.0f + Intensity * 100.0f);
    
    EchoParticleSystem->SetUserParametersDirectly(SpawnParams);
}
```

---

## Particle System 3: PS_CognitiveLoad

**Purpose:** Visualize cognitive load as heat/energy around head

### Visual Description
- Heat shimmer/distortion effect around head
- Intensity increases with cognitive load
- Color shifts from cool (low load) to hot (high load)
- Creates "brain on fire" effect at high loads

---

### Implementation Specifications

#### Emitter Settings
- **Emitter Type:** GPU Sprites (many small particles)
- **Max Particles:** 200
- **Spawn Rate:** 20-100 per second (controlled by cognitive load)
- **Lifetime:** 1-2 seconds

#### Spawn Location
```
Spawn Module: Sphere Location
- Radius: 15-25 cm around head bone
- Surface Only: true
- Velocity: Outward from head
```

#### Movement Behavior
```
Velocity Module:
- Initial Velocity: Radial outward + upward
- Speed: 10-30 cm/second
- Drag: 0.5 (slows down over time)

Noise Module:
- Turbulence: High
- Frequency: 2.0 Hz
- Creates chaotic, heat-like motion
```

#### Particle Appearance
```
Sprite Renderer:
- Size: 1-3 cm
- Material: M_HeatParticle (soft, glowing)
- Color Gradient (by cognitive load):
  - 0.0 (low): Cool blue (0.3, 0.5, 1.0)
  - 0.5 (medium): Yellow (1.0, 0.8, 0.3)
  - 1.0 (high): Hot red (1.0, 0.3, 0.1)

Emissive Intensity: 0.5-2.0
```

#### Alpha Over Life
```
Alpha Curve:
- 0.0s: Alpha 0 (fade in)
- 0.2s: Alpha 0.6 (peak)
- Lifetime: Alpha 0 (fade out)
```

#### Distortion (Heat Shimmer)
```
Refraction:
- Distortion Strength: 0.1-0.3 (based on cognitive load)
- Creates heat shimmer effect
- Requires post-process material support
```

#### Dynamic Control (C++)
```cpp
// In Avatar3DComponent
void UpdateCognitiveLoadVisualization(float CognitiveLoad)
{
    FLinearColor LoadColor = FMath::Lerp(
        FLinearColor(0.3f, 0.5f, 1.0f),  // Cool blue
        FLinearColor(1.0f, 0.3f, 0.1f),  // Hot red
        CognitiveLoad
    );
    
    FNiagaraUserRedirect Params;
    Params.SetFloat("SpawnRate", 20.0f + CognitiveLoad * 80.0f);
    Params.SetVector("LoadColor", LoadColor);
    Params.SetFloat("ParticleSpeed", 10.0f + CognitiveLoad * 20.0f);
    
    CognitiveLoadParticleSystem->SetUserParametersDirectly(Params);
}
```

---

## Particle System 4: PS_EmotionalAura

**Purpose:** Visualize emotional state as colored aura around avatar

### Visual Description
- Soft, glowing particles that form an aura
- Color matches emotional state
- Intensity matches emotional intensity
- Pulses with emotional changes

---

### Implementation Specifications

#### Emitter Settings
- **Emitter Type:** GPU Sprites
- **Max Particles:** 100
- **Spawn Rate:** 10-30 per second
- **Lifetime:** 3-5 seconds

#### Spawn Location
```
Spawn Module: Skeletal Mesh Location
- Mesh: Avatar body mesh
- Surface Sampling: Uniform
- Offset: 5-15 cm from surface (normal direction)
```

#### Movement Behavior
```
Velocity Module:
- Initial Velocity: Outward from spawn point
- Speed: 5-15 cm/second
- Drag: 0.8

Orbit Module:
- Orbit around avatar center
- Speed: 10 degrees/second
- Creates swirling effect
```

#### Particle Appearance
```
Sprite Renderer:
- Size: 3-8 cm
- Material: M_AuraParticle (soft, glowing sphere)
- Color (by emotion):
  - Happy: Warm yellow (1.0, 0.9, 0.5)
  - Sad: Cool blue (0.4, 0.5, 0.8)
  - Angry: Red (1.0, 0.3, 0.2)
  - Excited: Bright cyan (0.5, 1.0, 1.0)
  - Flirty: Pink (1.0, 0.6, 0.8)
  - Neutral: White (0.9, 0.9, 1.0)

Emissive Intensity: 1.0-3.0
```

#### Alpha Over Life
```
Alpha Curve:
- 0.0s: Alpha 0
- 1.0s: Alpha 0.4
- Lifetime-1s: Alpha 0.4
- Lifetime: Alpha 0
```

#### Pulsing Effect
```
Size Over Life:
- Base size × (1.0 + 0.2 × sin(Time × 2π))
- Creates gentle pulsing
```

#### Dynamic Control (C++)
```cpp
// In Avatar3DComponent
void UpdateEmotionalAura(EAvatarEmotionalState Emotion, float Intensity)
{
    FLinearColor AuraColor = GetEmotionColor(Emotion);
    
    FNiagaraUserRedirect Params;
    Params.SetVector("AuraColor", AuraColor);
    Params.SetFloat("SpawnRate", 10.0f + Intensity * 20.0f);
    Params.SetFloat("EmissiveIntensity", 1.0f + Intensity * 2.0f);
    Params.SetFloat("ParticleSize", 3.0f + Intensity * 5.0f);
    
    EmotionalAuraParticleSystem->SetUserParametersDirectly(Params);
}
```

---

## Material Specifications

### M_MemoryOrb
```
Material Type: Unlit (Emissive only)
Blend Mode: Additive

Base Color: (0, 0, 0)
Emissive Color: Parameter "OrbColor" × Parameter "Intensity"
Opacity: Radial gradient (center bright, edges soft)

Node Graph:
Radial Gradient → Power (2.0) → × OrbColor → Emissive Color
```

### M_MemoryConnection
```
Material Type: Unlit
Blend Mode: Additive

Base Color: (0, 0, 0)
Emissive Color: Parameter "LineColor" × 0.5
Opacity: 0.5

Thin line material for beam emitter
```

### M_EchoRing
```
Material Type: Unlit
Blend Mode: Additive

Base Color: (0, 0, 0)
Emissive Color: Parameter "RingColor" × Parameter "Intensity"
Opacity: Ring gradient (bright ring, transparent center/edges)

Node Graph:
Distance from center → Smoothstep (0.45, 0.55) → Ring mask
Ring mask × RingColor → Emissive Color
```

### M_HeatParticle
```
Material Type: Unlit
Blend Mode: Additive

Base Color: (0, 0, 0)
Emissive Color: Parameter "HeatColor" × Parameter "Intensity"
Opacity: Radial gradient with noise

Refraction: 0.1-0.3 (for heat shimmer)
```

### M_AuraParticle
```
Material Type: Unlit
Blend Mode: Additive

Base Color: (0, 0, 0)
Emissive Color: Parameter "AuraColor" × Parameter "Intensity"
Opacity: Soft radial gradient

Node Graph:
Radial Gradient → Power (3.0) → Opacity
AuraColor × Intensity → Emissive Color
```

---

## Integration with Avatar3DComponent

### Spawn and Control
```cpp
// In Avatar3DComponent.h
UPROPERTY(EditAnywhere, Category = "Particles")
UNiagaraSystem* MemoryNodeSystem;

UPROPERTY(EditAnywhere, Category = "Particles")
UNiagaraSystem* EchoResonanceSystem;

UPROPERTY(EditAnywhere, Category = "Particles")
UNiagaraSystem* CognitiveLoadSystem;

UPROPERTY(EditAnywhere, Category = "Particles")
UNiagaraSystem* EmotionalAuraSystem;

UPROPERTY()
UNiagaraComponent* MemoryNodeComponent;

UPROPERTY()
UNiagaraComponent* EchoResonanceComponent;

UPROPERTY()
UNiagaraComponent* CognitiveLoadComponent;

UPROPERTY()
UNiagaraComponent* EmotionalAuraComponent;

// In Avatar3DComponent.cpp
void UAvatar3DComponent::BeginPlay()
{
    Super::BeginPlay();
    
    // Spawn particle components
    MemoryNodeComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
        MemoryNodeSystem,
        GetOwner()->GetRootComponent(),
        NAME_None,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        EAttachLocation::KeepRelativeOffset,
        true
    );
    
    // Repeat for other systems...
}

void UAvatar3DComponent::UpdateCognitiveVisualization(float DeltaTime)
{
    // Update all particle systems based on current state
    UpdateMemoryNodes();
    UpdateEchoResonance();
    UpdateCognitiveLoad();
    UpdateEmotionalAura();
}
```

---

## Performance Considerations

### Target Performance
- **PC High-End:** All 4 systems active, 370 max particles
- **PC Mid-Range:** 3 systems (no memory nodes), 250 max particles
- **Mobile:** 1 system (emotional aura only), 50 max particles

### LOD System
```cpp
void UAvatar3DComponent::UpdateParticleLOD(float DistanceToCamera)
{
    if (DistanceToCamera < 500.0f)  // Close
    {
        // All systems active
        MemoryNodeComponent->SetActive(true);
        EchoResonanceComponent->SetActive(true);
        CognitiveLoadComponent->SetActive(true);
        EmotionalAuraComponent->SetActive(true);
    }
    else if (DistanceToCamera < 1000.0f)  // Medium
    {
        // Reduce particle counts
        MemoryNodeComponent->SetActive(false);
        // Others active but reduced spawn rates
    }
    else  // Far
    {
        // Only emotional aura
        MemoryNodeComponent->SetActive(false);
        EchoResonanceComponent->SetActive(false);
        CognitiveLoadComponent->SetActive(false);
    }
}
```

---

## Testing Procedures

### Visual Tests
1. **Memory Nodes:** Spawn 10-20 nodes, verify orbiting and connections
2. **Echo Resonance:** Trigger at different intensities, check ring expansion
3. **Cognitive Load:** Vary from 0.0 to 1.0, verify color gradient
4. **Emotional Aura:** Test all emotion types, verify colors

### Performance Tests
1. All systems active, measure FPS
2. Verify particle counts don't exceed max
3. Test LOD transitions

### Integration Tests
1. Verify C++ control parameters work
2. Test emotional state changes
3. Test cognitive load changes
4. Verify attachment to avatar bones

---

## Conclusion

These particle systems transform abstract cognitive processes into tangible, beautiful visualizations. They make the avatar's "mind" visible, creating a unique aesthetic that blends technology and consciousness. Combined with the material systems, they complete the visual language of the Deep Tree Echo avatar.

**Estimated Implementation Time:** 1-2 weeks (all 4 systems)

**Complexity:** High (Niagara system expertise required)

**Impact:** Very High (unique visual signature)

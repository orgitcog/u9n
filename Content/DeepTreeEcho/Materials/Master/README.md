# Master Materials

## Critical Assets

### 1. M_DTE_Skin (Priority 1)
**Guide**: `/Documentation/Implementation/M_DTE_Skin_Implementation.md`  
**Quick Start**: `/Documentation/QuickStart/QUICKSTART_M_DTE_Skin.md`

**Features**:
- Subsurface scattering (UE5 Subsurface Profile)
- Dynamic emotional blush
- Emotional aura (emissive)
- Glitch effect (UV distortion)
- PBR base (color, roughness, normal)

**Dependencies** (create in Functions/ first):
- `MF_SubsurfaceScattering`
- `MF_EmotionalBlush`
- `MF_GlitchEffect`

**Dynamic Parameters** (exposed to C++):
- `BlushIntensity` (0-1)
- `BlushColor` (Color)
- `AuraIntensity` (0-1)
- `AuraColor` (Color)
- `GlitchIntensity` (0-1)
- `Smoothness` (0-1)
- `SubsurfaceAmount` (0-1)

### 2. M_DTE_Eye (Priority 2)
**Guide**: `/Documentation/Implementation/M_DTE_Eye_Implementation.md`  
**Quick Start**: `/Documentation/QuickStart/QUICKSTART_M_DTE_Eye.md`

**Features**:
- Layered structure (sclera, iris, cornea)
- Cornea refraction (IOR 1.4)
- Dynamic pupil dilation
- Eye sparkle system
- Moisture/wetness control

**Dependencies**:
- `MF_EyeRefraction`

**Dynamic Parameters**:
- `PupilDilation` (0-1)
- `SparkleIntensity` (0-1)
- `SparklePosition` (Vector2D)
- `Moisture` (0-1)
- `IrisColor` (Color)

### 3. M_DTE_Hair (Priority 3)
**Guide**: `/Documentation/Implementation/M_DTE_Hair_Implementation.md`

**Features**:
- Anisotropic specular (realistic sheen)
- Dual highlight system
- Hair shimmer effect
- Chaos-driven color shift
- Alpha cutout support

**Dependencies**:
- `MF_AnisotropicSpecular`

**Dynamic Parameters**:
- `HairAnisotropy` (0-1)
- `ShimmerIntensity` (0-1)
- `ShimmerSpeed` (0-5)
- `ChaosColorShift` (0-1)
- `PrimaryHighlight` (Color)
- `SecondaryHighlight` (Color)

## Implementation Order

1. **Week 1, Day 3**: Create material functions in `/Materials/Functions/`
2. **Week 1, Day 4**: Create `M_DTE_Skin`
3. **Week 1, Day 5**: Create `M_DTE_Eye`
4. **Week 3, Day 1**: Create `M_DTE_Hair`

## Testing Integration

All materials integrate with C++ `UAvatarMaterialManager`:
```cpp
// Example usage
MaterialManager->SetScalarParameter(EAvatarMaterialSlot::Skin, "BlushIntensity", 0.8f);
MaterialManager->SetVectorParameter(EAvatarMaterialSlot::Eyes, "SparklePosition", FVector(0.3f, 0.3f, 0.0f));
```

## Material Instances

Create instances in `/Materials/Instances/` for variations:
- `MI_DTE_Skin_Default`
- `MI_DTE_Skin_Pale`
- `MI_DTE_Skin_Tan`
- `MI_DTE_Skin_Dark`
- `MI_DTE_Eye_Blue`
- `MI_DTE_Eye_Green`
- `MI_DTE_Eye_Brown`

## Textures

Required textures (create or source):
- `T_Skin_Diffuse` (4K recommended)
- `T_Skin_Normal`
- `T_Skin_Roughness`
- `T_Skin_BlushMask` (grayscale, cheek areas)
- `T_Iris_Diffuse` (radial detail)
- `T_Sclera_Diffuse` (subtle veins)
- `T_Hair_Diffuse`
- `T_Hair_Normal`
- `T_Hair_Alpha` (strand cutout)

## Success Criteria

### M_DTE_Skin
- Blush appears when emotional state is "happy" or "flirty"
- Aura glows with cognitive resonance
- Glitch effect visible during high chaos
- Skin looks realistic with SSS

### M_DTE_Eye
- Eyes sparkle at dynamic positions
- Pupils dilate based on emotional state
- Eyes look wet and alive
- Refraction creates depth

### M_DTE_Hair
- Hair has realistic specular highlights
- Shimmer effect is visible
- Chaos causes color shift
- Alpha cutout is clean

## See Also
- Material Functions: `/Content/DeepTreeEcho/Materials/Functions/`
- Textures: `/Content/DeepTreeEcho/Materials/Textures/`
- C++ Integration: `/UnrealEcho/Avatar/AvatarMaterialManager.h`

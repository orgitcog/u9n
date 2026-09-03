# Deep Tree Echo - Content Organization

This directory contains all Unreal Engine content assets for the Deep Tree Echo Avatar system.

## Directory Structure

### Materials/
Contains all material assets for avatar rendering

- **Master/** - Master materials (M_DTE_Skin, M_DTE_Eye, M_DTE_Hair)
- **Functions/** - Reusable material functions (MF_*)
- **Instances/** - Material instances for variations (MI_*)
- **Textures/** - All texture assets (T_*)

### Animations/
Contains all animation assets and blueprints

- **Blueprints/** - Animation blueprints (ABP_*)
- **Montages/** - Animation montages (AM_*)
- **Sequences/** - Animation sequences (AS_*)

### Audio/
Contains all audio assets

- **Voice/** - Voice recordings or TTS integration
- **Gestures/** - Sound effects for gestures
- **Emotional/** - Emotional state sound effects
- **Music/** - Music themes for emotional states

### Particles/
Contains particle system assets

- **Niagara/** - Niagara particle systems (NS_*)

### PostProcessing/
Contains post-processing materials

- Post-processing materials (PP_*)

### Blueprints/
Contains blueprint actors and components

- Blueprint actors (BP_*)

## Asset Naming Conventions

### Prefixes
- **M_** - Material
- **MI_** - Material Instance
- **MF_** - Material Function
- **T_** - Texture
- **ABP_** - Animation Blueprint
- **AM_** - Animation Montage
- **AS_** - Animation Sequence
- **NS_** - Niagara System
- **PP_** - Post Process Material
- **BP_** - Blueprint
- **SFX_** - Sound Effect
- **MUS_** - Music

### Naming Format
`[Prefix]_[System]_[AssetName]_[Variant]`

Examples:
- `M_DTE_Skin` - Deep Tree Echo Skin Master Material
- `MI_DTE_Skin_Pale` - Pale skin variant
- `AM_Emote_Happy_Smile` - Happy smile animation montage
- `NS_MemoryNode` - Memory node particle system

## Implementation Order

### Phase 1: Critical Assets (Week 1)
1. Animation Blueprint: `ABP_DeepTreeEcho_Avatar`
2. Material Functions: `MF_SubsurfaceScattering`, `MF_EmotionalBlush`, `MF_GlitchEffect`
3. Master Materials: `M_DTE_Skin`, `M_DTE_Eye`

### Phase 2: Animation Library (Week 2)
1. Idle gestures (8 montages)
2. Emotional emotes (11 montages)
3. Communicative gestures (10 montages)

### Phase 3: Advanced Assets (Week 3-4)
1. Hair material: `M_DTE_Hair` + `MF_AnisotropicSpecular`
2. Particle systems (4 Niagara systems)
3. Post-processing materials (4 materials)
4. Audio assets (voice, SFX, music)

## Documentation References

- **Master Implementation Roadmap**: `/MASTER_IMPLEMENTATION_ROADMAP.md`
- **Next Steps Guide**: `/NEXT_STEPS.md`
- **Implementation Guides**: `/Documentation/Implementation/`
- **Quick Start Guides**: `/Documentation/QuickStart/`

## Getting Started

1. Open `UnrealEngineCog.uproject` in Unreal Engine 5.3
2. Follow the quick start guide: `/Documentation/QuickStart/QUICKSTART_ABP_DeepTreeEcho_Avatar.md`
3. Create assets according to implementation guides
4. Test integration with C++ components

## Notes

- All assets should be created in this Content directory
- Use Git LFS for large binary files (textures, audio, animations)
- Test C++ integration regularly as assets are created
- Refer to implementation guides for detailed specifications

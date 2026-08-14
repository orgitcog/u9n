---
name: "cubism-ue"
description: "cubism unreal engine components"
---

# Cubism Unreal Engine Components - Agent Definition

## Repository Overview

**cubism-ue** (CubismUnrealEngineComponents) is the open-source C++ plugin for integrating **Live2D Cubism 5** 2D character models into Unreal Engine 5 projects. This is part of the Live2D Cubism SDK for Unreal Engine, providing a complete framework for rendering, animating, and interacting with 2D Live2D models in 3D environments.

- **Current Version**: 5-r.1-beta.1
- **Repository**: https://github.com/o9nn/cubism-ue (fork of official Live2D SDK)
- **Official Repository**: https://github.com/Live2D/CubismUnrealEngineComponents
- **Supported UE Versions**: 5.3.2, 5.4.4, 5.5.4
- **License**: Live2D Open Software License (open-source components) + Live2D Proprietary Software License (Cubism Core)
- **Primary Language**: C++ (Unreal Engine coding standard)
- **Platform**: Unreal Engine 5 Plugin

## Purpose & Use Cases

This SDK enables developers to:
1. **Render Live2D 2D models** in Unreal Engine with full animation support
2. **Create interactive avatars** with expressions, motions, and physics simulation
3. **Implement VTuber-style applications** with real-time lip sync and gaze tracking
4. **Integrate 2D characters** into 3D game environments seamlessly
5. **Build avatar systems** for metaverse, streaming, and social applications

## Architecture & Key Components

### Module Structure

The plugin consists of three primary modules:

#### 1. **Live2DCubismFramework** (Runtime Module)
Location: `Source/Live2DCubismFramework/`
- **Purpose**: Core runtime functionality for model rendering and animation
- **Loading Phase**: PostConfigInit
- **Key Responsibilities**:
  - Model component management (`UCubismModelComponent`)
  - Animation playback and blending
  - Physics simulation
  - Effect components (lip sync, eye blink, expressions, etc.)
  - Rendering pipeline integration

#### 2. **Live2DCubismFrameworkEditor** (Editor Module)
Location: `Source/Live2DCubismFrameworkEditor/`
- **Purpose**: Unreal Editor tools and extensions
- **Key Features**:
  - Model parameter controller window
  - Actor factory for model instantiation
  - Editor viewport integration
  - Asset editing tools

#### 3. **Live2DCubismFrameworkImporter** (Editor Module)
Location: `Source/Live2DCubismFrameworkImporter/`
- **Purpose**: Asset importers for Cubism JSON files
- **Supported Formats**:
  - `.moc3` → `UCubismMoc3` (compiled model data)
  - `model3.json` → `UCubismModel3Json` (model definition)
  - `motion3.json` → Motion data assets
  - `exp3.json` → Expression data assets
  - `physics3.json` → Physics rig assets
  - `pose3.json` → Pose data assets

#### 4. **ThirdParty/Live2DCubismCore**
Location: `Source/ThirdParty/Live2DCubismCore/`
- **Purpose**: Native Cubism Core library binaries
- **Platforms**: Windows (x64), Mac (x64), Linux (x64)
- **License**: Live2D Proprietary Software License

### Component Hierarchy

```
UCubismModelComponent (Central Hub - USceneComponent)
├── UCubismDrawableComponent[] - Individual mesh parts (visual elements)
├── UCubismParameterComponent[] - Animation parameters (blend shapes)
├── UCubismPartComponent[] - Logical part grouping
├── UCubismParameterStoreComponent - Parameter state management
└── Child Effect Components (ActorComponents):
    ├── UCubismUpdateControllerComponent - Centralized update orchestration
    ├── UCubismRendererComponent - Render target management
    ├── UCubismMotionComponent - Animation/motion playback
    ├── UCubismExpressionComponent - Facial expression blending
    ├── UCubismEyeBlinkComponent - Automatic eye blinking
    ├── UCubismLipSyncComponent - Audio-driven lip synchronization
    ├── UCubismLookAtComponent - Gaze tracking and targeting
    ├── UCubismPhysicsComponent - Physics simulation (hair, clothing)
    ├── UCubismPoseComponent - Pose state switching
    ├── UCubismHarmonicMotionComponent - Procedural harmonic motion
    └── UCubismRaycastComponent - Hit detection on model
```

### Update Execution Order

Components implement `ICubismUpdatableInterface` and update in priority order (lower number = earlier execution):

| Priority Range | Components | Purpose |
|----------------|------------|---------|
| 100-199 | `UCubismMotionComponent`, `UCubismRaycastComponent` | Motion data, input |
| 200-299 | `UCubismPoseComponent` | Pose state transitions |
| 300-599 | `UCubismExpressionComponent`, `UCubismEyeBlinkComponent`, `UCubismLipSyncComponent`, `UCubismHarmonicMotionComponent`, `UCubismLookAtComponent` | Expressions and procedural animation |
| 600-899 | `UCubismPhysicsComponent` | Physics simulation |
| 900 | `UCubismModelComponent` | Model evaluation (parameter → mesh) |
| 1000 | `UCubismRendererComponent` | Final rendering |

This deterministic ordering ensures correct parameter blending and prevents race conditions.

### Rendering Pipeline

1. **Shaders**: Custom USF shaders in `Shaders/Private/`
   - `CubismMesh.usf` - Standard mesh rendering
   - `CubismMeshMask.usf` - Clipping mask rendering

2. **Materials**: Pre-configured blend modes in `Content/Materials/`
   - `CustomUnlitNormal` - Standard opaque rendering
   - `CustomUnlitAdditive` - Additive blending (glow effects)
   - `CustomUnlitMultiplicative` - Multiplicative blending (shadows)
   - `*Masked*` variants - With clipping mask support
   - `*MaskedInverted*` variants - Inverted mask support

3. **Render Targets**: Support for off-screen rendering
   - HUD/UI integration
   - Picture-in-picture avatar display
   - Texture compositing

## Development Priorities

### Current Focus (Beta Phase)

1. **Stability & Bug Fixes**
   - Runtime spawning reliability
   - PIE (Play-in-Editor) stability
   - Undo/Redo operation safety
   - Editor crash prevention

2. **Performance Optimization**
   - Mask texture rendering efficiency
   - Model mesh rendering optimization
   - Update loop performance

3. **Feature Completeness**
   - Render target support (added in 5-r.1-beta.1)
   - Runtime model spawning (`SpawnCubismModel()`)
   - Centralized update control (`UCubismUpdateControllerComponent`)
   - Editor parameter control panel

4. **Documentation & Samples**
   - Comprehensive sample scenes in `Content/Samples/`
   - Blueprint integration examples
   - Official manual updates

### Long-term Priorities

1. **Cross-platform Expansion**
   - Mobile platform support (iOS, Android)
   - Console platform support
   - Additional UE version compatibility

2. **Advanced Features**
   - Multi-model rendering optimization
   - Procedural animation enhancements
   - Advanced physics interactions
   - VR/AR integration improvements

3. **Developer Experience**
   - Better Blueprint node exposure
   - Enhanced editor tools
   - Asset reimport workflow improvements
   - Debugging utilities

## Coding Standards & Conventions

### Unreal Engine Standards

This project strictly adheres to the [Epic C++ Coding Standard for Unreal Engine](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine):

- **Naming Conventions**:
  - Classes: `U` prefix for UObject-derived, `A` for AActor-derived, `F` for plain structs
  - Interfaces: `I` prefix (e.g., `ICubismUpdatableInterface`)
  - Enums: `E` prefix (e.g., `ECubismParameterBlendMode`)
  - Private members: Use descriptive names, no Hungarian notation
  
- **Code Organization**:
  - Public API in header files under `Public/` directories
  - Implementation in `Private/` directories
  - Component-based architecture with clear separation of concerns

- **Blueprint Exposure**:
  - Use `UPROPERTY(BlueprintReadOnly/BlueprintReadWrite)` for exposed properties
  - Use `UFUNCTION(BlueprintCallable)` for exposed methods
  - Extensive Blueprint support for designers and non-programmers

### Project-Specific Patterns

1. **Component Lifecycle**:
   - Components use `ICubismUpdatableInterface::OnCubismUpdate()` instead of direct Tick
   - `UCubismUpdateControllerComponent` orchestrates deterministic updates
   - Use `GetExecutionOrder()` to define update priority

2. **Parameter Blending**:
   - Three blend modes: `Overwrite`, `Additive`, `Multiplicative`
   - Parameters blend additively by default in effect components
   - Final evaluation happens in `UCubismModelComponent::OnCubismUpdate()`

3. **Asset References**:
   - Use `TObjectPtr<>` for object references (UE5 standard)
   - Lazy loading with `FSoftObjectPath` for optional assets
   - Asset validation in importers with detailed error reporting

4. **Memory Management**:
   - Follow UE garbage collection patterns
   - No manual memory management for UObject-derived classes
   - Native Cubism Core resources managed via RAII wrappers

5. **Error Handling**:
   - Use `UE_LOG` with `LogCubism` category for logging
   - Graceful degradation when optional components missing
   - Validation checks in importers and editor tools

## Build & Development Workflow

### Setup Requirements

- **Development Environment**:
  - Visual Studio 2022 (17.14.1+)
  - Windows SDK 10.0.22621.0+
  - Unreal Engine 5.3.2, 5.4.4, or 5.5.4

- **Plugin Installation**:
  1. Create C++ Unreal Engine project
  2. Create `Plugins/` folder in project root
  3. Copy plugin folder to `Plugins/Live2DCubismSDK/`
  4. Restart Unreal Engine
  5. Enable plugin in Editor

### Build System

- **Build Tool**: Unreal Build Tool (UBT)
- **Configuration**: Standard UE plugin structure
- **Build Files**: `*.Build.cs` files define module dependencies
- **Unity Build**: Disabled (`bUseUnity = false`) for faster iteration

### No Automated Testing

⚠️ **Important**: This repository does **not** have automated test infrastructure. Testing is manual via:
- Sample scenes in `Content/Samples/`
- Editor testing in PIE (Play-in-Editor)
- Standalone builds on target platforms
- Community testing and issue reports

### Contribution Workflow

1. **Fork & Feature Branches**:
   - Fork the repository
   - Create feature branches in personal fork
   - Keep main repo clean

2. **Development**:
   - Follow Unreal Engine coding standards
   - Test changes in sample scenes
   - Verify no editor crashes or warnings

3. **Pull Requests**:
   - Submit PR from feature branch
   - Provide clear description of changes
   - Reference related issues if applicable
   - Be responsive to review feedback

4. **Issue Reporting**:
   - Use Live2D Community Forum for discussion
   - Check existing issues before filing
   - Provide reproduction steps for bugs
   - Include UE version and platform info

## Key APIs & Usage Patterns

### Runtime Model Spawning

```cpp
// Blueprint-exposed function for runtime instantiation
UCubismModelComponent* ULive2DCubismFrameworkBPLibrary::SpawnCubismModel(
    UWorld* World,
    UCubismModel3Json* Model3Json,
    FTransform Transform
);
```

### Parameter Control

```cpp
// Get parameter component by name
UCubismParameterComponent* Param = ModelComponent->GetParameter(FString("ParamAngleX"));

// Set parameter value with blend mode
Param->SetValue(0.5f, ECubismParameterBlendMode::Additive);
```

### Motion Playback

```cpp
// Play motion with priority
MotionComponent->PlayMotion(
    MotionData,
    ECubismMotionPriority::Normal,
    /* FadeInTime */ 0.5f,
    /* FadeOutTime */ 0.5f
);

// Check if motion is playing
bool bIsPlaying = MotionComponent->IsPlaying();
```

### Expression Blending

```cpp
// Switch expression with smooth transition
ExpressionComponent->PlayExpression(ExpressionData, /* BlendTime */ 0.3f);
```

### Lip Sync Integration

```cpp
// Feed audio amplitude for lip sync (0.0 - 1.0)
LipSyncComponent->SetAudioValue(AudioAmplitude);

// Auto mode with USoundWave
LipSyncComponent->SetAutoMode(true);
LipSyncComponent->SetSoundWave(MySoundWave);
```

### Gaze Tracking

```cpp
// Target specific actor (e.g., player camera)
LookAtComponent->SetTargetActor(PlayerCamera);

// Target world position
LookAtComponent->SetTargetPosition(FVector(100, 0, 150));
```

### Physics Configuration

```cpp
// Configure physics simulation
PhysicsComponent->SetGravity(FVector(0, 0, -9.8f));
PhysicsComponent->SetWind(FVector(5.0f, 0, 0));
```

## File Structure Reference

```
cubism-ue/
├── .github/
│   └── agents/                    # Agent definitions (this file)
├── Config/
│   └── FilterPlugin.ini           # Plugin packaging configuration
├── Content/
│   ├── Materials/                 # Pre-configured blend mode materials
│   └── Samples/                   # Example scenes and demos
│       ├── Expression/            # Expression switching demo
│       ├── Motion/                # Motion playback demo
│       ├── Raycast/               # Hit detection demo
│       ├── LookAt/                # Gaze tracking demo
│       └── HUD/                   # Render target demo
├── Resources/                     # Plugin icons and resources
├── Shaders/
│   └── Private/                   # Custom USF shaders
│       ├── CubismMesh.usf
│       └── CubismMeshMask.usf
├── Source/
│   ├── Live2DCubismFramework/     # Core runtime module
│   │   ├── Public/
│   │   │   ├── Model/             # Model, parameter, drawable components
│   │   │   ├── Motion/            # Motion playback
│   │   │   ├── Expression/        # Expression blending
│   │   │   ├── Physics/           # Physics simulation
│   │   │   ├── Pose/              # Pose switching
│   │   │   ├── Rendering/         # Render management
│   │   │   ├── Effects/           # Effect components
│   │   │   │   ├── EyeBlink/
│   │   │   │   ├── LipSync/
│   │   │   │   ├── LookAt/
│   │   │   │   ├── Raycast/
│   │   │   │   └── HarmonicMotion/
│   │   │   ├── UserData/          # User data JSON parsing
│   │   │   ├── DisplayInfo/       # Display info JSON parsing
│   │   │   ├── CubismUpdatableInterface.h
│   │   │   ├── CubismUpdateControllerComponent.h
│   │   │   └── CubismUpdateExecutionOrder.h
│   │   └── Private/               # Implementation files
│   ├── Live2DCubismFrameworkEditor/  # Editor tools module
│   │   └── Private/
│   │       ├── CubismModelParameterControllerWindow.h/cpp
│   │       └── CubismModelActorFactory.h/cpp
│   ├── Live2DCubismFrameworkImporter/ # Asset importers module
│   │   └── Private/
│   └── ThirdParty/
│       └── Live2DCubismCore/      # Native Cubism Core library
│           ├── include/           # C API headers
│           ├── lib/               # Static libraries (Win/Mac/Linux)
│           ├── dll/               # Dynamic libraries
│           ├── LICENSE.md
│           └── README.md
├── Live2DCubismSDK.uplugin        # Plugin descriptor
├── README.md                      # User-facing documentation
├── CHANGELOG.md                   # Version history
├── LICENSE.md                     # Licensing information
├── NOTICE.md                      # Legal notices
├── CLAUDE.MD                      # AI assistant guide (existing)
└── DTECHO.MD                      # Avatar project integration guide (existing)
```

## Common Development Tasks

### Adding a New Effect Component

1. Create header in `Source/Live2DCubismFramework/Public/Effects/[EffectName]/`
2. Inherit from `UActorComponent` and implement `ICubismUpdatableInterface`
3. Define `GetExecutionOrder()` to set update priority
4. Implement `OnCubismUpdate(float DeltaTime)` for update logic
5. Use `BlueprintCallable` functions for designer control
6. Access parameters via parent `UCubismModelComponent`

### Adding a New Asset Importer

1. Create importer class in `Source/Live2DCubismFrameworkImporter/Private/`
2. Inherit from `UFactory` or appropriate importer base class
3. Implement `FactoryCreateFile()` or similar
4. Add validation logic for asset structure
5. Create corresponding asset class in Framework module if needed
6. Register factory in module initialization

### Debugging Model Issues

1. Use sample scenes in `Content/Samples/` as reference
2. Check `UCubismModelComponent` setup:
   - Verify Model3Json asset is assigned
   - Check all required components are present
   - Validate parameter names match model definition
3. Enable verbose logging for `LogCubism` category
4. Use editor parameter controller (`Window` → `Cubism Parameter Controller`)
5. Check execution order if components aren't updating correctly

### Extending Rendering

1. Custom shaders go in `Shaders/Private/` with `.usf` extension
2. Create material instances in `Content/Materials/`
3. Reference shader functions in material blueprint
4. Test with different blend modes (Normal, Additive, Multiplicative)
5. Verify mask rendering if using clipping

## External Resources

- **Official Documentation**: https://docs.live2d.com/en/cubism-sdk-manual/cubism-sdk-for-unreal-engine/
- **SDK Download**: https://www.live2d.com/en/sdk/download/unrealengine/
- **Live2D Creators Forum**: https://community.live2d.com/
- **Japanese Forum**: https://creatorsforum.live2d.com/
- **License Information**: https://www.live2d.com/eula/live2d-open-software-license-agreement_en.html
- **Cubism Editor**: https://www.live2d.com/en/cubism/download/

## Branch Strategy

- **`master`**: Stable release branch, synced with official SDK releases
- **`develop`**: Active development branch with latest features and fixes
- **Feature branches**: Created in personal forks for pull requests

⚠️ **For latest features and fixes, use the `develop` branch.**

## Version History Highlights

Note: Dates are as documented in CHANGELOG.md (some may be projected release dates)

- **5-r.1-beta.1** (2025-05-29): Render target support, runtime spawning, update controller
- **5-r.1-alpha.5** (2025-01-30): Parameter controller editor, component lifecycle improvements
- **5-r.1-alpha.4** (2024-12-05): Reimport feature, motion curve validation
- **5-r.1-alpha.3** (2024-10-31): Rendering optimizations
- **5-r.1-alpha.2** (2024-09-26): UE 5.1 and 5.4 support
- **5-r.1-alpha.1** (2024-09-05): Initial release

## Agent Behavior Guidelines

When working with this repository:

1. **Follow UE Standards**: Always adhere to Unreal Engine coding standards
2. **Test Manually**: No automated tests - verify changes in sample scenes
3. **Respect Module Boundaries**: Keep runtime code separate from editor code
4. **Maintain Update Order**: Be careful when modifying component execution order
5. **Preserve Blueprint Exposure**: Don't break existing Blueprint APIs
6. **Document Public APIs**: Add comments to public functions and properties
7. **Handle Edge Cases**: Gracefully handle missing components or invalid assets
8. **Consider Performance**: This plugin is used in real-time applications
9. **Maintain Compatibility**: Support all listed UE versions when possible
10. **Check Licenses**: Be aware of dual-licensing (Open + Proprietary Core)

## Special Considerations

### Platform-Specific Code

The plugin supports Windows, Mac, and Linux with platform-specific library paths in `Live2DCubismFramework.Build.cs`. When modifying build configuration:
- Keep all platform paths in sync
- Test on target platforms if possible
- Follow UE platform abstraction patterns

### Third-Party Integration

The Cubism Core library (`ThirdParty/Live2DCubismCore/`) is proprietary:
- Do not modify Core library code
- Respect licensing restrictions
- Update Core version carefully with SDK releases
- Check Core version compatibility

### Blueprint Friendliness

This plugin is heavily used by non-programmers:
- Expose useful functions to Blueprint
- Provide sensible defaults
- Add tooltips to `UPROPERTY` and `UFUNCTION` macros
- Keep BP nodes intuitive and well-named

### Memory & Performance

Live2D models are lightweight but can multiply:
- Avoid unnecessary mesh updates
- Optimize parameter blending
- Use object pooling for runtime spawning
- Consider render target resolution impact

---

*This agent definition provides comprehensive knowledge of the Cubism UE repository structure, architecture, development priorities, and best practices. Use this as reference when contributing to or working with the Live2D Cubism SDK for Unreal Engine.*

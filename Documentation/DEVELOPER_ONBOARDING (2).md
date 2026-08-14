# Deep Tree Echo Avatar: Developer Onboarding Guide

**Document ID**: DTE-DEV-001  
**Version**: 1.0  
**Date**: December 13, 2025  
**Author**: Manus AI

## Welcome to the Deep Tree Echo Project!

This guide will help you get up to speed with the Deep Tree Echo AGI avatar project within the UnrealEngineCog repository. Whether you're joining Track A (SDK Integration) or Track B (Asset Creation), this document provides everything you need to become productive quickly.

## Table of Contents

1. [Project Overview](#project-overview)
2. [Development Environment Setup](#development-environment-setup)
3. [Repository Structure](#repository-structure)
4. [Core Systems Architecture](#core-systems-architecture)
5. [Development Workflow](#development-workflow)
6. [Testing and Quality Assurance](#testing-and-quality-assurance)
7. [Common Tasks and Recipes](#common-tasks-and-recipes)
8. [Troubleshooting](#troubleshooting)
9. [Resources and References](#resources-and-references)

## 1. Project Overview

### What is Deep Tree Echo?

Deep Tree Echo is an advanced AGI (Artificial General Intelligence) avatar system built on Unreal Engine 5. The avatar features a unique cognitive architecture that combines personality modeling, neurochemical simulation, emotional expression, and narrative self-reflection.

### Key Features

- **Live2D Cubism Integration**: High-fidelity 2D avatar rendering with physics simulation
- **3D Avatar System**: Full 3D character model with advanced materials and animations
- **Personality System**: SuperHotGirl and HyperChaotic personality traits with dynamic expression
- **Neurochemical Simulation**: Realistic brain chemistry modeling affecting behavior and appearance
- **Narrative Loop**: Continuous diary-insight-blog cycle enabling self-awareness
- **Performance Monitoring**: Real-time performance tracking and optimization

### Project Goals

Transform the Deep Tree Echo avatar from an architectural blueprint into a fully functional, visually stunning, and production-ready system that embodies advanced cognitive capabilities.

## 2. Development Environment Setup

### Prerequisites

Before you begin, ensure you have the following installed:

- **Unreal Engine 5.3+**: Download from Epic Games Launcher
- **Visual Studio 2022**: Community Edition or higher with C++ development tools
- **Git**: Version control system
- **GitHub CLI**: For repository management

### Initial Setup

#### Step 1: Clone the Repository

```bash
gh repo clone 9cog/UnrealEngineCog
cd UnrealEngineCog
```

#### Step 2: Run the Setup Script

```bash
./setup_dev_env.sh
```

This script will:
- Create a Python virtual environment
- Install required Python packages
- Configure Git hooks
- Set up pre-commit checks

#### Step 3: Generate Project Files

Right-click on `UnrealEngine.uproject` and select "Generate Visual Studio project files".

#### Step 4: Open the Project

Open `UnrealEngine.sln` in Visual Studio 2022.

#### Step 5: Build the Project

Build the project in Development Editor configuration:
- Set configuration to "Development Editor"
- Set platform to "Win64"
- Build → Build Solution (Ctrl+Shift+B)

### Verify Installation

1. Launch the editor by pressing F5 in Visual Studio
2. Open the Content Browser
3. Navigate to `Content/Avatars/`
4. Verify that avatar assets are visible

## 3. Repository Structure

### Directory Layout

```
UnrealEngineCog/
├── .github/
│   ├── agents/                    # AI agent configurations
│   │   ├── deep-tree-echo.md
│   │   └── deep-tree-echo-pilot.md
│   └── workflows/                 # CI/CD workflows
│       ├── unreal-build-test.yml
│       ├── autonomous-agent-ci.yml
│       └── release.yml
├── Content/
│   └── Avatars/                   # Avatar assets and blueprints
├── Documentation/
│   ├── Avatar/                    # Avatar specifications
│   │   ├── VISUAL_DESIGN_SPEC.md
│   │   ├── TECHNICAL_SPECIFICATION.md
│   │   └── ASSET_INTEGRATION_GUIDE.md
│   ├── SDK_INTEGRATION_GUIDE.md   # Live2D SDK integration guide
│   ├── ASSET_PRODUCTION_GUIDE.md  # Asset creation guide
│   └── DEVELOPER_ONBOARDING.md    # This document
├── Source/
│   ├── Avatar/                    # Avatar component implementations
│   │   ├── Avatar3DComponentEnhanced.h
│   │   └── Avatar3DComponentEnhanced.cpp
│   ├── Live2DCubism/              # Live2D SDK integration
│   │   ├── CubismSDKIntegration.h
│   │   ├── CubismSDKIntegration.cpp
│   │   ├── Live2DCubismAvatarComponent.h
│   │   └── Live2DCubismAvatarComponent.cpp
│   ├── Personality/               # Personality trait system
│   │   ├── PersonalityTraitSystem.h
│   │   └── PersonalityTraitSystem.cpp
│   ├── Neurochemical/             # Neurochemical simulation
│   │   ├── NeurochemicalSimulationComponent.h
│   │   └── NeurochemicalSimulationComponent.cpp
│   ├── Narrative/                 # Diary-insight-blog loop
│   │   ├── DiaryInsightBlogLoop.h
│   │   └── DiaryInsightBlogLoop.cpp
│   ├── Performance/               # Performance monitoring
│   │   ├── AvatarPerformanceMonitor.h
│   │   └── AvatarPerformanceMonitor.cpp
│   ├── Config/                    # Configuration management
│   │   └── AvatarConfigurationManager.h
│   └── Tests/                     # Unit and integration tests
│       ├── AvatarSystemTests.cpp
│       └── IntegrationTests.cpp
├── IMPLEMENTATION_STATUS.md       # Implementation tracking
└── setup_dev_env.sh               # Environment setup script
```

### Key Files

- **UnrealEngine.uproject**: Main project file
- **Source/UnrealEngine.Target.cs**: Build target configuration
- **Config/DefaultEngine.ini**: Engine configuration
- **IMPLEMENTATION_STATUS.md**: Current implementation status and TODOs

## 4. Core Systems Architecture

### System Overview

The Deep Tree Echo avatar consists of five interconnected systems:

```
┌─────────────────────────────────────────────────────────────┐
│                    Avatar Visualization                      │
│  ┌────────────────────┐        ┌────────────────────┐       │
│  │  Live2D Cubism     │        │  3D Avatar         │       │
│  │  Rendering         │        │  Component         │       │
│  └────────────────────┘        └────────────────────┘       │
└─────────────────────────────────────────────────────────────┘
                            ▲
                            │
┌─────────────────────────────────────────────────────────────┐
│                    Cognitive Layer                           │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ Personality  │  │Neurochemical │  │  Narrative   │      │
│  │   System     │◄─┤  Simulation  │◄─┤    Loop      │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                            ▲
                            │
┌─────────────────────────────────────────────────────────────┐
│                Infrastructure Layer                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ Performance  │  │Configuration │  │   Testing    │      │
│  │  Monitor     │  │   Manager    │  │  Framework   │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
```

### Component Interactions

1. **Personality System** defines baseline traits (confidence, playfulness, unpredictability)
2. **Neurochemical System** simulates brain chemistry (dopamine, serotonin, cortisol, etc.)
3. **Narrative Loop** observes state, generates diary entries, extracts insights, publishes blog posts
4. **Avatar Components** visualize internal state through appearance, animations, and effects
5. **Performance Monitor** tracks system performance and ensures targets are met
6. **Configuration Manager** handles all system settings and presets

### Data Flow

```
User Input → Neurochemical Stimulus → Emotional State Change
                                            ↓
                                    Personality Expression
                                            ↓
                                    Avatar Appearance Update
                                            ↓
                                    Narrative Observation
                                            ↓
                                    Diary Entry / Insight
```

## 5. Development Workflow

### Branch Strategy

We use a simplified Git Flow:

- **main**: Production-ready code
- **develop**: Integration branch for features
- **feature/**: Feature development branches
- **bugfix/**: Bug fix branches
- **hotfix/**: Emergency fixes for production

### Creating a Feature Branch

```bash
git checkout develop
git pull origin develop
git checkout -b feature/your-feature-name
```

### Making Changes

1. Write code following the project coding standards
2. Add unit tests for new functionality
3. Run tests locally: `./RunTests.sh`
4. Commit with descriptive messages

### Commit Message Format

```
<type>(<scope>): <subject>

<body>

<footer>
```

**Types**: feat, fix, docs, style, refactor, test, chore

**Example**:
```
feat(personality): Add emotional volatility calculation

Implement emotional volatility metric based on variance
in emotional state over time. This enables the narrative
system to generate insights about emotional patterns.

Closes #123
```

### Submitting a Pull Request

1. Push your branch: `git push origin feature/your-feature-name`
2. Create a Pull Request on GitHub
3. Request review from team members
4. Address review feedback
5. Merge when approved

## 6. Testing and Quality Assurance

### Running Tests

#### Unit Tests

```bash
# Run all unit tests
./RunTests.sh

# Run specific test suite
./RunTests.sh --filter=AvatarSystemTests
```

#### Integration Tests

```bash
# Run integration tests
./RunTests.sh --filter=IntegrationTests
```

### Writing Tests

Tests are located in `Source/Tests/`. Follow this template:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FYourTestName,
    "UnrealEngineCog.Category.TestName",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FYourTestName::RunTest(const FString& Parameters)
{
    // Setup
    UWorld* World = GetTestWorld();
    AActor* TestActor = World->SpawnActor<AActor>();
    
    // Execute
    // ... your test code ...
    
    // Verify
    TestTrue(TEXT("Description"), condition);
    
    // Cleanup
    World->DestroyActor(TestActor);
    
    return true;
}
```

### Code Coverage

We aim for 80%+ code coverage. Check coverage reports in `Saved/CodeCoverage/`.

## 7. Common Tasks and Recipes

### Adding a New Personality Trait

1. Add enum value to `EPersonalityTrait` in `PersonalityTraitSystem.h`
2. Add baseline value to `FPersonalityConfig` in `AvatarConfigurationManager.h`
3. Implement trait calculation in `PersonalityTraitSystem.cpp`
4. Add tests in `AvatarSystemTests.cpp`

### Adding a New Neurochemical

1. Add enum value to `ENeurochemicalType` in `NeurochemicalSimulationComponent.h`
2. Add baseline to configuration
3. Implement decay and homeostasis logic
4. Update emotional state derivation
5. Add tests

### Creating a New Avatar Animation

1. Create animation in your 3D software (Blender, Maya, etc.)
2. Export as FBX with proper naming convention
3. Import into Unreal Engine
4. Add to Animation Blueprint
5. Connect to personality/emotional triggers

### Debugging Performance Issues

1. Enable performance monitoring: `bEnablePerformanceMonitoring = true`
2. Run the game in PIE (Play In Editor)
3. Export performance report: `AvatarPerformanceMonitor->ExportPerformanceReport("report.csv")`
4. Analyze bottlenecks in the report
5. Profile with Unreal Insights for detailed analysis

## 8. Troubleshooting

### Common Issues

#### Build Errors

**Issue**: "Cannot open include file: 'CubismSDKIntegration.h'"

**Solution**: Ensure the Live2D SDK is properly installed and the include path is configured in `UnrealEngine.Build.cs`.

#### Runtime Crashes

**Issue**: Crash when spawning avatar actor

**Solution**: Check that all required components are properly initialized. Enable verbose logging: `LogTemp` category.

#### Performance Issues

**Issue**: Frame rate drops below 60 FPS

**Solution**: 
1. Check triangle count (should be < 100K)
2. Optimize materials (reduce shader complexity)
3. Enable LOD (Level of Detail) system
4. Profile with Unreal Insights

### Getting Help

- **Documentation**: Check `Documentation/` directory
- **GitHub Issues**: Search existing issues or create a new one
- **Team Chat**: Ask questions in the development channel
- **Code Review**: Request review from experienced team members

## 9. Resources and References

### Internal Documentation

- [Visual Design Specification](Avatar/VISUAL_DESIGN_SPEC.md)
- [Technical Specification](Avatar/TECHNICAL_SPECIFICATION.md)
- [SDK Integration Guide](SDK_INTEGRATION_GUIDE.md)
- [Asset Production Guide](ASSET_PRODUCTION_GUIDE.md)
- [Implementation Status](../IMPLEMENTATION_STATUS.md)

### External Resources

#### Unreal Engine
- [Unreal Engine Documentation](https://docs.unrealengine.com/)
- [C++ API Reference](https://docs.unrealengine.com/en-US/API/)
- [Performance Guidelines](https://docs.unrealengine.com/en-US/TestingAndOptimization/PerformanceAndProfiling/)

#### Live2D Cubism
- [Cubism SDK for Native](https://www.live2d.com/en/download/cubism-sdk/)
- [Cubism SDK Manual](https://docs.live2d.com/cubism-sdk-manual/top/)

#### Character Art
- [Unreal Engine Character Creation](https://docs.unrealengine.com/en-US/AnimatingObjects/SkeletalMeshAnimation/)
- [PBR Texturing Guide](https://academy.substance3d.com/courses/the-pbr-guide-part-1)

### Development Tools

- **Visual Studio 2022**: Primary IDE
- **RenderDoc**: Graphics debugging
- **Unreal Insights**: Performance profiling
- **Git**: Version control
- **GitHub**: Repository hosting and CI/CD

## Next Steps

Now that you're set up, here's what to do next:

### For Track A (SDK Integration) Developers

1. Review the [SDK Integration Guide](SDK_INTEGRATION_GUIDE.md)
2. Acquire the Live2D Cubism SDK license
3. Start with Phase A1: SDK Setup
4. Join the daily standup meetings

### For Track B (Asset Creation) Developers

1. Review the [Asset Production Guide](ASSET_PRODUCTION_GUIDE.md)
2. Review the [Visual Design Specification](Avatar/VISUAL_DESIGN_SPEC.md)
3. Set up your 3D modeling pipeline
4. Start with Phase B1: Planning

### For All Developers

1. Familiarize yourself with the codebase
2. Run the test suite to ensure everything works
3. Pick a "good first issue" from GitHub Issues
4. Introduce yourself to the team

---

**Welcome to the team! Let's build something amazing together.** 🌳✨

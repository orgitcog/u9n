# Integration Specification: Neuro-Symbolic Fusion Reactor

## Overview

This document provides concrete implementation steps for consolidating the fragmented u9n codebase into a unified neuro-symbolic cognitive architecture.

---

## 1. File Consolidation Matrix

### 1.1 Critical Duplicates to Resolve

| Canonical File | Duplicate to Delete | Action |
|----------------|--------------------|---------|
| `Core/CognitiveCycleManager.h` | `Cognitive/CognitiveCycleManager.h` | Merge APIs, delete duplicate |
| `Core/Sys6CognitiveBridge.h` | `Sys6/Sys6CognitiveBridge.h` | Keep Core, delete Sys6 |
| `Core/Sys6OperadEngine.h` | `Sys6/Sys6OperadEngine.h` | Keep Core, delete Sys6 |
| `Sensorimotor/SensorimotorIntegration.h` | `Embodied/SensorimotorIntegration.h` | Keep Sensorimotor, redirect Embodied |

### 1.2 Avatar Component Consolidation

| Final Location | Files to Merge/Remove |
|----------------|----------------------|
| `Avatar/Avatar3DComponent.h` | `Avatar3DComponentEnhanced.h`, `Avatar3DComponentComplete.cpp`, `Source/Avatar/Avatar3DComponent.h` |

---

## 2. Include Path Migration

### 2.1 Files Requiring Update (../Cognitive/ → ../Core/)

```
DeepTreeEcho/Sensorimotor/SensorimotorIntegration.h
  - #include "../Cognitive/CognitiveCycleManager.h"
  + #include "../Core/CognitiveCycleManager.h"

DeepTreeEcho/ActiveInference/AXIOMActiveInference.h
  - #include "../Cognitive/CognitiveCycleManager.h"
  + #include "../Core/CognitiveCycleManager.h"

DeepTreeEcho/Memory/EpisodicMemorySystem.h
  - #include "../Cognitive/CognitiveCycleManager.h"
  + #include "../Core/CognitiveCycleManager.h"

DeepTreeEcho/Integration/DeepTreeEchoIntegration.h
  - #include "../Cognitive/CognitiveCycleManager.h"
  + #include "../Core/CognitiveCycleManager.h"

DeepTreeEcho/Goals/HierarchicalGoalManager.h
  - #include "../Cognitive/CognitiveCycleManager.h"
  + #include "../Core/CognitiveCycleManager.h"
```

### 2.2 Type Migration Map

When updating files, use the unified types from `Core/Types/CognitiveTypes.h`:

| Old Type (Cognitive/) | New Type (Core/Types/) | Notes |
|----------------------|------------------------|-------|
| `EConsciousnessStream` | `EConsciousnessStream` | Same name, kept |
| `ECognitiveMode` | `ECognitiveMode` | Same name, kept |
| `ECognitiveStepType` (3 values) | `EStepCategory` | Renamed for clarity |
| `ECognitiveStepType` (12 values) | `ECognitiveStepType` | Granular steps |
| `FStreamState` (Cognitive) | `FUnifiedStreamState` | Merged fields |
| `FStreamState` (Core) | `FUnifiedStreamState` | Merged fields |
| `FNestedShellState` | `FNestedShellState` | Kept |

---

## 3. API Compatibility Layer

For gradual migration, create compatibility typedefs in `Core/Types/CognitiveTypesCompat.h`:

```cpp
#pragma once

#include "CognitiveTypes.h"

// Backward compatibility aliases
// Remove after full migration

// From Cognitive/CognitiveCycleManager.h
using ECognitiveModeType = ECognitiveMode;  // Core used ECognitiveModeType

// Type alias for old FStreamState usage
using FStreamState = FUnifiedStreamState;

// Step type compatibility (3-value → category)
inline EStepCategory ToStepCategory(ECognitiveStepType OldType)
{
    return CognitiveTypeUtils::GetStepCategory(OldType);
}
```

---

## 4. Fusion Membrane Interface Definitions

### 4.1 INeuralToSymbolic Interface

Location: `DeepTreeEcho/Fusion/Interfaces/INeuralToSymbolic.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "../Core/Types/CognitiveTypes.h"

/**
 * Interface for Neural → Symbolic data translation
 * Implemented by fusion membrane components
 */
class INeuralToSymbolic
{
public:
    virtual ~INeuralToSymbolic() = default;

    /**
     * Convert reservoir activation to motor command
     * @param ReservoirState Current ESN activation vector
     * @param StreamState Consciousness stream context
     * @return Motor command for skeletal control
     */
    virtual FMotorCommand TranslateToMotor(
        const TArray<float>& ReservoirState,
        const FUnifiedStreamState& StreamState
    ) = 0;

    /**
     * Convert cognitive state to avatar expression
     * @param CognitiveState Current cognitive processing state
     * @param Intensity Expression intensity multiplier
     * @return Expression targets for blend shapes
     */
    virtual FAvatarExpressionState TranslateToExpression(
        const FCognitiveState& CognitiveState,
        float Intensity
    ) = 0;

    /**
     * Convert attention weights to gaze target
     * @param AttentionWeights Saliency map from reservoir
     * @param VisibleActors Actors in visual field
     * @return World-space gaze target
     */
    virtual FVector TranslateToGaze(
        const TArray<float>& AttentionWeights,
        const TArray<AActor*>& VisibleActors
    ) = 0;
};
```

### 4.2 ISymbolicToNeural Interface

Location: `DeepTreeEcho/Fusion/Interfaces/ISymbolicToNeural.h`

```cpp
#pragma once

#include "CoreMinimal.h"
#include "../Core/Types/CognitiveTypes.h"

/**
 * Interface for Symbolic → Neural data translation
 * Implemented by fusion membrane components
 */
class ISymbolicToNeural
{
public:
    virtual ~ISymbolicToNeural() = default;

    /**
     * Encode visual scene to reservoir input
     * @param Perceptions Visual perception data from Unreal
     * @param AttentionBias Attention bias for saliency
     * @return Input vector for reservoir
     */
    virtual TArray<float> EncodeVisual(
        const TArray<FVisualPerception>& Perceptions,
        float AttentionBias
    ) = 0;

    /**
     * Encode skeletal pose to proprioceptive input
     * @param Pose Current skeletal pose snapshot
     * @param Schema Body schema mapping
     * @return Proprioceptive input vector
     */
    virtual TArray<float> EncodeProprioceptive(
        const FPoseSnapshot& Pose,
        const FBodySchema& Schema
    ) = 0;

    /**
     * Encode physics state to embodiment input
     * @param State Current physics simulation state
     * @param Contacts Collision contacts
     * @return Physics-derived input vector
     */
    virtual TArray<float> EncodePhysics(
        const FPhysicsState& State,
        const TArray<FHitResult>& Contacts
    ) = 0;

    /**
     * Encode environmental affordances
     * @param Affordances Detected affordances
     * @param AgentLocation Current agent position
     * @return Affordance-encoded input vector
     */
    virtual TArray<float> EncodeAffordances(
        const TArray<FAffordance>& Affordances,
        const FVector& AgentLocation
    ) = 0;
};
```

---

## 5. Directory Structure Reorganization

### 5.1 Proposed Final Structure

```
DeepTreeEcho/
├── Core/                           # [CANONICAL] Central types and orchestration
│   ├── Types/
│   │   ├── CognitiveTypes.h        # ✓ Created - unified type definitions
│   │   ├── CognitiveTypesCompat.h  # Compatibility layer (temporary)
│   │   └── StreamTypes.h           # Stream-specific types
│   ├── DeepTreeEchoCore.h          # Central orchestrator
│   ├── CognitiveCycleManager.h     # [UNIFIED] Cognitive cycle management
│   ├── Sys6CognitiveBridge.h       # [UNIFIED] Sys6 integration
│   └── Sys6OperadEngine.h          # [UNIFIED] Operad computation
│
├── Neural/                         # [NEURAL HEMISPHERE]
│   ├── Reservoir/                  # Echo State Networks
│   │   ├── DeepTreeEchoReservoir.h
│   │   ├── ReservoirCognitiveIntegration.h
│   │   └── DeepCognitiveBridge.h
│   ├── Pattern/                    # Temporal pattern recognition
│   └── Inference/                  # Predictive processing
│       └── ActiveInference/        # (moved from root)
│
├── Fusion/                         # [FUSION MEMBRANE]
│   ├── Interfaces/
│   │   ├── INeuralToSymbolic.h
│   │   └── ISymbolicToNeural.h
│   ├── UnrealBridge/               # (moved from root)
│   │   └── DeepTreeEchoUnrealBridge.h
│   ├── DNABinding/
│   │   └── DNABodySchemaBinding.h  # (moved from 4ECognition)
│   ├── Sensorimotor/               # (moved from root)
│   │   └── SensorimotorIntegration.h
│   └── Expression/
│       └── ExpressiveAnimationSystem.h  # (moved from Avatar)
│
├── Cognition/                      # [4E EMBODIED COGNITION]
│   ├── Embodied/                   # Body schema, proprioception
│   │   └── EmbodiedCognitionComponent.h
│   ├── Embedded/                   # Environmental coupling
│   ├── Enacted/                    # Sensorimotor contingencies
│   └── Extended/                   # External tools, memory
│
├── Avatar/                         # [AVATAR SYSTEMS]
│   ├── Avatar3DComponent.h         # [UNIFIED] Single implementation
│   ├── Evolution/
│   │   └── AvatarEvolutionSystem.h
│   └── MetaHuman/
│       ├── MetaHumanDNABridge.h
│       └── PythonDNACalibWrapper.h
│
├── Systems/                        # [HIGHER-ORDER SYSTEMS]
│   ├── Cosmos/                     # System 1-5 state machine
│   ├── Entelechy/                  # Goal actualization
│   ├── Wisdom/                     # Relevance realization
│   ├── Metamodel/                  # Organizational dynamics
│   └── Memory/                     # Memory systems
│
├── Integration/                    # Master integration (keep)
│   └── DeepTreeEchoIntegration.h
│
└── Testing/                        # Unit and E2E tests
    ├── UnitTests/
    └── E2E/
```

### 5.2 Files to Delete After Migration

```
# Duplicate files (after merging unique code)
DeepTreeEcho/Cognitive/CognitiveCycleManager.h
DeepTreeEcho/Cognitive/CognitiveCycleManager.cpp
DeepTreeEcho/Sys6/Sys6CognitiveBridge.h
DeepTreeEcho/Sys6/Sys6CognitiveBridge.cpp
DeepTreeEcho/Sys6/Sys6OperadEngine.h
DeepTreeEcho/Sys6/Sys6OperadEngine.cpp
DeepTreeEcho/Embodied/SensorimotorIntegration.h
DeepTreeEcho/Embodied/SensorimotorIntegration.cpp

# Avatar duplicates (after consolidation)
UnrealEcho/Avatar/Avatar3DComponentEnhanced.h
UnrealEcho/Avatar/Avatar3DComponentEnhanced.cpp
UnrealEcho/Avatar/Avatar3DComponentComplete.cpp
Source/Avatar/Avatar3DComponent.h

# Empty stubs (Source/)
Source/DeepTreeEcho/DeepTreeEcho.generated.h
Source/DeepTreeEcho/CosmicOrderSystem.generated.h
Source/DeepTreeEcho/EchobeatsGamingIntegration.generated.h
Source/DeepTreeEcho/GamingMasterySystem.generated.h
Source/DeepTreeEcho/NestorDAG.generated.h
Source/DeepTreeEcho/SGramPatternSystem.generated.h
Source/DeepTreeEcho/StrategicCognitionBridge.generated.h
Source/DeepTreeEcho/UnrealGamingMasteryIntegration.generated.h
```

---

## 6. Migration Script Template

```bash
#!/bin/bash
# migrate_cognitive_types.sh
# Run from repository root

set -e

echo "=== Phase 1: Update include paths ==="

# Files using ../Cognitive/CognitiveCycleManager.h
files_to_update=(
    "DeepTreeEcho/Sensorimotor/SensorimotorIntegration.h"
    "DeepTreeEcho/ActiveInference/AXIOMActiveInference.h"
    "DeepTreeEcho/Memory/EpisodicMemorySystem.h"
    "DeepTreeEcho/Integration/DeepTreeEchoIntegration.h"
    "DeepTreeEcho/Goals/HierarchicalGoalManager.h"
)

for file in "${files_to_update[@]}"; do
    if [ -f "$file" ]; then
        sed -i 's|#include "../Cognitive/CognitiveCycleManager.h"|#include "../Core/CognitiveCycleManager.h"|g' "$file"
        echo "Updated: $file"
    fi
done

echo "=== Phase 2: Add unified types include ==="

# Add CognitiveTypes.h include to Core/CognitiveCycleManager.h
# (Manual step - requires merging API definitions)

echo "=== Phase 3: Verify compilation ==="
# cmake --build build --target DeepTreeEcho

echo "=== Migration complete ==="
```

---

## 7. Verification Checklist

### 7.1 Pre-Migration Verification

- [ ] All duplicate files identified and documented
- [ ] Type differences between duplicates analyzed
- [ ] Compatibility layer designed
- [ ] Unified types header created (`CognitiveTypes.h`)

### 7.2 Migration Verification

- [ ] All include paths updated
- [ ] No circular dependencies introduced
- [ ] Compilation succeeds without errors
- [ ] Unit tests pass
- [ ] Runtime behavior unchanged

### 7.3 Post-Migration Verification

- [ ] Duplicate files removed
- [ ] Directory structure matches specification
- [ ] Documentation updated
- [ ] CLAUDE.md reflects new structure
- [ ] Integration tests pass

---

## 8. Risk Mitigation

| Risk | Mitigation |
|------|------------|
| Breaking existing code | Compatibility layer with typedefs |
| Linker errors from duplicate symbols | Delete duplicates only after all references updated |
| Runtime behavior change | Preserve exact enum values and struct layouts |
| IDE/tooling confusion | Clear documentation, single canonical path |

---

## 9. Timeline-Free Implementation Steps

### Step 1: Foundation
1. Create `Core/Types/CognitiveTypes.h` (DONE)
2. Create compatibility layer header
3. Update Core/CognitiveCycleManager.h to include unified types

### Step 2: Include Path Migration
4. Update 5 files using ../Cognitive/ path
5. Verify compilation
6. Run existing tests

### Step 3: Duplicate Removal
7. Merge any unique code from Cognitive/ version
8. Delete Cognitive/CognitiveCycleManager.h
9. Delete Cognitive/CognitiveCycleManager.cpp

### Step 4: Sys6 Consolidation
10. Update Sys6/ imports to use Core/ versions
11. Delete Sys6/Sys6CognitiveBridge.h
12. Delete Sys6/Sys6OperadEngine.h

### Step 5: Avatar Consolidation
13. Merge Avatar3D variants into single implementation
14. Remove duplicate files
15. Update all Avatar references

### Step 6: Fusion Membrane
16. Create Fusion/ directory structure
17. Implement INeuralToSymbolic interface
18. Implement ISymbolicToNeural interface
19. Refactor existing bridges to use interfaces

### Step 7: Validation
20. Full compilation verification
21. Unit test suite
22. Integration tests
23. Performance benchmarks

---

*Document Version: 1.0*
*Created: 2026-01-09*
*Status: Ready for Implementation*

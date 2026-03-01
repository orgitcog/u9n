# Feature F1.4.3: ECAN Attention Allocation - Implementation Summary

**Feature ID:** F1.4.3  
**Phase:** 1.4 - Hypergraph Memory System  
**Implementation Date:** March 2026  
**Status:** Implemented

---

## Overview

Feature F1.4.3 implements Economic Attention Networks (ECAN) for importance-based resource allocation within the Deep Tree Echo cognitive architecture. This provides OpenCog-compatible attention value dynamics using STI (Short-Term Importance), LTI (Long-Term Importance), and VLTI (Very Long-Term Importance) for managing cognitive resources efficiently.

## Files Created

### Core Implementation
- **`DeepTreeEcho/Attention/ECANAttentionAllocation.h`** - Header file containing:
  - Attention focus level enumeration
  - ECAN spread type enumeration
  - Attention value structure (FECANAttentionValue)
  - ECAN atom structure (FECANAtom)
  - Hebbian link structure (FHebbianLink)
  - ECAN configuration structure (FECANConfig)
  - ECAN statistics structure (FECANStatistics)
  - Attention spread event structure
  - `UECANAttentionAllocation` component class declaration

- **`DeepTreeEcho/Attention/ECANAttentionAllocation.cpp`** - Implementation file containing:
  - Atom lifecycle operations (create, remove, query)
  - Attention value management (STI/LTI/VLTI)
  - Spreading activation algorithms
  - Hebbian learning dynamics
  - Economic rent/wage mechanisms
  - Forgetting and cleanup operations

### Agent Definition
- **`.github/agents/u9ci/F1.4.3.md`** - Maintenance agent specification for ongoing feature oversight

### Documentation
- **`FEATURE_F1.4.3_IMPLEMENTATION_SUMMARY.md`** - This file

## Key Features

### 1. Attention Value System (STI/LTI/VLTI)
```cpp
struct FECANAttentionValue
{
    float STI;   // Short-Term Importance: immediate relevance
    float LTI;   // Long-Term Importance: persistent value
    float VLTI;  // Very Long-Term Importance: protected importance
    EAttentionFocusLevel FocusLevel;
};
```

### 2. Focus Level Classification
```cpp
enum class EAttentionFocusLevel : uint8
{
    AttentionalFocus,   // Top STI atoms (being processed)
    ActiveSet,          // High STI atoms (readily available)
    PassiveSet,         // Moderate STI (background awareness)
    InactiveSet,        // Low STI (dormant)
    ForgetSet           // Below threshold (removal candidates)
};
```

### 3. Spreading Activation
- Attention propagates through hypergraph links
- Configurable spreading fraction (default 30%)
- Supports direct links and Hebbian associations
- Maximum spread targets limit

### 4. Hebbian Learning
- "Neurons that fire together wire together"
- Co-activation strengthens links
- Links decay without reinforcement
- Configurable learning rate and window

### 5. Economic Dynamics
- **Rent**: STI-proportional cost for holding attention
- **Wages**: LTI increase based on usefulness
- **Budget**: Total system STI is constrained
- **Normalization**: Maintains economic balance

## Architecture Integration

### OpenCog Compatibility
- Attention values align with OpenCog's FAttentionValue
- Focus levels map to OpenCog's attention bank structure
- Hebbian links compatible with OpenCog's HebbianLink
- Economic metaphor follows ECAN principles

### Deep Tree Echo Integration
- Works alongside existing AttentionSystem
- Integrates with AtomSpace for knowledge representation
- Compatible with CognitiveCycleManager
- Supports 12-step cognitive cycle

## Usage Example

```cpp
// Get ECAN component
UECANAttentionAllocation* ECAN = GetOwner()->FindComponentByClass<UECANAttentionAllocation>();

// Create atoms
FString AppleID = ECAN->CreateAtom(TEXT("Apple"), TEXT("ConceptNode"), 50.0f, 10.0f);
FString RedID = ECAN->CreateAtom(TEXT("Red"), TEXT("PredicateNode"), 30.0f, 5.0f);
FString FruitID = ECAN->CreateAtom(TEXT("Fruit"), TEXT("ConceptNode"), 60.0f, 20.0f);

// Link atoms
ECAN->AddAtomLink(AppleID, RedID);
ECAN->AddAtomLink(AppleID, FruitID);

// Stimulate attention
ECAN->StimulateSTI(AppleID, 25.0f);  // Boost Apple's importance

// Record co-activation (Hebbian learning)
ECAN->RecordCoActivation(AppleID, RedID);

// Spread attention from focus
ECAN->SpreadFromFocus();

// Get statistics
FECANStatistics Stats = ECAN->GetStatistics();
UE_LOG(LogTemp, Log, TEXT("Focus atoms: %d, Total STI: %.2f"), 
    Stats.AttentionalFocusCount, Stats.TotalSTI);

// Get top atoms
TArray<FECANAtom> TopAtoms = ECAN->GetTopAtomsBySTI(5);
for (const FECANAtom& Atom : TopAtoms)
{
    UE_LOG(LogTemp, Log, TEXT("%s: STI=%.2f, LTI=%.2f"), 
        *Atom.Name, Atom.AttentionValue.STI, Atom.AttentionValue.LTI);
}

// Protect important atoms from forgetting
ECAN->ProtectFromForgetting(FruitID);

// Process forgetting (removes low-STI atoms)
TArray<FString> Forgotten = ECAN->ProcessForgetting();
```

## Configuration Options

```cpp
FECANConfig Config;

// Focus Thresholds
Config.AttentionalFocusThreshold = 80.0f;  // Top tier
Config.ActiveSetThreshold = 50.0f;          // Active memory
Config.PassiveSetThreshold = 20.0f;         // Background
Config.ForgetThreshold = -50.0f;            // Removal threshold

// Economic Parameters
Config.MaxSTI = 100.0f;
Config.MinSTI = -100.0f;
Config.TotalSTIFunds = 10000.0f;  // Total budget
Config.STIRentRate = 0.01f;       // 1% per cycle
Config.LTIWageRate = 0.1f;        // Usage reward

// Decay Rates (per second)
Config.STIDecayRate = 0.1f;       // Fast decay
Config.LTIDecayRate = 0.01f;      // Slow decay
Config.VLTIDecayRate = 0.001f;    // Very slow decay

// Spreading Activation
Config.MaxSpreadTargets = 10;
Config.SpreadingFraction = 0.3f;  // 30% of STI spreads
Config.MinSpreadLinkStrength = 0.1f;

// Hebbian Learning
Config.HebbianLearningRate = 0.1f;
Config.HebbianDecayRate = 0.01f;
Config.CoActivationWindow = 1.0f;  // 1 second

// Limits
Config.MaxAttentionalFocusSize = 10;
Config.MaxActiveSetSize = 100;
Config.MaxTotalAtoms = 10000;
```

## Events

```cpp
// Focus level change
UPROPERTY(BlueprintAssignable)
FOnAtomFocusChanged OnAtomFocusChanged;

// Atom forgotten
UPROPERTY(BlueprintAssignable)
FOnAtomForgotten OnAtomForgotten;

// STI stimulation
UPROPERTY(BlueprintAssignable)
FOnSTIStimulus OnSTIStimulus;

// Attentional focus changed
UPROPERTY(BlueprintAssignable)
FOnAttentionalFocusChanged OnAttentionalFocusChanged;

// Hebbian link created
UPROPERTY(BlueprintAssignable)
FOnHebbianLinkCreated OnHebbianLinkCreated;
```

## Dependencies

- **Unreal Engine 5.x** - Actor component infrastructure
- **DeepTreeEcho Core** - Cognitive architecture integration
- **AttentionSystem** - Salience computation (optional)
- **AtomSpace** - Knowledge representation (optional)

## Next Steps

### Phase 1.4 Continuation
- **F1.4.4**: Attention-Based Retrieval - Use ECAN for memory retrieval
- **F1.4.5**: Forgetting Curve Implementation - Enhanced decay models

### Integration Tasks
- Connect ECAN to cognitive cycle manager
- Synchronize with AtomSpace atoms
- Implement attention-based retrieval queries
- Add visualization for attention dynamics
- Profile performance with large atom counts

## Testing Checklist

- [ ] Atom creation with various parameters
- [ ] STI stimulation and clamping
- [ ] LTI accumulation and decay
- [ ] VLTI protection from forgetting
- [ ] Focus level transitions
- [ ] Spreading activation through links
- [ ] Spreading through Hebbian associations
- [ ] Hebbian link creation on co-activation
- [ ] Hebbian link decay
- [ ] Rent collection
- [ ] Wage awarding
- [ ] STI normalization
- [ ] Forgetting threshold enforcement
- [ ] Event broadcasting
- [ ] Statistics calculation

## Performance Targets

| Operation | Target Latency |
|-----------|----------------|
| Atom creation | <0.1ms |
| STI stimulation | <0.1ms |
| Spreading (per atom) | <1ms |
| Hebbian update | <0.5ms |
| Focus update (1K atoms) | <1ms |
| Forgetting | <5ms |

---

**Document Version:** 1.0.0  
**Last Updated:** 2026-03-01

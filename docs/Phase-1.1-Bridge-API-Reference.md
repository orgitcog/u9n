# Phase 1.1: Neural-Symbolic Bridge API Reference

**Version:** 1.0.0  
**Phase:** 1.1 - Neural-Symbolic Bridge Architecture  
**Epic:** E1 - Foundation & Core Integration

---

## Overview

The Neural-Symbolic Bridge provides bidirectional data flow between neural subsystems (Echo State Networks, Deep Tree Echo Reservoir) and symbolic subsystems (Unreal Engine game logic, knowledge bases). It consists of four core components:

| Feature | Component | Header |
|---------|-----------|--------|
| F1.1.1 | Neural-to-Symbolic Translator | `DeepTreeEcho/Core/NeuralToSymbolicTranslator.h` |
| F1.1.2 | Symbolic-to-Neural Encoder | `DeepTreeEcho/Core/SymbolicToNeuralEncoder.h` |
| F1.1.3 | Bidirectional Message Protocol | `DeepTreeEcho/Core/BidirectionalMessageProtocol.h` |
| F1.1.4 | Synchronization Manager | `DeepTreeEcho/Core/NeuralSymbolicSyncManager.h` |

All components are `UActorComponent` subclasses that attach to the same actor and discover each other via `FindComponentByClass`.

---

## F1.1.1: Neural-to-Symbolic Translator

**Class:** `UNeuralToSymbolicTranslator`  
**Purpose:** Converts neural activation tensors into discrete symbolic atoms and predicates.

### Key Types

```cpp
// Output atom from neural activation
struct FSymbolicAtom {
    FString AtomID;             // Unique identifier
    FString AtomType;           // "Concept", "Action", "State", "Entity"
    float Confidence;           // 0-1 from neural activation
    int32 SourceFeatureIndex;   // Which neural feature produced this
    float ActivationValue;      // Raw activation that triggered creation
    TMap<FString, FString> Properties;
};

// Logical predicate derived from patterns
struct FPredicate {
    FString PredicateID;
    FString PredicateName;     // e.g., "IsActive", "HasProperty"
    TArray<FString> Arguments; // Atom IDs
    float TruthValue;          // 0-1
};
```

### Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `ActivationThreshold` | 0.3 | Minimum activation to create an atom |
| `ConfidenceThreshold` | 0.5 | Minimum confidence for output |
| `MaxAtomsPerTranslation` | 100 | Prevent explosion |
| `DiscretizationBins` | 10 | Categorical binning resolution |
| `bEnableBatchProcessing` | true | Batch efficiency mode |
| `BatchSize` | 32 | Items per batch |

### API

```cpp
// Translate a single activation tensor
TArray<FSymbolicAtom> TranslateTensor(const TArray<float>& Activations);

// Batch translate multiple tensors
TArray<FTranslationResult> TranslateBatch(const TArray<TArray<float>>& Batch);

// Generate predicates from atom patterns
TArray<FPredicate> GeneratePredicates(const TArray<FSymbolicAtom>& Atoms);
```

### Performance Targets
- Single translation: <0.5ms
- Batch processing: <0.3ms per item average

---

## F1.1.2: Symbolic-to-Neural Encoder

**Class:** `USymbolicToNeuralEncoder`  
**Purpose:** Encodes symbolic game state (entities, events, categories) into neural-processable embeddings.

### Key Types

```cpp
// Game entity for encoding
struct FGameEntityProperties {
    FString EntityID;
    FString EntityType;
    FVector Location;
    FVector Velocity;
    TMap<FString, FString> CategoricalProperties;
    TMap<FString, float> ContinuousProperties;
};

// Output embedding
struct FTensorEmbedding {
    TArray<float> Values;    // Dense vector
    int32 Dimensions;        // Vector size
    float Confidence;        // Encoding quality
};
```

### Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `EntityEmbeddingDim` | 128 | Entity embedding vector size |
| `EventEmbeddingDim` | 64 | Event embedding size |
| `bEnableStreamingMode` | true | Real-time incremental updates |
| `MaxStreamingBufferSize` | 1000 | Streaming buffer capacity |
| `CategoricalConfig.bUseOneHot` | false | Use learned vs one-hot encoding |
| `RelationalConfig.EncodingMethod` | Attention | Graph/Attention/Concatenation |
| `TemporalConfig.TemporalWindowSize` | 10 | Temporal context window |

### API

```cpp
// Encode a single entity
FTensorEmbedding EncodeEntity(const FGameEntityProperties& Entity);

// Encode categorical properties
TArray<float> EncodeCategorical(const FString& Category, const FString& Value);

// Encode continuous properties with normalization
TArray<float> EncodeContinuous(float Value, const FString& PropertyName);

// Streaming mode: update incrementally
void StreamUpdate(const FGameEntityProperties& Entity);
```

### Performance Targets
- Entity encoding: <0.5ms
- Event encoding: <0.3ms
- Streaming updates: <0.1ms

---

## F1.1.3: Bidirectional Message Protocol

**Class:** `UBidirectionalMessageProtocol`  
**Purpose:** Routes messages between subsystems with priority scheduling and topic-based pub/sub.

### Key Types

```cpp
enum class EMessageType : uint8 {
    Command, Query, Event, StateUpdate, Response
};

enum class EMessagePriority : uint8 {
    Critical, High, Normal, Low
};

struct FMessage {
    FString MessageID;
    EMessageType MessageType;
    EMessagePriority Priority;
    FString Topic;               // Pub/sub routing
    FString SenderID;
    TArray<float> PayloadTensor; // Neural data
    // ... symbolic payload fields
};
```

### Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `bEnablePriorityScheduling` | true | Priority-based delivery |
| `bEnableBatchProcessing` | true | Batch mode |
| `BatchSizeThreshold` | 32 | Batch trigger size |
| `CriticalQueueCapacity` | 256 | Critical priority queue |
| `HighQueueCapacity` | 512 | High priority queue |
| `NormalQueueCapacity` | 1024 | Normal priority queue |
| `LowQueueCapacity` | 2048 | Low priority queue |

### API

```cpp
// Send a message
bool SendMessage(const FMessage& Message);

// Subscribe to a topic
FString Subscribe(const FString& TopicPattern, const FString& SubscriberID, EMessagePriority MinPriority);

// Receive next message for subscriber
bool ReceiveMessage(FMessage& OutMessage);

// Get queue metrics
FMessageMetrics GetMetrics() const;
```

### Performance Targets
- Critical: <0.5ms latency (99th percentile)
- High: <1ms latency (99th percentile)
- Throughput: >10,000 messages/second

---

## F1.1.4: Synchronization Manager

**Class:** `UNeuralSymbolicSyncManager`  
**Purpose:** Coordinates timing and data consistency between neural and symbolic subsystems using double-buffered exchange aligned to the cognitive cycle.

### Key Types

```cpp
enum class ESyncPolicy : uint8 {
    Immediate,    // Sync on every write
    Batched,      // Fixed interval sync
    CycleAligned, // Sync at triadic points
    OnDemand      // Manual sync only
};

struct FNeuralSymbolicSyncConfig {
    ESyncPolicy SyncPolicy;
    float BatchIntervalSeconds;    // For Batched policy
    int32 MaxPendingUpdates;       // Overflow threshold
    bool bEnableDoubleBuffering;
    int32 NeuralBufferCapacity;
    int32 SymbolicBufferCapacity;
    int32 TriadSyncMask;           // Which triads trigger sync
    bool bSkipEmptySync;
    float SyncTimeoutMs;
};

struct FSyncMetrics {
    int64 TotalSyncCount;
    int64 TotalSwapCount;
    int64 SkippedSyncCount;
    float AverageSyncLatencyMs;
    float PeakSyncLatencyMs;
    float NeuralBufferUtilization;
    float SymbolicBufferUtilization;
    int64 ForcedSyncCount;
};
```

### Configuration

| Parameter | Default | Description |
|-----------|---------|-------------|
| `SyncPolicy` | CycleAligned | When to perform sync |
| `BatchIntervalSeconds` | 0.016 | ~60Hz for Batched mode |
| `MaxPendingUpdates` | 256 | Force sync threshold |
| `NeuralBufferCapacity` | 512 | Neural buffer size |
| `SymbolicBufferCapacity` | 512 | Symbolic buffer size |
| `TriadSyncMask` | 0x0F | All triads (bits 0-3) |
| `bSkipEmptySync` | true | Skip if no data changed |

### Triadic Sync Points

The cognitive cycle has 4 triads (cross-stream synchronization points):

| Triad | Steps | Mask Bit | Description |
|-------|-------|----------|-------------|
| Triad 1 | {1, 5, 9} | bit 0 | Early sync (perception aligned) |
| Triad 2 | {2, 6, 10} | bit 1 | Mid sync (action aligned) |
| Triad 3 | {3, 7, 11} | bit 2 | Late sync (reflection aligned) |
| Triad 4 | {4, 8, 12} | bit 3 | Boundary sync (cycle completion) |

### API

```cpp
// Lifecycle
void Initialize();
void Start();
void Stop();
void Reset();

// Neural buffer operations
bool SubmitNeuralActivation(const TArray<float>& Activation, float Timestamp);
int32 SubmitNeuralBatch(const TArray<TArray<float>>& Activations, const TArray<float>& Timestamps);
bool ReadNeuralBuffer(TArray<TArray<float>>& OutActivations, TArray<float>& OutTimestamps);
int32 GetPendingNeuralCount() const;

// Symbolic buffer operations
bool SubmitSymbolicUpdate(const FString& SymbolID, const FString& SymbolType, const TMap<FString, FString>& Properties);
int32 SubmitSymbolicBatch(const TArray<FString>& SymbolIDs, const TArray<FString>& SymbolTypes);
bool ReadSymbolicBuffer(TArray<FString>& OutIDs, TArray<FString>& OutTypes, TArray<TMap<FString, FString>>& OutProps);

// Sync control
bool ForceSync();
bool IsSyncPending() const;
bool IsAtSyncPoint() const;
int32 GetNextSyncPoint() const;
void SetSyncPolicy(ESyncPolicy NewPolicy);

// Metrics
FSyncMetrics GetMetrics() const;
float GetBufferUtilizationPercent() const;
bool IsBufferPressureHigh() const;
```

### Performance Targets
- Buffer swap: <0.1ms
- Sync latency (including swap): <0.5ms
- Memory: ~4MB per buffer pair at default capacity

---

## Integration Patterns

### Pattern 1: Neural → Symbolic Pipeline

```cpp
// Neural activation produced by reservoir
TArray<float> Activation = Reservoir->GetLatestActivation();

// Submit to sync manager's neural buffer
SyncManager->SubmitNeuralActivation(Activation, WorldTime);

// After sync (at triadic point), translate to symbols
TArray<TArray<float>> BufferedActivations;
TArray<float> Timestamps;
if (SyncManager->ReadNeuralBuffer(BufferedActivations, Timestamps))
{
    for (const auto& Act : BufferedActivations)
    {
        TArray<FSymbolicAtom> Atoms = Translator->TranslateTensor(Act);
        // Route via message protocol
        for (const auto& Atom : Atoms)
        {
            Protocol->SendMessage(CreateEventMessage(Atom));
        }
    }
}
```

### Pattern 2: Symbolic → Neural Pipeline

```cpp
// Game state change
FGameEntityProperties Entity = CaptureEntityState(Actor);

// Submit symbolic update
SyncManager->SubmitSymbolicUpdate(Entity.EntityID, Entity.EntityType, Entity.Properties);

// After sync, encode for neural processing
TArray<FString> IDs, Types;
TArray<TMap<FString, FString>> Props;
if (SyncManager->ReadSymbolicBuffer(IDs, Types, Props))
{
    for (int32 i = 0; i < IDs.Num(); i++)
    {
        FTensorEmbedding Embedding = Encoder->EncodeEntity(ReconstructEntity(IDs[i], Types[i], Props[i]));
        Reservoir->ProcessInput(Embedding.Values);
    }
}
```

### Pattern 3: Full Bidirectional Cycle

```cpp
void UMyComponent::TickComponent(float DeltaTime, ...)
{
    // 1. Submit neural outputs
    SyncManager->SubmitNeuralActivation(LatestNeural, GetWorld()->GetTimeSeconds());

    // 2. Submit symbolic state
    SyncManager->SubmitSymbolicUpdate("player", "Entity", PlayerProps);

    // 3. Sync happens automatically at triadic points
    //    (managed by SyncManager's TickComponent)

    // 4. Read results when available
    if (SyncManager->ReadNeuralBuffer(NeuralOut, Timestamps))
    {
        ProcessNeuralResults(NeuralOut);
    }
    if (SyncManager->ReadSymbolicBuffer(SymIDs, SymTypes, SymProps))
    {
        ProcessSymbolicResults(SymIDs, SymTypes, SymProps);
    }
}
```

---

## Component Wiring

All Phase 1.1 components should be attached to the same actor:

```cpp
// In your actor's constructor or BeginPlay
UNeuroSymbolicBridge* Bridge = CreateDefaultSubobject<UNeuroSymbolicBridge>("Bridge");
UNeuralToSymbolicTranslator* Translator = CreateDefaultSubobject<UNeuralToSymbolicTranslator>("Translator");
USymbolicToNeuralEncoder* Encoder = CreateDefaultSubobject<USymbolicToNeuralEncoder>("Encoder");
UBidirectionalMessageProtocol* Protocol = CreateDefaultSubobject<UBidirectionalMessageProtocol>("Protocol");
UNeuralSymbolicSyncManager* SyncManager = CreateDefaultSubobject<UNeuralSymbolicSyncManager>("SyncManager");
```

Components auto-discover each other via `FindComponentByClass<>()` during initialization.

---

## Error Handling

All buffer operations return `bool` indicating success:
- `false` from `Submit*` → buffer is full; sync or increase capacity
- `false` from `Read*` → no data available in read buffer; wait for next sync
- `false` from `ForceSync()` → sync was skipped (empty + `bSkipEmptySync`)

Monitor `IsBufferPressureHigh()` to detect approaching overflow. The system automatically forces a sync when `MaxPendingUpdates` is reached.

---

## Build & Test

### CMake (Standalone Tests)
```bash
mkdir build && cd build
cmake .. -DBUILD_TESTING=ON
cmake --build . -j$(nproc)
ctest --output-on-failure
```

### Unreal Engine (Full Suite)
1. Open `UnrealEngineCog.uproject`
2. Session Frontend → Automation
3. Run `DeepTreeEcho.SyncManager.*` tests

---

## Dependencies

| Dependency | Used By | Purpose |
|-----------|---------|---------|
| Eigen 3.3+ | F1.1.3, F1.1.4 | Linear algebra for type conversions |
| UE 5.x | All | Engine framework, actor components |
| C++17 | All | Language standard |
| GTest 1.14 | Tests | Standalone test framework |

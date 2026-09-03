# E1 Foundation API Reference

**Version:** 1.0.0  
**Last Updated:** 2026-03-02  
**Phase:** E1 - Foundation Layer  
**Status:** Production Ready

## Overview

This document provides comprehensive API documentation for all E1 Foundation components of the Deep Tree Echo cognitive framework. The E1 Foundation establishes the core infrastructure for embodied AI cognition, including reservoir computing, 4E cognition, memory systems, and cognitive cycle management.

---

## Table of Contents

1. [Core Components](#core-components)
2. [Reservoir Computing](#reservoir-computing)
3. [4E Embodied Cognition](#4e-embodied-cognition)
4. [Memory Systems](#memory-systems)
5. [Cognitive Cycle Management](#cognitive-cycle-management)
6. [Active Inference](#active-inference)
7. [Avatar System](#avatar-system)
8. [Wisdom & Meta-Cognition](#wisdom--meta-cognition)
9. [Data Structures](#data-structures)
10. [Configuration Reference](#configuration-reference)
11. [Error Handling](#error-handling)
12. [Examples](#examples)

---

## Core Components

### UDeepTreeEchoCore

**Location:** `DeepTreeEcho/Core/DeepTreeEchoCore.h`  
**Type:** UActorComponent  
**Purpose:** Central orchestrator for the Deep Tree Echo cognitive architecture

The core component integrates all cognitive subsystems and manages the 12-step cognitive cycle with 3 concurrent consciousness streams.

#### Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `bEnableCognitiveProcessing` | bool | true | Enable full cognitive processing |
| `bEnable4ECognition` | bool | true | Enable 4E embodied cognition |
| `bEnableRelevanceRealization` | bool | true | Enable relevance realization |
| `bEnableMemoryIntegration` | bool | true | Enable hypergraph memory integration |
| `bEnableBodySchemaBinding` | bool | true | Enable DNA body schema binding |
| `CycleDuration` | float | 12.0 | 12-step cognitive cycle duration (seconds) |
| `CurrentMode` | ECognitiveMode | Reactive | Current cognitive mode |
| `CurrentCycleStep` | int32 | 1 | Current cycle step (1-12) |
| `CurrentNestingLevel` | int32 | 1 | Current nesting level (1-4) |

#### Core Operations

```cpp
// Initialize the Deep Tree Echo system
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho")
void InitializeSystem();

// Process sensory input through the cognitive architecture
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho")
void ProcessSensoryInput(const TArray<float>& SensoryData, const FString& Modality);

// Generate action output
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho")
TArray<float> GenerateActionOutput();

// Set cognitive mode
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho")
void SetCognitiveMode(ECognitiveMode NewMode);
```

#### 4E Cognition API

```cpp
// Update embodied state
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|4E")
void UpdateEmbodiedState(
    const TArray<float>& ProprioceptiveData, 
    const TArray<float>& InteroceptiveData
);

// Update embedded state (environmental coupling)
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|4E")
void UpdateEmbeddedState(
    const TArray<FString>& Affordances, 
    const FString& Niche
);

// Update enacted state (sensorimotor loops)
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|4E")
void UpdateEnactedState(
    const TArray<FString>& Contingencies, 
    const TArray<float>& PredictionErrors
);

// Update extended state (cognitive scaffolding)
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|4E")
void UpdateExtendedState(
    const TArray<FString>& ExternalMemory, 
    const TArray<FString>& Tools
);

// Get 4E integration score (0.0 - 1.0)
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|4E")
float Get4EIntegrationScore() const;
```

#### Relevance Realization API

```cpp
// Update relevance frame
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Relevance")
void UpdateRelevanceFrame(const FString& Frame, const TArray<FString>& Constraints);

// Allocate attention to a target
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Relevance")
void AllocateAttention(const FString& Target, float Weight);

// Update salience landscape
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Relevance")
void UpdateSalienceLandscape(const TMap<FString, float>& SalienceUpdates);

// Check if at pivotal relevance realization step
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Relevance")
bool IsPivotalRelevanceStep() const;
```

#### Memory Integration API

```cpp
// Store experience in episodic memory
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Memory")
int64 StoreEpisodicMemory(const FString& Label, const TArray<float>& Embedding);

// Retrieve memories by similarity
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Memory")
TArray<int64> RetrieveSimilarMemories(
    const TArray<float>& QueryEmbedding, 
    float Threshold = 0.5f
);

// Create belief in intentional memory
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Memory")
int64 CreateBelief(const FString& Proposition, float Confidence = 0.7f);

// Create desire/goal in intentional memory
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Memory")
int64 CreateDesire(const FString& Goal, float Priority = 0.5f);

// Trigger memory consolidation
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Memory")
void TriggerMemoryConsolidation();
```

#### Cycle Management API

```cpp
// Get current cycle step (1-12)
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Cycle")
int32 GetCurrentCycleStep() const;

// Get current nesting level (1-4)
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Cycle")
int32 GetCurrentNestingLevel() const;

// Is this an expressive mode step (7 of 12)?
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Cycle")
bool IsExpressiveStep() const;

// Is this a reflective mode step (5 of 12)?
UFUNCTION(BlueprintCallable, Category = "DeepTreeEcho|Cycle")
bool IsReflectiveStep() const;
```

---

## Reservoir Computing

### UDeepTreeEchoReservoir

**Location:** `DeepTreeEcho/Reservoir/DeepTreeEchoReservoir.h`  
**Type:** UActorComponent  
**Purpose:** Echo State Network integration for temporal pattern recognition

#### Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `bEnableReservoirComputing` | bool | true | Enable reservoir computing |
| `EchoPropagationConfig` | FEchoPropagationConfig | — | Echo propagation configuration |
| `CycleDuration` | float | 12.0 | 12-step cognitive cycle duration |
| `HierarchicalReservoirs` | TArray<FReservoirState> | — | Hierarchical reservoir states |
| `CognitiveStreams` | TArray<FCognitiveStreamState> | — | Cognitive stream states (3 streams) |
| `DetectedPatterns` | TArray<FTemporalPattern> | — | Detected temporal patterns |
| `CurrentCycleStep` | int32 | 1 | Current cycle step (1-12) |

#### Reservoir Operations

```cpp
// Initialize hierarchical reservoirs
UFUNCTION(BlueprintCallable, Category = "Reservoir")
void InitializeReservoirs(int32 NumLevels = 3, int32 UnitsPerLevel = 100);

// Process input through reservoir hierarchy
UFUNCTION(BlueprintCallable, Category = "Reservoir")
TArray<float> ProcessInput(const TArray<float>& Input, int32 StreamID = 1);

// Get current reservoir state
UFUNCTION(BlueprintCallable, Category = "Reservoir")
FReservoirState GetReservoirState(int32 Level = 0) const;

// Detect temporal patterns in reservoir dynamics
UFUNCTION(BlueprintCallable, Category = "Reservoir")
TArray<FTemporalPattern> DetectTemporalPatterns();

// Propagate echo through hierarchy
UFUNCTION(BlueprintCallable, Category = "Reservoir")
void PropagateEcho(const TArray<float>& Activation, int32 SourceLevel = 0);
```

#### Cognitive Stream Operations

```cpp
// Update cognitive stream state
UFUNCTION(BlueprintCallable, Category = "Reservoir|Cognitive")
void UpdateCognitiveStream(int32 StreamID, const TArray<float>& Input);

// Get stream state
UFUNCTION(BlueprintCallable, Category = "Reservoir|Cognitive")
FCognitiveStreamState GetStreamState(int32 StreamID) const;

// Synchronize streams at triadic point
UFUNCTION(BlueprintCallable, Category = "Reservoir|Cognitive")
void SynchronizeStreams();

// Check if current step is triadic synchronization point
UFUNCTION(BlueprintCallable, Category = "Reservoir|Cognitive")
bool IsTriadicSyncPoint() const;
```

#### Memory Integration

```cpp
// Associate pattern with memory node
UFUNCTION(BlueprintCallable, Category = "Reservoir|Memory")
void AssociatePatternWithMemory(const FString& PatternID, const FString& MemoryNodeID);

// Retrieve patterns associated with memory
UFUNCTION(BlueprintCallable, Category = "Reservoir|Memory")
TArray<FTemporalPattern> GetPatternsForMemory(const FString& MemoryNodeID) const;

// Consolidate short-term patterns to long-term
UFUNCTION(BlueprintCallable, Category = "Reservoir|Memory")
void ConsolidatePatterns();
```

### UEchoStateNetwork

**Location:** `DeepTreeEcho/Reservoir/EchoStateNetwork.h`  
**Type:** UObject  
**Purpose:** Individual Echo State Network implementation

#### Key Methods

```cpp
// Initialize the ESN with given parameters
void Initialize(int32 InputSize, int32 ReservoirSize, int32 OutputSize);

// Process single input step
TArray<float> Process(const TArray<float>& Input);

// Train readout layer
void Train(const TArray<TArray<float>>& Inputs, const TArray<TArray<float>>& Targets);

// Get current reservoir state
TArray<float> GetReservoirState() const;

// Reset reservoir state
void Reset();
```

---

## 4E Embodied Cognition

### UEmbodiedCognitionComponent

**Location:** `DeepTreeEcho/4ECognition/EmbodiedCognitionComponent.h`  
**Type:** UActorComponent  
**Purpose:** Implements the four dimensions of embodied cognition

#### Properties

| Property | Type | Default | Description |
|----------|------|---------|-------------|
| `bEnableEmbodied` | bool | true | Enable embodied processing |
| `bEnableEmbedded` | bool | true | Enable embedded processing |
| `bEnableEnacted` | bool | true | Enable enacted processing |
| `bEnableExtended` | bool | true | Enable extended processing |
| `SensorimotorLearningRate` | float | 0.1 | Learning rate for sensorimotor contingencies |
| `BodySchema` | FBodySchema | — | Current body schema |
| `ActiveSomaticMarkers` | TArray<FSomaticMarker> | — | Active somatic markers |
| `DetectedAffordances` | TArray<FEnvironmentalAffordance> | — | Detected affordances |
| `LearnedContingencies` | TArray<FSensorimotorContingency> | — | Learned sensorimotor contingencies |
| `AvailableTools` | TArray<FCognitiveTool> | — | Available cognitive tools |

#### Embodied API

```cpp
// Update body schema from skeletal mesh
UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
void UpdateBodySchema(
    const TMap<FString, FVector>& BodyPartPositions, 
    const TMap<FString, FRotator>& BodyPartOrientations
);

// Add somatic marker
UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
void AddSomaticMarker(
    const FString& Stimulus, 
    const FString& BodyRegion, 
    float Valence, 
    float Arousal
);

// Get somatic marker for stimulus
UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
FSomaticMarker GetSomaticMarkerForStimulus(const FString& Stimulus) const;

// Check if position is in peripersonal space
UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
bool IsInPeripersonalSpace(const FVector& Position) const;
```

#### Embedded API

```cpp
// Detect affordances in environment
UFUNCTION(BlueprintCallable, Category = "4E|Embedded")
void DetectAffordances(const TArray<AActor*>& NearbyActors);

// Add affordance manually
UFUNCTION(BlueprintCallable, Category = "4E|Embedded")
void AddAffordance(
    const FString& Provider, 
    const FString& Type, 
    const FVector& Location, 
    float Salience
);

// Get most salient affordance
UFUNCTION(BlueprintCallable, Category = "4E|Embedded")
FEnvironmentalAffordance GetMostSalientAffordance() const;

// Set current environmental niche
UFUNCTION(BlueprintCallable, Category = "4E|Embedded")
void SetCurrentNiche(const FString& Niche);
```

#### Enacted API

```cpp
// Learn sensorimotor contingency from experience
UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
void LearnContingency(
    const FString& Action, 
    const FString& ExpectedOutcome, 
    const FString& ActualOutcome
);

// Predict outcome for proposed action
UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
FString PredictOutcome(const FString& Action) const;

// Get prediction error for last action
UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
float GetLastPredictionError() const;

// Update current action-perception state
UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
void UpdateActionPerceptionState(const FString& State);
```

#### Extended API

```cpp
// Register a cognitive tool
UFUNCTION(BlueprintCallable, Category = "4E|Extended")
void RegisterCognitiveTool(
    const FString& ToolName, 
    const FString& ToolType, 
    const FString& ExtendedFunction
);

// Activate cognitive tool
UFUNCTION(BlueprintCallable, Category = "4E|Extended")
void ActivateTool(const FString& ToolID);

// Deactivate cognitive tool
UFUNCTION(BlueprintCallable, Category = "4E|Extended")
void DeactivateTool(const FString& ToolID);

// Add external memory reference
UFUNCTION(BlueprintCallable, Category = "4E|Extended")
void AddExternalMemoryRef(const FString& Reference);

// Get tool integration level
UFUNCTION(BlueprintCallable, Category = "4E|Extended")
float GetToolIntegrationLevel(const FString& ToolID) const;
```

#### Integration Scores

```cpp
// Get overall 4E integration score
UFUNCTION(BlueprintCallable, Category = "4E")
float Get4EIntegrationScore() const;

// Get individual dimension scores
UFUNCTION(BlueprintCallable, Category = "4E")
float GetEmbodiedScore() const;

UFUNCTION(BlueprintCallable, Category = "4E")
float GetEmbeddedScore() const;

UFUNCTION(BlueprintCallable, Category = "4E")
float GetEnactedScore() const;

UFUNCTION(BlueprintCallable, Category = "4E")
float GetExtendedScore() const;
```

---

## Memory Systems

### UHypergraphMemorySystem

**Location:** `DeepTreeEcho/Memory/HypergraphMemorySystem.h`  
**Type:** UActorComponent  
**Purpose:** Hypergraph-based knowledge representation and memory

#### Key Methods

```cpp
// Create a node in the hypergraph
int64 CreateNode(const FString& NodeType, const FString& Name, const TMap<FString, FString>& Properties);

// Create a link between nodes
int64 CreateLink(const FString& LinkType, const TArray<int64>& NodeIDs, float TruthValue = 1.0f);

// Query nodes by type
TArray<int64> QueryNodesByType(const FString& NodeType);

// Query nodes by property
TArray<int64> QueryNodesByProperty(const FString& PropertyName, const FString& PropertyValue);

// Get node data
FHypergraphNode GetNode(int64 NodeID);

// Get link data
FHypergraphLink GetLink(int64 LinkID);

// Pattern matching query
TArray<FPatternMatch> PatternMatch(const FString& PatternSpec);

// Spread activation from source nodes
void SpreadActivation(const TArray<int64>& SourceNodes, float InitialActivation = 1.0f);
```

### UEpisodicMemorySystem

**Location:** `DeepTreeEcho/Memory/EpisodicMemorySystem.h`  
**Type:** UActorComponent  
**Purpose:** Episodic memory storage and retrieval

#### Key Methods

```cpp
// Store an episodic memory
int64 StoreEpisode(
    const FString& Label, 
    const TArray<float>& Embedding, 
    const FString& Context,
    float EmotionalValence = 0.0f
);

// Retrieve episode by ID
FEpisodicMemory GetEpisode(int64 EpisodeID);

// Query similar episodes
TArray<FEpisodicMemory> QuerySimilar(const TArray<float>& QueryEmbedding, int32 MaxResults = 10);

// Query episodes by time range
TArray<FEpisodicMemory> QueryByTimeRange(double StartTime, double EndTime);

// Consolidate memories (sleep-like process)
void ConsolidateMemories();

// Forget low-importance memories
void Forget(float ImportanceThreshold = 0.1f);
```

### UTemporalEventGraph

**Location:** `DeepTreeEcho/Memory/TemporalEventGraph.h`  
**Type:** UActorComponent  
**Purpose:** B-series rooted tree temporal event representation

#### Event Management

```cpp
// Create temporal event
int64 CreateEvent(FString Name, double StartTime, double EndTime, float Significance);

// Create point event
int64 CreatePointEvent(FString Name, double Time);

// Create interval event
int64 CreateIntervalEvent(FString Name, double StartTime, double Duration);

// Get event by ID
FTemporalEvent GetEvent(int64 EventID);

// Remove event
bool RemoveEvent(int64 EventID);

// Get all events
TArray<FTemporalEvent> GetAllEvents();

// Get events in time range
TArray<FTemporalEvent> GetEventsInRange(double StartTime, double EndTime);
```

#### Temporal Relations

```cpp
// Add temporal relation
int64 AddRelation(int64 SourceID, int64 TargetID, ETemporalRelation Relation, float Strength);

// Add before relation
int64 AddBeforeRelation(int64 EventA, int64 EventB, float Strength = 1.0f);

// Add after relation
int64 AddAfterRelation(int64 EventA, int64 EventB, float Strength = 1.0f);

// Add simultaneous relation
int64 AddSimultaneousRelation(int64 EventA, int64 EventB, float Strength = 1.0f);

// Get relation between events
ETemporalRelation GetRelation(int64 EventA, int64 EventB);

// Get predecessors
TArray<int64> GetPredecessors(int64 EventID);

// Get successors
TArray<int64> GetSuccessors(int64 EventID);

// Compute transitive closure
void ComputeTransitiveClosure();

// Infer temporal relations from timestamps
int32 InferTemporalRelations();
```

#### B-Series Trees

```cpp
// Generate rooted trees of given order
TArray<FRootedTree> GenerateRootedTrees(int32 Order);

// Get rooted tree count (A000081)
int32 GetRootedTreeCount(int32 Order);

// Create tree from notation
FRootedTree CreateTreeFromNotation(FString Notation);

// Get tree notation
FString GetTreeNotation(int64 TreeID);

// Compute B-series coefficient
float ComputeBSeriesCoefficient(int64 TreeID);

// Compute symmetry factor
int32 ComputeSymmetryFactor(int64 TreeID);

// Compute density
int32 ComputeDensity(int64 TreeID);
```

---

## Cognitive Cycle Management

### UCognitiveCycleManager

**Location:** `DeepTreeEcho/Core/CognitiveCycleManager.h`  
**Type:** UActorComponent  
**Purpose:** Manages the 12-step cognitive cycle with 3 concurrent streams

#### Architecture

The cognitive cycle operates on a 12-step loop with 3 concurrent consciousness streams phased 120° apart:

| Stream | Phase | Steps |
|--------|-------|-------|
| Stream 1 | Perceiving | 1, 4, 7, 10 |
| Stream 2 | Acting | 2, 5, 8, 11 |
| Stream 3 | Reflecting | 3, 6, 9, 12 |

Triadic synchronization points: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}

#### Key Methods

```cpp
// Initialize the cycle manager
void Initialize(float CycleDuration = 12.0f);

// Advance the cycle by delta time
void AdvanceCycle(float DeltaTime);

// Get current step for each stream
TArray<int32> GetStreamSteps() const;

// Get current step (global)
int32 GetCurrentStep() const;

// Check if at synchronization point
bool IsAtSyncPoint() const;

// Get stream at current step
int32 GetActiveStream() const;

// Register step callback
void RegisterStepCallback(int32 Step, TFunction<void()> Callback);
```

---

## Active Inference

### UActiveInferenceEngine

**Location:** `DeepTreeEcho/ActiveInference/ActiveInferenceEngine.h`  
**Type:** UActorComponent  
**Purpose:** Predictive processing and free energy minimization

#### Key Methods

```cpp
// Initialize with generative model
void Initialize(const FGenerativeModel& Model);

// Predict sensory observations
TArray<float> PredictObservations(const TArray<float>& HiddenStates);

// Compute prediction error
TArray<float> ComputePredictionError(
    const TArray<float>& Predicted, 
    const TArray<float>& Observed
);

// Update beliefs (infer hidden states)
TArray<float> UpdateBeliefs(const TArray<float>& Observations);

// Select action to minimize expected free energy
TArray<float> SelectAction(const TArray<TArray<float>>& PolicyOptions);

// Compute expected free energy for policy
float ComputeExpectedFreeEnergy(const TArray<float>& Policy);

// Get current free energy
float GetCurrentFreeEnergy() const;
```

### UNicheConstructionSystem

**Location:** `DeepTreeEcho/ActiveInference/NicheConstructionSystem.h`  
**Type:** UActorComponent  
**Purpose:** Environmental modification and niche sculpting

#### Key Methods

```cpp
// Identify modifiable environmental features
TArray<FNicheFeature> IdentifyModifiableFeatures();

// Plan niche modification
FNichePlan PlanModification(const FNicheFeature& Target, const FString& Goal);

// Execute modification
bool ExecuteModification(const FNichePlan& Plan);

// Evaluate niche fitness
float EvaluateNicheFitness();

// Get current niche state
FNicheState GetCurrentNicheState() const;
```

---

## Avatar System

### UAvatarEvolutionSystem

**Location:** `DeepTreeEcho/Avatar/AvatarEvolutionSystem.h`  
**Type:** UActorComponent  
**Purpose:** Ontogenetic development and avatar evolution

#### Key Methods

```cpp
// Initialize evolution system
void Initialize(const FAvatarConfig& Config);

// Advance developmental stage
void AdvanceDevelopment(float DeltaTime);

// Get current developmental stage
EAvatarDevelopmentStage GetCurrentStage() const;

// Evolve morphological features
void EvolveMorphology(const FMorphologyDelta& Delta);

// Adapt behavior patterns
void AdaptBehavior(const FBehaviorAdaptation& Adaptation);

// Get evolution metrics
FAvatarEvolutionMetrics GetEvolutionMetrics() const;
```

### UEmbodiedAvatarComponent

**Location:** `DeepTreeEcho/Avatar/EmbodiedAvatarComponent.h`  
**Type:** UActorComponent  
**Purpose:** Avatar-body coupling and expression

#### Key Methods

```cpp
// Synchronize avatar with body schema
void SyncWithBodySchema(const FBodySchema& Schema);

// Express emotional state
void ExpressEmotion(const FEmotionalState& Emotion);

// Apply facial expression
void ApplyFacialExpression(const FString& ExpressionName, float Intensity);

// Get current avatar state
FAvatarState GetAvatarState() const;

// Update from MetaHuman DNA
void UpdateFromDNA(const FDNAData& DNAData);
```

---

## Wisdom & Meta-Cognition

### UWisdomCultivation

**Location:** `DeepTreeEcho/Wisdom/WisdomCultivation.h`  
**Type:** UActorComponent  
**Purpose:** Higher-order cognitive processes and wisdom development

#### Key Methods

```cpp
// Evaluate decision from wisdom perspective
FWisdomEvaluation EvaluateDecision(const FDecisionContext& Context);

// Apply transformative experience
void ApplyTransformativeExperience(const FTransformativeEvent& Event);

// Get wisdom score
float GetWisdomScore() const;

// Cultivate virtue
void CultivateVirtue(const FString& VirtueName);

// Get virtue strengths
TMap<FString, float> GetVirtueStrengths() const;
```

### URelevanceRealizationEnnead

**Location:** `DeepTreeEcho/Wisdom/RelevanceRealizationEnnead.h`  
**Type:** UActorComponent  
**Purpose:** Nine-fold relevance realization framework

#### Key Methods

```cpp
// Realize relevance for situation
FRelevanceMap RealizeRelevance(const FSituationContext& Context);

// Apply ennead framework
FEnneadResult ApplyEnnead(const TArray<float>& Input);

// Get current relevance frame
FRelevanceFrame GetCurrentFrame() const;

// Transform frame
void TransformFrame(const FFrameTransformation& Transform);
```

---

## Data Structures

### Cognitive Mode Enumeration

```cpp
UENUM(BlueprintType)
enum class ECognitiveMode : uint8
{
    Reactive,      // Immediate stimulus-response
    Deliberative,  // Planned, goal-directed
    Reflective,    // Meta-cognitive, self-aware
    Creative,      // Novel pattern generation
    Integrative    // Cross-modal synthesis
};
```

### F4ECognitionState

```cpp
USTRUCT(BlueprintType)
struct F4ECognitionState
{
    // EMBODIED
    TArray<float> ProprioceptiveState;
    TArray<float> InteroceptiveState;
    float MotorReadiness;
    TMap<FString, float> SomaticMarkers;
    
    // EMBEDDED
    TArray<FString> DetectedAffordances;
    FString CurrentNiche;
    TMap<FString, float> SalienceMap;
    float EnvironmentCoupling;
    
    // ENACTED
    TArray<FString> SensorimotorContingencies;
    TArray<FString> ActiveInferencePredictions;
    TArray<float> PredictionErrors;
    float EnactiveEngagement;
    
    // EXTENDED
    TArray<FString> ExternalMemoryRefs;
    TArray<FString> ActiveTools;
    TArray<FString> SocialExtensions;
    float ExtensionIntegration;
};
```

### FReservoirState

```cpp
USTRUCT(BlueprintType)
struct FReservoirState
{
    FString ReservoirID;
    int32 Units;
    float SpectralRadius;
    float LeakRate;
    float InputScaling;
    TArray<float> ActivationState;
    bool bIsInitialized;
    float LastUpdateTime;
};
```

### FTemporalPattern

```cpp
USTRUCT(BlueprintType)
struct FTemporalPattern
{
    FString PatternID;
    FString PatternType;  // periodic, chaotic, transient
    float Frequency;
    float Strength;
    TArray<FString> AssociatedMemoryNodes;
    TArray<float> Signature;
};
```

### FEchoPropagationConfig

```cpp
USTRUCT(BlueprintType)
struct FEchoPropagationConfig
{
    bool bEnableHierarchicalEcho = true;
    int32 HierarchyLevels = 3;
    float EchoDecayFactor = 0.8;
    float CrossCouplingStrength = 0.1;
    bool bEnableIntrinsicPlasticity = true;
    float IntrinsicPlasticityRate = 0.001;
};
```

### FBodySchema

```cpp
USTRUCT(BlueprintType)
struct FBodySchema
{
    TMap<FString, FVector> BodyPartPositions;
    TMap<FString, FRotator> BodyPartOrientations;
    TMap<FString, float> JointAngles;
    float PeripersonalRadius;
    float SchemaCoherence;
};
```

### FSensorimotorContingency

```cpp
USTRUCT(BlueprintType)
struct FSensorimotorContingency
{
    FString ContingencyID;
    FString TriggeringAction;
    FString ExpectedOutcome;
    FString ActualOutcome;
    float PredictionError;
    float Strength;
};
```

### FEnvironmentalAffordance

```cpp
USTRUCT(BlueprintType)
struct FEnvironmentalAffordance
{
    FString AffordanceID;
    FString Provider;
    FString AffordanceType;
    FVector Location;
    float Salience;
    float Accessibility;
    FString RequiredCapability;
};
```

---

## Configuration Reference

### Reservoir Configuration

| Parameter | Type | Default | Range | Description |
|-----------|------|---------|-------|-------------|
| `Units` | int32 | 100 | 10-10000 | Reservoir size |
| `SpectralRadius` | float | 0.9 | 0.1-1.5 | Echo memory control |
| `LeakRate` | float | 0.3 | 0.0-1.0 | Temporal dynamics |
| `InputScaling` | float | 1.0 | 0.1-10.0 | Input amplitude |
| `HierarchyLevels` | int32 | 3 | 1-5 | Number of levels |
| `EchoDecayFactor` | float | 0.8 | 0.1-1.0 | Inter-level decay |
| `CrossCouplingStrength` | float | 0.1 | 0.0-1.0 | Cross-reservoir coupling |

### Cognitive Cycle Configuration

| Parameter | Type | Default | Range | Description |
|-----------|------|---------|-------|-------------|
| `CycleDuration` | float | 12.0 | 1.0-60.0 | Full cycle duration (s) |
| `NumStreams` | int32 | 3 | 1-12 | Consciousness streams |
| `SyncPointEnabled` | bool | true | — | Enable triadic sync |

### 4E Cognition Configuration

| Parameter | Type | Default | Range | Description |
|-----------|------|---------|-------|-------------|
| `SensorimotorLearningRate` | float | 0.1 | 0.0-1.0 | Contingency learning |
| `PeripersonalRadius` | float | 100.0 | 10-500 | Reachable space (cm) |
| `AffordanceSalienceDecay` | float | 0.95 | 0.5-1.0 | Salience decay rate |
| `ToolIntegrationRate` | float | 0.01 | 0.001-0.1 | Tool familiarity rate |

---

## Error Handling

### Common Error Codes

| Code | Description | Resolution |
|------|-------------|------------|
| `DTE_ERR_NOT_INITIALIZED` | System not initialized | Call `InitializeSystem()` first |
| `DTE_ERR_INVALID_STREAM` | Invalid stream ID | Use StreamID 1-3 |
| `DTE_ERR_INVALID_LEVEL` | Invalid hierarchy level | Check HierarchyLevels config |
| `DTE_ERR_MEMORY_FULL` | Memory capacity exceeded | Call `TriggerMemoryConsolidation()` |
| `DTE_ERR_PATTERN_NOT_FOUND` | Pattern ID not found | Verify pattern exists |

### Error Recovery

```cpp
// Check initialization
if (!bIsInitialized)
{
    InitializeSystem();
}

// Validate stream ID
if (StreamID < 1 || StreamID > 3)
{
    UE_LOG(LogDeepTreeEcho, Warning, TEXT("Invalid StreamID %d, defaulting to 1"), StreamID);
    StreamID = 1;
}

// Handle memory pressure
if (MemorySystem->IsNearCapacity())
{
    TriggerMemoryConsolidation();
}
```

---

## Examples

### Basic Initialization

```cpp
// Get the DeepTreeEcho core component
UDeepTreeEchoCore* DTE = GetOwner()->FindComponentByClass<UDeepTreeEchoCore>();

// Initialize the system
DTE->InitializeSystem();

// Process sensory input
TArray<float> SensoryData = {0.1f, 0.2f, 0.3f, 0.4f};
DTE->ProcessSensoryInput(SensoryData, "Visual");

// Generate action
TArray<float> Action = DTE->GenerateActionOutput();
```

### 4E Cognition Usage

```cpp
// Get 4E cognition component
UEmbodiedCognitionComponent* Cognition4E = GetOwner()->FindComponentByClass<UEmbodiedCognitionComponent>();

// Update body schema
TMap<FString, FVector> Positions;
Positions.Add("Head", FVector(0, 0, 180));
Positions.Add("LeftHand", FVector(-50, 0, 100));

TMap<FString, FRotator> Orientations;
Orientations.Add("Head", FRotator(0, 0, 0));

Cognition4E->UpdateBodySchema(Positions, Orientations);

// Add affordance
Cognition4E->AddAffordance("Chair_01", "Sittable", FVector(100, 50, 0), 0.8f);

// Learn contingency
Cognition4E->LearnContingency("Sit", "Resting", "Resting");

// Check integration score
float Score = Cognition4E->Get4EIntegrationScore();
```

### Reservoir Processing

```cpp
// Get reservoir component
UDeepTreeEchoReservoir* Reservoir = GetOwner()->FindComponentByClass<UDeepTreeEchoReservoir>();

// Initialize hierarchical reservoirs
Reservoir->InitializeReservoirs(3, 100);

// Process input through Stream 1 (Perceiving)
TArray<float> Input = {0.5f, 0.3f, 0.7f, 0.2f};
TArray<float> Output = Reservoir->ProcessInput(Input, 1);

// Detect temporal patterns
TArray<FTemporalPattern> Patterns = Reservoir->DetectTemporalPatterns();

// Check for triadic sync point
if (Reservoir->IsTriadicSyncPoint())
{
    Reservoir->SynchronizeStreams();
}
```

### Memory Integration

```cpp
// Store episodic memory
int64 MemoryID = DTE->StoreEpisodicMemory("FirstMeeting", Embedding);

// Create belief
int64 BeliefID = DTE->CreateBelief("Sky is blue", 0.95f);

// Create desire
int64 DesireID = DTE->CreateDesire("Learn new skill", 0.7f);

// Retrieve similar memories
TArray<int64> Similar = DTE->RetrieveSimilarMemories(QueryEmbedding, 0.5f);

// Trigger consolidation
DTE->TriggerMemoryConsolidation();
```

---

## Related Documentation

- [E1 Foundation Architecture](../architecture/E1_FOUNDATION_ARCHITECTURE.md)
- [RESERVOIRCPP_INTEGRATION_GUIDE.md](../../RESERVOIRCPP_INTEGRATION_GUIDE.md)
- [DOCUMENTATION_INDEX.md](../../DOCUMENTATION_INDEX.md)
- [ECHO_INTEGRATION_STATUS.md](../../ECHO_INTEGRATION_STATUS.md)

---

## Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0.0 | 2026-03-02 | Initial comprehensive API reference |

---

**Maintained by:** Deep Tree Echo Core Team  
**Feature:** F1.6.4 Documentation & API Reference  
**Contact:** Phase 1.6 - Documentation Team

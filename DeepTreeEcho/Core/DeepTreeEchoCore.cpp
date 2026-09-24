/**
 * Deep Tree Echo Core Implementation
 * 
 * Central integration point for the Deep Tree Echo cognitive system.
 * Implements the 12-step cognitive loop with 4E embodied cognition.
 */

#include "DeepTreeEchoCore.h"
#include "../../UnrealEcho/Cognitive/DeepTreeEchoCognitiveCore.h"
#include "../Reservoir/DeepTreeEchoReservoir.h"
#include "../../UnrealEcho/Consciousness/RecursiveMutualAwarenessSystem.h"
#include "../Memory/HypergraphMemorySystem.h"
#include "../Cognition4E/4ECognition/DNABodySchemaBinding.h"
#include "AutonomyPipeline.h"
#include "CognitiveCycleManager.h"
#include "Self/EchoSelf/EchoSelfIntegration.h"
#include "Self/Introspection/AutognosisSystem.h"
#include "Self/EchoSelf/ToroidalCognitiveAdapter.h"
#include "Self/EchoSelf/HypergraphBridgeAdapter.h"
#include "Self/EchoSelf/EchoSpaceMemoryBridge.h"
#include "Enaction/EngineSourceGripAdapter.h"
#include "Misc/Paths.h"

#include <string>

UDeepTreeEchoCore::UDeepTreeEchoCore()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.016f; // ~60 Hz

    EchoSelf = CreateDefaultSubobject<UEchoSelfIntegration>(TEXT("EchoSelf"));
    Autognosis = CreateDefaultSubobject<UAutognosisSystem>(TEXT("Autognosis"));
    Toroidal = CreateDefaultSubobject<UToroidalCognitiveAdapter>(TEXT("Toroidal"));
    HypergraphBridge = CreateDefaultSubobject<UHypergraphBridgeAdapter>(TEXT("Hypergraph"));
    EchoSpace = CreateDefaultSubobject<UEchoSpaceMemoryBridge>(TEXT("EchoSpace"));
}

void UDeepTreeEchoCore::BeginDestroy()
{
    delete AutonomyLoop;
    AutonomyLoop = nullptr;
    Super::BeginDestroy();
}

void UDeepTreeEchoCore::BeginPlay()
{
    Super::BeginPlay();

    // Find component references
    FindComponentReferences();

    // Initialize the system
    InitializeSystem();
}

void UDeepTreeEchoCore::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableCognitiveProcessing || !bIsInitialized)
    {
        return;
    }

    // Update the 12-step cognitive loop
    UpdateCognitiveLoop(DeltaTime);

    // Integrate 4E components
    if (bEnable4ECognition)
    {
        Integrate4EComponents();
    }

    // Update relevance realization
    if (bEnableRelevanceRealization)
    {
        UpdateRelevanceRealization();
    }

    // Update gestalt processing
    UpdateGestaltProcessing();

    // Update memory integration
    if (bEnableMemoryIntegration)
    {
        UpdateMemoryIntegration();
    }

    // Update body schema synchronization
    if (bEnableBodySchemaBinding)
    {
        UpdateBodySchemaSync();
    }

    TickAutonomyPipeline(DeltaTime);
}

void UDeepTreeEchoCore::InitializeSystem()
{
    // Initialize 4E cognition state
    CognitionState4E.ProprioceptiveState.SetNum(6); // 6 DOF
    CognitionState4E.InteroceptiveState.SetNum(4); // Basic interoceptive channels
    CognitionState4E.MotorReadiness = 0.5f;
    CognitionState4E.EnvironmentCoupling = 0.5f;
    CognitionState4E.EnactiveEngagement = 0.5f;
    CognitionState4E.ExtensionIntegration = 0.5f;

    // Initialize relevance state
    RelevanceState.RelevanceFrame = TEXT("Default");
    RelevanceState.RelevanceCoherence = 0.5f;
    RelevanceState.bIsPivotalStep = false;

    // Initialize gestalt state
    GestaltState.CurrentGestalt = TEXT("Undefined");
    GestaltState.GestaltCoherence = 0.5f;

    // Set initial mode
    CurrentMode = ECognitiveMode::Reactive;
    CurrentCycleStep = 1;
    CurrentNestingLevel = 1;

    if (!AutonomyLoop)
    {
        AutonomyLoop = new FAutonomyPipeline();
    }
    if (!AutonomyLoop->IsInitialized())
    {
        AutonomyLoop->Initialize();
    }
    if (Autognosis)
    {
        Autognosis->StartAutognosis();
    }

    bIsInitialized = true;
}

void UDeepTreeEchoCore::ProcessSensoryInput(const TArray<float>& SensoryData, const FString& Modality)
{
    if (!bIsInitialized)
    {
        return;
    }

    LastSensoryInput = SensoryData;

    // Route sensory data through reservoir system
    if (ReservoirSystem)
    {
        // Determine which stream based on modality
        int32 StreamID = 1; // Default to perceiving stream
        if (Modality == TEXT("Motor") || Modality == TEXT("Action"))
        {
            StreamID = 2; // Acting stream
        }
        else if (Modality == TEXT("Internal") || Modality == TEXT("Reflective"))
        {
            StreamID = 3; // Reflecting stream
        }

        ReservoirSystem->ProcessInput(SensoryData, StreamID);
    }

    // Update cognitive core memory if available
    if (CognitiveCore)
    {
        // Create memory node for this sensory event
        FString Content = FString::Printf(TEXT("Sensory_%s_%d"), *Modality, FMath::RandRange(0, 10000));
        // CognitiveCore->AddNode(Content, EMemoryType::Episodic);
    }

    // Update 4E embedded state with new sensory information
    if (bEnable4ECognition)
    {
        // Extract affordances from sensory data
        TArray<FString> NewAffordances;
        for (int32 i = 0; i < SensoryData.Num() && i < 5; ++i)
        {
            if (SensoryData[i] > 0.5f)
            {
                NewAffordances.Add(FString::Printf(TEXT("Affordance_%d"), i));
            }
        }
        
        if (NewAffordances.Num() > 0)
        {
            CognitionState4E.DetectedAffordances = NewAffordances;
        }
    }
}

TArray<float> UDeepTreeEchoCore::GenerateActionOutput()
{
    TArray<float> ActionOutput;

    if (!bIsInitialized)
    {
        return ActionOutput;
    }

    // Get output from acting stream (Stream 2)
    if (ReservoirSystem)
    {
        FCognitiveStreamState ActingStream = ReservoirSystem->GetStreamState(2);
        ActionOutput = ActingStream.ReservoirState.ActivationState;
    }

    // Modulate by motor readiness
    for (float& Val : ActionOutput)
    {
        Val *= CognitionState4E.MotorReadiness;
    }

    return ActionOutput;
}

void UDeepTreeEchoCore::SetCognitiveMode(ECognitiveMode NewMode)
{
    CurrentMode = NewMode;

    // Adjust processing parameters based on mode
    switch (NewMode)
    {
    case ECognitiveMode::Reactive:
        CognitionState4E.EnactiveEngagement = 0.8f;
        break;
    case ECognitiveMode::Deliberative:
        CognitionState4E.EnactiveEngagement = 0.5f;
        break;
    case ECognitiveMode::Reflective:
        CognitionState4E.EnactiveEngagement = 0.3f;
        break;
    case ECognitiveMode::Creative:
        CognitionState4E.EnactiveEngagement = 0.6f;
        break;
    case ECognitiveMode::Integrative:
        CognitionState4E.EnactiveEngagement = 0.7f;
        break;
    }
}

void UDeepTreeEchoCore::UpdateEmbodiedState(const TArray<float>& ProprioceptiveData, const TArray<float>& InteroceptiveData)
{
    // Update proprioceptive state
    if (ProprioceptiveData.Num() > 0)
    {
        CognitionState4E.ProprioceptiveState = ProprioceptiveData;
    }

    // Update interoceptive state
    if (InteroceptiveData.Num() > 0)
    {
        CognitionState4E.InteroceptiveState = InteroceptiveData;
    }

    // Compute motor readiness from interoceptive state
    float AvgIntero = 0.0f;
    for (float Val : CognitionState4E.InteroceptiveState)
    {
        AvgIntero += Val;
    }
    AvgIntero /= FMath::Max(1, CognitionState4E.InteroceptiveState.Num());
    
    // Motor readiness is high when interoceptive state is balanced (around 0.5)
    CognitionState4E.MotorReadiness = 1.0f - FMath::Abs(AvgIntero - 0.5f) * 2.0f;
}

void UDeepTreeEchoCore::UpdateEmbeddedState(const TArray<FString>& Affordances, const FString& Niche)
{
    CognitionState4E.DetectedAffordances = Affordances;
    CognitionState4E.CurrentNiche = Niche;

    // Compute environment coupling based on affordance count
    CognitionState4E.EnvironmentCoupling = FMath::Clamp(
        Affordances.Num() / 10.0f, 0.0f, 1.0f);
}

void UDeepTreeEchoCore::UpdateEnactedState(const TArray<FString>& Contingencies, const TArray<float>& PredictionErrors)
{
    CognitionState4E.SensorimotorContingencies = Contingencies;
    CognitionState4E.PredictionErrors = PredictionErrors;

    // Compute enactive engagement from prediction error
    float AvgError = 0.0f;
    for (float Err : PredictionErrors)
    {
        AvgError += FMath::Abs(Err);
    }
    AvgError /= FMath::Max(1, PredictionErrors.Num());

    // High engagement when prediction errors are moderate (learning opportunity)
    CognitionState4E.EnactiveEngagement = 1.0f - FMath::Abs(AvgError - 0.3f) * 2.0f;
    CognitionState4E.EnactiveEngagement = FMath::Clamp(CognitionState4E.EnactiveEngagement, 0.0f, 1.0f);
}

void UDeepTreeEchoCore::UpdateExtendedState(const TArray<FString>& ExternalMemory, const TArray<FString>& Tools)
{
    CognitionState4E.ExternalMemoryRefs = ExternalMemory;
    CognitionState4E.ActiveTools = Tools;

    // Compute extension integration
    int32 TotalExtensions = ExternalMemory.Num() + Tools.Num();
    CognitionState4E.ExtensionIntegration = FMath::Clamp(
        TotalExtensions / 5.0f, 0.0f, 1.0f);
}

float UDeepTreeEchoCore::Get4EIntegrationScore() const
{
    // Average of all 4E components
    return (CognitionState4E.MotorReadiness + 
            CognitionState4E.EnvironmentCoupling + 
            CognitionState4E.EnactiveEngagement + 
            CognitionState4E.ExtensionIntegration) / 4.0f;
}

void UDeepTreeEchoCore::UpdateRelevanceFrame(const FString& Frame, const TArray<FString>& Constraints)
{
    RelevanceState.RelevanceFrame = Frame;
    RelevanceState.FramingConstraints = Constraints;
}

void UDeepTreeEchoCore::AllocateAttention(const FString& Target, float Weight)
{
    RelevanceState.AttentionAllocation.Add(Target, Weight);

    // Normalize attention allocation
    float TotalWeight = 0.0f;
    for (const auto& Pair : RelevanceState.AttentionAllocation)
    {
        TotalWeight += Pair.Value;
    }

    if (TotalWeight > 1.0f)
    {
        for (auto& Pair : RelevanceState.AttentionAllocation)
        {
            Pair.Value /= TotalWeight;
        }
    }
}

void UDeepTreeEchoCore::UpdateSalienceLandscape(const TMap<FString, float>& SalienceUpdates)
{
    for (const auto& Update : SalienceUpdates)
    {
        RelevanceState.SalienceLandscape.Add(Update.Key, Update.Value);
    }
}

bool UDeepTreeEchoCore::IsPivotalRelevanceStep() const
{
    // Pivotal steps are at 1 and 7 (orienting present commitment)
    return CurrentCycleStep == 1 || CurrentCycleStep == 7;
}

void UDeepTreeEchoCore::UpdateGestalt(const FString& Gestalt, const FString& Figure, const FString& Ground)
{
    GestaltState.CurrentGestalt = Gestalt;
    GestaltState.Figure = Figure;
    GestaltState.Ground = Ground;
}

void UDeepTreeEchoCore::ApplyGestaltPrinciple(const FString& Principle)
{
    GestaltState.ActivePrinciples.AddUnique(Principle);
}

float UDeepTreeEchoCore::GetGestaltCoherence() const
{
    return GestaltState.GestaltCoherence;
}

bool UDeepTreeEchoCore::IsExpressiveStep() const
{
    // Expressive steps: 1, 2, 3, 4, 7, 8, 9 (7 of 12)
    // These are the affordance interaction and action steps
    return CurrentCycleStep <= 4 || (CurrentCycleStep >= 7 && CurrentCycleStep <= 9);
}

bool UDeepTreeEchoCore::IsReflectiveStep() const
{
    // Reflective steps: 5, 6, 10, 11, 12 (5 of 12)
    // These are the simulation and reflection steps
    return (CurrentCycleStep >= 5 && CurrentCycleStep <= 6) || CurrentCycleStep >= 10;
}

void UDeepTreeEchoCore::UpdateCognitiveLoop(float DeltaTime)
{
    CycleTimer += DeltaTime;

    float StepDuration = CycleDuration / 12.0f;

    if (CycleTimer >= StepDuration)
    {
        CycleTimer -= StepDuration;
        
        // Advance cycle step
        CurrentCycleStep = (CurrentCycleStep % 12) + 1;

        // Compute nesting level from step
        CurrentNestingLevel = ComputeNestingLevel(CurrentCycleStep);

        // Process at current nesting level
        ProcessAtNestingLevel(CurrentNestingLevel);

        // Update pivotal step flag
        RelevanceState.bIsPivotalStep = IsPivotalRelevanceStep();
    }
}

void UDeepTreeEchoCore::ProcessAtNestingLevel(int32 Level)
{
    // OEIS A000081 structure:
    // Level 1: 1 term (global context)
    // Level 2: 2 terms (local context)
    // Level 3: 4 terms (detailed processing)
    // Level 4: 9 terms (fine-grained)

    switch (Level)
    {
    case 1:
        // Global context processing
        // Update overall cognitive mode based on current state
        if (Get4EIntegrationScore() > 0.7f)
        {
            SetCognitiveMode(ECognitiveMode::Integrative);
        }
        break;

    case 2:
        // Local context processing
        // Balance between streams
        if (ReservoirSystem)
        {
            ReservoirSystem->SynchronizeStreams();
        }
        break;

    case 3:
        // Detailed processing
        // Pattern detection
        if (ReservoirSystem)
        {
            ReservoirSystem->DetectTemporalPatterns();
        }
        break;

    case 4:
        // Fine-grained processing
        // Memory consolidation
        if (ReservoirSystem)
        {
            ReservoirSystem->ConsolidatePatterns();
        }
        break;
    }
}

int32 UDeepTreeEchoCore::ComputeNestingLevel(int32 Step) const
{
    // Map 12 steps to 4 nesting levels
    // Steps 1, 5, 9 -> Level 1 (triadic sync points)
    // Steps 2, 6, 10 -> Level 2
    // Steps 3, 7, 11 -> Level 3
    // Steps 4, 8, 12 -> Level 4

    int32 StepInTriad = ((Step - 1) % 4) + 1;
    return StepInTriad;
}

void UDeepTreeEchoCore::Integrate4EComponents()
{
    // Cross-integrate 4E components

    // Embodied affects Enacted (body state influences action-perception)
    if (CognitionState4E.MotorReadiness > 0.7f)
    {
        CognitionState4E.EnactiveEngagement = FMath::Min(1.0f, 
            CognitionState4E.EnactiveEngagement + 0.1f);
    }

    // Embedded affects Extended (environment provides scaffolding)
    if (CognitionState4E.EnvironmentCoupling > 0.5f)
    {
        CognitionState4E.ExtensionIntegration = FMath::Min(1.0f,
            CognitionState4E.ExtensionIntegration + 0.05f);
    }

    // Enacted affects Embodied (actions update body state)
    for (float& Val : CognitionState4E.ProprioceptiveState)
    {
        Val = FMath::Lerp(Val, CognitionState4E.EnactiveEngagement, 0.1f);
    }
}

void UDeepTreeEchoCore::UpdateRelevanceRealization()
{
    // Update relevance coherence based on attention and salience alignment
    float Alignment = 0.0f;
    int32 Count = 0;

    for (const auto& AttentionPair : RelevanceState.AttentionAllocation)
    {
        if (const float* SaliencePtr = RelevanceState.SalienceLandscape.Find(AttentionPair.Key))
        {
            Alignment += 1.0f - FMath::Abs(AttentionPair.Value - *SaliencePtr);
            ++Count;
        }
    }

    if (Count > 0)
    {
        RelevanceState.RelevanceCoherence = Alignment / Count;
    }
}

void UDeepTreeEchoCore::UpdateGestaltProcessing()
{
    // Update gestalt coherence based on figure-ground clarity
    if (!GestaltState.Figure.IsEmpty() && !GestaltState.Ground.IsEmpty())
    {
        GestaltState.GestaltCoherence = FMath::Min(1.0f, 
            GestaltState.GestaltCoherence + 0.1f);
    }
    else
    {
        GestaltState.GestaltCoherence = FMath::Max(0.0f,
            GestaltState.GestaltCoherence - 0.05f);
    }
}

void UDeepTreeEchoCore::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        CognitiveCore = Owner->FindComponentByClass<UDeepTreeEchoCognitiveCore>();
        ReservoirSystem = Owner->FindComponentByClass<UDeepTreeEchoReservoir>();
        MutualAwarenessSystem = Owner->FindComponentByClass<URecursiveMutualAwarenessSystem>();
        MemorySystem = Owner->FindComponentByClass<UHypergraphMemorySystem>();
        BodySchemaBinding = Owner->FindComponentByClass<UDNABodySchemaBinding>();
        CycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
        if (!EchoSelf)
        {
            EchoSelf = Owner->FindComponentByClass<UEchoSelfIntegration>();
        }
        if (!Autognosis)
        {
            Autognosis = Owner->FindComponentByClass<UAutognosisSystem>();
        }
        if (!Toroidal)
        {
            Toroidal = Owner->FindComponentByClass<UToroidalCognitiveAdapter>();
        }
        if (!HypergraphBridge)
        {
            HypergraphBridge = Owner->FindComponentByClass<UHypergraphBridgeAdapter>();
        }
        if (!EchoSpace)
        {
            EchoSpace = Owner->FindComponentByClass<UEchoSpaceMemoryBridge>();
        }
    }
}

// ========================================
// MEMORY INTEGRATION IMPLEMENTATION
// ========================================

int64 UDeepTreeEchoCore::StoreEpisodicMemory(const FString& Label, const TArray<float>& Embedding)
{
    if (!MemorySystem)
    {
        return -1;
    }

    return MemorySystem->CreateNode(EMemoryNodeType::Episode, Label, Embedding, 0.7f);
}

TArray<int64> UDeepTreeEchoCore::RetrieveSimilarMemories(const TArray<float>& QueryEmbedding, float Threshold)
{
    if (!MemorySystem)
    {
        return TArray<int64>();
    }

    return MemorySystem->FindSimilarNodes(QueryEmbedding, Threshold, 10);
}

int64 UDeepTreeEchoCore::CreateBelief(const FString& Proposition, float Confidence)
{
    if (!MemorySystem)
    {
        return -1;
    }

    TArray<int64> EmptyContent;
    return MemorySystem->CreateBelief(Proposition, EmptyContent, EBeliefModality::Probable, Confidence);
}

int64 UDeepTreeEchoCore::CreateDesire(const FString& Goal, float Priority)
{
    if (!MemorySystem)
    {
        return -1;
    }

    TArray<int64> EmptyContent;
    return MemorySystem->CreateDesire(Goal, EmptyContent, 0.5f, Priority);
}

void UDeepTreeEchoCore::TriggerMemoryConsolidation()
{
    if (MemorySystem)
    {
        MemorySystem->RunConsolidation();
    }
}

// ========================================
// BODY SCHEMA IMPLEMENTATION
// ========================================

TArray<float> UDeepTreeEchoCore::GetBodyProprioceptiveState()
{
    if (BodySchemaBinding)
    {
        return BodySchemaBinding->GetProprioceptiveVector();
    }

    return CognitionState4E.ProprioceptiveState;
}

void UDeepTreeEchoCore::SyncBodySchemaToEmbodiedCognition()
{
    if (!BodySchemaBinding)
    {
        return;
    }

    // Get proprioceptive state from body schema
    TArray<float> ProprioVector = BodySchemaBinding->GetProprioceptiveVector();
    if (ProprioVector.Num() > 0)
    {
        CognitionState4E.ProprioceptiveState = ProprioVector;
    }

    // Update body measurements to motor readiness
    float ReachCapability = BodySchemaBinding->ComputePeripersonalRadius();
    CognitionState4E.MotorReadiness = FMath::Clamp(ReachCapability / 100.0f, 0.0f, 1.0f);
}

void UDeepTreeEchoCore::UpdateMemoryIntegration()
{
    if (!MemorySystem)
    {
        return;
    }

    // At reflective steps (5, 6, 10, 11, 12), consolidate memories
    if (IsReflectiveStep())
    {
        // Decay activations
        MemorySystem->DecayActivations(0.016f); // Assuming ~60Hz tick

        // At step 12 (end of cycle), run full consolidation
        if (CurrentCycleStep == 12)
        {
            MemorySystem->RunConsolidation();
        }
    }

    // At expressive steps, boost active memories
    if (IsExpressiveStep())
    {
        TArray<int64> ActiveNodes = MemorySystem->GetMostActiveNodes(5);
        for (int64 NodeID : ActiveNodes)
        {
            MemorySystem->BoostNodeActivation(NodeID, 0.05f);
        }
    }

    // Integrate reservoir patterns at nesting level 4
    if (CurrentNestingLevel == 4 && ReservoirSystem)
    {
        // Detect temporal patterns and integrate into memory
        TArray<FTemporalPattern> Patterns = ReservoirSystem->DetectTemporalPatterns();
        TArray<TArray<float>> PatternEmbeddings;
        for (const FTemporalPattern& Pattern : Patterns)
        {
            PatternEmbeddings.Add(Pattern.PatternEmbedding);
        }
        if (PatternEmbeddings.Num() > 0)
        {
            MemorySystem->IntegrateReservoirPatterns(PatternEmbeddings);
        }
    }
}

void UDeepTreeEchoCore::UpdateBodySchemaSync()
{
    if (!BodySchemaBinding)
    {
        return;
    }

    // Sync at perceiving steps (1, 4, 7, 10)
    if (CurrentCycleStep == 1 || CurrentCycleStep == 4 ||
        CurrentCycleStep == 7 || CurrentCycleStep == 10)
    {
        SyncBodySchemaToEmbodiedCognition();
    }

    // Encode body state to reservoir at acting steps
    if (ReservoirSystem && (CurrentCycleStep == 2 || CurrentCycleStep == 5 ||
                            CurrentCycleStep == 8 || CurrentCycleStep == 11))
    {
        TArray<float> BodyState = BodySchemaBinding->EncodeBodyState(2);
        if (BodyState.Num() > 0)
        {
            ReservoirSystem->ProcessInput(BodyState, 2); // Acting stream
        }
    }
}

void UDeepTreeEchoCore::TickAutonomyPipeline(float DeltaTime)
{
    if (!bEnableAutonomyPipeline || !AutonomyLoop || !AutonomyLoop->IsInitialized())
    {
        return;
    }

    Eigen::VectorXf Input(32);
    Input.setZero();
    const int32 N = FMath::Min(32, LastSensoryInput.Num());
    for (int32 i = 0; i < N; ++i)
    {
        Input(i) = LastSensoryInput[i];
    }

    AutonomyLoop->Tick(Input);
    ++AutonomyCycleCounter;

    const FPipelineTelemetry& Telemetry = AutonomyLoop->GetTelemetry();

    if (EchoSelf)
    {
        EchoSelf->RecordCycleMetrics(
            Telemetry.AARCoherence,
            Telemetry.MeanStreamEnergy,
            FMath::Clamp(Telemetry.ReservoirActivation, 0.0f, 1.0f),
            Telemetry.SelfModelAccuracy);
    }

    if (Autognosis)
    {
        Autognosis->IngestPipelineTelemetry(
            Telemetry.AARCoherence,
            Telemetry.SelfModelAccuracy,
            Telemetry.EchobeatStep,
            static_cast<int32>(Telemetry.AutognosisLevel));
    }

    if (Toroidal)
    {
        Toroidal->SyncToEchobeat(Telemetry.EchobeatStep, DeltaTime);
    }

    // Nest-4 reflective point: one identity ingest per 12-step echobeat.
    if (Telemetry.TotalCycles > 0 && (Telemetry.TotalCycles % 12) == 0)
    {
        if (HypergraphBridge)
        {
            HypergraphBridge->IngestCoreSelf(AutonomyLoop->GetCoreSelf());
        }
        if (EchoSpace)
        {
            EchoSpace->StoreIdentitySnapshot(AutonomyLoop->GetCoreSelf());
        }
        // Directory-level Engine overlay: nine catalog terms + unclassified count.
        // No Engine file walk. Missing Runtime/Core yields empty coverage.
        {
            const std::string RepoRoot = TCHAR_TO_UTF8(*FPaths::ProjectDir());
            const dte::engine_grip::FDirectoryTermSnapshot Snap =
                dte::engine_grip::InspectDirectoryTerms(RepoRoot);
            (void)Snap;
        }
    }

    // Level5/Level6 are N-cycle layers, not rival cores.
    if (Level5PulseInterval > 0 && Telemetry.TotalCycles > 0 &&
        (Telemetry.TotalCycles % Level5PulseInterval) == 0)
    {
        AutonomyLoop->ForceBackup(TEXT("Saved/DTE-Backups"));
    }

    if (Level6PulseInterval > 0 && Telemetry.TotalCycles > 0 &&
        (Telemetry.TotalCycles % Level6PulseInterval) == 0)
    {
        UE_LOG(LogTemp, Verbose,
            TEXT("DTE Core: Level6 N-cycle pulse at autonomy cycle %lld (echobeat %d)"),
            Telemetry.TotalCycles, Telemetry.EchobeatStep);
    }
}

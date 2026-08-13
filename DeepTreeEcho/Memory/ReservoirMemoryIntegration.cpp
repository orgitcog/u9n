// ReservoirMemoryIntegration.cpp
// Deep Tree Echo - Reservoir Computing Memory Integration Implementation
// Copyright (c) 2025 Deep Tree Echo Project

#include "ReservoirMemoryIntegration.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogReservoirMemory, Log, All);

// ========================================
// Constructor / Lifecycle
// ========================================

UReservoirMemoryIntegration::UReservoirMemoryIntegration()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    // Initialize working memory slots
    WorkingMemorySlots.SetNum(WorkingMemoryCapacity);
    for (int32 i = 0; i < WorkingMemoryCapacity; ++i)
    {
        WorkingMemorySlots[i].SlotIndex = i;
        WorkingMemorySlots[i].bIsActive = false;
    }
}

void UReservoirMemoryIntegration::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();

    // Resize working memory if capacity changed
    if (WorkingMemorySlots.Num() != WorkingMemoryCapacity)
    {
        WorkingMemorySlots.SetNum(WorkingMemoryCapacity);
        for (int32 i = 0; i < WorkingMemoryCapacity; ++i)
        {
            WorkingMemorySlots[i].SlotIndex = i;
        }
    }

    UE_LOG(LogReservoirMemory, Log, TEXT("Reservoir Memory Integration initialized with %d working memory slots"),
           WorkingMemoryCapacity);
}

void UReservoirMemoryIntegration::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        HypergraphMemory = Owner->FindComponentByClass<UHypergraphMemorySystem>();
        Reservoir = Owner->FindComponentByClass<UDeepTreeEchoReservoir>();

        if (!HypergraphMemory)
        {
            UE_LOG(LogReservoirMemory, Warning, TEXT("HypergraphMemorySystem not found on owner"));
        }
        if (!Reservoir)
        {
            UE_LOG(LogReservoirMemory, Warning, TEXT("DeepTreeEchoReservoir not found on owner"));
        }
    }
}

void UReservoirMemoryIntegration::TickComponent(float DeltaTime, ELevelTick TickType,
                                                 FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update working memory
    UpdateWorkingMemory(DeltaTime);

    // Update procedural execution
    if (bProceduralExecuting)
    {
        UpdateProceduralExecution(DeltaTime);
    }

    // Update memory replay
    if (bReplayActive)
    {
        UpdateReplay(DeltaTime);
    }

    // Run periodic consolidation
    if (bAutoConsolidation)
    {
        ConsolidationTimer += DeltaTime;
        if (ConsolidationTimer >= ConsolidationInterval)
        {
            RunConsolidationCycle();
            ConsolidationTimer = 0.0f;
        }
    }
}

// ========================================
// WORKING MEMORY IMPLEMENTATION
// ========================================

int32 UReservoirMemoryIntegration::AttendToMemory(int64 NodeID)
{
    if (!HypergraphMemory || !Reservoir)
    {
        return -1;
    }

    // Check if already in working memory
    for (int32 i = 0; i < WorkingMemorySlots.Num(); ++i)
    {
        if (WorkingMemorySlots[i].bIsActive && WorkingMemorySlots[i].MemoryNodeID == NodeID)
        {
            // Refresh existing slot
            RefreshWorkingMemory(i);
            return i;
        }
    }

    // Find free slot
    int32 SlotIndex = FindFreeWorkingMemorySlot();

    if (SlotIndex < 0)
    {
        // Working memory full - displace least important
        SlotIndex = FindLeastImportantSlot();
        ReleaseWorkingMemorySlot(SlotIndex);
    }

    // Load memory into slot
    FWorkingMemorySlot& Slot = WorkingMemorySlots[SlotIndex];
    Slot.MemoryNodeID = NodeID;
    Slot.bIsActive = true;
    Slot.DecayTimer = WorkingMemoryDecayRate;
    Slot.AttentionWeight = 0.5f;
    Slot.RefreshCount = 0;

    // Get current reservoir state as activation pattern
    Slot.ActivationPattern = GetCurrentReservoirState(0);

    // Associate with current cognitive stream based on cycle
    if (Reservoir)
    {
        int32 CycleStep = Reservoir->CurrentCycleStep;
        Slot.StreamAssociation = ((CycleStep - 1) % 3) + 1;
    }

    // Activate node in hypergraph
    HypergraphMemory->BoostNodeActivation(NodeID, 0.5f);

    OnWorkingMemoryUpdated.Broadcast(SlotIndex, NodeID);

    UE_LOG(LogReservoirMemory, Verbose, TEXT("Loaded node %lld into working memory slot %d"),
           NodeID, SlotIndex);

    return SlotIndex;
}

void UReservoirMemoryIntegration::ReleaseWorkingMemorySlot(int32 SlotIndex)
{
    if (SlotIndex < 0 || SlotIndex >= WorkingMemorySlots.Num())
    {
        return;
    }

    FWorkingMemorySlot& Slot = WorkingMemorySlots[SlotIndex];

    if (Slot.bIsActive)
    {
        // Check if should consolidate before release
        if (Slot.RefreshCount >= 3 || Slot.AttentionWeight > 0.7f)
        {
            // Strong enough pattern - consolidate
            TArray<int32> Slots;
            Slots.Add(SlotIndex);
            ConsolidateWorkingMemoryPattern(Slots);
        }

        Slot.bIsActive = false;
        Slot.MemoryNodeID = 0;
        Slot.ActivationPattern.Empty();
        Slot.AttentionWeight = 0.0f;
        Slot.RefreshCount = 0;
    }
}

void UReservoirMemoryIntegration::RefreshWorkingMemory(int32 SlotIndex)
{
    if (SlotIndex < 0 || SlotIndex >= WorkingMemorySlots.Num())
    {
        return;
    }

    FWorkingMemorySlot& Slot = WorkingMemorySlots[SlotIndex];

    if (Slot.bIsActive)
    {
        // Reset decay timer (rehearsal)
        Slot.DecayTimer = WorkingMemoryDecayRate;
        Slot.RefreshCount++;

        // Boost attention
        Slot.AttentionWeight = FMath::Min(1.0f, Slot.AttentionWeight + 0.1f);

        // Update activation pattern with current reservoir state
        TArray<float> CurrentState = GetCurrentReservoirState(Slot.StreamAssociation);
        if (CurrentState.Num() > 0 && Slot.ActivationPattern.Num() > 0)
        {
            // Blend old and new patterns
            for (int32 i = 0; i < FMath::Min(Slot.ActivationPattern.Num(), CurrentState.Num()); ++i)
            {
                Slot.ActivationPattern[i] = 0.7f * Slot.ActivationPattern[i] + 0.3f * CurrentState[i];
            }
        }

        // Strengthen in hypergraph
        if (HypergraphMemory)
        {
            HypergraphMemory->BoostNodeActivation(Slot.MemoryNodeID, 0.1f);
        }
    }
}

FWorkingMemorySlot UReservoirMemoryIntegration::GetWorkingMemorySlot(int32 SlotIndex) const
{
    if (SlotIndex >= 0 && SlotIndex < WorkingMemorySlots.Num())
    {
        return WorkingMemorySlots[SlotIndex];
    }
    return FWorkingMemorySlot();
}

TArray<FWorkingMemorySlot> UReservoirMemoryIntegration::GetActiveWorkingMemory() const
{
    TArray<FWorkingMemorySlot> ActiveSlots;

    for (const FWorkingMemorySlot& Slot : WorkingMemorySlots)
    {
        if (Slot.bIsActive)
        {
            ActiveSlots.Add(Slot);
        }
    }

    return ActiveSlots;
}

float UReservoirMemoryIntegration::GetWorkingMemoryLoad() const
{
    int32 ActiveCount = 0;
    for (const FWorkingMemorySlot& Slot : WorkingMemorySlots)
    {
        if (Slot.bIsActive)
        {
            ActiveCount++;
        }
    }
    return static_cast<float>(ActiveCount) / WorkingMemoryCapacity;
}

void UReservoirMemoryIntegration::FocusAttention(int32 SlotIndex, float Intensity)
{
    if (SlotIndex < 0 || SlotIndex >= WorkingMemorySlots.Num())
    {
        return;
    }

    FWorkingMemorySlot& Slot = WorkingMemorySlots[SlotIndex];

    if (Slot.bIsActive)
    {
        // Boost focused slot
        Slot.AttentionWeight = FMath::Clamp(Intensity, 0.0f, 1.0f);
        Slot.DecayTimer = WorkingMemoryDecayRate;

        // Reduce attention on other slots (limited capacity)
        float ReductionAmount = Intensity * 0.1f;
        for (int32 i = 0; i < WorkingMemorySlots.Num(); ++i)
        {
            if (i != SlotIndex && WorkingMemorySlots[i].bIsActive)
            {
                WorkingMemorySlots[i].AttentionWeight =
                    FMath::Max(0.1f, WorkingMemorySlots[i].AttentionWeight - ReductionAmount);
            }
        }

        // Inject pattern into reservoir
        if (Reservoir && Slot.ActivationPattern.Num() > 0)
        {
            InjectMemoryPattern(Slot.ActivationPattern, Slot.StreamAssociation);
        }
    }
}

void UReservoirMemoryIntegration::BindWorkingMemoryItems(int32 SlotA, int32 SlotB)
{
    if (SlotA < 0 || SlotA >= WorkingMemorySlots.Num() ||
        SlotB < 0 || SlotB >= WorkingMemorySlots.Num())
    {
        return;
    }

    FWorkingMemorySlot& SlotARef = WorkingMemorySlots[SlotA];
    FWorkingMemorySlot& SlotBRef = WorkingMemorySlots[SlotB];

    if (SlotARef.bIsActive && SlotBRef.bIsActive && HypergraphMemory)
    {
        // Create association in hypergraph
        HypergraphMemory->CreateEdge(SlotARef.MemoryNodeID, SlotBRef.MemoryNodeID,
                                      ESemanticRelation::ASSOCIATED_WITH, 0.8f);

        UE_LOG(LogReservoirMemory, Verbose, TEXT("Bound working memory items: %lld <-> %lld"),
               SlotARef.MemoryNodeID, SlotBRef.MemoryNodeID);
    }
}

void UReservoirMemoryIntegration::UpdateWorkingMemory(float DeltaTime)
{
    DecayWorkingMemorySlots(DeltaTime);
}

void UReservoirMemoryIntegration::DecayWorkingMemorySlots(float DeltaTime)
{
    for (FWorkingMemorySlot& Slot : WorkingMemorySlots)
    {
        if (Slot.bIsActive)
        {
            Slot.DecayTimer -= DeltaTime;

            // Decay attention weight
            Slot.AttentionWeight = FMath::Max(0.0f, Slot.AttentionWeight - DeltaTime * 0.05f);

            if (Slot.DecayTimer <= 0.0f)
            {
                // Memory decayed - release slot
                ReleaseWorkingMemorySlot(Slot.SlotIndex);
            }
        }
    }
}

int32 UReservoirMemoryIntegration::FindFreeWorkingMemorySlot()
{
    for (int32 i = 0; i < WorkingMemorySlots.Num(); ++i)
    {
        if (!WorkingMemorySlots[i].bIsActive)
        {
            return i;
        }
    }
    return -1;
}

int32 UReservoirMemoryIntegration::FindLeastImportantSlot()
{
    int32 LeastImportant = 0;
    float LowestScore = FLT_MAX;

    for (int32 i = 0; i < WorkingMemorySlots.Num(); ++i)
    {
        if (WorkingMemorySlots[i].bIsActive)
        {
            // Score based on attention, refresh count, and remaining decay time
            float Score = WorkingMemorySlots[i].AttentionWeight * 0.4f +
                         (WorkingMemorySlots[i].RefreshCount * 0.1f) +
                         (WorkingMemorySlots[i].DecayTimer / WorkingMemoryDecayRate) * 0.5f;

            if (Score < LowestScore)
            {
                LowestScore = Score;
                LeastImportant = i;
            }
        }
    }

    return LeastImportant;
}

// ========================================
// PROCEDURAL MEMORY IMPLEMENTATION
// ========================================

FString UReservoirMemoryIntegration::BeginProceduralLearning(const FString& SkillName)
{
    if (!bEnableProceduralLearning)
    {
        return FString();
    }

    FString TraceID = GenerateTraceID();

    FProceduralTrace Trace;
    Trace.TraceID = TraceID;
    Trace.Proficiency = 0.0f;
    Trace.PracticeCount = 0;
    Trace.LastPracticeTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    Trace.bIsExecuting = false;

    ProceduralTraces.Add(TraceID, Trace);

    UE_LOG(LogReservoirMemory, Log, TEXT("Started procedural learning: %s -> %s"), *SkillName, *TraceID);

    return TraceID;
}

void UReservoirMemoryIntegration::RecordProceduralStep(const FString& TraceID, const TArray<float>& MotorCommand)
{
    FProceduralTrace* Trace = ProceduralTraces.Find(TraceID);
    if (!Trace)
    {
        return;
    }

    // Get current reservoir state
    TArray<float> CurrentState = GetCurrentReservoirState(2); // Acting stream

    // Record state and timing
    Trace->SequenceStates.Add(CurrentState);

    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    if (Trace->StateTiming.Num() > 0)
    {
        float TimeSinceLast = CurrentTime - Trace->LastPracticeTime;
        Trace->StateTiming.Add(TimeSinceLast);
    }
    else
    {
        Trace->StateTiming.Add(0.0f);
    }
    Trace->LastPracticeTime = CurrentTime;

    UE_LOG(LogReservoirMemory, Verbose, TEXT("Recorded procedural step %d for trace %s"),
           Trace->SequenceStates.Num(), *TraceID);
}

int64 UReservoirMemoryIntegration::EndProceduralLearning(const FString& TraceID)
{
    FProceduralTrace* Trace = ProceduralTraces.Find(TraceID);
    if (!Trace || !HypergraphMemory)
    {
        return 0;
    }

    if (Trace->SequenceStates.Num() == 0)
    {
        ProceduralTraces.Remove(TraceID);
        return 0;
    }

    // Compute average pattern for the sequence
    TArray<float> AveragePattern = ComputePatternAverage(Trace->SequenceStates);

    // Create skill node in hypergraph
    int64 SkillNodeID = HypergraphMemory->CreateNode(EMemoryNodeType::Skill, TraceID, AveragePattern, 0.5f);
    Trace->SkillNodeID = SkillNodeID;

    // Initial proficiency based on sequence complexity
    Trace->Proficiency = 0.3f;
    Trace->PracticeCount = 1;

    OnProceduralLearned.Broadcast(TraceID, Trace->Proficiency);

    UE_LOG(LogReservoirMemory, Log, TEXT("Completed procedural learning: %s with %d steps, skill node %lld"),
           *TraceID, Trace->SequenceStates.Num(), SkillNodeID);

    return SkillNodeID;
}

bool UReservoirMemoryIntegration::ExecuteProceduralSequence(const FString& TraceID)
{
    FProceduralTrace* Trace = ProceduralTraces.Find(TraceID);
    if (!Trace || Trace->SequenceStates.Num() == 0)
    {
        return false;
    }

    if (bProceduralExecuting)
    {
        return false; // Already executing something
    }

    bProceduralExecuting = true;
    ExecutingTraceID = TraceID;
    Trace->bIsExecuting = true;
    Trace->CurrentStep = 0;
    ProceduralTimer = 0.0f;

    // Inject first state into reservoir
    InjectMemoryPattern(Trace->SequenceStates[0], 2); // Acting stream

    return true;
}

void UReservoirMemoryIntegration::UpdateProceduralExecution(float DeltaTime)
{
    FProceduralTrace* Trace = ProceduralTraces.Find(ExecutingTraceID);
    if (!Trace)
    {
        bProceduralExecuting = false;
        return;
    }

    ProceduralTimer += DeltaTime;

    // Check if time to advance to next step
    if (Trace->CurrentStep < Trace->StateTiming.Num())
    {
        float TargetTime = Trace->StateTiming[Trace->CurrentStep];

        if (ProceduralTimer >= TargetTime)
        {
            Trace->CurrentStep++;
            ProceduralTimer = 0.0f;

            if (Trace->CurrentStep < Trace->SequenceStates.Num())
            {
                // Inject next state
                InjectMemoryPattern(Trace->SequenceStates[Trace->CurrentStep], 2);
            }
            else
            {
                // Sequence complete
                Trace->bIsExecuting = false;
                bProceduralExecuting = false;

                // Increase proficiency through practice
                PracticeProceduralSequence(ExecutingTraceID);
            }
        }
    }
}

FProceduralTrace UReservoirMemoryIntegration::GetProceduralTrace(const FString& TraceID) const
{
    if (const FProceduralTrace* Trace = ProceduralTraces.Find(TraceID))
    {
        return *Trace;
    }
    return FProceduralTrace();
}

void UReservoirMemoryIntegration::PracticeProceduralSequence(const FString& TraceID)
{
    FProceduralTrace* Trace = ProceduralTraces.Find(TraceID);
    if (!Trace)
    {
        return;
    }

    Trace->PracticeCount++;
    Trace->LastPracticeTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

    // Increase proficiency with diminishing returns
    float ProficiencyGain = ProceduralLearningRate / FMath::Sqrt(static_cast<float>(Trace->PracticeCount));
    Trace->Proficiency = FMath::Min(1.0f, Trace->Proficiency + ProficiencyGain);

    // Strengthen skill node in hypergraph
    if (HypergraphMemory && Trace->SkillNodeID > 0)
    {
        HypergraphMemory->BoostNodeActivation(Trace->SkillNodeID, 0.1f);
    }

    OnProceduralLearned.Broadcast(TraceID, Trace->Proficiency);
}

TArray<FProceduralTrace> UReservoirMemoryIntegration::GetTracesForSkill(int64 SkillNodeID) const
{
    TArray<FProceduralTrace> Result;

    for (const auto& Pair : ProceduralTraces)
    {
        if (Pair.Value.SkillNodeID == SkillNodeID)
        {
            Result.Add(Pair.Value);
        }
    }

    return Result;
}

// ========================================
// EPISODIC MEMORY IMPLEMENTATION
// ========================================

int64 UReservoirMemoryIntegration::EncodeEpisode(const FString& Label, const TArray<float>& PerceptualContent)
{
    return EncodeEpisodeWithContext(Label, PerceptualContent, TArray<int64>());
}

int64 UReservoirMemoryIntegration::EncodeEpisodeWithContext(const FString& Label, const TArray<float>& Content,
                                                             const TArray<int64>& TemporalNeighbors)
{
    if (!HypergraphMemory || !Reservoir)
    {
        return 0;
    }

    // Process content through reservoir to get temporal embedding
    TArray<float> ReservoirEmbedding = Reservoir->ProcessInput(Content, 1); // Perceiving stream

    // Create episode node in hypergraph
    int64 EpisodeNodeID = HypergraphMemory->EncodeTemporalPattern(
        TArray<TArray<float>>({Content}), Label, 1);

    // Store episodic context
    FEpisodicContext Context;
    Context.EpisodeNodeID = EpisodeNodeID;
    Context.EncodingState = ReservoirEmbedding;
    Context.CycleStep = Reservoir->CurrentCycleStep;
    Context.TemporalNeighbors = TemporalNeighbors;
    Context.ContextCue = Content;
    Context.RetrievalStrength = 0.8f;

    // Capture all stream states
    for (int32 i = 1; i <= 3; ++i)
    {
        FCognitiveStreamState StreamState = Reservoir->GetStreamState(i);
        Context.StreamStates.Add(StreamState.ReservoirState.ActivationState);
    }

    EpisodicContexts.Add(EpisodeNodeID, Context);

    // Create temporal edges to neighbors
    for (int64 NeighborID : TemporalNeighbors)
    {
        HypergraphMemory->CreateEdge(EpisodeNodeID, NeighborID, ESemanticRelation::AFTER, 0.9f);
    }

    OnEpisodicEncoded.Broadcast(EpisodeNodeID, Context.CycleStep);

    UE_LOG(LogReservoirMemory, Log, TEXT("Encoded episode %lld: %s at cycle step %d"),
           EpisodeNodeID, *Label, Context.CycleStep);

    return EpisodeNodeID;
}

TArray<int64> UReservoirMemoryIntegration::RetrieveEpisodesByCue(const TArray<float>& Cue, float Threshold)
{
    TArray<int64> Results;

    if (!Reservoir)
    {
        return Results;
    }

    // Process cue through reservoir
    TArray<float> CueEmbedding = Reservoir->ProcessInput(Cue, 1);

    // Find episodes with similar encoding states
    for (const auto& Pair : EpisodicContexts)
    {
        float Similarity = ComputePatternSimilarity(CueEmbedding, Pair.Value.EncodingState);

        if (Similarity >= Threshold)
        {
            Results.Add(Pair.Key);

            // Boost activation in hypergraph
            if (HypergraphMemory)
            {
                HypergraphMemory->BoostNodeActivation(Pair.Key, Similarity * 0.3f);
            }
        }
    }

    return Results;
}

TArray<int64> UReservoirMemoryIntegration::RetrieveEpisodesByTime(double StartTime, double EndTime)
{
    TArray<int64> Results;

    if (!HypergraphMemory)
    {
        return Results;
    }

    // Use hypergraph time-based query
    TArray<int64> Episodes = HypergraphMemory->FindNodesByType(EMemoryNodeType::Episode, 100);

    for (int64 EpisodeID : Episodes)
    {
        FMemoryNode Node = HypergraphMemory->GetNode(EpisodeID);
        if (Node.CreationTime >= StartTime && Node.CreationTime <= EndTime)
        {
            Results.Add(EpisodeID);
        }
    }

    return Results;
}

FEpisodicContext UReservoirMemoryIntegration::GetEpisodicContext(int64 EpisodeNodeID) const
{
    if (const FEpisodicContext* Context = EpisodicContexts.Find(EpisodeNodeID))
    {
        return *Context;
    }
    return FEpisodicContext();
}

bool UReservoirMemoryIntegration::ReconstructEpisode(int64 EpisodeNodeID)
{
    const FEpisodicContext* Context = EpisodicContexts.Find(EpisodeNodeID);
    if (!Context || !Reservoir)
    {
        return false;
    }

    // Start replay of episodic sequence
    return StartMemoryReplay(EpisodeNodeID, EMemoryNodeType::Episode);
}

// ========================================
// SEMANTIC MEMORY IMPLEMENTATION
// ========================================

int64 UReservoirMemoryIntegration::FormSemanticAttractor(const TArray<float>& PatternCenter,
                                                          const FString& ConceptLabel)
{
    if (!HypergraphMemory)
    {
        return 0;
    }

    // Create concept node
    int64 ConceptNodeID = HypergraphMemory->CreateNode(EMemoryNodeType::Concept,
                                                        ConceptLabel, PatternCenter, 0.7f);

    // Create semantic attractor
    FSemanticAttractor Attractor;
    Attractor.ConceptNodeID = ConceptNodeID;
    Attractor.AttractorCenter = PatternCenter;
    Attractor.BasinRadius = 0.5f;
    Attractor.Stability = 0.5f;
    Attractor.ActivationThreshold = 0.3f;
    Attractor.AccessCount = 1;

    SemanticAttractors.Add(ConceptNodeID, Attractor);

    OnSemanticAttractorFormed.Broadcast(ConceptNodeID, Attractor.Stability);

    UE_LOG(LogReservoirMemory, Log, TEXT("Formed semantic attractor %lld: %s"),
           ConceptNodeID, *ConceptLabel);

    return ConceptNodeID;
}

bool UReservoirMemoryIntegration::IsInAttractorBasin(const TArray<float>& State, int64 ConceptNodeID) const
{
    const FSemanticAttractor* Attractor = SemanticAttractors.Find(ConceptNodeID);
    if (!Attractor)
    {
        return false;
    }

    float Distance = 1.0f - ComputePatternSimilarity(State, Attractor->AttractorCenter);
    return Distance < Attractor->BasinRadius;
}

int64 UReservoirMemoryIntegration::FindNearestAttractor(const TArray<float>& State)
{
    int64 NearestID = 0;
    float BestSimilarity = -1.0f;

    for (const auto& Pair : SemanticAttractors)
    {
        float Similarity = ComputePatternSimilarity(State, Pair.Value.AttractorCenter);

        if (Similarity > BestSimilarity && Similarity >= Pair.Value.ActivationThreshold)
        {
            BestSimilarity = Similarity;
            NearestID = Pair.Key;
        }
    }

    // Update access count if found
    if (NearestID > 0)
    {
        FSemanticAttractor* Attractor = SemanticAttractors.Find(NearestID);
        if (Attractor)
        {
            Attractor->AccessCount++;
            Attractor->Stability = FMath::Min(1.0f, Attractor->Stability + 0.01f);
        }
    }

    return NearestID;
}

FSemanticAttractor UReservoirMemoryIntegration::GetSemanticAttractor(int64 ConceptNodeID) const
{
    if (const FSemanticAttractor* Attractor = SemanticAttractors.Find(ConceptNodeID))
    {
        return *Attractor;
    }
    return FSemanticAttractor();
}

void UReservoirMemoryIntegration::ConnectAttractors(int64 ConceptA, int64 ConceptB, float Strength)
{
    FSemanticAttractor* AttractorA = SemanticAttractors.Find(ConceptA);
    FSemanticAttractor* AttractorB = SemanticAttractors.Find(ConceptB);

    if (AttractorA && AttractorB)
    {
        AttractorA->ConnectedAttractors.AddUnique(ConceptB);
        AttractorB->ConnectedAttractors.AddUnique(ConceptA);

        // Create edge in hypergraph
        if (HypergraphMemory)
        {
            HypergraphMemory->CreateBidirectionalEdge(ConceptA, ConceptB,
                                                       ESemanticRelation::SIMILAR_TO, Strength);
        }
    }
}

TArray<int64> UReservoirMemoryIntegration::TraverseSemanticSpace(int64 StartConcept, int64 EndConcept)
{
    if (!HypergraphMemory)
    {
        return TArray<int64>();
    }

    // Use hypergraph pathfinding
    TArray<ESemanticRelation> AllowedRelations = {ESemanticRelation::SIMILAR_TO, ESemanticRelation::IS_A};
    return HypergraphMemory->FindPath(StartConcept, EndConcept, 5, AllowedRelations);
}

// ========================================
// CONSOLIDATION IMPLEMENTATION
// ========================================

void UReservoirMemoryIntegration::RunConsolidationCycle()
{
    if (!HypergraphMemory || !Reservoir)
    {
        return;
    }

    // Detect patterns in reservoir
    TArray<FTemporalPattern> Patterns = Reservoir->DetectTemporalPatterns();

    // Consolidate strong patterns
    for (const FTemporalPattern& Pattern : Patterns)
    {
        if (Pattern.Strength >= ConsolidationThreshold)
        {
            // Form semantic attractor from pattern
            int64 ConceptID = FormSemanticAttractor(Pattern.Signature,
                FString::Printf(TEXT("ConsolidatedPattern_%s"), *Pattern.PatternID));

            // Log consolidation event
            FConsolidationEvent Event;
            Event.TargetNodeID = ConceptID;
            Event.ConsolidationType = TEXT("PatternToAttractor");
            Event.PatternStrength = Pattern.Strength;
            Event.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

            ConsolidationHistory.Add(Event);
            OnConsolidationComplete.Broadcast(Event);
        }
    }

    // Run hypergraph consolidation
    HypergraphMemory->RunConsolidation();

    // Consolidate reservoir patterns
    Reservoir->ConsolidatePatterns();

    UE_LOG(LogReservoirMemory, Verbose, TEXT("Consolidation cycle complete: %d patterns processed"),
           Patterns.Num());
}

int64 UReservoirMemoryIntegration::ConsolidateWorkingMemoryPattern(const TArray<int32>& SlotIndices)
{
    if (SlotIndices.Num() == 0 || !HypergraphMemory)
    {
        return 0;
    }

    // Collect patterns from slots
    TArray<TArray<float>> Patterns;
    TArray<int64> SourceNodes;

    for (int32 SlotIndex : SlotIndices)
    {
        if (SlotIndex >= 0 && SlotIndex < WorkingMemorySlots.Num())
        {
            const FWorkingMemorySlot& Slot = WorkingMemorySlots[SlotIndex];
            if (Slot.bIsActive && Slot.ActivationPattern.Num() > 0)
            {
                Patterns.Add(Slot.ActivationPattern);
                SourceNodes.Add(Slot.MemoryNodeID);
            }
        }
    }

    if (Patterns.Num() == 0)
    {
        return 0;
    }

    // Compute average pattern
    TArray<float> AveragePattern = ComputePatternAverage(Patterns);

    // Create consolidated concept
    int64 ConsolidatedID = HypergraphMemory->CreateNode(EMemoryNodeType::Schema,
        TEXT("WorkingMemoryConsolidation"), AveragePattern, 0.6f);

    // Link to source nodes
    for (int64 SourceID : SourceNodes)
    {
        HypergraphMemory->CreateEdge(ConsolidatedID, SourceID,
                                      ESemanticRelation::DERIVED_FROM, 0.8f);
    }

    // Log event
    FConsolidationEvent Event;
    Event.SourceNodes = SourceNodes;
    Event.TargetNodeID = ConsolidatedID;
    Event.ConsolidationType = TEXT("WorkingMemoryConsolidation");
    Event.PatternStrength = 0.6f;
    Event.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

    ConsolidationHistory.Add(Event);
    OnConsolidationComplete.Broadcast(Event);

    return ConsolidatedID;
}

TArray<FConsolidationEvent> UReservoirMemoryIntegration::GetRecentConsolidations(int32 Count) const
{
    TArray<FConsolidationEvent> Recent;

    int32 StartIndex = FMath::Max(0, ConsolidationHistory.Num() - Count);
    for (int32 i = StartIndex; i < ConsolidationHistory.Num(); ++i)
    {
        Recent.Add(ConsolidationHistory[i]);
    }

    return Recent;
}

void UReservoirMemoryIntegration::TriggerOfflineConsolidation(float Duration)
{
    // Intensive consolidation (simulating sleep)
    float EndTime = GetWorld() ? GetWorld()->GetTimeSeconds() + Duration : Duration;

    // Run multiple consolidation cycles
    int32 Cycles = FMath::CeilToInt(Duration / 5.0f);
    for (int32 i = 0; i < Cycles; ++i)
    {
        RunConsolidationCycle();
    }

    UE_LOG(LogReservoirMemory, Log, TEXT("Completed offline consolidation: %d cycles"), Cycles);
}

// ========================================
// MEMORY REPLAY IMPLEMENTATION
// ========================================

bool UReservoirMemoryIntegration::StartMemoryReplay(int64 NodeID, EMemoryNodeType MemoryType)
{
    if (bReplayActive)
    {
        return false;
    }

    // Get sequence to replay based on memory type
    ReplaySequence.Empty();

    if (MemoryType == EMemoryNodeType::Episode)
    {
        const FEpisodicContext* Context = EpisodicContexts.Find(NodeID);
        if (Context)
        {
            // Replay stream states
            ReplaySequence = Context->StreamStates;
            if (ReplaySequence.Num() == 0)
            {
                ReplaySequence.Add(Context->EncodingState);
            }
        }
    }
    else if (MemoryType == EMemoryNodeType::Skill)
    {
        // Find procedural trace for this skill
        for (const auto& Pair : ProceduralTraces)
        {
            if (Pair.Value.SkillNodeID == NodeID)
            {
                ReplaySequence = Pair.Value.SequenceStates;
                break;
            }
        }
    }

    if (ReplaySequence.Num() == 0)
    {
        return false;
    }

    bReplayActive = true;
    ReplayNodeID = NodeID;
    ReplayType = MemoryType;
    ReplayProgress = 0.0f;
    ReplayCurrentStep = 0;

    OnMemoryReplayStarted.Broadcast(NodeID, UEnum::GetValueAsString(MemoryType));

    UE_LOG(LogReservoirMemory, Log, TEXT("Started memory replay for node %lld, %d steps"),
           NodeID, ReplaySequence.Num());

    return true;
}

void UReservoirMemoryIntegration::StopMemoryReplay()
{
    bReplayActive = false;
    ReplaySequence.Empty();
    ReplayProgress = 0.0f;
}

bool UReservoirMemoryIntegration::IsReplayActive() const
{
    return bReplayActive;
}

float UReservoirMemoryIntegration::GetReplayProgress() const
{
    return ReplayProgress;
}

void UReservoirMemoryIntegration::UpdateReplay(float DeltaTime)
{
    if (!bReplayActive || ReplaySequence.Num() == 0)
    {
        return;
    }

    // Advance replay
    float StepDuration = 0.5f / ReplayConfig.SpeedMultiplier;

    ReplayProgress += DeltaTime / (StepDuration * ReplaySequence.Num());

    int32 NewStep = FMath::FloorToInt(ReplayProgress * ReplaySequence.Num());

    if (NewStep != ReplayCurrentStep && NewStep < ReplaySequence.Num())
    {
        ReplayCurrentStep = NewStep;

        // Inject replay state into reservoir
        InjectMemoryPattern(ReplaySequence[ReplayCurrentStep], ReplayConfig.TargetStream);
    }

    if (ReplayProgress >= 1.0f)
    {
        StopMemoryReplay();
    }
}

// ========================================
// RESERVOIR INTEGRATION IMPLEMENTATION
// ========================================

TArray<float> UReservoirMemoryIntegration::GetCurrentReservoirState(int32 StreamID) const
{
    if (!Reservoir)
    {
        return TArray<float>();
    }

    if (StreamID == 0)
    {
        // Return base reservoir state
        FReservoirState State = Reservoir->GetReservoirState(0);
        return State.ActivationState;
    }
    else if (StreamID >= 1 && StreamID <= 3)
    {
        FCognitiveStreamState StreamState = Reservoir->GetStreamState(StreamID);
        return StreamState.ReservoirState.ActivationState;
    }

    return TArray<float>();
}

void UReservoirMemoryIntegration::InjectMemoryPattern(const TArray<float>& Pattern, int32 StreamID)
{
    if (!Reservoir || Pattern.Num() == 0)
    {
        return;
    }

    // Process pattern through reservoir (updates state)
    TArray<float> ScaledPattern = Pattern;
    for (float& Val : ScaledPattern)
    {
        Val *= ReplayConfig.ReplayIntensity;
    }

    Reservoir->ProcessInput(ScaledPattern, StreamID);
}

float UReservoirMemoryIntegration::GetPatternSimilarity(const TArray<float>& Pattern) const
{
    TArray<float> CurrentState = GetCurrentReservoirState(0);
    return ComputePatternSimilarity(Pattern, CurrentState);
}

int64 UReservoirMemoryIntegration::DetectAttractorBasin() const
{
    TArray<float> CurrentState = GetCurrentReservoirState(0);

    for (const auto& Pair : SemanticAttractors)
    {
        if (IsInAttractorBasin(CurrentState, Pair.Key))
        {
            return Pair.Key;
        }
    }

    return 0;
}

// ========================================
// HELPER METHODS
// ========================================

float UReservoirMemoryIntegration::ComputePatternSimilarity(const TArray<float>& A,
                                                             const TArray<float>& B) const
{
    if (A.Num() == 0 || B.Num() == 0)
    {
        return 0.0f;
    }

    // Cosine similarity
    float DotProduct = 0.0f;
    float NormA = 0.0f;
    float NormB = 0.0f;

    int32 MinLen = FMath::Min(A.Num(), B.Num());

    for (int32 i = 0; i < MinLen; ++i)
    {
        DotProduct += A[i] * B[i];
        NormA += A[i] * A[i];
        NormB += B[i] * B[i];
    }

    NormA = FMath::Sqrt(NormA);
    NormB = FMath::Sqrt(NormB);

    if (NormA < KINDA_SMALL_NUMBER || NormB < KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    return DotProduct / (NormA * NormB);
}

TArray<float> UReservoirMemoryIntegration::ComputePatternAverage(const TArray<TArray<float>>& Patterns) const
{
    if (Patterns.Num() == 0)
    {
        return TArray<float>();
    }

    int32 Dimension = Patterns[0].Num();
    TArray<float> Average;
    Average.SetNumZeroed(Dimension);

    for (const TArray<float>& Pattern : Patterns)
    {
        for (int32 i = 0; i < FMath::Min(Dimension, Pattern.Num()); ++i)
        {
            Average[i] += Pattern[i];
        }
    }

    for (float& Val : Average)
    {
        Val /= Patterns.Num();
    }

    return Average;
}

FString UReservoirMemoryIntegration::GenerateTraceID()
{
    return FString::Printf(TEXT("Trace_%d"), ++NextTraceID);
}

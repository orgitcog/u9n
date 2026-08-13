/**
 * Episodic Memory System Implementation
 * 
 * Implements multi-store memory architecture for Deep-Tree-Echo.
 */

#include "EpisodicMemorySystem.h"
#include "../Sensorimotor/SensorimotorIntegration.h"

// ========================================
// CONSTRUCTOR
// ========================================

UEpisodicMemorySystem::UEpisodicMemorySystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;
}

// ========================================
// LIFECYCLE
// ========================================

void UEpisodicMemorySystem::BeginPlay()
{
    Super::BeginPlay();
    Initialize();
}

void UEpisodicMemorySystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    AccumulatedTime += DeltaTime;
    
    // Decay sensory memory
    DecaySensoryMemory(DeltaTime);
    
    // Decay memory strengths
    DecayMemoryStrengths(DeltaTime);
    
    // Update working memory
    UpdateWorkingMemoryActivations(DeltaTime);
    
    // Auto consolidation
    if (Config.bEnableAutoConsolidation)
    {
        if (AccumulatedTime - LastConsolidationTime > Config.ConsolidationInterval)
        {
            if (ConsolidationState.Phase == EConsolidationPhase::Idle)
            {
                StartConsolidation();
            }
            else
            {
                ProcessConsolidationStep();
            }
        }
    }
    
    // Integrate with cognitive cycle
    if (CognitiveCycleManager)
    {
        int32 GlobalStep = CognitiveCycleManager->CycleState.GlobalStep;
        if (GlobalStep != LastProcessedStep)
        {
            EConsciousnessStream ActiveStream;
            int32 StepInCycle = ((GlobalStep - 1) % 12) + 1;
            
            if (StepInCycle >= 1 && StepInCycle <= 4)
                ActiveStream = EConsciousnessStream::Perception;
            else if (StepInCycle >= 5 && StepInCycle <= 8)
                ActiveStream = EConsciousnessStream::Action;
            else
                ActiveStream = EConsciousnessStream::Simulation;
            
            ProcessCognitiveStep(GlobalStep, ActiveStream);
            LastProcessedStep = GlobalStep;
        }
    }
}

// ========================================
// INITIALIZATION
// ========================================

void UEpisodicMemorySystem::Initialize()
{
    // Initialize working memory
    WorkingMemory.Capacity = Config.WorkingMemoryCapacity;
    WorkingMemory.Items.Empty();
    WorkingMemory.Activations.Empty();
    WorkingMemory.FocusIndex = -1;
    WorkingMemory.CognitiveLoad = 0.0f;
    
    // Initialize consolidation state
    ConsolidationState.Phase = EConsolidationPhase::Idle;
    ConsolidationState.PendingEpisodes.Empty();
    ConsolidationState.CurrentEpisodeID = -1;
    ConsolidationState.Progress = 0.0f;
    
    UE_LOG(LogTemp, Log, TEXT("Episodic Memory System initialized with capacity %d"), Config.WorkingMemoryCapacity);
}

void UEpisodicMemorySystem::Reset()
{
    AllTraces.Empty();
    SensoryBuffer.Empty();
    Episodes.Empty();
    Concepts.Empty();
    ConceptNameToID.Empty();
    Skills.Empty();
    SkillNameToID.Empty();
    
    NextTraceID = 1;
    NextEpisodeID = 1;
    NextConceptID = 1;
    NextSkillID = 1;
    
    AccumulatedTime = 0.0f;
    LastConsolidationTime = 0.0f;
    CurrentRecordingEpisode = -1;
    
    Initialize();
}

// ========================================
// ENCODING
// ========================================

int64 UEpisodicMemorySystem::EncodeTrace(const TArray<float>& Content, EEncodingStrength Strength, float EmotionalValence)
{
    FMemoryTrace NewTrace;
    NewTrace.TraceID = NextTraceID++;
    NewTrace.Store = EMemoryStore::Sensory;
    NewTrace.Content = Content;
    NewTrace.EncodingStrength = Strength;
    NewTrace.Activation = 1.0f;
    NewTrace.CreationTime = AccumulatedTime;
    NewTrace.LastAccessTime = AccumulatedTime;
    NewTrace.AccessCount = 1;
    NewTrace.EmotionalValence = EmotionalValence;
    
    // Set initial strength based on encoding strength
    switch (Strength)
    {
    case EEncodingStrength::Weak:
        NewTrace.Strength = 0.3f;
        break;
    case EEncodingStrength::Moderate:
        NewTrace.Strength = 0.5f;
        break;
    case EEncodingStrength::Strong:
        NewTrace.Strength = 0.7f;
        NewTrace.EmotionalArousal = 0.5f;
        break;
    case EEncodingStrength::Flashbulb:
        NewTrace.Strength = 1.0f;
        NewTrace.EmotionalArousal = 1.0f;
        break;
    }
    
    AllTraces.Add(NewTrace.TraceID, NewTrace);
    SensoryBuffer.Add(NewTrace.TraceID);
    
    // Add to current episode if recording
    if (CurrentRecordingEpisode >= 0)
    {
        AddTraceToEpisode(NewTrace.TraceID);
    }
    
    return NewTrace.TraceID;
}

int32 UEpisodicMemorySystem::StartEpisode(const FString& Label, FVector SpatialContext)
{
    FEpisode NewEpisode;
    NewEpisode.EpisodeID = NextEpisodeID++;
    NewEpisode.Label = Label;
    NewEpisode.StartTime = AccumulatedTime;
    NewEpisode.SpatialContext = SpatialContext;
    NewEpisode.Significance = 0.5f;
    
    Episodes.Add(NewEpisode.EpisodeID, NewEpisode);
    CurrentRecordingEpisode = NewEpisode.EpisodeID;
    
    UE_LOG(LogTemp, Log, TEXT("Started episode %d: %s"), NewEpisode.EpisodeID, *Label);
    
    return NewEpisode.EpisodeID;
}

void UEpisodicMemorySystem::AddTraceToEpisode(int64 TraceID)
{
    if (CurrentRecordingEpisode >= 0)
    {
        if (FEpisode* Episode = Episodes.Find(CurrentRecordingEpisode))
        {
            Episode->TraceIDs.Add(TraceID);
            
            // Update episode emotional valence
            if (FMemoryTrace* Trace = GetTrace(TraceID))
            {
                float N = Episode->TraceIDs.Num();
                Episode->EmotionalValence = ((N - 1) * Episode->EmotionalValence + Trace->EmotionalValence) / N;
            }
        }
    }
}

void UEpisodicMemorySystem::EndEpisode(float Significance)
{
    if (CurrentRecordingEpisode >= 0)
    {
        if (FEpisode* Episode = Episodes.Find(CurrentRecordingEpisode))
        {
            Episode->EndTime = AccumulatedTime;
            Episode->Significance = Significance;
            
            // Move traces to episodic memory
            for (int64 TraceID : Episode->TraceIDs)
            {
                MoveToStore(TraceID, EMemoryStore::Episodic);
            }
            
            // Queue for consolidation if significant
            if (Significance > Config.ConsolidationThreshold)
            {
                ConsolidationState.PendingEpisodes.Add(CurrentRecordingEpisode);
            }
            
            OnEpisodeEncoded.Broadcast(CurrentRecordingEpisode);
            
            UE_LOG(LogTemp, Log, TEXT("Ended episode %d with %d traces, significance %.2f"), 
                CurrentRecordingEpisode, Episode->TraceIDs.Num(), Significance);
        }
        
        CurrentRecordingEpisode = -1;
    }
}

int32 UEpisodicMemorySystem::EncodeSkill(FName SkillName, const TArray<TArray<float>>& ActionSequence)
{
    // Check if skill exists
    if (int32* ExistingID = SkillNameToID.Find(SkillName))
    {
        // Update existing skill
        if (FProceduralSkill* Skill = Skills.Find(*ExistingID))
        {
            Skill->PracticeCount++;
            Skill->Proficiency = FMath::Min(1.0f, Skill->Proficiency + 0.05f);
            Skill->Automaticity = FMath::Min(1.0f, Skill->Automaticity + 0.02f);
            return *ExistingID;
        }
    }
    
    // Create new skill
    FProceduralSkill NewSkill;
    NewSkill.SkillID = NextSkillID++;
    NewSkill.SkillName = SkillName;
    NewSkill.ActionSequence = ActionSequence;
    NewSkill.Proficiency = 0.1f;
    NewSkill.PracticeCount = 1;
    NewSkill.SuccessRate = 0.5f;
    NewSkill.Automaticity = 0.0f;
    
    Skills.Add(NewSkill.SkillID, NewSkill);
    SkillNameToID.Add(SkillName, NewSkill.SkillID);
    
    return NewSkill.SkillID;
}

// ========================================
// RETRIEVAL
// ========================================

TArray<FMemoryTrace> UEpisodicMemorySystem::RetrieveByContent(const TArray<float>& Cue, int32 MaxResults)
{
    TArray<TPair<float, int64>> ScoredTraces;
    
    for (auto& Pair : AllTraces)
    {
        float Similarity = ComputeSimilarity(Cue, Pair.Value.Content);
        if (Similarity > 0.3f)
        {
            float Score = Similarity * Pair.Value.Strength * (1.0f + Pair.Value.Activation);
            ScoredTraces.Add(TPair<float, int64>(Score, Pair.Key));
        }
    }
    
    // Sort by score descending
    ScoredTraces.Sort([](const TPair<float, int64>& A, const TPair<float, int64>& B) {
        return A.Key > B.Key;
    });
    
    TArray<FMemoryTrace> Results;
    for (int32 i = 0; i < FMath::Min(MaxResults, ScoredTraces.Num()); ++i)
    {
        if (FMemoryTrace* Trace = GetTrace(ScoredTraces[i].Value))
        {
            // Update access info
            Trace->LastAccessTime = AccumulatedTime;
            Trace->AccessCount++;
            Trace->Activation = FMath::Min(1.0f, Trace->Activation + 0.2f);
            
            Results.Add(*Trace);
            OnMemoryRetrieved.Broadcast(Trace->TraceID, Trace->Activation);
        }
    }
    
    return Results;
}

TArray<FMemoryTrace> UEpisodicMemorySystem::RetrieveByTime(float StartTime, float EndTime, int32 MaxResults)
{
    TArray<TPair<float, int64>> ScoredTraces;
    
    for (auto& Pair : AllTraces)
    {
        if (Pair.Value.CreationTime >= StartTime && Pair.Value.CreationTime <= EndTime)
        {
            // Score by recency within range
            float Recency = 1.0f - (EndTime - Pair.Value.CreationTime) / FMath::Max(1.0f, EndTime - StartTime);
            float Score = Recency * Pair.Value.Strength;
            ScoredTraces.Add(TPair<float, int64>(Score, Pair.Key));
        }
    }
    
    ScoredTraces.Sort([](const TPair<float, int64>& A, const TPair<float, int64>& B) {
        return A.Key > B.Key;
    });
    
    TArray<FMemoryTrace> Results;
    for (int32 i = 0; i < FMath::Min(MaxResults, ScoredTraces.Num()); ++i)
    {
        if (FMemoryTrace* Trace = GetTrace(ScoredTraces[i].Value))
        {
            Trace->LastAccessTime = AccumulatedTime;
            Trace->AccessCount++;
            Results.Add(*Trace);
        }
    }
    
    return Results;
}

TArray<FMemoryTrace> UEpisodicMemorySystem::RetrieveBySpatial(FVector Location, float Radius, int32 MaxResults)
{
    TArray<TPair<float, int64>> ScoredTraces;
    
    // Find episodes near location
    for (auto& EpPair : Episodes)
    {
        float Distance = FVector::Dist(Location, EpPair.Value.SpatialContext);
        if (Distance <= Radius)
        {
            float Proximity = 1.0f - (Distance / Radius);
            
            // Score all traces in this episode
            for (int64 TraceID : EpPair.Value.TraceIDs)
            {
                if (FMemoryTrace* Trace = GetTrace(TraceID))
                {
                    float Score = Proximity * Trace->Strength;
                    ScoredTraces.Add(TPair<float, int64>(Score, TraceID));
                }
            }
        }
    }
    
    ScoredTraces.Sort([](const TPair<float, int64>& A, const TPair<float, int64>& B) {
        return A.Key > B.Key;
    });
    
    TArray<FMemoryTrace> Results;
    for (int32 i = 0; i < FMath::Min(MaxResults, ScoredTraces.Num()); ++i)
    {
        if (FMemoryTrace* Trace = GetTrace(ScoredTraces[i].Value))
        {
            Trace->LastAccessTime = AccumulatedTime;
            Trace->AccessCount++;
            Results.Add(*Trace);
        }
    }
    
    return Results;
}

TArray<FMemoryTrace> UEpisodicMemorySystem::RetrieveByEmotion(float TargetValence, float Tolerance, int32 MaxResults)
{
    TArray<TPair<float, int64>> ScoredTraces;
    
    for (auto& Pair : AllTraces)
    {
        float ValenceDiff = FMath::Abs(Pair.Value.EmotionalValence - TargetValence);
        if (ValenceDiff <= Tolerance)
        {
            float Match = 1.0f - (ValenceDiff / Tolerance);
            float Score = Match * Pair.Value.Strength * (1.0f + Pair.Value.EmotionalArousal);
            ScoredTraces.Add(TPair<float, int64>(Score, Pair.Key));
        }
    }
    
    ScoredTraces.Sort([](const TPair<float, int64>& A, const TPair<float, int64>& B) {
        return A.Key > B.Key;
    });
    
    TArray<FMemoryTrace> Results;
    for (int32 i = 0; i < FMath::Min(MaxResults, ScoredTraces.Num()); ++i)
    {
        if (FMemoryTrace* Trace = GetTrace(ScoredTraces[i].Value))
        {
            Trace->LastAccessTime = AccumulatedTime;
            Trace->AccessCount++;
            Results.Add(*Trace);
        }
    }
    
    return Results;
}

TArray<FMemoryTrace> UEpisodicMemorySystem::RetrieveForGoal(int32 GoalID, int32 MaxResults)
{
    TArray<TPair<float, int64>> ScoredTraces;
    
    for (auto& Pair : AllTraces)
    {
        if (Pair.Value.AssociatedGoals.Contains(GoalID))
        {
            float Score = Pair.Value.Strength * (1.0f + Pair.Value.Activation);
            ScoredTraces.Add(TPair<float, int64>(Score, Pair.Key));
        }
    }
    
    ScoredTraces.Sort([](const TPair<float, int64>& A, const TPair<float, int64>& B) {
        return A.Key > B.Key;
    });
    
    TArray<FMemoryTrace> Results;
    for (int32 i = 0; i < FMath::Min(MaxResults, ScoredTraces.Num()); ++i)
    {
        if (FMemoryTrace* Trace = GetTrace(ScoredTraces[i].Value))
        {
            Trace->LastAccessTime = AccumulatedTime;
            Trace->AccessCount++;
            Results.Add(*Trace);
        }
    }
    
    return Results;
}

TArray<FMemoryTrace> UEpisodicMemorySystem::Retrieve(const FRetrievalRequest& Request)
{
    switch (Request.CueType)
    {
    case ERetrievalCueType::Content:
        return RetrieveByContent(Request.CueContent, Request.MaxResults);
    case ERetrievalCueType::Temporal:
        return RetrieveByTime(Request.TemporalRange.X, Request.TemporalRange.Y, Request.MaxResults);
    case ERetrievalCueType::Spatial:
        return RetrieveBySpatial(Request.SpatialLocation, Request.SpatialRadius, Request.MaxResults);
    case ERetrievalCueType::Emotional:
        return RetrieveByEmotion(Request.TargetValence, 0.3f, Request.MaxResults);
    case ERetrievalCueType::GoalRelevant:
        return RetrieveForGoal(Request.GoalID, Request.MaxResults);
    default:
        return TArray<FMemoryTrace>();
    }
}

FEpisode UEpisodicMemorySystem::GetEpisode(int32 EpisodeID) const
{
    if (const FEpisode* Episode = Episodes.Find(EpisodeID))
    {
        return *Episode;
    }
    return FEpisode();
}

FSemanticConcept UEpisodicMemorySystem::GetConcept(FName ConceptName) const
{
    if (const int32* ConceptID = ConceptNameToID.Find(ConceptName))
    {
        if (const FSemanticConcept* Concept = Concepts.Find(*ConceptID))
        {
            return *Concept;
        }
    }
    return FSemanticConcept();
}

FProceduralSkill UEpisodicMemorySystem::GetSkill(FName SkillName) const
{
    if (const int32* SkillID = SkillNameToID.Find(SkillName))
    {
        if (const FProceduralSkill* Skill = Skills.Find(*SkillID))
        {
            return *Skill;
        }
    }
    return FProceduralSkill();
}

// ========================================
// WORKING MEMORY
// ========================================

bool UEpisodicMemorySystem::LoadToWorkingMemory(int64 TraceID)
{
    // Check if already in working memory
    if (WorkingMemory.Items.Contains(TraceID))
    {
        // Boost activation
        int32 Index = WorkingMemory.Items.Find(TraceID);
        WorkingMemory.Activations[Index] = FMath::Min(1.0f, WorkingMemory.Activations[Index] + 0.3f);
        return true;
    }
    
    // Check capacity
    if (WorkingMemory.Items.Num() >= WorkingMemory.Capacity)
    {
        // Remove least active item
        int32 MinIndex = 0;
        float MinActivation = FLT_MAX;
        for (int32 i = 0; i < WorkingMemory.Activations.Num(); ++i)
        {
            if (WorkingMemory.Activations[i] < MinActivation)
            {
                MinActivation = WorkingMemory.Activations[i];
                MinIndex = i;
            }
        }
        
        WorkingMemory.Items.RemoveAt(MinIndex);
        WorkingMemory.Activations.RemoveAt(MinIndex);
        
        if (WorkingMemory.FocusIndex >= MinIndex)
        {
            WorkingMemory.FocusIndex = FMath::Max(0, WorkingMemory.FocusIndex - 1);
        }
    }
    
    // Add new item
    WorkingMemory.Items.Add(TraceID);
    WorkingMemory.Activations.Add(1.0f);
    
    // Update cognitive load
    WorkingMemory.CognitiveLoad = (float)WorkingMemory.Items.Num() / (float)WorkingMemory.Capacity;
    
    // Activate the trace
    if (FMemoryTrace* Trace = GetTrace(TraceID))
    {
        Trace->Activation = 1.0f;
    }
    
    return true;
}

void UEpisodicMemorySystem::RemoveFromWorkingMemory(int64 TraceID)
{
    int32 Index = WorkingMemory.Items.Find(TraceID);
    if (Index != INDEX_NONE)
    {
        WorkingMemory.Items.RemoveAt(Index);
        WorkingMemory.Activations.RemoveAt(Index);
        
        if (WorkingMemory.FocusIndex >= Index)
        {
            WorkingMemory.FocusIndex = FMath::Max(-1, WorkingMemory.FocusIndex - 1);
        }
        
        WorkingMemory.CognitiveLoad = (float)WorkingMemory.Items.Num() / (float)WorkingMemory.Capacity;
    }
}

void UEpisodicMemorySystem::SetAttentionFocus(int32 ItemIndex)
{
    if (ItemIndex >= 0 && ItemIndex < WorkingMemory.Items.Num())
    {
        WorkingMemory.FocusIndex = ItemIndex;
        WorkingMemory.Activations[ItemIndex] = 1.0f;
    }
}

TArray<FMemoryTrace> UEpisodicMemorySystem::GetWorkingMemoryContents() const
{
    TArray<FMemoryTrace> Contents;
    for (int64 TraceID : WorkingMemory.Items)
    {
        if (const FMemoryTrace* Trace = GetTrace(TraceID))
        {
            Contents.Add(*Trace);
        }
    }
    return Contents;
}

float UEpisodicMemorySystem::GetCognitiveLoad() const
{
    return WorkingMemory.CognitiveLoad;
}

// ========================================
// CONSOLIDATION
// ========================================

void UEpisodicMemorySystem::StartConsolidation()
{
    if (ConsolidationState.PendingEpisodes.Num() == 0)
    {
        // Find episodes that need consolidation
        for (auto& Pair : Episodes)
        {
            if (!Pair.Value.bConsolidated && Pair.Value.Significance > Config.ConsolidationThreshold)
            {
                ConsolidationState.PendingEpisodes.Add(Pair.Key);
            }
        }
    }
    
    if (ConsolidationState.PendingEpisodes.Num() > 0)
    {
        ConsolidationState.Phase = EConsolidationPhase::Encoding;
        ConsolidationState.CurrentEpisodeID = ConsolidationState.PendingEpisodes[0];
        ConsolidationState.ReplayIteration = 0;
        ConsolidationState.Progress = 0.0f;
        
        UE_LOG(LogTemp, Log, TEXT("Starting consolidation with %d pending episodes"), ConsolidationState.PendingEpisodes.Num());
    }
}

void UEpisodicMemorySystem::ProcessConsolidationStep()
{
    if (ConsolidationState.Phase == EConsolidationPhase::Idle)
    {
        return;
    }
    
    switch (ConsolidationState.Phase)
    {
    case EConsolidationPhase::Encoding:
        // Strengthen encoding of current episode traces
        if (FEpisode* Episode = Episodes.Find(ConsolidationState.CurrentEpisodeID))
        {
            for (int64 TraceID : Episode->TraceIDs)
            {
                if (FMemoryTrace* Trace = GetTrace(TraceID))
                {
                    Trace->Strength = FMath::Min(1.0f, Trace->Strength * 1.1f);
                }
            }
        }
        ConsolidationState.Phase = EConsolidationPhase::Replay;
        ConsolidationState.Progress = 0.25f;
        break;
        
    case EConsolidationPhase::Replay:
        // Replay episode
        ReplayEpisode(ConsolidationState.CurrentEpisodeID);
        ConsolidationState.ReplayIteration++;
        
        if (ConsolidationState.ReplayIteration >= 3)
        {
            ConsolidationState.Phase = EConsolidationPhase::Integration;
            ConsolidationState.Progress = 0.5f;
        }
        break;
        
    case EConsolidationPhase::Integration:
        // Integrate with semantic memory
        IntegrateWithSemanticMemory(ConsolidationState.CurrentEpisodeID);
        ConsolidationState.Phase = EConsolidationPhase::Pruning;
        ConsolidationState.Progress = 0.75f;
        break;
        
    case EConsolidationPhase::Pruning:
        // Prune weak memories
        PruneWeakMemories();
        
        // Mark episode as consolidated
        if (FEpisode* Episode = Episodes.Find(ConsolidationState.CurrentEpisodeID))
        {
            Episode->bConsolidated = true;
        }
        
        OnConsolidationComplete.Broadcast(ConsolidationState.CurrentEpisodeID);
        
        // Move to next episode
        ConsolidationState.PendingEpisodes.RemoveAt(0);
        if (ConsolidationState.PendingEpisodes.Num() > 0)
        {
            ConsolidationState.CurrentEpisodeID = ConsolidationState.PendingEpisodes[0];
            ConsolidationState.Phase = EConsolidationPhase::Encoding;
            ConsolidationState.ReplayIteration = 0;
        }
        else
        {
            ConsolidationState.Phase = EConsolidationPhase::Idle;
            ConsolidationState.CurrentEpisodeID = -1;
            LastConsolidationTime = AccumulatedTime;
        }
        ConsolidationState.Progress = 1.0f;
        break;
        
    default:
        break;
    }
}

void UEpisodicMemorySystem::StopConsolidation()
{
    ConsolidationState.Phase = EConsolidationPhase::Idle;
}

float UEpisodicMemorySystem::GetConsolidationProgress() const
{
    return ConsolidationState.Progress;
}

int32 UEpisodicMemorySystem::AbstractToConcept(int32 EpisodeID, FName ConceptName)
{
    const FEpisode* Episode = Episodes.Find(EpisodeID);
    if (!Episode || Episode->TraceIDs.Num() == 0)
    {
        return -1;
    }
    
    // Compute prototype from episode traces
    TArray<float> Prototype;
    TArray<float> Variance;
    int32 ContentSize = 0;
    
    // First pass: determine size and compute mean
    for (int64 TraceID : Episode->TraceIDs)
    {
        if (const FMemoryTrace* Trace = GetTrace(TraceID))
        {
            if (Prototype.Num() == 0)
            {
                ContentSize = Trace->Content.Num();
                Prototype.SetNumZeroed(ContentSize);
                Variance.SetNumZeroed(ContentSize);
            }
            
            for (int32 i = 0; i < FMath::Min(ContentSize, Trace->Content.Num()); ++i)
            {
                Prototype[i] += Trace->Content[i];
            }
        }
    }
    
    float N = Episode->TraceIDs.Num();
    for (float& Val : Prototype)
    {
        Val /= N;
    }
    
    // Second pass: compute variance
    for (int64 TraceID : Episode->TraceIDs)
    {
        if (const FMemoryTrace* Trace = GetTrace(TraceID))
        {
            for (int32 i = 0; i < FMath::Min(ContentSize, Trace->Content.Num()); ++i)
            {
                float Diff = Trace->Content[i] - Prototype[i];
                Variance[i] += Diff * Diff;
            }
        }
    }
    
    for (float& Val : Variance)
    {
        Val = FMath::Sqrt(Val / N);
    }
    
    // Check if concept exists
    if (int32* ExistingID = ConceptNameToID.Find(ConceptName))
    {
        // Update existing concept
        if (FSemanticConcept* Concept = Concepts.Find(*ExistingID))
        {
            float Alpha = 1.0f / (Concept->InstanceCount + 1);
            for (int32 i = 0; i < FMath::Min(Concept->Prototype.Num(), Prototype.Num()); ++i)
            {
                Concept->Prototype[i] = (1.0f - Alpha) * Concept->Prototype[i] + Alpha * Prototype[i];
                Concept->Variance[i] = (1.0f - Alpha) * Concept->Variance[i] + Alpha * Variance[i];
            }
            Concept->InstanceCount++;
            Concept->SourceEpisodes.Add(EpisodeID);
            Concept->Confidence = FMath::Min(1.0f, Concept->Confidence + 0.1f);
            
            return *ExistingID;
        }
    }
    
    // Create new concept
    FSemanticConcept NewConcept;
    NewConcept.ConceptID = NextConceptID++;
    NewConcept.ConceptName = ConceptName;
    NewConcept.Prototype = Prototype;
    NewConcept.Variance = Variance;
    NewConcept.InstanceCount = 1;
    NewConcept.Confidence = 0.5f;
    NewConcept.SourceEpisodes.Add(EpisodeID);
    
    Concepts.Add(NewConcept.ConceptID, NewConcept);
    ConceptNameToID.Add(ConceptName, NewConcept.ConceptID);
    
    OnConceptFormed.Broadcast(NewConcept.ConceptID, ConceptName);
    
    return NewConcept.ConceptID;
}

// ========================================
// COGNITIVE CYCLE INTEGRATION
// ========================================

void UEpisodicMemorySystem::ProcessCognitiveStep(int32 GlobalStep, EConsciousnessStream ActiveStream)
{
    int32 StepInCycle = ((GlobalStep - 1) % 12) + 1;
    
    switch (ActiveStream)
    {
    case EConsciousnessStream::Perception:
        // Steps 1-4: Memory-guided perception
        if (StepInCycle == 3)
        {
            // T7R: Memory consolidation step
            // Retrieve relevant memories for current perception
            if (SensorimotorComponent)
            {
                TArray<float> CurrentFeatures = SensorimotorComponent->GetIntegratedSensoryFeatures();
                if (CurrentFeatures.Num() > 0)
                {
                    TArray<FMemoryTrace> Relevant = RetrieveByContent(CurrentFeatures, 3);
                    for (const FMemoryTrace& Trace : Relevant)
                    {
                        LoadToWorkingMemory(Trace.TraceID);
                    }
                }
            }
        }
        break;
        
    case EConsciousnessStream::Action:
        // Steps 5-8: Skill retrieval and execution
        if (StepInCycle == 7)
        {
            // T5E: Action sequence execution
            // Procedural memory guides action
        }
        break;
        
    case EConsciousnessStream::Simulation:
        // Steps 9-12: Memory consolidation
        if (StepInCycle == 9)
        {
            // Start consolidation if needed
            if (ConsolidationState.Phase == EConsolidationPhase::Idle && 
                ConsolidationState.PendingEpisodes.Num() > 0)
            {
                StartConsolidation();
            }
        }
        else if (StepInCycle == 10)
        {
            // T7R: Memory encoding
            ProcessConsolidationStep();
        }
        else if (StepInCycle == 12)
        {
            // Final consolidation step
            ProcessConsolidationStep();
        }
        break;
    }
}

TArray<float> UEpisodicMemorySystem::GetMemoryGuidedAttention(const TArray<float>& CurrentPerception) const
{
    TArray<float> AttentionWeights;
    AttentionWeights.SetNum(CurrentPerception.Num());
    
    // Initialize with uniform attention
    for (float& W : AttentionWeights)
    {
        W = 1.0f / CurrentPerception.Num();
    }
    
    // Modulate by working memory contents
    for (int64 TraceID : WorkingMemory.Items)
    {
        if (const FMemoryTrace* Trace = GetTrace(TraceID))
        {
            float Similarity = ComputeSimilarity(CurrentPerception, Trace->Content);
            if (Similarity > 0.3f)
            {
                // Boost attention to similar features
                for (int32 i = 0; i < FMath::Min(AttentionWeights.Num(), Trace->Content.Num()); ++i)
                {
                    AttentionWeights[i] += Similarity * FMath::Abs(Trace->Content[i]) * Trace->Activation;
                }
            }
        }
    }
    
    // Normalize
    float Sum = 0.0f;
    for (float W : AttentionWeights)
    {
        Sum += W;
    }
    if (Sum > 0.0f)
    {
        for (float& W : AttentionWeights)
        {
            W /= Sum;
        }
    }
    
    return AttentionWeights;
}

// ========================================
// ASSOCIATIONS
// ========================================

void UEpisodicMemorySystem::CreateAssociation(int64 TraceA, int64 TraceB, float Strength)
{
    if (FMemoryTrace* A = GetTrace(TraceA))
    {
        if (!A->LinkedTraces.Contains(TraceB))
        {
            A->LinkedTraces.Add(TraceB);
        }
    }
    
    if (FMemoryTrace* B = GetTrace(TraceB))
    {
        if (!B->LinkedTraces.Contains(TraceA))
        {
            B->LinkedTraces.Add(TraceA);
        }
    }
}

TArray<int64> UEpisodicMemorySystem::GetAssociatedTraces(int64 TraceID) const
{
    if (const FMemoryTrace* Trace = GetTrace(TraceID))
    {
        return Trace->LinkedTraces;
    }
    return TArray<int64>();
}

void UEpisodicMemorySystem::SpreadActivation(int64 SourceTrace, float ActivationAmount, int32 MaxDepth)
{
    if (MaxDepth <= 0 || ActivationAmount < 0.01f)
    {
        return;
    }
    
    if (FMemoryTrace* Source = GetTrace(SourceTrace))
    {
        Source->Activation = FMath::Min(1.0f, Source->Activation + ActivationAmount);
        
        // Spread to linked traces with decay
        float SpreadAmount = ActivationAmount * 0.5f;
        for (int64 LinkedID : Source->LinkedTraces)
        {
            SpreadActivation(LinkedID, SpreadAmount, MaxDepth - 1);
        }
    }
}

// ========================================
// INTERNAL METHODS
// ========================================

void UEpisodicMemorySystem::DecaySensoryMemory(float DeltaTime)
{
    float DecayAmount = Config.SensoryDecayRate * DeltaTime;
    
    TArray<int64> ToRemove;
    for (int64 TraceID : SensoryBuffer)
    {
        if (FMemoryTrace* Trace = GetTrace(TraceID))
        {
            Trace->Activation -= DecayAmount;
            if (Trace->Activation <= 0.0f)
            {
                ToRemove.Add(TraceID);
            }
        }
    }
    
    for (int64 TraceID : ToRemove)
    {
        SensoryBuffer.Remove(TraceID);
        AllTraces.Remove(TraceID);
    }
}

void UEpisodicMemorySystem::DecayMemoryStrengths(float DeltaTime)
{
    float DecayAmount = Config.StrengthDecayRate * DeltaTime;
    
    for (auto& Pair : AllTraces)
    {
        if (Pair.Value.Store != EMemoryStore::Sensory)
        {
            // Decay based on time since last access
            float TimeSinceAccess = AccumulatedTime - Pair.Value.LastAccessTime;
            float DecayFactor = 1.0f + TimeSinceAccess * 0.001f;
            
            Pair.Value.Strength -= DecayAmount * DecayFactor;
            Pair.Value.Strength = FMath::Max(0.0f, Pair.Value.Strength);
            
            // Also decay activation
            Pair.Value.Activation *= (1.0f - DecayAmount);
        }
    }
}

void UEpisodicMemorySystem::UpdateWorkingMemoryActivations(float DeltaTime)
{
    for (int32 i = 0; i < WorkingMemory.Activations.Num(); ++i)
    {
        if (i == WorkingMemory.FocusIndex)
        {
            // Focused item maintains high activation
            WorkingMemory.Activations[i] = FMath::Min(1.0f, WorkingMemory.Activations[i] + 0.1f * DeltaTime);
        }
        else
        {
            // Other items decay
            WorkingMemory.Activations[i] -= 0.05f * DeltaTime;
            WorkingMemory.Activations[i] = FMath::Max(0.0f, WorkingMemory.Activations[i]);
        }
    }
}

void UEpisodicMemorySystem::ReplayEpisode(int32 EpisodeID)
{
    if (FEpisode* Episode = Episodes.Find(EpisodeID))
    {
        Episode->ReplayCount++;
        
        // Strengthen traces
        for (int64 TraceID : Episode->TraceIDs)
        {
            if (FMemoryTrace* Trace = GetTrace(TraceID))
            {
                Trace->Strength = FMath::Min(1.0f, Trace->Strength * Config.ReplayStrengthFactor);
                Trace->Activation = FMath::Min(1.0f, Trace->Activation + 0.3f);
            }
        }
        
        // Strengthen associations between consecutive traces
        for (int32 i = 0; i < Episode->TraceIDs.Num() - 1; ++i)
        {
            CreateAssociation(Episode->TraceIDs[i], Episode->TraceIDs[i + 1], 0.8f);
        }
    }
}

void UEpisodicMemorySystem::IntegrateWithSemanticMemory(int32 EpisodeID)
{
    // Auto-generate concept name from episode
    if (const FEpisode* Episode = Episodes.Find(EpisodeID))
    {
        FName ConceptName = FName(*FString::Printf(TEXT("Concept_Ep%d"), EpisodeID));
        AbstractToConcept(EpisodeID, ConceptName);
    }
}

void UEpisodicMemorySystem::PruneWeakMemories()
{
    TArray<int64> ToRemove;
    
    for (auto& Pair : AllTraces)
    {
        if (Pair.Value.Store == EMemoryStore::Episodic && 
            Pair.Value.Strength < 0.1f &&
            Pair.Value.AccessCount < 3)
        {
            ToRemove.Add(Pair.Key);
        }
    }
    
    for (int64 TraceID : ToRemove)
    {
        AllTraces.Remove(TraceID);
    }
    
    // Also prune weak episodes
    TArray<int32> EpisodesToRemove;
    if (Episodes.Num() > Config.MaxEpisodes)
    {
        // Find least significant consolidated episodes
        TArray<TPair<float, int32>> ScoredEpisodes;
        for (auto& Pair : Episodes)
        {
            if (Pair.Value.bConsolidated)
            {
                ScoredEpisodes.Add(TPair<float, int32>(Pair.Value.Significance, Pair.Key));
            }
        }
        
        ScoredEpisodes.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) {
            return A.Key < B.Key;
        });
        
        int32 ToRemoveCount = Episodes.Num() - Config.MaxEpisodes;
        for (int32 i = 0; i < FMath::Min(ToRemoveCount, ScoredEpisodes.Num()); ++i)
        {
            EpisodesToRemove.Add(ScoredEpisodes[i].Value);
        }
    }
    
    for (int32 EpisodeID : EpisodesToRemove)
    {
        Episodes.Remove(EpisodeID);
    }
}

float UEpisodicMemorySystem::ComputeSimilarity(const TArray<float>& A, const TArray<float>& B) const
{
    if (A.Num() == 0 || B.Num() == 0) return 0.0f;
    
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
    
    float Denom = FMath::Sqrt(NormA) * FMath::Sqrt(NormB);
    if (Denom < 0.0001f) return 0.0f;
    
    return FMath::Clamp(DotProduct / Denom, 0.0f, 1.0f);
}

FMemoryTrace* UEpisodicMemorySystem::GetTrace(int64 TraceID)
{
    return AllTraces.Find(TraceID);
}

const FMemoryTrace* UEpisodicMemorySystem::GetTrace(int64 TraceID) const
{
    return AllTraces.Find(TraceID);
}

void UEpisodicMemorySystem::MoveToStore(int64 TraceID, EMemoryStore NewStore)
{
    if (FMemoryTrace* Trace = GetTrace(TraceID))
    {
        EMemoryStore OldStore = Trace->Store;
        Trace->Store = NewStore;
        
        // Remove from sensory buffer if moving out
        if (OldStore == EMemoryStore::Sensory && NewStore != EMemoryStore::Sensory)
        {
            SensoryBuffer.Remove(TraceID);
        }
    }
}

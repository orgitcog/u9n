// MemoryConsciousnessIntegration.cpp
// Implementation of Memory-Consciousness Stream integration

#include "MemoryConsciousnessIntegration.h"
#include "../Reservoir/DeepTreeEchoReservoir.h"

UMemoryConsciousnessIntegration::UMemoryConsciousnessIntegration()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    // Default configuration
    Config.WorkingMemoryCapacity = 4;
    Config.MemoryAttentionWeight = 0.5f;
    Config.ConsciousnessEncodingWeight = 0.7f;
    Config.bEnableCrossStreamAssociation = true;
    Config.bEnableGoalDirectedRetrieval = true;
    Config.RetrievalThreshold = 0.5f;
    Config.AttentionEncodingStrength = 1.2f;
}

void UMemoryConsciousnessIntegration::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeBindings();
}

void UMemoryConsciousnessIntegration::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        MemorySystem = Owner->FindComponentByClass<UHypergraphMemorySystem>();
        ConsciousnessSystem = Owner->FindComponentByClass<URecursiveMutualAwarenessSystem>();
        ReservoirComponent = Owner->FindComponentByClass<UDeepTreeEchoReservoir>();
    }
}

void UMemoryConsciousnessIntegration::InitializeBindings()
{
    // Initialize stream bindings for all three consciousness streams
    StreamBindings.Add(EConsciousnessStream::Perceiving, FStreamMemoryBinding());
    StreamBindings.Add(EConsciousnessStream::Acting, FStreamMemoryBinding());
    StreamBindings.Add(EConsciousnessStream::Reflecting, FStreamMemoryBinding());

    // Initialize stream binding properties
    StreamBindings[EConsciousnessStream::Perceiving].StreamType = EConsciousnessStream::Perceiving;
    StreamBindings[EConsciousnessStream::Perceiving].ActivationModifier = 1.0f;

    StreamBindings[EConsciousnessStream::Acting].StreamType = EConsciousnessStream::Acting;
    StreamBindings[EConsciousnessStream::Acting].ActivationModifier = 1.2f; // Actions get higher encoding

    StreamBindings[EConsciousnessStream::Reflecting].StreamType = EConsciousnessStream::Reflecting;
    StreamBindings[EConsciousnessStream::Reflecting].ActivationModifier = 0.8f;

    // Initialize working memory arrays
    StreamWorkingMemory.Add(EConsciousnessStream::Perceiving, TArray<FConsciousnessWorkingMemorySlot>());
    StreamWorkingMemory.Add(EConsciousnessStream::Acting, TArray<FConsciousnessWorkingMemorySlot>());
    StreamWorkingMemory.Add(EConsciousnessStream::Reflecting, TArray<FConsciousnessWorkingMemorySlot>());
}

void UMemoryConsciousnessIntegration::TickComponent(float DeltaTime, ELevelTick TickType,
                                                     FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AccumulatedTime += DeltaTime;

    if (!bEnableAutoIntegration)
    {
        return;
    }

    // Update working memory decay
    UpdateWorkingMemoryDecay(DeltaTime);

    // Get current cycle step from consciousness system
    if (ConsciousnessSystem)
    {
        int32 NewStep = ConsciousnessSystem->GetCurrentStep();
        if (NewStep != CurrentCycleStep)
        {
            CurrentCycleStep = NewStep;

            // Determine active stream based on cycle step
            EConsciousnessStream ActiveStream;
            switch ((CurrentCycleStep - 1) % 3)
            {
            case 0:
                ActiveStream = EConsciousnessStream::Perceiving;
                break;
            case 1:
                ActiveStream = EConsciousnessStream::Acting;
                break;
            default:
                ActiveStream = EConsciousnessStream::Reflecting;
                break;
            }

            ProcessCycleStep(CurrentCycleStep, ActiveStream);
        }

        // Check for triadic synchronization points
        if (ConsciousnessSystem->IsTriadicSynchronizationPoint())
        {
            int32 SyncPoint = (CurrentCycleStep - 1) / 4; // 0, 1, 2, 3
            HandleTriadicSynchronization(SyncPoint);
        }
    }

    // Process each stream
    ProcessPerceivingStream(DeltaTime);
    ProcessActingStream(DeltaTime);
    ProcessReflectingStream(DeltaTime);
}

// ========================================
// STREAM-MEMORY BINDING IMPLEMENTATION
// ========================================

void UMemoryConsciousnessIntegration::BindStreamToMemory(EConsciousnessStream Stream,
                                                          const TArray<float>& StateEmbedding)
{
    FStreamMemoryBinding* Binding = StreamBindings.Find(Stream);
    if (!Binding || !MemorySystem)
    {
        return;
    }

    Binding->RetrievalCue = StateEmbedding;

    // Find relevant memories for this state
    TArray<int64> RelevantNodes = MemorySystem->FindSimilarNodes(StateEmbedding,
                                                                   Config.RetrievalThreshold, 10);

    // Update active nodes
    Binding->ActiveNodes = RelevantNodes;

    // Spread activation to retrieved nodes
    for (int64 NodeID : RelevantNodes)
    {
        MemorySystem->BoostNodeActivation(NodeID, 0.2f * Binding->ActivationModifier);
    }
}

void UMemoryConsciousnessIntegration::UnbindStream(EConsciousnessStream Stream)
{
    FStreamMemoryBinding* Binding = StreamBindings.Find(Stream);
    if (Binding)
    {
        Binding->ActiveNodes.Empty();
        Binding->RetrievalCue.Empty();
        Binding->EncodingQueue.Empty();
    }
}

FStreamMemoryBinding UMemoryConsciousnessIntegration::GetStreamBinding(EConsciousnessStream Stream) const
{
    if (const FStreamMemoryBinding* Binding = StreamBindings.Find(Stream))
    {
        return *Binding;
    }
    return FStreamMemoryBinding();
}

void UMemoryConsciousnessIntegration::UpdateRetrievalCue(EConsciousnessStream Stream,
                                                          const TArray<float>& Cue)
{
    FStreamMemoryBinding* Binding = StreamBindings.Find(Stream);
    if (Binding)
    {
        Binding->RetrievalCue = Cue;
    }
}

// ========================================
// WORKING MEMORY IMPLEMENTATION
// ========================================

bool UMemoryConsciousnessIntegration::LoadToWorkingMemory(EConsciousnessStream Stream, int64 NodeID)
{
    TArray<FConsciousnessWorkingMemorySlot>* WM = StreamWorkingMemory.Find(Stream);
    if (!WM || !MemorySystem)
    {
        return false;
    }

    // Check if already loaded
    for (const FConsciousnessWorkingMemorySlot& Slot : *WM)
    {
        if (Slot.NodeID == NodeID)
        {
            return true; // Already loaded
        }
    }

    // Check capacity
    if (WM->Num() >= Config.WorkingMemoryCapacity)
    {
        // Evict lowest activation item
        int32 LowestIndex = 0;
        float LowestActivation = (*WM)[0].Activation;

        for (int32 i = 1; i < WM->Num(); ++i)
        {
            if ((*WM)[i].Activation < LowestActivation)
            {
                LowestActivation = (*WM)[i].Activation;
                LowestIndex = i;
            }
        }

        WM->RemoveAt(LowestIndex);
    }

    // Create new slot
    FConsciousnessWorkingMemorySlot NewSlot;
    NewSlot.NodeID = NodeID;
    NewSlot.SourceStream = Stream;
    NewSlot.Activation = 1.0f;
    NewSlot.TimeLoaded = AccumulatedTime;
    NewSlot.GoalRelevance = 0.5f;
    NewSlot.AttentionalWeight = 0.5f;

    WM->Add(NewSlot);

    OnWorkingMemoryUpdated.Broadcast(Stream);
    return true;
}

void UMemoryConsciousnessIntegration::RemoveFromWorkingMemory(EConsciousnessStream Stream, int64 NodeID)
{
    TArray<FConsciousnessWorkingMemorySlot>* WM = StreamWorkingMemory.Find(Stream);
    if (!WM)
    {
        return;
    }

    for (int32 i = WM->Num() - 1; i >= 0; --i)
    {
        if ((*WM)[i].NodeID == NodeID)
        {
            WM->RemoveAt(i);
            OnWorkingMemoryUpdated.Broadcast(Stream);
            break;
        }
    }
}

TArray<FConsciousnessWorkingMemorySlot> UMemoryConsciousnessIntegration::GetWorkingMemory(
    EConsciousnessStream Stream) const
{
    if (const TArray<FConsciousnessWorkingMemorySlot>* WM = StreamWorkingMemory.Find(Stream))
    {
        return *WM;
    }
    return TArray<FConsciousnessWorkingMemorySlot>();
}

void UMemoryConsciousnessIntegration::ClearWorkingMemory(EConsciousnessStream Stream)
{
    TArray<FConsciousnessWorkingMemorySlot>* WM = StreamWorkingMemory.Find(Stream);
    if (WM)
    {
        WM->Empty();
        OnWorkingMemoryUpdated.Broadcast(Stream);
    }
}

void UMemoryConsciousnessIntegration::UpdateWorkingMemoryAttention(EConsciousnessStream Stream,
                                                                    int64 NodeID, float Weight)
{
    TArray<FConsciousnessWorkingMemorySlot>* WM = StreamWorkingMemory.Find(Stream);
    if (!WM)
    {
        return;
    }

    for (FConsciousnessWorkingMemorySlot& Slot : *WM)
    {
        if (Slot.NodeID == NodeID)
        {
            Slot.AttentionalWeight = FMath::Clamp(Weight, 0.0f, 1.0f);
            // Boost activation based on attention
            Slot.Activation = FMath::Min(1.0f, Slot.Activation + Weight * 0.1f);
            break;
        }
    }
}

void UMemoryConsciousnessIntegration::UpdateWorkingMemoryDecay(float DeltaTime)
{
    float DecayRate = 0.05f; // 5% per second

    for (auto& Pair : StreamWorkingMemory)
    {
        for (FConsciousnessWorkingMemorySlot& Slot : Pair.Value)
        {
            // Decay activation, but slower for high-attention items
            float EffectiveDecay = DecayRate * (1.0f - 0.5f * Slot.AttentionalWeight);
            Slot.Activation = FMath::Max(0.0f, Slot.Activation - EffectiveDecay * DeltaTime);
            Slot.TimeLoaded += DeltaTime;
        }

        // Remove items with zero activation
        Pair.Value.RemoveAll([](const FConsciousnessWorkingMemorySlot& Slot) {
            return Slot.Activation <= 0.0f;
        });
    }
}

// ========================================
// MEMORY-GUIDED ATTENTION IMPLEMENTATION
// ========================================

FMemoryGuidedAttention UMemoryConsciousnessIntegration::ComputeMemoryGuidedAttention(
    EConsciousnessStream Stream)
{
    FMemoryGuidedAttention Attention;

    if (!MemorySystem)
    {
        return Attention;
    }

    // Get most active episodic memories
    TArray<int64> EpisodicNodes = MemorySystem->FindNodesByType(EMemoryNodeType::Episode, 10);
    for (int64 NodeID : EpisodicNodes)
    {
        FMemoryNode Node = MemorySystem->GetNode(NodeID);
        if (Node.Activation > 0.1f)
        {
            Attention.EpisodicWeights.Add(Node.Label, Node.Activation);
        }
    }

    // Get active concepts (semantic memory)
    TArray<int64> ConceptNodes = MemorySystem->FindNodesByType(EMemoryNodeType::Concept, 10);
    for (int64 NodeID : ConceptNodes)
    {
        FMemoryNode Node = MemorySystem->GetNode(NodeID);
        if (Node.Activation > 0.1f)
        {
            Attention.SemanticWeights.Add(Node.Label, Node.Activation);
        }
    }

    // Get active skills (procedural memory)
    TArray<int64> SkillNodes = MemorySystem->FindNodesByType(EMemoryNodeType::Skill, 5);
    for (int64 NodeID : SkillNodes)
    {
        FMemoryNode Node = MemorySystem->GetNode(NodeID);
        if (Node.Activation > 0.1f)
        {
            Attention.ProceduralWeights.Add(Node.Label, Node.Activation);
        }
    }

    // Get active intentions (goal memory)
    TArray<int64> IntentionNodes = MemorySystem->GetActiveIntentions();
    for (int64 NodeID : IntentionNodes)
    {
        FMemoryNode Node = MemorySystem->GetNode(NodeID);
        Attention.IntentionalWeights.Add(Node.Label, Node.Strength);
    }

    // Compute combined attention vector from working memory
    const TArray<FConsciousnessWorkingMemorySlot>* WM = StreamWorkingMemory.Find(Stream);
    if (WM && WM->Num() > 0)
    {
        // Determine embedding dimension from first node
        int32 EmbeddingDim = 0;
        for (const FConsciousnessWorkingMemorySlot& Slot : *WM)
        {
            FMemoryNode Node = MemorySystem->GetNode(Slot.NodeID);
            if (Node.Embedding.Num() > 0)
            {
                EmbeddingDim = Node.Embedding.Num();
                break;
            }
        }

        if (EmbeddingDim > 0)
        {
            Attention.CombinedAttention.SetNumZeroed(EmbeddingDim);
            float TotalWeight = 0.0f;

            for (const FConsciousnessWorkingMemorySlot& Slot : *WM)
            {
                FMemoryNode Node = MemorySystem->GetNode(Slot.NodeID);
                if (Node.Embedding.Num() == EmbeddingDim)
                {
                    float Weight = Slot.Activation * Slot.AttentionalWeight;
                    TotalWeight += Weight;

                    for (int32 i = 0; i < EmbeddingDim; ++i)
                    {
                        Attention.CombinedAttention[i] += Node.Embedding[i] * Weight;
                    }
                }
            }

            // Normalize
            if (TotalWeight > 0.0f)
            {
                for (float& Val : Attention.CombinedAttention)
                {
                    Val /= TotalWeight;
                }
            }
        }
    }

    return Attention;
}

TArray<float> UMemoryConsciousnessIntegration::GetCombinedAttentionVector(EConsciousnessStream Stream)
{
    FMemoryGuidedAttention Attention = ComputeMemoryGuidedAttention(Stream);
    return Attention.CombinedAttention;
}

TArray<float> UMemoryConsciousnessIntegration::ModulateWithMemoryAttention(EConsciousnessStream Stream,
                                                                            const TArray<float>& Perception)
{
    TArray<float> Modulated = Perception;
    TArray<float> AttentionVector = GetCombinedAttentionVector(Stream);

    if (AttentionVector.Num() == Perception.Num())
    {
        for (int32 i = 0; i < Perception.Num(); ++i)
        {
            // Blend perception with memory-guided attention
            float AttentionFactor = 1.0f + (AttentionVector[i] - 0.5f) * Config.MemoryAttentionWeight;
            Modulated[i] = Perception[i] * AttentionFactor;
        }
    }

    return Modulated;
}

// ========================================
// CONSCIOUSNESS-DRIVEN ENCODING IMPLEMENTATION
// ========================================

int64 UMemoryConsciousnessIntegration::EncodeConsciousnessState(EConsciousnessStream Stream,
                                                                  const FString& Label,
                                                                  const TArray<float>& StateEmbedding,
                                                                  float AttentionalWeight)
{
    if (!MemorySystem)
    {
        return -1;
    }

    // Determine node type based on stream
    EMemoryNodeType NodeType = GetNodeTypeForStream(Stream);

    // Modulate encoding strength with attention
    float EncodingStrength = 0.5f + AttentionalWeight * Config.AttentionEncodingStrength * 0.5f;
    EncodingStrength = FMath::Clamp(EncodingStrength, 0.0f, 1.0f);

    // Create memory node
    int64 NodeID = MemorySystem->CreateNode(NodeType, Label, StateEmbedding, EncodingStrength);

    if (NodeID > 0)
    {
        // Record event
        FConsciousnessMemoryEvent Event;
        Event.AccessMode = EMemoryAccessMode::PerceptualEncode;
        Event.SourceStream = Stream;
        Event.AffectedNodes.Add(NodeID);
        Event.CycleStep = CurrentCycleStep;
        Event.Timestamp = AccumulatedTime;

        if (ConsciousnessSystem)
        {
            Event.MutualAwarenessLevel = ConsciousnessSystem->GetTriadicCoherence();
        }

        RecordEvent(Event);
        OnMemoryConsciousnessEvent.Broadcast(Event, Stream);

        // Load to working memory
        LoadToWorkingMemory(Stream, NodeID);
    }

    return NodeID;
}

int64 UMemoryConsciousnessIntegration::EncodePerceptualExperience(const FPerceptualState& State,
                                                                    float EmotionalValence)
{
    if (!MemorySystem || !ReservoirComponent)
    {
        return -1;
    }

    // Compute embedding from perceptual state
    TArray<float> Embedding;

    // If we have sensations, use them to create embedding
    if (State.Sensations.Num() > 0)
    {
        // Simple hash-based embedding for sensations
        Embedding.SetNumZeroed(128);
        for (int32 i = 0; i < State.Sensations.Num(); ++i)
        {
            int32 Hash = GetTypeHash(State.Sensations[i]);
            Embedding[Hash % 128] += 1.0f;
        }

        // Normalize
        float Norm = 0.0f;
        for (float Val : Embedding)
        {
            Norm += Val * Val;
        }
        Norm = FMath::Sqrt(Norm);
        if (Norm > 0.0f)
        {
            for (float& Val : Embedding)
            {
                Val /= Norm;
            }
        }
    }
    else
    {
        // Use reservoir to get embedding
        TArray<float> DummyInput;
        DummyInput.SetNumZeroed(64);
        Embedding = ReservoirComponent->ProcessInput(DummyInput, 1);
    }

    // Create label
    FString Label = FString::Printf(TEXT("Percept_%s_%d"), *State.AttentionFocus,
                                      FMath::RandRange(0, 9999));

    // Encode with awareness levels as attention
    float Attention = (State.AwarenessOfAction + State.AwarenessOfReflection) / 2.0f;

    int64 NodeID = EncodeConsciousnessState(EConsciousnessStream::Perceiving, Label, Embedding, Attention);

    // Add emotional valence as property
    if (NodeID > 0 && FMath::Abs(EmotionalValence) > 0.1f)
    {
        MemorySystem->SetNodeProperty(NodeID, TEXT("EmotionalValence"),
                                       FString::SanitizeFloat(EmotionalValence));
    }

    return NodeID;
}

int64 UMemoryConsciousnessIntegration::EncodeActionSequence(const FActionState& State,
                                                             const TArray<TArray<float>>& ActionSequence)
{
    if (!MemorySystem || !ReservoirComponent)
    {
        return -1;
    }

    // Create label from actions
    FString Label = TEXT("ActionSeq");
    if (State.Actions.Num() > 0)
    {
        Label = FString::Printf(TEXT("ActionSeq_%s"), *State.Actions[0]);
    }

    // Encode action sequence through reservoir
    int64 NodeID = MemorySystem->EncodeTemporalPattern(ActionSequence, Label, 2);

    if (NodeID > 0)
    {
        // Store action count and emotions as properties
        MemorySystem->SetNodeProperty(NodeID, TEXT("ActionCount"),
                                       FString::FromInt(State.Actions.Num()));

        if (State.Emotions.Num() > 0)
        {
            MemorySystem->SetNodeProperty(NodeID, TEXT("PrimaryEmotion"), State.Emotions[0]);
        }

        // Record event
        FConsciousnessMemoryEvent Event;
        Event.AccessMode = EMemoryAccessMode::ActionRetrieval;
        Event.SourceStream = EConsciousnessStream::Acting;
        Event.AffectedNodes.Add(NodeID);
        Event.CycleStep = CurrentCycleStep;
        Event.Timestamp = AccumulatedTime;
        RecordEvent(Event);
    }

    return NodeID;
}

int64 UMemoryConsciousnessIntegration::EncodeReflectiveInsight(const FReflectiveState& State)
{
    if (!MemorySystem)
    {
        return -1;
    }

    // Create embedding from insights and thoughts
    TArray<float> Embedding;
    Embedding.SetNumZeroed(128);

    for (int32 i = 0; i < State.Insights.Num(); ++i)
    {
        int32 Hash = GetTypeHash(State.Insights[i]);
        Embedding[Hash % 128] += 2.0f; // Insights weighted higher
    }

    for (int32 i = 0; i < State.Thoughts.Num(); ++i)
    {
        int32 Hash = GetTypeHash(State.Thoughts[i]);
        Embedding[Hash % 128] += 1.0f;
    }

    // Normalize
    float Norm = 0.0f;
    for (float Val : Embedding)
    {
        Norm += Val * Val;
    }
    Norm = FMath::Sqrt(Norm);
    if (Norm > 0.0f)
    {
        for (float& Val : Embedding)
        {
            Val /= Norm;
        }
    }

    FString Label = TEXT("Insight");
    if (State.Insights.Num() > 0)
    {
        Label = FString::Printf(TEXT("Insight_%s"), *State.Insights[0].Left(20));
    }

    float Attention = (State.AwarenessOfPerception + State.AwarenessOfAction) / 2.0f;

    return EncodeConsciousnessState(EConsciousnessStream::Reflecting, Label, Embedding, Attention);
}

// ========================================
// CROSS-STREAM ASSOCIATION IMPLEMENTATION
// ========================================

int64 UMemoryConsciousnessIntegration::CreateCrossStreamAssociation(int64 NodeA,
                                                                      EConsciousnessStream StreamA,
                                                                      int64 NodeB,
                                                                      EConsciousnessStream StreamB,
                                                                      float AssociationStrength)
{
    if (!MemorySystem || NodeA == NodeB)
    {
        return -1;
    }

    // Determine relation type based on stream pair
    ESemanticRelation Relation = GetRelationForStreamPair(StreamA, StreamB);

    // Create edge
    int64 EdgeID = MemorySystem->CreateBidirectionalEdge(NodeA, NodeB, Relation, AssociationStrength);

    if (EdgeID > 0)
    {
        // Track cross-stream edge
        CrossStreamEdges.FindOrAdd(NodeA).AddUnique(EdgeID);
        CrossStreamEdges.FindOrAdd(NodeB).AddUnique(EdgeID);

        OnCrossStreamAssociation.Broadcast(NodeA, NodeB);

        // Record event
        FConsciousnessMemoryEvent Event;
        Event.AccessMode = EMemoryAccessMode::CrossStreamAssociation;
        Event.AffectedNodes.Add(NodeA);
        Event.AffectedNodes.Add(NodeB);
        Event.CycleStep = CurrentCycleStep;
        Event.Timestamp = AccumulatedTime;
        RecordEvent(Event);
    }

    return EdgeID;
}

TArray<int64> UMemoryConsciousnessIntegration::GetCrossStreamAssociations(int64 NodeID) const
{
    TArray<int64> Result;

    if (!MemorySystem)
    {
        return Result;
    }

    // Get all edges from/to this node
    TArray<FMemoryEdge> OutEdges = MemorySystem->GetOutgoingEdges(NodeID);
    TArray<FMemoryEdge> InEdges = MemorySystem->GetIncomingEdges(NodeID);

    // Collect unique associated nodes
    TSet<int64> AssociatedNodes;

    for (const FMemoryEdge& Edge : OutEdges)
    {
        if (Edge.bBidirectional)
        {
            AssociatedNodes.Add(Edge.TargetNodeID);
        }
    }

    for (const FMemoryEdge& Edge : InEdges)
    {
        if (Edge.bBidirectional)
        {
            AssociatedNodes.Add(Edge.SourceNodeID);
        }
    }

    Result = AssociatedNodes.Array();
    return Result;
}

void UMemoryConsciousnessIntegration::PropagateAcrossStreams(int64 SourceNodeID, float Activation)
{
    if (!MemorySystem || !Config.bEnableCrossStreamAssociation)
    {
        return;
    }

    // Use hypergraph's spreading activation
    MemorySystem->SpreadActivation(SourceNodeID, Activation);
}

// ========================================
// GOAL-DIRECTED RETRIEVAL IMPLEMENTATION
// ========================================

TArray<int64> UMemoryConsciousnessIntegration::RetrieveGoalRelevantMemories(int32 MaxResults)
{
    TArray<int64> Result;

    if (!MemorySystem)
    {
        return Result;
    }

    // Get active intentions
    TArray<int64> Intentions = MemorySystem->GetActiveIntentions();

    if (Intentions.Num() == 0)
    {
        return Result;
    }

    // For each intention, find related memories via graph traversal
    TSet<int64> RelevantSet;

    for (int64 IntentionID : Intentions)
    {
        // Get neighbors of intention node
        TArray<int64> Neighbors = MemorySystem->GetNeighbors(IntentionID, true, true);
        for (int64 NeighborID : Neighbors)
        {
            RelevantSet.Add(NeighborID);
        }

        // Also spread activation from intention
        MemorySystem->SpreadActivation(IntentionID, 0.5f);
    }

    // Get most activated nodes overall
    TArray<int64> MostActive = MemorySystem->GetMostActiveNodes(MaxResults);
    for (int64 NodeID : MostActive)
    {
        RelevantSet.Add(NodeID);
    }

    Result = RelevantSet.Array();

    // Limit to max results
    if (Result.Num() > MaxResults)
    {
        Result.SetNum(MaxResults);
    }

    return Result;
}

TArray<int64> UMemoryConsciousnessIntegration::RetrieveContextualMemories(
    const TArray<float>& ContextEmbedding, int32 MaxResults)
{
    if (!MemorySystem)
    {
        return TArray<int64>();
    }

    return MemorySystem->FindSimilarNodes(ContextEmbedding, Config.RetrievalThreshold, MaxResults);
}

TArray<int64> UMemoryConsciousnessIntegration::RetrieveActionPatterns(
    const TArray<float>& SituationEmbedding, int32 MaxResults)
{
    TArray<int64> Result;

    if (!MemorySystem)
    {
        return Result;
    }

    // First get similar nodes
    TArray<int64> SimilarNodes = MemorySystem->FindSimilarNodes(SituationEmbedding,
                                                                  Config.RetrievalThreshold, 20);

    // Filter for skill/schema nodes only
    for (int64 NodeID : SimilarNodes)
    {
        FMemoryNode Node = MemorySystem->GetNode(NodeID);
        if (Node.NodeType == EMemoryNodeType::Skill || Node.NodeType == EMemoryNodeType::Schema)
        {
            Result.Add(NodeID);
            if (Result.Num() >= MaxResults)
            {
                break;
            }
        }
    }

    return Result;
}

// ========================================
// COGNITIVE CYCLE INTEGRATION IMPLEMENTATION
// ========================================

void UMemoryConsciousnessIntegration::ProcessCycleStep(int32 CycleStep, EConsciousnessStream ActiveStream)
{
    if (!MemorySystem || !ConsciousnessSystem)
    {
        return;
    }

    // Determine memory access mode based on stream
    EMemoryAccessMode Mode;
    switch (ActiveStream)
    {
    case EConsciousnessStream::Perceiving:
        Mode = EMemoryAccessMode::PerceptualEncode;
        break;
    case EConsciousnessStream::Acting:
        Mode = EMemoryAccessMode::ActionRetrieval;
        break;
    case EConsciousnessStream::Reflecting:
        Mode = EMemoryAccessMode::ReflectiveConsolidate;
        break;
    default:
        Mode = EMemoryAccessMode::CrossStreamAssociation;
        break;
    }

    // Get stream embedding
    TArray<float> StreamEmbedding = ComputeStreamEmbedding(ActiveStream);

    // Bind stream to relevant memories
    if (StreamEmbedding.Num() > 0)
    {
        BindStreamToMemory(ActiveStream, StreamEmbedding);
    }

    // Special processing at certain steps
    switch (CycleStep)
    {
    case 1: // Beginning of cycle - refresh working memory
        if (Config.bEnableGoalDirectedRetrieval)
        {
            TArray<int64> GoalRelevant = RetrieveGoalRelevantMemories(3);
            for (int64 NodeID : GoalRelevant)
            {
                LoadToWorkingMemory(EConsciousnessStream::Reflecting, NodeID);
            }
        }
        break;

    case 4: // End of first triad - create associations
    case 8: // End of second triad
    case 12: // End of cycle
        if (Config.bEnableCrossStreamAssociation)
        {
            // Associate active working memory items across streams
            const TArray<FConsciousnessWorkingMemorySlot>* PerceivingWM =
                StreamWorkingMemory.Find(EConsciousnessStream::Perceiving);
            const TArray<FConsciousnessWorkingMemorySlot>* ActingWM =
                StreamWorkingMemory.Find(EConsciousnessStream::Acting);
            const TArray<FConsciousnessWorkingMemorySlot>* ReflectingWM =
                StreamWorkingMemory.Find(EConsciousnessStream::Reflecting);

            if (PerceivingWM && ActingWM && PerceivingWM->Num() > 0 && ActingWM->Num() > 0)
            {
                CreateCrossStreamAssociation(
                    (*PerceivingWM)[0].NodeID, EConsciousnessStream::Perceiving,
                    (*ActingWM)[0].NodeID, EConsciousnessStream::Acting,
                    0.5f);
            }

            if (ActingWM && ReflectingWM && ActingWM->Num() > 0 && ReflectingWM->Num() > 0)
            {
                CreateCrossStreamAssociation(
                    (*ActingWM)[0].NodeID, EConsciousnessStream::Acting,
                    (*ReflectingWM)[0].NodeID, EConsciousnessStream::Reflecting,
                    0.5f);
            }
        }
        break;
    }

    // Consolidate at end of cycle
    if (CycleStep == 12)
    {
        ConsolidateCycleMemories();
    }
}

void UMemoryConsciousnessIntegration::HandleTriadicSynchronization(int32 SyncPoint)
{
    if (!MemorySystem)
    {
        return;
    }

    // At triadic sync points, create hyperedge linking active memories from all streams
    TArray<int64> MemberNodes;
    TArray<FString> Roles;

    for (const auto& Pair : StreamWorkingMemory)
    {
        if (Pair.Value.Num() > 0)
        {
            MemberNodes.Add(Pair.Value[0].NodeID);

            switch (Pair.Key)
            {
            case EConsciousnessStream::Perceiving:
                Roles.Add(TEXT("Perceiving"));
                break;
            case EConsciousnessStream::Acting:
                Roles.Add(TEXT("Acting"));
                break;
            case EConsciousnessStream::Reflecting:
                Roles.Add(TEXT("Reflecting"));
                break;
            }
        }
    }

    if (MemberNodes.Num() >= 2)
    {
        FString HyperedgeType = FString::Printf(TEXT("TriadicSync_%d"), SyncPoint);
        MemorySystem->CreateHyperedge(MemberNodes, Roles, HyperedgeType, 0.7f);
    }
}

void UMemoryConsciousnessIntegration::ConsolidateCycleMemories()
{
    if (!MemorySystem)
    {
        return;
    }

    // Trigger memory system consolidation
    MemorySystem->RunConsolidation();

    // Boost strength of frequently accessed working memory items
    for (const auto& Pair : StreamWorkingMemory)
    {
        for (const FConsciousnessWorkingMemorySlot& Slot : Pair.Value)
        {
            if (Slot.AttentionalWeight > 0.7f)
            {
                MemorySystem->BoostNodeActivation(Slot.NodeID, 0.1f);
            }
        }
    }
}

// ========================================
// INTERNAL METHODS
// ========================================

void UMemoryConsciousnessIntegration::ProcessPerceivingStream(float DeltaTime)
{
    if (!ConsciousnessSystem)
    {
        return;
    }

    // Get current perceptual state
    const FSharedConsciousnessState& State = ConsciousnessSystem->SharedState;

    // Update retrieval cue from perceptual state
    TArray<float> PerceptualCue;
    PerceptualCue.SetNumZeroed(64);

    // Simple encoding of perceptual patterns
    for (int32 i = 0; i < State.PerceptualState.Patterns.Num() && i < 64; ++i)
    {
        PerceptualCue[i] = 1.0f;
    }

    UpdateRetrievalCue(EConsciousnessStream::Perceiving, PerceptualCue);
}

void UMemoryConsciousnessIntegration::ProcessActingStream(float DeltaTime)
{
    if (!ConsciousnessSystem)
    {
        return;
    }

    // Get current action state
    const FSharedConsciousnessState& State = ConsciousnessSystem->SharedState;

    // For acting stream, retrieve relevant procedural memories
    FStreamMemoryBinding* Binding = StreamBindings.Find(EConsciousnessStream::Acting);
    if (Binding && Binding->RetrievalCue.Num() > 0)
    {
        TArray<int64> ActionPatterns = RetrieveActionPatterns(Binding->RetrievalCue, 3);
        for (int64 NodeID : ActionPatterns)
        {
            LoadToWorkingMemory(EConsciousnessStream::Acting, NodeID);
        }
    }
}

void UMemoryConsciousnessIntegration::ProcessReflectingStream(float DeltaTime)
{
    if (!ConsciousnessSystem || !MemorySystem)
    {
        return;
    }

    // Get current reflective state
    const FSharedConsciousnessState& State = ConsciousnessSystem->SharedState;

    // Reflecting stream triggers abstraction detection periodically
    static float AbstractionTimer = 0.0f;
    AbstractionTimer += DeltaTime;

    if (AbstractionTimer >= 5.0f) // Every 5 seconds
    {
        AbstractionTimer = 0.0f;
        MemorySystem->DetectAbstractions(0.7f);
    }
}

void UMemoryConsciousnessIntegration::RecordEvent(const FConsciousnessMemoryEvent& Event)
{
    EventHistory.Add(Event);

    // Limit history size
    while (EventHistory.Num() > MaxEventHistorySize)
    {
        EventHistory.RemoveAt(0);
    }
}

TArray<float> UMemoryConsciousnessIntegration::ComputeStreamEmbedding(EConsciousnessStream Stream) const
{
    TArray<float> Embedding;

    if (!ReservoirComponent)
    {
        return Embedding;
    }

    // Get stream state from reservoir
    int32 StreamID = static_cast<int32>(Stream) + 1;
    FCognitiveStreamState StreamState = ReservoirComponent->GetStreamState(StreamID);

    return StreamState.ReservoirState.ActivationState;
}

EMemoryNodeType UMemoryConsciousnessIntegration::GetNodeTypeForStream(EConsciousnessStream Stream) const
{
    switch (Stream)
    {
    case EConsciousnessStream::Perceiving:
        return EMemoryNodeType::Percept;
    case EConsciousnessStream::Acting:
        return EMemoryNodeType::Skill;
    case EConsciousnessStream::Reflecting:
        return EMemoryNodeType::Concept;
    default:
        return EMemoryNodeType::Episode;
    }
}

ESemanticRelation UMemoryConsciousnessIntegration::GetRelationForStreamPair(
    EConsciousnessStream StreamA, EConsciousnessStream StreamB) const
{
    if (StreamA == EConsciousnessStream::Perceiving && StreamB == EConsciousnessStream::Acting)
    {
        return ESemanticRelation::TRIGGERS;
    }
    else if (StreamA == EConsciousnessStream::Acting && StreamB == EConsciousnessStream::Perceiving)
    {
        return ESemanticRelation::RESULTS_IN;
    }
    else if (StreamA == EConsciousnessStream::Acting && StreamB == EConsciousnessStream::Reflecting)
    {
        return ESemanticRelation::DERIVED_FROM;
    }
    else if (StreamA == EConsciousnessStream::Reflecting && StreamB == EConsciousnessStream::Acting)
    {
        return ESemanticRelation::GUIDES;
    }
    else if (StreamA == EConsciousnessStream::Perceiving && StreamB == EConsciousnessStream::Reflecting)
    {
        return ESemanticRelation::INFORMS;
    }
    else if (StreamA == EConsciousnessStream::Reflecting && StreamB == EConsciousnessStream::Perceiving)
    {
        return ESemanticRelation::PREDICTS;
    }

    return ESemanticRelation::ASSOCIATED_WITH;
}

// ========================================
// STATISTICS & DIAGNOSTICS
// ========================================

int32 UMemoryConsciousnessIntegration::GetTotalActiveBindings() const
{
    int32 Total = 0;

    for (const auto& Pair : StreamBindings)
    {
        Total += Pair.Value.ActiveNodes.Num();
    }

    return Total;
}

TArray<FConsciousnessMemoryEvent> UMemoryConsciousnessIntegration::GetRecentEvents(
    EConsciousnessStream Stream, int32 Count) const
{
    TArray<FConsciousnessMemoryEvent> Result;

    for (int32 i = EventHistory.Num() - 1; i >= 0 && Result.Num() < Count; --i)
    {
        if (EventHistory[i].SourceStream == Stream)
        {
            Result.Add(EventHistory[i]);
        }
    }

    return Result;
}

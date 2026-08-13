// HypergraphMemorySystem.cpp
// Implementation of hypergraph-based unified memory architecture

#include "HypergraphMemorySystem.h"
#include "../Reservoir/DeepTreeEchoReservoir.h"
#include "../Taskflow/TaskflowCognitiveScheduler.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformTime.h"

UHypergraphMemorySystem::UHypergraphMemorySystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    // Default configuration
    EmbeddingDimension = 128;
    bEnableAutoConsolidation = true;
    ConsolidationInterval = 5.0f;

    // Default spreading activation config
    SpreadingConfig.DecayFactor = 0.5f;
    SpreadingConfig.MaxDepth = 3;
    SpreadingConfig.MinActivation = 0.05f;
    SpreadingConfig.bUseEdgeWeights = true;
    SpreadingConfig.bParallelSpreading = true;

    // Default consolidation config
    ConsolidationConfig.StrengthDecayRate = 0.001f;
    ConsolidationConfig.ActivationDecayRate = 0.1f;
    ConsolidationConfig.PruningThreshold = 0.05f;
    ConsolidationConfig.ReplayStrength = 1.2f;
    ConsolidationConfig.AbstractionThreshold = 0.8f;
    ConsolidationConfig.bEnableReservoirIntegration = true;
}

void UHypergraphMemorySystem::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
}

void UHypergraphMemorySystem::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        ReservoirComponent = Owner->FindComponentByClass<UDeepTreeEchoReservoir>();
        TaskflowScheduler = Owner->FindComponentByClass<UTaskflowCognitiveScheduler>();
        LegacyMemorySystem = Owner->FindComponentByClass<UEpisodicMemorySystem>();
    }
}

void UHypergraphMemorySystem::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AccumulatedTime += DeltaTime;

    // Decay activations
    DecayActivations(DeltaTime);

    // Run consolidation periodically
    if (bEnableAutoConsolidation && (AccumulatedTime - LastConsolidationTime) >= ConsolidationInterval)
    {
        RunConsolidation();
        LastConsolidationTime = AccumulatedTime;
    }
}

// ========================================
// NODE MANAGEMENT
// ========================================

int64 UHypergraphMemorySystem::CreateNode(EMemoryNodeType NodeType, const FString& Label,
                                           const TArray<float>& Embedding, float InitialStrength)
{
    FScopeLock Lock(&GraphLock);

    FMemoryNode Node;
    Node.NodeID = NextNodeID++;
    Node.NodeType = NodeType;
    Node.Label = Label;
    Node.Embedding = Embedding;
    Node.Strength = FMath::Clamp(InitialStrength, 0.0f, 1.0f);
    Node.CreationTime = AccumulatedTime;
    Node.LastAccessTime = AccumulatedTime;
    Node.AccessCount = 1;

    Nodes.Add(Node.NodeID, Node);

    // Update type index
    NodeTypeIndex.FindOrAdd(NodeType).Add(Node.NodeID);

    OnNodeCreated.Broadcast(Node.NodeID, NodeType);

    return Node.NodeID;
}

FMemoryNode UHypergraphMemorySystem::GetNode(int64 NodeID) const
{
    FScopeLock Lock(&GraphLock);

    if (const FMemoryNode* Node = Nodes.Find(NodeID))
    {
        return *Node;
    }
    return FMemoryNode();
}

void UHypergraphMemorySystem::UpdateNodeEmbedding(int64 NodeID, const TArray<float>& NewEmbedding)
{
    FScopeLock Lock(&GraphLock);

    if (FMemoryNode* Node = Nodes.Find(NodeID))
    {
        Node->Embedding = NewEmbedding;
        RecordAccess(NodeID);
    }
}

void UHypergraphMemorySystem::SetNodeActivation(int64 NodeID, float Activation)
{
    FScopeLock Lock(&GraphLock);

    if (FMemoryNode* Node = Nodes.Find(NodeID))
    {
        float OldActivation = Node->Activation;
        Node->Activation = FMath::Clamp(Activation, 0.0f, 1.0f);

        if (Node->Activation > 0.1f && OldActivation <= 0.1f)
        {
            OnNodeActivated.Broadcast(NodeID, Node->Activation);
        }
    }
}

void UHypergraphMemorySystem::BoostNodeActivation(int64 NodeID, float Amount)
{
    FScopeLock Lock(&GraphLock);

    if (FMemoryNode* Node = Nodes.Find(NodeID))
    {
        float OldActivation = Node->Activation;
        Node->Activation = FMath::Clamp(Node->Activation + Amount, 0.0f, 1.0f);
        RecordAccess(NodeID);

        if (Node->Activation > 0.1f && OldActivation <= 0.1f)
        {
            OnNodeActivated.Broadcast(NodeID, Node->Activation);
        }
    }
}

void UHypergraphMemorySystem::SetNodeProperty(int64 NodeID, const FString& Key, const FString& Value)
{
    FScopeLock Lock(&GraphLock);

    if (FMemoryNode* Node = Nodes.Find(NodeID))
    {
        Node->Properties.Add(Key, Value);
    }
}

FString UHypergraphMemorySystem::GetNodeProperty(int64 NodeID, const FString& Key) const
{
    FScopeLock Lock(&GraphLock);

    if (const FMemoryNode* Node = Nodes.Find(NodeID))
    {
        if (const FString* Value = Node->Properties.Find(Key))
        {
            return *Value;
        }
    }
    return FString();
}

bool UHypergraphMemorySystem::DeleteNode(int64 NodeID)
{
    FScopeLock Lock(&GraphLock);

    FMemoryNode* Node = Nodes.Find(NodeID);
    if (!Node)
    {
        return false;
    }

    // Delete connected edges
    TArray<int64> EdgesToDelete;
    EdgesToDelete.Append(Node->OutgoingEdges);
    EdgesToDelete.Append(Node->IncomingEdges);

    for (int64 EdgeID : EdgesToDelete)
    {
        DeleteEdge(EdgeID);
    }

    // Remove from hyperedges
    for (int64 HyperedgeID : Node->HyperedgeMembership)
    {
        if (FMemoryHyperedge* Hyperedge = Hyperedges.Find(HyperedgeID))
        {
            Hyperedge->MemberNodes.Remove(NodeID);
        }
    }

    // Remove from type index
    if (TArray<int64>* TypeNodes = NodeTypeIndex.Find(Node->NodeType))
    {
        TypeNodes->Remove(NodeID);
    }

    // Remove intentional state if exists
    IntentionalStates.Remove(NodeID);

    // Remove node
    Nodes.Remove(NodeID);

    return true;
}

bool UHypergraphMemorySystem::NodeExists(int64 NodeID) const
{
    FScopeLock Lock(&GraphLock);
    return Nodes.Contains(NodeID);
}

// ========================================
// EDGE MANAGEMENT
// ========================================

int64 UHypergraphMemorySystem::CreateEdge(int64 SourceNodeID, int64 TargetNodeID,
                                           ESemanticRelation RelationType, float Weight)
{
    FScopeLock Lock(&GraphLock);

    FMemoryNode* SourceNode = Nodes.Find(SourceNodeID);
    FMemoryNode* TargetNode = Nodes.Find(TargetNodeID);

    if (!SourceNode || !TargetNode)
    {
        return 0;
    }

    FMemoryEdge Edge;
    Edge.EdgeID = NextEdgeID++;
    Edge.SourceNodeID = SourceNodeID;
    Edge.TargetNodeID = TargetNodeID;
    Edge.RelationType = RelationType;
    Edge.Weight = FMath::Clamp(Weight, 0.0f, 1.0f);
    Edge.CreationTime = AccumulatedTime;

    Edges.Add(Edge.EdgeID, Edge);

    // Update node adjacency
    SourceNode->OutgoingEdges.Add(Edge.EdgeID);
    TargetNode->IncomingEdges.Add(Edge.EdgeID);

    // Update relation index
    UpdateEdgeIndex(Edge.EdgeID, RelationType, true);

    OnEdgeCreated.Broadcast(Edge.EdgeID, SourceNodeID, TargetNodeID);

    return Edge.EdgeID;
}

int64 UHypergraphMemorySystem::CreateBidirectionalEdge(int64 NodeA, int64 NodeB,
                                                        ESemanticRelation RelationType, float Weight)
{
    int64 EdgeID = CreateEdge(NodeA, NodeB, RelationType, Weight);

    if (EdgeID > 0)
    {
        FScopeLock Lock(&GraphLock);
        if (FMemoryEdge* Edge = Edges.Find(EdgeID))
        {
            Edge->bBidirectional = true;

            // Also add reverse reference
            if (FMemoryNode* NodeBPtr = Nodes.Find(NodeB))
            {
                NodeBPtr->OutgoingEdges.Add(EdgeID);
            }
            if (FMemoryNode* NodeAPtr = Nodes.Find(NodeA))
            {
                NodeAPtr->IncomingEdges.Add(EdgeID);
            }
        }
    }

    return EdgeID;
}

int64 UHypergraphMemorySystem::CreateCustomEdge(int64 SourceNodeID, int64 TargetNodeID,
                                                 const FString& CustomRelation, float Weight)
{
    int64 EdgeID = CreateEdge(SourceNodeID, TargetNodeID, ESemanticRelation::CUSTOM, Weight);

    if (EdgeID > 0)
    {
        FScopeLock Lock(&GraphLock);
        if (FMemoryEdge* Edge = Edges.Find(EdgeID))
        {
            Edge->CustomRelationLabel = CustomRelation;
        }
    }

    return EdgeID;
}

FMemoryEdge UHypergraphMemorySystem::GetEdge(int64 EdgeID) const
{
    FScopeLock Lock(&GraphLock);

    if (const FMemoryEdge* Edge = Edges.Find(EdgeID))
    {
        return *Edge;
    }
    return FMemoryEdge();
}

TArray<FMemoryEdge> UHypergraphMemorySystem::GetEdgesBetween(int64 NodeA, int64 NodeB) const
{
    TArray<FMemoryEdge> Result;
    FScopeLock Lock(&GraphLock);

    const FMemoryNode* Node = Nodes.Find(NodeA);
    if (!Node)
    {
        return Result;
    }

    for (int64 EdgeID : Node->OutgoingEdges)
    {
        if (const FMemoryEdge* Edge = Edges.Find(EdgeID))
        {
            if (Edge->TargetNodeID == NodeB)
            {
                Result.Add(*Edge);
            }
        }
    }

    for (int64 EdgeID : Node->IncomingEdges)
    {
        if (const FMemoryEdge* Edge = Edges.Find(EdgeID))
        {
            if (Edge->SourceNodeID == NodeB)
            {
                Result.Add(*Edge);
            }
        }
    }

    return Result;
}

TArray<FMemoryEdge> UHypergraphMemorySystem::GetOutgoingEdges(int64 NodeID) const
{
    TArray<FMemoryEdge> Result;
    FScopeLock Lock(&GraphLock);

    const FMemoryNode* Node = Nodes.Find(NodeID);
    if (!Node)
    {
        return Result;
    }

    for (int64 EdgeID : Node->OutgoingEdges)
    {
        if (const FMemoryEdge* Edge = Edges.Find(EdgeID))
        {
            Result.Add(*Edge);
        }
    }

    return Result;
}

TArray<FMemoryEdge> UHypergraphMemorySystem::GetIncomingEdges(int64 NodeID) const
{
    TArray<FMemoryEdge> Result;
    FScopeLock Lock(&GraphLock);

    const FMemoryNode* Node = Nodes.Find(NodeID);
    if (!Node)
    {
        return Result;
    }

    for (int64 EdgeID : Node->IncomingEdges)
    {
        if (const FMemoryEdge* Edge = Edges.Find(EdgeID))
        {
            Result.Add(*Edge);
        }
    }

    return Result;
}

void UHypergraphMemorySystem::UpdateEdgeWeight(int64 EdgeID, float NewWeight)
{
    FScopeLock Lock(&GraphLock);

    if (FMemoryEdge* Edge = Edges.Find(EdgeID))
    {
        Edge->Weight = FMath::Clamp(NewWeight, 0.0f, 1.0f);
    }
}

bool UHypergraphMemorySystem::DeleteEdge(int64 EdgeID)
{
    FScopeLock Lock(&GraphLock);

    FMemoryEdge* Edge = Edges.Find(EdgeID);
    if (!Edge)
    {
        return false;
    }

    // Remove from source node
    if (FMemoryNode* SourceNode = Nodes.Find(Edge->SourceNodeID))
    {
        SourceNode->OutgoingEdges.Remove(EdgeID);
    }

    // Remove from target node
    if (FMemoryNode* TargetNode = Nodes.Find(Edge->TargetNodeID))
    {
        TargetNode->IncomingEdges.Remove(EdgeID);
    }

    // Update index
    UpdateEdgeIndex(EdgeID, Edge->RelationType, false);

    Edges.Remove(EdgeID);
    return true;
}

// ========================================
// HYPEREDGE MANAGEMENT
// ========================================

int64 UHypergraphMemorySystem::CreateHyperedge(const TArray<int64>& MemberNodes,
                                                const TArray<FString>& Roles,
                                                const FString& HyperedgeType, float Weight)
{
    FScopeLock Lock(&GraphLock);

    // Validate all member nodes exist
    for (int64 NodeID : MemberNodes)
    {
        if (!Nodes.Contains(NodeID))
        {
            return 0;
        }
    }

    FMemoryHyperedge Hyperedge;
    Hyperedge.HyperedgeID = NextHyperedgeID++;
    Hyperedge.MemberNodes = MemberNodes;
    Hyperedge.MemberRoles = Roles;
    Hyperedge.HyperedgeType = HyperedgeType;
    Hyperedge.Weight = FMath::Clamp(Weight, 0.0f, 1.0f);
    Hyperedge.CreationTime = AccumulatedTime;

    Hyperedges.Add(Hyperedge.HyperedgeID, Hyperedge);

    // Update node membership
    for (int64 NodeID : MemberNodes)
    {
        if (FMemoryNode* Node = Nodes.Find(NodeID))
        {
            Node->HyperedgeMembership.Add(Hyperedge.HyperedgeID);
        }
    }

    return Hyperedge.HyperedgeID;
}

FMemoryHyperedge UHypergraphMemorySystem::GetHyperedge(int64 HyperedgeID) const
{
    FScopeLock Lock(&GraphLock);

    if (const FMemoryHyperedge* Hyperedge = Hyperedges.Find(HyperedgeID))
    {
        return *Hyperedge;
    }
    return FMemoryHyperedge();
}

TArray<FMemoryHyperedge> UHypergraphMemorySystem::GetNodeHyperedges(int64 NodeID) const
{
    TArray<FMemoryHyperedge> Result;
    FScopeLock Lock(&GraphLock);

    const FMemoryNode* Node = Nodes.Find(NodeID);
    if (!Node)
    {
        return Result;
    }

    for (int64 HyperedgeID : Node->HyperedgeMembership)
    {
        if (const FMemoryHyperedge* Hyperedge = Hyperedges.Find(HyperedgeID))
        {
            Result.Add(*Hyperedge);
        }
    }

    return Result;
}

bool UHypergraphMemorySystem::DeleteHyperedge(int64 HyperedgeID)
{
    FScopeLock Lock(&GraphLock);

    FMemoryHyperedge* Hyperedge = Hyperedges.Find(HyperedgeID);
    if (!Hyperedge)
    {
        return false;
    }

    // Remove from member nodes
    for (int64 NodeID : Hyperedge->MemberNodes)
    {
        if (FMemoryNode* Node = Nodes.Find(NodeID))
        {
            Node->HyperedgeMembership.Remove(HyperedgeID);
        }
    }

    Hyperedges.Remove(HyperedgeID);
    return true;
}

// ========================================
// INTENTIONAL MEMORY (BDI)
// ========================================

int64 UHypergraphMemorySystem::CreateBelief(const FString& Proposition,
                                             const TArray<int64>& ContentNodes,
                                             EBeliefModality Modality, float Confidence)
{
    int64 NodeID = CreateNode(EMemoryNodeType::Belief, Proposition, {}, 0.7f);

    if (NodeID > 0)
    {
        FScopeLock Lock(&GraphLock);

        FIntentionalState State;
        State.NodeID = NodeID;
        State.Proposition = Proposition;
        State.ContentNodes = ContentNodes;
        State.Modality = Modality;
        State.Intensity = Confidence;

        IntentionalStates.Add(NodeID, State);

        // Create BELIEVES edges to content nodes
        for (int64 ContentNodeID : ContentNodes)
        {
            CreateEdge(NodeID, ContentNodeID, ESemanticRelation::BELIEVES, Confidence);
        }

        if (FMemoryNode* Node = Nodes.Find(NodeID))
        {
            Node->Confidence = Confidence;
        }
    }

    return NodeID;
}

int64 UHypergraphMemorySystem::CreateDesire(const FString& Goal,
                                             const TArray<int64>& ContentNodes,
                                             float Intensity, float Priority)
{
    int64 NodeID = CreateNode(EMemoryNodeType::Desire, Goal, {}, 0.6f);

    if (NodeID > 0)
    {
        FScopeLock Lock(&GraphLock);

        FIntentionalState State;
        State.NodeID = NodeID;
        State.Proposition = Goal;
        State.ContentNodes = ContentNodes;
        State.Intensity = Intensity;
        State.Priority = Priority;

        IntentionalStates.Add(NodeID, State);

        // Create DESIRES edges to content nodes
        for (int64 ContentNodeID : ContentNodes)
        {
            CreateEdge(NodeID, ContentNodeID, ESemanticRelation::DESIRES, Intensity);
        }
    }

    return NodeID;
}

int64 UHypergraphMemorySystem::CreateIntention(const FString& Plan,
                                                const TArray<int64>& ContentNodes,
                                                int64 DesireNodeID, float Priority)
{
    int64 NodeID = CreateNode(EMemoryNodeType::Intention, Plan, {}, 0.8f);

    if (NodeID > 0)
    {
        FScopeLock Lock(&GraphLock);

        FIntentionalState State;
        State.NodeID = NodeID;
        State.Proposition = Plan;
        State.ContentNodes = ContentNodes;
        State.Status = EIntentionStatus::Deliberating;
        State.Priority = Priority;

        IntentionalStates.Add(NodeID, State);

        // Link to desire
        if (DesireNodeID > 0)
        {
            CreateEdge(NodeID, DesireNodeID, ESemanticRelation::DERIVED_FROM, 1.0f);
        }

        // Create INTENDS edges to content nodes
        for (int64 ContentNodeID : ContentNodes)
        {
            CreateEdge(NodeID, ContentNodeID, ESemanticRelation::INTENDS, Priority);
        }
    }

    return NodeID;
}

void UHypergraphMemorySystem::UpdateIntentionStatus(int64 IntentionNodeID, EIntentionStatus NewStatus)
{
    FScopeLock Lock(&GraphLock);

    if (FIntentionalState* State = IntentionalStates.Find(IntentionNodeID))
    {
        State->Status = NewStatus;
        OnIntentionUpdated.Broadcast(IntentionNodeID, NewStatus);

        // Update node strength based on status
        if (FMemoryNode* Node = Nodes.Find(IntentionNodeID))
        {
            switch (NewStatus)
            {
            case EIntentionStatus::Committed:
            case EIntentionStatus::Executing:
                Node->Strength = FMath::Min(Node->Strength * 1.2f, 1.0f);
                break;
            case EIntentionStatus::Achieved:
                Node->Strength = 0.9f;
                break;
            case EIntentionStatus::Failed:
            case EIntentionStatus::Suspended:
                Node->Strength *= 0.8f;
                break;
            default:
                break;
            }
        }
    }
}

FIntentionalState UHypergraphMemorySystem::GetIntentionalState(int64 NodeID) const
{
    FScopeLock Lock(&GraphLock);

    if (const FIntentionalState* State = IntentionalStates.Find(NodeID))
    {
        return *State;
    }
    return FIntentionalState();
}

TArray<int64> UHypergraphMemorySystem::GetActiveBeliefs(float MinConfidence) const
{
    TArray<int64> Result;
    FScopeLock Lock(&GraphLock);

    for (const auto& Pair : IntentionalStates)
    {
        const FIntentionalState& State = Pair.Value;
        if (const FMemoryNode* Node = Nodes.Find(State.NodeID))
        {
            if (Node->NodeType == EMemoryNodeType::Belief &&
                Node->Confidence >= MinConfidence &&
                State.Modality != EBeliefModality::Rejected)
            {
                Result.Add(State.NodeID);
            }
        }
    }

    return Result;
}

TArray<int64> UHypergraphMemorySystem::GetActiveDesires(float MinIntensity) const
{
    TArray<int64> Result;
    FScopeLock Lock(&GraphLock);

    for (const auto& Pair : IntentionalStates)
    {
        const FIntentionalState& State = Pair.Value;
        if (const FMemoryNode* Node = Nodes.Find(State.NodeID))
        {
            if (Node->NodeType == EMemoryNodeType::Desire &&
                State.Intensity >= MinIntensity)
            {
                Result.Add(State.NodeID);
            }
        }
    }

    return Result;
}

TArray<int64> UHypergraphMemorySystem::GetActiveIntentions() const
{
    TArray<int64> Result;
    FScopeLock Lock(&GraphLock);

    for (const auto& Pair : IntentionalStates)
    {
        const FIntentionalState& State = Pair.Value;
        if (const FMemoryNode* Node = Nodes.Find(State.NodeID))
        {
            if (Node->NodeType == EMemoryNodeType::Intention &&
                (State.Status == EIntentionStatus::Committed ||
                 State.Status == EIntentionStatus::Executing ||
                 State.Status == EIntentionStatus::Deliberating))
            {
                Result.Add(State.NodeID);
            }
        }
    }

    return Result;
}

TArray<int64> UHypergraphMemorySystem::FindContradictingBeliefs(int64 BeliefNodeID) const
{
    TArray<int64> Contradictions;
    FScopeLock Lock(&GraphLock);

    const FMemoryNode* BeliefNode = Nodes.Find(BeliefNodeID);
    if (!BeliefNode || BeliefNode->NodeType != EMemoryNodeType::Belief)
    {
        return Contradictions;
    }

    // Check for CONTRADICTS edges
    for (int64 EdgeID : BeliefNode->OutgoingEdges)
    {
        if (const FMemoryEdge* Edge = Edges.Find(EdgeID))
        {
            if (Edge->RelationType == ESemanticRelation::CONTRADICTS)
            {
                Contradictions.Add(Edge->TargetNodeID);
            }
        }
    }

    for (int64 EdgeID : BeliefNode->IncomingEdges)
    {
        if (const FMemoryEdge* Edge = Edges.Find(EdgeID))
        {
            if (Edge->RelationType == ESemanticRelation::CONTRADICTS)
            {
                Contradictions.Add(Edge->SourceNodeID);
            }
        }
    }

    return Contradictions;
}

// ========================================
// SPREADING ACTIVATION
// ========================================

void UHypergraphMemorySystem::SpreadActivation(int64 SourceNodeID, float InitialActivation)
{
    TSet<int64> Visited;
    PropagateActivationInternal(SourceNodeID, InitialActivation, 0, Visited);
}

void UHypergraphMemorySystem::SpreadActivationMultiple(const TArray<int64>& SourceNodes,
                                                        const TArray<float>& InitialActivations)
{
    TSet<int64> Visited;

    int32 Count = FMath::Min(SourceNodes.Num(), InitialActivations.Num());
    for (int32 i = 0; i < Count; ++i)
    {
        PropagateActivationInternal(SourceNodes[i], InitialActivations[i], 0, Visited);
    }
}

void UHypergraphMemorySystem::PropagateActivationInternal(int64 NodeID, float Activation,
                                                           int32 Depth, TSet<int64>& Visited)
{
    if (Depth >= SpreadingConfig.MaxDepth || Activation < SpreadingConfig.MinActivation)
    {
        return;
    }

    if (Visited.Contains(NodeID))
    {
        return;
    }
    Visited.Add(NodeID);

    FScopeLock Lock(&GraphLock);

    FMemoryNode* Node = Nodes.Find(NodeID);
    if (!Node)
    {
        return;
    }

    // Add activation to node
    float OldActivation = Node->Activation;
    Node->Activation = FMath::Min(Node->Activation + Activation, 1.0f);

    if (Node->Activation > 0.1f && OldActivation <= 0.1f)
    {
        OnNodeActivated.Broadcast(NodeID, Node->Activation);
    }

    // Propagate to neighbors
    float DecayedActivation = Activation * SpreadingConfig.DecayFactor;

    for (int64 EdgeID : Node->OutgoingEdges)
    {
        const FMemoryEdge* Edge = Edges.Find(EdgeID);
        if (!Edge)
        {
            continue;
        }

        // Check if relation is allowed
        if (SpreadingConfig.AllowedRelations.Num() > 0 &&
            !SpreadingConfig.AllowedRelations.Contains(Edge->RelationType))
        {
            continue;
        }

        float PropagatedActivation = DecayedActivation;
        if (SpreadingConfig.bUseEdgeWeights)
        {
            PropagatedActivation *= Edge->Weight;
        }

        PropagateActivationInternal(Edge->TargetNodeID, PropagatedActivation, Depth + 1, Visited);
    }

    // Also spread through bidirectional edges
    for (int64 EdgeID : Node->IncomingEdges)
    {
        const FMemoryEdge* Edge = Edges.Find(EdgeID);
        if (!Edge || !Edge->bBidirectional)
        {
            continue;
        }

        if (SpreadingConfig.AllowedRelations.Num() > 0 &&
            !SpreadingConfig.AllowedRelations.Contains(Edge->RelationType))
        {
            continue;
        }

        float PropagatedActivation = DecayedActivation;
        if (SpreadingConfig.bUseEdgeWeights)
        {
            PropagatedActivation *= Edge->Weight;
        }

        PropagateActivationInternal(Edge->SourceNodeID, PropagatedActivation, Depth + 1, Visited);
    }
}

void UHypergraphMemorySystem::DecayActivations(float DeltaTime)
{
    FScopeLock Lock(&GraphLock);

    float DecayAmount = ConsolidationConfig.ActivationDecayRate * DeltaTime;

    for (auto& Pair : Nodes)
    {
        FMemoryNode& Node = Pair.Value;
        Node.Activation = FMath::Max(0.0f, Node.Activation - DecayAmount);
    }
}

TArray<int64> UHypergraphMemorySystem::GetMostActiveNodes(int32 Count) const
{
    FScopeLock Lock(&GraphLock);

    TArray<TPair<int64, float>> NodeActivations;
    for (const auto& Pair : Nodes)
    {
        if (Pair.Value.Activation > 0.0f)
        {
            NodeActivations.Add(TPair<int64, float>(Pair.Key, Pair.Value.Activation));
        }
    }

    NodeActivations.Sort([](const TPair<int64, float>& A, const TPair<int64, float>& B) {
        return A.Value > B.Value;
    });

    TArray<int64> Result;
    for (int32 i = 0; i < FMath::Min(Count, NodeActivations.Num()); ++i)
    {
        Result.Add(NodeActivations[i].Key);
    }

    return Result;
}

void UHypergraphMemorySystem::ResetActivations()
{
    FScopeLock Lock(&GraphLock);

    for (auto& Pair : Nodes)
    {
        Pair.Value.Activation = 0.0f;
    }
}

// ========================================
// GRAPH QUERIES
// ========================================

FMemoryQueryResult UHypergraphMemorySystem::ExecuteQuery(const FMemoryQuery& Query)
{
    double StartTime = FPlatformTime::Seconds();
    FMemoryQueryResult Result;

    FScopeLock Lock(&GraphLock);

    // Collect candidate nodes based on type filter
    TArray<int64> Candidates;

    if (Query.NodeTypes.Num() > 0)
    {
        for (EMemoryNodeType Type : Query.NodeTypes)
        {
            if (const TArray<int64>* TypeNodes = NodeTypeIndex.Find(Type))
            {
                Candidates.Append(*TypeNodes);
            }
        }
    }
    else
    {
        // All nodes
        Nodes.GetKeys(Candidates);
    }

    // Apply filters
    for (int64 NodeID : Candidates)
    {
        const FMemoryNode* Node = Nodes.Find(NodeID);
        if (!Node)
        {
            continue;
        }

        // Activation threshold
        if (Node->Activation < Query.ActivationThreshold)
        {
            continue;
        }

        // Similarity filter
        float Similarity = 1.0f;
        if (Query.ContentCue.Num() > 0 && Node->Embedding.Num() > 0)
        {
            Similarity = ComputeSimilarity(Query.ContentCue, Node->Embedding);
            if (Similarity < Query.SimilarityThreshold)
            {
                continue;
            }
        }

        Result.MatchingNodes.Add(NodeID);
        Result.RelevanceScores.Add(Similarity * (0.5f + 0.5f * Node->Activation));

        if (Result.MatchingNodes.Num() >= Query.MaxResults)
        {
            break;
        }
    }

    Result.TotalMatches = Result.MatchingNodes.Num();
    Result.ExecutionTimeMs = static_cast<float>((FPlatformTime::Seconds() - StartTime) * 1000.0);

    return Result;
}

TArray<int64> UHypergraphMemorySystem::FindNodesByType(EMemoryNodeType NodeType, int32 MaxResults) const
{
    FScopeLock Lock(&GraphLock);

    TArray<int64> Result;

    if (const TArray<int64>* TypeNodes = NodeTypeIndex.Find(NodeType))
    {
        int32 Count = FMath::Min(MaxResults, TypeNodes->Num());
        for (int32 i = 0; i < Count; ++i)
        {
            Result.Add((*TypeNodes)[i]);
        }
    }

    return Result;
}

TArray<int64> UHypergraphMemorySystem::FindSimilarNodes(const TArray<float>& Embedding,
                                                         float Threshold, int32 MaxResults)
{
    FScopeLock Lock(&GraphLock);

    TArray<TPair<int64, float>> Scored;

    for (const auto& Pair : Nodes)
    {
        if (Pair.Value.Embedding.Num() > 0)
        {
            float Similarity = ComputeSimilarity(Embedding, Pair.Value.Embedding);
            if (Similarity >= Threshold)
            {
                Scored.Add(TPair<int64, float>(Pair.Key, Similarity));
            }
        }
    }

    Scored.Sort([](const TPair<int64, float>& A, const TPair<int64, float>& B) {
        return A.Value > B.Value;
    });

    TArray<int64> Result;
    for (int32 i = 0; i < FMath::Min(MaxResults, Scored.Num()); ++i)
    {
        Result.Add(Scored[i].Key);
        RecordAccess(Scored[i].Key);
    }

    return Result;
}

TArray<int64> UHypergraphMemorySystem::FindPath(int64 StartNodeID, int64 EndNodeID,
                                                 int32 MaxLength,
                                                 const TArray<ESemanticRelation>& AllowedRelations)
{
    TSet<ESemanticRelation> RelationSet;
    for (ESemanticRelation Rel : AllowedRelations)
    {
        RelationSet.Add(Rel);
    }

    return BreadthFirstSearch(StartNodeID, EndNodeID, MaxLength, RelationSet);
}

TArray<int64> UHypergraphMemorySystem::GetNeighbors(int64 NodeID, bool bIncludeIncoming,
                                                     bool bIncludeOutgoing) const
{
    TArray<int64> Result;
    FScopeLock Lock(&GraphLock);

    const FMemoryNode* Node = Nodes.Find(NodeID);
    if (!Node)
    {
        return Result;
    }

    TSet<int64> NeighborSet;

    if (bIncludeOutgoing)
    {
        for (int64 EdgeID : Node->OutgoingEdges)
        {
            if (const FMemoryEdge* Edge = Edges.Find(EdgeID))
            {
                NeighborSet.Add(Edge->TargetNodeID);
            }
        }
    }

    if (bIncludeIncoming)
    {
        for (int64 EdgeID : Node->IncomingEdges)
        {
            if (const FMemoryEdge* Edge = Edges.Find(EdgeID))
            {
                NeighborSet.Add(Edge->SourceNodeID);
            }
        }
    }

    Result = NeighborSet.Array();
    return Result;
}

TArray<int64> UHypergraphMemorySystem::GetNeighborsByRelation(int64 NodeID,
                                                               ESemanticRelation Relation,
                                                               bool bOutgoing) const
{
    TArray<int64> Result;
    FScopeLock Lock(&GraphLock);

    const FMemoryNode* Node = Nodes.Find(NodeID);
    if (!Node)
    {
        return Result;
    }

    const TArray<int64>& EdgeIDs = bOutgoing ? Node->OutgoingEdges : Node->IncomingEdges;

    for (int64 EdgeID : EdgeIDs)
    {
        if (const FMemoryEdge* Edge = Edges.Find(EdgeID))
        {
            if (Edge->RelationType == Relation)
            {
                Result.Add(bOutgoing ? Edge->TargetNodeID : Edge->SourceNodeID);
            }
        }
    }

    return Result;
}

TArray<int64> UHypergraphMemorySystem::BreadthFirstSearch(int64 StartNodeID, int64 EndNodeID,
                                                           int32 MaxDepth,
                                                           const TSet<ESemanticRelation>& AllowedRelations) const
{
    FScopeLock Lock(&GraphLock);

    if (!Nodes.Contains(StartNodeID) || !Nodes.Contains(EndNodeID))
    {
        return {};
    }

    if (StartNodeID == EndNodeID)
    {
        return { StartNodeID };
    }

    TMap<int64, int64> Parent;
    TQueue<TPair<int64, int32>> Queue;

    Queue.Enqueue(TPair<int64, int32>(StartNodeID, 0));
    Parent.Add(StartNodeID, -1);

    while (!Queue.IsEmpty())
    {
        TPair<int64, int32> Current;
        Queue.Dequeue(Current);

        int64 CurrentNodeID = Current.Key;
        int32 CurrentDepth = Current.Value;

        if (CurrentDepth >= MaxDepth)
        {
            continue;
        }

        const FMemoryNode* CurrentNode = Nodes.Find(CurrentNodeID);
        if (!CurrentNode)
        {
            continue;
        }

        for (int64 EdgeID : CurrentNode->OutgoingEdges)
        {
            const FMemoryEdge* Edge = Edges.Find(EdgeID);
            if (!Edge)
            {
                continue;
            }

            if (AllowedRelations.Num() > 0 && !AllowedRelations.Contains(Edge->RelationType))
            {
                continue;
            }

            int64 NeighborID = Edge->TargetNodeID;

            if (Parent.Contains(NeighborID))
            {
                continue;
            }

            Parent.Add(NeighborID, CurrentNodeID);

            if (NeighborID == EndNodeID)
            {
                // Reconstruct path
                TArray<int64> Path;
                int64 Node = EndNodeID;
                while (Node != -1)
                {
                    Path.Insert(Node, 0);
                    if (const int64* ParentNode = Parent.Find(Node))
                    {
                        Node = *ParentNode;
                    }
                    else
                    {
                        break;
                    }
                }
                return Path;
            }

            Queue.Enqueue(TPair<int64, int32>(NeighborID, CurrentDepth + 1));
        }
    }

    return {}; // No path found
}

// ========================================
// CONSOLIDATION & MAINTENANCE
// ========================================

void UHypergraphMemorySystem::RunConsolidation()
{
    int32 NodesProcessed = 0;

    // Decay strengths
    {
        FScopeLock Lock(&GraphLock);

        for (auto& Pair : Nodes)
        {
            FMemoryNode& Node = Pair.Value;

            // Apply strength decay
            float TimeSinceAccess = static_cast<float>(AccumulatedTime - Node.LastAccessTime);
            float Decay = ConsolidationConfig.StrengthDecayRate * TimeSinceAccess;
            Node.Strength = FMath::Max(0.0f, Node.Strength - Decay);

            // Boost strength for frequently accessed nodes
            if (Node.AccessCount > 10)
            {
                Node.Strength = FMath::Min(1.0f, Node.Strength + 0.01f);
            }

            NodesProcessed++;
        }
    }

    // Prune weak memories
    PruneWeakMemories();

    // Strengthen frequent memories
    StrengthenFrequentMemories();

    // Detect abstractions if reservoir is available
    if (ConsolidationConfig.bEnableReservoirIntegration && ReservoirComponent)
    {
        DetectAbstractions(ConsolidationConfig.AbstractionThreshold);
    }

    OnConsolidationCycle.Broadcast(NodesProcessed);
}

int32 UHypergraphMemorySystem::PruneWeakMemories(float StrengthThreshold)
{
    float Threshold = StrengthThreshold >= 0.0f ? StrengthThreshold : ConsolidationConfig.PruningThreshold;

    TArray<int64> NodesToDelete;

    {
        FScopeLock Lock(&GraphLock);

        for (const auto& Pair : Nodes)
        {
            const FMemoryNode& Node = Pair.Value;

            // Don't prune intentional states or very recent memories
            if (IntentionalStates.Contains(Pair.Key))
            {
                continue;
            }

            float Age = static_cast<float>(AccumulatedTime - Node.CreationTime);
            if (Age < 10.0f) // Keep memories younger than 10 seconds
            {
                continue;
            }

            if (Node.Strength < Threshold && Node.Activation < 0.01f)
            {
                NodesToDelete.Add(Pair.Key);
            }
        }
    }

    for (int64 NodeID : NodesToDelete)
    {
        DeleteNode(NodeID);
    }

    return NodesToDelete.Num();
}

void UHypergraphMemorySystem::StrengthenFrequentMemories()
{
    FScopeLock Lock(&GraphLock);

    for (auto& Pair : Nodes)
    {
        FMemoryNode& Node = Pair.Value;

        // Strengthen based on access frequency
        if (Node.AccessCount > 5)
        {
            float Boost = FMath::Min(0.1f, Node.AccessCount * 0.01f);
            Node.Strength = FMath::Min(1.0f, Node.Strength + Boost);
        }
    }
}

TArray<int64> UHypergraphMemorySystem::DetectAbstractions(float SimilarityThreshold)
{
    TArray<int64> NewConcepts;
    FScopeLock Lock(&GraphLock);

    // Find clusters of similar episodic memories
    TArray<int64> Episodes;
    if (const TArray<int64>* EpisodeNodes = NodeTypeIndex.Find(EMemoryNodeType::Episode))
    {
        Episodes = *EpisodeNodes;
    }

    if (Episodes.Num() < 2)
    {
        return NewConcepts;
    }

    // Simple clustering: find pairs above threshold
    TSet<int64> Clustered;

    for (int32 i = 0; i < Episodes.Num(); ++i)
    {
        if (Clustered.Contains(Episodes[i]))
        {
            continue;
        }

        const FMemoryNode* NodeA = Nodes.Find(Episodes[i]);
        if (!NodeA || NodeA->Embedding.Num() == 0)
        {
            continue;
        }

        TArray<int64> ClusterMembers;
        ClusterMembers.Add(Episodes[i]);

        for (int32 j = i + 1; j < Episodes.Num(); ++j)
        {
            if (Clustered.Contains(Episodes[j]))
            {
                continue;
            }

            const FMemoryNode* NodeB = Nodes.Find(Episodes[j]);
            if (!NodeB || NodeB->Embedding.Num() == 0)
            {
                continue;
            }

            float Similarity = ComputeSimilarity(NodeA->Embedding, NodeB->Embedding);
            if (Similarity >= SimilarityThreshold)
            {
                ClusterMembers.Add(Episodes[j]);
            }
        }

        // Create concept if cluster has multiple members
        if (ClusterMembers.Num() >= 2)
        {
            // Compute average embedding
            TArray<float> AvgEmbedding;
            AvgEmbedding.SetNumZeroed(EmbeddingDimension);

            for (int64 MemberID : ClusterMembers)
            {
                const FMemoryNode* Member = Nodes.Find(MemberID);
                if (Member && Member->Embedding.Num() == EmbeddingDimension)
                {
                    for (int32 d = 0; d < EmbeddingDimension; ++d)
                    {
                        AvgEmbedding[d] += Member->Embedding[d];
                    }
                }
            }

            for (int32 d = 0; d < EmbeddingDimension; ++d)
            {
                AvgEmbedding[d] /= ClusterMembers.Num();
            }

            // Create concept node
            FString ConceptLabel = FString::Printf(TEXT("AbstractConcept_%d"), NextNodeID);
            int64 ConceptID = CreateNode(EMemoryNodeType::Concept, ConceptLabel, AvgEmbedding, 0.7f);

            // Link to source episodes
            for (int64 MemberID : ClusterMembers)
            {
                CreateEdge(MemberID, ConceptID, ESemanticRelation::INSTANCE_OF, 0.8f);
                Clustered.Add(MemberID);
            }

            NewConcepts.Add(ConceptID);

            // Fire pattern detected event
            OnPatternDetected.Broadcast(TEXT("AbstractConcept"), ClusterMembers);
        }
    }

    return NewConcepts;
}

void UHypergraphMemorySystem::IntegrateReservoirPatterns(const TArray<TArray<float>>& DetectedPatterns)
{
    for (const TArray<float>& Pattern : DetectedPatterns)
    {
        // Create a pattern node for each detected temporal pattern
        FString PatternLabel = FString::Printf(TEXT("ReservoirPattern_%d"), NextNodeID);
        int64 PatternID = CreateNode(EMemoryNodeType::Schema, PatternLabel, Pattern, 0.6f);

        // Find similar existing patterns and link them
        TArray<int64> SimilarNodes = FindSimilarNodes(Pattern, 0.7f, 5);

        for (int64 SimilarID : SimilarNodes)
        {
            if (SimilarID != PatternID)
            {
                CreateEdge(PatternID, SimilarID, ESemanticRelation::SIMILAR_TO, 0.8f);
            }
        }
    }
}

// ========================================
// RESERVOIR INTEGRATION
// ========================================

int64 UHypergraphMemorySystem::EncodeTemporalPattern(const TArray<TArray<float>>& Sequence,
                                                      const FString& Label, int32 StreamID)
{
    if (!ReservoirComponent || Sequence.Num() == 0)
    {
        return 0;
    }

    // Process sequence through reservoir to get embedding
    TArray<float> FinalState;
    for (const TArray<float>& Step : Sequence)
    {
        FinalState = ReservoirComponent->ProcessInput(Step, StreamID);
    }

    // Create episodic memory node with reservoir-processed embedding
    int64 NodeID = CreateNode(EMemoryNodeType::Episode, Label, FinalState, 0.6f);

    // Store sequence length as property
    SetNodeProperty(NodeID, TEXT("SequenceLength"), FString::FromInt(Sequence.Num()));
    SetNodeProperty(NodeID, TEXT("StreamID"), FString::FromInt(StreamID));

    return NodeID;
}

TArray<float> UHypergraphMemorySystem::GetReservoirEmbedding(const TArray<float>& Input, int32 StreamID)
{
    if (!ReservoirComponent)
    {
        return Input;
    }

    return ReservoirComponent->ProcessInput(Input, StreamID);
}

// ========================================
// STATISTICS & DEBUGGING
// ========================================

FHypergraphStats UHypergraphMemorySystem::GetStatistics() const
{
    FHypergraphStats Stats;
    FScopeLock Lock(&GraphLock);

    Stats.TotalNodes = Nodes.Num();
    Stats.TotalEdges = Edges.Num();
    Stats.TotalHyperedges = Hyperedges.Num();
    Stats.IntentionalStates = IntentionalStates.Num();

    float TotalActivation = 0.0f;
    float TotalStrength = 0.0f;

    for (const auto& Pair : Nodes)
    {
        const FMemoryNode& Node = Pair.Value;
        TotalActivation += Node.Activation;
        TotalStrength += Node.Strength;

        int32& TypeCount = Stats.NodesByType.FindOrAdd(Node.NodeType);
        TypeCount++;
    }

    if (Stats.TotalNodes > 0)
    {
        Stats.AverageActivation = TotalActivation / Stats.TotalNodes;
        Stats.AverageStrength = TotalStrength / Stats.TotalNodes;
    }

    for (const auto& Pair : Edges)
    {
        int32& RelCount = Stats.EdgesByRelation.FindOrAdd(Pair.Value.RelationType);
        RelCount++;
    }

    // Estimate memory usage
    Stats.MemoryUsageBytes = static_cast<float>(
        sizeof(FMemoryNode) * Nodes.Num() +
        sizeof(FMemoryEdge) * Edges.Num() +
        sizeof(FMemoryHyperedge) * Hyperedges.Num() +
        sizeof(FIntentionalState) * IntentionalStates.Num()
    );

    return Stats;
}

FString UHypergraphMemorySystem::ExportToDOT() const
{
    FScopeLock Lock(&GraphLock);

    FString DOT = TEXT("digraph HypergraphMemory {\n");
    DOT += TEXT("  rankdir=LR;\n");
    DOT += TEXT("  node [shape=ellipse];\n\n");

    // Export nodes
    for (const auto& Pair : Nodes)
    {
        const FMemoryNode& Node = Pair.Value;
        FString NodeShape = TEXT("ellipse");

        switch (Node.NodeType)
        {
        case EMemoryNodeType::Concept: NodeShape = TEXT("box"); break;
        case EMemoryNodeType::Belief: NodeShape = TEXT("diamond"); break;
        case EMemoryNodeType::Intention: NodeShape = TEXT("hexagon"); break;
        default: break;
        }

        DOT += FString::Printf(TEXT("  n%lld [label=\"%s\" shape=%s];\n"),
                               Node.NodeID, *Node.Label, *NodeShape);
    }

    DOT += TEXT("\n");

    // Export edges
    for (const auto& Pair : Edges)
    {
        const FMemoryEdge& Edge = Pair.Value;
        FString RelLabel = UEnum::GetValueAsString(Edge.RelationType);
        RelLabel = RelLabel.Replace(TEXT("ESemanticRelation::"), TEXT(""));

        DOT += FString::Printf(TEXT("  n%lld -> n%lld [label=\"%s\" weight=%.2f];\n"),
                               Edge.SourceNodeID, Edge.TargetNodeID, *RelLabel, Edge.Weight);
    }

    DOT += TEXT("}\n");
    return DOT;
}

bool UHypergraphMemorySystem::ExportToJSON(const FString& FilePath) const
{
    TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);

    // Export nodes
    TArray<TSharedPtr<FJsonValue>> NodesArray;
    {
        FScopeLock Lock(&GraphLock);

        for (const auto& Pair : Nodes)
        {
            const FMemoryNode& Node = Pair.Value;
            TSharedPtr<FJsonObject> NodeObj = MakeShareable(new FJsonObject);

            NodeObj->SetNumberField(TEXT("id"), static_cast<double>(Node.NodeID));
            NodeObj->SetStringField(TEXT("type"), UEnum::GetValueAsString(Node.NodeType));
            NodeObj->SetStringField(TEXT("label"), Node.Label);
            NodeObj->SetNumberField(TEXT("activation"), Node.Activation);
            NodeObj->SetNumberField(TEXT("strength"), Node.Strength);
            NodeObj->SetNumberField(TEXT("confidence"), Node.Confidence);

            NodesArray.Add(MakeShareable(new FJsonValueObject(NodeObj)));
        }
    }
    RootObject->SetArrayField(TEXT("nodes"), NodesArray);

    // Export edges
    TArray<TSharedPtr<FJsonValue>> EdgesArray;
    {
        FScopeLock Lock(&GraphLock);

        for (const auto& Pair : Edges)
        {
            const FMemoryEdge& Edge = Pair.Value;
            TSharedPtr<FJsonObject> EdgeObj = MakeShareable(new FJsonObject);

            EdgeObj->SetNumberField(TEXT("id"), static_cast<double>(Edge.EdgeID));
            EdgeObj->SetNumberField(TEXT("source"), static_cast<double>(Edge.SourceNodeID));
            EdgeObj->SetNumberField(TEXT("target"), static_cast<double>(Edge.TargetNodeID));
            EdgeObj->SetStringField(TEXT("relation"), UEnum::GetValueAsString(Edge.RelationType));
            EdgeObj->SetNumberField(TEXT("weight"), Edge.Weight);

            EdgesArray.Add(MakeShareable(new FJsonValueObject(EdgeObj)));
        }
    }
    RootObject->SetArrayField(TEXT("edges"), EdgesArray);

    // Serialize
    FString OutputString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
    FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);

    return FFileHelper::SaveStringToFile(OutputString, *FilePath);
}

bool UHypergraphMemorySystem::ImportFromJSON(const FString& FilePath)
{
    FString JsonString;
    if (!FFileHelper::LoadFileToString(JsonString, *FilePath))
    {
        return false;
    }

    TSharedPtr<FJsonObject> RootObject;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, RootObject))
    {
        return false;
    }

    ClearAll();

    // Import nodes
    const TArray<TSharedPtr<FJsonValue>>* NodesArray;
    if (RootObject->TryGetArrayField(TEXT("nodes"), NodesArray))
    {
        for (const TSharedPtr<FJsonValue>& NodeValue : *NodesArray)
        {
            TSharedPtr<FJsonObject> NodeObj = NodeValue->AsObject();
            if (!NodeObj)
            {
                continue;
            }

            FString TypeStr = NodeObj->GetStringField(TEXT("type"));
            FString Label = NodeObj->GetStringField(TEXT("label"));
            float Strength = static_cast<float>(NodeObj->GetNumberField(TEXT("strength")));

            EMemoryNodeType NodeType = EMemoryNodeType::Percept;
            // Parse type from string (simplified)
            if (TypeStr.Contains(TEXT("Concept"))) NodeType = EMemoryNodeType::Concept;
            else if (TypeStr.Contains(TEXT("Episode"))) NodeType = EMemoryNodeType::Episode;
            else if (TypeStr.Contains(TEXT("Belief"))) NodeType = EMemoryNodeType::Belief;
            else if (TypeStr.Contains(TEXT("Desire"))) NodeType = EMemoryNodeType::Desire;
            else if (TypeStr.Contains(TEXT("Intention"))) NodeType = EMemoryNodeType::Intention;

            CreateNode(NodeType, Label, {}, Strength);
        }
    }

    return true;
}

void UHypergraphMemorySystem::ClearAll()
{
    FScopeLock Lock(&GraphLock);

    Nodes.Empty();
    Edges.Empty();
    Hyperedges.Empty();
    IntentionalStates.Empty();
    NodeTypeIndex.Empty();
    EdgeRelationIndex.Empty();

    NextNodeID = 1;
    NextEdgeID = 1;
    NextHyperedgeID = 1;
}

// ========================================
// INTERNAL HELPER METHODS
// ========================================

float UHypergraphMemorySystem::ComputeSimilarity(const TArray<float>& A, const TArray<float>& B) const
{
    if (A.Num() != B.Num() || A.Num() == 0)
    {
        return 0.0f;
    }

    // Cosine similarity
    float DotProduct = 0.0f;
    float NormA = 0.0f;
    float NormB = 0.0f;

    for (int32 i = 0; i < A.Num(); ++i)
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

void UHypergraphMemorySystem::UpdateNodeIndex(int64 NodeID, EMemoryNodeType OldType, EMemoryNodeType NewType)
{
    if (TArray<int64>* OldList = NodeTypeIndex.Find(OldType))
    {
        OldList->Remove(NodeID);
    }

    NodeTypeIndex.FindOrAdd(NewType).Add(NodeID);
}

void UHypergraphMemorySystem::UpdateEdgeIndex(int64 EdgeID, ESemanticRelation Relation, bool bAdd)
{
    TArray<int64>& RelationEdges = EdgeRelationIndex.FindOrAdd(Relation);

    if (bAdd)
    {
        RelationEdges.AddUnique(EdgeID);
    }
    else
    {
        RelationEdges.Remove(EdgeID);
    }
}

void UHypergraphMemorySystem::RecordAccess(int64 NodeID)
{
    if (FMemoryNode* Node = Nodes.Find(NodeID))
    {
        Node->LastAccessTime = AccumulatedTime;
        Node->AccessCount++;

        // Strengthen on access
        Node->Strength = FMath::Min(1.0f, Node->Strength + 0.01f);
    }
}

// TemporalEventGraph.cpp
// Feature F1.5.1: Temporal Event Graph with B-Series Rooted Tree Structure
// Copyright (c) 2025-2026 Deep Tree Echo Project

#include "TemporalEventGraph.h"
#include "EpisodicMemorySystem.h"

// ============================================================================
// Frookedtree IMPLEMENTATION
// ============================================================================

const FRootedTreeNode* FRootedTree::GetRootNode() const
{
    for (const FRootedTreeNode& Node : Nodes)
    {
        if (Node.NodeID == RootNodeID)
        {
            return &Node;
        }
    }
    return nullptr;
}

const FRootedTreeNode* FRootedTree::GetNode(int64 NodeID) const
{
    for (const FRootedTreeNode& Node : Nodes)
    {
        if (Node.NodeID == NodeID)
        {
            return &Node;
        }
    }
    return nullptr;
}

// ============================================================================
// CONSTRUCTOR
// ============================================================================

UTemporalEventGraph::UTemporalEventGraph()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f; // Tick every 100ms
}

// ============================================================================
// LIFECYCLE
// ============================================================================

void UTemporalEventGraph::BeginPlay()
{
    Super::BeginPlay();
    Initialize();
}

void UTemporalEventGraph::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
}

void UTemporalEventGraph::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // Periodic maintenance tasks can be added here
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void UTemporalEventGraph::Initialize()
{
    if (bIsInitialized)
    {
        return;
    }

    // Initialize A000081 cache
    InitializeA000081Cache(Config.MaxBSeriesOrder);

    // Pre-compute B-series trees if configured
    if (Config.bCacheBSeriesTrees)
    {
        PrecomputeBSeriesTrees();
    }

    bIsInitialized = true;

    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("TemporalEventGraph: Initialized with max order %d"), Config.MaxBSeriesOrder);
    }
}

void UTemporalEventGraph::Reset()
{
    Events.Empty();
    Relations.Empty();
    Trees.Empty();
    TreesByOrder.Empty();
    
    NextEventID = 1;
    NextEdgeID = 1;
    NextTreeID = 1;
    NextNodeID = 1;

    Stats = FTemporalEventGraphStats();

    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("TemporalEventGraph: Reset complete"));
    }
}

void UTemporalEventGraph::PrecomputeBSeriesTrees()
{
    for (int32 Order = 1; Order <= Config.MaxBSeriesOrder; ++Order)
    {
        TArray<FRootedTree> OrderTrees = GenerateRootedTrees(Order);
        
        if (bEnableDebugLogging)
        {
            UE_LOG(LogTemp, Log, TEXT("TemporalEventGraph: Generated %d trees of order %d"), 
                   OrderTrees.Num(), Order);
        }
    }

    UpdateStats();
}

// ============================================================================
// EVENT MANAGEMENT
// ============================================================================

int64 UTemporalEventGraph::CreateEvent(const FString& Name, double StartTime, double EndTime, float Significance)
{
    if (Events.Num() >= Config.MaxEvents)
    {
        UE_LOG(LogTemp, Warning, TEXT("TemporalEventGraph: Maximum event limit reached (%d)"), Config.MaxEvents);
        return -1;
    }

    FTemporalEvent NewEvent;
    NewEvent.EventID = GenerateEventID();
    NewEvent.Name = Name;
    NewEvent.StartTime = StartTime;
    NewEvent.EndTime = (EndTime > 0.0) ? EndTime : StartTime;
    NewEvent.Duration = NewEvent.EndTime - NewEvent.StartTime;
    NewEvent.Significance = FMath::Clamp(Significance, 0.0f, 1.0f);

    Events.Add(NewEvent.EventID, NewEvent);
    
    // Auto-infer temporal relations if enabled
    if (Config.bEnableAutomaticInference)
    {
        for (const auto& Pair : Events)
        {
            if (Pair.Key != NewEvent.EventID)
            {
                const FTemporalEvent& OtherEvent = Pair.Value;
                
                // Infer before/after relations based on timestamps
                if (NewEvent.EndTime < OtherEvent.StartTime - Config.MinSequentialGap)
                {
                    AddBeforeRelation(NewEvent.EventID, Pair.Key, 1.0f);
                }
                else if (NewEvent.StartTime > OtherEvent.EndTime + Config.MinSequentialGap)
                {
                    AddAfterRelation(NewEvent.EventID, Pair.Key, 1.0f);
                }
                else if (FMath::Abs(NewEvent.StartTime - OtherEvent.StartTime) <= Config.MinSequentialGap &&
                         FMath::Abs(NewEvent.EndTime - OtherEvent.EndTime) <= Config.MinSequentialGap)
                {
                    AddSimultaneousRelation(NewEvent.EventID, Pair.Key, 1.0f);
                }
            }
        }
    }

    UpdateStats();
    OnEventCreated.Broadcast(NewEvent.EventID);

    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("TemporalEventGraph: Created event %lld '%s' at time %.3f"), 
               NewEvent.EventID, *Name, StartTime);
    }

    return NewEvent.EventID;
}

int64 UTemporalEventGraph::CreatePointEvent(const FString& Name, double Time)
{
    return CreateEvent(Name, Time, Time, 0.5f);
}

int64 UTemporalEventGraph::CreateIntervalEvent(const FString& Name, double StartTime, double Duration)
{
    return CreateEvent(Name, StartTime, StartTime + Duration, 0.5f);
}

FTemporalEvent UTemporalEventGraph::GetEvent(int64 EventID) const
{
    const FTemporalEvent* Event = Events.Find(EventID);
    return Event ? *Event : FTemporalEvent();
}

bool UTemporalEventGraph::EventExists(int64 EventID) const
{
    return Events.Contains(EventID);
}

bool UTemporalEventGraph::RemoveEvent(int64 EventID)
{
    if (!Events.Contains(EventID))
    {
        return false;
    }

    // Remove all relations involving this event
    TArray<int64> RelationsToRemove;
    for (const auto& Pair : Relations)
    {
        if (Pair.Value.SourceEventID == EventID || Pair.Value.TargetEventID == EventID)
        {
            RelationsToRemove.Add(Pair.Key);
        }
    }
    
    for (int64 EdgeID : RelationsToRemove)
    {
        Relations.Remove(EdgeID);
    }

    // Remove from other events' relation lists
    for (auto& Pair : Events)
    {
        Pair.Value.PredecessorIDs.Remove(EventID);
        Pair.Value.SuccessorIDs.Remove(EventID);
        Pair.Value.ConcurrentIDs.Remove(EventID);
    }

    Events.Remove(EventID);
    UpdateStats();

    return true;
}

TArray<FTemporalEvent> UTemporalEventGraph::GetAllEvents() const
{
    TArray<FTemporalEvent> Result;
    Events.GenerateValueArray(Result);
    return Result;
}

TArray<FTemporalEvent> UTemporalEventGraph::GetEventsInRange(double StartTime, double EndTime) const
{
    TArray<FTemporalEvent> Result;
    
    for (const auto& Pair : Events)
    {
        const FTemporalEvent& Event = Pair.Value;
        if (Event.EndTime >= StartTime && Event.StartTime <= EndTime)
        {
            Result.Add(Event);
        }
    }
    
    return Result;
}

// ============================================================================
// TEMPORAL RELATIONS
// ============================================================================

int64 UTemporalEventGraph::AddRelation(int64 SourceEventID, int64 TargetEventID, ETemporalRelation Relation, float Strength)
{
    if (!EventExists(SourceEventID) || !EventExists(TargetEventID))
    {
        return -1;
    }

    // Check for existing relation
    for (const auto& Pair : Relations)
    {
        if (Pair.Value.SourceEventID == SourceEventID && 
            Pair.Value.TargetEventID == TargetEventID &&
            Pair.Value.RelationType == Relation)
        {
            // Update strength if relation exists
            Relations[Pair.Key].Strength = Strength;
            return Pair.Key;
        }
    }

    FTemporalRelationEdge Edge(GenerateEdgeID(), SourceEventID, TargetEventID, Relation);
    Edge.Strength = FMath::Clamp(Strength, 0.0f, 1.0f);
    
    Relations.Add(Edge.EdgeID, Edge);
    UpdateEventRelationLists(SourceEventID, TargetEventID, Relation);
    UpdateStats();

    OnRelationAdded.Broadcast(SourceEventID, TargetEventID, Relation);

    return Edge.EdgeID;
}

int64 UTemporalEventGraph::AddBeforeRelation(int64 EventA, int64 EventB, float Strength)
{
    return AddRelation(EventA, EventB, ETemporalRelation::Before, Strength);
}

int64 UTemporalEventGraph::AddAfterRelation(int64 EventA, int64 EventB, float Strength)
{
    return AddRelation(EventA, EventB, ETemporalRelation::After, Strength);
}

int64 UTemporalEventGraph::AddSimultaneousRelation(int64 EventA, int64 EventB, float Strength)
{
    return AddRelation(EventA, EventB, ETemporalRelation::Simultaneous, Strength);
}

ETemporalRelation UTemporalEventGraph::GetRelation(int64 EventA, int64 EventB) const
{
    for (const auto& Pair : Relations)
    {
        if (Pair.Value.SourceEventID == EventA && Pair.Value.TargetEventID == EventB)
        {
            return Pair.Value.RelationType;
        }
    }
    
    // Check inverse
    for (const auto& Pair : Relations)
    {
        if (Pair.Value.SourceEventID == EventB && Pair.Value.TargetEventID == EventA)
        {
            return GetInverseRelation(Pair.Value.RelationType);
        }
    }

    return ETemporalRelation::Before; // Default
}

TArray<int64> UTemporalEventGraph::GetPredecessors(int64 EventID) const
{
    const FTemporalEvent* Event = Events.Find(EventID);
    return Event ? Event->PredecessorIDs : TArray<int64>();
}

TArray<int64> UTemporalEventGraph::GetSuccessors(int64 EventID) const
{
    const FTemporalEvent* Event = Events.Find(EventID);
    return Event ? Event->SuccessorIDs : TArray<int64>();
}

TArray<int64> UTemporalEventGraph::GetConcurrentEvents(int64 EventID) const
{
    const FTemporalEvent* Event = Events.Find(EventID);
    return Event ? Event->ConcurrentIDs : TArray<int64>();
}

void UTemporalEventGraph::ComputeTransitiveClosure()
{
    // Floyd-Warshall style transitive closure for before relations
    TArray<int64> EventIDs;
    Events.GenerateKeyArray(EventIDs);
    
    // Build reachability matrix
    TMap<TPair<int64, int64>, bool> Reachable;
    
    // Initialize with direct relations
    for (const auto& Pair : Relations)
    {
        if (Pair.Value.RelationType == ETemporalRelation::Before)
        {
            Reachable.Add(TPair<int64, int64>(Pair.Value.SourceEventID, Pair.Value.TargetEventID), true);
        }
    }
    
    // Compute transitive closure
    bool bChanged = true;
    while (bChanged)
    {
        bChanged = false;
        for (int64 K : EventIDs)
        {
            for (int64 I : EventIDs)
            {
                for (int64 J : EventIDs)
                {
                    if (I != J && I != K && J != K)
                    {
                        TPair<int64, int64> IK(I, K);
                        TPair<int64, int64> KJ(K, J);
                        TPair<int64, int64> IJ(I, J);
                        
                        if (Reachable.Contains(IK) && Reachable.Contains(KJ) && !Reachable.Contains(IJ))
                        {
                            Reachable.Add(IJ, true);
                            
                            // Add inferred relation
                            FTemporalRelationEdge Edge(GenerateEdgeID(), I, J, ETemporalRelation::Before);
                            Edge.bIsInferred = true;
                            Edge.Strength = 0.8f; // Lower strength for inferred relations
                            Relations.Add(Edge.EdgeID, Edge);
                            UpdateEventRelationLists(I, J, ETemporalRelation::Before);
                            
                            bChanged = true;
                        }
                    }
                }
            }
        }
    }

    UpdateStats();
}

int32 UTemporalEventGraph::InferTemporalRelations()
{
    int32 InferredCount = 0;
    TArray<int64> EventIDs;
    Events.GenerateKeyArray(EventIDs);

    for (int32 I = 0; I < EventIDs.Num(); ++I)
    {
        for (int32 J = I + 1; J < EventIDs.Num(); ++J)
        {
            const FTemporalEvent& EventA = Events[EventIDs[I]];
            const FTemporalEvent& EventB = Events[EventIDs[J]];

            // Check if relation already exists
            bool bRelationExists = false;
            for (const auto& Pair : Relations)
            {
                if ((Pair.Value.SourceEventID == EventIDs[I] && Pair.Value.TargetEventID == EventIDs[J]) ||
                    (Pair.Value.SourceEventID == EventIDs[J] && Pair.Value.TargetEventID == EventIDs[I]))
                {
                    bRelationExists = true;
                    break;
                }
            }

            if (!bRelationExists)
            {
                // Infer from timestamps
                if (EventA.EndTime < EventB.StartTime - Config.MinSequentialGap)
                {
                    AddBeforeRelation(EventIDs[I], EventIDs[J], 0.9f);
                    ++InferredCount;
                }
                else if (EventB.EndTime < EventA.StartTime - Config.MinSequentialGap)
                {
                    AddBeforeRelation(EventIDs[J], EventIDs[I], 0.9f);
                    ++InferredCount;
                }
            }
        }
    }

    if (Config.bEnableTransitiveClosure)
    {
        ComputeTransitiveClosure();
    }

    return InferredCount;
}

// ============================================================================
// B-SERIES ROOTED TREES
// ============================================================================

TArray<FRootedTree> UTemporalEventGraph::GenerateRootedTrees(int32 Order)
{
    if (Order < 1)
    {
        return TArray<FRootedTree>();
    }

    // Check cache first
    if (TreesByOrder.Contains(Order))
    {
        TArray<FRootedTree> Result;
        for (int64 TreeID : TreesByOrder[Order])
        {
            if (Trees.Contains(TreeID))
            {
                Result.Add(Trees[TreeID]);
            }
        }
        return Result;
    }

    TArray<FRootedTree> Result;

    if (Order == 1)
    {
        // Base case: single node tree "()"
        FRootedTree Tree;
        Tree.TreeID = GenerateTreeID();
        Tree.Order = 1;
        Tree.NodeCount = 1;
        Tree.ParenthesesNotation = TEXT("()");
        Tree.LevelSequence.Add(0);
        Tree.Coefficient = 1.0f;
        Tree.SymmetryFactor = 1;
        Tree.Density = 1;

        FRootedTreeNode RootNode = CreateNode(ERootedTreeNodeType::Root);
        RootNode.Depth = 0;
        RootNode.SubtreeSize = 1;
        RootNode.ParenthesesNotation = TEXT("()");
        Tree.RootNodeID = RootNode.NodeID;
        Tree.Nodes.Add(RootNode);

        Result.Add(Tree);
    }
    else
    {
        // Generate recursively using partitions
        // For order N, we need to partition (N-1) among child subtrees
        GenerateTreesRecursive(Order, Result);
    }

    // Cache results
    TArray<int64> TreeIDs;
    for (FRootedTree& Tree : Result)
    {
        Trees.Add(Tree.TreeID, Tree);
        TreeIDs.Add(Tree.TreeID);
        
        OnTreeGenerated.Broadcast(Tree.TreeID, Order);
    }
    TreesByOrder.Add(Order, TreeIDs);

    return Result;
}

int32 UTemporalEventGraph::GetRootedTreeCount(int32 Order) const
{
    if (Order < 0 || Order > 20)
    {
        return 0;
    }

    // A000081 sequence values
    static const int32 A000081[] = {
        0, 1, 1, 2, 4, 9, 20, 48, 115, 286,
        719, 1842, 4766, 12486, 32973, 87811, 235381, 634847, 1721159, 4688676,
        12826228
    };

    return A000081[Order];
}

FRootedTree UTemporalEventGraph::CreateTreeFromNotation(const FString& Notation)
{
    FRootedTree Tree;
    Tree.TreeID = GenerateTreeID();
    Tree.ParenthesesNotation = Notation;

    if (Notation.IsEmpty())
    {
        return Tree;
    }

    // Parse parentheses notation
    TArray<int64> NodeStack;
    int32 CurrentDepth = 0;
    int32 MaxDepth = 0;
    int32 NodeCount = 0;

    FRootedTreeNode* CurrentParent = nullptr;
    int64 RootID = -1;

    for (int32 I = 0; I < Notation.Len(); ++I)
    {
        TCHAR C = Notation[I];
        
        if (C == TEXT('('))
        {
            // Create new node
            FRootedTreeNode NewNode = CreateNode(CurrentDepth == 0 ? ERootedTreeNodeType::Root : ERootedTreeNodeType::Interior);
            NewNode.Depth = CurrentDepth;
            
            if (NodeStack.Num() > 0)
            {
                NewNode.ParentID = NodeStack.Last();
            }

            Tree.Nodes.Add(NewNode);
            Tree.LevelSequence.Add(CurrentDepth);
            
            if (CurrentDepth == 0)
            {
                RootID = NewNode.NodeID;
            }
            else
            {
                // Add as child to parent
                for (FRootedTreeNode& Node : Tree.Nodes)
                {
                    if (Node.NodeID == NewNode.ParentID)
                    {
                        Node.ChildIDs.Add(NewNode.NodeID);
                        break;
                    }
                }
            }

            NodeStack.Push(NewNode.NodeID);
            ++CurrentDepth;
            MaxDepth = FMath::Max(MaxDepth, CurrentDepth);
            ++NodeCount;
        }
        else if (C == TEXT(')'))
        {
            --CurrentDepth;
            if (NodeStack.Num() > 0)
            {
                NodeStack.Pop();
            }
        }
    }

    Tree.RootNodeID = RootID;
    Tree.NodeCount = NodeCount;
    Tree.Order = NodeCount;

    // Update node types for leaves
    for (FRootedTreeNode& Node : Tree.Nodes)
    {
        if (Node.ChildIDs.Num() == 0 && Node.NodeType != ERootedTreeNodeType::Root)
        {
            Node.NodeType = ERootedTreeNodeType::Leaf;
        }
    }

    // Compute subtree sizes
    ComputeSubtreeSizes(Tree, RootID);

    // Compute B-series properties
    Tree.Density = ComputeDensity(Tree.TreeID);
    Tree.SymmetryFactor = ComputeSymmetryFactor(Tree.TreeID);
    Tree.Coefficient = 1.0f / (float)(Tree.Density * Tree.SymmetryFactor);

    return Tree;
}

FString UTemporalEventGraph::GetTreeNotation(int64 TreeID) const
{
    const FRootedTree* Tree = Trees.Find(TreeID);
    return Tree ? Tree->ParenthesesNotation : TEXT("");
}

float UTemporalEventGraph::ComputeBSeriesCoefficient(int64 TreeID) const
{
    const FRootedTree* Tree = Trees.Find(TreeID);
    if (!Tree)
    {
        return 0.0f;
    }

    // B-series coefficient = 1 / (gamma * sigma)
    // where gamma = density (product of subtree sizes)
    // and sigma = symmetry factor
    return 1.0f / (float)(Tree->Density * Tree->SymmetryFactor);
}

int32 UTemporalEventGraph::ComputeSymmetryFactor(int64 TreeID) const
{
    const FRootedTree* Tree = Trees.Find(TreeID);
    if (!Tree || Tree->Nodes.Num() == 0)
    {
        return 1;
    }

    // Symmetry factor is the product of factorials of counts of identical subtrees
    // For simplicity, we compute based on child multiplicity at each node
    int32 Sigma = 1;
    
    for (const FRootedTreeNode& Node : Tree->Nodes)
    {
        if (Node.ChildIDs.Num() > 1)
        {
            // Count identical children (simplified: count by subtree size)
            TMap<int32, int32> SubtreeSizeCounts;
            for (int64 ChildID : Node.ChildIDs)
            {
                const FRootedTreeNode* ChildNode = Tree->GetNode(ChildID);
                if (ChildNode)
                {
                    SubtreeSizeCounts.FindOrAdd(ChildNode->SubtreeSize)++;
                }
            }

            // Multiply by factorial of each count
            for (const auto& Pair : SubtreeSizeCounts)
            {
                int32 Factorial = 1;
                for (int32 I = 2; I <= Pair.Value; ++I)
                {
                    Factorial *= I;
                }
                Sigma *= Factorial;
            }
        }
    }

    return Sigma;
}

int32 UTemporalEventGraph::ComputeDensity(int64 TreeID) const
{
    const FRootedTree* Tree = Trees.Find(TreeID);
    if (!Tree || Tree->Nodes.Num() == 0)
    {
        return 1;
    }

    // Density (gamma) = product of all subtree sizes
    int32 Gamma = 1;
    for (const FRootedTreeNode& Node : Tree->Nodes)
    {
        Gamma *= Node.SubtreeSize;
    }

    return Gamma;
}

TArray<FRootedTree> UTemporalEventGraph::GetAllTrees() const
{
    TArray<FRootedTree> Result;
    Trees.GenerateValueArray(Result);
    return Result;
}

TArray<FRootedTree> UTemporalEventGraph::GetTreesByOrder(int32 Order) const
{
    TArray<FRootedTree> Result;
    
    const TArray<int64>* TreeIDs = TreesByOrder.Find(Order);
    if (TreeIDs)
    {
        for (int64 TreeID : *TreeIDs)
        {
            const FRootedTree* Tree = Trees.Find(TreeID);
            if (Tree)
            {
                Result.Add(*Tree);
            }
        }
    }

    return Result;
}

// ============================================================================
// EVENT-TREE ASSOCIATION
// ============================================================================

bool UTemporalEventGraph::AssociateEventWithTree(int64 EventID, int64 TreeID, int64 NodeID)
{
    FTemporalEvent* Event = Events.Find(EventID);
    FRootedTree* Tree = Trees.Find(TreeID);
    
    if (!Event || !Tree)
    {
        return false;
    }

    // Find the node in the tree
    for (FRootedTreeNode& Node : Tree->Nodes)
    {
        if (Node.NodeID == NodeID)
        {
            Node.EventID = EventID;
            Event->TreeNodeIDs.AddUnique(NodeID);
            return true;
        }
    }

    return false;
}

FRootedTree UTemporalEventGraph::BuildTreeFromEventSequence(const TArray<int64>& EventIDs)
{
    FRootedTree Tree;
    Tree.TreeID = GenerateTreeID();
    Tree.Order = EventIDs.Num();
    Tree.NodeCount = EventIDs.Num();

    if (EventIDs.Num() == 0)
    {
        return Tree;
    }

    // Create a linear tree (chain) from the event sequence
    int64 PreviousNodeID = -1;
    
    for (int32 I = 0; I < EventIDs.Num(); ++I)
    {
        ERootedTreeNodeType NodeType = (I == 0) ? ERootedTreeNodeType::Root : ERootedTreeNodeType::Interior;
        if (I == EventIDs.Num() - 1 && I > 0)
        {
            NodeType = ERootedTreeNodeType::Leaf;
        }

        FRootedTreeNode Node = CreateNode(NodeType);
        Node.Depth = I;
        Node.EventID = EventIDs[I];
        Node.SubtreeSize = EventIDs.Num() - I;
        
        if (PreviousNodeID >= 0)
        {
            Node.ParentID = PreviousNodeID;
            
            // Update parent's child list
            for (FRootedTreeNode& ParentNode : Tree.Nodes)
            {
                if (ParentNode.NodeID == PreviousNodeID)
                {
                    ParentNode.ChildIDs.Add(Node.NodeID);
                    break;
                }
            }
        }
        else
        {
            Tree.RootNodeID = Node.NodeID;
        }

        Tree.Nodes.Add(Node);
        Tree.LevelSequence.Add(I);
        PreviousNodeID = Node.NodeID;
    }

    // Generate parentheses notation
    Tree.ParenthesesNotation = GenerateNotation(Tree, Tree.RootNodeID);

    // Store tree
    Trees.Add(Tree.TreeID, Tree);

    return Tree;
}

// ============================================================================
// MEMORY INTEGRATION
// ============================================================================

bool UTemporalEventGraph::LinkEventToMemory(int64 EventID, int64 MemoryTraceID)
{
    FTemporalEvent* Event = Events.Find(EventID);
    if (!Event)
    {
        return false;
    }

    Event->MemoryTraceID = MemoryTraceID;
    return true;
}

TArray<int64> UTemporalEventGraph::GetEventsForMemory(int64 MemoryTraceID) const
{
    TArray<int64> Result;
    
    for (const auto& Pair : Events)
    {
        if (Pair.Value.MemoryTraceID == MemoryTraceID)
        {
            Result.Add(Pair.Key);
        }
    }

    return Result;
}

// ============================================================================
// QUERY & ANALYSIS
// ============================================================================

TArray<int64> UTemporalEventGraph::GetTopologicalOrder() const
{
    TArray<int64> Result;
    TArray<int64> EventIDs;
    Events.GenerateKeyArray(EventIDs);

    // Count incoming edges (predecessors) for each event
    TMap<int64, int32> InDegree;
    for (int64 ID : EventIDs)
    {
        InDegree.Add(ID, 0);
    }

    for (const auto& Pair : Relations)
    {
        if (Pair.Value.RelationType == ETemporalRelation::Before)
        {
            InDegree.FindOrAdd(Pair.Value.TargetEventID)++;
        }
    }

    // Kahn's algorithm for topological sort
    TArray<int64> Queue;
    for (const auto& Pair : InDegree)
    {
        if (Pair.Value == 0)
        {
            Queue.Add(Pair.Key);
        }
    }

    while (Queue.Num() > 0)
    {
        int64 Current = Queue[0];
        Queue.RemoveAt(0);
        Result.Add(Current);

        // For each successor
        for (const auto& Pair : Relations)
        {
            if (Pair.Value.RelationType == ETemporalRelation::Before && 
                Pair.Value.SourceEventID == Current)
            {
                int64 Target = Pair.Value.TargetEventID;
                InDegree[Target]--;
                
                if (InDegree[Target] == 0)
                {
                    Queue.Add(Target);
                }
            }
        }
    }

    return Result;
}

TArray<TArray<int64>> UTemporalEventGraph::FindCausalChains(int32 MinLength) const
{
    TArray<TArray<int64>> Chains;
    
    // Find all events with no predecessors (chain starts)
    TArray<int64> StartEvents;
    for (const auto& Pair : Events)
    {
        if (Pair.Value.PredecessorIDs.Num() == 0)
        {
            StartEvents.Add(Pair.Key);
        }
    }

    // DFS from each start event
    for (int64 StartID : StartEvents)
    {
        TArray<int64> CurrentChain;
        CurrentChain.Add(StartID);
        
        TFunction<void(int64)> DFS = [&](int64 EventID)
        {
            const FTemporalEvent* Event = Events.Find(EventID);
            if (!Event)
            {
                return;
            }

            if (Event->SuccessorIDs.Num() == 0)
            {
                // End of chain
                if (CurrentChain.Num() >= MinLength)
                {
                    Chains.Add(CurrentChain);
                }
            }
            else
            {
                for (int64 SuccessorID : Event->SuccessorIDs)
                {
                    CurrentChain.Add(SuccessorID);
                    DFS(SuccessorID);
                    CurrentChain.Pop();
                }
            }
        };

        DFS(StartID);
    }

    return Chains;
}

bool UTemporalEventGraph::CanReach(int64 EventA, int64 EventB) const
{
    if (!EventExists(EventA) || !EventExists(EventB))
    {
        return false;
    }

    // BFS to check reachability
    TSet<int64> Visited;
    TArray<int64> Queue;
    Queue.Add(EventA);

    while (Queue.Num() > 0)
    {
        int64 Current = Queue[0];
        Queue.RemoveAt(0);

        if (Current == EventB)
        {
            return true;
        }

        if (Visited.Contains(Current))
        {
            continue;
        }
        Visited.Add(Current);

        const FTemporalEvent* Event = Events.Find(Current);
        if (Event)
        {
            for (int64 SuccessorID : Event->SuccessorIDs)
            {
                if (!Visited.Contains(SuccessorID))
                {
                    Queue.Add(SuccessorID);
                }
            }
        }
    }

    return false;
}

// ============================================================================
// STATISTICS
// ============================================================================

FTemporalEventGraphStats UTemporalEventGraph::GetStats() const
{
    return Stats;
}

TArray<FString> UTemporalEventGraph::GenerateDiagnosticReport() const
{
    TArray<FString> Report;
    
    Report.Add(TEXT("=== Temporal Event Graph Diagnostic Report ==="));
    Report.Add(FString::Printf(TEXT("Total Events: %d"), Stats.TotalEvents));
    Report.Add(FString::Printf(TEXT("Total Relations: %d"), Stats.TotalRelations));
    Report.Add(FString::Printf(TEXT("Total Trees: %d"), Stats.TotalTrees));
    Report.Add(FString::Printf(TEXT("Max Tree Depth: %d"), Stats.MaxTreeDepth));
    Report.Add(FString::Printf(TEXT("Max B-Series Order: %d"), Stats.MaxBSeriesOrder));
    Report.Add(TEXT(""));
    Report.Add(TEXT("Relation Counts:"));
    Report.Add(FString::Printf(TEXT("  Before: %d"), Stats.BeforeRelationCount));
    Report.Add(FString::Printf(TEXT("  After: %d"), Stats.AfterRelationCount));
    Report.Add(FString::Printf(TEXT("  Simultaneous: %d"), Stats.SimultaneousCount));
    Report.Add(TEXT(""));
    Report.Add(TEXT("A000081 Sequence (Rooted Trees):"));
    for (int32 I = 1; I <= FMath::Min(Config.MaxBSeriesOrder, 10); ++I)
    {
        Report.Add(FString::Printf(TEXT("  Order %d: %d trees"), I, GetRootedTreeCount(I)));
    }

    return Report;
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void UTemporalEventGraph::InitializeA000081Cache(int32 MaxOrder)
{
    // Pre-compute A000081 values using recurrence
    A000081Cache.Add(0, 0);
    A000081Cache.Add(1, 1);

    for (int32 N = 2; N <= MaxOrder; ++N)
    {
        A000081Cache.Add(N, ComputeA000081(N));
    }
}

void UTemporalEventGraph::GenerateTreesRecursive(int32 Order, TArray<FRootedTree>& OutTrees)
{
    if (Order <= 0)
    {
        return;
    }

    if (Order == 1)
    {
        // Single node
        FRootedTree Tree;
        Tree.TreeID = GenerateTreeID();
        Tree.Order = 1;
        Tree.NodeCount = 1;
        Tree.ParenthesesNotation = TEXT("()");
        Tree.LevelSequence.Add(0);

        FRootedTreeNode RootNode = CreateNode(ERootedTreeNodeType::Root);
        RootNode.Depth = 0;
        RootNode.SubtreeSize = 1;
        Tree.RootNodeID = RootNode.NodeID;
        Tree.Nodes.Add(RootNode);
        Tree.Density = 1;
        Tree.SymmetryFactor = 1;
        Tree.Coefficient = 1.0f;

        OutTrees.Add(Tree);
        return;
    }

    // Generate trees by partitioning (Order-1) among subtrees
    // This is a simplified generation - for full A000081 enumeration,
    // more sophisticated partition-based generation is needed
    
    // Get all smaller trees
    TArray<TArray<FRootedTree>> SmallerTrees;
    for (int32 K = 1; K < Order; ++K)
    {
        SmallerTrees.Add(GenerateRootedTrees(K));
    }

    AssembleTrees(Order, OutTrees, SmallerTrees);
}

void UTemporalEventGraph::AssembleTrees(int32 N, TArray<FRootedTree>& OutTrees, const TArray<TArray<FRootedTree>>& SmallerTrees)
{
    // Simple partition-based assembly
    // For N nodes, we need to partition (N-1) among child subtrees
    
    // Generate partitions of (N-1)
    TFunction<void(int32, int32, TArray<int32>&)> GeneratePartitions;
    TArray<TArray<int32>> Partitions;
    
    GeneratePartitions = [&](int32 Remaining, int32 MaxPart, TArray<int32>& Current)
    {
        if (Remaining == 0)
        {
            Partitions.Add(Current);
            return;
        }
        
        for (int32 Part = FMath::Min(Remaining, MaxPart); Part >= 1; --Part)
        {
            Current.Add(Part);
            GeneratePartitions(Remaining - Part, Part, Current);
            Current.Pop();
        }
    };

    TArray<int32> CurrentPartition;
    GeneratePartitions(N - 1, N - 1, CurrentPartition);

    // For each partition, create corresponding trees
    for (const TArray<int32>& Partition : Partitions)
    {
        // Create a tree with root and children according to partition
        FRootedTree Tree;
        Tree.TreeID = GenerateTreeID();
        Tree.Order = N;
        Tree.NodeCount = N;

        FRootedTreeNode RootNode = CreateNode(ERootedTreeNodeType::Root);
        RootNode.Depth = 0;
        Tree.RootNodeID = RootNode.NodeID;
        Tree.Nodes.Add(RootNode);

        // Add child nodes based on partition
        // This is simplified - full implementation would recursively use subtree structures
        int32 Depth = 1;
        int64 PreviousNodeID = RootNode.NodeID;
        
        for (int32 PartSize : Partition)
        {
            // Add a chain of nodes for this partition part
            for (int32 I = 0; I < PartSize; ++I)
            {
                bool bIsLeaf = (I == PartSize - 1);
                FRootedTreeNode ChildNode = CreateNode(bIsLeaf ? ERootedTreeNodeType::Leaf : ERootedTreeNodeType::Interior);
                ChildNode.Depth = Depth + I;
                ChildNode.ParentID = (I == 0) ? RootNode.NodeID : PreviousNodeID;

                // Update parent's child list
                for (FRootedTreeNode& ParentNode : Tree.Nodes)
                {
                    if (ParentNode.NodeID == ChildNode.ParentID)
                    {
                        ParentNode.ChildIDs.Add(ChildNode.NodeID);
                        break;
                    }
                }

                Tree.Nodes.Add(ChildNode);
                Tree.LevelSequence.Add(ChildNode.Depth);
                PreviousNodeID = ChildNode.NodeID;
            }
        }

        // Compute subtree sizes
        ComputeSubtreeSizes(Tree, Tree.RootNodeID);

        // Generate notation
        Tree.ParenthesesNotation = GenerateNotation(Tree, Tree.RootNodeID);

        // Compute B-series properties
        Tree.Density = 1;
        Tree.SymmetryFactor = 1;
        for (const FRootedTreeNode& Node : Tree.Nodes)
        {
            Tree.Density *= Node.SubtreeSize;
        }
        Tree.Coefficient = 1.0f / (float)Tree.Density;

        OutTrees.Add(Tree);
    }
}

FRootedTreeNode UTemporalEventGraph::CreateNode(ERootedTreeNodeType NodeType)
{
    FRootedTreeNode Node;
    Node.NodeID = NextNodeID++;
    Node.NodeType = NodeType;
    return Node;
}

int32 UTemporalEventGraph::ComputeSubtreeSizes(FRootedTree& Tree, int64 NodeID)
{
    FRootedTreeNode* Node = nullptr;
    for (FRootedTreeNode& N : Tree.Nodes)
    {
        if (N.NodeID == NodeID)
        {
            Node = &N;
            break;
        }
    }

    if (!Node)
    {
        return 0;
    }

    int32 Size = 1;
    for (int64 ChildID : Node->ChildIDs)
    {
        Size += ComputeSubtreeSizes(Tree, ChildID);
    }

    Node->SubtreeSize = Size;
    return Size;
}

FString UTemporalEventGraph::GenerateNotation(const FRootedTree& Tree, int64 NodeID) const
{
    const FRootedTreeNode* Node = Tree.GetNode(NodeID);
    if (!Node)
    {
        return TEXT("");
    }

    FString Result = TEXT("(");
    
    for (int64 ChildID : Node->ChildIDs)
    {
        Result += GenerateNotation(Tree, ChildID);
    }
    
    Result += TEXT(")");
    return Result;
}

void UTemporalEventGraph::UpdateEventRelationLists(int64 SourceID, int64 TargetID, ETemporalRelation Relation)
{
    FTemporalEvent* SourceEvent = Events.Find(SourceID);
    FTemporalEvent* TargetEvent = Events.Find(TargetID);

    if (!SourceEvent || !TargetEvent)
    {
        return;
    }

    switch (Relation)
    {
    case ETemporalRelation::Before:
        SourceEvent->SuccessorIDs.AddUnique(TargetID);
        TargetEvent->PredecessorIDs.AddUnique(SourceID);
        break;

    case ETemporalRelation::After:
        SourceEvent->PredecessorIDs.AddUnique(TargetID);
        TargetEvent->SuccessorIDs.AddUnique(SourceID);
        break;

    case ETemporalRelation::Simultaneous:
    case ETemporalRelation::Overlaps:
    case ETemporalRelation::During:
        SourceEvent->ConcurrentIDs.AddUnique(TargetID);
        TargetEvent->ConcurrentIDs.AddUnique(SourceID);
        break;

    default:
        break;
    }
}

int32 UTemporalEventGraph::ComputeA000081(int32 N)
{
    if (N <= 1)
    {
        return N;
    }

    // Use Euler transform recurrence:
    // a(n+1) = (1/n) * Sum_{k=1..n} ( Sum_{d|k} d*a(d) ) * a(n-k+1)
    
    // Check cache
    const int32* Cached = A000081Cache.Find(N);
    if (Cached)
    {
        return *Cached;
    }

    // Compute divisor sums
    TArray<int32> DivisorSums;
    DivisorSums.SetNum(N);

    for (int32 K = 1; K <= N - 1; ++K)
    {
        int32 Sum = 0;
        for (int32 D = 1; D <= K; ++D)
        {
            if (K % D == 0)
            {
                Sum += D * ComputeA000081(D);
            }
        }
        DivisorSums[K - 1] = Sum;
    }

    int32 Result = 0;
    for (int32 K = 1; K <= N - 1; ++K)
    {
        Result += DivisorSums[K - 1] * ComputeA000081(N - K);
    }
    Result /= (N - 1);

    A000081Cache.Add(N, Result);
    return Result;
}

int64 UTemporalEventGraph::GenerateEventID()
{
    return NextEventID++;
}

int64 UTemporalEventGraph::GenerateEdgeID()
{
    return NextEdgeID++;
}

int64 UTemporalEventGraph::GenerateTreeID()
{
    return NextTreeID++;
}

void UTemporalEventGraph::UpdateStats()
{
    Stats.TotalEvents = Events.Num();
    Stats.TotalRelations = Relations.Num();
    Stats.TotalTrees = Trees.Num();

    Stats.BeforeRelationCount = 0;
    Stats.AfterRelationCount = 0;
    Stats.SimultaneousCount = 0;

    for (const auto& Pair : Relations)
    {
        switch (Pair.Value.RelationType)
        {
        case ETemporalRelation::Before:
            ++Stats.BeforeRelationCount;
            break;
        case ETemporalRelation::After:
            ++Stats.AfterRelationCount;
            break;
        case ETemporalRelation::Simultaneous:
            ++Stats.SimultaneousCount;
            break;
        default:
            break;
        }
    }

    Stats.MaxTreeDepth = 0;
    Stats.MaxBSeriesOrder = 0;
    for (const auto& Pair : Trees)
    {
        Stats.MaxBSeriesOrder = FMath::Max(Stats.MaxBSeriesOrder, Pair.Value.Order);
        for (const FRootedTreeNode& Node : Pair.Value.Nodes)
        {
            Stats.MaxTreeDepth = FMath::Max(Stats.MaxTreeDepth, Node.Depth);
        }
    }
}

ETemporalRelation UTemporalEventGraph::GetInverseRelation(ETemporalRelation Relation) const
{
    switch (Relation)
    {
    case ETemporalRelation::Before:
        return ETemporalRelation::After;
    case ETemporalRelation::After:
        return ETemporalRelation::Before;
    case ETemporalRelation::Starts:
        return ETemporalRelation::Finishes;
    case ETemporalRelation::Finishes:
        return ETemporalRelation::Starts;
    case ETemporalRelation::Meets:
        return ETemporalRelation::Meets;
    default:
        return Relation;
    }
}

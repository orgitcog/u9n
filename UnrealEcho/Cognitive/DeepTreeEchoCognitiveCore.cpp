//=============================================================================
// DeepTreeEchoCognitiveCore.cpp
//
// Implementation of the Deep Tree Echo cognitive architecture with:
// - Hypergraph memory space for multi-relational knowledge storage
// - Echo propagation engine for activation spreading and pattern recognition
// - P-System membrane manager for nested execution contexts (OEIS A000081)
//
// Copyright (c) 2025 Deep Tree Echo Project
//=============================================================================

#include "DeepTreeEchoCognitiveCore.h"
#include "Engine/World.h"

UDeepTreeEchoCognitiveCore::UDeepTreeEchoCognitiveCore()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.05f; // 20Hz update rate

    // Default configuration
    bEnableEchoPropagation = true;
    ActivationDecayRate = 0.1f;
    ActivationSpreadFactor = 0.8f;
    PatternRecognitionThreshold = 0.7f;

    // Initialize counters
    NodeIDCounter = 0;
    EdgeIDCounter = 0;
    PatternIDCounter = 0;
    MembraneIDCounter = 0;
}

void UDeepTreeEchoCognitiveCore::BeginPlay()
{
    Super::BeginPlay();

    UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoCognitiveCore: Initializing cognitive architecture"));

    // Initialize the root membrane (global execution context)
    InitializeRootMembrane();

    // Create initial memory structure with core concepts
    FString SelfNode = AddNode(TEXT("Self"), EMemoryType::Declarative);
    FString AwarenessNode = AddNode(TEXT("Awareness"), EMemoryType::Declarative);
    FString ExperienceNode = AddNode(TEXT("Experience"), EMemoryType::Episodic);
    FString GoalNode = AddNode(TEXT("Goal"), EMemoryType::Intentional);

    // Create foundational relationships
    AddEdge({SelfNode}, {AwarenessNode}, TEXT("has"), 1.0f);
    AddEdge({SelfNode}, {ExperienceNode}, TEXT("accumulates"), 1.0f);
    AddEdge({SelfNode}, {GoalNode}, TEXT("pursues"), 1.0f);
    AddEdge({AwarenessNode}, {ExperienceNode}, TEXT("processes"), 0.8f);

    UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoCognitiveCore: Initialized with %d nodes, %d edges, %d membranes"),
           HypergraphNodes.Num(), HypergraphEdges.Num(), Membranes.Num());
}

void UDeepTreeEchoCognitiveCore::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableEchoPropagation)
    {
        return;
    }

    // Decay activation levels over time
    DecayActivations(DeltaTime);

    // Spread activation along edges
    SpreadActivation();
}

// ========================================
// MEMORY OPERATIONS
// ========================================

FString UDeepTreeEchoCognitiveCore::AddNode(const FString& Content, EMemoryType MemoryType)
{
    FString NewNodeID = GenerateNodeID();

    FHypergraphNode NewNode;
    NewNode.NodeID = NewNodeID;
    NewNode.Content = Content;
    NewNode.MemoryType = MemoryType;
    NewNode.Activation = 0.0f;
    NewNode.CreationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    NewNode.LastAccessTime = NewNode.CreationTime;
    NewNode.AccessCount = 0;

    HypergraphNodes.Add(NewNodeID, NewNode);

    UE_LOG(LogTemp, Verbose, TEXT("DeepTreeEchoCognitiveCore: Added node %s: %s"), *NewNodeID, *Content);

    return NewNodeID;
}

FString UDeepTreeEchoCognitiveCore::AddEdge(const TArray<FString>& SourceNodeIDs, const TArray<FString>& TargetNodeIDs, const FString& RelationType, float Weight)
{
    // Validate source nodes exist
    for (const FString& SourceID : SourceNodeIDs)
    {
        if (!HypergraphNodes.Contains(SourceID))
        {
            UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoCognitiveCore: Source node %s not found"), *SourceID);
            return TEXT("");
        }
    }

    // Validate target nodes exist
    for (const FString& TargetID : TargetNodeIDs)
    {
        if (!HypergraphNodes.Contains(TargetID))
        {
            UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoCognitiveCore: Target node %s not found"), *TargetID);
            return TEXT("");
        }
    }

    FString NewEdgeID = GenerateEdgeID();

    FHypergraphEdge NewEdge;
    NewEdge.EdgeID = NewEdgeID;
    NewEdge.SourceNodeIDs = SourceNodeIDs;
    NewEdge.TargetNodeIDs = TargetNodeIDs;
    NewEdge.RelationType = RelationType;
    NewEdge.Weight = FMath::Clamp(Weight, 0.0f, 1.0f);
    NewEdge.Activation = 0.0f;

    HypergraphEdges.Add(NewEdgeID, NewEdge);

    UE_LOG(LogTemp, Verbose, TEXT("DeepTreeEchoCognitiveCore: Added edge %s (%s) with weight %.2f"),
           *NewEdgeID, *RelationType, Weight);

    return NewEdgeID;
}

void UDeepTreeEchoCognitiveCore::ActivateNode(const FString& NodeID, float ActivationLevel)
{
    if (FHypergraphNode* Node = HypergraphNodes.Find(NodeID))
    {
        // Set activation (clamped to 0-1)
        Node->Activation = FMath::Clamp(ActivationLevel, 0.0f, 1.0f);

        // Update access statistics
        Node->LastAccessTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        Node->AccessCount++;

        UE_LOG(LogTemp, Verbose, TEXT("DeepTreeEchoCognitiveCore: Activated node %s to %.2f"),
               *NodeID, Node->Activation);
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoCognitiveCore: Cannot activate unknown node %s"), *NodeID);
    }
}

FHypergraphNode UDeepTreeEchoCognitiveCore::GetNode(const FString& NodeID) const
{
    if (const FHypergraphNode* Node = HypergraphNodes.Find(NodeID))
    {
        return *Node;
    }
    return FHypergraphNode();
}

TArray<FHypergraphNode> UDeepTreeEchoCognitiveCore::SearchNodes(const FString& Query, EMemoryType MemoryType) const
{
    TArray<FHypergraphNode> Results;

    FString LowerQuery = Query.ToLower();

    for (const auto& Pair : HypergraphNodes)
    {
        const FHypergraphNode& Node = Pair.Value;

        // Filter by memory type if specified (Declarative is default/any)
        bool bTypeMatches = (Node.MemoryType == MemoryType);

        // Check if content contains query
        bool bContentMatches = Node.Content.ToLower().Contains(LowerQuery);

        if (bTypeMatches && bContentMatches)
        {
            Results.Add(Node);
        }
    }

    // Sort by activation level (highest first)
    Results.Sort([](const FHypergraphNode& A, const FHypergraphNode& B) {
        return A.Activation > B.Activation;
    });

    return Results;
}

TArray<FHypergraphNode> UDeepTreeEchoCognitiveCore::GetMostActivatedNodes(int32 Count) const
{
    TArray<FHypergraphNode> AllNodes;

    // Collect all nodes
    for (const auto& Pair : HypergraphNodes)
    {
        AllNodes.Add(Pair.Value);
    }

    // Sort by activation (highest first)
    AllNodes.Sort([](const FHypergraphNode& A, const FHypergraphNode& B) {
        return A.Activation > B.Activation;
    });

    // Return top N
    TArray<FHypergraphNode> TopNodes;
    int32 NumToReturn = FMath::Min(Count, AllNodes.Num());
    for (int32 i = 0; i < NumToReturn; i++)
    {
        TopNodes.Add(AllNodes[i]);
    }

    return TopNodes;
}

// ========================================
// ECHO PROPAGATION
// ========================================

void UDeepTreeEchoCognitiveCore::PropagateActivation()
{
    // Create a map to accumulate new activations
    TMap<FString, float> NewActivations;

    // Initialize with current activations
    for (const auto& Pair : HypergraphNodes)
    {
        NewActivations.Add(Pair.Key, Pair.Value.Activation);
    }

    // Spread activation along edges
    for (const auto& EdgePair : HypergraphEdges)
    {
        const FHypergraphEdge& Edge = EdgePair.Value;

        // Calculate source activation (average of source nodes)
        float SourceActivation = 0.0f;
        for (const FString& SourceID : Edge.SourceNodeIDs)
        {
            if (const FHypergraphNode* SourceNode = HypergraphNodes.Find(SourceID))
            {
                SourceActivation += SourceNode->Activation;
            }
        }
        if (Edge.SourceNodeIDs.Num() > 0)
        {
            SourceActivation /= Edge.SourceNodeIDs.Num();
        }

        // Spread to target nodes if source is activated
        if (SourceActivation > 0.01f)
        {
            float SpreadAmount = SourceActivation * Edge.Weight * ActivationSpreadFactor;

            for (const FString& TargetID : Edge.TargetNodeIDs)
            {
                if (float* CurrentActivation = NewActivations.Find(TargetID))
                {
                    *CurrentActivation = FMath::Min(1.0f, *CurrentActivation + SpreadAmount);
                }
            }

            // Update edge activation
            if (FHypergraphEdge* MutableEdge = HypergraphEdges.Find(EdgePair.Key))
            {
                MutableEdge->Activation = SourceActivation;
            }
        }
    }

    // Apply new activations
    for (const auto& Pair : NewActivations)
    {
        if (FHypergraphNode* Node = HypergraphNodes.Find(Pair.Key))
        {
            Node->Activation = Pair.Value;
        }
    }
}

TArray<FEchoPropagationPattern> UDeepTreeEchoCognitiveCore::DetectPatterns()
{
    TArray<FEchoPropagationPattern> NewPatterns;

    // Find clusters of highly activated nodes
    TArray<FString> ActivatedNodeIDs;
    for (const auto& Pair : HypergraphNodes)
    {
        if (Pair.Value.Activation >= PatternRecognitionThreshold)
        {
            ActivatedNodeIDs.Add(Pair.Key);
        }
    }

    if (ActivatedNodeIDs.Num() < 2)
    {
        return NewPatterns;
    }

    // Find connected clusters among activated nodes
    TSet<FString> ProcessedNodes;

    for (const FString& StartNodeID : ActivatedNodeIDs)
    {
        if (ProcessedNodes.Contains(StartNodeID))
        {
            continue;
        }

        // BFS to find connected activated nodes
        TArray<FString> Cluster;
        TArray<FString> Queue;
        Queue.Add(StartNodeID);

        while (Queue.Num() > 0)
        {
            FString CurrentID = Queue[0];
            Queue.RemoveAt(0);

            if (ProcessedNodes.Contains(CurrentID))
            {
                continue;
            }

            if (const FHypergraphNode* Node = HypergraphNodes.Find(CurrentID))
            {
                if (Node->Activation >= PatternRecognitionThreshold)
                {
                    Cluster.Add(CurrentID);
                    ProcessedNodes.Add(CurrentID);

                    // Add connected nodes to queue
                    TArray<FString> Connected = FindConnectedNodes(CurrentID);
                    for (const FString& ConnectedID : Connected)
                    {
                        if (!ProcessedNodes.Contains(ConnectedID) &&
                            ActivatedNodeIDs.Contains(ConnectedID))
                        {
                            Queue.Add(ConnectedID);
                        }
                    }
                }
            }
        }

        // Create pattern if cluster has 2+ nodes
        if (Cluster.Num() >= 2)
        {
            FEchoPropagationPattern Pattern;
            Pattern.PatternID = GeneratePatternID();
            Pattern.NodeIDs = Cluster;

            // Calculate pattern strength (average activation)
            float TotalActivation = 0.0f;
            for (const FString& NodeID : Cluster)
            {
                if (const FHypergraphNode* Node = HypergraphNodes.Find(NodeID))
                {
                    TotalActivation += Node->Activation;
                }
            }
            Pattern.Strength = TotalActivation / Cluster.Num();

            // Generate description
            TArray<FString> NodeContents;
            for (const FString& NodeID : Cluster)
            {
                if (const FHypergraphNode* Node = HypergraphNodes.Find(NodeID))
                {
                    NodeContents.Add(Node->Content);
                }
            }
            Pattern.Description = FString::Join(NodeContents, TEXT(" <-> "));

            // Check if similar pattern already exists
            bool bIsNewPattern = true;
            for (FEchoPropagationPattern& ExistingPattern : DetectedPatterns)
            {
                // Compare node sets
                if (ExistingPattern.NodeIDs.Num() == Pattern.NodeIDs.Num())
                {
                    bool bSameNodes = true;
                    for (const FString& NodeID : Pattern.NodeIDs)
                    {
                        if (!ExistingPattern.NodeIDs.Contains(NodeID))
                        {
                            bSameNodes = false;
                            break;
                        }
                    }

                    if (bSameNodes)
                    {
                        // Update existing pattern
                        ExistingPattern.Frequency++;
                        ExistingPattern.Strength = FMath::Max(ExistingPattern.Strength, Pattern.Strength);
                        bIsNewPattern = false;
                        break;
                    }
                }
            }

            if (bIsNewPattern)
            {
                Pattern.Frequency = 1;
                DetectedPatterns.Add(Pattern);
                NewPatterns.Add(Pattern);

                UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoCognitiveCore: Detected new pattern %s: %s"),
                       *Pattern.PatternID, *Pattern.Description);
            }
        }
    }

    return NewPatterns;
}

FEchoPropagationPattern UDeepTreeEchoCognitiveCore::GetPattern(const FString& PatternID) const
{
    for (const FEchoPropagationPattern& Pattern : DetectedPatterns)
    {
        if (Pattern.PatternID == PatternID)
        {
            return Pattern;
        }
    }
    return FEchoPropagationPattern();
}

// ========================================
// MEMBRANE OPERATIONS
// ========================================

FString UDeepTreeEchoCognitiveCore::CreateMembrane(const FString& MembraneType, const FString& ParentMembraneID)
{
    FString NewMembraneID = GenerateMembraneID();

    FMembraneContext NewMembrane;
    NewMembrane.MembraneID = NewMembraneID;
    NewMembrane.MembraneType = MembraneType;

    // Determine parent and nesting level
    if (ParentMembraneID.IsEmpty())
    {
        // Use current membrane as parent
        NewMembrane.ParentMembraneID = CurrentMembraneID;
    }
    else
    {
        NewMembrane.ParentMembraneID = ParentMembraneID;
    }

    // Calculate nesting level based on OEIS A000081 (rooted tree enumeration)
    // Level 1: 1 term (global), Level 2: 2 terms, Level 3: 4 terms, Level 4: 9 terms
    if (FMembraneContext* ParentMembrane = Membranes.Find(NewMembrane.ParentMembraneID))
    {
        NewMembrane.NestingLevel = ParentMembrane->NestingLevel + 1;

        // Add this membrane as child of parent
        ParentMembrane->ChildMembraneIDs.Add(NewMembraneID);
    }
    else
    {
        NewMembrane.NestingLevel = 1;
    }

    Membranes.Add(NewMembraneID, NewMembrane);

    UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoCognitiveCore: Created membrane %s (type: %s, level: %d)"),
           *NewMembraneID, *MembraneType, NewMembrane.NestingLevel);

    return NewMembraneID;
}

bool UDeepTreeEchoCognitiveCore::EnterMembrane(const FString& MembraneID)
{
    if (!Membranes.Contains(MembraneID))
    {
        UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoCognitiveCore: Cannot enter unknown membrane %s"), *MembraneID);
        return false;
    }

    FString PreviousMembraneID = CurrentMembraneID;
    CurrentMembraneID = MembraneID;

    UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoCognitiveCore: Entered membrane %s (from %s)"),
           *MembraneID, *PreviousMembraneID);

    return true;
}

bool UDeepTreeEchoCognitiveCore::ExitMembrane()
{
    if (FMembraneContext* CurrentMembrane = Membranes.Find(CurrentMembraneID))
    {
        if (CurrentMembrane->ParentMembraneID.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("DeepTreeEchoCognitiveCore: Cannot exit root membrane"));
            return false;
        }

        FString ChildMembraneID = CurrentMembraneID;
        CurrentMembraneID = CurrentMembrane->ParentMembraneID;

        UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoCognitiveCore: Exited membrane %s (to parent %s)"),
               *ChildMembraneID, *CurrentMembraneID);

        return true;
    }

    return false;
}

FMembraneContext UDeepTreeEchoCognitiveCore::GetCurrentMembrane() const
{
    if (const FMembraneContext* Membrane = Membranes.Find(CurrentMembraneID))
    {
        return *Membrane;
    }
    return FMembraneContext();
}

int32 UDeepTreeEchoCognitiveCore::GetMembraneNestingDepth() const
{
    if (const FMembraneContext* Membrane = Membranes.Find(CurrentMembraneID))
    {
        return Membrane->NestingLevel;
    }
    return 0;
}

// ========================================
// INTERNAL METHODS
// ========================================

void UDeepTreeEchoCognitiveCore::InitializeRootMembrane()
{
    FString RootID = GenerateMembraneID();

    FMembraneContext RootMembrane;
    RootMembrane.MembraneID = RootID;
    RootMembrane.MembraneType = TEXT("Root");
    RootMembrane.ParentMembraneID = TEXT("");
    RootMembrane.NestingLevel = 1;

    Membranes.Add(RootID, RootMembrane);
    CurrentMembraneID = RootID;

    UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoCognitiveCore: Initialized root membrane %s"), *RootID);
}

void UDeepTreeEchoCognitiveCore::DecayActivations(float DeltaTime)
{
    float DecayAmount = ActivationDecayRate * DeltaTime;

    for (auto& Pair : HypergraphNodes)
    {
        FHypergraphNode& Node = Pair.Value;
        Node.Activation = FMath::Max(0.0f, Node.Activation - DecayAmount);
    }

    for (auto& Pair : HypergraphEdges)
    {
        FHypergraphEdge& Edge = Pair.Value;
        Edge.Activation = FMath::Max(0.0f, Edge.Activation - DecayAmount);
    }
}

void UDeepTreeEchoCognitiveCore::SpreadActivation()
{
    // Use PropagateActivation for spreading
    PropagateActivation();
}

TArray<FString> UDeepTreeEchoCognitiveCore::FindConnectedNodes(const FString& NodeID) const
{
    TArray<FString> ConnectedNodes;

    for (const auto& EdgePair : HypergraphEdges)
    {
        const FHypergraphEdge& Edge = EdgePair.Value;

        // Check if NodeID is in source nodes
        if (Edge.SourceNodeIDs.Contains(NodeID))
        {
            for (const FString& TargetID : Edge.TargetNodeIDs)
            {
                if (TargetID != NodeID && !ConnectedNodes.Contains(TargetID))
                {
                    ConnectedNodes.Add(TargetID);
                }
            }
        }

        // Check if NodeID is in target nodes (bidirectional connectivity)
        if (Edge.TargetNodeIDs.Contains(NodeID))
        {
            for (const FString& SourceID : Edge.SourceNodeIDs)
            {
                if (SourceID != NodeID && !ConnectedNodes.Contains(SourceID))
                {
                    ConnectedNodes.Add(SourceID);
                }
            }
        }
    }

    return ConnectedNodes;
}

FString UDeepTreeEchoCognitiveCore::GenerateNodeID()
{
    return FString::Printf(TEXT("N_%d"), ++NodeIDCounter);
}

FString UDeepTreeEchoCognitiveCore::GenerateEdgeID()
{
    return FString::Printf(TEXT("E_%d"), ++EdgeIDCounter);
}

FString UDeepTreeEchoCognitiveCore::GeneratePatternID()
{
    return FString::Printf(TEXT("P_%d"), ++PatternIDCounter);
}

FString UDeepTreeEchoCognitiveCore::GenerateMembraneID()
{
    return FString::Printf(TEXT("M_%d"), ++MembraneIDCounter);
}

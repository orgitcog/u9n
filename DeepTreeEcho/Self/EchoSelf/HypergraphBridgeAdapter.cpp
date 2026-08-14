// HypergraphBridgeAdapter.cpp
// Deep Tree Echo - Hypergraph Bridge Adapter
// Copyright (c) 2026 Deep Tree Echo Project

#include "HypergraphBridgeAdapter.h"
#include "Self/CoreSelfEngine.h"

UHypergraphBridgeAdapter::UHypergraphBridgeAdapter()
{
    PrimaryComponentTick.bCanEverTick = false;

    // Default salience keywords (from echoself's cognitive domain)
    SalienceKeywords = {
        TEXT("cognitive"), TEXT("memory"), TEXT("emotion"), TEXT("pattern"),
        TEXT("insight"), TEXT("wisdom"), TEXT("resonance"), TEXT("echo"),
        TEXT("awareness"), TEXT("perception"), TEXT("embodied"), TEXT("neural"),
        TEXT("hypergraph"), TEXT("consciousness"), TEXT("reservoir")
    };
}

// =============================================================================
// Node Operations
// =============================================================================

FString UHypergraphBridgeAdapter::AddConceptNode(const FString& Content, float InitialSalience)
{
    FHypergraphNode Node;
    Node.NodeID = GenerateNodeID();
    Node.NodeType = TEXT("Concept");
    Node.Content = Content;
    Node.Salience = FMath::Clamp(InitialSalience, 0.0f, 1.0f);
    Node.AttentionValue = InitialSalience;
    Nodes.Add(Node.NodeID, Node);
    return Node.NodeID;
}

FString UHypergraphBridgeAdapter::AddPredicateNode(const FString& Content, float InitialSalience)
{
    FHypergraphNode Node;
    Node.NodeID = GenerateNodeID();
    Node.NodeType = TEXT("Predicate");
    Node.Content = Content;
    Node.Salience = FMath::Clamp(InitialSalience, 0.0f, 1.0f);
    Node.AttentionValue = InitialSalience;
    Nodes.Add(Node.NodeID, Node);
    return Node.NodeID;
}

FHypergraphNode UHypergraphBridgeAdapter::GetNode(const FString& NodeID) const
{
    if (const FHypergraphNode* Node = Nodes.Find(NodeID))
    {
        return *Node;
    }
    return FHypergraphNode();
}

bool UHypergraphBridgeAdapter::RemoveNode(const FString& NodeID)
{
    return Nodes.Remove(NodeID) > 0;
}

// =============================================================================
// Edge Operations
// =============================================================================

FString UHypergraphBridgeAdapter::CreateEdge(const FString& EdgeType,
                                              const TArray<FString>& NodeIDs, float Weight)
{
    FHypergraphEdge Edge;
    Edge.EdgeID = GenerateEdgeID();
    Edge.EdgeType = EdgeType;
    Edge.ConnectedNodeIDs = NodeIDs;
    Edge.Weight = Weight;
    Edges.Add(Edge.EdgeID, Edge);
    return Edge.EdgeID;
}

TArray<FHypergraphEdge> UHypergraphBridgeAdapter::GetEdgesForNode(const FString& NodeID) const
{
    TArray<FHypergraphEdge> Result;
    for (const auto& Pair : Edges)
    {
        if (Pair.Value.ConnectedNodeIDs.Contains(NodeID))
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

// =============================================================================
// Salience & Attention
// =============================================================================

float UHypergraphBridgeAdapter::CalculateSalience(const FString& Content) const
{
    // Port of echoself's HypergraphBridge.calculate_salience()
    // Computes keyword density against salience keywords
    if (Content.IsEmpty() || SalienceKeywords.Num() == 0)
    {
        return 0.0f;
    }

    FString LowerContent = Content.ToLower();
    int32 MatchCount = 0;

    for (const FString& Keyword : SalienceKeywords)
    {
        if (LowerContent.Contains(Keyword.ToLower()))
        {
            MatchCount++;
        }
    }

    // Normalize: salience = matches / total_keywords, clamped to [0, 1]
    float RawSalience = static_cast<float>(MatchCount) / static_cast<float>(SalienceKeywords.Num());

    // Apply sigmoid-like scaling for non-linear emphasis
    float Scaled = 1.0f / (1.0f + FMath::Exp(-10.0f * (RawSalience - 0.3f)));
    return FMath::Clamp(Scaled, 0.0f, 1.0f);
}

float UHypergraphBridgeAdapter::GetAdaptiveAttentionThreshold() const
{
    // Port of echoself's adaptive_attention_threshold()
    // Dynamically adjusts based on current graph density
    if (Nodes.Num() == 0)
    {
        return 0.5f; // Default threshold
    }

    // Compute mean salience across all nodes
    float TotalSalience = 0.0f;
    for (const auto& Pair : Nodes)
    {
        TotalSalience += Pair.Value.Salience;
    }
    float MeanSalience = TotalSalience / static_cast<float>(Nodes.Num());

    // Threshold is mean + half standard deviation
    float Variance = 0.0f;
    for (const auto& Pair : Nodes)
    {
        float Diff = Pair.Value.Salience - MeanSalience;
        Variance += Diff * Diff;
    }
    Variance /= static_cast<float>(Nodes.Num());
    float StdDev = FMath::Sqrt(Variance);

    return FMath::Clamp(MeanSalience + StdDev * 0.5f, 0.1f, 0.9f);
}

TArray<FHypergraphNode> UHypergraphBridgeAdapter::GetSalientNodes() const
{
    float Threshold = GetAdaptiveAttentionThreshold();
    TArray<FHypergraphNode> Result;

    for (const auto& Pair : Nodes)
    {
        if (Pair.Value.Salience >= Threshold)
        {
            Result.Add(Pair.Value);
        }
    }

    // Sort by salience descending
    Result.Sort([](const FHypergraphNode& A, const FHypergraphNode& B)
    {
        return A.Salience > B.Salience;
    });

    return Result;
}

FString UHypergraphBridgeAdapter::CreateCognitivePrompt(int32 MaxNodes) const
{
    // Port of echoself's create_cognitive_prompt()
    TArray<FHypergraphNode> Salient = GetSalientNodes();

    FString Prompt = TEXT("Context from cognitive hypergraph:\n");

    int32 Count = FMath::Min(MaxNodes, Salient.Num());
    for (int32 i = 0; i < Count; ++i)
    {
        Prompt += FString::Printf(TEXT("- [%s] %s (salience: %.2f)\n"),
                                   *Salient[i].NodeType,
                                   *Salient[i].Content,
                                   Salient[i].Salience);
    }

    return Prompt;
}

// =============================================================================
// Internal
// =============================================================================

FString UHypergraphBridgeAdapter::GenerateNodeID()
{
    return FString::Printf(TEXT("node-%d"), NextNodeID++);
}

FString UHypergraphBridgeAdapter::GenerateEdgeID()
{
    return FString::Printf(TEXT("edge-%d"), NextEdgeID++);
}

void UHypergraphBridgeAdapter::IngestCoreSelf(const FCoreSelfEngine& Self)
{
    if (!Self.IsInitialized())
    {
        return;
    }

    TMap<FString, FString> ContentToId;
    for (const auto& Pair : Nodes)
    {
        ContentToId.Add(Pair.Value.Content, Pair.Key);
    }

    auto UpsertConcept = [this, &ContentToId](const FString& Content, float Salience) -> FString
    {
        if (const FString* Existing = ContentToId.Find(Content))
        {
            if (FHypergraphNode* Node = Nodes.Find(*Existing))
            {
                Node->Salience = FMath::Clamp(0.5f * Node->Salience + 0.5f * Salience, 0.0f, 1.0f);
                Node->AttentionValue = Node->Salience;
            }
            return *Existing;
        }
        FString Id = AddConceptNode(Content, Salience);
        ContentToId.Add(Content, Id);
        return Id;
    };

    for (const FIdentityGene& Gene : Self.GetGenes())
    {
        UpsertConcept(Gene.Id + TEXT(":") + Gene.Category, Gene.Expression);
    }

    for (const FIdentityEdge& Edge : Self.GetIdentityEdges())
    {
        const FString Src = UpsertConcept(Edge.SourceId, Edge.Strength);
        const FString Dst = UpsertConcept(Edge.TargetId, Edge.Strength);
        TArray<FString> Linked;
        Linked.Add(Src);
        Linked.Add(Dst);
        CreateEdge(Edge.Type, Linked, Edge.Weight);
    }
}

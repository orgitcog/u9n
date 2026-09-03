// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Shared Visualization Space Implementation

#include "SharedVisualizationSpace.h"
#include "../AtomSpace/AvatarAtomSpaceClient.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"

USharedVisualizationSpace::USharedVisualizationSpace()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.033f; // ~30 Hz for visualization updates

    bNeedsVisualUpdate = false;
    LayoutTimer = 0.0f;
    SyncTimer = 0.0f;
}

void USharedVisualizationSpace::BeginPlay()
{
    Super::BeginPlay();

    // Get AtomSpace client for knowledge sync
    AtomSpaceClient = GetOwner()->FindComponentByClass<UAvatarAtomSpaceClient>();

    // Create visualization components
    CreateVisualizationComponents();
}

void USharedVisualizationSpace::TickComponent(float DeltaTime, ELevelTick TickType,
                                               FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update attention particles
    if (Configuration.bShowAttentionFlow)
    {
        UpdateAttentionParticles(DeltaTime);
    }

    // Periodic knowledge sync
    SyncTimer += DeltaTime;
    if (SyncTimer >= 1.0f) // Every second
    {
        SyncTimer = 0.0f;
        if (AtomSpaceClient)
        {
            SyncKnowledgeFromAtomSpace();
        }
    }

    // Update visuals if needed
    if (bNeedsVisualUpdate)
    {
        bNeedsVisualUpdate = false;
        UpdateNodeVisuals();
        UpdateEdgeVisuals();
        UpdateLinkVisuals();
    }

    // Periodic layout optimization
    LayoutTimer += DeltaTime;
    if (LayoutTimer >= 5.0f) // Every 5 seconds
    {
        LayoutTimer = 0.0f;
        ApplyForceDirectedStep(0.1f);
    }

    // Detect shared insights between agents
    DetectSharedInsights();
}

// ===== Initialization =====

void USharedVisualizationSpace::Initialize(const TArray<FString>& AgentPaths)
{
    ParticipatingAgents = AgentPaths;

    // Clear existing data
    KnowledgeNodes.Empty();
    KnowledgeEdges.Empty();
    AttentionParticles.Empty();
    ActiveLinks.Empty();

    // Create initial structure
    for (const FString& AgentPath : AgentPaths)
    {
        // Create central node for each agent
        FKnowledgeNode AgentNode;
        AgentNode.NodeId = AgentPath + TEXT("_center");
        AgentNode.NodeType = TEXT("agent");
        AgentNode.Label = FPaths::GetBaseFilename(AgentPath);
        AgentNode.Size = 20.0f;
        AgentNode.Color = FLinearColor(0.4f, 0.6f, 1.0f, 1.0f);
        AgentNode.OwnerAgent = AgentPath;

        // Position agents in circle
        int32 Index = ParticipatingAgents.Find(AgentPath);
        float Angle = (2.0f * PI * Index) / FMath::Max(1, ParticipatingAgents.Num());
        AgentNode.Position = Configuration.CenterPosition;
        AgentNode.Position.X += FMath::Cos(Angle) * Configuration.SpaceRadius * 0.3f;
        AgentNode.Position.Y += FMath::Sin(Angle) * Configuration.SpaceRadius * 0.3f;

        AddKnowledgeNode(AgentNode);
    }

    bNeedsVisualUpdate = true;

    UE_LOG(LogTemp, Log, TEXT("SharedVisualizationSpace initialized with %d agents"), AgentPaths.Num());
}

void USharedVisualizationSpace::SetConfiguration(const FSharedSpaceConfiguration& Config)
{
    Configuration = Config;
    bNeedsVisualUpdate = true;
}

// ===== Agent Management =====

void USharedVisualizationSpace::AddAgent(const FString& AgentPath)
{
    if (!ParticipatingAgents.Contains(AgentPath))
    {
        ParticipatingAgents.Add(AgentPath);

        // Create agent node
        FKnowledgeNode AgentNode;
        AgentNode.NodeId = AgentPath + TEXT("_center");
        AgentNode.NodeType = TEXT("agent");
        AgentNode.Label = FPaths::GetBaseFilename(AgentPath);
        AgentNode.Size = 20.0f;
        AgentNode.Color = FLinearColor(0.4f, 0.6f, 1.0f, 1.0f);
        AgentNode.OwnerAgent = AgentPath;

        AddKnowledgeNode(AgentNode);
    }
}

void USharedVisualizationSpace::RemoveAgent(const FString& AgentPath)
{
    ParticipatingAgents.Remove(AgentPath);

    // Remove agent's nodes
    TArray<FString> NodesToRemove;
    for (auto& Pair : KnowledgeNodes)
    {
        if (Pair.Value.OwnerAgent == AgentPath)
        {
            NodesToRemove.Add(Pair.Key);
        }
    }

    for (const FString& NodeId : NodesToRemove)
    {
        RemoveKnowledgeNode(NodeId);
    }

    // Remove agent's attention flows
    AttentionParticles.RemoveAll([&AgentPath](const FAttentionFlowParticle& Particle) {
        return Particle.SourceAgent == AgentPath;
    });

    bNeedsVisualUpdate = true;
}

TArray<FString> USharedVisualizationSpace::GetParticipatingAgents()
{
    return ParticipatingAgents;
}

// ===== Knowledge Graph Visualization =====

void USharedVisualizationSpace::AddKnowledgeNode(const FKnowledgeNode& Node)
{
    KnowledgeNodes.Add(Node.NodeId, Node);
    SpawnNodeMesh(Node);
    bNeedsVisualUpdate = true;
}

void USharedVisualizationSpace::RemoveKnowledgeNode(const FString& NodeId)
{
    KnowledgeNodes.Remove(NodeId);

    // Remove associated mesh
    if (UStaticMeshComponent** MeshPtr = NodeMeshes.Find(NodeId))
    {
        if (*MeshPtr)
        {
            (*MeshPtr)->DestroyComponent();
        }
        NodeMeshes.Remove(NodeId);
    }

    // Remove edges connected to this node
    KnowledgeEdges.RemoveAll([&NodeId](const FKnowledgeEdge& Edge) {
        return Edge.SourceNodeId == NodeId || Edge.TargetNodeId == NodeId;
    });

    bNeedsVisualUpdate = true;
}

void USharedVisualizationSpace::AddKnowledgeEdge(const FKnowledgeEdge& Edge)
{
    // Check if both nodes exist
    if (KnowledgeNodes.Contains(Edge.SourceNodeId) && KnowledgeNodes.Contains(Edge.TargetNodeId))
    {
        KnowledgeEdges.Add(Edge);
        SpawnEdgeMesh(Edge);
        bNeedsVisualUpdate = true;
    }
}

void USharedVisualizationSpace::RemoveKnowledgeEdge(const FString& EdgeId)
{
    KnowledgeEdges.RemoveAll([&EdgeId](const FKnowledgeEdge& Edge) {
        return Edge.EdgeId == EdgeId;
    });

    // Remove spline
    if (USplineComponent** SplinePtr = EdgeSplines.Find(EdgeId))
    {
        if (*SplinePtr)
        {
            (*SplinePtr)->DestroyComponent();
        }
        EdgeSplines.Remove(EdgeId);
    }

    bNeedsVisualUpdate = true;
}

void USharedVisualizationSpace::UpdateKnowledgeGraph()
{
    UpdateNodeVisuals();
    UpdateEdgeVisuals();
}

void USharedVisualizationSpace::SyncKnowledgeFromAtomSpace()
{
    if (!AtomSpaceClient)
    {
        return;
    }

    // Query high-attention atoms for each participating agent
    for (const FString& AgentPath : ParticipatingAgents)
    {
        TArray<FAtom> HighAttentionAtoms = AtomSpaceClient->QueryByAttention(0.5f);

        for (const FAtom& Atom : HighAttentionAtoms)
        {
            // Create node for atom if not exists
            FString NodeId = Atom.AtomId;
            if (!KnowledgeNodes.Contains(NodeId))
            {
                FKnowledgeNode Node;
                Node.NodeId = NodeId;
                Node.NodeType = Atom.AtomType == EAtomType::ConceptNode ? TEXT("concept") : TEXT("predicate");
                Node.Label = Atom.Name;
                Node.AttentionValue = Atom.Attention.STI;
                Node.TruthValue = Atom.Truth.Strength;
                Node.Color = GetNodeColorForType(Node.NodeType);
                Node.Size = 5.0f + Node.AttentionValue * 15.0f;
                Node.OwnerAgent = AgentPath;

                AddKnowledgeNode(Node);
            }
            else
            {
                // Update existing node
                FKnowledgeNode& ExistingNode = KnowledgeNodes[NodeId];
                ExistingNode.AttentionValue = Atom.Attention.STI;
                ExistingNode.TruthValue = Atom.Truth.Strength;
                ExistingNode.Size = 5.0f + ExistingNode.AttentionValue * 15.0f;
            }
        }
    }

    bNeedsVisualUpdate = true;
}

void USharedVisualizationSpace::FocusOnNode(const FString& NodeId)
{
    if (FKnowledgeNode* Node = KnowledgeNodes.Find(NodeId))
    {
        FocusedNodeId = NodeId;
        OnKnowledgeNodeFocused.Broadcast(*Node);

        // Highlight connected nodes
        for (const FKnowledgeEdge& Edge : KnowledgeEdges)
        {
            if (Edge.SourceNodeId == NodeId || Edge.TargetNodeId == NodeId)
            {
                FString OtherNodeId = (Edge.SourceNodeId == NodeId) ? Edge.TargetNodeId : Edge.SourceNodeId;
                if (FKnowledgeNode* OtherNode = KnowledgeNodes.Find(OtherNodeId))
                {
                    // Increase visual prominence
                    OtherNode->Size *= 1.2f;
                }
            }
        }

        bNeedsVisualUpdate = true;
    }
}

TArray<FKnowledgeNode> USharedVisualizationSpace::GetVisibleNodes()
{
    TArray<FKnowledgeNode> Nodes;
    KnowledgeNodes.GenerateValueArray(Nodes);

    // Sort by attention value
    Nodes.Sort([](const FKnowledgeNode& A, const FKnowledgeNode& B) {
        return A.AttentionValue > B.AttentionValue;
    });

    // Limit to max visible
    if (Nodes.Num() > Configuration.MaxVisibleNodes)
    {
        Nodes.SetNum(Configuration.MaxVisibleNodes);
    }

    return Nodes;
}

void USharedVisualizationSpace::SetKnowledgeGraphVisible(bool bVisible)
{
    Configuration.bShowKnowledgeGraph = bVisible;

    // Update mesh visibility
    for (auto& Pair : NodeMeshes)
    {
        if (Pair.Value)
        {
            Pair.Value->SetVisibility(bVisible);
        }
    }

    for (auto& Pair : EdgeSplines)
    {
        if (Pair.Value)
        {
            Pair.Value->SetVisibility(bVisible);
        }
    }
}

// ===== Attention Flow Visualization =====

void USharedVisualizationSpace::AddAttentionFlow(const FString& SourceAgent, const FString& TargetConcept,
                                                  float Intensity)
{
    FAttentionFlowParticle Particle;
    Particle.SourceAgent = SourceAgent;
    Particle.TargetConcept = TargetConcept;
    Particle.Intensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    Particle.Progress = 0.0f;

    // Set color based on agent
    int32 AgentIndex = ParticipatingAgents.Find(SourceAgent);
    float Hue = (float)AgentIndex / FMath::Max(1, ParticipatingAgents.Num());
    Particle.Color = FLinearColor::MakeFromHSV8(Hue * 255, 200, 255);

    // Get starting position from agent node
    FString AgentNodeId = SourceAgent + TEXT("_center");
    if (FKnowledgeNode* AgentNode = KnowledgeNodes.Find(AgentNodeId))
    {
        Particle.CurrentPosition = AgentNode->Position;
    }

    AttentionParticles.Add(Particle);
}

void USharedVisualizationSpace::ClearAttentionFlows()
{
    AttentionParticles.Empty();
}

void USharedVisualizationSpace::SetAttentionFlowVisible(bool bVisible)
{
    Configuration.bShowAttentionFlow = bVisible;

    if (AttentionParticleSystem)
    {
        AttentionParticleSystem->SetVisibility(bVisible);
    }
}

void USharedVisualizationSpace::HighlightSharedAttention(const TArray<FString>& Agents, const FString& Concept)
{
    // Find concept node
    for (auto& Pair : KnowledgeNodes)
    {
        if (Pair.Value.Label == Concept || Pair.Value.NodeId == Concept)
        {
            // Create attention flows from all agents to this concept
            for (const FString& Agent : Agents)
            {
                AddAttentionFlow(Agent, Pair.Key, 1.0f);
            }

            // Highlight the concept node
            Pair.Value.Size *= 1.5f;
            Pair.Value.Color = FLinearColor(1.0f, 0.9f, 0.3f, 1.0f); // Gold

            bNeedsVisualUpdate = true;
            break;
        }
    }
}

void USharedVisualizationSpace::UpdateAttentionParticles(float DeltaTime)
{
    TArray<int32> ParticlesToRemove;

    for (int32 i = 0; i < AttentionParticles.Num(); ++i)
    {
        FAttentionFlowParticle& Particle = AttentionParticles[i];

        // Update progress
        Particle.Progress += DeltaTime * Configuration.AttentionFlowSpeed / Configuration.SpaceRadius;

        if (Particle.Progress >= 1.0f)
        {
            ParticlesToRemove.Add(i);
            continue;
        }

        // Get target position
        FVector TargetPosition = Configuration.CenterPosition;
        if (FKnowledgeNode* TargetNode = KnowledgeNodes.Find(Particle.TargetConcept))
        {
            TargetPosition = TargetNode->Position;
        }

        // Get source position
        FVector SourcePosition = Configuration.CenterPosition;
        FString AgentNodeId = Particle.SourceAgent + TEXT("_center");
        if (FKnowledgeNode* SourceNode = KnowledgeNodes.Find(AgentNodeId))
        {
            SourcePosition = SourceNode->Position;
        }

        // Interpolate position with slight curve
        float t = Particle.Progress;
        FVector MidPoint = (SourcePosition + TargetPosition) * 0.5f;
        MidPoint.Z += 50.0f; // Arc upward

        // Quadratic bezier
        FVector P0 = SourcePosition;
        FVector P1 = MidPoint;
        FVector P2 = TargetPosition;
        Particle.CurrentPosition = FMath::Lerp(FMath::Lerp(P0, P1, t), FMath::Lerp(P1, P2, t), t);
    }

    // Remove completed particles (reverse order)
    for (int32 i = ParticlesToRemove.Num() - 1; i >= 0; --i)
    {
        AttentionParticles.RemoveAt(ParticlesToRemove[i]);
    }
}

// ===== Communication Link Visualization =====

void USharedVisualizationSpace::AddCommunicationLink(const FCommunicationLink& Link)
{
    // Check if link already exists
    for (FCommunicationLink& ExistingLink : ActiveLinks)
    {
        if (ExistingLink.SourceAgentPath == Link.SourceAgentPath &&
            ExistingLink.TargetAgentPath == Link.TargetAgentPath)
        {
            // Update existing
            ExistingLink = Link;
            bNeedsVisualUpdate = true;
            return;
        }
    }

    ActiveLinks.Add(Link);
    bNeedsVisualUpdate = true;
}

void USharedVisualizationSpace::RemoveCommunicationLink(const FString& SourceAgent, const FString& TargetAgent)
{
    ActiveLinks.RemoveAll([&SourceAgent, &TargetAgent](const FCommunicationLink& Link) {
        return Link.SourceAgentPath == SourceAgent && Link.TargetAgentPath == TargetAgent;
    });

    // Remove spline
    FString SplineId = SourceAgent + TEXT("_to_") + TargetAgent;
    if (USplineComponent** SplinePtr = LinkSplines.Find(SplineId))
    {
        if (*SplinePtr)
        {
            (*SplinePtr)->DestroyComponent();
        }
        LinkSplines.Remove(SplineId);
    }

    bNeedsVisualUpdate = true;
}

void USharedVisualizationSpace::UpdateCommunicationLinks(const TArray<FCommunicationLink>& Links)
{
    ActiveLinks = Links;
    bNeedsVisualUpdate = true;
}

void USharedVisualizationSpace::SetCommunicationLinksVisible(bool bVisible)
{
    Configuration.bShowCommunicationLinks = bVisible;

    for (auto& Pair : LinkSplines)
    {
        if (Pair.Value)
        {
            Pair.Value->SetVisibility(bVisible);
        }
    }
}

void USharedVisualizationSpace::PulseAllLinks(float Intensity)
{
    for (FCommunicationLink& Link : ActiveLinks)
    {
        Link.ActivityPulse = FMath::Clamp(Intensity, 0.0f, 1.0f);
    }

    bNeedsVisualUpdate = true;
}

// ===== Layout =====

void USharedVisualizationSpace::LayoutForceDirected(int32 Iterations)
{
    for (int32 i = 0; i < Iterations; ++i)
    {
        float Alpha = 1.0f - (float)i / (float)Iterations;
        ApplyForceDirectedStep(Alpha);
    }

    bNeedsVisualUpdate = true;
}

void USharedVisualizationSpace::LayoutRadial(const FString& CenterNodeId)
{
    if (!KnowledgeNodes.Contains(CenterNodeId))
    {
        return;
    }

    // Place center node at center
    FKnowledgeNode& CenterNode = KnowledgeNodes[CenterNodeId];
    CenterNode.Position = Configuration.CenterPosition;

    // Find nodes connected to center
    TSet<FString> FirstRing;
    for (const FKnowledgeEdge& Edge : KnowledgeEdges)
    {
        if (Edge.SourceNodeId == CenterNodeId)
        {
            FirstRing.Add(Edge.TargetNodeId);
        }
        else if (Edge.TargetNodeId == CenterNodeId)
        {
            FirstRing.Add(Edge.SourceNodeId);
        }
    }

    // Position first ring
    int32 Index = 0;
    float Radius = Configuration.SpaceRadius * 0.3f;
    for (const FString& NodeId : FirstRing)
    {
        if (FKnowledgeNode* Node = KnowledgeNodes.Find(NodeId))
        {
            float Angle = (2.0f * PI * Index) / FirstRing.Num();
            Node->Position = Configuration.CenterPosition;
            Node->Position.X += FMath::Cos(Angle) * Radius;
            Node->Position.Y += FMath::Sin(Angle) * Radius;
            Index++;
        }
    }

    // Position remaining nodes in outer ring
    TArray<FString> AllNodeIds;
    KnowledgeNodes.GetKeys(AllNodeIds);

    Index = 0;
    Radius = Configuration.SpaceRadius * 0.6f;
    for (const FString& NodeId : AllNodeIds)
    {
        if (NodeId != CenterNodeId && !FirstRing.Contains(NodeId))
        {
            if (FKnowledgeNode* Node = KnowledgeNodes.Find(NodeId))
            {
                float Angle = (2.0f * PI * Index) / (AllNodeIds.Num() - FirstRing.Num() - 1);
                Node->Position = Configuration.CenterPosition;
                Node->Position.X += FMath::Cos(Angle) * Radius;
                Node->Position.Y += FMath::Sin(Angle) * Radius;
                Index++;
            }
        }
    }

    bNeedsVisualUpdate = true;
}

void USharedVisualizationSpace::LayoutHierarchical()
{
    // Group nodes by type
    TMap<FString, TArray<FString>> NodesByType;
    for (auto& Pair : KnowledgeNodes)
    {
        NodesByType.FindOrAdd(Pair.Value.NodeType).Add(Pair.Key);
    }

    // Layout each type on different Z levels
    float ZOffset = 0.0f;
    for (auto& TypePair : NodesByType)
    {
        TArray<FString>& NodesOfType = TypePair.Value;

        int32 Count = NodesOfType.Num();
        for (int32 i = 0; i < Count; ++i)
        {
            if (FKnowledgeNode* Node = KnowledgeNodes.Find(NodesOfType[i]))
            {
                float Angle = (2.0f * PI * i) / FMath::Max(1, Count);
                float Radius = Configuration.SpaceRadius * 0.4f;

                Node->Position = Configuration.CenterPosition;
                Node->Position.X += FMath::Cos(Angle) * Radius;
                Node->Position.Y += FMath::Sin(Angle) * Radius;
                Node->Position.Z += ZOffset;
            }
        }

        ZOffset += 100.0f;
    }

    bNeedsVisualUpdate = true;
}

void USharedVisualizationSpace::AutoLayout()
{
    // Use force-directed for general cases
    LayoutForceDirected(100);
}

void USharedVisualizationSpace::ApplyForceDirectedStep(float Alpha)
{
    const float RepulsionStrength = 10000.0f;
    const float AttractionStrength = 0.1f;
    const float Damping = 0.8f;

    TMap<FString, FVector> Forces;

    // Calculate repulsion forces between all node pairs
    for (auto& PairA : KnowledgeNodes)
    {
        FVector Force = FVector::ZeroVector;

        for (auto& PairB : KnowledgeNodes)
        {
            if (PairA.Key != PairB.Key)
            {
                FVector Direction = PairA.Value.Position - PairB.Value.Position;
                float Distance = Direction.Size();
                if (Distance > 0.1f)
                {
                    Direction.Normalize();
                    Force += Direction * (RepulsionStrength / (Distance * Distance));
                }
            }
        }

        Forces.Add(PairA.Key, Force);
    }

    // Calculate attraction forces along edges
    for (const FKnowledgeEdge& Edge : KnowledgeEdges)
    {
        if (KnowledgeNodes.Contains(Edge.SourceNodeId) && KnowledgeNodes.Contains(Edge.TargetNodeId))
        {
            FKnowledgeNode& SourceNode = KnowledgeNodes[Edge.SourceNodeId];
            FKnowledgeNode& TargetNode = KnowledgeNodes[Edge.TargetNodeId];

            FVector Direction = TargetNode.Position - SourceNode.Position;
            float Distance = Direction.Size();
            if (Distance > 0.1f)
            {
                Direction.Normalize();
                FVector AttractionForce = Direction * Distance * AttractionStrength * Edge.Strength;

                Forces[Edge.SourceNodeId] += AttractionForce;
                Forces[Edge.TargetNodeId] -= AttractionForce;
            }
        }
    }

    // Apply forces
    for (auto& Pair : KnowledgeNodes)
    {
        if (FVector* Force = Forces.Find(Pair.Key))
        {
            Pair.Value.Position += (*Force) * Alpha * Damping;

            // Constrain to space radius
            FVector Offset = Pair.Value.Position - Configuration.CenterPosition;
            if (Offset.Size() > Configuration.SpaceRadius)
            {
                Offset.Normalize();
                Pair.Value.Position = Configuration.CenterPosition + Offset * Configuration.SpaceRadius;
            }
        }
    }
}

// ===== Visual Effects =====

void USharedVisualizationSpace::EnableAmbientParticles(bool bEnable)
{
    if (AmbientParticleSystem)
    {
        if (bEnable)
        {
            AmbientParticleSystem->Activate();
        }
        else
        {
            AmbientParticleSystem->Deactivate();
        }
    }
}

void USharedVisualizationSpace::SetVisualizationQuality(int32 Quality)
{
    Quality = FMath::Clamp(Quality, 0, 4);

    // Adjust particle counts and detail based on quality
    switch (Quality)
    {
    case 0: // Low
        Configuration.MaxVisibleNodes = 25;
        Configuration.EdgeThickness = 1.0f;
        break;
    case 1: // Medium
        Configuration.MaxVisibleNodes = 50;
        Configuration.EdgeThickness = 1.5f;
        break;
    case 2: // High
        Configuration.MaxVisibleNodes = 100;
        Configuration.EdgeThickness = 2.0f;
        break;
    case 3: // Ultra
        Configuration.MaxVisibleNodes = 200;
        Configuration.EdgeThickness = 2.5f;
        break;
    case 4: // Cinematic
        Configuration.MaxVisibleNodes = 500;
        Configuration.EdgeThickness = 3.0f;
        break;
    }

    bNeedsVisualUpdate = true;
}

void USharedVisualizationSpace::TriggerInsightEffect(FVector Location, FLinearColor Color)
{
    // Spawn insight particle burst at location
    // This would typically use Niagara system
    UE_LOG(LogTemp, Log, TEXT("Insight effect triggered at %s"), *Location.ToString());
}

void USharedVisualizationSpace::TriggerResonanceEffect(const TArray<FString>& NodeIds)
{
    // Pulse connected nodes
    for (const FString& NodeId : NodeIds)
    {
        if (FKnowledgeNode* Node = KnowledgeNodes.Find(NodeId))
        {
            Node->Size *= 1.3f;
            Node->Color = FLinearColor(1.0f, 0.9f, 0.5f, 1.0f);
        }
    }

    bNeedsVisualUpdate = true;
}

// ===== Interaction =====

FKnowledgeNode USharedVisualizationSpace::GetNodeAtLocation(FVector WorldLocation)
{
    float ClosestDistance = MAX_FLT;
    FKnowledgeNode ClosestNode;

    for (auto& Pair : KnowledgeNodes)
    {
        float Distance = FVector::Dist(WorldLocation, Pair.Value.Position);
        if (Distance < Pair.Value.Size * 2.0f && Distance < ClosestDistance)
        {
            ClosestDistance = Distance;
            ClosestNode = Pair.Value;
        }
    }

    return ClosestNode;
}

void USharedVisualizationSpace::SelectNode(const FString& NodeId)
{
    SelectedNodeId = NodeId;

    // Update visuals for selection
    for (auto& Pair : KnowledgeNodes)
    {
        if (UStaticMeshComponent** MeshPtr = NodeMeshes.Find(Pair.Key))
        {
            if (*MeshPtr && NodeMaterial)
            {
                float SelectionIntensity = (Pair.Key == NodeId) ? 1.5f : 1.0f;
                // Would set material parameter here
            }
        }
    }
}

void USharedVisualizationSpace::ClearSelection()
{
    SelectedNodeId.Empty();
    bNeedsVisualUpdate = true;
}

// ===== Internal Methods =====

void USharedVisualizationSpace::CreateVisualizationComponents()
{
    // Create default materials (would typically load from assets)
    // NodeMaterial, EdgeMaterial, LinkMaterial setup would go here

    // Create particle systems for attention flow and ambient effects
    // AttentionParticleSystem, AmbientParticleSystem setup would go here
}

void USharedVisualizationSpace::UpdateNodeVisuals()
{
    if (!Configuration.bShowKnowledgeGraph)
    {
        return;
    }

    for (auto& Pair : KnowledgeNodes)
    {
        if (UStaticMeshComponent** MeshPtr = NodeMeshes.Find(Pair.Key))
        {
            if (*MeshPtr)
            {
                (*MeshPtr)->SetWorldLocation(Pair.Value.Position);
                (*MeshPtr)->SetWorldScale3D(FVector(Pair.Value.Size / 10.0f));

                // Update material color
                if (NodeMaterial)
                {
                    NodeMaterial->SetVectorParameterValue(TEXT("Color"), Pair.Value.Color);
                    NodeMaterial->SetScalarParameterValue(TEXT("Intensity"), Pair.Value.AttentionValue + 0.5f);
                }
            }
        }
    }
}

void USharedVisualizationSpace::UpdateEdgeVisuals()
{
    if (!Configuration.bShowKnowledgeGraph)
    {
        return;
    }

    for (const FKnowledgeEdge& Edge : KnowledgeEdges)
    {
        if (USplineComponent** SplinePtr = EdgeSplines.Find(Edge.EdgeId))
        {
            if (*SplinePtr)
            {
                FKnowledgeNode* SourceNode = KnowledgeNodes.Find(Edge.SourceNodeId);
                FKnowledgeNode* TargetNode = KnowledgeNodes.Find(Edge.TargetNodeId);

                if (SourceNode && TargetNode)
                {
                    (*SplinePtr)->SetLocationAtSplinePoint(0, SourceNode->Position, ESplineCoordinateSpace::World);
                    (*SplinePtr)->SetLocationAtSplinePoint(1, TargetNode->Position, ESplineCoordinateSpace::World);
                }
            }
        }
    }
}

void USharedVisualizationSpace::UpdateLinkVisuals()
{
    if (!Configuration.bShowCommunicationLinks)
    {
        return;
    }

    for (const FCommunicationLink& Link : ActiveLinks)
    {
        FString SplineId = Link.SourceAgentPath + TEXT("_to_") + Link.TargetAgentPath;
        if (USplineComponent** SplinePtr = LinkSplines.Find(SplineId))
        {
            if (*SplinePtr)
            {
                // Get agent positions
                FString SourceNodeId = Link.SourceAgentPath + TEXT("_center");
                FString TargetNodeId = Link.TargetAgentPath + TEXT("_center");

                FKnowledgeNode* SourceNode = KnowledgeNodes.Find(SourceNodeId);
                FKnowledgeNode* TargetNode = KnowledgeNodes.Find(TargetNodeId);

                if (SourceNode && TargetNode)
                {
                    (*SplinePtr)->SetLocationAtSplinePoint(0, SourceNode->Position, ESplineCoordinateSpace::World);
                    (*SplinePtr)->SetLocationAtSplinePoint(1, TargetNode->Position, ESplineCoordinateSpace::World);
                }
            }
        }
    }
}

FVector USharedVisualizationSpace::CalculateNodePosition(const FKnowledgeNode& Node)
{
    // Default position calculation
    return Node.Position;
}

FLinearColor USharedVisualizationSpace::GetNodeColorForType(const FString& NodeType)
{
    if (NodeType == TEXT("concept"))
    {
        return FLinearColor(0.3f, 0.6f, 1.0f, 1.0f); // Blue
    }
    else if (NodeType == TEXT("predicate"))
    {
        return FLinearColor(0.8f, 0.4f, 0.8f, 1.0f); // Purple
    }
    else if (NodeType == TEXT("agent"))
    {
        return FLinearColor(0.3f, 0.9f, 0.5f, 1.0f); // Green
    }
    else if (NodeType == TEXT("memory"))
    {
        return FLinearColor(1.0f, 0.7f, 0.3f, 1.0f); // Orange
    }

    return FLinearColor(0.5f, 0.5f, 0.5f, 1.0f); // Gray for unknown
}

void USharedVisualizationSpace::SpawnNodeMesh(const FKnowledgeNode& Node)
{
    // Create sphere mesh for node visualization
    UStaticMeshComponent* MeshComponent = NewObject<UStaticMeshComponent>(this);
    if (MeshComponent)
    {
        // Would typically set static mesh here
        MeshComponent->SetWorldLocation(Node.Position);
        MeshComponent->SetWorldScale3D(FVector(Node.Size / 10.0f));
        MeshComponent->RegisterComponent();

        NodeMeshes.Add(Node.NodeId, MeshComponent);
    }
}

void USharedVisualizationSpace::SpawnEdgeMesh(const FKnowledgeEdge& Edge)
{
    // Create spline for edge visualization
    USplineComponent* SplineComponent = NewObject<USplineComponent>(this);
    if (SplineComponent)
    {
        FKnowledgeNode* SourceNode = KnowledgeNodes.Find(Edge.SourceNodeId);
        FKnowledgeNode* TargetNode = KnowledgeNodes.Find(Edge.TargetNodeId);

        if (SourceNode && TargetNode)
        {
            SplineComponent->ClearSplinePoints();
            SplineComponent->AddSplinePoint(SourceNode->Position, ESplineCoordinateSpace::World);
            SplineComponent->AddSplinePoint(TargetNode->Position, ESplineCoordinateSpace::World);
            SplineComponent->RegisterComponent();

            EdgeSplines.Add(Edge.EdgeId, SplineComponent);
        }
    }
}

void USharedVisualizationSpace::DetectSharedInsights()
{
    // Find concepts that multiple agents are attending to
    TMap<FString, TArray<FString>> ConceptToAgents;

    for (const FAttentionFlowParticle& Particle : AttentionParticles)
    {
        if (Particle.Intensity > 0.5f)
        {
            ConceptToAgents.FindOrAdd(Particle.TargetConcept).AddUnique(Particle.SourceAgent);
        }
    }

    // Broadcast shared insights
    for (auto& Pair : ConceptToAgents)
    {
        if (Pair.Value.Num() >= 2)
        {
            // Multiple agents attending to same concept
            OnSharedInsightDiscovered.Broadcast(Pair.Key, Pair.Value);

            // Visual effect
            HighlightSharedAttention(Pair.Value, Pair.Key);
        }
    }
}

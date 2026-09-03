#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "AgentVisualizationManager.h"
#include "SharedVisualizationSpace.generated.h"

// Forward declarations
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class UNiagaraComponent;
class USplineComponent;
class UAvatarAtomSpaceClient;

/**
 * Knowledge Node for graph visualization
 */
USTRUCT(BlueprintType)
struct FKnowledgeNode
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString NodeId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString NodeType; // "concept", "predicate", "agent", "memory"

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Label;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Position;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Size;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor Color;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttentionValue; // 0-1, from AtomSpace STI

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TruthValue; // 0-1, confidence

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> ConnectedNodes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString OwnerAgent;

    FKnowledgeNode()
        : Position(FVector::ZeroVector)
        , Size(10.0f)
        , Color(FLinearColor::White)
        , AttentionValue(0.5f)
        , TruthValue(1.0f)
    {}
};

/**
 * Knowledge Edge connecting nodes
 */
USTRUCT(BlueprintType)
struct FKnowledgeEdge
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EdgeId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EdgeType; // "inheritance", "evaluation", "similarity", "association"

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SourceNodeId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TargetNodeId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Strength; // 0-1

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor Color;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bBidirectional;

    FKnowledgeEdge()
        : Strength(0.5f)
        , Color(FLinearColor(0.5f, 0.5f, 0.5f, 0.5f))
        , bBidirectional(false)
    {}
};

/**
 * Attention Flow Particle
 */
USTRUCT(BlueprintType)
struct FAttentionFlowParticle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SourceAgent;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TargetConcept;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Intensity; // 0-1

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector CurrentPosition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Progress; // 0-1, along path

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor Color;

    FAttentionFlowParticle()
        : Intensity(1.0f)
        , CurrentPosition(FVector::ZeroVector)
        , Progress(0.0f)
        , Color(FLinearColor(1.0f, 0.8f, 0.2f, 1.0f))
    {}
};

/**
 * Shared Environment Configuration
 */
USTRUCT(BlueprintType)
struct FSharedSpaceConfiguration
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SpaceRadius;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector CenterPosition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bShowKnowledgeGraph;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bShowAttentionFlow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bShowCommunicationLinks;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float KnowledgeGraphScale;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EdgeThickness;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxVisibleNodes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttentionFlowSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FLinearColor BackgroundColor;

    FSharedSpaceConfiguration()
        : SpaceRadius(1000.0f)
        , CenterPosition(FVector::ZeroVector)
        , bShowKnowledgeGraph(true)
        , bShowAttentionFlow(true)
        , bShowCommunicationLinks(true)
        , KnowledgeGraphScale(1.0f)
        , EdgeThickness(2.0f)
        , MaxVisibleNodes(100)
        , AttentionFlowSpeed(200.0f)
        , BackgroundColor(FLinearColor(0.02f, 0.02f, 0.05f, 0.3f))
    {}
};

/**
 * Delegates for shared space events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnKnowledgeNodeFocused, const FKnowledgeNode&, Node);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSharedInsightDiscovered, const FString&, InsightId, const TArray<FString>&, ParticipatingAgents);

/**
 * Shared Visualization Space Component
 *
 * Creates a shared 3D environment for multiple AGI-OS agents to visualize:
 * - Knowledge Graph: Nodes and edges representing shared knowledge
 * - Attention Flow: Particle streams showing where agents are focusing
 * - Communication Links: Visual representation of inter-agent messaging
 *
 * This enables visual debugging and understanding of multi-agent cognitive processes.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API USharedVisualizationSpace : public USceneComponent
{
    GENERATED_BODY()

public:
    USharedVisualizationSpace();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ===== Initialization =====

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Setup")
    void Initialize(const TArray<FString>& AgentPaths);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Setup")
    void SetConfiguration(const FSharedSpaceConfiguration& Config);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Setup")
    FSharedSpaceConfiguration GetConfiguration() const { return Configuration; }

    // ===== Agent Management =====

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Agents")
    void AddAgent(const FString& AgentPath);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Agents")
    void RemoveAgent(const FString& AgentPath);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Agents")
    TArray<FString> GetParticipatingAgents();

    // ===== Knowledge Graph Visualization =====

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Knowledge")
    void AddKnowledgeNode(const FKnowledgeNode& Node);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Knowledge")
    void RemoveKnowledgeNode(const FString& NodeId);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Knowledge")
    void AddKnowledgeEdge(const FKnowledgeEdge& Edge);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Knowledge")
    void RemoveKnowledgeEdge(const FString& EdgeId);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Knowledge")
    void UpdateKnowledgeGraph();

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Knowledge")
    void SyncKnowledgeFromAtomSpace();

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Knowledge")
    void FocusOnNode(const FString& NodeId);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Knowledge")
    TArray<FKnowledgeNode> GetVisibleNodes();

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Knowledge")
    void SetKnowledgeGraphVisible(bool bVisible);

    // ===== Attention Flow Visualization =====

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Attention")
    void AddAttentionFlow(const FString& SourceAgent, const FString& TargetConcept, float Intensity);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Attention")
    void ClearAttentionFlows();

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Attention")
    void SetAttentionFlowVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Attention")
    void HighlightSharedAttention(const TArray<FString>& Agents, const FString& Concept);

    // ===== Communication Link Visualization =====

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Communication")
    void AddCommunicationLink(const FCommunicationLink& Link);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Communication")
    void RemoveCommunicationLink(const FString& SourceAgent, const FString& TargetAgent);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Communication")
    void UpdateCommunicationLinks(const TArray<FCommunicationLink>& Links);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Communication")
    void SetCommunicationLinksVisible(bool bVisible);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Communication")
    void PulseAllLinks(float Intensity);

    // ===== Layout =====

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Layout")
    void LayoutForceDirected(int32 Iterations = 50);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Layout")
    void LayoutRadial(const FString& CenterNodeId);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Layout")
    void LayoutHierarchical();

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Layout")
    void AutoLayout();

    // ===== Visual Effects =====

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Effects")
    void EnableAmbientParticles(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Effects")
    void SetVisualizationQuality(int32 Quality);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Effects")
    void TriggerInsightEffect(FVector Location, FLinearColor Color);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Effects")
    void TriggerResonanceEffect(const TArray<FString>& NodeIds);

    // ===== Interaction =====

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Interaction")
    FKnowledgeNode GetNodeAtLocation(FVector WorldLocation);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Interaction")
    void SelectNode(const FString& NodeId);

    UFUNCTION(BlueprintCallable, Category = "SharedSpace|Interaction")
    void ClearSelection();

    // ===== Events =====

    UPROPERTY(BlueprintAssignable, Category = "SharedSpace|Events")
    FOnKnowledgeNodeFocused OnKnowledgeNodeFocused;

    UPROPERTY(BlueprintAssignable, Category = "SharedSpace|Events")
    FOnSharedInsightDiscovered OnSharedInsightDiscovered;

protected:
    // ===== Internal Methods =====

    void CreateVisualizationComponents();
    void UpdateNodeVisuals();
    void UpdateEdgeVisuals();
    void UpdateAttentionParticles(float DeltaTime);
    void UpdateLinkVisuals();
    void ApplyForceDirectedStep(float Alpha);
    FVector CalculateNodePosition(const FKnowledgeNode& Node);
    FLinearColor GetNodeColorForType(const FString& NodeType);
    void SpawnNodeMesh(const FKnowledgeNode& Node);
    void SpawnEdgeMesh(const FKnowledgeEdge& Edge);
    void DetectSharedInsights();

private:
    // ===== Configuration =====

    UPROPERTY()
    FSharedSpaceConfiguration Configuration;

    // ===== Participating Agents =====

    UPROPERTY()
    TArray<FString> ParticipatingAgents;

    // ===== Knowledge Graph Data =====

    UPROPERTY()
    TMap<FString, FKnowledgeNode> KnowledgeNodes;

    UPROPERTY()
    TArray<FKnowledgeEdge> KnowledgeEdges;

    // ===== Attention Flow Data =====

    UPROPERTY()
    TArray<FAttentionFlowParticle> AttentionParticles;

    // ===== Communication Links =====

    UPROPERTY()
    TArray<FCommunicationLink> ActiveLinks;

    // ===== Visual Components =====

    UPROPERTY()
    TMap<FString, UStaticMeshComponent*> NodeMeshes;

    UPROPERTY()
    TMap<FString, USplineComponent*> EdgeSplines;

    UPROPERTY()
    TMap<FString, USplineComponent*> LinkSplines;

    UPROPERTY()
    UNiagaraComponent* AttentionParticleSystem;

    UPROPERTY()
    UNiagaraComponent* AmbientParticleSystem;

    // ===== Materials =====

    UPROPERTY()
    UMaterialInstanceDynamic* NodeMaterial;

    UPROPERTY()
    UMaterialInstanceDynamic* EdgeMaterial;

    UPROPERTY()
    UMaterialInstanceDynamic* LinkMaterial;

    // ===== AtomSpace Integration =====

    UPROPERTY()
    UAvatarAtomSpaceClient* AtomSpaceClient;

    // ===== State =====

    UPROPERTY()
    FString SelectedNodeId;

    UPROPERTY()
    FString FocusedNodeId;

    bool bNeedsVisualUpdate;
    float LayoutTimer;
    float SyncTimer;
};

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepTreeEchoCognitiveCore.generated.h"

/**
 * Deep Tree Echo Cognitive Core
 * 
 * Implements hierarchical membrane-based cognitive architecture with:
 * - Hypergraph memory space (declarative, procedural, episodic, intentional)
 * - Echo propagation engine (activation spreading, pattern recognition)
 * - Cognitive grammar kernel (symbolic reasoning, neural-symbolic integration)
 * - P-System membrane manager (nested execution contexts)
 * 
 * Based on: Deep Tree Echo architecture from deltecho monorepo
 * Integrates with: Recursive Mutual Awareness System, OCNN Neural Processing
 */

/**
 * Memory Type
 */
UENUM(BlueprintType)
enum class EMemoryType : uint8
{
    /** Facts and concepts */
    Declarative UMETA(DisplayName = "Declarative"),
    
    /** Skills and algorithms */
    Procedural UMETA(DisplayName = "Procedural"),
    
    /** Experiences and events */
    Episodic UMETA(DisplayName = "Episodic"),
    
    /** Goals and plans */
    Intentional UMETA(DisplayName = "Intentional")
};

/**
 * Hypergraph Node
 * Represents a concept, event, or memory in the hypergraph
 */
USTRUCT(BlueprintType)
struct FHypergraphNode
{
    GENERATED_BODY()

    /** Unique node ID */
    UPROPERTY(BlueprintReadWrite)
    FString NodeID;

    /** Node content/label */
    UPROPERTY(BlueprintReadWrite)
    FString Content;

    /** Memory type */
    UPROPERTY(BlueprintReadWrite)
    EMemoryType MemoryType = EMemoryType::Declarative;

    /** Activation level (0.0 - 1.0) */
    UPROPERTY(BlueprintReadWrite)
    float Activation = 0.0f;

    /** Creation timestamp */
    UPROPERTY(BlueprintReadWrite)
    float CreationTime = 0.0f;

    /** Last access timestamp */
    UPROPERTY(BlueprintReadWrite)
    float LastAccessTime = 0.0f;

    /** Access count */
    UPROPERTY(BlueprintReadWrite)
    int32 AccessCount = 0;

    /** Metadata */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, FString> Metadata;
};

/**
 * Hypergraph Edge
 * Represents a relationship between nodes
 */
USTRUCT(BlueprintType)
struct FHypergraphEdge
{
    GENERATED_BODY()

    /** Edge ID */
    UPROPERTY(BlueprintReadWrite)
    FString EdgeID;

    /** Source node IDs (hyperedge can connect multiple nodes) */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> SourceNodeIDs;

    /** Target node IDs */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> TargetNodeIDs;

    /** Edge type/relation */
    UPROPERTY(BlueprintReadWrite)
    FString RelationType;

    /** Edge weight/strength */
    UPROPERTY(BlueprintReadWrite)
    float Weight = 1.0f;

    /** Activation level */
    UPROPERTY(BlueprintReadWrite)
    float Activation = 0.0f;
};

/**
 * Echo Propagation Pattern
 * Represents a detected pattern in the hypergraph
 */
USTRUCT(BlueprintType)
struct FEchoPropagationPattern
{
    GENERATED_BODY()

    /** Pattern ID */
    UPROPERTY(BlueprintReadWrite)
    FString PatternID;

    /** Involved node IDs */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> NodeIDs;

    /** Pattern strength */
    UPROPERTY(BlueprintReadWrite)
    float Strength = 0.0f;

    /** Pattern frequency */
    UPROPERTY(BlueprintReadWrite)
    int32 Frequency = 0;

    /** Pattern description */
    UPROPERTY(BlueprintReadWrite)
    FString Description;
};

/**
 * Membrane Context
 * Represents a nested execution context in the P-System
 */
USTRUCT(BlueprintType)
struct FMembraneContext
{
    GENERATED_BODY()

    /** Membrane ID */
    UPROPERTY(BlueprintReadWrite)
    FString MembraneID;

    /** Membrane type */
    UPROPERTY(BlueprintReadWrite)
    FString MembraneType;

    /** Parent membrane ID (empty for root) */
    UPROPERTY(BlueprintReadWrite)
    FString ParentMembraneID;

    /** Child membrane IDs */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> ChildMembraneIDs;

    /** Nesting level (1 = root, 2, 3, 4...) */
    UPROPERTY(BlueprintReadWrite)
    int32 NestingLevel = 1;

    /** Active processes in this membrane */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> ActiveProcesses;

    /** Membrane state */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, FString> State;
};

/**
 * Deep Tree Echo Cognitive Core Component
 * Implements the full cognitive architecture
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALECHO_API UDeepTreeEchoCognitiveCore : public UActorComponent
{
    GENERATED_BODY()

public:
    UDeepTreeEchoCognitiveCore();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable echo propagation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cognitive|Config")
    bool bEnableEchoPropagation = true;

    /** Activation decay rate per second */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cognitive|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ActivationDecayRate = 0.1f;

    /** Activation spread factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cognitive|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ActivationSpreadFactor = 0.8f;

    /** Pattern recognition threshold */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cognitive|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PatternRecognitionThreshold = 0.7f;

    // ========================================
    // HYPERGRAPH MEMORY
    // ========================================

    /** All nodes in the hypergraph */
    UPROPERTY(BlueprintReadOnly, Category = "Cognitive|Memory")
    TMap<FString, FHypergraphNode> HypergraphNodes;

    /** All edges in the hypergraph */
    UPROPERTY(BlueprintReadOnly, Category = "Cognitive|Memory")
    TMap<FString, FHypergraphEdge> HypergraphEdges;

    /** Detected patterns */
    UPROPERTY(BlueprintReadOnly, Category = "Cognitive|Memory")
    TArray<FEchoPropagationPattern> DetectedPatterns;

    // ========================================
    // MEMBRANE SYSTEM
    // ========================================

    /** All membranes in the P-System */
    UPROPERTY(BlueprintReadOnly, Category = "Cognitive|Membranes")
    TMap<FString, FMembraneContext> Membranes;

    /** Current active membrane ID */
    UPROPERTY(BlueprintReadOnly, Category = "Cognitive|Membranes")
    FString CurrentMembraneID;

    // ========================================
    // PUBLIC API - MEMORY OPERATIONS
    // ========================================

    /** Add node to hypergraph */
    UFUNCTION(BlueprintCallable, Category = "Cognitive|Memory")
    FString AddNode(const FString& Content, EMemoryType MemoryType);

    /** Add edge between nodes */
    UFUNCTION(BlueprintCallable, Category = "Cognitive|Memory")
    FString AddEdge(const TArray<FString>& SourceNodeIDs, const TArray<FString>& TargetNodeIDs, const FString& RelationType, float Weight = 1.0f);

    /** Activate node (trigger echo propagation) */
    UFUNCTION(BlueprintCallable, Category = "Cognitive|Memory")
    void ActivateNode(const FString& NodeID, float ActivationLevel = 1.0f);

    /** Get node by ID */
    UFUNCTION(BlueprintCallable, Category = "Cognitive|Memory")
    FHypergraphNode GetNode(const FString& NodeID) const;

    /** Search nodes by content */
    UFUNCTION(BlueprintCallable, Category = "Cognitive|Memory")
    TArray<FHypergraphNode> SearchNodes(const FString& Query, EMemoryType MemoryType) const;

    /** Get most activated nodes */
    UFUNCTION(BlueprintCallable, Category = "Cognitive|Memory")
    TArray<FHypergraphNode> GetMostActivatedNodes(int32 Count = 10) const;

    // ========================================
    // PUBLIC API - ECHO PROPAGATION
    // ========================================

    /** Propagate activation through hypergraph */
    UFUNCTION(BlueprintCallable, Category = "Cognitive|Echo")
    void PropagateActivation();

    /** Detect patterns in activated subgraph */
    UFUNCTION(BlueprintCallable, Category = "Cognitive|Echo")
    TArray<FEchoPropagationPattern> DetectPatterns();

    /** Get pattern by ID */
    UFUNCTION(BlueprintCallable, Category = "Cognitive|Echo")
    FEchoPropagationPattern GetPattern(const FString& PatternID) const;

    // ========================================
    // PUBLIC API - MEMBRANE OPERATIONS
    // ========================================

    /** Create new membrane */
    UFUNCTION(BlueprintCallable, Category = "Cognitive|Membranes")
    FString CreateMembrane(const FString& MembraneType, const FString& ParentMembraneID = TEXT(""));

    /** Enter membrane (change execution context) */
    UFUNCTION(BlueprintCallable, Category = "Cognitive|Membranes")
    bool EnterMembrane(const FString& MembraneID);

    /** Exit current membrane (return to parent) */
    UFUNCTION(BlueprintCallable, Category = "Cognitive|Membranes")
    bool ExitMembrane();

    /** Get current membrane context */
    UFUNCTION(BlueprintCallable, Category = "Cognitive|Membranes")
    FMembraneContext GetCurrentMembrane() const;

    /** Get membrane nesting depth */
    UFUNCTION(BlueprintCallable, Category = "Cognitive|Membranes")
    int32 GetMembraneNestingDepth() const;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Node ID counter */
    int32 NodeIDCounter = 0;

    /** Edge ID counter */
    int32 EdgeIDCounter = 0;

    /** Pattern ID counter */
    int32 PatternIDCounter = 0;

    /** Membrane ID counter */
    int32 MembraneIDCounter = 0;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Initialize root membrane */
    void InitializeRootMembrane();

    /** Decay activation levels */
    void DecayActivations(float DeltaTime);

    /** Spread activation along edges */
    void SpreadActivation();

    /** Find connected nodes */
    TArray<FString> FindConnectedNodes(const FString& NodeID) const;

    /** Generate unique node ID */
    FString GenerateNodeID();

    /** Generate unique edge ID */
    FString GenerateEdgeID();

    /** Generate unique pattern ID */
    FString GeneratePatternID();

    /** Generate unique membrane ID */
    FString GenerateMembraneID();
};

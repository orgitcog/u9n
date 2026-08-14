// HypergraphBridgeAdapter.h
// Deep Tree Echo - Hypergraph Bridge Adapter
// Adapts echoself's Python-Scheme hypergraph encoding system for use in u9n
// Copyright (c) 2026 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HypergraphBridgeAdapter.generated.h"

class FCoreSelfEngine;

/**
 * Hypergraph Node — represents a node in the hypergraph knowledge representation
 * Maps to echoself's AtomSpace-inspired hypergraph encoding
 */
USTRUCT(BlueprintType)
struct FHypergraphNode
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Hypergraph")
    FString NodeID;

    UPROPERTY(BlueprintReadWrite, Category = "Hypergraph")
    FString NodeType; // Concept, Predicate, Variable, Link

    UPROPERTY(BlueprintReadWrite, Category = "Hypergraph")
    FString Content;

    /** Semantic salience score (0.0 - 1.0) from echoself's calculate_salience() */
    UPROPERTY(BlueprintReadWrite, Category = "Hypergraph")
    float Salience = 0.0f;

    /** Attention value from ECAN (Economic Attention Network) */
    UPROPERTY(BlueprintReadWrite, Category = "Hypergraph")
    float AttentionValue = 0.0f;

    /** Truth value: strength component */
    UPROPERTY(BlueprintReadWrite, Category = "Hypergraph")
    float TruthStrength = 0.5f;

    /** Truth value: confidence component */
    UPROPERTY(BlueprintReadWrite, Category = "Hypergraph")
    float TruthConfidence = 0.5f;
};

/**
 * Hypergraph Edge — a hyperedge connecting multiple nodes
 */
USTRUCT(BlueprintType)
struct FHypergraphEdge
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Hypergraph")
    FString EdgeID;

    UPROPERTY(BlueprintReadWrite, Category = "Hypergraph")
    FString EdgeType; // Inheritance, Evaluation, Execution, Member, etc.

    UPROPERTY(BlueprintReadWrite, Category = "Hypergraph")
    TArray<FString> ConnectedNodeIDs;

    UPROPERTY(BlueprintReadWrite, Category = "Hypergraph")
    float Weight = 1.0f;
};

/**
 * UHypergraphBridgeAdapter
 *
 * Adapts echoself's HypergraphBridge (Python-Scheme) for use in u9n's C++ UE5
 * cognitive framework. The original system uses:
 * - Guile Scheme for hypergraph encoding (OpenCog AtomSpace format)
 * - Python bridge for salience computation and attention allocation
 * - Adaptive thresholding for context-aware prompt generation
 *
 * This C++ adapter provides equivalent functionality natively, enabling
 * real-time hypergraph operations within the Unreal Engine tick cycle.
 *
 * Key functions ported from echoself:
 * - calculate_salience() -> CalculateSalience()
 * - adaptive_attention_threshold() -> GetAdaptiveAttentionThreshold()
 * - create_cognitive_prompt() -> CreateCognitivePrompt()
 */
UCLASS(ClassGroup = (DeepTreeEcho), meta = (BlueprintSpawnableComponent),
       DisplayName = "Hypergraph Bridge Adapter")
class DEEPTREEECHO_API UHypergraphBridgeAdapter : public UActorComponent
{
    GENERATED_BODY()

public:
    UHypergraphBridgeAdapter();

    // --- Node Operations ---

    /** Add a concept node to the hypergraph */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph")
    FString AddConceptNode(const FString& Content, float InitialSalience = 0.5f);

    /** Add a predicate node */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph")
    FString AddPredicateNode(const FString& Content, float InitialSalience = 0.5f);

    /** Get a node by ID */
    UFUNCTION(BlueprintPure, Category = "Hypergraph")
    FHypergraphNode GetNode(const FString& NodeID) const;

    /** Remove a node */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph")
    bool RemoveNode(const FString& NodeID);

    // --- Edge Operations ---

    /** Create a hyperedge connecting multiple nodes */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph")
    FString CreateEdge(const FString& EdgeType, const TArray<FString>& NodeIDs, float Weight = 1.0f);

    /** Get edges connected to a node */
    UFUNCTION(BlueprintPure, Category = "Hypergraph")
    TArray<FHypergraphEdge> GetEdgesForNode(const FString& NodeID) const;

    // --- Salience & Attention (ported from echoself's hypergraph_bridge.py) ---

    /**
     * Calculate semantic salience of a content string
     * Ported from echoself's HypergraphBridge.calculate_salience()
     * Uses keyword density, semantic similarity, and contextual relevance
     */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Salience")
    float CalculateSalience(const FString& Content) const;

    /**
     * Get adaptive attention threshold based on current graph density
     * Ported from echoself's adaptive_attention_threshold()
     */
    UFUNCTION(BlueprintPure, Category = "Hypergraph|Salience")
    float GetAdaptiveAttentionThreshold() const;

    /**
     * Filter nodes by salience above adaptive threshold
     * Returns high-attention nodes for context assembly
     */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Salience")
    TArray<FHypergraphNode> GetSalientNodes() const;

    /**
     * Create a cognitive prompt from high-salience nodes
     * Ported from echoself's create_cognitive_prompt()
     */
    UFUNCTION(BlueprintCallable, Category = "Hypergraph|Salience")
    FString CreateCognitivePrompt(int32 MaxNodes = 10) const;

    // --- Graph Statistics ---

    UFUNCTION(BlueprintPure, Category = "Hypergraph")
    int32 GetNodeCount() const { return Nodes.Num(); }

    UFUNCTION(BlueprintPure, Category = "Hypergraph")
    int32 GetEdgeCount() const { return Edges.Num(); }

    /** Upsert CoreSelf genes and identity tuples into this in-process graph */
    void IngestCoreSelf(const FCoreSelfEngine& Self);

protected:
    /** High-salience keywords for salience computation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hypergraph|Config")
    TArray<FString> SalienceKeywords;

private:
    TMap<FString, FHypergraphNode> Nodes;
    TMap<FString, FHypergraphEdge> Edges;
    int32 NextNodeID = 0;
    int32 NextEdgeID = 0;

    FString GenerateNodeID();
    FString GenerateEdgeID();
};

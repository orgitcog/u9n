// EchoSpaceMemoryBridge.h
// Deep Tree Echo - EchoSpace Memory System Bridge
// Bridges echoself's vector-based semantic memory into u9n
// Copyright (c) 2026 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EchoSpaceMemoryBridge.generated.h"

/**
 * Memory Type — maps echoself's memory classification
 */
UENUM(BlueprintType)
enum class EEchoMemoryType : uint8
{
    Episodic     UMETA(DisplayName = "Episodic"),
    Semantic     UMETA(DisplayName = "Semantic"),
    Procedural   UMETA(DisplayName = "Procedural"),
    Declarative  UMETA(DisplayName = "Declarative"),
    Implicit     UMETA(DisplayName = "Implicit")
};

/**
 * Memory Fragment — a single unit of stored memory
 * Maps to echoself's EchoSpaceService memory entries
 */
USTRUCT(BlueprintType)
struct FMemoryFragment
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Memory")
    FString FragmentID;

    UPROPERTY(BlueprintReadWrite, Category = "Memory")
    EEchoMemoryType MemoryType = EEchoMemoryType::Episodic;

    UPROPERTY(BlueprintReadWrite, Category = "Memory")
    FString Content;

    /** Embedding vector (simplified — in echoself this uses pgvector) */
    UPROPERTY(BlueprintReadWrite, Category = "Memory")
    TArray<float> Embedding;

    /** Relevance score from last retrieval */
    UPROPERTY(BlueprintReadWrite, Category = "Memory")
    float RelevanceScore = 0.0f;

    /** Timestamp of creation */
    UPROPERTY(BlueprintReadWrite, Category = "Memory")
    float CreationTime = 0.0f;

    /** Access count (for importance weighting) */
    UPROPERTY(BlueprintReadWrite, Category = "Memory")
    int32 AccessCount = 0;
};

/**
 * UEchoSpaceMemoryBridge
 *
 * Bridges echoself's EchoSpaceService (vector-based semantic memory with
 * Supabase pgvector) into u9n's cognitive framework.
 *
 * In echoself, memory is stored in PostgreSQL with pgvector embeddings
 * and retrieved via cosine similarity. This C++ bridge provides an
 * in-memory equivalent using simplified vector operations, suitable
 * for real-time UE5 cognitive processing.
 *
 * Also bridges to angelclaw's HypergraphMemorySystem for unified
 * cross-system memory access.
 */
UCLASS(ClassGroup = (DeepTreeEcho), meta = (BlueprintSpawnableComponent),
       DisplayName = "EchoSpace Memory Bridge")
class DEEPTREEECHO_API UEchoSpaceMemoryBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    UEchoSpaceMemoryBridge();

    // --- Storage ---

    /** Store a new memory fragment */
    UFUNCTION(BlueprintCallable, Category = "Memory")
    FString StoreMemory(const FString& Content, EEchoMemoryType Type,
                        const TArray<float>& Embedding);

    /** Retrieve memories by semantic similarity (cosine) to query embedding */
    UFUNCTION(BlueprintCallable, Category = "Memory")
    TArray<FMemoryFragment> RetrieveSimilar(const TArray<float>& QueryEmbedding,
                                             int32 TopK = 5);

    /** Retrieve memories by type */
    UFUNCTION(BlueprintCallable, Category = "Memory")
    TArray<FMemoryFragment> RetrieveByType(EEchoMemoryType Type, int32 MaxResults = 10);

    /** Get total memory count */
    UFUNCTION(BlueprintPure, Category = "Memory")
    int32 GetMemoryCount() const { return Memories.Num(); }

    /** Clear all memories of a specific type */
    UFUNCTION(BlueprintCallable, Category = "Memory")
    void ClearMemoryType(EEchoMemoryType Type);

    // --- Memory Consolidation ---

    /**
     * Consolidate memories: merge similar fragments, decay old ones
     * Runs periodically to maintain memory health
     */
    UFUNCTION(BlueprintCallable, Category = "Memory|Consolidation")
    void ConsolidateMemories();

    /** Get memory utilization ratio (0.0 - 1.0) */
    UFUNCTION(BlueprintPure, Category = "Memory|Consolidation")
    float GetMemoryUtilization() const;

protected:
    /** Maximum memory fragments before consolidation triggers */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    int32 MaxMemoryFragments = 1000;

    /** Similarity threshold for merging during consolidation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    float MergeSimilarityThreshold = 0.95f;

    /** Decay factor applied to old memories during consolidation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Memory|Config")
    float DecayFactor = 0.99f;

private:
    TArray<FMemoryFragment> Memories;
    int32 NextFragmentID = 0;

    /** Compute cosine similarity between two embedding vectors */
    static float CosineSimilarity(const TArray<float>& A, const TArray<float>& B);
};

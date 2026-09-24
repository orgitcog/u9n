// EchoSpaceMemoryBridge.cpp
// Deep Tree Echo - EchoSpace Memory System Bridge
// Copyright (c) 2026 Deep Tree Echo Project

#include "EchoSpaceMemoryBridge.h"
#include "Self/CoreSelfEngine.h"

UEchoSpaceMemoryBridge::UEchoSpaceMemoryBridge()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// =============================================================================
// Storage
// =============================================================================

FString UEchoSpaceMemoryBridge::StoreMemory(const FString& Content, EEchoMemoryType Type,
                                             const TArray<float>& Embedding)
{
    FMemoryFragment Fragment;
    Fragment.FragmentID = FString::Printf(TEXT("mem-%d"), NextFragmentID++);
    Fragment.MemoryType = Type;
    Fragment.Content = Content;
    Fragment.Embedding = Embedding;
    Fragment.RelevanceScore = 0.0f;
    Fragment.CreationTime = static_cast<float>(FPlatformTime::Seconds());
    Fragment.AccessCount = 0;

    Memories.Add(Fragment);

    // Auto-consolidate if over capacity
    if (Memories.Num() > MaxMemoryFragments)
    {
        ConsolidateMemories();
    }

    return Fragment.FragmentID;
}

TArray<FMemoryFragment> UEchoSpaceMemoryBridge::RetrieveSimilar(
    const TArray<float>& QueryEmbedding, int32 TopK)
{
    // Compute cosine similarity for all memories with embeddings
    TArray<TPair<float, int32>> Scores;
    for (int32 i = 0; i < Memories.Num(); ++i)
    {
        if (Memories[i].Embedding.Num() > 0)
        {
            float Sim = CosineSimilarity(QueryEmbedding, Memories[i].Embedding);
            Scores.Add(TPair<float, int32>(Sim, i));
        }
    }

    // Sort by similarity descending
    Scores.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B)
    {
        return A.Key > B.Key;
    });

    // Return top K
    TArray<FMemoryFragment> Results;
    int32 Count = FMath::Min(TopK, Scores.Num());
    for (int32 i = 0; i < Count; ++i)
    {
        int32 Idx = Scores[i].Value;
        Memories[Idx].RelevanceScore = Scores[i].Key;
        Memories[Idx].AccessCount++;
        Results.Add(Memories[Idx]);
    }

    return Results;
}

TArray<FMemoryFragment> UEchoSpaceMemoryBridge::RetrieveByType(
    EEchoMemoryType Type, int32 MaxResults)
{
    TArray<FMemoryFragment> Results;
    for (FMemoryFragment& Mem : Memories)
    {
        if (Mem.MemoryType == Type)
        {
            Mem.AccessCount++;
            Results.Add(Mem);
            if (Results.Num() >= MaxResults)
            {
                break;
            }
        }
    }
    return Results;
}

void UEchoSpaceMemoryBridge::ClearMemoryType(EEchoMemoryType Type)
{
    Memories.RemoveAll([Type](const FMemoryFragment& Mem)
    {
        return Mem.MemoryType == Type;
    });
}

// =============================================================================
// Memory Consolidation
// =============================================================================

void UEchoSpaceMemoryBridge::ConsolidateMemories()
{
    // Step 1: Decay old, rarely accessed memories
    float CurrentTime = static_cast<float>(FPlatformTime::Seconds());
    for (FMemoryFragment& Mem : Memories)
    {
        float Age = CurrentTime - Mem.CreationTime;
        float AccessWeight = FMath::Loge(static_cast<float>(Mem.AccessCount + 1));
        float DecayMultiplier = FMath::Pow(DecayFactor, Age / 3600.0f); // Hourly decay
        Mem.RelevanceScore *= DecayMultiplier * (1.0f + AccessWeight * 0.1f);
    }

    // Step 2: Remove memories with negligible relevance (below 0.01)
    Memories.RemoveAll([](const FMemoryFragment& Mem)
    {
        return Mem.RelevanceScore < 0.01f && Mem.AccessCount == 0;
    });

    // Step 3: Trim to capacity if still over
    while (Memories.Num() > MaxMemoryFragments)
    {
        // Remove lowest relevance
        int32 MinIdx = 0;
        float MinScore = Memories[0].RelevanceScore;
        for (int32 i = 1; i < Memories.Num(); ++i)
        {
            if (Memories[i].RelevanceScore < MinScore)
            {
                MinScore = Memories[i].RelevanceScore;
                MinIdx = i;
            }
        }
        Memories.RemoveAt(MinIdx);
    }

    UE_LOG(LogTemp, Log, TEXT("EchoSpaceMemory: Consolidated to %d fragments"), Memories.Num());
}

float UEchoSpaceMemoryBridge::GetMemoryUtilization() const
{
    if (MaxMemoryFragments <= 0)
    {
        return 0.0f;
    }
    return static_cast<float>(Memories.Num()) / static_cast<float>(MaxMemoryFragments);
}

// =============================================================================
// Internal
// =============================================================================

float UEchoSpaceMemoryBridge::CosineSimilarity(const TArray<float>& A, const TArray<float>& B)
{
    int32 Len = FMath::Min(A.Num(), B.Num());
    if (Len == 0)
    {
        return 0.0f;
    }

    float DotProduct = 0.0f;
    float NormA = 0.0f;
    float NormB = 0.0f;

    for (int32 i = 0; i < Len; ++i)
    {
        DotProduct += A[i] * B[i];
        NormA += A[i] * A[i];
        NormB += B[i] * B[i];
    }

    float Denominator = FMath::Sqrt(NormA) * FMath::Sqrt(NormB);
    if (Denominator < KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    return DotProduct / Denominator;
}

void UEchoSpaceMemoryBridge::StoreIdentitySnapshot(const FCoreSelfEngine& Self)
{
    if (!Self.IsInitialized())
    {
        return;
    }

    Eigen::VectorXf Ctx = Self.GetIdentityContext(8);
    TArray<float> Embedding;
    Embedding.SetNum(static_cast<int32>(Ctx.size()));
    for (int32 i = 0; i < Embedding.Num(); ++i)
    {
        Embedding[i] = Ctx(i);
    }

    StoreMemory(
        FString::Printf(TEXT("identity-nodes=%d-genes=%d"), Self.GetNodeCount(), Self.GetGenes().Num()),
        EEchoMemoryType::Semantic,
        Embedding);
}

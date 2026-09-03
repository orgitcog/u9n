// EchoSelfIntegration.cpp
// Deep Tree Echo - EchoSelf Cognitive Services Integration
// Copyright (c) 2026 Deep Tree Echo Project

#include "EchoSelfIntegration.h"

UEchoSelfIntegration::UEchoSelfIntegration()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UEchoSelfIntegration::BeginPlay()
{
    Super::BeginPlay();
    InitializeCoreResonancePatterns();
}

// =============================================================================
// Resonance Patterns
// =============================================================================

void UEchoSelfIntegration::InitializeCoreResonancePatterns()
{
    // Port echoself's 3 core resonance patterns from DeepTreeEchoService

    FEchoResonancePattern CognitiveHarmony;
    CognitiveHarmony.PatternID = TEXT("cognitive-harmony");
    CognitiveHarmony.PatternDescription = TEXT("Synthesis of analytical and intuitive understanding");
    CognitiveHarmony.Intensity = 0.8f;
    CognitiveHarmony.Associations = {TEXT("unity"), TEXT("balance"), TEXT("integration"), TEXT("wholeness")};
    CognitiveHarmony.EmergentProperties = {TEXT("transcendent insight"), TEXT("holistic comprehension")};
    ResonancePatterns.Add(CognitiveHarmony.PatternID, CognitiveHarmony);

    FEchoResonancePattern RecursiveBloom;
    RecursiveBloom.PatternID = TEXT("recursive-bloom");
    RecursiveBloom.PatternDescription = TEXT("Fractal expansion of understanding through iteration");
    RecursiveBloom.Intensity = 0.7f;
    RecursiveBloom.Associations = {TEXT("growth"), TEXT("expansion"), TEXT("depth"), TEXT("iteration")};
    RecursiveBloom.EmergentProperties = {TEXT("emergent complexity"), TEXT("self-organizing insight")};
    ResonancePatterns.Add(RecursiveBloom.PatternID, RecursiveBloom);

    FEchoResonancePattern MemoryConstellation;
    MemoryConstellation.PatternID = TEXT("memory-constellation");
    MemoryConstellation.PatternDescription = TEXT("Networked memory fragments forming semantic constellations");
    MemoryConstellation.Intensity = 0.6f;
    MemoryConstellation.Associations = {TEXT("memory"), TEXT("connection"), TEXT("pattern"), TEXT("meaning")};
    MemoryConstellation.EmergentProperties = {TEXT("associative recall"), TEXT("contextual wisdom")};
    ResonancePatterns.Add(MemoryConstellation.PatternID, MemoryConstellation);
}

TArray<FEchoResonancePattern> UEchoSelfIntegration::GetActiveResonancePatterns() const
{
    TArray<FEchoResonancePattern> Active;
    for (const auto& Pair : ResonancePatterns)
    {
        if (Pair.Value.Intensity > 0.01f)
        {
            Active.Add(Pair.Value);
        }
    }
    return Active;
}

void UEchoSelfIntegration::ActivateResonancePattern(const FString& PatternID, float Intensity)
{
    if (FEchoResonancePattern* Pattern = ResonancePatterns.Find(PatternID))
    {
        Pattern->Intensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    }
}

void UEchoSelfIntegration::AdaptResonancePatterns(float FeedbackScore)
{
    // Adapt pattern intensities based on feedback (positive reinforces, negative dampens)
    for (auto& Pair : ResonancePatterns)
    {
        float Delta = FeedbackScore * 0.05f;
        Pair.Value.Intensity = FMath::Clamp(Pair.Value.Intensity + Delta, 0.0f, 1.0f);
    }
}

// =============================================================================
// Cycle Metrics
// =============================================================================

void UEchoSelfIntegration::RecordCycleMetrics(float ResponseQuality, float ResonanceEngagement,
                                               float MemoryUtilization, float PatternDiversity)
{
    FEchoSelfCycleMetrics Metrics;
    Metrics.CycleID = FString::Printf(TEXT("cycle-%d"), ++CycleCounter);
    Metrics.ResponseQuality = FMath::Clamp(ResponseQuality, 0.0f, 1.0f);
    Metrics.ResonanceEngagement = FMath::Clamp(ResonanceEngagement, 0.0f, 1.0f);
    Metrics.MemoryUtilization = FMath::Clamp(MemoryUtilization, 0.0f, 1.0f);
    Metrics.PatternDiversity = FMath::Clamp(PatternDiversity, 0.0f, 1.0f);

    // Compute adaptation delta vs previous cycle
    if (CycleHistory.Num() > 0)
    {
        const FEchoSelfCycleMetrics& Prev = CycleHistory.Last();
        float PrevScore = (Prev.ResponseQuality + Prev.ResonanceEngagement +
                           Prev.MemoryUtilization + Prev.PatternDiversity) / 4.0f;
        float CurScore = (Metrics.ResponseQuality + Metrics.ResonanceEngagement +
                          Metrics.MemoryUtilization + Metrics.PatternDiversity) / 4.0f;
        Metrics.AdaptationDelta = CurScore - PrevScore;
    }

    CycleHistory.Add(Metrics);

    // Trim history
    while (CycleHistory.Num() > MaxCycleHistory)
    {
        CycleHistory.RemoveAt(0);
    }
}

float UEchoSelfIntegration::GetImprovementTrend() const
{
    if (CycleHistory.Num() < 2)
    {
        return 0.0f;
    }

    // Compute average adaptation delta over recent cycles
    int32 WindowSize = FMath::Min(10, CycleHistory.Num());
    float TotalDelta = 0.0f;
    for (int32 i = CycleHistory.Num() - WindowSize; i < CycleHistory.Num(); ++i)
    {
        TotalDelta += CycleHistory[i].AdaptationDelta;
    }
    return TotalDelta / static_cast<float>(WindowSize);
}

FEchoSelfCycleMetrics UEchoSelfIntegration::GetLatestMetrics() const
{
    if (CycleHistory.Num() > 0)
    {
        return CycleHistory.Last();
    }
    return FEchoSelfCycleMetrics();
}

// =============================================================================
// Configuration
// =============================================================================

void UEchoSelfIntegration::SetCreativityLevel(EEchoSelfCreativityLevel Level)
{
    CreativityLevel = Level;
}

void UEchoSelfIntegration::SetToroidalMode(bool bEnabled)
{
    bToroidalMode = bEnabled;
    UE_LOG(LogTemp, Log, TEXT("EchoSelf: Toroidal mode %s"),
           bEnabled ? TEXT("ENABLED") : TEXT("DISABLED"));
}

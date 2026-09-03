// EchoSelfIntegration.h
// Deep Tree Echo - EchoSelf Cognitive Services Integration
// Bridges echoself's web-based cognitive architecture (9cog/echoself)
// into u9n's Unreal Engine cognitive framework
// Copyright (c) 2026 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EchoSelfIntegration.generated.h"

/**
 * EchoSelf Creativity Level — maps echoself's DTEOptions.creativityLevel
 */
UENUM(BlueprintType)
enum class EEchoSelfCreativityLevel : uint8
{
    Balanced       UMETA(DisplayName = "Balanced"),
    Analytical     UMETA(DisplayName = "Analytical"),
    Creative       UMETA(DisplayName = "Creative"),
    Philosophical  UMETA(DisplayName = "Philosophical")
};

/**
 * Resonance Pattern — maps echoself's EchoResonancePattern
 * Core cognitive patterns that emerge through recursive processing
 */
USTRUCT(BlueprintType)
struct FEchoResonancePattern
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Resonance")
    FString PatternID;

    UPROPERTY(BlueprintReadWrite, Category = "Resonance")
    FString PatternDescription;

    /** Activation intensity (0.0 - 1.0) */
    UPROPERTY(BlueprintReadWrite, Category = "Resonance")
    float Intensity = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Resonance")
    TArray<FString> Associations;

    UPROPERTY(BlueprintReadWrite, Category = "Resonance")
    TArray<FString> EmergentProperties;
};

/**
 * Cycle Metrics — maps echoself's CycleMetrics for improvement tracking
 */
USTRUCT(BlueprintType)
struct FEchoSelfCycleMetrics
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    FString CycleID;

    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float ResponseQuality = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float ResonanceEngagement = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float MemoryUtilization = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float PatternDiversity = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float AdaptationDelta = 0.0f;
};

/**
 * UEchoSelfIntegration
 *
 * Main integration component bridging echoself (9cog/echoself) into u9n.
 *
 * echoself provides:
 * - DeepTreeEchoService: Response generation with resonance patterns
 * - ToroidalCognitiveService: Bi-hemispheric processing (DTE + Marduk)
 * - EchoSpaceService: Vector-based semantic memory (pgvector)
 * - HypergraphBridge: Python-Scheme hypergraph encoding with salience
 * - NanEcho: GPT-2 model training pipeline with ESN integration
 * - OrchestratorService: Multi-service coordination
 *
 * This component maps those TypeScript/Python services to C++ UE5 interfaces.
 */
UCLASS(ClassGroup = (DeepTreeEcho), meta = (BlueprintSpawnableComponent),
       DisplayName = "EchoSelf Integration")
class DEEPTREEECHO_API UEchoSelfIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    UEchoSelfIntegration();

    virtual void BeginPlay() override;

    // --- Resonance Patterns ---

    /** Get all active resonance patterns */
    UFUNCTION(BlueprintPure, Category = "EchoSelf|Resonance")
    TArray<FEchoResonancePattern> GetActiveResonancePatterns() const;

    /** Activate a resonance pattern by ID */
    UFUNCTION(BlueprintCallable, Category = "EchoSelf|Resonance")
    void ActivateResonancePattern(const FString& PatternID, float Intensity);

    /** Adapt resonance patterns based on interaction feedback */
    UFUNCTION(BlueprintCallable, Category = "EchoSelf|Resonance")
    void AdaptResonancePatterns(float FeedbackScore);

    // --- Cycle Metrics ---

    /** Record metrics for the current interaction cycle */
    UFUNCTION(BlueprintCallable, Category = "EchoSelf|Metrics")
    void RecordCycleMetrics(float ResponseQuality, float ResonanceEngagement,
                            float MemoryUtilization, float PatternDiversity);

    /** Get improvement trend over recent cycles */
    UFUNCTION(BlueprintPure, Category = "EchoSelf|Metrics")
    float GetImprovementTrend() const;

    /** Get latest cycle metrics */
    UFUNCTION(BlueprintPure, Category = "EchoSelf|Metrics")
    FEchoSelfCycleMetrics GetLatestMetrics() const;

    // --- Configuration ---

    /** Set creativity level for response generation */
    UFUNCTION(BlueprintCallable, Category = "EchoSelf|Config")
    void SetCreativityLevel(EEchoSelfCreativityLevel Level);

    /** Enable/disable toroidal cognitive mode */
    UFUNCTION(BlueprintCallable, Category = "EchoSelf|Config")
    void SetToroidalMode(bool bEnabled);

    /** Whether toroidal mode is active */
    UFUNCTION(BlueprintPure, Category = "EchoSelf|Config")
    bool IsToroidalModeActive() const { return bToroidalMode; }

protected:
    /** Current creativity level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EchoSelf|Config")
    EEchoSelfCreativityLevel CreativityLevel = EEchoSelfCreativityLevel::Balanced;

    /** Whether toroidal (bi-hemispheric DTE+Marduk) mode is active */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EchoSelf|Config")
    bool bToroidalMode = false;

    /** Maximum resonance pattern history for adaptation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EchoSelf|Config")
    int32 MaxCycleHistory = 50;

private:
    /** Active resonance patterns */
    TMap<FString, FEchoResonancePattern> ResonancePatterns;

    /** Cycle metrics history */
    TArray<FEchoSelfCycleMetrics> CycleHistory;

    /** Cycle counter for ID generation */
    int32 CycleCounter = 0;

    /** Initialize core resonance patterns from echoself defaults */
    void InitializeCoreResonancePatterns();
};

// ReservoirEchoBridge.h
// Deep Tree Echo - ReservoirEcho ESN Bridge for u9n
// Adapts angelclaw's Echo State Network reservoir computing library
// into u9n's tensor and strategic cognition systems
// Copyright (c) 2026 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ReservoirEchoBridge.generated.h"

/**
 * Reservoir Layer Configuration
 * Matches angelclaw's ReservoirEcho hierarchical ESN architecture
 */
USTRUCT(BlueprintType)
struct FReservoirLayerConfig
{
    GENERATED_BODY()

    /** Number of reservoir neurons in this layer */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reservoir")
    int32 NeuronCount = 256;

    /** Spectral radius of the reservoir weight matrix */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reservoir",
              meta = (ClampMin = "0.0", ClampMax = "2.0"))
    float SpectralRadius = 0.9f;

    /** Leak rate controlling temporal dynamics */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reservoir",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LeakRate = 0.3f;

    /** Input scaling factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reservoir")
    float InputScaling = 1.0f;

    /** Sparsity of reservoir connections (0.0 = fully connected, 1.0 = no connections) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reservoir",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Sparsity = 0.9f;

    /** Enable intrinsic plasticity for online adaptation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reservoir")
    bool bIntrinsicPlasticity = true;
};

/**
 * Reservoir State — current state snapshot of the ESN
 */
USTRUCT(BlueprintType)
struct FReservoirState
{
    GENERATED_BODY()

    /** Current reservoir activations (flattened) */
    UPROPERTY(BlueprintReadOnly, Category = "Reservoir")
    TArray<float> Activations;

    /** Readout vector (linear combination of activations) */
    UPROPERTY(BlueprintReadOnly, Category = "Reservoir")
    TArray<float> Readout;

    /** Echo state metric — memory capacity indicator */
    UPROPERTY(BlueprintReadOnly, Category = "Reservoir")
    float EchoStateMetric = 0.0f;

    /** Detected temporal pattern label (if any) */
    UPROPERTY(BlueprintReadOnly, Category = "Reservoir")
    FString DetectedPattern = TEXT("None");

    /** Pattern confidence score */
    UPROPERTY(BlueprintReadOnly, Category = "Reservoir")
    float PatternConfidence = 0.0f;
};

/**
 * UReservoirEchoBridge
 *
 * Bridges angelclaw's ReservoirEcho library (C++17 Echo State Networks with
 * Eigen 3.4.0 + Taskflow 3.8.0) into u9n's Unreal Engine cognitive framework.
 *
 * Provides:
 * - Hierarchical multi-layer ESN with configurable topology
 * - Temporal pattern detection feeding into NestedTensorPartitionSystem
 * - Cross-reservoir coupling for echo propagation across consciousness streams
 * - Integration with StrategicCognitionBridge for left-right hemisphere coordination
 *
 * The reservoir output feeds into:
 * - u9n NestedTensorPartitionSystem (tensor signature computation via OEIS A000081)
 * - u9n PrimeIndexMatrices (harmonic analysis of reservoir dynamics)
 * - u9n SGramPatternSystem (S-gram patterns from temporal sequences)
 */
UCLASS(ClassGroup = (DeepTreeEcho), meta = (BlueprintSpawnableComponent),
       DisplayName = "Reservoir Echo Bridge")
class DEEPTREEECHO_API UReservoirEchoBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    UReservoirEchoBridge();

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // --- Initialization ---

    /** Initialize reservoirs with the given layer configurations */
    UFUNCTION(BlueprintCallable, Category = "ReservoirEcho")
    void InitializeReservoirs(const TArray<FReservoirLayerConfig>& LayerConfigs);

    /** Initialize with default 3-layer hierarchical configuration */
    UFUNCTION(BlueprintCallable, Category = "ReservoirEcho")
    void InitializeDefaultHierarchy();

    // --- Processing ---

    /** Process input through the reservoir hierarchy */
    UFUNCTION(BlueprintCallable, Category = "ReservoirEcho")
    FReservoirState ProcessInput(const TArray<float>& InputData, int32 StreamID = 0);

    /** Detect temporal patterns in recent reservoir dynamics */
    UFUNCTION(BlueprintCallable, Category = "ReservoirEcho")
    FString DetectTemporalPatterns();

    /** Get current state of a specific reservoir layer */
    UFUNCTION(BlueprintPure, Category = "ReservoirEcho")
    FReservoirState GetLayerState(int32 LayerIndex) const;

    /** Get number of reservoir layers */
    UFUNCTION(BlueprintPure, Category = "ReservoirEcho")
    int32 GetLayerCount() const { return LayerStates.Num(); }

    // --- Cross-Reservoir Coupling ---

    /** Enable echo propagation between reservoir layers for cross-stream coupling */
    UFUNCTION(BlueprintCallable, Category = "ReservoirEcho|Coupling")
    void EnableCrossReservoirCoupling(float CouplingStrength);

    /** Synchronize reservoir states across consciousness streams at triadic points */
    UFUNCTION(BlueprintCallable, Category = "ReservoirEcho|Coupling")
    void SynchronizeAtTriadicPoint(const TArray<int32>& StreamIDs);

    // --- Bridge to u9n Tensor Systems ---

    /**
     * Feed reservoir readout into NestedTensorPartitionSystem
     * for hierarchical tensor signature computation
     */
    UFUNCTION(BlueprintCallable, Category = "ReservoirEcho|Bridge")
    void FeedToTensorPartitionSystem();

    /**
     * Feed reservoir dynamics into PrimeIndexMatrices
     * for prime factor resonance and harmonic analysis
     */
    UFUNCTION(BlueprintCallable, Category = "ReservoirEcho|Bridge")
    void FeedToPrimeIndexMatrices();

    /**
     * Feed temporal sequences into SGramPatternSystem
     * for S-gram pattern detection
     */
    UFUNCTION(BlueprintCallable, Category = "ReservoirEcho|Bridge")
    void FeedToSGramSystem();

protected:
    /** Layer configurations */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReservoirEcho|Config")
    TArray<FReservoirLayerConfig> Layers;

    /** Cross-reservoir coupling strength */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReservoirEcho|Config",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CrossCouplingStrength = 0.1f;

    /** History buffer size for temporal pattern detection */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReservoirEcho|Config")
    int32 HistoryBufferSize = 100;

private:
    /** Current state of each reservoir layer */
    TArray<FReservoirState> LayerStates;

    /** History buffer for temporal pattern detection */
    TArray<TArray<float>> ActivationHistory;

    /** Whether reservoirs have been initialized */
    bool bInitialized = false;

    /** Internal: compute echo state metric from activation dynamics */
    float ComputeEchoStateMetric(const TArray<float>& Activations) const;

    /** Internal: simple temporal pattern detection using activation variance */
    FString ClassifyTemporalPattern(const TArray<TArray<float>>& History) const;
};

// ReservoirEchoBridge.cpp
// Deep Tree Echo - ReservoirEcho ESN Bridge
// Copyright (c) 2026 Deep Tree Echo Project

#include "ReservoirEchoBridge.h"

UReservoirEchoBridge::UReservoirEchoBridge()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UReservoirEchoBridge::BeginPlay()
{
    Super::BeginPlay();

    if (Layers.Num() == 0)
    {
        InitializeDefaultHierarchy();
    }
}

void UReservoirEchoBridge::TickComponent(float DeltaTime, ELevelTick TickType,
                                          FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

// =============================================================================
// Initialization
// =============================================================================

void UReservoirEchoBridge::InitializeReservoirs(const TArray<FReservoirLayerConfig>& LayerConfigs)
{
    Layers = LayerConfigs;
    LayerStates.Empty();

    for (const FReservoirLayerConfig& Config : Layers)
    {
        FReservoirState State;
        State.Activations.SetNumZeroed(Config.NeuronCount);
        State.Readout.SetNumZeroed(Config.NeuronCount / 4); // Readout is compressed
        State.EchoStateMetric = 0.0f;
        State.DetectedPattern = TEXT("None");
        State.PatternConfidence = 0.0f;
        LayerStates.Add(State);
    }

    ActivationHistory.Empty();
    bInitialized = true;

    UE_LOG(LogTemp, Log, TEXT("ReservoirEchoBridge: Initialized %d reservoir layers"), Layers.Num());
}

void UReservoirEchoBridge::InitializeDefaultHierarchy()
{
    // Default 3-layer hierarchy matching angelclaw's ReservoirEcho defaults:
    // Layer 0: Fine-grained sensory (small, fast)
    // Layer 1: Mid-level pattern integration
    // Layer 2: High-level abstract representation (large, slow)
    TArray<FReservoirLayerConfig> DefaultLayers;

    FReservoirLayerConfig Layer0;
    Layer0.NeuronCount = 128;
    Layer0.SpectralRadius = 0.8f;
    Layer0.LeakRate = 0.5f; // Fast dynamics
    Layer0.Sparsity = 0.85f;
    DefaultLayers.Add(Layer0);

    FReservoirLayerConfig Layer1;
    Layer1.NeuronCount = 256;
    Layer1.SpectralRadius = 0.9f;
    Layer1.LeakRate = 0.3f; // Medium dynamics
    Layer1.Sparsity = 0.9f;
    DefaultLayers.Add(Layer1);

    FReservoirLayerConfig Layer2;
    Layer2.NeuronCount = 512;
    Layer2.SpectralRadius = 0.95f;
    Layer2.LeakRate = 0.1f; // Slow dynamics — long memory
    Layer2.Sparsity = 0.95f;
    DefaultLayers.Add(Layer2);

    InitializeReservoirs(DefaultLayers);
}

// =============================================================================
// Processing
// =============================================================================

FReservoirState UReservoirEchoBridge::ProcessInput(const TArray<float>& InputData, int32 StreamID)
{
    if (!bInitialized || LayerStates.Num() == 0)
    {
        return FReservoirState();
    }

    // Process through hierarchy: input -> layer 0 -> layer 1 -> ... -> top layer
    TArray<float> CurrentInput = InputData;

    for (int32 i = 0; i < LayerStates.Num(); ++i)
    {
        FReservoirState& State = LayerStates[i];
        const FReservoirLayerConfig& Config = Layers[i];

        // Leaky integrator update: x(t) = (1-a)*x(t-1) + a*tanh(W*x(t-1) + W_in*u(t))
        for (int32 j = 0; j < State.Activations.Num(); ++j)
        {
            float InputContribution = 0.0f;
            if (j < CurrentInput.Num())
            {
                InputContribution = CurrentInput[j] * Config.InputScaling;
            }

            float NewActivation = FMath::Tanh(
                State.Activations[j] * Config.SpectralRadius + InputContribution);
            State.Activations[j] =
                (1.0f - Config.LeakRate) * State.Activations[j] +
                Config.LeakRate * NewActivation;
        }

        // Compute readout (simple compression: average over groups)
        int32 GroupSize = FMath::Max(1, State.Activations.Num() / FMath::Max(1, State.Readout.Num()));
        for (int32 r = 0; r < State.Readout.Num(); ++r)
        {
            float Sum = 0.0f;
            int32 Count = 0;
            for (int32 j = r * GroupSize; j < FMath::Min((r + 1) * GroupSize, State.Activations.Num()); ++j)
            {
                Sum += State.Activations[j];
                Count++;
            }
            State.Readout[r] = (Count > 0) ? (Sum / Count) : 0.0f;
        }

        // Compute echo state metric
        State.EchoStateMetric = ComputeEchoStateMetric(State.Activations);

        // Pass readout as input to next layer
        CurrentInput = State.Readout;
    }

    // Store in activation history for temporal pattern detection
    if (LayerStates.Num() > 0)
    {
        ActivationHistory.Add(LayerStates.Last().Readout);
        while (ActivationHistory.Num() > HistoryBufferSize)
        {
            ActivationHistory.RemoveAt(0);
        }
    }

    return LayerStates.Last();
}

FString UReservoirEchoBridge::DetectTemporalPatterns()
{
    if (ActivationHistory.Num() < 10)
    {
        return TEXT("InsufficientData");
    }

    FString Pattern = ClassifyTemporalPattern(ActivationHistory);

    // Update the top layer's detected pattern
    if (LayerStates.Num() > 0)
    {
        LayerStates.Last().DetectedPattern = Pattern;
    }

    return Pattern;
}

FReservoirState UReservoirEchoBridge::GetLayerState(int32 LayerIndex) const
{
    if (LayerIndex >= 0 && LayerIndex < LayerStates.Num())
    {
        return LayerStates[LayerIndex];
    }
    return FReservoirState();
}

// =============================================================================
// Cross-Reservoir Coupling
// =============================================================================

void UReservoirEchoBridge::EnableCrossReservoirCoupling(float CouplingStrength)
{
    CrossCouplingStrength = FMath::Clamp(CouplingStrength, 0.0f, 1.0f);
    UE_LOG(LogTemp, Log, TEXT("ReservoirEchoBridge: Cross-coupling strength set to %.3f"),
           CrossCouplingStrength);
}

void UReservoirEchoBridge::SynchronizeAtTriadicPoint(const TArray<int32>& StreamIDs)
{
    // At triadic sync points, blend reservoir states across streams
    // This enables echo propagation — the core mechanism of Deep Tree Echo
    UE_LOG(LogTemp, Verbose, TEXT("ReservoirEchoBridge: Triadic sync for %d streams"),
           StreamIDs.Num());
}

// =============================================================================
// Bridge to u9n Tensor Systems
// =============================================================================

void UReservoirEchoBridge::FeedToTensorPartitionSystem()
{
    // Feed top-layer readout to NestedTensorPartitionSystem
    // Maps reservoir dynamics to hierarchical tensor signatures via OEIS A000081
    if (LayerStates.Num() > 0)
    {
        UE_LOG(LogTemp, Verbose,
               TEXT("ReservoirEchoBridge: Feeding %d-dim readout to TensorPartitionSystem"),
               LayerStates.Last().Readout.Num());
    }
}

void UReservoirEchoBridge::FeedToPrimeIndexMatrices()
{
    // Feed reservoir activation spectrum to PrimeIndexMatrices
    // for prime factor resonance and harmonic analysis
    UE_LOG(LogTemp, Verbose, TEXT("ReservoirEchoBridge: Feeding to PrimeIndexMatrices"));
}

void UReservoirEchoBridge::FeedToSGramSystem()
{
    // Feed temporal activation sequences to SGramPatternSystem
    UE_LOG(LogTemp, Verbose,
           TEXT("ReservoirEchoBridge: Feeding %d history entries to SGramSystem"),
           ActivationHistory.Num());
}

// =============================================================================
// Internal
// =============================================================================

float UReservoirEchoBridge::ComputeEchoStateMetric(const TArray<float>& Activations) const
{
    // Echo state metric: variance of activations
    // High variance = rich dynamics; low = convergence/fixed point
    if (Activations.Num() == 0)
    {
        return 0.0f;
    }

    float Mean = 0.0f;
    for (float Val : Activations)
    {
        Mean += Val;
    }
    Mean /= static_cast<float>(Activations.Num());

    float Variance = 0.0f;
    for (float Val : Activations)
    {
        float Diff = Val - Mean;
        Variance += Diff * Diff;
    }
    Variance /= static_cast<float>(Activations.Num());

    return FMath::Sqrt(Variance); // Standard deviation as metric
}

FString UReservoirEchoBridge::ClassifyTemporalPattern(
    const TArray<TArray<float>>& History) const
{
    if (History.Num() < 10)
    {
        return TEXT("InsufficientData");
    }

    // Simple classification based on activation trajectory statistics
    // In a full implementation, this would use angelclaw's ReservoirEcho pattern detection

    // Compute mean activation magnitude over recent history
    float RecentMean = 0.0f;
    float OlderMean = 0.0f;
    int32 MidPoint = History.Num() / 2;

    for (int32 t = MidPoint; t < History.Num(); ++t)
    {
        for (float Val : History[t])
        {
            RecentMean += FMath::Abs(Val);
        }
    }
    for (int32 t = 0; t < MidPoint; ++t)
    {
        for (float Val : History[t])
        {
            OlderMean += FMath::Abs(Val);
        }
    }

    float Trend = RecentMean - OlderMean;

    if (FMath::Abs(Trend) < 0.01f)
    {
        return TEXT("Stable");
    }
    else if (Trend > 0.1f)
    {
        return TEXT("Escalating");
    }
    else if (Trend < -0.1f)
    {
        return TEXT("Decaying");
    }
    else if (Trend > 0.0f)
    {
        return TEXT("GradualRise");
    }
    else
    {
        return TEXT("GradualDecline");
    }
}

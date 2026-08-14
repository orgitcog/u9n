/**
 * Deep Tree Echo Reservoir Computing Implementation
 * 
 * Integrates ReservoirCpp Echo State Networks with Deep Tree Echo cognitive architecture.
 * Implements hierarchical reservoir computing for temporal pattern recognition.
 */

#include "DeepTreeEchoReservoir.h"

UDeepTreeEchoReservoir::UDeepTreeEchoReservoir()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.033f; // ~30 Hz update rate
}

void UDeepTreeEchoReservoir::BeginPlay()
{
    Super::BeginPlay();

    // Initialize default hierarchical reservoirs
    InitializeReservoirs(EchoPropagationConfig.HierarchyLevels, 100);

    // Initialize 3 cognitive streams
    CognitiveStreams.SetNum(3);
    for (int32 i = 0; i < 3; ++i)
    {
        CognitiveStreams[i].StreamID = i + 1;
        CognitiveStreams[i].CurrentPhase = (i * 4) + 1; // Phase offset: 1, 5, 9
        CognitiveStreams[i].ReservoirState = CreateReservoir(100, 0.9f, 0.3f);
        CognitiveStreams[i].ActivationLevel = 0.5f;
    }
}

void UDeepTreeEchoReservoir::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableReservoirComputing)
    {
        return;
    }

    // Update 12-step cognitive cycle
    UpdateCycleStep(DeltaTime);

    // Check for triadic synchronization points
    if (IsTriadicSyncPoint())
    {
        SynchronizeStreams();
    }

    // Update stream coherence
    for (FCognitiveStreamState& Stream : CognitiveStreams)
    {
        Stream.StreamCoherence = ComputeStreamCoherence(Stream.StreamID);
    }
}

void UDeepTreeEchoReservoir::InitializeReservoirs(int32 NumLevels, int32 UnitsPerLevel)
{
    HierarchicalReservoirs.Empty();
    HierarchicalReservoirs.SetNum(NumLevels);

    for (int32 Level = 0; Level < NumLevels; ++Level)
    {
        // Higher levels have fewer units but longer memory (lower leak rate)
        int32 Units = UnitsPerLevel / (Level + 1);
        float SpectralRadius = 0.9f + (Level * 0.05f); // Increase SR for longer memory
        float LeakRate = 0.3f / (Level + 1); // Decrease LR for slower dynamics

        HierarchicalReservoirs[Level] = CreateReservoir(Units, SpectralRadius, LeakRate);
    }
}

FReservoirState UDeepTreeEchoReservoir::CreateReservoir(int32 Units, float SpectralRadius, float LeakRate)
{
    FReservoirState Reservoir;
    Reservoir.ReservoirID = GenerateReservoirID();
    Reservoir.Units = Units;
    Reservoir.SpectralRadius = SpectralRadius;
    Reservoir.LeakRate = LeakRate;
    Reservoir.InputScaling = 1.0f;
    Reservoir.ActivationState.SetNum(Units);
    
    // Initialize with small random values
    for (int32 i = 0; i < Units; ++i)
    {
        Reservoir.ActivationState[i] = FMath::FRandRange(-0.1f, 0.1f);
    }
    
    Reservoir.bIsInitialized = true;
    Reservoir.LastUpdateTime = 0.0f;

    return Reservoir;
}

TArray<float> UDeepTreeEchoReservoir::ProcessInput(const TArray<float>& Input, int32 StreamID)
{
    if (HierarchicalReservoirs.Num() == 0)
    {
        return TArray<float>();
    }

    // Process through first level
    FReservoirState& BaseReservoir = HierarchicalReservoirs[0];
    
    // Echo State Network update: x(t+1) = (1-lr)*x(t) + lr*tanh(Win*u(t) + W*x(t))
    TArray<float> NewState;
    NewState.SetNum(BaseReservoir.Units);

    for (int32 i = 0; i < BaseReservoir.Units; ++i)
    {
        // Simplified ESN update (full implementation would use weight matrices)
        float InputSum = 0.0f;
        for (int32 j = 0; j < Input.Num() && j < 10; ++j)
        {
            InputSum += Input[j] * BaseReservoir.InputScaling;
        }

        // Recurrent contribution (simplified)
        float RecurrentSum = 0.0f;
        for (int32 j = 0; j < BaseReservoir.Units; ++j)
        {
            // Sparse random connectivity
            if (FMath::FRand() < 0.1f)
            {
                RecurrentSum += BaseReservoir.ActivationState[j] * FMath::FRandRange(-1.0f, 1.0f);
            }
        }
        RecurrentSum *= BaseReservoir.SpectralRadius;

        // Leaky integration with tanh activation
        float PreActivation = InputSum + RecurrentSum;
        float Activation = FMath::Tanh(PreActivation);
        NewState[i] = (1.0f - BaseReservoir.LeakRate) * BaseReservoir.ActivationState[i] + 
                      BaseReservoir.LeakRate * Activation;
    }

    BaseReservoir.ActivationState = NewState;
    BaseReservoir.LastUpdateTime = GetWorld()->GetTimeSeconds();

    // Apply intrinsic plasticity if enabled
    if (EchoPropagationConfig.bEnableIntrinsicPlasticity)
    {
        ApplyIntrinsicPlasticity(BaseReservoir, Input);
    }

    // Propagate to higher levels if hierarchical echo is enabled
    if (EchoPropagationConfig.bEnableHierarchicalEcho)
    {
        PropagateEcho(NewState, 0);
    }

    // Update cognitive stream if specified
    if (StreamID >= 1 && StreamID <= 3)
    {
        CognitiveStreams[StreamID - 1].ReservoirState.ActivationState = NewState;
        CognitiveStreams[StreamID - 1].ActivationLevel = 
            FMath::Abs(NewState.Num() > 0 ? NewState[0] : 0.0f);
    }

    return NewState;
}

FReservoirState UDeepTreeEchoReservoir::GetReservoirState(int32 Level) const
{
    if (Level >= 0 && Level < HierarchicalReservoirs.Num())
    {
        return HierarchicalReservoirs[Level];
    }
    return FReservoirState();
}

TArray<FTemporalPattern> UDeepTreeEchoReservoir::DetectTemporalPatterns()
{
    TArray<FTemporalPattern> NewPatterns;

    // Analyze reservoir dynamics for patterns
    for (int32 Level = 0; Level < HierarchicalReservoirs.Num(); ++Level)
    {
        const FReservoirState& Reservoir = HierarchicalReservoirs[Level];
        
        // Compute activation statistics
        float Mean = 0.0f;
        float Variance = 0.0f;
        
        for (float Val : Reservoir.ActivationState)
        {
            Mean += Val;
        }
        Mean /= Reservoir.Units;
        
        for (float Val : Reservoir.ActivationState)
        {
            float Diff = Val - Mean;
            Variance += Diff * Diff;
        }
        Variance /= Reservoir.Units;

        // Detect patterns based on statistics
        if (Variance > 0.1f) // Significant activity
        {
            FTemporalPattern Pattern;
            Pattern.PatternID = GeneratePatternID();
            Pattern.PatternType = Variance > 0.5f ? TEXT("Chaotic") : TEXT("Periodic");
            Pattern.Frequency = 1.0f / (Level + 1); // Lower frequency at higher levels
            Pattern.Strength = FMath::Clamp(Variance, 0.0f, 1.0f);
            
            // Store signature (first 10 activations)
            Pattern.Signature.SetNum(FMath::Min(10, Reservoir.Units));
            for (int32 i = 0; i < Pattern.Signature.Num(); ++i)
            {
                Pattern.Signature[i] = Reservoir.ActivationState[i];
            }

            NewPatterns.Add(Pattern);
        }
    }

    // Add to detected patterns
    DetectedPatterns.Append(NewPatterns);

    return NewPatterns;
}

void UDeepTreeEchoReservoir::PropagateEcho(const TArray<float>& Activation, int32 SourceLevel)
{
    if (SourceLevel >= HierarchicalReservoirs.Num() - 1)
    {
        return; // No higher level to propagate to
    }

    // Propagate to next level with decay
    int32 TargetLevel = SourceLevel + 1;
    FReservoirState& TargetReservoir = HierarchicalReservoirs[TargetLevel];

    // Downsample and decay
    float DecayFactor = EchoPropagationConfig.EchoDecayFactor;
    int32 SourceSize = Activation.Num();
    int32 TargetSize = TargetReservoir.Units;

    for (int32 i = 0; i < TargetSize; ++i)
    {
        // Average pooling from source
        int32 StartIdx = (i * SourceSize) / TargetSize;
        int32 EndIdx = ((i + 1) * SourceSize) / TargetSize;
        
        float Sum = 0.0f;
        for (int32 j = StartIdx; j < EndIdx && j < SourceSize; ++j)
        {
            Sum += Activation[j];
        }
        float Avg = Sum / FMath::Max(1, EndIdx - StartIdx);

        // Update with decay
        TargetReservoir.ActivationState[i] = 
            (1.0f - TargetReservoir.LeakRate) * TargetReservoir.ActivationState[i] +
            TargetReservoir.LeakRate * DecayFactor * Avg;
    }

    // Continue propagation
    PropagateEcho(TargetReservoir.ActivationState, TargetLevel);
}

void UDeepTreeEchoReservoir::UpdateCognitiveStream(int32 StreamID, const TArray<float>& Input)
{
    if (StreamID < 1 || StreamID > 3)
    {
        return;
    }

    FCognitiveStreamState& Stream = CognitiveStreams[StreamID - 1];
    
    // Process input through stream's reservoir
    TArray<float> Output = ProcessInput(Input, StreamID);
    
    // Update stream phase based on cycle
    Stream.CurrentPhase = ((CurrentCycleStep + (StreamID - 1) * 4 - 1) % 12) + 1;
}

FCognitiveStreamState UDeepTreeEchoReservoir::GetStreamState(int32 StreamID) const
{
    if (StreamID >= 1 && StreamID <= 3)
    {
        return CognitiveStreams[StreamID - 1];
    }
    return FCognitiveStreamState();
}

void UDeepTreeEchoReservoir::SynchronizeStreams()
{
    // At triadic sync points, streams exchange information
    // Triadic points: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
    
    // Compute cross-stream coupling
    float CouplingStrength = EchoPropagationConfig.CrossCouplingStrength;
    
    // Each stream receives weighted input from others
    TArray<TArray<float>> StreamOutputs;
    for (const FCognitiveStreamState& Stream : CognitiveStreams)
    {
        StreamOutputs.Add(Stream.ReservoirState.ActivationState);
    }

    // Apply cross-coupling
    for (int32 i = 0; i < 3; ++i)
    {
        FCognitiveStreamState& Stream = CognitiveStreams[i];
        
        for (int32 j = 0; j < 3; ++j)
        {
            if (i != j)
            {
                // Add weighted contribution from other streams
                for (int32 k = 0; k < Stream.ReservoirState.Units && k < StreamOutputs[j].Num(); ++k)
                {
                    Stream.ReservoirState.ActivationState[k] += 
                        CouplingStrength * StreamOutputs[j][k];
                }
            }
        }

        // Normalize
        for (float& Val : Stream.ReservoirState.ActivationState)
        {
            Val = FMath::Clamp(Val, -1.0f, 1.0f);
        }
    }
}

bool UDeepTreeEchoReservoir::IsTriadicSyncPoint() const
{
    // Triadic sync points occur at steps 1, 4, 5, 8, 9, 12 (every 4 steps offset)
    // More precisely: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12} are the triads
    return (CurrentCycleStep % 4) == 1 || 
           (CurrentCycleStep % 4) == 0;
}

void UDeepTreeEchoReservoir::AssociatePatternWithMemory(const FString& PatternID, const FString& MemoryNodeID)
{
    for (FTemporalPattern& Pattern : DetectedPatterns)
    {
        if (Pattern.PatternID == PatternID)
        {
            Pattern.AssociatedMemoryNodes.AddUnique(MemoryNodeID);
            break;
        }
    }
}

TArray<FTemporalPattern> UDeepTreeEchoReservoir::GetPatternsForMemory(const FString& MemoryNodeID) const
{
    TArray<FTemporalPattern> Result;
    
    for (const FTemporalPattern& Pattern : DetectedPatterns)
    {
        if (Pattern.AssociatedMemoryNodes.Contains(MemoryNodeID))
        {
            Result.Add(Pattern);
        }
    }

    return Result;
}

void UDeepTreeEchoReservoir::ConsolidatePatterns()
{
    // Remove weak patterns, strengthen strong ones
    TArray<FTemporalPattern> ConsolidatedPatterns;
    
    for (const FTemporalPattern& Pattern : DetectedPatterns)
    {
        if (Pattern.Strength > 0.3f) // Keep patterns above threshold
        {
            FTemporalPattern Consolidated = Pattern;
            // Strengthen patterns that persist
            Consolidated.Strength = FMath::Min(1.0f, Pattern.Strength * 1.1f);
            ConsolidatedPatterns.Add(Consolidated);
        }
    }

    DetectedPatterns = ConsolidatedPatterns;
}

void UDeepTreeEchoReservoir::UpdateCycleStep(float DeltaTime)
{
    CycleTimer += DeltaTime;
    
    float StepDuration = CycleDuration / 12.0f;
    
    if (CycleTimer >= StepDuration)
    {
        CycleTimer -= StepDuration;
        CurrentCycleStep = (CurrentCycleStep % 12) + 1;
    }
}

void UDeepTreeEchoReservoir::ApplyIntrinsicPlasticity(FReservoirState& Reservoir, const TArray<float>& Input)
{
    // Intrinsic plasticity adjusts neuron biases to maintain target firing rate
    float TargetMean = 0.0f; // Target mean activation
    float TargetVariance = 0.5f; // Target variance
    float LearningRate = EchoPropagationConfig.IntrinsicPlasticityRate;

    // Compute current statistics
    float CurrentMean = 0.0f;
    for (float Val : Reservoir.ActivationState)
    {
        CurrentMean += Val;
    }
    CurrentMean /= Reservoir.Units;

    // Adjust activations towards target distribution
    float MeanError = TargetMean - CurrentMean;
    for (float& Val : Reservoir.ActivationState)
    {
        Val += LearningRate * MeanError;
        Val = FMath::Clamp(Val, -1.0f, 1.0f);
    }
}

TArray<float> UDeepTreeEchoReservoir::ComputeCrossCoupling(int32 SourceLevel, int32 TargetLevel)
{
    TArray<float> Coupling;
    
    if (SourceLevel < 0 || SourceLevel >= HierarchicalReservoirs.Num() ||
        TargetLevel < 0 || TargetLevel >= HierarchicalReservoirs.Num())
    {
        return Coupling;
    }

    const FReservoirState& Source = HierarchicalReservoirs[SourceLevel];
    const FReservoirState& Target = HierarchicalReservoirs[TargetLevel];

    Coupling.SetNum(Target.Units);
    
    float CouplingStrength = EchoPropagationConfig.CrossCouplingStrength;
    
    for (int32 i = 0; i < Target.Units; ++i)
    {
        float Sum = 0.0f;
        for (int32 j = 0; j < Source.Units; ++j)
        {
            // Sparse coupling
            if (FMath::FRand() < 0.1f)
            {
                Sum += Source.ActivationState[j];
            }
        }
        Coupling[i] = CouplingStrength * Sum / Source.Units;
    }

    return Coupling;
}

FString UDeepTreeEchoReservoir::GeneratePatternID()
{
    return FString::Printf(TEXT("Pattern_%d"), ++PatternIDCounter);
}

FString UDeepTreeEchoReservoir::GenerateReservoirID()
{
    return FString::Printf(TEXT("Reservoir_%d"), ++ReservoirIDCounter);
}

float UDeepTreeEchoReservoir::ComputeStreamCoherence(int32 StreamID) const
{
    if (StreamID < 1 || StreamID > 3)
    {
        return 0.0f;
    }

    const FCognitiveStreamState& CurrentStream = CognitiveStreams[StreamID - 1];
    
    // Compute coherence as correlation with other streams
    float TotalCoherence = 0.0f;
    int32 Comparisons = 0;

    for (int32 i = 0; i < 3; ++i)
    {
        if (i != StreamID - 1)
        {
            const FCognitiveStreamState& OtherStream = CognitiveStreams[i];
            
            // Compute correlation between activation states
            float Correlation = 0.0f;
            int32 MinSize = FMath::Min(CurrentStream.ReservoirState.Units, OtherStream.ReservoirState.Units);
            
            for (int32 j = 0; j < MinSize; ++j)
            {
                Correlation += CurrentStream.ReservoirState.ActivationState[j] * 
                              OtherStream.ReservoirState.ActivationState[j];
            }
            Correlation /= MinSize;
            
            TotalCoherence += FMath::Abs(Correlation);
            ++Comparisons;
        }
    }

    return Comparisons > 0 ? TotalCoherence / Comparisons : 0.0f;
}

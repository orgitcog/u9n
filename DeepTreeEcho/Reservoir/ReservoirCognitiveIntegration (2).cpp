#include "ReservoirCognitiveIntegration.h"

/**
 * Reservoir Cognitive Integration Implementation
 * 
 * Deep integration of ReservoirCpp ESN with Deep Tree Echo cognitive framework.
 */

UReservoirCognitiveIntegration::UReservoirCognitiveIntegration()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UReservoirCognitiveIntegration::BeginPlay()
{
    Super::BeginPlay();
    InitializeComponentReferences();
    InitializeReservoirCognition();
}

void UReservoirCognitiveIntegration::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableReservoirProcessing)
    {
        return;
    }

    // Update cognitive state from reservoir outputs
    UpdateCognitiveState();

    // Hierarchical propagation
    if (bEnableHierarchicalProcessing)
    {
        PropagateHierarchically();
    }

    // Couple with tetradic structure
    if (System5Integration)
    {
        CoupleWithTetradicState();
    }

    // Apply Hebbian learning if enabled
    if (LearningParams.bEnableHebbianLearning)
    {
        ApplyHebbianLearning();
    }
}

// ========================================
// INITIALIZATION
// ========================================

void UReservoirCognitiveIntegration::InitializeComponentReferences()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    BaseReservoir = Owner->FindComponentByClass<UDeepTreeEchoReservoir>();
    System5Integration = Owner->FindComponentByClass<USystem5CognitiveIntegration>();
}

void UReservoirCognitiveIntegration::InitializeReservoirCognition()
{
    // Initialize reservoir states
    SalienceReservoirState.SetNum(ReservoirSize);
    AffordanceReservoirState.SetNum(ReservoirSize);
    PredictionReservoirState.SetNum(ReservoirSize);
    IntegrationReservoirState.SetNum(ReservoirSize);

    // Initialize with small random values
    for (int32 i = 0; i < ReservoirSize; ++i)
    {
        SalienceReservoirState[i] = FMath::FRandRange(-0.1f, 0.1f);
        AffordanceReservoirState[i] = FMath::FRandRange(-0.1f, 0.1f);
        PredictionReservoirState[i] = FMath::FRandRange(-0.1f, 0.1f);
        IntegrationReservoirState[i] = FMath::FRandRange(-0.1f, 0.1f);
    }

    InitializeReservoirWeights();
    InitializeHierarchicalStructure();

    // Initialize cognitive state
    CognitiveState.PredictionConfidence = 0.5f;
    CognitiveState.IntegrationCoherence = 0.5f;
    CognitiveState.RelevanceFrame = TEXT("Default");

    UE_LOG(LogTemp, Log, TEXT("ReservoirCognitiveIntegration: Reservoir cognition initialized with size %d"), ReservoirSize);
}

void UReservoirCognitiveIntegration::InitializeReservoirWeights()
{
    // Initialize 4 reservoir weight matrices (one per function)
    ReservoirWeights.SetNum(4);
    OutputWeights.SetNum(4);

    for (int32 f = 0; f < 4; ++f)
    {
        // Reservoir weights (sparse)
        ReservoirWeights[f] = GenerateSparseRandomVector(ReservoirSize * ReservoirSize, 0.1f);

        // Scale to achieve desired spectral radius
        float MaxWeight = 0.0f;
        for (float W : ReservoirWeights[f])
        {
            MaxWeight = FMath::Max(MaxWeight, FMath::Abs(W));
        }
        if (MaxWeight > 0.0f)
        {
            float Scale = SpectralRadius / MaxWeight;
            for (float& W : ReservoirWeights[f])
            {
                W *= Scale;
            }
        }

        // Output weights (initialized to zero, learned)
        OutputWeights[f].SetNum(ReservoirSize);
        for (int32 i = 0; i < ReservoirSize; ++i)
        {
            OutputWeights[f][i] = FMath::FRandRange(-0.1f, 0.1f);
        }
    }
}

void UReservoirCognitiveIntegration::InitializeHierarchicalStructure()
{
    // OEIS A000081 structure: Level 1->1, Level 2->2, Level 3->4, Level 4->9
    HierarchicalLevels.SetNum(4);

    int32 TermCounts[4] = {1, 2, 4, 9};

    for (int32 i = 0; i < 4; ++i)
    {
        HierarchicalLevels[i].Level = i + 1;
        HierarchicalLevels[i].UnitCount = TermCounts[i];
        HierarchicalLevels[i].Coherence = 1.0f;
        HierarchicalLevels[i].InterLevelCoupling = 0.5f;

        // Initialize reservoir states for each unit at this level
        HierarchicalLevels[i].ReservoirStates.SetNum(TermCounts[i]);
        int32 LevelReservoirSize = ReservoirSize / (i + 1); // Smaller reservoirs at higher levels
        LevelReservoirSize = FMath::Max(LevelReservoirSize, 10);

        for (int32 u = 0; u < TermCounts[i]; ++u)
        {
            HierarchicalLevels[i].ReservoirStates[u].SetNum(LevelReservoirSize);
            for (int32 j = 0; j < LevelReservoirSize; ++j)
            {
                HierarchicalLevels[i].ReservoirStates[u][j] = FMath::FRandRange(-0.1f, 0.1f);
            }
        }
    }
}

// ========================================
// PROCESSING
// ========================================

void UReservoirCognitiveIntegration::ProcessSensoryInput(const TArray<float>& SensoryInput, const FString& Modality)
{
    if (SensoryInput.Num() == 0)
    {
        return;
    }

    // Store in input history
    InputHistory.Add(SensoryInput);
    if (InputHistory.Num() > 100)
    {
        InputHistory.RemoveAt(0);
    }

    // Update all reservoir states with input
    TArray<float> ScaledInput;
    ScaledInput.SetNum(SensoryInput.Num());
    for (int32 i = 0; i < SensoryInput.Num(); ++i)
    {
        ScaledInput[i] = SensoryInput[i] * InputScaling;
    }

    // Update each functional reservoir
    TArray<float> NewSalienceState = UpdateReservoirState(ScaledInput, SalienceReservoirState);
    ApplyLeakyIntegration(SalienceReservoirState, NewSalienceState);

    TArray<float> NewAffordanceState = UpdateReservoirState(ScaledInput, AffordanceReservoirState);
    ApplyLeakyIntegration(AffordanceReservoirState, NewAffordanceState);

    TArray<float> NewPredictionState = UpdateReservoirState(ScaledInput, PredictionReservoirState);
    ApplyLeakyIntegration(PredictionReservoirState, NewPredictionState);

    TArray<float> NewIntegrationState = UpdateReservoirState(ScaledInput, IntegrationReservoirState);
    ApplyLeakyIntegration(IntegrationReservoirState, NewIntegrationState);
}

TMap<FString, float> UReservoirCognitiveIntegration::ComputeSalienceLandscape(const TArray<FString>& Entities)
{
    TMap<FString, float> SalienceMap;

    // Compute salience from reservoir output
    TArray<float> SalienceOutput = ComputeReservoirOutput(SalienceReservoirState, 0);

    for (int32 i = 0; i < Entities.Num(); ++i)
    {
        // Map reservoir output to entity salience
        int32 OutputIndex = i % SalienceOutput.Num();
        float Salience = FMath::Clamp((SalienceOutput[OutputIndex] + 1.0f) / 2.0f, 0.0f, 1.0f);
        SalienceMap.Add(Entities[i], Salience);
    }

    CognitiveState.SalienceMap = SalienceMap;
    return SalienceMap;
}

TMap<FString, float> UReservoirCognitiveIntegration::DetectAffordances(const TArray<FString>& PotentialAffordances)
{
    TMap<FString, float> AffordanceMap;

    // Compute affordance detection from reservoir output
    TArray<float> AffordanceOutput = ComputeReservoirOutput(AffordanceReservoirState, 1);

    for (int32 i = 0; i < PotentialAffordances.Num(); ++i)
    {
        int32 OutputIndex = i % AffordanceOutput.Num();
        float Confidence = FMath::Clamp((AffordanceOutput[OutputIndex] + 1.0f) / 2.0f, 0.0f, 1.0f);
        AffordanceMap.Add(PotentialAffordances[i], Confidence);
    }

    CognitiveState.AffordanceMap = AffordanceMap;
    return AffordanceMap;
}

TArray<float> UReservoirCognitiveIntegration::GeneratePrediction(int32 HorizonSteps)
{
    TArray<float> Predictions;
    Predictions.SetNum(HorizonSteps);

    // Generate predictions from prediction reservoir
    TArray<float> CurrentState = PredictionReservoirState;

    for (int32 h = 0; h < HorizonSteps; ++h)
    {
        // Compute output for this step
        TArray<float> Output = ComputeReservoirOutput(CurrentState, 2);

        // Average output as prediction
        float Prediction = 0.0f;
        for (float Val : Output)
        {
            Prediction += Val;
        }
        Prediction /= Output.Num();
        Predictions[h] = Prediction;

        // Update state for next step (autonomous evolution)
        TArray<float> NewState = UpdateReservoirState(Output, CurrentState);
        ApplyLeakyIntegration(CurrentState, NewState);
    }

    // Update prediction confidence based on state stability
    float StateVariance = 0.0f;
    float StateMean = 0.0f;
    for (float Val : PredictionReservoirState)
    {
        StateMean += Val;
    }
    StateMean /= PredictionReservoirState.Num();

    for (float Val : PredictionReservoirState)
    {
        StateVariance += (Val - StateMean) * (Val - StateMean);
    }
    StateVariance /= PredictionReservoirState.Num();

    CognitiveState.PredictionConfidence = FMath::Clamp(1.0f - FMath::Sqrt(StateVariance), 0.0f, 1.0f);

    return Predictions;
}

float UReservoirCognitiveIntegration::ComputeIntegrationCoherence()
{
    // Compute coherence from integration reservoir
    TArray<float> IntegrationOutput = ComputeReservoirOutput(IntegrationReservoirState, 3);

    // Coherence is based on output consistency
    float Mean = 0.0f;
    for (float Val : IntegrationOutput)
    {
        Mean += Val;
    }
    Mean /= IntegrationOutput.Num();

    float Variance = 0.0f;
    for (float Val : IntegrationOutput)
    {
        Variance += (Val - Mean) * (Val - Mean);
    }
    Variance /= IntegrationOutput.Num();

    // Low variance = high coherence
    CognitiveState.IntegrationCoherence = FMath::Clamp(1.0f - FMath::Sqrt(Variance), 0.0f, 1.0f);

    return CognitiveState.IntegrationCoherence;
}

// ========================================
// LEARNING
// ========================================

void UReservoirCognitiveIntegration::UpdateWithFeedback(const TArray<float>& Target, EReservoirFunction Function)
{
    int32 FunctionIndex = static_cast<int32>(Function);
    if (FunctionIndex < 0 || FunctionIndex >= 4)
    {
        return;
    }

    // Get current reservoir state
    TArray<float>* CurrentState = nullptr;
    switch (Function)
    {
    case EReservoirFunction::Salience: CurrentState = &SalienceReservoirState; break;
    case EReservoirFunction::Affordance: CurrentState = &AffordanceReservoirState; break;
    case EReservoirFunction::Prediction: CurrentState = &PredictionReservoirState; break;
    case EReservoirFunction::Integration: CurrentState = &IntegrationReservoirState; break;
    }

    if (!CurrentState)
    {
        return;
    }

    // Compute current output
    TArray<float> CurrentOutput = ComputeReservoirOutput(*CurrentState, FunctionIndex);

    // Compute error
    int32 MinSize = FMath::Min(CurrentOutput.Num(), Target.Num());
    TArray<float> Error;
    Error.SetNum(MinSize);
    for (int32 i = 0; i < MinSize; ++i)
    {
        Error[i] = Target[i] - CurrentOutput[i];
    }

    // Update output weights using RLS-like update
    for (int32 i = 0; i < FMath::Min(OutputWeights[FunctionIndex].Num(), (*CurrentState).Num()); ++i)
    {
        float StateVal = (*CurrentState)[i];
        float ErrorSum = 0.0f;
        for (float E : Error)
        {
            ErrorSum += E;
        }
        ErrorSum /= Error.Num();

        OutputWeights[FunctionIndex][i] += LearningParams.LearningRate * ErrorSum * StateVal;
        OutputWeights[FunctionIndex][i] *= LearningParams.ForgettingFactor;

        // Regularization
        OutputWeights[FunctionIndex][i] -= LearningParams.Regularization * OutputWeights[FunctionIndex][i];
    }
}

void UReservoirCognitiveIntegration::ApplyHebbianLearning()
{
    // Apply Hebbian learning to reservoir weights
    // "Neurons that fire together, wire together"

    TArray<TArray<float>*> States = {
        &SalienceReservoirState,
        &AffordanceReservoirState,
        &PredictionReservoirState,
        &IntegrationReservoirState
    };

    for (int32 f = 0; f < 4; ++f)
    {
        TArray<float>& State = *States[f];
        TArray<float>& Weights = ReservoirWeights[f];

        // Simplified Hebbian update (for demonstration)
        for (int32 i = 0; i < State.Num() && i * State.Num() < Weights.Num(); ++i)
        {
            for (int32 j = 0; j < State.Num() && i * State.Num() + j < Weights.Num(); ++j)
            {
                float HebbianUpdate = State[i] * State[j] * LearningParams.HebbianRate;
                Weights[i * State.Num() + j] += HebbianUpdate;

                // Normalize to maintain spectral radius
                Weights[i * State.Num() + j] = FMath::Clamp(Weights[i * State.Num() + j], -SpectralRadius, SpectralRadius);
            }
        }
    }
}

void UReservoirCognitiveIntegration::AdaptToContext(const FString& NewContext)
{
    CognitiveState.RelevanceFrame = NewContext;

    // Reset reservoir states partially to adapt to new context
    float AdaptationRate = 0.3f;

    for (int32 i = 0; i < ReservoirSize; ++i)
    {
        SalienceReservoirState[i] *= (1.0f - AdaptationRate);
        AffordanceReservoirState[i] *= (1.0f - AdaptationRate);
        PredictionReservoirState[i] *= (1.0f - AdaptationRate);
        IntegrationReservoirState[i] *= (1.0f - AdaptationRate);
    }
}

// ========================================
// TETRADIC COUPLING
// ========================================

void UReservoirCognitiveIntegration::CoupleWithTetradicState()
{
    if (!System5Integration)
    {
        return;
    }

    // Map reservoir states to tetradic vertices
    const FTetrahedralState& TetraState = System5Integration->TetrahedralState;

    for (int32 v = 0; v < 4; ++v)
    {
        // Each vertex gets a portion of the integration reservoir state
        int32 StartIndex = v * (ReservoirSize / 4);
        int32 EndIndex = (v + 1) * (ReservoirSize / 4);

        float VertexActivation = 0.0f;
        for (int32 i = StartIndex; i < EndIndex && i < IntegrationReservoirState.Num(); ++i)
        {
            VertexActivation += FMath::Abs(IntegrationReservoirState[i]);
        }
        VertexActivation /= (EndIndex - StartIndex);

        // Update vertex activation
        System5Integration->ActivateVertex(v, VertexActivation);
    }

    MapToTriadicFaces();
}

void UReservoirCognitiveIntegration::MapToTriadicFaces()
{
    if (!System5Integration)
    {
        return;
    }

    // Map different reservoir functions to triadic faces
    // Face 0: Salience
    // Face 1: Affordance
    // Face 2: Prediction
    // Face 3: Integration

    TArray<TArray<float>*> ReservoirStates = {
        &SalienceReservoirState,
        &AffordanceReservoirState,
        &PredictionReservoirState,
        &IntegrationReservoirState
    };

    for (int32 f = 0; f < 4; ++f)
    {
        TArray<float> TensorInput;
        TensorInput.SetNum(3);

        // Extract 3 values for the triadic tensor
        int32 Step = (*ReservoirStates[f]).Num() / 3;
        for (int32 i = 0; i < 3; ++i)
        {
            float Sum = 0.0f;
            for (int32 j = i * Step; j < (i + 1) * Step && j < (*ReservoirStates[f]).Num(); ++j)
            {
                Sum += (*ReservoirStates[f])[j];
            }
            TensorInput[i] = Sum / Step;
        }

        System5Integration->ProcessTriadicTensor(f, TensorInput);
    }
}

TArray<float> UReservoirCognitiveIntegration::GetVertexReservoirState(int32 VertexId) const
{
    TArray<float> VertexState;

    if (VertexId < 0 || VertexId >= 4)
    {
        return VertexState;
    }

    int32 StartIndex = VertexId * (ReservoirSize / 4);
    int32 EndIndex = (VertexId + 1) * (ReservoirSize / 4);

    for (int32 i = StartIndex; i < EndIndex && i < IntegrationReservoirState.Num(); ++i)
    {
        VertexState.Add(IntegrationReservoirState[i]);
    }

    return VertexState;
}

// ========================================
// STATE ACCESS
// ========================================

TArray<float> UReservoirCognitiveIntegration::GetReservoirStateSummary() const
{
    TArray<float> Summary;

    // Compute mean activation for each reservoir
    auto ComputeMean = [](const TArray<float>& State) -> float
    {
        float Sum = 0.0f;
        for (float Val : State)
        {
            Sum += FMath::Abs(Val);
        }
        return State.Num() > 0 ? Sum / State.Num() : 0.0f;
    };

    Summary.Add(ComputeMean(SalienceReservoirState));
    Summary.Add(ComputeMean(AffordanceReservoirState));
    Summary.Add(ComputeMean(PredictionReservoirState));
    Summary.Add(ComputeMean(IntegrationReservoirState));

    return Summary;
}

float UReservoirCognitiveIntegration::GetHierarchicalCoherence() const
{
    if (HierarchicalLevels.Num() == 0)
    {
        return 0.0f;
    }

    float TotalCoherence = 0.0f;
    for (const FHierarchicalReservoirLevel& Level : HierarchicalLevels)
    {
        TotalCoherence += Level.Coherence;
    }
    return TotalCoherence / HierarchicalLevels.Num();
}

// ========================================
// INTERNAL METHODS
// ========================================

TArray<float> UReservoirCognitiveIntegration::UpdateReservoirState(const TArray<float>& Input, TArray<float>& State)
{
    TArray<float> NewState;
    NewState.SetNum(State.Num());

    // Simplified reservoir update: tanh(W_res * state + W_in * input)
    for (int32 i = 0; i < State.Num(); ++i)
    {
        float Activation = 0.0f;

        // Recurrent contribution (simplified)
        for (int32 j = 0; j < State.Num(); ++j)
        {
            int32 WeightIndex = i * State.Num() + j;
            if (WeightIndex < ReservoirWeights[0].Num())
            {
                Activation += ReservoirWeights[0][WeightIndex] * State[j];
            }
        }

        // Input contribution
        if (i < Input.Num())
        {
            Activation += Input[i] * InputScaling;
        }

        // Nonlinearity
        NewState[i] = FMath::Tanh(Activation);
    }

    return NewState;
}

TArray<float> UReservoirCognitiveIntegration::ComputeReservoirOutput(const TArray<float>& State, int32 FunctionIndex)
{
    TArray<float> Output;

    if (FunctionIndex < 0 || FunctionIndex >= OutputWeights.Num())
    {
        return Output;
    }

    // Linear readout
    float OutputVal = 0.0f;
    for (int32 i = 0; i < FMath::Min(State.Num(), OutputWeights[FunctionIndex].Num()); ++i)
    {
        OutputVal += State[i] * OutputWeights[FunctionIndex][i];
    }

    Output.Add(OutputVal);
    return Output;
}

void UReservoirCognitiveIntegration::ApplyLeakyIntegration(TArray<float>& State, const TArray<float>& NewState)
{
    for (int32 i = 0; i < FMath::Min(State.Num(), NewState.Num()); ++i)
    {
        State[i] = (1.0f - LeakingRate) * State[i] + LeakingRate * NewState[i];
    }
}

void UReservoirCognitiveIntegration::PropagateHierarchically()
{
    // Propagate information between hierarchical levels
    for (int32 l = 0; l < HierarchicalLevels.Num() - 1; ++l)
    {
        FHierarchicalReservoirLevel& CurrentLevel = HierarchicalLevels[l];
        FHierarchicalReservoirLevel& NextLevel = HierarchicalLevels[l + 1];

        // Bottom-up propagation
        for (int32 u = 0; u < NextLevel.UnitCount; ++u)
        {
            // Aggregate from lower level
            int32 SourceUnit = u % CurrentLevel.UnitCount;
            if (SourceUnit < CurrentLevel.ReservoirStates.Num() && u < NextLevel.ReservoirStates.Num())
            {
                for (int32 i = 0; i < FMath::Min(CurrentLevel.ReservoirStates[SourceUnit].Num(), NextLevel.ReservoirStates[u].Num()); ++i)
                {
                    NextLevel.ReservoirStates[u][i] += CurrentLevel.ReservoirStates[SourceUnit][i] * CurrentLevel.InterLevelCoupling;
                    NextLevel.ReservoirStates[u][i] = FMath::Clamp(NextLevel.ReservoirStates[u][i], -1.0f, 1.0f);
                }
            }
        }

        // Update level coherence
        float Coherence = 0.0f;
        for (int32 u = 0; u < NextLevel.UnitCount; ++u)
        {
            float UnitMean = 0.0f;
            for (float Val : NextLevel.ReservoirStates[u])
            {
                UnitMean += FMath::Abs(Val);
            }
            UnitMean /= NextLevel.ReservoirStates[u].Num();
            Coherence += UnitMean;
        }
        NextLevel.Coherence = FMath::Clamp(Coherence / NextLevel.UnitCount, 0.0f, 1.0f);
    }
}

void UReservoirCognitiveIntegration::UpdateCognitiveState()
{
    // Update attention allocation based on salience
    CognitiveState.AttentionAllocation.Reset();
    for (const auto& Pair : CognitiveState.SalienceMap)
    {
        if (Pair.Value > 0.3f) // Threshold for attention
        {
            CognitiveState.AttentionAllocation.Add(Pair.Key, Pair.Value);
        }
    }

    // Update integration coherence
    ComputeIntegrationCoherence();
}

TArray<float> UReservoirCognitiveIntegration::GenerateSparseRandomVector(int32 Size, float Sparsity)
{
    TArray<float> Vector;
    Vector.SetNum(Size);

    for (int32 i = 0; i < Size; ++i)
    {
        if (FMath::FRand() < Sparsity)
        {
            Vector[i] = FMath::FRandRange(-1.0f, 1.0f);
        }
        else
        {
            Vector[i] = 0.0f;
        }
    }

    return Vector;
}

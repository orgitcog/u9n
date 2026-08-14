// PredictiveAdaptationEngine.cpp
// Implementation of advanced predictive adaptation for Deep Tree Echo

#include "PredictiveAdaptationEngine.h"
#include "../Echobeats/EchobeatsStreamEngine.h"
#include "../Sensory/SensoryInputIntegration.h"
#include "../Echobeats/TensionalCouplingDynamics.h"
#include "../Avatar/AdvancedEmotionBlending.h"
#include "../Core/CognitiveMemoryManager.h"
#include "Math/UnrealMathUtility.h"

UPredictiveAdaptationEngine::UPredictiveAdaptationEngine()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UPredictiveAdaptationEngine::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeDefaultModels();

    // Initialize meta-learning state
    MetaState.AdaptedLearningRate = BaseLearningRate;
    MetaState.ExplorationBalance = 0.5f;
    MetaState.MetaConfidence = 0.5f;

    // Initialize strategy effectiveness
    MetaState.StrategyEffectiveness.Add(EAdaptationStrategy::GradientDescent, 0.5f);
    MetaState.StrategyEffectiveness.Add(EAdaptationStrategy::HebbianLearning, 0.5f);
    MetaState.StrategyEffectiveness.Add(EAdaptationStrategy::PredictiveCoding, 0.5f);
    MetaState.StrategyEffectiveness.Add(EAdaptationStrategy::FreeEnergyMin, 0.5f);
    MetaState.StrategyEffectiveness.Add(EAdaptationStrategy::ActiveInference, 0.5f);
    MetaState.StrategyEffectiveness.Add(EAdaptationStrategy::MetaLearning, 0.5f);

    CurrentLearningRate = BaseLearningRate;
}

void UPredictiveAdaptationEngine::TickComponent(float DeltaTime, ELevelTick TickType,
                                                  FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update predictions
    UpdatePredictions(DeltaTime);

    // Process prediction errors
    ProcessPredictionErrors();

    // Update meta-learning
    if (bEnableMetaLearning)
    {
        UpdateMetaState();
    }
}

void UPredictiveAdaptationEngine::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        EchobeatsEngine = Owner->FindComponentByClass<UEchobeatsStreamEngine>();
        SensoryIntegration = Owner->FindComponentByClass<USensoryInputIntegration>();
        CouplingDynamics = Owner->FindComponentByClass<UTensionalCouplingDynamics>();
        EmotionBlending = Owner->FindComponentByClass<UAdvancedEmotionBlending>();
        MemoryManager = UCognitiveMemoryManager::GetInstance(this);
    }
}

void UPredictiveAdaptationEngine::InitializeDefaultModels()
{
    // Create default models for each prediction type
    CreateModel(EPredictionType::Sensory, TEXT("DefaultSensory"));
    CreateModel(EPredictionType::Motor, TEXT("DefaultMotor"));
    CreateModel(EPredictionType::Emotional, TEXT("DefaultEmotional"));
    CreateModel(EPredictionType::Cognitive, TEXT("DefaultCognitive"));
    CreateModel(EPredictionType::Social, TEXT("DefaultSocial"));
    CreateModel(EPredictionType::Temporal, TEXT("DefaultTemporal"));

    // Create stream-specific models for sys6
    for (int32 i = 0; i < 3; ++i)
    {
        FString ModelName = FString::Printf(TEXT("Stream%d_Sensory"), i);
        CreateModel(EPredictionType::Sensory, ModelName);
    }
}

TArray<float> UPredictiveAdaptationEngine::GeneratePrediction(EPredictionType Type, const TArray<float>& Context)
{
    // Find best model for this type
    TArray<FGenerativeModelState> Models = GetModelsOfType(Type);

    if (Models.Num() == 0)
    {
        // Return zero prediction
        TArray<float> ZeroPrediction;
        ZeroPrediction.SetNumZeroed(StateVectorDimension);
        return ZeroPrediction;
    }

    // Use model with lowest free energy
    FGenerativeModelState* BestModel = nullptr;
    float LowestFreeEnergy = FLT_MAX;

    for (FGenerativeModelState& Model : Models)
    {
        if (Model.FreeEnergy < LowestFreeEnergy)
        {
            LowestFreeEnergy = Model.FreeEnergy;
            BestModel = &Model;
        }
    }

    if (BestModel)
    {
        return GenerateModelPrediction(BestModel->ModelID, Context);
    }

    TArray<float> ZeroPrediction;
    ZeroPrediction.SetNumZeroed(StateVectorDimension);
    return ZeroPrediction;
}

TArray<float> UPredictiveAdaptationEngine::GenerateModelPrediction(const FString& ModelID, const TArray<float>& Context)
{
    if (!GenerativeModels.Contains(ModelID))
    {
        TArray<float> ZeroPrediction;
        ZeroPrediction.SetNumZeroed(StateVectorDimension);
        return ZeroPrediction;
    }

    FGenerativeModelState& Model = GenerativeModels[ModelID];

    // Simple linear prediction: prediction = prior + context influence
    TArray<float> Prediction;
    Prediction.SetNum(StateVectorDimension);

    float PrecisionWeight = ComputePrecisionWeight(Model.Precision);

    for (int32 i = 0; i < StateVectorDimension; ++i)
    {
        float Prior = (i < Model.PriorBeliefs.Num()) ? Model.PriorBeliefs[i] : 0.0f;
        float ContextInfluence = (i < Context.Num()) ? Context[i] * 0.3f : 0.0f;

        Prediction[i] = Prior * PrecisionWeight + ContextInfluence * (1.0f - PrecisionWeight);
    }

    return Prediction;
}

FPredictionError UPredictiveAdaptationEngine::ComputePredictionError(EPredictionType Type, const TArray<float>& Predicted, const TArray<float>& Observed)
{
    FPredictionError Error;
    Error.ErrorID = NextErrorID++;
    Error.Type = Type;
    Error.Timestamp = GetWorld()->GetTimeSeconds();
    Error.EchobeatStep = Sys6State.EchobeatStep;

    // Compute error direction and magnitude
    int32 Dim = FMath::Min(Predicted.Num(), Observed.Num());
    Error.Direction.SetNum(Dim);

    float SumSquaredError = 0.0f;

    for (int32 i = 0; i < Dim; ++i)
    {
        float Diff = Observed[i] - Predicted[i];
        Error.Direction[i] = Diff;
        SumSquaredError += Diff * Diff;
    }

    Error.Magnitude = FMath::Sqrt(SumSquaredError / FMath::Max(1, Dim));

    // Precision based on model confidence
    Error.Precision = 1.0f; // Default precision

    return Error;
}

void UPredictiveAdaptationEngine::ProcessSensoryInput(const TArray<float>& SensoryData, EPredictionType Type)
{
    // Generate prediction
    TArray<float> Prediction = GeneratePrediction(Type, TArray<float>());

    // Compute error
    FPredictionError Error = ComputePredictionError(Type, Prediction, SensoryData);

    // Store error
    RecentErrors.Add(Error);

    // Limit error history
    while (RecentErrors.Num() > 100)
    {
        RecentErrors.RemoveAt(0);
    }

    // Broadcast error if significant
    if (Error.Magnitude > ErrorThreshold)
    {
        OnPredictionError.Broadcast(Error);

        // Trigger adaptation
        TArray<FGenerativeModelState> Models = GetModelsOfType(Type);
        for (const FGenerativeModelState& Model : Models)
        {
            TriggerAdaptation(Model.ModelID, Error);
        }
    }
}

TArray<float> UPredictiveAdaptationEngine::GetCurrentPrediction(EPredictionType Type) const
{
    TArray<FGenerativeModelState> Models = GetModelsOfType(Type);

    if (Models.Num() > 0)
    {
        return Models[0].PosteriorBeliefs;
    }

    TArray<float> Empty;
    Empty.SetNumZeroed(StateVectorDimension);
    return Empty;
}

FString UPredictiveAdaptationEngine::CreateModel(EPredictionType Type, const FString& ModelName)
{
    FString ModelID = FString::Printf(TEXT("Model_%d_%s"), NextModelID++, *ModelName);

    FGenerativeModelState Model;
    Model.ModelID = ModelID;
    Model.Type = Type;
    Model.Precision = EModelPrecision::Medium;
    Model.FreeEnergy = 1.0f;
    Model.Complexity = 0.0f;
    Model.Accuracy = 0.0f;
    Model.LastUpdateTime = GetWorld()->GetTimeSeconds();
    Model.UpdateCount = 0;

    // Initialize state vectors
    Model.StateVector.SetNumZeroed(StateVectorDimension);
    Model.PriorBeliefs.SetNumZeroed(StateVectorDimension);
    Model.PosteriorBeliefs.SetNumZeroed(StateVectorDimension);

    // Initialize with small random values
    for (int32 i = 0; i < StateVectorDimension; ++i)
    {
        Model.PriorBeliefs[i] = FMath::FRandRange(-0.1f, 0.1f);
        Model.PosteriorBeliefs[i] = Model.PriorBeliefs[i];
    }

    GenerativeModels.Add(ModelID, Model);

    return ModelID;
}

FGenerativeModelState UPredictiveAdaptationEngine::GetModelState(const FString& ModelID) const
{
    if (GenerativeModels.Contains(ModelID))
    {
        return GenerativeModels[ModelID];
    }

    return FGenerativeModelState();
}

TArray<FGenerativeModelState> UPredictiveAdaptationEngine::GetModelsOfType(EPredictionType Type) const
{
    TArray<FGenerativeModelState> Result;

    for (const auto& Pair : GenerativeModels)
    {
        if (Pair.Value.Type == Type)
        {
            Result.Add(Pair.Value);
        }
    }

    return Result;
}

void UPredictiveAdaptationEngine::UpdateModel(const FString& ModelID, const TArray<float>& Observation)
{
    if (!GenerativeModels.Contains(ModelID))
    {
        return;
    }

    FGenerativeModelState& Model = GenerativeModels[ModelID];

    // Update posterior beliefs based on observation
    float PrecisionWeight = ComputePrecisionWeight(Model.Precision);

    for (int32 i = 0; i < FMath::Min(Model.PosteriorBeliefs.Num(), Observation.Num()); ++i)
    {
        // Bayesian update: posterior = prior * likelihood
        float Prior = Model.PriorBeliefs[i];
        float Likelihood = Observation[i];

        Model.PosteriorBeliefs[i] = Prior * PrecisionWeight + Likelihood * (1.0f - PrecisionWeight);
    }

    // Update state
    Model.StateVector = Model.PosteriorBeliefs;
    Model.LastUpdateTime = GetWorld()->GetTimeSeconds();
    Model.UpdateCount++;

    // Recompute free energy
    Model.FreeEnergy = ComputeFreeEnergy(ModelID);

    OnModelUpdated.Broadcast(Model);
}

void UPredictiveAdaptationEngine::SetModelPrecision(const FString& ModelID, EModelPrecision Precision)
{
    if (GenerativeModels.Contains(ModelID))
    {
        GenerativeModels[ModelID].Precision = Precision;
    }
}

void UPredictiveAdaptationEngine::RemoveModel(const FString& ModelID)
{
    GenerativeModels.Remove(ModelID);
}

FAdaptationEvent UPredictiveAdaptationEngine::TriggerAdaptation(const FString& ModelID, const FPredictionError& Error)
{
    FAdaptationEvent Event;
    Event.EventID = NextEventID++;
    Event.Strategy = CurrentStrategy;
    Event.TargetModelID = ModelID;
    Event.TriggeringError = Error;
    Event.LearningRate = CurrentLearningRate;
    Event.Timestamp = GetWorld()->GetTimeSeconds();

    if (!GenerativeModels.Contains(ModelID))
    {
        return Event;
    }

    Event.FreeEnergyBefore = GenerativeModels[ModelID].FreeEnergy;

    // Apply adaptation based on strategy
    ApplyAdaptation(ModelID, Error);

    Event.FreeEnergyAfter = GenerativeModels[ModelID].FreeEnergy;

    // Store event
    AdaptationHistory.Add(Event);
    while (AdaptationHistory.Num() > 100)
    {
        AdaptationHistory.RemoveAt(0);
    }

    OnAdaptationEvent.Broadcast(Event);

    // Update meta-learning
    if (bEnableMetaLearning)
    {
        UpdateMetaLearning(Event);
    }

    return Event;
}

void UPredictiveAdaptationEngine::SetAdaptationStrategy(EAdaptationStrategy Strategy)
{
    CurrentStrategy = Strategy;
}

EAdaptationStrategy UPredictiveAdaptationEngine::GetAdaptationStrategy() const
{
    return CurrentStrategy;
}

TArray<FAdaptationEvent> UPredictiveAdaptationEngine::GetAdaptationHistory(int32 Count) const
{
    TArray<FAdaptationEvent> Result;

    int32 StartIndex = FMath::Max(0, AdaptationHistory.Num() - Count);
    for (int32 i = StartIndex; i < AdaptationHistory.Num(); ++i)
    {
        Result.Add(AdaptationHistory[i]);
    }

    return Result;
}

float UPredictiveAdaptationEngine::GetCurrentLearningRate() const
{
    return CurrentLearningRate;
}

float UPredictiveAdaptationEngine::ComputeFreeEnergy(const FString& ModelID) const
{
    if (!GenerativeModels.Contains(ModelID))
    {
        return 0.0f;
    }

    const FGenerativeModelState& Model = GenerativeModels[ModelID];

    // Free Energy = Complexity - Accuracy
    // Complexity: KL divergence between posterior and prior
    // Accuracy: negative log likelihood of observations

    float Complexity = 0.0f;
    float Accuracy = 0.0f;

    for (int32 i = 0; i < Model.PosteriorBeliefs.Num(); ++i)
    {
        float Posterior = Model.PosteriorBeliefs[i];
        float Prior = Model.PriorBeliefs[i];

        // Simplified KL divergence
        if (FMath::Abs(Prior) > 0.001f)
        {
            Complexity += FMath::Abs(Posterior - Prior);
        }

        // Simplified accuracy (how well posterior matches state)
        float StateVal = (i < Model.StateVector.Num()) ? Model.StateVector[i] : 0.0f;
        Accuracy += FMath::Square(Posterior - StateVal);
    }

    Complexity /= FMath::Max(1, Model.PosteriorBeliefs.Num());
    Accuracy = FMath::Sqrt(Accuracy / FMath::Max(1, Model.PosteriorBeliefs.Num()));

    return Complexity * ComplexityPenalty + Accuracy;
}

float UPredictiveAdaptationEngine::GetTotalFreeEnergy() const
{
    float Total = 0.0f;

    for (const auto& Pair : GenerativeModels)
    {
        Total += Pair.Value.FreeEnergy;
    }

    return Total;
}

TArray<float> UPredictiveAdaptationEngine::MinimizeFreeEnergyAction(const FString& ModelID)
{
    TArray<float> OptimalAction;
    OptimalAction.SetNumZeroed(StateVectorDimension);

    if (!GenerativeModels.Contains(ModelID))
    {
        return OptimalAction;
    }

    const FGenerativeModelState& Model = GenerativeModels[ModelID];

    // Compute action that would minimize prediction error
    // Action = direction that moves state toward prior beliefs
    for (int32 i = 0; i < StateVectorDimension; ++i)
    {
        float Prior = (i < Model.PriorBeliefs.Num()) ? Model.PriorBeliefs[i] : 0.0f;
        float Current = (i < Model.StateVector.Num()) ? Model.StateVector[i] : 0.0f;

        OptimalAction[i] = (Prior - Current) * CurrentLearningRate;
    }

    return OptimalAction;
}

void UPredictiveAdaptationEngine::MinimizeFreeEnergyPerception(const FString& ModelID)
{
    if (!GenerativeModels.Contains(ModelID))
    {
        return;
    }

    // Update beliefs to minimize free energy
    ApplyFreeEnergyMinimization(ModelID);
}

void UPredictiveAdaptationEngine::ProcessSys6Step(int32 LCMStep)
{
    if (!bEnableSys6Sync)
    {
        return;
    }

    // Compute sys6 parameters
    ComputeSys6StepParameters(LCMStep);

    // Update predictions for each stream
    SynchronizeStreamPredictions();

    // Compute cross-stream coherence
    Sys6State.CrossStreamCoherence = ComputeCrossStreamCoherence();

    // Update total free energy
    float OldFreeEnergy = Sys6State.TotalFreeEnergy;
    Sys6State.TotalFreeEnergy = GetTotalFreeEnergy();

    if (FMath::Abs(Sys6State.TotalFreeEnergy - OldFreeEnergy) > 0.01f)
    {
        OnFreeEnergyChanged.Broadcast(OldFreeEnergy, Sys6State.TotalFreeEnergy);
    }
}

FSys6PredictionState UPredictiveAdaptationEngine::GetSys6PredictionState() const
{
    return Sys6State;
}

void UPredictiveAdaptationEngine::SynchronizeStreamPredictions()
{
    Sys6State.StreamPredictions.Empty();

    for (int32 i = 0; i < 3; ++i)
    {
        FString ModelID = FString::Printf(TEXT("Model_%d_Stream%d_Sensory"), i + 7, i);

        if (GenerativeModels.Contains(ModelID))
        {
            Sys6State.StreamPredictions.Add(GenerativeModels[ModelID]);
        }
    }
}

TArray<float> UPredictiveAdaptationEngine::GetStreamPrediction(int32 StreamIndex) const
{
    if (StreamIndex >= 0 && StreamIndex < Sys6State.StreamPredictions.Num())
    {
        return Sys6State.StreamPredictions[StreamIndex].PosteriorBeliefs;
    }

    TArray<float> Empty;
    Empty.SetNumZeroed(StateVectorDimension);
    return Empty;
}

FMetaLearningState UPredictiveAdaptationEngine::GetMetaLearningState() const
{
    return MetaState;
}

void UPredictiveAdaptationEngine::UpdateMetaLearning(const FAdaptationEvent& Event)
{
    // Compute effectiveness of this adaptation
    float FreeEnergyReduction = Event.FreeEnergyBefore - Event.FreeEnergyAfter;
    float Effectiveness = FMath::Clamp(FreeEnergyReduction, -1.0f, 1.0f);

    // Update strategy effectiveness
    if (MetaState.StrategyEffectiveness.Contains(Event.Strategy))
    {
        float OldEffectiveness = MetaState.StrategyEffectiveness[Event.Strategy];
        float NewEffectiveness = OldEffectiveness + MetaLearningRate * (Effectiveness - OldEffectiveness);
        MetaState.StrategyEffectiveness[Event.Strategy] = FMath::Clamp(NewEffectiveness, 0.0f, 1.0f);
    }

    // Adapt learning rate
    if (Effectiveness > 0.0f)
    {
        // Successful adaptation - increase learning rate slightly
        MetaState.AdaptedLearningRate = FMath::Min(MetaState.AdaptedLearningRate * 1.01f, MaxLearningRate);
    }
    else
    {
        // Unsuccessful adaptation - decrease learning rate
        MetaState.AdaptedLearningRate = FMath::Max(MetaState.AdaptedLearningRate * 0.99f, MinLearningRate);
    }

    CurrentLearningRate = MetaState.AdaptedLearningRate;

    // Update meta-confidence
    float AvgEffectiveness = 0.0f;
    for (const auto& Pair : MetaState.StrategyEffectiveness)
    {
        AvgEffectiveness += Pair.Value;
    }
    AvgEffectiveness /= FMath::Max(1, MetaState.StrategyEffectiveness.Num());

    MetaState.MetaConfidence = FMath::Lerp(MetaState.MetaConfidence, AvgEffectiveness, MetaLearningRate);

    OnMetaLearningUpdate.Broadcast(MetaState);
}

EAdaptationStrategy UPredictiveAdaptationEngine::GetRecommendedStrategy() const
{
    EAdaptationStrategy BestStrategy = EAdaptationStrategy::PredictiveCoding;
    float BestEffectiveness = 0.0f;

    for (const auto& Pair : MetaState.StrategyEffectiveness)
    {
        if (Pair.Value > BestEffectiveness)
        {
            BestEffectiveness = Pair.Value;
            BestStrategy = Pair.Key;
        }
    }

    return BestStrategy;
}

void UPredictiveAdaptationEngine::ResetMetaLearning()
{
    MetaState.AdaptedLearningRate = BaseLearningRate;
    MetaState.ExplorationBalance = 0.5f;
    MetaState.MetaConfidence = 0.5f;

    for (auto& Pair : MetaState.StrategyEffectiveness)
    {
        Pair.Value = 0.5f;
    }

    MetaState.PrecisionAdjustments.Empty();
    CurrentLearningRate = BaseLearningRate;
}

void UPredictiveAdaptationEngine::ProcessEchobeatStep(int32 Step)
{
    Sys6State.EchobeatStep = Step;

    // Map echobeat step to LCM step
    // 12 echobeat steps map to 30 LCM steps
    // Each echobeat step spans 2.5 LCM steps
    int32 LCMStep = ((Step - 1) * 30 / 12) % 30;

    ProcessSys6Step(LCMStep);
}

TArray<float> UPredictiveAdaptationEngine::GetEchobeatPhasePrediction(int32 Phase) const
{
    // Phase 0: Pivotal (steps 1, 5, 9)
    // Phase 1: Affordance (steps 2, 6, 10)
    // Phase 2: Salience (steps 3, 7, 11)
    // Phase 3: Integration (steps 4, 8, 12)

    EPredictionType Type;
    switch (Phase)
    {
    case 0:
        Type = EPredictionType::Cognitive;
        break;
    case 1:
        Type = EPredictionType::Motor;
        break;
    case 2:
        Type = EPredictionType::Sensory;
        break;
    case 3:
    default:
        Type = EPredictionType::Temporal;
        break;
    }

    return GetCurrentPrediction(Type);
}

void UPredictiveAdaptationEngine::UpdateFromStreamState(int32 StreamIndex, const TArray<float>& StreamState)
{
    FString ModelID = FString::Printf(TEXT("Model_%d_Stream%d_Sensory"), StreamIndex + 7, StreamIndex);

    if (GenerativeModels.Contains(ModelID))
    {
        UpdateModel(ModelID, StreamState);
    }
}

void UPredictiveAdaptationEngine::UpdatePredictions(float DeltaTime)
{
    // Update all models with time decay
    for (auto& Pair : GenerativeModels)
    {
        FGenerativeModelState& Model = Pair.Value;

        // Slowly drift posterior toward prior
        float DriftRate = 0.01f * DeltaTime;
        for (int32 i = 0; i < Model.PosteriorBeliefs.Num(); ++i)
        {
            float Prior = (i < Model.PriorBeliefs.Num()) ? Model.PriorBeliefs[i] : 0.0f;
            Model.PosteriorBeliefs[i] = FMath::Lerp(Model.PosteriorBeliefs[i], Prior, DriftRate);
        }
    }
}

void UPredictiveAdaptationEngine::ProcessPredictionErrors()
{
    // Process accumulated errors
    for (const FPredictionError& Error : RecentErrors)
    {
        if (Error.Magnitude > ErrorThreshold)
        {
            // Find relevant models
            TArray<FGenerativeModelState> Models = GetModelsOfType(Error.Type);

            for (const FGenerativeModelState& Model : Models)
            {
                // Apply adaptation
                ApplyAdaptation(Model.ModelID, Error);
            }
        }
    }
}

void UPredictiveAdaptationEngine::ApplyAdaptation(const FString& ModelID, const FPredictionError& Error)
{
    switch (CurrentStrategy)
    {
    case EAdaptationStrategy::GradientDescent:
        {
            TArray<float> Gradient = ComputeGradient(ModelID, Error);
            ApplyGradientDescent(ModelID, Gradient);
        }
        break;

    case EAdaptationStrategy::HebbianLearning:
        {
            if (GenerativeModels.Contains(ModelID))
            {
                ApplyHebbianUpdate(ModelID, GenerativeModels[ModelID].PriorBeliefs, Error.Direction);
            }
        }
        break;

    case EAdaptationStrategy::PredictiveCoding:
        ApplyPredictiveCodingUpdate(ModelID, Error);
        break;

    case EAdaptationStrategy::FreeEnergyMin:
        ApplyFreeEnergyMinimization(ModelID);
        break;

    case EAdaptationStrategy::ActiveInference:
        ApplyActiveInference(ModelID);
        break;

    case EAdaptationStrategy::MetaLearning:
        {
            // Use recommended strategy
            EAdaptationStrategy Recommended = GetRecommendedStrategy();
            EAdaptationStrategy OldStrategy = CurrentStrategy;
            CurrentStrategy = Recommended;
            ApplyAdaptation(ModelID, Error);
            CurrentStrategy = OldStrategy;
        }
        break;
    }

    // Recompute free energy
    if (GenerativeModels.Contains(ModelID))
    {
        GenerativeModels[ModelID].FreeEnergy = ComputeFreeEnergy(ModelID);
    }
}

void UPredictiveAdaptationEngine::UpdateMetaState()
{
    // Adjust exploration-exploitation balance based on recent performance
    float RecentAvgError = 0.0f;
    int32 RecentCount = FMath::Min(10, RecentErrors.Num());

    for (int32 i = RecentErrors.Num() - RecentCount; i < RecentErrors.Num(); ++i)
    {
        RecentAvgError += RecentErrors[i].Magnitude;
    }

    if (RecentCount > 0)
    {
        RecentAvgError /= RecentCount;
    }

    // High error -> more exploration
    // Low error -> more exploitation
    MetaState.ExplorationBalance = FMath::Clamp(RecentAvgError, 0.1f, 0.9f);
}

float UPredictiveAdaptationEngine::ComputePrecisionWeight(EModelPrecision Precision) const
{
    switch (Precision)
    {
    case EModelPrecision::VeryLow:
        return 0.2f;
    case EModelPrecision::Low:
        return 0.4f;
    case EModelPrecision::Medium:
        return 0.6f;
    case EModelPrecision::High:
        return 0.8f;
    case EModelPrecision::VeryHigh:
        return 0.95f;
    default:
        return 0.6f;
    }
}

TArray<float> UPredictiveAdaptationEngine::ComputeGradient(const FString& ModelID, const FPredictionError& Error)
{
    TArray<float> Gradient;
    Gradient.SetNum(StateVectorDimension);

    // Gradient is proportional to error direction scaled by precision
    for (int32 i = 0; i < StateVectorDimension; ++i)
    {
        float ErrorComponent = (i < Error.Direction.Num()) ? Error.Direction[i] : 0.0f;
        Gradient[i] = ErrorComponent * Error.Precision;
    }

    return Gradient;
}

void UPredictiveAdaptationEngine::ApplyGradientDescent(const FString& ModelID, const TArray<float>& Gradient)
{
    if (!GenerativeModels.Contains(ModelID))
    {
        return;
    }

    FGenerativeModelState& Model = GenerativeModels[ModelID];

    // Update posterior beliefs in direction of gradient
    for (int32 i = 0; i < Model.PosteriorBeliefs.Num(); ++i)
    {
        float GradComponent = (i < Gradient.Num()) ? Gradient[i] : 0.0f;
        Model.PosteriorBeliefs[i] += CurrentLearningRate * GradComponent;
    }

    Model.UpdateCount++;
    Model.LastUpdateTime = GetWorld()->GetTimeSeconds();
}

void UPredictiveAdaptationEngine::ApplyHebbianUpdate(const FString& ModelID, const TArray<float>& PreState, const TArray<float>& PostState)
{
    if (!GenerativeModels.Contains(ModelID))
    {
        return;
    }

    FGenerativeModelState& Model = GenerativeModels[ModelID];

    // Hebbian: "neurons that fire together wire together"
    // Update = learning_rate * pre * post
    for (int32 i = 0; i < Model.PosteriorBeliefs.Num(); ++i)
    {
        float Pre = (i < PreState.Num()) ? PreState[i] : 0.0f;
        float Post = (i < PostState.Num()) ? PostState[i] : 0.0f;

        Model.PosteriorBeliefs[i] += CurrentLearningRate * Pre * Post;
    }

    Model.UpdateCount++;
    Model.LastUpdateTime = GetWorld()->GetTimeSeconds();
}

void UPredictiveAdaptationEngine::ApplyPredictiveCodingUpdate(const FString& ModelID, const FPredictionError& Error)
{
    if (!GenerativeModels.Contains(ModelID))
    {
        return;
    }

    FGenerativeModelState& Model = GenerativeModels[ModelID];
    float PrecisionWeight = ComputePrecisionWeight(Model.Precision);

    // Predictive coding: update beliefs to reduce prediction error
    // weighted by precision
    for (int32 i = 0; i < Model.PosteriorBeliefs.Num(); ++i)
    {
        float ErrorComponent = (i < Error.Direction.Num()) ? Error.Direction[i] : 0.0f;
        float Update = CurrentLearningRate * Error.Precision * PrecisionWeight * ErrorComponent;
        Model.PosteriorBeliefs[i] += Update;
    }

    Model.UpdateCount++;
    Model.LastUpdateTime = GetWorld()->GetTimeSeconds();
}

void UPredictiveAdaptationEngine::ApplyFreeEnergyMinimization(const FString& ModelID)
{
    if (!GenerativeModels.Contains(ModelID))
    {
        return;
    }

    FGenerativeModelState& Model = GenerativeModels[ModelID];

    // Minimize free energy by moving posterior toward prior
    // while maintaining accuracy
    for (int32 i = 0; i < Model.PosteriorBeliefs.Num(); ++i)
    {
        float Prior = (i < Model.PriorBeliefs.Num()) ? Model.PriorBeliefs[i] : 0.0f;
        float Posterior = Model.PosteriorBeliefs[i];

        // Move toward prior (reduces complexity)
        float ComplexityGradient = Posterior - Prior;

        Model.PosteriorBeliefs[i] -= CurrentLearningRate * ComplexityPenalty * ComplexityGradient;
    }

    Model.UpdateCount++;
    Model.LastUpdateTime = GetWorld()->GetTimeSeconds();
}

void UPredictiveAdaptationEngine::ApplyActiveInference(const FString& ModelID)
{
    if (!GenerativeModels.Contains(ModelID))
    {
        return;
    }

    FGenerativeModelState& Model = GenerativeModels[ModelID];

    // Active inference: update beliefs AND plan actions to minimize expected free energy
    // Here we just update beliefs; action selection would be handled by motor system

    // Compute expected free energy gradient
    for (int32 i = 0; i < Model.PosteriorBeliefs.Num(); ++i)
    {
        float Prior = (i < Model.PriorBeliefs.Num()) ? Model.PriorBeliefs[i] : 0.0f;
        float Posterior = Model.PosteriorBeliefs[i];
        float State = (i < Model.StateVector.Num()) ? Model.StateVector[i] : 0.0f;

        // Expected free energy gradient combines:
        // 1. Epistemic value (information gain)
        // 2. Pragmatic value (goal achievement)
        float EpistemicGrad = Posterior - Prior;
        float PragmaticGrad = Posterior - State;

        float TotalGrad = EpistemicGrad * 0.5f + PragmaticGrad * 0.5f;
        Model.PosteriorBeliefs[i] -= CurrentLearningRate * TotalGrad;
    }

    Model.UpdateCount++;
    Model.LastUpdateTime = GetWorld()->GetTimeSeconds();
}

void UPredictiveAdaptationEngine::ComputeSys6StepParameters(int32 LCMStep)
{
    Sys6State.LCMStep = LCMStep;

    // Compute echobeat step from LCM step
    // 30 LCM steps -> 12 echobeat steps
    Sys6State.EchobeatStep = ((LCMStep * 12) / 30) + 1;
    if (Sys6State.EchobeatStep > 12) Sys6State.EchobeatStep = 12;

    // Compute dyad state (A or B) based on 4-step pattern
    // Pattern: A, A, B, B (repeating)
    int32 PatternStep = LCMStep % 4;
    Sys6State.DyadState = (PatternStep < 2) ? TEXT("A") : TEXT("B");

    // Compute triad state (1, 2, or 3)
    // Pattern: 1, 2, 2, 3 (repeating)
    switch (PatternStep)
    {
    case 0:
        Sys6State.TriadState = 1;
        break;
    case 1:
    case 2:
        Sys6State.TriadState = 2;
        break;
    case 3:
        Sys6State.TriadState = 3;
        break;
    }
}

float UPredictiveAdaptationEngine::ComputeCrossStreamCoherence() const
{
    if (Sys6State.StreamPredictions.Num() < 2)
    {
        return 1.0f;
    }

    float TotalCoherence = 0.0f;
    int32 PairCount = 0;

    // Compute pairwise coherence between streams
    for (int32 i = 0; i < Sys6State.StreamPredictions.Num(); ++i)
    {
        for (int32 j = i + 1; j < Sys6State.StreamPredictions.Num(); ++j)
        {
            const TArray<float>& A = Sys6State.StreamPredictions[i].PosteriorBeliefs;
            const TArray<float>& B = Sys6State.StreamPredictions[j].PosteriorBeliefs;

            // Compute cosine similarity
            float DotProduct = 0.0f;
            float NormA = 0.0f;
            float NormB = 0.0f;

            int32 Dim = FMath::Min(A.Num(), B.Num());
            for (int32 k = 0; k < Dim; ++k)
            {
                DotProduct += A[k] * B[k];
                NormA += A[k] * A[k];
                NormB += B[k] * B[k];
            }

            if (NormA > 0.0f && NormB > 0.0f)
            {
                float Similarity = DotProduct / (FMath::Sqrt(NormA) * FMath::Sqrt(NormB));
                TotalCoherence += (Similarity + 1.0f) / 2.0f; // Normalize to 0-1
            }

            PairCount++;
        }
    }

    return (PairCount > 0) ? TotalCoherence / PairCount : 1.0f;
}

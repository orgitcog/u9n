// FutureStatePrediction.cpp
// Implementation of Future State Prediction System for Deep Tree Echo

#include "FutureStatePrediction.h"
#include "../Learning/PredictiveAdaptationEngine.h"
#include "../ActiveInference/ActiveInferenceEngine.h"
#include "../Echobeats/EchobeatsStreamEngine.h"
#include "../Reservoir/DeepTreeEchoReservoir.h"
#include "Math/UnrealMathUtility.h"

UFutureStatePrediction::UFutureStatePrediction()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UFutureStatePrediction::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeMethodWeights();
}

void UFutureStatePrediction::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Auto-validate pending predictions if enabled
    if (bAutoValidate)
    {
        UpdatePendingValidations();
    }

    // Trim prediction history
    TrimPredictionHistory();
}

void UFutureStatePrediction::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        PredictiveEngine = Owner->FindComponentByClass<UPredictiveAdaptationEngine>();
        ActiveInference = Owner->FindComponentByClass<UActiveInferenceEngine>();
        EchobeatsEngine = Owner->FindComponentByClass<UEchobeatsStreamEngine>();
        Reservoir = Owner->FindComponentByClass<UDeepTreeEchoReservoir>();
    }
}

void UFutureStatePrediction::InitializeMethodWeights()
{
    // Initialize method weights with equal weighting
    MethodWeights.Add(ETrajectoryType::Linear, 0.25f);
    MethodWeights.Add(ETrajectoryType::Polynomial, 0.25f);
    MethodWeights.Add(ETrajectoryType::Exponential, 0.2f);
    MethodWeights.Add(ETrajectoryType::Reservoir, 0.3f);

    // Initialize statistics
    Statistics.TotalPredictions = 0;
    Statistics.ValidatedPredictions = 0;
    Statistics.AverageMSE = 0.0f;
    Statistics.AverageConfidence = 0.0f;
    Statistics.WithinBoundsRatio = 0.0f;
    Statistics.BestMethod = ETrajectoryType::Linear;
}

FString UFutureStatePrediction::CreateTrajectory(EStateDomain Domain, const FString& Name)
{
    FString TrajectoryID = FString::Printf(TEXT("Trajectory_%d_%s"), NextTrajectoryID++, 
        Name.IsEmpty() ? TEXT("Auto") : *Name);

    FStateTrajectory Trajectory;
    Trajectory.TrajectoryID = TrajectoryID;
    Trajectory.Domain = Domain;
    Trajectory.FittingType = DefaultPredictionMethod;
    Trajectory.GoodnessOfFit = 0.0f;
    Trajectory.CreatedAt = GetWorld()->GetTimeSeconds();
    Trajectory.LastUpdatedAt = Trajectory.CreatedAt;

    Trajectories.Add(TrajectoryID, Trajectory);

    return TrajectoryID;
}

void UFutureStatePrediction::AddObservation(const FString& TrajectoryID, const TArray<float>& StateVector, float Reliability)
{
    if (!Trajectories.Contains(TrajectoryID))
    {
        return;
    }

    FStateTrajectory& Trajectory = Trajectories[TrajectoryID];

    FStateObservation Observation;
    Observation.Timestamp = GetWorld()->GetTimeSeconds();
    Observation.EchobeatStep = CurrentEchobeatStep;
    Observation.StateVector = StateVector;
    Observation.Domain = Trajectory.Domain;
    Observation.Reliability = FMath::Clamp(Reliability, 0.0f, 1.0f);

    Trajectory.Observations.Add(Observation);
    Trajectory.LastUpdatedAt = Observation.Timestamp;

    // Trim to max length
    while (Trajectory.Observations.Num() > MaxTrajectoryLength)
    {
        Trajectory.Observations.RemoveAt(0);
    }

    // Auto-fit if enough observations
    if (Trajectory.Observations.Num() >= MinObservationsForFitting)
    {
        FitTrajectory(TrajectoryID, Trajectory.FittingType);
    }

    OnTrajectoryUpdated.Broadcast(Trajectory);
}

FStateTrajectory UFutureStatePrediction::GetTrajectory(const FString& TrajectoryID) const
{
    if (Trajectories.Contains(TrajectoryID))
    {
        return Trajectories[TrajectoryID];
    }
    return FStateTrajectory();
}

TArray<FStateTrajectory> UFutureStatePrediction::GetTrajectoriesForDomain(EStateDomain Domain) const
{
    TArray<FStateTrajectory> Result;
    for (const auto& Pair : Trajectories)
    {
        if (Pair.Value.Domain == Domain)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

float UFutureStatePrediction::FitTrajectory(const FString& TrajectoryID, ETrajectoryType Method)
{
    if (!Trajectories.Contains(TrajectoryID))
    {
        return 0.0f;
    }

    FStateTrajectory& Trajectory = Trajectories[TrajectoryID];

    if (Trajectory.Observations.Num() < MinObservationsForFitting)
    {
        return 0.0f;
    }

    Trajectory.FittingType = Method;

    // Compute fitted parameters based on method
    int32 Dim = StateVectorDimension;
    if (Trajectory.Observations.Num() > 0 && Trajectory.Observations[0].StateVector.Num() > 0)
    {
        Dim = Trajectory.Observations[0].StateVector.Num();
    }

    // For linear fitting: slope and intercept per dimension
    if (Method == ETrajectoryType::Linear)
    {
        Trajectory.FittedParameters.SetNum(Dim * 2);
        
        int32 N = Trajectory.Observations.Num();
        float BaseTime = Trajectory.Observations[0].Timestamp;

        for (int32 d = 0; d < Dim; ++d)
        {
            // Linear regression: y = mx + b
            float SumX = 0.0f, SumY = 0.0f, SumXY = 0.0f, SumX2 = 0.0f;
            
            for (int32 i = 0; i < N; ++i)
            {
                float X = Trajectory.Observations[i].Timestamp - BaseTime;
                float Y = (d < Trajectory.Observations[i].StateVector.Num()) ? 
                    Trajectory.Observations[i].StateVector[d] : 0.0f;
                
                SumX += X;
                SumY += Y;
                SumXY += X * Y;
                SumX2 += X * X;
            }

            float Denom = N * SumX2 - SumX * SumX;
            if (FMath::Abs(Denom) > 1e-6f)
            {
                float Slope = (N * SumXY - SumX * SumY) / Denom;
                float Intercept = (SumY - Slope * SumX) / N;
                Trajectory.FittedParameters[d * 2] = Slope;
                Trajectory.FittedParameters[d * 2 + 1] = Intercept;
            }
            else
            {
                Trajectory.FittedParameters[d * 2] = 0.0f;
                Trajectory.FittedParameters[d * 2 + 1] = SumY / N;
            }
        }
    }
    else if (Method == ETrajectoryType::Polynomial)
    {
        // Simplified polynomial: store coefficients for quadratic (a, b, c per dimension)
        int32 Degree = FMath::Min(PolynomialDegree, 3);
        Trajectory.FittedParameters.SetNum(Dim * (Degree + 1));
        
        int32 N = Trajectory.Observations.Num();
        float BaseTime = Trajectory.Observations[0].Timestamp;

        for (int32 d = 0; d < Dim; ++d)
        {
            // Simplified quadratic fitting using least squares approximation
            float SumY = 0.0f;
            float LastY = 0.0f;
            float FirstY = 0.0f;
            
            for (int32 i = 0; i < N; ++i)
            {
                float Y = (d < Trajectory.Observations[i].StateVector.Num()) ? 
                    Trajectory.Observations[i].StateVector[d] : 0.0f;
                SumY += Y;
                if (i == 0) FirstY = Y;
                if (i == N - 1) LastY = Y;
            }

            float AvgY = SumY / N;
            float TotalTime = Trajectory.Observations[N-1].Timestamp - BaseTime;
            
            // Approximate: c = first value, a = curvature estimate, b = slope estimate
            float Slope = (TotalTime > 1e-6f) ? (LastY - FirstY) / TotalTime : 0.0f;
            
            Trajectory.FittedParameters[d * (Degree + 1)] = 0.0f;  // a (curvature, simplified to 0)
            Trajectory.FittedParameters[d * (Degree + 1) + 1] = Slope;  // b (slope)
            Trajectory.FittedParameters[d * (Degree + 1) + 2] = FirstY;  // c (intercept)
            if (Degree >= 3)
            {
                Trajectory.FittedParameters[d * (Degree + 1) + 3] = 0.0f;  // d (cubic term)
            }
        }
    }
    else if (Method == ETrajectoryType::Exponential)
    {
        // Exponential: y = a * exp(b * t) + c per dimension
        Trajectory.FittedParameters.SetNum(Dim * 3);
        
        int32 N = Trajectory.Observations.Num();
        float BaseTime = Trajectory.Observations[0].Timestamp;

        for (int32 d = 0; d < Dim; ++d)
        {
            float FirstY = (d < Trajectory.Observations[0].StateVector.Num()) ? 
                Trajectory.Observations[0].StateVector[d] : 0.0f;
            float LastY = (d < Trajectory.Observations[N-1].StateVector.Num()) ? 
                Trajectory.Observations[N-1].StateVector[d] : 0.0f;
            float TotalTime = Trajectory.Observations[N-1].Timestamp - BaseTime;

            // Simplified exponential fit
            float Ratio = (FMath::Abs(FirstY) > 1e-6f) ? LastY / FirstY : 1.0f;
            float Rate = (TotalTime > 1e-6f && Ratio > 0.0f) ? 
                FMath::Loge(FMath::Max(Ratio, 0.001f)) / TotalTime : 0.0f;

            Trajectory.FittedParameters[d * 3] = FirstY;  // a (amplitude)
            Trajectory.FittedParameters[d * 3 + 1] = Rate;  // b (rate)
            Trajectory.FittedParameters[d * 3 + 2] = 0.0f;  // c (offset)
        }
    }

    // Compute goodness of fit
    Trajectory.GoodnessOfFit = ComputeGoodnessOfFit(Trajectory, Trajectory.FittedParameters, Method);

    return Trajectory.GoodnessOfFit;
}

void UFutureStatePrediction::ClearTrajectory(const FString& TrajectoryID)
{
    if (Trajectories.Contains(TrajectoryID))
    {
        Trajectories[TrajectoryID].Observations.Empty();
        Trajectories[TrajectoryID].FittedParameters.Empty();
        Trajectories[TrajectoryID].GoodnessOfFit = 0.0f;
    }
}

void UFutureStatePrediction::RemoveTrajectory(const FString& TrajectoryID)
{
    Trajectories.Remove(TrajectoryID);
}

FFuturePrediction UFutureStatePrediction::PredictFutureState(const FString& TrajectoryID, int32 StepsAhead, ETrajectoryType Method)
{
    FFuturePrediction Prediction;
    Prediction.PredictionID = NextPredictionID++;
    Prediction.SourceTrajectoryID = TrajectoryID;
    Prediction.StepsAhead = StepsAhead;
    Prediction.PredictionMethod = Method;
    Prediction.CreatedAt = GetWorld()->GetTimeSeconds();
    Prediction.Horizon = ClassifyHorizon(StepsAhead);

    if (!Trajectories.Contains(TrajectoryID))
    {
        Prediction.Confidence = 0.0f;
        Prediction.ConfidenceLevel = EPredictionConfidence::VeryLow;
        return Prediction;
    }

    const FStateTrajectory& Trajectory = Trajectories[TrajectoryID];
    Prediction.Domain = Trajectory.Domain;

    if (Trajectory.Observations.Num() < MinObservationsForFitting)
    {
        Prediction.Confidence = 0.0f;
        Prediction.ConfidenceLevel = EPredictionConfidence::VeryLow;
        return Prediction;
    }

    // Compute target timestamp
    float LastTime = Trajectory.Observations.Last().Timestamp;
    float AvgDelta = 0.0f;
    if (Trajectory.Observations.Num() > 1)
    {
        AvgDelta = (LastTime - Trajectory.Observations[0].Timestamp) / (Trajectory.Observations.Num() - 1);
    }
    else
    {
        AvgDelta = 0.1f;  // Default 100ms step
    }
    Prediction.TargetTimestamp = LastTime + AvgDelta * StepsAhead;

    // Generate prediction based on method
    if (Method == ETrajectoryType::Ensemble && bEnableEnsemble)
    {
        FEnsemblePrediction Ensemble = GenerateEnsemblePrediction(TrajectoryID, StepsAhead);
        Prediction.PredictedState = Ensemble.EnsembleState;
        Prediction.Confidence = Ensemble.EnsembleConfidence;
    }
    else
    {
        switch (Method)
        {
        case ETrajectoryType::Linear:
            Prediction.PredictedState = PredictLinear(Trajectory, StepsAhead);
            break;
        case ETrajectoryType::Polynomial:
            Prediction.PredictedState = PredictPolynomial(Trajectory, StepsAhead);
            break;
        case ETrajectoryType::Exponential:
            Prediction.PredictedState = PredictExponential(Trajectory, StepsAhead);
            break;
        case ETrajectoryType::Reservoir:
            Prediction.PredictedState = PredictReservoir(Trajectory, StepsAhead);
            break;
        default:
            Prediction.PredictedState = PredictLinear(Trajectory, StepsAhead);
            break;
        }
        Prediction.Confidence = ComputePredictionConfidence(TrajectoryID, StepsAhead);
    }

    // Compute uncertainty bounds
    Prediction.UncertaintyBounds = ComputeUncertaintyBounds(TrajectoryID, StepsAhead);
    Prediction.ConfidenceLevel = ClassifyConfidence(Prediction.Confidence);

    // Store prediction
    Predictions.Add(Prediction.PredictionID, Prediction);
    Statistics.TotalPredictions++;

    OnPredictionGenerated.Broadcast(Prediction);

    return Prediction;
}

TArray<FFuturePrediction> UFutureStatePrediction::PredictMultipleHorizons(const FString& TrajectoryID, const TArray<int32>& StepsAhead)
{
    TArray<FFuturePrediction> Result;
    for (int32 Steps : StepsAhead)
    {
        Result.Add(PredictFutureState(TrajectoryID, Steps, DefaultPredictionMethod));
    }
    return Result;
}

FEnsemblePrediction UFutureStatePrediction::GenerateEnsemblePrediction(const FString& TrajectoryID, int32 StepsAhead)
{
    FEnsemblePrediction Ensemble;

    if (!Trajectories.Contains(TrajectoryID))
    {
        return Ensemble;
    }

    const FStateTrajectory& Trajectory = Trajectories[TrajectoryID];

    // Generate predictions from each method
    TArray<ETrajectoryType> Methods = {
        ETrajectoryType::Linear,
        ETrajectoryType::Polynomial,
        ETrajectoryType::Exponential,
        ETrajectoryType::Reservoir
    };

    for (ETrajectoryType Method : Methods)
    {
        FFuturePrediction IndividualPred;
        IndividualPred.PredictionID = NextPredictionID++;
        IndividualPred.SourceTrajectoryID = TrajectoryID;
        IndividualPred.StepsAhead = StepsAhead;
        IndividualPred.PredictionMethod = Method;
        IndividualPred.Domain = Trajectory.Domain;
        IndividualPred.CreatedAt = GetWorld()->GetTimeSeconds();

        switch (Method)
        {
        case ETrajectoryType::Linear:
            IndividualPred.PredictedState = PredictLinear(Trajectory, StepsAhead);
            break;
        case ETrajectoryType::Polynomial:
            IndividualPred.PredictedState = PredictPolynomial(Trajectory, StepsAhead);
            break;
        case ETrajectoryType::Exponential:
            IndividualPred.PredictedState = PredictExponential(Trajectory, StepsAhead);
            break;
        case ETrajectoryType::Reservoir:
            IndividualPred.PredictedState = PredictReservoir(Trajectory, StepsAhead);
            break;
        default:
            IndividualPred.PredictedState = PredictLinear(Trajectory, StepsAhead);
            break;
        }

        IndividualPred.Confidence = ComputePredictionConfidence(TrajectoryID, StepsAhead) * 
            (MethodWeights.Contains(Method) ? MethodWeights[Method] : 0.25f);

        Ensemble.IndividualPredictions.Add(IndividualPred);
    }

    // Compute weighted ensemble
    int32 Dim = 0;
    for (const FFuturePrediction& Pred : Ensemble.IndividualPredictions)
    {
        Dim = FMath::Max(Dim, Pred.PredictedState.Num());
    }

    Ensemble.EnsembleState.SetNumZeroed(Dim);
    float TotalWeight = 0.0f;

    for (int32 i = 0; i < Ensemble.IndividualPredictions.Num(); ++i)
    {
        const FFuturePrediction& Pred = Ensemble.IndividualPredictions[i];
        float Weight = MethodWeights.Contains(Pred.PredictionMethod) ? 
            MethodWeights[Pred.PredictionMethod] : 0.25f;
        
        Ensemble.MethodWeights.Add(Weight);
        TotalWeight += Weight;

        for (int32 d = 0; d < Pred.PredictedState.Num(); ++d)
        {
            Ensemble.EnsembleState[d] += Pred.PredictedState[d] * Weight;
        }
    }

    // Normalize
    if (TotalWeight > 1e-6f)
    {
        for (int32 d = 0; d < Dim; ++d)
        {
            Ensemble.EnsembleState[d] /= TotalWeight;
        }
    }

    // Compute ensemble confidence
    Ensemble.EnsembleConfidence = ComputePredictionConfidence(TrajectoryID, StepsAhead);

    // Compute method disagreement (variance)
    float Disagreement = 0.0f;
    for (const FFuturePrediction& Pred : Ensemble.IndividualPredictions)
    {
        float Dist = ComputeMSE(Pred.PredictedState, Ensemble.EnsembleState);
        Disagreement += Dist;
    }
    Ensemble.MethodDisagreement = Disagreement / FMath::Max(1, Ensemble.IndividualPredictions.Num());

    // Reduce confidence if methods disagree
    Ensemble.EnsembleConfidence *= (1.0f - FMath::Clamp(Ensemble.MethodDisagreement, 0.0f, 0.5f));

    OnEnsemblePrediction.Broadcast(Ensemble);

    return Ensemble;
}

FFuturePrediction UFutureStatePrediction::GetPrediction(int32 PredictionID) const
{
    if (Predictions.Contains(PredictionID))
    {
        return Predictions[PredictionID];
    }
    return FFuturePrediction();
}

TArray<FFuturePrediction> UFutureStatePrediction::GetRecentPredictions(int32 Count) const
{
    TArray<FFuturePrediction> Result;
    
    // Get predictions sorted by creation time
    TArray<FFuturePrediction> AllPreds;
    for (const auto& Pair : Predictions)
    {
        AllPreds.Add(Pair.Value);
    }
    
    // Sort by creation time descending
    AllPreds.Sort([](const FFuturePrediction& A, const FFuturePrediction& B) {
        return A.CreatedAt > B.CreatedAt;
    });

    int32 N = FMath::Min(Count, AllPreds.Num());
    for (int32 i = 0; i < N; ++i)
    {
        Result.Add(AllPreds[i]);
    }

    return Result;
}

TArray<FFuturePrediction> UFutureStatePrediction::GetPendingPredictions() const
{
    TArray<FFuturePrediction> Result;
    float CurrentTime = GetWorld()->GetTimeSeconds();

    for (const auto& Pair : Predictions)
    {
        const FFuturePrediction& Pred = Pair.Value;
        if (!Pred.bValidated && Pred.TargetTimestamp <= CurrentTime)
        {
            Result.Add(Pred);
        }
    }

    return Result;
}

FPredictionValidation UFutureStatePrediction::ValidatePrediction(int32 PredictionID, const TArray<float>& ActualState)
{
    FPredictionValidation Validation;
    Validation.PredictionID = PredictionID;
    Validation.ValidatedAt = GetWorld()->GetTimeSeconds();

    if (!Predictions.Contains(PredictionID))
    {
        return Validation;
    }

    FFuturePrediction& Prediction = Predictions[PredictionID];
    Prediction.bValidated = true;
    Prediction.ActualState = ActualState;

    // Compute errors
    Validation.MSE = ComputeMSE(Prediction.PredictedState, ActualState);
    Validation.MAE = ComputeMAE(Prediction.PredictedState, ActualState);
    Validation.Correlation = ComputeCorrelation(Prediction.PredictedState, ActualState);
    
    Prediction.PredictionError = Validation.MSE;

    // Check if within uncertainty bounds
    bool WithinBounds = true;
    for (int32 i = 0; i < FMath::Min(Prediction.PredictedState.Num(), ActualState.Num()); ++i)
    {
        float Diff = FMath::Abs(Prediction.PredictedState[i] - ActualState[i]);
        float Bound = (i < Prediction.UncertaintyBounds.Num()) ? Prediction.UncertaintyBounds[i] : 1.0f;
        if (Diff > Bound)
        {
            WithinBounds = false;
            break;
        }
    }
    Validation.bWithinBounds = WithinBounds;

    // Store validation
    ValidationHistory.Add(Validation);
    while (ValidationHistory.Num() > MaxPredictionHistory)
    {
        ValidationHistory.RemoveAt(0);
    }

    // Update statistics
    UpdateStatistics(Validation, Prediction);

    OnPredictionValidated.Broadcast(Validation);

    // Broadcast error if significant
    if (Validation.MSE > 0.1f)
    {
        float AvgPredicted = 0.0f, AvgActual = 0.0f;
        for (float V : Prediction.PredictedState) AvgPredicted += V;
        for (float V : ActualState) AvgActual += V;
        if (Prediction.PredictedState.Num() > 0) AvgPredicted /= Prediction.PredictedState.Num();
        if (ActualState.Num() > 0) AvgActual /= ActualState.Num();
        OnPredictionError.Broadcast(AvgPredicted, AvgActual);
    }

    return Validation;
}

TArray<FPredictionValidation> UFutureStatePrediction::AutoValidatePredictions(const TArray<float>& CurrentState)
{
    TArray<FPredictionValidation> Results;
    TArray<FFuturePrediction> Pending = GetPendingPredictions();

    for (const FFuturePrediction& Pred : Pending)
    {
        Results.Add(ValidatePrediction(Pred.PredictionID, CurrentState));
    }

    return Results;
}

TArray<FPredictionValidation> UFutureStatePrediction::GetValidationHistory(int32 Count) const
{
    TArray<FPredictionValidation> Result;
    int32 Start = FMath::Max(0, ValidationHistory.Num() - Count);
    for (int32 i = Start; i < ValidationHistory.Num(); ++i)
    {
        Result.Add(ValidationHistory[i]);
    }
    return Result;
}

FPredictionStatistics UFutureStatePrediction::GetPredictionStatistics() const
{
    return Statistics;
}

float UFutureStatePrediction::ComputePredictionConfidence(const FString& TrajectoryID, int32 StepsAhead) const
{
    if (!Trajectories.Contains(TrajectoryID))
    {
        return 0.0f;
    }

    const FStateTrajectory& Trajectory = Trajectories[TrajectoryID];

    // Base confidence from trajectory goodness of fit
    float Confidence = BaseConfidence * FMath::Clamp(Trajectory.GoodnessOfFit, 0.1f, 1.0f);

    // Decay confidence with prediction horizon
    float Decay = FMath::Exp(-ConfidenceDecayRate * StepsAhead);
    Confidence *= Decay;

    // Boost confidence with more observations
    float ObservationFactor = FMath::Clamp(static_cast<float>(Trajectory.Observations.Num()) / MaxTrajectoryLength, 0.2f, 1.0f);
    Confidence *= ObservationFactor;

    // Factor in recent validation performance for this method
    if (Statistics.MethodMSE.Contains(Trajectory.FittingType))
    {
        float MethodMSE = Statistics.MethodMSE[Trajectory.FittingType];
        float MethodFactor = FMath::Clamp(1.0f - MethodMSE, 0.2f, 1.0f);
        Confidence *= MethodFactor;
    }

    return FMath::Clamp(Confidence, 0.0f, 1.0f);
}

TArray<float> UFutureStatePrediction::ComputeUncertaintyBounds(const FString& TrajectoryID, int32 StepsAhead) const
{
    TArray<float> Bounds;

    if (!Trajectories.Contains(TrajectoryID))
    {
        return Bounds;
    }

    const FStateTrajectory& Trajectory = Trajectories[TrajectoryID];
    
    int32 Dim = StateVectorDimension;
    if (Trajectory.Observations.Num() > 0 && Trajectory.Observations[0].StateVector.Num() > 0)
    {
        Dim = Trajectory.Observations[0].StateVector.Num();
    }

    Bounds.SetNum(Dim);

    // Compute variance of each dimension
    for (int32 d = 0; d < Dim; ++d)
    {
        float Sum = 0.0f;
        float SumSq = 0.0f;
        int32 Count = 0;

        for (const FStateObservation& Obs : Trajectory.Observations)
        {
            if (d < Obs.StateVector.Num())
            {
                float Val = Obs.StateVector[d];
                Sum += Val;
                SumSq += Val * Val;
                Count++;
            }
        }

        if (Count > 1)
        {
            float Mean = Sum / Count;
            float Variance = (SumSq - Sum * Mean) / (Count - 1);
            float StdDev = FMath::Sqrt(FMath::Max(0.0f, Variance));
            
            // Expand bounds with prediction horizon
            float HorizonFactor = 1.0f + 0.1f * StepsAhead;
            Bounds[d] = StdDev * HorizonFactor * 2.0f;  // 2 sigma bounds
        }
        else
        {
            Bounds[d] = 1.0f;  // Default bound
        }
    }

    return Bounds;
}

EPredictionConfidence UFutureStatePrediction::ClassifyConfidence(float ConfidenceScore) const
{
    if (ConfidenceScore < 0.2f)
        return EPredictionConfidence::VeryLow;
    else if (ConfidenceScore < 0.4f)
        return EPredictionConfidence::Low;
    else if (ConfidenceScore < 0.6f)
        return EPredictionConfidence::Medium;
    else if (ConfidenceScore < 0.8f)
        return EPredictionConfidence::High;
    else
        return EPredictionConfidence::VeryHigh;
}

void UFutureStatePrediction::ProcessEchobeatStep(int32 Step)
{
    CurrentEchobeatStep = Step;

    // Update trajectories based on current state if sources available
    if (PredictiveEngine)
    {
        // Get current cognitive prediction as observation
        TArray<float> CognitiveState = PredictiveEngine->GetCurrentPrediction(EPredictionType::Cognitive);
        
        // Find or create cognitive trajectory
        TArray<FStateTrajectory> CognitiveTrajectories = GetTrajectoriesForDomain(EStateDomain::Cognitive);
        FString TrajectoryID;
        if (CognitiveTrajectories.Num() > 0)
        {
            TrajectoryID = CognitiveTrajectories[0].TrajectoryID;
        }
        else
        {
            TrajectoryID = CreateTrajectory(EStateDomain::Cognitive, TEXT("MainCognitive"));
        }

        if (CognitiveState.Num() > 0)
        {
            AddObservation(TrajectoryID, CognitiveState, 1.0f);
        }
    }
}

FFuturePrediction UFutureStatePrediction::GetEchobeatPhasePrediction(int32 Phase) const
{
    // Map phase to domain
    EStateDomain Domain;
    switch (Phase)
    {
    case 0:
        Domain = EStateDomain::Cognitive;
        break;
    case 1:
        Domain = EStateDomain::Motor;
        break;
    case 2:
        Domain = EStateDomain::Emotional;
        break;
    case 3:
    default:
        Domain = EStateDomain::Integrated;
        break;
    }

    TArray<FStateTrajectory> DomainTrajectories = GetTrajectoriesForDomain(Domain);
    if (DomainTrajectories.Num() > 0)
    {
        // Return most recent prediction for this domain
        for (const auto& Pair : Predictions)
        {
            if (Pair.Value.Domain == Domain && !Pair.Value.bValidated)
            {
                return Pair.Value;
            }
        }
    }

    return FFuturePrediction();
}

void UFutureStatePrediction::UpdateFromCognitiveState(const TArray<float>& CognitiveState)
{
    TArray<FStateTrajectory> CognitiveTrajectories = GetTrajectoriesForDomain(EStateDomain::Cognitive);
    
    if (CognitiveTrajectories.Num() == 0)
    {
        FString TrajectoryID = CreateTrajectory(EStateDomain::Cognitive, TEXT("MainCognitive"));
        AddObservation(TrajectoryID, CognitiveState, 1.0f);
    }
    else
    {
        AddObservation(CognitiveTrajectories[0].TrajectoryID, CognitiveState, 1.0f);
    }
}

ETrajectoryType UFutureStatePrediction::GetBestMethod(const FString& TrajectoryID) const
{
    // Return method with lowest MSE
    ETrajectoryType BestMethod = ETrajectoryType::Linear;
    float LowestMSE = FLT_MAX;

    for (const auto& Pair : Statistics.MethodMSE)
    {
        if (Pair.Value < LowestMSE)
        {
            LowestMSE = Pair.Value;
            BestMethod = Pair.Key;
        }
    }

    return BestMethod;
}

TArray<float> UFutureStatePrediction::GetMethodWeights() const
{
    TArray<float> Weights;
    Weights.Add(MethodWeights.Contains(ETrajectoryType::Linear) ? MethodWeights[ETrajectoryType::Linear] : 0.25f);
    Weights.Add(MethodWeights.Contains(ETrajectoryType::Polynomial) ? MethodWeights[ETrajectoryType::Polynomial] : 0.25f);
    Weights.Add(MethodWeights.Contains(ETrajectoryType::Exponential) ? MethodWeights[ETrajectoryType::Exponential] : 0.2f);
    Weights.Add(MethodWeights.Contains(ETrajectoryType::Reservoir) ? MethodWeights[ETrajectoryType::Reservoir] : 0.3f);
    return Weights;
}

void UFutureStatePrediction::UpdateMethodWeights()
{
    // Update weights based on recent validation performance
    if (Statistics.MethodMSE.Num() == 0)
    {
        return;
    }

    // Compute inverse MSE as weight
    float TotalInverseMSE = 0.0f;
    TMap<ETrajectoryType, float> InverseMSE;

    for (const auto& Pair : Statistics.MethodMSE)
    {
        float Inv = 1.0f / FMath::Max(0.01f, Pair.Value);
        InverseMSE.Add(Pair.Key, Inv);
        TotalInverseMSE += Inv;
    }

    // Normalize to get weights
    if (TotalInverseMSE > 1e-6f)
    {
        for (auto& Pair : InverseMSE)
        {
            MethodWeights.Add(Pair.Key, Pair.Value / TotalInverseMSE);
        }
    }

    Statistics.BestMethod = GetBestMethod(TEXT(""));
}

void UFutureStatePrediction::UpdatePendingValidations()
{
    // This would be called with actual current state from sensors
    // In practice, would need current state input
}

TArray<float> UFutureStatePrediction::PredictLinear(const FStateTrajectory& Trajectory, int32 StepsAhead) const
{
    TArray<float> Prediction;
    
    if (Trajectory.Observations.Num() < 2)
    {
        if (Trajectory.Observations.Num() == 1)
        {
            Prediction = Trajectory.Observations[0].StateVector;
        }
        return Prediction;
    }

    int32 Dim = Trajectory.Observations[0].StateVector.Num();
    Prediction.SetNum(Dim);

    float LastTime = Trajectory.Observations.Last().Timestamp;
    float AvgDelta = (LastTime - Trajectory.Observations[0].Timestamp) / (Trajectory.Observations.Num() - 1);
    float TargetTime = LastTime + AvgDelta * StepsAhead;
    float BaseTime = Trajectory.Observations[0].Timestamp;

    // Use fitted parameters if available
    if (Trajectory.FittedParameters.Num() >= Dim * 2)
    {
        for (int32 d = 0; d < Dim; ++d)
        {
            float Slope = Trajectory.FittedParameters[d * 2];
            float Intercept = Trajectory.FittedParameters[d * 2 + 1];
            Prediction[d] = Slope * (TargetTime - BaseTime) + Intercept;
        }
    }
    else
    {
        // Simple linear extrapolation from last two points
        const FStateObservation& Last = Trajectory.Observations.Last();
        const FStateObservation& SecondLast = Trajectory.Observations[Trajectory.Observations.Num() - 2];
        float TimeDelta = Last.Timestamp - SecondLast.Timestamp;
        
        for (int32 d = 0; d < Dim; ++d)
        {
            float LastVal = (d < Last.StateVector.Num()) ? Last.StateVector[d] : 0.0f;
            float PrevVal = (d < SecondLast.StateVector.Num()) ? SecondLast.StateVector[d] : 0.0f;
            float Slope = (TimeDelta > 1e-6f) ? (LastVal - PrevVal) / TimeDelta : 0.0f;
            Prediction[d] = LastVal + Slope * AvgDelta * StepsAhead;
        }
    }

    return Prediction;
}

TArray<float> UFutureStatePrediction::PredictPolynomial(const FStateTrajectory& Trajectory, int32 StepsAhead) const
{
    TArray<float> Prediction;
    
    if (Trajectory.Observations.Num() < 3)
    {
        return PredictLinear(Trajectory, StepsAhead);
    }

    int32 Dim = Trajectory.Observations[0].StateVector.Num();
    int32 Degree = FMath::Min(PolynomialDegree, 3);
    Prediction.SetNum(Dim);

    float LastTime = Trajectory.Observations.Last().Timestamp;
    float AvgDelta = (LastTime - Trajectory.Observations[0].Timestamp) / (Trajectory.Observations.Num() - 1);
    float T = AvgDelta * StepsAhead;  // Time from last observation
    float BaseTime = Trajectory.Observations[0].Timestamp;

    // Use fitted parameters if available
    if (Trajectory.FittedParameters.Num() >= Dim * (Degree + 1))
    {
        for (int32 d = 0; d < Dim; ++d)
        {
            float TotalT = (LastTime - BaseTime) + T;
            // y = a*t^2 + b*t + c (quadratic)
            float A = Trajectory.FittedParameters[d * (Degree + 1)];
            float B = Trajectory.FittedParameters[d * (Degree + 1) + 1];
            float C = Trajectory.FittedParameters[d * (Degree + 1) + 2];
            Prediction[d] = A * TotalT * TotalT + B * TotalT + C;
        }
    }
    else
    {
        // Fallback to linear
        return PredictLinear(Trajectory, StepsAhead);
    }

    return Prediction;
}

TArray<float> UFutureStatePrediction::PredictExponential(const FStateTrajectory& Trajectory, int32 StepsAhead) const
{
    TArray<float> Prediction;
    
    if (Trajectory.Observations.Num() < 2)
    {
        return PredictLinear(Trajectory, StepsAhead);
    }

    int32 Dim = Trajectory.Observations[0].StateVector.Num();
    Prediction.SetNum(Dim);

    float LastTime = Trajectory.Observations.Last().Timestamp;
    float AvgDelta = (LastTime - Trajectory.Observations[0].Timestamp) / (Trajectory.Observations.Num() - 1);
    float T = AvgDelta * StepsAhead;
    float BaseTime = Trajectory.Observations[0].Timestamp;

    // Use fitted parameters if available: y = a * exp(b * t) + c
    if (Trajectory.FittedParameters.Num() >= Dim * 3)
    {
        for (int32 d = 0; d < Dim; ++d)
        {
            float TotalT = (LastTime - BaseTime) + T;
            float A = Trajectory.FittedParameters[d * 3];
            float Rate = Trajectory.FittedParameters[d * 3 + 1];
            float C = Trajectory.FittedParameters[d * 3 + 2];
            
            // Clamp rate to prevent explosion
            Rate = FMath::Clamp(Rate, -2.0f, 2.0f);
            Prediction[d] = A * FMath::Exp(Rate * TotalT) + C;
        }
    }
    else
    {
        return PredictLinear(Trajectory, StepsAhead);
    }

    return Prediction;
}

TArray<float> UFutureStatePrediction::PredictReservoir(const FStateTrajectory& Trajectory, int32 StepsAhead) const
{
    // If reservoir component is available, use it
    if (Reservoir && Trajectory.Observations.Num() > 0)
    {
        // Would call reservoir for prediction
        // For now, use enhanced linear as fallback
    }

    // Enhanced linear with momentum
    if (Trajectory.Observations.Num() < 3)
    {
        return PredictLinear(Trajectory, StepsAhead);
    }

    int32 Dim = Trajectory.Observations[0].StateVector.Num();
    TArray<float> Prediction;
    Prediction.SetNum(Dim);

    int32 N = Trajectory.Observations.Num();
    float AvgDelta = (Trajectory.Observations.Last().Timestamp - Trajectory.Observations[0].Timestamp) / (N - 1);

    // Use exponential moving average of slopes
    for (int32 d = 0; d < Dim; ++d)
    {
        float Momentum = 0.0f;
        float Alpha = 0.3f;  // EMA decay

        for (int32 i = 1; i < N; ++i)
        {
            float Curr = (d < Trajectory.Observations[i].StateVector.Num()) ? 
                Trajectory.Observations[i].StateVector[d] : 0.0f;
            float Prev = (d < Trajectory.Observations[i-1].StateVector.Num()) ? 
                Trajectory.Observations[i-1].StateVector[d] : 0.0f;
            float TimeDelta = Trajectory.Observations[i].Timestamp - Trajectory.Observations[i-1].Timestamp;
            
            float Slope = (TimeDelta > 1e-6f) ? (Curr - Prev) / TimeDelta : 0.0f;
            Momentum = Alpha * Slope + (1.0f - Alpha) * Momentum;
        }

        float LastVal = (d < Trajectory.Observations.Last().StateVector.Num()) ?
            Trajectory.Observations.Last().StateVector[d] : 0.0f;
        Prediction[d] = LastVal + Momentum * AvgDelta * StepsAhead;
    }

    return Prediction;
}

float UFutureStatePrediction::ComputeGoodnessOfFit(const FStateTrajectory& Trajectory, const TArray<float>& FittedParams, ETrajectoryType Method) const
{
    if (Trajectory.Observations.Num() < 2)
    {
        return 0.0f;
    }

    // Compute R-squared
    int32 Dim = Trajectory.Observations[0].StateVector.Num();
    float TotalSS = 0.0f;
    float ResidualSS = 0.0f;

    // Compute mean per dimension
    TArray<float> Mean;
    Mean.SetNumZeroed(Dim);
    for (const FStateObservation& Obs : Trajectory.Observations)
    {
        for (int32 d = 0; d < FMath::Min(Dim, Obs.StateVector.Num()); ++d)
        {
            Mean[d] += Obs.StateVector[d];
        }
    }
    for (int32 d = 0; d < Dim; ++d)
    {
        Mean[d] /= Trajectory.Observations.Num();
    }

    // Compute SS
    float BaseTime = Trajectory.Observations[0].Timestamp;
    for (const FStateObservation& Obs : Trajectory.Observations)
    {
        float T = Obs.Timestamp - BaseTime;
        
        for (int32 d = 0; d < FMath::Min(Dim, Obs.StateVector.Num()); ++d)
        {
            float Actual = Obs.StateVector[d];
            float Predicted = 0.0f;

            // Compute predicted value based on method
            if (Method == ETrajectoryType::Linear && FittedParams.Num() >= (d + 1) * 2)
            {
                float Slope = FittedParams[d * 2];
                float Intercept = FittedParams[d * 2 + 1];
                Predicted = Slope * T + Intercept;
            }
            else
            {
                Predicted = Mean[d];
            }

            TotalSS += FMath::Square(Actual - Mean[d]);
            ResidualSS += FMath::Square(Actual - Predicted);
        }
    }

    if (TotalSS > 1e-6f)
    {
        return FMath::Clamp(1.0f - ResidualSS / TotalSS, 0.0f, 1.0f);
    }

    return 0.0f;
}

float UFutureStatePrediction::ComputeMSE(const TArray<float>& Predicted, const TArray<float>& Actual) const
{
    if (Predicted.Num() == 0 || Actual.Num() == 0)
    {
        return 0.0f;
    }

    float SumSqError = 0.0f;
    int32 N = FMath::Min(Predicted.Num(), Actual.Num());

    for (int32 i = 0; i < N; ++i)
    {
        SumSqError += FMath::Square(Predicted[i] - Actual[i]);
    }

    return SumSqError / N;
}

float UFutureStatePrediction::ComputeMAE(const TArray<float>& Predicted, const TArray<float>& Actual) const
{
    if (Predicted.Num() == 0 || Actual.Num() == 0)
    {
        return 0.0f;
    }

    float SumAbsError = 0.0f;
    int32 N = FMath::Min(Predicted.Num(), Actual.Num());

    for (int32 i = 0; i < N; ++i)
    {
        SumAbsError += FMath::Abs(Predicted[i] - Actual[i]);
    }

    return SumAbsError / N;
}

float UFutureStatePrediction::ComputeCorrelation(const TArray<float>& A, const TArray<float>& B) const
{
    if (A.Num() == 0 || B.Num() == 0)
    {
        return 0.0f;
    }

    int32 N = FMath::Min(A.Num(), B.Num());

    float SumA = 0.0f, SumB = 0.0f;
    for (int32 i = 0; i < N; ++i)
    {
        SumA += A[i];
        SumB += B[i];
    }
    float MeanA = SumA / N;
    float MeanB = SumB / N;

    float Numerator = 0.0f;
    float DenomA = 0.0f, DenomB = 0.0f;

    for (int32 i = 0; i < N; ++i)
    {
        float DA = A[i] - MeanA;
        float DB = B[i] - MeanB;
        Numerator += DA * DB;
        DenomA += DA * DA;
        DenomB += DB * DB;
    }

    float Denom = FMath::Sqrt(DenomA * DenomB);
    if (Denom > 1e-6f)
    {
        return Numerator / Denom;
    }

    return 0.0f;
}

EPredictionHorizon UFutureStatePrediction::ClassifyHorizon(int32 StepsAhead) const
{
    if (StepsAhead <= 2)
        return EPredictionHorizon::Immediate;
    else if (StepsAhead <= 5)
        return EPredictionHorizon::ShortTerm;
    else if (StepsAhead <= 12)
        return EPredictionHorizon::MediumTerm;
    else
        return EPredictionHorizon::LongTerm;
}

void UFutureStatePrediction::UpdateStatistics(const FPredictionValidation& Validation, const FFuturePrediction& Prediction)
{
    Statistics.ValidatedPredictions++;

    // Running average of MSE
    float Alpha = 1.0f / Statistics.ValidatedPredictions;
    Statistics.AverageMSE = (1.0f - Alpha) * Statistics.AverageMSE + Alpha * Validation.MSE;
    Statistics.AverageConfidence = (1.0f - Alpha) * Statistics.AverageConfidence + Alpha * Prediction.Confidence;

    // Update within bounds ratio
    float BoundsCount = Statistics.WithinBoundsRatio * (Statistics.ValidatedPredictions - 1);
    if (Validation.bWithinBounds) BoundsCount += 1.0f;
    Statistics.WithinBoundsRatio = BoundsCount / Statistics.ValidatedPredictions;

    // Update method-specific MSE
    if (!Statistics.MethodMSE.Contains(Prediction.PredictionMethod))
    {
        Statistics.MethodMSE.Add(Prediction.PredictionMethod, Validation.MSE);
    }
    else
    {
        float& MethodMSE = Statistics.MethodMSE[Prediction.PredictionMethod];
        MethodMSE = 0.9f * MethodMSE + 0.1f * Validation.MSE;  // EMA update
    }

    // Update horizon-specific accuracy
    float Accuracy = FMath::Clamp(1.0f - Validation.MSE, 0.0f, 1.0f);
    if (!Statistics.HorizonAccuracy.Contains(Prediction.Horizon))
    {
        Statistics.HorizonAccuracy.Add(Prediction.Horizon, Accuracy);
    }
    else
    {
        float& HorizonAcc = Statistics.HorizonAccuracy[Prediction.Horizon];
        HorizonAcc = 0.9f * HorizonAcc + 0.1f * Accuracy;
    }

    // Update method weights
    UpdateMethodWeights();
}

void UFutureStatePrediction::TrimPredictionHistory()
{
    if (Predictions.Num() > MaxPredictionHistory)
    {
        // Remove oldest validated predictions
        TArray<int32> ValidatedIDs;
        for (const auto& Pair : Predictions)
        {
            if (Pair.Value.bValidated)
            {
                ValidatedIDs.Add(Pair.Key);
            }
        }

        // Sort by creation time
        ValidatedIDs.Sort([this](int32 A, int32 B) {
            return Predictions[A].CreatedAt < Predictions[B].CreatedAt;
        });

        // Remove oldest
        int32 ToRemove = Predictions.Num() - MaxPredictionHistory;
        for (int32 i = 0; i < FMath::Min(ToRemove, ValidatedIDs.Num()); ++i)
        {
            Predictions.Remove(ValidatedIDs[i]);
        }
    }
}

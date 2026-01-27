/**
 * Readout Layer Training System Implementation
 * Feature F1.2.4 - Phase 1.2
 */

#include "ReadoutLayerTraining.h"
#include <cmath>
#include <algorithm>
#include <chrono>

UReadoutLayerTraining::UReadoutLayerTraining()
{
    PrimaryComponentTick.bCanEverTick = false;
    
    // Initialize default metrics
    CrossValidationConfig.Metrics = {
        TEXT("MSE"),
        TEXT("RMSE"),
        TEXT("MAE"),
        TEXT("RSquared")
    };
}

void UReadoutLayerTraining::BeginPlay()
{
    Super::BeginPlay();
    
    // Initialize multi-task configuration if enabled
    if (bEnableMultiTask && MultiTaskConfig.NumTasks > 0)
    {
        // Ensure task arrays are sized correctly
        if (MultiTaskConfig.TaskNames.Num() != MultiTaskConfig.NumTasks)
        {
            MultiTaskConfig.TaskNames.SetNum(MultiTaskConfig.NumTasks);
            for (int32 i = 0; i < MultiTaskConfig.NumTasks; ++i)
            {
                if (MultiTaskConfig.TaskNames[i].IsEmpty())
                {
                    MultiTaskConfig.TaskNames[i] = FString::Printf(TEXT("Task_%d"), i);
                }
            }
        }
        
        // Initialize regularization params and weights
        if (MultiTaskConfig.TaskRegularizationParams.Num() != MultiTaskConfig.NumTasks)
        {
            MultiTaskConfig.TaskRegularizationParams.SetNum(MultiTaskConfig.NumTasks);
            for (int32 i = 0; i < MultiTaskConfig.NumTasks; ++i)
            {
                MultiTaskConfig.TaskRegularizationParams[i] = RidgeConfig.RegularizationParameter;
            }
        }
        
        if (MultiTaskConfig.TaskWeights.Num() != MultiTaskConfig.NumTasks)
        {
            MultiTaskConfig.TaskWeights.Init(1.0f / MultiTaskConfig.NumTasks, MultiTaskConfig.NumTasks);
        }
        
        if (MultiTaskConfig.TaskOutputDimensions.Num() != MultiTaskConfig.NumTasks)
        {
            MultiTaskConfig.TaskOutputDimensions.Init(1, MultiTaskConfig.NumTasks);
        }
    }
}

// ============================================================================
// Ridge Regression Training
// ============================================================================

FReadoutTrainingMetrics UReadoutLayerTraining::TrainRidgeRegression(
    const TArray<TArray<float>>& ReservoirStates,
    const TArray<TArray<float>>& TargetOutputs)
{
    auto StartTime = std::chrono::high_resolution_clock::now();
    
    FReadoutTrainingMetrics Metrics;
    bIsTraining = true;
    
    if (ReservoirStates.Num() == 0 || TargetOutputs.Num() == 0 ||
        ReservoirStates.Num() != TargetOutputs.Num())
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid input dimensions for ridge regression"));
        bIsTraining = false;
        return Metrics;
    }
    
    Metrics.NumTrainingSamples = ReservoirStates.Num();
    TotalSamplesProcessed += Metrics.NumTrainingSamples;
    
    // Add bias if enabled
    TArray<TArray<float>> X = ReservoirStates;
    if (RidgeConfig.bUseBias)
    {
        for (int32 i = 0; i < X.Num(); ++i)
        {
            X[i].Insert(1.0f, 0); // Add bias term at beginning
        }
    }
    
    // Compute optimal regularization if auto-tuning enabled
    float Lambda = RidgeConfig.RegularizationParameter;
    if (bEnableAutoTuning)
    {
        Lambda = TuneRegularization(ReservoirStates, TargetOutputs);
        RidgeConfig.RegularizationParameter = Lambda;
    }
    
    // Solve ridge regression: W = (X^T X + lambda*I)^(-1) X^T Y
    SolveRidge(X, TargetOutputs, Lambda);
    
    // Compute predictions and metrics
    TArray<TArray<float>> Predictions = Predict(ReservoirStates);
    UpdateMetrics(Predictions, TargetOutputs, Metrics);
    
    Metrics.bConverged = true;
    Metrics.NumIterations = 1;
    
    auto EndTime = std::chrono::high_resolution_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);
    Metrics.TrainingTimeMs = Duration.count() / 1000.0f;
    
    LatestMetrics = Metrics;
    TrainingHistory.Add(Metrics);
    bIsTraining = false;
    
    return Metrics;
}

FReadoutTrainingMetrics UReadoutLayerTraining::UpdateRidgeRegression(
    const TArray<TArray<float>>& NewReservoirStates,
    const TArray<TArray<float>>& NewTargetOutputs)
{
    // For incremental ridge, we need to accumulate X^T X and Y^T X
    // This is a simplified implementation that retrains on all data
    // For true incremental learning, consider using RLS instead
    
    FReadoutTrainingMetrics Metrics;
    
    if (NewReservoirStates.Num() == 0 || NewTargetOutputs.Num() == 0)
    {
        return Metrics;
    }
    
    // In a full implementation, we would accumulate statistics
    // For now, just retrain (placeholder for true incremental update)
    Metrics = TrainRidgeRegression(NewReservoirStates, NewTargetOutputs);
    
    UE_LOG(LogTemp, Warning, TEXT("UpdateRidgeRegression: Using full retrain. Consider RLS for online learning."));
    
    return Metrics;
}

// ============================================================================
// Recursive Least Squares Training
// ============================================================================

FReadoutTrainingMetrics UReadoutLayerTraining::TrainRLSOnline(
    const TArray<float>& ReservoirState,
    const TArray<float>& TargetOutput)
{
    auto StartTime = std::chrono::high_resolution_clock::now();
    
    FReadoutTrainingMetrics Metrics;
    bIsTraining = true;
    
    if (ReservoirState.Num() == 0 || TargetOutput.Num() == 0)
    {
        bIsTraining = false;
        return Metrics;
    }
    
    // Initialize weights and P matrix if first call
    if (ReadoutWeights.Num() == 0)
    {
        int32 InputDim = ReservoirState.Num() + (RLSConfig.bUseBias ? 1 : 0);
        int32 OutputDim = TargetOutput.Num();
        
        ReadoutWeights.SetNum(OutputDim);
        for (int32 i = 0; i < OutputDim; ++i)
        {
            ReadoutWeights[i].Init(0.0f, InputDim);
        }
        
        BiasWeights.Init(0.0f, OutputDim);
        InitializeRLSMatrix(ReservoirState.Num());
    }
    
    // Add bias to state
    TArray<float> State = ReservoirState;
    if (RLSConfig.bUseBias)
    {
        State.Insert(1.0f, 0);
    }
    
    // Compute current prediction
    TArray<float> Prediction;
    Prediction.SetNum(TargetOutput.Num());
    for (int32 i = 0; i < TargetOutput.Num(); ++i)
    {
        float Sum = 0.0f;
        for (int32 j = 0; j < State.Num(); ++j)
        {
            Sum += ReadoutWeights[i][j] * State[j];
        }
        Prediction[i] = Sum;
    }
    
    // Compute error
    TArray<float> Error;
    Error.SetNum(TargetOutput.Num());
    for (int32 i = 0; i < TargetOutput.Num(); ++i)
    {
        Error[i] = TargetOutput[i] - Prediction[i];
    }
    
    // RLS update: k = P * r / (lambda + r^T * P * r)
    // P_new = (P - k * r^T * P) / lambda
    // W_new = W + k * e^T
    
    int32 StateDim = State.Num();
    TArray<float> k;
    k.SetNum(StateDim);
    
    // Compute P * r
    TArray<float> Pr;
    Pr.SetNum(StateDim);
    for (int32 i = 0; i < StateDim; ++i)
    {
        float Sum = 0.0f;
        for (int32 j = 0; j < StateDim; ++j)
        {
            Sum += RLS_P_Matrix[i][j] * State[j];
        }
        Pr[i] = Sum;
    }
    
    // Compute r^T * P * r
    float rPr = 0.0f;
    for (int32 i = 0; i < StateDim; ++i)
    {
        rPr += State[i] * Pr[i];
    }
    
    // Compute k
    float Lambda = RLSConfig.ForgettingFactor;
    float Denominator = Lambda * (Lambda + rPr);
    if (FMath::Abs(Denominator) > 1e-10f)
    {
        for (int32 i = 0; i < StateDim; ++i)
        {
            k[i] = Pr[i] / Denominator;
        }
    }
    
    // Update P matrix: P = (P - k * (P * r)^T) / lambda
    for (int32 i = 0; i < StateDim; ++i)
    {
        for (int32 j = 0; j < StateDim; ++j)
        {
            RLS_P_Matrix[i][j] = (RLS_P_Matrix[i][j] - k[i] * Pr[j]) / Lambda;
        }
    }
    
    // Update weights: W = W + k * e^T
    for (int32 i = 0; i < TargetOutput.Num(); ++i)
    {
        for (int32 j = 0; j < StateDim; ++j)
        {
            ReadoutWeights[i][j] += k[j] * Error[i];
        }
    }
    
    // Compute metrics
    float MSE = 0.0f;
    for (int32 i = 0; i < Error.Num(); ++i)
    {
        MSE += Error[i] * Error[i];
    }
    MSE /= Error.Num();
    
    Metrics.MSE = MSE;
    Metrics.RMSE = FMath::Sqrt(MSE);
    Metrics.MAE = 0.0f;
    for (int32 i = 0; i < Error.Num(); ++i)
    {
        Metrics.MAE += FMath::Abs(Error[i]);
    }
    Metrics.MAE /= Error.Num();
    
    Metrics.NumTrainingSamples = 1;
    Metrics.bConverged = MSE < RLSConfig.Alpha * 10.0f;
    Metrics.NumIterations = 1;
    
    TotalSamplesProcessed += 1;
    
    auto EndTime = std::chrono::high_resolution_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - StartTime);
    Metrics.TrainingTimeMs = Duration.count() / 1000.0f;
    
    LatestMetrics = Metrics;
    bIsTraining = false;
    
    // Adaptive forgetting factor update
    if (RLSConfig.bAdaptiveForgetting)
    {
        float ErrorMagnitude = MSE;
        float TargetError = RLSConfig.Alpha;
        
        if (ErrorMagnitude > TargetError)
        {
            // Increase forgetting (decrease lambda) when error is high
            RLSConfig.ForgettingFactor = FMath::Max(
                RLSConfig.MinForgettingFactor,
                RLSConfig.ForgettingFactor * RLSConfig.ForgettingAdaptationRate
            );
        }
        else
        {
            // Decrease forgetting (increase lambda) when error is low
            RLSConfig.ForgettingFactor = FMath::Min(
                RLSConfig.MaxForgettingFactor,
                RLSConfig.ForgettingFactor / RLSConfig.ForgettingAdaptationRate
            );
        }
    }
    
    return Metrics;
}

FReadoutTrainingMetrics UReadoutLayerTraining::TrainRLSBatch(
    const TArray<TArray<float>>& ReservoirStates,
    const TArray<TArray<float>>& TargetOutputs)
{
    FReadoutTrainingMetrics BatchMetrics;
    
    if (ReservoirStates.Num() != TargetOutputs.Num())
    {
        return BatchMetrics;
    }
    
    // Process each sample sequentially with RLS
    for (int32 i = 0; i < ReservoirStates.Num(); ++i)
    {
        FReadoutTrainingMetrics SampleMetrics = TrainRLSOnline(
            ReservoirStates[i],
            TargetOutputs[i]
        );
        
        // Accumulate metrics
        BatchMetrics.MSE += SampleMetrics.MSE;
        BatchMetrics.RMSE += SampleMetrics.RMSE;
        BatchMetrics.MAE += SampleMetrics.MAE;
        BatchMetrics.TrainingTimeMs += SampleMetrics.TrainingTimeMs;
    }
    
    // Average metrics
    int32 NumSamples = ReservoirStates.Num();
    BatchMetrics.MSE /= NumSamples;
    BatchMetrics.RMSE /= NumSamples;
    BatchMetrics.MAE /= NumSamples;
    BatchMetrics.NumTrainingSamples = NumSamples;
    BatchMetrics.bConverged = true;
    BatchMetrics.NumIterations = NumSamples;
    
    return BatchMetrics;
}

void UReadoutLayerTraining::ResetRLSState()
{
    ReadoutWeights.Empty();
    BiasWeights.Empty();
    RLS_P_Matrix.Empty();
    TotalSamplesProcessed = 0;
    
    UE_LOG(LogTemp, Log, TEXT("RLS state reset"));
}

// ============================================================================
// Multi-Task Learning
// ============================================================================

TMap<FString, FReadoutTrainingMetrics> UReadoutLayerTraining::TrainMultiTask(
    const TArray<TArray<float>>& ReservoirStates,
    const TMap<FString, TArray<TArray<float>>>& TaskOutputs)
{
    TMap<FString, FReadoutTrainingMetrics> TaskMetrics;
    
    if (ReservoirStates.Num() == 0)
    {
        return TaskMetrics;
    }
    
    // Train each task separately
    for (const auto& TaskPair : TaskOutputs)
    {
        const FString& TaskName = TaskPair.Key;
        const TArray<TArray<float>>& Outputs = TaskPair.Value;
        
        // Find task index for regularization parameter
        int32 TaskIndex = MultiTaskConfig.TaskNames.IndexOfByKey(TaskName);
        if (TaskIndex == INDEX_NONE)
        {
            TaskIndex = 0;
        }
        
        // Temporarily set regularization for this task
        float OriginalLambda = RidgeConfig.RegularizationParameter;
        if (TaskIndex < MultiTaskConfig.TaskRegularizationParams.Num())
        {
            RidgeConfig.RegularizationParameter = MultiTaskConfig.TaskRegularizationParams[TaskIndex];
        }
        
        // Train this task
        FReadoutTrainingMetrics Metrics = TrainRidgeRegression(ReservoirStates, Outputs);
        
        // Store task-specific weights
        MultiTaskReadoutWeights.Add(TaskName, ReadoutWeights);
        MultiTaskBiasWeights.Add(TaskName, BiasWeights);
        
        TaskMetrics.Add(TaskName, Metrics);
        
        // Restore original lambda
        RidgeConfig.RegularizationParameter = OriginalLambda;
    }
    
    return TaskMetrics;
}

TMap<FString, FReadoutTrainingMetrics> UReadoutLayerTraining::UpdateMultiTask(
    const TArray<TArray<float>>& ReservoirStates,
    const TMap<FString, TArray<TArray<float>>>& TaskOutputs)
{
    // For online multi-task learning, use RLS for each task
    TMap<FString, FReadoutTrainingMetrics> TaskMetrics;
    
    // This is a simplified implementation
    // Full implementation would maintain separate RLS state per task
    return TrainMultiTask(ReservoirStates, TaskOutputs);
}

// ============================================================================
// Regularization Tuning
// ============================================================================

float UReadoutLayerTraining::TuneRegularization(
    const TArray<TArray<float>>& ReservoirStates,
    const TArray<TArray<float>>& TargetOutputs)
{
    if (!bEnableAutoTuning)
    {
        return RidgeConfig.RegularizationParameter;
    }
    
    // Generate candidate regularization parameters (log scale)
    TArray<float> CandidateParams = LogSpace(
        TuningConfig.SearchRange.X,
        TuningConfig.SearchRange.Y,
        TuningConfig.NumSearchPoints
    );
    
    // Perform grid search with cross-validation
    TArray<float> CVScores = GridSearchRegularization(
        ReservoirStates,
        TargetOutputs,
        CandidateParams
    );
    
    // Find best parameter
    int32 BestIndex = 0;
    float BestScore = CVScores[0];
    for (int32 i = 1; i < CVScores.Num(); ++i)
    {
        if (CVScores[i] < BestScore)  // Lower is better for MSE
        {
            BestScore = CVScores[i];
            BestIndex = i;
        }
    }
    
    float OptimalLambda = CandidateParams[BestIndex];
    CVResults.OptimalRegularization = OptimalLambda;
    
    UE_LOG(LogTemp, Log, TEXT("Optimal regularization parameter: %f (MSE: %f)"), 
        OptimalLambda, BestScore);
    
    return OptimalLambda;
}

TArray<float> UReadoutLayerTraining::GridSearchRegularization(
    const TArray<TArray<float>>& ReservoirStates,
    const TArray<TArray<float>>& TargetOutputs,
    const TArray<float>& ParameterValues)
{
    TArray<float> Scores;
    Scores.SetNum(ParameterValues.Num());
    
    float OriginalLambda = RidgeConfig.RegularizationParameter;
    bool OriginalCVFlag = bEnableCrossValidation;
    bEnableCrossValidation = false;  // Disable nested CV
    
    for (int32 i = 0; i < ParameterValues.Num(); ++i)
    {
        RidgeConfig.RegularizationParameter = ParameterValues[i];
        
        // Perform cross-validation for this parameter
        FCrossValidationResults CVRes = PerformCrossValidation(
            ReservoirStates,
            TargetOutputs
        );
        
        Scores[i] = CVRes.MeanMetrics.MSE;
    }
    
    RidgeConfig.RegularizationParameter = OriginalLambda;
    bEnableCrossValidation = OriginalCVFlag;
    
    return Scores;
}

// ============================================================================
// Cross-Validation
// ============================================================================

FCrossValidationResults UReadoutLayerTraining::PerformCrossValidation(
    const TArray<TArray<float>>& ReservoirStates,
    const TArray<TArray<float>>& TargetOutputs)
{
    FCrossValidationResults Results;
    
    switch (CrossValidationConfig.Strategy)
    {
        case ECrossValidationStrategy::KFold:
            Results = KFoldCrossValidation(
                ReservoirStates,
                TargetOutputs,
                CrossValidationConfig.NumFolds
            );
            break;
            
        case ECrossValidationStrategy::TimeSeriesSplit:
            Results = TimeSeriesCrossValidation(ReservoirStates, TargetOutputs);
            break;
            
        default:
            Results = KFoldCrossValidation(
                ReservoirStates,
                TargetOutputs,
                CrossValidationConfig.NumFolds
            );
            break;
    }
    
    CVResults = Results;
    return Results;
}

FCrossValidationResults UReadoutLayerTraining::KFoldCrossValidation(
    const TArray<TArray<float>>& ReservoirStates,
    const TArray<TArray<float>>& TargetOutputs,
    int32 NumFolds)
{
    FCrossValidationResults Results;
    Results.FoldMetrics.SetNum(NumFolds);
    
    int32 NumSamples = ReservoirStates.Num();
    int32 FoldSize = NumSamples / NumFolds;
    
    // Accumulate metrics across folds
    FReadoutTrainingMetrics SumMetrics;
    FReadoutTrainingMetrics SumSqMetrics;
    
    for (int32 Fold = 0; Fold < NumFolds; ++Fold)
    {
        TArray<TArray<float>> TrainStates, TrainTargets;
        TArray<TArray<float>> TestStates, TestTargets;
        
        SplitData(
            ReservoirStates, TargetOutputs,
            Fold, NumFolds,
            TrainStates, TrainTargets,
            TestStates, TestTargets
        );
        
        // Train on training fold
        bool OriginalAutoTune = bEnableAutoTuning;
        bEnableAutoTuning = false;  // Disable nested tuning
        
        TrainRidgeRegression(TrainStates, TrainTargets);
        
        bEnableAutoTuning = OriginalAutoTune;
        
        // Test on validation fold
        TArray<TArray<float>> Predictions = Predict(TestStates);
        FReadoutTrainingMetrics FoldMetrics = ComputeMetrics(Predictions, TestTargets);
        
        Results.FoldMetrics[Fold] = FoldMetrics;
        
        // Accumulate for mean and std
        SumMetrics.MSE += FoldMetrics.MSE;
        SumMetrics.RMSE += FoldMetrics.RMSE;
        SumMetrics.MAE += FoldMetrics.MAE;
        SumMetrics.RSquared += FoldMetrics.RSquared;
        
        SumSqMetrics.MSE += FoldMetrics.MSE * FoldMetrics.MSE;
        SumSqMetrics.RMSE += FoldMetrics.RMSE * FoldMetrics.RMSE;
        SumSqMetrics.MAE += FoldMetrics.MAE * FoldMetrics.MAE;
        SumSqMetrics.RSquared += FoldMetrics.RSquared * FoldMetrics.RSquared;
    }
    
    // Compute mean
    Results.MeanMetrics.MSE = SumMetrics.MSE / NumFolds;
    Results.MeanMetrics.RMSE = SumMetrics.RMSE / NumFolds;
    Results.MeanMetrics.MAE = SumMetrics.MAE / NumFolds;
    Results.MeanMetrics.RSquared = SumMetrics.RSquared / NumFolds;
    
    // Compute standard deviation
    Results.StdMetrics.MSE = FMath::Sqrt(
        (SumSqMetrics.MSE / NumFolds) - (Results.MeanMetrics.MSE * Results.MeanMetrics.MSE)
    );
    Results.StdMetrics.RMSE = FMath::Sqrt(
        (SumSqMetrics.RMSE / NumFolds) - (Results.MeanMetrics.RMSE * Results.MeanMetrics.RMSE)
    );
    Results.StdMetrics.MAE = FMath::Sqrt(
        (SumSqMetrics.MAE / NumFolds) - (Results.MeanMetrics.MAE * Results.MeanMetrics.MAE)
    );
    Results.StdMetrics.RSquared = FMath::Sqrt(
        (SumSqMetrics.RSquared / NumFolds) - (Results.MeanMetrics.RSquared * Results.MeanMetrics.RSquared)
    );
    
    // Find best fold
    float BestMSE = Results.FoldMetrics[0].MSE;
    Results.BestFoldIndex = 0;
    for (int32 i = 1; i < NumFolds; ++i)
    {
        if (Results.FoldMetrics[i].MSE < BestMSE)
        {
            BestMSE = Results.FoldMetrics[i].MSE;
            Results.BestFoldIndex = i;
        }
    }
    
    return Results;
}

FCrossValidationResults UReadoutLayerTraining::TimeSeriesCrossValidation(
    const TArray<TArray<float>>& ReservoirStates,
    const TArray<TArray<float>>& TargetOutputs)
{
    FCrossValidationResults Results;
    
    int32 NumSamples = ReservoirStates.Num();
    int32 NumFolds = CrossValidationConfig.NumFolds;
    Results.FoldMetrics.SetNum(NumFolds);
    
    // For time series, use expanding window
    int32 MinTrainSize = NumSamples / (NumFolds + 1);
    
    FReadoutTrainingMetrics SumMetrics;
    FReadoutTrainingMetrics SumSqMetrics;
    
    for (int32 Fold = 0; Fold < NumFolds; ++Fold)
    {
        int32 TrainSize = MinTrainSize * (Fold + 1);
        int32 TestSize = FMath::Min(MinTrainSize, NumSamples - TrainSize);
        
        if (TestSize <= 0) continue;
        
        // Split data temporally
        TArray<TArray<float>> TrainStates, TrainTargets;
        TArray<TArray<float>> TestStates, TestTargets;
        
        TrainStates.Append(ReservoirStates.GetData(), TrainSize);
        TrainTargets.Append(TargetOutputs.GetData(), TrainSize);
        
        TestStates.Append(ReservoirStates.GetData() + TrainSize, TestSize);
        TestTargets.Append(TargetOutputs.GetData() + TrainSize, TestSize);
        
        // Train and test
        bool OriginalAutoTune = bEnableAutoTuning;
        bEnableAutoTuning = false;
        
        TrainRidgeRegression(TrainStates, TrainTargets);
        
        bEnableAutoTuning = OriginalAutoTune;
        
        TArray<TArray<float>> Predictions = Predict(TestStates);
        FReadoutTrainingMetrics FoldMetrics = ComputeMetrics(Predictions, TestTargets);
        
        Results.FoldMetrics[Fold] = FoldMetrics;
        
        SumMetrics.MSE += FoldMetrics.MSE;
        SumMetrics.RMSE += FoldMetrics.RMSE;
        SumMetrics.MAE += FoldMetrics.MAE;
        SumMetrics.RSquared += FoldMetrics.RSquared;
        
        SumSqMetrics.MSE += FoldMetrics.MSE * FoldMetrics.MSE;
        SumSqMetrics.RMSE += FoldMetrics.RMSE * FoldMetrics.RMSE;
        SumSqMetrics.MAE += FoldMetrics.MAE * FoldMetrics.MAE;
        SumSqMetrics.RSquared += FoldMetrics.RSquared * FoldMetrics.RSquared;
    }
    
    // Compute statistics
    Results.MeanMetrics.MSE = SumMetrics.MSE / NumFolds;
    Results.MeanMetrics.RMSE = SumMetrics.RMSE / NumFolds;
    Results.MeanMetrics.MAE = SumMetrics.MAE / NumFolds;
    Results.MeanMetrics.RSquared = SumMetrics.RSquared / NumFolds;
    
    Results.StdMetrics.MSE = FMath::Sqrt(
        (SumSqMetrics.MSE / NumFolds) - (Results.MeanMetrics.MSE * Results.MeanMetrics.MSE)
    );
    Results.StdMetrics.RMSE = FMath::Sqrt(
        (SumSqMetrics.RMSE / NumFolds) - (Results.MeanMetrics.RMSE * Results.MeanMetrics.RMSE)
    );
    Results.StdMetrics.MAE = FMath::Sqrt(
        (SumSqMetrics.MAE / NumFolds) - (Results.MeanMetrics.MAE * Results.MeanMetrics.MAE)
    );
    Results.StdMetrics.RSquared = FMath::Sqrt(
        (SumSqMetrics.RSquared / NumFolds) - (Results.MeanMetrics.RSquared * Results.MeanMetrics.RSquared)
    );
    
    return Results;
}

// ============================================================================
// Metrics and Evaluation
// ============================================================================

FReadoutTrainingMetrics UReadoutLayerTraining::ComputeMetrics(
    const TArray<TArray<float>>& Predictions,
    const TArray<TArray<float>>& Targets)
{
    FReadoutTrainingMetrics Metrics;
    
    if (Predictions.Num() != Targets.Num() || Predictions.Num() == 0)
    {
        return Metrics;
    }
    
    int32 NumSamples = Predictions.Num();
    int32 OutputDim = Predictions[0].Num();
    
    float SumSquaredError = 0.0f;
    float SumAbsError = 0.0f;
    float SumTargets = 0.0f;
    float SumSquaredTargets = 0.0f;
    
    for (int32 i = 0; i < NumSamples; ++i)
    {
        for (int32 j = 0; j < OutputDim; ++j)
        {
            float Error = Targets[i][j] - Predictions[i][j];
            SumSquaredError += Error * Error;
            SumAbsError += FMath::Abs(Error);
            
            SumTargets += Targets[i][j];
            SumSquaredTargets += Targets[i][j] * Targets[i][j];
        }
    }
    
    int32 TotalElements = NumSamples * OutputDim;
    
    Metrics.MSE = SumSquaredError / TotalElements;
    Metrics.RMSE = FMath::Sqrt(Metrics.MSE);
    Metrics.MAE = SumAbsError / TotalElements;
    
    // Compute R-squared
    float MeanTarget = SumTargets / TotalElements;
    float TotalSumSquares = 0.0f;
    
    for (int32 i = 0; i < NumSamples; ++i)
    {
        for (int32 j = 0; j < OutputDim; ++j)
        {
            float Deviation = Targets[i][j] - MeanTarget;
            TotalSumSquares += Deviation * Deviation;
        }
    }
    
    if (TotalSumSquares > 1e-10f)
    {
        Metrics.RSquared = 1.0f - (SumSquaredError / TotalSumSquares);
    }
    else
    {
        Metrics.RSquared = 0.0f;
    }
    
    Metrics.NumTrainingSamples = NumSamples;
    
    return Metrics;
}

TArray<TArray<float>> UReadoutLayerTraining::GetReadoutWeights() const
{
    return ReadoutWeights;
}

void UReadoutLayerTraining::SetReadoutWeights(const TArray<TArray<float>>& Weights)
{
    ReadoutWeights = Weights;
}

// ============================================================================
// Internal Helper Functions
// ============================================================================

void UReadoutLayerTraining::InitializeRLSMatrix(int32 ReservoirSize)
{
    int32 InputDim = ReservoirSize + (RLSConfig.bUseBias ? 1 : 0);
    
    RLS_P_Matrix.SetNum(InputDim);
    for (int32 i = 0; i < InputDim; ++i)
    {
        RLS_P_Matrix[i].Init(0.0f, InputDim);
        RLS_P_Matrix[i][i] = 1.0f / RLSConfig.Alpha;  // Initialize as scaled identity
    }
}

void UReadoutLayerTraining::SolveRidge(
    const TArray<TArray<float>>& X,
    const TArray<TArray<float>>& Y,
    float Lambda)
{
    if (X.Num() == 0 || Y.Num() == 0)
    {
        return;
    }
    
    // Simplified ridge regression using normal equations
    // W = (X^T X + lambda*I)^(-1) X^T Y
    
    int32 NumSamples = X.Num();
    int32 InputDim = X[0].Num();
    int32 OutputDim = Y[0].Num();
    
    // Compute X^T X
    TArray<TArray<float>> XTX;
    XTX.SetNum(InputDim);
    for (int32 i = 0; i < InputDim; ++i)
    {
        XTX[i].Init(0.0f, InputDim);
        for (int32 j = 0; j < InputDim; ++j)
        {
            float Sum = 0.0f;
            for (int32 k = 0; k < NumSamples; ++k)
            {
                Sum += X[k][i] * X[k][j];
            }
            XTX[i][j] = Sum;
        }
    }
    
    // Add lambda*I
    for (int32 i = 0; i < InputDim; ++i)
    {
        XTX[i][i] += Lambda;
    }
    
    // Compute X^T Y
    TArray<TArray<float>> XTY;
    XTY.SetNum(InputDim);
    for (int32 i = 0; i < InputDim; ++i)
    {
        XTY[i].Init(0.0f, OutputDim);
        for (int32 j = 0; j < OutputDim; ++j)
        {
            float Sum = 0.0f;
            for (int32 k = 0; k < NumSamples; ++k)
            {
                Sum += X[k][i] * Y[k][j];
            }
            XTY[i][j] = Sum;
        }
    }
    
    // Solve using Cholesky decomposition (simplified - assumes positive definite)
    // For production, use proper linear solver from Eigen or similar library
    TArray<TArray<float>> XTX_inv = MatrixInvert(XTX);
    
    // W = XTX_inv * XTY
    ReadoutWeights.SetNum(OutputDim);
    for (int32 i = 0; i < OutputDim; ++i)
    {
        ReadoutWeights[i].Init(0.0f, InputDim);
        for (int32 j = 0; j < InputDim; ++j)
        {
            float Sum = 0.0f;
            for (int32 k = 0; k < InputDim; ++k)
            {
                Sum += XTX_inv[j][k] * XTY[k][i];
            }
            ReadoutWeights[i][j] = Sum;
        }
    }
    
    // Extract bias if used
    if (RidgeConfig.bUseBias && InputDim > 0)
    {
        BiasWeights.SetNum(OutputDim);
        for (int32 i = 0; i < OutputDim; ++i)
        {
            BiasWeights[i] = ReadoutWeights[i][0];
            // Remove bias from weights
            ReadoutWeights[i].RemoveAt(0);
        }
    }
}

TArray<TArray<float>> UReadoutLayerTraining::MatrixInvert(const TArray<TArray<float>>& A) const
{
    // Simplified matrix inversion using Gauss-Jordan elimination
    // For production use, integrate with Eigen library for robust numerical methods
    
    int32 N = A.Num();
    if (N == 0 || A[0].Num() != N)
    {
        return TArray<TArray<float>>();
    }
    
    // Create augmented matrix [A | I]
    TArray<TArray<float>> Aug;
    Aug.SetNum(N);
    for (int32 i = 0; i < N; ++i)
    {
        Aug[i].SetNum(2 * N);
        for (int32 j = 0; j < N; ++j)
        {
            Aug[i][j] = A[i][j];
            Aug[i][N + j] = (i == j) ? 1.0f : 0.0f;
        }
    }
    
    // Forward elimination with partial pivoting
    for (int32 i = 0; i < N; ++i)
    {
        // Find pivot
        int32 MaxRow = i;
        float MaxVal = FMath::Abs(Aug[i][i]);
        for (int32 k = i + 1; k < N; ++k)
        {
            if (FMath::Abs(Aug[k][i]) > MaxVal)
            {
                MaxVal = FMath::Abs(Aug[k][i]);
                MaxRow = k;
            }
        }
        
        // Swap rows
        if (MaxRow != i)
        {
            Aug.Swap(i, MaxRow);
        }
        
        // Check for singular matrix
        if (FMath::Abs(Aug[i][i]) < 1e-10f)
        {
            UE_LOG(LogTemp, Error, TEXT("Matrix is singular or nearly singular"));
            return TArray<TArray<float>>();
        }
        
        // Scale pivot row
        float Pivot = Aug[i][i];
        for (int32 j = 0; j < 2 * N; ++j)
        {
            Aug[i][j] /= Pivot;
        }
        
        // Eliminate column
        for (int32 k = 0; k < N; ++k)
        {
            if (k != i)
            {
                float Factor = Aug[k][i];
                for (int32 j = 0; j < 2 * N; ++j)
                {
                    Aug[k][j] -= Factor * Aug[i][j];
                }
            }
        }
    }
    
    // Extract inverse from augmented matrix
    TArray<TArray<float>> Inv;
    Inv.SetNum(N);
    for (int32 i = 0; i < N; ++i)
    {
        Inv[i].SetNum(N);
        for (int32 j = 0; j < N; ++j)
        {
            Inv[i][j] = Aug[i][N + j];
        }
    }
    
    return Inv;
}

TArray<TArray<float>> UReadoutLayerTraining::Predict(const TArray<TArray<float>>& ReservoirStates) const
{
    TArray<TArray<float>> Predictions;
    
    if (ReservoirStates.Num() == 0 || ReadoutWeights.Num() == 0)
    {
        return Predictions;
    }
    
    int32 NumSamples = ReservoirStates.Num();
    int32 OutputDim = ReadoutWeights.Num();
    
    Predictions.SetNum(NumSamples);
    
    for (int32 i = 0; i < NumSamples; ++i)
    {
        Predictions[i].SetNum(OutputDim);
        
        for (int32 j = 0; j < OutputDim; ++j)
        {
            float Sum = 0.0f;
            
            // Compute weighted sum
            for (int32 k = 0; k < ReservoirStates[i].Num(); ++k)
            {
                if (k < ReadoutWeights[j].Num())
                {
                    Sum += ReadoutWeights[j][k] * ReservoirStates[i][k];
                }
            }
            
            // Add bias if present
            if (RidgeConfig.bUseBias && j < BiasWeights.Num())
            {
                Sum += BiasWeights[j];
            }
            
            Predictions[i][j] = Sum;
        }
    }
    
    return Predictions;
}

void UReadoutLayerTraining::UpdateMetrics(
    const TArray<TArray<float>>& Predictions,
    const TArray<TArray<float>>& Targets,
    FReadoutTrainingMetrics& Metrics)
{
    FReadoutTrainingMetrics ComputedMetrics = ComputeMetrics(Predictions, Targets);
    
    Metrics.MSE = ComputedMetrics.MSE;
    Metrics.RMSE = ComputedMetrics.RMSE;
    Metrics.MAE = ComputedMetrics.MAE;
    Metrics.RSquared = ComputedMetrics.RSquared;
}

void UReadoutLayerTraining::SplitData(
    const TArray<TArray<float>>& ReservoirStates,
    const TArray<TArray<float>>& TargetOutputs,
    int32 FoldIndex,
    int32 NumFolds,
    TArray<TArray<float>>& TrainStates,
    TArray<TArray<float>>& TrainTargets,
    TArray<TArray<float>>& TestStates,
    TArray<TArray<float>>& TestTargets)
{
    int32 NumSamples = ReservoirStates.Num();
    int32 FoldSize = NumSamples / NumFolds;
    int32 TestStart = FoldIndex * FoldSize;
    int32 TestEnd = (FoldIndex == NumFolds - 1) ? NumSamples : (FoldIndex + 1) * FoldSize;
    
    // Clear output arrays
    TrainStates.Empty();
    TrainTargets.Empty();
    TestStates.Empty();
    TestTargets.Empty();
    
    // Split data
    for (int32 i = 0; i < NumSamples; ++i)
    {
        if (i >= TestStart && i < TestEnd)
        {
            TestStates.Add(ReservoirStates[i]);
            TestTargets.Add(TargetOutputs[i]);
        }
        else
        {
            TrainStates.Add(ReservoirStates[i]);
            TrainTargets.Add(TargetOutputs[i]);
        }
    }
}

TArray<float> UReadoutLayerTraining::LogSpace(float Start, float End, int32 NumPoints) const
{
    TArray<float> Values;
    Values.SetNum(NumPoints);
    
    if (NumPoints == 1)
    {
        Values[0] = FMath::Pow(10.0f, (Start + End) / 2.0f);
        return Values;
    }
    
    float Step = (End - Start) / (NumPoints - 1);
    
    for (int32 i = 0; i < NumPoints; ++i)
    {
        float Exponent = Start + i * Step;
        Values[i] = FMath::Pow(10.0f, Exponent);
    }
    
    return Values;
}

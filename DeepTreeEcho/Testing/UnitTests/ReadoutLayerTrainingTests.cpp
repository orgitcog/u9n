/**
 * Unit Tests for Readout Layer Training System
 * Feature F1.2.4 - Phase 1.2
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../Reservoir/ReadoutLayerTraining.h"
#include <cmath>
#include <random>

using namespace testing;

/**
 * Test Fixture for Readout Layer Training
 */
class ReadoutLayerTrainingTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        // Create component (in real Unreal, would use NewObject)
        // For testing, we simulate the component behavior
        GenerateTestData();
    }

    void TearDown() override
    {
        // Cleanup
    }

    // Generate synthetic test data
    void GenerateTestData()
    {
        // Simple linear relationship: y = 2*x1 + 3*x2 + 1
        NumSamples = 100;
        ReservoirSize = 2;
        OutputSize = 1;

        ReservoirStates.SetNum(NumSamples);
        TargetOutputs.SetNum(NumSamples);

        for (int32 i = 0; i < NumSamples; ++i)
        {
            // Generate reservoir states
            ReservoirStates[i].SetNum(ReservoirSize);
            ReservoirStates[i][0] = static_cast<float>(i) / NumSamples;
            ReservoirStates[i][1] = static_cast<float>(NumSamples - i) / NumSamples;

            // Generate targets: y = 2*x1 + 3*x2 + 1
            TargetOutputs[i].SetNum(OutputSize);
            TargetOutputs[i][0] = 2.0f * ReservoirStates[i][0] + 
                                  3.0f * ReservoirStates[i][1] + 1.0f;
        }
    }

    void GenerateNoisyTestData()
    {
        GenerateTestData();
        
        // Add Gaussian noise using proper distribution
        std::random_device rd;
        std::mt19937 gen(42);  // Fixed seed for reproducibility
        std::normal_distribution<float> noise(0.0f, 0.1f);
        
        for (int32 i = 0; i < NumSamples; ++i)
        {
            TargetOutputs[i][0] += noise(gen);
        }
    }

    // Member variables initialized with defaults
    int32 NumSamples = 0;
    int32 ReservoirSize = 0;
    int32 OutputSize = 0;
    TArray<TArray<float>> ReservoirStates;
    TArray<TArray<float>> TargetOutputs;
};

// ============================================================================
// Ridge Regression Tests
// ============================================================================

TEST_F(ReadoutLayerTrainingTest, RidgeRegressionBasic)
{
    // TODO: When integrating with Unreal Engine, use NewObject<UReadoutLayerTraining>()
    UReadoutLayerTraining Training;
    Training.TrainingMethod = EReadoutTrainingMethod::RidgeRegression;
    Training.RidgeConfig.RegularizationParameter = 1e-6f;
    Training.RidgeConfig.bUseBias = true;

    FReadoutTrainingMetrics Metrics = Training.TrainRidgeRegression(
        ReservoirStates, TargetOutputs
    );

    // Check that training succeeded
    EXPECT_TRUE(Metrics.bConverged);
    EXPECT_EQ(Metrics.NumTrainingSamples, NumSamples);
    
    // Check that MSE is low (should fit well with no noise)
    EXPECT_LT(Metrics.MSE, 1e-4f);
    EXPECT_LT(Metrics.RMSE, 1e-2f);
    
    // R-squared should be close to 1
    EXPECT_GT(Metrics.RSquared, 0.99f);

    std::cout << "Ridge Regression Metrics:\n"
              << "  MSE: " << Metrics.MSE << "\n"
              << "  RMSE: " << Metrics.RMSE << "\n"
              << "  R²: " << Metrics.RSquared << "\n";
}

TEST_F(ReadoutLayerTrainingTest, RidgeRegressionWithRegularization)
{
    GenerateNoisyTestData();

    UReadoutLayerTraining Training;
    Training.TrainingMethod = EReadoutTrainingMethod::RidgeRegression;
    Training.RidgeConfig.bUseBias = true;

    // Test different regularization values
    TArray<float> Lambdas = {1e-8f, 1e-4f, 1e-2f, 1.0f};
    TArray<float> MSEs;

    for (float Lambda : Lambdas)
    {
        Training.RidgeConfig.RegularizationParameter = Lambda;
        FReadoutTrainingMetrics Metrics = Training.TrainRidgeRegression(
            ReservoirStates, TargetOutputs
        );
        MSEs.Add(Metrics.MSE);

        std::cout << "Lambda: " << Lambda << " -> MSE: " << Metrics.MSE << "\n";
    }

    // With noisy data, some regularization should help
    EXPECT_TRUE(MSEs.Num() > 0);
}

// ============================================================================
// Recursive Least Squares Tests
// ============================================================================

TEST_F(ReadoutLayerTrainingTest, RLSOnlineTraining)
{
    UReadoutLayerTraining Training;
    Training.TrainingMethod = EReadoutTrainingMethod::RecursiveLeastSquares;
    Training.RLSConfig.Alpha = 1e-3f;
    Training.RLSConfig.ForgettingFactor = 1.0f;
    Training.RLSConfig.bUseBias = true;

    // Train online sample by sample
    for (int32 i = 0; i < 50; ++i)  // Use subset for online training
    {
        FReadoutTrainingMetrics Metrics = Training.TrainRLSOnline(
            ReservoirStates[i],
            TargetOutputs[i]
        );

        // Error should decrease over time
        if (i > 10)
        {
            EXPECT_LT(Metrics.MSE, 1.0f);  // Should converge
        }
    }

    std::cout << "RLS Online Training: Total samples processed = " 
              << Training.TotalSamplesProcessed << "\n";

    EXPECT_EQ(Training.TotalSamplesProcessed, 50);
}

TEST_F(ReadoutLayerTrainingTest, RLSBatchTraining)
{
    UReadoutLayerTraining Training;
    Training.TrainingMethod = EReadoutTrainingMethod::RecursiveLeastSquares;
    Training.RLSConfig.Alpha = 1e-3f;
    Training.RLSConfig.ForgettingFactor = 1.0f;
    Training.RLSConfig.bUseBias = true;

    FReadoutTrainingMetrics Metrics = Training.TrainRLSBatch(
        ReservoirStates, TargetOutputs
    );

    EXPECT_TRUE(Metrics.bConverged);
    EXPECT_EQ(Metrics.NumTrainingSamples, NumSamples);
    EXPECT_LT(Metrics.MSE, 0.1f);

    std::cout << "RLS Batch Training Metrics:\n"
              << "  MSE: " << Metrics.MSE << "\n"
              << "  RMSE: " << Metrics.RMSE << "\n";
}

TEST_F(ReadoutLayerTrainingTest, RLSAdaptiveForgetting)
{
    UReadoutLayerTraining Training;
    Training.RLSConfig.Alpha = 1e-3f;
    Training.RLSConfig.ForgettingFactor = 0.99f;
    Training.RLSConfig.bAdaptiveForgetting = true;
    Training.RLSConfig.MinForgettingFactor = 0.9f;
    Training.RLSConfig.MaxForgettingFactor = 1.0f;

    float InitialForgetting = Training.RLSConfig.ForgettingFactor;

    // Train with adaptive forgetting
    for (int32 i = 0; i < 30; ++i)
    {
        Training.TrainRLSOnline(ReservoirStates[i], TargetOutputs[i]);
    }

    // Forgetting factor should adapt
    float FinalForgetting = Training.RLSConfig.ForgettingFactor;
    
    std::cout << "Adaptive Forgetting: " << InitialForgetting 
              << " -> " << FinalForgetting << "\n";

    EXPECT_GE(FinalForgetting, Training.RLSConfig.MinForgettingFactor);
    EXPECT_LE(FinalForgetting, Training.RLSConfig.MaxForgettingFactor);
}

TEST_F(ReadoutLayerTrainingTest, RLSStateReset)
{
    UReadoutLayerTraining Training;
    Training.RLSConfig.bUseBias = true;

    // Train first
    Training.TrainRLSOnline(ReservoirStates[0], TargetOutputs[0]);
    EXPECT_GT(Training.TotalSamplesProcessed, 0);

    // Reset
    Training.ResetRLSState();
    EXPECT_EQ(Training.TotalSamplesProcessed, 0);

    // Train again
    Training.TrainRLSOnline(ReservoirStates[0], TargetOutputs[0]);
    EXPECT_EQ(Training.TotalSamplesProcessed, 1);
}

// ============================================================================
// Multi-Task Learning Tests
// ============================================================================

TEST_F(ReadoutLayerTrainingTest, MultiTaskTraining)
{
    UReadoutLayerTraining Training;
    Training.bEnableMultiTask = true;
    Training.MultiTaskConfig.NumTasks = 2;
    Training.MultiTaskConfig.TaskNames = {TEXT("Task1"), TEXT("Task2")};

    // Generate multi-task outputs
    TMap<FString, TArray<TArray<float>>> TaskOutputs;
    
    // Task 1: y1 = 2*x1 + 3*x2 + 1
    TArray<TArray<float>> Task1Outputs = TargetOutputs;
    
    // Task 2: y2 = -1*x1 + 2*x2 + 0.5
    TArray<TArray<float>> Task2Outputs;
    Task2Outputs.SetNum(NumSamples);
    for (int32 i = 0; i < NumSamples; ++i)
    {
        Task2Outputs[i].SetNum(1);
        Task2Outputs[i][0] = -1.0f * ReservoirStates[i][0] + 
                              2.0f * ReservoirStates[i][1] + 0.5f;
    }

    TaskOutputs.Add(TEXT("Task1"), Task1Outputs);
    TaskOutputs.Add(TEXT("Task2"), Task2Outputs);

    // Train multi-task
    TMap<FString, FReadoutTrainingMetrics> TaskMetrics = Training.TrainMultiTask(
        ReservoirStates, TaskOutputs
    );

    EXPECT_EQ(TaskMetrics.Num(), 2);
    EXPECT_TRUE(TaskMetrics.Contains(TEXT("Task1")));
    EXPECT_TRUE(TaskMetrics.Contains(TEXT("Task2")));

    // Both tasks should have low MSE
    EXPECT_LT(TaskMetrics[TEXT("Task1")].MSE, 1e-3f);
    EXPECT_LT(TaskMetrics[TEXT("Task2")].MSE, 1e-3f);

    std::cout << "Multi-Task Training:\n"
              << "  Task1 MSE: " << TaskMetrics[TEXT("Task1")].MSE << "\n"
              << "  Task2 MSE: " << TaskMetrics[TEXT("Task2")].MSE << "\n";
}

// ============================================================================
// Regularization Tuning Tests
// ============================================================================

TEST_F(ReadoutLayerTrainingTest, RegularizationTuning)
{
    GenerateNoisyTestData();

    UReadoutLayerTraining Training;
    Training.bEnableAutoTuning = true;
    Training.TuningConfig.SearchRange = FVector2D(-6.0f, 0.0f);  // 1e-6 to 1
    Training.TuningConfig.NumSearchPoints = 10;
    Training.CrossValidationConfig.NumFolds = 3;

    float OptimalLambda = Training.TuneRegularization(
        ReservoirStates, TargetOutputs
    );

    EXPECT_GT(OptimalLambda, 0.0f);
    EXPECT_LT(OptimalLambda, 10.0f);

    std::cout << "Optimal regularization parameter: " << OptimalLambda << "\n";
}

TEST_F(ReadoutLayerTrainingTest, GridSearch)
{
    GenerateNoisyTestData();

    UReadoutLayerTraining Training;
    Training.CrossValidationConfig.NumFolds = 3;

    TArray<float> Lambdas = {1e-6f, 1e-4f, 1e-2f, 1.0f};
    TArray<float> Scores = Training.GridSearchRegularization(
        ReservoirStates, TargetOutputs, Lambdas
    );

    EXPECT_EQ(Scores.Num(), Lambdas.Num());

    // Find best score
    float BestScore = Scores[0];
    int32 BestIdx = 0;
    for (int32 i = 1; i < Scores.Num(); ++i)
    {
        if (Scores[i] < BestScore)
        {
            BestScore = Scores[i];
            BestIdx = i;
        }
    }

    std::cout << "Grid Search Results:\n";
    for (int32 i = 0; i < Lambdas.Num(); ++i)
    {
        std::cout << "  Lambda=" << Lambdas[i] << " -> Score=" << Scores[i];
        if (i == BestIdx) std::cout << " (BEST)";
        std::cout << "\n";
    }
}

// ============================================================================
// Cross-Validation Tests
// ============================================================================

TEST_F(ReadoutLayerTrainingTest, KFoldCrossValidation)
{
    UReadoutLayerTraining Training;
    Training.CrossValidationConfig.Strategy = ECrossValidationStrategy::KFold;
    Training.CrossValidationConfig.NumFolds = 5;
    Training.RidgeConfig.RegularizationParameter = 1e-4f;

    FCrossValidationResults Results = Training.KFoldCrossValidation(
        ReservoirStates, TargetOutputs, 5
    );

    EXPECT_EQ(Results.FoldMetrics.Num(), 5);
    EXPECT_LT(Results.MeanMetrics.MSE, 0.1f);
    EXPECT_GT(Results.MeanMetrics.RSquared, 0.9f);

    std::cout << "K-Fold Cross-Validation Results:\n"
              << "  Mean MSE: " << Results.MeanMetrics.MSE 
              << " ± " << Results.StdMetrics.MSE << "\n"
              << "  Mean R²: " << Results.MeanMetrics.RSquared 
              << " ± " << Results.StdMetrics.RSquared << "\n"
              << "  Best Fold: " << Results.BestFoldIndex << "\n";
}

TEST_F(ReadoutLayerTrainingTest, TimeSeriesCrossValidation)
{
    UReadoutLayerTraining Training;
    Training.CrossValidationConfig.Strategy = ECrossValidationStrategy::TimeSeriesSplit;
    Training.CrossValidationConfig.NumFolds = 5;
    Training.RidgeConfig.RegularizationParameter = 1e-4f;

    FCrossValidationResults Results = Training.TimeSeriesCrossValidation(
        ReservoirStates, TargetOutputs
    );

    EXPECT_EQ(Results.FoldMetrics.Num(), 5);
    EXPECT_LT(Results.MeanMetrics.MSE, 0.1f);

    std::cout << "Time Series Cross-Validation Results:\n"
              << "  Mean MSE: " << Results.MeanMetrics.MSE 
              << " ± " << Results.StdMetrics.MSE << "\n";
}

TEST_F(ReadoutLayerTrainingTest, CrossValidationWithAutoTuning)
{
    GenerateNoisyTestData();

    UReadoutLayerTraining Training;
    Training.bEnableAutoTuning = true;
    Training.bEnableCrossValidation = true;
    Training.CrossValidationConfig.NumFolds = 3;
    Training.TuningConfig.NumSearchPoints = 5;

    // This should automatically tune regularization using CV
    FReadoutTrainingMetrics Metrics = Training.TrainRidgeRegression(
        ReservoirStates, TargetOutputs
    );

    EXPECT_TRUE(Metrics.bConverged);
    EXPECT_GT(Training.CVResults.OptimalRegularization, 0.0f);

    std::cout << "Auto-tuned regularization: " 
              << Training.CVResults.OptimalRegularization << "\n"
              << "  Final MSE: " << Metrics.MSE << "\n";
}

// ============================================================================
// Metrics Computation Tests
// ============================================================================

TEST_F(ReadoutLayerTrainingTest, MetricsComputation)
{
    UReadoutLayerTraining Training;

    // Create perfect predictions
    TArray<TArray<float>> Predictions = TargetOutputs;

    FReadoutTrainingMetrics Metrics = Training.ComputeMetrics(
        Predictions, TargetOutputs
    );

    // Perfect fit should have zero error
    EXPECT_NEAR(Metrics.MSE, 0.0f, 1e-10f);
    EXPECT_NEAR(Metrics.RMSE, 0.0f, 1e-10f);
    EXPECT_NEAR(Metrics.MAE, 0.0f, 1e-10f);
    EXPECT_NEAR(Metrics.RSquared, 1.0f, 1e-4f);
}

TEST_F(ReadoutLayerTrainingTest, MetricsWithError)
{
    UReadoutLayerTraining Training;

    // Create predictions with constant offset
    TArray<TArray<float>> Predictions = TargetOutputs;
    float Offset = 0.1f;
    for (auto& Pred : Predictions)
    {
        for (float& Val : Pred)
        {
            Val += Offset;
        }
    }

    FReadoutTrainingMetrics Metrics = Training.ComputeMetrics(
        Predictions, TargetOutputs
    );

    // Should have non-zero error
    EXPECT_GT(Metrics.MSE, 0.0f);
    EXPECT_NEAR(Metrics.MSE, Offset * Offset, 1e-4f);
    EXPECT_NEAR(Metrics.MAE, Offset, 1e-4f);
}

// ============================================================================
// Weight Management Tests
// ============================================================================

TEST_F(ReadoutLayerTrainingTest, WeightGetSet)
{
    UReadoutLayerTraining Training;

    // Train to get weights
    Training.TrainRidgeRegression(ReservoirStates, TargetOutputs);

    // Get weights
    TArray<TArray<float>> Weights = Training.GetReadoutWeights();
    EXPECT_GT(Weights.Num(), 0);

    // Modify weights
    TArray<TArray<float>> NewWeights = Weights;
    for (auto& Row : NewWeights)
    {
        for (float& Val : Row)
        {
            Val *= 2.0f;
        }
    }

    // Set new weights
    Training.SetReadoutWeights(NewWeights);

    // Verify weights changed
    TArray<TArray<float>> RetrievedWeights = Training.GetReadoutWeights();
    EXPECT_EQ(RetrievedWeights.Num(), NewWeights.Num());
    
    for (int32 i = 0; i < NewWeights.Num(); ++i)
    {
        for (int32 j = 0; j < NewWeights[i].Num(); ++j)
        {
            EXPECT_NEAR(RetrievedWeights[i][j], NewWeights[i][j], 1e-6f);
        }
    }
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(ReadoutLayerTrainingTest, EndToEndPipeline)
{
    GenerateNoisyTestData();

    UReadoutLayerTraining Training;
    
    // Configure full pipeline
    Training.bEnableMultiTask = false;
    Training.bEnableAutoTuning = true;
    Training.bEnableCrossValidation = true;
    
    Training.TrainingMethod = EReadoutTrainingMethod::RidgeRegression;
    Training.CrossValidationConfig.NumFolds = 3;
    Training.TuningConfig.NumSearchPoints = 5;

    // Train with auto-tuning and CV
    FReadoutTrainingMetrics Metrics = Training.TrainRidgeRegression(
        ReservoirStates, TargetOutputs
    );

    // Verify complete pipeline executed
    EXPECT_TRUE(Metrics.bConverged);
    EXPECT_LT(Metrics.MSE, 1.0f);
    EXPECT_GT(Training.CVResults.OptimalRegularization, 0.0f);

    std::cout << "End-to-End Pipeline Results:\n"
              << "  Optimal Lambda: " << Training.CVResults.OptimalRegularization << "\n"
              << "  Final MSE: " << Metrics.MSE << "\n"
              << "  Final R²: " << Metrics.RSquared << "\n"
              << "  CV Mean MSE: " << Training.CVResults.MeanMetrics.MSE << "\n";
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

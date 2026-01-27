#pragma once

/**
 * Readout Layer Training System
 * Feature F1.2.4 - Phase 1.2
 * 
 * Comprehensive training system for Echo State Network readout layers,
 * implementing ridge regression, online learning (RLS), multi-task support,
 * regularization tuning, and cross-validation frameworks.
 * 
 * Architecture:
 * - Ridge Regression: Batch training with Tikhonov regularization
 * - Recursive Least Squares: Online learning with forgetting factor
 * - Multi-Task Learning: Shared reservoir with multiple output heads
 * - Regularization Tuning: Automated hyperparameter optimization
 * - Cross-Validation: K-fold and time-series aware validation
 * 
 * Integration Points:
 * - ReservoirEcho/reservoircpp_cpp for base ESN functionality
 * - DeepTreeEchoReservoir for cognitive reservoir integration
 * - System 5 tetradic architecture for multi-scale learning
 * 
 * @see DeepTreeEchoReservoir
 * @see ReservoirCognitiveIntegration
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepTreeEchoReservoir.h"
#include "ReadoutLayerTraining.generated.h"

/**
 * Training Method Type
 */
UENUM(BlueprintType)
enum class EReadoutTrainingMethod : uint8
{
    /** Ridge regression (batch) */
    RidgeRegression UMETA(DisplayName = "Ridge Regression"),
    
    /** Recursive Least Squares (online) */
    RecursiveLeastSquares UMETA(DisplayName = "Recursive Least Squares"),
    
    /** FORCE learning */
    FORCE UMETA(DisplayName = "FORCE"),
    
    /** Least Mean Squares */
    LeastMeanSquares UMETA(DisplayName = "Least Mean Squares")
};

/**
 * Regularization Method
 */
UENUM(BlueprintType)
enum class ERegularizationMethod : uint8
{
    /** L2 (Tikhonov) regularization */
    L2 UMETA(DisplayName = "L2 (Ridge)"),
    
    /** L1 (Lasso) regularization */
    L1 UMETA(DisplayName = "L1 (Lasso)"),
    
    /** Elastic Net (L1 + L2) */
    ElasticNet UMETA(DisplayName = "Elastic Net"),
    
    /** No regularization */
    None UMETA(DisplayName = "None")
};

/**
 * Cross-Validation Strategy
 */
UENUM(BlueprintType)
enum class ECrossValidationStrategy : uint8
{
    /** Standard K-fold cross-validation */
    KFold UMETA(DisplayName = "K-Fold"),
    
    /** Time-series aware train-test split */
    TimeSeriesSplit UMETA(DisplayName = "Time Series Split"),
    
    /** Leave-one-out cross-validation */
    LeaveOneOut UMETA(DisplayName = "Leave One Out"),
    
    /** Stratified K-fold (for classification) */
    StratifiedKFold UMETA(DisplayName = "Stratified K-Fold")
};

/**
 * Ridge Regression Configuration
 */
USTRUCT(BlueprintType)
struct FRidgeRegressionConfig
{
    GENERATED_BODY()

    /** Regularization parameter (lambda) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge")
    float RegularizationParameter = 1e-6f;

    /** Use bias term */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge")
    bool bUseBias = true;

    /** Solver method (auto, svd, cholesky) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge")
    FString SolverMethod = TEXT("auto");

    /** Maximum iterations for iterative solvers */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge")
    int32 MaxIterations = 1000;

    /** Convergence tolerance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ridge")
    float Tolerance = 1e-6f;
};

/**
 * Recursive Least Squares Configuration
 */
USTRUCT(BlueprintType)
struct FRecursiveLeastSquaresConfig
{
    GENERATED_BODY()

    /** Initial covariance matrix scaling (alpha) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RLS")
    float Alpha = 1e-6f;

    /** Forgetting factor (lambda) - 1.0 = no forgetting */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RLS")
    float ForgettingFactor = 1.0f;

    /** Use bias term */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RLS")
    bool bUseBias = true;

    /** Adaptive forgetting factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RLS")
    bool bAdaptiveForgetting = false;

    /** Forgetting factor adaptation rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RLS")
    float ForgettingAdaptationRate = 0.95f;

    /** Minimum forgetting factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RLS")
    float MinForgettingFactor = 0.9f;

    /** Maximum forgetting factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RLS")
    float MaxForgettingFactor = 1.0f;
};

/**
 * Multi-Task Readout Configuration
 */
USTRUCT(BlueprintType)
struct FMultiTaskReadoutConfig
{
    GENERATED_BODY()

    /** Number of tasks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MultiTask")
    int32 NumTasks = 1;

    /** Task names */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MultiTask")
    TArray<FString> TaskNames;

    /** Task-specific regularization parameters */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MultiTask")
    TArray<float> TaskRegularizationParams;

    /** Task weights for joint optimization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MultiTask")
    TArray<float> TaskWeights;

    /** Share readout weights across tasks */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MultiTask")
    bool bShareReadoutWeights = false;

    /** Task-specific output dimensions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MultiTask")
    TArray<int32> TaskOutputDimensions;
};

/**
 * Regularization Tuning Configuration
 */
USTRUCT(BlueprintType)
struct FRegularizationTuningConfig
{
    GENERATED_BODY()

    /** Regularization method */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
    ERegularizationMethod Method = ERegularizationMethod::L2;

    /** Regularization parameter search range (log scale) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
    FVector2D SearchRange = FVector2D(-8.0f, 2.0f);  // 1e-8 to 1e2

    /** Number of parameter values to test */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
    int32 NumSearchPoints = 20;

    /** Use grid search */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
    bool bUseGridSearch = true;

    /** Use Bayesian optimization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
    bool bUseBayesianOptimization = false;

    /** Maximum optimization iterations */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
    int32 MaxOptimizationIterations = 50;

    /** Validation metric to optimize */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning")
    FString ValidationMetric = TEXT("MSE");
};

/**
 * Cross-Validation Configuration
 */
USTRUCT(BlueprintType)
struct FCrossValidationConfig
{
    GENERATED_BODY()

    /** Cross-validation strategy */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CrossValidation")
    ECrossValidationStrategy Strategy = ECrossValidationStrategy::KFold;

    /** Number of folds for K-fold */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CrossValidation")
    int32 NumFolds = 5;

    /** Train/test split ratio for time series */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CrossValidation")
    float TrainTestSplitRatio = 0.8f;

    /** Shuffle data before splitting (not for time series) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CrossValidation")
    bool bShuffleData = true;

    /** Random seed for reproducibility */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CrossValidation")
    int32 RandomSeed = 42;

    /** Metrics to compute */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CrossValidation")
    TArray<FString> Metrics;
};

/**
 * Training Performance Metrics
 */
USTRUCT(BlueprintType)
struct FReadoutTrainingMetrics
{
    GENERATED_BODY()

    /** Mean Squared Error */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float MSE = 0.0f;

    /** Root Mean Squared Error */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float RMSE = 0.0f;

    /** Mean Absolute Error */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float MAE = 0.0f;

    /** R-squared coefficient of determination */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float RSquared = 0.0f;

    /** Training time in milliseconds */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    float TrainingTimeMs = 0.0f;

    /** Number of training samples */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int32 NumTrainingSamples = 0;

    /** Convergence achieved */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    bool bConverged = false;

    /** Number of iterations */
    UPROPERTY(BlueprintReadOnly, Category = "Metrics")
    int32 NumIterations = 0;
};

/**
 * Cross-Validation Results
 */
USTRUCT(BlueprintType)
struct FCrossValidationResults
{
    GENERATED_BODY()

    /** Mean metrics across folds */
    UPROPERTY(BlueprintReadOnly, Category = "Results")
    FReadoutTrainingMetrics MeanMetrics;

    /** Standard deviation of metrics across folds */
    UPROPERTY(BlueprintReadOnly, Category = "Results")
    FReadoutTrainingMetrics StdMetrics;

    /** Per-fold metrics */
    UPROPERTY(BlueprintReadOnly, Category = "Results")
    TArray<FReadoutTrainingMetrics> FoldMetrics;

    /** Best fold index */
    UPROPERTY(BlueprintReadOnly, Category = "Results")
    int32 BestFoldIndex = 0;

    /** Optimal regularization parameter */
    UPROPERTY(BlueprintReadOnly, Category = "Results")
    float OptimalRegularization = 0.0f;
};

/**
 * Readout Layer Training Component
 * 
 * Provides comprehensive training functionality for ESN readout layers,
 * including batch and online learning, multi-task support, hyperparameter
 * tuning, and cross-validation.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class UReadoutLayerTraining : public UActorComponent
{
    GENERATED_BODY()

public:
    UReadoutLayerTraining();

protected:
    virtual void BeginPlay() override;

public:
    // ============================================================================
    // Configuration
    // ============================================================================

    /** Training method */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training")
    EReadoutTrainingMethod TrainingMethod = EReadoutTrainingMethod::RidgeRegression;

    /** Ridge regression configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Ridge")
    FRidgeRegressionConfig RidgeConfig;

    /** Recursive Least Squares configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|RLS")
    FRecursiveLeastSquaresConfig RLSConfig;

    /** Multi-task readout configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|MultiTask")
    FMultiTaskReadoutConfig MultiTaskConfig;

    /** Regularization tuning configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Tuning")
    FRegularizationTuningConfig TuningConfig;

    /** Cross-validation configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Validation")
    FCrossValidationConfig CrossValidationConfig;

    /** Enable multi-task learning */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training")
    bool bEnableMultiTask = false;

    /** Enable automatic regularization tuning */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training")
    bool bEnableAutoTuning = false;

    /** Enable cross-validation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training")
    bool bEnableCrossValidation = false;

    // ============================================================================
    // Runtime State
    // ============================================================================

    /** Latest training metrics */
    UPROPERTY(BlueprintReadOnly, Category = "Training|State")
    FReadoutTrainingMetrics LatestMetrics;

    /** Cross-validation results */
    UPROPERTY(BlueprintReadOnly, Category = "Training|State")
    FCrossValidationResults CVResults;

    /** Is training in progress */
    UPROPERTY(BlueprintReadOnly, Category = "Training|State")
    bool bIsTraining = false;

    /** Total training samples processed */
    UPROPERTY(BlueprintReadOnly, Category = "Training|State")
    int32 TotalSamplesProcessed = 0;

    // ============================================================================
    // Ridge Regression Training
    // ============================================================================

    /**
     * Train readout layer using ridge regression (batch mode)
     * 
     * @param ReservoirStates - Matrix of reservoir states (samples x reservoir_size)
     * @param TargetOutputs - Matrix of target outputs (samples x output_size)
     * @return Training metrics
     */
    UFUNCTION(BlueprintCallable, Category = "Training|Ridge")
    FReadoutTrainingMetrics TrainRidgeRegression(
        const TArray<TArray<float>>& ReservoirStates,
        const TArray<TArray<float>>& TargetOutputs
    );

    /**
     * Incremental ridge regression training
     * Add more data to existing ridge solution
     */
    UFUNCTION(BlueprintCallable, Category = "Training|Ridge")
    FReadoutTrainingMetrics UpdateRidgeRegression(
        const TArray<TArray<float>>& NewReservoirStates,
        const TArray<TArray<float>>& NewTargetOutputs
    );

    // ============================================================================
    // Recursive Least Squares Training
    // ============================================================================

    /**
     * Train readout layer using RLS (online mode)
     * Updates weights incrementally for each sample
     * 
     * @param ReservoirState - Single reservoir state vector
     * @param TargetOutput - Single target output vector
     * @return Training metrics
     */
    UFUNCTION(BlueprintCallable, Category = "Training|RLS")
    FReadoutTrainingMetrics TrainRLSOnline(
        const TArray<float>& ReservoirState,
        const TArray<float>& TargetOutput
    );

    /**
     * Batch RLS training - process multiple samples
     */
    UFUNCTION(BlueprintCallable, Category = "Training|RLS")
    FReadoutTrainingMetrics TrainRLSBatch(
        const TArray<TArray<float>>& ReservoirStates,
        const TArray<TArray<float>>& TargetOutputs
    );

    /**
     * Reset RLS state (P matrix and weights)
     */
    UFUNCTION(BlueprintCallable, Category = "Training|RLS")
    void ResetRLSState();

    // ============================================================================
    // Multi-Task Learning
    // ============================================================================

    /**
     * Train multi-task readout layer
     * Shared reservoir with multiple output heads
     * 
     * @param ReservoirStates - Reservoir states
     * @param TaskOutputs - Map of task name to target outputs
     * @return Per-task training metrics
     */
    UFUNCTION(BlueprintCallable, Category = "Training|MultiTask")
    TMap<FString, FReadoutTrainingMetrics> TrainMultiTask(
        const TArray<TArray<float>>& ReservoirStates,
        const TMap<FString, TArray<TArray<float>>>& TaskOutputs
    );

    /**
     * Update multi-task readout with new data
     */
    UFUNCTION(BlueprintCallable, Category = "Training|MultiTask")
    TMap<FString, FReadoutTrainingMetrics> UpdateMultiTask(
        const TArray<TArray<float>>& ReservoirStates,
        const TMap<FString, TArray<TArray<float>>>& TaskOutputs
    );

    // ============================================================================
    // Regularization Tuning
    // ============================================================================

    /**
     * Automatically tune regularization parameter
     * Uses grid search or Bayesian optimization with cross-validation
     * 
     * @param ReservoirStates - Training reservoir states
     * @param TargetOutputs - Training target outputs
     * @return Optimal regularization parameter
     */
    UFUNCTION(BlueprintCallable, Category = "Training|Tuning")
    float TuneRegularization(
        const TArray<TArray<float>>& ReservoirStates,
        const TArray<TArray<float>>& TargetOutputs
    );

    /**
     * Grid search for optimal regularization
     */
    UFUNCTION(BlueprintCallable, Category = "Training|Tuning")
    TArray<float> GridSearchRegularization(
        const TArray<TArray<float>>& ReservoirStates,
        const TArray<TArray<float>>& TargetOutputs,
        const TArray<float>& ParameterValues
    );

    // ============================================================================
    // Cross-Validation
    // ============================================================================

    /**
     * Perform cross-validation
     * 
     * @param ReservoirStates - All available reservoir states
     * @param TargetOutputs - All available target outputs
     * @return Cross-validation results
     */
    UFUNCTION(BlueprintCallable, Category = "Training|Validation")
    FCrossValidationResults PerformCrossValidation(
        const TArray<TArray<float>>& ReservoirStates,
        const TArray<TArray<float>>& TargetOutputs
    );

    /**
     * K-fold cross-validation
     */
    UFUNCTION(BlueprintCallable, Category = "Training|Validation")
    FCrossValidationResults KFoldCrossValidation(
        const TArray<TArray<float>>& ReservoirStates,
        const TArray<TArray<float>>& TargetOutputs,
        int32 NumFolds
    );

    /**
     * Time-series cross-validation
     * Uses expanding window for temporal data
     */
    UFUNCTION(BlueprintCallable, Category = "Training|Validation")
    FCrossValidationResults TimeSeriesCrossValidation(
        const TArray<TArray<float>>& ReservoirStates,
        const TArray<TArray<float>>& TargetOutputs
    );

    // ============================================================================
    // Metrics and Evaluation
    // ============================================================================

    /**
     * Compute training metrics
     */
    UFUNCTION(BlueprintCallable, Category = "Training|Metrics")
    FReadoutTrainingMetrics ComputeMetrics(
        const TArray<TArray<float>>& Predictions,
        const TArray<TArray<float>>& Targets
    );

    /**
     * Get current readout weights
     */
    UFUNCTION(BlueprintCallable, Category = "Training")
    TArray<TArray<float>> GetReadoutWeights() const;

    /**
     * Set readout weights
     */
    UFUNCTION(BlueprintCallable, Category = "Training")
    void SetReadoutWeights(const TArray<TArray<float>>& Weights);

private:
    // ============================================================================
    // Internal State
    // ============================================================================

    /** Current readout weight matrix */
    TArray<TArray<float>> ReadoutWeights;

    /** Current bias vector */
    TArray<float> BiasWeights;

    /** RLS covariance matrix (P) */
    TArray<TArray<float>> RLS_P_Matrix;

    /** Multi-task readout weights */
    TMap<FString, TArray<TArray<float>>> MultiTaskReadoutWeights;

    /** Multi-task bias weights */
    TMap<FString, TArray<float>> MultiTaskBiasWeights;

    /** Training history */
    TArray<FReadoutTrainingMetrics> TrainingHistory;

    // ============================================================================
    // Internal Helper Functions
    // ============================================================================

    /** Initialize RLS P matrix */
    void InitializeRLSMatrix(int32 ReservoirSize);

    /** Update RLS P matrix */
    void UpdateRLSMatrix(const TArray<float>& ReservoirState, const TArray<float>& Error);

    /** Solve ridge regression */
    void SolveRidge(
        const TArray<TArray<float>>& X,
        const TArray<TArray<float>>& Y,
        float Lambda
    );

    /** Compute matrix product */
    TArray<TArray<float>> MatrixMultiply(
        const TArray<TArray<float>>& A,
        const TArray<TArray<float>>& B
    ) const;

    /** Compute matrix transpose */
    TArray<TArray<float>> MatrixTranspose(const TArray<TArray<float>>& A) const;

    /** Invert matrix using Cholesky decomposition */
    TArray<TArray<float>> MatrixInvert(const TArray<TArray<float>>& A) const;

    /** Add identity matrix scaled by lambda */
    TArray<TArray<float>> AddScaledIdentity(
        const TArray<TArray<float>>& A,
        float Lambda
    ) const;

    /** Split data for cross-validation */
    void SplitData(
        const TArray<TArray<float>>& ReservoirStates,
        const TArray<TArray<float>>& TargetOutputs,
        int32 FoldIndex,
        int32 NumFolds,
        TArray<TArray<float>>& TrainStates,
        TArray<TArray<float>>& TrainTargets,
        TArray<TArray<float>>& TestStates,
        TArray<TArray<float>>& TestTargets
    );

    /** Generate logarithmically spaced values */
    TArray<float> LogSpace(float Start, float End, int32 NumPoints) const;

    /** Compute predictions using current weights */
    TArray<TArray<float>> Predict(const TArray<TArray<float>>& ReservoirStates) const;

    /** Update training metrics */
    void UpdateMetrics(
        const TArray<TArray<float>>& Predictions,
        const TArray<TArray<float>>& Targets,
        FReadoutTrainingMetrics& Metrics
    );
};

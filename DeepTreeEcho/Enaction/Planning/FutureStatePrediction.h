// FutureStatePrediction.h
// Future State Prediction System for Deep Tree Echo
// Implements predictive modeling for anticipating future states based on current trajectories

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FutureStatePrediction.generated.h"

// Forward declarations
class UPredictiveAdaptationEngine;
class UActiveInferenceEngine;
class UEchobeatsStreamEngine;
class UDeepTreeEchoReservoir;

/**
 * Prediction horizon type
 */
UENUM(BlueprintType)
enum class EPredictionHorizon : uint8
{
    Immediate       UMETA(DisplayName = "Immediate (1-2 steps)"),
    ShortTerm       UMETA(DisplayName = "Short Term (3-5 steps)"),
    MediumTerm      UMETA(DisplayName = "Medium Term (6-12 steps)"),
    LongTerm        UMETA(DisplayName = "Long Term (12+ steps)")
};

/**
 * Trajectory type for state prediction
 */
UENUM(BlueprintType)
enum class ETrajectoryType : uint8
{
    Linear          UMETA(DisplayName = "Linear Extrapolation"),
    Polynomial      UMETA(DisplayName = "Polynomial Fitting"),
    Exponential     UMETA(DisplayName = "Exponential Trend"),
    Reservoir       UMETA(DisplayName = "Reservoir Computing"),
    Ensemble        UMETA(DisplayName = "Ensemble Method")
};

/**
 * State domain for prediction
 */
UENUM(BlueprintType)
enum class EStateDomain : uint8
{
    Cognitive       UMETA(DisplayName = "Cognitive State"),
    Emotional       UMETA(DisplayName = "Emotional State"),
    Motor           UMETA(DisplayName = "Motor State"),
    Environmental   UMETA(DisplayName = "Environmental State"),
    Social          UMETA(DisplayName = "Social State"),
    Integrated      UMETA(DisplayName = "Integrated Multi-Domain")
};

/**
 * Confidence level for predictions
 */
UENUM(BlueprintType)
enum class EPredictionConfidence : uint8
{
    VeryLow         UMETA(DisplayName = "Very Low (<20%)"),
    Low             UMETA(DisplayName = "Low (20-40%)"),
    Medium          UMETA(DisplayName = "Medium (40-60%)"),
    High            UMETA(DisplayName = "High (60-80%)"),
    VeryHigh        UMETA(DisplayName = "Very High (>80%)")
};

/**
 * Single state observation for trajectory building
 */
USTRUCT(BlueprintType)
struct FStateObservation
{
    GENERATED_BODY()

    /** Timestamp of observation */
    UPROPERTY(BlueprintReadOnly)
    float Timestamp = 0.0f;

    /** Echobeat step at observation time */
    UPROPERTY(BlueprintReadOnly)
    int32 EchobeatStep = 0;

    /** State vector */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> StateVector;

    /** Domain of the state */
    UPROPERTY(BlueprintReadOnly)
    EStateDomain Domain = EStateDomain::Cognitive;

    /** Observation reliability (0-1) */
    UPROPERTY(BlueprintReadOnly)
    float Reliability = 1.0f;
};

/**
 * Trajectory representing state evolution over time
 */
USTRUCT(BlueprintType)
struct FStateTrajectory
{
    GENERATED_BODY()

    /** Unique trajectory ID */
    UPROPERTY(BlueprintReadOnly)
    FString TrajectoryID;

    /** Domain of the trajectory */
    UPROPERTY(BlueprintReadOnly)
    EStateDomain Domain = EStateDomain::Cognitive;

    /** Ordered observations forming the trajectory */
    UPROPERTY(BlueprintReadOnly)
    TArray<FStateObservation> Observations;

    /** Fitted trajectory parameters (for polynomial/exponential) */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> FittedParameters;

    /** Trajectory type used for fitting */
    UPROPERTY(BlueprintReadOnly)
    ETrajectoryType FittingType = ETrajectoryType::Linear;

    /** Goodness of fit (R-squared) */
    UPROPERTY(BlueprintReadOnly)
    float GoodnessOfFit = 0.0f;

    /** Creation timestamp */
    UPROPERTY(BlueprintReadOnly)
    float CreatedAt = 0.0f;

    /** Last update timestamp */
    UPROPERTY(BlueprintReadOnly)
    float LastUpdatedAt = 0.0f;
};

/**
 * Predicted future state
 */
USTRUCT(BlueprintType)
struct FFuturePrediction
{
    GENERATED_BODY()

    /** Unique prediction ID */
    UPROPERTY(BlueprintReadOnly)
    int32 PredictionID = 0;

    /** Source trajectory ID */
    UPROPERTY(BlueprintReadOnly)
    FString SourceTrajectoryID;

    /** Predicted state vector */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> PredictedState;

    /** Prediction horizon */
    UPROPERTY(BlueprintReadOnly)
    EPredictionHorizon Horizon = EPredictionHorizon::Immediate;

    /** Target timestamp for prediction */
    UPROPERTY(BlueprintReadOnly)
    float TargetTimestamp = 0.0f;

    /** Steps into the future */
    UPROPERTY(BlueprintReadOnly)
    int32 StepsAhead = 1;

    /** Confidence score (0-1) */
    UPROPERTY(BlueprintReadOnly)
    float Confidence = 0.5f;

    /** Confidence level classification */
    UPROPERTY(BlueprintReadOnly)
    EPredictionConfidence ConfidenceLevel = EPredictionConfidence::Medium;

    /** Uncertainty bounds (standard deviation per dimension) */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> UncertaintyBounds;

    /** Domain of prediction */
    UPROPERTY(BlueprintReadOnly)
    EStateDomain Domain = EStateDomain::Cognitive;

    /** Method used for prediction */
    UPROPERTY(BlueprintReadOnly)
    ETrajectoryType PredictionMethod = ETrajectoryType::Linear;

    /** Prediction creation timestamp */
    UPROPERTY(BlueprintReadOnly)
    float CreatedAt = 0.0f;

    /** Whether prediction has been validated */
    UPROPERTY(BlueprintReadOnly)
    bool bValidated = false;

    /** Actual state (filled when validated) */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> ActualState;

    /** Prediction error (filled when validated) */
    UPROPERTY(BlueprintReadOnly)
    float PredictionError = 0.0f;
};

/**
 * Prediction validation result
 */
USTRUCT(BlueprintType)
struct FPredictionValidation
{
    GENERATED_BODY()

    /** Prediction ID that was validated */
    UPROPERTY(BlueprintReadOnly)
    int32 PredictionID = 0;

    /** Mean squared error */
    UPROPERTY(BlueprintReadOnly)
    float MSE = 0.0f;

    /** Mean absolute error */
    UPROPERTY(BlueprintReadOnly)
    float MAE = 0.0f;

    /** Correlation coefficient */
    UPROPERTY(BlueprintReadOnly)
    float Correlation = 0.0f;

    /** Was prediction within uncertainty bounds */
    UPROPERTY(BlueprintReadOnly)
    bool bWithinBounds = false;

    /** Validation timestamp */
    UPROPERTY(BlueprintReadOnly)
    float ValidatedAt = 0.0f;
};

/**
 * Ensemble prediction combining multiple methods
 */
USTRUCT(BlueprintType)
struct FEnsemblePrediction
{
    GENERATED_BODY()

    /** Individual predictions from different methods */
    UPROPERTY(BlueprintReadOnly)
    TArray<FFuturePrediction> IndividualPredictions;

    /** Combined ensemble prediction */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> EnsembleState;

    /** Weights for each method */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> MethodWeights;

    /** Overall ensemble confidence */
    UPROPERTY(BlueprintReadOnly)
    float EnsembleConfidence = 0.5f;

    /** Disagreement between methods (variance) */
    UPROPERTY(BlueprintReadOnly)
    float MethodDisagreement = 0.0f;
};

/**
 * Prediction performance statistics
 */
USTRUCT(BlueprintType)
struct FPredictionStatistics
{
    GENERATED_BODY()

    /** Total predictions made */
    UPROPERTY(BlueprintReadOnly)
    int32 TotalPredictions = 0;

    /** Validated predictions */
    UPROPERTY(BlueprintReadOnly)
    int32 ValidatedPredictions = 0;

    /** Average MSE across validations */
    UPROPERTY(BlueprintReadOnly)
    float AverageMSE = 0.0f;

    /** Average confidence */
    UPROPERTY(BlueprintReadOnly)
    float AverageConfidence = 0.0f;

    /** Predictions within bounds ratio */
    UPROPERTY(BlueprintReadOnly)
    float WithinBoundsRatio = 0.0f;

    /** Best performing method */
    UPROPERTY(BlueprintReadOnly)
    ETrajectoryType BestMethod = ETrajectoryType::Linear;

    /** Method-specific MSE */
    UPROPERTY(BlueprintReadOnly)
    TMap<ETrajectoryType, float> MethodMSE;

    /** Horizon-specific accuracy */
    UPROPERTY(BlueprintReadOnly)
    TMap<EPredictionHorizon, float> HorizonAccuracy;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrajectoryUpdated, const FStateTrajectory&, Trajectory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPredictionGenerated, const FFuturePrediction&, Prediction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPredictionValidated, const FPredictionValidation&, Validation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPredictionError, float, PredictedValue, float, ActualValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnsemblePrediction, const FEnsemblePrediction&, Ensemble);

/**
 * Future State Prediction System
 * 
 * Implements predictive modeling for anticipating future states based on current
 * trajectories within the Deep Tree Echo cognitive architecture.
 * 
 * Key features:
 * - Multi-method trajectory extrapolation (linear, polynomial, reservoir)
 * - Multi-horizon prediction (immediate to long-term)
 * - Uncertainty quantification with confidence bounds
 * - Ensemble prediction combining multiple methods
 * - Continuous validation and method adaptation
 * - Integration with 12-step cognitive cycle
 * - Cross-domain state prediction (cognitive, emotional, motor, etc.)
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UFutureStatePrediction : public UActorComponent
{
    GENERATED_BODY()

public:
    UFutureStatePrediction();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Default state vector dimension */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Model")
    int32 StateVectorDimension = 64;

    /** Maximum trajectory history length */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Trajectory")
    int32 MaxTrajectoryLength = 100;

    /** Minimum observations for trajectory fitting */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Trajectory")
    int32 MinObservationsForFitting = 5;

    /** Maximum predictions to keep in history */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|History")
    int32 MaxPredictionHistory = 200;

    /** Default prediction method */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Prediction")
    ETrajectoryType DefaultPredictionMethod = ETrajectoryType::Ensemble;

    /** Enable automatic validation of predictions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Validation")
    bool bAutoValidate = true;

    /** Validation window (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Validation")
    float ValidationWindow = 5.0f;

    /** Enable ensemble predictions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Ensemble")
    bool bEnableEnsemble = true;

    /** Polynomial degree for polynomial fitting */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Fitting")
    int32 PolynomialDegree = 3;

    /** Confidence decay rate per step */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Confidence")
    float ConfidenceDecayRate = 0.1f;

    /** Base confidence for predictions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Confidence")
    float BaseConfidence = 0.8f;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnTrajectoryUpdated OnTrajectoryUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPredictionGenerated OnPredictionGenerated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPredictionValidated OnPredictionValidated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPredictionError OnPredictionError;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnEnsemblePrediction OnEnsemblePrediction;

    // ========================================
    // PUBLIC API - TRAJECTORY MANAGEMENT
    // ========================================

    /** Create a new trajectory for state tracking */
    UFUNCTION(BlueprintCallable, Category = "Trajectory")
    FString CreateTrajectory(EStateDomain Domain, const FString& Name = TEXT(""));

    /** Add observation to trajectory */
    UFUNCTION(BlueprintCallable, Category = "Trajectory")
    void AddObservation(const FString& TrajectoryID, const TArray<float>& StateVector, float Reliability = 1.0f);

    /** Get trajectory by ID */
    UFUNCTION(BlueprintPure, Category = "Trajectory")
    FStateTrajectory GetTrajectory(const FString& TrajectoryID) const;

    /** Get all trajectories for domain */
    UFUNCTION(BlueprintPure, Category = "Trajectory")
    TArray<FStateTrajectory> GetTrajectoriesForDomain(EStateDomain Domain) const;

    /** Fit trajectory with specified method */
    UFUNCTION(BlueprintCallable, Category = "Trajectory")
    float FitTrajectory(const FString& TrajectoryID, ETrajectoryType Method);

    /** Clear trajectory history */
    UFUNCTION(BlueprintCallable, Category = "Trajectory")
    void ClearTrajectory(const FString& TrajectoryID);

    /** Remove trajectory */
    UFUNCTION(BlueprintCallable, Category = "Trajectory")
    void RemoveTrajectory(const FString& TrajectoryID);

    // ========================================
    // PUBLIC API - PREDICTION
    // ========================================

    /** Generate prediction for future state */
    UFUNCTION(BlueprintCallable, Category = "Prediction")
    FFuturePrediction PredictFutureState(const FString& TrajectoryID, int32 StepsAhead, ETrajectoryType Method = ETrajectoryType::Ensemble);

    /** Generate predictions for multiple horizons */
    UFUNCTION(BlueprintCallable, Category = "Prediction")
    TArray<FFuturePrediction> PredictMultipleHorizons(const FString& TrajectoryID, const TArray<int32>& StepsAhead);

    /** Generate ensemble prediction combining all methods */
    UFUNCTION(BlueprintCallable, Category = "Prediction")
    FEnsemblePrediction GenerateEnsemblePrediction(const FString& TrajectoryID, int32 StepsAhead);

    /** Get prediction by ID */
    UFUNCTION(BlueprintPure, Category = "Prediction")
    FFuturePrediction GetPrediction(int32 PredictionID) const;

    /** Get recent predictions */
    UFUNCTION(BlueprintPure, Category = "Prediction")
    TArray<FFuturePrediction> GetRecentPredictions(int32 Count) const;

    /** Get pending predictions (not yet validated) */
    UFUNCTION(BlueprintPure, Category = "Prediction")
    TArray<FFuturePrediction> GetPendingPredictions() const;

    // ========================================
    // PUBLIC API - VALIDATION
    // ========================================

    /** Validate prediction against actual state */
    UFUNCTION(BlueprintCallable, Category = "Validation")
    FPredictionValidation ValidatePrediction(int32 PredictionID, const TArray<float>& ActualState);

    /** Auto-validate pending predictions */
    UFUNCTION(BlueprintCallable, Category = "Validation")
    TArray<FPredictionValidation> AutoValidatePredictions(const TArray<float>& CurrentState);

    /** Get validation history */
    UFUNCTION(BlueprintPure, Category = "Validation")
    TArray<FPredictionValidation> GetValidationHistory(int32 Count) const;

    /** Get prediction statistics */
    UFUNCTION(BlueprintPure, Category = "Validation")
    FPredictionStatistics GetPredictionStatistics() const;

    // ========================================
    // PUBLIC API - CONFIDENCE & UNCERTAINTY
    // ========================================

    /** Compute confidence for prediction */
    UFUNCTION(BlueprintPure, Category = "Confidence")
    float ComputePredictionConfidence(const FString& TrajectoryID, int32 StepsAhead) const;

    /** Get uncertainty bounds for prediction */
    UFUNCTION(BlueprintPure, Category = "Confidence")
    TArray<float> ComputeUncertaintyBounds(const FString& TrajectoryID, int32 StepsAhead) const;

    /** Classify confidence level */
    UFUNCTION(BlueprintPure, Category = "Confidence")
    EPredictionConfidence ClassifyConfidence(float ConfidenceScore) const;

    // ========================================
    // PUBLIC API - ECHOBEATS INTEGRATION
    // ========================================

    /** Process echobeat step for predictions */
    UFUNCTION(BlueprintCallable, Category = "Echobeats")
    void ProcessEchobeatStep(int32 Step);

    /** Get prediction for specific echobeat phase */
    UFUNCTION(BlueprintPure, Category = "Echobeats")
    FFuturePrediction GetEchobeatPhasePrediction(int32 Phase) const;

    /** Update from cognitive state */
    UFUNCTION(BlueprintCallable, Category = "Echobeats")
    void UpdateFromCognitiveState(const TArray<float>& CognitiveState);

    // ========================================
    // PUBLIC API - METHOD SELECTION
    // ========================================

    /** Get best prediction method for trajectory */
    UFUNCTION(BlueprintPure, Category = "Methods")
    ETrajectoryType GetBestMethod(const FString& TrajectoryID) const;

    /** Get method weights for ensemble */
    UFUNCTION(BlueprintPure, Category = "Methods")
    TArray<float> GetMethodWeights() const;

    /** Update method weights based on performance */
    UFUNCTION(BlueprintCallable, Category = "Methods")
    void UpdateMethodWeights();

protected:
    // Component references
    UPROPERTY()
    UPredictiveAdaptationEngine* PredictiveEngine;

    UPROPERTY()
    UActiveInferenceEngine* ActiveInference;

    UPROPERTY()
    UEchobeatsStreamEngine* EchobeatsEngine;

    UPROPERTY()
    UDeepTreeEchoReservoir* Reservoir;

    // Internal state
    TMap<FString, FStateTrajectory> Trajectories;
    TMap<int32, FFuturePrediction> Predictions;
    TArray<FPredictionValidation> ValidationHistory;
    TMap<ETrajectoryType, float> MethodWeights;
    FPredictionStatistics Statistics;
    
    int32 NextTrajectoryID = 1;
    int32 NextPredictionID = 1;
    int32 CurrentEchobeatStep = 1;

    // Internal methods
    void FindComponentReferences();
    void InitializeMethodWeights();
    void UpdatePendingValidations();
    
    TArray<float> PredictLinear(const FStateTrajectory& Trajectory, int32 StepsAhead) const;
    TArray<float> PredictPolynomial(const FStateTrajectory& Trajectory, int32 StepsAhead) const;
    TArray<float> PredictExponential(const FStateTrajectory& Trajectory, int32 StepsAhead) const;
    TArray<float> PredictReservoir(const FStateTrajectory& Trajectory, int32 StepsAhead) const;
    
    float ComputeGoodnessOfFit(const FStateTrajectory& Trajectory, const TArray<float>& FittedParams, ETrajectoryType Method) const;
    float ComputeMSE(const TArray<float>& Predicted, const TArray<float>& Actual) const;
    float ComputeMAE(const TArray<float>& Predicted, const TArray<float>& Actual) const;
    float ComputeCorrelation(const TArray<float>& A, const TArray<float>& B) const;
    
    EPredictionHorizon ClassifyHorizon(int32 StepsAhead) const;
    void UpdateStatistics(const FPredictionValidation& Validation, const FFuturePrediction& Prediction);
    void TrimPredictionHistory();
};

// SensorimotorIntegration.h
// 4E Embodied Cognition Sensorimotor Integration for Deep Tree Echo
// Implements the sensory-motor loop with affordance detection and action selection

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SensorimotorIntegration.generated.h"

// Forward declarations
class UCognitiveCycleManager;
class UWisdomCultivation;

/**
 * Sensory modality types
 */
UENUM(BlueprintType)
enum class ESensoryModality : uint8
{
    Visual          UMETA(DisplayName = "Visual"),
    Auditory        UMETA(DisplayName = "Auditory"),
    Tactile         UMETA(DisplayName = "Tactile"),
    Proprioceptive  UMETA(DisplayName = "Proprioceptive"),
    Vestibular      UMETA(DisplayName = "Vestibular"),
    Interoceptive   UMETA(DisplayName = "Interoceptive")
};

/**
 * Motor action types
 */
UENUM(BlueprintType)
enum class EMotorActionType : uint8
{
    Locomotion      UMETA(DisplayName = "Locomotion"),
    Manipulation    UMETA(DisplayName = "Manipulation"),
    Gesture         UMETA(DisplayName = "Gesture"),
    Expression      UMETA(DisplayName = "Expression"),
    Vocalization    UMETA(DisplayName = "Vocalization"),
    Orientation     UMETA(DisplayName = "Orientation")
};

/**
 * Embodiment dimension (4E cognition)
 */
UENUM(BlueprintType)
enum class EEmbodimentDimension : uint8
{
    Embodied        UMETA(DisplayName = "Embodied"),
    Embedded        UMETA(DisplayName = "Embedded"),
    Enacted         UMETA(DisplayName = "Enacted"),
    Extended        UMETA(DisplayName = "Extended")
};

/**
 * Sensory input data
 */
USTRUCT(BlueprintType)
struct FSensoryInput
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString InputID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESensoryModality Modality = ESensoryModality::Visual;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> RawData;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Intensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Salience = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector SpatialLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;
};

/**
 * Motor command data
 */
USTRUCT(BlueprintType)
struct FMotorCommand
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CommandID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMotorActionType ActionType = EMotorActionType::Locomotion;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> Parameters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Intensity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Duration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector TargetLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator TargetRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Priority = 0.5f;
};

/**
 * Detected affordance
 */
USTRUCT(BlueprintType)
struct FDetectedAffordance
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AffordanceID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ObjectID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ActionVerb; // e.g., "grasp", "sit", "push"

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Availability = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SkillMatch = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ContextualRelevance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExpectedUtility = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Risk = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Location = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FMotorCommand> RequiredActions;
};

/**
 * Body schema representation
 */
USTRUCT(BlueprintType)
struct FBodySchema
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Position = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator Orientation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Velocity = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector AngularVelocity = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, FTransform> LimbTransforms;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, float> JointAngles;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnergyLevel = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Stability = 1.0f;
};

/**
 * Peripersonal space representation
 */
USTRUCT(BlueprintType)
struct FPeripersonalSpace
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ReachRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float GraspRadius = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FDetectedAffordance> NearbyAffordances;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> ObjectsInReach;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Crowdedness = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ThreatLevel = 0.0f;
};

/**
 * Sensorimotor contingency
 */
USTRUCT(BlueprintType)
struct FSensorimotorContingency
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ContingencyID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FMotorCommand Action;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FSensoryInput> ExpectedOutcome;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FSensoryInput> ActualOutcome;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PredictionError = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ExperienceCount = 0;
};

/**
 * 4E Embodiment state
 */
USTRUCT(BlueprintType)
struct F4EEmbodimentState
{
    GENERATED_BODY()

    // Embodied: Body-based cognition
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmbodiedLevel = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FBodySchema BodySchema;

    // Embedded: Environmental coupling
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmbeddedLevel = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FPeripersonalSpace PeripersonalSpace;

    // Enacted: Action-based understanding
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnactedLevel = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FSensorimotorContingency> ActiveContingencies;

    // Extended: Tool/environment extension
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExtendedLevel = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> ExtendedTools;

    // Overall integration
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OverallEmbodiment = 0.5f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSensoryInputReceived, const FSensoryInput&, Input);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMotorCommandIssued, const FMotorCommand&, Command);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAffordanceDetected, const FDetectedAffordance&, Affordance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPredictionError, const FString&, ContingencyID, float, Error);

/**
 * Sensorimotor Integration Component
 * Implements 4E embodied cognition for the Deep Tree Echo avatar
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class UNREALECHO_API USensorimotorIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    USensorimotorIntegration();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    bool bEnableSensorimotorLoop = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    bool bEnableAffordanceDetection = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    bool bEnablePredictiveCoding = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float SensoryIntegrationRate = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float MotorExecutionRate = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float AffordanceDetectionRadius = 500.0f;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSensoryInputReceived OnSensoryInput;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnMotorCommandIssued OnMotorCommand;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnAffordanceDetected OnAffordanceDetected;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPredictionError OnPredictionError;

    // ========================================
    // SENSORY INPUT
    // ========================================

    /** Register sensory input */
    UFUNCTION(BlueprintCallable, Category = "Sensory")
    void RegisterSensoryInput(const FSensoryInput& Input);

    /** Get all current sensory inputs */
    UFUNCTION(BlueprintPure, Category = "Sensory")
    TArray<FSensoryInput> GetCurrentSensoryInputs() const;

    /** Get inputs by modality */
    UFUNCTION(BlueprintPure, Category = "Sensory")
    TArray<FSensoryInput> GetInputsByModality(ESensoryModality Modality) const;

    /** Get the most salient input */
    UFUNCTION(BlueprintPure, Category = "Sensory")
    FSensoryInput GetMostSalientInput() const;

    /** Clear old sensory inputs */
    UFUNCTION(BlueprintCallable, Category = "Sensory")
    void ClearOldInputs(float MaxAge);

    // ========================================
    // MOTOR OUTPUT
    // ========================================

    /** Issue a motor command */
    UFUNCTION(BlueprintCallable, Category = "Motor")
    void IssueMotorCommand(const FMotorCommand& Command);

    /** Get pending motor commands */
    UFUNCTION(BlueprintPure, Category = "Motor")
    TArray<FMotorCommand> GetPendingCommands() const;

    /** Cancel a motor command */
    UFUNCTION(BlueprintCallable, Category = "Motor")
    void CancelCommand(const FString& CommandID);

    /** Cancel all commands */
    UFUNCTION(BlueprintCallable, Category = "Motor")
    void CancelAllCommands();

    /** Get current motor state */
    UFUNCTION(BlueprintPure, Category = "Motor")
    FMotorCommand GetCurrentCommand() const;

    // ========================================
    // AFFORDANCE DETECTION
    // ========================================

    /** Detect affordances in the environment */
    UFUNCTION(BlueprintCallable, Category = "Affordances")
    TArray<FDetectedAffordance> DetectAffordances();

    /** Get the best affordance for current goals */
    UFUNCTION(BlueprintPure, Category = "Affordances")
    FDetectedAffordance GetBestAffordance() const;

    /** Select an affordance for action */
    UFUNCTION(BlueprintCallable, Category = "Affordances")
    void SelectAffordance(const FString& AffordanceID);

    /** Get all detected affordances */
    UFUNCTION(BlueprintPure, Category = "Affordances")
    TArray<FDetectedAffordance> GetDetectedAffordances() const;

    // ========================================
    // BODY SCHEMA
    // ========================================

    /** Update body schema from sensors */
    UFUNCTION(BlueprintCallable, Category = "Body Schema")
    void UpdateBodySchema();

    /** Get current body schema */
    UFUNCTION(BlueprintPure, Category = "Body Schema")
    FBodySchema GetBodySchema() const;

    /** Set limb transform */
    UFUNCTION(BlueprintCallable, Category = "Body Schema")
    void SetLimbTransform(const FString& LimbName, const FTransform& Transform);

    /** Get limb transform */
    UFUNCTION(BlueprintPure, Category = "Body Schema")
    FTransform GetLimbTransform(const FString& LimbName) const;

    // ========================================
    // PERIPERSONAL SPACE
    // ========================================

    /** Update peripersonal space */
    UFUNCTION(BlueprintCallable, Category = "Peripersonal Space")
    void UpdatePeripersonalSpace();

    /** Get peripersonal space */
    UFUNCTION(BlueprintPure, Category = "Peripersonal Space")
    FPeripersonalSpace GetPeripersonalSpace() const;

    /** Check if object is in reach */
    UFUNCTION(BlueprintPure, Category = "Peripersonal Space")
    bool IsObjectInReach(const FString& ObjectID) const;

    // ========================================
    // SENSORIMOTOR CONTINGENCIES
    // ========================================

    /** Learn a new contingency */
    UFUNCTION(BlueprintCallable, Category = "Contingencies")
    void LearnContingency(const FMotorCommand& Action, const TArray<FSensoryInput>& Outcome);

    /** Predict outcome of action */
    UFUNCTION(BlueprintPure, Category = "Contingencies")
    TArray<FSensoryInput> PredictOutcome(const FMotorCommand& Action) const;

    /** Update contingency with actual outcome */
    UFUNCTION(BlueprintCallable, Category = "Contingencies")
    void UpdateContingency(const FString& ContingencyID, const TArray<FSensoryInput>& ActualOutcome);

    /** Get prediction error for contingency */
    UFUNCTION(BlueprintPure, Category = "Contingencies")
    float GetPredictionError(const FString& ContingencyID) const;

    // ========================================
    // 4E EMBODIMENT
    // ========================================

    /** Get 4E embodiment state */
    UFUNCTION(BlueprintPure, Category = "4E Embodiment")
    F4EEmbodimentState Get4EState() const;

    /** Get embodiment level for dimension */
    UFUNCTION(BlueprintPure, Category = "4E Embodiment")
    float GetEmbodimentLevel(EEmbodimentDimension Dimension) const;

    /** Register extended tool */
    UFUNCTION(BlueprintCallable, Category = "4E Embodiment")
    void RegisterExtendedTool(const FString& ToolID);

    /** Unregister extended tool */
    UFUNCTION(BlueprintCallable, Category = "4E Embodiment")
    void UnregisterExtendedTool(const FString& ToolID);

    /** Get overall embodiment level */
    UFUNCTION(BlueprintPure, Category = "4E Embodiment")
    float GetOverallEmbodiment() const;

protected:
    // Component references
    UPROPERTY()
    UCognitiveCycleManager* CycleManager;

    UPROPERTY()
    UWisdomCultivation* WisdomComponent;

    // Internal state
    TArray<FSensoryInput> CurrentInputs;
    TArray<FMotorCommand> PendingCommands;
    TArray<FDetectedAffordance> DetectedAffordances;
    TArray<FSensorimotorContingency> LearnedContingencies;
    F4EEmbodimentState EmbodimentState;

    FMotorCommand CurrentCommand;
    FString SelectedAffordanceID;

    float SensoryTimer = 0.0f;
    float MotorTimer = 0.0f;

    // Internal methods
    void FindComponentReferences();
    void ProcessSensoryIntegration(float DeltaTime);
    void ProcessMotorExecution(float DeltaTime);
    void ProcessAffordanceDetection(float DeltaTime);
    void ProcessPredictiveCoding(float DeltaTime);

    void IntegrateSensoryInputs();
    void ExecuteMotorCommand(const FMotorCommand& Command);
    void UpdateAffordanceScores();

    void Update4EState(float DeltaTime);
    void UpdateEmbodiedLevel();
    void UpdateEmbeddedLevel();
    void UpdateEnactedLevel();
    void UpdateExtendedLevel();

    float ComputeSensoryDistance(const TArray<FSensoryInput>& A, const TArray<FSensoryInput>& B) const;
    FString GenerateContingencyID() const;
    FString GenerateAffordanceID() const;

    int32 ContingencyIDCounter = 0;
    int32 AffordanceIDCounter = 0;
};

#pragma once

/**
 * Sensorimotor Integration for Deep-Tree-Echo Avatar Control
 * 
 * MERGED VERSION: Contains all functionality from both Sensorimotor and Embodied versions
 * 
 * Implements the sensorimotor coupling layer that bridges perception and action
 * for embodied avatar control. Based on 4E cognitive science principles:
 * - Embodied: Cognition shaped by body morphology and capabilities
 * - Embedded: Cognition situated in environmental context
 * - Enacted: Knowledge through sensorimotor interaction
 * - Extended: Cognitive processes distributed across agent-environment
 * 
 * Integrates with:
 * - CognitiveCycleManager: 12-step cognitive loop
 * - AXIOMActiveInference: Active inference planning
 * - EntelechyFramework: Goal-directed actualization
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../Cognitive/CognitiveCycleManager.h"
#include "SensorimotorIntegration.generated.h"

// ========================================
// FORWARD DECLARATIONS
// ========================================

class UAXIOMActiveInference;
class UEntelechyFramework;
class UWisdomCultivation;

// ========================================
// ENUMERATIONS
// ========================================

/**
 * Sensory Modality Types
 */
UENUM(BlueprintType)
enum class ESensoryModality : uint8
{
    /** Visual perception */
    Visual UMETA(DisplayName = "Visual"),
    
    /** Auditory perception */
    Auditory UMETA(DisplayName = "Auditory"),
    
    /** Proprioceptive (body position) */
    Proprioceptive UMETA(DisplayName = "Proprioceptive"),
    
    /** Vestibular (balance/orientation) */
    Vestibular UMETA(DisplayName = "Vestibular"),
    
    /** Tactile (touch/contact) */
    Tactile UMETA(DisplayName = "Tactile"),
    
    /** Interoceptive (internal state) */
    Interoceptive UMETA(DisplayName = "Interoceptive")
};

/**
 * Motor Effector Types
 */
UENUM(BlueprintType)
enum class EMotorEffector : uint8
{
    /** Locomotion (movement) */
    Locomotion UMETA(DisplayName = "Locomotion"),
    
    /** Manipulation (hands/arms) */
    Manipulation UMETA(DisplayName = "Manipulation"),
    
    /** Gaze (eye/head direction) */
    Gaze UMETA(DisplayName = "Gaze"),
    
    /** Vocalization (speech/sound) */
    Vocalization UMETA(DisplayName = "Vocalization"),
    
    /** Expression (facial/body) */
    Expression UMETA(DisplayName = "Expression"),
    
    /** Posture (body stance) */
    Posture UMETA(DisplayName = "Posture")
};

/**
 * Motor Action Types (legacy)
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
 * Sensorimotor Coupling Mode
 */
UENUM(BlueprintType)
enum class ECouplingMode : uint8
{
    /** Reactive: Direct stimulus-response */
    Reactive UMETA(DisplayName = "Reactive"),
    
    /** Predictive: Anticipatory control */
    Predictive UMETA(DisplayName = "Predictive"),
    
    /** Exploratory: Active sensing */
    Exploratory UMETA(DisplayName = "Exploratory"),
    
    /** Habitual: Automated skill execution */
    Habitual UMETA(DisplayName = "Habitual")
};

/**
 * Body Schema State
 */
UENUM(BlueprintType)
enum class EBodySchemaState : uint8
{
    /** Calibrating body model */
    Calibrating UMETA(DisplayName = "Calibrating"),
    
    /** Normal operation */
    Normal UMETA(DisplayName = "Normal"),
    
    /** Adapting to change */
    Adapting UMETA(DisplayName = "Adapting"),
    
    /** Extended (tool use) */
    Extended UMETA(DisplayName = "Extended")
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

// ========================================
// STRUCTURES
// ========================================

/**
 * Sensory Channel - Single modality input stream
 */
USTRUCT(BlueprintType)
struct FSensoryChannel
{
    GENERATED_BODY()

    /** Channel identifier */
    UPROPERTY(BlueprintReadWrite)
    FName ChannelID;

    /** Sensory modality */
    UPROPERTY(BlueprintReadWrite)
    ESensoryModality Modality = ESensoryModality::Visual;

    /** Raw sensory data (normalized 0-1) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> RawData;

    /** Processed features */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> Features;

    /** Attention weight (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float AttentionWeight = 1.0f;

    /** Reliability estimate (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Reliability = 1.0f;

    /** Timestamp of last update */
    UPROPERTY(BlueprintReadWrite)
    float LastUpdateTime = 0.0f;

    /** Prediction error from last cycle */
    UPROPERTY(BlueprintReadWrite)
    float PredictionError = 0.0f;
};

/**
 * Sensory input data (legacy)
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
 * Motor Channel - Single effector output stream
 */
USTRUCT(BlueprintType)
struct FMotorChannel
{
    GENERATED_BODY()

    /** Channel identifier */
    UPROPERTY(BlueprintReadWrite)
    FName ChannelID;

    /** Motor effector type */
    UPROPERTY(BlueprintReadWrite)
    EMotorEffector Effector = EMotorEffector::Locomotion;

    /** Target command (normalized) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> TargetCommand;

    /** Current state feedback */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> CurrentState;

    /** Command gain/strength (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float CommandGain = 1.0f;

    /** Execution confidence (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Confidence = 1.0f;

    /** Is currently executing */
    UPROPERTY(BlueprintReadWrite)
    bool bIsExecuting = false;

    /** Execution progress (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Progress = 0.0f;
};

/**
 * Motor command data (legacy)
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
 * Sensorimotor Contingency - Learned action-perception coupling
 */
USTRUCT(BlueprintType)
struct FSensorimotorContingency
{
    GENERATED_BODY()

    /** Contingency identifier */
    UPROPERTY(BlueprintReadWrite)
    int32 ContingencyID = 0;

    /** String-based contingency ID (legacy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ContingencyIDString;

    /** Associated motor action pattern */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> ActionPattern;

    /** Motor command (legacy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FMotorCommand Action;

    /** Expected sensory consequence */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> ExpectedSensoryChange;

    /** Expected outcome (legacy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FSensoryInput> ExpectedOutcome;

    /** Actual outcome (legacy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FSensoryInput> ActualOutcome;

    /** Sensory modalities involved */
    UPROPERTY(BlueprintReadWrite)
    TArray<ESensoryModality> InvolvedModalities;

    /** Motor effectors involved */
    UPROPERTY(BlueprintReadWrite)
    TArray<EMotorEffector> InvolvedEffectors;

    /** Reliability of this contingency */
    UPROPERTY(BlueprintReadWrite)
    float Reliability = 0.5f;

    /** Prediction error */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PredictionError = 0.0f;

    /** Confidence */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence = 0.5f;

    /** Number of times activated */
    UPROPERTY(BlueprintReadWrite)
    int32 ActivationCount = 0;

    /** Experience count (legacy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ExperienceCount = 0;

    /** Last activation time */
    UPROPERTY(BlueprintReadWrite)
    float LastActivationTime = 0.0f;
};

/**
 * Affordance - Action possibility in current context
 */
USTRUCT(BlueprintType)
struct FAffordance
{
    GENERATED_BODY()

    /** Affordance identifier */
    UPROPERTY(BlueprintReadWrite)
    int32 AffordanceID = 0;

    /** Action type this affords */
    UPROPERTY(BlueprintReadWrite)
    FName ActionType;

    /** Target object/location (if applicable) */
    UPROPERTY(BlueprintReadWrite)
    FVector TargetLocation = FVector::ZeroVector;

    /** Affordance strength (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Strength = 0.0f;

    /** Expected utility if acted upon */
    UPROPERTY(BlueprintReadWrite)
    float ExpectedUtility = 0.0f;

    /** Required motor capabilities */
    UPROPERTY(BlueprintReadWrite)
    TArray<EMotorEffector> RequiredEffectors;

    /** Relevant sensory cues */
    UPROPERTY(BlueprintReadWrite)
    TArray<ESensoryModality> RelevantModalities;

    /** Is currently reachable */
    UPROPERTY(BlueprintReadWrite)
    bool bIsReachable = false;
};

/**
 * Detected affordance (legacy)
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
 * Body Schema - Internal model of body capabilities
 */
USTRUCT(BlueprintType)
struct FBodySchema
{
    GENERATED_BODY()

    /** Schema state */
    UPROPERTY(BlueprintReadWrite)
    EBodySchemaState State = EBodySchemaState::Normal;

    /** Body position */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Position = FVector::ZeroVector;

    /** Body orientation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator Orientation = FRotator::ZeroRotator;

    /** Body velocity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Velocity = FVector::ZeroVector;

    /** Angular velocity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector AngularVelocity = FVector::ZeroVector;

    /** Body part positions (relative to root) */
    UPROPERTY(BlueprintReadWrite)
    TMap<FName, FVector> PartPositions;

    /** Body part orientations */
    UPROPERTY(BlueprintReadWrite)
    TMap<FName, FRotator> PartOrientations;

    /** Limb transforms (legacy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, FTransform> LimbTransforms;

    /** Joint angles */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, float> JointAngles;

    /** Reachable workspace bounds */
    UPROPERTY(BlueprintReadWrite)
    FBox ReachableSpace;

    /** Movement capabilities per effector */
    UPROPERTY(BlueprintReadWrite)
    TMap<EMotorEffector, float> EffectorCapabilities;

    /** Current tool extension (if any) */
    UPROPERTY(BlueprintReadWrite)
    FName CurrentTool;

    /** Tool extension offset */
    UPROPERTY(BlueprintReadWrite)
    FVector ToolExtension = FVector::ZeroVector;

    /** Energy level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnergyLevel = 1.0f;

    /** Stability */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Stability = 1.0f;
};

/**
 * Peripersonal space representation (legacy)
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
 * 4E Embodiment state (legacy)
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
 * Sensorimotor State - Complete state snapshot
 */
USTRUCT(BlueprintType)
struct FSensorimotorState
{
    GENERATED_BODY()

    /** Timestamp */
    UPROPERTY(BlueprintReadWrite)
    float Timestamp = 0.0f;

    /** Active sensory channels */
    UPROPERTY(BlueprintReadWrite)
    TArray<FSensoryChannel> SensoryChannels;

    /** Active motor channels */
    UPROPERTY(BlueprintReadWrite)
    TArray<FMotorChannel> MotorChannels;

    /** Current body schema */
    UPROPERTY(BlueprintReadWrite)
    FBodySchema BodySchema;

    /** Detected affordances */
    UPROPERTY(BlueprintReadWrite)
    TArray<FAffordance> Affordances;

    /** Current coupling mode */
    UPROPERTY(BlueprintReadWrite)
    ECouplingMode CouplingMode = ECouplingMode::Reactive;

    /** Overall prediction error */
    UPROPERTY(BlueprintReadWrite)
    float TotalPredictionError = 0.0f;

    /** Sensorimotor coherence (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Coherence = 1.0f;
};

/**
 * Sensorimotor Configuration
 */
USTRUCT(BlueprintType)
struct FSensorimotorConfig
{
    GENERATED_BODY()

    /** Prediction horizon (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float PredictionHorizon = 0.1f;

    /** Learning rate for contingencies */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.001", ClampMax = "0.5"))
    float ContingencyLearningRate = 0.01f;

    /** Attention decay rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AttentionDecay = 0.1f;

    /** Prediction error threshold for adaptation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float AdaptationThreshold = 0.3f;

    /** Maximum contingencies to store */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "10", ClampMax = "1000"))
    int32 MaxContingencies = 100;

    /** Enable predictive control */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnablePredictiveControl = true;

    /** Enable affordance detection */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableAffordanceDetection = true;

    /** Enable body schema adaptation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableBodySchemaAdaptation = true;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSensoryInputReceived, const FSensoryInput&, Input);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMotorCommandIssued, const FMotorCommand&, Command);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAffordanceDetectedLegacy, const FDetectedAffordance&, Affordance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPredictionErrorLegacy, const FString&, ContingencyID, float, Error);

/**
 * Sensorimotor Integration Component
 * 
 * MERGED VERSION: Contains all functionality from both Sensorimotor and Embodied implementations
 * 
 * Manages the bidirectional coupling between perception and action
 * for embodied avatar control in the Deep-Tree-Echo framework.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API USensorimotorIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    USensorimotorIntegration();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Sensorimotor configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensorimotor|Config")
    FSensorimotorConfig Config;

    /** Reference to CognitiveCycleManager */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensorimotor|Config")
    UCognitiveCycleManager* CognitiveCycleManager;

    /** Reference to AXIOM Active Inference */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sensorimotor|Config")
    UAXIOMActiveInference* AXIOMComponent;

    /** Enable sensorimotor loop (legacy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    bool bEnableSensorimotorLoop = true;

    /** Enable affordance detection (legacy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    bool bEnableAffordanceDetectionLegacy = true;

    /** Enable predictive coding (legacy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    bool bEnablePredictiveCoding = true;

    /** Sensory integration rate (legacy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float SensoryIntegrationRate = 0.1f;

    /** Motor execution rate (legacy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float MotorExecutionRate = 0.05f;

    /** Affordance detection radius (legacy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float AffordanceDetectionRadius = 500.0f;

    // ========================================
    // STATE
    // ========================================

    /** Current sensorimotor state */
    UPROPERTY(BlueprintReadOnly, Category = "Sensorimotor|State")
    FSensorimotorState CurrentState;

    /** Previous sensorimotor state */
    UPROPERTY(BlueprintReadOnly, Category = "Sensorimotor|State")
    FSensorimotorState PreviousState;

    /** Learned sensorimotor contingencies */
    UPROPERTY(BlueprintReadOnly, Category = "Sensorimotor|State")
    TArray<FSensorimotorContingency> Contingencies;

    // ========================================
    // EVENTS (New API)
    // ========================================

    /** Called when new contingency is learned */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnContingencyLearned, int32, ContingencyID);
    UPROPERTY(BlueprintAssignable, Category = "Sensorimotor|Events")
    FOnContingencyLearned OnContingencyLearned;

    /** Called when affordance is detected */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAffordanceDetected, FName, ActionType, float, Strength);
    UPROPERTY(BlueprintAssignable, Category = "Sensorimotor|Events")
    FOnAffordanceDetected OnAffordanceDetected;

    /** Called when prediction error exceeds threshold */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPredictionError, FName, ChannelID, float, Error);
    UPROPERTY(BlueprintAssignable, Category = "Sensorimotor|Events")
    FOnPredictionError OnPredictionError;

    /** Called when body schema adapts */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBodySchemaAdapted, EBodySchemaState, NewState);
    UPROPERTY(BlueprintAssignable, Category = "Sensorimotor|Events")
    FOnBodySchemaAdapted OnBodySchemaAdapted;

    // ========================================
    // EVENTS (Legacy API)
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSensoryInputReceived OnSensoryInput;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnMotorCommandIssued OnMotorCommand;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnAffordanceDetectedLegacy OnAffordanceDetectedLegacy;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPredictionErrorLegacy OnPredictionErrorLegacy;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize sensorimotor system */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor")
    void Initialize();

    /** Reset to initial state */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor")
    void Reset();

    // ========================================
    // PUBLIC API - SENSORY INPUT (New)
    // ========================================

    /** Register a sensory channel */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Sensory")
    void RegisterSensoryChannel(FName ChannelID, ESensoryModality Modality, int32 DataDimension);

    /** Update sensory channel data */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Sensory")
    void UpdateSensoryChannel(FName ChannelID, const TArray<float>& RawData);

    /** Set attention weight for channel */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Sensory")
    void SetChannelAttention(FName ChannelID, float AttentionWeight);

    /** Get integrated sensory features */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Sensory")
    TArray<float> GetIntegratedSensoryFeatures() const;

    /** Compute prediction error for sensory channel */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Sensory")
    float ComputeSensoryPredictionError(FName ChannelID) const;

    // ========================================
    // PUBLIC API - SENSORY INPUT (Legacy)
    // ========================================

    /** Register sensory input (legacy) */
    UFUNCTION(BlueprintCallable, Category = "Sensory")
    void RegisterSensoryInput(const FSensoryInput& Input);

    /** Get all current sensory inputs (legacy) */
    UFUNCTION(BlueprintPure, Category = "Sensory")
    TArray<FSensoryInput> GetCurrentSensoryInputs() const;

    /** Get inputs by modality (legacy) */
    UFUNCTION(BlueprintPure, Category = "Sensory")
    TArray<FSensoryInput> GetInputsByModality(ESensoryModality Modality) const;

    /** Get the most salient input (legacy) */
    UFUNCTION(BlueprintPure, Category = "Sensory")
    FSensoryInput GetMostSalientInput() const;

    /** Clear old sensory inputs (legacy) */
    UFUNCTION(BlueprintCallable, Category = "Sensory")
    void ClearOldInputs(float MaxAge);

    // ========================================
    // PUBLIC API - MOTOR OUTPUT (New)
    // ========================================

    /** Register a motor channel */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Motor")
    void RegisterMotorChannel(FName ChannelID, EMotorEffector Effector, int32 CommandDimension);

    /** Set motor command for channel */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Motor")
    void SetMotorCommand(FName ChannelID, const TArray<float>& Command, float Gain = 1.0f);

    /** Update motor state feedback */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Motor")
    void UpdateMotorFeedback(FName ChannelID, const TArray<float>& CurrentStateData);

    /** Get current motor commands for all channels */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Motor")
    TMap<FName, TArray<float>> GetAllMotorCommands() const;

    /** Execute motor command with predictive control */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Motor")
    void ExecuteWithPrediction(FName ChannelID);

    // ========================================
    // PUBLIC API - MOTOR OUTPUT (Legacy)
    // ========================================

    /** Issue a motor command (legacy) */
    UFUNCTION(BlueprintCallable, Category = "Motor")
    void IssueMotorCommand(const FMotorCommand& Command);

    /** Get pending motor commands (legacy) */
    UFUNCTION(BlueprintPure, Category = "Motor")
    TArray<FMotorCommand> GetPendingCommands() const;

    /** Cancel a motor command (legacy) */
    UFUNCTION(BlueprintCallable, Category = "Motor")
    void CancelCommand(const FString& CommandID);

    /** Cancel all commands (legacy) */
    UFUNCTION(BlueprintCallable, Category = "Motor")
    void CancelAllCommands();

    /** Get current motor state (legacy) */
    UFUNCTION(BlueprintPure, Category = "Motor")
    FMotorCommand GetCurrentCommand() const;

    // ========================================
    // PUBLIC API - CONTINGENCY LEARNING (New)
    // ========================================

    /** Learn new sensorimotor contingency */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Learning")
    int32 LearnContingency(const TArray<float>& ActionPattern, const TArray<float>& SensoryChange);

    /** Update contingency reliability */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Learning")
    void UpdateContingencyReliability(int32 ContingencyID, bool bSuccessful);

    /** Predict sensory consequence of action */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Learning")
    TArray<float> PredictSensoryConsequence(const TArray<float>& ActionPattern) const;

    /** Find matching contingency for current context */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Learning")
    int32 FindMatchingContingency(const TArray<float>& ActionPattern) const;

    // ========================================
    // PUBLIC API - CONTINGENCY LEARNING (Legacy)
    // ========================================

    /** Learn a new contingency (legacy) */
    UFUNCTION(BlueprintCallable, Category = "Contingencies")
    void LearnContingencyLegacy(const FMotorCommand& Action, const TArray<FSensoryInput>& Outcome);

    /** Predict outcome of action (legacy) */
    UFUNCTION(BlueprintPure, Category = "Contingencies")
    TArray<FSensoryInput> PredictOutcome(const FMotorCommand& Action) const;

    /** Update contingency with actual outcome (legacy) */
    UFUNCTION(BlueprintCallable, Category = "Contingencies")
    void UpdateContingency(const FString& ContingencyID, const TArray<FSensoryInput>& ActualOutcome);

    /** Get prediction error for contingency (legacy) */
    UFUNCTION(BlueprintPure, Category = "Contingencies")
    float GetPredictionError(const FString& ContingencyID) const;

    // ========================================
    // PUBLIC API - AFFORDANCE DETECTION (New)
    // ========================================

    /** Detect affordances in current sensory state */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Affordance")
    void DetectAffordances();

    /** Get affordances for specific action type */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Affordance")
    TArray<FAffordance> GetAffordancesForAction(FName ActionType) const;

    /** Get strongest affordance */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Affordance")
    FAffordance GetStrongestAffordance() const;

    /** Check if action is currently afforded */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|Affordance")
    bool IsActionAfforded(FName ActionType) const;

    // ========================================
    // PUBLIC API - AFFORDANCE DETECTION (Legacy)
    // ========================================

    /** Detect affordances in the environment (legacy) */
    UFUNCTION(BlueprintCallable, Category = "Affordances")
    TArray<FDetectedAffordance> DetectAffordancesLegacy();

    /** Get the best affordance for current goals (legacy) */
    UFUNCTION(BlueprintPure, Category = "Affordances")
    FDetectedAffordance GetBestAffordance() const;

    /** Select an affordance for action (legacy) */
    UFUNCTION(BlueprintCallable, Category = "Affordances")
    void SelectAffordance(const FString& AffordanceID);

    /** Get all detected affordances (legacy) */
    UFUNCTION(BlueprintPure, Category = "Affordances")
    TArray<FDetectedAffordance> GetDetectedAffordances() const;

    // ========================================
    // PUBLIC API - BODY SCHEMA (New)
    // ========================================

    /** Update body part position */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|BodySchema")
    void UpdateBodyPartPosition(FName PartName, FVector Position, FRotator Orientation);

    /** Extend body schema with tool */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|BodySchema")
    void ExtendBodySchema(FName ToolName, FVector Extension);

    /** Reset body schema to default */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|BodySchema")
    void ResetBodySchema();

    /** Check if position is reachable */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|BodySchema")
    bool IsPositionReachable(FVector Position) const;

    /** Get effector capability */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|BodySchema")
    float GetEffectorCapability(EMotorEffector Effector) const;

    // ========================================
    // PUBLIC API - BODY SCHEMA (Legacy)
    // ========================================

    /** Update body schema from sensors (legacy) */
    UFUNCTION(BlueprintCallable, Category = "Body Schema")
    void UpdateBodySchema();

    /** Get current body schema (legacy) */
    UFUNCTION(BlueprintPure, Category = "Body Schema")
    FBodySchema GetBodySchema() const;

    /** Set limb transform (legacy) */
    UFUNCTION(BlueprintCallable, Category = "Body Schema")
    void SetLimbTransform(const FString& LimbName, const FTransform& Transform);

    /** Get limb transform (legacy) */
    UFUNCTION(BlueprintPure, Category = "Body Schema")
    FTransform GetLimbTransform(const FString& LimbName) const;

    // ========================================
    // PUBLIC API - PERIPERSONAL SPACE (Legacy)
    // ========================================

    /** Update peripersonal space (legacy) */
    UFUNCTION(BlueprintCallable, Category = "Peripersonal Space")
    void UpdatePeripersonalSpace();

    /** Get peripersonal space (legacy) */
    UFUNCTION(BlueprintPure, Category = "Peripersonal Space")
    FPeripersonalSpace GetPeripersonalSpace() const;

    /** Check if object is in reach (legacy) */
    UFUNCTION(BlueprintPure, Category = "Peripersonal Space")
    bool IsObjectInReach(const FString& ObjectID) const;

    // ========================================
    // PUBLIC API - 4E EMBODIMENT (Legacy)
    // ========================================

    /** Get 4E embodiment state (legacy) */
    UFUNCTION(BlueprintPure, Category = "4E Embodiment")
    F4EEmbodimentState Get4EState() const;

    /** Get embodiment level for dimension (legacy) */
    UFUNCTION(BlueprintPure, Category = "4E Embodiment")
    float GetEmbodimentLevel(EEmbodimentDimension Dimension) const;

    /** Register extended tool (legacy) */
    UFUNCTION(BlueprintCallable, Category = "4E Embodiment")
    void RegisterExtendedTool(const FString& ToolID);

    /** Unregister extended tool (legacy) */
    UFUNCTION(BlueprintCallable, Category = "4E Embodiment")
    void UnregisterExtendedTool(const FString& ToolID);

    /** Get overall embodiment level (legacy) */
    UFUNCTION(BlueprintPure, Category = "4E Embodiment")
    float GetOverallEmbodiment() const;

    // ========================================
    // PUBLIC API - COGNITIVE CYCLE INTEGRATION
    // ========================================

    /** Process cognitive cycle step */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|CognitiveCycle")
    void ProcessCognitiveStep(int32 GlobalStep, EConsciousnessStream ActiveStream);

    /** Get salience map for perception stream */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|CognitiveCycle")
    TArray<float> GetSalienceMap() const;

    /** Get action readiness for action stream */
    UFUNCTION(BlueprintCallable, Category = "Sensorimotor|CognitiveCycle")
    float GetActionReadiness() const;

protected:
    virtual void BeginPlay() override;

    // Component references (legacy)
    UPROPERTY()
    UWisdomCultivation* WisdomComponent;

    // Internal state (legacy)
    TArray<FSensoryInput> CurrentInputs;
    TArray<FMotorCommand> PendingCommands;
    TArray<FDetectedAffordance> DetectedAffordancesLegacy;
    TArray<FSensorimotorContingency> LearnedContingencies;
    F4EEmbodimentState EmbodimentState;

    FMotorCommand CurrentCommand;
    FString SelectedAffordanceID;

    float SensoryTimer = 0.0f;
    float MotorTimer = 0.0f;

private:
    // ========================================
    // INTERNAL METHODS (New)
    // ========================================

    /** Process sensory prediction */
    void ProcessSensoryPrediction();

    /** Process motor prediction */
    void ProcessMotorPrediction();

    /** Update attention based on prediction error */
    void UpdateAttention();

    /** Consolidate contingencies (merge similar, prune weak) */
    void ConsolidateContingencies();

    /** Adapt body schema based on feedback */
    void AdaptBodySchema();

    /** Compute feature similarity */
    float ComputeFeatureSimilarity(const TArray<float>& A, const TArray<float>& B) const;

    /** Normalize feature vector */
    TArray<float> NormalizeFeatures(const TArray<float>& Features) const;

    // ========================================
    // INTERNAL METHODS (Legacy)
    // ========================================

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

    // Internal state
    float AccumulatedTime = 0.0f;
    int32 LastProcessedStep = -1;
    TMap<FName, TArray<float>> PredictedSensory;
    TMap<FName, TArray<float>> PredictedMotor;

    int32 ContingencyIDCounter = 0;
    int32 AffordanceIDCounter = 0;
};

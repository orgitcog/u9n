#pragma once

/**
 * AXIOM Active Inference Integration for Deep-Tree-Echo
 * 
 * Implements the AXIOM (Active eXpanding Inference with Object-centric Models)
 * architecture for game learning and avatar control, integrated with the
 * CognitiveCycleManager's 3-stream, 12-step cognitive cycle.
 * 
 * Core Components:
 * - sMM (Slot Mixture Model): Object-centric perception from pixels
 * - iMM (Identity Mixture Model): Object type classification
 * - tMM (Transition Mixture Model): Motion prototype learning
 * - rMM (Recurrent Mixture Model): Interaction and reward prediction
 * 
 * Integration with CognitiveCycleManager:
 * - Perception Stream ↔ sMM + iMM
 * - Action Stream ↔ Policy Selection + Expected Free Energy
 * - Simulation Stream ↔ tMM + rMM
 * 
 * Reference: Heins et al. (2025) "AXIOM: Learning to Play Games in Minutes
 * with Expanding Object-Centric Models" arXiv:2505.24784
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../../Cognitive/CognitiveCycleManager.h"
#include "AXIOMActiveInference.generated.h"

// ========================================
// FORWARD DECLARATIONS
// ========================================

class USlotMixtureModel;
class UIdentityMixtureModel;
class UTransitionMixtureModel;
class URecurrentMixtureModel;

// ========================================
// ENUMERATIONS
// ========================================

/**
 * AXIOM Learning Phase
 */
UENUM(BlueprintType)
enum class EAXIOMLearningPhase : uint8
{
    /** Initial exploration - building basic sensorimotor contingencies */
    Exploration UMETA(DisplayName = "Exploration"),
    
    /** Model building - constructing world model through active inference */
    ModelBuilding UMETA(DisplayName = "Model Building"),
    
    /** Skill acquisition - learning goal-directed behaviors */
    SkillAcquisition UMETA(DisplayName = "Skill Acquisition"),
    
    /** Mastery - balancing exploration and exploitation */
    Mastery UMETA(DisplayName = "Mastery")
};

/**
 * Object Slot State
 */
UENUM(BlueprintType)
enum class ESlotState : uint8
{
    /** Slot is empty/inactive */
    Empty UMETA(DisplayName = "Empty"),
    
    /** Slot is tracking an object */
    Active UMETA(DisplayName = "Active"),
    
    /** Slot is occluded but predicted */
    Occluded UMETA(DisplayName = "Occluded"),
    
    /** Slot is newly spawned */
    Spawned UMETA(DisplayName = "Spawned")
};

/**
 * Motion Prototype Type (tMM switch states)
 */
UENUM(BlueprintType)
enum class EMotionPrototype : uint8
{
    /** Stationary - no movement */
    Stationary UMETA(DisplayName = "Stationary"),
    
    /** Linear - constant velocity */
    Linear UMETA(DisplayName = "Linear"),
    
    /** Falling - gravity-affected */
    Falling UMETA(DisplayName = "Falling"),
    
    /** Bouncing - collision response */
    Bouncing UMETA(DisplayName = "Bouncing"),
    
    /** Controlled - player-controlled */
    Controlled UMETA(DisplayName = "Controlled"),
    
    /** Custom - learned prototype */
    Custom UMETA(DisplayName = "Custom")
};

// ========================================
// STRUCTURES
// ========================================

/**
 * Object Slot - Object-centric representation
 */
USTRUCT(BlueprintType)
struct FObjectSlot
{
    GENERATED_BODY()

    /** Slot index */
    UPROPERTY(BlueprintReadWrite)
    int32 SlotIndex = 0;

    /** Slot state */
    UPROPERTY(BlueprintReadWrite)
    ESlotState State = ESlotState::Empty;

    /** Continuous latent: Position (x, y) */
    UPROPERTY(BlueprintReadWrite)
    FVector2D Position = FVector2D::ZeroVector;

    /** Continuous latent: Velocity (vx, vy) */
    UPROPERTY(BlueprintReadWrite)
    FVector2D Velocity = FVector2D::ZeroVector;

    /** Continuous latent: Size (width, height) */
    UPROPERTY(BlueprintReadWrite)
    FVector2D Size = FVector2D(1.0f, 1.0f);

    /** Continuous latent: Color (RGB) */
    UPROPERTY(BlueprintReadWrite)
    FLinearColor Color = FLinearColor::White;

    /** Discrete latent: Object type ID */
    UPROPERTY(BlueprintReadWrite)
    int32 ObjectTypeID = -1;

    /** Discrete latent: Current motion prototype */
    UPROPERTY(BlueprintReadWrite)
    EMotionPrototype MotionPrototype = EMotionPrototype::Stationary;

    /** Uncertainty: Position variance */
    UPROPERTY(BlueprintReadWrite)
    float PositionUncertainty = 1.0f;

    /** Uncertainty: Type confidence */
    UPROPERTY(BlueprintReadWrite)
    float TypeConfidence = 0.0f;

    /** Slot activation (assignment probability) */
    UPROPERTY(BlueprintReadWrite)
    float Activation = 0.0f;
};

/**
 * Gaussian Mixture Component - For mixture models
 */
USTRUCT(BlueprintType)
struct FGaussianComponent
{
    GENERATED_BODY()

    /** Component index */
    UPROPERTY(BlueprintReadWrite)
    int32 ComponentIndex = 0;

    /** Mean vector */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> Mean;

    /** Diagonal covariance (variance per dimension) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> Variance;

    /** Mixing weight (prior probability) */
    UPROPERTY(BlueprintReadWrite)
    float Weight = 1.0f;

    /** Number of observations assigned to this component */
    UPROPERTY(BlueprintReadWrite)
    int32 ObservationCount = 0;
};

/**
 * Transition Prototype - Linear dynamical system for tMM
 */
USTRUCT(BlueprintType)
struct FTransitionPrototype
{
    GENERATED_BODY()

    /** Prototype index */
    UPROPERTY(BlueprintReadWrite)
    int32 PrototypeIndex = 0;

    /** Motion type */
    UPROPERTY(BlueprintReadWrite)
    EMotionPrototype MotionType = EMotionPrototype::Stationary;

    /** State transition matrix A (flattened 4x4 for 2D position+velocity) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> TransitionMatrix;

    /** Control matrix B (flattened 4x2 for action influence) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> ControlMatrix;

    /** Process noise covariance Q (diagonal) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> ProcessNoise;

    /** Usage count */
    UPROPERTY(BlueprintReadWrite)
    int32 UsageCount = 0;
};

/**
 * Interaction Cluster - For rMM relational modeling
 */
USTRUCT(BlueprintType)
struct FInteractionCluster
{
    GENERATED_BODY()

    /** Cluster index */
    UPROPERTY(BlueprintReadWrite)
    int32 ClusterIndex = 0;

    /** Object type pair (type1, type2) */
    UPROPERTY(BlueprintReadWrite)
    FIntPoint ObjectTypePair = FIntPoint(-1, -1);

    /** Distance threshold for interaction */
    UPROPERTY(BlueprintReadWrite)
    float DistanceThreshold = 0.0f;

    /** Expected reward when this interaction occurs */
    UPROPERTY(BlueprintReadWrite)
    float ExpectedReward = 0.0f;

    /** Reward variance */
    UPROPERTY(BlueprintReadWrite)
    float RewardVariance = 1.0f;

    /** Predicted next motion prototype for object 1 */
    UPROPERTY(BlueprintReadWrite)
    EMotionPrototype NextPrototype1 = EMotionPrototype::Stationary;

    /** Predicted next motion prototype for object 2 */
    UPROPERTY(BlueprintReadWrite)
    EMotionPrototype NextPrototype2 = EMotionPrototype::Stationary;

    /** Cluster activation count */
    UPROPERTY(BlueprintReadWrite)
    int32 ActivationCount = 0;
};

/**
 * Policy - Sequence of actions for planning
 */
USTRUCT(BlueprintType)
struct FPolicy
{
    GENERATED_BODY()

    /** Policy index */
    UPROPERTY(BlueprintReadWrite)
    int32 PolicyIndex = 0;

    /** Action sequence (action indices) */
    UPROPERTY(BlueprintReadWrite)
    TArray<int32> ActionSequence;

    /** Expected Free Energy for this policy */
    UPROPERTY(BlueprintReadWrite)
    float ExpectedFreeEnergy = 0.0f;

    /** Expected utility (reward) component */
    UPROPERTY(BlueprintReadWrite)
    float ExpectedUtility = 0.0f;

    /** Information gain (epistemic value) component */
    UPROPERTY(BlueprintReadWrite)
    float InformationGain = 0.0f;

    /** Policy probability (softmax of -EFE) */
    UPROPERTY(BlueprintReadWrite)
    float Probability = 0.0f;
};

/**
 * World State - Complete state representation
 */
USTRUCT(BlueprintType)
struct FWorldState
{
    GENERATED_BODY()

    /** Timestamp */
    UPROPERTY(BlueprintReadWrite)
    int32 Timestep = 0;

    /** Object slots */
    UPROPERTY(BlueprintReadWrite)
    TArray<FObjectSlot> Slots;

    /** Current action */
    UPROPERTY(BlueprintReadWrite)
    int32 CurrentAction = 0;

    /** Current reward */
    UPROPERTY(BlueprintReadWrite)
    float CurrentReward = 0.0f;

    /** Cumulative reward */
    UPROPERTY(BlueprintReadWrite)
    float CumulativeReward = 0.0f;

    /** Variational free energy (model fit) */
    UPROPERTY(BlueprintReadWrite)
    float VariationalFreeEnergy = 0.0f;
};

/**
 * AXIOM Configuration
 */
USTRUCT(BlueprintType)
struct FAXIOMConfig
{
    GENERATED_BODY()

    /** Maximum number of object slots */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "32"))
    int32 MaxSlots = 16;

    /** Maximum number of object types (iMM components) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "64"))
    int32 MaxObjectTypes = 16;

    /** Maximum number of transition prototypes (tMM components) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "32"))
    int32 MaxTransitionPrototypes = 8;

    /** Maximum number of interaction clusters (rMM components) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "256"))
    int32 MaxInteractionClusters = 64;

    /** Planning horizon (number of steps to look ahead) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "32"))
    int32 PlanningHorizon = 8;

    /** Number of policies to evaluate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "512"))
    int32 NumPolicies = 64;

    /** Number of rollout samples per policy */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "32"))
    int32 RolloutsPerPolicy = 4;

    /** Information gain weight (exploration vs exploitation) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float InformationGainWeight = 1.0f;

    /** Bayesian Model Reduction threshold */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BMRThreshold = 0.1f;

    /** Enable automatic structure expansion */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableStructureExpansion = true;

    /** Enable Bayesian Model Reduction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableBMR = true;
};

/**
 * AXIOM Active Inference Component
 * 
 * Integrates the AXIOM active inference architecture with the
 * Deep-Tree-Echo CognitiveCycleManager for game learning and avatar control.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UAXIOMActiveInference : public UActorComponent
{
    GENERATED_BODY()

public:
    UAXIOMActiveInference();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** AXIOM configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AXIOM|Config")
    FAXIOMConfig Config;

    /** Reference to CognitiveCycleManager */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AXIOM|Config")
    UCognitiveCycleManager* CognitiveCycleManager;

    // ========================================
    // STATE
    // ========================================

    /** Current learning phase */
    UPROPERTY(BlueprintReadOnly, Category = "AXIOM|State")
    EAXIOMLearningPhase LearningPhase = EAXIOMLearningPhase::Exploration;

    /** Current world state */
    UPROPERTY(BlueprintReadOnly, Category = "AXIOM|State")
    FWorldState CurrentState;

    /** Previous world state */
    UPROPERTY(BlueprintReadOnly, Category = "AXIOM|State")
    FWorldState PreviousState;

    /** Object type components (iMM) */
    UPROPERTY(BlueprintReadOnly, Category = "AXIOM|State")
    TArray<FGaussianComponent> ObjectTypes;

    /** Transition prototypes (tMM) */
    UPROPERTY(BlueprintReadOnly, Category = "AXIOM|State")
    TArray<FTransitionPrototype> TransitionPrototypes;

    /** Interaction clusters (rMM) */
    UPROPERTY(BlueprintReadOnly, Category = "AXIOM|State")
    TArray<FInteractionCluster> InteractionClusters;

    /** Current policies being evaluated */
    UPROPERTY(BlueprintReadOnly, Category = "AXIOM|State")
    TArray<FPolicy> Policies;

    /** Selected policy */
    UPROPERTY(BlueprintReadOnly, Category = "AXIOM|State")
    FPolicy SelectedPolicy;

    /** Total interaction steps */
    UPROPERTY(BlueprintReadOnly, Category = "AXIOM|State")
    int32 TotalSteps = 0;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize AXIOM with default priors */
    UFUNCTION(BlueprintCallable, Category = "AXIOM")
    void Initialize();

    /** Reset to initial state */
    UFUNCTION(BlueprintCallable, Category = "AXIOM")
    void Reset();

    /** Set reference to CognitiveCycleManager */
    UFUNCTION(BlueprintCallable, Category = "AXIOM")
    void SetCognitiveCycleManager(UCognitiveCycleManager* Manager);

    // ========================================
    // PUBLIC API - PERCEPTION (sMM + iMM)
    // ========================================

    /** Process visual observation (sMM inference) */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Perception")
    void ProcessObservation(const TArray<FColor>& Pixels, int32 Width, int32 Height);

    /** Update object slots from observation */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Perception")
    void UpdateSlots(const TArray<FObjectSlot>& DetectedObjects);

    /** Classify object types (iMM inference) */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Perception")
    void ClassifyObjectTypes();

    /** Get current object slots */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Perception")
    TArray<FObjectSlot> GetObjectSlots() const;

    // ========================================
    // PUBLIC API - DYNAMICS (tMM)
    // ========================================

    /** Predict next state for all objects */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Dynamics")
    void PredictNextState(int32 Action);

    /** Update transition model from observation */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Dynamics")
    void UpdateTransitionModel();

    /** Get motion prototype for object */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Dynamics")
    EMotionPrototype GetMotionPrototype(int32 SlotIndex) const;

    // ========================================
    // PUBLIC API - INTERACTIONS (rMM)
    // ========================================

    /** Predict interactions and rewards */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Interactions")
    float PredictReward(int32 Action);

    /** Update interaction model from observation */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Interactions")
    void UpdateInteractionModel(float ObservedReward);

    /** Find nearest interacting object for slot */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Interactions")
    int32 FindNearestInteractor(int32 SlotIndex) const;

    // ========================================
    // PUBLIC API - PLANNING (Active Inference)
    // ========================================

    /** Generate policies for evaluation */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Planning")
    void GeneratePolicies();

    /** Evaluate policies using Expected Free Energy */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Planning")
    void EvaluatePolicies();

    /** Select action from policy distribution */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Planning")
    int32 SelectAction();

    /** Compute Expected Free Energy for policy */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Planning")
    float ComputeExpectedFreeEnergy(const FPolicy& Policy);

    /** Compute information gain for policy */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Planning")
    float ComputeInformationGain(const FPolicy& Policy);

    /** Compute expected utility for policy */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Planning")
    float ComputeExpectedUtility(const FPolicy& Policy);

    // ========================================
    // PUBLIC API - STRUCTURE LEARNING
    // ========================================

    /** Expand model structure if needed */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Structure")
    void ExpandStructure();

    /** Perform Bayesian Model Reduction */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Structure")
    void PerformBMR();

    /** Add new object type component */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Structure")
    int32 AddObjectType(const FGaussianComponent& Component);

    /** Add new transition prototype */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Structure")
    int32 AddTransitionPrototype(const FTransitionPrototype& Prototype);

    /** Add new interaction cluster */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|Structure")
    int32 AddInteractionCluster(const FInteractionCluster& Cluster);

    // ========================================
    // PUBLIC API - COGNITIVE CYCLE INTEGRATION
    // ========================================

    /** Process cognitive cycle step */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|CognitiveCycle")
    void ProcessCognitiveStep(int32 GlobalStep, EConsciousnessStream ActiveStream);

    /** Get salience value for current state */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|CognitiveCycle")
    float GetSalienceValue() const;

    /** Get affordance value for action */
    UFUNCTION(BlueprintCallable, Category = "AXIOM|CognitiveCycle")
    float GetAffordanceValue(int32 Action) const;

    // ========================================
    // DELEGATES
    // ========================================

    /** Called when a new object type is discovered */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectTypeDiscovered, int32, TypeID);
    UPROPERTY(BlueprintAssignable, Category = "AXIOM|Events")
    FOnObjectTypeDiscovered OnObjectTypeDiscovered;

    /** Called when a new interaction is learned */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInteractionLearned, int32, ClusterID, float, ExpectedReward);
    UPROPERTY(BlueprintAssignable, Category = "AXIOM|Events")
    FOnInteractionLearned OnInteractionLearned;

    /** Called when action is selected */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActionSelected, int32, Action, float, ExpectedFreeEnergy);
    UPROPERTY(BlueprintAssignable, Category = "AXIOM|Events")
    FOnActionSelected OnActionSelected;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Initialize default transition prototypes */
    void InitializeTransitionPrototypes();

    /** Compute slot assignment probabilities */
    void ComputeSlotAssignments(const TArray<FColor>& Pixels, int32 Width, int32 Height);

    /** Perform variational inference update */
    void VariationalUpdate();

    /** Rollout policy for planning */
    TArray<FWorldState> RolloutPolicy(const FPolicy& Policy, int32 NumSamples);

    /** Merge similar components (BMR) */
    void MergeSimilarComponents();

    /** Check if structure expansion is needed */
    bool NeedsStructureExpansion() const;

    /** Compute KL divergence between Gaussians */
    float ComputeKLDivergence(const FGaussianComponent& P, const FGaussianComponent& Q) const;

    /** Sample from categorical distribution */
    int32 SampleCategorical(const TArray<float>& Probabilities) const;

    /** Softmax normalization */
    TArray<float> Softmax(const TArray<float>& Values, float Temperature = 1.0f) const;

    // Internal state
    float AccumulatedDeltaTime = 0.0f;
    int32 LastProcessedStep = -1;
};

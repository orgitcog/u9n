// ActiveInferenceEngine.h
// Active Inference Engine for Deep Tree Echo
// Implements the Free Energy Principle with action-oriented predictive processing

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ActiveInferenceEngine.generated.h"

// Forward declarations
class UPredictiveAdaptationEngine;
class UEchobeatsStreamEngine;
class USys6LCMClockSynchronizer;
class UNicheConstructionSystem;

/**
 * Active inference mode
 */
UENUM(BlueprintType)
enum class EActiveInferenceMode : uint8
{
    Perception      UMETA(DisplayName = "Perceptual Inference"),
    Action          UMETA(DisplayName = "Active Inference"),
    Planning        UMETA(DisplayName = "Planning as Inference"),
    Learning        UMETA(DisplayName = "Structure Learning"),
    NicheConstruct  UMETA(DisplayName = "Niche Construction")
};

/**
 * Policy type for action selection
 */
UENUM(BlueprintType)
enum class EPolicyType : uint8
{
    Habitual        UMETA(DisplayName = "Habitual"),
    GoalDirected    UMETA(DisplayName = "Goal-Directed"),
    Exploratory     UMETA(DisplayName = "Exploratory"),
    Epistemic       UMETA(DisplayName = "Epistemic"),
    Pragmatic       UMETA(DisplayName = "Pragmatic"),
    NicheShaping    UMETA(DisplayName = "Niche Shaping")
};

/**
 * Markov blanket state
 */
UENUM(BlueprintType)
enum class EMarkovBlanketState : uint8
{
    Internal        UMETA(DisplayName = "Internal"),
    Sensory         UMETA(DisplayName = "Sensory"),
    Active          UMETA(DisplayName = "Active"),
    External        UMETA(DisplayName = "External")
};

/**
 * Belief state (probability distribution)
 */
USTRUCT(BlueprintType)
struct FBeliefState
{
    GENERATED_BODY()

    /** Belief ID */
    UPROPERTY(BlueprintReadOnly)
    FString BeliefID;

    /** State space dimension */
    UPROPERTY(BlueprintReadOnly)
    int32 Dimension = 0;

    /** Mean of belief distribution */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> Mean;

    /** Precision (inverse covariance diagonal) */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> Precision;

    /** Entropy of belief */
    UPROPERTY(BlueprintReadOnly)
    float Entropy = 0.0f;

    /** Confidence (1 - entropy normalized) */
    UPROPERTY(BlueprintReadOnly)
    float Confidence = 0.0f;

    /** Last update timestamp */
    UPROPERTY(BlueprintReadOnly)
    float LastUpdate = 0.0f;
};

/**
 * Generative model for active inference
 */
USTRUCT(BlueprintType)
struct FGenerativeModel
{
    GENERATED_BODY()

    /** Model ID */
    UPROPERTY(BlueprintReadOnly)
    FString ModelID;

    /** Hidden state beliefs */
    UPROPERTY(BlueprintReadOnly)
    FBeliefState HiddenStates;

    /** Observation likelihood mapping (A matrix) */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> LikelihoodMapping;

    /** State transition probabilities (B matrix) */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> TransitionProbabilities;

    /** Prior preferences over outcomes (C vector) */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> Preferences;

    /** Prior beliefs about initial states (D vector) */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> InitialStatePrior;

    /** Model evidence (log) */
    UPROPERTY(BlueprintReadOnly)
    float LogEvidence = 0.0f;

    /** Complexity term */
    UPROPERTY(BlueprintReadOnly)
    float Complexity = 0.0f;

    /** Accuracy term */
    UPROPERTY(BlueprintReadOnly)
    float Accuracy = 0.0f;
};

/**
 * Policy (sequence of actions)
 */
USTRUCT(BlueprintType)
struct FPolicy
{
    GENERATED_BODY()

    /** Policy ID */
    UPROPERTY(BlueprintReadOnly)
    FString PolicyID;

    /** Policy type */
    UPROPERTY(BlueprintReadOnly)
    EPolicyType Type = EPolicyType::GoalDirected;

    /** Action sequence */
    UPROPERTY(BlueprintReadOnly)
    TArray<int32> ActionSequence;

    /** Expected free energy (G) */
    UPROPERTY(BlueprintReadOnly)
    float ExpectedFreeEnergy = 0.0f;

    /** Epistemic value (information gain) */
    UPROPERTY(BlueprintReadOnly)
    float EpistemicValue = 0.0f;

    /** Pragmatic value (goal achievement) */
    UPROPERTY(BlueprintReadOnly)
    float PragmaticValue = 0.0f;

    /** Niche construction value */
    UPROPERTY(BlueprintReadOnly)
    float NicheValue = 0.0f;

    /** Policy probability */
    UPROPERTY(BlueprintReadOnly)
    float Probability = 0.0f;

    /** Horizon (planning depth) */
    UPROPERTY(BlueprintReadOnly)
    int32 Horizon = 1;
};

/**
 * Markov blanket partition
 */
USTRUCT(BlueprintType)
struct FMarkovBlanket
{
    GENERATED_BODY()

    /** Internal states (hidden from environment) */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> InternalStates;

    /** Sensory states (observations) */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> SensoryStates;

    /** Active states (actions) */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> ActiveStates;

    /** External states (environment, inferred) */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> ExternalStates;

    /** Blanket integrity (coupling strength) */
    UPROPERTY(BlueprintReadOnly)
    float BlanketIntegrity = 1.0f;

    /** Flow towards steady state */
    UPROPERTY(BlueprintReadOnly)
    float SteadyStateFlow = 0.0f;
};

/**
 * Expected free energy components
 */
USTRUCT(BlueprintType)
struct FExpectedFreeEnergy
{
    GENERATED_BODY()

    /** Total expected free energy */
    UPROPERTY(BlueprintReadOnly)
    float Total = 0.0f;

    /** Ambiguity (expected surprise about outcomes) */
    UPROPERTY(BlueprintReadOnly)
    float Ambiguity = 0.0f;

    /** Risk (expected divergence from preferences) */
    UPROPERTY(BlueprintReadOnly)
    float Risk = 0.0f;

    /** Epistemic value (expected information gain) */
    UPROPERTY(BlueprintReadOnly)
    float EpistemicValue = 0.0f;

    /** Pragmatic value (expected preference satisfaction) */
    UPROPERTY(BlueprintReadOnly)
    float PragmaticValue = 0.0f;

    /** Niche construction value */
    UPROPERTY(BlueprintReadOnly)
    float NicheConstructionValue = 0.0f;

    /** Temporal horizon */
    UPROPERTY(BlueprintReadOnly)
    int32 Horizon = 1;
};

/**
 * Variational free energy
 */
USTRUCT(BlueprintType)
struct FVariationalFreeEnergy
{
    GENERATED_BODY()

    /** Total variational free energy */
    UPROPERTY(BlueprintReadOnly)
    float Total = 0.0f;

    /** Energy term (expected log joint) */
    UPROPERTY(BlueprintReadOnly)
    float Energy = 0.0f;

    /** Entropy term (negative) */
    UPROPERTY(BlueprintReadOnly)
    float NegativeEntropy = 0.0f;

    /** KL divergence from prior */
    UPROPERTY(BlueprintReadOnly)
    float KLDivergence = 0.0f;

    /** Surprise (negative log evidence) */
    UPROPERTY(BlueprintReadOnly)
    float Surprise = 0.0f;

    /** Bound tightness */
    UPROPERTY(BlueprintReadOnly)
    float BoundTightness = 0.0f;
};

/**
 * Action outcome
 */
USTRUCT(BlueprintType)
struct FActionOutcome
{
    GENERATED_BODY()

    /** Action ID */
    UPROPERTY(BlueprintReadOnly)
    int32 ActionID = 0;

    /** Action type */
    UPROPERTY(BlueprintReadOnly)
    FString ActionType;

    /** Predicted outcome */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> PredictedOutcome;

    /** Actual outcome */
    UPROPERTY(BlueprintReadOnly)
    TArray<float> ActualOutcome;

    /** Prediction error */
    UPROPERTY(BlueprintReadOnly)
    float PredictionError = 0.0f;

    /** Free energy change */
    UPROPERTY(BlueprintReadOnly)
    float FreeEnergyChange = 0.0f;

    /** Niche modification */
    UPROPERTY(BlueprintReadOnly)
    float NicheModification = 0.0f;

    /** Timestamp */
    UPROPERTY(BlueprintReadOnly)
    float Timestamp = 0.0f;
};

/**
 * Inference cycle state
 */
USTRUCT(BlueprintType)
struct FInferenceCycleState
{
    GENERATED_BODY()

    /** Current mode */
    UPROPERTY(BlueprintReadOnly)
    EActiveInferenceMode Mode = EActiveInferenceMode::Perception;

    /** Current echobeat step */
    UPROPERTY(BlueprintReadOnly)
    int32 EchobeatStep = 1;

    /** Current LCM step */
    UPROPERTY(BlueprintReadOnly)
    int32 LCMStep = 0;

    /** Variational free energy */
    UPROPERTY(BlueprintReadOnly)
    FVariationalFreeEnergy VFE;

    /** Expected free energy */
    UPROPERTY(BlueprintReadOnly)
    FExpectedFreeEnergy EFE;

    /** Selected policy */
    UPROPERTY(BlueprintReadOnly)
    FPolicy SelectedPolicy;

    /** Markov blanket state */
    UPROPERTY(BlueprintReadOnly)
    FMarkovBlanket Blanket;

    /** Inference iterations this cycle */
    UPROPERTY(BlueprintReadOnly)
    int32 InferenceIterations = 0;

    /** Convergence achieved */
    UPROPERTY(BlueprintReadOnly)
    bool bConverged = false;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeliefUpdated, const FBeliefState&, Belief);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPolicySelected, const FPolicy&, Policy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionExecuted, const FActionOutcome&, Outcome);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFreeEnergyComputed, float, Variational, float, Expected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInferenceModeChanged, EActiveInferenceMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMarkovBlanketUpdated, const FMarkovBlanket&, Blanket);

/**
 * Active Inference Engine
 * 
 * Implements the Free Energy Principle for action-oriented predictive processing.
 * Enables the Deep Tree Echo avatar to minimize surprise through both perception
 * (updating beliefs) and action (changing the world to match predictions).
 * 
 * Key features:
 * - Variational free energy minimization
 * - Expected free energy for policy selection
 * - Markov blanket formulation
 * - Epistemic and pragmatic value computation
 * - Integration with niche construction
 * - Sys6 synchronized inference cycles
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UActiveInferenceEngine : public UActorComponent
{
    GENERATED_BODY()

public:
    UActiveInferenceEngine();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Number of hidden state factors */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Model")
    int32 NumHiddenFactors = 4;

    /** States per factor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Model")
    int32 StatesPerFactor = 8;

    /** Number of observation modalities */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Model")
    int32 NumObservationModalities = 3;

    /** Number of action factors */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Model")
    int32 NumActionFactors = 2;

    /** Planning horizon */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Planning")
    int32 PlanningHorizon = 5;

    /** Number of policies to evaluate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Planning")
    int32 NumPolicies = 16;

    /** Inference iterations per step */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Inference")
    int32 InferenceIterations = 16;

    /** Convergence threshold */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Inference")
    float ConvergenceThreshold = 0.001f;

    /** Epistemic weight (curiosity) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Values")
    float EpistemicWeight = 1.0f;

    /** Pragmatic weight (goal-seeking) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Values")
    float PragmaticWeight = 1.0f;

    /** Niche construction weight */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Values")
    float NicheWeight = 0.5f;

    /** Habit strength (prior policy precision) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Values")
    float HabitStrength = 1.0f;

    /** Action precision (inverse temperature) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Values")
    float ActionPrecision = 4.0f;

    /** Enable niche construction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config|Niche")
    bool bEnableNicheConstruction = true;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnBeliefUpdated OnBeliefUpdated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPolicySelected OnPolicySelected;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnActionExecuted OnActionExecuted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnFreeEnergyComputed OnFreeEnergyComputed;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnInferenceModeChanged OnInferenceModeChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnMarkovBlanketUpdated OnMarkovBlanketUpdated;

    // ========================================
    // PUBLIC API - INFERENCE
    // ========================================

    /** Run perceptual inference (update beliefs from observations) */
    UFUNCTION(BlueprintCallable, Category = "Inference")
    void RunPerceptualInference(const TArray<float>& Observations);

    /** Run active inference (select and execute action) */
    UFUNCTION(BlueprintCallable, Category = "Inference")
    int32 RunActiveInference();

    /** Run planning as inference */
    UFUNCTION(BlueprintCallable, Category = "Inference")
    FPolicy RunPlanningInference(int32 Horizon);

    /** Update beliefs with new observation */
    UFUNCTION(BlueprintCallable, Category = "Inference")
    void UpdateBeliefs(const TArray<float>& Observation, int32 Modality);

    /** Get current belief state */
    UFUNCTION(BlueprintPure, Category = "Inference")
    FBeliefState GetBeliefState(int32 Factor) const;

    /** Get all belief states */
    UFUNCTION(BlueprintPure, Category = "Inference")
    TArray<FBeliefState> GetAllBeliefStates() const;

    // ========================================
    // PUBLIC API - FREE ENERGY
    // ========================================

    /** Compute variational free energy */
    UFUNCTION(BlueprintCallable, Category = "FreeEnergy")
    FVariationalFreeEnergy ComputeVariationalFreeEnergy();

    /** Compute expected free energy for policy */
    UFUNCTION(BlueprintCallable, Category = "FreeEnergy")
    FExpectedFreeEnergy ComputeExpectedFreeEnergy(const FPolicy& Policy);

    /** Get current variational free energy */
    UFUNCTION(BlueprintPure, Category = "FreeEnergy")
    float GetVariationalFreeEnergy() const;

    /** Get expected free energy for selected policy */
    UFUNCTION(BlueprintPure, Category = "FreeEnergy")
    float GetExpectedFreeEnergy() const;

    /** Minimize variational free energy (gradient step) */
    UFUNCTION(BlueprintCallable, Category = "FreeEnergy")
    float MinimizeVariationalFreeEnergy(int32 Iterations);

    // ========================================
    // PUBLIC API - POLICY SELECTION
    // ========================================

    /** Evaluate all policies */
    UFUNCTION(BlueprintCallable, Category = "Policy")
    TArray<FPolicy> EvaluatePolicies();

    /** Select policy (softmax over expected free energy) */
    UFUNCTION(BlueprintCallable, Category = "Policy")
    FPolicy SelectPolicy();

    /** Get policy probability */
    UFUNCTION(BlueprintPure, Category = "Policy")
    float GetPolicyProbability(const FPolicy& Policy) const;

    /** Get selected policy */
    UFUNCTION(BlueprintPure, Category = "Policy")
    FPolicy GetSelectedPolicy() const;

    /** Generate policies */
    UFUNCTION(BlueprintCallable, Category = "Policy")
    TArray<FPolicy> GeneratePolicies(int32 Count, int32 Horizon);

    // ========================================
    // PUBLIC API - ACTION
    // ========================================

    /** Execute action from policy */
    UFUNCTION(BlueprintCallable, Category = "Action")
    FActionOutcome ExecuteAction(int32 ActionIndex);

    /** Get available actions */
    UFUNCTION(BlueprintPure, Category = "Action")
    TArray<int32> GetAvailableActions() const;

    /** Predict action outcome */
    UFUNCTION(BlueprintPure, Category = "Action")
    TArray<float> PredictActionOutcome(int32 ActionIndex) const;

    /** Get action history */
    UFUNCTION(BlueprintPure, Category = "Action")
    TArray<FActionOutcome> GetActionHistory() const;

    // ========================================
    // PUBLIC API - MARKOV BLANKET
    // ========================================

    /** Get Markov blanket state */
    UFUNCTION(BlueprintPure, Category = "MarkovBlanket")
    FMarkovBlanket GetMarkovBlanket() const;

    /** Update Markov blanket */
    UFUNCTION(BlueprintCallable, Category = "MarkovBlanket")
    void UpdateMarkovBlanket(const TArray<float>& Sensory, const TArray<float>& Active);

    /** Compute blanket integrity */
    UFUNCTION(BlueprintPure, Category = "MarkovBlanket")
    float ComputeBlanketIntegrity() const;

    /** Get steady state flow */
    UFUNCTION(BlueprintPure, Category = "MarkovBlanket")
    float GetSteadyStateFlow() const;

    // ========================================
    // PUBLIC API - GENERATIVE MODEL
    // ========================================

    /** Get generative model */
    UFUNCTION(BlueprintPure, Category = "Model")
    FGenerativeModel GetGenerativeModel() const;

    /** Update likelihood mapping (A matrix) */
    UFUNCTION(BlueprintCallable, Category = "Model")
    void UpdateLikelihoodMapping(const TArray<float>& NewMapping);

    /** Update transition probabilities (B matrix) */
    UFUNCTION(BlueprintCallable, Category = "Model")
    void UpdateTransitionProbabilities(const TArray<float>& NewTransitions);

    /** Set preferences (C vector) */
    UFUNCTION(BlueprintCallable, Category = "Model")
    void SetPreferences(const TArray<float>& NewPreferences);

    /** Learn model structure */
    UFUNCTION(BlueprintCallable, Category = "Model")
    void LearnModelStructure(const TArray<FActionOutcome>& Experiences);

    // ========================================
    // PUBLIC API - NICHE CONSTRUCTION
    // ========================================

    /** Compute niche construction value */
    UFUNCTION(BlueprintPure, Category = "Niche")
    float ComputeNicheConstructionValue(const FPolicy& Policy) const;

    /** Get niche modification potential */
    UFUNCTION(BlueprintPure, Category = "Niche")
    float GetNicheModificationPotential(int32 ActionIndex) const;

    /** Plan niche construction action */
    UFUNCTION(BlueprintCallable, Category = "Niche")
    FPolicy PlanNicheConstruction(int32 Horizon);

    // ========================================
    // PUBLIC API - SYS6 INTEGRATION
    // ========================================

    /** Process sys6 step */
    UFUNCTION(BlueprintCallable, Category = "Sys6")
    void ProcessSys6Step(int32 LCMStep);

    /** Get inference mode for echobeat step */
    UFUNCTION(BlueprintPure, Category = "Sys6")
    EActiveInferenceMode GetInferenceModeForStep(int32 EchobeatStep) const;

    /** Get current inference cycle state */
    UFUNCTION(BlueprintPure, Category = "Sys6")
    FInferenceCycleState GetInferenceCycleState() const;

protected:
    // Component references
    UPROPERTY()
    UPredictiveAdaptationEngine* PredictiveEngine;

    UPROPERTY()
    UEchobeatsStreamEngine* EchobeatsEngine;

    UPROPERTY()
    USys6LCMClockSynchronizer* LCMClock;

    UPROPERTY()
    UNicheConstructionSystem* NicheSystem;

    // Internal state
    FGenerativeModel GenerativeModel;
    TArray<FBeliefState> BeliefStates;
    TArray<FPolicy> EvaluatedPolicies;
    FPolicy CurrentPolicy;
    FMarkovBlanket MarkovBlanket;
    FInferenceCycleState CycleState;
    TArray<FActionOutcome> ActionHistory;

    float CurrentVFE = 0.0f;
    float CurrentEFE = 0.0f;
    int32 NextActionIndex = 0;

    // Internal methods
    void FindComponentReferences();
    void InitializeGenerativeModel();
    void InitializeBeliefStates();
    void InitializeMarkovBlanket();
    TArray<float> SoftmaxPolicies(const TArray<float>& ExpectedFreeEnergies) const;
    float ComputeKLDivergence(const TArray<float>& Q, const TArray<float>& P) const;
    float ComputeEntropy(const TArray<float>& Distribution) const;
    TArray<float> NormalizeProbabilities(const TArray<float>& Probs) const;
    void UpdateInferenceMode(int32 EchobeatStep);
    float ComputeEpistemicValue(const FPolicy& Policy) const;
    float ComputePragmaticValue(const FPolicy& Policy) const;
};

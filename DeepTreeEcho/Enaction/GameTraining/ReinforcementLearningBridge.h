// ReinforcementLearningBridge.h
// Connects Game Training with Deep Tree Echo Cognitive Learning Systems
// Bridges RL rewards to reservoir computing, active inference, and cognitive systems

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameControllerInterface.h"
#include "GameTrainingEnvironment.h"
#include "GameSkillTrainingSystem.h"
#include "ReinforcementLearningBridge.generated.h"

// Forward declarations
class UOnlineLearningSystem;
class UEmbodiedCognitionComponent;
class UDeepTreeEchoCore;
class UDeepTreeEchoReservoir;

/**
 * Policy type for action selection
 */
UENUM(BlueprintType)
enum class EActionSelectionPolicy : uint8
{
    EpsilonGreedy   UMETA(DisplayName = "Epsilon Greedy"),
    Softmax         UMETA(DisplayName = "Softmax"),
    UCB             UMETA(DisplayName = "Upper Confidence Bound"),
    Thompson        UMETA(DisplayName = "Thompson Sampling"),
    ActiveInference UMETA(DisplayName = "Active Inference")
};

/**
 * Learning algorithm
 */
UENUM(BlueprintType)
enum class ELearningAlgorithm : uint8
{
    QLearning       UMETA(DisplayName = "Q-Learning"),
    SARSA           UMETA(DisplayName = "SARSA"),
    DQN             UMETA(DisplayName = "Deep Q-Network"),
    PolicyGradient  UMETA(DisplayName = "Policy Gradient"),
    ActorCritic     UMETA(DisplayName = "Actor-Critic"),
    ReservoirRL     UMETA(DisplayName = "Reservoir RL")
};

/**
 * Action for the environment
 */
USTRUCT(BlueprintType)
struct FRLAction
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    int32 ActionIndex = 0;

    UPROPERTY(BlueprintReadWrite)
    FString ActionName;

    UPROPERTY(BlueprintReadWrite)
    TArray<float> ContinuousAction;

    UPROPERTY(BlueprintReadWrite)
    float QValue = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float Probability = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float ExpectedFreeEnergy = 0.0f;
};

/**
 * Transition for experience replay
 */
USTRUCT(BlueprintType)
struct FTransition
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    TArray<float> State;

    UPROPERTY(BlueprintReadWrite)
    FRLAction Action;

    UPROPERTY(BlueprintReadWrite)
    float Reward = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    TArray<float> NextState;

    UPROPERTY(BlueprintReadWrite)
    bool bTerminal = false;

    UPROPERTY(BlueprintReadWrite)
    float Timestamp = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float Priority = 1.0f;
};

/**
 * Cognitive modulation of learning
 */
USTRUCT(BlueprintType)
struct FCognitiveModulation
{
    GENERATED_BODY()

    /** Attention level (affects learning rate) */
    UPROPERTY(BlueprintReadWrite)
    float Attention = 1.0f;

    /** Arousal level (affects exploration) */
    UPROPERTY(BlueprintReadWrite)
    float Arousal = 0.5f;

    /** Confidence (affects action selection) */
    UPROPERTY(BlueprintReadWrite)
    float Confidence = 0.5f;

    /** Curiosity (affects exploration bonus) */
    UPROPERTY(BlueprintReadWrite)
    float Curiosity = 0.5f;

    /** Frustration (can trigger strategy change) */
    UPROPERTY(BlueprintReadWrite)
    float Frustration = 0.0f;

    /** Flow state (optimal challenge level) */
    UPROPERTY(BlueprintReadWrite)
    float FlowState = 0.0f;
};

/**
 * Reservoir RL state
 */
USTRUCT(BlueprintType)
struct FReservoirRLState
{
    GENERATED_BODY()

    /** Current reservoir state */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> ReservoirState;

    /** Echo state memory */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> EchoMemory;

    /** Temporal pattern buffer */
    UPROPERTY(BlueprintReadWrite)
    TArray<TArray<float>> TemporalBuffer;

    /** Detected patterns */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> DetectedPatterns;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActionSelected, const FRLAction&, Action, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTransitionRecorded, const FTransition&, Transition);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLearningUpdate, float, Loss, float, AverageReward);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCognitiveModulationChanged, const FCognitiveModulation&, Modulation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPolicyImproved, float, OldValue, float, NewValue);

/**
 * Reinforcement Learning Bridge Component
 * Connects game training with Deep Tree Echo cognitive systems
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UReinforcementLearningBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    UReinforcementLearningBridge();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Action selection policy */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    EActionSelectionPolicy SelectionPolicy = EActionSelectionPolicy::EpsilonGreedy;

    /** Learning algorithm */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    ELearningAlgorithm Algorithm = ELearningAlgorithm::QLearning;

    /** Learning rate (alpha) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LearningRate = 0.1f;

    /** Discount factor (gamma) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DiscountFactor = 0.99f;

    /** Exploration rate (epsilon) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ExplorationRate = 0.3f;

    /** Exploration decay */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float ExplorationDecay = 0.995f;

    /** Minimum exploration rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float MinExplorationRate = 0.01f;

    /** Softmax temperature */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float SoftmaxTemperature = 1.0f;

    /** Experience replay buffer size */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 ReplayBufferSize = 10000;

    /** Batch size for learning updates */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 BatchSize = 32;

    /** Update frequency (steps between updates) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 UpdateFrequency = 4;

    /** Enable cognitive modulation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    bool bUseCognitiveModulation = true;

    /** Enable reservoir computing integration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    bool bUseReservoirIntegration = true;

    /** Number of discrete actions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 NumActions = 16;

    /** Action names */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    TArray<FString> ActionNames;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnActionSelected OnActionSelected;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnTransitionRecorded OnTransitionRecorded;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnLearningUpdate OnLearningUpdate;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCognitiveModulationChanged OnCognitiveModulationChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPolicyImproved OnPolicyImproved;

    // ========================================
    // ACTION SELECTION
    // ========================================

    /** Select action given current state */
    UFUNCTION(BlueprintCallable, Category = "Actions")
    FRLAction SelectAction(const TArray<float>& State);

    /** Select action from game state observation */
    UFUNCTION(BlueprintCallable, Category = "Actions")
    FRLAction SelectActionFromObservation(const FGameStateObservation& Observation);

    /** Get all action Q-values for state */
    UFUNCTION(BlueprintPure, Category = "Actions")
    TArray<float> GetQValues(const TArray<float>& State) const;

    /** Get action probabilities (softmax) */
    UFUNCTION(BlueprintPure, Category = "Actions")
    TArray<float> GetActionProbabilities(const TArray<float>& State) const;

    /** Convert action to controller output */
    UFUNCTION(BlueprintPure, Category = "Actions")
    FControllerOutputCommand ActionToControllerOutput(const FRLAction& Action) const;

    /** Get greedy action (no exploration) */
    UFUNCTION(BlueprintPure, Category = "Actions")
    FRLAction GetGreedyAction(const TArray<float>& State) const;

    // ========================================
    // LEARNING
    // ========================================

    /** Record transition (s, a, r, s', done) */
    UFUNCTION(BlueprintCallable, Category = "Learning")
    void RecordTransition(const TArray<float>& State, const FRLAction& Action,
                          float Reward, const TArray<float>& NextState, bool bTerminal);

    /** Record transition from game components */
    UFUNCTION(BlueprintCallable, Category = "Learning")
    void RecordGameTransition(const FGameStateObservation& State,
                               const FControllerInputState& Input,
                               float Reward,
                               const FGameStateObservation& NextState,
                               bool bTerminal);

    /** Perform learning update */
    UFUNCTION(BlueprintCallable, Category = "Learning")
    float PerformLearningUpdate();

    /** Perform batch update from replay buffer */
    UFUNCTION(BlueprintCallable, Category = "Learning")
    float PerformBatchUpdate();

    /** Update Q-value directly */
    UFUNCTION(BlueprintCallable, Category = "Learning")
    void UpdateQValue(const TArray<float>& State, int32 ActionIndex, float Target);

    /** Clear experience replay buffer */
    UFUNCTION(BlueprintCallable, Category = "Learning")
    void ClearReplayBuffer();

    // ========================================
    // COGNITIVE INTEGRATION
    // ========================================

    /** Get current cognitive modulation */
    UFUNCTION(BlueprintPure, Category = "Cognitive")
    FCognitiveModulation GetCognitiveModulation() const;

    /** Set cognitive modulation manually */
    UFUNCTION(BlueprintCallable, Category = "Cognitive")
    void SetCognitiveModulation(const FCognitiveModulation& Modulation);

    /** Update modulation from cognitive state */
    UFUNCTION(BlueprintCallable, Category = "Cognitive")
    void UpdateCognitiveModulation();

    /** Get effective learning rate (modulated) */
    UFUNCTION(BlueprintPure, Category = "Cognitive")
    float GetEffectiveLearningRate() const;

    /** Get effective exploration rate (modulated) */
    UFUNCTION(BlueprintPure, Category = "Cognitive")
    float GetEffectiveExplorationRate() const;

    // ========================================
    // RESERVOIR INTEGRATION
    // ========================================

    /** Get reservoir-enhanced state representation */
    UFUNCTION(BlueprintCallable, Category = "Reservoir")
    TArray<float> GetReservoirState(const TArray<float>& Input);

    /** Update reservoir with current state */
    UFUNCTION(BlueprintCallable, Category = "Reservoir")
    void UpdateReservoir(const TArray<float>& State);

    /** Get temporal patterns from reservoir */
    UFUNCTION(BlueprintPure, Category = "Reservoir")
    TArray<FString> GetDetectedPatterns() const;

    /** Get echo memory for temporal context */
    UFUNCTION(BlueprintPure, Category = "Reservoir")
    TArray<float> GetEchoMemory() const;

    // ========================================
    // ACTIVE INFERENCE INTEGRATION
    // ========================================

    /** Select action using active inference */
    UFUNCTION(BlueprintCallable, Category = "ActiveInference")
    FRLAction SelectActionActiveInference(const TArray<float>& State);

    /** Compute expected free energy for action */
    UFUNCTION(BlueprintPure, Category = "ActiveInference")
    float ComputeExpectedFreeEnergy(const TArray<float>& State, int32 ActionIndex) const;

    /** Update beliefs from observation */
    UFUNCTION(BlueprintCallable, Category = "ActiveInference")
    void UpdateBeliefs(const TArray<float>& Observation);

    // ========================================
    // TRAINING CONTROL
    // ========================================

    /** Start training mode */
    UFUNCTION(BlueprintCallable, Category = "Training")
    void StartTraining();

    /** Stop training mode */
    UFUNCTION(BlueprintCallable, Category = "Training")
    void StopTraining();

    /** Is currently training */
    UFUNCTION(BlueprintPure, Category = "Training")
    bool IsTraining() const;

    /** Decay exploration rate */
    UFUNCTION(BlueprintCallable, Category = "Training")
    void DecayExploration();

    /** Save policy to array */
    UFUNCTION(BlueprintPure, Category = "Training")
    TArray<float> SavePolicy() const;

    /** Load policy from array */
    UFUNCTION(BlueprintCallable, Category = "Training")
    void LoadPolicy(const TArray<float>& PolicyData);

    /** Reset learning */
    UFUNCTION(BlueprintCallable, Category = "Training")
    void ResetLearning();

    // ========================================
    // STATISTICS
    // ========================================

    /** Get average reward */
    UFUNCTION(BlueprintPure, Category = "Statistics")
    float GetAverageReward() const;

    /** Get total steps */
    UFUNCTION(BlueprintPure, Category = "Statistics")
    int32 GetTotalSteps() const;

    /** Get total episodes */
    UFUNCTION(BlueprintPure, Category = "Statistics")
    int32 GetTotalEpisodes() const;

    /** Get replay buffer size */
    UFUNCTION(BlueprintPure, Category = "Statistics")
    int32 GetReplayBufferCount() const;

protected:
    // Component references
    UPROPERTY()
    UGameControllerInterface* ControllerInterface;

    UPROPERTY()
    UGameTrainingEnvironment* TrainingEnvironment;

    UPROPERTY()
    UGameSkillTrainingSystem* SkillSystem;

    UPROPERTY()
    UOnlineLearningSystem* LearningSystem;

    UPROPERTY()
    UEmbodiedCognitionComponent* EmbodimentComponent;

    UPROPERTY()
    UDeepTreeEchoCore* DTECore;

    // Q-table (for tabular Q-learning)
    TMap<FString, TArray<float>> QTable;

    // Experience replay buffer
    TArray<FTransition> ReplayBuffer;

    // State tracking
    TArray<float> LastState;
    FRLAction LastAction;
    int32 StepCounter = 0;

    // Training state
    bool bIsTraining = false;
    int32 TotalSteps = 0;
    int32 TotalEpisodes = 0;
    float TotalReward = 0.0f;
    int32 RewardCount = 0;

    // Cognitive modulation
    FCognitiveModulation CurrentModulation;

    // Reservoir state
    FReservoirRLState ReservoirState;

    // Internal methods
    void FindComponentReferences();
    void InitializeActionNames();
    void InitializeQTable();

    FString StateToKey(const TArray<float>& State) const;
    TArray<float> GetOrCreateQValues(const FString& StateKey);

    FRLAction SelectEpsilonGreedy(const TArray<float>& State);
    FRLAction SelectSoftmax(const TArray<float>& State);
    FRLAction SelectUCB(const TArray<float>& State);
    FRLAction SelectThompson(const TArray<float>& State);

    void ApplyQLearningUpdate(const FTransition& Transition);
    void ApplySARSAUpdate(const FTransition& Transition);

    TArray<FTransition> SampleFromReplayBuffer(int32 Count);
    void AddToReplayBuffer(const FTransition& Transition);

    float ComputeIntrinsicReward(const TArray<float>& State, int32 ActionIndex) const;
    float ComputeCuriosityBonus(const TArray<float>& State) const;

    void SyncWithCognitiveSystem();
};

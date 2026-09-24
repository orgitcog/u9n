// GameTrainingEnvironment.h
// Game State and Training Episode Management for Deep Tree Echo
// Provides the reinforcement learning environment interface

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameTrainingEnvironment.generated.h"

// Forward declarations
class UGameControllerInterface;
class UOnlineLearningSystem;
class UEmbodiedCognitionComponent;

/**
 * Game genre for preset configurations
 */
UENUM(BlueprintType)
enum class EGameGenre : uint8
{
    ActionRPG       UMETA(DisplayName = "Action RPG"),
    FPS             UMETA(DisplayName = "First Person Shooter"),
    Fighting        UMETA(DisplayName = "Fighting Game"),
    Racing          UMETA(DisplayName = "Racing Game"),
    Platformer      UMETA(DisplayName = "Platformer"),
    Sports          UMETA(DisplayName = "Sports Game"),
    Strategy        UMETA(DisplayName = "Strategy Game"),
    Puzzle          UMETA(DisplayName = "Puzzle Game"),
    Survival        UMETA(DisplayName = "Survival Game"),
    Custom          UMETA(DisplayName = "Custom")
};

/**
 * Training mode
 */
UENUM(BlueprintType)
enum class ETrainingMode : uint8
{
    Exploration     UMETA(DisplayName = "Exploration"),
    Exploitation    UMETA(DisplayName = "Exploitation"),
    Imitation       UMETA(DisplayName = "Imitation Learning"),
    SelfPlay        UMETA(DisplayName = "Self-Play"),
    CurriculumEasy  UMETA(DisplayName = "Curriculum - Easy"),
    CurriculumMed   UMETA(DisplayName = "Curriculum - Medium"),
    CurriculumHard  UMETA(DisplayName = "Curriculum - Hard"),
    Evaluation      UMETA(DisplayName = "Evaluation Only")
};

/**
 * Episode termination reason
 */
UENUM(BlueprintType)
enum class EEpisodeTermination : uint8
{
    None            UMETA(DisplayName = "Not Terminated"),
    Success         UMETA(DisplayName = "Success"),
    Failure         UMETA(DisplayName = "Failure"),
    Timeout         UMETA(DisplayName = "Timeout"),
    UserAbort       UMETA(DisplayName = "User Abort"),
    OutOfBounds     UMETA(DisplayName = "Out of Bounds"),
    Death           UMETA(DisplayName = "Death")
};

/**
 * Game state observation
 */
USTRUCT(BlueprintType)
struct FGameStateObservation
{
    GENERATED_BODY()

    /** Raw observation vector (varies by game) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> RawObservation;

    /** Player position in world */
    UPROPERTY(BlueprintReadWrite)
    FVector PlayerPosition = FVector::ZeroVector;

    /** Player velocity */
    UPROPERTY(BlueprintReadWrite)
    FVector PlayerVelocity = FVector::ZeroVector;

    /** Player rotation */
    UPROPERTY(BlueprintReadWrite)
    FRotator PlayerRotation = FRotator::ZeroRotator;

    /** Player health (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Health = 1.0f;

    /** Player stamina/energy (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Stamina = 1.0f;

    /** Player score */
    UPROPERTY(BlueprintReadWrite)
    float Score = 0.0f;

    /** Time remaining (if applicable) */
    UPROPERTY(BlueprintReadWrite)
    float TimeRemaining = -1.0f;

    /** Current objective/waypoint position */
    UPROPERTY(BlueprintReadWrite)
    FVector ObjectivePosition = FVector::ZeroVector;

    /** Distance to objective */
    UPROPERTY(BlueprintReadWrite)
    float DistanceToObjective = 0.0f;

    /** Nearby entities (enemies, items, etc.) */
    UPROPERTY(BlueprintReadWrite)
    TArray<FVector> NearbyEntities;

    /** Entity types (corresponding to NearbyEntities) */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> EntityTypes;

    /** Current game phase/level */
    UPROPERTY(BlueprintReadWrite)
    int32 GamePhase = 0;

    /** Is player grounded */
    UPROPERTY(BlueprintReadWrite)
    bool bIsGrounded = true;

    /** Is player in combat */
    UPROPERTY(BlueprintReadWrite)
    bool bInCombat = false;

    /** Custom game-specific data */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> CustomData;

    /** Timestamp */
    UPROPERTY(BlueprintReadWrite)
    float Timestamp = 0.0f;

    /** Convert to normalized vector for neural network input */
    TArray<float> ToNormalizedVector() const;

    /** Get state hash for Q-learning */
    FString GetStateHash() const;
};

/**
 * Reward signal breakdown
 */
USTRUCT(BlueprintType)
struct FRewardBreakdown
{
    GENERATED_BODY()

    /** Total reward */
    UPROPERTY(BlueprintReadWrite)
    float Total = 0.0f;

    /** Progress reward (moving toward objectives) */
    UPROPERTY(BlueprintReadWrite)
    float Progress = 0.0f;

    /** Survival reward (staying alive) */
    UPROPERTY(BlueprintReadWrite)
    float Survival = 0.0f;

    /** Combat reward (damage dealt minus received) */
    UPROPERTY(BlueprintReadWrite)
    float Combat = 0.0f;

    /** Exploration reward (discovering new areas) */
    UPROPERTY(BlueprintReadWrite)
    float Exploration = 0.0f;

    /** Efficiency reward (time/resource efficiency) */
    UPROPERTY(BlueprintReadWrite)
    float Efficiency = 0.0f;

    /** Skill execution reward (combos, techniques) */
    UPROPERTY(BlueprintReadWrite)
    float SkillExecution = 0.0f;

    /** Penalty (negative rewards) */
    UPROPERTY(BlueprintReadWrite)
    float Penalty = 0.0f;

    void ComputeTotal();
};

/**
 * Training episode data
 */
USTRUCT(BlueprintType)
struct FTrainingEpisode
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    int32 EpisodeNumber = 0;

    UPROPERTY(BlueprintReadWrite)
    float StartTime = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float EndTime = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float Duration = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float TotalReward = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    int32 StepCount = 0;

    UPROPERTY(BlueprintReadWrite)
    EEpisodeTermination TerminationReason = EEpisodeTermination::None;

    UPROPERTY(BlueprintReadWrite)
    float FinalScore = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    FGameStateObservation InitialState;

    UPROPERTY(BlueprintReadWrite)
    FGameStateObservation FinalState;

    /** Performance metrics */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> Metrics;
};

/**
 * Training statistics
 */
USTRUCT(BlueprintType)
struct FTrainingStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    int32 TotalEpisodes = 0;

    UPROPERTY(BlueprintReadWrite)
    int32 SuccessfulEpisodes = 0;

    UPROPERTY(BlueprintReadWrite)
    float WinRate = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float AverageReward = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float BestReward = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float AverageDuration = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    int32 TotalSteps = 0;

    UPROPERTY(BlueprintReadWrite)
    float TotalTrainingTime = 0.0f;

    /** Recent episode rewards (sliding window) */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> RecentRewards;

    /** Moving average reward */
    UPROPERTY(BlueprintReadWrite)
    float MovingAverageReward = 0.0f;
};

/**
 * Reward shaping configuration
 */
USTRUCT(BlueprintType)
struct FRewardShaping
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ProgressWeight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SurvivalWeight = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CombatWeight = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExplorationWeight = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EfficiencyWeight = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SkillWeight = 0.3f;

    /** Reward clipping bounds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RewardClipMin = -10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RewardClipMax = 10.0f;

    /** Reward scaling */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RewardScale = 1.0f;

    /** Death penalty */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DeathPenalty = -5.0f;

    /** Success bonus */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SuccessBonus = 10.0f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEpisodeStarted, int32, EpisodeNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEpisodeEnded, const FTrainingEpisode&, Episode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStepCompleted, const FGameStateObservation&, State, float, Reward);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRewardReceived, const FRewardBreakdown&, Reward);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStateObserved, const FGameStateObservation&, State);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrainingModeChanged, ETrainingMode, NewMode);

/**
 * Game Training Environment Component
 * Provides RL environment interface for Deep Tree Echo game training
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UGameTrainingEnvironment : public UActorComponent
{
    GENERATED_BODY()

public:
    UGameTrainingEnvironment();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Game genre (affects reward shaping) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    EGameGenre GameGenre = EGameGenre::ActionRPG;

    /** Current training mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    ETrainingMode TrainingMode = ETrainingMode::Exploration;

    /** Maximum episode duration (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float MaxEpisodeDuration = 300.0f;

    /** Maximum steps per episode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxStepsPerEpisode = 10000;

    /** Observation update rate (per second) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float ObservationRate = 30.0f;

    /** Reward shaping configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    FRewardShaping RewardConfig;

    /** Auto-reset on episode end */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    bool bAutoReset = true;

    /** Record episode history */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    bool bRecordHistory = true;

    /** Maximum history size */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxHistorySize = 100;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnEpisodeStarted OnEpisodeStarted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnEpisodeEnded OnEpisodeEnded;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnStepCompleted OnStepCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnRewardReceived OnRewardReceived;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnStateObserved OnStateObserved;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnTrainingModeChanged OnTrainingModeChanged;

    // ========================================
    // ENVIRONMENT INTERFACE
    // ========================================

    /** Reset environment and start new episode */
    UFUNCTION(BlueprintCallable, Category = "Environment")
    FGameStateObservation Reset();

    /** Execute action and get next state/reward */
    UFUNCTION(BlueprintCallable, Category = "Environment")
    void Step(const TArray<float>& Action, FGameStateObservation& NextState,
              float& Reward, bool& bDone, FString& Info);

    /** Get current observation */
    UFUNCTION(BlueprintCallable, Category = "Environment")
    FGameStateObservation GetObservation() const;

    /** Manually end episode */
    UFUNCTION(BlueprintCallable, Category = "Environment")
    void EndEpisode(EEpisodeTermination Reason);

    /** Check if episode is active */
    UFUNCTION(BlueprintPure, Category = "Environment")
    bool IsEpisodeActive() const;

    /** Get current step in episode */
    UFUNCTION(BlueprintPure, Category = "Environment")
    int32 GetCurrentStep() const;

    /** Get episode elapsed time */
    UFUNCTION(BlueprintPure, Category = "Environment")
    float GetEpisodeElapsedTime() const;

    // ========================================
    // STATE MANAGEMENT
    // ========================================

    /** Set player state directly */
    UFUNCTION(BlueprintCallable, Category = "State")
    void SetPlayerState(const FVector& Position, const FRotator& Rotation,
                        float Health, float Stamina);

    /** Set objective position */
    UFUNCTION(BlueprintCallable, Category = "State")
    void SetObjective(const FVector& Position, const FString& ObjectiveType);

    /** Add nearby entity */
    UFUNCTION(BlueprintCallable, Category = "State")
    void AddNearbyEntity(const FVector& Position, const FString& EntityType);

    /** Clear nearby entities */
    UFUNCTION(BlueprintCallable, Category = "State")
    void ClearNearbyEntities();

    /** Set custom observation data */
    UFUNCTION(BlueprintCallable, Category = "State")
    void SetCustomData(const FString& Key, float Value);

    /** Set score */
    UFUNCTION(BlueprintCallable, Category = "State")
    void SetScore(float NewScore);

    /** Add to score */
    UFUNCTION(BlueprintCallable, Category = "State")
    void AddScore(float Delta);

    // ========================================
    // REWARD SYSTEM
    // ========================================

    /** Compute reward from current and previous state */
    UFUNCTION(BlueprintCallable, Category = "Reward")
    FRewardBreakdown ComputeReward(const FGameStateObservation& PrevState,
                                    const FGameStateObservation& CurrentState);

    /** Apply manual reward */
    UFUNCTION(BlueprintCallable, Category = "Reward")
    void ApplyReward(float Reward, const FString& Category);

    /** Apply penalty */
    UFUNCTION(BlueprintCallable, Category = "Reward")
    void ApplyPenalty(float Penalty, const FString& Reason);

    /** Signal combat hit (damage dealt) */
    UFUNCTION(BlueprintCallable, Category = "Reward")
    void SignalCombatHit(float DamageDealt);

    /** Signal combat damage (damage received) */
    UFUNCTION(BlueprintCallable, Category = "Reward")
    void SignalCombatDamage(float DamageReceived);

    /** Signal successful skill execution */
    UFUNCTION(BlueprintCallable, Category = "Reward")
    void SignalSkillSuccess(const FString& SkillName, float Quality);

    /** Signal exploration discovery */
    UFUNCTION(BlueprintCallable, Category = "Reward")
    void SignalExploration(const FVector& NewArea);

    // ========================================
    // TRAINING CONTROL
    // ========================================

    /** Set training mode */
    UFUNCTION(BlueprintCallable, Category = "Training")
    void SetTrainingMode(ETrainingMode Mode);

    /** Get current exploration rate (for epsilon-greedy) */
    UFUNCTION(BlueprintPure, Category = "Training")
    float GetExplorationRate() const;

    /** Set exploration rate */
    UFUNCTION(BlueprintCallable, Category = "Training")
    void SetExplorationRate(float Rate);

    /** Get curriculum difficulty level (0-1) */
    UFUNCTION(BlueprintPure, Category = "Training")
    float GetCurriculumDifficulty() const;

    /** Set curriculum difficulty */
    UFUNCTION(BlueprintCallable, Category = "Training")
    void SetCurriculumDifficulty(float Difficulty);

    /** Advance curriculum based on performance */
    UFUNCTION(BlueprintCallable, Category = "Training")
    void AdvanceCurriculum();

    // ========================================
    // STATISTICS
    // ========================================

    /** Get training statistics */
    UFUNCTION(BlueprintPure, Category = "Statistics")
    FTrainingStats GetTrainingStats() const;

    /** Get current episode data */
    UFUNCTION(BlueprintPure, Category = "Statistics")
    FTrainingEpisode GetCurrentEpisode() const;

    /** Get episode history */
    UFUNCTION(BlueprintPure, Category = "Statistics")
    TArray<FTrainingEpisode> GetEpisodeHistory() const;

    /** Get best episode */
    UFUNCTION(BlueprintPure, Category = "Statistics")
    FTrainingEpisode GetBestEpisode() const;

    /** Clear statistics */
    UFUNCTION(BlueprintCallable, Category = "Statistics")
    void ClearStatistics();

    // ========================================
    // PRESETS
    // ========================================

    /** Load reward preset for genre */
    UFUNCTION(BlueprintCallable, Category = "Presets")
    void LoadGenrePreset(EGameGenre Genre);

    /** Save current configuration */
    UFUNCTION(BlueprintCallable, Category = "Presets")
    FString SaveConfiguration() const;

    /** Load configuration from string */
    UFUNCTION(BlueprintCallable, Category = "Presets")
    bool LoadConfiguration(const FString& ConfigJson);

protected:
    // Component references
    UPROPERTY()
    UGameControllerInterface* ControllerInterface;

    UPROPERTY()
    UOnlineLearningSystem* LearningSystem;

    UPROPERTY()
    UEmbodiedCognitionComponent* EmbodimentComponent;

    // Current state
    FGameStateObservation CurrentObservation;
    FGameStateObservation PreviousObservation;

    // Episode state
    FTrainingEpisode CurrentEpisode;
    bool bEpisodeActive = false;
    int32 CurrentStep = 0;
    float EpisodeStartTime = 0.0f;
    float AccumulatedReward = 0.0f;

    // Training state
    float ExplorationRate = 0.3f;
    float CurriculumDifficulty = 0.0f;

    // History
    TArray<FTrainingEpisode> EpisodeHistory;
    FTrainingStats Stats;

    // Exploration tracking
    TSet<FIntVector> ExploredCells;
    float ExplorationCellSize = 100.0f;

    // Timing
    float LastObservationTime = 0.0f;
    float ObservationInterval = 0.0f;

    // Internal methods
    void FindComponentReferences();
    void InitializeEnvironment();

    void UpdateObservation();
    void CheckEpisodeTermination();
    void RecordEpisodeToHistory();
    void UpdateStatistics();

    FIntVector WorldToCell(const FVector& WorldPos) const;
    bool IsNewExploration(const FVector& Position);

    void ApplyRewardToLearning(float Reward, const FString& Context);
};

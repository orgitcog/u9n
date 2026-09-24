// OnlineLearningSystem.h
// Continuous Online Learning for Deep Tree Echo
// Implements reinforcement learning, pattern extraction, and skill acquisition

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OnlineLearningSystem.generated.h"

// Forward declarations
class UCognitiveCycleManager;
class UMemorySystems;
class UEmotionalSystem;

/**
 * Learning type enumeration
 */
UENUM(BlueprintType)
enum class ELearningType : uint8
{
    Reinforcement   UMETA(DisplayName = "Reinforcement"),
    Supervised      UMETA(DisplayName = "Supervised"),
    Unsupervised    UMETA(DisplayName = "Unsupervised"),
    Imitation       UMETA(DisplayName = "Imitation"),
    SelfSupervised  UMETA(DisplayName = "Self-Supervised")
};

/**
 * Skill level
 */
UENUM(BlueprintType)
enum class ESkillLevel : uint8
{
    Novice          UMETA(DisplayName = "Novice"),
    Beginner        UMETA(DisplayName = "Beginner"),
    Intermediate    UMETA(DisplayName = "Intermediate"),
    Advanced        UMETA(DisplayName = "Advanced"),
    Expert          UMETA(DisplayName = "Expert"),
    Master          UMETA(DisplayName = "Master")
};

/**
 * Learning experience
 */
USTRUCT(BlueprintType)
struct FLearningExperience
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ExperienceID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString State;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Action;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString NextState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Reward = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> ContextTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsTerminal = false;
};

/**
 * Learned pattern
 */
USTRUCT(BlueprintType)
struct FLearnedPattern
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PatternID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PatternName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Conditions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Actions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ActivationCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SuccessRate = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LastActivationTime = 0.0f;
};

/**
 * Acquired skill
 */
USTRUCT(BlueprintType)
struct FAcquiredSkill
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SkillID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SkillName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESkillLevel Level = ESkillLevel::Novice;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Proficiency = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PracticeCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LastPracticeTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Prerequisites;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> RelatedPatterns;
};

/**
 * Q-value entry
 */
USTRUCT(BlueprintType)
struct FQValueEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString State;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Action;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float QValue = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 VisitCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LastUpdateTime = 0.0f;
};

/**
 * Learning progress
 */
USTRUCT(BlueprintType)
struct FLearningProgress
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TotalExperiences = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PatternsLearned = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SkillsAcquired = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AverageReward = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LearningRate = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ExplorationRate = 0.3f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnExperienceRecorded, const FLearningExperience&, Experience);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPatternLearned, const FLearnedPattern&, Pattern);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillAcquired, const FAcquiredSkill&, Skill);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillLevelUp, const FString&, SkillID, ESkillLevel, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRewardReceived, float, Reward, const FString&, Context);

/**
 * Online Learning System Component
 * Implements continuous learning and skill acquisition
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UOnlineLearningSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UOnlineLearningSystem();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float LearningRate = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float DiscountFactor = 0.95f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float ExplorationRate = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float ExplorationDecay = 0.995f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float MinExplorationRate = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxExperienceBuffer = 1000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 PatternMinSupport = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float SkillDecayRate = 0.001f;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnExperienceRecorded OnExperienceRecorded;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPatternLearned OnPatternLearned;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSkillAcquired OnSkillAcquired;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSkillLevelUp OnSkillLevelUp;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnRewardReceived OnRewardReceived;

    // ========================================
    // EXPERIENCE RECORDING
    // ========================================

    /** Record a learning experience */
    UFUNCTION(BlueprintCallable, Category = "Experience")
    FLearningExperience RecordExperience(const FString& State, const FString& Action, 
                                          const FString& NextState, float Reward,
                                          const TArray<FString>& ContextTags, bool bTerminal);

    /** Get recent experiences */
    UFUNCTION(BlueprintPure, Category = "Experience")
    TArray<FLearningExperience> GetRecentExperiences(int32 Count) const;

    /** Get experiences by context */
    UFUNCTION(BlueprintPure, Category = "Experience")
    TArray<FLearningExperience> GetExperiencesByContext(const FString& ContextTag) const;

    /** Clear experience buffer */
    UFUNCTION(BlueprintCallable, Category = "Experience")
    void ClearExperienceBuffer();

    // ========================================
    // REINFORCEMENT LEARNING
    // ========================================

    /** Update Q-value for state-action pair */
    UFUNCTION(BlueprintCallable, Category = "Reinforcement")
    void UpdateQValue(const FString& State, const FString& Action, float Reward, const FString& NextState);

    /** Get Q-value for state-action pair */
    UFUNCTION(BlueprintPure, Category = "Reinforcement")
    float GetQValue(const FString& State, const FString& Action) const;

    /** Get best action for state */
    UFUNCTION(BlueprintPure, Category = "Reinforcement")
    FString GetBestAction(const FString& State) const;

    /** Get action using epsilon-greedy policy */
    UFUNCTION(BlueprintCallable, Category = "Reinforcement")
    FString SelectAction(const FString& State, const TArray<FString>& AvailableActions);

    /** Provide reward signal */
    UFUNCTION(BlueprintCallable, Category = "Reinforcement")
    void ProvideReward(float Reward, const FString& Context);

    // ========================================
    // PATTERN LEARNING
    // ========================================

    /** Extract patterns from experiences */
    UFUNCTION(BlueprintCallable, Category = "Patterns")
    void ExtractPatterns();

    /** Get learned pattern by ID */
    UFUNCTION(BlueprintPure, Category = "Patterns")
    FLearnedPattern GetPattern(const FString& PatternID) const;

    /** Get all learned patterns */
    UFUNCTION(BlueprintPure, Category = "Patterns")
    TArray<FLearnedPattern> GetAllPatterns() const;

    /** Match current situation to patterns */
    UFUNCTION(BlueprintPure, Category = "Patterns")
    TArray<FLearnedPattern> MatchPatterns(const TArray<FString>& CurrentConditions) const;

    /** Activate pattern */
    UFUNCTION(BlueprintCallable, Category = "Patterns")
    void ActivatePattern(const FString& PatternID, bool bSuccess);

    // ========================================
    // SKILL ACQUISITION
    // ========================================

    /** Acquire a new skill */
    UFUNCTION(BlueprintCallable, Category = "Skills")
    FAcquiredSkill AcquireSkill(const FString& SkillName, const FString& Description,
                                 const TArray<FString>& Prerequisites);

    /** Practice a skill */
    UFUNCTION(BlueprintCallable, Category = "Skills")
    void PracticeSkill(const FString& SkillID, float PerformanceScore);

    /** Get skill by ID */
    UFUNCTION(BlueprintPure, Category = "Skills")
    FAcquiredSkill GetSkill(const FString& SkillID) const;

    /** Get skill by name */
    UFUNCTION(BlueprintPure, Category = "Skills")
    FAcquiredSkill GetSkillByName(const FString& SkillName) const;

    /** Get all acquired skills */
    UFUNCTION(BlueprintPure, Category = "Skills")
    TArray<FAcquiredSkill> GetAllSkills() const;

    /** Get skills at or above level */
    UFUNCTION(BlueprintPure, Category = "Skills")
    TArray<FAcquiredSkill> GetSkillsAtLevel(ESkillLevel MinLevel) const;

    /** Check if skill prerequisites are met */
    UFUNCTION(BlueprintPure, Category = "Skills")
    bool ArePrerequisitesMet(const TArray<FString>& Prerequisites) const;

    // ========================================
    // LEARNING CONTROL
    // ========================================

    /** Set learning rate */
    UFUNCTION(BlueprintCallable, Category = "Control")
    void SetLearningRate(float NewRate);

    /** Set exploration rate */
    UFUNCTION(BlueprintCallable, Category = "Control")
    void SetExplorationRate(float NewRate);

    /** Decay exploration rate */
    UFUNCTION(BlueprintCallable, Category = "Control")
    void DecayExploration();

    /** Get learning progress */
    UFUNCTION(BlueprintPure, Category = "Control")
    FLearningProgress GetLearningProgress() const;

    /** Reset learning system */
    UFUNCTION(BlueprintCallable, Category = "Control")
    void ResetLearning();

protected:
    // Component references
    UPROPERTY()
    UCognitiveCycleManager* CycleManager;

    UPROPERTY()
    UMemorySystems* MemoryComponent;

    UPROPERTY()
    UEmotionalSystem* EmotionalComponent;

    // Internal state
    TArray<FLearningExperience> ExperienceBuffer;
    TMap<FString, FQValueEntry> QTable;
    TArray<FLearnedPattern> LearnedPatterns;
    TArray<FAcquiredSkill> AcquiredSkills;

    float TotalReward = 0.0f;
    int32 RewardCount = 0;

    int32 ExperienceIDCounter = 0;
    int32 PatternIDCounter = 0;
    int32 SkillIDCounter = 0;

    // Internal methods
    void FindComponentReferences();
    void InitializeLearningSystem();

    void UpdateSkillDecay(float DeltaTime);
    void PeriodicPatternExtraction();

    FString MakeQKey(const FString& State, const FString& Action) const;
    float GetMaxQValue(const FString& State) const;

    ESkillLevel ComputeSkillLevel(float Proficiency) const;
    void CheckSkillLevelUp(FAcquiredSkill& Skill);

    FString GenerateExperienceID();
    FString GeneratePatternID();
    FString GenerateSkillID();

    int32 FindSkillIndex(const FString& SkillID) const;
    int32 FindPatternIndex(const FString& PatternID) const;
};

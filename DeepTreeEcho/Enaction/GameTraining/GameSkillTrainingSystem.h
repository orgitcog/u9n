// GameSkillTrainingSystem.h
// Game-Specific Skill Acquisition and Training for Deep Tree Echo
// Builds on cognitive skill system with game-specific motor learning

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameControllerInterface.h"
#include "GameTrainingEnvironment.h"
#include "GameSkillTrainingSystem.generated.h"

// Forward declarations
class UOnlineLearningSystem;
class UEmbodiedCognitionComponent;

/**
 * Skill category for games
 */
UENUM(BlueprintType)
enum class EGameSkillCategory : uint8
{
    Movement        UMETA(DisplayName = "Movement"),
    Combat          UMETA(DisplayName = "Combat"),
    Navigation      UMETA(DisplayName = "Navigation"),
    Timing          UMETA(DisplayName = "Timing"),
    Strategy        UMETA(DisplayName = "Strategy"),
    Combo           UMETA(DisplayName = "Combo Execution"),
    Defense         UMETA(DisplayName = "Defense"),
    Resource        UMETA(DisplayName = "Resource Management"),
    Social          UMETA(DisplayName = "Social/Communication")
};

/**
 * Mastery level for skills
 */
UENUM(BlueprintType)
enum class ESkillMastery : uint8
{
    Unknown         UMETA(DisplayName = "Unknown"),
    Learning        UMETA(DisplayName = "Learning"),
    Competent       UMETA(DisplayName = "Competent"),
    Proficient      UMETA(DisplayName = "Proficient"),
    Expert          UMETA(DisplayName = "Expert"),
    Mastered        UMETA(DisplayName = "Mastered")
};

/**
 * Game skill definition
 */
USTRUCT(BlueprintType)
struct FGameSkill
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SkillID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString SkillName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGameSkillCategory Category = EGameSkillCategory::Movement;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESkillMastery Mastery = ESkillMastery::Unknown;

    /** Proficiency (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Proficiency = 0.0f;

    /** Number of practice attempts */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PracticeCount = 0;

    /** Successful executions */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SuccessCount = 0;

    /** Success rate */
    UPROPERTY(BlueprintReadWrite)
    float SuccessRate = 0.0f;

    /** Average execution quality (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float AverageQuality = 0.0f;

    /** Best execution quality */
    UPROPERTY(BlueprintReadWrite)
    float BestQuality = 0.0f;

    /** Time spent practicing (seconds) */
    UPROPERTY(BlueprintReadWrite)
    float TotalPracticeTime = 0.0f;

    /** Prerequisites (skill IDs) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Prerequisites;

    /** Controller input pattern for this skill */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FControllerInputState> InputPattern;

    /** Timing window for input pattern (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TimingWindow = 0.5f;

    /** Difficulty rating (1-10) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Difficulty = 1;

    /** Is this a combo skill */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsCombo = false;

    /** Last practice timestamp */
    UPROPERTY(BlueprintReadWrite)
    float LastPracticeTime = 0.0f;

    void UpdateSuccessRate();
    void UpdateMastery();
};

/**
 * Skill practice session
 */
USTRUCT(BlueprintType)
struct FSkillPracticeSession
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString SkillID;

    UPROPERTY(BlueprintReadWrite)
    float StartTime = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float EndTime = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    int32 Attempts = 0;

    UPROPERTY(BlueprintReadWrite)
    int32 Successes = 0;

    UPROPERTY(BlueprintReadWrite)
    float AverageQuality = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float Improvement = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    TArray<float> QualityHistory;
};

/**
 * Skill execution attempt
 */
USTRUCT(BlueprintType)
struct FSkillAttempt
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString SkillID;

    UPROPERTY(BlueprintReadWrite)
    float Timestamp = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    bool bSuccess = false;

    /** Quality of execution (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Quality = 0.0f;

    /** Timing accuracy (-1 to 1, 0 = perfect) */
    UPROPERTY(BlueprintReadWrite)
    float TimingAccuracy = 0.0f;

    /** Input accuracy (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float InputAccuracy = 0.0f;

    /** Context of attempt */
    UPROPERTY(BlueprintReadWrite)
    FString Context;

    /** Actual inputs used */
    UPROPERTY(BlueprintReadWrite)
    TArray<FControllerInputState> ActualInputs;
};

/**
 * Training curriculum stage
 */
USTRUCT(BlueprintType)
struct FCurriculumStage
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString StageName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> SkillsToLearn;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RequiredProficiency = 0.6f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MinPracticeAttempts = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCompleted = false;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillAttempted, const FString&, SkillID, const FSkillAttempt&, Attempt);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillMasteryChanged, const FString&, SkillID, ESkillMastery, NewMastery);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPracticeSessionStarted, const FString&, SkillID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPracticeSessionEnded, const FSkillPracticeSession&, Session);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCurriculumStageCompleted, const FCurriculumStage&, Stage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSkillUnlocked, const FString&, SkillID, const FGameSkill&, Skill);

/**
 * Game Skill Training System Component
 * Manages game-specific skill acquisition with motor learning
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UGameSkillTrainingSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UGameSkillTrainingSystem();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Base learning rate for skills */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float BaseLearningRate = 0.05f;

    /** Skill decay rate (per hour of not practicing) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float SkillDecayRate = 0.01f;

    /** Minimum proficiency before decay starts */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float DecayThreshold = 0.3f;

    /** Enable adaptive difficulty */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    bool bAdaptiveDifficulty = true;

    /** Input pattern matching tolerance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float PatternMatchTolerance = 0.2f;

    /** Maximum recent attempts to track */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxRecentAttempts = 100;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSkillAttempted OnSkillAttempted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSkillMasteryChanged OnSkillMasteryChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPracticeSessionStarted OnPracticeSessionStarted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPracticeSessionEnded OnPracticeSessionEnded;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCurriculumStageCompleted OnCurriculumStageCompleted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnSkillUnlocked OnSkillUnlocked;

    // ========================================
    // SKILL MANAGEMENT
    // ========================================

    /** Register a new skill */
    UFUNCTION(BlueprintCallable, Category = "Skills")
    FGameSkill RegisterSkill(const FString& SkillName, EGameSkillCategory Category,
                              const TArray<FString>& Prerequisites, int32 Difficulty);

    /** Register skill with input pattern */
    UFUNCTION(BlueprintCallable, Category = "Skills")
    FGameSkill RegisterSkillWithPattern(const FString& SkillName, EGameSkillCategory Category,
                                         const TArray<FControllerInputState>& InputPattern,
                                         float TimingWindow, int32 Difficulty);

    /** Get skill by ID */
    UFUNCTION(BlueprintPure, Category = "Skills")
    FGameSkill GetSkill(const FString& SkillID) const;

    /** Get skill by name */
    UFUNCTION(BlueprintPure, Category = "Skills")
    FGameSkill GetSkillByName(const FString& SkillName) const;

    /** Get all skills */
    UFUNCTION(BlueprintPure, Category = "Skills")
    TArray<FGameSkill> GetAllSkills() const;

    /** Get skills by category */
    UFUNCTION(BlueprintPure, Category = "Skills")
    TArray<FGameSkill> GetSkillsByCategory(EGameSkillCategory Category) const;

    /** Get skills at mastery level or above */
    UFUNCTION(BlueprintPure, Category = "Skills")
    TArray<FGameSkill> GetSkillsAtMastery(ESkillMastery MinMastery) const;

    /** Check if skill prerequisites are met */
    UFUNCTION(BlueprintPure, Category = "Skills")
    bool ArePrerequisitesMet(const FString& SkillID) const;

    /** Get available skills (prerequisites met, not mastered) */
    UFUNCTION(BlueprintPure, Category = "Skills")
    TArray<FGameSkill> GetAvailableSkills() const;

    // ========================================
    // SKILL PRACTICE
    // ========================================

    /** Record a skill attempt */
    UFUNCTION(BlueprintCallable, Category = "Practice")
    FSkillAttempt RecordAttempt(const FString& SkillID, bool bSuccess, float Quality,
                                 const TArray<FControllerInputState>& ActualInputs,
                                 const FString& Context);

    /** Evaluate skill execution from input */
    UFUNCTION(BlueprintCallable, Category = "Practice")
    FSkillAttempt EvaluateExecution(const FString& SkillID,
                                     const TArray<FControllerInputState>& Inputs);

    /** Start focused practice session */
    UFUNCTION(BlueprintCallable, Category = "Practice")
    void StartPracticeSession(const FString& SkillID);

    /** End practice session */
    UFUNCTION(BlueprintCallable, Category = "Practice")
    FSkillPracticeSession EndPracticeSession();

    /** Get current practice session */
    UFUNCTION(BlueprintPure, Category = "Practice")
    FSkillPracticeSession GetCurrentSession() const;

    /** Is practice session active */
    UFUNCTION(BlueprintPure, Category = "Practice")
    bool IsPracticing() const;

    /** Get recent attempts for skill */
    UFUNCTION(BlueprintPure, Category = "Practice")
    TArray<FSkillAttempt> GetRecentAttempts(const FString& SkillID, int32 Count) const;

    // ========================================
    // INPUT PATTERN MATCHING
    // ========================================

    /** Match input sequence to skill patterns */
    UFUNCTION(BlueprintCallable, Category = "Pattern")
    TArray<FString> MatchInputToSkills(const TArray<FControllerInputState>& Inputs) const;

    /** Get match quality for specific skill */
    UFUNCTION(BlueprintPure, Category = "Pattern")
    float GetPatternMatchQuality(const FString& SkillID,
                                  const TArray<FControllerInputState>& Inputs) const;

    /** Detect skill execution in real-time */
    UFUNCTION(BlueprintCallable, Category = "Pattern")
    FString DetectSkillExecution(const TArray<FControllerInputState>& RecentInputs);

    // ========================================
    // CURRICULUM
    // ========================================

    /** Add curriculum stage */
    UFUNCTION(BlueprintCallable, Category = "Curriculum")
    void AddCurriculumStage(const FCurriculumStage& Stage);

    /** Get current curriculum stage */
    UFUNCTION(BlueprintPure, Category = "Curriculum")
    FCurriculumStage GetCurrentStage() const;

    /** Get all curriculum stages */
    UFUNCTION(BlueprintPure, Category = "Curriculum")
    TArray<FCurriculumStage> GetCurriculum() const;

    /** Advance to next curriculum stage */
    UFUNCTION(BlueprintCallable, Category = "Curriculum")
    bool AdvanceCurriculum();

    /** Get recommended skill to practice */
    UFUNCTION(BlueprintPure, Category = "Curriculum")
    FString GetRecommendedSkill() const;

    /** Load preset curriculum for game type */
    UFUNCTION(BlueprintCallable, Category = "Curriculum")
    void LoadPresetCurriculum(EGameGenre Genre);

    // ========================================
    // ANALYTICS
    // ========================================

    /** Get learning curve for skill */
    UFUNCTION(BlueprintPure, Category = "Analytics")
    TArray<float> GetLearningCurve(const FString& SkillID) const;

    /** Get overall skill profile */
    UFUNCTION(BlueprintPure, Category = "Analytics")
    TMap<EGameSkillCategory, float> GetSkillProfile() const;

    /** Get estimated time to mastery */
    UFUNCTION(BlueprintPure, Category = "Analytics")
    float EstimateTimeToMastery(const FString& SkillID) const;

    /** Get skill correlation (which skills help each other) */
    UFUNCTION(BlueprintPure, Category = "Analytics")
    TMap<FString, float> GetSkillCorrelations(const FString& SkillID) const;

    // ========================================
    // PRESETS
    // ========================================

    /** Load skill presets for game type */
    UFUNCTION(BlueprintCallable, Category = "Presets")
    void LoadSkillPresets(EGameGenre Genre);

    /** Register common movement skills */
    UFUNCTION(BlueprintCallable, Category = "Presets")
    void RegisterMovementSkills();

    /** Register common combat skills */
    UFUNCTION(BlueprintCallable, Category = "Presets")
    void RegisterCombatSkills();

protected:
    // Component references
    UPROPERTY()
    UGameControllerInterface* ControllerInterface;

    UPROPERTY()
    UGameTrainingEnvironment* TrainingEnvironment;

    UPROPERTY()
    UOnlineLearningSystem* LearningSystem;

    UPROPERTY()
    UEmbodiedCognitionComponent* EmbodimentComponent;

    // Skill registry
    TMap<FString, FGameSkill> Skills;
    int32 SkillIDCounter = 0;

    // Attempt history
    TArray<FSkillAttempt> RecentAttempts;

    // Practice session
    FSkillPracticeSession CurrentSession;
    bool bInPracticeSession = false;

    // Curriculum
    TArray<FCurriculumStage> Curriculum;
    int32 CurrentStageIndex = 0;

    // Internal methods
    void FindComponentReferences();
    void InitializeDefaultSkills();

    void UpdateSkillDecay(float DeltaTime);
    void CheckCurriculumProgress();

    FString GenerateSkillID();
    int32 FindSkillIndex(const FString& SkillID) const;

    float ComputeLearningRate(const FGameSkill& Skill) const;
    float ComputeInputSimilarity(const FControllerInputState& A, const FControllerInputState& B) const;
    float ComputeSequenceSimilarity(const TArray<FControllerInputState>& A,
                                     const TArray<FControllerInputState>& B) const;

    void UpdateSkillProficiency(FGameSkill& Skill, const FSkillAttempt& Attempt);
    void BroadcastToLearningSystem(const FGameSkill& Skill, const FSkillAttempt& Attempt);
};

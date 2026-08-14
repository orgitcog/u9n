#pragma once

/**
 * Hierarchical Goal Manager for Deep-Tree-Echo
 * 
 * Implements a multi-level goal management system based on:
 * - Entelechy: Goal-directed actualization toward potential
 * - System 5 Architecture: 7 sets with nested concurrency
 * - 4E Cognition: Goals as embodied, embedded, enacted, extended
 * 
 * Goal Hierarchy:
 * - Life Goals (Entelechy): Long-term self-actualization
 * - Strategic Goals: Medium-term objectives
 * - Tactical Goals: Short-term plans
 * - Operational Goals: Immediate actions
 * 
 * Integrates with the 12-step cognitive cycle for goal-directed behavior.
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../../Cognitive/CognitiveCycleManager.h"
#include "HierarchicalGoalManager.generated.h"

// ========================================
// FORWARD DECLARATIONS
// ========================================

class UEntelechyFramework;
class UEpisodicMemorySystem;
class USensorimotorIntegration;

// ========================================
// ENUMERATIONS
// ========================================

/**
 * Goal Level in Hierarchy
 */
UENUM(BlueprintType)
enum class EGoalLevel : uint8
{
    /** Life-level goals (entelechy) */
    Life UMETA(DisplayName = "Life"),
    
    /** Strategic goals (long-term) */
    Strategic UMETA(DisplayName = "Strategic"),
    
    /** Tactical goals (medium-term) */
    Tactical UMETA(DisplayName = "Tactical"),
    
    /** Operational goals (immediate) */
    Operational UMETA(DisplayName = "Operational")
};

/**
 * Goal Status
 */
UENUM(BlueprintType)
enum class EGoalStatus : uint8
{
    /** Not yet started */
    Pending UMETA(DisplayName = "Pending"),
    
    /** Currently being pursued */
    Active UMETA(DisplayName = "Active"),
    
    /** Temporarily suspended */
    Suspended UMETA(DisplayName = "Suspended"),
    
    /** Successfully completed */
    Completed UMETA(DisplayName = "Completed"),
    
    /** Failed or abandoned */
    Failed UMETA(DisplayName = "Failed"),
    
    /** Superseded by another goal */
    Superseded UMETA(DisplayName = "Superseded")
};

/**
 * Goal Priority
 */
UENUM(BlueprintType)
enum class EGoalPriority : uint8
{
    /** Critical - must be addressed immediately */
    Critical UMETA(DisplayName = "Critical"),
    
    /** High - important for progress */
    High UMETA(DisplayName = "High"),
    
    /** Medium - standard priority */
    Medium UMETA(DisplayName = "Medium"),
    
    /** Low - can be deferred */
    Low UMETA(DisplayName = "Low"),
    
    /** Background - opportunistic */
    Background UMETA(DisplayName = "Background")
};

/**
 * Goal Type (System 5 mapping)
 */
UENUM(BlueprintType)
enum class EGoalType : uint8
{
    /** Universal Primary (U1) - Core being */
    UniversalPrimary UMETA(DisplayName = "Universal Primary"),
    
    /** Universal Secondary (U2) - Growth */
    UniversalSecondary UMETA(DisplayName = "Universal Secondary"),
    
    /** Universal Tertiary (U3) - Integration */
    UniversalTertiary UMETA(DisplayName = "Universal Tertiary"),
    
    /** Particular 1 (P1) - Immediate needs */
    Particular1 UMETA(DisplayName = "Particular 1"),
    
    /** Particular 2 (P2) - Short-term objectives */
    Particular2 UMETA(DisplayName = "Particular 2"),
    
    /** Particular 3 (P3) - Medium-term plans */
    Particular3 UMETA(DisplayName = "Particular 3"),
    
    /** Particular 4 (P4) - Long-term aspirations */
    Particular4 UMETA(DisplayName = "Particular 4")
};

/**
 * Conflict Resolution Strategy
 */
UENUM(BlueprintType)
enum class EConflictResolution : uint8
{
    /** Higher priority wins */
    PriorityBased UMETA(DisplayName = "Priority Based"),
    
    /** Higher level wins */
    HierarchyBased UMETA(DisplayName = "Hierarchy Based"),
    
    /** More urgent wins */
    UrgencyBased UMETA(DisplayName = "Urgency Based"),
    
    /** Seek compromise */
    Compromise UMETA(DisplayName = "Compromise"),
    
    /** Defer decision */
    Defer UMETA(DisplayName = "Defer")
};

// ========================================
// STRUCTURES
// ========================================

/**
 * Goal Condition - Success/failure criteria
 */
USTRUCT(BlueprintType)
struct FGoalCondition
{
    GENERATED_BODY()

    /** Condition identifier */
    UPROPERTY(BlueprintReadWrite)
    FName ConditionID;

    /** Description of condition */
    UPROPERTY(BlueprintReadWrite)
    FString Description;

    /** Target value (for numeric conditions) */
    UPROPERTY(BlueprintReadWrite)
    float TargetValue = 1.0f;

    /** Current value */
    UPROPERTY(BlueprintReadWrite)
    float CurrentValue = 0.0f;

    /** Comparison operator (>, <, ==, >=, <=) */
    UPROPERTY(BlueprintReadWrite)
    FString ComparisonOp = TEXT(">=");

    /** Is this condition satisfied */
    UPROPERTY(BlueprintReadWrite)
    bool bSatisfied = false;

    /** Weight in overall goal completion (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Weight = 1.0f;
};

/**
 * Goal - Hierarchical goal structure
 */
USTRUCT(BlueprintType)
struct FGoal
{
    GENERATED_BODY()

    /** Unique goal identifier */
    UPROPERTY(BlueprintReadWrite)
    int32 GoalID = 0;

    /** Goal name */
    UPROPERTY(BlueprintReadWrite)
    FName GoalName;

    /** Goal description */
    UPROPERTY(BlueprintReadWrite)
    FString Description;

    /** Goal level in hierarchy */
    UPROPERTY(BlueprintReadWrite)
    EGoalLevel Level = EGoalLevel::Operational;

    /** Goal status */
    UPROPERTY(BlueprintReadWrite)
    EGoalStatus Status = EGoalStatus::Pending;

    /** Goal priority */
    UPROPERTY(BlueprintReadWrite)
    EGoalPriority Priority = EGoalPriority::Medium;

    /** Goal type (System 5 mapping) */
    UPROPERTY(BlueprintReadWrite)
    EGoalType Type = EGoalType::Particular1;

    /** Parent goal ID (-1 if root) */
    UPROPERTY(BlueprintReadWrite)
    int32 ParentGoalID = -1;

    /** Child goal IDs */
    UPROPERTY(BlueprintReadWrite)
    TArray<int32> ChildGoalIDs;

    /** Success conditions */
    UPROPERTY(BlueprintReadWrite)
    TArray<FGoalCondition> SuccessConditions;

    /** Failure conditions */
    UPROPERTY(BlueprintReadWrite)
    TArray<FGoalCondition> FailureConditions;

    /** Progress toward completion (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Progress = 0.0f;

    /** Urgency (0-1, increases over time) */
    UPROPERTY(BlueprintReadWrite)
    float Urgency = 0.0f;

    /** Importance (0-1, intrinsic value) */
    UPROPERTY(BlueprintReadWrite)
    float Importance = 0.5f;

    /** Expected utility if achieved */
    UPROPERTY(BlueprintReadWrite)
    float ExpectedUtility = 0.0f;

    /** Creation timestamp */
    UPROPERTY(BlueprintReadWrite)
    float CreationTime = 0.0f;

    /** Deadline (0 = no deadline) */
    UPROPERTY(BlueprintReadWrite)
    float Deadline = 0.0f;

    /** Last update timestamp */
    UPROPERTY(BlueprintReadWrite)
    float LastUpdateTime = 0.0f;

    /** Associated affordances */
    UPROPERTY(BlueprintReadWrite)
    TArray<FName> AssociatedAffordances;

    /** Required skills */
    UPROPERTY(BlueprintReadWrite)
    TArray<FName> RequiredSkills;

    /** Conflicting goal IDs */
    UPROPERTY(BlueprintReadWrite)
    TArray<int32> ConflictingGoals;

    /** Supporting goal IDs */
    UPROPERTY(BlueprintReadWrite)
    TArray<int32> SupportingGoals;
};

/**
 * Goal Stack Frame - Active goal context
 */
USTRUCT(BlueprintType)
struct FGoalStackFrame
{
    GENERATED_BODY()

    /** Goal ID */
    UPROPERTY(BlueprintReadWrite)
    int32 GoalID = -1;

    /** Entry timestamp */
    UPROPERTY(BlueprintReadWrite)
    float EntryTime = 0.0f;

    /** Accumulated pursuit time */
    UPROPERTY(BlueprintReadWrite)
    float PursuitTime = 0.0f;

    /** Current subgoal index */
    UPROPERTY(BlueprintReadWrite)
    int32 CurrentSubgoalIndex = 0;

    /** Local context data */
    UPROPERTY(BlueprintReadWrite)
    TMap<FName, float> ContextData;
};

/**
 * Goal Conflict - Detected conflict between goals
 */
USTRUCT(BlueprintType)
struct FGoalConflict
{
    GENERATED_BODY()

    /** First conflicting goal */
    UPROPERTY(BlueprintReadWrite)
    int32 GoalA = -1;

    /** Second conflicting goal */
    UPROPERTY(BlueprintReadWrite)
    int32 GoalB = -1;

    /** Conflict severity (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Severity = 0.0f;

    /** Conflict type description */
    UPROPERTY(BlueprintReadWrite)
    FString ConflictType;

    /** Recommended resolution */
    UPROPERTY(BlueprintReadWrite)
    EConflictResolution RecommendedResolution = EConflictResolution::PriorityBased;

    /** Is resolved */
    UPROPERTY(BlueprintReadWrite)
    bool bResolved = false;
};

/**
 * Goal Manager Configuration
 */
USTRUCT(BlueprintType)
struct FGoalManagerConfig
{
    GENERATED_BODY()

    /** Maximum active goals per level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "1", ClampMax = "10"))
    int32 MaxActivePerLevel = 3;

    /** Urgency increase rate per second */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", ClampMax = "0.1"))
    float UrgencyIncreaseRate = 0.01f;

    /** Goal timeout (seconds, 0 = no timeout) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0"))
    float DefaultTimeout = 0.0f;

    /** Enable automatic subgoal generation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableAutoSubgoals = true;

    /** Enable conflict detection */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableConflictDetection = true;

    /** Default conflict resolution strategy */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EConflictResolution DefaultConflictResolution = EConflictResolution::PriorityBased;

    /** Progress update interval (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.1", ClampMax = "5.0"))
    float ProgressUpdateInterval = 0.5f;
};

/**
 * Goal Manager State
 */
USTRUCT(BlueprintType)
struct FGoalManagerState
{
    GENERATED_BODY()

    /** Current goal stack */
    UPROPERTY(BlueprintReadWrite)
    TArray<FGoalStackFrame> GoalStack;

    /** Active goal count per level */
    UPROPERTY(BlueprintReadWrite)
    TMap<EGoalLevel, int32> ActiveCountPerLevel;

    /** Current focus goal ID */
    UPROPERTY(BlueprintReadWrite)
    int32 FocusGoalID = -1;

    /** Detected conflicts */
    UPROPERTY(BlueprintReadWrite)
    TArray<FGoalConflict> ActiveConflicts;

    /** Overall goal coherence (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float GoalCoherence = 1.0f;

    /** Total goals created */
    UPROPERTY(BlueprintReadWrite)
    int32 TotalGoalsCreated = 0;

    /** Total goals completed */
    UPROPERTY(BlueprintReadWrite)
    int32 TotalGoalsCompleted = 0;
};

/**
 * Hierarchical Goal Manager Component
 * 
 * Manages goal creation, pursuit, and resolution for Deep-Tree-Echo.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UHierarchicalGoalManager : public UActorComponent
{
    GENERATED_BODY()

public:
    UHierarchicalGoalManager();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Goal manager configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goals|Config")
    FGoalManagerConfig Config;

    /** Reference to CognitiveCycleManager */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goals|Config")
    UCognitiveCycleManager* CognitiveCycleManager;

    /** Reference to Entelechy Framework */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goals|Config")
    UEntelechyFramework* EntelechyFramework;

    /** Reference to Memory System */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Goals|Config")
    UEpisodicMemorySystem* MemorySystem;

    // ========================================
    // STATE
    // ========================================

    /** Current manager state */
    UPROPERTY(BlueprintReadOnly, Category = "Goals|State")
    FGoalManagerState State;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize goal manager */
    UFUNCTION(BlueprintCallable, Category = "Goals")
    void Initialize();

    /** Reset goal manager */
    UFUNCTION(BlueprintCallable, Category = "Goals")
    void Reset();

    // ========================================
    // PUBLIC API - GOAL CREATION
    // ========================================

    /** Create new goal */
    UFUNCTION(BlueprintCallable, Category = "Goals|Creation")
    int32 CreateGoal(FName GoalName, const FString& Description, EGoalLevel Level, EGoalPriority Priority = EGoalPriority::Medium);

    /** Create subgoal under parent */
    UFUNCTION(BlueprintCallable, Category = "Goals|Creation")
    int32 CreateSubgoal(int32 ParentGoalID, FName GoalName, const FString& Description);

    /** Add success condition to goal */
    UFUNCTION(BlueprintCallable, Category = "Goals|Creation")
    void AddSuccessCondition(int32 GoalID, FName ConditionID, float TargetValue, const FString& ComparisonOp = TEXT(">="));

    /** Add failure condition to goal */
    UFUNCTION(BlueprintCallable, Category = "Goals|Creation")
    void AddFailureCondition(int32 GoalID, FName ConditionID, float TargetValue, const FString& ComparisonOp = TEXT(">="));

    /** Set goal deadline */
    UFUNCTION(BlueprintCallable, Category = "Goals|Creation")
    void SetGoalDeadline(int32 GoalID, float Deadline);

    /** Associate affordance with goal */
    UFUNCTION(BlueprintCallable, Category = "Goals|Creation")
    void AssociateAffordance(int32 GoalID, FName AffordanceName);

    /** Associate required skill with goal */
    UFUNCTION(BlueprintCallable, Category = "Goals|Creation")
    void AssociateSkill(int32 GoalID, FName SkillName);

    // ========================================
    // PUBLIC API - GOAL PURSUIT
    // ========================================

    /** Activate goal */
    UFUNCTION(BlueprintCallable, Category = "Goals|Pursuit")
    bool ActivateGoal(int32 GoalID);

    /** Suspend goal */
    UFUNCTION(BlueprintCallable, Category = "Goals|Pursuit")
    void SuspendGoal(int32 GoalID);

    /** Resume suspended goal */
    UFUNCTION(BlueprintCallable, Category = "Goals|Pursuit")
    void ResumeGoal(int32 GoalID);

    /** Abandon goal */
    UFUNCTION(BlueprintCallable, Category = "Goals|Pursuit")
    void AbandonGoal(int32 GoalID);

    /** Push goal onto stack (make current focus) */
    UFUNCTION(BlueprintCallable, Category = "Goals|Pursuit")
    void PushGoal(int32 GoalID);

    /** Pop goal from stack */
    UFUNCTION(BlueprintCallable, Category = "Goals|Pursuit")
    int32 PopGoal();

    /** Set focus goal */
    UFUNCTION(BlueprintCallable, Category = "Goals|Pursuit")
    void SetFocusGoal(int32 GoalID);

    // ========================================
    // PUBLIC API - GOAL PROGRESS
    // ========================================

    /** Update condition value */
    UFUNCTION(BlueprintCallable, Category = "Goals|Progress")
    void UpdateConditionValue(int32 GoalID, FName ConditionID, float NewValue);

    /** Update goal progress directly */
    UFUNCTION(BlueprintCallable, Category = "Goals|Progress")
    void UpdateGoalProgress(int32 GoalID, float Progress);

    /** Mark goal as completed */
    UFUNCTION(BlueprintCallable, Category = "Goals|Progress")
    void CompleteGoal(int32 GoalID);

    /** Mark goal as failed */
    UFUNCTION(BlueprintCallable, Category = "Goals|Progress")
    void FailGoal(int32 GoalID, const FString& Reason = TEXT(""));

    /** Get goal progress */
    UFUNCTION(BlueprintCallable, Category = "Goals|Progress")
    float GetGoalProgress(int32 GoalID) const;

    // ========================================
    // PUBLIC API - GOAL QUERIES
    // ========================================

    /** Get goal by ID */
    UFUNCTION(BlueprintCallable, Category = "Goals|Query")
    FGoal GetGoal(int32 GoalID) const;

    /** Get all goals at level */
    UFUNCTION(BlueprintCallable, Category = "Goals|Query")
    TArray<FGoal> GetGoalsAtLevel(EGoalLevel Level) const;

    /** Get active goals */
    UFUNCTION(BlueprintCallable, Category = "Goals|Query")
    TArray<FGoal> GetActiveGoals() const;

    /** Get current focus goal */
    UFUNCTION(BlueprintCallable, Category = "Goals|Query")
    FGoal GetFocusGoal() const;

    /** Get child goals */
    UFUNCTION(BlueprintCallable, Category = "Goals|Query")
    TArray<FGoal> GetChildGoals(int32 ParentGoalID) const;

    /** Get goals by priority */
    UFUNCTION(BlueprintCallable, Category = "Goals|Query")
    TArray<FGoal> GetGoalsByPriority(EGoalPriority Priority) const;

    /** Get goals associated with affordance */
    UFUNCTION(BlueprintCallable, Category = "Goals|Query")
    TArray<FGoal> GetGoalsForAffordance(FName AffordanceName) const;

    /** Check if goal exists */
    UFUNCTION(BlueprintCallable, Category = "Goals|Query")
    bool GoalExists(int32 GoalID) const;

    // ========================================
    // PUBLIC API - CONFLICT MANAGEMENT
    // ========================================

    /** Detect conflicts between goals */
    UFUNCTION(BlueprintCallable, Category = "Goals|Conflict")
    TArray<FGoalConflict> DetectConflicts();

    /** Resolve conflict */
    UFUNCTION(BlueprintCallable, Category = "Goals|Conflict")
    void ResolveConflict(int32 GoalA, int32 GoalB, EConflictResolution Strategy);

    /** Mark goals as conflicting */
    UFUNCTION(BlueprintCallable, Category = "Goals|Conflict")
    void MarkConflicting(int32 GoalA, int32 GoalB);

    /** Mark goals as supporting */
    UFUNCTION(BlueprintCallable, Category = "Goals|Conflict")
    void MarkSupporting(int32 GoalA, int32 GoalB);

    // ========================================
    // PUBLIC API - COGNITIVE CYCLE INTEGRATION
    // ========================================

    /** Process cognitive cycle step */
    UFUNCTION(BlueprintCallable, Category = "Goals|CognitiveCycle")
    void ProcessCognitiveStep(int32 GlobalStep, EConsciousnessStream ActiveStream);

    /** Get goal-directed attention weights */
    UFUNCTION(BlueprintCallable, Category = "Goals|CognitiveCycle")
    TArray<float> GetGoalDirectedAttention(const TArray<float>& CurrentPerception) const;

    /** Select action based on current goals */
    UFUNCTION(BlueprintCallable, Category = "Goals|CognitiveCycle")
    FName SelectGoalDirectedAction() const;

    // ========================================
    // PUBLIC API - SYSTEM 5 INTEGRATION
    // ========================================

    /** Get goals by System 5 type */
    UFUNCTION(BlueprintCallable, Category = "Goals|System5")
    TArray<FGoal> GetGoalsByType(EGoalType Type) const;

    /** Get Universal goals (U1, U2, U3) */
    UFUNCTION(BlueprintCallable, Category = "Goals|System5")
    TArray<FGoal> GetUniversalGoals() const;

    /** Get Particular goals (P1, P2, P3, P4) */
    UFUNCTION(BlueprintCallable, Category = "Goals|System5")
    TArray<FGoal> GetParticularGoals() const;

    /** Compute goal balance (Universal vs Particular) */
    UFUNCTION(BlueprintCallable, Category = "Goals|System5")
    float ComputeGoalBalance() const;

    // ========================================
    // DELEGATES
    // ========================================

    /** Called when goal is created */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGoalCreated, int32, GoalID, FName, GoalName);
    UPROPERTY(BlueprintAssignable, Category = "Goals|Events")
    FOnGoalCreated OnGoalCreated;

    /** Called when goal is activated */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGoalActivated, int32, GoalID);
    UPROPERTY(BlueprintAssignable, Category = "Goals|Events")
    FOnGoalActivated OnGoalActivated;

    /** Called when goal is completed */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGoalCompleted, int32, GoalID);
    UPROPERTY(BlueprintAssignable, Category = "Goals|Events")
    FOnGoalCompleted OnGoalCompleted;

    /** Called when goal fails */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGoalFailed, int32, GoalID, FString, Reason);
    UPROPERTY(BlueprintAssignable, Category = "Goals|Events")
    FOnGoalFailed OnGoalFailed;

    /** Called when conflict is detected */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnConflictDetected, int32, GoalA, int32, GoalB);
    UPROPERTY(BlueprintAssignable, Category = "Goals|Events")
    FOnConflictDetected OnConflictDetected;

    /** Called when focus changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFocusChanged, int32, OldGoalID, int32, NewGoalID);
    UPROPERTY(BlueprintAssignable, Category = "Goals|Events")
    FOnFocusChanged OnFocusChanged;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL DATA
    // ========================================

    /** All goals */
    TMap<int32, FGoal> AllGoals;

    /** Next goal ID */
    int32 NextGoalID = 1;

    /** Accumulated time */
    float AccumulatedTime = 0.0f;

    /** Last progress update time */
    float LastProgressUpdateTime = 0.0f;

    /** Last processed cognitive step */
    int32 LastProcessedStep = -1;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Update urgency for all goals */
    void UpdateUrgencies(float DeltaTime);

    /** Check goal conditions */
    void CheckGoalConditions(int32 GoalID);

    /** Compute goal progress from conditions */
    float ComputeProgressFromConditions(const FGoal& Goal) const;

    /** Propagate progress to parent goals */
    void PropagateProgressToParent(int32 GoalID);

    /** Auto-generate subgoals */
    void AutoGenerateSubgoals(int32 GoalID);

    /** Update goal coherence */
    void UpdateGoalCoherence();

    /** Get goal pointer */
    FGoal* GetGoalPtr(int32 GoalID);
    const FGoal* GetGoalPtr(int32 GoalID) const;

    /** Evaluate condition */
    bool EvaluateCondition(const FGoalCondition& Condition) const;

    /** Compute goal utility */
    float ComputeGoalUtility(const FGoal& Goal) const;
};

// PlanningSystem.h
// Goal-Directed Planning and Action Selection for Deep Tree Echo
// Implements hierarchical task network planning with means-ends analysis

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlanningSystem.generated.h"

// Forward declarations
class UCognitiveCycleManager;
class UMemorySystems;
class UOnlineLearningSystem;

/**
 * Goal status enumeration
 */
UENUM(BlueprintType)
enum class EGoalStatus : uint8
{
    Pending     UMETA(DisplayName = "Pending"),
    Active      UMETA(DisplayName = "Active"),
    Achieved    UMETA(DisplayName = "Achieved"),
    Failed      UMETA(DisplayName = "Failed"),
    Suspended   UMETA(DisplayName = "Suspended"),
    Abandoned   UMETA(DisplayName = "Abandoned")
};

/**
 * Goal priority
 */
UENUM(BlueprintType)
enum class EGoalPriority : uint8
{
    Low         UMETA(DisplayName = "Low"),
    Medium      UMETA(DisplayName = "Medium"),
    High        UMETA(DisplayName = "High"),
    Critical    UMETA(DisplayName = "Critical"),
    Survival    UMETA(DisplayName = "Survival")
};

/**
 * Plan status
 */
UENUM(BlueprintType)
enum class EPlanStatus : uint8
{
    Formulating UMETA(DisplayName = "Formulating"),
    Ready       UMETA(DisplayName = "Ready"),
    Executing   UMETA(DisplayName = "Executing"),
    Completed   UMETA(DisplayName = "Completed"),
    Failed      UMETA(DisplayName = "Failed"),
    Replanning  UMETA(DisplayName = "Replanning")
};

/**
 * Action status
 */
UENUM(BlueprintType)
enum class EActionStatus : uint8
{
    Pending     UMETA(DisplayName = "Pending"),
    Executing   UMETA(DisplayName = "Executing"),
    Completed   UMETA(DisplayName = "Completed"),
    Failed      UMETA(DisplayName = "Failed"),
    Skipped     UMETA(DisplayName = "Skipped")
};

/**
 * Goal structure
 */
USTRUCT(BlueprintType)
struct FGoal
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString GoalID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString GoalName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGoalStatus Status = EGoalStatus::Pending;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGoalPriority Priority = EGoalPriority::Medium;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Preconditions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> SuccessConditions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> SubGoals;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ParentGoalID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Progress = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Urgency = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Importance = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CreationTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Deadline = -1.0f;
};

/**
 * Planned action
 */
USTRUCT(BlueprintType)
struct FPlannedAction
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ActionID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ActionName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EActionStatus Status = EActionStatus::Pending;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Preconditions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Effects;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EstimatedDuration = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ActualDuration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Cost = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SequenceIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TargetGoalID;
};

/**
 * Plan structure
 */
USTRUCT(BlueprintType)
struct FPlan
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PlanID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PlanName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TargetGoalID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EPlanStatus Status = EPlanStatus::Formulating;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FPlannedAction> Actions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentActionIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EstimatedTotalDuration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TotalCost = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SuccessProbability = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CreationTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ReplanCount = 0;
};

/**
 * World state
 */
USTRUCT(BlueprintType)
struct FWorldState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, bool> BooleanFacts;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, float> NumericFacts;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, FString> StringFacts;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;
};

/**
 * Action template
 */
USTRUCT(BlueprintType)
struct FActionTemplate
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TemplateID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ActionName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Preconditions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Effects;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseCost = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BaseDuration = 1.0f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGoalCreated, const FGoal&, Goal);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGoalStatusChanged, const FString&, GoalID, EGoalStatus, NewStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlanCreated, const FPlan&, Plan);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlanStatusChanged, const FString&, PlanID, EPlanStatus, NewStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionStarted, const FPlannedAction&, Action);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActionCompleted, const FPlannedAction&, Action, bool, bSuccess);

/**
 * Planning System Component
 * Implements goal-directed planning and action selection
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UPlanningSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UPlanningSystem();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxGoals = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxPlans = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxPlanDepth = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float ReplanThreshold = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float ActionTimeout = 30.0f;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnGoalCreated OnGoalCreated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnGoalStatusChanged OnGoalStatusChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPlanCreated OnPlanCreated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPlanStatusChanged OnPlanStatusChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnActionStarted OnActionStarted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnActionCompleted OnActionCompleted;

    // ========================================
    // GOAL MANAGEMENT
    // ========================================

    /** Create a new goal */
    UFUNCTION(BlueprintCallable, Category = "Goals")
    FGoal CreateGoal(const FString& Name, const FString& Description, EGoalPriority Priority,
                      const TArray<FString>& SuccessConditions);

    /** Create a subgoal */
    UFUNCTION(BlueprintCallable, Category = "Goals")
    FGoal CreateSubGoal(const FString& ParentGoalID, const FString& Name, 
                         const TArray<FString>& SuccessConditions);

    /** Activate goal */
    UFUNCTION(BlueprintCallable, Category = "Goals")
    void ActivateGoal(const FString& GoalID);

    /** Suspend goal */
    UFUNCTION(BlueprintCallable, Category = "Goals")
    void SuspendGoal(const FString& GoalID);

    /** Abandon goal */
    UFUNCTION(BlueprintCallable, Category = "Goals")
    void AbandonGoal(const FString& GoalID);

    /** Get goal by ID */
    UFUNCTION(BlueprintPure, Category = "Goals")
    FGoal GetGoal(const FString& GoalID) const;

    /** Get all goals */
    UFUNCTION(BlueprintPure, Category = "Goals")
    TArray<FGoal> GetAllGoals() const;

    /** Get active goals */
    UFUNCTION(BlueprintPure, Category = "Goals")
    TArray<FGoal> GetActiveGoals() const;

    /** Get highest priority goal */
    UFUNCTION(BlueprintPure, Category = "Goals")
    FGoal GetHighestPriorityGoal() const;

    /** Check if goal is achieved */
    UFUNCTION(BlueprintPure, Category = "Goals")
    bool IsGoalAchieved(const FString& GoalID) const;

    // ========================================
    // PLANNING
    // ========================================

    /** Create plan for goal */
    UFUNCTION(BlueprintCallable, Category = "Planning")
    FPlan CreatePlan(const FString& GoalID);

    /** Execute plan */
    UFUNCTION(BlueprintCallable, Category = "Planning")
    void ExecutePlan(const FString& PlanID);

    /** Pause plan execution */
    UFUNCTION(BlueprintCallable, Category = "Planning")
    void PausePlan(const FString& PlanID);

    /** Resume plan execution */
    UFUNCTION(BlueprintCallable, Category = "Planning")
    void ResumePlan(const FString& PlanID);

    /** Cancel plan */
    UFUNCTION(BlueprintCallable, Category = "Planning")
    void CancelPlan(const FString& PlanID);

    /** Replan for goal */
    UFUNCTION(BlueprintCallable, Category = "Planning")
    FPlan Replan(const FString& GoalID);

    /** Get plan by ID */
    UFUNCTION(BlueprintPure, Category = "Planning")
    FPlan GetPlan(const FString& PlanID) const;

    /** Get plan for goal */
    UFUNCTION(BlueprintPure, Category = "Planning")
    FPlan GetPlanForGoal(const FString& GoalID) const;

    /** Get all plans */
    UFUNCTION(BlueprintPure, Category = "Planning")
    TArray<FPlan> GetAllPlans() const;

    /** Get current action */
    UFUNCTION(BlueprintPure, Category = "Planning")
    FPlannedAction GetCurrentAction() const;

    // ========================================
    // ACTION MANAGEMENT
    // ========================================

    /** Register action template */
    UFUNCTION(BlueprintCallable, Category = "Actions")
    void RegisterActionTemplate(const FString& ActionName, const TArray<FString>& Preconditions,
                                 const TArray<FString>& Effects, float Cost, float Duration);

    /** Complete current action */
    UFUNCTION(BlueprintCallable, Category = "Actions")
    void CompleteCurrentAction(bool bSuccess);

    /** Skip current action */
    UFUNCTION(BlueprintCallable, Category = "Actions")
    void SkipCurrentAction();

    /** Get available actions */
    UFUNCTION(BlueprintPure, Category = "Actions")
    TArray<FActionTemplate> GetAvailableActions() const;

    /** Get applicable actions for current state */
    UFUNCTION(BlueprintPure, Category = "Actions")
    TArray<FActionTemplate> GetApplicableActions() const;

    // ========================================
    // WORLD STATE
    // ========================================

    /** Set boolean fact */
    UFUNCTION(BlueprintCallable, Category = "World State")
    void SetBooleanFact(const FString& FactName, bool Value);

    /** Get boolean fact */
    UFUNCTION(BlueprintPure, Category = "World State")
    bool GetBooleanFact(const FString& FactName) const;

    /** Set numeric fact */
    UFUNCTION(BlueprintCallable, Category = "World State")
    void SetNumericFact(const FString& FactName, float Value);

    /** Get numeric fact */
    UFUNCTION(BlueprintPure, Category = "World State")
    float GetNumericFact(const FString& FactName) const;

    /** Set string fact */
    UFUNCTION(BlueprintCallable, Category = "World State")
    void SetStringFact(const FString& FactName, const FString& Value);

    /** Get string fact */
    UFUNCTION(BlueprintPure, Category = "World State")
    FString GetStringFact(const FString& FactName) const;

    /** Get current world state */
    UFUNCTION(BlueprintPure, Category = "World State")
    FWorldState GetWorldState() const;

    /** Check if condition is satisfied */
    UFUNCTION(BlueprintPure, Category = "World State")
    bool IsConditionSatisfied(const FString& Condition) const;

protected:
    // Component references
    UPROPERTY()
    UCognitiveCycleManager* CycleManager;

    UPROPERTY()
    UMemorySystems* MemoryComponent;

    UPROPERTY()
    UOnlineLearningSystem* LearningComponent;

    // Internal state
    TArray<FGoal> Goals;
    TArray<FPlan> Plans;
    TArray<FActionTemplate> ActionTemplates;
    FWorldState WorldState;

    FString CurrentPlanID;
    float CurrentActionStartTime = 0.0f;

    int32 GoalIDCounter = 0;
    int32 PlanIDCounter = 0;
    int32 ActionIDCounter = 0;

    // Internal methods
    void FindComponentReferences();
    void InitializePlanningSystem();
    void InitializeDefaultActions();

    void UpdateGoalProgress(float DeltaTime);
    void UpdatePlanExecution(float DeltaTime);
    void CheckGoalConditions();

    TArray<FPlannedAction> GenerateActionSequence(const FGoal& Goal);
    bool CanApplyAction(const FActionTemplate& Action) const;
    void ApplyActionEffects(const FPlannedAction& Action);

    float ComputeGoalUtility(const FGoal& Goal) const;
    void PrioritizeGoals();

    FString GenerateGoalID();
    FString GeneratePlanID();
    FString GenerateActionID();

    int32 FindGoalIndex(const FString& GoalID) const;
    int32 FindPlanIndex(const FString& PlanID) const;
};

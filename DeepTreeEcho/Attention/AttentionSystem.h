// AttentionSystem.h
// Selective Attention and Salience Computation for Deep Tree Echo
// Implements bottom-up and top-down attention with relevance realization

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AttentionSystem.generated.h"

// Forward declarations
class UCognitiveCycleManager;
class URelevanceRealizationEnnead;
class UMemorySystems;

/**
 * Attention mode enumeration
 */
UENUM(BlueprintType)
enum class EAttentionMode : uint8
{
    Diffuse         UMETA(DisplayName = "Diffuse (Broad)"),
    Focused         UMETA(DisplayName = "Focused (Narrow)"),
    Divided         UMETA(DisplayName = "Divided (Multi-Target)"),
    Sustained       UMETA(DisplayName = "Sustained (Long-Term)"),
    Alternating     UMETA(DisplayName = "Alternating (Switching)")
};

/**
 * Salience source type
 */
UENUM(BlueprintType)
enum class ESalienceSource : uint8
{
    BottomUp        UMETA(DisplayName = "Bottom-Up (Stimulus)"),
    TopDown         UMETA(DisplayName = "Top-Down (Goal)"),
    Emotional       UMETA(DisplayName = "Emotional (Affective)"),
    Social          UMETA(DisplayName = "Social (Agent)"),
    Novelty         UMETA(DisplayName = "Novelty (Surprise)")
};

/**
 * Attention target
 */
USTRUCT(BlueprintType)
struct FAttentionTarget
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TargetID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString TargetName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector WorldLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SalienceScore = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float BottomUpSalience = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TopDownSalience = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalSalience = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NoveltySalience = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttentionDuration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float LastAttendedTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsCurrentFocus = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> SemanticTags;
};

/**
 * Attention spotlight state
 */
USTRUCT(BlueprintType)
struct FAttentionSpotlight
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CurrentTargetID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector FocusLocation = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FocusRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Intensity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EAttentionMode Mode = EAttentionMode::Diffuse;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FocusDuration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FatigueLevel = 0.0f;
};

/**
 * Salience map entry
 */
USTRUCT(BlueprintType)
struct FSalienceMapEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Location = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Salience = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESalienceSource PrimarySource = ESalienceSource::BottomUp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Timestamp = 0.0f;
};

/**
 * Attention shift event
 */
USTRUCT(BlueprintType)
struct FAttentionShift
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString FromTargetID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ToTargetID;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ShiftTime = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ESalienceSource TriggerSource = ESalienceSource::BottomUp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bWasVoluntary = false;
};

/**
 * Attention state summary
 */
USTRUCT(BlueprintType)
struct FAttentionState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FAttentionSpotlight Spotlight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FAttentionTarget> ActiveTargets;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EAttentionMode CurrentMode = EAttentionMode::Diffuse;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OverallAlertness = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttentionalCapacity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ShiftCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AverageFixationDuration = 0.0f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttentionShifted, const FAttentionShift&, Shift);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetAcquired, const FAttentionTarget&, Target);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetLost, const FString&, TargetID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAttentionModeChanged, EAttentionMode, OldMode, EAttentionMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttentionFatigued, float, FatigueLevel);

/**
 * Attention System Component
 * Implements selective attention and salience computation
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UAttentionSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UAttentionSystem();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    int32 MaxActiveTargets = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float AttentionDecayRate = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float SalienceThreshold = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float FatigueRate = 0.01f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float RecoveryRate = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float MinShiftInterval = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float TopDownWeight = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float BottomUpWeight = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float EmotionalWeight = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float NoveltyWeight = 0.1f;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnAttentionShifted OnAttentionShifted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnTargetAcquired OnTargetAcquired;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnTargetLost OnTargetLost;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnAttentionModeChanged OnAttentionModeChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnAttentionFatigued OnAttentionFatigued;

    // ========================================
    // TARGET MANAGEMENT
    // ========================================

    /** Register a new attention target */
    UFUNCTION(BlueprintCallable, Category = "Targets")
    FAttentionTarget RegisterTarget(const FString& Name, FVector Location, 
                                     float InitialSalience, const TArray<FString>& Tags);

    /** Update target location */
    UFUNCTION(BlueprintCallable, Category = "Targets")
    void UpdateTargetLocation(const FString& TargetID, FVector NewLocation);

    /** Update target salience */
    UFUNCTION(BlueprintCallable, Category = "Targets")
    void UpdateTargetSalience(const FString& TargetID, ESalienceSource Source, float Salience);

    /** Remove target */
    UFUNCTION(BlueprintCallable, Category = "Targets")
    void RemoveTarget(const FString& TargetID);

    /** Get target by ID */
    UFUNCTION(BlueprintPure, Category = "Targets")
    FAttentionTarget GetTarget(const FString& TargetID) const;

    /** Get all active targets */
    UFUNCTION(BlueprintPure, Category = "Targets")
    TArray<FAttentionTarget> GetActiveTargets() const;

    /** Get current focus target */
    UFUNCTION(BlueprintPure, Category = "Targets")
    FAttentionTarget GetCurrentFocus() const;

    // ========================================
    // ATTENTION CONTROL
    // ========================================

    /** Shift attention to target (voluntary) */
    UFUNCTION(BlueprintCallable, Category = "Control")
    bool ShiftAttentionTo(const FString& TargetID);

    /** Shift attention to location */
    UFUNCTION(BlueprintCallable, Category = "Control")
    bool ShiftAttentionToLocation(FVector Location);

    /** Set attention mode */
    UFUNCTION(BlueprintCallable, Category = "Control")
    void SetAttentionMode(EAttentionMode NewMode);

    /** Get current attention mode */
    UFUNCTION(BlueprintPure, Category = "Control")
    EAttentionMode GetAttentionMode() const;

    /** Set focus radius */
    UFUNCTION(BlueprintCallable, Category = "Control")
    void SetFocusRadius(float Radius);

    /** Release current focus */
    UFUNCTION(BlueprintCallable, Category = "Control")
    void ReleaseFocus();

    // ========================================
    // SALIENCE COMPUTATION
    // ========================================

    /** Compute combined salience for target */
    UFUNCTION(BlueprintPure, Category = "Salience")
    float ComputeCombinedSalience(const FAttentionTarget& Target) const;

    /** Get salience at location */
    UFUNCTION(BlueprintPure, Category = "Salience")
    float GetSalienceAtLocation(FVector Location) const;

    /** Get most salient target */
    UFUNCTION(BlueprintPure, Category = "Salience")
    FAttentionTarget GetMostSalientTarget() const;

    /** Add salience boost at location */
    UFUNCTION(BlueprintCallable, Category = "Salience")
    void AddSalienceBoost(FVector Location, float Boost, ESalienceSource Source);

    /** Set goal-driven salience for tags */
    UFUNCTION(BlueprintCallable, Category = "Salience")
    void SetGoalSalience(const TArray<FString>& Tags, float Salience);

    // ========================================
    // STATE QUERIES
    // ========================================

    /** Get attention state */
    UFUNCTION(BlueprintPure, Category = "State")
    FAttentionState GetAttentionState() const;

    /** Get spotlight state */
    UFUNCTION(BlueprintPure, Category = "State")
    FAttentionSpotlight GetSpotlight() const;

    /** Get fatigue level */
    UFUNCTION(BlueprintPure, Category = "State")
    float GetFatigueLevel() const;

    /** Get alertness level */
    UFUNCTION(BlueprintPure, Category = "State")
    float GetAlertnessLevel() const;

    /** Get attentional capacity */
    UFUNCTION(BlueprintPure, Category = "State")
    float GetAttentionalCapacity() const;

    /** Is currently focused */
    UFUNCTION(BlueprintPure, Category = "State")
    bool IsFocused() const;

    /** Get recent attention shifts */
    UFUNCTION(BlueprintPure, Category = "State")
    TArray<FAttentionShift> GetRecentShifts(int32 Count) const;

protected:
    // Component references
    UPROPERTY()
    UCognitiveCycleManager* CycleManager;

    UPROPERTY()
    URelevanceRealizationEnnead* EnneadComponent;

    UPROPERTY()
    UMemorySystems* MemoryComponent;

    // Internal state
    FAttentionSpotlight Spotlight;
    TArray<FAttentionTarget> Targets;
    TArray<FSalienceMapEntry> SalienceMap;
    TArray<FAttentionShift> ShiftHistory;
    TMap<FString, float> GoalSalienceMap;

    float LastShiftTime = 0.0f;
    float Alertness = 1.0f;
    float AttentionalCapacity = 1.0f;
    int32 TargetIDCounter = 0;

    // Internal methods
    void FindComponentReferences();
    void InitializeAttention();

    void UpdateSalienceScores(float DeltaTime);
    void UpdateSpotlight(float DeltaTime);
    void UpdateFatigue(float DeltaTime);
    void CheckForInvoluntaryShifts();

    float ComputeBottomUpSalience(const FAttentionTarget& Target) const;
    float ComputeTopDownSalience(const FAttentionTarget& Target) const;
    float ComputeEmotionalSalience(const FAttentionTarget& Target) const;
    float ComputeNoveltySalience(const FAttentionTarget& Target) const;

    void PerformAttentionShift(const FString& ToTargetID, ESalienceSource TriggerSource, bool bVoluntary);
    void RecordShift(const FAttentionShift& Shift);

    FString GenerateTargetID();
    int32 FindTargetIndex(const FString& TargetID) const;
};

// UnrealGamingMasteryIntegration.h
// Deep Tree Echo - Unreal Engine Gaming Mastery Integration
// Full integration of all Unreal Engine functions for 4E embodied cognition gaming
// Copyright (c) 2025 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "GamingMasterySystem.h"
#include "StrategicCognitionBridge.h"
#include "EchobeatsGamingIntegration.h"
#include "UnrealGamingMasteryIntegration.generated.h"

// Forward declarations for Unreal Engine systems
class UEnhancedInputComponent;
class UInputAction;
class UInputMappingContext;
class UCameraComponent;
class UCharacterMovementComponent;
class UAIPerceptionComponent;
class UBehaviorTree;
class UBlackboardComponent;
class UAnimInstance;
class UNiagaraComponent;
class USoundBase;
class UMaterialInstanceDynamic;
class UWidgetComponent;

/**
 * Input Processing Mode
 * How input is processed for gaming mastery
 */
UENUM(BlueprintType)
enum class EInputProcessingMode : uint8
{
    /** Direct input passthrough */
    Direct UMETA(DisplayName = "Direct"),
    
    /** ML-assisted input optimization */
    MLAssisted UMETA(DisplayName = "ML Assisted"),
    
    /** Fully autonomous AI control */
    Autonomous UMETA(DisplayName = "Autonomous"),
    
    /** Training mode with guidance */
    Training UMETA(DisplayName = "Training"),
    
    /** Replay analysis mode */
    ReplayAnalysis UMETA(DisplayName = "Replay Analysis")
};

/**
 * Visual Feedback Type
 * Types of visual feedback for gaming mastery
 */
UENUM(BlueprintType)
enum class EVisualFeedbackType : uint8
{
    /** Highlight optimal targets */
    TargetHighlight UMETA(DisplayName = "Target Highlight"),
    
    /** Show movement paths */
    MovementPath UMETA(DisplayName = "Movement Path"),
    
    /** Display timing windows */
    TimingWindow UMETA(DisplayName = "Timing Window"),
    
    /** Show threat indicators */
    ThreatIndicator UMETA(DisplayName = "Threat Indicator"),
    
    /** Display opportunity markers */
    OpportunityMarker UMETA(DisplayName = "Opportunity Marker"),
    
    /** Show skill execution feedback */
    SkillFeedback UMETA(DisplayName = "Skill Feedback")
};

/**
 * Perception Data
 * Data from Unreal's perception system
 */
USTRUCT(BlueprintType)
struct FPerceptionData
{
    GENERATED_BODY()

    /** Perceived actors */
    UPROPERTY(BlueprintReadWrite, Category = "Perception")
    TArray<AActor*> PerceivedActors;

    /** Sight stimuli */
    UPROPERTY(BlueprintReadWrite, Category = "Perception")
    TArray<FVector> SightLocations;

    /** Hearing stimuli */
    UPROPERTY(BlueprintReadWrite, Category = "Perception")
    TArray<FVector> HearingLocations;

    /** Damage stimuli */
    UPROPERTY(BlueprintReadWrite, Category = "Perception")
    TArray<FVector> DamageLocations;

    /** Perception confidence per actor */
    UPROPERTY(BlueprintReadWrite, Category = "Perception")
    TMap<AActor*, float> PerceptionConfidence;

    /** Last update time */
    UPROPERTY(BlueprintReadWrite, Category = "Perception")
    float LastUpdateTime = 0.0f;

    FPerceptionData()
        : LastUpdateTime(0.0f)
    {}
};

/**
 * Movement Analysis
 * Analysis of movement for skill development
 */
USTRUCT(BlueprintType)
struct FMovementAnalysis
{
    GENERATED_BODY()

    /** Movement efficiency (0.0 - 1.0) */
    UPROPERTY(BlueprintReadWrite, Category = "Movement")
    float Efficiency = 0.0f;

    /** Path optimality (0.0 - 1.0) */
    UPROPERTY(BlueprintReadWrite, Category = "Movement")
    float PathOptimality = 0.0f;

    /** Timing accuracy (0.0 - 1.0) */
    UPROPERTY(BlueprintReadWrite, Category = "Movement")
    float TimingAccuracy = 0.0f;

    /** Smoothness (0.0 - 1.0) */
    UPROPERTY(BlueprintReadWrite, Category = "Movement")
    float Smoothness = 0.0f;

    /** Predicted vs actual deviation */
    UPROPERTY(BlueprintReadWrite, Category = "Movement")
    float PredictionDeviation = 0.0f;

    /** Movement type classification */
    UPROPERTY(BlueprintReadWrite, Category = "Movement")
    FString MovementType;

    FMovementAnalysis()
        : Efficiency(0.0f)
        , PathOptimality(0.0f)
        , TimingAccuracy(0.0f)
        , Smoothness(0.0f)
        , PredictionDeviation(0.0f)
    {}
};

/**
 * Combat Analysis
 * Analysis of combat performance
 */
USTRUCT(BlueprintType)
struct FCombatAnalysis
{
    GENERATED_BODY()

    /** Accuracy (hits / attempts) */
    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    float Accuracy = 0.0f;

    /** Damage efficiency (damage dealt / damage taken) */
    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    float DamageEfficiency = 0.0f;

    /** Reaction time (seconds) */
    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    float ReactionTime = 0.0f;

    /** Combo execution rate */
    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    float ComboExecutionRate = 0.0f;

    /** Positioning score */
    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    float PositioningScore = 0.0f;

    /** Target prioritization score */
    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    float TargetPrioritization = 0.0f;

    /** Cooldown management */
    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    float CooldownManagement = 0.0f;

    FCombatAnalysis()
        : Accuracy(0.0f)
        , DamageEfficiency(0.0f)
        , ReactionTime(0.0f)
        , ComboExecutionRate(0.0f)
        , PositioningScore(0.0f)
        , TargetPrioritization(0.0f)
        , CooldownManagement(0.0f)
    {}
};

/**
 * Visual Feedback Config
 * Configuration for visual feedback system
 */
USTRUCT(BlueprintType)
struct FVisualFeedbackConfig
{
    GENERATED_BODY()

    /** Enable visual feedback */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
    bool bEnabled = true;

    /** Feedback opacity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
    float Opacity = 0.7f;

    /** Highlight color for positive feedback */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
    FLinearColor PositiveColor = FLinearColor::Green;

    /** Highlight color for negative feedback */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
    FLinearColor NegativeColor = FLinearColor::Red;

    /** Highlight color for neutral feedback */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
    FLinearColor NeutralColor = FLinearColor::Yellow;

    /** Feedback duration (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")
    float FeedbackDuration = 1.0f;

    FVisualFeedbackConfig()
        : bEnabled(true)
        , Opacity(0.7f)
        , PositiveColor(FLinearColor::Green)
        , NegativeColor(FLinearColor::Red)
        , NeutralColor(FLinearColor::Yellow)
        , FeedbackDuration(1.0f)
    {}
};

/**
 * UUnrealGamingMasteryIntegration
 * 
 * Full integration of Deep Tree Echo gaming mastery with Unreal Engine systems.
 * 
 * Integrates with:
 * 
 * **Input System:**
 * - Enhanced Input System integration
 * - Input timing analysis
 * - Input prediction and optimization
 * - Gesture recognition
 * 
 * **Movement System:**
 * - Character Movement Component integration
 * - Path optimization
 * - Movement prediction
 * - Dodge/evade timing
 * 
 * **Combat System:**
 * - Damage system integration
 * - Target prioritization
 * - Combo system analysis
 * - Cooldown optimization
 * 
 * **AI System:**
 * - Behavior Tree integration
 * - Blackboard data exchange
 * - AI perception integration
 * - Opponent modeling
 * 
 * **Animation System:**
 * - Animation state analysis
 * - Animation timing
 * - Cancel window detection
 * - Animation blending optimization
 * 
 * **Visual Feedback:**
 * - HUD integration
 * - World-space indicators
 * - Particle effects for feedback
 * - Post-process effects
 * 
 * **Audio System:**
 * - Audio cue integration
 * - Spatial audio analysis
 * - Rhythm game support
 * - Audio feedback
 * 
 * **Replay System:**
 * - Replay recording
 * - Replay analysis
 * - Key moment extraction
 * - Performance comparison
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UUnrealGamingMasteryIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    UUnrealGamingMasteryIntegration();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CORE REFERENCES
    // ========================================

    /** Gaming Mastery System reference */
    UPROPERTY(BlueprintReadWrite, Category = "Integration|Core")
    UGamingMasterySystem* GamingMasterySystem;

    /** Strategic Cognition Bridge reference */
    UPROPERTY(BlueprintReadWrite, Category = "Integration|Core")
    UStrategicCognitionBridge* CognitionBridge;

    /** Echobeats Gaming Integration reference */
    UPROPERTY(BlueprintReadWrite, Category = "Integration|Core")
    UEchobeatsGamingIntegration* EchobeatsIntegration;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Input processing mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration|Config")
    EInputProcessingMode InputMode = EInputProcessingMode::MLAssisted;

    /** Visual feedback configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration|Config")
    FVisualFeedbackConfig VisualFeedbackConfig;

    /** Enable input optimization */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration|Config")
    bool bEnableInputOptimization = true;

    /** Enable movement analysis */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration|Config")
    bool bEnableMovementAnalysis = true;

    /** Enable combat analysis */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration|Config")
    bool bEnableCombatAnalysis = true;

    /** Enable AI integration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration|Config")
    bool bEnableAIIntegration = true;

    /** Enable replay recording */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration|Config")
    bool bEnableReplayRecording = false;

    // ========================================
    // CURRENT STATE
    // ========================================

    /** Current perception data */
    UPROPERTY(BlueprintReadOnly, Category = "Integration|State")
    FPerceptionData CurrentPerception;

    /** Current movement analysis */
    UPROPERTY(BlueprintReadOnly, Category = "Integration|State")
    FMovementAnalysis MovementAnalysis;

    /** Current combat analysis */
    UPROPERTY(BlueprintReadOnly, Category = "Integration|State")
    FCombatAnalysis CombatAnalysis;

    // ========================================
    // INPUT SYSTEM INTEGRATION
    // ========================================

    /** Process input action */
    UFUNCTION(BlueprintCallable, Category = "Integration|Input")
    void ProcessInputAction(const FString& ActionName, float InputValue, float Timestamp);

    /** Get optimized input timing */
    UFUNCTION(BlueprintCallable, Category = "Integration|Input")
    float GetOptimizedInputTiming(const FString& ActionName) const;

    /** Predict next input */
    UFUNCTION(BlueprintCallable, Category = "Integration|Input")
    FString PredictNextInput(const TArray<FString>& RecentInputs) const;

    /** Analyze input sequence */
    UFUNCTION(BlueprintCallable, Category = "Integration|Input")
    TMap<FString, float> AnalyzeInputSequence(const TArray<FString>& InputSequence) const;

    /** Get input improvement suggestions */
    UFUNCTION(BlueprintCallable, Category = "Integration|Input")
    TArray<FString> GetInputImprovementSuggestions() const;

    // ========================================
    // MOVEMENT SYSTEM INTEGRATION
    // ========================================

    /** Analyze movement frame */
    UFUNCTION(BlueprintCallable, Category = "Integration|Movement")
    FMovementAnalysis AnalyzeMovementFrame(FVector CurrentPosition, FVector CurrentVelocity, FVector TargetPosition);

    /** Get optimal movement path */
    UFUNCTION(BlueprintCallable, Category = "Integration|Movement")
    TArray<FVector> GetOptimalMovementPath(FVector Start, FVector End, const TArray<AActor*>& Obstacles);

    /** Predict movement outcome */
    UFUNCTION(BlueprintCallable, Category = "Integration|Movement")
    FVector PredictMovementOutcome(FVector CurrentPosition, FVector InputDirection, float DeltaTime);

    /** Get dodge recommendation */
    UFUNCTION(BlueprintCallable, Category = "Integration|Movement")
    FVector GetDodgeRecommendation(FVector ThreatDirection, float ThreatSpeed);

    /** Analyze positioning */
    UFUNCTION(BlueprintCallable, Category = "Integration|Movement")
    float AnalyzePositioning(FVector CurrentPosition, const TArray<FVector>& EnemyPositions, const TArray<FVector>& CoverPositions);

    // ========================================
    // COMBAT SYSTEM INTEGRATION
    // ========================================

    /** Process combat event */
    UFUNCTION(BlueprintCallable, Category = "Integration|Combat")
    void ProcessCombatEvent(const FString& EventType, AActor* Source, AActor* Target, float Damage);

    /** Get target priority */
    UFUNCTION(BlueprintCallable, Category = "Integration|Combat")
    AActor* GetHighestPriorityTarget(const TArray<AActor*>& PotentialTargets);

    /** Analyze combat performance */
    UFUNCTION(BlueprintCallable, Category = "Integration|Combat")
    FCombatAnalysis AnalyzeCombatPerformance() const;

    /** Get optimal attack timing */
    UFUNCTION(BlueprintCallable, Category = "Integration|Combat")
    float GetOptimalAttackTiming(AActor* Target) const;

    /** Get combo recommendation */
    UFUNCTION(BlueprintCallable, Category = "Integration|Combat")
    TArray<FString> GetComboRecommendation(const FString& CurrentState) const;

    /** Predict enemy attack */
    UFUNCTION(BlueprintCallable, Category = "Integration|Combat")
    FString PredictEnemyAttack(AActor* Enemy) const;

    // ========================================
    // AI SYSTEM INTEGRATION
    // ========================================

    /** Update AI perception data */
    UFUNCTION(BlueprintCallable, Category = "Integration|AI")
    void UpdateAIPerception(UAIPerceptionComponent* PerceptionComponent);

    /** Get AI decision recommendation */
    UFUNCTION(BlueprintCallable, Category = "Integration|AI")
    FString GetAIDecisionRecommendation(UBlackboardComponent* Blackboard);

    /** Update blackboard with mastery data */
    UFUNCTION(BlueprintCallable, Category = "Integration|AI")
    void UpdateBlackboardWithMasteryData(UBlackboardComponent* Blackboard);

    /** Model opponent behavior */
    UFUNCTION(BlueprintCallable, Category = "Integration|AI")
    void ModelOpponentBehavior(AActor* Opponent, const FString& ObservedAction);

    /** Get counter-strategy for opponent */
    UFUNCTION(BlueprintCallable, Category = "Integration|AI")
    FString GetCounterStrategyForOpponent(AActor* Opponent) const;

    // ========================================
    // ANIMATION SYSTEM INTEGRATION
    // ========================================

    /** Analyze animation state */
    UFUNCTION(BlueprintCallable, Category = "Integration|Animation")
    void AnalyzeAnimationState(UAnimInstance* AnimInstance);

    /** Get cancel window */
    UFUNCTION(BlueprintCallable, Category = "Integration|Animation")
    bool GetCancelWindow(UAnimInstance* AnimInstance, FString& OutCancelType, float& OutWindowStart, float& OutWindowEnd);

    /** Predict animation completion */
    UFUNCTION(BlueprintCallable, Category = "Integration|Animation")
    float PredictAnimationCompletion(UAnimInstance* AnimInstance) const;

    /** Get optimal animation transition */
    UFUNCTION(BlueprintCallable, Category = "Integration|Animation")
    FString GetOptimalAnimationTransition(UAnimInstance* AnimInstance, const FString& TargetAction) const;

    // ========================================
    // VISUAL FEEDBACK SYSTEM
    // ========================================

    /** Show visual feedback */
    UFUNCTION(BlueprintCallable, Category = "Integration|Feedback")
    void ShowVisualFeedback(EVisualFeedbackType FeedbackType, FVector Location, float Duration = -1.0f);

    /** Hide visual feedback */
    UFUNCTION(BlueprintCallable, Category = "Integration|Feedback")
    void HideVisualFeedback(EVisualFeedbackType FeedbackType);

    /** Update HUD with mastery data */
    UFUNCTION(BlueprintCallable, Category = "Integration|Feedback")
    void UpdateHUDWithMasteryData();

    /** Show skill execution feedback */
    UFUNCTION(BlueprintCallable, Category = "Integration|Feedback")
    void ShowSkillExecutionFeedback(const FString& SkillName, float ExecutionQuality);

    /** Highlight optimal target */
    UFUNCTION(BlueprintCallable, Category = "Integration|Feedback")
    void HighlightOptimalTarget(AActor* Target);

    /** Show timing window */
    UFUNCTION(BlueprintCallable, Category = "Integration|Feedback")
    void ShowTimingWindow(float WindowStart, float WindowEnd, float CurrentTime);

    // ========================================
    // AUDIO SYSTEM INTEGRATION
    // ========================================

    /** Process audio cue */
    UFUNCTION(BlueprintCallable, Category = "Integration|Audio")
    void ProcessAudioCue(USoundBase* Sound, FVector Location, float Volume);

    /** Get audio-based threat assessment */
    UFUNCTION(BlueprintCallable, Category = "Integration|Audio")
    float GetAudioThreatAssessment() const;

    /** Play feedback sound */
    UFUNCTION(BlueprintCallable, Category = "Integration|Audio")
    void PlayFeedbackSound(const FString& FeedbackType, float Quality);

    // ========================================
    // REPLAY SYSTEM INTEGRATION
    // ========================================

    /** Start replay recording */
    UFUNCTION(BlueprintCallable, Category = "Integration|Replay")
    void StartReplayRecording(const FString& SessionName);

    /** Stop replay recording */
    UFUNCTION(BlueprintCallable, Category = "Integration|Replay")
    void StopReplayRecording();

    /** Analyze replay */
    UFUNCTION(BlueprintCallable, Category = "Integration|Replay")
    FReplayAnalysisResult AnalyzeReplaySession(const FString& SessionName);

    /** Get key moments from replay */
    UFUNCTION(BlueprintCallable, Category = "Integration|Replay")
    TArray<float> GetKeyMomentsFromReplay(const FString& SessionName) const;

    /** Compare performance to baseline */
    UFUNCTION(BlueprintCallable, Category = "Integration|Replay")
    TMap<FString, float> ComparePerformanceToBaseline(const FString& SessionName, const FString& BaselineName) const;

    // ========================================
    // GAME STATE EXTRACTION
    // ========================================

    /** Extract game state features */
    UFUNCTION(BlueprintCallable, Category = "Integration|State")
    TArray<float> ExtractGameStateFeatures();

    /** Get normalized game state */
    UFUNCTION(BlueprintCallable, Category = "Integration|State")
    TArray<float> GetNormalizedGameState() const;

    /** Update game state for ML */
    UFUNCTION(BlueprintCallable, Category = "Integration|State")
    void UpdateGameStateForML();

    // ========================================
    // TRAINING MODE
    // ========================================

    /** Enter training mode */
    UFUNCTION(BlueprintCallable, Category = "Integration|Training")
    void EnterTrainingMode(const FString& SkillToTrain);

    /** Exit training mode */
    UFUNCTION(BlueprintCallable, Category = "Integration|Training")
    void ExitTrainingMode();

    /** Get training guidance */
    UFUNCTION(BlueprintCallable, Category = "Integration|Training")
    FString GetTrainingGuidance() const;

    /** Set training difficulty */
    UFUNCTION(BlueprintCallable, Category = "Integration|Training")
    void SetTrainingDifficulty(float Difficulty);

    /** Get training progress */
    UFUNCTION(BlueprintCallable, Category = "Integration|Training")
    float GetTrainingProgress() const;

    // ========================================
    // EVENTS
    // ========================================

    /** Called when skill execution is analyzed */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSkillExecutionAnalyzed, const FString&, SkillName, float, Quality, const FString&, Feedback);
    UPROPERTY(BlueprintAssignable, Category = "Integration|Events")
    FOnSkillExecutionAnalyzed OnSkillExecutionAnalyzed;

    /** Called when optimal action is identified */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOptimalActionIdentified, const FString&, ActionName, float, Confidence);
    UPROPERTY(BlueprintAssignable, Category = "Integration|Events")
    FOnOptimalActionIdentified OnOptimalActionIdentified;

    /** Called when threat is detected */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnThreatDetected, AActor*, ThreatSource, float, ThreatLevel, FVector, ThreatDirection);
    UPROPERTY(BlueprintAssignable, Category = "Integration|Events")
    FOnThreatDetected OnThreatDetected;

    /** Called when opportunity is detected */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnOpportunityDetected, const FString&, OpportunityType, float, Value, float, TimeWindow);
    UPROPERTY(BlueprintAssignable, Category = "Integration|Events")
    FOnOpportunityDetected OnOpportunityDetected;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Input history for analysis */
    TArray<TPair<FString, float>> InputHistory;

    /** Position history for movement analysis */
    TArray<FVector> PositionHistory;

    /** Combat event history */
    TArray<TPair<FString, float>> CombatEventHistory;

    /** Current training skill */
    FString CurrentTrainingSkill;

    /** Training mode active */
    bool bTrainingModeActive = false;

    /** Current training difficulty */
    float TrainingDifficulty = 0.5f;

    /** Replay recording active */
    bool bReplayRecordingActive = false;

    /** Current replay session name */
    FString CurrentReplaySession;

    /** Frame counter for state updates */
    int32 FrameCounter = 0;

    /** Last game state features */
    TArray<float> LastGameStateFeatures;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Initialize component references */
    void InitializeReferences();

    /** Update perception data */
    void UpdatePerceptionData();

    /** Update movement analysis */
    void UpdateMovementAnalysis(float DeltaTime);

    /** Update combat analysis */
    void UpdateCombatAnalysis(float DeltaTime);

    /** Process ML recommendations */
    void ProcessMLRecommendations();

    /** Record frame for replay */
    void RecordFrameForReplay();

    /** Calculate target priority score */
    float CalculateTargetPriorityScore(AActor* Target) const;

    /** Calculate movement efficiency */
    float CalculateMovementEfficiency(const TArray<FVector>& Path) const;

    /** Analyze input timing */
    float AnalyzeInputTiming(const FString& ActionName, float Timestamp) const;
};

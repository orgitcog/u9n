// StrategicCognitionBridge.h
// Deep Tree Echo - Strategic Cognition Bridge for Gaming Mastery
// Connects GamingMasterySystem with 4E Embodied Cognition
// Copyright (c) 2025 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GamingMasterySystem.h"
#include "StrategicCognitionBridge.generated.h"

/**
 * Cognitive Load Type
 * Different types of cognitive demands in gaming
 */
UENUM(BlueprintType)
enum class ECognitiveLoadType : uint8
{
    /** Processing visual information */
    Perceptual UMETA(DisplayName = "Perceptual"),
    
    /** Working memory demands */
    WorkingMemory UMETA(DisplayName = "Working Memory"),
    
    /** Decision-making complexity */
    DecisionMaking UMETA(DisplayName = "Decision Making"),
    
    /** Motor execution precision */
    MotorExecution UMETA(DisplayName = "Motor Execution"),
    
    /** Temporal pressure */
    TimePressure UMETA(DisplayName = "Time Pressure"),
    
    /** Social/team coordination */
    SocialCoordination UMETA(DisplayName = "Social Coordination")
};

/**
 * Embodied Gaming State
 * Represents the embodied aspects of gaming performance
 */
USTRUCT(BlueprintType)
struct FEmbodiedGamingState
{
    GENERATED_BODY()

    /** Current motor readiness (reaction time potential) */
    UPROPERTY(BlueprintReadWrite, Category = "Embodied")
    float MotorReadiness = 0.5f;

    /** Hand-eye coordination quality */
    UPROPERTY(BlueprintReadWrite, Category = "Embodied")
    float HandEyeCoordination = 0.5f;

    /** Proprioceptive accuracy (controller feel) */
    UPROPERTY(BlueprintReadWrite, Category = "Embodied")
    float ProprioceptiveAccuracy = 0.5f;

    /** Muscle memory activation level */
    UPROPERTY(BlueprintReadWrite, Category = "Embodied")
    float MuscleMemoryActivation = 0.0f;

    /** Physical fatigue level */
    UPROPERTY(BlueprintReadWrite, Category = "Embodied")
    float PhysicalFatigue = 0.0f;

    /** Posture quality score */
    UPROPERTY(BlueprintReadWrite, Category = "Embodied")
    float PostureQuality = 0.8f;

    /** Breathing pattern regularity */
    UPROPERTY(BlueprintReadWrite, Category = "Embodied")
    float BreathingRegularity = 0.7f;

    FEmbodiedGamingState()
        : MotorReadiness(0.5f)
        , HandEyeCoordination(0.5f)
        , ProprioceptiveAccuracy(0.5f)
        , MuscleMemoryActivation(0.0f)
        , PhysicalFatigue(0.0f)
        , PostureQuality(0.8f)
        , BreathingRegularity(0.7f)
    {}
};

/**
 * Embedded Gaming Context
 * Environmental and contextual factors affecting performance
 */
USTRUCT(BlueprintType)
struct FEmbeddedGamingContext
{
    GENERATED_BODY()

    /** Current game phase (early, mid, late) */
    UPROPERTY(BlueprintReadWrite, Category = "Embedded")
    FString GamePhase;

    /** Map/level familiarity (0.0 - 1.0) */
    UPROPERTY(BlueprintReadWrite, Category = "Embedded")
    float MapFamiliarity = 0.0f;

    /** Meta-game awareness (current strategies in community) */
    UPROPERTY(BlueprintReadWrite, Category = "Embedded")
    float MetaAwareness = 0.5f;

    /** Team composition understanding */
    UPROPERTY(BlueprintReadWrite, Category = "Embedded")
    float TeamCompositionUnderstanding = 0.5f;

    /** Resource availability awareness */
    UPROPERTY(BlueprintReadWrite, Category = "Embedded")
    float ResourceAwareness = 0.5f;

    /** Threat assessment accuracy */
    UPROPERTY(BlueprintReadWrite, Category = "Embedded")
    float ThreatAssessmentAccuracy = 0.5f;

    /** Opportunity recognition speed */
    UPROPERTY(BlueprintReadWrite, Category = "Embedded")
    float OpportunityRecognitionSpeed = 0.5f;

    FEmbeddedGamingContext()
        : MapFamiliarity(0.0f)
        , MetaAwareness(0.5f)
        , TeamCompositionUnderstanding(0.5f)
        , ResourceAwareness(0.5f)
        , ThreatAssessmentAccuracy(0.5f)
        , OpportunityRecognitionSpeed(0.5f)
    {}
};

/**
 * Enacted Gaming Strategy
 * Active strategic engagement and world-making
 */
USTRUCT(BlueprintType)
struct FEnactedGamingStrategy
{
    GENERATED_BODY()

    /** Current strategic stance (aggressive, defensive, balanced) */
    UPROPERTY(BlueprintReadWrite, Category = "Enacted")
    FString StrategicStance;

    /** Active exploration tendency */
    UPROPERTY(BlueprintReadWrite, Category = "Enacted")
    float ExplorationTendency = 0.5f;

    /** Risk tolerance level */
    UPROPERTY(BlueprintReadWrite, Category = "Enacted")
    float RiskTolerance = 0.5f;

    /** Adaptation rate to opponent changes */
    UPROPERTY(BlueprintReadWrite, Category = "Enacted")
    float AdaptationRate = 0.5f;

    /** Initiative taking frequency */
    UPROPERTY(BlueprintReadWrite, Category = "Enacted")
    float InitiativeTaking = 0.5f;

    /** Counter-play effectiveness */
    UPROPERTY(BlueprintReadWrite, Category = "Enacted")
    float CounterPlayEffectiveness = 0.5f;

    /** Map control priority */
    UPROPERTY(BlueprintReadWrite, Category = "Enacted")
    float MapControlPriority = 0.5f;

    FEnactedGamingStrategy()
        : ExplorationTendency(0.5f)
        , RiskTolerance(0.5f)
        , AdaptationRate(0.5f)
        , InitiativeTaking(0.5f)
        , CounterPlayEffectiveness(0.5f)
        , MapControlPriority(0.5f)
    {}
};

/**
 * Extended Gaming Cognition
 * Cognitive extension through tools and social networks
 */
USTRUCT(BlueprintType)
struct FExtendedGamingCognition
{
    GENERATED_BODY()

    /** Controller/input device proficiency */
    UPROPERTY(BlueprintReadWrite, Category = "Extended")
    float InputDeviceProficiency = 0.5f;

    /** HUD/UI utilization efficiency */
    UPROPERTY(BlueprintReadWrite, Category = "Extended")
    float UIUtilizationEfficiency = 0.5f;

    /** External tool usage (overlays, trackers) */
    UPROPERTY(BlueprintReadWrite, Category = "Extended")
    float ExternalToolUsage = 0.0f;

    /** Team communication effectiveness */
    UPROPERTY(BlueprintReadWrite, Category = "Extended")
    float TeamCommunicationEffectiveness = 0.5f;

    /** Community knowledge integration */
    UPROPERTY(BlueprintReadWrite, Category = "Extended")
    float CommunityKnowledgeIntegration = 0.3f;

    /** Replay/VOD analysis utilization */
    UPROPERTY(BlueprintReadWrite, Category = "Extended")
    float ReplayAnalysisUtilization = 0.0f;

    /** Coaching/mentorship integration */
    UPROPERTY(BlueprintReadWrite, Category = "Extended")
    float CoachingIntegration = 0.0f;

    FExtendedGamingCognition()
        : InputDeviceProficiency(0.5f)
        , UIUtilizationEfficiency(0.5f)
        , ExternalToolUsage(0.0f)
        , TeamCommunicationEffectiveness(0.5f)
        , CommunityKnowledgeIntegration(0.3f)
        , ReplayAnalysisUtilization(0.0f)
        , CoachingIntegration(0.0f)
    {}
};

/**
 * Cognitive Load Profile
 * Current cognitive load distribution
 */
USTRUCT(BlueprintType)
struct FCognitiveLoadProfile
{
    GENERATED_BODY()

    /** Load levels by type */
    UPROPERTY(BlueprintReadWrite, Category = "Load")
    TMap<ECognitiveLoadType, float> LoadLevels;

    /** Total cognitive load (0.0 - 1.0) */
    UPROPERTY(BlueprintReadWrite, Category = "Load")
    float TotalLoad = 0.0f;

    /** Cognitive reserve remaining */
    UPROPERTY(BlueprintReadWrite, Category = "Load")
    float CognitiveReserve = 1.0f;

    /** Overload warning threshold */
    UPROPERTY(BlueprintReadWrite, Category = "Load")
    float OverloadThreshold = 0.8f;

    /** Time at current load level */
    UPROPERTY(BlueprintReadWrite, Category = "Load")
    float TimeAtCurrentLoad = 0.0f;

    FCognitiveLoadProfile()
        : TotalLoad(0.0f)
        , CognitiveReserve(1.0f)
        , OverloadThreshold(0.8f)
        , TimeAtCurrentLoad(0.0f)
    {}
};

/**
 * Attention Allocation
 * How attention is distributed across game elements
 */
USTRUCT(BlueprintType)
struct FAttentionAllocation
{
    GENERATED_BODY()

    /** Primary focus target */
    UPROPERTY(BlueprintReadWrite, Category = "Attention")
    FString PrimaryFocus;

    /** Secondary attention targets */
    UPROPERTY(BlueprintReadWrite, Category = "Attention")
    TArray<FString> SecondaryTargets;

    /** Attention split ratio (primary vs secondary) */
    UPROPERTY(BlueprintReadWrite, Category = "Attention")
    float AttentionSplitRatio = 0.7f;

    /** Peripheral awareness level */
    UPROPERTY(BlueprintReadWrite, Category = "Attention")
    float PeripheralAwareness = 0.5f;

    /** Attention switching cost */
    UPROPERTY(BlueprintReadWrite, Category = "Attention")
    float SwitchingCost = 0.2f;

    /** Sustained attention duration */
    UPROPERTY(BlueprintReadWrite, Category = "Attention")
    float SustainedAttentionDuration = 0.0f;

    FAttentionAllocation()
        : AttentionSplitRatio(0.7f)
        , PeripheralAwareness(0.5f)
        , SwitchingCost(0.2f)
        , SustainedAttentionDuration(0.0f)
    {}
};

/**
 * UStrategicCognitionBridge
 * 
 * Bridges GamingMasterySystem with 4E Embodied Cognition for holistic
 * gaming skill development and strategic mastery.
 * 
 * Key Functions:
 * 
 * **Embodied Integration:**
 * - Motor skill development tracking
 * - Muscle memory formation monitoring
 * - Physical state impact on performance
 * - Sensorimotor contingency learning for gaming
 * 
 * **Embedded Integration:**
 * - Context-aware strategy adaptation
 * - Environmental affordance recognition
 * - Situational awareness enhancement
 * - Meta-game knowledge integration
 * 
 * **Enacted Integration:**
 * - Active exploration and experimentation
 * - Strategy enactment and validation
 * - World-model construction through play
 * - Adaptive response generation
 * 
 * **Extended Integration:**
 * - Tool proficiency development
 * - Social cognition for team play
 * - External resource utilization
 * - Community knowledge integration
 * 
 * **Cognitive Load Management:**
 * - Multi-dimensional load tracking
 * - Overload prevention
 * - Attention allocation optimization
 * - Recovery and regeneration
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UStrategicCognitionBridge : public UActorComponent
{
    GENERATED_BODY()

public:
    UStrategicCognitionBridge();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // REFERENCES
    // ========================================

    /** Reference to Gaming Mastery System */
    UPROPERTY(BlueprintReadWrite, Category = "Bridge|References")
    UGamingMasterySystem* GamingMasterySystem;

    // ========================================
    // 4E STATES
    // ========================================

    /** Current embodied gaming state */
    UPROPERTY(BlueprintReadOnly, Category = "Bridge|4E")
    FEmbodiedGamingState EmbodiedState;

    /** Current embedded gaming context */
    UPROPERTY(BlueprintReadOnly, Category = "Bridge|4E")
    FEmbeddedGamingContext EmbeddedContext;

    /** Current enacted gaming strategy */
    UPROPERTY(BlueprintReadOnly, Category = "Bridge|4E")
    FEnactedGamingStrategy EnactedStrategy;

    /** Current extended gaming cognition */
    UPROPERTY(BlueprintReadOnly, Category = "Bridge|4E")
    FExtendedGamingCognition ExtendedCognition;

    // ========================================
    // COGNITIVE MANAGEMENT
    // ========================================

    /** Current cognitive load profile */
    UPROPERTY(BlueprintReadOnly, Category = "Bridge|Cognitive")
    FCognitiveLoadProfile CognitiveLoad;

    /** Current attention allocation */
    UPROPERTY(BlueprintReadOnly, Category = "Bridge|Cognitive")
    FAttentionAllocation AttentionState;

    // ========================================
    // EMBODIED COGNITION API
    // ========================================

    /** Update embodied state from sensor data */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Embodied")
    void UpdateEmbodiedState(float ReactionTime, float InputAccuracy, float MovementSmoothing);

    /** Record motor execution for muscle memory */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Embodied")
    void RecordMotorExecution(const FString& ActionType, float ExecutionQuality, float TimingAccuracy);

    /** Get motor readiness for action type */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Bridge|Embodied")
    float GetMotorReadiness(const FString& ActionType) const;

    /** Check if muscle memory is active for action */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Bridge|Embodied")
    bool IsMuscleMemoryActive(const FString& ActionType) const;

    /** Get embodied performance multiplier */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Bridge|Embodied")
    float GetEmbodiedPerformanceMultiplier() const;

    // ========================================
    // EMBEDDED COGNITION API
    // ========================================

    /** Update embedded context from game state */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Embedded")
    void UpdateEmbeddedContext(const FString& GamePhase, const TArray<float>& EnvironmentFeatures);

    /** Record map/level experience */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Embedded")
    void RecordMapExperience(const FString& MapID, float PerformanceScore);

    /** Get context-appropriate strategy recommendation */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Embedded")
    FString GetContextualStrategyRecommendation() const;

    /** Assess current threat level */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Embedded")
    float AssessThreatLevel(const TArray<float>& ThreatIndicators);

    /** Identify opportunity in current context */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Embedded")
    TArray<FString> IdentifyOpportunities(const TArray<float>& GameState);

    // ========================================
    // ENACTED COGNITION API
    // ========================================

    /** Update enacted strategy based on outcomes */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Enacted")
    void UpdateEnactedStrategy(const FString& ActionTaken, bool bSuccess, float Impact);

    /** Get exploration recommendation */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Enacted")
    FString GetExplorationRecommendation() const;

    /** Adjust risk tolerance based on game state */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Enacted")
    void AdjustRiskTolerance(float GameProgress, float CurrentAdvantage);

    /** Get adaptive response to opponent action */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Enacted")
    FString GetAdaptiveResponse(const FString& OpponentAction, const TArray<float>& GameState);

    /** Evaluate initiative opportunity */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Enacted")
    float EvaluateInitiativeOpportunity(const TArray<float>& GameState);

    // ========================================
    // EXTENDED COGNITION API
    // ========================================

    /** Update extended cognition state */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Extended")
    void UpdateExtendedCognition(float InputDevicePerformance, float UIUsage, float TeamCommQuality);

    /** Record tool usage effectiveness */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Extended")
    void RecordToolUsage(const FString& ToolType, float EffectivenessScore);

    /** Get team coordination recommendation */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Extended")
    FString GetTeamCoordinationRecommendation(const TArray<FString>& TeamRoles, const TArray<float>& TeamState);

    /** Integrate community knowledge */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Extended")
    void IntegrateCommunityKnowledge(const FString& KnowledgeType, const FString& Content);

    /** Get recommended external resources */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Extended")
    TArray<FString> GetRecommendedExternalResources() const;

    // ========================================
    // COGNITIVE LOAD MANAGEMENT
    // ========================================

    /** Update cognitive load from task demands */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Load")
    void UpdateCognitiveLoad(ECognitiveLoadType LoadType, float LoadLevel);

    /** Get current total cognitive load */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Bridge|Load")
    float GetTotalCognitiveLoad() const;

    /** Check if cognitive overload is imminent */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Bridge|Load")
    bool IsCognitiveOverloadImminent() const;

    /** Get load reduction recommendations */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Load")
    TArray<FString> GetLoadReductionRecommendations() const;

    /** Allocate cognitive resources */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Load")
    void AllocateCognitiveResources(const TMap<ECognitiveLoadType, float>& Allocation);

    // ========================================
    // ATTENTION MANAGEMENT
    // ========================================

    /** Set primary attention focus */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Attention")
    void SetPrimaryFocus(const FString& FocusTarget);

    /** Add secondary attention target */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Attention")
    void AddSecondaryTarget(const FString& Target);

    /** Get attention allocation recommendation */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Attention")
    TMap<FString, float> GetAttentionAllocationRecommendation(const TArray<FString>& PotentialTargets);

    /** Calculate attention switching cost */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Bridge|Attention")
    float CalculateAttentionSwitchingCost(const FString& CurrentFocus, const FString& NewFocus) const;

    // ========================================
    // HOLISTIC INTEGRATION
    // ========================================

    /** Get integrated performance assessment */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Integration")
    TMap<FString, float> GetIntegratedPerformanceAssessment() const;

    /** Get holistic improvement recommendations */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Integration")
    TArray<FString> GetHolisticImprovementRecommendations() const;

    /** Calculate 4E integration score */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Bridge|Integration")
    float Calculate4EIntegrationScore() const;

    /** Get personalized training focus */
    UFUNCTION(BlueprintCallable, Category = "Bridge|Integration")
    FString GetPersonalizedTrainingFocus() const;

    // ========================================
    // EVENTS
    // ========================================

    /** Called when cognitive overload is detected */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCognitiveOverload, float, OverloadLevel);
    UPROPERTY(BlueprintAssignable, Category = "Bridge|Events")
    FOnCognitiveOverload OnCognitiveOverload;

    /** Called when muscle memory is formed */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMuscleMemoryFormed, const FString&, ActionType, float, Strength);
    UPROPERTY(BlueprintAssignable, Category = "Bridge|Events")
    FOnMuscleMemoryFormed OnMuscleMemoryFormed;

    /** Called when strategic adaptation occurs */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStrategicAdaptation, const FString&, OldStrategy, const FString&, NewStrategy);
    UPROPERTY(BlueprintAssignable, Category = "Bridge|Events")
    FOnStrategicAdaptation OnStrategicAdaptation;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Motor execution history for muscle memory */
    TMap<FString, TArray<float>> MotorExecutionHistory;

    /** Map experience database */
    TMap<FString, float> MapExperienceDB;

    /** Strategy effectiveness history */
    TMap<FString, TArray<bool>> StrategyEffectivenessHistory;

    /** Tool usage effectiveness history */
    TMap<FString, TArray<float>> ToolUsageHistory;

    /** Cognitive load history for trend analysis */
    TArray<float> CognitiveLoadHistory;

    /** Previous overload state */
    bool bWasOverloaded = false;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Calculate muscle memory strength for action */
    float CalculateMuscleMemoryStrength(const FString& ActionType) const;

    /** Update cognitive reserve based on load */
    void UpdateCognitiveReserve(float DeltaTime);

    /** Calculate strategy effectiveness */
    float CalculateStrategyEffectiveness(const FString& Strategy) const;

    /** Determine optimal attention split */
    float DetermineOptimalAttentionSplit(int32 TargetCount) const;

    /** Integrate 4E factors for performance */
    float Integrate4EFactors() const;
};

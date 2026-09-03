// EchobeatsGamingIntegration.h
// Deep Tree Echo - Echobeats Gaming Integration for 3-Stream Cognitive Processing
// Implements 12-step cognitive loop with gaming-specific optimizations
// Copyright (c) 2025 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GamingMasterySystem.h"
#include "StrategicCognitionBridge.h"
#include "EchobeatsGamingIntegration.generated.h"

/**
 * Cognitive Stream Phase
 * The three concurrent streams in the 12-step cognitive loop
 */
UENUM(BlueprintType)
enum class ECognitiveStreamPhase : uint8
{
    /** Perception stream - sensing and pattern recognition */
    Perception UMETA(DisplayName = "Perception"),
    
    /** Action stream - decision and execution */
    Action UMETA(DisplayName = "Action"),
    
    /** Simulation stream - prediction and planning */
    Simulation UMETA(DisplayName = "Simulation")
};

/**
 * Gaming Cognitive Step
 * Individual step in the 12-step gaming cognitive loop
 */
UENUM(BlueprintType)
enum class EGamingCognitiveStep : uint8
{
    /** Step 1: Sensory input processing */
    Step1_SensoryInput UMETA(DisplayName = "1: Sensory Input"),
    
    /** Step 2: Feature extraction */
    Step2_FeatureExtraction UMETA(DisplayName = "2: Feature Extraction"),
    
    /** Step 3: Pattern matching */
    Step3_PatternMatching UMETA(DisplayName = "3: Pattern Matching"),
    
    /** Step 4: Situation assessment */
    Step4_SituationAssessment UMETA(DisplayName = "4: Situation Assessment"),
    
    /** Step 5: Goal evaluation */
    Step5_GoalEvaluation UMETA(DisplayName = "5: Goal Evaluation"),
    
    /** Step 6: Strategy selection */
    Step6_StrategySelection UMETA(DisplayName = "6: Strategy Selection"),
    
    /** Step 7: Action planning */
    Step7_ActionPlanning UMETA(DisplayName = "7: Action Planning"),
    
    /** Step 8: Motor preparation */
    Step8_MotorPreparation UMETA(DisplayName = "8: Motor Preparation"),
    
    /** Step 9: Action execution */
    Step9_ActionExecution UMETA(DisplayName = "9: Action Execution"),
    
    /** Step 10: Outcome prediction */
    Step10_OutcomePrediction UMETA(DisplayName = "10: Outcome Prediction"),
    
    /** Step 11: Feedback integration */
    Step11_FeedbackIntegration UMETA(DisplayName = "11: Feedback Integration"),
    
    /** Step 12: Learning update */
    Step12_LearningUpdate UMETA(DisplayName = "12: Learning Update")
};

/**
 * Stream State
 * Current state of a cognitive stream
 */
USTRUCT(BlueprintType)
struct FStreamState
{
    GENERATED_BODY()

    /** Stream identifier */
    UPROPERTY(BlueprintReadWrite, Category = "Stream")
    ECognitiveStreamPhase Phase;

    /** Current step in 12-step cycle */
    UPROPERTY(BlueprintReadWrite, Category = "Stream")
    EGamingCognitiveStep CurrentStep;

    /** Step index (1-12) */
    UPROPERTY(BlueprintReadWrite, Category = "Stream")
    int32 StepIndex = 1;

    /** Stream activation level */
    UPROPERTY(BlueprintReadWrite, Category = "Stream")
    float ActivationLevel = 1.0f;

    /** Current processing data */
    UPROPERTY(BlueprintReadWrite, Category = "Stream")
    TArray<float> ProcessingData;

    /** Stream output */
    UPROPERTY(BlueprintReadWrite, Category = "Stream")
    TArray<float> OutputData;

    /** Processing latency (ms) */
    UPROPERTY(BlueprintReadWrite, Category = "Stream")
    float ProcessingLatency = 0.0f;

    /** Error rate */
    UPROPERTY(BlueprintReadWrite, Category = "Stream")
    float ErrorRate = 0.0f;

    FStreamState()
        : Phase(ECognitiveStreamPhase::Perception)
        , CurrentStep(EGamingCognitiveStep::Step1_SensoryInput)
        , StepIndex(1)
        , ActivationLevel(1.0f)
        , ProcessingLatency(0.0f)
        , ErrorRate(0.0f)
    {}
};

/**
 * Triad State
 * State of a step triad (steps that occur together across streams)
 * Triads: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
 */
USTRUCT(BlueprintType)
struct FTriadState
{
    GENERATED_BODY()

    /** Triad index (0-3) */
    UPROPERTY(BlueprintReadWrite, Category = "Triad")
    int32 TriadIndex = 0;

    /** Steps in this triad */
    UPROPERTY(BlueprintReadWrite, Category = "Triad")
    TArray<int32> Steps;

    /** Triad synchronization quality */
    UPROPERTY(BlueprintReadWrite, Category = "Triad")
    float SynchronizationQuality = 1.0f;

    /** Cross-stream coherence */
    UPROPERTY(BlueprintReadWrite, Category = "Triad")
    float CrossStreamCoherence = 1.0f;

    /** Triad output */
    UPROPERTY(BlueprintReadWrite, Category = "Triad")
    TArray<float> IntegratedOutput;

    FTriadState()
        : TriadIndex(0)
        , SynchronizationQuality(1.0f)
        , CrossStreamCoherence(1.0f)
    {}
};

/**
 * Gaming Salience Map
 * Attention allocation across game elements
 */
USTRUCT(BlueprintType)
struct FGamingSalienceMap
{
    GENERATED_BODY()

    /** Salience values for game elements */
    UPROPERTY(BlueprintReadWrite, Category = "Salience")
    TMap<FString, float> ElementSalience;

    /** Current focus point */
    UPROPERTY(BlueprintReadWrite, Category = "Salience")
    FVector2D FocusPoint;

    /** Attention radius */
    UPROPERTY(BlueprintReadWrite, Category = "Salience")
    float AttentionRadius = 100.0f;

    /** Peripheral awareness level */
    UPROPERTY(BlueprintReadWrite, Category = "Salience")
    float PeripheralAwareness = 0.5f;

    /** Threat salience boost */
    UPROPERTY(BlueprintReadWrite, Category = "Salience")
    float ThreatBoost = 1.5f;

    /** Opportunity salience boost */
    UPROPERTY(BlueprintReadWrite, Category = "Salience")
    float OpportunityBoost = 1.3f;

    FGamingSalienceMap()
        : AttentionRadius(100.0f)
        , PeripheralAwareness(0.5f)
        , ThreatBoost(1.5f)
        , OpportunityBoost(1.3f)
    {}
};

/**
 * Affordance Detection
 * Detected action possibilities in the game environment
 */
USTRUCT(BlueprintType)
struct FAffordanceDetection
{
    GENERATED_BODY()

    /** Affordance identifier */
    UPROPERTY(BlueprintReadWrite, Category = "Affordance")
    FString AffordanceID;

    /** Affordance type (attack, defend, resource, etc.) */
    UPROPERTY(BlueprintReadWrite, Category = "Affordance")
    FString AffordanceType;

    /** Detection confidence */
    UPROPERTY(BlueprintReadWrite, Category = "Affordance")
    float Confidence = 0.0f;

    /** Estimated value */
    UPROPERTY(BlueprintReadWrite, Category = "Affordance")
    float EstimatedValue = 0.0f;

    /** Time window (seconds) */
    UPROPERTY(BlueprintReadWrite, Category = "Affordance")
    float TimeWindow = 0.0f;

    /** Required skill level */
    UPROPERTY(BlueprintReadWrite, Category = "Affordance")
    float RequiredSkillLevel = 0.0f;

    /** Risk level */
    UPROPERTY(BlueprintReadWrite, Category = "Affordance")
    float RiskLevel = 0.0f;

    FAffordanceDetection()
        : Confidence(0.0f)
        , EstimatedValue(0.0f)
        , TimeWindow(0.0f)
        , RequiredSkillLevel(0.0f)
        , RiskLevel(0.0f)
    {}
};

/**
 * Prediction State
 * State of the simulation stream's predictions
 */
USTRUCT(BlueprintType)
struct FPredictionState
{
    GENERATED_BODY()

    /** Predicted game state */
    UPROPERTY(BlueprintReadWrite, Category = "Prediction")
    TArray<float> PredictedState;

    /** Prediction horizon (seconds) */
    UPROPERTY(BlueprintReadWrite, Category = "Prediction")
    float PredictionHorizon = 1.0f;

    /** Prediction confidence */
    UPROPERTY(BlueprintReadWrite, Category = "Prediction")
    float Confidence = 0.5f;

    /** Predicted opponent actions */
    UPROPERTY(BlueprintReadWrite, Category = "Prediction")
    TArray<FString> PredictedOpponentActions;

    /** Predicted outcomes for each action */
    UPROPERTY(BlueprintReadWrite, Category = "Prediction")
    TMap<FString, float> ActionOutcomePredictions;

    /** Simulation depth (tree search) */
    UPROPERTY(BlueprintReadWrite, Category = "Prediction")
    int32 SimulationDepth = 3;

    FPredictionState()
        : PredictionHorizon(1.0f)
        , Confidence(0.5f)
        , SimulationDepth(3)
    {}
};

/**
 * UEchobeatsGamingIntegration
 * 
 * Implements the Echobeats 3-stream concurrent cognitive processing
 * optimized for gaming skill development and strategic mastery.
 * 
 * Architecture:
 * 
 * **Three Concurrent Streams (120° phase offset):**
 * 
 * 1. **Perception Stream** (Steps 1,2,3,4)
 *    - Sensory input processing
 *    - Feature extraction
 *    - Pattern matching
 *    - Situation assessment
 * 
 * 2. **Action Stream** (Steps 5,6,7,8,9)
 *    - Goal evaluation
 *    - Strategy selection
 *    - Action planning
 *    - Motor preparation
 *    - Action execution
 * 
 * 3. **Simulation Stream** (Steps 10,11,12)
 *    - Outcome prediction
 *    - Feedback integration
 *    - Learning update
 * 
 * **Step Triads (concurrent across streams):**
 * - Triad 0: {1, 5, 9}  - Input/Goal/Execute
 * - Triad 1: {2, 6, 10} - Extract/Select/Predict
 * - Triad 2: {3, 7, 11} - Match/Plan/Feedback
 * - Triad 3: {4, 8, 12} - Assess/Prepare/Learn
 * 
 * **Gaming Optimizations:**
 * - Real-time salience mapping for attention allocation
 * - Affordance detection for action possibilities
 * - Predictive simulation for strategic planning
 * - Cross-stream coherence for integrated decision-making
 * - Adaptive timing for different game speeds
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UEchobeatsGamingIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    UEchobeatsGamingIntegration();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // REFERENCES
    // ========================================

    /** Reference to Gaming Mastery System */
    UPROPERTY(BlueprintReadWrite, Category = "Echobeats|References")
    UGamingMasterySystem* GamingMasterySystem;

    /** Reference to Strategic Cognition Bridge */
    UPROPERTY(BlueprintReadWrite, Category = "Echobeats|References")
    UStrategicCognitionBridge* CognitionBridge;

    // ========================================
    // STREAM STATES
    // ========================================

    /** Perception stream state */
    UPROPERTY(BlueprintReadOnly, Category = "Echobeats|Streams")
    FStreamState PerceptionStream;

    /** Action stream state */
    UPROPERTY(BlueprintReadOnly, Category = "Echobeats|Streams")
    FStreamState ActionStream;

    /** Simulation stream state */
    UPROPERTY(BlueprintReadOnly, Category = "Echobeats|Streams")
    FStreamState SimulationStream;

    /** Current triad states */
    UPROPERTY(BlueprintReadOnly, Category = "Echobeats|Streams")
    TArray<FTriadState> TriadStates;

    // ========================================
    // GAMING STATE
    // ========================================

    /** Current salience map */
    UPROPERTY(BlueprintReadOnly, Category = "Echobeats|Gaming")
    FGamingSalienceMap SalienceMap;

    /** Detected affordances */
    UPROPERTY(BlueprintReadOnly, Category = "Echobeats|Gaming")
    TArray<FAffordanceDetection> DetectedAffordances;

    /** Current prediction state */
    UPROPERTY(BlueprintReadOnly, Category = "Echobeats|Gaming")
    FPredictionState PredictionState;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Cognitive loop frequency (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats|Config")
    float LoopFrequency = 60.0f;

    /** Enable adaptive timing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats|Config")
    bool bAdaptiveTiming = true;

    /** Enable cross-stream coherence */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats|Config")
    bool bCrossStreamCoherence = true;

    /** Prediction horizon (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats|Config")
    float PredictionHorizon = 2.0f;

    /** Salience decay rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats|Config")
    float SalienceDecayRate = 0.1f;

    // ========================================
    // STREAM CONTROL
    // ========================================

    /** Advance all streams by one step */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Control")
    void AdvanceStreams();

    /** Process current triad */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Control")
    void ProcessCurrentTriad();

    /** Get current global step (1-12) */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Echobeats|Control")
    int32 GetCurrentGlobalStep() const;

    /** Get current triad index (0-3) */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Echobeats|Control")
    int32 GetCurrentTriadIndex() const;

    /** Synchronize streams */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Control")
    void SynchronizeStreams();

    // ========================================
    // PERCEPTION STREAM API
    // ========================================

    /** Process sensory input */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Perception")
    void ProcessSensoryInput(const TArray<float>& SensoryData);

    /** Extract features from input */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Perception")
    TArray<float> ExtractFeatures(const TArray<float>& RawInput);

    /** Match patterns in features */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Perception")
    TArray<FString> MatchPatterns(const TArray<float>& Features);

    /** Assess current situation */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Perception")
    TMap<FString, float> AssessSituation(const TArray<FString>& MatchedPatterns);

    /** Update salience map */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Perception")
    void UpdateSalienceMap(const TArray<float>& GameState, const TArray<FString>& GameElements);

    // ========================================
    // ACTION STREAM API
    // ========================================

    /** Evaluate goals based on situation */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Action")
    TMap<FString, float> EvaluateGoals(const TMap<FString, float>& SituationAssessment);

    /** Select strategy for goals */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Action")
    FString SelectStrategy(const TMap<FString, float>& GoalEvaluation);

    /** Plan actions for strategy */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Action")
    TArray<FString> PlanActions(const FString& SelectedStrategy);

    /** Prepare motor execution */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Action")
    void PrepareMotorExecution(const TArray<FString>& PlannedActions);

    /** Execute planned action */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Action")
    bool ExecuteAction(const FString& ActionID);

    /** Detect affordances in current state */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Action")
    TArray<FAffordanceDetection> DetectAffordances(const TArray<float>& GameState);

    // ========================================
    // SIMULATION STREAM API
    // ========================================

    /** Predict outcome of action */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Simulation")
    TArray<float> PredictOutcome(const FString& ActionID, const TArray<float>& CurrentState);

    /** Integrate feedback from execution */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Simulation")
    void IntegrateFeedback(const TArray<float>& ExpectedOutcome, const TArray<float>& ActualOutcome);

    /** Update learning from feedback */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Simulation")
    void UpdateLearning(float RewardSignal, const TArray<float>& StateFeatures);

    /** Run mental simulation */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Simulation")
    FPredictionState RunMentalSimulation(const TArray<float>& CurrentState, int32 Depth);

    /** Predict opponent behavior */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Simulation")
    TMap<FString, float> PredictOpponentBehavior(const FString& OpponentID, const TArray<float>& GameState);

    // ========================================
    // INTEGRATION API
    // ========================================

    /** Get integrated decision from all streams */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Integration")
    FString GetIntegratedDecision();

    /** Calculate cross-stream coherence */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Echobeats|Integration")
    float CalculateCrossStreamCoherence() const;

    /** Get stream synchronization quality */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Echobeats|Integration")
    float GetStreamSynchronizationQuality() const;

    /** Balance stream activation */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Integration")
    void BalanceStreamActivation();

    /** Get processing bottleneck */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Integration")
    ECognitiveStreamPhase GetProcessingBottleneck() const;

    // ========================================
    // GAMING-SPECIFIC API
    // ========================================

    /** Process game frame */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Gaming")
    void ProcessGameFrame(const TArray<float>& FrameData, float DeltaTime);

    /** Get recommended action */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Gaming")
    FString GetRecommendedAction(const TArray<float>& GameState);

    /** Get attention focus recommendation */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Gaming")
    FString GetAttentionFocusRecommendation() const;

    /** Get timing recommendation for action */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Gaming")
    float GetActionTimingRecommendation(const FString& ActionID) const;

    /** Evaluate action risk/reward */
    UFUNCTION(BlueprintCallable, Category = "Echobeats|Gaming")
    TMap<FString, float> EvaluateActionRiskReward(const FString& ActionID, const TArray<float>& GameState);

    // ========================================
    // EVENTS
    // ========================================

    /** Called when triad completes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTriadComplete, int32, TriadIndex, const TArray<float>&, IntegratedOutput);
    UPROPERTY(BlueprintAssignable, Category = "Echobeats|Events")
    FOnTriadComplete OnTriadComplete;

    /** Called when pattern is recognized */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPatternRecognized, const FString&, PatternID, float, Confidence);
    UPROPERTY(BlueprintAssignable, Category = "Echobeats|Events")
    FOnPatternRecognized OnPatternRecognized;

    /** Called when affordance is detected */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAffordanceDetected, const FAffordanceDetection&, Affordance);
    UPROPERTY(BlueprintAssignable, Category = "Echobeats|Events")
    FOnAffordanceDetected OnAffordanceDetected;

    /** Called when prediction error exceeds threshold */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPredictionError, float, ErrorMagnitude, const FString&, Context);
    UPROPERTY(BlueprintAssignable, Category = "Echobeats|Events")
    FOnPredictionError OnPredictionError;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Global step counter */
    int32 GlobalStepCounter = 0;

    /** Time accumulator for loop timing */
    float TimeAccumulator = 0.0f;

    /** Step duration (seconds) */
    float StepDuration = 0.0f;

    /** Last processed game state */
    TArray<float> LastGameState;

    /** Action queue */
    TArray<FString> ActionQueue;

    /** Prediction error history */
    TArray<float> PredictionErrorHistory;

    /** Feature extraction weights */
    TArray<TArray<float>> FeatureWeights;

    /** Pattern templates */
    TMap<FString, TArray<float>> PatternTemplates;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Initialize stream states */
    void InitializeStreams();

    /** Initialize triad states */
    void InitializeTriads();

    /** Process perception stream step */
    void ProcessPerceptionStep(EGamingCognitiveStep Step);

    /** Process action stream step */
    void ProcessActionStep(EGamingCognitiveStep Step);

    /** Process simulation stream step */
    void ProcessSimulationStep(EGamingCognitiveStep Step);

    /** Calculate step for stream at global step */
    EGamingCognitiveStep CalculateStreamStep(ECognitiveStreamPhase Phase, int32 GlobalStep) const;

    /** Update triad state */
    void UpdateTriadState(int32 TriadIndex);

    /** Apply salience decay */
    void ApplySalienceDecay(float DeltaTime);

    /** Calculate feature similarity */
    float CalculateFeatureSimilarity(const TArray<float>& Features1, const TArray<float>& Features2) const;

    /** Generate action from affordance */
    FString GenerateActionFromAffordance(const FAffordanceDetection& Affordance) const;

    /** Update prediction model */
    void UpdatePredictionModel(const TArray<float>& State, const TArray<float>& NextState, const FString& Action);
};

// EchobeatsGamingIntegration.cpp
// Deep Tree Echo - Echobeats Gaming Integration for 3-Stream Cognitive Processing
// Copyright (c) 2025 Deep Tree Echo Project

#include "EchobeatsGamingIntegration.h"

UEchobeatsGamingIntegration::UEchobeatsGamingIntegration()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f; // Tick every frame for responsive gaming
}

void UEchobeatsGamingIntegration::BeginPlay()
{
    Super::BeginPlay();

    // Calculate step duration from loop frequency
    StepDuration = 1.0f / (LoopFrequency * 12.0f); // 12 steps per loop

    // Initialize streams and triads
    InitializeStreams();
    InitializeTriads();

    // Find references if not set
    if (!GamingMasterySystem)
    {
        GamingMasterySystem = GetOwner()->FindComponentByClass<UGamingMasterySystem>();
    }
    if (!CognitionBridge)
    {
        CognitionBridge = GetOwner()->FindComponentByClass<UStrategicCognitionBridge>();
    }

    // Initialize feature weights (simple random initialization)
    FeatureWeights.SetNum(32);
    for (int32 i = 0; i < 32; i++)
    {
        FeatureWeights[i].SetNum(64);
        for (int32 j = 0; j < 64; j++)
        {
            FeatureWeights[i][j] = FMath::FRandRange(-1.0f, 1.0f);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("EchobeatsGamingIntegration initialized - 3-Stream Cognitive Processing Active"));
}

void UEchobeatsGamingIntegration::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Accumulate time
    TimeAccumulator += DeltaTime;

    // Advance streams based on timing
    while (TimeAccumulator >= StepDuration)
    {
        AdvanceStreams();
        TimeAccumulator -= StepDuration;
    }

    // Apply salience decay
    ApplySalienceDecay(DeltaTime);

    // Adaptive timing adjustment
    if (bAdaptiveTiming)
    {
        // Adjust loop frequency based on game demands
        float AvgLatency = (PerceptionStream.ProcessingLatency + ActionStream.ProcessingLatency + SimulationStream.ProcessingLatency) / 3.0f;
        if (AvgLatency > StepDuration * 1000.0f) // Latency in ms
        {
            // Slow down if processing can't keep up
            LoopFrequency = FMath::Max(30.0f, LoopFrequency - 1.0f);
            StepDuration = 1.0f / (LoopFrequency * 12.0f);
        }
        else if (AvgLatency < StepDuration * 500.0f)
        {
            // Speed up if we have headroom
            LoopFrequency = FMath::Min(120.0f, LoopFrequency + 0.5f);
            StepDuration = 1.0f / (LoopFrequency * 12.0f);
        }
    }
}

// ========================================
// INITIALIZATION
// ========================================

void UEchobeatsGamingIntegration::InitializeStreams()
{
    // Initialize Perception Stream (starts at step 1)
    PerceptionStream.Phase = ECognitiveStreamPhase::Perception;
    PerceptionStream.CurrentStep = EGamingCognitiveStep::Step1_SensoryInput;
    PerceptionStream.StepIndex = 1;
    PerceptionStream.ActivationLevel = 1.0f;

    // Initialize Action Stream (starts at step 5, 4 steps offset)
    ActionStream.Phase = ECognitiveStreamPhase::Action;
    ActionStream.CurrentStep = EGamingCognitiveStep::Step5_GoalEvaluation;
    ActionStream.StepIndex = 5;
    ActionStream.ActivationLevel = 1.0f;

    // Initialize Simulation Stream (starts at step 9, 8 steps offset)
    SimulationStream.Phase = ECognitiveStreamPhase::Simulation;
    SimulationStream.CurrentStep = EGamingCognitiveStep::Step9_ActionExecution;
    SimulationStream.StepIndex = 9;
    SimulationStream.ActivationLevel = 1.0f;

    GlobalStepCounter = 0;
}

void UEchobeatsGamingIntegration::InitializeTriads()
{
    TriadStates.SetNum(4);

    // Triad 0: {1, 5, 9}
    TriadStates[0].TriadIndex = 0;
    TriadStates[0].Steps.Add(1);
    TriadStates[0].Steps.Add(5);
    TriadStates[0].Steps.Add(9);

    // Triad 1: {2, 6, 10}
    TriadStates[1].TriadIndex = 1;
    TriadStates[1].Steps.Add(2);
    TriadStates[1].Steps.Add(6);
    TriadStates[1].Steps.Add(10);

    // Triad 2: {3, 7, 11}
    TriadStates[2].TriadIndex = 2;
    TriadStates[2].Steps.Add(3);
    TriadStates[2].Steps.Add(7);
    TriadStates[2].Steps.Add(11);

    // Triad 3: {4, 8, 12}
    TriadStates[3].TriadIndex = 3;
    TriadStates[3].Steps.Add(4);
    TriadStates[3].Steps.Add(8);
    TriadStates[3].Steps.Add(12);
}

// ========================================
// STREAM CONTROL
// ========================================

void UEchobeatsGamingIntegration::AdvanceStreams()
{
    // Increment global step
    GlobalStepCounter = (GlobalStepCounter % 12) + 1;

    // Calculate step for each stream (120° phase offset)
    // Perception: steps 1,2,3,4,5,6,7,8,9,10,11,12
    // Action: steps 5,6,7,8,9,10,11,12,1,2,3,4 (offset by 4)
    // Simulation: steps 9,10,11,12,1,2,3,4,5,6,7,8 (offset by 8)

    PerceptionStream.StepIndex = GlobalStepCounter;
    PerceptionStream.CurrentStep = CalculateStreamStep(ECognitiveStreamPhase::Perception, GlobalStepCounter);

    ActionStream.StepIndex = ((GlobalStepCounter + 3) % 12) + 1; // +4 offset, but 0-indexed
    ActionStream.CurrentStep = CalculateStreamStep(ECognitiveStreamPhase::Action, ActionStream.StepIndex);

    SimulationStream.StepIndex = ((GlobalStepCounter + 7) % 12) + 1; // +8 offset
    SimulationStream.CurrentStep = CalculateStreamStep(ECognitiveStreamPhase::Simulation, SimulationStream.StepIndex);

    // Process each stream's current step
    ProcessPerceptionStep(PerceptionStream.CurrentStep);
    ProcessActionStep(ActionStream.CurrentStep);
    ProcessSimulationStep(SimulationStream.CurrentStep);

    // Process current triad
    ProcessCurrentTriad();

    // Apply cross-stream coherence if enabled
    if (bCrossStreamCoherence)
    {
        float Coherence = CalculateCrossStreamCoherence();
        if (Coherence < 0.7f)
        {
            SynchronizeStreams();
        }
    }
}

void UEchobeatsGamingIntegration::ProcessCurrentTriad()
{
    int32 TriadIndex = GetCurrentTriadIndex();
    UpdateTriadState(TriadIndex);

    // Integrate outputs from all streams in this triad
    FTriadState& Triad = TriadStates[TriadIndex];

    // Combine stream outputs
    Triad.IntegratedOutput.Empty();
    int32 MaxSize = FMath::Max3(PerceptionStream.OutputData.Num(), ActionStream.OutputData.Num(), SimulationStream.OutputData.Num());

    for (int32 i = 0; i < MaxSize; i++)
    {
        float PerceptionVal = i < PerceptionStream.OutputData.Num() ? PerceptionStream.OutputData[i] : 0.0f;
        float ActionVal = i < ActionStream.OutputData.Num() ? ActionStream.OutputData[i] : 0.0f;
        float SimulationVal = i < SimulationStream.OutputData.Num() ? SimulationStream.OutputData[i] : 0.0f;

        // Weighted integration based on stream activation
        float Integrated = (PerceptionVal * PerceptionStream.ActivationLevel +
                           ActionVal * ActionStream.ActivationLevel +
                           SimulationVal * SimulationStream.ActivationLevel) /
                          (PerceptionStream.ActivationLevel + ActionStream.ActivationLevel + SimulationStream.ActivationLevel);

        Triad.IntegratedOutput.Add(Integrated);
    }

    // Broadcast triad completion
    OnTriadComplete.Broadcast(TriadIndex, Triad.IntegratedOutput);
}

int32 UEchobeatsGamingIntegration::GetCurrentGlobalStep() const
{
    return GlobalStepCounter;
}

int32 UEchobeatsGamingIntegration::GetCurrentTriadIndex() const
{
    // Triads: {1,5,9}=0, {2,6,10}=1, {3,7,11}=2, {4,8,12}=3
    return (GlobalStepCounter - 1) % 4;
}

void UEchobeatsGamingIntegration::SynchronizeStreams()
{
    // Adjust stream activation to improve coherence
    float AvgActivation = (PerceptionStream.ActivationLevel + ActionStream.ActivationLevel + SimulationStream.ActivationLevel) / 3.0f;

    // Move each stream toward average
    PerceptionStream.ActivationLevel = FMath::Lerp(PerceptionStream.ActivationLevel, AvgActivation, 0.1f);
    ActionStream.ActivationLevel = FMath::Lerp(ActionStream.ActivationLevel, AvgActivation, 0.1f);
    SimulationStream.ActivationLevel = FMath::Lerp(SimulationStream.ActivationLevel, AvgActivation, 0.1f);

    // Reduce error rates
    PerceptionStream.ErrorRate *= 0.9f;
    ActionStream.ErrorRate *= 0.9f;
    SimulationStream.ErrorRate *= 0.9f;
}

// ========================================
// PERCEPTION STREAM API
// ========================================

void UEchobeatsGamingIntegration::ProcessSensoryInput(const TArray<float>& SensoryData)
{
    PerceptionStream.ProcessingData = SensoryData;
    LastGameState = SensoryData;

    // Update salience based on input changes
    if (SensoryData.Num() > 0)
    {
        // Detect significant changes
        for (int32 i = 0; i < SensoryData.Num() && i < LastGameState.Num(); i++)
        {
            if (FMath::Abs(SensoryData[i] - LastGameState[i]) > 0.1f)
            {
                // Significant change detected - boost salience
                FString ElementID = FString::Printf(TEXT("element_%d"), i);
                if (SalienceMap.ElementSalience.Contains(ElementID))
                {
                    SalienceMap.ElementSalience[ElementID] = FMath::Min(1.0f, SalienceMap.ElementSalience[ElementID] + 0.2f);
                }
                else
                {
                    SalienceMap.ElementSalience.Add(ElementID, 0.5f);
                }
            }
        }
    }
}

TArray<float> UEchobeatsGamingIntegration::ExtractFeatures(const TArray<float>& RawInput)
{
    TArray<float> Features;

    if (RawInput.Num() == 0 || FeatureWeights.Num() == 0)
    {
        return Features;
    }

    // Simple feature extraction using learned weights
    Features.SetNum(FeatureWeights.Num());

    for (int32 i = 0; i < FeatureWeights.Num(); i++)
    {
        float FeatureVal = 0.0f;
        for (int32 j = 0; j < FMath::Min(RawInput.Num(), FeatureWeights[i].Num()); j++)
        {
            FeatureVal += RawInput[j] * FeatureWeights[i][j];
        }
        Features[i] = FMath::Tanh(FeatureVal); // Normalize with tanh
    }

    return Features;
}

TArray<FString> UEchobeatsGamingIntegration::MatchPatterns(const TArray<float>& Features)
{
    TArray<FString> MatchedPatterns;

    for (const auto& Pair : PatternTemplates)
    {
        float Similarity = CalculateFeatureSimilarity(Features, Pair.Value);
        if (Similarity > 0.7f)
        {
            MatchedPatterns.Add(Pair.Key);
            OnPatternRecognized.Broadcast(Pair.Key, Similarity);
        }
    }

    return MatchedPatterns;
}

TMap<FString, float> UEchobeatsGamingIntegration::AssessSituation(const TArray<FString>& MatchedPatterns)
{
    TMap<FString, float> Assessment;

    // Default situation factors
    Assessment.Add(TEXT("threat_level"), 0.0f);
    Assessment.Add(TEXT("opportunity_level"), 0.0f);
    Assessment.Add(TEXT("resource_status"), 0.5f);
    Assessment.Add(TEXT("position_advantage"), 0.5f);
    Assessment.Add(TEXT("time_pressure"), 0.0f);

    // Adjust based on matched patterns
    for (const FString& Pattern : MatchedPatterns)
    {
        if (Pattern.Contains(TEXT("threat")) || Pattern.Contains(TEXT("danger")))
        {
            Assessment[TEXT("threat_level")] += 0.2f;
        }
        if (Pattern.Contains(TEXT("opportunity")) || Pattern.Contains(TEXT("opening")))
        {
            Assessment[TEXT("opportunity_level")] += 0.2f;
        }
        if (Pattern.Contains(TEXT("advantage")))
        {
            Assessment[TEXT("position_advantage")] += 0.1f;
        }
        if (Pattern.Contains(TEXT("pressure")) || Pattern.Contains(TEXT("rush")))
        {
            Assessment[TEXT("time_pressure")] += 0.2f;
        }
    }

    // Clamp values - use key-based access for modification
    TArray<FString> Keys;
    for (const auto& Pair : Assessment)
    {
        Keys.Add(Pair.Key);
    }
    for (const FString& Key : Keys)
    {
        Assessment[Key] = FMath::Clamp(Assessment[Key], 0.0f, 1.0f);
    }

    return Assessment;
}

void UEchobeatsGamingIntegration::UpdateSalienceMap(const TArray<float>& GameState, const TArray<FString>& GameElements)
{
    // Update salience for each game element
    for (int32 i = 0; i < GameElements.Num(); i++)
    {
        const FString& Element = GameElements[i];
        float BaseSalience = i < GameState.Num() ? GameState[i] : 0.5f;

        // Apply threat/opportunity boosts
        if (Element.Contains(TEXT("enemy")) || Element.Contains(TEXT("threat")))
        {
            BaseSalience *= SalienceMap.ThreatBoost;
        }
        if (Element.Contains(TEXT("resource")) || Element.Contains(TEXT("objective")))
        {
            BaseSalience *= SalienceMap.OpportunityBoost;
        }

        SalienceMap.ElementSalience.Add(Element, FMath::Clamp(BaseSalience, 0.0f, 1.0f));
    }
}

// ========================================
// ACTION STREAM API
// ========================================

TMap<FString, float> UEchobeatsGamingIntegration::EvaluateGoals(const TMap<FString, float>& SituationAssessment)
{
    TMap<FString, float> GoalEvaluation;

    // Default goals
    GoalEvaluation.Add(TEXT("survive"), 0.5f);
    GoalEvaluation.Add(TEXT("attack"), 0.3f);
    GoalEvaluation.Add(TEXT("defend"), 0.3f);
    GoalEvaluation.Add(TEXT("expand"), 0.3f);
    GoalEvaluation.Add(TEXT("gather"), 0.3f);

    // Adjust based on situation
    if (SituationAssessment.Contains(TEXT("threat_level")))
    {
        float Threat = SituationAssessment[TEXT("threat_level")];
        GoalEvaluation[TEXT("survive")] += Threat * 0.3f;
        GoalEvaluation[TEXT("defend")] += Threat * 0.2f;
        GoalEvaluation[TEXT("attack")] -= Threat * 0.1f;
    }

    if (SituationAssessment.Contains(TEXT("opportunity_level")))
    {
        float Opportunity = SituationAssessment[TEXT("opportunity_level")];
        GoalEvaluation[TEXT("attack")] += Opportunity * 0.3f;
        GoalEvaluation[TEXT("expand")] += Opportunity * 0.2f;
    }

    if (SituationAssessment.Contains(TEXT("resource_status")))
    {
        float Resources = SituationAssessment[TEXT("resource_status")];
        if (Resources < 0.3f)
        {
            GoalEvaluation[TEXT("gather")] += 0.3f;
        }
    }

    // Normalize
    float TotalGoal = 0.0f;
    for (const auto& Pair : GoalEvaluation)
    {
        TotalGoal += Pair.Value;
    }
    TArray<FString> GoalKeys;
    for (const auto& Pair : GoalEvaluation)
    {
        GoalKeys.Add(Pair.Key);
    }
    for (const FString& Key : GoalKeys)
    {
        GoalEvaluation[Key] /= TotalGoal;
    }

    return GoalEvaluation;
}

FString UEchobeatsGamingIntegration::SelectStrategy(const TMap<FString, float>& GoalEvaluation)
{
    // Find highest priority goal
    FString TopGoal;
    float TopPriority = 0.0f;

    for (const auto& Pair : GoalEvaluation)
    {
        if (Pair.Value > TopPriority)
        {
            TopPriority = Pair.Value;
            TopGoal = Pair.Key;
        }
    }

    // Map goal to strategy
    if (TopGoal == TEXT("survive"))
    {
        return TEXT("defensive_retreat");
    }
    else if (TopGoal == TEXT("attack"))
    {
        return TEXT("aggressive_push");
    }
    else if (TopGoal == TEXT("defend"))
    {
        return TEXT("hold_position");
    }
    else if (TopGoal == TEXT("expand"))
    {
        return TEXT("map_control");
    }
    else if (TopGoal == TEXT("gather"))
    {
        return TEXT("resource_focus");
    }

    return TEXT("balanced");
}

TArray<FString> UEchobeatsGamingIntegration::PlanActions(const FString& SelectedStrategy)
{
    TArray<FString> PlannedActions;

    // Generate action sequence based on strategy
    if (SelectedStrategy == TEXT("defensive_retreat"))
    {
        PlannedActions.Add(TEXT("disengage"));
        PlannedActions.Add(TEXT("move_to_safety"));
        PlannedActions.Add(TEXT("heal_recover"));
    }
    else if (SelectedStrategy == TEXT("aggressive_push"))
    {
        PlannedActions.Add(TEXT("identify_target"));
        PlannedActions.Add(TEXT("approach"));
        PlannedActions.Add(TEXT("engage"));
        PlannedActions.Add(TEXT("follow_up"));
    }
    else if (SelectedStrategy == TEXT("hold_position"))
    {
        PlannedActions.Add(TEXT("fortify"));
        PlannedActions.Add(TEXT("watch_angles"));
        PlannedActions.Add(TEXT("counter_when_attacked"));
    }
    else if (SelectedStrategy == TEXT("map_control"))
    {
        PlannedActions.Add(TEXT("scout"));
        PlannedActions.Add(TEXT("claim_territory"));
        PlannedActions.Add(TEXT("establish_presence"));
    }
    else if (SelectedStrategy == TEXT("resource_focus"))
    {
        PlannedActions.Add(TEXT("locate_resources"));
        PlannedActions.Add(TEXT("gather"));
        PlannedActions.Add(TEXT("return_safely"));
    }
    else
    {
        PlannedActions.Add(TEXT("observe"));
        PlannedActions.Add(TEXT("adapt"));
    }

    ActionQueue = PlannedActions;
    return PlannedActions;
}

void UEchobeatsGamingIntegration::PrepareMotorExecution(const TArray<FString>& PlannedActions)
{
    // Prepare motor system for upcoming actions
    if (CognitionBridge)
    {
        for (const FString& Action : PlannedActions)
        {
            // Check motor readiness for each action
            float Readiness = CognitionBridge->GetMotorReadiness(Action);
            if (Readiness < 0.5f)
            {
                // Need to warm up motor system
                UE_LOG(LogTemp, Verbose, TEXT("Motor readiness low for %s: %.2f"), *Action, Readiness);
            }
        }
    }

    // Store in action stream output
    ActionStream.OutputData.Empty();
    for (int32 i = 0; i < PlannedActions.Num(); i++)
    {
        ActionStream.OutputData.Add(1.0f); // Action ready flag
    }
}

bool UEchobeatsGamingIntegration::ExecuteAction(const FString& ActionID)
{
    // Execute the action
    // In full implementation, this would interface with game input system

    // Record execution for learning
    if (CognitionBridge)
    {
        CognitionBridge->RecordMotorExecution(ActionID, 0.8f, 0.9f); // Placeholder quality scores
    }

    // Remove from queue
    ActionQueue.Remove(ActionID);

    return true;
}

TArray<FAffordanceDetection> UEchobeatsGamingIntegration::DetectAffordances(const TArray<float>& GameState)
{
    TArray<FAffordanceDetection> Affordances;

    // Analyze game state for action possibilities
    // This is a simplified implementation

    if (GameState.Num() >= 4)
    {
        // Check for attack affordance
        if (GameState[0] > 0.6f) // Enemy in range
        {
            FAffordanceDetection Attack;
            Attack.AffordanceID = TEXT("attack_opportunity");
            Attack.AffordanceType = TEXT("attack");
            Attack.Confidence = GameState[0];
            Attack.EstimatedValue = 0.7f;
            Attack.TimeWindow = 2.0f;
            Attack.RequiredSkillLevel = 0.5f;
            Attack.RiskLevel = 0.4f;
            Affordances.Add(Attack);
            OnAffordanceDetected.Broadcast(Attack);
        }

        // Check for resource affordance
        if (GameState[1] > 0.5f) // Resource available
        {
            FAffordanceDetection Resource;
            Resource.AffordanceID = TEXT("resource_opportunity");
            Resource.AffordanceType = TEXT("resource");
            Resource.Confidence = GameState[1];
            Resource.EstimatedValue = 0.5f;
            Resource.TimeWindow = 5.0f;
            Resource.RequiredSkillLevel = 0.2f;
            Resource.RiskLevel = 0.2f;
            Affordances.Add(Resource);
            OnAffordanceDetected.Broadcast(Resource);
        }

        // Check for escape affordance
        if (GameState[2] > 0.7f) // Threat level high
        {
            FAffordanceDetection Escape;
            Escape.AffordanceID = TEXT("escape_opportunity");
            Escape.AffordanceType = TEXT("escape");
            Escape.Confidence = 0.8f;
            Escape.EstimatedValue = 0.9f;
            Escape.TimeWindow = 1.0f;
            Escape.RequiredSkillLevel = 0.3f;
            Escape.RiskLevel = 0.1f;
            Affordances.Add(Escape);
            OnAffordanceDetected.Broadcast(Escape);
        }
    }

    DetectedAffordances = Affordances;
    return Affordances;
}

// ========================================
// SIMULATION STREAM API
// ========================================

TArray<float> UEchobeatsGamingIntegration::PredictOutcome(const FString& ActionID, const TArray<float>& CurrentState)
{
    TArray<float> PredictedState = CurrentState;

    // Simple outcome prediction based on action type
    // In full implementation, this would use learned models

    if (ActionID.Contains(TEXT("attack")))
    {
        // Predict damage dealt, resources spent, position change
        if (PredictedState.Num() > 0) PredictedState[0] -= 0.2f; // Enemy health
        if (PredictedState.Num() > 1) PredictedState[1] -= 0.1f; // Our resources
    }
    else if (ActionID.Contains(TEXT("gather")))
    {
        if (PredictedState.Num() > 1) PredictedState[1] += 0.15f; // Resources gained
    }
    else if (ActionID.Contains(TEXT("move")) || ActionID.Contains(TEXT("retreat")))
    {
        if (PredictedState.Num() > 2) PredictedState[2] -= 0.1f; // Threat reduced
    }

    // Clamp predictions
    for (float& Val : PredictedState)
    {
        Val = FMath::Clamp(Val, 0.0f, 1.0f);
    }

    return PredictedState;
}

void UEchobeatsGamingIntegration::IntegrateFeedback(const TArray<float>& ExpectedOutcome, const TArray<float>& ActualOutcome)
{
    // Calculate prediction error
    float TotalError = 0.0f;
    int32 CompareCount = FMath::Min(ExpectedOutcome.Num(), ActualOutcome.Num());

    for (int32 i = 0; i < CompareCount; i++)
    {
        TotalError += FMath::Abs(ExpectedOutcome[i] - ActualOutcome[i]);
    }

    float AvgError = CompareCount > 0 ? TotalError / CompareCount : 0.0f;

    // Store error for trend analysis
    PredictionErrorHistory.Add(AvgError);
    if (PredictionErrorHistory.Num() > 100)
    {
        PredictionErrorHistory.RemoveAt(0);
    }

    // Broadcast if error is significant
    if (AvgError > 0.3f)
    {
        OnPredictionError.Broadcast(AvgError, TEXT("outcome_prediction"));
    }

    // Update simulation stream error rate
    SimulationStream.ErrorRate = SimulationStream.ErrorRate * 0.9f + AvgError * 0.1f;
}

void UEchobeatsGamingIntegration::UpdateLearning(float RewardSignal, const TArray<float>& StateFeatures)
{
    // Simple learning update
    // In full implementation, this would use TD learning or similar

    // Update feature weights based on reward
    float LearningRate = 0.01f;

    for (int32 i = 0; i < FeatureWeights.Num() && i < StateFeatures.Num(); i++)
    {
        for (int32 j = 0; j < FeatureWeights[i].Num(); j++)
        {
            // Hebbian-like update
            FeatureWeights[i][j] += LearningRate * RewardSignal * StateFeatures[i];
            FeatureWeights[i][j] = FMath::Clamp(FeatureWeights[i][j], -2.0f, 2.0f);
        }
    }

    // Update Gaming Mastery System if available
    if (GamingMasterySystem)
    {
        GamingMasterySystem->UpdateESNTraining(StateFeatures, SimulationStream.OutputData);
    }
}

FPredictionState UEchobeatsGamingIntegration::RunMentalSimulation(const TArray<float>& CurrentState, int32 Depth)
{
    FPredictionState Result;
    Result.PredictedState = CurrentState;
    Result.SimulationDepth = Depth;
    Result.PredictionHorizon = PredictionHorizon;

    // Simple tree search simulation
    TArray<FString> PossibleActions;
    PossibleActions.Add(TEXT("attack"));
    PossibleActions.Add(TEXT("defend"));
    PossibleActions.Add(TEXT("gather"));
    PossibleActions.Add(TEXT("move"));

    for (const FString& Action : PossibleActions)
    {
        TArray<float> SimulatedState = CurrentState;

        // Simulate action outcomes
        for (int32 d = 0; d < Depth; d++)
        {
            SimulatedState = PredictOutcome(Action, SimulatedState);
        }

        // Evaluate final state
        float StateValue = 0.0f;
        for (float Val : SimulatedState)
        {
            StateValue += Val;
        }
        StateValue /= FMath::Max(1, SimulatedState.Num());

        Result.ActionOutcomePredictions.Add(Action, StateValue);
    }

    // Find best predicted outcome
    float BestValue = -FLT_MAX;
    for (const auto& Pair : Result.ActionOutcomePredictions)
    {
        if (Pair.Value > BestValue)
        {
            BestValue = Pair.Value;
        }
    }

    Result.Confidence = FMath::Clamp(1.0f - SimulationStream.ErrorRate, 0.0f, 1.0f);

    PredictionState = Result;
    return Result;
}

TMap<FString, float> UEchobeatsGamingIntegration::PredictOpponentBehavior(const FString& OpponentID, const TArray<float>& GameState)
{
    TMap<FString, float> Predictions;

    // Use Gaming Mastery System's opponent modeling if available
    if (GamingMasterySystem && GamingMasterySystem->bEnableOpponentModeling)
    {
        Predictions = GamingMasterySystem->PredictOpponentAction(OpponentID, GameState);
    }
    else
    {
        // Default predictions
        Predictions.Add(TEXT("attack"), 0.3f);
        Predictions.Add(TEXT("defend"), 0.3f);
        Predictions.Add(TEXT("expand"), 0.2f);
        Predictions.Add(TEXT("gather"), 0.2f);
    }

    return Predictions;
}

// ========================================
// INTEGRATION API
// ========================================

FString UEchobeatsGamingIntegration::GetIntegratedDecision()
{
    // Combine outputs from all three streams

    // Get situation assessment from perception
    TMap<FString, float> Situation = AssessSituation(MatchPatterns(PerceptionStream.OutputData));

    // Get goal evaluation from action stream
    TMap<FString, float> Goals = EvaluateGoals(Situation);

    // Get predictions from simulation stream
    FPredictionState Prediction = RunMentalSimulation(LastGameState, 3);

    // Combine to select best action
    FString BestAction;
    float BestScore = -FLT_MAX;

    for (const auto& Pair : Prediction.ActionOutcomePredictions)
    {
        float Score = Pair.Value;

        // Weight by goal alignment
        if (Goals.Contains(Pair.Key))
        {
            Score *= (1.0f + Goals[Pair.Key]);
        }

        // Weight by prediction confidence
        Score *= Prediction.Confidence;

        if (Score > BestScore)
        {
            BestScore = Score;
            BestAction = Pair.Key;
        }
    }

    return BestAction;
}

float UEchobeatsGamingIntegration::CalculateCrossStreamCoherence() const
{
    // Calculate how well the three streams are aligned

    // Compare output magnitudes
    float PerceptionMag = 0.0f, ActionMag = 0.0f, SimulationMag = 0.0f;

    for (float Val : PerceptionStream.OutputData) PerceptionMag += Val * Val;
    for (float Val : ActionStream.OutputData) ActionMag += Val * Val;
    for (float Val : SimulationStream.OutputData) SimulationMag += Val * Val;

    PerceptionMag = FMath::Sqrt(PerceptionMag);
    ActionMag = FMath::Sqrt(ActionMag);
    SimulationMag = FMath::Sqrt(SimulationMag);

    // Calculate variance in magnitudes
    float AvgMag = (PerceptionMag + ActionMag + SimulationMag) / 3.0f;
    float Variance = (FMath::Square(PerceptionMag - AvgMag) +
                     FMath::Square(ActionMag - AvgMag) +
                     FMath::Square(SimulationMag - AvgMag)) / 3.0f;

    // Low variance = high coherence
    float Coherence = 1.0f - FMath::Min(1.0f, FMath::Sqrt(Variance) / FMath::Max(0.01f, AvgMag));

    // Also factor in error rates
    float AvgError = (PerceptionStream.ErrorRate + ActionStream.ErrorRate + SimulationStream.ErrorRate) / 3.0f;
    Coherence *= (1.0f - AvgError);

    return FMath::Clamp(Coherence, 0.0f, 1.0f);
}

float UEchobeatsGamingIntegration::GetStreamSynchronizationQuality() const
{
    // Check if streams are properly phased
    int32 ExpectedPerception = GlobalStepCounter;
    int32 ExpectedAction = ((GlobalStepCounter + 3) % 12) + 1;
    int32 ExpectedSimulation = ((GlobalStepCounter + 7) % 12) + 1;

    float SyncQuality = 1.0f;

    if (PerceptionStream.StepIndex != ExpectedPerception) SyncQuality -= 0.33f;
    if (ActionStream.StepIndex != ExpectedAction) SyncQuality -= 0.33f;
    if (SimulationStream.StepIndex != ExpectedSimulation) SyncQuality -= 0.33f;

    return FMath::Max(0.0f, SyncQuality);
}

void UEchobeatsGamingIntegration::BalanceStreamActivation()
{
    // Balance activation levels based on current demands

    // Perception should be high when situation is uncertain
    float SituationUncertainty = SimulationStream.ErrorRate;
    PerceptionStream.ActivationLevel = 0.5f + SituationUncertainty * 0.5f;

    // Action should be high when execution is needed
    float ExecutionDemand = ActionQueue.Num() > 0 ? 1.0f : 0.5f;
    ActionStream.ActivationLevel = ExecutionDemand;

    // Simulation should be high when planning is needed
    float PlanningDemand = 1.0f - PredictionState.Confidence;
    SimulationStream.ActivationLevel = 0.5f + PlanningDemand * 0.5f;

    // Normalize
    float TotalActivation = PerceptionStream.ActivationLevel + ActionStream.ActivationLevel + SimulationStream.ActivationLevel;
    PerceptionStream.ActivationLevel /= TotalActivation / 3.0f;
    ActionStream.ActivationLevel /= TotalActivation / 3.0f;
    SimulationStream.ActivationLevel /= TotalActivation / 3.0f;
}

ECognitiveStreamPhase UEchobeatsGamingIntegration::GetProcessingBottleneck() const
{
    // Find stream with highest latency
    if (PerceptionStream.ProcessingLatency >= ActionStream.ProcessingLatency &&
        PerceptionStream.ProcessingLatency >= SimulationStream.ProcessingLatency)
    {
        return ECognitiveStreamPhase::Perception;
    }
    else if (ActionStream.ProcessingLatency >= SimulationStream.ProcessingLatency)
    {
        return ECognitiveStreamPhase::Action;
    }
    return ECognitiveStreamPhase::Simulation;
}

// ========================================
// GAMING-SPECIFIC API
// ========================================

void UEchobeatsGamingIntegration::ProcessGameFrame(const TArray<float>& FrameData, float DeltaTime)
{
    // Process a complete game frame through all streams
    ProcessSensoryInput(FrameData);

    // Extract and match patterns
    TArray<float> Features = ExtractFeatures(FrameData);
    TArray<FString> Patterns = MatchPatterns(Features);

    // Assess situation
    TMap<FString, float> Situation = AssessSituation(Patterns);

    // Detect affordances
    DetectAffordances(FrameData);

    // Run simulation
    RunMentalSimulation(FrameData, 3);

    // Update cognition bridge if available
    if (CognitionBridge)
    {
        // Update cognitive load based on processing demands
        CognitionBridge->UpdateCognitiveLoad(ECognitiveLoadType::Perceptual, PerceptionStream.ActivationLevel * 0.5f);
        CognitionBridge->UpdateCognitiveLoad(ECognitiveLoadType::DecisionMaking, ActionStream.ActivationLevel * 0.5f);
        CognitionBridge->UpdateCognitiveLoad(ECognitiveLoadType::WorkingMemory, SimulationStream.ActivationLevel * 0.5f);
    }
}

FString UEchobeatsGamingIntegration::GetRecommendedAction(const TArray<float>& GameState)
{
    // Get integrated decision
    FString Decision = GetIntegratedDecision();

    // Check affordances for better options
    for (const FAffordanceDetection& Affordance : DetectedAffordances)
    {
        if (Affordance.Confidence > 0.8f && Affordance.EstimatedValue > 0.7f)
        {
            // High-confidence, high-value affordance - consider it
            FString AffordanceAction = GenerateActionFromAffordance(Affordance);
            if (!AffordanceAction.IsEmpty())
            {
                return AffordanceAction;
            }
        }
    }

    return Decision;
}

FString UEchobeatsGamingIntegration::GetAttentionFocusRecommendation() const
{
    // Find highest salience element
    FString HighestSalience;
    float MaxSalience = 0.0f;

    for (const auto& Pair : SalienceMap.ElementSalience)
    {
        if (Pair.Value > MaxSalience)
        {
            MaxSalience = Pair.Value;
            HighestSalience = Pair.Key;
        }
    }

    if (MaxSalience > 0.7f)
    {
        return FString::Printf(TEXT("Focus on %s (salience: %.2f)"), *HighestSalience, MaxSalience);
    }

    return TEXT("Maintain broad awareness");
}

float UEchobeatsGamingIntegration::GetActionTimingRecommendation(const FString& ActionID) const
{
    // Recommend optimal timing for action

    // Check affordances for time windows
    for (const FAffordanceDetection& Affordance : DetectedAffordances)
    {
        if (Affordance.AffordanceType == ActionID || Affordance.AffordanceID.Contains(ActionID))
        {
            return Affordance.TimeWindow;
        }
    }

    // Default timing based on action type
    if (ActionID.Contains(TEXT("attack")))
    {
        return 0.5f; // Quick execution
    }
    else if (ActionID.Contains(TEXT("defend")))
    {
        return 1.0f; // Reactive timing
    }

    return 2.0f; // Default window
}

TMap<FString, float> UEchobeatsGamingIntegration::EvaluateActionRiskReward(const FString& ActionID, const TArray<float>& GameState)
{
    TMap<FString, float> Evaluation;

    // Predict outcome
    TArray<float> PredictedState = PredictOutcome(ActionID, GameState);

    // Calculate reward (improvement in state)
    float Reward = 0.0f;
    for (int32 i = 0; i < FMath::Min(GameState.Num(), PredictedState.Num()); i++)
    {
        Reward += PredictedState[i] - GameState[i];
    }
    Reward /= FMath::Max(1, GameState.Num());

    // Calculate risk from affordances
    float Risk = 0.3f; // Base risk
    for (const FAffordanceDetection& Affordance : DetectedAffordances)
    {
        if (Affordance.AffordanceType == ActionID)
        {
            Risk = Affordance.RiskLevel;
            break;
        }
    }

    // Adjust for prediction confidence
    Risk *= (2.0f - PredictionState.Confidence);

    Evaluation.Add(TEXT("reward"), FMath::Clamp(Reward, -1.0f, 1.0f));
    Evaluation.Add(TEXT("risk"), FMath::Clamp(Risk, 0.0f, 1.0f));
    Evaluation.Add(TEXT("expected_value"), Reward * (1.0f - Risk));
    Evaluation.Add(TEXT("confidence"), PredictionState.Confidence);

    return Evaluation;
}

// ========================================
// INTERNAL METHODS
// ========================================

void UEchobeatsGamingIntegration::ProcessPerceptionStep(EGamingCognitiveStep Step)
{
    double StartTime = FPlatformTime::Seconds();

    switch (Step)
    {
        case EGamingCognitiveStep::Step1_SensoryInput:
            // Already processed in ProcessSensoryInput
            break;
        case EGamingCognitiveStep::Step2_FeatureExtraction:
            PerceptionStream.OutputData = ExtractFeatures(PerceptionStream.ProcessingData);
            break;
        case EGamingCognitiveStep::Step3_PatternMatching:
            // Pattern matching updates internal state
            MatchPatterns(PerceptionStream.OutputData);
            break;
        case EGamingCognitiveStep::Step4_SituationAssessment:
            // Situation assessment updates internal state
            break;
        default:
            break;
    }

    PerceptionStream.ProcessingLatency = (FPlatformTime::Seconds() - StartTime) * 1000.0f;
}

void UEchobeatsGamingIntegration::ProcessActionStep(EGamingCognitiveStep Step)
{
    double StartTime = FPlatformTime::Seconds();

    switch (Step)
    {
        case EGamingCognitiveStep::Step5_GoalEvaluation:
            // Goal evaluation
            break;
        case EGamingCognitiveStep::Step6_StrategySelection:
            // Strategy selection
            break;
        case EGamingCognitiveStep::Step7_ActionPlanning:
            // Action planning
            break;
        case EGamingCognitiveStep::Step8_MotorPreparation:
            // Motor preparation
            break;
        case EGamingCognitiveStep::Step9_ActionExecution:
            // Execute next action in queue
            if (ActionQueue.Num() > 0)
            {
                ExecuteAction(ActionQueue[0]);
            }
            break;
        default:
            break;
    }

    ActionStream.ProcessingLatency = (FPlatformTime::Seconds() - StartTime) * 1000.0f;
}

void UEchobeatsGamingIntegration::ProcessSimulationStep(EGamingCognitiveStep Step)
{
    double StartTime = FPlatformTime::Seconds();

    switch (Step)
    {
        case EGamingCognitiveStep::Step10_OutcomePrediction:
            // Outcome prediction
            break;
        case EGamingCognitiveStep::Step11_FeedbackIntegration:
            // Feedback integration
            break;
        case EGamingCognitiveStep::Step12_LearningUpdate:
            // Learning update
            break;
        default:
            break;
    }

    SimulationStream.ProcessingLatency = (FPlatformTime::Seconds() - StartTime) * 1000.0f;
}

EGamingCognitiveStep UEchobeatsGamingIntegration::CalculateStreamStep(ECognitiveStreamPhase Phase, int32 GlobalStep) const
{
    return static_cast<EGamingCognitiveStep>(GlobalStep - 1);
}

void UEchobeatsGamingIntegration::UpdateTriadState(int32 TriadIndex)
{
    if (!TriadStates.IsValidIndex(TriadIndex))
    {
        return;
    }

    FTriadState& Triad = TriadStates[TriadIndex];

    // Calculate synchronization quality
    float SyncQuality = GetStreamSynchronizationQuality();
    Triad.SynchronizationQuality = SyncQuality;

    // Calculate cross-stream coherence
    Triad.CrossStreamCoherence = CalculateCrossStreamCoherence();
}

void UEchobeatsGamingIntegration::ApplySalienceDecay(float DeltaTime)
{
    TArray<FString> ToRemove;
    TArray<FString> AllKeys;
    
    for (const auto& Pair : SalienceMap.ElementSalience)
    {
        AllKeys.Add(Pair.Key);
    }
    
    for (const FString& Key : AllKeys)
    {
        SalienceMap.ElementSalience[Key] -= SalienceDecayRate * DeltaTime;
        if (SalienceMap.ElementSalience[Key] <= 0.0f)
        {
            ToRemove.Add(Key);
        }
    }

    for (const FString& Key : ToRemove)
    {
        SalienceMap.ElementSalience.Remove(Key);
    }
}

float UEchobeatsGamingIntegration::CalculateFeatureSimilarity(const TArray<float>& Features1, const TArray<float>& Features2) const
{
    if (Features1.Num() != Features2.Num() || Features1.Num() == 0)
    {
        return 0.0f;
    }

    // Cosine similarity
    float DotProduct = 0.0f;
    float Norm1 = 0.0f;
    float Norm2 = 0.0f;

    for (int32 i = 0; i < Features1.Num(); i++)
    {
        DotProduct += Features1[i] * Features2[i];
        Norm1 += Features1[i] * Features1[i];
        Norm2 += Features2[i] * Features2[i];
    }

    if (Norm1 == 0.0f || Norm2 == 0.0f)
    {
        return 0.0f;
    }

    return DotProduct / (FMath::Sqrt(Norm1) * FMath::Sqrt(Norm2));
}

FString UEchobeatsGamingIntegration::GenerateActionFromAffordance(const FAffordanceDetection& Affordance) const
{
    // Map affordance type to action
    if (Affordance.AffordanceType == TEXT("attack"))
    {
        return TEXT("execute_attack");
    }
    else if (Affordance.AffordanceType == TEXT("resource"))
    {
        return TEXT("gather_resource");
    }
    else if (Affordance.AffordanceType == TEXT("escape"))
    {
        return TEXT("execute_escape");
    }
    else if (Affordance.AffordanceType == TEXT("defend"))
    {
        return TEXT("execute_defense");
    }

    return TEXT("");
}

void UEchobeatsGamingIntegration::UpdatePredictionModel(const TArray<float>& State, const TArray<float>& NextState, const FString& Action)
{
    // Update prediction model based on observed transition
    // This would be more sophisticated in full implementation

    // Calculate prediction error for this transition
    TArray<float> Predicted = PredictOutcome(Action, State);
    IntegrateFeedback(Predicted, NextState);
}

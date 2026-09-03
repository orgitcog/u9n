// StrategicCognitionBridge.cpp
// Deep Tree Echo - Strategic Cognition Bridge for Gaming Mastery
// Copyright (c) 2025 Deep Tree Echo Project

#include "StrategicCognitionBridge.h"

UStrategicCognitionBridge::UStrategicCognitionBridge()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.05f; // 20Hz for responsive cognitive tracking
}

void UStrategicCognitionBridge::BeginPlay()
{
    Super::BeginPlay();

    // Initialize cognitive load levels
    CognitiveLoad.LoadLevels.Add(ECognitiveLoadType::Perceptual, 0.0f);
    CognitiveLoad.LoadLevels.Add(ECognitiveLoadType::WorkingMemory, 0.0f);
    CognitiveLoad.LoadLevels.Add(ECognitiveLoadType::DecisionMaking, 0.0f);
    CognitiveLoad.LoadLevels.Add(ECognitiveLoadType::MotorExecution, 0.0f);
    CognitiveLoad.LoadLevels.Add(ECognitiveLoadType::TimePressure, 0.0f);
    CognitiveLoad.LoadLevels.Add(ECognitiveLoadType::SocialCoordination, 0.0f);

    // Find Gaming Mastery System if not set
    if (!GamingMasterySystem)
    {
        GamingMasterySystem = GetOwner()->FindComponentByClass<UGamingMasterySystem>();
    }

    UE_LOG(LogTemp, Log, TEXT("StrategicCognitionBridge initialized - 4E Gaming Integration Active"));
}

void UStrategicCognitionBridge::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update cognitive reserve
    UpdateCognitiveReserve(DeltaTime);

    // Track time at current load
    CognitiveLoad.TimeAtCurrentLoad += DeltaTime;

    // Store load history for trend analysis
    CognitiveLoadHistory.Add(CognitiveLoad.TotalLoad);
    if (CognitiveLoadHistory.Num() > 600) // Keep 30 seconds at 20Hz
    {
        CognitiveLoadHistory.RemoveAt(0);
    }

    // Check for cognitive overload
    bool bCurrentlyOverloaded = IsCognitiveOverloadImminent();
    if (bCurrentlyOverloaded && !bWasOverloaded)
    {
        OnCognitiveOverload.Broadcast(CognitiveLoad.TotalLoad);
    }
    bWasOverloaded = bCurrentlyOverloaded;

    // Update attention sustained duration
    AttentionState.SustainedAttentionDuration += DeltaTime;

    // Decay physical fatigue slowly during rest
    if (EmbodiedState.MotorReadiness < 0.3f)
    {
        EmbodiedState.PhysicalFatigue = FMath::Max(0.0f, EmbodiedState.PhysicalFatigue - DeltaTime * 0.01f);
    }
}

// ========================================
// EMBODIED COGNITION API
// ========================================

void UStrategicCognitionBridge::UpdateEmbodiedState(float ReactionTime, float InputAccuracy, float MovementSmoothing)
{
    // Convert reaction time to readiness (faster = higher readiness)
    // Assuming 150ms is excellent, 300ms is average
    EmbodiedState.MotorReadiness = FMath::Clamp(1.0f - (ReactionTime - 0.15f) / 0.15f, 0.0f, 1.0f);

    // Update hand-eye coordination from input accuracy
    EmbodiedState.HandEyeCoordination = EmbodiedState.HandEyeCoordination * 0.9f + InputAccuracy * 0.1f;

    // Update proprioceptive accuracy from movement smoothing
    EmbodiedState.ProprioceptiveAccuracy = EmbodiedState.ProprioceptiveAccuracy * 0.9f + MovementSmoothing * 0.1f;

    // Accumulate fatigue based on activity
    if (EmbodiedState.MotorReadiness > 0.7f)
    {
        EmbodiedState.PhysicalFatigue = FMath::Min(1.0f, EmbodiedState.PhysicalFatigue + 0.001f);
    }
}

void UStrategicCognitionBridge::RecordMotorExecution(const FString& ActionType, float ExecutionQuality, float TimingAccuracy)
{
    // Store execution quality for muscle memory calculation
    if (!MotorExecutionHistory.Contains(ActionType))
    {
        MotorExecutionHistory.Add(ActionType, TArray<float>());
    }

    TArray<float>& History = MotorExecutionHistory[ActionType];
    History.Add(ExecutionQuality * TimingAccuracy);

    // Keep last 100 executions
    if (History.Num() > 100)
    {
        History.RemoveAt(0);
    }

    // Check for muscle memory formation
    float MemoryStrength = CalculateMuscleMemoryStrength(ActionType);
    if (MemoryStrength > 0.8f && History.Num() >= 50)
    {
        OnMuscleMemoryFormed.Broadcast(ActionType, MemoryStrength);
    }

    // Update overall muscle memory activation
    float TotalMemory = 0.0f;
    int32 ActionCount = 0;
    for (const auto& Pair : MotorExecutionHistory)
    {
        TotalMemory += CalculateMuscleMemoryStrength(Pair.Key);
        ActionCount++;
    }
    EmbodiedState.MuscleMemoryActivation = ActionCount > 0 ? TotalMemory / ActionCount : 0.0f;
}

float UStrategicCognitionBridge::GetMotorReadiness(const FString& ActionType) const
{
    float BaseReadiness = EmbodiedState.MotorReadiness;
    float MuscleMemoryBonus = CalculateMuscleMemoryStrength(ActionType) * 0.2f;
    float FatiguePenalty = EmbodiedState.PhysicalFatigue * 0.3f;

    return FMath::Clamp(BaseReadiness + MuscleMemoryBonus - FatiguePenalty, 0.0f, 1.0f);
}

bool UStrategicCognitionBridge::IsMuscleMemoryActive(const FString& ActionType) const
{
    return CalculateMuscleMemoryStrength(ActionType) > 0.7f;
}

float UStrategicCognitionBridge::GetEmbodiedPerformanceMultiplier() const
{
    float Multiplier = 1.0f;

    // Positive factors
    Multiplier += EmbodiedState.MotorReadiness * 0.2f;
    Multiplier += EmbodiedState.HandEyeCoordination * 0.15f;
    Multiplier += EmbodiedState.ProprioceptiveAccuracy * 0.1f;
    Multiplier += EmbodiedState.MuscleMemoryActivation * 0.15f;
    Multiplier += EmbodiedState.PostureQuality * 0.05f;
    Multiplier += EmbodiedState.BreathingRegularity * 0.05f;

    // Negative factors
    Multiplier -= EmbodiedState.PhysicalFatigue * 0.3f;

    return FMath::Clamp(Multiplier, 0.5f, 1.5f);
}

// ========================================
// EMBEDDED COGNITION API
// ========================================

void UStrategicCognitionBridge::UpdateEmbeddedContext(const FString& GamePhase, const TArray<float>& EnvironmentFeatures)
{
    EmbeddedContext.GamePhase = GamePhase;

    // Update awareness metrics based on environment features
    if (EnvironmentFeatures.Num() >= 4)
    {
        EmbeddedContext.ResourceAwareness = EmbeddedContext.ResourceAwareness * 0.8f + EnvironmentFeatures[0] * 0.2f;
        EmbeddedContext.ThreatAssessmentAccuracy = EmbeddedContext.ThreatAssessmentAccuracy * 0.8f + EnvironmentFeatures[1] * 0.2f;
        EmbeddedContext.OpportunityRecognitionSpeed = EmbeddedContext.OpportunityRecognitionSpeed * 0.8f + EnvironmentFeatures[2] * 0.2f;
        EmbeddedContext.TeamCompositionUnderstanding = EmbeddedContext.TeamCompositionUnderstanding * 0.8f + EnvironmentFeatures[3] * 0.2f;
    }
}

void UStrategicCognitionBridge::RecordMapExperience(const FString& MapID, float PerformanceScore)
{
    if (MapExperienceDB.Contains(MapID))
    {
        // Exponential moving average
        MapExperienceDB[MapID] = MapExperienceDB[MapID] * 0.9f + PerformanceScore * 0.1f;
    }
    else
    {
        MapExperienceDB.Add(MapID, PerformanceScore);
    }

    // Update map familiarity
    EmbeddedContext.MapFamiliarity = MapExperienceDB[MapID];
}

FString UStrategicCognitionBridge::GetContextualStrategyRecommendation() const
{
    // Recommend strategy based on game phase and context
    if (EmbeddedContext.GamePhase == TEXT("early"))
    {
        if (EmbeddedContext.MapFamiliarity < 0.5f)
        {
            return TEXT("Focus on map exploration and information gathering");
        }
        return TEXT("Establish early resource control and map presence");
    }
    else if (EmbeddedContext.GamePhase == TEXT("mid"))
    {
        if (EmbeddedContext.ThreatAssessmentAccuracy > 0.7f)
        {
            return TEXT("Capitalize on identified opponent weaknesses");
        }
        return TEXT("Maintain pressure while building strategic advantage");
    }
    else if (EmbeddedContext.GamePhase == TEXT("late"))
    {
        if (EmbeddedContext.ResourceAwareness > 0.8f)
        {
            return TEXT("Execute decisive plays with resource advantage");
        }
        return TEXT("Play conservatively and wait for opponent mistakes");
    }

    return TEXT("Adapt to current situation dynamically");
}

float UStrategicCognitionBridge::AssessThreatLevel(const TArray<float>& ThreatIndicators)
{
    if (ThreatIndicators.Num() == 0)
    {
        return 0.0f;
    }

    float TotalThreat = 0.0f;
    for (float Indicator : ThreatIndicators)
    {
        TotalThreat += Indicator;
    }

    float RawThreat = TotalThreat / ThreatIndicators.Num();

    // Adjust based on assessment accuracy
    float AdjustedThreat = RawThreat * EmbeddedContext.ThreatAssessmentAccuracy;

    return FMath::Clamp(AdjustedThreat, 0.0f, 1.0f);
}

TArray<FString> UStrategicCognitionBridge::IdentifyOpportunities(const TArray<float>& GameState)
{
    TArray<FString> Opportunities;

    // Analyze game state for opportunities
    // This is a simplified implementation - full version would use ML

    if (GameState.Num() >= 3)
    {
        if (GameState[0] > 0.7f) // Resource advantage
        {
            Opportunities.Add(TEXT("Resource advantage - consider aggressive expansion"));
        }
        if (GameState[1] < 0.3f) // Opponent weakness
        {
            Opportunities.Add(TEXT("Opponent vulnerability detected - timing attack opportunity"));
        }
        if (GameState[2] > 0.6f) // Map control
        {
            Opportunities.Add(TEXT("Map control advantage - deny opponent resources"));
        }
    }

    // Speed of recognition affects quality
    if (EmbeddedContext.OpportunityRecognitionSpeed < 0.5f)
    {
        // May miss some opportunities
        if (Opportunities.Num() > 1)
        {
            Opportunities.RemoveAt(Opportunities.Num() - 1);
        }
    }

    return Opportunities;
}

// ========================================
// ENACTED COGNITION API
// ========================================

void UStrategicCognitionBridge::UpdateEnactedStrategy(const FString& ActionTaken, bool bSuccess, float Impact)
{
    // Track strategy effectiveness
    if (!StrategyEffectivenessHistory.Contains(ActionTaken))
    {
        StrategyEffectivenessHistory.Add(ActionTaken, TArray<bool>());
    }

    StrategyEffectivenessHistory[ActionTaken].Add(bSuccess);

    // Keep last 50 outcomes
    if (StrategyEffectivenessHistory[ActionTaken].Num() > 50)
    {
        StrategyEffectivenessHistory[ActionTaken].RemoveAt(0);
    }

    // Update adaptation rate based on outcome variance
    float Effectiveness = CalculateStrategyEffectiveness(ActionTaken);
    if (Effectiveness < 0.4f)
    {
        // Strategy not working - increase adaptation rate
        EnactedStrategy.AdaptationRate = FMath::Min(1.0f, EnactedStrategy.AdaptationRate + 0.1f);
    }
    else if (Effectiveness > 0.7f)
    {
        // Strategy working well - decrease adaptation rate
        EnactedStrategy.AdaptationRate = FMath::Max(0.1f, EnactedStrategy.AdaptationRate - 0.05f);
    }

    // Update counter-play effectiveness
    if (ActionTaken.Contains(TEXT("counter")) || ActionTaken.Contains(TEXT("Counter")))
    {
        EnactedStrategy.CounterPlayEffectiveness = EnactedStrategy.CounterPlayEffectiveness * 0.9f + (bSuccess ? 1.0f : 0.0f) * 0.1f;
    }
}

FString UStrategicCognitionBridge::GetExplorationRecommendation() const
{
    if (EnactedStrategy.ExplorationTendency < 0.3f)
    {
        return TEXT("Consider trying new strategies - current approach may be too predictable");
    }
    else if (EnactedStrategy.ExplorationTendency > 0.7f)
    {
        return TEXT("Focus on refining effective strategies rather than exploring new ones");
    }

    return TEXT("Balance exploration with exploitation of known effective strategies");
}

void UStrategicCognitionBridge::AdjustRiskTolerance(float GameProgress, float CurrentAdvantage)
{
    FString OldStance = EnactedStrategy.StrategicStance;

    // Adjust risk tolerance based on game state
    if (CurrentAdvantage > 0.3f)
    {
        // Winning - can afford more risk or play safe
        if (GameProgress > 0.7f)
        {
            // Late game with advantage - play safe
            EnactedStrategy.RiskTolerance = FMath::Max(0.2f, EnactedStrategy.RiskTolerance - 0.1f);
            EnactedStrategy.StrategicStance = TEXT("Conservative");
        }
        else
        {
            // Early/mid with advantage - press it
            EnactedStrategy.RiskTolerance = FMath::Min(0.8f, EnactedStrategy.RiskTolerance + 0.05f);
            EnactedStrategy.StrategicStance = TEXT("Aggressive");
        }
    }
    else if (CurrentAdvantage < -0.3f)
    {
        // Losing - may need to take risks
        EnactedStrategy.RiskTolerance = FMath::Min(0.9f, EnactedStrategy.RiskTolerance + 0.15f);
        EnactedStrategy.StrategicStance = TEXT("Desperate");
    }
    else
    {
        // Even game - balanced approach
        EnactedStrategy.RiskTolerance = 0.5f;
        EnactedStrategy.StrategicStance = TEXT("Balanced");
    }

    // Broadcast if stance changed
    if (OldStance != EnactedStrategy.StrategicStance)
    {
        OnStrategicAdaptation.Broadcast(OldStance, EnactedStrategy.StrategicStance);
    }
}

FString UStrategicCognitionBridge::GetAdaptiveResponse(const FString& OpponentAction, const TArray<float>& GameState)
{
    // Generate adaptive response based on opponent action and game state
    // This would integrate with GamingMasterySystem's opponent modeling

    FString Response;

    if (OpponentAction.Contains(TEXT("aggressive")) || OpponentAction.Contains(TEXT("attack")))
    {
        if (EnactedStrategy.CounterPlayEffectiveness > 0.6f)
        {
            Response = TEXT("Counter-attack with prepared response");
        }
        else
        {
            Response = TEXT("Defensive posture while gathering information");
        }
    }
    else if (OpponentAction.Contains(TEXT("defensive")) || OpponentAction.Contains(TEXT("turtle")))
    {
        Response = TEXT("Apply pressure and force reactions");
    }
    else if (OpponentAction.Contains(TEXT("expand")) || OpponentAction.Contains(TEXT("economic")))
    {
        if (EnactedStrategy.RiskTolerance > 0.6f)
        {
            Response = TEXT("Punish greedy play with timing attack");
        }
        else
        {
            Response = TEXT("Match expansion while maintaining safety");
        }
    }
    else
    {
        Response = TEXT("Maintain current strategy and observe");
    }

    return Response;
}

float UStrategicCognitionBridge::EvaluateInitiativeOpportunity(const TArray<float>& GameState)
{
    float OpportunityScore = 0.0f;

    if (GameState.Num() >= 4)
    {
        // Factors favoring initiative
        OpportunityScore += GameState[0] * 0.3f; // Resource advantage
        OpportunityScore += (1.0f - GameState[1]) * 0.2f; // Opponent vulnerability
        OpportunityScore += GameState[2] * 0.2f; // Position advantage
        OpportunityScore += GameState[3] * 0.3f; // Timing window

        // Adjust based on initiative-taking tendency
        OpportunityScore *= (0.5f + EnactedStrategy.InitiativeTaking * 0.5f);
    }

    return FMath::Clamp(OpportunityScore, 0.0f, 1.0f);
}

// ========================================
// EXTENDED COGNITION API
// ========================================

void UStrategicCognitionBridge::UpdateExtendedCognition(float InputDevicePerformance, float UIUsage, float TeamCommQuality)
{
    ExtendedCognition.InputDeviceProficiency = ExtendedCognition.InputDeviceProficiency * 0.95f + InputDevicePerformance * 0.05f;
    ExtendedCognition.UIUtilizationEfficiency = ExtendedCognition.UIUtilizationEfficiency * 0.95f + UIUsage * 0.05f;
    ExtendedCognition.TeamCommunicationEffectiveness = ExtendedCognition.TeamCommunicationEffectiveness * 0.9f + TeamCommQuality * 0.1f;
}

void UStrategicCognitionBridge::RecordToolUsage(const FString& ToolType, float EffectivenessScore)
{
    if (!ToolUsageHistory.Contains(ToolType))
    {
        ToolUsageHistory.Add(ToolType, TArray<float>());
    }

    ToolUsageHistory[ToolType].Add(EffectivenessScore);

    // Keep last 50 uses
    if (ToolUsageHistory[ToolType].Num() > 50)
    {
        ToolUsageHistory[ToolType].RemoveAt(0);
    }

    // Update external tool usage metric
    float TotalEffectiveness = 0.0f;
    int32 ToolCount = 0;
    for (const auto& Pair : ToolUsageHistory)
    {
        float ToolAvg = 0.0f;
        for (float Score : Pair.Value)
        {
            ToolAvg += Score;
        }
        if (Pair.Value.Num() > 0)
        {
            TotalEffectiveness += ToolAvg / Pair.Value.Num();
            ToolCount++;
        }
    }

    ExtendedCognition.ExternalToolUsage = ToolCount > 0 ? TotalEffectiveness / ToolCount : 0.0f;
}

FString UStrategicCognitionBridge::GetTeamCoordinationRecommendation(const TArray<FString>& TeamRoles, const TArray<float>& TeamState)
{
    if (TeamRoles.Num() == 0)
    {
        return TEXT("No team coordination needed for solo play");
    }

    if (ExtendedCognition.TeamCommunicationEffectiveness < 0.4f)
    {
        return TEXT("Focus on improving communication clarity and frequency");
    }

    // Analyze team state for coordination opportunities
    float AvgTeamState = 0.0f;
    for (float State : TeamState)
    {
        AvgTeamState += State;
    }
    AvgTeamState /= FMath::Max(1, TeamState.Num());

    if (AvgTeamState > 0.7f)
    {
        return TEXT("Team is strong - coordinate for decisive team play");
    }
    else if (AvgTeamState < 0.3f)
    {
        return TEXT("Team is struggling - focus on supporting weakest members");
    }

    return TEXT("Maintain current coordination and adapt to opportunities");
}

void UStrategicCognitionBridge::IntegrateCommunityKnowledge(const FString& KnowledgeType, const FString& Content)
{
    // Track community knowledge integration
    ExtendedCognition.CommunityKnowledgeIntegration = FMath::Min(1.0f, ExtendedCognition.CommunityKnowledgeIntegration + 0.05f);

    // In full implementation, this would update strategy databases
    UE_LOG(LogTemp, Log, TEXT("Integrated community knowledge: %s - %s"), *KnowledgeType, *Content);
}

TArray<FString> UStrategicCognitionBridge::GetRecommendedExternalResources() const
{
    TArray<FString> Resources;

    if (ExtendedCognition.ReplayAnalysisUtilization < 0.3f)
    {
        Resources.Add(TEXT("Replay analysis - review recent games for improvement areas"));
    }

    if (ExtendedCognition.CommunityKnowledgeIntegration < 0.5f)
    {
        Resources.Add(TEXT("Community guides - study current meta strategies"));
    }

    if (ExtendedCognition.CoachingIntegration < 0.2f)
    {
        Resources.Add(TEXT("Coaching resources - consider structured learning"));
    }

    if (ExtendedCognition.ExternalToolUsage < 0.4f)
    {
        Resources.Add(TEXT("Performance tracking tools - monitor improvement metrics"));
    }

    return Resources;
}

// ========================================
// COGNITIVE LOAD MANAGEMENT
// ========================================

void UStrategicCognitionBridge::UpdateCognitiveLoad(ECognitiveLoadType LoadType, float LoadLevel)
{
    CognitiveLoad.LoadLevels[LoadType] = FMath::Clamp(LoadLevel, 0.0f, 1.0f);

    // Recalculate total load (weighted sum)
    float TotalLoad = 0.0f;
    TotalLoad += CognitiveLoad.LoadLevels[ECognitiveLoadType::Perceptual] * 0.2f;
    TotalLoad += CognitiveLoad.LoadLevels[ECognitiveLoadType::WorkingMemory] * 0.25f;
    TotalLoad += CognitiveLoad.LoadLevels[ECognitiveLoadType::DecisionMaking] * 0.25f;
    TotalLoad += CognitiveLoad.LoadLevels[ECognitiveLoadType::MotorExecution] * 0.1f;
    TotalLoad += CognitiveLoad.LoadLevels[ECognitiveLoadType::TimePressure] * 0.1f;
    TotalLoad += CognitiveLoad.LoadLevels[ECognitiveLoadType::SocialCoordination] * 0.1f;

    CognitiveLoad.TotalLoad = TotalLoad;

    // Reset time at current load if significant change
    if (FMath::Abs(TotalLoad - CognitiveLoad.TotalLoad) > 0.1f)
    {
        CognitiveLoad.TimeAtCurrentLoad = 0.0f;
    }
}

float UStrategicCognitionBridge::GetTotalCognitiveLoad() const
{
    return CognitiveLoad.TotalLoad;
}

bool UStrategicCognitionBridge::IsCognitiveOverloadImminent() const
{
    // Check if approaching overload threshold
    if (CognitiveLoad.TotalLoad > CognitiveLoad.OverloadThreshold)
    {
        return true;
    }

    // Check if sustained high load
    if (CognitiveLoad.TotalLoad > 0.6f && CognitiveLoad.TimeAtCurrentLoad > 300.0f) // 5 minutes
    {
        return true;
    }

    // Check if cognitive reserve is depleted
    if (CognitiveLoad.CognitiveReserve < 0.2f)
    {
        return true;
    }

    return false;
}

TArray<FString> UStrategicCognitionBridge::GetLoadReductionRecommendations() const
{
    TArray<FString> Recommendations;

    // Find highest load types
    ECognitiveLoadType HighestType = ECognitiveLoadType::Perceptual;
    float HighestLoad = 0.0f;

    for (const auto& Pair : CognitiveLoad.LoadLevels)
    {
        if (Pair.Value > HighestLoad)
        {
            HighestLoad = Pair.Value;
            HighestType = Pair.Key;
        }
    }

    // Provide specific recommendations
    switch (HighestType)
    {
        case ECognitiveLoadType::Perceptual:
            Recommendations.Add(TEXT("Reduce visual clutter - focus on key information"));
            Recommendations.Add(TEXT("Use audio cues to supplement visual processing"));
            break;
        case ECognitiveLoadType::WorkingMemory:
            Recommendations.Add(TEXT("Simplify decision trees - use heuristics"));
            Recommendations.Add(TEXT("Externalize information to UI elements"));
            break;
        case ECognitiveLoadType::DecisionMaking:
            Recommendations.Add(TEXT("Pre-plan responses to common situations"));
            Recommendations.Add(TEXT("Reduce decision points by automating routine choices"));
            break;
        case ECognitiveLoadType::MotorExecution:
            Recommendations.Add(TEXT("Simplify input sequences where possible"));
            Recommendations.Add(TEXT("Take micro-breaks to prevent motor fatigue"));
            break;
        case ECognitiveLoadType::TimePressure:
            Recommendations.Add(TEXT("Practice time management in low-stakes situations"));
            Recommendations.Add(TEXT("Develop faster pattern recognition for common scenarios"));
            break;
        case ECognitiveLoadType::SocialCoordination:
            Recommendations.Add(TEXT("Establish clear communication protocols"));
            Recommendations.Add(TEXT("Assign specific coordination responsibilities"));
            break;
    }

    // General recommendations
    if (CognitiveLoad.TimeAtCurrentLoad > 600.0f) // 10 minutes
    {
        Recommendations.Add(TEXT("Consider taking a short break to restore cognitive resources"));
    }

    return Recommendations;
}

void UStrategicCognitionBridge::AllocateCognitiveResources(const TMap<ECognitiveLoadType, float>& Allocation)
{
    // Validate allocation sums to 1.0
    float TotalAllocation = 0.0f;
    for (const auto& Pair : Allocation)
    {
        TotalAllocation += Pair.Value;
    }

    if (FMath::Abs(TotalAllocation - 1.0f) > 0.01f)
    {
        UE_LOG(LogTemp, Warning, TEXT("Cognitive resource allocation does not sum to 1.0"));
        return;
    }

    // Apply allocation (affects load thresholds)
    for (const auto& Pair : Allocation)
    {
        // Higher allocation = higher tolerance for that load type
        // This is a simplified model
        float CurrentLoad = CognitiveLoad.LoadLevels[Pair.Key];
        float EffectiveLoad = CurrentLoad / (0.5f + Pair.Value);
        CognitiveLoad.LoadLevels[Pair.Key] = FMath::Clamp(EffectiveLoad, 0.0f, 1.0f);
    }
}

// ========================================
// ATTENTION MANAGEMENT
// ========================================

void UStrategicCognitionBridge::SetPrimaryFocus(const FString& FocusTarget)
{
    if (AttentionState.PrimaryFocus != FocusTarget)
    {
        // Calculate switching cost
        float Cost = CalculateAttentionSwitchingCost(AttentionState.PrimaryFocus, FocusTarget);
        AttentionState.SwitchingCost = Cost;

        // Reset sustained attention
        AttentionState.SustainedAttentionDuration = 0.0f;

        AttentionState.PrimaryFocus = FocusTarget;
    }
}

void UStrategicCognitionBridge::AddSecondaryTarget(const FString& Target)
{
    if (!AttentionState.SecondaryTargets.Contains(Target))
    {
        AttentionState.SecondaryTargets.Add(Target);

        // Adjust attention split
        AttentionState.AttentionSplitRatio = DetermineOptimalAttentionSplit(AttentionState.SecondaryTargets.Num() + 1);
    }
}

TMap<FString, float> UStrategicCognitionBridge::GetAttentionAllocationRecommendation(const TArray<FString>& PotentialTargets)
{
    TMap<FString, float> Allocation;

    if (PotentialTargets.Num() == 0)
    {
        return Allocation;
    }

    // Simple priority-based allocation
    // In full implementation, this would use ML to determine optimal allocation

    float PrimaryShare = 0.6f;
    float SecondaryShare = 0.4f / FMath::Max(1, PotentialTargets.Num() - 1);

    for (int32 i = 0; i < PotentialTargets.Num(); i++)
    {
        if (i == 0)
        {
            Allocation.Add(PotentialTargets[i], PrimaryShare);
        }
        else
        {
            Allocation.Add(PotentialTargets[i], SecondaryShare);
        }
    }

    return Allocation;
}

float UStrategicCognitionBridge::CalculateAttentionSwitchingCost(const FString& CurrentFocus, const FString& NewFocus) const
{
    // Base switching cost
    float Cost = 0.2f;

    // Higher cost for dissimilar focuses
    if (CurrentFocus.Contains(TEXT("enemy")) && NewFocus.Contains(TEXT("resource")))
    {
        Cost += 0.2f;
    }
    else if (CurrentFocus.Contains(TEXT("micro")) && NewFocus.Contains(TEXT("macro")))
    {
        Cost += 0.3f;
    }

    // Lower cost with practice
    Cost *= (1.0f - EmbodiedState.MuscleMemoryActivation * 0.3f);

    return FMath::Clamp(Cost, 0.1f, 0.5f);
}

// ========================================
// HOLISTIC INTEGRATION
// ========================================

TMap<FString, float> UStrategicCognitionBridge::GetIntegratedPerformanceAssessment() const
{
    TMap<FString, float> Assessment;

    // Embodied factors
    Assessment.Add(TEXT("Embodied_MotorReadiness"), EmbodiedState.MotorReadiness);
    Assessment.Add(TEXT("Embodied_Coordination"), EmbodiedState.HandEyeCoordination);
    Assessment.Add(TEXT("Embodied_MuscleMemory"), EmbodiedState.MuscleMemoryActivation);
    Assessment.Add(TEXT("Embodied_Fatigue"), 1.0f - EmbodiedState.PhysicalFatigue);

    // Embedded factors
    Assessment.Add(TEXT("Embedded_MapFamiliarity"), EmbeddedContext.MapFamiliarity);
    Assessment.Add(TEXT("Embedded_ThreatAwareness"), EmbeddedContext.ThreatAssessmentAccuracy);
    Assessment.Add(TEXT("Embedded_OpportunityRecognition"), EmbeddedContext.OpportunityRecognitionSpeed);

    // Enacted factors
    Assessment.Add(TEXT("Enacted_Adaptation"), EnactedStrategy.AdaptationRate);
    Assessment.Add(TEXT("Enacted_CounterPlay"), EnactedStrategy.CounterPlayEffectiveness);
    Assessment.Add(TEXT("Enacted_Initiative"), EnactedStrategy.InitiativeTaking);

    // Extended factors
    Assessment.Add(TEXT("Extended_ToolProficiency"), ExtendedCognition.InputDeviceProficiency);
    Assessment.Add(TEXT("Extended_TeamComm"), ExtendedCognition.TeamCommunicationEffectiveness);
    Assessment.Add(TEXT("Extended_CommunityKnowledge"), ExtendedCognition.CommunityKnowledgeIntegration);

    // Cognitive factors
    Assessment.Add(TEXT("Cognitive_Reserve"), CognitiveLoad.CognitiveReserve);
    Assessment.Add(TEXT("Cognitive_Load"), 1.0f - CognitiveLoad.TotalLoad);

    // Overall 4E integration
    Assessment.Add(TEXT("Overall_4E_Integration"), Calculate4EIntegrationScore());

    return Assessment;
}

TArray<FString> UStrategicCognitionBridge::GetHolisticImprovementRecommendations() const
{
    TArray<FString> Recommendations;

    // Find weakest areas across all 4Es
    float EmbodiedScore = GetEmbodiedPerformanceMultiplier();
    float EmbeddedScore = (EmbeddedContext.MapFamiliarity + EmbeddedContext.ThreatAssessmentAccuracy + EmbeddedContext.OpportunityRecognitionSpeed) / 3.0f;
    float EnactedScore = (EnactedStrategy.AdaptationRate + EnactedStrategy.CounterPlayEffectiveness + EnactedStrategy.InitiativeTaking) / 3.0f;
    float ExtendedScore = (ExtendedCognition.InputDeviceProficiency + ExtendedCognition.TeamCommunicationEffectiveness + ExtendedCognition.CommunityKnowledgeIntegration) / 3.0f;

    // Recommend based on weakest area
    float MinScore = FMath::Min(FMath::Min(EmbodiedScore, EmbeddedScore), FMath::Min(EnactedScore, ExtendedScore));

    if (MinScore == EmbodiedScore)
    {
        Recommendations.Add(TEXT("PRIORITY: Embodied - Focus on mechanical skill drills and reaction time training"));
    }
    else if (MinScore == EmbeddedScore)
    {
        Recommendations.Add(TEXT("PRIORITY: Embedded - Study maps, learn common strategies, improve game sense"));
    }
    else if (MinScore == EnactedScore)
    {
        Recommendations.Add(TEXT("PRIORITY: Enacted - Practice adaptive play, work on reading opponents"));
    }
    else
    {
        Recommendations.Add(TEXT("PRIORITY: Extended - Improve tool usage, communication, and community engagement"));
    }

    // Add cognitive management recommendation if needed
    if (IsCognitiveOverloadImminent())
    {
        Recommendations.Add(TEXT("URGENT: Cognitive load is high - implement load reduction strategies"));
    }

    return Recommendations;
}

float UStrategicCognitionBridge::Calculate4EIntegrationScore() const
{
    return Integrate4EFactors();
}

FString UStrategicCognitionBridge::GetPersonalizedTrainingFocus() const
{
    TMap<FString, float> Assessment = GetIntegratedPerformanceAssessment();

    // Find lowest scoring area
    FString LowestArea;
    float LowestScore = 1.0f;

    for (const auto& Pair : Assessment)
    {
        if (Pair.Value < LowestScore && !Pair.Key.Contains(TEXT("Overall")))
        {
            LowestScore = Pair.Value;
            LowestArea = Pair.Key;
        }
    }

    // Generate personalized focus
    if (LowestArea.Contains(TEXT("Embodied")))
    {
        return FString::Printf(TEXT("Focus on %s through targeted mechanical practice"), *LowestArea);
    }
    else if (LowestArea.Contains(TEXT("Embedded")))
    {
        return FString::Printf(TEXT("Improve %s through map study and game analysis"), *LowestArea);
    }
    else if (LowestArea.Contains(TEXT("Enacted")))
    {
        return FString::Printf(TEXT("Develop %s through varied practice scenarios"), *LowestArea);
    }
    else if (LowestArea.Contains(TEXT("Extended")))
    {
        return FString::Printf(TEXT("Enhance %s through tool training and community engagement"), *LowestArea);
    }

    return TEXT("Maintain balanced practice across all areas");
}

// ========================================
// INTERNAL METHODS
// ========================================

float UStrategicCognitionBridge::CalculateMuscleMemoryStrength(const FString& ActionType) const
{
    if (!MotorExecutionHistory.Contains(ActionType))
    {
        return 0.0f;
    }

    const TArray<float>& History = MotorExecutionHistory[ActionType];

    if (History.Num() < 10)
    {
        return 0.0f; // Not enough data
    }

    // Calculate consistency (low variance = strong muscle memory)
    float Mean = 0.0f;
    for (float Value : History)
    {
        Mean += Value;
    }
    Mean /= History.Num();

    float Variance = 0.0f;
    for (float Value : History)
    {
        Variance += FMath::Square(Value - Mean);
    }
    Variance /= History.Num();

    // High mean + low variance = strong muscle memory
    float Consistency = 1.0f - FMath::Sqrt(Variance);
    float Strength = Mean * Consistency;

    // Bonus for volume
    float VolumeBonus = FMath::Min(0.2f, History.Num() / 500.0f);

    return FMath::Clamp(Strength + VolumeBonus, 0.0f, 1.0f);
}

void UStrategicCognitionBridge::UpdateCognitiveReserve(float DeltaTime)
{
    // Deplete reserve under load
    float DepletionRate = CognitiveLoad.TotalLoad * 0.01f;
    CognitiveLoad.CognitiveReserve -= DepletionRate * DeltaTime;

    // Recover reserve when load is low
    if (CognitiveLoad.TotalLoad < 0.3f)
    {
        float RecoveryRate = 0.02f * (1.0f - CognitiveLoad.TotalLoad);
        CognitiveLoad.CognitiveReserve += RecoveryRate * DeltaTime;
    }

    CognitiveLoad.CognitiveReserve = FMath::Clamp(CognitiveLoad.CognitiveReserve, 0.0f, 1.0f);
}

float UStrategicCognitionBridge::CalculateStrategyEffectiveness(const FString& Strategy) const
{
    if (!StrategyEffectivenessHistory.Contains(Strategy))
    {
        return 0.5f; // Unknown effectiveness
    }

    const TArray<bool>& History = StrategyEffectivenessHistory[Strategy];

    if (History.Num() == 0)
    {
        return 0.5f;
    }

    int32 Successes = 0;
    for (bool Success : History)
    {
        if (Success) Successes++;
    }

    return (float)Successes / History.Num();
}

float UStrategicCognitionBridge::DetermineOptimalAttentionSplit(int32 TargetCount) const
{
    // Optimal split based on number of targets
    // More targets = less attention per target
    if (TargetCount <= 1)
    {
        return 1.0f;
    }
    else if (TargetCount == 2)
    {
        return 0.7f; // 70/30 split
    }
    else if (TargetCount == 3)
    {
        return 0.5f; // 50/25/25 split
    }
    else
    {
        return 0.4f; // Primary gets 40%, rest split evenly
    }
}

float UStrategicCognitionBridge::Integrate4EFactors() const
{
    // Calculate integrated 4E score
    float EmbodiedScore = GetEmbodiedPerformanceMultiplier() / 1.5f; // Normalize from 0.5-1.5 to 0-1
    float EmbeddedScore = (EmbeddedContext.MapFamiliarity + EmbeddedContext.ThreatAssessmentAccuracy + 
                          EmbeddedContext.OpportunityRecognitionSpeed + EmbeddedContext.MetaAwareness) / 4.0f;
    float EnactedScore = (EnactedStrategy.AdaptationRate + EnactedStrategy.CounterPlayEffectiveness + 
                         EnactedStrategy.InitiativeTaking + EnactedStrategy.ExplorationTendency) / 4.0f;
    float ExtendedScore = (ExtendedCognition.InputDeviceProficiency + ExtendedCognition.UIUtilizationEfficiency +
                          ExtendedCognition.TeamCommunicationEffectiveness + ExtendedCognition.CommunityKnowledgeIntegration) / 4.0f;

    // Weighted integration (all 4Es contribute equally)
    float IntegratedScore = (EmbodiedScore + EmbeddedScore + EnactedScore + ExtendedScore) / 4.0f;

    // Bonus for balance (all 4Es at similar levels)
    float Variance = 0.0f;
    float Mean = IntegratedScore;
    Variance += FMath::Square(EmbodiedScore - Mean);
    Variance += FMath::Square(EmbeddedScore - Mean);
    Variance += FMath::Square(EnactedScore - Mean);
    Variance += FMath::Square(ExtendedScore - Mean);
    Variance /= 4.0f;

    float BalanceBonus = (1.0f - FMath::Sqrt(Variance)) * 0.1f;

    return FMath::Clamp(IntegratedScore + BalanceBonus, 0.0f, 1.0f);
}

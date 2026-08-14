// Enhanced4EAvatarEmbodiment.cpp
// Deep Tree Echo - Enhanced 4E Avatar Embodiment Implementation
// Copyright (c) 2025 Deep Tree Echo Project

#include "Enhanced4EAvatarEmbodiment.h"
#include "../Core/DeepTreeEchoCore.h"
#include "../Core/CognitiveCycleManager.h"
#include "ExpressiveAnimationSystem.h"

UEnhanced4EAvatarEmbodiment::UEnhanced4EAvatarEmbodiment()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.016f; // ~60Hz
}

void UEnhanced4EAvatarEmbodiment::BeginPlay()
{
    Super::BeginPlay();
    InitializeEmbodiment();
}

void UEnhanced4EAvatarEmbodiment::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    AccumulatedTime += DeltaTime;

    // Update expression at configured interval
    if (bEnableAutoExpression && AccumulatedTime >= ExpressionUpdateInterval)
    {
        // Update from various sources
        UpdateExpressionFrom4ECognition();
        
        if (bEnableSomaticMarkerExpression)
        {
            UpdateExpressionFromSomaticMarkers();
        }
        
        if (bEnableAffordanceDrivenAttention)
        {
            UpdateAttentionFromAffordances();
        }
        
        if (bEnableNeuroSymbolicModulation)
        {
            UpdateExpressionFromNeuroSymbolic();
        }
        
        // Update cognitive-motor coupling
        UpdateCognitiveMotorCoupling();
        
        AccumulatedTime = 0.0f;
    }

    // Always smooth expression
    SmoothExpression(DeltaTime);
    
    // Apply to animation system
    ApplyExpressionToAnimation();
    
    // Record history periodically
    static float HistoryAccumulator = 0.0f;
    HistoryAccumulator += DeltaTime;
    if (HistoryAccumulator >= 0.1f)
    {
        RecordExpressionToHistory();
        HistoryAccumulator = 0.0f;
    }
}

void UEnhanced4EAvatarEmbodiment::InitializeEmbodiment()
{
    DiscoverSubsystems();
    ResetToDefault();
    
    UE_LOG(LogTemp, Log, TEXT("Enhanced4EAvatarEmbodiment: Initialized"));
}

void UEnhanced4EAvatarEmbodiment::DiscoverSubsystems()
{
    if (AActor* Owner = GetOwner())
    {
        EmbodiedCognition = Owner->FindComponentByClass<UEmbodiedCognitionComponent>();
        NeuroSymbolicBridge = Owner->FindComponentByClass<UNeuroSymbolicBridge>();
        DeepTreeEchoCore = Owner->FindComponentByClass<UDeepTreeEchoCore>();
        CognitiveCycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
        ExpressiveAnimationSystem = Owner->FindComponentByClass<UExpressiveAnimationSystem>();
        
        UE_LOG(LogTemp, Log, TEXT("Enhanced4EAvatarEmbodiment: Discovered - 4E:%s, Bridge:%s, Core:%s, Cycle:%s, Anim:%s"),
            EmbodiedCognition ? TEXT("Yes") : TEXT("No"),
            NeuroSymbolicBridge ? TEXT("Yes") : TEXT("No"),
            DeepTreeEchoCore ? TEXT("Yes") : TEXT("No"),
            CognitiveCycleManager ? TEXT("Yes") : TEXT("No"),
            ExpressiveAnimationSystem ? TEXT("Yes") : TEXT("No"));
    }
}

void UEnhanced4EAvatarEmbodiment::ResetToDefault()
{
    CurrentExpression = FAvatarExpressionState();
    TargetExpression = FAvatarExpressionState();
    CurrentIntention = FEmbodiedActionIntention();
    CouplingState = FCognitiveMotorCoupling();
    ExpressionHistory.Empty();
}

// ========================================
// EXPRESSION API
// ========================================

FAvatarExpressionState UEnhanced4EAvatarEmbodiment::GetCurrentExpression() const
{
    return CurrentExpression;
}

void UEnhanced4EAvatarEmbodiment::SetTargetExpression(const FAvatarExpressionState& Expression)
{
    TargetExpression = Expression;
}

void UEnhanced4EAvatarEmbodiment::SetEmotion(const FString& Emotion, float Intensity)
{
    TargetExpression.PrimaryEmotion = Emotion;
    TargetExpression.EmotionIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    
    // Update aura color based on emotion
    TargetExpression.AuraColor = CalculateAuraColorFromEmotion(Emotion, Intensity);
}

void UEnhanced4EAvatarEmbodiment::SetArousalValence(float Arousal, float Valence)
{
    TargetExpression.Arousal = FMath::Clamp(Arousal, 0.0f, 1.0f);
    TargetExpression.Valence = FMath::Clamp(Valence, -1.0f, 1.0f);
    
    // Derive emotion from arousal/valence
    TargetExpression.PrimaryEmotion = CalculateEmotionFromArousalValence(Arousal, Valence);
    
    // Update related expression parameters
    TargetExpression.MovementEnergy = Arousal;
    TargetExpression.BreathingRateModifier = 0.5f + Arousal;
    TargetExpression.BioluminescenceIntensity = 0.2f + 0.6f * FMath::Abs(Valence);
}

void UEnhanced4EAvatarEmbodiment::SetAttentionFocus(const FVector& Direction, float Intensity)
{
    TargetExpression.AttentionDirection = Direction.GetSafeNormal();
    TargetExpression.AttentionIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    TargetExpression.EyeGlowIntensity = 0.3f + 0.7f * Intensity;
}

void UEnhanced4EAvatarEmbodiment::SetAuraColor(const FLinearColor& Color)
{
    TargetExpression.AuraColor = Color;
}

// ========================================
// ACTION API
// ========================================

void UEnhanced4EAvatarEmbodiment::SetActionIntention(const FString& ActionType, const FVector& TargetLocation, float Strength)
{
    CurrentIntention.ActionType = ActionType;
    CurrentIntention.TargetLocation = TargetLocation;
    CurrentIntention.TargetActor = nullptr;
    CurrentIntention.IntentionStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
    
    // Update expression based on intention
    TargetExpression.PostureOpenness = 0.3f + 0.5f * Strength;
    TargetExpression.MovementEnergy = FMath::Max(TargetExpression.MovementEnergy, Strength * 0.7f);
    
    OnActionIntentionSet.Broadcast(CurrentIntention);
}

void UEnhanced4EAvatarEmbodiment::SetActionIntentionWithTarget(const FString& ActionType, AActor* TargetActor, float Strength)
{
    CurrentIntention.ActionType = ActionType;
    CurrentIntention.TargetActor = TargetActor;
    CurrentIntention.IntentionStrength = FMath::Clamp(Strength, 0.0f, 1.0f);
    
    if (TargetActor)
    {
        CurrentIntention.TargetLocation = TargetActor->GetActorLocation();
        
        // Set attention toward target
        if (AActor* Owner = GetOwner())
        {
            FVector Direction = (TargetActor->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal();
            SetAttentionFocus(Direction, Strength);
        }
    }
    
    OnActionIntentionSet.Broadcast(CurrentIntention);
}

void UEnhanced4EAvatarEmbodiment::ClearActionIntention()
{
    CurrentIntention = FEmbodiedActionIntention();
}

float UEnhanced4EAvatarEmbodiment::GetMotorReadinessForAction(const FString& ActionType) const
{
    // Base readiness from coupling state
    float Readiness = CouplingState.MotorReadiness;
    
    // Modify based on current intention alignment
    if (CurrentIntention.ActionType == ActionType)
    {
        Readiness += 0.2f * CurrentIntention.IntentionStrength;
    }
    
    // Reduce readiness under high cognitive load
    Readiness -= 0.3f * CouplingState.CognitiveLoad;
    
    return FMath::Clamp(Readiness, 0.0f, 1.0f);
}

// ========================================
// 4E INTEGRATION
// ========================================

void UEnhanced4EAvatarEmbodiment::UpdateExpressionFrom4ECognition()
{
    if (!EmbodiedCognition)
    {
        return;
    }
    
    // Get 4E scores
    float EmbodiedScore = EmbodiedCognition->GetEmbodiedScore();
    float EmbeddedScore = EmbodiedCognition->GetEmbeddedScore();
    float EnactedScore = EmbodiedCognition->GetEnactedScore();
    float ExtendedScore = EmbodiedCognition->GetExtendedScore();
    
    // Embodied: Affects posture and body awareness
    TargetExpression.PostureOpenness = FMath::Lerp(TargetExpression.PostureOpenness, EmbodiedScore, 0.3f);
    
    // Embedded: Affects environmental awareness and attention spread
    float EnvironmentalAwareness = EmbeddedScore;
    TargetExpression.TechPatternComplexity = FMath::Lerp(TargetExpression.TechPatternComplexity, EnvironmentalAwareness, 0.2f);
    
    // Enacted: Affects movement readiness and energy
    TargetExpression.MovementEnergy = FMath::Lerp(TargetExpression.MovementEnergy, EnactedScore, 0.3f);
    
    // Extended: Affects bioluminescence (cognitive extension visualization)
    TargetExpression.BioluminescenceIntensity = FMath::Lerp(TargetExpression.BioluminescenceIntensity, 0.2f + 0.6f * ExtendedScore, 0.2f);
}

void UEnhanced4EAvatarEmbodiment::UpdateExpressionFromSomaticMarkers()
{
    if (!EmbodiedCognition)
    {
        return;
    }
    
    // Get active somatic markers
    const TArray<FSomaticMarker>& Markers = EmbodiedCognition->ActiveSomaticMarkers;
    
    if (Markers.Num() == 0)
    {
        return;
    }
    
    // Aggregate somatic marker influence
    float TotalValence = 0.0f;
    float TotalArousal = 0.0f;
    float TotalStrength = 0.0f;
    
    for (const FSomaticMarker& Marker : Markers)
    {
        TotalValence += Marker.Valence * Marker.Strength;
        TotalArousal += Marker.Arousal * Marker.Strength;
        TotalStrength += Marker.Strength;
    }
    
    if (TotalStrength > 0.0f)
    {
        float AvgValence = TotalValence / TotalStrength;
        float AvgArousal = TotalArousal / TotalStrength;
        
        // Blend with current target
        TargetExpression.Valence = FMath::Lerp(TargetExpression.Valence, AvgValence, 0.4f);
        TargetExpression.Arousal = FMath::Lerp(TargetExpression.Arousal, AvgArousal, 0.4f);
        
        // Update derived emotion
        TargetExpression.PrimaryEmotion = CalculateEmotionFromArousalValence(TargetExpression.Arousal, TargetExpression.Valence);
    }
}

void UEnhanced4EAvatarEmbodiment::UpdateAttentionFromAffordances()
{
    if (!EmbodiedCognition)
    {
        return;
    }
    
    // Get most salient affordance
    FEnvironmentalAffordance MostSalient = EmbodiedCognition->GetMostSalientAffordance();
    
    if (MostSalient.Salience > 0.3f)
    {
        // Calculate direction to affordance
        if (AActor* Owner = GetOwner())
        {
            FVector Direction = (MostSalient.Location - Owner->GetActorLocation()).GetSafeNormal();
            
            // Blend attention toward affordance
            FVector CurrentDir = TargetExpression.AttentionDirection;
            FVector NewDir = FMath::Lerp(CurrentDir, Direction, MostSalient.Salience * 0.5f).GetSafeNormal();
            
            TargetExpression.AttentionDirection = NewDir;
            TargetExpression.AttentionIntensity = FMath::Max(TargetExpression.AttentionIntensity, MostSalient.Salience);
        }
    }
}

void UEnhanced4EAvatarEmbodiment::UpdateExpressionFromNeuroSymbolic()
{
    if (!NeuroSymbolicBridge)
    {
        return;
    }
    
    // Get neuro-symbolic metrics
    FNeuroSymbolicMetrics Metrics = NeuroSymbolicBridge->GetMetrics();
    
    // Integration coherence affects overall expression coherence
    float Coherence = Metrics.IntegrationCoherence;
    
    // High coherence = stable, confident expression
    // Low coherence = uncertain, searching expression
    if (Coherence > 0.7f)
    {
        // Confident state
        TargetExpression.EyeGlowIntensity = FMath::Lerp(TargetExpression.EyeGlowIntensity, 0.7f, 0.2f);
        TargetExpression.PostureOpenness = FMath::Lerp(TargetExpression.PostureOpenness, 0.7f, 0.2f);
    }
    else if (Coherence < 0.3f)
    {
        // Uncertain state
        TargetExpression.EyeGlowIntensity = FMath::Lerp(TargetExpression.EyeGlowIntensity, 0.3f, 0.2f);
        TargetExpression.TechPatternComplexity = FMath::Lerp(TargetExpression.TechPatternComplexity, 0.8f, 0.3f);
    }
    
    // Active bindings affect cognitive visualization
    float BindingActivity = (float)Metrics.ActiveBindings / FMath::Max(1, Metrics.TotalBindings);
    TargetExpression.BioluminescenceIntensity = FMath::Lerp(TargetExpression.BioluminescenceIntensity, 0.2f + 0.5f * BindingActivity, 0.2f);
}

float UEnhanced4EAvatarEmbodiment::Get4EEmbodimentScore() const
{
    if (!EmbodiedCognition)
    {
        return 0.0f;
    }
    
    return EmbodiedCognition->Get4EIntegrationScore();
}

// ========================================
// COGNITIVE-MOTOR COUPLING
// ========================================

void UEnhanced4EAvatarEmbodiment::UpdateCognitiveMotorCoupling()
{
    // Calculate coupling strength from expression coherence
    float ExpressionCoherence = 1.0f - CalculateExpressionDifference(CurrentExpression, TargetExpression);
    CouplingState.CouplingStrength = FMath::Lerp(CouplingState.CouplingStrength, ExpressionCoherence, 0.1f);
    
    // Update prediction-action alignment
    if (EmbodiedCognition)
    {
        float PredictionError = EmbodiedCognition->GetLastPredictionError();
        CouplingState.PredictionActionAlignment = FMath::Lerp(CouplingState.PredictionActionAlignment, 1.0f - PredictionError, 0.2f);
    }
    
    // Calculate embodiment coherence
    CouplingState.EmbodimentCoherence = (CouplingState.CouplingStrength + CouplingState.PredictionActionAlignment) / 2.0f;
}

FCognitiveMotorCoupling UEnhanced4EAvatarEmbodiment::GetCouplingState() const
{
    return CouplingState;
}

void UEnhanced4EAvatarEmbodiment::SetMotorReadiness(float Readiness)
{
    CouplingState.MotorReadiness = FMath::Clamp(Readiness, 0.0f, 1.0f);
}

void UEnhanced4EAvatarEmbodiment::SetCognitiveLoad(float Load)
{
    CouplingState.CognitiveLoad = FMath::Clamp(Load, 0.0f, 1.0f);
}

// ========================================
// DIAGNOSTICS
// ========================================

TArray<FString> UEnhanced4EAvatarEmbodiment::GenerateDiagnosticReport() const
{
    TArray<FString> Report;
    
    Report.Add(TEXT("=== Enhanced 4E Avatar Embodiment Report ==="));
    Report.Add(FString::Printf(TEXT("Primary Emotion: %s (%.2f)"), *CurrentExpression.PrimaryEmotion, CurrentExpression.EmotionIntensity));
    Report.Add(FString::Printf(TEXT("Arousal: %.2f, Valence: %.2f"), CurrentExpression.Arousal, CurrentExpression.Valence));
    Report.Add(FString::Printf(TEXT("Attention Intensity: %.2f"), CurrentExpression.AttentionIntensity));
    Report.Add(FString::Printf(TEXT("Posture Openness: %.2f"), CurrentExpression.PostureOpenness));
    Report.Add(FString::Printf(TEXT("Movement Energy: %.2f"), CurrentExpression.MovementEnergy));
    Report.Add(FString::Printf(TEXT("Bioluminescence: %.2f"), CurrentExpression.BioluminescenceIntensity));
    Report.Add(FString::Printf(TEXT("Eye Glow: %.2f"), CurrentExpression.EyeGlowIntensity));
    Report.Add(FString::Printf(TEXT("Tech Pattern Complexity: %.2f"), CurrentExpression.TechPatternComplexity));
    
    Report.Add(TEXT("--- Coupling State ---"));
    Report.Add(FString::Printf(TEXT("Coupling Strength: %.2f"), CouplingState.CouplingStrength));
    Report.Add(FString::Printf(TEXT("Motor Readiness: %.2f"), CouplingState.MotorReadiness));
    Report.Add(FString::Printf(TEXT("Cognitive Load: %.2f"), CouplingState.CognitiveLoad));
    Report.Add(FString::Printf(TEXT("Embodiment Coherence: %.2f"), CouplingState.EmbodimentCoherence));
    
    Report.Add(TEXT("--- Subsystems ---"));
    Report.Add(FString::Printf(TEXT("4E Cognition: %s"), EmbodiedCognition ? TEXT("Connected") : TEXT("Not Found")));
    Report.Add(FString::Printf(TEXT("Neuro-Symbolic Bridge: %s"), NeuroSymbolicBridge ? TEXT("Connected") : TEXT("Not Found")));
    Report.Add(FString::Printf(TEXT("Expression History: %d entries"), ExpressionHistory.Num()));
    
    return Report;
}

TArray<FAvatarExpressionState> UEnhanced4EAvatarEmbodiment::GetExpressionHistory(int32 Count) const
{
    TArray<FAvatarExpressionState> Result;
    int32 StartIndex = FMath::Max(0, ExpressionHistory.Num() - Count);
    
    for (int32 i = StartIndex; i < ExpressionHistory.Num(); i++)
    {
        Result.Add(ExpressionHistory[i]);
    }
    
    return Result;
}

// ========================================
// PRIVATE METHODS
// ========================================

void UEnhanced4EAvatarEmbodiment::SmoothExpression(float DeltaTime)
{
    float SmoothFactor = 1.0f - FMath::Pow(ExpressionSmoothing, DeltaTime * 60.0f);
    
    // Smooth all expression parameters
    CurrentExpression.EmotionIntensity = FMath::Lerp(CurrentExpression.EmotionIntensity, TargetExpression.EmotionIntensity, SmoothFactor);
    CurrentExpression.Arousal = FMath::Lerp(CurrentExpression.Arousal, TargetExpression.Arousal, SmoothFactor);
    CurrentExpression.Valence = FMath::Lerp(CurrentExpression.Valence, TargetExpression.Valence, SmoothFactor);
    CurrentExpression.AttentionDirection = FMath::Lerp(CurrentExpression.AttentionDirection, TargetExpression.AttentionDirection, SmoothFactor).GetSafeNormal();
    CurrentExpression.AttentionIntensity = FMath::Lerp(CurrentExpression.AttentionIntensity, TargetExpression.AttentionIntensity, SmoothFactor);
    CurrentExpression.PostureOpenness = FMath::Lerp(CurrentExpression.PostureOpenness, TargetExpression.PostureOpenness, SmoothFactor);
    CurrentExpression.MovementEnergy = FMath::Lerp(CurrentExpression.MovementEnergy, TargetExpression.MovementEnergy, SmoothFactor);
    CurrentExpression.BreathingRateModifier = FMath::Lerp(CurrentExpression.BreathingRateModifier, TargetExpression.BreathingRateModifier, SmoothFactor);
    CurrentExpression.BioluminescenceIntensity = FMath::Lerp(CurrentExpression.BioluminescenceIntensity, TargetExpression.BioluminescenceIntensity, SmoothFactor);
    CurrentExpression.AuraColor = FMath::Lerp(CurrentExpression.AuraColor, TargetExpression.AuraColor, SmoothFactor);
    CurrentExpression.EyeGlowIntensity = FMath::Lerp(CurrentExpression.EyeGlowIntensity, TargetExpression.EyeGlowIntensity, SmoothFactor);
    CurrentExpression.TechPatternComplexity = FMath::Lerp(CurrentExpression.TechPatternComplexity, TargetExpression.TechPatternComplexity, SmoothFactor);
    
    // Snap emotion if intensity is high enough
    if (TargetExpression.EmotionIntensity > 0.5f)
    {
        CurrentExpression.PrimaryEmotion = TargetExpression.PrimaryEmotion;
    }
    
    // Check for significant change
    if (HasExpressionChangedSignificantly())
    {
        OnExpressionChanged.Broadcast(CurrentExpression);
    }
}

void UEnhanced4EAvatarEmbodiment::ApplyExpressionToAnimation()
{
    if (!ExpressiveAnimationSystem)
    {
        return;
    }
    
    // Apply expression to animation system
    // This would call into the ExpressiveAnimationSystem API
    // Implementation depends on the animation system interface
}

FString UEnhanced4EAvatarEmbodiment::CalculateEmotionFromArousalValence(float Arousal, float Valence) const
{
    // Circumplex model of emotion
    if (Arousal > 0.6f)
    {
        if (Valence > 0.3f) return TEXT("joy");
        if (Valence < -0.3f) return TEXT("anger");
        return TEXT("surprise");
    }
    else if (Arousal < 0.4f)
    {
        if (Valence > 0.3f) return TEXT("contentment");
        if (Valence < -0.3f) return TEXT("sadness");
        return TEXT("neutral");
    }
    else
    {
        if (Valence > 0.3f) return TEXT("interest");
        if (Valence < -0.3f) return TEXT("fear");
        return TEXT("neutral");
    }
}

FLinearColor UEnhanced4EAvatarEmbodiment::CalculateAuraColorFromEmotion(const FString& Emotion, float Intensity) const
{
    FLinearColor BaseColor;
    
    if (Emotion == TEXT("joy"))
    {
        BaseColor = FLinearColor(1.0f, 0.9f, 0.2f, 1.0f); // Warm yellow
    }
    else if (Emotion == TEXT("sadness"))
    {
        BaseColor = FLinearColor(0.2f, 0.3f, 0.8f, 1.0f); // Deep blue
    }
    else if (Emotion == TEXT("anger"))
    {
        BaseColor = FLinearColor(0.9f, 0.2f, 0.1f, 1.0f); // Red
    }
    else if (Emotion == TEXT("fear"))
    {
        BaseColor = FLinearColor(0.5f, 0.2f, 0.6f, 1.0f); // Purple
    }
    else if (Emotion == TEXT("surprise"))
    {
        BaseColor = FLinearColor(1.0f, 0.6f, 0.0f, 1.0f); // Orange
    }
    else if (Emotion == TEXT("contentment"))
    {
        BaseColor = FLinearColor(0.3f, 0.8f, 0.5f, 1.0f); // Soft green
    }
    else if (Emotion == TEXT("interest"))
    {
        BaseColor = FLinearColor(0.0f, 0.8f, 1.0f, 1.0f); // Cyan
    }
    else
    {
        BaseColor = FLinearColor(0.5f, 0.5f, 0.6f, 1.0f); // Neutral gray-blue
    }
    
    // Modulate by intensity
    BaseColor.A = 0.3f + 0.5f * Intensity;
    
    return BaseColor;
}

void UEnhanced4EAvatarEmbodiment::RecordExpressionToHistory()
{
    ExpressionHistory.Add(CurrentExpression);
    
    // Trim history if too large
    while (ExpressionHistory.Num() > MaxHistorySize)
    {
        ExpressionHistory.RemoveAt(0);
    }
}

bool UEnhanced4EAvatarEmbodiment::HasExpressionChangedSignificantly() const
{
    if (ExpressionHistory.Num() == 0)
    {
        return true;
    }
    
    const FAvatarExpressionState& Previous = ExpressionHistory.Last();
    float Difference = CalculateExpressionDifference(Previous, CurrentExpression);
    
    return Difference > 0.1f;
}

float UEnhanced4EAvatarEmbodiment::CalculateExpressionDifference(const FAvatarExpressionState& A, const FAvatarExpressionState& B) const
{
    float Diff = 0.0f;
    
    Diff += FMath::Abs(A.EmotionIntensity - B.EmotionIntensity);
    Diff += FMath::Abs(A.Arousal - B.Arousal);
    Diff += FMath::Abs(A.Valence - B.Valence) / 2.0f; // Valence is -1 to 1
    Diff += FMath::Abs(A.AttentionIntensity - B.AttentionIntensity);
    Diff += FMath::Abs(A.PostureOpenness - B.PostureOpenness);
    Diff += FMath::Abs(A.MovementEnergy - B.MovementEnergy);
    Diff += FMath::Abs(A.BioluminescenceIntensity - B.BioluminescenceIntensity);
    Diff += FMath::Abs(A.EyeGlowIntensity - B.EyeGlowIntensity);
    
    // Normalize by number of parameters
    return Diff / 8.0f;
}

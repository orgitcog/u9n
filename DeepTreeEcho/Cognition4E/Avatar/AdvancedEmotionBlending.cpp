// AdvancedEmotionBlending.cpp
// Implementation of advanced emotion blending system

#include "AdvancedEmotionBlending.h"
#include "Math/UnrealMathUtility.h"

// FEmotionVector implementation
FEmotionVector FEmotionVector::operator+(const FEmotionVector& Other) const
{
    FEmotionVector Result;
    Result.Valence = Valence + Other.Valence;
    Result.Arousal = Arousal + Other.Arousal;
    Result.Dominance = Dominance + Other.Dominance;
    Result.Approach = Approach + Other.Approach;
    Result.Certainty = Certainty + Other.Certainty;
    Result.Novelty = Novelty + Other.Novelty;
    Result.Agency = Agency + Other.Agency;
    Result.Temporal = Temporal + Other.Temporal;
    Result.Intensity = Intensity + Other.Intensity;
    return Result;
}

FEmotionVector FEmotionVector::operator*(float Scalar) const
{
    FEmotionVector Result;
    Result.Valence = Valence * Scalar;
    Result.Arousal = Arousal * Scalar;
    Result.Dominance = Dominance * Scalar;
    Result.Approach = Approach * Scalar;
    Result.Certainty = Certainty * Scalar;
    Result.Novelty = Novelty * Scalar;
    Result.Agency = Agency * Scalar;
    Result.Temporal = Temporal * Scalar;
    Result.Intensity = Intensity * Scalar;
    return Result;
}

FEmotionVector FEmotionVector::Lerp(const FEmotionVector& A, const FEmotionVector& B, float Alpha)
{
    FEmotionVector Result;
    Result.Valence = FMath::Lerp(A.Valence, B.Valence, Alpha);
    Result.Arousal = FMath::Lerp(A.Arousal, B.Arousal, Alpha);
    Result.Dominance = FMath::Lerp(A.Dominance, B.Dominance, Alpha);
    Result.Approach = FMath::Lerp(A.Approach, B.Approach, Alpha);
    Result.Certainty = FMath::Lerp(A.Certainty, B.Certainty, Alpha);
    Result.Novelty = FMath::Lerp(A.Novelty, B.Novelty, Alpha);
    Result.Agency = FMath::Lerp(A.Agency, B.Agency, Alpha);
    Result.Temporal = FMath::Lerp(A.Temporal, B.Temporal, Alpha);
    Result.Intensity = FMath::Lerp(A.Intensity, B.Intensity, Alpha);
    return Result;
}

float FEmotionVector::DistanceTo(const FEmotionVector& Other) const
{
    float Sum = 0.0f;
    Sum += FMath::Square(Valence - Other.Valence);
    Sum += FMath::Square(Arousal - Other.Arousal);
    Sum += FMath::Square(Dominance - Other.Dominance);
    Sum += FMath::Square(Approach - Other.Approach);
    Sum += FMath::Square(Certainty - Other.Certainty);
    Sum += FMath::Square(Novelty - Other.Novelty);
    Sum += FMath::Square(Agency - Other.Agency);
    Sum += FMath::Square(Temporal - Other.Temporal);
    return FMath::Sqrt(Sum);
}

void FEmotionVector::Normalize()
{
    Valence = FMath::Clamp(Valence, -1.0f, 1.0f);
    Arousal = FMath::Clamp(Arousal, 0.0f, 1.0f);
    Dominance = FMath::Clamp(Dominance, -1.0f, 1.0f);
    Approach = FMath::Clamp(Approach, -1.0f, 1.0f);
    Certainty = FMath::Clamp(Certainty, 0.0f, 1.0f);
    Novelty = FMath::Clamp(Novelty, 0.0f, 1.0f);
    Agency = FMath::Clamp(Agency, -1.0f, 1.0f);
    Temporal = FMath::Clamp(Temporal, -1.0f, 1.0f);
    Intensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
}

// UAdvancedEmotionBlending implementation
UAdvancedEmotionBlending::UAdvancedEmotionBlending()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UAdvancedEmotionBlending::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeDefaultRules();

    // Initialize state
    State.CurrentEmotion.Valence = 0.0f;
    State.CurrentEmotion.Arousal = 0.5f;
    State.CurrentEmotion.Intensity = 0.5f;
    State.EmotionalStability = 0.5f;
}

void UAdvancedEmotionBlending::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update transition
    UpdateTransition(DeltaTime);

    // Decay layers
    DecayLayers(DeltaTime);

    // Blend layers
    BlendLayers();

    // Update momentum
    if (bEnableMomentumTracking)
    {
        UpdateMomentum(DeltaTime);
    }

    // Update stability
    UpdateStability(DeltaTime);

    // Evaluate expression rules
    EvaluateExpressionRules();

    // Apply to avatar
    ApplyToAvatar();

    // Store previous emotion
    PreviousEmotion = State.CurrentEmotion;
}

void UAdvancedEmotionBlending::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        AvatarCognition = Owner->FindComponentByClass<UUnrealAvatarCognition>();
        EchobeatsEngine = Owner->FindComponentByClass<UEchobeatsStreamEngine>();
        CouplingDynamics = Owner->FindComponentByClass<UTensionalCouplingDynamics>();
        SensoryIntegration = Owner->FindComponentByClass<USensoryInputIntegration>();
    }
}

void UAdvancedEmotionBlending::AddEmotionLayer(const FString& LayerName, const FEmotionVector& Emotion, float Weight, int32 Priority)
{
    // Check if layer already exists
    for (FEmotionLayer& Layer : State.ActiveLayers)
    {
        if (Layer.LayerName == LayerName)
        {
            Layer.Emotion = Emotion;
            Layer.Weight = Weight;
            Layer.Priority = Priority;
            return;
        }
    }

    // Add new layer
    if (State.ActiveLayers.Num() < MaxActiveLayers)
    {
        FEmotionLayer NewLayer;
        NewLayer.LayerName = LayerName;
        NewLayer.Emotion = Emotion;
        NewLayer.Weight = Weight;
        NewLayer.Priority = Priority;
        NewLayer.bIsActive = true;
        NewLayer.DecayRate = 0.0f;

        State.ActiveLayers.Add(NewLayer);

        // Sort by priority
        State.ActiveLayers.Sort([](const FEmotionLayer& A, const FEmotionLayer& B) {
            return A.Priority > B.Priority;
        });

        OnLayerAdded.Broadcast(LayerName, Emotion);
    }
}

void UAdvancedEmotionBlending::RemoveEmotionLayer(const FString& LayerName)
{
    for (int32 i = State.ActiveLayers.Num() - 1; i >= 0; --i)
    {
        if (State.ActiveLayers[i].LayerName == LayerName)
        {
            State.ActiveLayers.RemoveAt(i);
            OnLayerRemoved.Broadcast(LayerName);
            break;
        }
    }
}

void UAdvancedEmotionBlending::UpdateEmotionLayer(const FString& LayerName, const FEmotionVector& Emotion)
{
    for (FEmotionLayer& Layer : State.ActiveLayers)
    {
        if (Layer.LayerName == LayerName)
        {
            Layer.Emotion = Emotion;
            break;
        }
    }
}

void UAdvancedEmotionBlending::SetLayerWeight(const FString& LayerName, float Weight)
{
    for (FEmotionLayer& Layer : State.ActiveLayers)
    {
        if (Layer.LayerName == LayerName)
        {
            Layer.Weight = FMath::Clamp(Weight, 0.0f, 1.0f);
            break;
        }
    }
}

FEmotionLayer UAdvancedEmotionBlending::GetEmotionLayer(const FString& LayerName) const
{
    for (const FEmotionLayer& Layer : State.ActiveLayers)
    {
        if (Layer.LayerName == LayerName)
        {
            return Layer;
        }
    }
    return FEmotionLayer();
}

TArray<FEmotionLayer> UAdvancedEmotionBlending::GetActiveLayers() const
{
    return State.ActiveLayers;
}

void UAdvancedEmotionBlending::TransitionToEmotion(const FEmotionVector& TargetEmotion, float Duration, EEmotionTransitionCurve Curve)
{
    State.CurrentTransition.SourceEmotion = State.CurrentEmotion;
    State.CurrentTransition.TargetEmotion = TargetEmotion;
    State.CurrentTransition.Duration = (Duration < 0.0f) ? DefaultTransitionDuration : Duration;
    State.CurrentTransition.ElapsedTime = 0.0f;
    State.CurrentTransition.Curve = Curve;
    State.CurrentTransition.bIsActive = true;

    OnTransitionStarted.Broadcast(State.CurrentTransition.SourceEmotion, TargetEmotion);
}

void UAdvancedEmotionBlending::TransitionToNamedEmotion(EAvatarEmotionState EmotionState, float Intensity, float Duration)
{
    FEmotionVector TargetEmotion = NamedEmotionToVector(EmotionState, Intensity);
    TransitionToEmotion(TargetEmotion, Duration, DefaultTransitionCurve);
}

void UAdvancedEmotionBlending::CancelTransition()
{
    State.CurrentTransition.bIsActive = false;
}

bool UAdvancedEmotionBlending::IsTransitioning() const
{
    return State.CurrentTransition.bIsActive;
}

float UAdvancedEmotionBlending::GetTransitionProgress() const
{
    if (!State.CurrentTransition.bIsActive || State.CurrentTransition.Duration <= 0.0f)
    {
        return 1.0f;
    }
    return FMath::Clamp(State.CurrentTransition.ElapsedTime / State.CurrentTransition.Duration, 0.0f, 1.0f);
}

void UAdvancedEmotionBlending::AddExpressionRule(const FEmotionExpressionRule& Rule)
{
    ExpressionRules.Add(Rule);
}

void UAdvancedEmotionBlending::RemoveExpressionRule(const FString& RuleName)
{
    ExpressionRules.RemoveAll([&RuleName](const FEmotionExpressionRule& Rule) {
        return Rule.RuleName == RuleName;
    });
}

void UAdvancedEmotionBlending::SetRuleEnabled(const FString& RuleName, bool bEnabled)
{
    for (FEmotionExpressionRule& Rule : ExpressionRules)
    {
        if (Rule.RuleName == RuleName)
        {
            Rule.bIsEnabled = bEnabled;
            break;
        }
    }
}

TArray<FString> UAdvancedEmotionBlending::GetActiveRuleNames() const
{
    return State.ActiveRuleNames;
}

void UAdvancedEmotionBlending::InitializeDefaultRules()
{
    ExpressionRules.Empty();

    // Joy rule
    {
        FEmotionExpressionRule JoyRule;
        JoyRule.RuleName = TEXT("Joy");
        JoyRule.EmotionCondition.Valence = 0.8f;
        JoyRule.EmotionCondition.Arousal = 0.7f;
        JoyRule.EmotionCondition.Approach = 0.5f;
        JoyRule.ActivationRadius = 0.4f;
        JoyRule.Priority = 10;
        JoyRule.bIsEnabled = true;

        JoyRule.ExpressionTarget.FacialExpression.Happiness = 0.9f;
        JoyRule.ExpressionTarget.FacialExpression.EyeOpenness = 0.8f;
        JoyRule.ExpressionTarget.BodySchema.Posture = 0.8f;
        JoyRule.ExpressionTarget.AuraColor = FLinearColor(1.0f, 0.9f, 0.3f, 1.0f);
        JoyRule.ExpressionTarget.AuraIntensity = 0.7f;

        ExpressionRules.Add(JoyRule);
    }

    // Sadness rule
    {
        FEmotionExpressionRule SadRule;
        SadRule.RuleName = TEXT("Sadness");
        SadRule.EmotionCondition.Valence = -0.7f;
        SadRule.EmotionCondition.Arousal = 0.3f;
        SadRule.EmotionCondition.Approach = -0.3f;
        SadRule.ActivationRadius = 0.4f;
        SadRule.Priority = 10;
        SadRule.bIsEnabled = true;

        SadRule.ExpressionTarget.FacialExpression.Sadness = 0.8f;
        SadRule.ExpressionTarget.FacialExpression.EyeOpenness = 0.4f;
        SadRule.ExpressionTarget.BodySchema.Posture = 0.3f;
        SadRule.ExpressionTarget.AuraColor = FLinearColor(0.3f, 0.4f, 0.7f, 1.0f);
        SadRule.ExpressionTarget.AuraIntensity = 0.4f;

        ExpressionRules.Add(SadRule);
    }

    // Anger rule
    {
        FEmotionExpressionRule AngerRule;
        AngerRule.RuleName = TEXT("Anger");
        AngerRule.EmotionCondition.Valence = -0.6f;
        AngerRule.EmotionCondition.Arousal = 0.9f;
        AngerRule.EmotionCondition.Dominance = 0.7f;
        AngerRule.EmotionCondition.Approach = 0.5f;
        AngerRule.ActivationRadius = 0.4f;
        AngerRule.Priority = 15;
        AngerRule.bIsEnabled = true;

        AngerRule.ExpressionTarget.FacialExpression.Anger = 0.9f;
        AngerRule.ExpressionTarget.FacialExpression.BrowFurrow = 0.8f;
        AngerRule.ExpressionTarget.BodySchema.Tension = 0.9f;
        AngerRule.ExpressionTarget.AuraColor = FLinearColor(0.9f, 0.2f, 0.1f, 1.0f);
        AngerRule.ExpressionTarget.AuraIntensity = 0.8f;

        ExpressionRules.Add(AngerRule);
    }

    // Fear rule
    {
        FEmotionExpressionRule FearRule;
        FearRule.RuleName = TEXT("Fear");
        FearRule.EmotionCondition.Valence = -0.5f;
        FearRule.EmotionCondition.Arousal = 0.8f;
        FearRule.EmotionCondition.Dominance = -0.7f;
        FearRule.EmotionCondition.Approach = -0.8f;
        FearRule.ActivationRadius = 0.4f;
        FearRule.Priority = 15;
        FearRule.bIsEnabled = true;

        FearRule.ExpressionTarget.FacialExpression.Fear = 0.9f;
        FearRule.ExpressionTarget.FacialExpression.EyeOpenness = 1.0f;
        FearRule.ExpressionTarget.BodySchema.Tension = 0.8f;
        FearRule.ExpressionTarget.AuraColor = FLinearColor(0.5f, 0.3f, 0.6f, 1.0f);
        FearRule.ExpressionTarget.AuraIntensity = 0.6f;

        ExpressionRules.Add(FearRule);
    }

    // Surprise rule
    {
        FEmotionExpressionRule SurpriseRule;
        SurpriseRule.RuleName = TEXT("Surprise");
        SurpriseRule.EmotionCondition.Novelty = 0.9f;
        SurpriseRule.EmotionCondition.Arousal = 0.8f;
        SurpriseRule.EmotionCondition.Certainty = 0.2f;
        SurpriseRule.ActivationRadius = 0.35f;
        SurpriseRule.Priority = 12;
        SurpriseRule.bIsEnabled = true;

        SurpriseRule.ExpressionTarget.FacialExpression.Surprise = 0.9f;
        SurpriseRule.ExpressionTarget.FacialExpression.EyeOpenness = 1.0f;
        SurpriseRule.ExpressionTarget.FacialExpression.BrowRaise = 0.9f;
        SurpriseRule.ExpressionTarget.AuraColor = FLinearColor(0.8f, 0.8f, 1.0f, 1.0f);
        SurpriseRule.ExpressionTarget.AuraIntensity = 0.7f;

        ExpressionRules.Add(SurpriseRule);
    }

    // Curiosity rule
    {
        FEmotionExpressionRule CuriosityRule;
        CuriosityRule.RuleName = TEXT("Curiosity");
        CuriosityRule.EmotionCondition.Valence = 0.3f;
        CuriosityRule.EmotionCondition.Arousal = 0.6f;
        CuriosityRule.EmotionCondition.Novelty = 0.7f;
        CuriosityRule.EmotionCondition.Approach = 0.6f;
        CuriosityRule.ActivationRadius = 0.4f;
        CuriosityRule.Priority = 8;
        CuriosityRule.bIsEnabled = true;

        CuriosityRule.ExpressionTarget.FacialExpression.Interest = 0.8f;
        CuriosityRule.ExpressionTarget.FacialExpression.EyeOpenness = 0.7f;
        CuriosityRule.ExpressionTarget.BodySchema.Posture = 0.7f;
        CuriosityRule.ExpressionTarget.AuraColor = FLinearColor(0.4f, 0.8f, 0.9f, 1.0f);
        CuriosityRule.ExpressionTarget.AuraIntensity = 0.5f;

        ExpressionRules.Add(CuriosityRule);
    }

    // Contemplation rule
    {
        FEmotionExpressionRule ContemplationRule;
        ContemplationRule.RuleName = TEXT("Contemplation");
        ContemplationRule.EmotionCondition.Arousal = 0.4f;
        ContemplationRule.EmotionCondition.Temporal = -0.3f;
        ContemplationRule.EmotionCondition.Agency = 0.5f;
        ContemplationRule.ActivationRadius = 0.4f;
        ContemplationRule.Priority = 5;
        ContemplationRule.bIsEnabled = true;

        ContemplationRule.ExpressionTarget.FacialExpression.Contemplation = 0.7f;
        ContemplationRule.ExpressionTarget.FacialExpression.EyeOpenness = 0.5f;
        ContemplationRule.ExpressionTarget.BodySchema.Posture = 0.5f;
        ContemplationRule.ExpressionTarget.AuraColor = FLinearColor(0.6f, 0.6f, 0.8f, 1.0f);
        ContemplationRule.ExpressionTarget.AuraIntensity = 0.4f;

        ExpressionRules.Add(ContemplationRule);
    }

    // Neutral rule (lowest priority, always matches somewhat)
    {
        FEmotionExpressionRule NeutralRule;
        NeutralRule.RuleName = TEXT("Neutral");
        NeutralRule.EmotionCondition.Valence = 0.0f;
        NeutralRule.EmotionCondition.Arousal = 0.5f;
        NeutralRule.ActivationRadius = 1.0f; // Always active to some degree
        NeutralRule.Priority = 0;
        NeutralRule.bIsEnabled = true;

        NeutralRule.ExpressionTarget.FacialExpression.Neutral = 1.0f;
        NeutralRule.ExpressionTarget.BodySchema.Posture = 0.5f;
        NeutralRule.ExpressionTarget.AuraColor = FLinearColor(0.7f, 0.7f, 0.7f, 1.0f);
        NeutralRule.ExpressionTarget.AuraIntensity = 0.3f;

        ExpressionRules.Add(NeutralRule);
    }
}

FEmotionVector UAdvancedEmotionBlending::GetCurrentEmotion() const
{
    return State.CurrentEmotion;
}

FExpressionTarget UAdvancedEmotionBlending::GetCurrentExpression() const
{
    return State.CurrentExpression;
}

float UAdvancedEmotionBlending::GetEmotionalStability() const
{
    return State.EmotionalStability;
}

FEmotionVector UAdvancedEmotionBlending::GetEmotionalMomentum() const
{
    return State.EmotionalMomentum;
}

FEmotionBlendingState UAdvancedEmotionBlending::GetBlendingState() const
{
    return State;
}

FEmotionVector UAdvancedEmotionBlending::NamedEmotionToVector(EAvatarEmotionState EmotionState, float Intensity)
{
    FEmotionVector Result;
    Result.Intensity = Intensity;

    switch (EmotionState)
    {
    case EAvatarEmotionState::Neutral:
        Result.Valence = 0.0f;
        Result.Arousal = 0.5f;
        Result.Dominance = 0.0f;
        break;

    case EAvatarEmotionState::Happy:
        Result.Valence = 0.8f * Intensity;
        Result.Arousal = 0.7f * Intensity;
        Result.Approach = 0.5f * Intensity;
        Result.Agency = 0.3f * Intensity;
        break;

    case EAvatarEmotionState::Sad:
        Result.Valence = -0.7f * Intensity;
        Result.Arousal = 0.3f * Intensity;
        Result.Approach = -0.3f * Intensity;
        Result.Agency = -0.2f * Intensity;
        break;

    case EAvatarEmotionState::Angry:
        Result.Valence = -0.6f * Intensity;
        Result.Arousal = 0.9f * Intensity;
        Result.Dominance = 0.7f * Intensity;
        Result.Approach = 0.5f * Intensity;
        break;

    case EAvatarEmotionState::Fearful:
        Result.Valence = -0.5f * Intensity;
        Result.Arousal = 0.8f * Intensity;
        Result.Dominance = -0.7f * Intensity;
        Result.Approach = -0.8f * Intensity;
        Result.Certainty = 0.2f;
        break;

    case EAvatarEmotionState::Surprised:
        Result.Arousal = 0.8f * Intensity;
        Result.Novelty = 0.9f * Intensity;
        Result.Certainty = 0.2f;
        break;

    case EAvatarEmotionState::Disgusted:
        Result.Valence = -0.6f * Intensity;
        Result.Arousal = 0.5f * Intensity;
        Result.Approach = -0.9f * Intensity;
        break;

    case EAvatarEmotionState::Curious:
        Result.Valence = 0.3f * Intensity;
        Result.Arousal = 0.6f * Intensity;
        Result.Novelty = 0.7f * Intensity;
        Result.Approach = 0.6f * Intensity;
        Result.Temporal = 0.3f * Intensity;
        break;

    case EAvatarEmotionState::Contemplative:
        Result.Arousal = 0.4f * Intensity;
        Result.Temporal = -0.3f * Intensity;
        Result.Agency = 0.5f * Intensity;
        break;

    case EAvatarEmotionState::Excited:
        Result.Valence = 0.7f * Intensity;
        Result.Arousal = 0.95f * Intensity;
        Result.Approach = 0.7f * Intensity;
        Result.Temporal = 0.5f * Intensity;
        break;

    case EAvatarEmotionState::Calm:
        Result.Valence = 0.2f * Intensity;
        Result.Arousal = 0.2f * Intensity;
        Result.Certainty = 0.7f * Intensity;
        break;

    case EAvatarEmotionState::Focused:
        Result.Arousal = 0.6f * Intensity;
        Result.Certainty = 0.8f * Intensity;
        Result.Agency = 0.6f * Intensity;
        break;
    }

    return Result;
}

EAvatarEmotionState UAdvancedEmotionBlending::GetClosestNamedEmotion(const FEmotionVector& Emotion)
{
    EAvatarEmotionState ClosestState = EAvatarEmotionState::Neutral;
    float MinDistance = FLT_MAX;

    TArray<EAvatarEmotionState> AllStates = {
        EAvatarEmotionState::Neutral,
        EAvatarEmotionState::Happy,
        EAvatarEmotionState::Sad,
        EAvatarEmotionState::Angry,
        EAvatarEmotionState::Fearful,
        EAvatarEmotionState::Surprised,
        EAvatarEmotionState::Disgusted,
        EAvatarEmotionState::Curious,
        EAvatarEmotionState::Contemplative,
        EAvatarEmotionState::Excited,
        EAvatarEmotionState::Calm,
        EAvatarEmotionState::Focused
    };

    for (EAvatarEmotionState State : AllStates)
    {
        FEmotionVector StateVector = NamedEmotionToVector(State, Emotion.Intensity);
        float Distance = Emotion.DistanceTo(StateVector);

        if (Distance < MinDistance)
        {
            MinDistance = Distance;
            ClosestState = State;
        }
    }

    return ClosestState;
}

FEmotionVector UAdvancedEmotionBlending::BlendEmotions(const FEmotionVector& A, const FEmotionVector& B, float Alpha)
{
    return FEmotionVector::Lerp(A, B, Alpha);
}

void UAdvancedEmotionBlending::ProcessEchobeatStep(int32 Step, const FEchobeatsFullState& EchobeatsState)
{
    State.CurrentEchobeatStep = Step;

    if (bEnableEchobeatModulation)
    {
        ModulateByEchobeat(Step);
    }

    // Create emotion layer from echobeats state
    FEmotionVector EchobeatEmotion;
    EchobeatEmotion.Arousal = EchobeatsState.OverallCoherence;
    EchobeatEmotion.Certainty = EchobeatsState.OverallCoherence;

    // Modulate by stream states
    for (const FCognitiveStreamState& StreamState : EchobeatsState.StreamStates)
    {
        if (StreamState.StreamType == ECognitiveStreamType::Pivotal)
        {
            EchobeatEmotion.Agency += StreamState.ActivationLevel * 0.3f;
        }
        else if (StreamState.StreamType == ECognitiveStreamType::Affordance)
        {
            EchobeatEmotion.Approach += StreamState.ActivationLevel * 0.3f;
        }
        else if (StreamState.StreamType == ECognitiveStreamType::Salience)
        {
            EchobeatEmotion.Novelty += StreamState.ActivationLevel * 0.3f;
        }
    }

    EchobeatEmotion.Normalize();
    AddEmotionLayer(TEXT("Echobeat"), EchobeatEmotion, 0.3f, 5);
}

void UAdvancedEmotionBlending::ProcessSensoryInput(const FSensoryInputData& Input)
{
    FEmotionVector SensoryEmotion;
    SensoryEmotion.Valence = Input.EmotionalValence;
    SensoryEmotion.Arousal = Input.Intensity;
    SensoryEmotion.Novelty = Input.NoveltyScore;

    // Priority-based arousal
    float PriorityArousal = static_cast<float>(Input.Priority) / 4.0f;
    SensoryEmotion.Arousal = FMath::Max(SensoryEmotion.Arousal, PriorityArousal);

    // Modality-specific effects
    switch (Input.Modality)
    {
    case ESensoryModality::Social:
        SensoryEmotion.Agency = 0.3f;
        break;
    case ESensoryModality::Tactile:
        SensoryEmotion.Approach = 0.2f;
        break;
    default:
        break;
    }

    SensoryEmotion.Normalize();
    SensoryEmotion.Intensity = Input.Intensity;

    FString LayerName = FString::Printf(TEXT("Sensory_%d"), Input.InputID);
    AddEmotionLayer(LayerName, SensoryEmotion, Input.Intensity * 0.5f, 3);
}

void UAdvancedEmotionBlending::ProcessTensionalCoupling(const FTetrahedralCouplingState& CouplingState)
{
    FEmotionVector CouplingEmotion;
    CouplingEmotion.Certainty = CouplingState.TetrahedralCoherence;
    CouplingEmotion.Arousal = CouplingState.TensionBalance;

    // Entanglement affects agency
    if (CouplingState.ActiveEntanglements.Num() > 0)
    {
        CouplingEmotion.Agency = 0.5f;
    }

    CouplingEmotion.Normalize();
    AddEmotionLayer(TEXT("TensionalCoupling"), CouplingEmotion, 0.2f, 2);
}

void UAdvancedEmotionBlending::ApplyToAvatar()
{
    if (AvatarCognition)
    {
        // Convert current emotion to avatar emotion state
        EAvatarEmotionState ClosestEmotion = GetClosestNamedEmotion(State.CurrentEmotion);

        // Apply to avatar cognition
        // This would call AvatarCognition->SetEmotionState(ClosestEmotion, State.CurrentEmotion.Intensity);
    }
}

void UAdvancedEmotionBlending::BlendLayers()
{
    if (State.ActiveLayers.Num() == 0)
    {
        return;
    }

    FEmotionVector BlendedEmotion;
    float TotalWeight = 0.0f;

    switch (DefaultBlendMode)
    {
    case EEmotionBlendMode::Linear:
        // Simple average
        for (const FEmotionLayer& Layer : State.ActiveLayers)
        {
            if (Layer.bIsActive)
            {
                BlendedEmotion = BlendedEmotion + Layer.Emotion * Layer.Weight;
                TotalWeight += Layer.Weight;
            }
        }
        if (TotalWeight > 0.0f)
        {
            BlendedEmotion = BlendedEmotion * (1.0f / TotalWeight);
        }
        break;

    case EEmotionBlendMode::Weighted:
        // Priority-weighted blend
        for (const FEmotionLayer& Layer : State.ActiveLayers)
        {
            if (Layer.bIsActive)
            {
                float PriorityWeight = Layer.Weight * (1.0f + Layer.Priority * 0.1f);
                BlendedEmotion = BlendedEmotion + Layer.Emotion * PriorityWeight;
                TotalWeight += PriorityWeight;
            }
        }
        if (TotalWeight > 0.0f)
        {
            BlendedEmotion = BlendedEmotion * (1.0f / TotalWeight);
        }
        break;

    case EEmotionBlendMode::Dominant:
        // Highest priority wins
        {
            int32 HighestPriority = -1;
            for (const FEmotionLayer& Layer : State.ActiveLayers)
            {
                if (Layer.bIsActive && Layer.Priority > HighestPriority)
                {
                    HighestPriority = Layer.Priority;
                    BlendedEmotion = Layer.Emotion;
                }
            }
        }
        break;

    case EEmotionBlendMode::Layered:
        // Background + foreground layers
        {
            FEmotionVector Background;
            FEmotionVector Foreground;
            float BackgroundWeight = 0.0f;
            float ForegroundWeight = 0.0f;

            for (const FEmotionLayer& Layer : State.ActiveLayers)
            {
                if (Layer.bIsActive)
                {
                    if (Layer.Priority < 5)
                    {
                        Background = Background + Layer.Emotion * Layer.Weight;
                        BackgroundWeight += Layer.Weight;
                    }
                    else
                    {
                        Foreground = Foreground + Layer.Emotion * Layer.Weight;
                        ForegroundWeight += Layer.Weight;
                    }
                }
            }

            if (BackgroundWeight > 0.0f)
            {
                Background = Background * (1.0f / BackgroundWeight);
            }
            if (ForegroundWeight > 0.0f)
            {
                Foreground = Foreground * (1.0f / ForegroundWeight);
            }

            // Blend foreground over background
            float ForegroundInfluence = FMath::Min(ForegroundWeight, 1.0f);
            BlendedEmotion = FEmotionVector::Lerp(Background, Foreground, ForegroundInfluence);
        }
        break;

    case EEmotionBlendMode::Oscillating:
        // Time-varying blend
        {
            float Time = GetWorld()->GetTimeSeconds();
            float OscillationPhase = FMath::Sin(Time * 2.0f) * 0.5f + 0.5f;

            for (int32 i = 0; i < State.ActiveLayers.Num(); ++i)
            {
                const FEmotionLayer& Layer = State.ActiveLayers[i];
                if (Layer.bIsActive)
                {
                    float PhaseOffset = (float)i / State.ActiveLayers.Num();
                    float LayerPhase = FMath::Sin((Time + PhaseOffset * 2.0f * PI) * 2.0f) * 0.5f + 0.5f;
                    float ModulatedWeight = Layer.Weight * LayerPhase;

                    BlendedEmotion = BlendedEmotion + Layer.Emotion * ModulatedWeight;
                    TotalWeight += ModulatedWeight;
                }
            }
            if (TotalWeight > 0.0f)
            {
                BlendedEmotion = BlendedEmotion * (1.0f / TotalWeight);
            }
        }
        break;
    }

    // Apply emotional inertia
    State.CurrentEmotion = FEmotionVector::Lerp(State.CurrentEmotion, BlendedEmotion, 1.0f - EmotionalInertia);
    State.CurrentEmotion.Normalize();
}

void UAdvancedEmotionBlending::UpdateTransition(float DeltaTime)
{
    if (!State.CurrentTransition.bIsActive)
    {
        return;
    }

    State.CurrentTransition.ElapsedTime += DeltaTime;

    float Progress = GetTransitionProgress();
    float CurvedProgress = EvaluateTransitionCurve(Progress, State.CurrentTransition.Curve);

    State.CurrentEmotion = FEmotionVector::Lerp(
        State.CurrentTransition.SourceEmotion,
        State.CurrentTransition.TargetEmotion,
        CurvedProgress
    );

    if (Progress >= 1.0f)
    {
        State.CurrentTransition.bIsActive = false;
        OnTransitionCompleted.Broadcast(State.CurrentEmotion);
    }
}

void UAdvancedEmotionBlending::UpdateMomentum(float DeltaTime)
{
    if (DeltaTime <= 0.0f)
    {
        return;
    }

    // Compute rate of change
    State.EmotionalMomentum.Valence = (State.CurrentEmotion.Valence - PreviousEmotion.Valence) / DeltaTime;
    State.EmotionalMomentum.Arousal = (State.CurrentEmotion.Arousal - PreviousEmotion.Arousal) / DeltaTime;
    State.EmotionalMomentum.Dominance = (State.CurrentEmotion.Dominance - PreviousEmotion.Dominance) / DeltaTime;
    State.EmotionalMomentum.Approach = (State.CurrentEmotion.Approach - PreviousEmotion.Approach) / DeltaTime;
    State.EmotionalMomentum.Certainty = (State.CurrentEmotion.Certainty - PreviousEmotion.Certainty) / DeltaTime;
    State.EmotionalMomentum.Novelty = (State.CurrentEmotion.Novelty - PreviousEmotion.Novelty) / DeltaTime;
    State.EmotionalMomentum.Agency = (State.CurrentEmotion.Agency - PreviousEmotion.Agency) / DeltaTime;
    State.EmotionalMomentum.Temporal = (State.CurrentEmotion.Temporal - PreviousEmotion.Temporal) / DeltaTime;
}

void UAdvancedEmotionBlending::UpdateStability(float DeltaTime)
{
    // Compute magnitude of momentum
    float MomentumMagnitude = 0.0f;
    MomentumMagnitude += FMath::Abs(State.EmotionalMomentum.Valence);
    MomentumMagnitude += FMath::Abs(State.EmotionalMomentum.Arousal);
    MomentumMagnitude += FMath::Abs(State.EmotionalMomentum.Dominance);
    MomentumMagnitude += FMath::Abs(State.EmotionalMomentum.Approach);
    MomentumMagnitude /= 4.0f;

    // Stability is inverse of momentum magnitude
    float TargetStability = 1.0f / (1.0f + MomentumMagnitude * 2.0f);

    float OldStability = State.EmotionalStability;
    State.EmotionalStability = FMath::Lerp(State.EmotionalStability, TargetStability, DeltaTime * 2.0f);

    if (FMath::Abs(State.EmotionalStability - OldStability) > 0.1f)
    {
        OnStabilityChanged.Broadcast(State.EmotionalStability);
    }
}

void UAdvancedEmotionBlending::EvaluateExpressionRules()
{
    State.ActiveRuleNames.Empty();

    TArray<FExpressionTarget> MatchingTargets;
    TArray<float> MatchingWeights;

    for (const FEmotionExpressionRule& Rule : ExpressionRules)
    {
        if (!Rule.bIsEnabled)
        {
            continue;
        }

        float Distance = State.CurrentEmotion.DistanceTo(Rule.EmotionCondition);

        if (Distance < Rule.ActivationRadius)
        {
            float ActivationStrength = 1.0f - (Distance / Rule.ActivationRadius);

            MatchingTargets.Add(Rule.ExpressionTarget);
            MatchingWeights.Add(ActivationStrength * (1.0f + Rule.Priority * 0.1f));

            State.ActiveRuleNames.Add(Rule.RuleName);
            OnRuleActivated.Broadcast(Rule.RuleName, ActivationStrength);
        }
    }

    if (MatchingTargets.Num() > 0)
    {
        State.CurrentExpression = BlendExpressionTargets(MatchingTargets, MatchingWeights);
    }
}

void UAdvancedEmotionBlending::DecayLayers(float DeltaTime)
{
    for (int32 i = State.ActiveLayers.Num() - 1; i >= 0; --i)
    {
        FEmotionLayer& Layer = State.ActiveLayers[i];

        if (Layer.DecayRate > 0.0f)
        {
            Layer.Weight -= Layer.DecayRate * DeltaTime;

            if (Layer.Weight <= 0.0f)
            {
                OnLayerRemoved.Broadcast(Layer.LayerName);
                State.ActiveLayers.RemoveAt(i);
            }
        }
    }
}

float UAdvancedEmotionBlending::EvaluateTransitionCurve(float T, EEmotionTransitionCurve Curve)
{
    T = FMath::Clamp(T, 0.0f, 1.0f);

    switch (Curve)
    {
    case EEmotionTransitionCurve::Linear:
        return T;

    case EEmotionTransitionCurve::EaseIn:
        return T * T;

    case EEmotionTransitionCurve::EaseOut:
        return 1.0f - (1.0f - T) * (1.0f - T);

    case EEmotionTransitionCurve::EaseInOut:
        return T < 0.5f ? 2.0f * T * T : 1.0f - FMath::Pow(-2.0f * T + 2.0f, 2.0f) / 2.0f;

    case EEmotionTransitionCurve::Bounce:
        {
            float n1 = 7.5625f;
            float d1 = 2.75f;
            if (T < 1.0f / d1)
            {
                return n1 * T * T;
            }
            else if (T < 2.0f / d1)
            {
                T -= 1.5f / d1;
                return n1 * T * T + 0.75f;
            }
            else if (T < 2.5f / d1)
            {
                T -= 2.25f / d1;
                return n1 * T * T + 0.9375f;
            }
            else
            {
                T -= 2.625f / d1;
                return n1 * T * T + 0.984375f;
            }
        }

    case EEmotionTransitionCurve::Overshoot:
        {
            float c1 = 1.70158f;
            float c3 = c1 + 1.0f;
            return 1.0f + c3 * FMath::Pow(T - 1.0f, 3.0f) + c1 * FMath::Pow(T - 1.0f, 2.0f);
        }

    case EEmotionTransitionCurve::Step:
        return T >= 1.0f ? 1.0f : 0.0f;

    default:
        return T;
    }
}

FExpressionTarget UAdvancedEmotionBlending::BlendExpressionTargets(const TArray<FExpressionTarget>& Targets, const TArray<float>& Weights)
{
    FExpressionTarget Result;

    if (Targets.Num() == 0 || Weights.Num() == 0)
    {
        return Result;
    }

    float TotalWeight = 0.0f;
    for (float W : Weights)
    {
        TotalWeight += W;
    }

    if (TotalWeight <= 0.0f)
    {
        return Targets[0];
    }

    // Blend facial expressions
    for (int32 i = 0; i < Targets.Num(); ++i)
    {
        float NormWeight = Weights[i] / TotalWeight;
        const FExpressionTarget& Target = Targets[i];

        Result.FacialExpression.Happiness += Target.FacialExpression.Happiness * NormWeight;
        Result.FacialExpression.Sadness += Target.FacialExpression.Sadness * NormWeight;
        Result.FacialExpression.Anger += Target.FacialExpression.Anger * NormWeight;
        Result.FacialExpression.Fear += Target.FacialExpression.Fear * NormWeight;
        Result.FacialExpression.Surprise += Target.FacialExpression.Surprise * NormWeight;
        Result.FacialExpression.Disgust += Target.FacialExpression.Disgust * NormWeight;
        Result.FacialExpression.Neutral += Target.FacialExpression.Neutral * NormWeight;
        Result.FacialExpression.EyeOpenness += Target.FacialExpression.EyeOpenness * NormWeight;
        Result.FacialExpression.BrowRaise += Target.FacialExpression.BrowRaise * NormWeight;
        Result.FacialExpression.BrowFurrow += Target.FacialExpression.BrowFurrow * NormWeight;

        Result.BodySchema.Posture += Target.BodySchema.Posture * NormWeight;
        Result.BodySchema.Tension += Target.BodySchema.Tension * NormWeight;

        Result.AuraColor += Target.AuraColor * NormWeight;
        Result.AuraIntensity += Target.AuraIntensity * NormWeight;
    }

    return Result;
}

void UAdvancedEmotionBlending::ModulateByEchobeat(int32 Step)
{
    // Modulate arousal based on step type
    // Pivotal steps (1, 5, 9) - higher arousal
    // Affordance steps - moderate arousal
    // Salience steps - variable arousal

    int32 StepInTriad = ((Step - 1) % 4);
    float ArousalModulation = 0.0f;

    if (StepInTriad == 0)
    {
        // Pivotal step
        ArousalModulation = 0.1f;
    }
    else if (StepInTriad == 1 || StepInTriad == 2)
    {
        // Affordance steps
        ArousalModulation = 0.05f;
    }
    else
    {
        // Salience steps
        ArousalModulation = -0.05f;
    }

    State.CurrentEmotion.Arousal = FMath::Clamp(State.CurrentEmotion.Arousal + ArousalModulation, 0.0f, 1.0f);

    // Temporal modulation based on step
    float TemporalShift = (Step <= 6) ? 0.1f : -0.1f;
    State.CurrentEmotion.Temporal = FMath::Clamp(State.CurrentEmotion.Temporal + TemporalShift * 0.1f, -1.0f, 1.0f);
}

// UnrealAvatarCognition.cpp
// Implementation of Unreal Engine avatar cognitive integration

#include "UnrealAvatarCognition.h"
#include "Math/UnrealMathUtility.h"

UUnrealAvatarCognition::UUnrealAvatarCognition()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UUnrealAvatarCognition::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeDefaultMappings();

    // Initialize 4E state
    State.E4State.Add(TEXT("Embodied"), 0.5f);
    State.E4State.Add(TEXT("Embedded"), 0.5f);
    State.E4State.Add(TEXT("Enacted"), 0.5f);
    State.E4State.Add(TEXT("Extended"), 0.5f);
}

void UUnrealAvatarCognition::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableAutoExpression)
    {
        return;
    }

    // Update expression timer
    ExpressionTimer += DeltaTime;
    float UpdateInterval = 1.0f / ExpressionUpdateRate;

    if (ExpressionTimer >= UpdateInterval)
    {
        ExpressionTimer -= UpdateInterval;

        // Update expression from cognitive state
        ComputeExpressionFromCognition();
        UpdateExpressionFromMappings();
    }

    // Blend expression states
    BlendExpressionStates(DeltaTime);

    // Apply expression to avatar
    ApplyExpressionToAvatar();

    // Handle emotion blending
    if (EmotionBlendTimer > 0.0f)
    {
        EmotionBlendTimer -= DeltaTime;
        if (EmotionBlendTimer <= 0.0f)
        {
            EmotionBlendTimer = 0.0f;
            State.EmotionState = TargetEmotionState;
            State.EmotionIntensity = TargetEmotionIntensity;
        }
        else
        {
            // Interpolate emotion intensity
            float BlendAlpha = 1.0f - (EmotionBlendTimer / EmotionTransitionDuration);
            State.EmotionIntensity = FMath::Lerp(State.EmotionIntensity, TargetEmotionIntensity, BlendAlpha);
        }
    }
}

void UUnrealAvatarCognition::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        SkeletalMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
        EchobeatsEngine = Owner->FindComponentByClass<UEchobeatsStreamEngine>();
        Sys6Avatar = Owner->FindComponentByClass<USys6AvatarIntegration>();

        // Bind to echobeats events
        if (EchobeatsEngine)
        {
            EchobeatsEngine->OnStepChanged.AddDynamic(this, &UUnrealAvatarCognition::HandleEchobeatStepChanged);
            EchobeatsEngine->OnRelevanceRealized.AddDynamic(this, &UUnrealAvatarCognition::HandleRelevanceRealized);
            EchobeatsEngine->OnStreamActivated.AddDynamic(this, &UUnrealAvatarCognition::HandleStreamActivated);
        }
    }
}

void UUnrealAvatarCognition::InitializeDefaultMappings()
{
    ExpressionMappings.Empty();

    // Coherence -> Facial stability
    FCognitiveExpressionMapping CoherenceMapping;
    CoherenceMapping.CognitiveParameter = TEXT("Coherence");
    CoherenceMapping.ExpressionChannel = EAvatarExpressionChannel::Facial;
    CoherenceMapping.MappingWeight = 0.8f;
    CoherenceMapping.ActivationThreshold = 0.1f;
    ExpressionMappings.Add(CoherenceMapping);

    // Relevance -> Gaze focus
    FCognitiveExpressionMapping RelevanceMapping;
    RelevanceMapping.CognitiveParameter = TEXT("Relevance");
    RelevanceMapping.ExpressionChannel = EAvatarExpressionChannel::Gaze;
    RelevanceMapping.MappingWeight = 1.0f;
    RelevanceMapping.ActivationThreshold = 0.3f;
    ExpressionMappings.Add(RelevanceMapping);

    // Embodied -> Body tension
    FCognitiveExpressionMapping EmbodiedMapping;
    EmbodiedMapping.CognitiveParameter = TEXT("Embodied");
    EmbodiedMapping.ExpressionChannel = EAvatarExpressionChannel::Body;
    EmbodiedMapping.MappingWeight = 0.9f;
    EmbodiedMapping.ActivationThreshold = 0.2f;
    ExpressionMappings.Add(EmbodiedMapping);

    // Enacted -> Gesture
    FCognitiveExpressionMapping EnactedMapping;
    EnactedMapping.CognitiveParameter = TEXT("Enacted");
    EnactedMapping.ExpressionChannel = EAvatarExpressionChannel::Gesture;
    EnactedMapping.MappingWeight = 0.85f;
    EnactedMapping.ActivationThreshold = 0.25f;
    ExpressionMappings.Add(EnactedMapping);

    // Entelechy -> Posture
    FCognitiveExpressionMapping EntelechyMapping;
    EntelechyMapping.CognitiveParameter = TEXT("Entelechy");
    EntelechyMapping.ExpressionChannel = EAvatarExpressionChannel::Posture;
    EntelechyMapping.MappingWeight = 0.7f;
    EntelechyMapping.ActivationThreshold = 0.15f;
    ExpressionMappings.Add(EntelechyMapping);

    // Emotion -> Aura
    FCognitiveExpressionMapping EmotionMapping;
    EmotionMapping.CognitiveParameter = TEXT("EmotionIntensity");
    EmotionMapping.ExpressionChannel = EAvatarExpressionChannel::Aura;
    EmotionMapping.MappingWeight = 1.0f;
    EmotionMapping.ActivationThreshold = 0.1f;
    ExpressionMappings.Add(EmotionMapping);
}

void UUnrealAvatarCognition::SetEmotionState(EAvatarEmotionState NewState, float Intensity)
{
    EAvatarEmotionState OldState = State.EmotionState;
    State.EmotionState = NewState;
    State.EmotionIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);

    if (OldState != NewState)
    {
        OnEmotionStateChanged.Broadcast(OldState, NewState);

        // Update facial expression for new emotion
        TargetFacialExpression = GetEmotionFacialExpression(NewState, State.EmotionIntensity);
        TargetBodySchema = GetEmotionBodySchema(NewState, State.EmotionIntensity);
    }
}

void UUnrealAvatarCognition::BlendToEmotionState(EAvatarEmotionState TargetState, float TargetIntensity, float BlendTime)
{
    TargetEmotionState = TargetState;
    TargetEmotionIntensity = FMath::Clamp(TargetIntensity, 0.0f, 1.0f);
    EmotionBlendTimer = BlendTime;
    EmotionTransitionDuration = BlendTime;

    // Set target expressions
    TargetFacialExpression = GetEmotionFacialExpression(TargetState, TargetIntensity);
    TargetBodySchema = GetEmotionBodySchema(TargetState, TargetIntensity);
}

EAvatarEmotionState UUnrealAvatarCognition::GetEmotionState() const
{
    return State.EmotionState;
}

float UUnrealAvatarCognition::GetEmotionIntensity() const
{
    return State.EmotionIntensity;
}

void UUnrealAvatarCognition::UpdateFacialExpression(const FFacialExpressionState& NewExpression)
{
    TargetFacialExpression = NewExpression;
    OnExpressionUpdated.Broadcast(EAvatarExpressionChannel::Facial);
}

void UUnrealAvatarCognition::UpdateBodySchema(const FAvatarBodySchema& NewSchema)
{
    TargetBodySchema = NewSchema;
    OnExpressionUpdated.Broadcast(EAvatarExpressionChannel::Body);
}

void UUnrealAvatarCognition::UpdateVoiceModulation(const FVoiceModulationState& NewModulation)
{
    State.VoiceModulation = NewModulation;
    OnExpressionUpdated.Broadcast(EAvatarExpressionChannel::Voice);
}

void UUnrealAvatarCognition::SetGazeTarget(const FVector& WorldTarget)
{
    State.BodySchema.GazeTarget = WorldTarget;
    OnExpressionUpdated.Broadcast(EAvatarExpressionChannel::Gaze);
}

void UUnrealAvatarCognition::SetHeadOrientation(const FRotator& Orientation)
{
    State.BodySchema.HeadOrientation = Orientation;
}

void UUnrealAvatarCognition::ProcessEchobeatsState(const FEchobeatsFullState& EchobeatsState)
{
    State.CurrentEchobeatStep = EchobeatsState.CurrentStep;
    State.CognitiveCoherence = EchobeatsState.OverallCoherence;

    // Map echobeats state to emotion
    ComputeEmotionFromCognition();

    // Update 4E state based on stream activations
    for (const FCognitiveStreamState& StreamState : EchobeatsState.StreamStates)
    {
        switch (StreamState.StreamType)
        {
        case ECognitiveStreamType::Pivotal:
            State.E4State.Add(TEXT("Embodied"), StreamState.ActivationLevel);
            break;
        case ECognitiveStreamType::Affordance:
            State.E4State.Add(TEXT("Embedded"), StreamState.ActivationLevel);
            break;
        case ECognitiveStreamType::Salience:
            State.E4State.Add(TEXT("Enacted"), StreamState.ActivationLevel);
            break;
        }
    }

    OnCognitiveStateChanged.Broadcast(TEXT("EchobeatsStep"), (float)EchobeatsState.CurrentStep);
}

void UUnrealAvatarCognition::ProcessSys6State(const FSys6FullState& Sys6State)
{
    State.CurrentSys6Step = Sys6State.CurrentStep;
    State.EntelechyLevel = Sys6State.Entelechy;

    // Update extended cognition based on sys6 state
    State.E4State.Add(TEXT("Extended"), Sys6State.Coherence);

    OnCognitiveStateChanged.Broadcast(TEXT("Sys6Step"), (float)Sys6State.CurrentStep);
}

void UUnrealAvatarCognition::Update4EState(const TMap<FString, float>& E4Values)
{
    for (const auto& Pair : E4Values)
    {
        State.E4State.Add(Pair.Key, Pair.Value);
        OnCognitiveStateChanged.Broadcast(Pair.Key, Pair.Value);
    }
}

void UUnrealAvatarCognition::ComputeExpressionFromCognition()
{
    // Compute emotion from cognitive state
    ComputeEmotionFromCognition();

    // Get target expressions for current emotion
    TargetFacialExpression = GetEmotionFacialExpression(State.EmotionState, State.EmotionIntensity);
    TargetBodySchema = GetEmotionBodySchema(State.EmotionState, State.EmotionIntensity);

    // Modulate based on 4E state
    float* Embodied = State.E4State.Find(TEXT("Embodied"));
    float* Enacted = State.E4State.Find(TEXT("Enacted"));

    if (Embodied)
    {
        TargetBodySchema.ShoulderTension *= *Embodied;
        TargetBodySchema.BreathingDepth = FMath::Lerp(0.3f, 0.8f, *Embodied);
    }

    if (Enacted)
    {
        TargetBodySchema.HandExpressiveness = FMath::Lerp(0.1f, 0.9f, *Enacted);
    }

    // Modulate based on coherence
    TargetFacialExpression.EyeOpenness = FMath::Lerp(0.5f, 0.9f, State.CognitiveCoherence);

    // Modulate based on entelechy
    TargetBodySchema.SpineCurvature = FMath::Lerp(0.3f, 0.0f, State.EntelechyLevel);
}

void UUnrealAvatarCognition::UpdateExpressionFromMappings()
{
    for (const FCognitiveExpressionMapping& Mapping : ExpressionMappings)
    {
        float CognitiveValue = 0.0f;

        // Get cognitive parameter value
        if (Mapping.CognitiveParameter == TEXT("Coherence"))
        {
            CognitiveValue = State.CognitiveCoherence;
        }
        else if (Mapping.CognitiveParameter == TEXT("Entelechy"))
        {
            CognitiveValue = State.EntelechyLevel;
        }
        else if (Mapping.CognitiveParameter == TEXT("EmotionIntensity"))
        {
            CognitiveValue = State.EmotionIntensity;
        }
        else if (float* E4Value = State.E4State.Find(Mapping.CognitiveParameter))
        {
            CognitiveValue = *E4Value;
        }

        // Check threshold
        if (CognitiveValue < Mapping.ActivationThreshold)
        {
            continue;
        }

        // Apply mapping weight
        float MappedValue = CognitiveValue * Mapping.MappingWeight;

        // Apply to expression channel
        switch (Mapping.ExpressionChannel)
        {
        case EAvatarExpressionChannel::Facial:
            TargetFacialExpression.EyeOpenness = FMath::Lerp(TargetFacialExpression.EyeOpenness, MappedValue, 0.5f);
            break;
        case EAvatarExpressionChannel::Body:
            TargetBodySchema.ShoulderTension = FMath::Lerp(TargetBodySchema.ShoulderTension, MappedValue, 0.5f);
            break;
        case EAvatarExpressionChannel::Posture:
            TargetBodySchema.SpineCurvature = FMath::Lerp(TargetBodySchema.SpineCurvature, 1.0f - MappedValue, 0.5f);
            break;
        case EAvatarExpressionChannel::Gesture:
            TargetBodySchema.HandExpressiveness = FMath::Lerp(TargetBodySchema.HandExpressiveness, MappedValue, 0.5f);
            break;
        default:
            break;
        }
    }
}

void UUnrealAvatarCognition::BlendExpressionStates(float DeltaTime)
{
    float BlendSpeed = 1.0f - ExpressionSmoothingFactor;
    float BlendAlpha = FMath::Clamp(BlendSpeed * DeltaTime * 10.0f, 0.0f, 1.0f);

    // Blend facial expression
    State.FacialExpression.BrowRaise = FMath::Lerp(State.FacialExpression.BrowRaise, TargetFacialExpression.BrowRaise, BlendAlpha);
    State.FacialExpression.BrowFurrow = FMath::Lerp(State.FacialExpression.BrowFurrow, TargetFacialExpression.BrowFurrow, BlendAlpha);
    State.FacialExpression.EyeOpenness = FMath::Lerp(State.FacialExpression.EyeOpenness, TargetFacialExpression.EyeOpenness, BlendAlpha);
    State.FacialExpression.Smile = FMath::Lerp(State.FacialExpression.Smile, TargetFacialExpression.Smile, BlendAlpha);
    State.FacialExpression.MouthOpenness = FMath::Lerp(State.FacialExpression.MouthOpenness, TargetFacialExpression.MouthOpenness, BlendAlpha);
    State.FacialExpression.LipTension = FMath::Lerp(State.FacialExpression.LipTension, TargetFacialExpression.LipTension, BlendAlpha);
    State.FacialExpression.CheekRaise = FMath::Lerp(State.FacialExpression.CheekRaise, TargetFacialExpression.CheekRaise, BlendAlpha);
    State.FacialExpression.NoseWrinkle = FMath::Lerp(State.FacialExpression.NoseWrinkle, TargetFacialExpression.NoseWrinkle, BlendAlpha);
    State.FacialExpression.JawClench = FMath::Lerp(State.FacialExpression.JawClench, TargetFacialExpression.JawClench, BlendAlpha);

    // Blend body schema
    State.BodySchema.HeadOrientation = FMath::Lerp(State.BodySchema.HeadOrientation, TargetBodySchema.HeadOrientation, BlendAlpha);
    State.BodySchema.SpineCurvature = FMath::Lerp(State.BodySchema.SpineCurvature, TargetBodySchema.SpineCurvature, BlendAlpha);
    State.BodySchema.ShoulderTension = FMath::Lerp(State.BodySchema.ShoulderTension, TargetBodySchema.ShoulderTension, BlendAlpha);
    State.BodySchema.ArmOpenness = FMath::Lerp(State.BodySchema.ArmOpenness, TargetBodySchema.ArmOpenness, BlendAlpha);
    State.BodySchema.HandExpressiveness = FMath::Lerp(State.BodySchema.HandExpressiveness, TargetBodySchema.HandExpressiveness, BlendAlpha);
    State.BodySchema.StanceWidth = FMath::Lerp(State.BodySchema.StanceWidth, TargetBodySchema.StanceWidth, BlendAlpha);
    State.BodySchema.WeightDistribution = FMath::Lerp(State.BodySchema.WeightDistribution, TargetBodySchema.WeightDistribution, BlendAlpha);
    State.BodySchema.BreathingRate = FMath::Lerp(State.BodySchema.BreathingRate, TargetBodySchema.BreathingRate, BlendAlpha);
    State.BodySchema.BreathingDepth = FMath::Lerp(State.BodySchema.BreathingDepth, TargetBodySchema.BreathingDepth, BlendAlpha);
}

void UUnrealAvatarCognition::ApplyExpressionToAvatar()
{
    if (!SkeletalMesh) return;

    // Apply morph targets for facial expression
    // These would map to actual morph target names in the skeletal mesh
    SkeletalMesh->SetMorphTarget(TEXT("BrowRaise"), State.FacialExpression.BrowRaise);
    SkeletalMesh->SetMorphTarget(TEXT("BrowFurrow"), State.FacialExpression.BrowFurrow);
    SkeletalMesh->SetMorphTarget(TEXT("EyeOpenness"), State.FacialExpression.EyeOpenness);
    SkeletalMesh->SetMorphTarget(TEXT("Smile"), State.FacialExpression.Smile);
    SkeletalMesh->SetMorphTarget(TEXT("MouthOpen"), State.FacialExpression.MouthOpenness);
    SkeletalMesh->SetMorphTarget(TEXT("LipTension"), State.FacialExpression.LipTension);
    SkeletalMesh->SetMorphTarget(TEXT("CheekRaise"), State.FacialExpression.CheekRaise);
    SkeletalMesh->SetMorphTarget(TEXT("NoseWrinkle"), State.FacialExpression.NoseWrinkle);
    SkeletalMesh->SetMorphTarget(TEXT("JawClench"), State.FacialExpression.JawClench);

    // Body schema would be applied through animation blueprint parameters
    // This is a placeholder for the actual implementation
}

void UUnrealAvatarCognition::ComputeEmotionFromCognition()
{
    // Compute emotion based on cognitive state
    float Coherence = State.CognitiveCoherence;
    float Entelechy = State.EntelechyLevel;

    float* Embodied = State.E4State.Find(TEXT("Embodied"));
    float* Enacted = State.E4State.Find(TEXT("Enacted"));

    // High coherence + high entelechy = Determination
    if (Coherence > 0.8f && Entelechy > 0.7f)
    {
        if (State.EmotionState != EAvatarEmotionState::Determination)
        {
            BlendToEmotionState(EAvatarEmotionState::Determination, 0.7f, 0.5f);
        }
    }
    // High coherence + moderate entelechy = Focus
    else if (Coherence > 0.7f && Entelechy > 0.5f)
    {
        if (State.EmotionState != EAvatarEmotionState::Focus)
        {
            BlendToEmotionState(EAvatarEmotionState::Focus, 0.6f, 0.5f);
        }
    }
    // High enacted = Curiosity
    else if (Enacted && *Enacted > 0.7f)
    {
        if (State.EmotionState != EAvatarEmotionState::Curiosity)
        {
            BlendToEmotionState(EAvatarEmotionState::Curiosity, 0.65f, 0.5f);
        }
    }
    // High embodied = Serenity
    else if (Embodied && *Embodied > 0.7f)
    {
        if (State.EmotionState != EAvatarEmotionState::Serenity)
        {
            BlendToEmotionState(EAvatarEmotionState::Serenity, 0.5f, 0.5f);
        }
    }
    // Low coherence = Contemplation
    else if (Coherence < 0.4f)
    {
        if (State.EmotionState != EAvatarEmotionState::Contemplation)
        {
            BlendToEmotionState(EAvatarEmotionState::Contemplation, 0.4f, 0.5f);
        }
    }
}

FFacialExpressionState UUnrealAvatarCognition::GetEmotionFacialExpression(EAvatarEmotionState Emotion, float Intensity)
{
    FFacialExpressionState Expression;

    switch (Emotion)
    {
    case EAvatarEmotionState::Joy:
        Expression.Smile = 0.8f * Intensity;
        Expression.CheekRaise = 0.6f * Intensity;
        Expression.EyeOpenness = 0.7f;
        Expression.BrowRaise = 0.3f * Intensity;
        break;

    case EAvatarEmotionState::Wonder:
        Expression.BrowRaise = 0.7f * Intensity;
        Expression.EyeOpenness = 0.9f * Intensity;
        Expression.MouthOpenness = 0.3f * Intensity;
        break;

    case EAvatarEmotionState::Curiosity:
        Expression.BrowRaise = 0.4f * Intensity;
        Expression.EyeOpenness = 0.8f;
        Expression.Smile = 0.2f * Intensity;
        break;

    case EAvatarEmotionState::Focus:
        Expression.BrowFurrow = 0.3f * Intensity;
        Expression.EyeOpenness = 0.75f;
        Expression.LipTension = 0.2f * Intensity;
        break;

    case EAvatarEmotionState::Concern:
        Expression.BrowFurrow = 0.5f * Intensity;
        Expression.EyeOpenness = 0.65f;
        Expression.LipTension = 0.4f * Intensity;
        break;

    case EAvatarEmotionState::Contemplation:
        Expression.EyeOpenness = 0.6f;
        Expression.BrowFurrow = 0.2f * Intensity;
        break;

    case EAvatarEmotionState::Excitement:
        Expression.Smile = 0.7f * Intensity;
        Expression.EyeOpenness = 0.85f;
        Expression.BrowRaise = 0.5f * Intensity;
        break;

    case EAvatarEmotionState::Serenity:
        Expression.EyeOpenness = 0.65f;
        Expression.Smile = 0.3f * Intensity;
        break;

    case EAvatarEmotionState::Determination:
        Expression.BrowFurrow = 0.4f * Intensity;
        Expression.JawClench = 0.3f * Intensity;
        Expression.EyeOpenness = 0.75f;
        break;

    default: // Neutral
        Expression.EyeOpenness = 0.7f;
        break;
    }

    return Expression;
}

FAvatarBodySchema UUnrealAvatarCognition::GetEmotionBodySchema(EAvatarEmotionState Emotion, float Intensity)
{
    FAvatarBodySchema Schema;

    switch (Emotion)
    {
    case EAvatarEmotionState::Joy:
        Schema.SpineCurvature = 0.0f;
        Schema.ShoulderTension = 0.1f;
        Schema.ArmOpenness = 0.7f * Intensity;
        Schema.HandExpressiveness = 0.6f * Intensity;
        Schema.BreathingRate = 14.0f;
        Schema.BreathingDepth = 0.6f;
        break;

    case EAvatarEmotionState::Wonder:
        Schema.SpineCurvature = -0.1f; // Slight lean forward
        Schema.ShoulderTension = 0.2f;
        Schema.ArmOpenness = 0.5f;
        Schema.HandExpressiveness = 0.4f * Intensity;
        Schema.BreathingRate = 10.0f;
        Schema.BreathingDepth = 0.7f;
        break;

    case EAvatarEmotionState::Curiosity:
        Schema.SpineCurvature = -0.15f; // Lean forward
        Schema.ShoulderTension = 0.15f;
        Schema.ArmOpenness = 0.4f;
        Schema.HandExpressiveness = 0.5f * Intensity;
        Schema.BreathingRate = 13.0f;
        Schema.BreathingDepth = 0.5f;
        break;

    case EAvatarEmotionState::Focus:
        Schema.SpineCurvature = 0.0f;
        Schema.ShoulderTension = 0.3f * Intensity;
        Schema.ArmOpenness = 0.3f;
        Schema.HandExpressiveness = 0.2f;
        Schema.BreathingRate = 11.0f;
        Schema.BreathingDepth = 0.6f;
        break;

    case EAvatarEmotionState::Concern:
        Schema.SpineCurvature = 0.1f; // Slight slouch
        Schema.ShoulderTension = 0.5f * Intensity;
        Schema.ArmOpenness = 0.3f;
        Schema.HandExpressiveness = 0.3f;
        Schema.BreathingRate = 14.0f;
        Schema.BreathingDepth = 0.4f;
        break;

    case EAvatarEmotionState::Contemplation:
        Schema.SpineCurvature = 0.05f;
        Schema.ShoulderTension = 0.1f;
        Schema.ArmOpenness = 0.4f;
        Schema.HandExpressiveness = 0.2f;
        Schema.BreathingRate = 10.0f;
        Schema.BreathingDepth = 0.7f;
        break;

    case EAvatarEmotionState::Excitement:
        Schema.SpineCurvature = -0.05f;
        Schema.ShoulderTension = 0.2f;
        Schema.ArmOpenness = 0.8f * Intensity;
        Schema.HandExpressiveness = 0.8f * Intensity;
        Schema.BreathingRate = 16.0f;
        Schema.BreathingDepth = 0.5f;
        break;

    case EAvatarEmotionState::Serenity:
        Schema.SpineCurvature = 0.0f;
        Schema.ShoulderTension = 0.05f;
        Schema.ArmOpenness = 0.5f;
        Schema.HandExpressiveness = 0.1f;
        Schema.BreathingRate = 8.0f;
        Schema.BreathingDepth = 0.8f;
        break;

    case EAvatarEmotionState::Determination:
        Schema.SpineCurvature = -0.05f;
        Schema.ShoulderTension = 0.4f * Intensity;
        Schema.ArmOpenness = 0.4f;
        Schema.HandExpressiveness = 0.3f;
        Schema.StanceWidth = 0.6f;
        Schema.BreathingRate = 12.0f;
        Schema.BreathingDepth = 0.65f;
        break;

    default: // Neutral
        Schema.SpineCurvature = 0.0f;
        Schema.ShoulderTension = 0.15f;
        Schema.ArmOpenness = 0.5f;
        Schema.HandExpressiveness = 0.3f;
        Schema.BreathingRate = 12.0f;
        Schema.BreathingDepth = 0.5f;
        break;
    }

    return Schema;
}

// State query implementations
FAvatarCognitiveState UUnrealAvatarCognition::GetCognitiveState() const
{
    return State;
}

FAvatarBodySchema UUnrealAvatarCognition::GetBodySchema() const
{
    return State.BodySchema;
}

FFacialExpressionState UUnrealAvatarCognition::GetFacialExpression() const
{
    return State.FacialExpression;
}

FVoiceModulationState UUnrealAvatarCognition::GetVoiceModulation() const
{
    return State.VoiceModulation;
}

float UUnrealAvatarCognition::GetCognitiveCoherence() const
{
    return State.CognitiveCoherence;
}

float UUnrealAvatarCognition::GetEntelechyLevel() const
{
    return State.EntelechyLevel;
}

void UUnrealAvatarCognition::HandleEchobeatStepChanged(int32 OldStep, int32 NewStep)
{
    State.CurrentEchobeatStep = NewStep;

    // Get full echobeats state and process
    if (EchobeatsEngine)
    {
        ProcessEchobeatsState(EchobeatsEngine->GetFullState());
    }
}

void UUnrealAvatarCognition::HandleRelevanceRealized(int32 Step, float RelevanceLevel)
{
    // Trigger relevance realization event
    OnRelevanceRealizationTriggered.Broadcast(RelevanceLevel);

    // Potentially trigger emotion transition
    if (RelevanceLevel > 0.8f)
    {
        BlendToEmotionState(EAvatarEmotionState::Wonder, 0.7f, 0.3f);
    }
    else if (RelevanceLevel > 0.6f)
    {
        BlendToEmotionState(EAvatarEmotionState::Curiosity, 0.6f, 0.4f);
    }
}

void UUnrealAvatarCognition::HandleStreamActivated(ECognitiveStreamType StreamType, float ActivationLevel)
{
    // Update 4E state based on stream activation
    switch (StreamType)
    {
    case ECognitiveStreamType::Pivotal:
        State.E4State.Add(TEXT("Embodied"), ActivationLevel);
        break;
    case ECognitiveStreamType::Affordance:
        State.E4State.Add(TEXT("Embedded"), ActivationLevel);
        break;
    case ECognitiveStreamType::Salience:
        State.E4State.Add(TEXT("Enacted"), ActivationLevel);
        break;
    }
}

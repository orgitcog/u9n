#include "DeepTreeEchoUnrealBridge.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetMathLibrary.h"

/**
 * Deep Tree Echo Unreal Bridge Implementation
 * 
 * Implements the comprehensive integration between Deep Tree Echo cognitive
 * framework and Unreal Engine systems for 4E embodied cognition.
 */

UDeepTreeEchoUnrealBridge::UDeepTreeEchoUnrealBridge()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    // Initialize default motor mapping
    MotorMapping.ModeToAnimationMap.Add(TEXT("Reactive"), TEXT("Idle_Alert"));
    MotorMapping.ModeToAnimationMap.Add(TEXT("Deliberative"), TEXT("Idle_Thinking"));
    MotorMapping.ModeToAnimationMap.Add(TEXT("Reflective"), TEXT("Idle_Contemplative"));
    MotorMapping.ModeToAnimationMap.Add(TEXT("Creative"), TEXT("Idle_Expressive"));
    MotorMapping.ModeToAnimationMap.Add(TEXT("Integrative"), TEXT("Idle_Focused"));

    MotorMapping.EmotionToGestureMap.Add(TEXT("Joy"), TEXT("Gesture_Happy"));
    MotorMapping.EmotionToGestureMap.Add(TEXT("Curiosity"), TEXT("Gesture_Interested"));
    MotorMapping.EmotionToGestureMap.Add(TEXT("Contemplation"), TEXT("Gesture_Thinking"));
    MotorMapping.EmotionToGestureMap.Add(TEXT("Wonder"), TEXT("Gesture_Amazed"));
    MotorMapping.EmotionToGestureMap.Add(TEXT("Concern"), TEXT("Gesture_Worried"));

    // Initialize default expression
    CurrentExpression.EmotionIntensities.Add(TEXT("Neutral"), 1.0f);
}

void UDeepTreeEchoUnrealBridge::BeginPlay()
{
    Super::BeginPlay();
    InitializeComponentReferences();
}

void UDeepTreeEchoUnrealBridge::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (IntegrationMode == EUnrealIntegrationMode::Minimal)
    {
        return;
    }

    // Process sensory inputs
    if (IntegrationMode == EUnrealIntegrationMode::Full || IntegrationMode == EUnrealIntegrationMode::CognitiveOnly)
    {
        ProcessAllSensoryInputs();
    }

    // Update motor outputs
    if (IntegrationMode == EUnrealIntegrationMode::Full || IntegrationMode == EUnrealIntegrationMode::AvatarOnly)
    {
        UpdateMotorOutputs(DeltaTime);
    }

    // Draw debug visualization if enabled
    if (bEnableCognitiveVisualization)
    {
        DrawCognitiveVisualization();
    }
}

void UDeepTreeEchoUnrealBridge::InitializeBridge()
{
    InitializeComponentReferences();

    if (CognitiveCore)
    {
        CognitiveCore->InitializeSystem();
    }

    if (ReservoirSystem)
    {
        ReservoirSystem->InitializeReservoir();
    }

    if (EmbodiedCognition)
    {
        EmbodiedCognition->Initialize();
    }

    UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoUnrealBridge: Bridge initialized successfully"));
}

void UDeepTreeEchoUnrealBridge::InitializeComponentReferences()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    // Find cognitive components
    CognitiveCore = Owner->FindComponentByClass<UDeepTreeEchoCore>();
    ReservoirSystem = Owner->FindComponentByClass<UDeepTreeEchoReservoir>();
    EmbodiedCognition = Owner->FindComponentByClass<UEmbodiedCognitionComponent>();

    // Find avatar components
    AvatarMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
    if (AvatarMesh)
    {
        AnimInstance = AvatarMesh->GetAnimInstance();
    }
}

void UDeepTreeEchoUnrealBridge::SetAvatarMesh(USkeletalMeshComponent* InMesh)
{
    AvatarMesh = InMesh;
    if (AvatarMesh)
    {
        AnimInstance = AvatarMesh->GetAnimInstance();
    }
}

// ========================================
// SENSORY INPUT PROCESSING
// ========================================

void UDeepTreeEchoUnrealBridge::ProcessAllSensoryInputs()
{
    if (SensoryConfig.bEnableProprioceptiveInput)
    {
        ProcessProprioceptiveInput();
    }
}

void UDeepTreeEchoUnrealBridge::ProcessVisualInput(const TArray<AActor*>& VisibleActors)
{
    if (!SensoryConfig.bEnableVisualInput || !CognitiveCore)
    {
        return;
    }

    // Convert visible actors to sensory data
    VisualSensoryBuffer.Reset();
    
    for (const AActor* Actor : VisibleActors)
    {
        if (!Actor) continue;

        FVector Direction = Actor->GetActorLocation() - GetOwner()->GetActorLocation();
        float Distance = Direction.Size();
        Direction.Normalize();

        // Encode: direction (3), distance (1), type hash (1)
        VisualSensoryBuffer.Add(Direction.X);
        VisualSensoryBuffer.Add(Direction.Y);
        VisualSensoryBuffer.Add(Direction.Z);
        VisualSensoryBuffer.Add(FMath::Clamp(1.0f - (Distance / 5000.0f), 0.0f, 1.0f));
        VisualSensoryBuffer.Add(FMath::Frac(GetTypeHash(Actor->GetClass()->GetName()) / 1000000.0f));
    }

    // Send to cognitive core
    CognitiveCore->ProcessSensoryInput(VisualSensoryBuffer, TEXT("Visual"));
}

void UDeepTreeEchoUnrealBridge::ProcessAuditoryInput(const TArray<FVector>& SoundSources, const TArray<float>& SoundIntensities)
{
    if (!SensoryConfig.bEnableAuditoryInput || !CognitiveCore)
    {
        return;
    }

    AuditorySensoryBuffer.Reset();

    for (int32 i = 0; i < SoundSources.Num() && i < SoundIntensities.Num(); ++i)
    {
        FVector Direction = SoundSources[i] - GetOwner()->GetActorLocation();
        float Distance = Direction.Size();

        if (Distance > SensoryConfig.AuditoryRange)
        {
            continue;
        }

        Direction.Normalize();

        // Encode: direction (3), intensity (1), distance factor (1)
        AuditorySensoryBuffer.Add(Direction.X);
        AuditorySensoryBuffer.Add(Direction.Y);
        AuditorySensoryBuffer.Add(Direction.Z);
        AuditorySensoryBuffer.Add(SoundIntensities[i]);
        AuditorySensoryBuffer.Add(1.0f - (Distance / SensoryConfig.AuditoryRange));
    }

    CognitiveCore->ProcessSensoryInput(AuditorySensoryBuffer, TEXT("Auditory"));
}

void UDeepTreeEchoUnrealBridge::ProcessProprioceptiveInput()
{
    if (!AvatarMesh || !CognitiveCore)
    {
        return;
    }

    ProprioceptiveBuffer.Reset();

    // Get bone transforms for proprioception
    TArray<FName> BoneNames = {
        TEXT("spine_01"), TEXT("spine_02"), TEXT("spine_03"),
        TEXT("head"), TEXT("neck_01"),
        TEXT("upperarm_l"), TEXT("upperarm_r"),
        TEXT("lowerarm_l"), TEXT("lowerarm_r"),
        TEXT("hand_l"), TEXT("hand_r"),
        TEXT("thigh_l"), TEXT("thigh_r"),
        TEXT("calf_l"), TEXT("calf_r")
    };

    for (const FName& BoneName : BoneNames)
    {
        int32 BoneIndex = AvatarMesh->GetBoneIndex(BoneName);
        if (BoneIndex != INDEX_NONE)
        {
            FTransform BoneTransform = AvatarMesh->GetBoneTransform(BoneIndex);
            FRotator BoneRotation = BoneTransform.Rotator();

            // Encode rotation as normalized values
            ProprioceptiveBuffer.Add(BoneRotation.Pitch / 180.0f);
            ProprioceptiveBuffer.Add(BoneRotation.Yaw / 180.0f);
            ProprioceptiveBuffer.Add(BoneRotation.Roll / 180.0f);
        }
    }

    // Update embodied cognition component
    if (EmbodiedCognition)
    {
        TArray<float> InteroceptiveData; // Would come from physiological simulation
        EmbodiedCognition->UpdateEmbodiedState(ProprioceptiveBuffer, InteroceptiveData);
    }

    CognitiveCore->ProcessSensoryInput(ProprioceptiveBuffer, TEXT("Proprioceptive"));
}

void UDeepTreeEchoUnrealBridge::ProcessEnvironmentalInput(const TArray<FString>& Affordances)
{
    if (!SensoryConfig.bEnableEnvironmentalInput || !CognitiveCore)
    {
        return;
    }

    AffordanceBuffer = Affordances;

    // Update embedded state in 4E cognition
    if (EmbodiedCognition)
    {
        FString CurrentNiche = TEXT("Default");
        if (Affordances.Num() > 0)
        {
            // Determine niche from affordances
            if (Affordances.Contains(TEXT("Sit")) && Affordances.Contains(TEXT("Rest")))
            {
                CurrentNiche = TEXT("RestArea");
            }
            else if (Affordances.Contains(TEXT("Interact")) && Affordances.Contains(TEXT("Communicate")))
            {
                CurrentNiche = TEXT("SocialSpace");
            }
            else if (Affordances.Contains(TEXT("Explore")) && Affordances.Contains(TEXT("Navigate")))
            {
                CurrentNiche = TEXT("ExplorationZone");
            }
        }

        EmbodiedCognition->UpdateEmbeddedState(Affordances, CurrentNiche);
    }
}

void UDeepTreeEchoUnrealBridge::ProcessSocialInput(const TArray<AActor*>& NearbyCharacters)
{
    if (!SensoryConfig.bEnableSocialInput || !CognitiveCore)
    {
        return;
    }

    SocialContextBuffer.Reset();

    for (const AActor* Character : NearbyCharacters)
    {
        if (!Character) continue;

        // Extract social context
        FString CharacterName = Character->GetName();
        FVector RelativePosition = Character->GetActorLocation() - GetOwner()->GetActorLocation();
        float Distance = RelativePosition.Size();

        FString SocialContext = FString::Printf(TEXT("%s:%.0f"), *CharacterName, Distance);
        SocialContextBuffer.Add(SocialContext);
    }

    // Update extended cognition with social context
    if (EmbodiedCognition)
    {
        TArray<FString> EmptyTools;
        EmbodiedCognition->UpdateExtendedState(SocialContextBuffer, EmptyTools);
    }
}

// ========================================
// MOTOR OUTPUT
// ========================================

void UDeepTreeEchoUnrealBridge::UpdateMotorOutputs(float DeltaTime)
{
    ExpressionUpdateTimer += DeltaTime;

    if (ExpressionUpdateTimer >= ExpressionUpdateInterval)
    {
        ExpressionUpdateTimer = 0.0f;
        UpdateAvatarExpression();
        UpdateBodyPosture();
    }
}

void UDeepTreeEchoUnrealBridge::UpdateAvatarExpression()
{
    CurrentExpression = MapCognitiveStateToExpression();
    ApplyFacialBlendShapes(CurrentExpression.FacialBlendShapes);

    // Update gaze
    FVector GazeTarget = ComputeGazeFromAttention();
    SetGazeTarget(GazeTarget);
}

void UDeepTreeEchoUnrealBridge::ApplyFacialBlendShapes(const TMap<FString, float>& BlendShapes)
{
    if (!AvatarMesh)
    {
        return;
    }

    for (const auto& Pair : BlendShapes)
    {
        // Apply morph target
        AvatarMesh->SetMorphTarget(FName(*Pair.Key), Pair.Value);
    }
}

void UDeepTreeEchoUnrealBridge::SetGazeTarget(const FVector& TargetLocation)
{
    CurrentExpression.GazeDirection = (TargetLocation - GetOwner()->GetActorLocation()).GetSafeNormal();

    // Apply to animation instance if available
    if (AnimInstance)
    {
        // Set gaze target via animation blueprint variable
        // This would typically be done through a custom AnimInstance class
    }
}

void UDeepTreeEchoUnrealBridge::TriggerGesture(const FString& GestureName, float Intensity)
{
    if (!AnimInstance)
    {
        return;
    }

    // Find and play gesture montage
    // This would typically load from a data asset
    UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoUnrealBridge: Triggering gesture %s with intensity %.2f"), *GestureName, Intensity);
}

void UDeepTreeEchoUnrealBridge::UpdateBodyPosture()
{
    if (!CognitiveCore || !AnimInstance)
    {
        return;
    }

    // Map cognitive mode to posture
    FString ModeString = GetCurrentCognitiveModeString();
    if (MotorMapping.ModeToAnimationMap.Contains(ModeString))
    {
        FString AnimState = MotorMapping.ModeToAnimationMap[ModeString];
        // Apply animation state
    }
}

FAvatarExpressionState UDeepTreeEchoUnrealBridge::MapCognitiveStateToExpression() const
{
    FAvatarExpressionState Expression;

    if (!CognitiveCore)
    {
        Expression.EmotionIntensities.Add(TEXT("Neutral"), 1.0f);
        return Expression;
    }

    // Get relevance state for emotional mapping
    const FRelevanceRealizationState& RelevanceState = CognitiveCore->RelevanceState;

    // Map salience to emotions
    float TotalSalience = 0.0f;
    for (const auto& Pair : RelevanceState.SalienceLandscape)
    {
        TotalSalience += Pair.Value;
    }

    // Determine primary emotion from cognitive state
    ECognitiveMode Mode = CognitiveCore->CurrentMode;
    switch (Mode)
    {
    case ECognitiveMode::Reactive:
        Expression.EmotionIntensities.Add(TEXT("Alert"), 0.7f);
        Expression.EmotionIntensities.Add(TEXT("Neutral"), 0.3f);
        break;
    case ECognitiveMode::Deliberative:
        Expression.EmotionIntensities.Add(TEXT("Focused"), 0.8f);
        Expression.EmotionIntensities.Add(TEXT("Contemplation"), 0.2f);
        break;
    case ECognitiveMode::Reflective:
        Expression.EmotionIntensities.Add(TEXT("Contemplation"), 0.7f);
        Expression.EmotionIntensities.Add(TEXT("Wonder"), 0.3f);
        break;
    case ECognitiveMode::Creative:
        Expression.EmotionIntensities.Add(TEXT("Joy"), 0.5f);
        Expression.EmotionIntensities.Add(TEXT("Curiosity"), 0.5f);
        break;
    case ECognitiveMode::Integrative:
        Expression.EmotionIntensities.Add(TEXT("Serenity"), 0.6f);
        Expression.EmotionIntensities.Add(TEXT("Focus"), 0.4f);
        break;
    }

    // Map emotions to facial blend shapes
    for (const auto& EmotionPair : Expression.EmotionIntensities)
    {
        TMap<FString, float> EmotionBlendShapes = MapEmotionToBlendShapes(EmotionPair.Key, EmotionPair.Value);
        for (const auto& BlendPair : EmotionBlendShapes)
        {
            if (Expression.FacialBlendShapes.Contains(BlendPair.Key))
            {
                Expression.FacialBlendShapes[BlendPair.Key] = FMath::Max(Expression.FacialBlendShapes[BlendPair.Key], BlendPair.Value);
            }
            else
            {
                Expression.FacialBlendShapes.Add(BlendPair.Key, BlendPair.Value);
            }
        }
    }

    // Set expression intensity based on relevance coherence
    Expression.ExpressionIntensity = RelevanceState.RelevanceCoherence;

    // Modulate blink rate based on cognitive load
    float CognitiveLoad = 1.0f - RelevanceState.RelevanceCoherence;
    Expression.BlinkRateModifier = 1.0f + (CognitiveLoad * 0.5f);

    return Expression;
}

TMap<FString, float> UDeepTreeEchoUnrealBridge::MapEmotionToBlendShapes(const FString& Emotion, float Intensity) const
{
    TMap<FString, float> BlendShapes;

    if (Emotion == TEXT("Joy") || Emotion == TEXT("Happy"))
    {
        BlendShapes.Add(TEXT("Smile"), Intensity * 0.8f);
        BlendShapes.Add(TEXT("EyeSquint"), Intensity * 0.3f);
        BlendShapes.Add(TEXT("CheekRaise"), Intensity * 0.5f);
    }
    else if (Emotion == TEXT("Curiosity") || Emotion == TEXT("Interested"))
    {
        BlendShapes.Add(TEXT("BrowRaise"), Intensity * 0.6f);
        BlendShapes.Add(TEXT("EyeWiden"), Intensity * 0.4f);
        BlendShapes.Add(TEXT("LipsPart"), Intensity * 0.2f);
    }
    else if (Emotion == TEXT("Contemplation") || Emotion == TEXT("Thinking"))
    {
        BlendShapes.Add(TEXT("BrowFurrow"), Intensity * 0.3f);
        BlendShapes.Add(TEXT("EyeSquint"), Intensity * 0.2f);
        BlendShapes.Add(TEXT("LipsPurse"), Intensity * 0.3f);
    }
    else if (Emotion == TEXT("Wonder") || Emotion == TEXT("Amazed"))
    {
        BlendShapes.Add(TEXT("BrowRaise"), Intensity * 0.8f);
        BlendShapes.Add(TEXT("EyeWiden"), Intensity * 0.7f);
        BlendShapes.Add(TEXT("JawDrop"), Intensity * 0.4f);
    }
    else if (Emotion == TEXT("Alert"))
    {
        BlendShapes.Add(TEXT("EyeWiden"), Intensity * 0.5f);
        BlendShapes.Add(TEXT("BrowRaise"), Intensity * 0.3f);
    }
    else if (Emotion == TEXT("Focused") || Emotion == TEXT("Focus"))
    {
        BlendShapes.Add(TEXT("BrowFurrow"), Intensity * 0.4f);
        BlendShapes.Add(TEXT("EyeSquint"), Intensity * 0.3f);
    }
    else if (Emotion == TEXT("Serenity"))
    {
        BlendShapes.Add(TEXT("Smile"), Intensity * 0.3f);
        BlendShapes.Add(TEXT("EyeRelax"), Intensity * 0.5f);
    }
    else // Neutral
    {
        BlendShapes.Add(TEXT("Neutral"), Intensity);
    }

    return BlendShapes;
}

FVector UDeepTreeEchoUnrealBridge::ComputeGazeFromAttention() const
{
    if (!CognitiveCore)
    {
        return GetOwner()->GetActorForwardVector();
    }

    const FRelevanceRealizationState& RelevanceState = CognitiveCore->RelevanceState;

    // Find highest attention target
    FString HighestAttentionTarget;
    float HighestAttention = 0.0f;

    for (const auto& Pair : RelevanceState.AttentionAllocation)
    {
        if (Pair.Value > HighestAttention)
        {
            HighestAttention = Pair.Value;
            HighestAttentionTarget = Pair.Key;
        }
    }

    // Default to forward if no specific target
    if (HighestAttentionTarget.IsEmpty())
    {
        return GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * 500.0f;
    }

    // Would need to resolve target name to world position
    // For now, return forward with slight variation based on attention
    FVector BaseDirection = GetOwner()->GetActorForwardVector();
    return GetOwner()->GetActorLocation() + BaseDirection * 500.0f;
}

// ========================================
// COGNITIVE INTEGRATION
// ========================================

FString UDeepTreeEchoUnrealBridge::GetCurrentCognitiveModeString() const
{
    if (!CognitiveCore)
    {
        return TEXT("Unknown");
    }

    switch (CognitiveCore->CurrentMode)
    {
    case ECognitiveMode::Reactive: return TEXT("Reactive");
    case ECognitiveMode::Deliberative: return TEXT("Deliberative");
    case ECognitiveMode::Reflective: return TEXT("Reflective");
    case ECognitiveMode::Creative: return TEXT("Creative");
    case ECognitiveMode::Integrative: return TEXT("Integrative");
    default: return TEXT("Unknown");
    }
}

TArray<FString> UDeepTreeEchoUnrealBridge::GetConsciousnessStreamStates() const
{
    TArray<FString> States;

    // Would integrate with RecursiveMutualAwarenessSystem
    States.Add(TEXT("Stream1:Perceiving"));
    States.Add(TEXT("Stream2:Acting"));
    States.Add(TEXT("Stream3:Reflecting"));

    return States;
}

float UDeepTreeEchoUnrealBridge::Get4EIntegrationScore() const
{
    if (!CognitiveCore)
    {
        return 0.0f;
    }

    return CognitiveCore->Get4EIntegrationScore();
}

FString UDeepTreeEchoUnrealBridge::GetCurrentRelevanceFrame() const
{
    if (!CognitiveCore)
    {
        return TEXT("None");
    }

    return CognitiveCore->RelevanceState.RelevanceFrame;
}

TArray<float> UDeepTreeEchoUnrealBridge::GetReservoirStateSummary() const
{
    TArray<float> Summary;

    if (ReservoirSystem)
    {
        Summary = ReservoirSystem->GetReservoirStateSummary();
    }

    return Summary;
}

// ========================================
// NARRATIVE INTEGRATION
// ========================================

void UDeepTreeEchoUnrealBridge::AddDiaryEntryFromCognitiveState(const FString& Context)
{
    // Would integrate with diary system
    UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoUnrealBridge: Adding diary entry - Context: %s, Mode: %s"),
        *Context, *GetCurrentCognitiveModeString());
}

FString UDeepTreeEchoUnrealBridge::GenerateInsight()
{
    // Would integrate with insight generation system
    return TEXT("Insight: Patterns in recent experience suggest adaptive learning.");
}

float UDeepTreeEchoUnrealBridge::GetCurrentWisdomWeight() const
{
    // Would integrate with wisdom cultivation system
    if (CognitiveCore)
    {
        return CognitiveCore->RelevanceState.RelevanceCoherence;
    }
    return 0.5f;
}

// ========================================
// EVOLUTION
// ========================================

void UDeepTreeEchoUnrealBridge::TriggerDevelopmentStep()
{
    // Would integrate with AvatarEvolutionSystem
    UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoUnrealBridge: Triggering ontogenetic development step"));
}

int32 UDeepTreeEchoUnrealBridge::GetDevelopmentalStage() const
{
    // Would integrate with AvatarEvolutionSystem
    return 1;
}

float UDeepTreeEchoUnrealBridge::GetEntelechyScore() const
{
    // Compute entelechy actualization score
    float Score = 0.0f;

    if (CognitiveCore)
    {
        Score += CognitiveCore->Get4EIntegrationScore() * 0.25f;
        Score += CognitiveCore->RelevanceState.RelevanceCoherence * 0.25f;
    }

    if (ReservoirSystem)
    {
        Score += 0.25f; // Reservoir presence
    }

    if (EmbodiedCognition)
    {
        Score += 0.25f; // 4E cognition presence
    }

    return Score;
}

// ========================================
// DEBUG VISUALIZATION
// ========================================

void UDeepTreeEchoUnrealBridge::DrawCognitiveVisualization() const
{
#if WITH_EDITOR
    if (!GetWorld() || !GetOwner())
    {
        return;
    }

    FVector OwnerLocation = GetOwner()->GetActorLocation();

    // Draw gaze direction
    DrawDebugLine(GetWorld(), OwnerLocation + FVector(0, 0, 100),
        OwnerLocation + FVector(0, 0, 100) + CurrentExpression.GazeDirection * 200.0f,
        FColor::Cyan, false, -1.0f, 0, 2.0f);

    // Draw cognitive mode indicator
    FColor ModeColor;
    switch (CognitiveCore ? CognitiveCore->CurrentMode : ECognitiveMode::Reactive)
    {
    case ECognitiveMode::Reactive: ModeColor = FColor::Red; break;
    case ECognitiveMode::Deliberative: ModeColor = FColor::Yellow; break;
    case ECognitiveMode::Reflective: ModeColor = FColor::Blue; break;
    case ECognitiveMode::Creative: ModeColor = FColor::Magenta; break;
    case ECognitiveMode::Integrative: ModeColor = FColor::Green; break;
    default: ModeColor = FColor::White; break;
    }

    DrawDebugSphere(GetWorld(), OwnerLocation + FVector(0, 0, 150), 20.0f, 8, ModeColor, false, -1.0f, 0, 1.0f);

    // Draw 4E integration score as vertical bar
    float IntegrationScore = Get4EIntegrationScore();
    DrawDebugLine(GetWorld(), OwnerLocation + FVector(50, 0, 0),
        OwnerLocation + FVector(50, 0, IntegrationScore * 100.0f),
        FColor::Green, false, -1.0f, 0, 5.0f);
#endif
}

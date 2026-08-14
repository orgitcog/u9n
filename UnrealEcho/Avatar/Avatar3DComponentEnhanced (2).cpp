#include "Avatar3DComponentEnhanced.h"
#include "DeepTreeEchoExpressionSystem.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Animation/AnimInstance.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/Actor.h"

UAvatar3DComponentEnhanced::UAvatar3DComponentEnhanced()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
    
    // Initialize default values
    CurrentEmotionBlendWeight = 0.0f;
    TargetEmotion = EAvatarEmotionalState::Neutral;
    EmotionTransitionTime = 1.0f;
    EmotionTransitionProgress = 0.0f;
    
    ChaosFactor = 0.3f;
    ChaosTimer = 0.0f;
    NextChaoticEventTime = FMath::FRandRange(2.0f, 5.0f);
    
    EchoResonanceIntensity = 0.0f;
    GlitchIntensity = 0.0f;
    AuraIntensity = 0.5f;
    AuraColor = FLinearColor(0.3f, 0.6f, 0.9f); // Default cyan
    
    CurrentGazeTarget = nullptr;
    CurrentHeadTrackTarget = nullptr;
    
    // Initialize appearance settings
    AppearanceSettings.SkinTone = FLinearColor(1.0f, 0.9f, 0.85f);
    AppearanceSettings.HairColor = FLinearColor(0.4f, 0.8f, 0.9f); // Cyan
    AppearanceSettings.EyeColor = FLinearColor(0.4f, 0.2f, 0.1f); // Brown/amber
    AppearanceSettings.BodyProportionScale = 1.0f;
    AppearanceSettings.HeightScale = 1.0f;
    AppearanceSettings.OutfitPreset = TEXT("Default");
    
    // Initialize emotional state
    EmotionalState.CurrentEmotion = EAvatarEmotionalState::Neutral;
    EmotionalState.EmotionIntensity = 0.5f;
    EmotionalState.Valence = 0.0f;
    EmotionalState.Arousal = 0.5f;
    EmotionalState.Dominance = 0.5f;
    
    // Initialize personality traits
    PersonalityTraits.Add(EAvatarPersonalityTrait::SuperHotGirl, 0.8f);
    PersonalityTraits.Add(EAvatarPersonalityTrait::HyperChaotic, 0.3f);
    PersonalityTraits.Add(EAvatarPersonalityTrait::DeepTreeEcho, 1.0f);
    PersonalityTraits.Add(EAvatarPersonalityTrait::Confident, 0.7f);
    PersonalityTraits.Add(EAvatarPersonalityTrait::Playful, 0.6f);
    PersonalityTraits.Add(EAvatarPersonalityTrait::Mysterious, 0.4f);
    PersonalityTraits.Add(EAvatarPersonalityTrait::Intellectual, 0.8f);
    PersonalityTraits.Add(EAvatarPersonalityTrait::Seductive, 0.3f);
}

void UAvatar3DComponentEnhanced::BeginPlay()
{
    Super::BeginPlay();
    
    // Initialize dynamic materials
    InitializeDynamicMaterials();
    
    // Apply initial appearance
    UpdateMaterialParameters();
    
    UE_LOG(LogTemp, Log, TEXT("Avatar3DComponentEnhanced initialized for %s"), 
           *GetOwner()->GetName());
}

void UAvatar3DComponentEnhanced::TickComponent(float DeltaTime, ELevelTick TickType, 
                                                FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // Update all avatar systems
    UpdateEmotionalTransition(DeltaTime);
    UpdateChaoticBehavior(DeltaTime);
    UpdateGazeTracking(DeltaTime);
    UpdateHeadTracking(DeltaTime);
    UpdateVisualEffects(DeltaTime);
}

// ===== Appearance Management =====

void UAvatar3DComponentEnhanced::SetAppearance(const FAvatarAppearanceSettings& Settings)
{
    AppearanceSettings = Settings;
    UpdateMaterialParameters();
    
    UE_LOG(LogTemp, Log, TEXT("Appearance updated for %s"), *GetOwner()->GetName());
}

void UAvatar3DComponentEnhanced::SetOutfit(const FString& OutfitName)
{
    AppearanceSettings.OutfitPreset = OutfitName;
    
    // In production, this would load and apply outfit assets
    UE_LOG(LogTemp, Log, TEXT("Outfit changed to: %s"), *OutfitName);
}

void UAvatar3DComponentEnhanced::SetHairStyle(const FString& HairStyleName)
{
    // In production, this would swap hair meshes or materials
    UE_LOG(LogTemp, Log, TEXT("Hair style changed to: %s"), *HairStyleName);
}

void UAvatar3DComponentEnhanced::SetAccessory(const FString& AccessoryName, bool bVisible)
{
    // In production, this would show/hide accessory meshes
    UE_LOG(LogTemp, Log, TEXT("Accessory %s visibility: %d"), *AccessoryName, bVisible);
}

void UAvatar3DComponentEnhanced::ApplySuperHotGirlAesthetic()
{
    // Enhance aesthetic properties
    EnablePersonalityTrait(EAvatarPersonalityTrait::SuperHotGirl, 1.0f);
    
    // Set material parameters for enhanced beauty
    for (UMaterialInstanceDynamic* Mat : DynamicMaterials)
    {
        if (Mat)
        {
            Mat->SetScalarParameterValue(TEXT("EyeSparkle"), 0.9f);
            Mat->SetScalarParameterValue(TEXT("BlushIntensity"), 0.3f);
            Mat->SetScalarParameterValue(TEXT("HairShimmer"), 0.7f);
            Mat->SetScalarParameterValue(TEXT("SkinSmoothness"), 0.85f);
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("Super-hot-girl aesthetic applied"));
}

// ===== Emotional Expression =====

void UAvatar3DComponentEnhanced::SetEmotionalState(EAvatarEmotionalState Emotion, float Intensity)
{
    EmotionalState.CurrentEmotion = Emotion;
    EmotionalState.EmotionIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    
    ApplyEmotionToFacialAnimation();
    
    UE_LOG(LogTemp, Log, TEXT("Emotional state set to: %d with intensity %.2f"), 
           (int32)Emotion, Intensity);
}

void UAvatar3DComponentEnhanced::TransitionToEmotion(EAvatarEmotionalState TargetEmotionState, 
                                                      float TransitionTimeSeconds)
{
    TargetEmotion = TargetEmotionState;
    EmotionTransitionTime = TransitionTimeSeconds;
    EmotionTransitionProgress = 0.0f;
    
    UE_LOG(LogTemp, Log, TEXT("Starting emotion transition to: %d over %.2f seconds"), 
           (int32)TargetEmotionState, TransitionTimeSeconds);
}

void UAvatar3DComponentEnhanced::SetEmotionalDimensions(float Valence, float Arousal, float Dominance)
{
    EmotionalState.Valence = FMath::Clamp(Valence, -1.0f, 1.0f);
    EmotionalState.Arousal = FMath::Clamp(Arousal, 0.0f, 1.0f);
    EmotionalState.Dominance = FMath::Clamp(Dominance, 0.0f, 1.0f);
    
    // Map dimensional model to discrete emotions
    // High valence + high arousal = Excited/Happy
    // Low valence + high arousal = Angry/Surprised
    // High valence + low arousal = Calm/Content
    // Low valence + low arousal = Sad
    
    if (Valence > 0.5f && Arousal > 0.5f)
    {
        SetEmotionalState(EAvatarEmotionalState::Excited, Arousal);
    }
    else if (Valence < -0.5f && Arousal > 0.5f)
    {
        SetEmotionalState(EAvatarEmotionalState::Angry, Arousal);
    }
    else if (Valence > 0.5f && Arousal < 0.5f)
    {
        SetEmotionalState(EAvatarEmotionalState::Happy, Valence);
    }
    else if (Valence < -0.5f && Arousal < 0.5f)
    {
        SetEmotionalState(EAvatarEmotionalState::Sad, FMath::Abs(Valence));
    }
}

FAvatarEmotionalState UAvatar3DComponentEnhanced::GetCurrentEmotionalState() const
{
    return EmotionalState;
}

void UAvatar3DComponentEnhanced::ApplyEmotionalBlush(float Intensity)
{
    for (UMaterialInstanceDynamic* Mat : DynamicMaterials)
    {
        if (Mat)
        {
            Mat->SetScalarParameterValue(TEXT("BlushIntensity"), Intensity);
        }
    }
}

// ===== Personality Traits =====

void UAvatar3DComponentEnhanced::EnablePersonalityTrait(EAvatarPersonalityTrait Trait, float Intensity)
{
    PersonalityTraits.Add(Trait, FMath::Clamp(Intensity, 0.0f, 1.0f));
    
    UE_LOG(LogTemp, Log, TEXT("Personality trait %d set to intensity %.2f"), 
           (int32)Trait, Intensity);
}

void UAvatar3DComponentEnhanced::SetHyperChaoticBehavior(float ChaosFactor_)
{
    ChaosFactor = FMath::Clamp(ChaosFactor_, 0.0f, 1.0f);
    EnablePersonalityTrait(EAvatarPersonalityTrait::HyperChaotic, ChaosFactor);
    
    UE_LOG(LogTemp, Log, TEXT("Hyper-chaotic behavior set to: %.2f"), ChaosFactor);
}

void UAvatar3DComponentEnhanced::TriggerChaoticMicroExpression()
{
    // Generate random micro-expression
    float RandomIntensity = FMath::FRandRange(0.1f, 0.3f);
    int32 RandomEmotion = FMath::RandRange(0, 9);
    
    EAvatarEmotionalState TempEmotion = static_cast<EAvatarEmotionalState>(RandomEmotion);
    TransitionToEmotion(TempEmotion, 0.1f);
    
    UE_LOG(LogTemp, Log, TEXT("Chaotic micro-expression triggered"));
}

void UAvatar3DComponentEnhanced::SetConfidenceLevel(float Confidence)
{
    EnablePersonalityTrait(EAvatarPersonalityTrait::Confident, Confidence);
    
    // Confidence affects posture and gaze
    EmotionalState.Dominance = Confidence;
}

void UAvatar3DComponentEnhanced::SetFlirtinessLevel(float Flirtiness)
{
    EnablePersonalityTrait(EAvatarPersonalityTrait::Seductive, Flirtiness);
}

// ===== Animation Control =====

void UAvatar3DComponentEnhanced::PlayGesture(const FString& GestureName, float BlendInTime)
{
    // In production, this would play an animation montage
    UE_LOG(LogTemp, Log, TEXT("Playing gesture: %s with blend time %.2f"), 
           *GestureName, BlendInTime);
}

void UAvatar3DComponentEnhanced::PlayEmote(const FString& EmoteName)
{
    // In production, this would trigger an emote animation
    UE_LOG(LogTemp, Log, TEXT("Playing emote: %s"), *EmoteName);
}

void UAvatar3DComponentEnhanced::SetIdleAnimation(const FString& IdleAnimName)
{
    // In production, this would change the idle animation in the AnimBP
    UE_LOG(LogTemp, Log, TEXT("Idle animation set to: %s"), *IdleAnimName);
}

void UAvatar3DComponentEnhanced::SetWalkStyle(const FString& WalkStyleName)
{
    // In production, this would change locomotion animations
    UE_LOG(LogTemp, Log, TEXT("Walk style set to: %s"), *WalkStyleName);
}

// ===== Gaze and Attention =====

void UAvatar3DComponentEnhanced::SetGazeTarget(AActor* Target)
{
    CurrentGazeTarget = Target;
    UE_LOG(LogTemp, Log, TEXT("Gaze target set to: %s"), 
           Target ? *Target->GetName() : TEXT("None"));
}

void UAvatar3DComponentEnhanced::SetGazeDirection(const FVector& Direction)
{
    // In production, this would set eye bone rotations
    UE_LOG(LogTemp, Log, TEXT("Gaze direction set to: %s"), *Direction.ToString());
}

void UAvatar3DComponentEnhanced::EnableEyeTracking(bool bEnable)
{
    // In production, this would enable/disable eye tracking system
    UE_LOG(LogTemp, Log, TEXT("Eye tracking %s"), bEnable ? TEXT("enabled") : TEXT("disabled"));
}

void UAvatar3DComponentEnhanced::SetHeadTrackingTarget(AActor* Target, float TrackingSpeed)
{
    CurrentHeadTrackTarget = Target;
    UE_LOG(LogTemp, Log, TEXT("Head tracking target set to: %s with speed %.2f"), 
           Target ? *Target->GetName() : TEXT("None"), TrackingSpeed);
}

// ===== Deep Tree Echo Integration =====

void UAvatar3DComponentEnhanced::ApplyEchoResonanceEffect(float Resonance)
{
    EchoResonanceIntensity = FMath::Clamp(Resonance, 0.0f, 1.0f);
    
    // Update aura color based on resonance
    AuraColor = FLinearColor::LerpUsingHSV(
        FLinearColor(0.3f, 0.6f, 0.9f), // Cyan
        FLinearColor(0.8f, 0.3f, 0.9f), // Purple
        Resonance
    );
    
    UE_LOG(LogTemp, Log, TEXT("Echo resonance set to: %.2f"), Resonance);
}

void UAvatar3DComponentEnhanced::ApplyCognitiveLoadVisualization(float CognitiveLoad)
{
    float ClampedLoad = FMath::Clamp(CognitiveLoad, 0.0f, 1.0f);
    
    // High cognitive load triggers glitch effects
    if (ClampedLoad > 0.7f)
    {
        TriggerGlitchEffect((ClampedLoad - 0.7f) / 0.3f);
    }
    
    // Update aura intensity
    AuraIntensity = 0.3f + (ClampedLoad * 0.7f);
    
    UE_LOG(LogTemp, Log, TEXT("Cognitive load visualization: %.2f"), CognitiveLoad);
}

void UAvatar3DComponentEnhanced::TriggerGlitchEffect(float Severity)
{
    GlitchIntensity = FMath::Clamp(Severity, 0.0f, 1.0f);
    
    // Glitch effect fades over time
    // In production, this would trigger post-process effects
    
    UE_LOG(LogTemp, Log, TEXT("Glitch effect triggered with severity: %.2f"), Severity);
}

void UAvatar3DComponentEnhanced::SetEmotionalAuraColor(const FLinearColor& Color, float Intensity)
{
    AuraColor = Color;
    AuraIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
}

void UAvatar3DComponentEnhanced::VisualizeMemoryConstellation(const TArray<FVector>& MemoryNodes)
{
    // In production, this would spawn particle effects at memory node positions
    UE_LOG(LogTemp, Log, TEXT("Visualizing memory constellation with %d nodes"), 
           MemoryNodes.Num());
}

// ===== Material and Visual Effects =====

void UAvatar3DComponentEnhanced::SetSkinShaderParameters(float Smoothness, float Subsurface, 
                                                          float Translucency)
{
    for (UMaterialInstanceDynamic* Mat : DynamicMaterials)
    {
        if (Mat)
        {
            Mat->SetScalarParameterValue(TEXT("SkinSmoothness"), Smoothness);
            Mat->SetScalarParameterValue(TEXT("SubsurfaceScattering"), Subsurface);
            Mat->SetScalarParameterValue(TEXT("Translucency"), Translucency);
        }
    }
}

void UAvatar3DComponentEnhanced::ApplyHairShimmer(float Intensity)
{
    for (UMaterialInstanceDynamic* Mat : DynamicMaterials)
    {
        if (Mat)
        {
            Mat->SetScalarParameterValue(TEXT("HairShimmer"), Intensity);
        }
    }
}

void UAvatar3DComponentEnhanced::SetEyeSparkle(float Intensity)
{
    for (UMaterialInstanceDynamic* Mat : DynamicMaterials)
    {
        if (Mat)
        {
            Mat->SetScalarParameterValue(TEXT("EyeSparkle"), Intensity);
        }
    }
}

void UAvatar3DComponentEnhanced::ApplyGlowEffect(const FLinearColor& GlowColor, float Intensity)
{
    for (UMaterialInstanceDynamic* Mat : DynamicMaterials)
    {
        if (Mat)
        {
            Mat->SetVectorParameterValue(TEXT("GlowColor"), GlowColor);
            Mat->SetScalarParameterValue(TEXT("GlowIntensity"), Intensity);
        }
    }
}

// ===== Lip Sync =====

void UAvatar3DComponentEnhanced::StartLipSync(const TArray<float>& AudioData)
{
    // In production, this would drive mouth blend shapes based on audio
    UE_LOG(LogTemp, Log, TEXT("Lip sync started with %d audio samples"), AudioData.Num());
}

void UAvatar3DComponentEnhanced::StopLipSync()
{
    // In production, this would reset mouth to neutral
    UE_LOG(LogTemp, Log, TEXT("Lip sync stopped"));
}

void UAvatar3DComponentEnhanced::SetLipSyncEmotionalModulation(float EmotionalIntensity)
{
    // In production, this would modulate lip sync based on emotion
    UE_LOG(LogTemp, Log, TEXT("Lip sync emotional modulation: %.2f"), EmotionalIntensity);
}

// ===== Internal Methods =====

void UAvatar3DComponentEnhanced::UpdateEmotionalTransition(float DeltaTime)
{
    if (EmotionTransitionProgress < 1.0f)
    {
        EmotionTransitionProgress += DeltaTime / EmotionTransitionTime;
        EmotionTransitionProgress = FMath::Clamp(EmotionTransitionProgress, 0.0f, 1.0f);
        
        // Blend between current and target emotion
        CurrentEmotionBlendWeight = EmotionTransitionProgress;
        
        if (EmotionTransitionProgress >= 1.0f)
        {
            // Transition complete
            EmotionalState.CurrentEmotion = TargetEmotion;
            ApplyEmotionToFacialAnimation();
        }
    }
}

void UAvatar3DComponentEnhanced::UpdateChaoticBehavior(float DeltaTime)
{
    if (ChaosFactor > 0.1f)
    {
        ChaosTimer += DeltaTime;
        
        if (ChaosTimer >= NextChaoticEventTime)
        {
            TriggerRandomChaoticEvent();
            ChaosTimer = 0.0f;
            NextChaoticEventTime = FMath::FRandRange(2.0f / ChaosFactor, 5.0f / ChaosFactor);
        }
    }
}

void UAvatar3DComponentEnhanced::UpdateGazeTracking(float DeltaTime)
{
    if (CurrentGazeTarget)
    {
        FVector GazeDirection = CalculateGazeDirection();
        SetGazeDirection(GazeDirection);
    }
}

void UAvatar3DComponentEnhanced::UpdateHeadTracking(float DeltaTime)
{
    if (CurrentHeadTrackTarget)
    {
        FRotator HeadRotation = CalculateHeadRotation();
        // In production, apply to head bone
    }
}

void UAvatar3DComponentEnhanced::UpdateVisualEffects(float DeltaTime)
{
    // Update pulsing effects
    float Time = GetWorld()->GetTimeSeconds();
    float PulseValue = 0.5f + 0.5f * FMath::Sin(Time * 2.0f * EchoResonanceIntensity);
    
    // Update aura
    SetEmotionalAuraColor(AuraColor, AuraIntensity * PulseValue);
    
    // Fade glitch effect
    if (GlitchIntensity > 0.0f)
    {
        GlitchIntensity = FMath::Max(0.0f, GlitchIntensity - DeltaTime * 0.5f);
    }
    
    // Update material parameters
    UpdateMaterialParameters();
}

void UAvatar3DComponentEnhanced::UpdateMaterialParameters()
{
    for (UMaterialInstanceDynamic* Mat : DynamicMaterials)
    {
        if (Mat)
        {
            // Update all dynamic parameters
            Mat->SetVectorParameterValue(TEXT("SkinTone"), AppearanceSettings.SkinTone);
            Mat->SetVectorParameterValue(TEXT("HairColor"), AppearanceSettings.HairColor);
            Mat->SetVectorParameterValue(TEXT("EyeColor"), AppearanceSettings.EyeColor);
            Mat->SetVectorParameterValue(TEXT("AuraColor"), AuraColor);
            Mat->SetScalarParameterValue(TEXT("AuraIntensity"), AuraIntensity);
            Mat->SetScalarParameterValue(TEXT("GlitchIntensity"), GlitchIntensity);
        }
    }
}

void UAvatar3DComponentEnhanced::InitializeDynamicMaterials()
{
    DynamicMaterials.Empty();
    
    // Get all materials from the skeletal mesh
    TArray<UMaterialInterface*> Materials = GetMaterials();
    
    for (int32 i = 0; i < Materials.Num(); i++)
    {
        if (Materials[i])
        {
            UMaterialInstanceDynamic* DynMat = CreateDynamicMaterialInstance(i, Materials[i]);
            if (DynMat)
            {
                DynamicMaterials.Add(DynMat);
            }
        }
    }
    
    UE_LOG(LogTemp, Log, TEXT("Initialized %d dynamic materials"), DynamicMaterials.Num());
}

void UAvatar3DComponentEnhanced::ApplyEmotionToFacialAnimation()
{
    // Map high-level EAvatarEmotionalState to EExpressionState for the expression system.
    // Selects the representative expression for each emotion category.
    EExpressionState TargetExpression = EExpressionState::Neutral;
    float TransitionTime = 0.5f;

    switch (EmotionalState.CurrentEmotion)
    {
        case EAvatarEmotionalState::Happy:
            TargetExpression = (EmotionalState.EmotionIntensity > 0.7f)
                ? EExpressionState::BroadSmile : EExpressionState::GentleSmile;
            break;
        case EAvatarEmotionalState::Excited:
            TargetExpression = EExpressionState::Laughing;
            break;
        case EAvatarEmotionalState::Surprised:
            TargetExpression = EExpressionState::Awe;
            break;
        case EAvatarEmotionalState::Thoughtful:
            TargetExpression = EExpressionState::Contemplative;
            break;
        case EAvatarEmotionalState::Flirty:
            TargetExpression = EExpressionState::PlayfulGrin;
            break;
        case EAvatarEmotionalState::Mysterious:
            TargetExpression = EExpressionState::CuriousGaze;
            break;
        case EAvatarEmotionalState::Chaotic:
            TargetExpression = EExpressionState::FearStartle;
            TransitionTime = 0.1f;
            break;
        // Negative emotions (FACS-Complete v4)
        case EAvatarEmotionalState::Sad:
            TargetExpression = (EmotionalState.EmotionIntensity > 0.6f)
                ? EExpressionState::SadGrief : EExpressionState::SadMelancholy;
            break;
        case EAvatarEmotionalState::Grieving:
            TargetExpression = EExpressionState::SadCrying;
            break;
        case EAvatarEmotionalState::Angry:
            TargetExpression = (EmotionalState.EmotionIntensity > 0.7f)
                ? EExpressionState::AngerRage : EExpressionState::AngerIrritation;
            break;
        case EAvatarEmotionalState::Afraid:
            TargetExpression = (EmotionalState.EmotionIntensity > 0.7f)
                ? EExpressionState::FearTerror : EExpressionState::FearWorried;
            break;
        case EAvatarEmotionalState::Disgusted:
            TargetExpression = EExpressionState::DisgustRevulsion;
            break;
        case EAvatarEmotionalState::Contemptuous:
            TargetExpression = EExpressionState::DisgustContempt;
            break;
        default:
            TargetExpression = EExpressionState::Neutral;
            break;
    }

    // Delegate to the expression system component if present on the owner
    if (AActor* Owner = GetOwner())
    {
        if (UDeepTreeEchoExpressionSystem* ExprSys = Owner->FindComponentByClass<UDeepTreeEchoExpressionSystem>())
        {
            ExprSys->SetExpressionState(TargetExpression, TransitionTime);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("ApplyEmotionToFacialAnimation: emotion=%d -> expression=%d"),
        (int32)EmotionalState.CurrentEmotion, (int32)TargetExpression);
}

void UAvatar3DComponentEnhanced::TriggerRandomChaoticEvent()
{
    int32 EventType = FMath::RandRange(0, 2);
    
    switch (EventType)
    {
        case 0:
            // Micro-expression
            TriggerChaoticMicroExpression();
            break;
        case 1:
            // Glitch effect
            TriggerGlitchEffect(FMath::FRandRange(0.2f, 0.5f) * ChaosFactor);
            break;
        case 2:
            // Eye sparkle burst
            SetEyeSparkle(1.0f);
            break;
    }
}

FVector UAvatar3DComponentEnhanced::CalculateGazeDirection() const
{
    if (CurrentGazeTarget)
    {
        FVector TargetLocation = CurrentGazeTarget->GetActorLocation();
        FVector EyeLocation = GetComponentLocation(); // Approximate
        return (TargetLocation - EyeLocation).GetSafeNormal();
    }
    return FVector::ForwardVector;
}

FRotator UAvatar3DComponentEnhanced::CalculateHeadRotation() const
{
    if (CurrentHeadTrackTarget)
    {
        FVector TargetLocation = CurrentHeadTrackTarget->GetActorLocation();
        FVector HeadLocation = GetComponentLocation();
        return UKismetMathLibrary::FindLookAtRotation(HeadLocation, TargetLocation);
    }
    return FRotator::ZeroRotator;
}

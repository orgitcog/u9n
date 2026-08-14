// Copyright Epic Games, Inc. All Rights Reserved.
// Deep Tree Echo Avatar - Agent Avatar Actor Implementation

#include "AgentAvatarActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/SpotLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimBlueprint.h"

AAgentAvatarActor::AAgentAvatarActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.0f; // Every frame for smooth visuals

    // Create root component
    USceneComponent* RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
    RootComponent = RootScene;

    // Create avatar mesh
    AvatarMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AvatarMesh"));
    AvatarMesh->SetupAttachment(RootComponent);
    AvatarMesh->SetRelativeLocation(FVector::ZeroVector);

    // Create aura mesh (sphere)
    AuraMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AuraMesh"));
    AuraMesh->SetupAttachment(RootComponent);
    AuraMesh->SetRelativeScale3D(FVector(1.0f));
    AuraMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // Create aura light
    AuraLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("AuraLight"));
    AuraLight->SetupAttachment(RootComponent);
    AuraLight->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
    AuraLight->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
    AuraLight->Intensity = 5000.0f;
    AuraLight->OuterConeAngle = 45.0f;
    AuraLight->InnerConeAngle = 30.0f;

    // Create label widget
    LabelWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("LabelWidget"));
    LabelWidget->SetupAttachment(RootComponent);
    LabelWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 200.0f));
    LabelWidget->SetWidgetSpace(EWidgetSpace::Screen);
    LabelWidget->SetDrawSize(FVector2D(200.0f, 50.0f));

    // Initialize state
    AttentionLevel = 0.5f;
    ActivityLevel = 0.5f;
    bIsOnline = true;
    CurrentLODLevel = 0;
    bIsThinking = false;
    bIsSpeaking = false;

    AuraPulseTimer = 0.0f;
    AuraPulseTarget = 0.0f;
    AuraPulseDuration = 0.0f;

    ExpressionBlendAlpha = 1.0f;
    ExpressionBlendTime = 0.0f;
    bIsBlendingExpression = false;

    MicroExpressionTimer = 0.0f;
    HighlightTimer = 0.0f;
}

void AAgentAvatarActor::BeginPlay()
{
    Super::BeginPlay();

    // Create dynamic materials
    if (AuraMesh && AuraMesh->GetMaterial(0))
    {
        AuraMaterial = UMaterialInstanceDynamic::Create(AuraMesh->GetMaterial(0), this);
        if (AuraMaterial)
        {
            AuraMesh->SetMaterial(0, AuraMaterial);
        }
    }

    if (AvatarMesh && AvatarMesh->GetMaterial(0))
    {
        AvatarMaterial = UMaterialInstanceDynamic::Create(AvatarMesh->GetMaterial(0), this);
        if (AvatarMaterial)
        {
            AvatarMesh->SetMaterial(0, AvatarMaterial);
        }
    }

    // Apply default aura settings
    SetAuraSettings(AuraSettings);
}

void AAgentAvatarActor::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Update aura visuals
    UpdateAuraVisuals(DeltaTime);

    // Update expression blend
    if (bIsBlendingExpression)
    {
        UpdateExpressionBlend(DeltaTime);
    }

    // Update look-at
    if (LookAtTarget)
    {
        FVector TargetLocation = LookAtTarget->GetActorLocation();
        FVector Direction = TargetLocation - GetActorLocation();
        Direction.Normalize();

        // Smoothly rotate towards target
        FRotator TargetRotation = Direction.Rotation();
        FRotator CurrentRotation = GetActorRotation();
        FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaTime, 5.0f);
        SetActorRotation(FRotator(0.0f, NewRotation.Yaw, 0.0f));
    }

    // Update timers
    if (MicroExpressionTimer > 0.0f)
    {
        MicroExpressionTimer -= DeltaTime;
        if (MicroExpressionTimer <= 0.0f)
        {
            // Reset to base expression
            BlendToExpression(FExpressionMorphState(), 0.3f);
        }
    }

    if (HighlightTimer > 0.0f)
    {
        HighlightTimer -= DeltaTime;
        if (HighlightTimer <= 0.0f)
        {
            // Reset highlight
            if (AuraMaterial)
            {
                AuraMaterial->SetScalarParameterValue(TEXT("HighlightIntensity"), 0.0f);
            }
        }
    }
}

// ===== Agent Identity =====

void AAgentAvatarActor::SetAgentPath(const FString& Path)
{
    AgentPath = Path;
    AgentName = FPaths::GetBaseFilename(Path);
    LabelSettings.DisplayName = AgentName;
    UpdateLabelWidget();
}

void AAgentAvatarActor::SetAgentName(const FString& Name)
{
    AgentName = Name;
    LabelSettings.DisplayName = Name;
    UpdateLabelWidget();
}

// ===== Visual Style =====

void AAgentAvatarActor::SetVisualStyle(const FAgentVisualStyle& Style)
{
    CurrentStyle = Style;

    // Apply style properties
    if (Style.AvatarMesh.IsValid())
    {
        SetAvatarMesh(Style.AvatarMesh.LoadSynchronous());
    }

    if (Style.AnimationBlueprint.IsValid())
    {
        SetAnimationBlueprint(Style.AnimationBlueprint.LoadSynchronous());
    }

    SetActorScale3D(FVector(Style.DefaultScale));
    SetAuraColor(Style.DefaultAuraColor);

    // Update label
    LabelSettings.AgentType = Style.StyleName;
    UpdateLabelWidget();
}

void AAgentAvatarActor::SetAvatarMesh(USkeletalMesh* Mesh)
{
    if (AvatarMesh && Mesh)
    {
        AvatarMesh->SetSkeletalMesh(Mesh);
    }
}

void AAgentAvatarActor::SetAnimationBlueprint(UAnimBlueprint* AnimBP)
{
    if (AvatarMesh && AnimBP)
    {
        AvatarMesh->SetAnimInstanceClass(AnimBP->GeneratedClass);
    }
}

// ===== Aura Effects =====

void AAgentAvatarActor::SetAuraColor(FLinearColor Color)
{
    AuraSettings.BaseColor = Color;

    if (AuraMaterial)
    {
        AuraMaterial->SetVectorParameterValue(TEXT("AuraColor"), Color);
    }

    if (AuraLight)
    {
        AuraLight->SetLightColor(Color);
    }
}

void AAgentAvatarActor::SetAuraIntensity(float Intensity)
{
    AuraSettings.Intensity = FMath::Clamp(Intensity, 0.0f, 3.0f);

    if (AuraMaterial)
    {
        AuraMaterial->SetScalarParameterValue(TEXT("AuraIntensity"), AuraSettings.Intensity);
    }

    if (AuraLight)
    {
        AuraLight->SetIntensity(5000.0f * AuraSettings.Intensity);
    }
}

void AAgentAvatarActor::SetAuraSettings(const FAuraSettings& Settings)
{
    AuraSettings = Settings;

    SetAuraColor(Settings.BaseColor);
    SetAuraIntensity(Settings.Intensity);

    if (AuraMesh)
    {
        AuraMesh->SetRelativeScale3D(FVector(Settings.Radius / 100.0f));
    }
}

void AAgentAvatarActor::PulseAura(float Intensity, float Duration)
{
    AuraPulseTarget = Intensity;
    AuraPulseDuration = Duration;
    AuraPulseTimer = Duration;
}

void AAgentAvatarActor::SetAuraVisible(bool bVisible)
{
    if (AuraMesh)
    {
        AuraMesh->SetVisibility(bVisible);
    }

    if (AuraParticles)
    {
        AuraParticles->SetVisibility(bVisible);
    }

    if (AuraLight)
    {
        AuraLight->SetVisibility(bVisible);
    }
}

void AAgentAvatarActor::UpdateAuraVisuals(float DeltaTime)
{
    if (!AuraSettings.bEnablePulse && AuraPulseTimer <= 0.0f)
    {
        return;
    }

    float PulseValue = 0.0f;

    // Handle manual pulse
    if (AuraPulseTimer > 0.0f)
    {
        AuraPulseTimer -= DeltaTime;
        float Alpha = AuraPulseTimer / AuraPulseDuration;
        PulseValue = AuraPulseTarget * Alpha;
    }
    // Handle automatic pulse
    else if (AuraSettings.bEnablePulse)
    {
        float Time = GetWorld()->GetTimeSeconds();
        PulseValue = FMath::Sin(Time * AuraSettings.PulseFrequency * 2.0f * PI) * AuraSettings.PulseAmplitude;
    }

    // Apply pulse to material
    if (AuraMaterial)
    {
        float CurrentIntensity = AuraSettings.Intensity + PulseValue;
        AuraMaterial->SetScalarParameterValue(TEXT("AuraIntensity"), CurrentIntensity);
    }

    // Apply pulse to light
    if (AuraLight)
    {
        AuraLight->SetIntensity(5000.0f * (AuraSettings.Intensity + PulseValue));
    }
}

// ===== Emotional Expression =====

void AAgentAvatarActor::UpdateEmotionalExpression(const TMap<FString, float>& EmotionalState)
{
    FExpressionMorphState NewExpression;

    NewExpression.Happiness = EmotionalState.FindRef(TEXT("happiness"));
    NewExpression.Sadness = EmotionalState.FindRef(TEXT("sadness"));
    NewExpression.Anger = EmotionalState.FindRef(TEXT("anger"));
    NewExpression.Fear = EmotionalState.FindRef(TEXT("fear"));
    NewExpression.Surprise = EmotionalState.FindRef(TEXT("surprise"));
    NewExpression.Disgust = EmotionalState.FindRef(TEXT("disgust"));
    NewExpression.Contempt = EmotionalState.FindRef(TEXT("contempt"));
    NewExpression.Curiosity = EmotionalState.FindRef(TEXT("curiosity"));
    NewExpression.Excitement = EmotionalState.FindRef(TEXT("excitement"));

    BlendToExpression(NewExpression, 0.5f);

    // Update aura color based on emotion
    if (AuraSettings.bReactToEmotion)
    {
        FLinearColor EmotionColor = CalculateEmotionColor(EmotionalState);
        FLinearColor BlendedColor = FLinearColor::LerpUsingHSV(AuraSettings.BaseColor, EmotionColor, 0.5f);
        SetAuraColor(BlendedColor);
    }
}

void AAgentAvatarActor::SetExpression(const FExpressionMorphState& Expression)
{
    CurrentExpression = Expression;
    TargetExpression = Expression;
    bIsBlendingExpression = false;
    ApplyExpressionToMesh();
}

void AAgentAvatarActor::BlendToExpression(const FExpressionMorphState& NewTargetExpression, float BlendTime)
{
    TargetExpression = NewTargetExpression;
    ExpressionBlendTime = BlendTime;
    ExpressionBlendAlpha = 0.0f;
    bIsBlendingExpression = true;
}

void AAgentAvatarActor::TriggerMicroExpression(const FString& ExpressionType, float Intensity, float Duration)
{
    FExpressionMorphState MicroExpression = CurrentExpression;

    if (ExpressionType == TEXT("surprise"))
    {
        MicroExpression.Surprise = FMath::Clamp(Intensity, 0.0f, 1.0f);
    }
    else if (ExpressionType == TEXT("interest"))
    {
        MicroExpression.Curiosity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    }
    else if (ExpressionType == TEXT("recognition"))
    {
        MicroExpression.Happiness = FMath::Clamp(Intensity * 0.5f, 0.0f, 1.0f);
        MicroExpression.Surprise = FMath::Clamp(Intensity * 0.3f, 0.0f, 1.0f);
    }

    SetExpression(MicroExpression);
    MicroExpressionTimer = Duration;
}

void AAgentAvatarActor::UpdateExpressionBlend(float DeltaTime)
{
    if (!bIsBlendingExpression)
    {
        return;
    }

    ExpressionBlendAlpha += DeltaTime / ExpressionBlendTime;

    if (ExpressionBlendAlpha >= 1.0f)
    {
        ExpressionBlendAlpha = 1.0f;
        CurrentExpression = TargetExpression;
        bIsBlendingExpression = false;
    }
    else
    {
        // Lerp all expression values
        CurrentExpression.Happiness = FMath::Lerp(CurrentExpression.Happiness, TargetExpression.Happiness, ExpressionBlendAlpha);
        CurrentExpression.Sadness = FMath::Lerp(CurrentExpression.Sadness, TargetExpression.Sadness, ExpressionBlendAlpha);
        CurrentExpression.Anger = FMath::Lerp(CurrentExpression.Anger, TargetExpression.Anger, ExpressionBlendAlpha);
        CurrentExpression.Fear = FMath::Lerp(CurrentExpression.Fear, TargetExpression.Fear, ExpressionBlendAlpha);
        CurrentExpression.Surprise = FMath::Lerp(CurrentExpression.Surprise, TargetExpression.Surprise, ExpressionBlendAlpha);
        CurrentExpression.Disgust = FMath::Lerp(CurrentExpression.Disgust, TargetExpression.Disgust, ExpressionBlendAlpha);
        CurrentExpression.Contempt = FMath::Lerp(CurrentExpression.Contempt, TargetExpression.Contempt, ExpressionBlendAlpha);
        CurrentExpression.Curiosity = FMath::Lerp(CurrentExpression.Curiosity, TargetExpression.Curiosity, ExpressionBlendAlpha);
        CurrentExpression.Excitement = FMath::Lerp(CurrentExpression.Excitement, TargetExpression.Excitement, ExpressionBlendAlpha);
    }

    ApplyExpressionToMesh();
}

void AAgentAvatarActor::ApplyExpressionToMesh()
{
    if (!AvatarMesh)
    {
        return;
    }

    // Apply morph targets (requires skeletal mesh with morph targets)
    AvatarMesh->SetMorphTarget(TEXT("Happiness"), CurrentExpression.Happiness);
    AvatarMesh->SetMorphTarget(TEXT("Sadness"), CurrentExpression.Sadness);
    AvatarMesh->SetMorphTarget(TEXT("Anger"), CurrentExpression.Anger);
    AvatarMesh->SetMorphTarget(TEXT("Fear"), CurrentExpression.Fear);
    AvatarMesh->SetMorphTarget(TEXT("Surprise"), CurrentExpression.Surprise);
    AvatarMesh->SetMorphTarget(TEXT("Disgust"), CurrentExpression.Disgust);
    AvatarMesh->SetMorphTarget(TEXT("Contempt"), CurrentExpression.Contempt);
    AvatarMesh->SetMorphTarget(TEXT("Curiosity"), CurrentExpression.Curiosity);
    AvatarMesh->SetMorphTarget(TEXT("Excitement"), CurrentExpression.Excitement);
}

FLinearColor AAgentAvatarActor::CalculateEmotionColor(const TMap<FString, float>& EmotionalState)
{
    FLinearColor ResultColor = FLinearColor::Black;
    float TotalWeight = 0.0f;

    // Color mapping for emotions
    TMap<FString, FLinearColor> EmotionColors;
    EmotionColors.Add(TEXT("happiness"), FLinearColor(1.0f, 0.9f, 0.3f, 1.0f));     // Yellow
    EmotionColors.Add(TEXT("sadness"), FLinearColor(0.2f, 0.3f, 0.8f, 1.0f));       // Blue
    EmotionColors.Add(TEXT("anger"), FLinearColor(0.9f, 0.2f, 0.1f, 1.0f));         // Red
    EmotionColors.Add(TEXT("fear"), FLinearColor(0.5f, 0.2f, 0.6f, 1.0f));          // Purple
    EmotionColors.Add(TEXT("surprise"), FLinearColor(1.0f, 0.6f, 0.2f, 1.0f));      // Orange
    EmotionColors.Add(TEXT("disgust"), FLinearColor(0.4f, 0.6f, 0.2f, 1.0f));       // Olive
    EmotionColors.Add(TEXT("curiosity"), FLinearColor(0.3f, 0.8f, 0.9f, 1.0f));     // Cyan
    EmotionColors.Add(TEXT("excitement"), FLinearColor(1.0f, 0.4f, 0.6f, 1.0f));    // Pink

    for (const auto& Pair : EmotionalState)
    {
        if (FLinearColor* Color = EmotionColors.Find(Pair.Key))
        {
            ResultColor += (*Color) * Pair.Value;
            TotalWeight += Pair.Value;
        }
    }

    if (TotalWeight > 0.0f)
    {
        ResultColor /= TotalWeight;
    }
    else
    {
        ResultColor = FLinearColor(0.5f, 0.5f, 0.8f, 1.0f); // Default neutral color
    }

    ResultColor.A = 1.0f;
    return ResultColor;
}

// ===== Activity State =====

void AAgentAvatarActor::SetAttentionLevel(float Level)
{
    AttentionLevel = FMath::Clamp(Level, 0.0f, 1.0f);

    // Visual feedback - more attentive = brighter eyes, more focused gaze
    if (AvatarMaterial)
    {
        AvatarMaterial->SetScalarParameterValue(TEXT("EyeBrightness"), 0.5f + AttentionLevel * 0.5f);
    }
}

void AAgentAvatarActor::SetActivityLevel(float Level)
{
    ActivityLevel = FMath::Clamp(Level, 0.0f, 1.0f);

    // Visual feedback - higher activity = faster aura pulse, more particle emission
    AuraSettings.PulseFrequency = 0.5f + ActivityLevel * 1.5f;

    if (AuraParticles)
    {
        // Would adjust spawn rate based on activity
    }
}

void AAgentAvatarActor::SetOnlineState(bool bOnline)
{
    bool bWasOnline = bIsOnline;
    bIsOnline = bOnline;

    if (bWasOnline != bOnline)
    {
        // Visual feedback for online/offline transition
        if (bOnline)
        {
            SetAuraIntensity(AuraSettings.Intensity);
            PulseAura(1.0f, 0.5f); // Pulse when coming online
        }
        else
        {
            SetAuraIntensity(AuraSettings.Intensity * 0.3f); // Dim when offline
        }

        OnAvatarStateChanged.Broadcast(bOnline ? TEXT("online") : TEXT("offline"));
        UpdateLabelWidget();
    }
}

// ===== Label/UI =====

void AAgentAvatarActor::SetLabelSettings(const FAgentLabelSettings& Settings)
{
    LabelSettings = Settings;

    if (LabelWidget)
    {
        LabelWidget->SetRelativeLocation(FVector(0.0f, 0.0f, Settings.LabelHeight));
        LabelWidget->SetVisibility(Settings.bShowLabel);
    }

    UpdateLabelWidget();
}

void AAgentAvatarActor::SetLabelVisible(bool bVisible)
{
    LabelSettings.bShowLabel = bVisible;

    if (LabelWidget)
    {
        LabelWidget->SetVisibility(bVisible);
    }
}

void AAgentAvatarActor::SetStatusText(const FString& Status)
{
    CurrentStatusText = Status;
    UpdateLabelWidget();
}

void AAgentAvatarActor::UpdateLabelWidget()
{
    // Would update the widget's text here
    // This requires a custom widget blueprint
    UE_LOG(LogTemp, Verbose, TEXT("Label updated: %s - %s"),
           *LabelSettings.DisplayName, *CurrentStatusText);
}

// ===== LOD =====

void AAgentAvatarActor::SetLODLevel(int32 Level)
{
    CurrentLODLevel = FMath::Clamp(Level, 0, 3);
    ApplyLODSettings();
}

void AAgentAvatarActor::ApplyLODSettings()
{
    if (!AvatarMesh)
    {
        return;
    }

    switch (CurrentLODLevel)
    {
    case 0: // Full detail
        AvatarMesh->SetForcedLOD(0);
        if (AuraParticles) AuraParticles->SetVisibility(true);
        if (ThinkingParticles) ThinkingParticles->SetVisibility(true);
        break;

    case 1: // Reduced detail
        AvatarMesh->SetForcedLOD(1);
        if (AuraParticles) AuraParticles->SetVisibility(true);
        if (ThinkingParticles) ThinkingParticles->SetVisibility(false);
        break;

    case 2: // Minimal detail
        AvatarMesh->SetForcedLOD(2);
        if (AuraParticles) AuraParticles->SetVisibility(false);
        if (ThinkingParticles) ThinkingParticles->SetVisibility(false);
        break;

    case 3: // Lowest detail
        AvatarMesh->SetForcedLOD(3);
        if (AuraParticles) AuraParticles->SetVisibility(false);
        if (ThinkingParticles) ThinkingParticles->SetVisibility(false);
        if (AuraLight) AuraLight->SetVisibility(false);
        break;
    }
}

// ===== Animation =====

void AAgentAvatarActor::PlayAnimation(UAnimSequence* Animation, bool bLoop)
{
    if (AvatarMesh && Animation)
    {
        AvatarMesh->PlayAnimation(Animation, bLoop);
    }
}

void AAgentAvatarActor::SetIdleVariation(int32 Variation)
{
    // Would set animation blueprint variable for idle variation
}

void AAgentAvatarActor::TriggerGesture(const FString& GestureType)
{
    // Would trigger gesture montage
    UE_LOG(LogTemp, Log, TEXT("Gesture triggered: %s"), *GestureType);
}

// ===== Interaction =====

void AAgentAvatarActor::LookAt(FVector Target)
{
    LookAtTarget = nullptr;

    // Immediate look at
    FVector Direction = Target - GetActorLocation();
    Direction.Normalize();
    FRotator TargetRotation = Direction.Rotation();
    SetActorRotation(FRotator(0.0f, TargetRotation.Yaw, 0.0f));
}

void AAgentAvatarActor::LookAtActor(AActor* Target)
{
    LookAtTarget = Target;
}

void AAgentAvatarActor::StartConversation(AActor* OtherActor)
{
    ConversationPartner = OtherActor;
    LookAtActor(OtherActor);

    OnAvatarInteraction.Broadcast(TEXT("conversation_start"), OtherActor);

    // Face the conversation partner
    if (OtherActor)
    {
        LookAtActor(OtherActor);
    }
}

void AAgentAvatarActor::EndConversation()
{
    AActor* PreviousPartner = ConversationPartner;
    ConversationPartner = nullptr;
    LookAtTarget = nullptr;

    OnAvatarInteraction.Broadcast(TEXT("conversation_end"), PreviousPartner);
}

// ===== Visual Effects =====

void AAgentAvatarActor::TriggerThinkingEffect(bool bEnable)
{
    bIsThinking = bEnable;

    if (ThinkingParticles)
    {
        if (bEnable)
        {
            ThinkingParticles->Activate();
        }
        else
        {
            ThinkingParticles->Deactivate();
        }
    }

    // Subtle expression change
    if (bEnable)
    {
        FExpressionMorphState ThinkingExpression = CurrentExpression;
        ThinkingExpression.Curiosity = FMath::Max(ThinkingExpression.Curiosity, 0.3f);
        BlendToExpression(ThinkingExpression, 0.3f);
    }
}

void AAgentAvatarActor::TriggerSpeakingEffect(bool bEnable)
{
    bIsSpeaking = bEnable;

    // Would animate mouth/lips
    // This typically connects to lip sync system
}

void AAgentAvatarActor::TriggerHighlight(float Duration, FLinearColor Color)
{
    HighlightTimer = Duration;

    if (AuraMaterial)
    {
        AuraMaterial->SetVectorParameterValue(TEXT("HighlightColor"), Color);
        AuraMaterial->SetScalarParameterValue(TEXT("HighlightIntensity"), 1.0f);
    }

    PulseAura(1.5f, Duration * 0.5f);
}

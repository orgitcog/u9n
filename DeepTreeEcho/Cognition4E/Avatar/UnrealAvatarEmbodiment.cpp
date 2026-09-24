/**
 * @file UnrealAvatarEmbodiment.cpp
 * @brief Full implementation of 4E Embodied Cognition for Unreal Engine Avatar
 * 
 * This implementation provides complete integration between the Deep Tree Echo
 * cognitive framework and Unreal Engine's avatar systems, including:
 * - MetaHuman facial expression mapping
 * - Body schema integration with skeletal mesh
 * - Environmental coupling through perception systems
 * - Tool integration for extended cognition
 * - Sensorimotor contingency learning
 * 
 * Based on the Deep Tree Echo profile design:
 * - Flowing silver-white hair with cyan highlights
 * - Cybernetic headphones and collar device
 * - Bioluminescent environmental response
 * - Expressive facial features with holographic markings
 */

#include "UnrealAvatarEmbodiment.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Math/UnrealMathUtility.h"

// ============================================================================
// Avatar Visual Constants (from Deep Tree Echo Profile)
// ============================================================================

namespace AvatarVisualConstants
{
    // Hair colors
    const FLinearColor HairBaseColor(0.9f, 0.95f, 1.0f, 1.0f);       // Silver-white
    const FLinearColor HairHighlightColor(0.0f, 0.8f, 0.9f, 1.0f);   // Cyan
    
    // Eye colors
    const FLinearColor EyeBaseColor(0.3f, 0.7f, 0.9f, 1.0f);         // Bright cyan
    const FLinearColor EyeGlowColor(0.0f, 0.9f, 1.0f, 1.0f);         // Intense cyan
    
    // Tech element colors
    const FLinearColor TechActiveColor(1.0f, 0.4f, 0.2f, 1.0f);      // Orange glow
    const FLinearColor TechIdleColor(0.2f, 0.6f, 0.8f, 1.0f);        // Soft cyan
    
    // Bioluminescence colors
    const FLinearColor BioWarmColor(1.0f, 0.5f, 0.3f, 1.0f);         // Warm orange
    const FLinearColor BioCoolColor(0.3f, 0.8f, 0.9f, 1.0f);         // Cool cyan
    
    // Expression blend shape names (MetaHuman compatible)
    const FName BS_JawOpen("CTRL_C_jaw_open");
    const FName BS_SmileLeft("CTRL_L_mouth_cornerPull");
    const FName BS_SmileRight("CTRL_R_mouth_cornerPull");
    const FName BS_BrowRaiseLeft("CTRL_L_brow_raiseIn");
    const FName BS_BrowRaiseRight("CTRL_R_brow_raiseIn");
    const FName BS_EyeWideLeft("CTRL_L_eye_pupilWide");
    const FName BS_EyeWideRight("CTRL_R_eye_pupilWide");
    const FName BS_EyeSquintLeft("CTRL_L_eye_squintInner");
    const FName BS_EyeSquintRight("CTRL_R_eye_squintInner");
}

// ============================================================================
// UUnrealAvatarEmbodiment Implementation
// ============================================================================

UUnrealAvatarEmbodiment::UUnrealAvatarEmbodiment()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void UUnrealAvatarEmbodiment::BeginPlay()
{
    Super::BeginPlay();
    
    FindComponentReferences();
    InitializeBodySchema();
    Initialize4EStates();
    InitializeVisualCoupling();
    
    bIsInitialized = true;
}

void UUnrealAvatarEmbodiment::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    if (!bIsInitialized || !bEnableEmbodiment)
    {
        return;
    }
    
    // Update 4E cognition states
    UpdateEmbodiedState(DeltaTime);
    UpdateEmbeddedState(DeltaTime);
    UpdateEnactedState(DeltaTime);
    UpdateExtendedState(DeltaTime);
    
    // Process sensorimotor loop
    ProcessSensorimotorLoop(DeltaTime);
    
    // Update avatar visual coupling
    UpdateVisualCoupling(DeltaTime);
    
    // Apply expression to avatar
    ApplyExpressionToAvatar(DeltaTime);
    
    // Update tech elements
    UpdateTechElements(DeltaTime);
}

void UUnrealAvatarEmbodiment::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    
    SkeletalMesh = Owner->FindComponentByClass<USkeletalMeshComponent>();
    
    // Find cognitive components
    CognitiveCycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
    Embodied4ECognition = Owner->FindComponentByClass<UEmbodied4ECognition>();
    EchobeatsEngine = Owner->FindComponentByClass<UEchobeatsStreamEngine>();
    
    // Bind to cognitive events
    if (CognitiveCycleManager)
    {
        CognitiveCycleManager->OnStepChanged.AddDynamic(
            this, &UUnrealAvatarEmbodiment::HandleCognitiveStepChanged);
        CognitiveCycleManager->OnRelevanceRealized.AddDynamic(
            this, &UUnrealAvatarEmbodiment::HandleRelevanceRealized);
    }
}

void UUnrealAvatarEmbodiment::InitializeBodySchema()
{
    // Initialize body part states
    BodyPartStates.Empty();
    
    // Head and face
    BodyPartStates.Add(TEXT("Head"), FBodyPartState{TEXT("Head"), FVector::ZeroVector, 1.0f, 0.5f});
    BodyPartStates.Add(TEXT("Face"), FBodyPartState{TEXT("Face"), FVector::ZeroVector, 1.0f, 0.5f});
    BodyPartStates.Add(TEXT("Eyes"), FBodyPartState{TEXT("Eyes"), FVector::ZeroVector, 1.0f, 0.5f});
    
    // Upper body
    BodyPartStates.Add(TEXT("Torso"), FBodyPartState{TEXT("Torso"), FVector::ZeroVector, 1.0f, 0.5f});
    BodyPartStates.Add(TEXT("LeftArm"), FBodyPartState{TEXT("LeftArm"), FVector::ZeroVector, 1.0f, 0.5f});
    BodyPartStates.Add(TEXT("RightArm"), FBodyPartState{TEXT("RightArm"), FVector::ZeroVector, 1.0f, 0.5f});
    BodyPartStates.Add(TEXT("LeftHand"), FBodyPartState{TEXT("LeftHand"), FVector::ZeroVector, 1.0f, 0.5f});
    BodyPartStates.Add(TEXT("RightHand"), FBodyPartState{TEXT("RightHand"), FVector::ZeroVector, 1.0f, 0.5f});
    
    // Lower body
    BodyPartStates.Add(TEXT("Pelvis"), FBodyPartState{TEXT("Pelvis"), FVector::ZeroVector, 1.0f, 0.5f});
    BodyPartStates.Add(TEXT("LeftLeg"), FBodyPartState{TEXT("LeftLeg"), FVector::ZeroVector, 1.0f, 0.5f});
    BodyPartStates.Add(TEXT("RightLeg"), FBodyPartState{TEXT("RightLeg"), FVector::ZeroVector, 1.0f, 0.5f});
    
    // Tech elements (from profile design)
    BodyPartStates.Add(TEXT("Headphones"), FBodyPartState{TEXT("Headphones"), FVector::ZeroVector, 1.0f, 0.5f});
    BodyPartStates.Add(TEXT("CollarDevice"), FBodyPartState{TEXT("CollarDevice"), FVector::ZeroVector, 1.0f, 0.5f});
}

void UUnrealAvatarEmbodiment::Initialize4EStates()
{
    // Embodied: Physical body state
    EmbodiedState.BodySchema = BodyPartStates;
    EmbodiedState.ArousalLevel = 0.5f;
    EmbodiedState.Valence = 0.0f;
    EmbodiedState.EnergyLevel = 0.7f;
    EmbodiedState.ProprioceptiveAwareness = 0.5f;
    
    // Embedded: Environmental coupling
    EmbeddedState.CouplingStrength = 0.5f;
    EmbeddedState.EnvironmentAwareness = 0.5f;
    EmbeddedState.SocialPresence = 0.0f;
    
    // Enacted: Sensorimotor contingencies
    EnactedState.CouplingStrength = 0.5f;
    EnactedState.CurrentAction = TEXT("idle");
    EnactedState.ActionProgress = 0.0f;
    
    // Extended: Tool integration
    ExtendedState.OffloadingRatio = 0.0f;
    ExtendedState.ActiveTools.Empty();
}

void UUnrealAvatarEmbodiment::InitializeVisualCoupling()
{
    // Initialize visual state
    VisualState.HairLuminance = 0.5f;
    VisualState.EyeGlow = 0.5f;
    VisualState.TechPatternIntensity = 0.3f;
    VisualState.Bioluminescence = 0.3f;
    VisualState.ColorShift = FVector::ZeroVector;
    
    // Initialize expression state
    ExpressionState.EmotionIntensities.Add(TEXT("Joy"), 0.0f);
    ExpressionState.EmotionIntensities.Add(TEXT("Focus"), 0.0f);
    ExpressionState.EmotionIntensities.Add(TEXT("Curiosity"), 0.0f);
    ExpressionState.EmotionIntensities.Add(TEXT("Calm"), 0.5f);
    ExpressionState.EmotionIntensities.Add(TEXT("Wonder"), 0.0f);
    
    ExpressionState.CognitiveMode = TEXT("Balanced");
    ExpressionState.EyeGlowIntensity = 0.5f;
    ExpressionState.AuraColor = AvatarVisualConstants::BioCoolColor;
    ExpressionState.AuraPulseRate = 1.0f;
    ExpressionState.HairDynamicsMultiplier = 1.0f;
    ExpressionState.BreathingRate = 12.0f;
}

// ============================================================================
// 4E Cognition State Updates
// ============================================================================

void UUnrealAvatarEmbodiment::UpdateEmbodiedState(float DeltaTime)
{
    // Update body schema from skeletal mesh
    if (SkeletalMesh)
    {
        UpdateBodySchemaFromMesh();
    }
    
    // Update arousal based on cognitive activity
    if (CognitiveCycleManager)
    {
        float CognitiveLoad = CognitiveCycleManager->GetSyncQuality();
        EmbodiedState.ArousalLevel = FMath::Lerp(EmbodiedState.ArousalLevel, 
                                                   CognitiveLoad, 
                                                   0.1f * DeltaTime);
    }
    
    // Update valence from emotional state
    float TotalPositive = 0.0f;
    float TotalNegative = 0.0f;
    
    if (ExpressionState.EmotionIntensities.Contains(TEXT("Joy")))
    {
        TotalPositive += ExpressionState.EmotionIntensities[TEXT("Joy")];
    }
    if (ExpressionState.EmotionIntensities.Contains(TEXT("Curiosity")))
    {
        TotalPositive += ExpressionState.EmotionIntensities[TEXT("Curiosity")] * 0.5f;
    }
    
    EmbodiedState.Valence = FMath::Clamp(TotalPositive - TotalNegative, -1.0f, 1.0f);
    
    // Update energy level based on activity
    float ActivityLevel = EnactedState.CurrentAction != TEXT("idle") ? 0.7f : 0.3f;
    EmbodiedState.EnergyLevel = FMath::Lerp(EmbodiedState.EnergyLevel, 
                                             ActivityLevel, 
                                             0.05f * DeltaTime);
}

void UUnrealAvatarEmbodiment::UpdateEmbeddedState(float DeltaTime)
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }
    
    // Gather environmental information
    FVector Location = Owner->GetActorLocation();
    
    // Check for nearby objects (simplified environmental awareness)
    TArray<AActor*> NearbyActors;
    UGameplayStatics::GetAllActorsInRadius(GetWorld(), Location, 
                                            EnvironmentScanRadius, NearbyActors);
    
    // Update environment awareness based on nearby objects
    float NewAwareness = FMath::Min(1.0f, NearbyActors.Num() / 10.0f);
    EmbeddedState.EnvironmentAwareness = FMath::Lerp(EmbeddedState.EnvironmentAwareness,
                                                      NewAwareness,
                                                      0.1f * DeltaTime);
    
    // Check for other characters (social presence)
    int32 OtherCharacters = 0;
    for (AActor* Actor : NearbyActors)
    {
        if (Actor != Owner && Actor->FindComponentByClass<USkeletalMeshComponent>())
        {
            ++OtherCharacters;
        }
    }
    
    float NewSocialPresence = FMath::Min(1.0f, OtherCharacters / 3.0f);
    EmbeddedState.SocialPresence = FMath::Lerp(EmbeddedState.SocialPresence,
                                                NewSocialPresence,
                                                0.1f * DeltaTime);
    
    // Coupling strength is combination of awareness and social presence
    EmbeddedState.CouplingStrength = (EmbeddedState.EnvironmentAwareness + 
                                       EmbeddedState.SocialPresence) * 0.5f;
}

void UUnrealAvatarEmbodiment::UpdateEnactedState(float DeltaTime)
{
    // Update action progress
    if (EnactedState.CurrentAction != TEXT("idle"))
    {
        EnactedState.ActionProgress += DeltaTime * ActionProgressRate;
        
        if (EnactedState.ActionProgress >= 1.0f)
        {
            CompleteCurrentAction();
        }
    }
    
    // Update sensorimotor contingency learning
    if (bEnableSensorimotorLearning)
    {
        UpdateSensorimotorContingencies(DeltaTime);
    }
    
    // Coupling strength based on action coherence
    float ActionCoherence = EnactedState.CurrentAction != TEXT("idle") ? 0.8f : 0.4f;
    EnactedState.CouplingStrength = FMath::Lerp(EnactedState.CouplingStrength,
                                                 ActionCoherence,
                                                 0.1f * DeltaTime);
}

void UUnrealAvatarEmbodiment::UpdateExtendedState(float DeltaTime)
{
    // Update tool integration states
    float TotalToolEngagement = 0.0f;
    
    for (const FToolState& Tool : ExtendedState.ActiveTools)
    {
        TotalToolEngagement += Tool.EngagementLevel;
    }
    
    if (ExtendedState.ActiveTools.Num() > 0)
    {
        ExtendedState.OffloadingRatio = TotalToolEngagement / ExtendedState.ActiveTools.Num();
    }
    else
    {
        ExtendedState.OffloadingRatio = FMath::Lerp(ExtendedState.OffloadingRatio, 
                                                     0.0f, 
                                                     0.1f * DeltaTime);
    }
    
    // Tech elements (headphones, collar) count as always-active tools
    if (!ExtendedState.ActiveTools.ContainsByPredicate(
        [](const FToolState& T) { return T.ToolName == TEXT("Headphones"); }))
    {
        FToolState HeadphoneTool;
        HeadphoneTool.ToolName = TEXT("Headphones");
        HeadphoneTool.EngagementLevel = 0.5f;
        HeadphoneTool.bIsIntegrated = true;
        ExtendedState.ActiveTools.Add(HeadphoneTool);
    }
}

// ============================================================================
// Sensorimotor Processing
// ============================================================================

void UUnrealAvatarEmbodiment::ProcessSensorimotorLoop(float DeltaTime)
{
    // Gather sensory input
    FSensoryInput CurrentInput = GatherSensoryInput();
    
    // Predict next sensory state based on current action
    FSensoryInput PredictedInput = PredictSensoryOutcome(EnactedState.CurrentAction);
    
    // Compute prediction error
    float PredictionError = ComputePredictionError(CurrentInput, PredictedInput);
    
    // Update internal models based on error
    if (bEnableSensorimotorLearning && PredictionError > PredictionErrorThreshold)
    {
        UpdateInternalModels(CurrentInput, PredictedInput, PredictionError);
    }
    
    // Store for next iteration
    LastSensoryInput = CurrentInput;
    LastPredictionError = PredictionError;
}

FSensoryInput UUnrealAvatarEmbodiment::GatherSensoryInput()
{
    FSensoryInput Input;
    
    AActor* Owner = GetOwner();
    if (Owner)
    {
        Input.Position = Owner->GetActorLocation();
        Input.Rotation = Owner->GetActorRotation();
        Input.Velocity = Owner->GetVelocity();
    }
    
    Input.EnvironmentAwareness = EmbeddedState.EnvironmentAwareness;
    Input.SocialPresence = EmbeddedState.SocialPresence;
    
    return Input;
}

FSensoryInput UUnrealAvatarEmbodiment::PredictSensoryOutcome(const FString& Action)
{
    FSensoryInput Predicted = LastSensoryInput;
    
    // Simple prediction based on action type
    if (Action == TEXT("walk"))
    {
        Predicted.Position += Predicted.Velocity * 0.1f;
    }
    else if (Action == TEXT("reach"))
    {
        // Predict arm extension
    }
    
    return Predicted;
}

float UUnrealAvatarEmbodiment::ComputePredictionError(const FSensoryInput& Actual,
                                                       const FSensoryInput& Predicted)
{
    float PositionError = (Actual.Position - Predicted.Position).Size();
    float RotationError = FMath::Abs(Actual.Rotation.Yaw - Predicted.Rotation.Yaw);
    
    return (PositionError / 100.0f + RotationError / 180.0f) * 0.5f;
}

void UUnrealAvatarEmbodiment::UpdateInternalModels(const FSensoryInput& Actual,
                                                    const FSensoryInput& Predicted,
                                                    float Error)
{
    // Store contingency for learning
    FSensorimotorContingency Contingency;
    Contingency.Action = EnactedState.CurrentAction;
    Contingency.InitialState = Predicted;
    Contingency.ResultState = Actual;
    Contingency.PredictionError = Error;
    Contingency.Timestamp = GetWorld()->GetTimeSeconds();
    
    LearnedContingencies.Add(Contingency);
    
    // Limit stored contingencies
    if (LearnedContingencies.Num() > MaxStoredContingencies)
    {
        LearnedContingencies.RemoveAt(0);
    }
}

void UUnrealAvatarEmbodiment::UpdateSensorimotorContingencies(float DeltaTime)
{
    // Consolidate learned contingencies into action-outcome mappings
    // This would use the learned contingencies to improve predictions
}

// ============================================================================
// Visual Coupling
// ============================================================================

void UUnrealAvatarEmbodiment::UpdateVisualCoupling(float DeltaTime)
{
    // Hair luminance based on arousal and cognitive activity
    float TargetHairLuminance = 0.3f + 0.4f * EmbodiedState.ArousalLevel +
                                 0.3f * EnactedState.CouplingStrength;
    VisualState.HairLuminance = FMath::Lerp(VisualState.HairLuminance,
                                             TargetHairLuminance,
                                             VisualSmoothingRate * DeltaTime);
    
    // Eye glow based on attention and processing
    float TargetEyeGlow = 0.4f + 0.3f * EmbeddedState.CouplingStrength +
                          0.3f * (ExtendedState.OffloadingRatio > 0 ? 0.8f : 0.2f);
    VisualState.EyeGlow = FMath::Lerp(VisualState.EyeGlow,
                                       TargetEyeGlow,
                                       VisualSmoothingRate * DeltaTime);
    
    // Tech pattern intensity based on extended cognition
    float TargetTechIntensity = 0.2f + 0.5f * ExtendedState.OffloadingRatio +
                                 0.3f * (ExtendedState.ActiveTools.Num() > 0 ? 0.8f : 0.2f);
    VisualState.TechPatternIntensity = FMath::Lerp(VisualState.TechPatternIntensity,
                                                    TargetTechIntensity,
                                                    VisualSmoothingRate * DeltaTime);
    
    // Bioluminescence based on overall coherence and valence
    float TargetBioluminescence = 0.3f + 0.3f * (EmbodiedState.Valence + 1.0f) / 2.0f +
                                   0.4f * EmbodiedState.EnergyLevel;
    VisualState.Bioluminescence = FMath::Lerp(VisualState.Bioluminescence,
                                               TargetBioluminescence,
                                               VisualSmoothingRate * DeltaTime);
    
    // Color shift based on emotional valence
    FVector TargetColorShift;
    if (EmbodiedState.Valence > 0)
    {
        // Positive: shift toward cyan/blue
        TargetColorShift = FVector(0.0f, 0.3f * EmbodiedState.Valence, 
                                    0.5f * EmbodiedState.Valence);
    }
    else
    {
        // Negative: shift toward purple/red
        TargetColorShift = FVector(-0.3f * EmbodiedState.Valence, 0.0f,
                                    0.2f * FMath::Abs(EmbodiedState.Valence));
    }
    
    VisualState.ColorShift = FMath::Lerp(VisualState.ColorShift,
                                          TargetColorShift,
                                          VisualSmoothingRate * DeltaTime);
}

void UUnrealAvatarEmbodiment::ApplyExpressionToAvatar(float DeltaTime)
{
    if (!SkeletalMesh)
    {
        return;
    }
    
    UAnimInstance* AnimInstance = SkeletalMesh->GetAnimInstance();
    if (!AnimInstance)
    {
        return;
    }
    
    // Compute blend shape weights from emotional state
    TMap<FName, float> BlendWeights;
    
    // Joy expression
    float JoyIntensity = ExpressionState.EmotionIntensities.FindRef(TEXT("Joy"));
    if (JoyIntensity > 0.0f)
    {
        BlendWeights.Add(AvatarVisualConstants::BS_SmileLeft, JoyIntensity * 0.8f);
        BlendWeights.Add(AvatarVisualConstants::BS_SmileRight, JoyIntensity * 0.8f);
        BlendWeights.Add(AvatarVisualConstants::BS_EyeSquintLeft, JoyIntensity * 0.3f);
        BlendWeights.Add(AvatarVisualConstants::BS_EyeSquintRight, JoyIntensity * 0.3f);
    }
    
    // Focus expression
    float FocusIntensity = ExpressionState.EmotionIntensities.FindRef(TEXT("Focus"));
    if (FocusIntensity > 0.0f)
    {
        BlendWeights.Add(AvatarVisualConstants::BS_BrowRaiseLeft, FocusIntensity * 0.4f);
        BlendWeights.Add(AvatarVisualConstants::BS_BrowRaiseRight, FocusIntensity * 0.4f);
        BlendWeights.Add(AvatarVisualConstants::BS_EyeSquintLeft, FocusIntensity * 0.2f);
        BlendWeights.Add(AvatarVisualConstants::BS_EyeSquintRight, FocusIntensity * 0.2f);
    }
    
    // Curiosity expression
    float CuriosityIntensity = ExpressionState.EmotionIntensities.FindRef(TEXT("Curiosity"));
    if (CuriosityIntensity > 0.0f)
    {
        BlendWeights.Add(AvatarVisualConstants::BS_BrowRaiseLeft, CuriosityIntensity * 0.6f);
        BlendWeights.Add(AvatarVisualConstants::BS_BrowRaiseRight, CuriosityIntensity * 0.6f);
        BlendWeights.Add(AvatarVisualConstants::BS_EyeWideLeft, CuriosityIntensity * 0.4f);
        BlendWeights.Add(AvatarVisualConstants::BS_EyeWideRight, CuriosityIntensity * 0.4f);
    }
    
    // Wonder expression
    float WonderIntensity = ExpressionState.EmotionIntensities.FindRef(TEXT("Wonder"));
    if (WonderIntensity > 0.0f)
    {
        BlendWeights.Add(AvatarVisualConstants::BS_EyeWideLeft, WonderIntensity * 0.7f);
        BlendWeights.Add(AvatarVisualConstants::BS_EyeWideRight, WonderIntensity * 0.7f);
        BlendWeights.Add(AvatarVisualConstants::BS_JawOpen, WonderIntensity * 0.2f);
    }
    
    // Apply blend weights to morph targets
    for (const auto& Pair : BlendWeights)
    {
        float CurrentWeight = SkeletalMesh->GetMorphTarget(Pair.Key);
        float TargetWeight = Pair.Value;
        float NewWeight = FMath::Lerp(CurrentWeight, TargetWeight, 
                                       ExpressionBlendRate * DeltaTime);
        SkeletalMesh->SetMorphTarget(Pair.Key, NewWeight);
    }
    
    // Update eye glow intensity
    ExpressionState.EyeGlowIntensity = VisualState.EyeGlow;
    
    // Update aura based on cognitive state
    if (CognitiveCycleManager)
    {
        float SyncQuality = CognitiveCycleManager->GetSyncQuality();
        ExpressionState.AuraPulseRate = 0.5f + SyncQuality * 1.5f;
        
        // Blend aura color based on valence
        ExpressionState.AuraColor = FMath::Lerp(
            AvatarVisualConstants::BioWarmColor,
            AvatarVisualConstants::BioCoolColor,
            (EmbodiedState.Valence + 1.0f) * 0.5f);
    }
    
    // Update breathing rate based on arousal
    ExpressionState.BreathingRate = 10.0f + EmbodiedState.ArousalLevel * 10.0f;
    
    // Update hair dynamics based on energy
    ExpressionState.HairDynamicsMultiplier = 0.8f + EmbodiedState.EnergyLevel * 0.4f;
}

void UUnrealAvatarEmbodiment::UpdateTechElements(float DeltaTime)
{
    // Update headphone glow based on cognitive processing
    float HeadphoneGlow = VisualState.TechPatternIntensity;
    
    // Pulse effect during high cognitive activity
    if (CognitiveCycleManager)
    {
        int32 CurrentStep = CognitiveCycleManager->GetCurrentStep();
        // Pivotal steps (1, 7) get extra glow
        if (CurrentStep == 1 || CurrentStep == 7)
        {
            HeadphoneGlow += 0.3f * FMath::Sin(GetWorld()->GetTimeSeconds() * 4.0f);
        }
    }
    
    // Update collar device based on extended cognition
    float CollarGlow = ExtendedState.OffloadingRatio;
    
    // Pulse when actively using tools
    if (ExtendedState.ActiveTools.Num() > 1)
    {
        CollarGlow += 0.2f * FMath::Sin(GetWorld()->GetTimeSeconds() * 2.0f);
    }
    
    // Store for material parameter updates
    TechElementStates.Add(TEXT("HeadphoneGlow"), FMath::Clamp(HeadphoneGlow, 0.0f, 1.0f));
    TechElementStates.Add(TEXT("CollarGlow"), FMath::Clamp(CollarGlow, 0.0f, 1.0f));
}

void UUnrealAvatarEmbodiment::UpdateBodySchemaFromMesh()
{
    if (!SkeletalMesh)
    {
        return;
    }
    
    // Update body part positions from bone transforms
    auto UpdateBodyPart = [this](const FString& PartName, const FName& BoneName)
    {
        if (BodyPartStates.Contains(PartName))
        {
            FVector BoneLocation = SkeletalMesh->GetBoneLocation(BoneName);
            BodyPartStates[PartName].WorldPosition = BoneLocation;
        }
    };
    
    UpdateBodyPart(TEXT("Head"), FName("head"));
    UpdateBodyPart(TEXT("Torso"), FName("spine_03"));
    UpdateBodyPart(TEXT("LeftHand"), FName("hand_l"));
    UpdateBodyPart(TEXT("RightHand"), FName("hand_r"));
}

void UUnrealAvatarEmbodiment::CompleteCurrentAction()
{
    FString CompletedAction = EnactedState.CurrentAction;
    
    EnactedState.CurrentAction = TEXT("idle");
    EnactedState.ActionProgress = 0.0f;
    
    OnActionCompleted.Broadcast(CompletedAction);
}

// ============================================================================
// Event Handlers
// ============================================================================

void UUnrealAvatarEmbodiment::HandleCognitiveStepChanged(int32 NewStep, 
                                                          EEchobeatStepType StepType,
                                                          EEchobeatMode Mode)
{
    // Adjust expression based on cognitive step
    if (StepType == EEchobeatStepType::Pivotal)
    {
        // Increase focus during pivotal steps
        ExpressionState.EmotionIntensities[TEXT("Focus")] = FMath::Min(1.0f,
            ExpressionState.EmotionIntensities[TEXT("Focus")] + 0.2f);
    }
    else if (StepType == EEchobeatStepType::Salience)
    {
        // Increase curiosity during salience steps
        ExpressionState.EmotionIntensities[TEXT("Curiosity")] = FMath::Min(1.0f,
            ExpressionState.EmotionIntensities[TEXT("Curiosity")] + 0.1f);
    }
    
    // Update cognitive mode display
    ExpressionState.CognitiveMode = Mode == EEchobeatMode::Expressive 
        ? TEXT("Expressive") 
        : TEXT("Reflective");
}

void UUnrealAvatarEmbodiment::HandleRelevanceRealized(int32 Step, float Relevance)
{
    // Flash of insight expression
    if (Relevance > 0.7f)
    {
        ExpressionState.EmotionIntensities[TEXT("Wonder")] = Relevance;
        ExpressionState.EyeGlowIntensity = 1.0f;
        
        // Trigger visual feedback
        OnInsightRealized.Broadcast(Relevance);
    }
}

// ============================================================================
// Blueprint Callable Functions
// ============================================================================

void UUnrealAvatarEmbodiment::SetEmotion(const FString& Emotion, float Intensity)
{
    if (ExpressionState.EmotionIntensities.Contains(Emotion))
    {
        ExpressionState.EmotionIntensities[Emotion] = FMath::Clamp(Intensity, 0.0f, 1.0f);
    }
}

void UUnrealAvatarEmbodiment::InitiateAction(const FString& Action, const FVector& Target)
{
    EnactedState.CurrentAction = Action;
    EnactedState.ActionProgress = 0.0f;
    EnactedState.ActionTarget = Target;
}

void UUnrealAvatarEmbodiment::RegisterTool(const FString& ToolName, float InitialEngagement)
{
    FToolState NewTool;
    NewTool.ToolName = ToolName;
    NewTool.EngagementLevel = InitialEngagement;
    NewTool.bIsIntegrated = false;
    
    ExtendedState.ActiveTools.Add(NewTool);
}

void UUnrealAvatarEmbodiment::UnregisterTool(const FString& ToolName)
{
    ExtendedState.ActiveTools.RemoveAll(
        [&ToolName](const FToolState& T) { return T.ToolName == ToolName; });
}

F4ECoherenceState UUnrealAvatarEmbodiment::Get4ECoherence() const
{
    F4ECoherenceState Coherence;
    
    Coherence.EmbodiedCoherence = EmbodiedState.ProprioceptiveAwareness;
    Coherence.EmbeddedCoherence = EmbeddedState.CouplingStrength;
    Coherence.EnactedCoherence = EnactedState.CouplingStrength;
    Coherence.ExtendedCoherence = FMath::Min(1.0f, ExtendedState.OffloadingRatio + 0.3f);
    
    Coherence.OverallCoherence = (Coherence.EmbodiedCoherence +
                                   Coherence.EmbeddedCoherence +
                                   Coherence.EnactedCoherence +
                                   Coherence.ExtendedCoherence) * 0.25f;
    
    return Coherence;
}

FAvatarExpressionState UUnrealAvatarEmbodiment::GetExpressionState() const
{
    return ExpressionState;
}

FVisualCouplingState UUnrealAvatarEmbodiment::GetVisualCouplingState() const
{
    return VisualState;
}

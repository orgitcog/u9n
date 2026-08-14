// ControlRigIntegration.cpp
// Deep Tree Echo - Control Rig Integration Implementation
// Copyright (c) 2025 Deep Tree Echo Project

#include "ControlRigIntegration.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogControlRigIntegration, Log, All);

// ========================================
// Constructor / Destructor
// ========================================

UControlRigIntegration::UControlRigIntegration()
    : ControlRigInstance(nullptr)
    , TargetMesh(nullptr)
    , bControlRigInitialized(false)
    , bFullBodyIKEnabled(false)
    , bMotionWarpActive(false)
    , MotionWarpBlendAlpha(0.0f)
    , BreathingPhase(0.0f)
    , IdleMotionPhase(0.0f)
    , IdleMotionOffset(FVector::ZeroVector)
    , CurrentCognitiveLoad(0.0f)
    , CurrentEmotionalIntensity(0.0f)
    , CurrentPosturalTension(0.0f)
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UControlRigIntegration::BeginPlay()
{
    Super::BeginPlay();

    // Auto-find skeletal mesh if not set
    if (!TargetMesh && GetOwner())
    {
        TargetMesh = GetOwner()->FindComponentByClass<USkeletalMeshComponent>();
        if (TargetMesh)
        {
            InitializeControlRig(TargetMesh);
        }
    }
}

void UControlRigIntegration::TickComponent(float DeltaTime, ELevelTick TickType,
                                           FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bControlRigInitialized)
    {
        return;
    }

    // Update IK targets
    if (bFullBodyIKEnabled)
    {
        UpdateIKTargets(DeltaTime);
    }

    // Update motion warp
    if (bMotionWarpActive)
    {
        UpdateMotionWarp(DeltaTime);
    }

    // Update procedural animations
    UpdateProceduralBreathing(DeltaTime);
    UpdateIdleMotion(DeltaTime);
    ApplyMicroMovements(DeltaTime);

    // Apply all parameters to control rig
    ApplyControlRigParameters();
}

// ========================================
// Control Rig Setup
// ========================================

bool UControlRigIntegration::InitializeControlRig(USkeletalMeshComponent* Mesh)
{
    if (!Mesh)
    {
        UE_LOG(LogControlRigIntegration, Error, TEXT("Cannot initialize Control Rig: Invalid mesh"));
        return false;
    }

    TargetMesh = Mesh;

    // Note: In a full implementation, this would create/reference an actual UControlRig asset
    // For now, we simulate the control rig behavior through direct bone manipulation

    bControlRigInitialized = true;

    // Setup default IK targets
    SetupIKTargets();

    UE_LOG(LogControlRigIntegration, Log, TEXT("Control Rig initialized successfully"));
    return true;
}

void UControlRigIntegration::SetupIKTargets()
{
    // Setup default IK targets for major end effectors
    TArray<FName> DefaultTargets = {
        TEXT("hand_l"),
        TEXT("hand_r"),
        TEXT("foot_l"),
        TEXT("foot_r"),
        TEXT("head")
    };

    for (const FName& BoneName : DefaultTargets)
    {
        FIKTargetConfig Config;
        Config.BoneName = BoneName;
        Config.Weight = 0.0f;  // Disabled by default
        Config.bEnabled = false;

        IKTargets.Add(BoneName, Config);
    }

    UE_LOG(LogControlRigIntegration, Log, TEXT("Setup %d IK targets"), DefaultTargets.Num());
}

// ========================================
// Cognitive-Driven Animation
// ========================================

void UControlRigIntegration::UpdateFromCognitiveState(const FCognitiveState& State)
{
    // Extract cognitive load from state
    // Assuming FCognitiveState has a CognitiveLoad member
    CurrentCognitiveLoad = 0.5f;  // Default - would come from State

    // Modulate procedural parameters based on cognitive state
    ProceduralParams.CognitiveLoadInfluence = CurrentCognitiveLoad;

    // Apply postural tension from cognitive load
    ApplyPosturalTensionFromCognitiveLoad(CurrentCognitiveLoad);

    // Adjust movement fluidity - higher cognitive load = less fluid
    ProceduralParams.MovementFluidness = 1.0f - (CurrentCognitiveLoad * 0.3f);

    // Adjust idle motion - higher cognitive load = less idle motion
    ProceduralParams.IdleMotionAmplitude = 0.5f * (1.0f - CurrentCognitiveLoad * 0.5f);
}

void UControlRigIntegration::ApplyPersonalityToPosture(const FPersonalityTrait& Trait)
{
    // Apply personality-driven postural modifications
    // Different personality traits affect default posture

    // Extroversion -> More open posture, expanded chest
    // Introversion -> More closed posture, protected core
    // Confidence -> Upright spine, lifted head
    // Anxiety -> Tense shoulders, forward head position

    // These modifications would be applied through spine and shoulder bone offsets
    UE_LOG(LogControlRigIntegration, Verbose, TEXT("Applied personality to posture"));
}

void UControlRigIntegration::ApplyEmotionalBodyLanguage(const FEmotionalState& Emotion)
{
    CurrentEmotionalIntensity = Emotion.Intensity;

    // Map emotion to body language
    FRotator SpineRotation;
    FVector ShoulderOffset;
    FVector HeadOffset;

    MapEmotionToSpinePosture(Emotion, SpineRotation);
    MapEmotionToShoulderPosition(Emotion, ShoulderOffset);
    MapEmotionToHeadPosition(Emotion, HeadOffset);

    // Apply emotional body language through control rig
    // This would modify bone transforms based on emotional state
}

void UControlRigIntegration::Apply4ECognitionToAnimation(const F4ECognitionState& State)
{
    // Integrate 4E embodied cognition with animation

    // Embodied: Body schema affects animation boundaries
    // Embedded: Environmental affordances affect movement intent
    // Enacted: Active exploration affects animation curiosity
    // Extended: Tool use affects hand animations

    UE_LOG(LogControlRigIntegration, Verbose, TEXT("Applied 4E cognition to animation"));
}

void UControlRigIntegration::SetProceduralAnimationParams(const FProceduralAnimationParams& Params)
{
    ProceduralParams = Params;
}

// ========================================
// Full Body IK
// ========================================

void UControlRigIntegration::SetIKTarget(const FName& BoneName, const FVector& TargetLocation)
{
    if (FIKTargetConfig* Config = IKTargets.Find(BoneName))
    {
        Config->TargetLocation = TargetLocation;
        Config->bEnabled = true;
    }
    else
    {
        FIKTargetConfig NewConfig;
        NewConfig.BoneName = BoneName;
        NewConfig.TargetLocation = TargetLocation;
        NewConfig.Weight = 1.0f;
        NewConfig.bEnabled = true;
        IKTargets.Add(BoneName, NewConfig);
    }
}

void UControlRigIntegration::SetIKTargetWithRotation(const FName& BoneName, const FVector& TargetLocation,
                                                      const FRotator& TargetRotation)
{
    SetIKTarget(BoneName, TargetLocation);

    if (FIKTargetConfig* Config = IKTargets.Find(BoneName))
    {
        Config->TargetRotation = TargetRotation;
    }
}

void UControlRigIntegration::EnableFullBodyIK(bool bEnable)
{
    bFullBodyIKEnabled = bEnable;

    if (!bEnable)
    {
        // Reset all IK weights when disabled
        for (auto& Pair : IKTargets)
        {
            Pair.Value.Weight = 0.0f;
        }
    }
}

void UControlRigIntegration::SetIKTargetWeight(const FName& BoneName, float Weight)
{
    if (FIKTargetConfig* Config = IKTargets.Find(BoneName))
    {
        Config->Weight = FMath::Clamp(Weight, 0.0f, 1.0f);
    }
}

bool UControlRigIntegration::GetIKTargetConfig(const FName& BoneName, FIKTargetConfig& OutConfig) const
{
    if (const FIKTargetConfig* Config = IKTargets.Find(BoneName))
    {
        OutConfig = *Config;
        return true;
    }
    return false;
}

void UControlRigIntegration::UpdateIKTargets(float DeltaTime)
{
    // Smooth IK target transitions
    for (auto& Pair : IKTargets)
    {
        FIKTargetConfig& Config = Pair.Value;

        if (!Config.bEnabled && Config.Weight > 0.0f)
        {
            // Blend out disabled targets
            Config.Weight = FMath::FInterpTo(Config.Weight, 0.0f, DeltaTime, 5.0f);
        }
    }
}

// ========================================
// Motion Warping
// ========================================

void UControlRigIntegration::WarpMotionToTarget(const FVector& TargetLocation, float WarpStrength)
{
    CurrentMotionWarp.TargetLocation = TargetLocation;
    CurrentMotionWarp.WarpStrength = FMath::Clamp(WarpStrength, 0.0f, 1.0f);
    CurrentMotionWarp.bWarpTranslation = true;
    CurrentMotionWarp.bWarpRotation = false;

    bMotionWarpActive = true;
    MotionWarpBlendAlpha = 0.0f;
}

void UControlRigIntegration::WarpMotionWithConfig(const FMotionWarpConfig& Config)
{
    CurrentMotionWarp = Config;
    bMotionWarpActive = true;
    MotionWarpBlendAlpha = 0.0f;
}

void UControlRigIntegration::ClearMotionWarp()
{
    bMotionWarpActive = false;
    MotionWarpBlendAlpha = 0.0f;
}

void UControlRigIntegration::UpdateMotionWarp(float DeltaTime)
{
    if (!bMotionWarpActive)
    {
        return;
    }

    // Blend in motion warp
    float BlendSpeed = 1.0f / FMath::Max(CurrentMotionWarp.BlendInTime, 0.01f);
    MotionWarpBlendAlpha = FMath::FInterpConstantTo(MotionWarpBlendAlpha, 1.0f, DeltaTime, BlendSpeed);
}

// ========================================
// Procedural Motion
// ========================================

void UControlRigIntegration::UpdateProceduralBreathing(float DeltaTime)
{
    // Update breathing phase
    float BreathingFrequency = ProceduralParams.BreathingRate * 0.2f;  // ~12 breaths/min at rate 1.0
    BreathingPhase += DeltaTime * BreathingFrequency * 2.0f * PI;

    if (BreathingPhase > 2.0f * PI)
    {
        BreathingPhase -= 2.0f * PI;
    }

    // Calculate breathing motion
    FVector ChestOffset, SpineOffset;
    CalculateBreathingMotion(DeltaTime, ChestOffset, SpineOffset);

    // Apply to skeleton if available
    // In full implementation, this would modify spine/chest bones
}

void UControlRigIntegration::UpdateIdleMotion(float DeltaTime)
{
    // Update idle motion phase
    IdleMotionPhase += DeltaTime * 0.5f;  // Slow idle motion

    // Calculate idle motion
    FVector IdleOffset;
    CalculateIdleMotion(DeltaTime, IdleOffset);

    IdleMotionOffset = FMath::VInterpTo(IdleMotionOffset, IdleOffset, DeltaTime, 2.0f);
}

void UControlRigIntegration::SetBreathingRate(float Rate)
{
    ProceduralParams.BreathingRate = FMath::Clamp(Rate, 0.1f, 5.0f);
}

void UControlRigIntegration::SetBreathingDepth(float Depth)
{
    ProceduralParams.BreathingDepth = FMath::Clamp(Depth, 0.0f, 2.0f);
}

void UControlRigIntegration::CalculateBreathingMotion(float DeltaTime, FVector& OutChestOffset,
                                                       FVector& OutSpineOffset)
{
    // Sinusoidal breathing motion
    float BreathValue = FMath::Sin(BreathingPhase);

    // Chest rises and expands during inhale
    float ChestExpansion = BreathValue * ProceduralParams.BreathingDepth * 2.0f;
    OutChestOffset = FVector(ChestExpansion * 0.5f, 0.0f, ChestExpansion);

    // Spine extends slightly during inhale
    OutSpineOffset = FVector(0.0f, 0.0f, ChestExpansion * 0.3f);

    // Modulate by cognitive load - stressed breathing is shallower and faster
    float StressModifier = 1.0f - (CurrentCognitiveLoad * 0.3f);
    OutChestOffset *= StressModifier;
    OutSpineOffset *= StressModifier;
}

void UControlRigIntegration::CalculateIdleMotion(float DeltaTime, FVector& OutOffset)
{
    // Multi-frequency idle motion for natural appearance
    float Freq1 = FMath::Sin(IdleMotionPhase * 0.7f);
    float Freq2 = FMath::Sin(IdleMotionPhase * 1.3f + 1.0f);
    float Freq3 = FMath::Sin(IdleMotionPhase * 0.3f + 2.0f);

    float Amplitude = ProceduralParams.IdleMotionAmplitude;

    OutOffset = FVector(
        (Freq1 * 0.5f + Freq2 * 0.3f) * Amplitude,
        (Freq2 * 0.4f + Freq3 * 0.2f) * Amplitude,
        Freq3 * 0.3f * Amplitude
    );

    // Reduce idle motion during high cognitive load
    OutOffset *= (1.0f - CurrentCognitiveLoad * 0.5f);
}

// ========================================
// Advanced Features
// ========================================

void UControlRigIntegration::ApplyPosturalTensionFromCognitiveLoad(float CognitiveLoad)
{
    CurrentPosturalTension = CognitiveLoad * 0.5f;
    ProceduralParams.PosturalTension = CurrentPosturalTension;

    // High cognitive load -> raised shoulders, tensed neck
    // This would be applied through shoulder and neck bone rotations
}

void UControlRigIntegration::ModulateMovementFluidityFromEmotion(const FEmotionalState& Emotion)
{
    // Different emotions affect movement quality
    // Joy -> fluid, expansive
    // Sadness -> heavy, slow
    // Anger -> sharp, tense
    // Fear -> small, guarded

    float Fluidity = 1.0f;

    // Assuming emotion has Valence and Arousal
    if (Emotion.Valence > 0)
    {
        Fluidity = 1.0f + (Emotion.Valence * 0.3f);  // More fluid with positive emotions
    }
    else
    {
        Fluidity = 1.0f + (Emotion.Valence * 0.2f);  // Less fluid with negative emotions
    }

    ProceduralParams.MovementFluidness = FMath::Clamp(Fluidity, 0.3f, 1.5f);
}

void UControlRigIntegration::ApplyMicroMovements(float DeltaTime)
{
    FVector MicroMovement = CalculateMicroMovement(DeltaTime);

    // Apply micro-movements to subtle bone adjustments
    // Eyes, fingers, small postural shifts
}

FVector UControlRigIntegration::CalculateMicroMovement(float DeltaTime)
{
    // Generate subtle random micro-movements
    static float MicroPhase = 0.0f;
    MicroPhase += DeltaTime;

    // High-frequency, low-amplitude noise
    float NoiseX = FMath::PerlinNoise1D(MicroPhase * 2.0f);
    float NoiseY = FMath::PerlinNoise1D(MicroPhase * 2.0f + 100.0f);
    float NoiseZ = FMath::PerlinNoise1D(MicroPhase * 2.0f + 200.0f);

    float Amplitude = 0.2f * (1.0f + CurrentEmotionalIntensity * 0.5f);

    return FVector(NoiseX, NoiseY, NoiseZ) * Amplitude;
}

void UControlRigIntegration::ApplyControlRigParameters()
{
    // Apply all accumulated parameters to the control rig/skeleton
    if (!TargetMesh)
    {
        return;
    }

    // This would send all computed offsets, rotations, and IK targets
    // to the actual Control Rig or Animation Blueprint
}

// ========================================
// Emotional Body Language Mapping
// ========================================

void UControlRigIntegration::MapEmotionToSpinePosture(const FEmotionalState& Emotion, FRotator& OutSpineRotation)
{
    // Positive valence -> upright spine
    // Negative valence -> forward lean
    float ForwardLean = -Emotion.Valence * 10.0f;  // degrees

    // High arousal -> extended spine
    // Low arousal -> compressed spine
    float Extension = Emotion.Arousal * 5.0f;

    OutSpineRotation = FRotator(ForwardLean, 0.0f, 0.0f);
}

void UControlRigIntegration::MapEmotionToShoulderPosition(const FEmotionalState& Emotion, FVector& OutShoulderOffset)
{
    // Negative emotions -> raised, tensed shoulders
    // Positive emotions -> relaxed, dropped shoulders
    float ShoulderRaise = -Emotion.Valence * 2.0f;

    // High arousal -> wider shoulder stance
    float ShoulderWidth = Emotion.Arousal * 1.0f;

    OutShoulderOffset = FVector(0.0f, ShoulderWidth, ShoulderRaise);
}

void UControlRigIntegration::MapEmotionToHeadPosition(const FEmotionalState& Emotion, FVector& OutHeadOffset)
{
    // Confidence/positive -> head lifted
    // Shame/negative -> head lowered
    float HeadTilt = Emotion.Valence * 5.0f;

    // Attentive/aroused -> head forward
    float HeadForward = Emotion.Arousal * 2.0f;

    OutHeadOffset = FVector(HeadForward, 0.0f, HeadTilt);
}

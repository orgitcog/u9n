// ControlRigIntegration.h
// Deep Tree Echo - Control Rig Integration for Cognitive-Driven Animation
// Copyright (c) 2025 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Animation/AnimInstance.h"
#include "ControlRig.h"
#include "ControlRigIntegration.generated.h"

// Forward declarations
struct FCognitiveState;
struct FPersonalityTrait;
struct FEmotionalState;
struct F4ECognitionState;

/**
 * IK Target Configuration
 * Defines an IK target for Full Body IK
 */
USTRUCT(BlueprintType)
struct FIKTargetConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK")
    FName BoneName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK")
    FVector TargetLocation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK")
    FRotator TargetRotation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK")
    float Weight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "IK")
    bool bEnabled;

    FIKTargetConfig()
        : BoneName(NAME_None)
        , TargetLocation(FVector::ZeroVector)
        , TargetRotation(FRotator::ZeroRotator)
        , Weight(1.0f)
        , bEnabled(true)
    {}
};

/**
 * Procedural Animation Parameters
 * Parameters for generating procedural animations from cognitive state
 */
USTRUCT(BlueprintType)
struct FProceduralAnimationParams
{
    GENERATED_BODY()

    // Cognitive influence
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural")
    float CognitiveLoadInfluence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural")
    float EmotionalIntensityInfluence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural")
    float PersonalityInfluence;

    // Movement modulation
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural")
    float MovementSpeed;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural")
    float MovementFluidness;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural")
    float PosturalTension;

    // Breathing and idle motion
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural")
    float BreathingRate;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural")
    float BreathingDepth;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural")
    float IdleMotionAmplitude;

    FProceduralAnimationParams()
        : CognitiveLoadInfluence(0.5f)
        , EmotionalIntensityInfluence(0.5f)
        , PersonalityInfluence(0.5f)
        , MovementSpeed(1.0f)
        , MovementFluidness(1.0f)
        , PosturalTension(0.0f)
        , BreathingRate(1.0f)
        , BreathingDepth(1.0f)
        , IdleMotionAmplitude(0.5f)
    {}
};

/**
 * Motion Warp Configuration
 * Configuration for motion warping to adapt animations to context
 */
USTRUCT(BlueprintType)
struct FMotionWarpConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warp")
    FVector TargetLocation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warp")
    FRotator TargetRotation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warp")
    float WarpStrength;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warp")
    float BlendInTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warp")
    float BlendOutTime;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warp")
    bool bWarpRotation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Warp")
    bool bWarpTranslation;

    FMotionWarpConfig()
        : TargetLocation(FVector::ZeroVector)
        , TargetRotation(FRotator::ZeroRotator)
        , WarpStrength(1.0f)
        , BlendInTime(0.2f)
        , BlendOutTime(0.2f)
        , bWarpRotation(true)
        , bWarpTranslation(true)
    {}
};

/**
 * UControlRigIntegration
 * 
 * Integrates Unreal Engine Control Rig system with Deep Tree Echo cognitive architecture.
 * Enables procedural animation generation from cognitive state, personality traits, and emotional state.
 * 
 * Key Features:
 * - Control Rig setup and management
 * - Full Body IK (FBIK) integration
 * - Cognitive-driven animation modulation
 * - Personality trait influence on posture and movement
 * - Emotional body language
 * - Motion warping for context adaptation
 * - Procedural breathing and idle motion
 * - 4E embodied cognition integration
 * 
 * Integration Points:
 * - Avatar3DComponentEnhanced: Real-time animation control
 * - PersonalityTraitSystem: Personality influence on movement
 * - NeurochemicalSimulationComponent: Neurochemical modulation
 * - EmbodiedCognitionComponent: Sensorimotor integration
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALECHO_API UControlRigIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    UControlRigIntegration();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // Control Rig Setup
    // ========================================

    /**
     * Initialize Control Rig for skeletal mesh
     * @param Mesh - Target skeletal mesh component
     * @return True if successfully initialized
     */
    UFUNCTION(BlueprintCallable, Category = "Control Rig")
    bool InitializeControlRig(USkeletalMeshComponent* Mesh);

    /**
     * Setup IK targets for Full Body IK
     */
    UFUNCTION(BlueprintCallable, Category = "Control Rig")
    void SetupIKTargets();

    /**
     * Get Control Rig instance
     * @return Control Rig instance
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Control Rig")
    UControlRig* GetControlRigInstance() const { return ControlRigInstance; }

    /**
     * Check if Control Rig is initialized
     * @return True if initialized
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Control Rig")
    bool IsControlRigInitialized() const { return bControlRigInitialized; }

    // ========================================
    // Cognitive-Driven Animation
    // ========================================

    /**
     * Update animation from cognitive state
     * Modulates animation based on cognitive load, attention, and processing
     * @param State - Current cognitive state
     */
    UFUNCTION(BlueprintCallable, Category = "Cognitive Animation")
    void UpdateFromCognitiveState(const FCognitiveState& State);

    /**
     * Apply personality traits to posture and movement
     * @param Trait - Personality trait to apply
     */
    UFUNCTION(BlueprintCallable, Category = "Cognitive Animation")
    void ApplyPersonalityToPosture(const FPersonalityTrait& Trait);

    /**
     * Apply emotional body language
     * Maps emotional state to body posture, tension, and movement quality
     * @param Emotion - Current emotional state
     */
    UFUNCTION(BlueprintCallable, Category = "Cognitive Animation")
    void ApplyEmotionalBodyLanguage(const FEmotionalState& Emotion);

    /**
     * Apply 4E embodied cognition to animation
     * Integrates sensorimotor contingencies and environmental coupling
     * @param State - Current 4E cognition state
     */
    UFUNCTION(BlueprintCallable, Category = "Cognitive Animation")
    void Apply4ECognitionToAnimation(const F4ECognitionState& State);

    /**
     * Set procedural animation parameters
     * @param Params - Procedural animation parameters
     */
    UFUNCTION(BlueprintCallable, Category = "Cognitive Animation")
    void SetProceduralAnimationParams(const FProceduralAnimationParams& Params);

    /**
     * Get current procedural animation parameters
     * @return Current parameters
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Cognitive Animation")
    FProceduralAnimationParams GetProceduralAnimationParams() const { return ProceduralParams; }

    // ========================================
    // Full Body IK
    // ========================================

    /**
     * Set IK target for specific bone
     * @param BoneName - Name of bone
     * @param TargetLocation - Target location in world space
     */
    UFUNCTION(BlueprintCallable, Category = "Full Body IK")
    void SetIKTarget(const FName& BoneName, const FVector& TargetLocation);

    /**
     * Set IK target with rotation
     * @param BoneName - Name of bone
     * @param TargetLocation - Target location in world space
     * @param TargetRotation - Target rotation in world space
     */
    UFUNCTION(BlueprintCallable, Category = "Full Body IK")
    void SetIKTargetWithRotation(const FName& BoneName, const FVector& TargetLocation, const FRotator& TargetRotation);

    /**
     * Enable or disable Full Body IK
     * @param bEnable - Enable state
     */
    UFUNCTION(BlueprintCallable, Category = "Full Body IK")
    void EnableFullBodyIK(bool bEnable);

    /**
     * Set IK target weight
     * @param BoneName - Name of bone
     * @param Weight - IK weight (0-1)
     */
    UFUNCTION(BlueprintCallable, Category = "Full Body IK")
    void SetIKTargetWeight(const FName& BoneName, float Weight);

    /**
     * Get IK target configuration
     * @param BoneName - Name of bone
     * @param OutConfig - Output IK target configuration
     * @return True if target exists
     */
    UFUNCTION(BlueprintCallable, Category = "Full Body IK")
    bool GetIKTargetConfig(const FName& BoneName, FIKTargetConfig& OutConfig) const;

    // ========================================
    // Motion Warping
    // ========================================

    /**
     * Warp motion to target location
     * Adapts animation to reach specific location/rotation
     * @param TargetLocation - Target location in world space
     * @param WarpStrength - Strength of warping (0-1)
     */
    UFUNCTION(BlueprintCallable, Category = "Motion Warping")
    void WarpMotionToTarget(const FVector& TargetLocation, float WarpStrength = 1.0f);

    /**
     * Warp motion with full configuration
     * @param Config - Motion warp configuration
     */
    UFUNCTION(BlueprintCallable, Category = "Motion Warping")
    void WarpMotionWithConfig(const FMotionWarpConfig& Config);

    /**
     * Clear motion warp
     */
    UFUNCTION(BlueprintCallable, Category = "Motion Warping")
    void ClearMotionWarp();

    /**
     * Check if motion warp is active
     * @return True if active
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Motion Warping")
    bool IsMotionWarpActive() const { return bMotionWarpActive; }

    // ========================================
    // Procedural Motion
    // ========================================

    /**
     * Update procedural breathing
     * Generates breathing motion based on neurochemical state
     * @param DeltaTime - Time delta
     */
    UFUNCTION(BlueprintCallable, Category = "Procedural Motion")
    void UpdateProceduralBreathing(float DeltaTime);

    /**
     * Update idle motion
     * Generates subtle idle motion to prevent static appearance
     * @param DeltaTime - Time delta
     */
    UFUNCTION(BlueprintCallable, Category = "Procedural Motion")
    void UpdateIdleMotion(float DeltaTime);

    /**
     * Set breathing rate
     * @param Rate - Breathing rate multiplier (1.0 = normal)
     */
    UFUNCTION(BlueprintCallable, Category = "Procedural Motion")
    void SetBreathingRate(float Rate);

    /**
     * Set breathing depth
     * @param Depth - Breathing depth multiplier (1.0 = normal)
     */
    UFUNCTION(BlueprintCallable, Category = "Procedural Motion")
    void SetBreathingDepth(float Depth);

    // ========================================
    // Advanced Features
    // ========================================

    /**
     * Apply postural tension from cognitive load
     * Higher cognitive load increases postural tension
     * @param CognitiveLoad - Current cognitive load (0-1)
     */
    UFUNCTION(BlueprintCallable, Category = "Advanced")
    void ApplyPosturalTensionFromCognitiveLoad(float CognitiveLoad);

    /**
     * Modulate movement fluidity from emotional state
     * Different emotions affect movement quality
     * @param Emotion - Current emotional state
     */
    UFUNCTION(BlueprintCallable, Category = "Advanced")
    void ModulateMovementFluidityFromEmotion(const FEmotionalState& Emotion);

    /**
     * Apply micro-movements from neurochemical state
     * Subtle movements reflecting internal state
     * @param DeltaTime - Time delta
     */
    UFUNCTION(BlueprintCallable, Category = "Advanced")
    void ApplyMicroMovements(float DeltaTime);

protected:
    // Control Rig instance
    UPROPERTY()
    UControlRig* ControlRigInstance;

    UPROPERTY()
    USkeletalMeshComponent* TargetMesh;

    UPROPERTY()
    bool bControlRigInitialized;

    // IK targets
    UPROPERTY()
    TMap<FName, FIKTargetConfig> IKTargets;

    UPROPERTY()
    bool bFullBodyIKEnabled;

    // Motion warping
    UPROPERTY()
    FMotionWarpConfig CurrentMotionWarp;

    UPROPERTY()
    bool bMotionWarpActive;

    UPROPERTY()
    float MotionWarpBlendAlpha;

    // Procedural animation
    UPROPERTY()
    FProceduralAnimationParams ProceduralParams;

    UPROPERTY()
    float BreathingPhase;

    UPROPERTY()
    float IdleMotionPhase;

    UPROPERTY()
    FVector IdleMotionOffset;

    // Cognitive state cache
    UPROPERTY()
    float CurrentCognitiveLoad;

    UPROPERTY()
    float CurrentEmotionalIntensity;

    UPROPERTY()
    float CurrentPosturalTension;

    // Helper functions
    void UpdateIKTargets(float DeltaTime);
    void UpdateMotionWarp(float DeltaTime);
    void ApplyControlRigParameters();
    void CalculateBreathingMotion(float DeltaTime, FVector& OutChestOffset, FVector& OutSpineOffset);
    void CalculateIdleMotion(float DeltaTime, FVector& OutOffset);
    FVector CalculateMicroMovement(float DeltaTime);
    
    // Personality to posture mapping
    void MapSuperHotGirlToPosture(const struct FSuperHotGirlTrait& Trait);
    void MapHyperChaoticToMovement(const struct FHyperChaoticTrait& Trait);
    
    // Emotional body language mapping
    void MapEmotionToSpinePosture(const FEmotionalState& Emotion, FRotator& OutSpineRotation);
    void MapEmotionToShoulderPosition(const FEmotionalState& Emotion, FVector& OutShoulderOffset);
    void MapEmotionToHeadPosition(const FEmotionalState& Emotion, FVector& OutHeadOffset);
};

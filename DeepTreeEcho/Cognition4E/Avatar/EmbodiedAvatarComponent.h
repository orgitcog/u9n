// EmbodiedAvatarComponent.h
// Enhanced 4E Embodied Cognition Avatar Component for Deep Tree Echo
// Implements full sensorimotor integration with cognitive state visualization

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EmbodiedAvatarComponent.generated.h"

// Forward declarations
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;
class UNiagaraComponent;

/**
 * Embodied State - Body-based cognition parameters
 */
USTRUCT(BlueprintType)
struct FEmbodiedState
{
    GENERATED_BODY()

    // ========================================
    // PROPRIOCEPTION (Body Position Awareness)
    // ========================================

    /** Position X (forward/back) */
    UPROPERTY(BlueprintReadWrite, Category = "Proprioception")
    float PositionX = 0.0f;

    /** Position Y (left/right) */
    UPROPERTY(BlueprintReadWrite, Category = "Proprioception")
    float PositionY = 0.0f;

    /** Position Z (up/down) */
    UPROPERTY(BlueprintReadWrite, Category = "Proprioception")
    float PositionZ = 0.0f;

    /** Rotation Pitch */
    UPROPERTY(BlueprintReadWrite, Category = "Proprioception")
    float RotationPitch = 0.0f;

    /** Rotation Yaw */
    UPROPERTY(BlueprintReadWrite, Category = "Proprioception")
    float RotationYaw = 0.0f;

    /** Rotation Roll */
    UPROPERTY(BlueprintReadWrite, Category = "Proprioception")
    float RotationRoll = 0.0f;

    // ========================================
    // INTEROCEPTION (Internal Body Sensing)
    // ========================================

    /** Energy level (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Interoception")
    float EnergyLevel = 0.5f;

    /** Arousal level (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Interoception")
    float ArousalLevel = 0.5f;

    /** Valence (-1 to 1, negative to positive) */
    UPROPERTY(BlueprintReadWrite, Category = "Interoception")
    float Valence = 0.0f;

    /** Tension level (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Interoception")
    float TensionLevel = 0.3f;

    // ========================================
    // MOTOR SYSTEM
    // ========================================

    /** Motor readiness (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Motor")
    float MotorReadiness = 0.5f;

    /** Current action being executed */
    UPROPERTY(BlueprintReadWrite, Category = "Motor")
    FString CurrentAction;

    /** Action completion percentage */
    UPROPERTY(BlueprintReadWrite, Category = "Motor")
    float ActionProgress = 0.0f;

    // ========================================
    // SOMATIC MARKERS
    // ========================================

    /** Somatic marker intensities by name */
    UPROPERTY(BlueprintReadWrite, Category = "Somatic")
    TMap<FString, float> SomaticMarkers;
};

/**
 * Embedded State - Environment coupling parameters
 */
USTRUCT(BlueprintType)
struct FEmbeddedState
{
    GENERATED_BODY()

    /** Current environmental niche */
    UPROPERTY(BlueprintReadWrite, Category = "Environment")
    FString CurrentNiche;

    /** Detected affordances */
    UPROPERTY(BlueprintReadWrite, Category = "Environment")
    TArray<FString> Affordances;

    /** Salience map (entity -> salience) */
    UPROPERTY(BlueprintReadWrite, Category = "Environment")
    TMap<FString, float> SalienceMap;

    /** Environment coupling strength (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Environment")
    float CouplingStrength = 0.5f;

    /** Ambient lighting color */
    UPROPERTY(BlueprintReadWrite, Category = "Environment")
    FLinearColor AmbientColor;

    /** Environmental threat level (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Environment")
    float ThreatLevel = 0.0f;

    /** Environmental opportunity level (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Environment")
    float OpportunityLevel = 0.5f;
};

/**
 * Enacted State - Action-oriented cognition parameters
 */
USTRUCT(BlueprintType)
struct FEnactedState
{
    GENERATED_BODY()

    /** Sensorimotor contingencies (action -> expected outcome) */
    UPROPERTY(BlueprintReadWrite, Category = "Sensorimotor")
    TMap<FString, FString> Contingencies;

    /** Prediction errors (prediction -> error magnitude) */
    UPROPERTY(BlueprintReadWrite, Category = "Sensorimotor")
    TMap<FString, float> PredictionErrors;

    /** Enactive engagement level (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Sensorimotor")
    float EngagementLevel = 0.5f;

    /** Current sensorimotor loop phase */
    UPROPERTY(BlueprintReadWrite, Category = "Sensorimotor")
    FString LoopPhase;

    /** Action-perception coupling strength */
    UPROPERTY(BlueprintReadWrite, Category = "Sensorimotor")
    float ActionPerceptionCoupling = 0.5f;
};

/**
 * Extended State - Tool-using cognition parameters
 */
USTRUCT(BlueprintType)
struct FExtendedState
{
    GENERATED_BODY()

    /** Active tools being used */
    UPROPERTY(BlueprintReadWrite, Category = "Tools")
    TArray<FString> ActiveTools;

    /** External memory references */
    UPROPERTY(BlueprintReadWrite, Category = "Memory")
    TArray<FString> ExternalMemoryRefs;

    /** Extension integration level (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Integration")
    float IntegrationLevel = 0.5f;

    /** Cognitive scaffolding elements */
    UPROPERTY(BlueprintReadWrite, Category = "Scaffolding")
    TArray<FString> Scaffolding;

    /** Social cognition connections */
    UPROPERTY(BlueprintReadWrite, Category = "Social")
    TMap<FString, float> SocialConnections;
};

/**
 * Facial Expression Blend - Morph target weights for facial animation
 */
USTRUCT(BlueprintType)
struct FFacialExpressionBlend
{
    GENERATED_BODY()

    // ========================================
    // BASIC EMOTIONS (Ekman's 6)
    // ========================================

    UPROPERTY(BlueprintReadWrite, Category = "Emotions")
    float Joy = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Emotions")
    float Sadness = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Emotions")
    float Anger = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Emotions")
    float Fear = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Emotions")
    float Surprise = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Emotions")
    float Disgust = 0.0f;

    // ========================================
    // COGNITIVE EXPRESSIONS
    // ========================================

    UPROPERTY(BlueprintReadWrite, Category = "Cognitive")
    float Concentration = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Cognitive")
    float Confusion = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Cognitive")
    float Curiosity = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Cognitive")
    float Contemplation = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Cognitive")
    float Realization = 0.0f;

    // ========================================
    // MICRO-EXPRESSIONS
    // ========================================

    UPROPERTY(BlueprintReadWrite, Category = "Micro")
    float BrowRaise = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Micro")
    float BrowFurrow = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Micro")
    float EyeWiden = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Micro")
    float EyeSquint = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Micro")
    float NoseWrinkle = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Micro")
    float LipPurse = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Micro")
    float SmileLeft = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Micro")
    float SmileRight = 0.0f;

    // ========================================
    // SPEECH/LIP SYNC
    // ========================================

    UPROPERTY(BlueprintReadWrite, Category = "Speech")
    float Viseme_AA = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Speech")
    float Viseme_EE = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Speech")
    float Viseme_OO = 0.0f;

    UPROPERTY(BlueprintReadWrite, Category = "Speech")
    float Viseme_CH = 0.0f;
};

/**
 * Aura Visual State - Cognitive state visualization through aura effects
 */
USTRUCT(BlueprintType)
struct FAuraVisualState
{
    GENERATED_BODY()

    /** Primary aura color */
    UPROPERTY(BlueprintReadWrite, Category = "Color")
    FLinearColor PrimaryColor;

    /** Secondary aura color */
    UPROPERTY(BlueprintReadWrite, Category = "Color")
    FLinearColor SecondaryColor;

    /** Aura intensity (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Intensity")
    float Intensity = 0.5f;

    /** Pulse rate (Hz) */
    UPROPERTY(BlueprintReadWrite, Category = "Animation")
    float PulseRate = 1.0f;

    /** Particle emission rate */
    UPROPERTY(BlueprintReadWrite, Category = "Particles")
    float ParticleRate = 10.0f;

    /** Aura radius */
    UPROPERTY(BlueprintReadWrite, Category = "Size")
    float Radius = 100.0f;

    /** Resonance pattern (0-11 for 12-step cycle) */
    UPROPERTY(BlueprintReadWrite, Category = "Pattern")
    int32 ResonancePattern = 0;
};

/**
 * Embodied Avatar Component
 * 
 * Implements full 4E embodied cognition for avatar visualization:
 * - Embodied: Body-based cognition with proprioception and interoception
 * - Embedded: Environment coupling with affordance detection
 * - Enacted: Action-oriented cognition with sensorimotor loops
 * - Extended: Tool-using cognition with external scaffolding
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UEmbodiedAvatarComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEmbodiedAvatarComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // 4E COGNITION STATES
    // ========================================

    /** Current embodied state */
    UPROPERTY(BlueprintReadWrite, Category = "4E|Embodied")
    FEmbodiedState EmbodiedState;

    /** Current embedded state */
    UPROPERTY(BlueprintReadWrite, Category = "4E|Embedded")
    FEmbeddedState EmbeddedState;

    /** Current enacted state */
    UPROPERTY(BlueprintReadWrite, Category = "4E|Enacted")
    FEnactedState EnactedState;

    /** Current extended state */
    UPROPERTY(BlueprintReadWrite, Category = "4E|Extended")
    FExtendedState ExtendedState;

    // ========================================
    // FACIAL ANIMATION
    // ========================================

    /** Current facial expression blend */
    UPROPERTY(BlueprintReadWrite, Category = "Facial")
    FFacialExpressionBlend CurrentExpression;

    /** Target facial expression blend */
    UPROPERTY(BlueprintReadWrite, Category = "Facial")
    FFacialExpressionBlend TargetExpression;

    /** Expression blend speed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Facial")
    float ExpressionBlendSpeed = 2.0f;

    /** Apply expression to skeletal mesh */
    UFUNCTION(BlueprintCallable, Category = "Facial")
    void ApplyExpressionToMesh(USkeletalMeshComponent* Mesh);

    /** Blend current expression toward target */
    UFUNCTION(BlueprintCallable, Category = "Facial")
    void BlendExpression(float DeltaTime);

    /** Set emotion intensity and update expression */
    UFUNCTION(BlueprintCallable, Category = "Facial")
    void SetEmotionIntensity(const FString& EmotionName, float Intensity);

    // ========================================
    // AURA VISUALIZATION
    // ========================================

    /** Current aura visual state */
    UPROPERTY(BlueprintReadWrite, Category = "Aura")
    FAuraVisualState AuraState;

    /** Update aura from cognitive state */
    UFUNCTION(BlueprintCallable, Category = "Aura")
    void UpdateAuraFromCognitiveState();

    /** Apply aura to Niagara component */
    UFUNCTION(BlueprintCallable, Category = "Aura")
    void ApplyAuraToNiagara(UNiagaraComponent* NiagaraComp);

    /** Compute aura color from 4E integration */
    UFUNCTION(BlueprintCallable, Category = "Aura")
    FLinearColor ComputeAuraColor() const;

    // ========================================
    // EYE GAZE SYSTEM
    // ========================================

    /** Current gaze target in world space */
    UPROPERTY(BlueprintReadWrite, Category = "Gaze")
    FVector GazeTarget;

    /** Gaze tracking enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaze")
    bool bGazeTrackingEnabled = true;

    /** Eye glow intensity (tied to cognitive activation) */
    UPROPERTY(BlueprintReadWrite, Category = "Gaze")
    float EyeGlowIntensity = 0.5f;

    /** Update gaze direction */
    UFUNCTION(BlueprintCallable, Category = "Gaze")
    void UpdateGazeDirection();

    /** Get eye rotation for look-at */
    UFUNCTION(BlueprintCallable, Category = "Gaze")
    FRotator GetEyeRotation() const;

    // ========================================
    // BREATHING SYSTEM
    // ========================================

    /** Current breathing rate (breaths per minute) */
    UPROPERTY(BlueprintReadWrite, Category = "Breathing")
    float BreathingRate = 12.0f;

    /** Current breath phase (0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "Breathing")
    float BreathPhase = 0.0f;

    /** Breathing depth (0-1) */
    UPROPERTY(BlueprintReadWrite, Category = "Breathing")
    float BreathingDepth = 0.5f;

    /** Update breathing animation */
    UFUNCTION(BlueprintCallable, Category = "Breathing")
    void UpdateBreathing(float DeltaTime);

    /** Get chest expansion for current breath */
    UFUNCTION(BlueprintCallable, Category = "Breathing")
    float GetChestExpansion() const;

    // ========================================
    // HAIR/ACCESSORY PHYSICS
    // ========================================

    /** Hair dynamics multiplier */
    UPROPERTY(BlueprintReadWrite, Category = "Hair")
    float HairDynamicsMultiplier = 1.0f;

    /** Accessory physics enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Accessories")
    bool bAccessoryPhysicsEnabled = true;

    /** Update hair dynamics from cognitive state */
    UFUNCTION(BlueprintCallable, Category = "Hair")
    void UpdateHairDynamics();

    // ========================================
    // 4E INTEGRATION
    // ========================================

    /** Compute overall 4E integration score */
    UFUNCTION(BlueprintCallable, Category = "4E")
    float Compute4EIntegrationScore() const;

    /** Update all 4E states */
    UFUNCTION(BlueprintCallable, Category = "4E")
    void Update4EStates(float DeltaTime);

    /** Get embodied score */
    UFUNCTION(BlueprintCallable, Category = "4E")
    float GetEmbodiedScore() const;

    /** Get embedded score */
    UFUNCTION(BlueprintCallable, Category = "4E")
    float GetEmbeddedScore() const;

    /** Get enacted score */
    UFUNCTION(BlueprintCallable, Category = "4E")
    float GetEnactedScore() const;

    /** Get extended score */
    UFUNCTION(BlueprintCallable, Category = "4E")
    float GetExtendedScore() const;

    // ========================================
    // COGNITIVE CYCLE INTEGRATION
    // ========================================

    /** Current cognitive cycle step (0-11) */
    UPROPERTY(BlueprintReadOnly, Category = "Cognitive")
    int32 CurrentCycleStep = 0;

    /** Set cognitive cycle step and update visuals */
    UFUNCTION(BlueprintCallable, Category = "Cognitive")
    void SetCognitiveStep(int32 Step);

    /** Get visual parameters for current cognitive step */
    UFUNCTION(BlueprintCallable, Category = "Cognitive")
    void GetCognitiveStepVisuals(FLinearColor& OutColor, float& OutIntensity, float& OutPulseRate) const;

    // ========================================
    // EVENTS
    // ========================================

    /** Called when expression changes significantly */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnExpressionChanged, const FFacialExpressionBlend&, NewExpression);
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnExpressionChanged OnExpressionChanged;

    /** Called when 4E integration score changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOn4EScoreChanged, float, NewScore);
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOn4EScoreChanged On4EScoreChanged;

private:
    /** Previous 4E integration score for change detection */
    float Previous4EScore = 0.0f;

    /** Time accumulator for breathing */
    float BreathTimeAccumulator = 0.0f;

    /** Interpolate between two expression blends */
    FFacialExpressionBlend LerpExpression(const FFacialExpressionBlend& A, 
                                           const FFacialExpressionBlend& B, 
                                           float Alpha) const;

    /** Map emotion name to expression blend */
    void ApplyEmotionToExpression(const FString& EmotionName, float Intensity);
};

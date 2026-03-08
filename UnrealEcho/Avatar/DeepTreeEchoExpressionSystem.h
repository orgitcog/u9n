#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepTreeEchoExpressionSystem.generated.h"

/**
 * Deep-Tree-Echo Expression System
 * 
 * Priority Implementation for Unreal Engine Avatar
 * Based on video reference analysis - focusing on photorealistic cyberpunk/biopunk avatar
 * 
 * Top 5 Priority Expressions:
 * 1. Neutral + Breathing (Foundation)
 * 2. Gentle Smile (JOY_03) - Core positive valence
 * 3. Open Vowel (SPEAK_01) - Essential communication
 * 4. Contemplative (WONDER_02/03) - Represents "thinking"
 * 5. Broad Smile/Laugh (JOY_01/02) - Peak positive emotion
 * 
 * Top 3 Priority Microexpressions:
 * 1. Procedural Blinking
 * 2. Saccadic Eye Movement
 * 3. Wonder-to-Joy Transition
 */

// Forward declarations
class USkeletalMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Cognitive state enum - maps to Deep-Tree-Echo cognitive architecture
 */
UENUM(BlueprintType)
enum class ECognitiveState : uint8
{
    Idle            UMETA(DisplayName = "Idle"),
    Processing      UMETA(DisplayName = "Processing"),
    Reasoning       UMETA(DisplayName = "Reasoning"),
    Insight         UMETA(DisplayName = "Insight"),
    Communicating   UMETA(DisplayName = "Communicating"),
    Listening       UMETA(DisplayName = "Listening"),
    Reflecting      UMETA(DisplayName = "Reflecting"),
    Engaging        UMETA(DisplayName = "Engaging")
};

/**
 * Expression state enum - priority expressions from video analysis
 * 
 * Positive valence (JOY, WONDER, SPEAK, FOCUS) established in v3.
 * Negative valence (FEAR, ANGER, SADNESS, DISGUST) added in v4 per FACS-complete catalog.
 */
UENUM(BlueprintType)
enum class EExpressionState : uint8
{
    // Foundation (Priority 1)
    Neutral             UMETA(DisplayName = "Neutral"),
    
    // Joy Spectrum (Priority 2 & 5)
    GentleSmile         UMETA(DisplayName = "Gentle Smile (JOY_03)"),
    BroadSmile          UMETA(DisplayName = "Broad Smile (JOY_01)"),
    Laughing            UMETA(DisplayName = "Laughing (JOY_02)"),
    Blissful            UMETA(DisplayName = "Blissful (JOY_05)"),
    PlayfulGrin         UMETA(DisplayName = "Playful Grin (JOY_06)"),
    
    // Wonder Spectrum (Priority 4)
    Contemplative       UMETA(DisplayName = "Contemplative (WONDER_03)"),
    CuriousGaze         UMETA(DisplayName = "Curious Gaze (WONDER_02)"),
    Awe                 UMETA(DisplayName = "Awe (WONDER_01)"),
    
    // Speaking (Priority 3)
    SpeakingVowel       UMETA(DisplayName = "Speaking Vowel (SPEAK_01)"),
    SpeakingConsonant   UMETA(DisplayName = "Speaking Consonant (SPEAK_02)"),
    
    // Attention
    AlertGaze           UMETA(DisplayName = "Alert Gaze (FOCUS_01)"),
    UpwardGaze          UMETA(DisplayName = "Upward Gaze (FOCUS_02)"),

    // --- Negative Valence (FACS-Complete v4) ---

    // Fear Spectrum (FEAR_01-04) — AU1+AU2+AU4+AU5+AU7+AU20+AU26
    FearStartle         UMETA(DisplayName = "Startle (FEAR_01)"),
    FearAnxious         UMETA(DisplayName = "Anxious Dread (FEAR_02)"),
    FearTerror          UMETA(DisplayName = "Frozen Terror (FEAR_03)"),
    FearWorried         UMETA(DisplayName = "Worried Concern (FEAR_04)"),

    // Anger Spectrum (ANGER_01-04) — AU4+AU5+AU7+AU23+AU24
    AngerIrritation     UMETA(DisplayName = "Irritation (ANGER_01)"),
    AngerFury           UMETA(DisplayName = "Controlled Fury (ANGER_02)"),
    AngerRage           UMETA(DisplayName = "Rage (ANGER_03)"),
    AngerResolve        UMETA(DisplayName = "Determined Resolve (ANGER_04)"),

    // Sadness Spectrum (SAD_01-04) — AU1+AU4+AU15+AU17
    SadMelancholy       UMETA(DisplayName = "Melancholy (SAD_01)"),
    SadGrief            UMETA(DisplayName = "Grief (SAD_02)"),
    SadCrying           UMETA(DisplayName = "Crying (SAD_03)"),
    SadDisappointed     UMETA(DisplayName = "Disappointed (SAD_04)"),

    // Disgust Spectrum (DISG_01-03) — AU9+AU10+AU15+AU17
    DisgustDistaste     UMETA(DisplayName = "Mild Distaste (DISG_01)"),
    DisgustRevulsion    UMETA(DisplayName = "Revulsion (DISG_02)"),
    DisgustContempt     UMETA(DisplayName = "Contemptuous Disgust (DISG_03)")
};

/**
 * Microexpression pattern enum
 */
UENUM(BlueprintType)
enum class EMicroExpression : uint8
{
    None            UMETA(DisplayName = "None"),
    Blink           UMETA(DisplayName = "Blink"),
    UnilateralBrowRaise UMETA(DisplayName = "Unilateral Brow Raise"),
    LipCornerTwitch UMETA(DisplayName = "Lip Corner Twitch"),
    NoseWrinkle     UMETA(DisplayName = "Nose Wrinkle"),
    EyelidFlutter   UMETA(DisplayName = "Eyelid Flutter"),
    LipPurse        UMETA(DisplayName = "Lip Purse"),
    JawSlack        UMETA(DisplayName = "Jaw Slack")
};

/**
 * Complete morph target data structure
 * Based on expression catalog analysis
 */
USTRUCT(BlueprintType)
struct FDeepTreeEchoMorphTargets
{
    GENERATED_BODY()

    // Brow controls
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brow")
    float BrowRaiseL = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brow")
    float BrowRaiseR = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Brow")
    float BrowFurrow = 0.0f;

    // Eye controls
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye")
    float EyeWideL = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye")
    float EyeWideR = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye")
    float EyeSquintL = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye")
    float EyeSquintR = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye")
    float EyeCloseL = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Eye")
    float EyeCloseR = 0.0f;

    // Nose controls
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nose")
    float NoseScrunch = 0.0f;

    // Cheek controls
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cheek")
    float CheekRaiseL = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cheek")
    float CheekRaiseR = 0.0f;

    // Mouth controls
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float MouthSmileL = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float MouthSmileR = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float MouthOpen = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float MouthWide = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float LipPucker = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float LipPart = 0.0f;

    // Visemes for speech
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viseme")
    float VisemeAA = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viseme")
    float VisemeEE = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viseme")
    float VisemeOH = 0.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Viseme")
    float VisemeOO = 0.0f;

    // --- Negative Emotion Morph Targets (FACS-Complete v4) ---
    // Required for FEAR, ANGER, SADNESS, DISGUST spectra.

    // AU15 — Lip corner depressor (sadness, disgust)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float MouthFrownL = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float MouthFrownR = 0.0f;

    // AU17 — Chin raiser (sadness, anger)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chin")
    float ChinRaiseL = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chin")
    float ChinRaiseR = 0.0f;

    // AU24 — Lip pressor (anger)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float LipPressL = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float LipPressR = 0.0f;

    // AU23 — Lip tightener (anger)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float LipTightenL = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float LipTightenR = 0.0f;

    // AU10 — Upper lip raiser (disgust)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float LipRaiseL = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float LipRaiseR = 0.0f;

    // AU20 — Lip stretcher (fear)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float MouthStretchL = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mouth")
    float MouthStretchR = 0.0f;

    // AU11 — Nasolabial deepener (sadness)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nose")
    float NasolabialDeepenL = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nose")
    float NasolabialDeepenR = 0.0f;

    // Interpolation helper
    static FDeepTreeEchoMorphTargets Lerp(const FDeepTreeEchoMorphTargets& A, const FDeepTreeEchoMorphTargets& B, float Alpha);
    
    // Additive blend helper
    static FDeepTreeEchoMorphTargets Add(const FDeepTreeEchoMorphTargets& Base, const FDeepTreeEchoMorphTargets& Additive, float Weight = 1.0f);
};

/**
 * Expression preset data
 */
USTRUCT(BlueprintType)
struct FExpressionPreset
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Expression")
    EExpressionState ExpressionState;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Expression")
    FDeepTreeEchoMorphTargets MorphTargets;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Expression")
    float EmissiveIntensity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Expression")
    FVector2D HeadTilt = FVector2D::ZeroVector;
};

/**
 * Procedural animation settings
 */
USTRUCT(BlueprintType)
struct FProceduralAnimSettings
{
    GENERATED_BODY()

    // Breathing
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breathing")
    float BreathingRate = 0.2f;  // Breaths per second
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Breathing")
    float BreathingAmplitude = 0.02f;

    // Blinking
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blinking")
    float MinBlinkInterval = 2.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blinking")
    float MaxBlinkInterval = 6.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blinking")
    float BlinkDuration = 0.15f;

    // Saccades (eye movement)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saccades")
    float SaccadeFrequency = 0.5f;  // Per second
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saccades")
    float SaccadeAmplitude = 5.0f;  // Degrees
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Saccades")
    float SaccadeSpeed = 500.0f;  // Degrees per second

    // Head micro-movement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeadMovement")
    float HeadSwayAmplitude = 1.0f;  // Degrees
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HeadMovement")
    float HeadSwayFrequency = 0.1f;
};

/**
 * Echobeats cognitive loop step data
 * Maps the 12-step cognitive cycle to expression hints
 */
USTRUCT(BlueprintType)
struct FEchobeatsStep
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats")
    int32 StepNumber = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats")
    FString CognitivePhase;  // Perception, Action, Simulation, Pivotal

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Echobeats")
    EExpressionState ExpressionHint;
};

/**
 * Main Deep-Tree-Echo Expression System Component
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREECHO_API UDeepTreeEchoExpressionSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UDeepTreeEchoExpressionSystem();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========== Core Expression Control ==========

    /** Set expression state with transition */
    UFUNCTION(BlueprintCallable, Category = "Expression")
    void SetExpressionState(EExpressionState NewState, float TransitionTime = 0.3f);

    /** Get current expression state */
    UFUNCTION(BlueprintPure, Category = "Expression")
    EExpressionState GetCurrentExpressionState() const { return CurrentExpressionState; }

    /** Set cognitive state (maps to expression automatically) */
    UFUNCTION(BlueprintCallable, Category = "Cognitive")
    void SetCognitiveState(ECognitiveState NewState);

    /** Get current cognitive state */
    UFUNCTION(BlueprintPure, Category = "Cognitive")
    ECognitiveState GetCurrentCognitiveState() const { return CurrentCognitiveState; }

    // ========== Microexpression Control ==========

    /** Trigger a microexpression */
    UFUNCTION(BlueprintCallable, Category = "Microexpression")
    void TriggerMicroExpression(EMicroExpression MicroExp, float Duration = 0.2f);

    /** Trigger the Wonder-to-Joy transition pattern */
    UFUNCTION(BlueprintCallable, Category = "Microexpression")
    void TriggerWonderToJoyTransition(float Duration = 1.5f);

    // ========== Procedural Animation Control ==========

    /** Enable/disable procedural blinking */
    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void SetBlinkingEnabled(bool bEnabled);

    /** Enable/disable procedural breathing */
    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void SetBreathingEnabled(bool bEnabled);

    /** Enable/disable saccadic eye movement */
    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void SetSaccadesEnabled(bool bEnabled);

    /** Trigger manual blink */
    UFUNCTION(BlueprintCallable, Category = "Procedural")
    void TriggerBlink();

    // ========== Gaze Control ==========

    /** Set gaze target in world space */
    UFUNCTION(BlueprintCallable, Category = "Gaze")
    void SetGazeTarget(const FVector& WorldTarget);

    /** Clear gaze target (return to forward) */
    UFUNCTION(BlueprintCallable, Category = "Gaze")
    void ClearGazeTarget();

    // ========== Speech/Lip Sync ==========

    /** Set speaking state with audio amplitude */
    UFUNCTION(BlueprintCallable, Category = "Speech")
    void SetSpeakingAmplitude(float Amplitude);

    /** Set specific viseme */
    UFUNCTION(BlueprintCallable, Category = "Speech")
    void SetViseme(const FName& VisemeName, float Weight);

    // ========== Emissive/Tech Element Control ==========

    /** Set bioluminescent marking intensity */
    UFUNCTION(BlueprintCallable, Category = "Emissive")
    void SetEmissiveIntensity(float Intensity);

    /** Pulse emissives (for insight/engagement moments) */
    UFUNCTION(BlueprintCallable, Category = "Emissive")
    void PulseEmissives(float Duration = 0.5f, float PeakIntensity = 2.0f);

    // ========== Echobeats Integration ==========

    /** Update expression based on echobeats step */
    UFUNCTION(BlueprintCallable, Category = "Echobeats")
    void OnEchobeatsStep(int32 StepNumber);

    // ========== Output ==========

    /** Get current morph targets for application to skeletal mesh */
    UFUNCTION(BlueprintPure, Category = "Output")
    FDeepTreeEchoMorphTargets GetCurrentMorphTargets() const { return CurrentMorphTargets; }

    /** Apply morph targets to skeletal mesh component */
    UFUNCTION(BlueprintCallable, Category = "Output")
    void ApplyToSkeletalMesh(USkeletalMeshComponent* TargetMesh);

    /** Apply emissive settings to material */
    UFUNCTION(BlueprintCallable, Category = "Output")
    void ApplyToMaterial(UMaterialInstanceDynamic* TargetMaterial);

protected:
    // ========== Internal Update Functions ==========

    void InitializeExpressionPresets();
    void InitializeNegativeEmotionPresets();
    void InitializeEchobeatsMapping();
    void InitializeCognitiveMapping();

    void UpdateExpressionTransition(float DeltaTime);
    void UpdateProceduralBlinking(float DeltaTime);
    void UpdateProceduralBreathing(float DeltaTime);
    void UpdateProceduralSaccades(float DeltaTime);
    void UpdateMicroExpression(float DeltaTime);
    void UpdateEmissivePulse(float DeltaTime);
    void UpdateWonderToJoyTransition(float DeltaTime);

    EExpressionState MapCognitiveToExpression(ECognitiveState CogState);

    // ========== State Variables ==========

    UPROPERTY()
    EExpressionState CurrentExpressionState;

    UPROPERTY()
    EExpressionState TargetExpressionState;

    UPROPERTY()
    ECognitiveState CurrentCognitiveState;

    UPROPERTY()
    FDeepTreeEchoMorphTargets CurrentMorphTargets;

    UPROPERTY()
    FDeepTreeEchoMorphTargets TargetMorphTargets;

    UPROPERTY()
    FDeepTreeEchoMorphTargets ProceduralMorphTargets;

    // Expression presets
    UPROPERTY()
    TMap<EExpressionState, FExpressionPreset> ExpressionPresets;

    // Cognitive to expression mapping
    UPROPERTY()
    TMap<ECognitiveState, EExpressionState> CognitiveMapping;

    // Echobeats step mapping
    UPROPERTY()
    TArray<FEchobeatsStep> EchobeatsSteps;

    // ========== Transition State ==========

    float ExpressionTransitionProgress;
    float ExpressionTransitionDuration;

    // ========== Procedural Animation State ==========

    UPROPERTY(EditAnywhere, Category = "Procedural Settings")
    FProceduralAnimSettings ProceduralSettings;

    bool bBlinkingEnabled;
    bool bBreathingEnabled;
    bool bSaccadesEnabled;

    float BlinkTimer;
    float NextBlinkTime;
    bool bIsBlinking;
    float BlinkProgress;

    float BreathingPhase;

    FVector2D CurrentSaccadeOffset;
    FVector2D TargetSaccadeOffset;
    float SaccadeTimer;

    // ========== Gaze State ==========

    bool bHasGazeTarget;
    FVector GazeTarget;
    FRotator CurrentEyeRotation;

    // ========== Microexpression State ==========

    bool bMicroExpressionActive;
    EMicroExpression ActiveMicroExpression;
    float MicroExpressionTimer;
    float MicroExpressionDuration;

    // ========== Wonder-to-Joy Transition State ==========

    bool bWonderToJoyActive;
    float WonderToJoyProgress;
    float WonderToJoyDuration;

    // ========== Emissive State ==========

    float CurrentEmissiveIntensity;
    float TargetEmissiveIntensity;
    bool bEmissivePulseActive;
    float EmissivePulseTimer;
    float EmissivePulseDuration;
    float EmissivePulsePeak;

    // ========== Speech State ==========

    float CurrentSpeakingAmplitude;
};

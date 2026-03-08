#include "DeepTreeEchoExpressionSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

// Static helper implementations
FDeepTreeEchoMorphTargets FDeepTreeEchoMorphTargets::Lerp(const FDeepTreeEchoMorphTargets& A, const FDeepTreeEchoMorphTargets& B, float Alpha)
{
    FDeepTreeEchoMorphTargets Result;
    
    // Brow
    Result.BrowRaiseL = FMath::Lerp(A.BrowRaiseL, B.BrowRaiseL, Alpha);
    Result.BrowRaiseR = FMath::Lerp(A.BrowRaiseR, B.BrowRaiseR, Alpha);
    Result.BrowFurrow = FMath::Lerp(A.BrowFurrow, B.BrowFurrow, Alpha);
    
    // Eye
    Result.EyeWideL = FMath::Lerp(A.EyeWideL, B.EyeWideL, Alpha);
    Result.EyeWideR = FMath::Lerp(A.EyeWideR, B.EyeWideR, Alpha);
    Result.EyeSquintL = FMath::Lerp(A.EyeSquintL, B.EyeSquintL, Alpha);
    Result.EyeSquintR = FMath::Lerp(A.EyeSquintR, B.EyeSquintR, Alpha);
    Result.EyeCloseL = FMath::Lerp(A.EyeCloseL, B.EyeCloseL, Alpha);
    Result.EyeCloseR = FMath::Lerp(A.EyeCloseR, B.EyeCloseR, Alpha);
    
    // Nose
    Result.NoseScrunch = FMath::Lerp(A.NoseScrunch, B.NoseScrunch, Alpha);
    
    // Cheek
    Result.CheekRaiseL = FMath::Lerp(A.CheekRaiseL, B.CheekRaiseL, Alpha);
    Result.CheekRaiseR = FMath::Lerp(A.CheekRaiseR, B.CheekRaiseR, Alpha);
    
    // Mouth
    Result.MouthSmileL = FMath::Lerp(A.MouthSmileL, B.MouthSmileL, Alpha);
    Result.MouthSmileR = FMath::Lerp(A.MouthSmileR, B.MouthSmileR, Alpha);
    Result.MouthOpen = FMath::Lerp(A.MouthOpen, B.MouthOpen, Alpha);
    Result.MouthWide = FMath::Lerp(A.MouthWide, B.MouthWide, Alpha);
    Result.LipPucker = FMath::Lerp(A.LipPucker, B.LipPucker, Alpha);
    Result.LipPart = FMath::Lerp(A.LipPart, B.LipPart, Alpha);
    
    // Visemes
    Result.VisemeAA = FMath::Lerp(A.VisemeAA, B.VisemeAA, Alpha);
    Result.VisemeEE = FMath::Lerp(A.VisemeEE, B.VisemeEE, Alpha);
    Result.VisemeOH = FMath::Lerp(A.VisemeOH, B.VisemeOH, Alpha);
    Result.VisemeOO = FMath::Lerp(A.VisemeOO, B.VisemeOO, Alpha);

    // Negative emotion morph targets
    Result.MouthFrownL = FMath::Lerp(A.MouthFrownL, B.MouthFrownL, Alpha);
    Result.MouthFrownR = FMath::Lerp(A.MouthFrownR, B.MouthFrownR, Alpha);
    Result.ChinRaiseL = FMath::Lerp(A.ChinRaiseL, B.ChinRaiseL, Alpha);
    Result.ChinRaiseR = FMath::Lerp(A.ChinRaiseR, B.ChinRaiseR, Alpha);
    Result.LipPressL = FMath::Lerp(A.LipPressL, B.LipPressL, Alpha);
    Result.LipPressR = FMath::Lerp(A.LipPressR, B.LipPressR, Alpha);
    Result.LipTightenL = FMath::Lerp(A.LipTightenL, B.LipTightenL, Alpha);
    Result.LipTightenR = FMath::Lerp(A.LipTightenR, B.LipTightenR, Alpha);
    Result.LipRaiseL = FMath::Lerp(A.LipRaiseL, B.LipRaiseL, Alpha);
    Result.LipRaiseR = FMath::Lerp(A.LipRaiseR, B.LipRaiseR, Alpha);
    Result.MouthStretchL = FMath::Lerp(A.MouthStretchL, B.MouthStretchL, Alpha);
    Result.MouthStretchR = FMath::Lerp(A.MouthStretchR, B.MouthStretchR, Alpha);
    Result.NasolabialDeepenL = FMath::Lerp(A.NasolabialDeepenL, B.NasolabialDeepenL, Alpha);
    Result.NasolabialDeepenR = FMath::Lerp(A.NasolabialDeepenR, B.NasolabialDeepenR, Alpha);
    
    return Result;
}

FDeepTreeEchoMorphTargets FDeepTreeEchoMorphTargets::Add(const FDeepTreeEchoMorphTargets& Base, const FDeepTreeEchoMorphTargets& Additive, float Weight)
{
    FDeepTreeEchoMorphTargets Result = Base;
    
    Result.BrowRaiseL = FMath::Clamp(Base.BrowRaiseL + Additive.BrowRaiseL * Weight, 0.0f, 1.0f);
    Result.BrowRaiseR = FMath::Clamp(Base.BrowRaiseR + Additive.BrowRaiseR * Weight, 0.0f, 1.0f);
    Result.BrowFurrow = FMath::Clamp(Base.BrowFurrow + Additive.BrowFurrow * Weight, 0.0f, 1.0f);
    Result.EyeWideL = FMath::Clamp(Base.EyeWideL + Additive.EyeWideL * Weight, 0.0f, 1.0f);
    Result.EyeWideR = FMath::Clamp(Base.EyeWideR + Additive.EyeWideR * Weight, 0.0f, 1.0f);
    Result.EyeSquintL = FMath::Clamp(Base.EyeSquintL + Additive.EyeSquintL * Weight, 0.0f, 1.0f);
    Result.EyeSquintR = FMath::Clamp(Base.EyeSquintR + Additive.EyeSquintR * Weight, 0.0f, 1.0f);
    Result.EyeCloseL = FMath::Clamp(Base.EyeCloseL + Additive.EyeCloseL * Weight, 0.0f, 1.0f);
    Result.EyeCloseR = FMath::Clamp(Base.EyeCloseR + Additive.EyeCloseR * Weight, 0.0f, 1.0f);
    Result.NoseScrunch = FMath::Clamp(Base.NoseScrunch + Additive.NoseScrunch * Weight, 0.0f, 1.0f);
    Result.CheekRaiseL = FMath::Clamp(Base.CheekRaiseL + Additive.CheekRaiseL * Weight, 0.0f, 1.0f);
    Result.CheekRaiseR = FMath::Clamp(Base.CheekRaiseR + Additive.CheekRaiseR * Weight, 0.0f, 1.0f);
    Result.MouthSmileL = FMath::Clamp(Base.MouthSmileL + Additive.MouthSmileL * Weight, -1.0f, 1.0f);
    Result.MouthSmileR = FMath::Clamp(Base.MouthSmileR + Additive.MouthSmileR * Weight, -1.0f, 1.0f);
    Result.MouthOpen = FMath::Clamp(Base.MouthOpen + Additive.MouthOpen * Weight, 0.0f, 1.0f);
    Result.MouthWide = FMath::Clamp(Base.MouthWide + Additive.MouthWide * Weight, 0.0f, 1.0f);
    Result.LipPucker = FMath::Clamp(Base.LipPucker + Additive.LipPucker * Weight, 0.0f, 1.0f);
    Result.LipPart = FMath::Clamp(Base.LipPart + Additive.LipPart * Weight, 0.0f, 1.0f);
    Result.VisemeAA = FMath::Clamp(Base.VisemeAA + Additive.VisemeAA * Weight, 0.0f, 1.0f);
    Result.VisemeEE = FMath::Clamp(Base.VisemeEE + Additive.VisemeEE * Weight, 0.0f, 1.0f);
    Result.VisemeOH = FMath::Clamp(Base.VisemeOH + Additive.VisemeOH * Weight, 0.0f, 1.0f);
    Result.VisemeOO = FMath::Clamp(Base.VisemeOO + Additive.VisemeOO * Weight, 0.0f, 1.0f);

    // Negative emotion morph targets
    Result.MouthFrownL = FMath::Clamp(Base.MouthFrownL + Additive.MouthFrownL * Weight, 0.0f, 1.0f);
    Result.MouthFrownR = FMath::Clamp(Base.MouthFrownR + Additive.MouthFrownR * Weight, 0.0f, 1.0f);
    Result.ChinRaiseL = FMath::Clamp(Base.ChinRaiseL + Additive.ChinRaiseL * Weight, 0.0f, 1.0f);
    Result.ChinRaiseR = FMath::Clamp(Base.ChinRaiseR + Additive.ChinRaiseR * Weight, 0.0f, 1.0f);
    Result.LipPressL = FMath::Clamp(Base.LipPressL + Additive.LipPressL * Weight, 0.0f, 1.0f);
    Result.LipPressR = FMath::Clamp(Base.LipPressR + Additive.LipPressR * Weight, 0.0f, 1.0f);
    Result.LipTightenL = FMath::Clamp(Base.LipTightenL + Additive.LipTightenL * Weight, 0.0f, 1.0f);
    Result.LipTightenR = FMath::Clamp(Base.LipTightenR + Additive.LipTightenR * Weight, 0.0f, 1.0f);
    Result.LipRaiseL = FMath::Clamp(Base.LipRaiseL + Additive.LipRaiseL * Weight, 0.0f, 1.0f);
    Result.LipRaiseR = FMath::Clamp(Base.LipRaiseR + Additive.LipRaiseR * Weight, 0.0f, 1.0f);
    Result.MouthStretchL = FMath::Clamp(Base.MouthStretchL + Additive.MouthStretchL * Weight, 0.0f, 1.0f);
    Result.MouthStretchR = FMath::Clamp(Base.MouthStretchR + Additive.MouthStretchR * Weight, 0.0f, 1.0f);
    Result.NasolabialDeepenL = FMath::Clamp(Base.NasolabialDeepenL + Additive.NasolabialDeepenL * Weight, 0.0f, 1.0f);
    Result.NasolabialDeepenR = FMath::Clamp(Base.NasolabialDeepenR + Additive.NasolabialDeepenR * Weight, 0.0f, 1.0f);
    
    return Result;
}

UDeepTreeEchoExpressionSystem::UDeepTreeEchoExpressionSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    // Initialize states
    CurrentExpressionState = EExpressionState::Neutral;
    TargetExpressionState = EExpressionState::Neutral;
    CurrentCognitiveState = ECognitiveState::Idle;

    ExpressionTransitionProgress = 1.0f;
    ExpressionTransitionDuration = 0.3f;

    // Procedural animation defaults
    bBlinkingEnabled = true;
    bBreathingEnabled = true;
    bSaccadesEnabled = true;

    BlinkTimer = 0.0f;
    NextBlinkTime = 3.0f;
    bIsBlinking = false;
    BlinkProgress = 0.0f;

    BreathingPhase = 0.0f;

    CurrentSaccadeOffset = FVector2D::ZeroVector;
    TargetSaccadeOffset = FVector2D::ZeroVector;
    SaccadeTimer = 0.0f;

    // Gaze
    bHasGazeTarget = false;
    CurrentEyeRotation = FRotator::ZeroRotator;

    // Microexpression
    bMicroExpressionActive = false;
    ActiveMicroExpression = EMicroExpression::None;
    MicroExpressionTimer = 0.0f;
    MicroExpressionDuration = 0.2f;

    // Wonder-to-Joy
    bWonderToJoyActive = false;
    WonderToJoyProgress = 0.0f;
    WonderToJoyDuration = 1.5f;

    // Emissive
    CurrentEmissiveIntensity = 1.0f;
    TargetEmissiveIntensity = 1.0f;
    bEmissivePulseActive = false;
    EmissivePulseTimer = 0.0f;
    EmissivePulseDuration = 0.5f;
    EmissivePulsePeak = 2.0f;

    // Speech
    CurrentSpeakingAmplitude = 0.0f;
}

void UDeepTreeEchoExpressionSystem::BeginPlay()
{
    Super::BeginPlay();

    InitializeExpressionPresets();
    InitializeNegativeEmotionPresets();
    InitializeEchobeatsMapping();
    InitializeCognitiveMapping();

    // Set initial morph targets to neutral
    if (ExpressionPresets.Contains(EExpressionState::Neutral))
    {
        CurrentMorphTargets = ExpressionPresets[EExpressionState::Neutral].MorphTargets;
        TargetMorphTargets = CurrentMorphTargets;
    }

    // Randomize first blink time
    NextBlinkTime = FMath::RandRange(ProceduralSettings.MinBlinkInterval, ProceduralSettings.MaxBlinkInterval);

    UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoExpressionSystem initialized with %d expression presets"), ExpressionPresets.Num());
}

void UDeepTreeEchoExpressionSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Update all systems
    UpdateExpressionTransition(DeltaTime);
    UpdateProceduralBlinking(DeltaTime);
    UpdateProceduralBreathing(DeltaTime);
    UpdateProceduralSaccades(DeltaTime);
    UpdateMicroExpression(DeltaTime);
    UpdateEmissivePulse(DeltaTime);
    UpdateWonderToJoyTransition(DeltaTime);

    // Combine base expression with procedural animations
    CurrentMorphTargets = FDeepTreeEchoMorphTargets::Add(CurrentMorphTargets, ProceduralMorphTargets, 1.0f);
}

void UDeepTreeEchoExpressionSystem::InitializeExpressionPresets()
{
    // ========== PRIORITY 1: Neutral (Foundation) ==========
    {
        FExpressionPreset Neutral;
        Neutral.ExpressionState = EExpressionState::Neutral;
        Neutral.EmissiveIntensity = 1.0f;
        Neutral.HeadTilt = FVector2D(0.0f, 0.0f);
        // All morph targets at 0 (default)
        ExpressionPresets.Add(EExpressionState::Neutral, Neutral);
    }

    // ========== PRIORITY 2: Gentle Smile (JOY_03) ==========
    {
        FExpressionPreset GentleSmile;
        GentleSmile.ExpressionState = EExpressionState::GentleSmile;
        GentleSmile.MorphTargets.MouthSmileL = 0.4f;
        GentleSmile.MorphTargets.MouthSmileR = 0.4f;
        GentleSmile.MorphTargets.CheekRaiseL = 0.3f;
        GentleSmile.MorphTargets.CheekRaiseR = 0.3f;
        GentleSmile.MorphTargets.EyeSquintL = 0.15f;
        GentleSmile.MorphTargets.EyeSquintR = 0.15f;
        GentleSmile.MorphTargets.LipPart = 0.1f;
        GentleSmile.EmissiveIntensity = 1.1f;
        GentleSmile.HeadTilt = FVector2D(3.0f, 0.0f);  // Slight upward tilt
        ExpressionPresets.Add(EExpressionState::GentleSmile, GentleSmile);
    }

    // ========== PRIORITY 3: Speaking Vowel (SPEAK_01) ==========
    {
        FExpressionPreset SpeakingVowel;
        SpeakingVowel.ExpressionState = EExpressionState::SpeakingVowel;
        SpeakingVowel.MorphTargets.MouthOpen = 0.5f;
        SpeakingVowel.MorphTargets.LipPart = 0.4f;
        SpeakingVowel.MorphTargets.VisemeAA = 0.6f;
        SpeakingVowel.EmissiveIntensity = 1.0f;
        ExpressionPresets.Add(EExpressionState::SpeakingVowel, SpeakingVowel);
    }

    // ========== PRIORITY 4: Contemplative (WONDER_03) ==========
    {
        FExpressionPreset Contemplative;
        Contemplative.ExpressionState = EExpressionState::Contemplative;
        Contemplative.MorphTargets.BrowRaiseL = 0.2f;
        Contemplative.MorphTargets.BrowRaiseR = 0.2f;
        Contemplative.MorphTargets.EyeWideL = 0.1f;
        Contemplative.MorphTargets.EyeWideR = 0.1f;
        Contemplative.MorphTargets.LipPart = 0.15f;
        Contemplative.EmissiveIntensity = 0.9f;  // Slightly dimmer during thought
        Contemplative.HeadTilt = FVector2D(5.0f, -5.0f);  // Upward, slight left
        ExpressionPresets.Add(EExpressionState::Contemplative, Contemplative);
    }

    // ========== PRIORITY 5: Broad Smile (JOY_01) ==========
    {
        FExpressionPreset BroadSmile;
        BroadSmile.ExpressionState = EExpressionState::BroadSmile;
        BroadSmile.MorphTargets.MouthSmileL = 0.85f;
        BroadSmile.MorphTargets.MouthSmileR = 0.85f;
        BroadSmile.MorphTargets.MouthOpen = 0.3f;
        BroadSmile.MorphTargets.MouthWide = 0.4f;
        BroadSmile.MorphTargets.CheekRaiseL = 0.7f;
        BroadSmile.MorphTargets.CheekRaiseR = 0.7f;
        BroadSmile.MorphTargets.EyeSquintL = 0.4f;
        BroadSmile.MorphTargets.EyeSquintR = 0.4f;
        BroadSmile.MorphTargets.NoseScrunch = 0.2f;
        BroadSmile.EmissiveIntensity = 1.3f;  // Brighter during joy
        BroadSmile.HeadTilt = FVector2D(8.0f, 0.0f);  // Upward tilt
        ExpressionPresets.Add(EExpressionState::BroadSmile, BroadSmile);
    }

    // ========== Additional Expressions ==========

    // Laughing (JOY_02)
    {
        FExpressionPreset Laughing;
        Laughing.ExpressionState = EExpressionState::Laughing;
        Laughing.MorphTargets.MouthSmileL = 0.9f;
        Laughing.MorphTargets.MouthSmileR = 0.9f;
        Laughing.MorphTargets.MouthOpen = 0.6f;
        Laughing.MorphTargets.MouthWide = 0.5f;
        Laughing.MorphTargets.CheekRaiseL = 0.8f;
        Laughing.MorphTargets.CheekRaiseR = 0.8f;
        Laughing.MorphTargets.EyeSquintL = 0.6f;
        Laughing.MorphTargets.EyeSquintR = 0.6f;
        Laughing.MorphTargets.NoseScrunch = 0.3f;
        Laughing.EmissiveIntensity = 1.5f;
        Laughing.HeadTilt = FVector2D(10.0f, 5.0f);
        ExpressionPresets.Add(EExpressionState::Laughing, Laughing);
    }

    // Blissful (JOY_05)
    {
        FExpressionPreset Blissful;
        Blissful.ExpressionState = EExpressionState::Blissful;
        Blissful.MorphTargets.MouthSmileL = 0.5f;
        Blissful.MorphTargets.MouthSmileR = 0.5f;
        Blissful.MorphTargets.EyeCloseL = 0.7f;
        Blissful.MorphTargets.EyeCloseR = 0.7f;
        Blissful.MorphTargets.CheekRaiseL = 0.5f;
        Blissful.MorphTargets.CheekRaiseR = 0.5f;
        Blissful.EmissiveIntensity = 1.2f;
        Blissful.HeadTilt = FVector2D(15.0f, 0.0f);
        ExpressionPresets.Add(EExpressionState::Blissful, Blissful);
    }

    // Playful Grin (JOY_06)
    {
        FExpressionPreset PlayfulGrin;
        PlayfulGrin.ExpressionState = EExpressionState::PlayfulGrin;
        PlayfulGrin.MorphTargets.MouthSmileL = 0.7f;
        PlayfulGrin.MorphTargets.MouthSmileR = 0.5f;  // Asymmetric
        PlayfulGrin.MorphTargets.BrowRaiseL = 0.4f;
        PlayfulGrin.MorphTargets.BrowRaiseR = 0.1f;  // Asymmetric
        PlayfulGrin.MorphTargets.EyeSquintL = 0.2f;
        PlayfulGrin.MorphTargets.EyeSquintR = 0.1f;
        PlayfulGrin.EmissiveIntensity = 1.2f;
        PlayfulGrin.HeadTilt = FVector2D(5.0f, 8.0f);  // Tilted to side
        ExpressionPresets.Add(EExpressionState::PlayfulGrin, PlayfulGrin);
    }

    // Curious Gaze (WONDER_02)
    {
        FExpressionPreset CuriousGaze;
        CuriousGaze.ExpressionState = EExpressionState::CuriousGaze;
        CuriousGaze.MorphTargets.BrowRaiseL = 0.5f;
        CuriousGaze.MorphTargets.BrowRaiseR = 0.5f;
        CuriousGaze.MorphTargets.EyeWideL = 0.3f;
        CuriousGaze.MorphTargets.EyeWideR = 0.3f;
        CuriousGaze.MorphTargets.LipPart = 0.2f;
        CuriousGaze.EmissiveIntensity = 1.1f;
        CuriousGaze.HeadTilt = FVector2D(0.0f, 10.0f);  // Head tilt to side
        ExpressionPresets.Add(EExpressionState::CuriousGaze, CuriousGaze);
    }

    // Awe (WONDER_01)
    {
        FExpressionPreset Awe;
        Awe.ExpressionState = EExpressionState::Awe;
        Awe.MorphTargets.BrowRaiseL = 0.7f;
        Awe.MorphTargets.BrowRaiseR = 0.7f;
        Awe.MorphTargets.EyeWideL = 0.6f;
        Awe.MorphTargets.EyeWideR = 0.6f;
        Awe.MorphTargets.MouthOpen = 0.3f;
        Awe.MorphTargets.LipPart = 0.4f;
        Awe.EmissiveIntensity = 1.4f;
        Awe.HeadTilt = FVector2D(10.0f, 0.0f);
        ExpressionPresets.Add(EExpressionState::Awe, Awe);
    }

    // Speaking Consonant (SPEAK_02)
    {
        FExpressionPreset SpeakingConsonant;
        SpeakingConsonant.ExpressionState = EExpressionState::SpeakingConsonant;
        SpeakingConsonant.MorphTargets.MouthOpen = 0.2f;
        SpeakingConsonant.MorphTargets.LipPucker = 0.3f;
        SpeakingConsonant.MorphTargets.VisemeOO = 0.4f;
        SpeakingConsonant.EmissiveIntensity = 1.0f;
        ExpressionPresets.Add(EExpressionState::SpeakingConsonant, SpeakingConsonant);
    }

    // Alert Gaze (FOCUS_01)
    {
        FExpressionPreset AlertGaze;
        AlertGaze.ExpressionState = EExpressionState::AlertGaze;
        AlertGaze.MorphTargets.EyeWideL = 0.2f;
        AlertGaze.MorphTargets.EyeWideR = 0.2f;
        AlertGaze.MorphTargets.BrowRaiseL = 0.15f;
        AlertGaze.MorphTargets.BrowRaiseR = 0.15f;
        AlertGaze.EmissiveIntensity = 1.15f;
        AlertGaze.HeadTilt = FVector2D(-5.0f, 0.0f);  // Slight forward lean
        ExpressionPresets.Add(EExpressionState::AlertGaze, AlertGaze);
    }

    // Upward Gaze (FOCUS_02)
    {
        FExpressionPreset UpwardGaze;
        UpwardGaze.ExpressionState = EExpressionState::UpwardGaze;
        UpwardGaze.MorphTargets.BrowRaiseL = 0.3f;
        UpwardGaze.MorphTargets.BrowRaiseR = 0.3f;
        UpwardGaze.MorphTargets.EyeWideL = 0.15f;
        UpwardGaze.MorphTargets.EyeWideR = 0.15f;
        UpwardGaze.EmissiveIntensity = 1.0f;
        UpwardGaze.HeadTilt = FVector2D(12.0f, 0.0f);  // Looking up
        ExpressionPresets.Add(EExpressionState::UpwardGaze, UpwardGaze);
    }

    UE_LOG(LogTemp, Log, TEXT("Expression presets initialized: %d expressions"), ExpressionPresets.Num());
}

// ===================================================================
// FACS-Complete Negative Emotion Presets (v4)
// Values derived from FACS AU analysis in expression_catalog_facs_complete.md
// ===================================================================
void UDeepTreeEchoExpressionSystem::InitializeNegativeEmotionPresets()
{
    // ========== FEAR SPECTRUM ==========

    // FEAR_01 — Startle (AU1D+AU2D+AU5D+AU20B+AU26C — High onset)
    {
        FExpressionPreset FearStartle;
        FearStartle.ExpressionState = EExpressionState::FearStartle;
        FearStartle.MorphTargets.BrowRaiseL = 0.8f;
        FearStartle.MorphTargets.BrowRaiseR = 0.8f;
        FearStartle.MorphTargets.EyeWideL = 0.9f;
        FearStartle.MorphTargets.EyeWideR = 0.9f;
        FearStartle.MorphTargets.MouthStretchL = 0.4f;
        FearStartle.MorphTargets.MouthStretchR = 0.4f;
        FearStartle.MorphTargets.MouthOpen = 0.6f;
        FearStartle.EmissiveIntensity = 1.2f;
        FearStartle.HeadTilt = FVector2D(-10.0f, 0.0f);  // Head back (M59)
        ExpressionPresets.Add(EExpressionState::FearStartle, FearStartle);
    }

    // FEAR_02 — Anxious Dread (AU1C+AU4B+AU5B+AU7B+AU20A+AU25B — Sustained)
    {
        FExpressionPreset FearAnxious;
        FearAnxious.ExpressionState = EExpressionState::FearAnxious;
        FearAnxious.MorphTargets.BrowRaiseL = 0.6f;
        FearAnxious.MorphTargets.BrowRaiseR = 0.6f;
        FearAnxious.MorphTargets.BrowFurrow = 0.4f;
        FearAnxious.MorphTargets.EyeWideL = 0.4f;
        FearAnxious.MorphTargets.EyeWideR = 0.4f;
        FearAnxious.MorphTargets.EyeSquintL = 0.3f;
        FearAnxious.MorphTargets.EyeSquintR = 0.3f;
        FearAnxious.MorphTargets.MouthStretchL = 0.2f;
        FearAnxious.MorphTargets.MouthStretchR = 0.2f;
        FearAnxious.MorphTargets.LipPart = 0.4f;
        FearAnxious.EmissiveIntensity = 0.9f;
        FearAnxious.HeadTilt = FVector2D(0.0f, 0.0f);
        ExpressionPresets.Add(EExpressionState::FearAnxious, FearAnxious);
    }

    // FEAR_03 — Frozen Terror (AU1D+AU2D+AU4C+AU5E+AU7C+AU20C+AU26D — Maximum)
    {
        FExpressionPreset FearTerror;
        FearTerror.ExpressionState = EExpressionState::FearTerror;
        FearTerror.MorphTargets.BrowRaiseL = 0.9f;
        FearTerror.MorphTargets.BrowRaiseR = 0.9f;
        FearTerror.MorphTargets.BrowFurrow = 0.6f;
        FearTerror.MorphTargets.EyeWideL = 1.0f;
        FearTerror.MorphTargets.EyeWideR = 1.0f;
        FearTerror.MorphTargets.EyeSquintL = 0.6f;
        FearTerror.MorphTargets.EyeSquintR = 0.6f;
        FearTerror.MorphTargets.MouthStretchL = 0.7f;
        FearTerror.MorphTargets.MouthStretchR = 0.7f;
        FearTerror.MorphTargets.MouthOpen = 0.8f;
        FearTerror.EmissiveIntensity = 0.7f;
        FearTerror.HeadTilt = FVector2D(-8.0f, 0.0f);
        ExpressionPresets.Add(EExpressionState::FearTerror, FearTerror);
    }

    // FEAR_04 — Worried Concern (AU1B+AU4A+AU5A+AU7A+AU25A — Subtle)
    {
        FExpressionPreset FearWorried;
        FearWorried.ExpressionState = EExpressionState::FearWorried;
        FearWorried.MorphTargets.BrowRaiseL = 0.3f;
        FearWorried.MorphTargets.BrowRaiseR = 0.3f;
        FearWorried.MorphTargets.BrowFurrow = 0.2f;
        FearWorried.MorphTargets.EyeWideL = 0.2f;
        FearWorried.MorphTargets.EyeWideR = 0.2f;
        FearWorried.MorphTargets.EyeSquintL = 0.15f;
        FearWorried.MorphTargets.EyeSquintR = 0.15f;
        FearWorried.MorphTargets.LipPart = 0.2f;
        FearWorried.EmissiveIntensity = 0.95f;
        FearWorried.HeadTilt = FVector2D(0.0f, 0.0f);
        ExpressionPresets.Add(EExpressionState::FearWorried, FearWorried);
    }

    // ========== ANGER SPECTRUM ==========

    // ANGER_01 — Irritation (AU4B+AU7B+AU23A+AU24A — Subtle)
    {
        FExpressionPreset AngerIrritation;
        AngerIrritation.ExpressionState = EExpressionState::AngerIrritation;
        AngerIrritation.MorphTargets.BrowFurrow = 0.4f;
        AngerIrritation.MorphTargets.EyeSquintL = 0.3f;
        AngerIrritation.MorphTargets.EyeSquintR = 0.3f;
        AngerIrritation.MorphTargets.LipTightenL = 0.2f;
        AngerIrritation.MorphTargets.LipTightenR = 0.2f;
        AngerIrritation.MorphTargets.LipPressL = 0.2f;
        AngerIrritation.MorphTargets.LipPressR = 0.2f;
        AngerIrritation.EmissiveIntensity = 1.0f;
        AngerIrritation.HeadTilt = FVector2D(-5.0f, 0.0f);  // Slight forward (M57)
        ExpressionPresets.Add(EExpressionState::AngerIrritation, AngerIrritation);
    }

    // ANGER_02 — Controlled Fury (AU4C+AU5B+AU7C+AU23B+AU24B — Moderate)
    {
        FExpressionPreset AngerFury;
        AngerFury.ExpressionState = EExpressionState::AngerFury;
        AngerFury.MorphTargets.BrowFurrow = 0.6f;
        AngerFury.MorphTargets.EyeWideL = 0.3f;
        AngerFury.MorphTargets.EyeWideR = 0.3f;
        AngerFury.MorphTargets.EyeSquintL = 0.6f;
        AngerFury.MorphTargets.EyeSquintR = 0.6f;
        AngerFury.MorphTargets.LipTightenL = 0.4f;
        AngerFury.MorphTargets.LipTightenR = 0.4f;
        AngerFury.MorphTargets.LipPressL = 0.4f;
        AngerFury.MorphTargets.LipPressR = 0.4f;
        AngerFury.EmissiveIntensity = 1.1f;
        AngerFury.HeadTilt = FVector2D(-8.0f, 0.0f);  // Head forward (M57)
        ExpressionPresets.Add(EExpressionState::AngerFury, AngerFury);
    }

    // ANGER_03 — Rage (AU4D+AU5C+AU7D+AU9C+AU10C+AU23C+AU25C+AU26C — Maximum)
    {
        FExpressionPreset AngerRage;
        AngerRage.ExpressionState = EExpressionState::AngerRage;
        AngerRage.MorphTargets.BrowFurrow = 0.8f;
        AngerRage.MorphTargets.EyeWideL = 0.5f;
        AngerRage.MorphTargets.EyeWideR = 0.5f;
        AngerRage.MorphTargets.EyeSquintL = 0.8f;
        AngerRage.MorphTargets.EyeSquintR = 0.8f;
        AngerRage.MorphTargets.NoseScrunch = 0.6f;
        AngerRage.MorphTargets.LipRaiseL = 0.4f;
        AngerRage.MorphTargets.LipRaiseR = 0.4f;
        AngerRage.MorphTargets.LipTightenL = 0.6f;
        AngerRage.MorphTargets.LipTightenR = 0.6f;
        AngerRage.MorphTargets.LipPressL = 0.6f;
        AngerRage.MorphTargets.LipPressR = 0.6f;
        AngerRage.MorphTargets.MouthOpen = 0.3f;
        AngerRage.EmissiveIntensity = 1.4f;
        AngerRage.HeadTilt = FVector2D(-10.0f, 0.0f);
        ExpressionPresets.Add(EExpressionState::AngerRage, AngerRage);
    }

    // ANGER_04 — Determined Resolve (AU4B+AU7B+AU17A+AU24A — Contained)
    {
        FExpressionPreset AngerResolve;
        AngerResolve.ExpressionState = EExpressionState::AngerResolve;
        AngerResolve.MorphTargets.BrowFurrow = 0.4f;
        AngerResolve.MorphTargets.EyeSquintL = 0.3f;
        AngerResolve.MorphTargets.EyeSquintR = 0.3f;
        AngerResolve.MorphTargets.ChinRaiseL = 0.2f;
        AngerResolve.MorphTargets.ChinRaiseR = 0.2f;
        AngerResolve.MorphTargets.LipPressL = 0.2f;
        AngerResolve.MorphTargets.LipPressR = 0.2f;
        AngerResolve.EmissiveIntensity = 1.05f;
        AngerResolve.HeadTilt = FVector2D(-5.0f, 0.0f);
        ExpressionPresets.Add(EExpressionState::AngerResolve, AngerResolve);
    }

    // ========== SADNESS SPECTRUM ==========

    // SAD_01 — Melancholy (AU1B+AU4A+AU15A+AU17A — Subtle)
    {
        FExpressionPreset SadMelancholy;
        SadMelancholy.ExpressionState = EExpressionState::SadMelancholy;
        SadMelancholy.MorphTargets.BrowRaiseL = 0.3f;
        SadMelancholy.MorphTargets.BrowRaiseR = 0.3f;
        SadMelancholy.MorphTargets.BrowFurrow = 0.2f;
        SadMelancholy.MorphTargets.MouthFrownL = 0.2f;
        SadMelancholy.MorphTargets.MouthFrownR = 0.2f;
        SadMelancholy.MorphTargets.ChinRaiseL = 0.2f;
        SadMelancholy.MorphTargets.ChinRaiseR = 0.2f;
        SadMelancholy.EmissiveIntensity = 0.8f;
        SadMelancholy.HeadTilt = FVector2D(0.0f, -5.0f);  // Eyes down (M69)
        ExpressionPresets.Add(EExpressionState::SadMelancholy, SadMelancholy);
    }

    // SAD_02 — Grief (AU1C+AU4B+AU6B+AU11B+AU15C+AU17B — Deep)
    {
        FExpressionPreset SadGrief;
        SadGrief.ExpressionState = EExpressionState::SadGrief;
        SadGrief.MorphTargets.BrowRaiseL = 0.6f;
        SadGrief.MorphTargets.BrowRaiseR = 0.6f;
        SadGrief.MorphTargets.BrowFurrow = 0.4f;
        SadGrief.MorphTargets.CheekRaiseL = 0.3f;
        SadGrief.MorphTargets.CheekRaiseR = 0.3f;
        SadGrief.MorphTargets.NasolabialDeepenL = 0.4f;
        SadGrief.MorphTargets.NasolabialDeepenR = 0.4f;
        SadGrief.MorphTargets.MouthFrownL = 0.6f;
        SadGrief.MorphTargets.MouthFrownR = 0.6f;
        SadGrief.MorphTargets.ChinRaiseL = 0.4f;
        SadGrief.MorphTargets.ChinRaiseR = 0.4f;
        SadGrief.EmissiveIntensity = 0.7f;
        SadGrief.HeadTilt = FVector2D(0.0f, -8.0f);
        ExpressionPresets.Add(EExpressionState::SadGrief, SadGrief);
    }

    // SAD_03 — Crying (AU1C+AU4B+AU6C+AU7C+AU11B+AU15C+AU17C+AU25B — Intense)
    {
        FExpressionPreset SadCrying;
        SadCrying.ExpressionState = EExpressionState::SadCrying;
        SadCrying.MorphTargets.BrowRaiseL = 0.6f;
        SadCrying.MorphTargets.BrowRaiseR = 0.6f;
        SadCrying.MorphTargets.BrowFurrow = 0.4f;
        SadCrying.MorphTargets.CheekRaiseL = 0.6f;
        SadCrying.MorphTargets.CheekRaiseR = 0.6f;
        SadCrying.MorphTargets.EyeSquintL = 0.6f;
        SadCrying.MorphTargets.EyeSquintR = 0.6f;
        SadCrying.MorphTargets.NasolabialDeepenL = 0.4f;
        SadCrying.MorphTargets.NasolabialDeepenR = 0.4f;
        SadCrying.MorphTargets.MouthFrownL = 0.6f;
        SadCrying.MorphTargets.MouthFrownR = 0.6f;
        SadCrying.MorphTargets.ChinRaiseL = 0.6f;
        SadCrying.MorphTargets.ChinRaiseR = 0.6f;
        SadCrying.MorphTargets.LipPart = 0.4f;
        SadCrying.EmissiveIntensity = 0.6f;
        SadCrying.HeadTilt = FVector2D(0.0f, -10.0f);
        ExpressionPresets.Add(EExpressionState::SadCrying, SadCrying);
    }

    // SAD_04 — Disappointed (AU1A+AU15A+AU17A+AU25A — Minimal)
    {
        FExpressionPreset SadDisappointed;
        SadDisappointed.ExpressionState = EExpressionState::SadDisappointed;
        SadDisappointed.MorphTargets.BrowRaiseL = 0.2f;
        SadDisappointed.MorphTargets.BrowRaiseR = 0.2f;
        SadDisappointed.MorphTargets.MouthFrownL = 0.2f;
        SadDisappointed.MorphTargets.MouthFrownR = 0.2f;
        SadDisappointed.MorphTargets.ChinRaiseL = 0.2f;
        SadDisappointed.MorphTargets.ChinRaiseR = 0.2f;
        SadDisappointed.MorphTargets.LipPart = 0.2f;
        SadDisappointed.EmissiveIntensity = 0.85f;
        SadDisappointed.HeadTilt = FVector2D(0.0f, -4.0f);
        ExpressionPresets.Add(EExpressionState::SadDisappointed, SadDisappointed);
    }

    // ========== DISGUST SPECTRUM ==========

    // DISG_01 — Mild Distaste (AU9B+AU15A+AU17A — Subtle)
    {
        FExpressionPreset DisgustDistaste;
        DisgustDistaste.ExpressionState = EExpressionState::DisgustDistaste;
        DisgustDistaste.MorphTargets.NoseScrunch = 0.3f;
        DisgustDistaste.MorphTargets.MouthFrownL = 0.2f;
        DisgustDistaste.MorphTargets.MouthFrownR = 0.2f;
        DisgustDistaste.MorphTargets.ChinRaiseL = 0.2f;
        DisgustDistaste.MorphTargets.ChinRaiseR = 0.2f;
        DisgustDistaste.EmissiveIntensity = 0.9f;
        DisgustDistaste.HeadTilt = FVector2D(0.0f, 0.0f);
        ExpressionPresets.Add(EExpressionState::DisgustDistaste, DisgustDistaste);
    }

    // DISG_02 — Revulsion (AU9C+AU10C+AU15B+AU17B+AU25B+AU26A — Moderate)
    {
        FExpressionPreset DisgustRevulsion;
        DisgustRevulsion.ExpressionState = EExpressionState::DisgustRevulsion;
        DisgustRevulsion.MorphTargets.NoseScrunch = 0.6f;
        DisgustRevulsion.MorphTargets.LipRaiseL = 0.4f;
        DisgustRevulsion.MorphTargets.LipRaiseR = 0.4f;
        DisgustRevulsion.MorphTargets.MouthFrownL = 0.4f;
        DisgustRevulsion.MorphTargets.MouthFrownR = 0.4f;
        DisgustRevulsion.MorphTargets.ChinRaiseL = 0.4f;
        DisgustRevulsion.MorphTargets.ChinRaiseR = 0.4f;
        DisgustRevulsion.MorphTargets.LipPart = 0.3f;
        DisgustRevulsion.MorphTargets.MouthOpen = 0.2f;
        DisgustRevulsion.EmissiveIntensity = 0.85f;
        DisgustRevulsion.HeadTilt = FVector2D(0.0f, 0.0f);
        ExpressionPresets.Add(EExpressionState::DisgustRevulsion, DisgustRevulsion);
    }

    // DISG_03 — Contemptuous Disgust (R9B+R14B+AU15A+AU17A — Asymmetric)
    {
        FExpressionPreset DisgustContempt;
        DisgustContempt.ExpressionState = EExpressionState::DisgustContempt;
        DisgustContempt.MorphTargets.NoseScrunch = 0.3f;  // Right-side (bilateral approximation)
        DisgustContempt.MorphTargets.MouthSmileR = 0.3f;  // Unilateral smirk right
        DisgustContempt.MorphTargets.MouthFrownL = 0.2f;
        DisgustContempt.MorphTargets.MouthFrownR = 0.2f;
        DisgustContempt.MorphTargets.ChinRaiseL = 0.2f;
        DisgustContempt.MorphTargets.ChinRaiseR = 0.2f;
        DisgustContempt.EmissiveIntensity = 0.95f;
        DisgustContempt.HeadTilt = FVector2D(0.0f, 5.0f);  // Slight head turn
        ExpressionPresets.Add(EExpressionState::DisgustContempt, DisgustContempt);
    }

    UE_LOG(LogTemp, Log, TEXT("Negative emotion presets initialized: %d total expressions"),
        ExpressionPresets.Num());
}

void UDeepTreeEchoExpressionSystem::InitializeEchobeatsMapping()
{
    // Map the 12-step echobeats cognitive cycle to expression hints
    // Based on the 3 concurrent streams phased 4 steps apart
    
    EchobeatsSteps.Empty();
    
    // Steps {1,5,9} - Pivotal Relevance Realization (Stream 1)
    EchobeatsSteps.Add({1, TEXT("Pivotal-Perception"), EExpressionState::AlertGaze});
    EchobeatsSteps.Add({5, TEXT("Pivotal-Action"), EExpressionState::GentleSmile});
    EchobeatsSteps.Add({9, TEXT("Pivotal-Simulation"), EExpressionState::Contemplative});
    
    // Steps {2,6,10} - Affordance Interaction (Stream 2)
    EchobeatsSteps.Add({2, TEXT("Affordance-Perception"), EExpressionState::CuriousGaze});
    EchobeatsSteps.Add({6, TEXT("Affordance-Action"), EExpressionState::SpeakingVowel});
    EchobeatsSteps.Add({10, TEXT("Affordance-Simulation"), EExpressionState::Contemplative});
    
    // Steps {3,7,11} - Salience Processing (Stream 3)
    EchobeatsSteps.Add({3, TEXT("Salience-Perception"), EExpressionState::Awe});
    EchobeatsSteps.Add({7, TEXT("Salience-Action"), EExpressionState::BroadSmile});
    EchobeatsSteps.Add({11, TEXT("Salience-Simulation"), EExpressionState::Blissful});
    
    // Steps {4,8,12} - Integration/Transition
    EchobeatsSteps.Add({4, TEXT("Integration-1"), EExpressionState::GentleSmile});
    EchobeatsSteps.Add({8, TEXT("Integration-2"), EExpressionState::Neutral});
    EchobeatsSteps.Add({12, TEXT("Integration-3"), EExpressionState::Contemplative});

    UE_LOG(LogTemp, Log, TEXT("Echobeats mapping initialized: %d steps"), EchobeatsSteps.Num());
}

void UDeepTreeEchoExpressionSystem::InitializeCognitiveMapping()
{
    // Map cognitive states to default expressions
    CognitiveMapping.Add(ECognitiveState::Idle, EExpressionState::Neutral);
    CognitiveMapping.Add(ECognitiveState::Processing, EExpressionState::Contemplative);
    CognitiveMapping.Add(ECognitiveState::Reasoning, EExpressionState::CuriousGaze);
    CognitiveMapping.Add(ECognitiveState::Insight, EExpressionState::BroadSmile);
    CognitiveMapping.Add(ECognitiveState::Communicating, EExpressionState::SpeakingVowel);
    CognitiveMapping.Add(ECognitiveState::Listening, EExpressionState::AlertGaze);
    CognitiveMapping.Add(ECognitiveState::Reflecting, EExpressionState::Blissful);
    CognitiveMapping.Add(ECognitiveState::Engaging, EExpressionState::GentleSmile);

    UE_LOG(LogTemp, Log, TEXT("Cognitive mapping initialized: %d states"), CognitiveMapping.Num());
}

void UDeepTreeEchoExpressionSystem::SetExpressionState(EExpressionState NewState, float TransitionTime)
{
    if (NewState == CurrentExpressionState && ExpressionTransitionProgress >= 1.0f)
        return;

    TargetExpressionState = NewState;
    ExpressionTransitionDuration = FMath::Max(TransitionTime, 0.01f);
    ExpressionTransitionProgress = 0.0f;

    if (ExpressionPresets.Contains(TargetExpressionState))
    {
        TargetMorphTargets = ExpressionPresets[TargetExpressionState].MorphTargets;
        TargetEmissiveIntensity = ExpressionPresets[TargetExpressionState].EmissiveIntensity;
    }

    UE_LOG(LogTemp, Log, TEXT("Expression transition: %d -> %d (%.2fs)"),
        (int32)CurrentExpressionState, (int32)TargetExpressionState, TransitionTime);
}

void UDeepTreeEchoExpressionSystem::SetCognitiveState(ECognitiveState NewState)
{
    CurrentCognitiveState = NewState;
    
    EExpressionState MappedExpression = MapCognitiveToExpression(NewState);
    SetExpressionState(MappedExpression, 0.5f);

    UE_LOG(LogTemp, Log, TEXT("Cognitive state set: %d -> Expression: %d"),
        (int32)NewState, (int32)MappedExpression);
}

EExpressionState UDeepTreeEchoExpressionSystem::MapCognitiveToExpression(ECognitiveState CogState)
{
    if (CognitiveMapping.Contains(CogState))
    {
        return CognitiveMapping[CogState];
    }
    return EExpressionState::Neutral;
}

void UDeepTreeEchoExpressionSystem::UpdateExpressionTransition(float DeltaTime)
{
    if (ExpressionTransitionProgress < 1.0f)
    {
        ExpressionTransitionProgress += DeltaTime / ExpressionTransitionDuration;
        ExpressionTransitionProgress = FMath::Clamp(ExpressionTransitionProgress, 0.0f, 1.0f);

        // Smooth step for more natural transitions
        float SmoothAlpha = FMath::SmoothStep(0.0f, 1.0f, ExpressionTransitionProgress);

        // Interpolate morph targets
        FDeepTreeEchoMorphTargets BaseMorphTargets;
        if (ExpressionPresets.Contains(CurrentExpressionState))
        {
            BaseMorphTargets = ExpressionPresets[CurrentExpressionState].MorphTargets;
        }
        
        CurrentMorphTargets = FDeepTreeEchoMorphTargets::Lerp(BaseMorphTargets, TargetMorphTargets, SmoothAlpha);

        // Interpolate emissive
        float BaseEmissive = 1.0f;
        if (ExpressionPresets.Contains(CurrentExpressionState))
        {
            BaseEmissive = ExpressionPresets[CurrentExpressionState].EmissiveIntensity;
        }
        CurrentEmissiveIntensity = FMath::Lerp(BaseEmissive, TargetEmissiveIntensity, SmoothAlpha);

        if (ExpressionTransitionProgress >= 1.0f)
        {
            CurrentExpressionState = TargetExpressionState;
        }
    }
}

void UDeepTreeEchoExpressionSystem::UpdateProceduralBlinking(float DeltaTime)
{
    if (!bBlinkingEnabled)
        return;

    // Reset procedural eye close values
    ProceduralMorphTargets.EyeCloseL = 0.0f;
    ProceduralMorphTargets.EyeCloseR = 0.0f;

    if (bIsBlinking)
    {
        // Blink animation
        BlinkProgress += DeltaTime / ProceduralSettings.BlinkDuration;
        
        if (BlinkProgress >= 1.0f)
        {
            bIsBlinking = false;
            BlinkProgress = 0.0f;
            NextBlinkTime = FMath::RandRange(ProceduralSettings.MinBlinkInterval, ProceduralSettings.MaxBlinkInterval);
            BlinkTimer = 0.0f;
        }
        else
        {
            // Smooth blink curve (fast close, slower open)
            float BlinkValue;
            if (BlinkProgress < 0.3f)
            {
                // Fast close
                BlinkValue = FMath::Lerp(0.0f, 1.0f, BlinkProgress / 0.3f);
            }
            else
            {
                // Slower open
                BlinkValue = FMath::Lerp(1.0f, 0.0f, (BlinkProgress - 0.3f) / 0.7f);
            }
            
            ProceduralMorphTargets.EyeCloseL = BlinkValue;
            ProceduralMorphTargets.EyeCloseR = BlinkValue;
        }
    }
    else
    {
        // Wait for next blink
        BlinkTimer += DeltaTime;
        if (BlinkTimer >= NextBlinkTime)
        {
            TriggerBlink();
        }
    }
}

void UDeepTreeEchoExpressionSystem::UpdateProceduralBreathing(float DeltaTime)
{
    if (!bBreathingEnabled)
        return;

    BreathingPhase += DeltaTime * ProceduralSettings.BreathingRate * 2.0f * PI;
    if (BreathingPhase > 2.0f * PI)
    {
        BreathingPhase -= 2.0f * PI;
    }

    // Breathing affects subtle mouth/lip movement
    float BreathValue = FMath::Sin(BreathingPhase) * ProceduralSettings.BreathingAmplitude;
    ProceduralMorphTargets.LipPart += BreathValue * 0.5f;
}

void UDeepTreeEchoExpressionSystem::UpdateProceduralSaccades(float DeltaTime)
{
    if (!bSaccadesEnabled)
        return;

    SaccadeTimer += DeltaTime;

    // Check if we should start a new saccade
    if (SaccadeTimer >= 1.0f / ProceduralSettings.SaccadeFrequency)
    {
        SaccadeTimer = 0.0f;
        
        // Generate new random target
        TargetSaccadeOffset.X = FMath::RandRange(-ProceduralSettings.SaccadeAmplitude, ProceduralSettings.SaccadeAmplitude);
        TargetSaccadeOffset.Y = FMath::RandRange(-ProceduralSettings.SaccadeAmplitude, ProceduralSettings.SaccadeAmplitude);
    }

    // Interpolate towards target
    float SaccadeSpeed = ProceduralSettings.SaccadeSpeed * DeltaTime;
    CurrentSaccadeOffset = FMath::Vector2DInterpTo(CurrentSaccadeOffset, TargetSaccadeOffset, DeltaTime, SaccadeSpeed);

    // Apply to eye rotation (this would be used by the animation blueprint)
    CurrentEyeRotation.Pitch = CurrentSaccadeOffset.Y;
    CurrentEyeRotation.Yaw = CurrentSaccadeOffset.X;
}

void UDeepTreeEchoExpressionSystem::UpdateMicroExpression(float DeltaTime)
{
    if (!bMicroExpressionActive)
        return;

    MicroExpressionTimer += DeltaTime;

    if (MicroExpressionTimer >= MicroExpressionDuration)
    {
        bMicroExpressionActive = false;
        ActiveMicroExpression = EMicroExpression::None;
        MicroExpressionTimer = 0.0f;
        return;
    }

    // Apply microexpression based on type
    float MicroProgress = MicroExpressionTimer / MicroExpressionDuration;
    float MicroIntensity = FMath::Sin(MicroProgress * PI);  // Fade in and out

    switch (ActiveMicroExpression)
    {
    case EMicroExpression::Blink:
        ProceduralMorphTargets.EyeCloseL = MicroIntensity;
        ProceduralMorphTargets.EyeCloseR = MicroIntensity;
        break;
        
    case EMicroExpression::UnilateralBrowRaise:
        ProceduralMorphTargets.BrowRaiseL += MicroIntensity * 0.3f;
        break;
        
    case EMicroExpression::LipCornerTwitch:
        ProceduralMorphTargets.MouthSmileL += MicroIntensity * 0.2f;
        break;
        
    case EMicroExpression::NoseWrinkle:
        ProceduralMorphTargets.NoseScrunch += MicroIntensity * 0.4f;
        break;
        
    case EMicroExpression::EyelidFlutter:
        ProceduralMorphTargets.EyeCloseL = FMath::Sin(MicroExpressionTimer * 30.0f) * 0.3f * MicroIntensity;
        ProceduralMorphTargets.EyeCloseR = FMath::Sin(MicroExpressionTimer * 30.0f) * 0.3f * MicroIntensity;
        break;
        
    case EMicroExpression::LipPurse:
        ProceduralMorphTargets.LipPucker += MicroIntensity * 0.3f;
        break;
        
    case EMicroExpression::JawSlack:
        ProceduralMorphTargets.MouthOpen += MicroIntensity * 0.15f;
        break;
        
    default:
        break;
    }
}

void UDeepTreeEchoExpressionSystem::UpdateEmissivePulse(float DeltaTime)
{
    if (!bEmissivePulseActive)
        return;

    EmissivePulseTimer += DeltaTime;

    if (EmissivePulseTimer >= EmissivePulseDuration)
    {
        bEmissivePulseActive = false;
        EmissivePulseTimer = 0.0f;
        return;
    }

    // Pulse curve
    float PulseProgress = EmissivePulseTimer / EmissivePulseDuration;
    float PulseValue = FMath::Sin(PulseProgress * PI);  // Smooth pulse
    
    CurrentEmissiveIntensity = FMath::Lerp(TargetEmissiveIntensity, EmissivePulsePeak, PulseValue);
}

void UDeepTreeEchoExpressionSystem::UpdateWonderToJoyTransition(float DeltaTime)
{
    if (!bWonderToJoyActive)
        return;

    WonderToJoyProgress += DeltaTime / WonderToJoyDuration;

    if (WonderToJoyProgress >= 1.0f)
    {
        bWonderToJoyActive = false;
        WonderToJoyProgress = 0.0f;
        SetExpressionState(EExpressionState::GentleSmile, 0.3f);
        return;
    }

    // Phase 1 (0-0.4): Wonder/Contemplative with raised brows
    // Phase 2 (0.4-0.6): Deliberate blink (the "aha" moment)
    // Phase 3 (0.6-1.0): Transition to smile

    if (WonderToJoyProgress < 0.4f)
    {
        // Wonder phase
        float WonderAlpha = WonderToJoyProgress / 0.4f;
        SetExpressionState(EExpressionState::Contemplative, 0.1f);
    }
    else if (WonderToJoyProgress < 0.6f)
    {
        // Deliberate blink phase
        float BlinkAlpha = (WonderToJoyProgress - 0.4f) / 0.2f;
        float BlinkValue = FMath::Sin(BlinkAlpha * PI);
        ProceduralMorphTargets.EyeCloseL = BlinkValue * 0.8f;
        ProceduralMorphTargets.EyeCloseR = BlinkValue * 0.8f;
    }
    else
    {
        // Joy phase
        float JoyAlpha = (WonderToJoyProgress - 0.6f) / 0.4f;
        if (JoyAlpha < 0.1f)
        {
            SetExpressionState(EExpressionState::GentleSmile, 0.3f);
        }
        // Pulse emissives at the moment of insight
        if (JoyAlpha > 0.1f && JoyAlpha < 0.2f)
        {
            PulseEmissives(0.3f, 1.8f);
        }
    }
}

void UDeepTreeEchoExpressionSystem::TriggerMicroExpression(EMicroExpression MicroExp, float Duration)
{
    if (bMicroExpressionActive)
        return;

    ActiveMicroExpression = MicroExp;
    MicroExpressionDuration = Duration;
    MicroExpressionTimer = 0.0f;
    bMicroExpressionActive = true;

    UE_LOG(LogTemp, Log, TEXT("Microexpression triggered: %d for %.2fs"), (int32)MicroExp, Duration);
}

void UDeepTreeEchoExpressionSystem::TriggerWonderToJoyTransition(float Duration)
{
    if (bWonderToJoyActive)
        return;

    bWonderToJoyActive = true;
    WonderToJoyProgress = 0.0f;
    WonderToJoyDuration = Duration;

    // Start in contemplative state
    SetExpressionState(EExpressionState::Contemplative, 0.2f);

    UE_LOG(LogTemp, Log, TEXT("Wonder-to-Joy transition triggered for %.2fs"), Duration);
}

void UDeepTreeEchoExpressionSystem::SetBlinkingEnabled(bool bEnabled)
{
    bBlinkingEnabled = bEnabled;
    if (!bEnabled)
    {
        bIsBlinking = false;
        BlinkProgress = 0.0f;
        ProceduralMorphTargets.EyeCloseL = 0.0f;
        ProceduralMorphTargets.EyeCloseR = 0.0f;
    }
}

void UDeepTreeEchoExpressionSystem::SetBreathingEnabled(bool bEnabled)
{
    bBreathingEnabled = bEnabled;
}

void UDeepTreeEchoExpressionSystem::SetSaccadesEnabled(bool bEnabled)
{
    bSaccadesEnabled = bEnabled;
    if (!bEnabled)
    {
        CurrentSaccadeOffset = FVector2D::ZeroVector;
        TargetSaccadeOffset = FVector2D::ZeroVector;
        CurrentEyeRotation = FRotator::ZeroRotator;
    }
}

void UDeepTreeEchoExpressionSystem::TriggerBlink()
{
    if (!bIsBlinking)
    {
        bIsBlinking = true;
        BlinkProgress = 0.0f;
    }
}

void UDeepTreeEchoExpressionSystem::SetGazeTarget(const FVector& WorldTarget)
{
    GazeTarget = WorldTarget;
    bHasGazeTarget = true;
    
    // Disable procedural saccades when we have a specific target
    bSaccadesEnabled = false;
}

void UDeepTreeEchoExpressionSystem::ClearGazeTarget()
{
    bHasGazeTarget = false;
    bSaccadesEnabled = true;
}

void UDeepTreeEchoExpressionSystem::SetSpeakingAmplitude(float Amplitude)
{
    CurrentSpeakingAmplitude = FMath::Clamp(Amplitude, 0.0f, 1.0f);
    
    // Apply amplitude to mouth open
    ProceduralMorphTargets.MouthOpen = CurrentSpeakingAmplitude * 0.5f;
    ProceduralMorphTargets.LipPart = CurrentSpeakingAmplitude * 0.3f;
}

void UDeepTreeEchoExpressionSystem::SetViseme(const FName& VisemeName, float Weight)
{
    Weight = FMath::Clamp(Weight, 0.0f, 1.0f);
    
    if (VisemeName == TEXT("AA") || VisemeName == TEXT("ah"))
    {
        ProceduralMorphTargets.VisemeAA = Weight;
        ProceduralMorphTargets.MouthOpen = Weight * 0.6f;
    }
    else if (VisemeName == TEXT("EE") || VisemeName == TEXT("ee"))
    {
        ProceduralMorphTargets.VisemeEE = Weight;
        ProceduralMorphTargets.MouthWide = Weight * 0.4f;
    }
    else if (VisemeName == TEXT("OH") || VisemeName == TEXT("oh"))
    {
        ProceduralMorphTargets.VisemeOH = Weight;
        ProceduralMorphTargets.MouthOpen = Weight * 0.5f;
    }
    else if (VisemeName == TEXT("OO") || VisemeName == TEXT("oo"))
    {
        ProceduralMorphTargets.VisemeOO = Weight;
        ProceduralMorphTargets.LipPucker = Weight * 0.5f;
    }
}

void UDeepTreeEchoExpressionSystem::SetEmissiveIntensity(float Intensity)
{
    TargetEmissiveIntensity = FMath::Max(Intensity, 0.0f);
}

void UDeepTreeEchoExpressionSystem::PulseEmissives(float Duration, float PeakIntensity)
{
    if (bEmissivePulseActive)
        return;

    bEmissivePulseActive = true;
    EmissivePulseTimer = 0.0f;
    EmissivePulseDuration = Duration;
    EmissivePulsePeak = PeakIntensity;

    UE_LOG(LogTemp, Log, TEXT("Emissive pulse triggered: %.2fs, peak %.2f"), Duration, PeakIntensity);
}

void UDeepTreeEchoExpressionSystem::OnEchobeatsStep(int32 StepNumber)
{
    // Find the expression hint for this step
    for (const FEchobeatsStep& Step : EchobeatsSteps)
    {
        if (Step.StepNumber == StepNumber)
        {
            SetExpressionState(Step.ExpressionHint, 0.2f);
            
            UE_LOG(LogTemp, Log, TEXT("Echobeats step %d (%s) -> Expression: %d"),
                StepNumber, *Step.CognitivePhase, (int32)Step.ExpressionHint);
            return;
        }
    }
}

void UDeepTreeEchoExpressionSystem::ApplyToSkeletalMesh(USkeletalMeshComponent* TargetMesh)
{
    if (!TargetMesh)
        return;

    // Apply morph targets to the skeletal mesh
    // These names should match the morph target names in the skeletal mesh asset
    
    TargetMesh->SetMorphTarget(TEXT("BrowRaiseL"), CurrentMorphTargets.BrowRaiseL);
    TargetMesh->SetMorphTarget(TEXT("BrowRaiseR"), CurrentMorphTargets.BrowRaiseR);
    TargetMesh->SetMorphTarget(TEXT("BrowFurrow"), CurrentMorphTargets.BrowFurrow);
    
    TargetMesh->SetMorphTarget(TEXT("EyeWideL"), CurrentMorphTargets.EyeWideL);
    TargetMesh->SetMorphTarget(TEXT("EyeWideR"), CurrentMorphTargets.EyeWideR);
    TargetMesh->SetMorphTarget(TEXT("EyeSquintL"), CurrentMorphTargets.EyeSquintL);
    TargetMesh->SetMorphTarget(TEXT("EyeSquintR"), CurrentMorphTargets.EyeSquintR);
    TargetMesh->SetMorphTarget(TEXT("EyeCloseL"), CurrentMorphTargets.EyeCloseL);
    TargetMesh->SetMorphTarget(TEXT("EyeCloseR"), CurrentMorphTargets.EyeCloseR);
    
    TargetMesh->SetMorphTarget(TEXT("NoseScrunch"), CurrentMorphTargets.NoseScrunch);
    
    TargetMesh->SetMorphTarget(TEXT("CheekRaiseL"), CurrentMorphTargets.CheekRaiseL);
    TargetMesh->SetMorphTarget(TEXT("CheekRaiseR"), CurrentMorphTargets.CheekRaiseR);
    
    TargetMesh->SetMorphTarget(TEXT("MouthSmileL"), CurrentMorphTargets.MouthSmileL);
    TargetMesh->SetMorphTarget(TEXT("MouthSmileR"), CurrentMorphTargets.MouthSmileR);
    TargetMesh->SetMorphTarget(TEXT("MouthOpen"), CurrentMorphTargets.MouthOpen);
    TargetMesh->SetMorphTarget(TEXT("MouthWide"), CurrentMorphTargets.MouthWide);
    TargetMesh->SetMorphTarget(TEXT("LipPucker"), CurrentMorphTargets.LipPucker);
    TargetMesh->SetMorphTarget(TEXT("LipPart"), CurrentMorphTargets.LipPart);
    
    TargetMesh->SetMorphTarget(TEXT("VisemeAA"), CurrentMorphTargets.VisemeAA);
    TargetMesh->SetMorphTarget(TEXT("VisemeEE"), CurrentMorphTargets.VisemeEE);
    TargetMesh->SetMorphTarget(TEXT("VisemeOH"), CurrentMorphTargets.VisemeOH);
    TargetMesh->SetMorphTarget(TEXT("VisemeOO"), CurrentMorphTargets.VisemeOO);

    // Negative emotion morph targets (FACS-Complete v4)
    TargetMesh->SetMorphTarget(TEXT("MouthFrownL"), CurrentMorphTargets.MouthFrownL);
    TargetMesh->SetMorphTarget(TEXT("MouthFrownR"), CurrentMorphTargets.MouthFrownR);
    TargetMesh->SetMorphTarget(TEXT("ChinRaiseL"), CurrentMorphTargets.ChinRaiseL);
    TargetMesh->SetMorphTarget(TEXT("ChinRaiseR"), CurrentMorphTargets.ChinRaiseR);
    TargetMesh->SetMorphTarget(TEXT("LipPressL"), CurrentMorphTargets.LipPressL);
    TargetMesh->SetMorphTarget(TEXT("LipPressR"), CurrentMorphTargets.LipPressR);
    TargetMesh->SetMorphTarget(TEXT("LipTightenL"), CurrentMorphTargets.LipTightenL);
    TargetMesh->SetMorphTarget(TEXT("LipTightenR"), CurrentMorphTargets.LipTightenR);
    TargetMesh->SetMorphTarget(TEXT("LipRaiseL"), CurrentMorphTargets.LipRaiseL);
    TargetMesh->SetMorphTarget(TEXT("LipRaiseR"), CurrentMorphTargets.LipRaiseR);
    TargetMesh->SetMorphTarget(TEXT("MouthStretchL"), CurrentMorphTargets.MouthStretchL);
    TargetMesh->SetMorphTarget(TEXT("MouthStretchR"), CurrentMorphTargets.MouthStretchR);
    TargetMesh->SetMorphTarget(TEXT("NasolabialDeepenL"), CurrentMorphTargets.NasolabialDeepenL);
    TargetMesh->SetMorphTarget(TEXT("NasolabialDeepenR"), CurrentMorphTargets.NasolabialDeepenR);
}

void UDeepTreeEchoExpressionSystem::ApplyToMaterial(UMaterialInstanceDynamic* TargetMaterial)
{
    if (!TargetMaterial)
        return;

    // Apply emissive intensity to material
    TargetMaterial->SetScalarParameterValue(TEXT("EmissiveIntensity"), CurrentEmissiveIntensity);
    
    // Could also set color parameters based on cognitive state
    // TargetMaterial->SetVectorParameterValue(TEXT("EmissiveColor"), ...);
}

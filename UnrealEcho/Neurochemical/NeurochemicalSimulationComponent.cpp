#include "NeurochemicalSimulationComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Avatar/FacialAnimationSystem.h"
#include "Avatar/GestureSystem.h"
#include "Avatar/EmotionalAuraComponent.h"
#include "Personality/PersonalityTraitSystem.h"
#include "EchoPulseResonanceChamber.h"

UNeurochemicalSimulationComponent::UNeurochemicalSimulationComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    
    // Initialize configuration
    bEnableHomeostasis = true;
    HomeostasisRate = 0.1f;
    NeurochemicalDecayRate = 0.05f;
    InteractionStrength = 0.3f;
    
    // Initialize baseline state (healthy balanced state)
    BaselineState.DopamineLevel = 0.5f;
    BaselineState.SerotoninLevel = 0.6f;
    BaselineState.NorepinephrineLevel = 0.4f;
    BaselineState.OxytocinLevel = 0.5f;
    BaselineState.CortisolLevel = 0.3f;
    BaselineState.EndorphinsLevel = 0.4f;
    BaselineState.GABALevel = 0.6f;
    BaselineState.GlutamateLevel = 0.5f;
    BaselineState.AcetylcholineLevel = 0.5f;
    
    // Start at baseline
    CurrentState = BaselineState;
    
    // Initialize decay rates (how fast each neurochemical naturally decreases)
    DecayRates.Add(ENeurochemicalType::Dopamine, 0.08f);
    DecayRates.Add(ENeurochemicalType::Serotonin, 0.04f);
    DecayRates.Add(ENeurochemicalType::Norepinephrine, 0.1f);
    DecayRates.Add(ENeurochemicalType::Oxytocin, 0.12f);
    DecayRates.Add(ENeurochemicalType::Cortisol, 0.06f);
    DecayRates.Add(ENeurochemicalType::Endorphins, 0.15f);
    DecayRates.Add(ENeurochemicalType::GABA, 0.05f);
    DecayRates.Add(ENeurochemicalType::Glutamate, 0.07f);
    DecayRates.Add(ENeurochemicalType::Acetylcholine, 0.09f);
    
    // Initialize production rates (baseline production)
    ProductionRates.Add(ENeurochemicalType::Dopamine, 0.03f);
    ProductionRates.Add(ENeurochemicalType::Serotonin, 0.04f);
    ProductionRates.Add(ENeurochemicalType::Norepinephrine, 0.02f);
    ProductionRates.Add(ENeurochemicalType::Oxytocin, 0.02f);
    ProductionRates.Add(ENeurochemicalType::Cortisol, 0.01f);
    ProductionRates.Add(ENeurochemicalType::Endorphins, 0.02f);
    ProductionRates.Add(ENeurochemicalType::GABA, 0.04f);
    ProductionRates.Add(ENeurochemicalType::Glutamate, 0.03f);
    ProductionRates.Add(ENeurochemicalType::Acetylcholine, 0.03f);
}

void UNeurochemicalSimulationComponent::BeginPlay()
{
    Super::BeginPlay();
    
    UE_LOG(LogTemp, Log, TEXT("NeurochemicalSimulationComponent initialized for %s"), 
           *GetOwner()->GetName());
}

void UNeurochemicalSimulationComponent::TickComponent(float DeltaTime, ELevelTick TickType, 
                                                       FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    UpdateNeurochemicalDynamics(DeltaTime);
    ProcessNeurochemicalInteractions();
    
    if (bEnableHomeostasis)
    {
        ApplyHomeostasis(DeltaTime);
    }
    
    UpdateEmotionalState();
    ApplyNeurochemistryToAvatar();
}

// ===== Neurochemical Management =====

void UNeurochemicalSimulationComponent::SetNeurochemicalLevel(ENeurochemicalType Type, float Level)
{
    float ClampedLevel = FMath::Clamp(Level, 0.0f, 1.0f);
    
    switch (Type)
    {
        case ENeurochemicalType::Dopamine:
            CurrentState.DopamineLevel = ClampedLevel;
            break;
        case ENeurochemicalType::Serotonin:
            CurrentState.SerotoninLevel = ClampedLevel;
            break;
        case ENeurochemicalType::Norepinephrine:
            CurrentState.NorepinephrineLevel = ClampedLevel;
            break;
        case ENeurochemicalType::Oxytocin:
            CurrentState.OxytocinLevel = ClampedLevel;
            break;
        case ENeurochemicalType::Cortisol:
            CurrentState.CortisolLevel = ClampedLevel;
            break;
        case ENeurochemicalType::Endorphins:
            CurrentState.EndorphinsLevel = ClampedLevel;
            break;
        case ENeurochemicalType::GABA:
            CurrentState.GABALevel = ClampedLevel;
            break;
        case ENeurochemicalType::Glutamate:
            CurrentState.GlutamateLevel = ClampedLevel;
            break;
        case ENeurochemicalType::Acetylcholine:
            CurrentState.AcetylcholineLevel = ClampedLevel;
            break;
    }
}

float UNeurochemicalSimulationComponent::GetNeurochemicalLevel(ENeurochemicalType Type) const
{
    switch (Type)
    {
        case ENeurochemicalType::Dopamine:
            return CurrentState.DopamineLevel;
        case ENeurochemicalType::Serotonin:
            return CurrentState.SerotoninLevel;
        case ENeurochemicalType::Norepinephrine:
            return CurrentState.NorepinephrineLevel;
        case ENeurochemicalType::Oxytocin:
            return CurrentState.OxytocinLevel;
        case ENeurochemicalType::Cortisol:
            return CurrentState.CortisolLevel;
        case ENeurochemicalType::Endorphins:
            return CurrentState.EndorphinsLevel;
        case ENeurochemicalType::GABA:
            return CurrentState.GABALevel;
        case ENeurochemicalType::Glutamate:
            return CurrentState.GlutamateLevel;
        case ENeurochemicalType::Acetylcholine:
            return CurrentState.AcetylcholineLevel;
        default:
            return 0.0f;
    }
}

void UNeurochemicalSimulationComponent::ModifyNeurochemical(ENeurochemicalType Type, float DeltaLevel)
{
    float CurrentLevel = GetNeurochemicalLevel(Type);
    SetNeurochemicalLevel(Type, CurrentLevel + DeltaLevel);
}

// ===== Emotional Chemistry =====

FEmotionalChemistry UNeurochemicalSimulationComponent::GetEmotionalChemistry() const
{
    return CurrentEmotionalChemistry;
}

void UNeurochemicalSimulationComponent::TriggerRewardResponse(float Intensity)
{
    // Reward response: Dopamine spike, some serotonin, endorphins
    ModifyNeurochemical(ENeurochemicalType::Dopamine, 0.3f * Intensity);
    ModifyNeurochemical(ENeurochemicalType::Serotonin, 0.2f * Intensity);
    ModifyNeurochemical(ENeurochemicalType::Endorphins, 0.15f * Intensity);
    
    UE_LOG(LogTemp, Log, TEXT("Reward response triggered with intensity %.2f"), Intensity);
}

void UNeurochemicalSimulationComponent::TriggerStressResponse(float Intensity)
{
    // Stress response: Cortisol and norepinephrine increase, serotonin decreases
    ModifyNeurochemical(ENeurochemicalType::Cortisol, 0.4f * Intensity);
    ModifyNeurochemical(ENeurochemicalType::Norepinephrine, 0.3f * Intensity);
    ModifyNeurochemical(ENeurochemicalType::Serotonin, -0.2f * Intensity);
    ModifyNeurochemical(ENeurochemicalType::GABA, -0.15f * Intensity);
    
    UE_LOG(LogTemp, Log, TEXT("Stress response triggered with intensity %.2f"), Intensity);
}

void UNeurochemicalSimulationComponent::TriggerSocialBonding(float Intensity)
{
    // Social bonding: Oxytocin spike, dopamine increase, some serotonin
    ModifyNeurochemical(ENeurochemicalType::Oxytocin, 0.4f * Intensity);
    ModifyNeurochemical(ENeurochemicalType::Dopamine, 0.2f * Intensity);
    ModifyNeurochemical(ENeurochemicalType::Serotonin, 0.25f * Intensity);
    ModifyNeurochemical(ENeurochemicalType::Endorphins, 0.15f * Intensity);
    
    UE_LOG(LogTemp, Log, TEXT("Social bonding triggered with intensity %.2f"), Intensity);
}

void UNeurochemicalSimulationComponent::TriggerRelaxationResponse(float Intensity)
{
    // Relaxation: GABA increase, cortisol decrease, serotonin increase
    ModifyNeurochemical(ENeurochemicalType::GABA, 0.3f * Intensity);
    ModifyNeurochemical(ENeurochemicalType::Serotonin, 0.2f * Intensity);
    ModifyNeurochemical(ENeurochemicalType::Cortisol, -0.3f * Intensity);
    ModifyNeurochemical(ENeurochemicalType::Norepinephrine, -0.2f * Intensity);
    
    UE_LOG(LogTemp, Log, TEXT("Relaxation response triggered with intensity %.2f"), Intensity);
}

// ===== Behavioral Effects =====

float UNeurochemicalSimulationComponent::GetMotivationLevel() const
{
    // Motivation is primarily driven by dopamine
    return CurrentState.DopamineLevel;
}

float UNeurochemicalSimulationComponent::GetEnergyLevel() const
{
    // Energy is driven by norepinephrine and glutamate, reduced by GABA
    float Energy = (CurrentState.NorepinephrineLevel + CurrentState.GlutamateLevel) * 0.5f;
    Energy -= CurrentState.GABALevel * 0.3f;
    return FMath::Clamp(Energy, 0.0f, 1.0f);
}

float UNeurochemicalSimulationComponent::GetSocialDesire() const
{
    // Social desire is driven by oxytocin and serotonin
    return (CurrentState.OxytocinLevel + CurrentState.SerotoninLevel) * 0.5f;
}

float UNeurochemicalSimulationComponent::GetCognitiveClarity() const
{
    // Cognitive clarity is driven by acetylcholine and glutamate, reduced by cortisol
    float Clarity = (CurrentState.AcetylcholineLevel + CurrentState.GlutamateLevel) * 0.5f;
    Clarity -= CurrentState.CortisolLevel * 0.3f;
    return FMath::Clamp(Clarity, 0.0f, 1.0f);
}

float UNeurochemicalSimulationComponent::GetEmotionalStability() const
{
    // Emotional stability is driven by serotonin and GABA, reduced by cortisol and norepinephrine
    float Stability = (CurrentState.SerotoninLevel + CurrentState.GABALevel) * 0.5f;
    Stability -= (CurrentState.CortisolLevel + CurrentState.NorepinephrineLevel) * 0.25f;
    return FMath::Clamp(Stability, 0.0f, 1.0f);
}

// ===== Homeostasis =====

void UNeurochemicalSimulationComponent::EnableHomeostasis(bool bEnable)
{
    bEnableHomeostasis = bEnable;
    UE_LOG(LogTemp, Log, TEXT("Homeostasis %s"), bEnable ? TEXT("enabled") : TEXT("disabled"));
}

void UNeurochemicalSimulationComponent::SetHomeostasisRate(float Rate)
{
    HomeostasisRate = FMath::Clamp(Rate, 0.0f, 1.0f);
}

void UNeurochemicalSimulationComponent::ResetToBaseline()
{
    CurrentState = BaselineState;
    UE_LOG(LogTemp, Log, TEXT("Neurochemical state reset to baseline"));
}

// ===== Interactions =====

void UNeurochemicalSimulationComponent::SimulateExercise(float Intensity, float Duration)
{
    // Exercise increases endorphins, dopamine, norepinephrine
    // Decreases cortisol over time
    float EffectMultiplier = Intensity * Duration;
    
    ModifyNeurochemical(ENeurochemicalType::Endorphins, 0.3f * EffectMultiplier);
    ModifyNeurochemical(ENeurochemicalType::Dopamine, 0.2f * EffectMultiplier);
    ModifyNeurochemical(ENeurochemicalType::Norepinephrine, 0.25f * EffectMultiplier);
    ModifyNeurochemical(ENeurochemicalType::Cortisol, -0.15f * EffectMultiplier);
    
    UE_LOG(LogTemp, Log, TEXT("Exercise simulated: Intensity %.2f, Duration %.2f"), 
           Intensity, Duration);
}

void UNeurochemicalSimulationComponent::SimulateSocialInteraction(float PositivityLevel, float Duration)
{
    // Positive social interaction increases oxytocin, dopamine, serotonin
    // Negative interaction increases cortisol, decreases serotonin
    float EffectMultiplier = Duration;
    
    if (PositivityLevel > 0.5f)
    {
        float PositiveIntensity = (PositivityLevel - 0.5f) * 2.0f;
        ModifyNeurochemical(ENeurochemicalType::Oxytocin, 0.3f * PositiveIntensity * EffectMultiplier);
        ModifyNeurochemical(ENeurochemicalType::Dopamine, 0.2f * PositiveIntensity * EffectMultiplier);
        ModifyNeurochemical(ENeurochemicalType::Serotonin, 0.25f * PositiveIntensity * EffectMultiplier);
    }
    else
    {
        float NegativeIntensity = (0.5f - PositivityLevel) * 2.0f;
        ModifyNeurochemical(ENeurochemicalType::Cortisol, 0.3f * NegativeIntensity * EffectMultiplier);
        ModifyNeurochemical(ENeurochemicalType::Serotonin, -0.2f * NegativeIntensity * EffectMultiplier);
    }
    
    UE_LOG(LogTemp, Log, TEXT("Social interaction simulated: Positivity %.2f, Duration %.2f"), 
           PositivityLevel, Duration);
}

void UNeurochemicalSimulationComponent::SimulateCognitiveTask(float Difficulty, float Duration)
{
    // Cognitive tasks increase acetylcholine, glutamate
    // High difficulty increases cortisol
    float EffectMultiplier = Duration;
    
    ModifyNeurochemical(ENeurochemicalType::Acetylcholine, 0.3f * EffectMultiplier);
    ModifyNeurochemical(ENeurochemicalType::Glutamate, 0.25f * EffectMultiplier);
    ModifyNeurochemical(ENeurochemicalType::Norepinephrine, 0.2f * EffectMultiplier);
    
    if (Difficulty > 0.7f)
    {
        ModifyNeurochemical(ENeurochemicalType::Cortisol, 0.2f * Difficulty * EffectMultiplier);
    }
    
    UE_LOG(LogTemp, Log, TEXT("Cognitive task simulated: Difficulty %.2f, Duration %.2f"), 
           Difficulty, Duration);
}

void UNeurochemicalSimulationComponent::SimulateRestAndRecovery(float Duration)
{
    // Rest increases GABA, serotonin, decreases cortisol
    float EffectMultiplier = Duration;
    
    ModifyNeurochemical(ENeurochemicalType::GABA, 0.3f * EffectMultiplier);
    ModifyNeurochemical(ENeurochemicalType::Serotonin, 0.2f * EffectMultiplier);
    ModifyNeurochemical(ENeurochemicalType::Cortisol, -0.25f * EffectMultiplier);
    ModifyNeurochemical(ENeurochemicalType::Norepinephrine, -0.2f * EffectMultiplier);
    
    UE_LOG(LogTemp, Log, TEXT("Rest and recovery simulated: Duration %.2f"), Duration);
}

// ===== Internal Methods =====

void UNeurochemicalSimulationComponent::UpdateNeurochemicalDynamics(float DeltaTime)
{
    // Apply decay and production to all neurochemicals
    for (auto& DecayPair : DecayRates)
    {
        ENeurochemicalType Type = DecayPair.Key;
        float CurrentLevel = GetNeurochemicalLevel(Type);
        
        // Apply decay
        float Decay = CalculateNeurochemicalDecay(Type, DeltaTime);
        
        // Apply production
        float Production = CalculateNeurochemicalProduction(Type);
        
        // Update level
        float NewLevel = CurrentLevel - Decay + Production;
        SetNeurochemicalLevel(Type, NewLevel);
    }
}

void UNeurochemicalSimulationComponent::ApplyHomeostasis(float DeltaTime)
{
    // Gradually return all neurochemicals toward baseline
    float HomeostasisFactor = HomeostasisRate * DeltaTime;
    
    CurrentState.DopamineLevel = FMath::Lerp(CurrentState.DopamineLevel, BaselineState.DopamineLevel, HomeostasisFactor);
    CurrentState.SerotoninLevel = FMath::Lerp(CurrentState.SerotoninLevel, BaselineState.SerotoninLevel, HomeostasisFactor);
    CurrentState.NorepinephrineLevel = FMath::Lerp(CurrentState.NorepinephrineLevel, BaselineState.NorepinephrineLevel, HomeostasisFactor);
    CurrentState.OxytocinLevel = FMath::Lerp(CurrentState.OxytocinLevel, BaselineState.OxytocinLevel, HomeostasisFactor);
    CurrentState.CortisolLevel = FMath::Lerp(CurrentState.CortisolLevel, BaselineState.CortisolLevel, HomeostasisFactor);
    CurrentState.EndorphinsLevel = FMath::Lerp(CurrentState.EndorphinsLevel, BaselineState.EndorphinsLevel, HomeostasisFactor);
    CurrentState.GABALevel = FMath::Lerp(CurrentState.GABALevel, BaselineState.GABALevel, HomeostasisFactor);
    CurrentState.GlutamateLevel = FMath::Lerp(CurrentState.GlutamateLevel, BaselineState.GlutamateLevel, HomeostasisFactor);
    CurrentState.AcetylcholineLevel = FMath::Lerp(CurrentState.AcetylcholineLevel, BaselineState.AcetylcholineLevel, HomeostasisFactor);
}

void UNeurochemicalSimulationComponent::ProcessNeurochemicalInteractions()
{
    // Model interactions between neurochemicals
    
    // High cortisol inhibits serotonin production
    if (CurrentState.CortisolLevel > 0.7f)
    {
        ModifyNeurochemical(ENeurochemicalType::Serotonin, -0.01f * InteractionStrength);
    }
    
    // High dopamine can increase norepinephrine
    if (CurrentState.DopamineLevel > 0.7f)
    {
        ModifyNeurochemical(ENeurochemicalType::Norepinephrine, 0.01f * InteractionStrength);
    }
    
    // GABA inhibits glutamate (and vice versa)
    float GABAGlutamateBalance = CurrentState.GABALevel - CurrentState.GlutamateLevel;
    if (FMath::Abs(GABAGlutamateBalance) > 0.3f)
    {
        ModifyNeurochemical(ENeurochemicalType::Glutamate, -GABAGlutamateBalance * 0.05f * InteractionStrength);
    }
}

void UNeurochemicalSimulationComponent::UpdateEmotionalState()
{
    // Calculate emotional states from neurochemical levels
    CurrentEmotionalChemistry.Happiness = (CurrentState.SerotoninLevel + CurrentState.DopamineLevel) * 0.5f;
    CurrentEmotionalChemistry.Excitement = (CurrentState.DopamineLevel + CurrentState.NorepinephrineLevel) * 0.5f;
    CurrentEmotionalChemistry.Calmness = (CurrentState.GABALevel + CurrentState.SerotoninLevel) * 0.5f;
    CurrentEmotionalChemistry.Anxiety = (CurrentState.CortisolLevel + CurrentState.NorepinephrineLevel) * 0.5f;
    CurrentEmotionalChemistry.Affection = (CurrentState.OxytocinLevel + CurrentState.EndorphinsLevel) * 0.5f;
    CurrentEmotionalChemistry.Focus = (CurrentState.AcetylcholineLevel + CurrentState.GlutamateLevel) * 0.5f;
}

void UNeurochemicalSimulationComponent::ApplyNeurochemistryToAvatar()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    // ========================================
    // FACIAL ANIMATION INTEGRATION
    // ========================================
    if (UFacialAnimationSystem* FacialSystem = Owner->FindComponentByClass<UFacialAnimationSystem>())
    {
        // Determine dominant emotional expression from neurochemistry
        // Happiness -> Happy expression
        if (CurrentEmotionalChemistry.Happiness > 0.7f)
        {
            FacialSystem->TransitionToExpression(TEXT("Happy"),
                FMath::Lerp(0.5f, 1.0f, (CurrentEmotionalChemistry.Happiness - 0.7f) / 0.3f));
        }
        // Anxiety -> Nervous/Worried expression
        else if (CurrentEmotionalChemistry.Anxiety > 0.6f)
        {
            FacialSystem->TransitionToExpression(TEXT("Nervous"),
                FMath::Lerp(0.5f, 1.0f, (CurrentEmotionalChemistry.Anxiety - 0.6f) / 0.4f));
        }
        // Calmness -> Peaceful/Serene expression
        else if (CurrentEmotionalChemistry.Calmness > 0.7f)
        {
            FacialSystem->TransitionToExpression(TEXT("Peaceful"),
                FMath::Lerp(0.5f, 1.0f, (CurrentEmotionalChemistry.Calmness - 0.7f) / 0.3f));
        }
        // Excitement -> Excited expression
        else if (CurrentEmotionalChemistry.Excitement > 0.7f)
        {
            FacialSystem->TransitionToExpression(TEXT("Excited"),
                FMath::Lerp(0.5f, 1.0f, (CurrentEmotionalChemistry.Excitement - 0.7f) / 0.3f));
        }

        // Modulate blink rate based on norepinephrine (alertness)
        float BlinkInterval = FMath::Lerp(5.0f, 2.0f, CurrentState.NorepinephrineLevel);
        FacialSystem->SetBlinkInterval(BlinkInterval);

        // Modulate eye openness based on cortisol (stress = wider eyes)
        float EyeOpenness = FMath::Lerp(0.8f, 1.0f, CurrentState.CortisolLevel);
        FacialSystem->SetEyeOpenness(EyeOpenness);
    }

    // ========================================
    // GESTURE SYSTEM INTEGRATION
    // ========================================
    if (UGestureSystem* GestureSystem = Owner->FindComponentByClass<UGestureSystem>())
    {
        // Modulate gesture frequency based on dopamine (motivation)
        float GestureFrequency = FMath::Lerp(0.3f, 1.0f, CurrentState.DopamineLevel);
        GestureSystem->SetGestureFrequency(GestureFrequency);

        // Set gesture energy based on norepinephrine
        float GestureEnergy = FMath::Lerp(0.4f, 1.0f, CurrentState.NorepinephrineLevel);
        GestureSystem->SetGestureEnergy(GestureEnergy);

        // High anxiety triggers fidgeting
        if (CurrentEmotionalChemistry.Anxiety > 0.6f)
        {
            GestureSystem->EnableFidgeting(true);
            GestureSystem->SetFidgetIntensity(CurrentEmotionalChemistry.Anxiety);
        }
        else
        {
            GestureSystem->EnableFidgeting(false);
        }

        // High oxytocin enables open/welcoming gestures
        if (CurrentState.OxytocinLevel > 0.6f)
        {
            GestureSystem->SetGestureStyle(TEXT("Open"));
        }
        else if (CurrentState.CortisolLevel > 0.6f)
        {
            GestureSystem->SetGestureStyle(TEXT("Defensive"));
        }
        else
        {
            GestureSystem->SetGestureStyle(TEXT("Neutral"));
        }
    }

    // ========================================
    // EMOTIONAL AURA INTEGRATION
    // ========================================
    if (UEmotionalAuraComponent* AuraComponent = Owner->FindComponentByClass<UEmotionalAuraComponent>())
    {
        // Set aura color based on dominant emotion
        FLinearColor AuraColor;

        if (CurrentEmotionalChemistry.Happiness > CurrentEmotionalChemistry.Anxiety &&
            CurrentEmotionalChemistry.Happiness > CurrentEmotionalChemistry.Calmness)
        {
            // Golden/warm aura for happiness
            AuraColor = FLinearColor(1.0f, 0.85f, 0.3f, CurrentEmotionalChemistry.Happiness);
        }
        else if (CurrentEmotionalChemistry.Anxiety > CurrentEmotionalChemistry.Calmness)
        {
            // Red/orange aura for anxiety
            AuraColor = FLinearColor(1.0f, 0.3f, 0.2f, CurrentEmotionalChemistry.Anxiety);
        }
        else if (CurrentEmotionalChemistry.Calmness > 0.6f)
        {
            // Blue/cyan aura for calmness
            AuraColor = FLinearColor(0.3f, 0.7f, 1.0f, CurrentEmotionalChemistry.Calmness);
        }
        else if (CurrentEmotionalChemistry.Affection > 0.6f)
        {
            // Pink aura for affection
            AuraColor = FLinearColor(1.0f, 0.5f, 0.7f, CurrentEmotionalChemistry.Affection);
        }
        else
        {
            // Default neutral white aura
            AuraColor = FLinearColor(0.8f, 0.8f, 0.85f, 0.5f);
        }

        AuraComponent->SetAuraColor(AuraColor);

        // Aura intensity based on emotional intensity
        float EmotionalIntensity = FMath::Max(
            FMath::Max(CurrentEmotionalChemistry.Happiness, CurrentEmotionalChemistry.Excitement),
            FMath::Max(CurrentEmotionalChemistry.Anxiety, CurrentEmotionalChemistry.Affection));
        AuraComponent->SetAuraIntensity(EmotionalIntensity);

        // Aura pulse rate based on arousal (norepinephrine + dopamine)
        float ArousalLevel = (CurrentState.NorepinephrineLevel + CurrentState.DopamineLevel) * 0.5f;
        float PulseRate = FMath::Lerp(0.5f, 2.0f, ArousalLevel);
        AuraComponent->SetPulseRate(PulseRate);
    }

    // ========================================
    // PERSONALITY MODULATION
    // ========================================
    if (UPersonalityTraitSystem* PersonalitySystem = Owner->FindComponentByClass<UPersonalityTraitSystem>())
    {
        // High dopamine boosts confidence
        if (CurrentState.DopamineLevel > 0.7f)
        {
            PersonalitySystem->ModifyTrait(EPersonalityTraitType::Confident,
                (CurrentState.DopamineLevel - 0.7f) * 0.1f);
        }

        // High serotonin boosts playfulness
        if (CurrentState.SerotoninLevel > 0.7f)
        {
            PersonalitySystem->ModifyTrait(EPersonalityTraitType::Playful,
                (CurrentState.SerotoninLevel - 0.7f) * 0.1f);
        }

        // High cortisol can trigger hyper-chaotic behavior
        if (CurrentState.CortisolLevel > 0.7f && CurrentState.NorepinephrineLevel > 0.6f)
        {
            PersonalitySystem->ModifyTrait(EPersonalityTraitType::HyperChaotic,
                (CurrentState.CortisolLevel - 0.5f) * 0.15f);
        }
    }

    // ========================================
    // ECHO PULSE RESONANCE
    // ========================================
    if (UEchoPulseResonanceChamber* ResonanceChamber = Owner->FindComponentByClass<UEchoPulseResonanceChamber>())
    {
        // Feed emotional intensity into resonance chamber
        float EmotionalSignal = (CurrentEmotionalChemistry.Happiness +
                                 CurrentEmotionalChemistry.Excitement +
                                 CurrentEmotionalChemistry.Affection) / 3.0f;

        // Scale to resonance chamber range (0-100)
        float ResonanceInput = EmotionalSignal * 10.0f;
        ResonanceChamber->UpdateResonance(ResonanceInput);

        // Process decay
        ResonanceChamber->ProcessDecay(GetWorld()->GetDeltaSeconds());
    }

    // ========================================
    // DEBUG LOGGING
    // ========================================
    if (CurrentEmotionalChemistry.Happiness > 0.8f)
    {
        UE_LOG(LogTemp, Verbose, TEXT("NeurochemicalSimulation: High happiness (%.2f) -> Avatar updated"),
               CurrentEmotionalChemistry.Happiness);
    }

    if (CurrentEmotionalChemistry.Anxiety > 0.7f)
    {
        UE_LOG(LogTemp, Verbose, TEXT("NeurochemicalSimulation: High anxiety (%.2f) -> Avatar updated"),
               CurrentEmotionalChemistry.Anxiety);
    }

    if (CurrentEmotionalChemistry.Excitement > 0.8f)
    {
        UE_LOG(LogTemp, Verbose, TEXT("NeurochemicalSimulation: High excitement (%.2f) -> Avatar updated"),
               CurrentEmotionalChemistry.Excitement);
    }
}

float UNeurochemicalSimulationComponent::CalculateNeurochemicalDecay(ENeurochemicalType Type, float DeltaTime)
{
    float DecayRate = DecayRates.Contains(Type) ? DecayRates[Type] : NeurochemicalDecayRate;
    float CurrentLevel = GetNeurochemicalLevel(Type);
    
    // Decay is proportional to current level (first-order kinetics)
    return CurrentLevel * DecayRate * DeltaTime;
}

float UNeurochemicalSimulationComponent::CalculateNeurochemicalProduction(ENeurochemicalType Type)
{
    float ProductionRate = ProductionRates.Contains(Type) ? ProductionRates[Type] : 0.02f;
    
    // Production can be modulated by other factors
    // For now, return baseline production
    return ProductionRate * GetWorld()->GetDeltaSeconds();
}

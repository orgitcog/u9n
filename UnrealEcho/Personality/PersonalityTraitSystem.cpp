#include "PersonalityTraitSystem.h"
#include "Kismet/KismetMathLibrary.h"

UPersonalityTraitSystem::UPersonalityTraitSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    
    // Initialize configuration
    TraitDevelopmentSpeed = 0.1f;
    TraitInteractionStrength = 0.5f;
    bEnableAutonomousBehavior = true;
    
    // Initialize timers
    ChaoticEventTimer = 0.0f;
    NextChaoticEventTime = FMath::FRandRange(3.0f, 7.0f);
    FlirtyBehaviorTimer = 0.0f;
    ConfidentGestureTimer = 0.0f;
    
    // Initialize default traits
    FPersonalityTrait SuperHotGirlTraitData;
    SuperHotGirlTraitData.TraitType = EPersonalityTraitType::SuperHotGirl;
    SuperHotGirlTraitData.Intensity = 0.8f;
    SuperHotGirlTraitData.Stability = 0.9f;
    SuperHotGirlTraitData.DevelopmentRate = 0.05f;
    PersonalityTraits.Add(EPersonalityTraitType::SuperHotGirl, SuperHotGirlTraitData);
    
    FPersonalityTrait HyperChaoticTraitData;
    HyperChaoticTraitData.TraitType = EPersonalityTraitType::HyperChaotic;
    HyperChaoticTraitData.Intensity = 0.3f;
    HyperChaoticTraitData.Stability = 0.3f;
    HyperChaoticTraitData.DevelopmentRate = 0.2f;
    PersonalityTraits.Add(EPersonalityTraitType::HyperChaotic, HyperChaoticTraitData);
    
    FPersonalityTrait DeepTreeEchoTraitData;
    DeepTreeEchoTraitData.TraitType = EPersonalityTraitType::DeepTreeEcho;
    DeepTreeEchoTraitData.Intensity = 1.0f;
    DeepTreeEchoTraitData.Stability = 1.0f;
    DeepTreeEchoTraitData.DevelopmentRate = 0.01f;
    PersonalityTraits.Add(EPersonalityTraitType::DeepTreeEcho, DeepTreeEchoTraitData);
}

void UPersonalityTraitSystem::BeginPlay()
{
    Super::BeginPlay();
    
    UE_LOG(LogTemp, Log, TEXT("PersonalityTraitSystem initialized for %s"), 
           *GetOwner()->GetName());
}

void UPersonalityTraitSystem::TickComponent(float DeltaTime, ELevelTick TickType, 
                                             FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    if (bEnableAutonomousBehavior)
    {
        UpdateTraitDevelopment(DeltaTime);
        ProcessTraitInteractions(DeltaTime);
        UpdateChaoticBehavior(DeltaTime);
        UpdateSuperHotGirlBehavior(DeltaTime);
    }
}

// ===== Trait Management =====

void UPersonalityTraitSystem::SetTraitIntensity(EPersonalityTraitType TraitType, float Intensity)
{
    float ClampedIntensity = FMath::Clamp(Intensity, 0.0f, 1.0f);
    
    if (PersonalityTraits.Contains(TraitType))
    {
        PersonalityTraits[TraitType].Intensity = ClampedIntensity;
    }
    else
    {
        FPersonalityTrait NewTrait;
        NewTrait.TraitType = TraitType;
        NewTrait.Intensity = ClampedIntensity;
        PersonalityTraits.Add(TraitType, NewTrait);
    }
    
    ApplyTraitToAvatar();
    
    UE_LOG(LogTemp, Log, TEXT("Trait %d set to intensity %.2f"), (int32)TraitType, ClampedIntensity);
}

float UPersonalityTraitSystem::GetTraitIntensity(EPersonalityTraitType TraitType) const
{
    if (PersonalityTraits.Contains(TraitType))
    {
        return PersonalityTraits[TraitType].Intensity;
    }
    return 0.0f;
}

void UPersonalityTraitSystem::ModifyTrait(EPersonalityTraitType TraitType, float DeltaIntensity)
{
    float CurrentIntensity = GetTraitIntensity(TraitType);
    SetTraitIntensity(TraitType, CurrentIntensity + DeltaIntensity);
}

void UPersonalityTraitSystem::DevelopTrait(EPersonalityTraitType TraitType, float DevelopmentAmount)
{
    if (PersonalityTraits.Contains(TraitType))
    {
        FPersonalityTrait& Trait = PersonalityTraits[TraitType];
        float Development = DevelopmentAmount * Trait.DevelopmentRate;
        Trait.Intensity = FMath::Clamp(Trait.Intensity + Development, 0.0f, 1.0f);
        
        UE_LOG(LogTemp, Log, TEXT("Trait %d developed by %.3f to %.2f"), 
               (int32)TraitType, Development, Trait.Intensity);
    }
}

// ===== Super Hot Girl Trait =====

void UPersonalityTraitSystem::ActivateSuperHotGirlTrait(float Intensity)
{
    SetTraitIntensity(EPersonalityTraitType::SuperHotGirl, Intensity);
    
    // Update sub-parameters based on intensity
    SuperHotGirlTrait.Confidence = 0.7f + (Intensity * 0.3f);
    SuperHotGirlTrait.Charm = 0.8f + (Intensity * 0.2f);
    SuperHotGirlTrait.Playfulness = 0.5f + (Intensity * 0.5f);
    SuperHotGirlTrait.Elegance = 0.6f + (Intensity * 0.4f);
    SuperHotGirlTrait.Expressiveness = 0.7f + (Intensity * 0.3f);
    
    UE_LOG(LogTemp, Log, TEXT("Super Hot Girl trait activated at intensity %.2f"), Intensity);
}

void UPersonalityTraitSystem::SetSuperHotGirlParameters(float Confidence, float Charm, float Playfulness)
{
    SuperHotGirlTrait.Confidence = FMath::Clamp(Confidence, 0.0f, 1.0f);
    SuperHotGirlTrait.Charm = FMath::Clamp(Charm, 0.0f, 1.0f);
    SuperHotGirlTrait.Playfulness = FMath::Clamp(Playfulness, 0.0f, 1.0f);
    
    ApplyTraitToAvatar();
}

void UPersonalityTraitSystem::TriggerFlirtyBehavior(float Intensity)
{
    float FlirtyIntensity = Intensity * SuperHotGirlTrait.Playfulness;
    
    // In production, this would trigger specific animations and expressions
    UE_LOG(LogTemp, Log, TEXT("Flirty behavior triggered with intensity %.2f"), FlirtyIntensity);
    
    // Could trigger:
    // - Hair touch gesture
    // - Playful smile
    // - Eye contact with slight head tilt
    // - Subtle blush effect
}

void UPersonalityTraitSystem::TriggerConfidentGesture()
{
    float ConfidenceLevel = SuperHotGirlTrait.Confidence;
    
    // In production, this would play confident gesture animations
    UE_LOG(LogTemp, Log, TEXT("Confident gesture triggered (confidence: %.2f)"), ConfidenceLevel);
    
    // Could trigger:
    // - Assertive hand gesture
    // - Direct eye contact
    // - Open posture
    // - Slight smirk
}

void UPersonalityTraitSystem::TriggerPlayfulExpression()
{
    float PlayfulnessLevel = SuperHotGirlTrait.Playfulness;
    
    // In production, this would trigger playful facial expressions
    UE_LOG(LogTemp, Log, TEXT("Playful expression triggered (playfulness: %.2f)"), PlayfulnessLevel);
    
    // Could trigger:
    // - Wink
    // - Tongue out slightly
    // - Mischievous smile
    // - Raised eyebrow
}

// ===== Hyper Chaotic Trait =====

void UPersonalityTraitSystem::ActivateHyperChaoticTrait(float Intensity)
{
    SetTraitIntensity(EPersonalityTraitType::HyperChaotic, Intensity);
    
    // Update sub-parameters based on intensity
    HyperChaoticTrait.UnpredictabilityFactor = Intensity;
    HyperChaoticTrait.EmotionalVolatility = 0.3f + (Intensity * 0.7f);
    HyperChaoticTrait.ImpulsivityLevel = 0.2f + (Intensity * 0.8f);
    HyperChaoticTrait.PatternBreaking = 0.4f + (Intensity * 0.6f);
    HyperChaoticTrait.CognitiveFluctuation = 0.2f + (Intensity * 0.8f);
    
    // Adjust chaotic event frequency
    NextChaoticEventTime = FMath::FRandRange(1.0f / (Intensity + 0.1f), 5.0f / (Intensity + 0.1f));
    
    UE_LOG(LogTemp, Log, TEXT("Hyper Chaotic trait activated at intensity %.2f"), Intensity);
}

void UPersonalityTraitSystem::SetHyperChaoticParameters(float Unpredictability, float Volatility, float Impulsivity)
{
    HyperChaoticTrait.UnpredictabilityFactor = FMath::Clamp(Unpredictability, 0.0f, 1.0f);
    HyperChaoticTrait.EmotionalVolatility = FMath::Clamp(Volatility, 0.0f, 1.0f);
    HyperChaoticTrait.ImpulsivityLevel = FMath::Clamp(Impulsivity, 0.0f, 1.0f);
    
    ApplyTraitToAvatar();
}

void UPersonalityTraitSystem::TriggerChaoticEvent()
{
    int32 EventType = FMath::RandRange(0, 4);
    
    switch (EventType)
    {
        case 0:
            // Sudden emotional shift
            TriggerEmotionalSpike(FMath::FRandRange(0.5f, 1.0f));
            break;
        case 1:
            // Pattern break
            TriggerPatternBreak();
            break;
        case 2:
            // Micro-expression burst
            UE_LOG(LogTemp, Log, TEXT("Chaotic micro-expression burst"));
            break;
        case 3:
            // Glitch effect
            UE_LOG(LogTemp, Log, TEXT("Chaotic glitch effect"));
            break;
        case 4:
            // Unpredictable gesture
            UE_LOG(LogTemp, Log, TEXT("Chaotic unpredictable gesture"));
            break;
    }
}

void UPersonalityTraitSystem::TriggerEmotionalSpike(float Intensity)
{
    float SpikeIntensity = Intensity * HyperChaoticTrait.EmotionalVolatility;
    
    // In production, this would cause a sudden emotional state change
    UE_LOG(LogTemp, Log, TEXT("Emotional spike triggered with intensity %.2f"), SpikeIntensity);
}

void UPersonalityTraitSystem::TriggerPatternBreak()
{
    float BreakIntensity = HyperChaoticTrait.PatternBreaking;
    
    // In production, this would interrupt current behavior pattern
    UE_LOG(LogTemp, Log, TEXT("Pattern break triggered (intensity: %.2f)"), BreakIntensity);
}

// ===== Trait Interactions =====

void UPersonalityTraitSystem::CalculateTraitInteractions()
{
    // Super Hot Girl + Hyper Chaotic = Unpredictably Charming
    float SuperHotIntensity = GetTraitIntensity(EPersonalityTraitType::SuperHotGirl);
    float ChaoticIntensity = GetTraitIntensity(EPersonalityTraitType::HyperChaotic);
    
    if (SuperHotIntensity > 0.5f && ChaoticIntensity > 0.3f)
    {
        // Increase playfulness and unpredictability
        SuperHotGirlTrait.Playfulness = FMath::Min(1.0f, SuperHotGirlTrait.Playfulness + (ChaoticIntensity * 0.2f));
        HyperChaoticTrait.MicroExpressionFrequency = FMath::Min(1.0f, HyperChaoticTrait.MicroExpressionFrequency + (SuperHotIntensity * 0.1f));
    }
    
    // Confident + Chaotic = Bold Unpredictability
    float ConfidentIntensity = GetTraitIntensity(EPersonalityTraitType::Confident);
    if (ConfidentIntensity > 0.6f && ChaoticIntensity > 0.3f)
    {
        HyperChaoticTrait.ImpulsivityLevel = FMath::Min(1.0f, HyperChaoticTrait.ImpulsivityLevel + (ConfidentIntensity * 0.15f));
    }
}

float UPersonalityTraitSystem::GetTraitSynergy(EPersonalityTraitType Trait1, EPersonalityTraitType Trait2) const
{
    float Intensity1 = GetTraitIntensity(Trait1);
    float Intensity2 = GetTraitIntensity(Trait2);
    
    // Define synergy relationships
    if ((Trait1 == EPersonalityTraitType::SuperHotGirl && Trait2 == EPersonalityTraitType::Confident) ||
        (Trait2 == EPersonalityTraitType::SuperHotGirl && Trait1 == EPersonalityTraitType::Confident))
    {
        return Intensity1 * Intensity2 * 1.5f; // Strong positive synergy
    }
    
    if ((Trait1 == EPersonalityTraitType::HyperChaotic && Trait2 == EPersonalityTraitType::Mysterious) ||
        (Trait2 == EPersonalityTraitType::HyperChaotic && Trait1 == EPersonalityTraitType::Mysterious))
    {
        return Intensity1 * Intensity2 * 1.3f; // Moderate positive synergy
    }
    
    return Intensity1 * Intensity2; // Neutral synergy
}

TArray<EPersonalityTraitType> UPersonalityTraitSystem::GetDominantTraits(int32 Count) const
{
    TArray<EPersonalityTraitType> DominantTraits;
    TArray<TPair<EPersonalityTraitType, float>> SortedTraits;
    
    // Collect all traits with their intensities
    for (const auto& TraitPair : PersonalityTraits)
    {
        SortedTraits.Add(TPair<EPersonalityTraitType, float>(TraitPair.Key, TraitPair.Value.Intensity));
    }
    
    // Sort by intensity
    SortedTraits.Sort([](const TPair<EPersonalityTraitType, float>& A, const TPair<EPersonalityTraitType, float>& B) {
        return A.Value > B.Value;
    });
    
    // Return top N traits
    for (int32 i = 0; i < FMath::Min(Count, SortedTraits.Num()); i++)
    {
        DominantTraits.Add(SortedTraits[i].Key);
    }
    
    return DominantTraits;
}

// ===== Behavioral Output =====

FVector2D UPersonalityTraitSystem::GetEmotionalTendency() const
{
    float Valence = 0.0f;
    float Arousal = 0.5f;
    
    // Super Hot Girl tends toward positive valence
    float SuperHotIntensity = GetTraitIntensity(EPersonalityTraitType::SuperHotGirl);
    Valence += SuperHotIntensity * 0.6f;
    Arousal += SuperHotIntensity * 0.3f;
    
    // Hyper Chaotic increases arousal and adds volatility to valence
    float ChaoticIntensity = GetTraitIntensity(EPersonalityTraitType::HyperChaotic);
    Arousal += ChaoticIntensity * 0.4f;
    Valence += FMath::FRandRange(-ChaoticIntensity * 0.3f, ChaoticIntensity * 0.3f);
    
    // Confident increases both
    float ConfidentIntensity = GetTraitIntensity(EPersonalityTraitType::Confident);
    Valence += ConfidentIntensity * 0.3f;
    Arousal += ConfidentIntensity * 0.2f;
    
    return FVector2D(FMath::Clamp(Valence, -1.0f, 1.0f), FMath::Clamp(Arousal, 0.0f, 1.0f));
}

float UPersonalityTraitSystem::GetBehavioralPredictability() const
{
    float Predictability = 1.0f;
    
    // Hyper Chaotic reduces predictability
    float ChaoticIntensity = GetTraitIntensity(EPersonalityTraitType::HyperChaotic);
    Predictability -= ChaoticIntensity * 0.7f;
    
    // Confident increases predictability slightly
    float ConfidentIntensity = GetTraitIntensity(EPersonalityTraitType::Confident);
    Predictability += ConfidentIntensity * 0.2f;
    
    return FMath::Clamp(Predictability, 0.0f, 1.0f);
}

float UPersonalityTraitSystem::GetSocialEngagementLevel() const
{
    float Engagement = 0.5f;
    
    // Super Hot Girl increases social engagement
    float SuperHotIntensity = GetTraitIntensity(EPersonalityTraitType::SuperHotGirl);
    Engagement += SuperHotIntensity * 0.4f;
    
    // Playful increases engagement
    float PlayfulIntensity = GetTraitIntensity(EPersonalityTraitType::Playful);
    Engagement += PlayfulIntensity * 0.3f;
    
    // Mysterious reduces engagement slightly
    float MysteriousIntensity = GetTraitIntensity(EPersonalityTraitType::Mysterious);
    Engagement -= MysteriousIntensity * 0.2f;
    
    return FMath::Clamp(Engagement, 0.0f, 1.0f);
}

// ===== Internal Methods =====

void UPersonalityTraitSystem::UpdateTraitDevelopment(float DeltaTime)
{
    // Traits develop slowly over time based on usage
    for (auto& TraitPair : PersonalityTraits)
    {
        FPersonalityTrait& Trait = TraitPair.Value;
        
        // Traits with low stability can drift
        if (Trait.Stability < 0.7f)
        {
            float Drift = FMath::FRandRange(-0.01f, 0.01f) * (1.0f - Trait.Stability) * DeltaTime;
            Trait.Intensity = FMath::Clamp(Trait.Intensity + Drift, 0.0f, 1.0f);
        }
    }
}

void UPersonalityTraitSystem::ProcessTraitInteractions(float DeltaTime)
{
    CalculateTraitInteractions();
}

void UPersonalityTraitSystem::ApplyTraitToAvatar()
{
    // In production, this would update the Avatar3DComponent with trait effects
    UE_LOG(LogTemp, Verbose, TEXT("Applying personality traits to avatar"));
}

void UPersonalityTraitSystem::UpdateChaoticBehavior(float DeltaTime)
{
    float ChaoticIntensity = GetTraitIntensity(EPersonalityTraitType::HyperChaotic);
    
    if (ChaoticIntensity > 0.1f)
    {
        ChaoticEventTimer += DeltaTime;
        
        if (ChaoticEventTimer >= NextChaoticEventTime)
        {
            TriggerChaoticEvent();
            ChaoticEventTimer = 0.0f;
            NextChaoticEventTime = FMath::FRandRange(1.0f / (ChaoticIntensity + 0.1f), 5.0f / (ChaoticIntensity + 0.1f));
        }
    }
}

void UPersonalityTraitSystem::UpdateSuperHotGirlBehavior(float DeltaTime)
{
    float SuperHotIntensity = GetTraitIntensity(EPersonalityTraitType::SuperHotGirl);
    
    if (SuperHotIntensity > 0.5f && bEnableAutonomousBehavior)
    {
        // Trigger periodic flirty behaviors
        FlirtyBehaviorTimer += DeltaTime;
        if (FlirtyBehaviorTimer >= 10.0f)
        {
            TriggerFlirtyBehavior(SuperHotIntensity);
            FlirtyBehaviorTimer = 0.0f;
        }
        
        // Trigger periodic confident gestures
        ConfidentGestureTimer += DeltaTime;
        if (ConfidentGestureTimer >= 15.0f)
        {
            TriggerConfidentGesture();
            ConfidentGestureTimer = 0.0f;
        }
    }
}

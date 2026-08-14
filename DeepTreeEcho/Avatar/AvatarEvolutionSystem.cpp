/**
 * Avatar Evolution System Implementation
 * 
 * Implements relentless enhancement of avatar with 4E embodied cognition.
 */

#include "AvatarEvolutionSystem.h"

UAvatarEvolutionSystem::UAvatarEvolutionSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f; // 10 Hz update
}

void UAvatarEvolutionSystem::BeginPlay()
{
    Super::BeginPlay();

    // Initialize evolution system
    InitializeEvolution();
}

void UAvatarEvolutionSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableEvolution)
    {
        return;
    }

    // Trigger evolution step
    TriggerEvolutionStep(DeltaTime);
}

void UAvatarEvolutionSystem::InitializeEvolution()
{
    // Initialize capabilities
    InitializeCapabilities();

    // Initialize morphological traits
    InitializeDefaultTraits();

    // Set stage thresholds
    StageThresholds.Add(EDevelopmentalStage::Embryonic, 0.0f);
    StageThresholds.Add(EDevelopmentalStage::Juvenile, 0.25f);
    StageThresholds.Add(EDevelopmentalStage::Adult, 0.6f);
    StageThresholds.Add(EDevelopmentalStage::Transcendent, 0.9f);

    // Start at embryonic stage
    CurrentStage = EDevelopmentalStage::Embryonic;
    TotalEvolutionTime = 0.0f;
    OverallFitness = 0.0f;

    // Record initialization event
    RecordEvolutionEvent(TEXT("SystemInitialized"), ECapabilityDomain::Cognitive, 0.0f, TEXT("BeginPlay"));
}

void UAvatarEvolutionSystem::InitializeCapabilities()
{
    // Initialize all capability domains
    TArray<ECapabilityDomain> Domains = {
        ECapabilityDomain::Motor,
        ECapabilityDomain::Sensory,
        ECapabilityDomain::Cognitive,
        ECapabilityDomain::Social,
        ECapabilityDomain::Emotional,
        ECapabilityDomain::Creative
    };

    for (ECapabilityDomain Domain : Domains)
    {
        FCapabilityLevel Level;
        Level.Domain = Domain;
        Level.Level = 0.1f; // Start with minimal capability
        Level.MaxPotential = 1.0f;
        Level.Plasticity = 0.8f; // High plasticity at start
        Level.Experience = 0.0f;
        Level.LastUpdateTime = 0.0f;

        CapabilityLevels.Add(Domain, Level);
    }
}

void UAvatarEvolutionSystem::InitializeDefaultTraits()
{
    // Motor-related traits
    AddMorphologicalTrait(TEXT("Strength"), 0.5f, {ECapabilityDomain::Motor});
    AddMorphologicalTrait(TEXT("Agility"), 0.5f, {ECapabilityDomain::Motor, ECapabilityDomain::Sensory});
    AddMorphologicalTrait(TEXT("Endurance"), 0.5f, {ECapabilityDomain::Motor});

    // Sensory traits
    AddMorphologicalTrait(TEXT("VisualAcuity"), 0.5f, {ECapabilityDomain::Sensory});
    AddMorphologicalTrait(TEXT("AuditoryAcuity"), 0.5f, {ECapabilityDomain::Sensory});
    AddMorphologicalTrait(TEXT("ProprioceptiveSensitivity"), 0.5f, {ECapabilityDomain::Sensory, ECapabilityDomain::Motor});

    // Cognitive traits
    AddMorphologicalTrait(TEXT("WorkingMemoryCapacity"), 0.5f, {ECapabilityDomain::Cognitive});
    AddMorphologicalTrait(TEXT("ProcessingSpeed"), 0.5f, {ECapabilityDomain::Cognitive});
    AddMorphologicalTrait(TEXT("AttentionalControl"), 0.5f, {ECapabilityDomain::Cognitive, ECapabilityDomain::Sensory});

    // Social traits
    AddMorphologicalTrait(TEXT("EmpathicResonance"), 0.5f, {ECapabilityDomain::Social, ECapabilityDomain::Emotional});
    AddMorphologicalTrait(TEXT("CommunicativeClarity"), 0.5f, {ECapabilityDomain::Social});

    // Emotional traits
    AddMorphologicalTrait(TEXT("EmotionalRange"), 0.5f, {ECapabilityDomain::Emotional});
    AddMorphologicalTrait(TEXT("EmotionalRegulation"), 0.5f, {ECapabilityDomain::Emotional, ECapabilityDomain::Cognitive});

    // Creative traits
    AddMorphologicalTrait(TEXT("NoveltyGeneration"), 0.5f, {ECapabilityDomain::Creative, ECapabilityDomain::Cognitive});
    AddMorphologicalTrait(TEXT("AestheticSensitivity"), 0.5f, {ECapabilityDomain::Creative, ECapabilityDomain::Sensory});
}

void UAvatarEvolutionSystem::TriggerEvolutionStep(float DeltaTime)
{
    TotalEvolutionTime += DeltaTime;

    // Update capabilities
    UpdateCapabilities(DeltaTime);

    // Update morphology
    if (bEnableMorphologicalAdaptation)
    {
        UpdateMorphology(DeltaTime);
    }

    // Decay plasticity
    DecayPlasticity(DeltaTime);

    // Check for stage transition
    CheckStageTransition();

    // Update overall fitness
    OverallFitness = EvaluateFitness();
}

void UAvatarEvolutionSystem::UpdateCapabilities(float DeltaTime)
{
    for (auto& Pair : CapabilityLevels)
    {
        FCapabilityLevel& Level = Pair.Value;

        // Convert experience to capability level
        if (Level.Experience > 0.0f)
        {
            float LevelGain = Level.Experience * Level.Plasticity * BaseEvolutionRate * DeltaTime;
            Level.Level = FMath::Min(Level.MaxPotential, Level.Level + LevelGain);
            
            // Consume experience
            Level.Experience = FMath::Max(0.0f, Level.Experience - LevelGain);
            Level.LastUpdateTime = TotalEvolutionTime;

            // Record significant changes
            if (LevelGain > 0.01f)
            {
                RecordEvolutionEvent(TEXT("CapabilityGain"), Level.Domain, LevelGain, TEXT("ExperienceConversion"));
            }
        }
    }
}

void UAvatarEvolutionSystem::UpdateMorphology(float DeltaTime)
{
    for (FMorphologicalTrait& Trait : MorphologicalTraits)
    {
        // Traits slowly drift toward optimal values based on linked capability usage
        float TargetValue = 0.5f;
        float TotalWeight = 0.0f;

        for (ECapabilityDomain Domain : Trait.LinkedCapabilities)
        {
            if (const FCapabilityLevel* Level = CapabilityLevels.Find(Domain))
            {
                TargetValue += Level->Level;
                TotalWeight += 1.0f;
            }
        }

        if (TotalWeight > 0.0f)
        {
            TargetValue /= TotalWeight;
        }

        // Adapt toward target
        float Diff = TargetValue - Trait.Value;
        Trait.Value += Diff * Trait.AdaptationRate * DeltaTime;
        Trait.Value = FMath::Clamp(Trait.Value, Trait.MinValue, Trait.MaxValue);
    }
}

void UAvatarEvolutionSystem::DecayPlasticity(float DeltaTime)
{
    for (auto& Pair : CapabilityLevels)
    {
        FCapabilityLevel& Level = Pair.Value;

        // Plasticity decays over time (critical periods)
        float MinPlasticity = 0.1f; // Never fully lose plasticity
        Level.Plasticity = FMath::Max(MinPlasticity, Level.Plasticity - PlasticityDecayRate * DeltaTime);
    }
}

void UAvatarEvolutionSystem::CheckStageTransition()
{
    float CurrentFitness = EvaluateFitness();

    EDevelopmentalStage NewStage = CurrentStage;

    // Check thresholds in reverse order (highest first)
    if (CurrentFitness >= ComputeStageThreshold(EDevelopmentalStage::Transcendent))
    {
        NewStage = EDevelopmentalStage::Transcendent;
    }
    else if (CurrentFitness >= ComputeStageThreshold(EDevelopmentalStage::Adult))
    {
        NewStage = EDevelopmentalStage::Adult;
    }
    else if (CurrentFitness >= ComputeStageThreshold(EDevelopmentalStage::Juvenile))
    {
        NewStage = EDevelopmentalStage::Juvenile;
    }
    else
    {
        NewStage = EDevelopmentalStage::Embryonic;
    }

    if (NewStage != CurrentStage)
    {
        ForceStageTransition(NewStage);
    }
}

void UAvatarEvolutionSystem::ForceStageTransition(EDevelopmentalStage NewStage)
{
    EDevelopmentalStage OldStage = CurrentStage;
    CurrentStage = NewStage;

    // Stage-specific effects
    switch (NewStage)
    {
    case EDevelopmentalStage::Juvenile:
        // Boost plasticity for rapid learning
        for (auto& Pair : CapabilityLevels)
        {
            Pair.Value.Plasticity = FMath::Min(1.0f, Pair.Value.Plasticity + 0.2f);
        }
        break;

    case EDevelopmentalStage::Adult:
        // Stabilize capabilities
        for (auto& Pair : CapabilityLevels)
        {
            Pair.Value.Plasticity *= 0.5f;
        }
        break;

    case EDevelopmentalStage::Transcendent:
        // Unlock extended potential
        for (auto& Pair : CapabilityLevels)
        {
            Pair.Value.MaxPotential = 1.5f; // Beyond normal limits
        }
        break;

    default:
        break;
    }

    // Record transition
    RecordEvolutionEvent(TEXT("StageTransition"), ECapabilityDomain::Cognitive, 
        static_cast<float>(NewStage) - static_cast<float>(OldStage),
        FString::Printf(TEXT("From %d to %d"), static_cast<int32>(OldStage), static_cast<int32>(NewStage)));
}

float UAvatarEvolutionSystem::GetCapabilityLevel(ECapabilityDomain Domain) const
{
    if (const FCapabilityLevel* Level = CapabilityLevels.Find(Domain))
    {
        return Level->Level;
    }
    return 0.0f;
}

void UAvatarEvolutionSystem::AddCapabilityExperience(ECapabilityDomain Domain, float Experience)
{
    if (FCapabilityLevel* Level = CapabilityLevels.Find(Domain))
    {
        Level->Experience += Experience;
    }
}

float UAvatarEvolutionSystem::GetCapabilityPlasticity(ECapabilityDomain Domain) const
{
    if (const FCapabilityLevel* Level = CapabilityLevels.Find(Domain))
    {
        return Level->Plasticity;
    }
    return 0.0f;
}

void UAvatarEvolutionSystem::BoostPlasticity(ECapabilityDomain Domain, float Amount)
{
    if (FCapabilityLevel* Level = CapabilityLevels.Find(Domain))
    {
        Level->Plasticity = FMath::Min(1.0f, Level->Plasticity + Amount);
        RecordEvolutionEvent(TEXT("PlasticityBoost"), Domain, Amount, TEXT("ExternalBoost"));
    }
}

void UAvatarEvolutionSystem::AddMorphologicalTrait(const FString& TraitName, float InitialValue, const TArray<ECapabilityDomain>& LinkedCapabilities)
{
    FMorphologicalTrait Trait;
    Trait.TraitName = TraitName;
    Trait.Value = InitialValue;
    Trait.MinValue = 0.0f;
    Trait.MaxValue = 1.0f;
    Trait.AdaptationRate = 0.1f;
    Trait.LinkedCapabilities = LinkedCapabilities;

    MorphologicalTraits.Add(Trait);
}

void UAvatarEvolutionSystem::AdaptTrait(const FString& TraitName, float UsageIntensity)
{
    for (FMorphologicalTrait& Trait : MorphologicalTraits)
    {
        if (Trait.TraitName == TraitName)
        {
            // Usage increases trait value (use it or lose it)
            float Change = UsageIntensity * Trait.AdaptationRate;
            Trait.Value = FMath::Clamp(Trait.Value + Change, Trait.MinValue, Trait.MaxValue);

            // Also add experience to linked capabilities
            for (ECapabilityDomain Domain : Trait.LinkedCapabilities)
            {
                AddCapabilityExperience(Domain, UsageIntensity * 0.1f);
            }
            break;
        }
    }
}

float UAvatarEvolutionSystem::GetTraitValue(const FString& TraitName) const
{
    for (const FMorphologicalTrait& Trait : MorphologicalTraits)
    {
        if (Trait.TraitName == TraitName)
        {
            return Trait.Value;
        }
    }
    return 0.0f;
}

void UAvatarEvolutionSystem::RecordNicheConstruction(const FString& NicheType, const FString& Action, float FitnessImpact)
{
    if (!bEnableNicheConstruction)
    {
        return;
    }

    // Find existing record or create new
    FNicheConstructionRecord* ExistingRecord = nullptr;
    for (FNicheConstructionRecord& Record : NicheConstructions)
    {
        if (Record.NicheType == NicheType)
        {
            ExistingRecord = &Record;
            break;
        }
    }

    if (ExistingRecord)
    {
        ExistingRecord->ConstructionActions.Add(Action);
        ExistingRecord->FitnessImprovement += FitnessImpact;
        ExistingRecord->Stability = FMath::Min(1.0f, ExistingRecord->Stability + 0.1f);
    }
    else
    {
        FNicheConstructionRecord NewRecord;
        NewRecord.RecordID = GenerateRecordID();
        NewRecord.NicheType = NicheType;
        NewRecord.ConstructionActions.Add(Action);
        NewRecord.FitnessImprovement = FitnessImpact;
        NewRecord.Stability = 0.5f;

        NicheConstructions.Add(NewRecord);
    }

    RecordEvolutionEvent(TEXT("NicheConstruction"), ECapabilityDomain::Cognitive, FitnessImpact, Action);
}

float UAvatarEvolutionSystem::GetNicheStability(const FString& NicheType) const
{
    for (const FNicheConstructionRecord& Record : NicheConstructions)
    {
        if (Record.NicheType == NicheType)
        {
            return Record.Stability;
        }
    }
    return 0.0f;
}

float UAvatarEvolutionSystem::EvaluateFitness() const
{
    float TotalFitness = 0.0f;
    int32 Count = 0;

    for (const auto& Pair : CapabilityLevels)
    {
        TotalFitness += Pair.Value.Level;
        ++Count;
    }

    if (Count > 0)
    {
        TotalFitness /= Count;
    }

    // Bonus for morphological trait development
    float TraitBonus = 0.0f;
    for (const FMorphologicalTrait& Trait : MorphologicalTraits)
    {
        TraitBonus += Trait.Value;
    }
    if (MorphologicalTraits.Num() > 0)
    {
        TraitBonus /= MorphologicalTraits.Num();
    }

    // Bonus for niche construction
    float NicheBonus = 0.0f;
    for (const FNicheConstructionRecord& Record : NicheConstructions)
    {
        NicheBonus += Record.Stability * 0.1f;
    }

    return FMath::Clamp(TotalFitness + TraitBonus * 0.2f + NicheBonus, 0.0f, 1.5f);
}

float UAvatarEvolutionSystem::GetDomainFitness(ECapabilityDomain Domain) const
{
    if (const FCapabilityLevel* Level = CapabilityLevels.Find(Domain))
    {
        return Level->Level;
    }
    return 0.0f;
}

TArray<FEvolutionEvent> UAvatarEvolutionSystem::GetEvolutionEventsForDomain(ECapabilityDomain Domain) const
{
    TArray<FEvolutionEvent> Result;
    for (const FEvolutionEvent& Event : EvolutionHistory)
    {
        if (Event.AffectedDomain == Domain)
        {
            Result.Add(Event);
        }
    }
    return Result;
}

TArray<FEvolutionEvent> UAvatarEvolutionSystem::GetRecentEvolutionEvents(int32 Count) const
{
    TArray<FEvolutionEvent> Result;
    int32 StartIndex = FMath::Max(0, EvolutionHistory.Num() - Count);
    for (int32 i = StartIndex; i < EvolutionHistory.Num(); ++i)
    {
        Result.Add(EvolutionHistory[i]);
    }
    return Result;
}

void UAvatarEvolutionSystem::RecordEvolutionEvent(const FString& EventType, ECapabilityDomain Domain, float Magnitude, const FString& Trigger)
{
    FEvolutionEvent Event;
    Event.EventID = GenerateEventID();
    Event.Timestamp = FDateTime::Now();
    Event.EventType = EventType;
    Event.AffectedDomain = Domain;
    Event.ChangeMagnitude = Magnitude;
    Event.Trigger = Trigger;

    EvolutionHistory.Add(Event);

    // Keep history manageable
    if (EvolutionHistory.Num() > 1000)
    {
        EvolutionHistory.RemoveAt(0, 100);
    }
}

FString UAvatarEvolutionSystem::GenerateEventID()
{
    return FString::Printf(TEXT("EvtEvo_%d"), ++EventIDCounter);
}

FString UAvatarEvolutionSystem::GenerateRecordID()
{
    return FString::Printf(TEXT("RecNiche_%d"), ++RecordIDCounter);
}

float UAvatarEvolutionSystem::ComputeStageThreshold(EDevelopmentalStage Stage) const
{
    if (const float* Threshold = StageThresholds.Find(Stage))
    {
        return *Threshold;
    }
    return 0.0f;
}

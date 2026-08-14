#include "EntelechyFramework.h"

UEntelechyFramework::UEntelechyFramework()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;
    
    InitializeDimensionWeights();
}

void UEntelechyFramework::BeginPlay()
{
    Super::BeginPlay();
    InitializeEntelechy(TEXT("DeepTreeEcho-001"));
}

void UEntelechyFramework::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bEnableAutoProcessing)
    {
        AccumulatedTime += DeltaTime;
        if (AccumulatedTime >= ProcessingInterval)
        {
            AccumulatedTime -= ProcessingInterval;
            AssessAllDimensions();
            CurrentState.Genome.Age++;
        }
    }
}

// ========================================
// INITIALIZATION
// ========================================

void UEntelechyFramework::InitializeEntelechy(const FString& SystemId)
{
    CurrentState.Stage = EEntelechyStage::Embryonic;
    
    InitializeGenome(SystemId);
    InitializeDevelopmentPhases();
    
    // Initialize dimensions
    CurrentState.Ontological.FoundationHealth = 0.0f;
    CurrentState.Ontological.CoreHealth = 0.0f;
    CurrentState.Ontological.SpecializedHealth = 0.0f;
    CurrentState.Ontological.ArchitecturalCompleteness = 0.0f;

    CurrentState.Teleological.RoadmapAlignment = 0.0f;
    CurrentState.Teleological.ActualizationTrajectory = 0.0f;
    CurrentState.Teleological.PurposeClarity = 0.5f;
    CurrentState.Teleological.Telos = TEXT("Achieve AGI through cognitive integration");

    CurrentState.Cognitive.CognitiveCompleteness = 0.0f;

    CurrentState.Integrative.IntegrationHealth = 0.0f;

    CurrentState.Evolutionary.EvolutionaryPotential = 0.5f;
    CurrentState.Evolutionary.bHasAutognosis = false;
    CurrentState.Evolutionary.bHasOntogenesis = false;

    UpdateMetrics();
}

void UEntelechyFramework::ResetEntelechy()
{
    InitializeEntelechy(CurrentState.Genome.Id);
}

void UEntelechyFramework::InitializeDimensionWeights()
{
    // Fitness weights from entelechy.md:
    // ontologicalHealth * 0.2 + teleologicalAlignment * 0.25 + 
    // cognitiveCompleteness * 0.25 + integrativeHealth * 0.15 + 
    // evolutionaryPotential * 0.15
    
    DimensionWeights.Add(EEntelechyDimension::Ontological, 0.2f);
    DimensionWeights.Add(EEntelechyDimension::Teleological, 0.25f);
    DimensionWeights.Add(EEntelechyDimension::Cognitive, 0.25f);
    DimensionWeights.Add(EEntelechyDimension::Integrative, 0.15f);
    DimensionWeights.Add(EEntelechyDimension::Evolutionary, 0.15f);
}

void UEntelechyFramework::InitializeGenome(const FString& SystemId)
{
    CurrentState.Genome.Id = SystemId;
    CurrentState.Genome.Generation = 0;
    CurrentState.Genome.Lineage.Empty();
    CurrentState.Genome.Lineage.Add(SystemId);
    CurrentState.Genome.Fitness = 0.0f;
    CurrentState.Genome.Age = 0;
    CurrentState.Genome.ActualizationLevel = 0.0f;

    // Initialize genes (random values for now)
    const int32 GeneCount = 10;
    
    CurrentState.Genome.OntologicalGenes.SetNum(GeneCount);
    CurrentState.Genome.TeleologicalGenes.SetNum(GeneCount);
    CurrentState.Genome.CognitiveGenes.SetNum(GeneCount);
    CurrentState.Genome.IntegrativeGenes.SetNum(GeneCount);
    CurrentState.Genome.EvolutionaryGenes.SetNum(GeneCount);

    for (int32 i = 0; i < GeneCount; ++i)
    {
        CurrentState.Genome.OntologicalGenes[i] = FMath::FRandRange(0.0f, 1.0f);
        CurrentState.Genome.TeleologicalGenes[i] = FMath::FRandRange(0.0f, 1.0f);
        CurrentState.Genome.CognitiveGenes[i] = FMath::FRandRange(0.0f, 1.0f);
        CurrentState.Genome.IntegrativeGenes[i] = FMath::FRandRange(0.0f, 1.0f);
        CurrentState.Genome.EvolutionaryGenes[i] = FMath::FRandRange(0.0f, 1.0f);
    }
}

void UEntelechyFramework::InitializeDevelopmentPhases()
{
    CurrentState.Teleological.DevelopmentPhases.Empty();

    for (int32 i = 0; i < 5; ++i)
    {
        FPhaseProgress Phase;
        Phase.Phase = static_cast<EDevelopmentPhase>(i);
        Phase.Progress = 0.0f;
        Phase.bComplete = false;
        CurrentState.Teleological.DevelopmentPhases.Add(Phase);
    }
}

// ========================================
// ASSESSMENT
// ========================================

void UEntelechyFramework::AssessAllDimensions()
{
    AssessOntological();
    AssessTeleological();
    AssessCognitive();
    AssessIntegrative();
    AssessEvolutionary();
    
    UpdateDevelopmentStage();
    UpdateMetrics();
    
    // Update genome fitness
    CurrentState.Genome.Fitness = CalculateFitness();
    CurrentState.Genome.ActualizationLevel = GetActualizationPercentage();
}

void UEntelechyFramework::AssessOntological()
{
    // Calculate foundation health
    float FoundationSum = 0.0f;
    int32 FoundationCount = CurrentState.Ontological.FoundationComponents.Num();
    for (const FComponentState& Comp : CurrentState.Ontological.FoundationComponents)
    {
        if (Comp.bExists && Comp.bFunctional)
        {
            FoundationSum += Comp.Health;
        }
    }
    CurrentState.Ontological.FoundationHealth = FoundationCount > 0 ? FoundationSum / FoundationCount : 0.0f;

    // Calculate core health
    float CoreSum = 0.0f;
    int32 CoreCount = CurrentState.Ontological.CoreComponents.Num();
    for (const FComponentState& Comp : CurrentState.Ontological.CoreComponents)
    {
        if (Comp.bExists && Comp.bFunctional)
        {
            CoreSum += Comp.Health;
        }
    }
    CurrentState.Ontological.CoreHealth = CoreCount > 0 ? CoreSum / CoreCount : 0.0f;

    // Calculate specialized health
    float SpecializedSum = 0.0f;
    int32 SpecializedCount = CurrentState.Ontological.SpecializedComponents.Num();
    for (const FComponentState& Comp : CurrentState.Ontological.SpecializedComponents)
    {
        if (Comp.bExists && Comp.bFunctional)
        {
            SpecializedSum += Comp.Health;
        }
    }
    CurrentState.Ontological.SpecializedHealth = SpecializedCount > 0 ? SpecializedSum / SpecializedCount : 0.0f;

    // Calculate architectural completeness
    CurrentState.Ontological.ArchitecturalCompleteness = 
        (CurrentState.Ontological.FoundationHealth * 0.3f +
         CurrentState.Ontological.CoreHealth * 0.4f +
         CurrentState.Ontological.SpecializedHealth * 0.3f);
}

void UEntelechyFramework::AssessTeleological()
{
    // Calculate phase progress
    float TotalProgress = 0.0f;
    int32 CompletedPhases = 0;
    
    for (const FPhaseProgress& Phase : CurrentState.Teleological.DevelopmentPhases)
    {
        TotalProgress += Phase.Progress;
        if (Phase.bComplete)
        {
            CompletedPhases++;
        }
    }

    int32 PhaseCount = CurrentState.Teleological.DevelopmentPhases.Num();
    CurrentState.Teleological.ActualizationTrajectory = PhaseCount > 0 ? TotalProgress / PhaseCount : 0.0f;
    
    // Roadmap alignment based on sequential completion
    CurrentState.Teleological.RoadmapAlignment = PhaseCount > 0 ? 
        static_cast<float>(CompletedPhases) / PhaseCount : 0.0f;
}

void UEntelechyFramework::AssessCognitive()
{
    // Calculate cognitive completeness from subsystems
    float ReasoningHealth = CurrentState.Cognitive.ReasoningSystems.Health;
    float PatternHealth = CurrentState.Cognitive.PatternSystems.Health;
    float AttentionHealth = CurrentState.Cognitive.AttentionSystems.Health;
    float LearningHealth = CurrentState.Cognitive.LearningSystems.Health;

    CurrentState.Cognitive.CognitiveCompleteness = 
        (ReasoningHealth * 0.3f +
         PatternHealth * 0.2f +
         AttentionHealth * 0.25f +
         LearningHealth * 0.25f);
}

void UEntelechyFramework::AssessIntegrative()
{
    // Calculate dependency health
    if (CurrentState.Integrative.TotalDependencies > 0)
    {
        CurrentState.Integrative.DependencyHealth = 
            static_cast<float>(CurrentState.Integrative.SatisfiedDependencies) / 
            CurrentState.Integrative.TotalDependencies;
    }
    else
    {
        CurrentState.Integrative.DependencyHealth = 1.0f;
    }

    // Calculate overall integration health
    CurrentState.Integrative.IntegrationHealth = 
        (CurrentState.Integrative.DependencyHealth * 0.4f +
         CurrentState.Integrative.BuildIntegrationHealth * 0.3f +
         CurrentState.Integrative.TestIntegrationHealth * 0.3f);
}

void UEntelechyFramework::AssessEvolutionary()
{
    // Calculate code health from markers
    int32 TotalMarkers = CurrentState.Evolutionary.TodoCount + 
                         CurrentState.Evolutionary.FixmeCount + 
                         CurrentState.Evolutionary.StubCount;
    
    // Assume a baseline of 100 markers as "normal" for a developing system
    float MarkerRatio = FMath::Min(1.0f, static_cast<float>(TotalMarkers) / 100.0f);
    CurrentState.Evolutionary.CodeHealth = 1.0f - MarkerRatio;

    // Calculate self-improvement capacity
    float MetaToolsHealth = 0.0f;
    if (CurrentState.Evolutionary.bHasAutognosis) MetaToolsHealth += 0.5f;
    if (CurrentState.Evolutionary.bHasOntogenesis) MetaToolsHealth += 0.5f;
    CurrentState.Evolutionary.SelfImprovementCapacity = MetaToolsHealth;

    // Calculate evolutionary potential
    CurrentState.Evolutionary.EvolutionaryPotential = 
        (CurrentState.Evolutionary.CodeHealth * 0.3f +
         CurrentState.Evolutionary.ImplementationDepth * 0.3f +
         CurrentState.Evolutionary.SelfImprovementCapacity * 0.4f);
}

// ========================================
// METRICS
// ========================================

float UEntelechyFramework::CalculateFitness() const
{
    float Fitness = 0.0f;
    
    Fitness += CalculateOntologicalHealth() * DimensionWeights.FindRef(EEntelechyDimension::Ontological);
    Fitness += CalculateTeleologicalHealth() * DimensionWeights.FindRef(EEntelechyDimension::Teleological);
    Fitness += CalculateCognitiveHealth() * DimensionWeights.FindRef(EEntelechyDimension::Cognitive);
    Fitness += CalculateIntegrativeHealth() * DimensionWeights.FindRef(EEntelechyDimension::Integrative);
    Fitness += CalculateEvolutionaryHealth() * DimensionWeights.FindRef(EEntelechyDimension::Evolutionary);

    return FMath::Clamp(Fitness, 0.0f, 1.0f);
}

float UEntelechyFramework::GetDimensionHealth(EEntelechyDimension Dimension) const
{
    switch (Dimension)
    {
        case EEntelechyDimension::Ontological:
            return CalculateOntologicalHealth();
        case EEntelechyDimension::Teleological:
            return CalculateTeleologicalHealth();
        case EEntelechyDimension::Cognitive:
            return CalculateCognitiveHealth();
        case EEntelechyDimension::Integrative:
            return CalculateIntegrativeHealth();
        case EEntelechyDimension::Evolutionary:
            return CalculateEvolutionaryHealth();
        default:
            return 0.0f;
    }
}

EEntelechyStage UEntelechyFramework::GetDevelopmentStage() const
{
    return CurrentState.Stage;
}

float UEntelechyFramework::GetActualizationPercentage() const
{
    return CalculateFitness() * 100.0f;
}

float UEntelechyFramework::CalculateOntologicalHealth() const
{
    return CurrentState.Ontological.ArchitecturalCompleteness;
}

float UEntelechyFramework::CalculateTeleologicalHealth() const
{
    return (CurrentState.Teleological.ActualizationTrajectory * 0.5f +
            CurrentState.Teleological.RoadmapAlignment * 0.3f +
            CurrentState.Teleological.PurposeClarity * 0.2f);
}

float UEntelechyFramework::CalculateCognitiveHealth() const
{
    return CurrentState.Cognitive.CognitiveCompleteness;
}

float UEntelechyFramework::CalculateIntegrativeHealth() const
{
    return CurrentState.Integrative.IntegrationHealth;
}

float UEntelechyFramework::CalculateEvolutionaryHealth() const
{
    return CurrentState.Evolutionary.EvolutionaryPotential;
}

// ========================================
// EVOLUTION
// ========================================

bool UEntelechyFramework::TryAdvanceStage()
{
    float Actualization = GetActualizationPercentage();
    EEntelechyStage NewStage = CurrentState.Stage;

    // Determine appropriate stage based on actualization
    if (Actualization >= 95.0f)
    {
        NewStage = EEntelechyStage::Transcendent;
    }
    else if (Actualization >= 80.0f)
    {
        NewStage = EEntelechyStage::Adult;
    }
    else if (Actualization >= 60.0f)
    {
        NewStage = EEntelechyStage::Adolescent;
    }
    else if (Actualization >= 30.0f)
    {
        NewStage = EEntelechyStage::Juvenile;
    }
    else
    {
        NewStage = EEntelechyStage::Embryonic;
    }

    if (NewStage != CurrentState.Stage)
    {
        CurrentState.Stage = NewStage;
        CurrentState.Genome.Generation++;
        return true;
    }

    return false;
}

void UEntelechyFramework::RegisterComponent(const FString& Name, EEntelechyDimension Dimension)
{
    FComponentState NewComponent;
    NewComponent.Name = Name;
    NewComponent.bExists = true;
    NewComponent.bFunctional = false;
    NewComponent.Health = 0.0f;
    NewComponent.IntegrationLevel = 0.0f;

    switch (Dimension)
    {
        case EEntelechyDimension::Ontological:
            CurrentState.Ontological.SpecializedComponents.Add(NewComponent);
            break;
        case EEntelechyDimension::Cognitive:
            CurrentState.Cognitive.ReasoningSystems.Components.Add(NewComponent);
            break;
        default:
            break;
    }
}

void UEntelechyFramework::UpdateComponentHealth(const FString& Name, float Health)
{
    // Search through all component arrays
    auto UpdateInArray = [&](TArray<FComponentState>& Components) -> bool
    {
        for (FComponentState& Comp : Components)
        {
            if (Comp.Name == Name)
            {
                Comp.Health = FMath::Clamp(Health, 0.0f, 1.0f);
                Comp.bFunctional = Health > 0.5f;
                return true;
            }
        }
        return false;
    };

    if (UpdateInArray(CurrentState.Ontological.FoundationComponents)) return;
    if (UpdateInArray(CurrentState.Ontological.CoreComponents)) return;
    if (UpdateInArray(CurrentState.Ontological.SpecializedComponents)) return;
    if (UpdateInArray(CurrentState.Cognitive.ReasoningSystems.Components)) return;
    if (UpdateInArray(CurrentState.Cognitive.PatternSystems.Components)) return;
    if (UpdateInArray(CurrentState.Cognitive.AttentionSystems.Components)) return;
    if (UpdateInArray(CurrentState.Cognitive.LearningSystems.Components)) return;
}

void UEntelechyFramework::AddMilestone(EDevelopmentPhase Phase, const FString& Milestone)
{
    int32 PhaseIndex = static_cast<int32>(Phase);
    if (CurrentState.Teleological.DevelopmentPhases.IsValidIndex(PhaseIndex))
    {
        CurrentState.Teleological.DevelopmentPhases[PhaseIndex].Milestones.Add(Milestone);
        
        // Update progress based on milestones (assume 5 milestones per phase)
        float Progress = FMath::Min(1.0f, 
            CurrentState.Teleological.DevelopmentPhases[PhaseIndex].Milestones.Num() / 5.0f);
        CurrentState.Teleological.DevelopmentPhases[PhaseIndex].Progress = Progress;
        
        if (Progress >= 1.0f)
        {
            CurrentState.Teleological.DevelopmentPhases[PhaseIndex].bComplete = true;
        }
    }
}

// ========================================
// GENOME
// ========================================

FEntelechyGenome UEntelechyFramework::GetGenome() const
{
    return CurrentState.Genome;
}

void UEntelechyFramework::MutateGenome(float MutationRate)
{
    auto MutateGenes = [MutationRate](TArray<float>& Genes)
    {
        for (float& Gene : Genes)
        {
            if (FMath::FRand() < MutationRate)
            {
                Gene = FMath::Clamp(Gene + FMath::FRandRange(-0.1f, 0.1f), 0.0f, 1.0f);
            }
        }
    };

    MutateGenes(CurrentState.Genome.OntologicalGenes);
    MutateGenes(CurrentState.Genome.TeleologicalGenes);
    MutateGenes(CurrentState.Genome.CognitiveGenes);
    MutateGenes(CurrentState.Genome.IntegrativeGenes);
    MutateGenes(CurrentState.Genome.EvolutionaryGenes);

    CurrentState.Genome.Generation++;
}

FEntelechyGenome UEntelechyFramework::CrossoverGenome(const FEntelechyGenome& Other) const
{
    FEntelechyGenome Child;
    Child.Id = FString::Printf(TEXT("%s-x-%s"), *CurrentState.Genome.Id, *Other.Id);
    Child.Generation = FMath::Max(CurrentState.Genome.Generation, Other.Generation) + 1;
    Child.Lineage = CurrentState.Genome.Lineage;
    Child.Lineage.Append(Other.Lineage);
    Child.Age = 0;

    auto CrossoverGenes = [](const TArray<float>& A, const TArray<float>& B) -> TArray<float>
    {
        TArray<float> Result;
        int32 Count = FMath::Min(A.Num(), B.Num());
        Result.SetNum(Count);
        
        for (int32 i = 0; i < Count; ++i)
        {
            // Uniform crossover
            Result[i] = FMath::FRand() < 0.5f ? A[i] : B[i];
        }
        
        return Result;
    };

    Child.OntologicalGenes = CrossoverGenes(CurrentState.Genome.OntologicalGenes, Other.OntologicalGenes);
    Child.TeleologicalGenes = CrossoverGenes(CurrentState.Genome.TeleologicalGenes, Other.TeleologicalGenes);
    Child.CognitiveGenes = CrossoverGenes(CurrentState.Genome.CognitiveGenes, Other.CognitiveGenes);
    Child.IntegrativeGenes = CrossoverGenes(CurrentState.Genome.IntegrativeGenes, Other.IntegrativeGenes);
    Child.EvolutionaryGenes = CrossoverGenes(CurrentState.Genome.EvolutionaryGenes, Other.EvolutionaryGenes);

    return Child;
}

// ========================================
// INTERNAL METHODS
// ========================================

void UEntelechyFramework::UpdateDevelopmentStage()
{
    TryAdvanceStage();
}

void UEntelechyFramework::UpdateMetrics()
{
    CurrentState.Metrics.Actualization = GetActualizationPercentage() / 100.0f;
    CurrentState.Metrics.Completeness = CurrentState.Ontological.ArchitecturalCompleteness;
    CurrentState.Metrics.Coherence = CurrentState.Integrative.IntegrationHealth;
    CurrentState.Metrics.Vitality = CurrentState.Evolutionary.EvolutionaryPotential;
    CurrentState.Metrics.Alignment = CurrentState.Teleological.RoadmapAlignment;

    // Count components
    CurrentState.Metrics.TotalComponents = 
        CurrentState.Ontological.FoundationComponents.Num() +
        CurrentState.Ontological.CoreComponents.Num() +
        CurrentState.Ontological.SpecializedComponents.Num();

    CurrentState.Metrics.IntegratedComponents = 0;
    CurrentState.Metrics.FragmentedComponents = 0;

    auto CountComponents = [&](const TArray<FComponentState>& Components)
    {
        for (const FComponentState& Comp : Components)
        {
            if (Comp.bFunctional && Comp.IntegrationLevel > 0.5f)
            {
                CurrentState.Metrics.IntegratedComponents++;
            }
            else if (Comp.bExists)
            {
                CurrentState.Metrics.FragmentedComponents++;
            }
        }
    };

    CountComponents(CurrentState.Ontological.FoundationComponents);
    CountComponents(CurrentState.Ontological.CoreComponents);
    CountComponents(CurrentState.Ontological.SpecializedComponents);

    CurrentState.Metrics.TotalCodeMarkers = 
        CurrentState.Evolutionary.TodoCount +
        CurrentState.Evolutionary.FixmeCount +
        CurrentState.Evolutionary.StubCount;
}

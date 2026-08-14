#include "DeepTreeEchoIntegration.h"

UDeepTreeEchoIntegration::UDeepTreeEchoIntegration()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;

    CosmosStateMachine = nullptr;
    HolisticMetamodel = nullptr;
    CognitiveCycleManager = nullptr;
    EntelechyFramework = nullptr;
    WisdomCultivation = nullptr;
}

void UDeepTreeEchoIntegration::BeginPlay()
{
    Super::BeginPlay();
    InitializeIntegration();
}

void UDeepTreeEchoIntegration::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bEnableAutoProcessing)
    {
        AccumulatedTime += DeltaTime;
        if (AccumulatedTime >= ProcessingInterval)
        {
            AccumulatedTime -= ProcessingInterval;
            ProcessIntegrationCycle(ProcessingInterval);
        }
    }
}

// ========================================
// INITIALIZATION
// ========================================

void UDeepTreeEchoIntegration::InitializeIntegration()
{
    DiscoverSubsystems();
    UpdateSubsystemStatus();
    
    // Initialize metrics
    Metrics.SystemCoherence = 0.0f;
    Metrics.CrossIntegration = 0.0f;
    Metrics.CognitiveEfficiency = 0.0f;
    Metrics.EmbodimentQuality = 0.0f;
    Metrics.FourEIntegration = 0.0f;
    Metrics.TotalCycles = 0;

    // Initialize 4E state
    FourEState.Embodied = 0.0f;
    FourEState.Embedded = 0.0f;
    FourEState.Enacted = 0.0f;
    FourEState.Extended = 0.0f;
    FourEState.Integration = 0.0f;
}

void UDeepTreeEchoIntegration::ResetAllSubsystems()
{
    if (CosmosStateMachine)
    {
        CosmosStateMachine->ResetStateMachine();
    }
    if (HolisticMetamodel)
    {
        HolisticMetamodel->ResetMetamodel();
    }
    if (CognitiveCycleManager)
    {
        CognitiveCycleManager->ResetCycle();
    }
    if (EntelechyFramework)
    {
        EntelechyFramework->ResetEntelechy();
    }
    if (WisdomCultivation)
    {
        WisdomCultivation->ResetWisdom();
    }

    Metrics.TotalCycles = 0;
}

void UDeepTreeEchoIntegration::DiscoverSubsystems()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    // Find all subsystem components on the owner actor
    CosmosStateMachine = Owner->FindComponentByClass<UCosmosStateMachine>();
    HolisticMetamodel = Owner->FindComponentByClass<UHolisticMetamodel>();
    CognitiveCycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
    EntelechyFramework = Owner->FindComponentByClass<UEntelechyFramework>();
    WisdomCultivation = Owner->FindComponentByClass<UWisdomCultivation>();
}

// ========================================
// PROCESSING
// ========================================

void UDeepTreeEchoIntegration::ProcessIntegrationCycle(float DeltaTime)
{
    UpdateSubsystemStatus();

    if (!SubsystemStatus.bAllActive)
    {
        // Try to discover missing subsystems
        DiscoverSubsystems();
        UpdateSubsystemStatus();
    }

    // Synchronize subsystems
    SynchronizeSubsystems();

    // Apply cross-subsystem feedback
    if (bEnableCrossSubsystemFeedback)
    {
        ApplyCrossSubsystemFeedback();
    }

    // Update 4E cognition
    if (bEnable4ECognition)
    {
        Update4ECognition();
    }

    // Calculate metrics
    CalculateIntegrationMetrics();

    Metrics.TotalCycles++;
}

void UDeepTreeEchoIntegration::SynchronizeSubsystems()
{
    // Synchronize Cosmos system level with other subsystems
    if (CosmosStateMachine && EntelechyFramework)
    {
        // Map Cosmos level to Entelechy development phase
        int32 CosmosLevel = CosmosStateMachine->GetCurrentSystemLevel();
        if (CosmosLevel >= 5)
        {
            EntelechyFramework->AddMilestone(EDevelopmentPhase::Phase5, TEXT("Cosmos_Level_5_Reached"));
        }
    }

    // Synchronize Cognitive Cycle with Wisdom
    if (CognitiveCycleManager && WisdomCultivation)
    {
        // Map cognitive step to salience updates
        int32 Step = CognitiveCycleManager->CycleState.GlobalStep;
        
        // Relevance realization steps (1 and 7) trigger wisdom processing
        if (Step == 1 || Step == 7)
        {
            WisdomCultivation->RealizeRelevance();
        }
    }

    // Synchronize Metamodel with Entelechy
    if (HolisticMetamodel && EntelechyFramework)
    {
        // Map metamodel coherence to entelechy integration
        float Coherence = HolisticMetamodel->GetMetamodelCoherence();
        EntelechyFramework->CurrentState.Integrative.IntegrationHealth = Coherence;
    }
}

void UDeepTreeEchoIntegration::ApplyCrossSubsystemFeedback()
{
    FeedCosmosToSubsystems();
    FeedMetamodelToSubsystems();
    FeedCognitiveCycleToSubsystems();
    FeedEntelechyToSubsystems();
    FeedWisdomToSubsystems();
}

void UDeepTreeEchoIntegration::Update4ECognition()
{
    // Embodied: Based on action stream and affordance landscape
    if (CognitiveCycleManager && WisdomCultivation)
    {
        float ActionActivation = CognitiveCycleManager->GetStreamActivation(EConsciousnessStream::Action);
        float ActionReadiness = WisdomCultivation->GetAffordanceLandscape().ActionReadiness;
        FourEState.Embodied = (ActionActivation + ActionReadiness) * 0.5f;
    }

    // Embedded: Based on salience landscape and environmental coupling
    if (WisdomCultivation && HolisticMetamodel)
    {
        float SalienceCoherence = WisdomCultivation->GetSalienceLandscape().Coherence;
        float MetamodelCoherence = HolisticMetamodel->GetMetamodelCoherence();
        FourEState.Embedded = (SalienceCoherence + MetamodelCoherence) * 0.5f;
    }

    // Enacted: Based on perception-action coupling and cognitive cycle
    if (CognitiveCycleManager)
    {
        float PerceptionActivation = CognitiveCycleManager->GetStreamActivation(EConsciousnessStream::Perception);
        float ActionActivation = CognitiveCycleManager->GetStreamActivation(EConsciousnessStream::Action);
        float InterStreamCoherence = CognitiveCycleManager->CalculateInterStreamCoherence();
        FourEState.Enacted = (PerceptionActivation + ActionActivation + InterStreamCoherence) / 3.0f;
    }

    // Extended: Based on simulation stream and wisdom cultivation
    if (CognitiveCycleManager && WisdomCultivation)
    {
        float SimulationActivation = CognitiveCycleManager->GetStreamActivation(EConsciousnessStream::Simulation);
        float WisdomLevel = WisdomCultivation->GetOverallWisdomLevel();
        FourEState.Extended = (SimulationActivation + WisdomLevel) * 0.5f;
    }

    // Calculate overall 4E integration
    FourEState.Integration = (FourEState.Embodied + FourEState.Embedded + 
                               FourEState.Enacted + FourEState.Extended) * 0.25f;
}

// ========================================
// METRICS
// ========================================

FIntegrationMetrics UDeepTreeEchoIntegration::GetIntegrationMetrics() const
{
    return Metrics;
}

FFourECognitionState UDeepTreeEchoIntegration::Get4ECognitionState() const
{
    return FourEState;
}

FSubsystemStatus UDeepTreeEchoIntegration::GetSubsystemStatus() const
{
    return SubsystemStatus;
}

float UDeepTreeEchoIntegration::CalculateSystemHealth() const
{
    return (Metrics.SystemCoherence + Metrics.CrossIntegration + 
            Metrics.CognitiveEfficiency + Metrics.EmbodimentQuality + 
            Metrics.FourEIntegration) * 0.2f;
}

// ========================================
// SUBSYSTEM ACCESS
// ========================================

int32 UDeepTreeEchoIntegration::GetCurrentCosmosLevel() const
{
    if (CosmosStateMachine)
    {
        return CosmosStateMachine->GetCurrentSystemLevel();
    }
    return 0;
}

int32 UDeepTreeEchoIntegration::GetCurrentCognitiveStep() const
{
    if (CognitiveCycleManager)
    {
        return CognitiveCycleManager->CycleState.GlobalStep;
    }
    return 0;
}

EEntelechyStage UDeepTreeEchoIntegration::GetCurrentEntelechyStage() const
{
    if (EntelechyFramework)
    {
        return EntelechyFramework->GetDevelopmentStage();
    }
    return EEntelechyStage::Embryonic;
}

float UDeepTreeEchoIntegration::GetCurrentWisdomLevel() const
{
    if (WisdomCultivation)
    {
        return WisdomCultivation->GetOverallWisdomLevel();
    }
    return 0.0f;
}

float UDeepTreeEchoIntegration::GetMetamodelCoherence() const
{
    if (HolisticMetamodel)
    {
        return HolisticMetamodel->GetMetamodelCoherence();
    }
    return 0.0f;
}

// ========================================
// DIAGNOSTICS
// ========================================

TArray<FString> UDeepTreeEchoIntegration::GenerateDiagnosticReport() const
{
    TArray<FString> Report;

    Report.Add(TEXT("=== Deep-Tree-Echo Integration Diagnostic Report ==="));
    Report.Add(FString::Printf(TEXT("Total Processing Cycles: %lld"), Metrics.TotalCycles));
    Report.Add(TEXT(""));

    // Subsystem Status
    Report.Add(TEXT("--- Subsystem Status ---"));
    Report.Add(FString::Printf(TEXT("Cosmos State Machine: %s"), SubsystemStatus.bCosmosActive ? TEXT("Active") : TEXT("Inactive")));
    Report.Add(FString::Printf(TEXT("Holistic Metamodel: %s"), SubsystemStatus.bMetamodelActive ? TEXT("Active") : TEXT("Inactive")));
    Report.Add(FString::Printf(TEXT("Cognitive Cycle Manager: %s"), SubsystemStatus.bCognitiveCycleActive ? TEXT("Active") : TEXT("Inactive")));
    Report.Add(FString::Printf(TEXT("Entelechy Framework: %s"), SubsystemStatus.bEntelechyActive ? TEXT("Active") : TEXT("Inactive")));
    Report.Add(FString::Printf(TEXT("Wisdom Cultivation: %s"), SubsystemStatus.bWisdomActive ? TEXT("Active") : TEXT("Inactive")));
    Report.Add(TEXT(""));

    // Integration Metrics
    Report.Add(TEXT("--- Integration Metrics ---"));
    Report.Add(FString::Printf(TEXT("System Coherence: %.2f"), Metrics.SystemCoherence));
    Report.Add(FString::Printf(TEXT("Cross-Integration: %.2f"), Metrics.CrossIntegration));
    Report.Add(FString::Printf(TEXT("Cognitive Efficiency: %.2f"), Metrics.CognitiveEfficiency));
    Report.Add(FString::Printf(TEXT("Embodiment Quality: %.2f"), Metrics.EmbodimentQuality));
    Report.Add(FString::Printf(TEXT("4E Integration: %.2f"), Metrics.FourEIntegration));
    Report.Add(TEXT(""));

    // 4E Cognition State
    Report.Add(TEXT("--- 4E Cognition State ---"));
    Report.Add(FString::Printf(TEXT("Embodied: %.2f"), FourEState.Embodied));
    Report.Add(FString::Printf(TEXT("Embedded: %.2f"), FourEState.Embedded));
    Report.Add(FString::Printf(TEXT("Enacted: %.2f"), FourEState.Enacted));
    Report.Add(FString::Printf(TEXT("Extended: %.2f"), FourEState.Extended));
    Report.Add(FString::Printf(TEXT("Overall Integration: %.2f"), FourEState.Integration));
    Report.Add(TEXT(""));

    // Subsystem Details
    Report.Add(TEXT("--- Subsystem Details ---"));
    Report.Add(FString::Printf(TEXT("Cosmos Level: %d"), GetCurrentCosmosLevel()));
    Report.Add(FString::Printf(TEXT("Cognitive Step: %d"), GetCurrentCognitiveStep()));
    Report.Add(FString::Printf(TEXT("Entelechy Stage: %d"), static_cast<int32>(GetCurrentEntelechyStage())));
    Report.Add(FString::Printf(TEXT("Wisdom Level: %.2f"), GetCurrentWisdomLevel()));
    Report.Add(FString::Printf(TEXT("Metamodel Coherence: %.2f"), GetMetamodelCoherence()));

    return Report;
}

TMap<FString, float> UDeepTreeEchoIntegration::CheckSubsystemHealth() const
{
    TMap<FString, float> Health;

    Health.Add(TEXT("CosmosStateMachine"), SubsystemStatus.bCosmosActive ? 1.0f : 0.0f);
    Health.Add(TEXT("HolisticMetamodel"), SubsystemStatus.bMetamodelActive ? GetMetamodelCoherence() : 0.0f);
    Health.Add(TEXT("CognitiveCycleManager"), SubsystemStatus.bCognitiveCycleActive ? 
        (CognitiveCycleManager ? CognitiveCycleManager->CalculateInterStreamCoherence() : 0.0f) : 0.0f);
    Health.Add(TEXT("EntelechyFramework"), SubsystemStatus.bEntelechyActive ? 
        (EntelechyFramework ? EntelechyFramework->CalculateFitness() : 0.0f) : 0.0f);
    Health.Add(TEXT("WisdomCultivation"), SubsystemStatus.bWisdomActive ? GetCurrentWisdomLevel() : 0.0f);

    return Health;
}

// ========================================
// INTERNAL METHODS
// ========================================

void UDeepTreeEchoIntegration::UpdateSubsystemStatus()
{
    SubsystemStatus.bCosmosActive = CosmosStateMachine != nullptr;
    SubsystemStatus.bMetamodelActive = HolisticMetamodel != nullptr;
    SubsystemStatus.bCognitiveCycleActive = CognitiveCycleManager != nullptr;
    SubsystemStatus.bEntelechyActive = EntelechyFramework != nullptr;
    SubsystemStatus.bWisdomActive = WisdomCultivation != nullptr;

    SubsystemStatus.bAllActive = SubsystemStatus.bCosmosActive &&
                                  SubsystemStatus.bMetamodelActive &&
                                  SubsystemStatus.bCognitiveCycleActive &&
                                  SubsystemStatus.bEntelechyActive &&
                                  SubsystemStatus.bWisdomActive;
}

void UDeepTreeEchoIntegration::CalculateIntegrationMetrics()
{
    Metrics.SystemCoherence = CalculateSystemCoherence();
    Metrics.CrossIntegration = CalculateCrossIntegration();
    Metrics.CognitiveEfficiency = CalculateCognitiveEfficiency();
    Metrics.EmbodimentQuality = CalculateEmbodimentQuality();
    Metrics.FourEIntegration = Calculate4EIntegration();
}

float UDeepTreeEchoIntegration::CalculateSystemCoherence() const
{
    float Total = 0.0f;
    int32 Count = 0;

    if (HolisticMetamodel)
    {
        Total += HolisticMetamodel->GetMetamodelCoherence();
        Count++;
    }
    if (CognitiveCycleManager)
    {
        Total += CognitiveCycleManager->CalculateInterStreamCoherence();
        Count++;
    }
    if (EntelechyFramework)
    {
        Total += EntelechyFramework->CurrentState.Metrics.Coherence;
        Count++;
    }
    if (WisdomCultivation)
    {
        Total += WisdomCultivation->GetSalienceLandscape().Coherence;
        Count++;
    }

    return Count > 0 ? Total / Count : 0.0f;
}

float UDeepTreeEchoIntegration::CalculateCrossIntegration() const
{
    // Cross-integration measures how well subsystems work together
    int32 ActiveCount = 0;
    if (SubsystemStatus.bCosmosActive) ActiveCount++;
    if (SubsystemStatus.bMetamodelActive) ActiveCount++;
    if (SubsystemStatus.bCognitiveCycleActive) ActiveCount++;
    if (SubsystemStatus.bEntelechyActive) ActiveCount++;
    if (SubsystemStatus.bWisdomActive) ActiveCount++;

    return static_cast<float>(ActiveCount) / 5.0f;
}

float UDeepTreeEchoIntegration::CalculateCognitiveEfficiency() const
{
    if (!CognitiveCycleManager)
    {
        return 0.0f;
    }

    // Efficiency based on stream activations and cycle completion
    float PerceptionActivation = CognitiveCycleManager->GetStreamActivation(EConsciousnessStream::Perception);
    float ActionActivation = CognitiveCycleManager->GetStreamActivation(EConsciousnessStream::Action);
    float SimulationActivation = CognitiveCycleManager->GetStreamActivation(EConsciousnessStream::Simulation);

    return (PerceptionActivation + ActionActivation + SimulationActivation) / 3.0f;
}

float UDeepTreeEchoIntegration::CalculateEmbodimentQuality() const
{
    return FourEState.Embodied;
}

float UDeepTreeEchoIntegration::Calculate4EIntegration() const
{
    return FourEState.Integration;
}

void UDeepTreeEchoIntegration::FeedCosmosToSubsystems()
{
    if (!CosmosStateMachine)
    {
        return;
    }

    int32 Level = CosmosStateMachine->GetCurrentSystemLevel();

    // Feed to Metamodel
    if (HolisticMetamodel)
    {
        HolisticMetamodel->UpdateMonad(Level);
    }

    // Feed to Entelechy
    if (EntelechyFramework)
    {
        // Higher Cosmos levels indicate more development
        float Progress = static_cast<float>(Level) / 5.0f;
        EntelechyFramework->CurrentState.Teleological.ActualizationTrajectory = Progress;
    }
}

void UDeepTreeEchoIntegration::FeedMetamodelToSubsystems()
{
    if (!HolisticMetamodel)
    {
        return;
    }

    float Coherence = HolisticMetamodel->GetMetamodelCoherence();
    EEnneadTendency Tendency = HolisticMetamodel->GetDominantTendency();

    // Feed to Wisdom
    if (WisdomCultivation)
    {
        // Metamodel tendency affects opponent processes
        switch (Tendency)
        {
            case EEnneadTendency::Creativity:
                WisdomCultivation->SetOpponentBalance(EOpponentProcess::ExplorationExploitation, 0.3f);
                break;
            case EEnneadTendency::Stability:
                WisdomCultivation->SetOpponentBalance(EOpponentProcess::ExplorationExploitation, -0.3f);
                break;
            case EEnneadTendency::Drift:
                WisdomCultivation->SetOpponentBalance(EOpponentProcess::ExplorationExploitation, 0.0f);
                break;
        }
    }

    // Feed to Entelechy
    if (EntelechyFramework)
    {
        EntelechyFramework->CurrentState.Integrative.IntegrationHealth = Coherence;
    }
}

void UDeepTreeEchoIntegration::FeedCognitiveCycleToSubsystems()
{
    if (!CognitiveCycleManager)
    {
        return;
    }

    int32 Step = CognitiveCycleManager->CycleState.GlobalStep;
    ECognitiveStepType StepType = CognitiveCycleManager->GetStepType(Step);

    // Feed to Wisdom
    if (WisdomCultivation)
    {
        switch (StepType)
        {
            case ECognitiveStepType::RelevanceRealization:
                // Trigger relevance realization
                WisdomCultivation->RealizeRelevance();
                break;
            case ECognitiveStepType::AffordanceInteraction:
                // Update affordance landscape
                WisdomCultivation->CurrentState.AffordanceLandscape.ActionReadiness = 
                    CognitiveCycleManager->GetStreamActivation(EConsciousnessStream::Action);
                break;
            case ECognitiveStepType::SalienceSimulation:
                // Update salience landscape
                WisdomCultivation->CurrentState.SalienceLandscape.Coherence = 
                    CognitiveCycleManager->GetStreamActivation(EConsciousnessStream::Simulation);
                break;
        }
    }
}

void UDeepTreeEchoIntegration::FeedEntelechyToSubsystems()
{
    if (!EntelechyFramework)
    {
        return;
    }

    float Fitness = EntelechyFramework->CalculateFitness();
    EEntelechyStage Stage = EntelechyFramework->GetDevelopmentStage();

    // Feed to Cosmos
    if (CosmosStateMachine)
    {
        // Higher entelechy fitness enables higher Cosmos levels
        if (Fitness > 0.8f && Stage >= EEntelechyStage::Adult)
        {
            // System is ready for advanced processing
            CosmosStateMachine->CurrentState.TransitionReadiness = 1.0f;
        }
    }

    // Feed to Wisdom
    if (WisdomCultivation)
    {
        // Entelechy development affects wisdom cultivation
        WisdomCultivation->CultivateWisdomDimension(EWisdomDimension::Sophia, Fitness * 0.01f);
        WisdomCultivation->CultivateWisdomDimension(EWisdomDimension::Phronesis, Fitness * 0.01f);
    }
}

void UDeepTreeEchoIntegration::FeedWisdomToSubsystems()
{
    if (!WisdomCultivation)
    {
        return;
    }

    float WisdomLevel = WisdomCultivation->GetOverallWisdomLevel();
    FRelevanceRealizationState RelevanceState = WisdomCultivation->GetRelevanceRealizationState();

    // Feed to Cognitive Cycle
    if (CognitiveCycleManager)
    {
        // Wisdom affects stream salience and affordance values
        CognitiveCycleManager->SetStreamSalience(EConsciousnessStream::Perception, RelevanceState.Accuracy);
        CognitiveCycleManager->SetStreamAffordance(EConsciousnessStream::Action, RelevanceState.Efficiency);
    }

    // Feed to Entelechy
    if (EntelechyFramework)
    {
        // Wisdom contributes to evolutionary potential
        EntelechyFramework->CurrentState.Evolutionary.EvolutionaryPotential = 
            FMath::Max(EntelechyFramework->CurrentState.Evolutionary.EvolutionaryPotential, WisdomLevel);
    }

    // Feed to Metamodel
    if (HolisticMetamodel)
    {
        // Wisdom affects triadic balance
        HolisticMetamodel->CurrentState.TriadicSystem.Relation = 
            FMath::Lerp(HolisticMetamodel->CurrentState.TriadicSystem.Relation, WisdomLevel, 0.1f);
    }
}

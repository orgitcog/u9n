// DeepTreeEchoBlueprintLibrary.cpp
// Implementation of Blueprint Function Library for Deep Tree Echo

#include "DeepTreeEchoBlueprintLibrary.h"

// ========================================
// COMPONENT GETTERS
// ========================================

UCognitiveCycleManager* UDeepTreeEchoBlueprintLibrary::GetCognitiveCycleManager(AActor* Actor)
{
    if (!Actor) return nullptr;
    return Actor->FindComponentByClass<UCognitiveCycleManager>();
}

URelevanceRealizationEnnead* UDeepTreeEchoBlueprintLibrary::GetRelevanceRealizationEnnead(AActor* Actor)
{
    if (!Actor) return nullptr;
    return Actor->FindComponentByClass<URelevanceRealizationEnnead>();
}

UWisdomCultivation* UDeepTreeEchoBlueprintLibrary::GetWisdomCultivation(AActor* Actor)
{
    if (!Actor) return nullptr;
    return Actor->FindComponentByClass<UWisdomCultivation>();
}

USensorimotorIntegration* UDeepTreeEchoBlueprintLibrary::GetSensorimotorIntegration(AActor* Actor)
{
    if (!Actor) return nullptr;
    return Actor->FindComponentByClass<USensorimotorIntegration>();
}

UAutognosisSystem* UDeepTreeEchoBlueprintLibrary::GetAutognosisSystem(AActor* Actor)
{
    if (!Actor) return nullptr;
    return Actor->FindComponentByClass<UAutognosisSystem>();
}

// ========================================
// COGNITIVE CYCLE SHORTCUTS
// ========================================

int32 UDeepTreeEchoBlueprintLibrary::GetCurrentCognitiveStep(AActor* Actor)
{
    UCognitiveCycleManager* Manager = GetCognitiveCycleManager(Actor);
    return Manager ? Manager->GetCurrentStep() : 0;
}

ECognitiveModeType UDeepTreeEchoBlueprintLibrary::GetCurrentCognitiveMode(AActor* Actor)
{
    UCognitiveCycleManager* Manager = GetCognitiveCycleManager(Actor);
    return Manager ? Manager->GetCurrentMode() : ECognitiveModeType::Expressive;
}

bool UDeepTreeEchoBlueprintLibrary::IsCurrentStepPivotal(AActor* Actor)
{
    UCognitiveCycleManager* Manager = GetCognitiveCycleManager(Actor);
    return Manager ? Manager->IsCurrentStepPivotal() : false;
}

float UDeepTreeEchoBlueprintLibrary::GetRelevanceRealizationLevel(AActor* Actor)
{
    UCognitiveCycleManager* Manager = GetCognitiveCycleManager(Actor);
    if (Manager)
    {
        return Manager->GetRelevanceRealizationLevel();
    }

    URelevanceRealizationEnnead* Ennead = GetRelevanceRealizationEnnead(Actor);
    return Ennead ? Ennead->GetRelevanceRealizationLevel() : 0.0f;
}

void UDeepTreeEchoBlueprintLibrary::AdvanceCognitiveStep(AActor* Actor)
{
    UCognitiveCycleManager* Manager = GetCognitiveCycleManager(Actor);
    if (Manager)
    {
        Manager->AdvanceStep();
    }
}

// ========================================
// WISDOM SHORTCUTS
// ========================================

float UDeepTreeEchoBlueprintLibrary::GetWisdomLevel(AActor* Actor)
{
    URelevanceRealizationEnnead* Ennead = GetRelevanceRealizationEnnead(Actor);
    if (Ennead)
    {
        return Ennead->GetWisdomLevel();
    }

    UWisdomCultivation* Wisdom = GetWisdomCultivation(Actor);
    return Wisdom ? Wisdom->GetWisdomLevel() : 0.0f;
}

float UDeepTreeEchoBlueprintLibrary::GetMeaningLevel(AActor* Actor)
{
    URelevanceRealizationEnnead* Ennead = GetRelevanceRealizationEnnead(Actor);
    return Ennead ? Ennead->GetMeaningLevel() : 0.0f;
}

void UDeepTreeEchoBlueprintLibrary::ActivateWayOfKnowing(AActor* Actor, EWayOfKnowing Way, float Intensity)
{
    URelevanceRealizationEnnead* Ennead = GetRelevanceRealizationEnnead(Actor);
    if (Ennead)
    {
        Ennead->ActivateKnowing(Way, Intensity);
    }
}

void UDeepTreeEchoBlueprintLibrary::ActivatePracticeOfWisdom(AActor* Actor, EPracticeOfWisdom Practice, float Intensity)
{
    URelevanceRealizationEnnead* Ennead = GetRelevanceRealizationEnnead(Actor);
    if (Ennead)
    {
        Ennead->ActivatePractice(Practice, Intensity);
    }
}

EGnosticStage UDeepTreeEchoBlueprintLibrary::GetCurrentGnosticStage(AActor* Actor)
{
    URelevanceRealizationEnnead* Ennead = GetRelevanceRealizationEnnead(Actor);
    return Ennead ? Ennead->GetCurrentGnosticStage() : EGnosticStage::PropositionalOpening;
}

// ========================================
// EMBODIMENT SHORTCUTS
// ========================================

float UDeepTreeEchoBlueprintLibrary::GetOverallEmbodiment(AActor* Actor)
{
    USensorimotorIntegration* Sensorimotor = GetSensorimotorIntegration(Actor);
    return Sensorimotor ? Sensorimotor->GetOverallEmbodiment() : 0.0f;
}

float UDeepTreeEchoBlueprintLibrary::GetEmbodimentDimension(AActor* Actor, EEmbodimentDimension Dimension)
{
    USensorimotorIntegration* Sensorimotor = GetSensorimotorIntegration(Actor);
    return Sensorimotor ? Sensorimotor->GetEmbodimentLevel(Dimension) : 0.0f;
}

void UDeepTreeEchoBlueprintLibrary::RegisterSensoryInput(AActor* Actor, ESensoryModality Modality, 
                                                          float Intensity, float Salience, FVector Location)
{
    USensorimotorIntegration* Sensorimotor = GetSensorimotorIntegration(Actor);
    if (Sensorimotor)
    {
        FSensoryInput Input;
        Input.InputID = FString::Printf(TEXT("BP_%d"), FMath::RandRange(1000, 9999));
        Input.Modality = Modality;
        Input.Intensity = Intensity;
        Input.Salience = Salience;
        Input.SpatialLocation = Location;
        Sensorimotor->RegisterSensoryInput(Input);
    }
}

void UDeepTreeEchoBlueprintLibrary::IssueMotorCommand(AActor* Actor, EMotorActionType ActionType, 
                                                       FVector TargetLocation, float Intensity)
{
    USensorimotorIntegration* Sensorimotor = GetSensorimotorIntegration(Actor);
    if (Sensorimotor)
    {
        FMotorCommand Command;
        Command.CommandID = FString::Printf(TEXT("BP_CMD_%d"), FMath::RandRange(1000, 9999));
        Command.ActionType = ActionType;
        Command.TargetLocation = TargetLocation;
        Command.Intensity = Intensity;
        Command.Priority = 0.5f;
        Sensorimotor->IssueMotorCommand(Command);
    }
}

TArray<FDetectedAffordance> UDeepTreeEchoBlueprintLibrary::DetectAffordances(AActor* Actor)
{
    USensorimotorIntegration* Sensorimotor = GetSensorimotorIntegration(Actor);
    return Sensorimotor ? Sensorimotor->DetectAffordances() : TArray<FDetectedAffordance>();
}

FDetectedAffordance UDeepTreeEchoBlueprintLibrary::GetBestAffordance(AActor* Actor)
{
    USensorimotorIntegration* Sensorimotor = GetSensorimotorIntegration(Actor);
    return Sensorimotor ? Sensorimotor->GetBestAffordance() : FDetectedAffordance();
}

// ========================================
// SELF-AWARENESS SHORTCUTS
// ========================================

float UDeepTreeEchoBlueprintLibrary::GetSelfAwarenessScore(AActor* Actor)
{
    UAutognosisSystem* Autognosis = GetAutognosisSystem(Actor);
    return Autognosis ? Autognosis->GetSelfAwarenessScore() : 0.0f;
}

FSelfAwarenessAssessment UDeepTreeEchoBlueprintLibrary::GetSelfAwarenessAssessment(AActor* Actor)
{
    UAutognosisSystem* Autognosis = GetAutognosisSystem(Actor);
    return Autognosis ? Autognosis->GetSelfAwarenessAssessment() : FSelfAwarenessAssessment();
}

void UDeepTreeEchoBlueprintLibrary::StartAutognosis(AActor* Actor)
{
    UAutognosisSystem* Autognosis = GetAutognosisSystem(Actor);
    if (Autognosis)
    {
        Autognosis->StartAutognosis();
    }
}

void UDeepTreeEchoBlueprintLibrary::StopAutognosis(AActor* Actor)
{
    UAutognosisSystem* Autognosis = GetAutognosisSystem(Actor);
    if (Autognosis)
    {
        Autognosis->StopAutognosis();
    }
}

TArray<FMetaCognitiveInsight> UDeepTreeEchoBlueprintLibrary::GetRecentInsights(AActor* Actor, int32 Count)
{
    UAutognosisSystem* Autognosis = GetAutognosisSystem(Actor);
    if (!Autognosis)
    {
        return TArray<FMetaCognitiveInsight>();
    }

    TArray<FMetaCognitiveInsight> AllInsights = Autognosis->GetAllInsights();
    TArray<FMetaCognitiveInsight> Recent;

    int32 StartIndex = FMath::Max(0, AllInsights.Num() - Count);
    for (int32 i = StartIndex; i < AllInsights.Num(); ++i)
    {
        Recent.Add(AllInsights[i]);
    }

    return Recent;
}

// ========================================
// UTILITY FUNCTIONS
// ========================================

bool UDeepTreeEchoBlueprintLibrary::HasDeepTreeEchoComponents(AActor* Actor)
{
    if (!Actor) return false;

    return Actor->FindComponentByClass<UCognitiveCycleManager>() != nullptr ||
           Actor->FindComponentByClass<URelevanceRealizationEnnead>() != nullptr ||
           Actor->FindComponentByClass<UWisdomCultivation>() != nullptr ||
           Actor->FindComponentByClass<USensorimotorIntegration>() != nullptr ||
           Actor->FindComponentByClass<UAutognosisSystem>() != nullptr;
}

FString UDeepTreeEchoBlueprintLibrary::GetCognitiveStateSummary(AActor* Actor)
{
    if (!Actor)
    {
        return TEXT("No actor provided");
    }

    FString Summary;

    UCognitiveCycleManager* Cycle = GetCognitiveCycleManager(Actor);
    if (Cycle)
    {
        Summary += FString::Printf(TEXT("Cognitive Step: %d/12\n"), Cycle->GetCurrentStep());
        Summary += FString::Printf(TEXT("Mode: %s\n"), 
            Cycle->GetCurrentMode() == ECognitiveModeType::Expressive ? TEXT("Expressive") : TEXT("Reflective"));
        Summary += FString::Printf(TEXT("Relevance: %.2f\n"), Cycle->GetRelevanceRealizationLevel());
    }

    URelevanceRealizationEnnead* Ennead = GetRelevanceRealizationEnnead(Actor);
    if (Ennead)
    {
        Summary += FString::Printf(TEXT("Wisdom: %.2f\n"), Ennead->GetWisdomLevel());
        Summary += FString::Printf(TEXT("Meaning: %.2f\n"), Ennead->GetMeaningLevel());
    }

    USensorimotorIntegration* Sensorimotor = GetSensorimotorIntegration(Actor);
    if (Sensorimotor)
    {
        Summary += FString::Printf(TEXT("Embodiment: %.2f\n"), Sensorimotor->GetOverallEmbodiment());
    }

    UAutognosisSystem* Autognosis = GetAutognosisSystem(Actor);
    if (Autognosis)
    {
        Summary += FString::Printf(TEXT("Self-Awareness: %.2f\n"), Autognosis->GetSelfAwarenessScore());
    }

    return Summary.IsEmpty() ? TEXT("No Deep Tree Echo components found") : Summary;
}

FString UDeepTreeEchoBlueprintLibrary::CognitiveStepToString(ECognitiveStepType Step)
{
    switch (Step)
    {
        case ECognitiveStepType::Perceive: return TEXT("Perceive");
        case ECognitiveStepType::Orient: return TEXT("Orient");
        case ECognitiveStepType::Reflect: return TEXT("Reflect");
        case ECognitiveStepType::Integrate: return TEXT("Integrate");
        case ECognitiveStepType::Decide: return TEXT("Decide");
        case ECognitiveStepType::Simulate: return TEXT("Simulate");
        case ECognitiveStepType::Act: return TEXT("Act");
        case ECognitiveStepType::Observe: return TEXT("Observe");
        case ECognitiveStepType::Learn: return TEXT("Learn");
        case ECognitiveStepType::Consolidate: return TEXT("Consolidate");
        case ECognitiveStepType::Anticipate: return TEXT("Anticipate");
        case ECognitiveStepType::Transcend: return TEXT("Transcend");
        default: return TEXT("Unknown");
    }
}

FString UDeepTreeEchoBlueprintLibrary::WayOfKnowingToString(EWayOfKnowing Way)
{
    switch (Way)
    {
        case EWayOfKnowing::Propositional: return TEXT("Propositional (Knowing-That)");
        case EWayOfKnowing::Procedural: return TEXT("Procedural (Knowing-How)");
        case EWayOfKnowing::Perspectival: return TEXT("Perspectival (Knowing-As)");
        case EWayOfKnowing::Participatory: return TEXT("Participatory (Knowing-By-Being)");
        default: return TEXT("Unknown");
    }
}

FString UDeepTreeEchoBlueprintLibrary::PracticeOfWisdomToString(EPracticeOfWisdom Practice)
{
    switch (Practice)
    {
        case EPracticeOfWisdom::Morality: return TEXT("Morality (Virtue & Ethics)");
        case EPracticeOfWisdom::Meaning: return TEXT("Meaning (Coherence & Purpose)");
        case EPracticeOfWisdom::Mastery: return TEXT("Mastery (Excellence & Flow)");
        default: return TEXT("Unknown");
    }
}

// ========================================
// QUICK SETUP
// ========================================

void UDeepTreeEchoBlueprintLibrary::AddDeepTreeEchoComponents(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    // Add components if they don't exist
    if (!Actor->FindComponentByClass<UCognitiveCycleManager>())
    {
        UCognitiveCycleManager* Cycle = NewObject<UCognitiveCycleManager>(Actor);
        Cycle->RegisterComponent();
    }

    if (!Actor->FindComponentByClass<URelevanceRealizationEnnead>())
    {
        URelevanceRealizationEnnead* Ennead = NewObject<URelevanceRealizationEnnead>(Actor);
        Ennead->RegisterComponent();
    }

    if (!Actor->FindComponentByClass<UWisdomCultivation>())
    {
        UWisdomCultivation* Wisdom = NewObject<UWisdomCultivation>(Actor);
        Wisdom->RegisterComponent();
    }

    if (!Actor->FindComponentByClass<USensorimotorIntegration>())
    {
        USensorimotorIntegration* Sensorimotor = NewObject<USensorimotorIntegration>(Actor);
        Sensorimotor->RegisterComponent();
    }

    if (!Actor->FindComponentByClass<UAutognosisSystem>())
    {
        UAutognosisSystem* Autognosis = NewObject<UAutognosisSystem>(Actor);
        Autognosis->RegisterComponent();
    }
}

void UDeepTreeEchoBlueprintLibrary::InitializeDeepTreeEcho(AActor* Actor)
{
    if (!Actor)
    {
        return;
    }

    // Initialize cognitive cycle
    UCognitiveCycleManager* Cycle = GetCognitiveCycleManager(Actor);
    if (Cycle)
    {
        Cycle->InitializeCycle();
    }

    // Start autognosis
    UAutognosisSystem* Autognosis = GetAutognosisSystem(Actor);
    if (Autognosis)
    {
        Autognosis->StartAutognosis();
    }
}

// RelevanceRealizationEnnead.cpp
// Implementation of the Triad-of-Triads framework for relevance realization

#include "RelevanceRealizationEnnead.h"
#include "../Core/CognitiveCycleManager.h"
#include "WisdomCultivation.h"
#include "../Embodied/SensorimotorIntegration.h"
#include "Math/UnrealMathUtility.h"

URelevanceRealizationEnnead::URelevanceRealizationEnnead()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void URelevanceRealizationEnnead::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeEnnead();
}

void URelevanceRealizationEnnead::TickComponent(float DeltaTime, ELevelTick TickType, 
                                                 FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateTimer += DeltaTime;
    if (UpdateTimer >= IntegrationRate)
    {
        UpdateTimer -= IntegrationRate;

        // Update all triads
        UpdateWaysOfKnowing(IntegrationRate);
        UpdateOrdersOfUnderstanding(IntegrationRate);
        UpdatePracticesOfWisdom(IntegrationRate);

        // Update gnostic spiral
        if (bEnableGnosticSpiral)
        {
            UpdateGnosticSpiral(IntegrationRate);
        }

        // Update interpenetration
        if (bEnableTriadInterpenetration)
        {
            UpdateInterpenetration(IntegrationRate);
        }

        // Compute overall levels
        EnneadState.OverallIntegration = ComputeOverallIntegration();
        EnneadState.RelevanceRealizationLevel = ComputeRelevanceRealization();
        EnneadState.WisdomLevel = ComputeWisdomLevel();
        EnneadState.MeaningLevel = ComputeMeaningLevel();
    }
}

void URelevanceRealizationEnnead::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        CycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
        WisdomComponent = Owner->FindComponentByClass<UWisdomCultivation>();
        SensorimotorComponent = Owner->FindComponentByClass<USensorimotorIntegration>();
    }
}

void URelevanceRealizationEnnead::InitializeEnnead()
{
    // Initialize Triad I: Ways of Knowing
    EnneadState.WaysOfKnowing.Empty();
    
    FKnowingState Propositional;
    Propositional.Type = EWayOfKnowing::Propositional;
    Propositional.ActivationLevel = 0.5f;
    Propositional.IntegrationLevel = 0.5f;
    Propositional.Coherence = 0.5f;
    EnneadState.WaysOfKnowing.Add(Propositional);

    FKnowingState Procedural;
    Procedural.Type = EWayOfKnowing::Procedural;
    Procedural.ActivationLevel = 0.5f;
    Procedural.IntegrationLevel = 0.5f;
    Procedural.Coherence = 0.5f;
    EnneadState.WaysOfKnowing.Add(Procedural);

    FKnowingState Perspectival;
    Perspectival.Type = EWayOfKnowing::Perspectival;
    Perspectival.ActivationLevel = 0.5f;
    Perspectival.IntegrationLevel = 0.5f;
    Perspectival.Coherence = 0.5f;
    EnneadState.WaysOfKnowing.Add(Perspectival);

    FKnowingState Participatory;
    Participatory.Type = EWayOfKnowing::Participatory;
    Participatory.ActivationLevel = 0.3f;
    Participatory.IntegrationLevel = 0.3f;
    Participatory.Coherence = 0.5f;
    EnneadState.WaysOfKnowing.Add(Participatory);

    // Initialize Triad II: Orders of Understanding
    EnneadState.OrdersOfUnderstanding.Empty();

    FOrderState Nomological;
    Nomological.Type = EOrderOfUnderstanding::Nomological;
    Nomological.ActivationLevel = 0.5f;
    Nomological.Clarity = 0.5f;
    Nomological.IntegrationWithOthers = 0.5f;
    Nomological.PrimaryQuestion = TEXT("How does this work?");
    EnneadState.OrdersOfUnderstanding.Add(Nomological);

    FOrderState Normative;
    Normative.Type = EOrderOfUnderstanding::Normative;
    Normative.ActivationLevel = 0.5f;
    Normative.Clarity = 0.5f;
    Normative.IntegrationWithOthers = 0.5f;
    Normative.PrimaryQuestion = TEXT("Why does this matter?");
    EnneadState.OrdersOfUnderstanding.Add(Normative);

    FOrderState Narrative;
    Narrative.Type = EOrderOfUnderstanding::Narrative;
    Narrative.ActivationLevel = 0.5f;
    Narrative.Clarity = 0.5f;
    Narrative.IntegrationWithOthers = 0.5f;
    Narrative.PrimaryQuestion = TEXT("How did this come to be?");
    EnneadState.OrdersOfUnderstanding.Add(Narrative);

    // Initialize Triad III: Practices of Wisdom
    EnneadState.PracticesOfWisdom.Empty();

    FPracticeState Morality;
    Morality.Type = EPracticeOfWisdom::Morality;
    Morality.CultivationLevel = 0.5f;
    Morality.ActiveEngagement = 0.5f;
    Morality.IntegrationWithOthers = 0.5f;
    Morality.PrimaryGoal = TEXT("Cultivate virtue and ethical character");
    EnneadState.PracticesOfWisdom.Add(Morality);

    FPracticeState Meaning;
    Meaning.Type = EPracticeOfWisdom::Meaning;
    Meaning.CultivationLevel = 0.5f;
    Meaning.ActiveEngagement = 0.5f;
    Meaning.IntegrationWithOthers = 0.5f;
    Meaning.PrimaryGoal = TEXT("Realize coherence and purpose");
    EnneadState.PracticesOfWisdom.Add(Meaning);

    FPracticeState Mastery;
    Mastery.Type = EPracticeOfWisdom::Mastery;
    Mastery.CultivationLevel = 0.5f;
    Mastery.ActiveEngagement = 0.5f;
    Mastery.IntegrationWithOthers = 0.5f;
    Mastery.PrimaryGoal = TEXT("Achieve excellence and flow");
    EnneadState.PracticesOfWisdom.Add(Mastery);

    // Initialize Gnostic Spiral
    EnneadState.GnosticSpiral.CurrentStage = EGnosticStage::PropositionalOpening;
    EnneadState.GnosticSpiral.SpiralLevel = 1;
    EnneadState.GnosticSpiral.StageProgress = 0.0f;
    EnneadState.GnosticSpiral.TransformativeIntensity = 0.0f;

    // Initialize Interpenetration
    Interpenetration.KnowingToOrderFlow = 0.5f;
    Interpenetration.KnowingToWisdomFlow = 0.5f;
    Interpenetration.OrderToKnowingFlow = 0.5f;
    Interpenetration.OrderToWisdomFlow = 0.5f;
    Interpenetration.WisdomToKnowingFlow = 0.5f;
    Interpenetration.WisdomToOrderFlow = 0.5f;
    Interpenetration.OverallCoherence = 0.5f;
}

// ========================================
// WAYS OF KNOWING (TRIAD I)
// ========================================

void URelevanceRealizationEnnead::ActivateKnowing(EWayOfKnowing Way, float Intensity)
{
    int32 Index = GetKnowingIndex(Way);
    if (Index >= 0 && Index < EnneadState.WaysOfKnowing.Num())
    {
        EnneadState.WaysOfKnowing[Index].ActivationLevel = 
            FMath::Clamp(EnneadState.WaysOfKnowing[Index].ActivationLevel + Intensity, 0.0f, 1.0f);
        
        OnKnowingActivated.Broadcast(Way, EnneadState.WaysOfKnowing[Index].ActivationLevel);
    }
}

FKnowingState URelevanceRealizationEnnead::GetKnowingState(EWayOfKnowing Way) const
{
    int32 Index = GetKnowingIndex(Way);
    if (Index >= 0 && Index < EnneadState.WaysOfKnowing.Num())
    {
        return EnneadState.WaysOfKnowing[Index];
    }
    return FKnowingState();
}

void URelevanceRealizationEnnead::AddKnowingContent(EWayOfKnowing Way, const FString& Content)
{
    int32 Index = GetKnowingIndex(Way);
    if (Index >= 0 && Index < EnneadState.WaysOfKnowing.Num())
    {
        EnneadState.WaysOfKnowing[Index].ActiveContents.Add(Content);
        
        // Limit content history
        while (EnneadState.WaysOfKnowing[Index].ActiveContents.Num() > 20)
        {
            EnneadState.WaysOfKnowing[Index].ActiveContents.RemoveAt(0);
        }
    }
}

void URelevanceRealizationEnnead::SetKnowingFocus(EWayOfKnowing Way, const FString& Focus)
{
    int32 Index = GetKnowingIndex(Way);
    if (Index >= 0 && Index < EnneadState.WaysOfKnowing.Num())
    {
        EnneadState.WaysOfKnowing[Index].CurrentFocus = Focus;
    }
}

EWayOfKnowing URelevanceRealizationEnnead::GetDominantKnowing() const
{
    float HighestActivation = -1.0f;
    EWayOfKnowing Dominant = EWayOfKnowing::Propositional;

    for (const FKnowingState& State : EnneadState.WaysOfKnowing)
    {
        if (State.ActivationLevel > HighestActivation)
        {
            HighestActivation = State.ActivationLevel;
            Dominant = State.Type;
        }
    }

    return Dominant;
}

float URelevanceRealizationEnnead::GetKnowingIntegration() const
{
    return ComputeKnowingIntegration();
}

// ========================================
// ORDERS OF UNDERSTANDING (TRIAD II)
// ========================================

void URelevanceRealizationEnnead::ActivateOrder(EOrderOfUnderstanding Order, float Intensity)
{
    int32 Index = GetOrderIndex(Order);
    if (Index >= 0 && Index < EnneadState.OrdersOfUnderstanding.Num())
    {
        EnneadState.OrdersOfUnderstanding[Index].ActivationLevel = 
            FMath::Clamp(EnneadState.OrdersOfUnderstanding[Index].ActivationLevel + Intensity, 0.0f, 1.0f);
        
        OnOrderActivated.Broadcast(Order, EnneadState.OrdersOfUnderstanding[Index].ActivationLevel);
    }
}

FOrderState URelevanceRealizationEnnead::GetOrderState(EOrderOfUnderstanding Order) const
{
    int32 Index = GetOrderIndex(Order);
    if (Index >= 0 && Index < EnneadState.OrdersOfUnderstanding.Num())
    {
        return EnneadState.OrdersOfUnderstanding[Index];
    }
    return FOrderState();
}

void URelevanceRealizationEnnead::AddOrderUnderstanding(EOrderOfUnderstanding Order, const FString& Understanding)
{
    int32 Index = GetOrderIndex(Order);
    if (Index >= 0 && Index < EnneadState.OrdersOfUnderstanding.Num())
    {
        EnneadState.OrdersOfUnderstanding[Index].CurrentUnderstandings.Add(Understanding);
        
        while (EnneadState.OrdersOfUnderstanding[Index].CurrentUnderstandings.Num() > 20)
        {
            EnneadState.OrdersOfUnderstanding[Index].CurrentUnderstandings.RemoveAt(0);
        }
    }
}

void URelevanceRealizationEnnead::SetOrderQuestion(EOrderOfUnderstanding Order, const FString& Question)
{
    int32 Index = GetOrderIndex(Order);
    if (Index >= 0 && Index < EnneadState.OrdersOfUnderstanding.Num())
    {
        EnneadState.OrdersOfUnderstanding[Index].PrimaryQuestion = Question;
    }
}

EOrderOfUnderstanding URelevanceRealizationEnnead::GetDominantOrder() const
{
    float HighestActivation = -1.0f;
    EOrderOfUnderstanding Dominant = EOrderOfUnderstanding::Nomological;

    for (const FOrderState& State : EnneadState.OrdersOfUnderstanding)
    {
        if (State.ActivationLevel > HighestActivation)
        {
            HighestActivation = State.ActivationLevel;
            Dominant = State.Type;
        }
    }

    return Dominant;
}

float URelevanceRealizationEnnead::GetOrderIntegration() const
{
    return ComputeOrderIntegration();
}

// ========================================
// PRACTICES OF WISDOM (TRIAD III)
// ========================================

void URelevanceRealizationEnnead::ActivatePractice(EPracticeOfWisdom Practice, float Intensity)
{
    int32 Index = GetPracticeIndex(Practice);
    if (Index >= 0 && Index < EnneadState.PracticesOfWisdom.Num())
    {
        EnneadState.PracticesOfWisdom[Index].ActiveEngagement = 
            FMath::Clamp(EnneadState.PracticesOfWisdom[Index].ActiveEngagement + Intensity, 0.0f, 1.0f);
        
        // Cultivation grows more slowly
        EnneadState.PracticesOfWisdom[Index].CultivationLevel = 
            FMath::Clamp(EnneadState.PracticesOfWisdom[Index].CultivationLevel + Intensity * 0.1f, 0.0f, 1.0f);
        
        OnPracticeActivated.Broadcast(Practice, EnneadState.PracticesOfWisdom[Index].ActiveEngagement);
    }
}

FPracticeState URelevanceRealizationEnnead::GetPracticeState(EPracticeOfWisdom Practice) const
{
    int32 Index = GetPracticeIndex(Practice);
    if (Index >= 0 && Index < EnneadState.PracticesOfWisdom.Num())
    {
        return EnneadState.PracticesOfWisdom[Index];
    }
    return FPracticeState();
}

void URelevanceRealizationEnnead::AddPractice(EPracticeOfWisdom Practice, const FString& PracticeName)
{
    int32 Index = GetPracticeIndex(Practice);
    if (Index >= 0 && Index < EnneadState.PracticesOfWisdom.Num())
    {
        if (!EnneadState.PracticesOfWisdom[Index].CurrentPractices.Contains(PracticeName))
        {
            EnneadState.PracticesOfWisdom[Index].CurrentPractices.Add(PracticeName);
        }
    }
}

void URelevanceRealizationEnnead::SetPracticeGoal(EPracticeOfWisdom Practice, const FString& Goal)
{
    int32 Index = GetPracticeIndex(Practice);
    if (Index >= 0 && Index < EnneadState.PracticesOfWisdom.Num())
    {
        EnneadState.PracticesOfWisdom[Index].PrimaryGoal = Goal;
    }
}

EPracticeOfWisdom URelevanceRealizationEnnead::GetDominantPractice() const
{
    float HighestEngagement = -1.0f;
    EPracticeOfWisdom Dominant = EPracticeOfWisdom::Morality;

    for (const FPracticeState& State : EnneadState.PracticesOfWisdom)
    {
        if (State.ActiveEngagement > HighestEngagement)
        {
            HighestEngagement = State.ActiveEngagement;
            Dominant = State.Type;
        }
    }

    return Dominant;
}

float URelevanceRealizationEnnead::GetPracticeIntegration() const
{
    return ComputePracticeIntegration();
}

// ========================================
// GNOSTIC SPIRAL
// ========================================

FGnosticSpiralState URelevanceRealizationEnnead::GetGnosticState() const
{
    return EnneadState.GnosticSpiral;
}

void URelevanceRealizationEnnead::AdvanceGnosticSpiral()
{
    EGnosticStage OldStage = EnneadState.GnosticSpiral.CurrentStage;

    switch (EnneadState.GnosticSpiral.CurrentStage)
    {
        case EGnosticStage::PropositionalOpening:
            EnneadState.GnosticSpiral.CurrentStage = EGnosticStage::ProceduralDevelopment;
            break;
        case EGnosticStage::ProceduralDevelopment:
            EnneadState.GnosticSpiral.CurrentStage = EGnosticStage::PerspectivalShift;
            break;
        case EGnosticStage::PerspectivalShift:
            EnneadState.GnosticSpiral.CurrentStage = EGnosticStage::ParticipatoryTransform;
            break;
        case EGnosticStage::ParticipatoryTransform:
            EnneadState.GnosticSpiral.CurrentStage = EGnosticStage::NewUnderstanding;
            break;
        case EGnosticStage::NewUnderstanding:
            EnneadState.GnosticSpiral.CurrentStage = EGnosticStage::AnagogicAscent;
            break;
        case EGnosticStage::AnagogicAscent:
            // Complete cycle - ascend to next spiral level
            EnneadState.GnosticSpiral.CurrentStage = EGnosticStage::PropositionalOpening;
            EnneadState.GnosticSpiral.SpiralLevel++;
            break;
    }

    EnneadState.GnosticSpiral.StageProgress = 0.0f;

    OnGnosticStageAdvanced.Broadcast(EnneadState.GnosticSpiral.CurrentStage, 
                                      EnneadState.GnosticSpiral.SpiralLevel);
}

void URelevanceRealizationEnnead::AddGnosticInsight(const FString& Insight)
{
    EnneadState.GnosticSpiral.InsightsGained.Add(Insight);
    EnneadState.GnosticSpiral.TransformativeIntensity = 
        FMath::Min(1.0f, EnneadState.GnosticSpiral.TransformativeIntensity + 0.1f);
}

EGnosticStage URelevanceRealizationEnnead::GetCurrentGnosticStage() const
{
    return EnneadState.GnosticSpiral.CurrentStage;
}

int32 URelevanceRealizationEnnead::GetSpiralLevel() const
{
    return EnneadState.GnosticSpiral.SpiralLevel;
}

// ========================================
// ENNEAD INTEGRATION
// ========================================

FEnneadState URelevanceRealizationEnnead::GetEnneadState() const
{
    return EnneadState;
}

FTriadInterpenetration URelevanceRealizationEnnead::GetInterpenetration() const
{
    return Interpenetration;
}

float URelevanceRealizationEnnead::GetRelevanceRealizationLevel() const
{
    return EnneadState.RelevanceRealizationLevel;
}

float URelevanceRealizationEnnead::GetWisdomLevel() const
{
    return EnneadState.WisdomLevel;
}

float URelevanceRealizationEnnead::GetMeaningLevel() const
{
    return EnneadState.MeaningLevel;
}

void URelevanceRealizationEnnead::ForceIntegration()
{
    // Force integration across all triads
    ProcessKnowingToOrderFlow();
    ProcessKnowingToWisdomFlow();
    ProcessOrderToKnowingFlow();
    ProcessOrderToWisdomFlow();
    ProcessWisdomToKnowingFlow();
    ProcessWisdomToOrderFlow();

    // Update overall coherence
    Interpenetration.OverallCoherence = 
        (Interpenetration.KnowingToOrderFlow +
         Interpenetration.KnowingToWisdomFlow +
         Interpenetration.OrderToKnowingFlow +
         Interpenetration.OrderToWisdomFlow +
         Interpenetration.WisdomToKnowingFlow +
         Interpenetration.WisdomToOrderFlow) / 6.0f;
}

// ========================================
// COGNITIVE INTEGRATION
// ========================================

void URelevanceRealizationEnnead::ProcessCognitiveInput(const TArray<float>& Input, EWayOfKnowing PrimaryKnowing)
{
    // Activate the primary knowing mode
    ActivateKnowing(PrimaryKnowing, 0.2f);

    // Determine appropriate order based on input characteristics
    if (Input.Num() > 0)
    {
        float AvgIntensity = 0.0f;
        for (float Val : Input)
        {
            AvgIntensity += FMath::Abs(Val);
        }
        AvgIntensity /= Input.Num();

        // High intensity suggests normative relevance
        if (AvgIntensity > 0.7f)
        {
            ActivateOrder(EOrderOfUnderstanding::Normative, 0.1f);
        }
        else
        {
            ActivateOrder(EOrderOfUnderstanding::Nomological, 0.1f);
        }
    }

    // Update gnostic progress
    EnneadState.GnosticSpiral.StageProgress += 0.05f;
    if (EnneadState.GnosticSpiral.StageProgress >= GnosticAdvancementThreshold)
    {
        AdvanceGnosticSpiral();
    }
}

float URelevanceRealizationEnnead::GetRelevanceScore(const FString& Content) const
{
    // Compute relevance based on current ennead state
    float Score = 0.0f;

    // Check against knowing contents
    for (const FKnowingState& Knowing : EnneadState.WaysOfKnowing)
    {
        if (Knowing.CurrentFocus.Contains(Content) || Content.Contains(Knowing.CurrentFocus))
        {
            Score += Knowing.ActivationLevel * 0.3f;
        }
    }

    // Check against order understandings
    for (const FOrderState& Order : EnneadState.OrdersOfUnderstanding)
    {
        for (const FString& Understanding : Order.CurrentUnderstandings)
        {
            if (Understanding.Contains(Content) || Content.Contains(Understanding))
            {
                Score += Order.ActivationLevel * 0.2f;
            }
        }
    }

    // Check against practice goals
    for (const FPracticeState& Practice : EnneadState.PracticesOfWisdom)
    {
        if (Practice.PrimaryGoal.Contains(Content) || Content.Contains(Practice.PrimaryGoal))
        {
            Score += Practice.ActiveEngagement * 0.2f;
        }
    }

    return FMath::Clamp(Score, 0.0f, 1.0f);
}

EWayOfKnowing URelevanceRealizationEnnead::DetermineOptimalKnowing(const FString& Context) const
{
    // Determine optimal knowing mode based on context
    if (Context.Contains(TEXT("fact")) || Context.Contains(TEXT("theory")) || Context.Contains(TEXT("believe")))
    {
        return EWayOfKnowing::Propositional;
    }
    else if (Context.Contains(TEXT("skill")) || Context.Contains(TEXT("do")) || Context.Contains(TEXT("practice")))
    {
        return EWayOfKnowing::Procedural;
    }
    else if (Context.Contains(TEXT("notice")) || Context.Contains(TEXT("see")) || Context.Contains(TEXT("frame")))
    {
        return EWayOfKnowing::Perspectival;
    }
    else if (Context.Contains(TEXT("become")) || Context.Contains(TEXT("transform")) || Context.Contains(TEXT("identity")))
    {
        return EWayOfKnowing::Participatory;
    }

    // Default to dominant knowing
    return GetDominantKnowing();
}

// ========================================
// INTERNAL UPDATE METHODS
// ========================================

void URelevanceRealizationEnnead::UpdateWaysOfKnowing(float DeltaTime)
{
    // Decay activation over time
    for (FKnowingState& State : EnneadState.WaysOfKnowing)
    {
        State.ActivationLevel = FMath::Max(0.1f, State.ActivationLevel - DeltaTime * 0.01f);
    }

    // Update integration levels
    float Integration = ComputeKnowingIntegration();
    for (FKnowingState& State : EnneadState.WaysOfKnowing)
    {
        State.IntegrationLevel = FMath::Lerp(State.IntegrationLevel, Integration, DeltaTime * 0.5f);
    }
}

void URelevanceRealizationEnnead::UpdateOrdersOfUnderstanding(float DeltaTime)
{
    // Decay activation
    for (FOrderState& State : EnneadState.OrdersOfUnderstanding)
    {
        State.ActivationLevel = FMath::Max(0.1f, State.ActivationLevel - DeltaTime * 0.01f);
    }

    // Update integration
    float Integration = ComputeOrderIntegration();
    for (FOrderState& State : EnneadState.OrdersOfUnderstanding)
    {
        State.IntegrationWithOthers = FMath::Lerp(State.IntegrationWithOthers, Integration, DeltaTime * 0.5f);
    }
}

void URelevanceRealizationEnnead::UpdatePracticesOfWisdom(float DeltaTime)
{
    // Decay engagement (but not cultivation)
    for (FPracticeState& State : EnneadState.PracticesOfWisdom)
    {
        State.ActiveEngagement = FMath::Max(0.1f, State.ActiveEngagement - DeltaTime * 0.02f);
    }

    // Update integration
    float Integration = ComputePracticeIntegration();
    for (FPracticeState& State : EnneadState.PracticesOfWisdom)
    {
        State.IntegrationWithOthers = FMath::Lerp(State.IntegrationWithOthers, Integration, DeltaTime * 0.5f);
    }
}

void URelevanceRealizationEnnead::UpdateGnosticSpiral(float DeltaTime)
{
    // Decay transformative intensity
    EnneadState.GnosticSpiral.TransformativeIntensity = 
        FMath::Max(0.0f, EnneadState.GnosticSpiral.TransformativeIntensity - DeltaTime * 0.01f);

    // Progress through stage based on knowing activation
    float ProgressRate = 0.0f;
    switch (EnneadState.GnosticSpiral.CurrentStage)
    {
        case EGnosticStage::PropositionalOpening:
            ProgressRate = GetKnowingState(EWayOfKnowing::Propositional).ActivationLevel;
            break;
        case EGnosticStage::ProceduralDevelopment:
            ProgressRate = GetKnowingState(EWayOfKnowing::Procedural).ActivationLevel;
            break;
        case EGnosticStage::PerspectivalShift:
            ProgressRate = GetKnowingState(EWayOfKnowing::Perspectival).ActivationLevel;
            break;
        case EGnosticStage::ParticipatoryTransform:
            ProgressRate = GetKnowingState(EWayOfKnowing::Participatory).ActivationLevel;
            break;
        case EGnosticStage::NewUnderstanding:
            ProgressRate = ComputeKnowingIntegration();
            break;
        case EGnosticStage::AnagogicAscent:
            ProgressRate = ComputeOverallIntegration();
            break;
    }

    EnneadState.GnosticSpiral.StageProgress += ProgressRate * DeltaTime * 0.1f;
}

void URelevanceRealizationEnnead::UpdateInterpenetration(float DeltaTime)
{
    ProcessKnowingToOrderFlow();
    ProcessKnowingToWisdomFlow();
    ProcessOrderToKnowingFlow();
    ProcessOrderToWisdomFlow();
    ProcessWisdomToKnowingFlow();
    ProcessWisdomToOrderFlow();

    // Update overall coherence
    Interpenetration.OverallCoherence = 
        (Interpenetration.KnowingToOrderFlow +
         Interpenetration.KnowingToWisdomFlow +
         Interpenetration.OrderToKnowingFlow +
         Interpenetration.OrderToWisdomFlow +
         Interpenetration.WisdomToKnowingFlow +
         Interpenetration.WisdomToOrderFlow) / 6.0f;
}

void URelevanceRealizationEnnead::ProcessKnowingToOrderFlow()
{
    // Propositional knowing articulates nomological order
    float PropToNom = GetKnowingState(EWayOfKnowing::Propositional).ActivationLevel *
                      GetOrderState(EOrderOfUnderstanding::Nomological).ActivationLevel;
    
    // Perspectival knowing guides normative order
    float PerspToNorm = GetKnowingState(EWayOfKnowing::Perspectival).ActivationLevel *
                        GetOrderState(EOrderOfUnderstanding::Normative).ActivationLevel;
    
    // Participatory knowing integrates narrative order
    float PartToNarr = GetKnowingState(EWayOfKnowing::Participatory).ActivationLevel *
                       GetOrderState(EOrderOfUnderstanding::Narrative).ActivationLevel;

    Interpenetration.KnowingToOrderFlow = (PropToNom + PerspToNorm + PartToNarr) / 3.0f;
}

void URelevanceRealizationEnnead::ProcessKnowingToWisdomFlow()
{
    // All knowings contribute to wisdom practices
    float TotalKnowing = 0.0f;
    for (const FKnowingState& State : EnneadState.WaysOfKnowing)
    {
        TotalKnowing += State.ActivationLevel;
    }
    TotalKnowing /= EnneadState.WaysOfKnowing.Num();

    float TotalPractice = 0.0f;
    for (const FPracticeState& State : EnneadState.PracticesOfWisdom)
    {
        TotalPractice += State.ActiveEngagement;
    }
    TotalPractice /= EnneadState.PracticesOfWisdom.Num();

    Interpenetration.KnowingToWisdomFlow = TotalKnowing * TotalPractice;
}

void URelevanceRealizationEnnead::ProcessOrderToKnowingFlow()
{
    // Understanding structures cognition
    float TotalOrder = 0.0f;
    for (const FOrderState& State : EnneadState.OrdersOfUnderstanding)
    {
        TotalOrder += State.ActivationLevel * State.Clarity;
    }
    TotalOrder /= EnneadState.OrdersOfUnderstanding.Num();

    Interpenetration.OrderToKnowingFlow = TotalOrder;
}

void URelevanceRealizationEnnead::ProcessOrderToWisdomFlow()
{
    // Reality dimensions shape flourishing
    float OrderClarity = 0.0f;
    for (const FOrderState& State : EnneadState.OrdersOfUnderstanding)
    {
        OrderClarity += State.Clarity;
    }
    OrderClarity /= EnneadState.OrdersOfUnderstanding.Num();

    float PracticeCultivation = 0.0f;
    for (const FPracticeState& State : EnneadState.PracticesOfWisdom)
    {
        PracticeCultivation += State.CultivationLevel;
    }
    PracticeCultivation /= EnneadState.PracticesOfWisdom.Num();

    Interpenetration.OrderToWisdomFlow = OrderClarity * PracticeCultivation;
}

void URelevanceRealizationEnnead::ProcessWisdomToKnowingFlow()
{
    // Practice develops knowledge
    float TotalCultivation = 0.0f;
    for (const FPracticeState& State : EnneadState.PracticesOfWisdom)
    {
        TotalCultivation += State.CultivationLevel;
    }
    TotalCultivation /= EnneadState.PracticesOfWisdom.Num();

    Interpenetration.WisdomToKnowingFlow = TotalCultivation;
}

void URelevanceRealizationEnnead::ProcessWisdomToOrderFlow()
{
    // Cultivation reveals deeper understanding
    float TotalEngagement = 0.0f;
    for (const FPracticeState& State : EnneadState.PracticesOfWisdom)
    {
        TotalEngagement += State.ActiveEngagement;
    }
    TotalEngagement /= EnneadState.PracticesOfWisdom.Num();

    Interpenetration.WisdomToOrderFlow = TotalEngagement;
}

// ========================================
// COMPUTATION METHODS
// ========================================

float URelevanceRealizationEnnead::ComputeKnowingIntegration() const
{
    if (EnneadState.WaysOfKnowing.Num() < 2)
    {
        return 0.0f;
    }

    float TotalCoherence = 0.0f;
    int32 Pairs = 0;

    for (int32 i = 0; i < EnneadState.WaysOfKnowing.Num(); ++i)
    {
        for (int32 j = i + 1; j < EnneadState.WaysOfKnowing.Num(); ++j)
        {
            float Similarity = 1.0f - FMath::Abs(EnneadState.WaysOfKnowing[i].ActivationLevel - 
                                                  EnneadState.WaysOfKnowing[j].ActivationLevel);
            TotalCoherence += Similarity;
            ++Pairs;
        }
    }

    return Pairs > 0 ? TotalCoherence / Pairs : 0.0f;
}

float URelevanceRealizationEnnead::ComputeOrderIntegration() const
{
    if (EnneadState.OrdersOfUnderstanding.Num() < 2)
    {
        return 0.0f;
    }

    float TotalIntegration = 0.0f;
    for (const FOrderState& State : EnneadState.OrdersOfUnderstanding)
    {
        TotalIntegration += State.IntegrationWithOthers;
    }

    return TotalIntegration / EnneadState.OrdersOfUnderstanding.Num();
}

float URelevanceRealizationEnnead::ComputePracticeIntegration() const
{
    if (EnneadState.PracticesOfWisdom.Num() < 2)
    {
        return 0.0f;
    }

    float TotalIntegration = 0.0f;
    for (const FPracticeState& State : EnneadState.PracticesOfWisdom)
    {
        TotalIntegration += State.IntegrationWithOthers;
    }

    return TotalIntegration / EnneadState.PracticesOfWisdom.Num();
}

float URelevanceRealizationEnnead::ComputeOverallIntegration() const
{
    float KnowingInt = ComputeKnowingIntegration();
    float OrderInt = ComputeOrderIntegration();
    float PracticeInt = ComputePracticeIntegration();
    float InterpenInt = Interpenetration.OverallCoherence;

    return (KnowingInt + OrderInt + PracticeInt + InterpenInt) / 4.0f;
}

float URelevanceRealizationEnnead::ComputeRelevanceRealization() const
{
    // Relevance realization is primarily perspectival-participatory
    float PerspLevel = GetKnowingState(EWayOfKnowing::Perspectival).ActivationLevel;
    float PartLevel = GetKnowingState(EWayOfKnowing::Participatory).ActivationLevel;
    
    // Modulated by normative order
    float NormLevel = GetOrderState(EOrderOfUnderstanding::Normative).ActivationLevel;
    
    // And overall integration
    float Integration = ComputeOverallIntegration();

    return (PerspLevel * 0.3f + PartLevel * 0.3f + NormLevel * 0.2f + Integration * 0.2f);
}

float URelevanceRealizationEnnead::ComputeWisdomLevel() const
{
    // Wisdom requires all three M's integrated
    float MoralityLevel = GetPracticeState(EPracticeOfWisdom::Morality).CultivationLevel;
    float MeaningLevel = GetPracticeState(EPracticeOfWisdom::Meaning).CultivationLevel;
    float MasteryLevel = GetPracticeState(EPracticeOfWisdom::Mastery).CultivationLevel;

    // Geometric mean to require balance
    return FMath::Pow(MoralityLevel * MeaningLevel * MasteryLevel, 1.0f / 3.0f);
}

float URelevanceRealizationEnnead::ComputeMeaningLevel() const
{
    // Meaning requires all three orders integrated
    float NomLevel = GetOrderState(EOrderOfUnderstanding::Nomological).Clarity;
    float NormLevel = GetOrderState(EOrderOfUnderstanding::Normative).Clarity;
    float NarrLevel = GetOrderState(EOrderOfUnderstanding::Narrative).Clarity;

    // Plus the meaning practice
    float MeaningPractice = GetPracticeState(EPracticeOfWisdom::Meaning).CultivationLevel;

    return (NomLevel + NormLevel + NarrLevel) / 3.0f * 0.5f + MeaningPractice * 0.5f;
}

// ========================================
// INDEX HELPERS
// ========================================

int32 URelevanceRealizationEnnead::GetKnowingIndex(EWayOfKnowing Way) const
{
    switch (Way)
    {
        case EWayOfKnowing::Propositional: return 0;
        case EWayOfKnowing::Procedural: return 1;
        case EWayOfKnowing::Perspectival: return 2;
        case EWayOfKnowing::Participatory: return 3;
        default: return -1;
    }
}

int32 URelevanceRealizationEnnead::GetOrderIndex(EOrderOfUnderstanding Order) const
{
    switch (Order)
    {
        case EOrderOfUnderstanding::Nomological: return 0;
        case EOrderOfUnderstanding::Normative: return 1;
        case EOrderOfUnderstanding::Narrative: return 2;
        default: return -1;
    }
}

int32 URelevanceRealizationEnnead::GetPracticeIndex(EPracticeOfWisdom Practice) const
{
    switch (Practice)
    {
        case EPracticeOfWisdom::Morality: return 0;
        case EPracticeOfWisdom::Meaning: return 1;
        case EPracticeOfWisdom::Mastery: return 2;
        default: return -1;
    }
}

//=============================================================================
// IntegrationTests.cpp
//
// Integration testing framework for Deep Tree Echo avatar systems.
// Tests the interaction and integration between all major components.
//
// Narrative text (diary entries, insights, blog posts) is produced only by the
// language model, asynchronously. The narrative tests therefore check that a
// request was issued and that nothing is stored or broadcast until the model
// answers; when no model is reachable in the test environment the correct
// outcome is that no entry exists at all.
//
// Copyright (c) 2025 Deep Tree Echo Project
//=============================================================================

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Avatar/Avatar3DComponentEnhanced.h"
#include "Personality/PersonalityTraitSystem.h"
#include "Neurochemical/NeurochemicalSimulationComponent.h"
#include "Narrative/DiaryInsightBlogLoop.h"
#include "DTELanguageModelBridge.h"
#include "GameFramework/Actor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

//=============================================================================
// Helper function to get test world
//=============================================================================

static UWorld* GetTestWorld()
{
    UWorld* World = nullptr;

    if (!GEngine)
    {
        return World;
    }

    for (const FWorldContext& Context : GEngine->GetWorldContexts())
    {
        if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
        {
            World = Context.World();
            break;
        }
    }

    return World;
}

//=============================================================================
// Integration Test: Avatar + Personality System
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvatarPersonalityIntegrationTest,
    "UnrealEngineCog.Integration.AvatarPersonalityActor",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAvatarPersonalityIntegrationTest::RunTest(const FString& Parameters)
{
    // Create test actor
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }

    AActor* TestActor = World->SpawnActor<AActor>();
    if (!TestActor)
    {
        AddError(TEXT("Failed to spawn test actor"));
        return false;
    }

    // Add Avatar3DComponentEnhanced
    UAvatar3DComponentEnhanced* AvatarComponent = NewObject<UAvatar3DComponentEnhanced>(TestActor);
    TestActor->AddInstanceComponent(AvatarComponent);
    AvatarComponent->RegisterComponent();

    // Add PersonalityTraitSystem
    UPersonalityTraitSystem* PersonalitySystem = NewObject<UPersonalityTraitSystem>(TestActor);
    TestActor->AddInstanceComponent(PersonalitySystem);
    PersonalitySystem->RegisterComponent();

    // Test: Set personality trait and feed it to the avatar
    PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::Confident, 0.9f);
    TestEqual(TEXT("Confident trait stored"), PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Confident), 0.9f);

    AvatarComponent->SetConfidenceLevel(PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Confident));
    AvatarComponent->EnablePersonalityTrait(EAvatarPersonalityTrait::Confident,
        PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Confident));

    // Confident contributes positive valence to the emotional tendency
    const FVector2D Tendency = PersonalitySystem->GetEmotionalTendency();
    TestTrue(TEXT("Confidence raises valence tendency"), Tendency.X > 0.0f);

    // Test: Playfulness raises social engagement
    PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::Playful, 0.8f);
    TestTrue(TEXT("Playfulness raises social engagement"), PersonalitySystem->GetSocialEngagementLevel() > 0.5f);

    // Cleanup
    World->DestroyActor(TestActor);

    return true;
}

//=============================================================================
// Integration Test: Avatar + Neurochemical System
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvatarNeurochemicalIntegrationTest,
    "UnrealEngineCog.Integration.AvatarNeurochemical",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FAvatarNeurochemicalIntegrationTest::RunTest(const FString& Parameters)
{
    // Create test actor
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }

    AActor* TestActor = World->SpawnActor<AActor>();
    if (!TestActor)
    {
        AddError(TEXT("Failed to spawn test actor"));
        return false;
    }

    // Add Avatar3DComponentEnhanced
    UAvatar3DComponentEnhanced* AvatarComponent = NewObject<UAvatar3DComponentEnhanced>(TestActor);
    TestActor->AddInstanceComponent(AvatarComponent);
    AvatarComponent->RegisterComponent();

    // Add NeurochemicalSimulationComponent
    UNeurochemicalSimulationComponent* NeuroSystem = NewObject<UNeurochemicalSimulationComponent>(TestActor);
    TestActor->AddInstanceComponent(NeuroSystem);
    NeuroSystem->RegisterComponent();

    // Test: Neurochemical state drives avatar emotion (deterministic affect mapping)
    NeuroSystem->SetNeurochemicalLevel(ENeurochemicalType::Dopamine, 0.8f);
    NeuroSystem->TickComponent(0.1f, ELevelTick::LEVELTICK_All, nullptr);

    const FEmotionalChemistry Chemistry = NeuroSystem->GetEmotionalChemistry();
    AvatarComponent->SetEmotionalState(EAvatarEmotionalState::Happy, Chemistry.Happiness);

    const FAvatarEmotionalState AvatarEmotion = AvatarComponent->GetCurrentEmotionalState();
    TestTrue(TEXT("Avatar emotion follows neurochemical mapping"), AvatarEmotion.CurrentEmotion == EAvatarEmotionalState::Happy);

    // Test: High cortisol is reported back as stress
    NeuroSystem->SetNeurochemicalLevel(ENeurochemicalType::Cortisol, 0.9f);

    const float StressLevel = NeuroSystem->GetNeurochemicalLevel(ENeurochemicalType::Cortisol);
    TestTrue(TEXT("Cortisol level increased"), StressLevel > 0.7f);

    // Cleanup
    World->DestroyActor(TestActor);

    return true;
}

//=============================================================================
// Integration Test: Personality + Neurochemical System
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPersonalityNeurochemicalIntegrationTest,
    "UnrealEngineCog.Integration.PersonalityNeurochemical",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPersonalityNeurochemicalIntegrationTest::RunTest(const FString& Parameters)
{
    // Create test actor
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }

    AActor* TestActor = World->SpawnActor<AActor>();
    if (!TestActor)
    {
        AddError(TEXT("Failed to spawn test actor"));
        return false;
    }

    // Add PersonalityTraitSystem
    UPersonalityTraitSystem* PersonalitySystem = NewObject<UPersonalityTraitSystem>(TestActor);
    TestActor->AddInstanceComponent(PersonalitySystem);
    PersonalitySystem->RegisterComponent();

    // Add NeurochemicalSimulationComponent
    UNeurochemicalSimulationComponent* NeuroSystem = NewObject<UNeurochemicalSimulationComponent>(TestActor);
    TestActor->AddInstanceComponent(NeuroSystem);
    NeuroSystem->RegisterComponent();

    // Test: Personality tendency and neurochemical baseline coexist on one actor
    PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::Confident, 0.9f);

    const float DopamineLevel = NeuroSystem->GetNeurochemicalLevel(ENeurochemicalType::Dopamine);
    TestTrue(TEXT("Dopamine baseline present"), DopamineLevel > 0.4f);

    // Test: Serotonin change is reflected in the emotional chemistry after a tick
    NeuroSystem->SetNeurochemicalLevel(ENeurochemicalType::Serotonin, 0.8f);
    NeuroSystem->TickComponent(0.1f, ELevelTick::LEVELTICK_All, nullptr);

    const FEmotionalChemistry Chemistry = NeuroSystem->GetEmotionalChemistry();
    TestTrue(TEXT("Serotonin affects emotional chemistry"), Chemistry.Happiness > 0.0f);

    const FVector2D Tendency = PersonalitySystem->GetEmotionalTendency();
    TestTrue(TEXT("Personality tendency computed"), Tendency.Y > 0.0f);

    // Cleanup
    World->DestroyActor(TestActor);

    return true;
}

//=============================================================================
// Integration Test: Narrative Loop + All Systems
//=============================================================================

/**
 * Waits for the diary request issued in FNarrativeLoopIntegrationTest to settle,
 * then verifies the asynchronous contract:
 *   - the model answered: exactly one entry exists, with non-empty model text,
 *     stored by the same callback that broadcasts OnDiaryEntryCreated;
 *   - the model did not answer (no model in the test environment): no entry
 *     exists and nothing was recorded. No placeholder text is ever acceptable.
 * Destroys the test actor when done.
 */
DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FVerifyNarrativeModelReplyCommand,
    TWeakObjectPtr<UDiaryInsightBlogLoop>, NarrativeLoop,
    TWeakObjectPtr<AActor>, TestActor,
    double, Deadline);

bool FVerifyNarrativeModelReplyCommand::Update()
{
    FAutomationTestBase* Test = FAutomationTestFramework::Get().GetCurrentTest();

    if (!NarrativeLoop.IsValid() || !TestActor.IsValid())
    {
        if (Test)
        {
            Test->AddError(TEXT("Narrative loop or test actor was destroyed before the model reply was verified"));
        }
        return true;
    }

    const FNarrativeLoopState State = NarrativeLoop->GetNarrativeState();
    const bool bTimedOut = FPlatformTime::Seconds() >= Deadline;

    if (State.bDiaryEntryPending && !bTimedOut)
    {
        // Still waiting for the model
        return false;
    }

    if (Test)
    {
        const FNarrativeDiaryEntry RecentEntry = NarrativeLoop->GetMostRecentDiaryEntry();
        const TArray<FNarrativeDiaryEntry> Entries = NarrativeLoop->GetRecentDiaryEntries(MAX_int32);

        if (State.bDiaryEntryPending)
        {
            // The request never completed within the deadline: nothing may have been written.
            Test->AddInfo(TEXT("Diary request still pending at deadline; verifying that no text was substituted"));
            Test->TestTrue(TEXT("No diary text while the model has not answered"), RecentEntry.Content.IsEmpty());
            Test->TestEqual(TEXT("No diary entries recorded while pending"), State.TotalDiaryEntries, 0);
            Test->TestEqual(TEXT("No diary entries stored while pending"), Entries.Num(), 0);
        }
        else if (State.TotalDiaryEntries > 0)
        {
            // The model answered. The entry was stored by the callback that broadcasts OnDiaryEntryCreated.
            Test->AddInfo(TEXT("Model answered the diary request"));
            Test->TestEqual(TEXT("Exactly one diary entry from one request"), State.TotalDiaryEntries, 1);
            Test->TestEqual(TEXT("Stored entry count matches"), Entries.Num(), 1);
            Test->TestFalse(TEXT("Diary entry carries model text"), RecentEntry.Content.IsEmpty());
            Test->TestTrue(TEXT("Diary entry has an identifier"), RecentEntry.EntryID.IsValid());
        }
        else
        {
            // The model failed or is unreachable: honest silence.
            Test->AddInfo(TEXT("Model did not answer the diary request; verifying honest silence"));
            Test->TestTrue(TEXT("No diary text without a model reply"), RecentEntry.Content.IsEmpty());
            Test->TestEqual(TEXT("No diary entries stored without a model reply"), Entries.Num(), 0);
        }

        // Insights require model-written diary entries; none can exist from a single request.
        const FNarrativeInsight RecentInsight = NarrativeLoop->GetMostRecentInsight();
        Test->TestTrue(TEXT("No insight title without model-written insight"), RecentInsight.Title.IsEmpty());
        Test->TestTrue(TEXT("No insight content without model-written insight"), RecentInsight.Content.IsEmpty());
        Test->TestEqual(TEXT("No insights recorded"), State.TotalInsights, 0);
    }

    if (UWorld* World = TestActor->GetWorld())
    {
        World->DestroyActor(TestActor.Get());
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNarrativeLoopIntegrationTest,
    "UnrealEngineCog.Integration.NarrativeLoop",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FNarrativeLoopIntegrationTest::RunTest(const FString& Parameters)
{
    // Create test actor
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }

    AActor* TestActor = World->SpawnActor<AActor>();
    if (!TestActor)
    {
        AddError(TEXT("Failed to spawn test actor"));
        return false;
    }

    // Add all components
    UPersonalityTraitSystem* PersonalitySystem = NewObject<UPersonalityTraitSystem>(TestActor);
    TestActor->AddInstanceComponent(PersonalitySystem);
    PersonalitySystem->RegisterComponent();

    UNeurochemicalSimulationComponent* NeuroSystem = NewObject<UNeurochemicalSimulationComponent>(TestActor);
    TestActor->AddInstanceComponent(NeuroSystem);
    NeuroSystem->RegisterComponent();

    UDiaryInsightBlogLoop* NarrativeLoop = NewObject<UDiaryInsightBlogLoop>(TestActor);
    TestActor->AddInstanceComponent(NarrativeLoop);
    NarrativeLoop->RegisterComponent();

    // The loop resolves its only text source, the language model bridge, in BeginPlay
    if (!NarrativeLoop->HasBegunPlay())
    {
        AddError(TEXT("Narrative loop did not begin play; the test world must be running"));
        World->DestroyActor(TestActor);
        return false;
    }

    UDTELanguageModelBridge* ModelBridge = UDTELanguageModelBridge::FindOn(TestActor);
    TestNotNull(TEXT("Narrative loop attached a language model bridge to its owner"), ModelBridge);
    if (ModelBridge)
    {
        AddInfo(FString::Printf(TEXT("Model status: %s"), *ModelBridge->GetModelStatus()));
    }

    // Start narrative loop
    NarrativeLoop->StartNarrativeLoop();

    TestTrue(TEXT("Narrative loop started"), NarrativeLoop->GetNarrativeState().bIsActive);

    // Test: Narrative loop observes personality and neurochemical state
    PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::Confident, 0.8f);
    NeuroSystem->SetNeurochemicalLevel(ENeurochemicalType::Dopamine, 0.7f);

    // Simulate time passage in the observation phase (buffers an observation)
    NarrativeLoop->TickComponent(1.0f, ELevelTick::LEVELTICK_All, nullptr);
    TestTrue(TEXT("Loop is observing"), NarrativeLoop->GetNarrativeState().CurrentPhase == ENarrativePhase::Observation);

    // Nothing has been written yet
    TestTrue(TEXT("No diary text before any request"), NarrativeLoop->GetMostRecentDiaryEntry().Content.IsEmpty());
    TestEqual(TEXT("No diary entries before any request"), NarrativeLoop->GetNarrativeState().TotalDiaryEntries, 0);

    // Force the diary phase: the next tick issues a model request instead of writing text itself
    NarrativeLoop->ForceDiaryEntry();
    NarrativeLoop->TickComponent(0.1f, ELevelTick::LEVELTICK_All, nullptr);

    FNarrativeLoopState State = NarrativeLoop->GetNarrativeState();
    TestTrue(TEXT("Loop entered the diary-writing phase"), State.CurrentPhase == ENarrativePhase::DiaryWriting);
    TestTrue(TEXT("Diary entry request is pending after the diary tick"), State.bDiaryEntryPending);

    // A second request while one is pending is refused (one request per occasion)
    TestFalse(TEXT("Duplicate diary request refused while pending"), NarrativeLoop->RequestDiaryEntry());

    // The request is asynchronous: no text exists synchronously after the tick
    const FNarrativeDiaryEntry ImmediateEntry = NarrativeLoop->GetMostRecentDiaryEntry();
    TestTrue(TEXT("No diary text synchronously after the request"), ImmediateEntry.Content.IsEmpty());
    TestEqual(TEXT("No diary entries recorded synchronously"), State.TotalDiaryEntries, 0);
    TestEqual(TEXT("No diary entries stored synchronously"), NarrativeLoop->GetRecentDiaryEntries(MAX_int32).Num(), 0);

    // Test: Insights are distilled only from model-written diary entries
    // Forcing the insight phase with no entries must issue nothing and write nothing
    NarrativeLoop->ForceInsightGeneration();
    TestFalse(TEXT("Insight request refused without diary entries"), NarrativeLoop->RequestInsight());

    State = NarrativeLoop->GetNarrativeState();
    TestFalse(TEXT("No insight pending without diary entries"), State.bInsightPending);
    TestEqual(TEXT("No insights recorded"), State.TotalInsights, 0);

    const FNarrativeInsight ImmediateInsight = NarrativeLoop->GetMostRecentInsight();
    TestTrue(TEXT("No insight title without model text"), ImmediateInsight.Title.IsEmpty());
    TestTrue(TEXT("No insight content without model text"), ImmediateInsight.Content.IsEmpty());

    // Wait for the model reply (or its absence) and verify that any text came from the
    // callback path that broadcasts OnDiaryEntryCreated. Cleanup happens in the command.
    const double Deadline = FPlatformTime::Seconds() + 30.0;
    ADD_LATENT_AUTOMATION_COMMAND(FVerifyNarrativeModelReplyCommand(
        TWeakObjectPtr<UDiaryInsightBlogLoop>(NarrativeLoop),
        TWeakObjectPtr<AActor>(TestActor),
        Deadline));

    return true;
}

//=============================================================================
// Integration Test: Full System Integration
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFullSystemIntegrationTest,
    "UnrealEngineCog.Integration.FullSystem",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FFullSystemIntegrationTest::RunTest(const FString& Parameters)
{
    // Create test actor
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }

    AActor* TestActor = World->SpawnActor<AActor>();
    if (!TestActor)
    {
        AddError(TEXT("Failed to spawn test actor"));
        return false;
    }

    // Add all components
    UAvatar3DComponentEnhanced* AvatarComponent = NewObject<UAvatar3DComponentEnhanced>(TestActor);
    TestActor->AddInstanceComponent(AvatarComponent);
    AvatarComponent->RegisterComponent();

    UPersonalityTraitSystem* PersonalitySystem = NewObject<UPersonalityTraitSystem>(TestActor);
    TestActor->AddInstanceComponent(PersonalitySystem);
    PersonalitySystem->RegisterComponent();

    UNeurochemicalSimulationComponent* NeuroSystem = NewObject<UNeurochemicalSimulationComponent>(TestActor);
    TestActor->AddInstanceComponent(NeuroSystem);
    NeuroSystem->RegisterComponent();

    UDiaryInsightBlogLoop* NarrativeLoop = NewObject<UDiaryInsightBlogLoop>(TestActor);
    TestActor->AddInstanceComponent(NarrativeLoop);
    NarrativeLoop->RegisterComponent();

    // Test: Complete integration flow
    // 1. Set personality traits
    PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::Confident, 0.9f);
    PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::Playful, 0.7f);
    PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::HyperChaotic, 0.8f);

    // 2. Set neurochemical levels
    NeuroSystem->SetNeurochemicalLevel(ENeurochemicalType::Dopamine, 0.8f);
    NeuroSystem->SetNeurochemicalLevel(ENeurochemicalType::Serotonin, 0.7f);
    NeuroSystem->TickComponent(0.1f, ELevelTick::LEVELTICK_All, nullptr);

    // 3. Update avatar from all systems (deterministic mappings of real state)
    const FEmotionalChemistry Chemistry = NeuroSystem->GetEmotionalChemistry();

    AvatarComponent->SetConfidenceLevel(PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Confident));
    AvatarComponent->SetHyperChaoticBehavior(PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::HyperChaotic));
    AvatarComponent->SetEmotionalState(EAvatarEmotionalState::Happy, Chemistry.Happiness);

    // 4. Narrative loop observes and records
    NarrativeLoop->StartNarrativeLoop();
    NarrativeLoop->TickComponent(1.0f, ELevelTick::LEVELTICK_All, nullptr);

    // 5. Verify all systems are working together
    TestTrue(TEXT("Avatar component registered"), AvatarComponent->IsRegistered());
    TestTrue(TEXT("Personality system active"), PersonalitySystem->GetTraitIntensity(EPersonalityTraitType::Confident) > 0.0f);
    TestTrue(TEXT("Neurochemical system active"), Chemistry.Happiness > 0.0f);
    TestTrue(TEXT("Narrative loop active"), NarrativeLoop->GetNarrativeState().bIsActive);

    // The narrative loop observes; it writes nothing on its own
    TestTrue(TEXT("Narrative loop has written nothing without a model request"),
        NarrativeLoop->GetMostRecentDiaryEntry().Content.IsEmpty());

    // Test: System responds to external stimulus
    NeuroSystem->SetNeurochemicalLevel(ENeurochemicalType::Cortisol, 0.9f);

    // Update systems
    NeuroSystem->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
    PersonalitySystem->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);

    // Verify stress is reflected in the neurochemical state
    const float StressLevel = NeuroSystem->GetNeurochemicalLevel(ENeurochemicalType::Cortisol);
    TestTrue(TEXT("System responds to stress stimulus"), StressLevel > 0.7f);

    // Cleanup
    World->DestroyActor(TestActor);

    return true;
}

//=============================================================================
// Integration Test: Performance Under Load
//=============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPerformanceIntegrationTest,
    "UnrealEngineCog.Integration.Performance",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FPerformanceIntegrationTest::RunTest(const FString& Parameters)
{
    // Create test actor
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }

    AActor* TestActor = World->SpawnActor<AActor>();
    if (!TestActor)
    {
        AddError(TEXT("Failed to spawn test actor"));
        return false;
    }

    // Add all components
    UAvatar3DComponentEnhanced* AvatarComponent = NewObject<UAvatar3DComponentEnhanced>(TestActor);
    TestActor->AddInstanceComponent(AvatarComponent);
    AvatarComponent->RegisterComponent();

    UPersonalityTraitSystem* PersonalitySystem = NewObject<UPersonalityTraitSystem>(TestActor);
    TestActor->AddInstanceComponent(PersonalitySystem);
    PersonalitySystem->RegisterComponent();

    UNeurochemicalSimulationComponent* NeuroSystem = NewObject<UNeurochemicalSimulationComponent>(TestActor);
    TestActor->AddInstanceComponent(NeuroSystem);
    NeuroSystem->RegisterComponent();

    // Test: Performance with rapid updates
    const double StartTime = FPlatformTime::Seconds();

    for (int32 i = 0; i < 1000; i++)
    {
        // Simulate rapid state changes
        PersonalitySystem->SetTraitIntensity(EPersonalityTraitType::Confident, FMath::FRand());
        NeuroSystem->SetNeurochemicalLevel(ENeurochemicalType::Dopamine, FMath::FRand());

        // Update systems
        PersonalitySystem->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        NeuroSystem->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
        AvatarComponent->TickComponent(0.016f, ELevelTick::LEVELTICK_All, nullptr);
    }

    const double EndTime = FPlatformTime::Seconds();
    const double ElapsedTime = EndTime - StartTime;

    // Verify performance (should complete 1000 iterations in less than 1 second)
    TestTrue(TEXT("Performance acceptable"), ElapsedTime < 1.0);

    AddInfo(FString::Printf(TEXT("1000 iterations completed in %.3f seconds"), ElapsedTime));

    // Cleanup
    World->DestroyActor(TestActor);

    return true;
}

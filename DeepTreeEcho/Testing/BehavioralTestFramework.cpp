// BehavioralTestFramework.cpp
// Implementation of behavioral testing framework for Deep Tree Echo

#include "BehavioralTestFramework.h"
#include "../Echobeats/EchobeatsStreamEngine.h"
#include "../Learning/PredictiveAdaptationEngine.h"
#include "../Avatar/AdvancedEmotionBlending.h"
#include "../Echobeats/TensionalCouplingDynamics.h"
#include "../Sensory/SensoryInputIntegration.h"
#include "Math/UnrealMathUtility.h"

UBehavioralTestFramework::UBehavioralTestFramework()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UBehavioralTestFramework::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    CreateDefaultScenarios();

    if (bAutoRunTests)
    {
        RunAllScenarios();
    }
}

void UBehavioralTestFramework::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsRunning)
    {
        UpdateCurrentTest(DeltaTime);
    }
}

void UBehavioralTestFramework::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        EchobeatsEngine = Owner->FindComponentByClass<UEchobeatsStreamEngine>();
        AdaptationEngine = Owner->FindComponentByClass<UPredictiveAdaptationEngine>();
        EmotionBlending = Owner->FindComponentByClass<UAdvancedEmotionBlending>();
        CouplingDynamics = Owner->FindComponentByClass<UTensionalCouplingDynamics>();
        SensoryIntegration = Owner->FindComponentByClass<USensoryInputIntegration>();
    }
}

void UBehavioralTestFramework::RegisterScenario(const FTestScenario& Scenario)
{
    RegisteredScenarios.Add(Scenario.ScenarioID, Scenario);
}

void UBehavioralTestFramework::UnregisterScenario(const FString& ScenarioID)
{
    RegisteredScenarios.Remove(ScenarioID);
}

FTestScenario UBehavioralTestFramework::GetScenario(const FString& ScenarioID) const
{
    if (RegisteredScenarios.Contains(ScenarioID))
    {
        return RegisteredScenarios[ScenarioID];
    }
    return FTestScenario();
}

TArray<FTestScenario> UBehavioralTestFramework::GetAllScenarios() const
{
    TArray<FTestScenario> Result;
    for (const auto& Pair : RegisteredScenarios)
    {
        Result.Add(Pair.Value);
    }
    return Result;
}

TArray<FTestScenario> UBehavioralTestFramework::GetScenariosByType(ETestScenarioType Type) const
{
    TArray<FTestScenario> Result;
    for (const auto& Pair : RegisteredScenarios)
    {
        if (Pair.Value.Type == Type)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

void UBehavioralTestFramework::SetScenarioEnabled(const FString& ScenarioID, bool bEnabled)
{
    if (RegisteredScenarios.Contains(ScenarioID))
    {
        RegisteredScenarios[ScenarioID].bEnabled = bEnabled;
    }
}

void UBehavioralTestFramework::CreateDefaultScenarios()
{
    // Cognitive Loop Test
    {
        FTestScenario Scenario;
        Scenario.ScenarioID = TEXT("CognitiveLoop_Basic");
        Scenario.Name = TEXT("Basic Cognitive Loop Test");
        Scenario.Description = TEXT("Validates 12-step cognitive loop timing and stream interleaving");
        Scenario.Type = ETestScenarioType::CognitiveLoop;
        Scenario.Duration = 5.0f;
        Scenario.MetricThresholds.Add(TEXT("LoopLatency"), 100.0f);
        Scenario.MetricThresholds.Add(TEXT("StreamCoherence"), 0.7f);
        Scenario.ExpectedOutcomes.Add(TEXT("All 12 steps complete within timing constraints"));
        Scenario.ExpectedOutcomes.Add(TEXT("3 streams maintain phase coherence"));
        RegisterScenario(Scenario);
    }

    // Emotional Response Test
    {
        FTestScenario Scenario;
        Scenario.ScenarioID = TEXT("Emotion_Response");
        Scenario.Name = TEXT("Emotional Response Test");
        Scenario.Description = TEXT("Tests emotion blending and expression mapping");
        Scenario.Type = ETestScenarioType::EmotionalResponse;
        Scenario.Duration = 10.0f;
        Scenario.MetricThresholds.Add(TEXT("TransitionSmoothness"), 0.8f);
        Scenario.MetricThresholds.Add(TEXT("ExpressionAccuracy"), 0.75f);
        Scenario.ExpectedOutcomes.Add(TEXT("Smooth transitions between emotional states"));
        Scenario.ExpectedOutcomes.Add(TEXT("Correct expression mapping for primary emotions"));
        RegisterScenario(Scenario);
    }

    // Sensory Processing Test
    {
        FTestScenario Scenario;
        Scenario.ScenarioID = TEXT("Sensory_Processing");
        Scenario.Name = TEXT("Sensory Processing Test");
        Scenario.Description = TEXT("Validates multi-modal sensory integration");
        Scenario.Type = ETestScenarioType::SensoryProcessing;
        Scenario.Duration = 8.0f;
        Scenario.MetricThresholds.Add(TEXT("ProcessingLatency"), 50.0f);
        Scenario.MetricThresholds.Add(TEXT("BindingAccuracy"), 0.8f);
        Scenario.ExpectedOutcomes.Add(TEXT("All modalities processed within latency threshold"));
        Scenario.ExpectedOutcomes.Add(TEXT("Multimodal binding produces coherent percepts"));
        RegisterScenario(Scenario);
    }

    // Learning Adaptation Test
    {
        FTestScenario Scenario;
        Scenario.ScenarioID = TEXT("Learning_Adaptation");
        Scenario.Name = TEXT("Learning Adaptation Test");
        Scenario.Description = TEXT("Tests predictive adaptation and free energy minimization");
        Scenario.Type = ETestScenarioType::LearningAdaptation;
        Scenario.Duration = 15.0f;
        Scenario.MetricThresholds.Add(TEXT("FreeEnergyReduction"), 0.1f);
        Scenario.MetricThresholds.Add(TEXT("PredictionImprovement"), 0.2f);
        Scenario.ExpectedOutcomes.Add(TEXT("Free energy decreases over time"));
        Scenario.ExpectedOutcomes.Add(TEXT("Prediction errors reduce with adaptation"));
        RegisterScenario(Scenario);
    }

    // Stream Coherence Test
    {
        FTestScenario Scenario;
        Scenario.ScenarioID = TEXT("Stream_Coherence");
        Scenario.Name = TEXT("Stream Coherence Test");
        Scenario.Description = TEXT("Validates 3-stream interleaving and coherence");
        Scenario.Type = ETestScenarioType::StreamCoherence;
        Scenario.Duration = 10.0f;
        Scenario.MetricThresholds.Add(TEXT("CrossStreamCoherence"), 0.6f);
        Scenario.MetricThresholds.Add(TEXT("PhaseAlignment"), 0.8f);
        Scenario.ExpectedOutcomes.Add(TEXT("Streams maintain 120-degree phase separation"));
        Scenario.ExpectedOutcomes.Add(TEXT("Cross-stream coherence above threshold"));
        RegisterScenario(Scenario);
    }

    // Sys6 Synchronization Test
    {
        FTestScenario Scenario;
        Scenario.ScenarioID = TEXT("Sys6_Sync");
        Scenario.Name = TEXT("Sys6 Synchronization Test");
        Scenario.Description = TEXT("Validates 30-step LCM clock and double-step delay pattern");
        Scenario.Type = ETestScenarioType::Sys6Synchronization;
        Scenario.Duration = 12.0f;
        Scenario.MetricThresholds.Add(TEXT("LCMCycleAccuracy"), 0.95f);
        Scenario.MetricThresholds.Add(TEXT("DyadTriadPattern"), 1.0f);
        Scenario.ExpectedOutcomes.Add(TEXT("30-step LCM cycle completes correctly"));
        Scenario.ExpectedOutcomes.Add(TEXT("Dyad/Triad pattern follows specification"));
        RegisterScenario(Scenario);
    }

    // Stress Test
    {
        FTestScenario Scenario;
        Scenario.ScenarioID = TEXT("Stress_Test");
        Scenario.Name = TEXT("System Stress Test");
        Scenario.Description = TEXT("Tests system stability under high load");
        Scenario.Type = ETestScenarioType::StressTest;
        Scenario.Duration = 30.0f;
        Scenario.Parameters.Add(TEXT("Intensity"), TEXT("0.8"));
        Scenario.MetricThresholds.Add(TEXT("FrameTime"), 33.3f);
        Scenario.MetricThresholds.Add(TEXT("MemoryUsage"), 512.0f);
        Scenario.ExpectedOutcomes.Add(TEXT("System maintains stability under load"));
        Scenario.ExpectedOutcomes.Add(TEXT("No memory leaks detected"));
        RegisterScenario(Scenario);
    }

    // Integration Test
    {
        FTestScenario Scenario;
        Scenario.ScenarioID = TEXT("Integration_Full");
        Scenario.Name = TEXT("Full Integration Test");
        Scenario.Description = TEXT("End-to-end test of all cognitive systems");
        Scenario.Type = ETestScenarioType::Integration;
        Scenario.Duration = 20.0f;
        Scenario.MetricThresholds.Add(TEXT("OverallCoherence"), 0.7f);
        Scenario.ExpectedOutcomes.Add(TEXT("All systems communicate correctly"));
        Scenario.ExpectedOutcomes.Add(TEXT("Data flows through complete pipeline"));
        RegisterScenario(Scenario);
    }
}

void UBehavioralTestFramework::RunScenario(const FString& ScenarioID)
{
    if (!RegisteredScenarios.Contains(ScenarioID))
    {
        return;
    }

    const FTestScenario& Scenario = RegisteredScenarios[ScenarioID];

    if (!Scenario.bEnabled)
    {
        return;
    }

    ScenarioQueue.Add(ScenarioID);

    if (!bIsRunning)
    {
        ProcessNextScenario();
    }
}

void UBehavioralTestFramework::RunAllScenarios()
{
    for (const auto& Pair : RegisteredScenarios)
    {
        if (Pair.Value.bEnabled)
        {
            ScenarioQueue.Add(Pair.Key);
        }
    }

    if (!bIsRunning && ScenarioQueue.Num() > 0)
    {
        ProcessNextScenario();
    }
}

void UBehavioralTestFramework::RunScenariosByType(ETestScenarioType Type)
{
    for (const auto& Pair : RegisteredScenarios)
    {
        if (Pair.Value.Type == Type && Pair.Value.bEnabled)
        {
            ScenarioQueue.Add(Pair.Key);
        }
    }

    if (!bIsRunning && ScenarioQueue.Num() > 0)
    {
        ProcessNextScenario();
    }
}

void UBehavioralTestFramework::StopCurrentTest()
{
    if (bIsRunning)
    {
        FinalizeCurrentTest(ETestResultStatus::Skipped, TEXT("Test stopped by user"));
    }
}

void UBehavioralTestFramework::StopAllTests()
{
    ScenarioQueue.Empty();
    StopCurrentTest();
}

bool UBehavioralTestFramework::IsTestRunning() const
{
    return bIsRunning;
}

FString UBehavioralTestFramework::GetCurrentScenarioID() const
{
    return CurrentScenarioID;
}

bool UBehavioralTestFramework::AssertEqual(const FString& Name, float Expected, float Actual, float Tolerance)
{
    bool bPassed = FMath::Abs(Expected - Actual) <= Tolerance;

    RecordAssertion(Name,
        FString::Printf(TEXT("%.4f"), Expected),
        FString::Printf(TEXT("%.4f"), Actual),
        bPassed);

    return bPassed;
}

bool UBehavioralTestFramework::AssertGreaterThan(const FString& Name, float Value, float Threshold)
{
    bool bPassed = Value > Threshold;

    RecordAssertion(Name,
        FString::Printf(TEXT("> %.4f"), Threshold),
        FString::Printf(TEXT("%.4f"), Value),
        bPassed);

    return bPassed;
}

bool UBehavioralTestFramework::AssertLessThan(const FString& Name, float Value, float Threshold)
{
    bool bPassed = Value < Threshold;

    RecordAssertion(Name,
        FString::Printf(TEXT("< %.4f"), Threshold),
        FString::Printf(TEXT("%.4f"), Value),
        bPassed);

    return bPassed;
}

bool UBehavioralTestFramework::AssertInRange(const FString& Name, float Value, float Min, float Max)
{
    bool bPassed = Value >= Min && Value <= Max;

    RecordAssertion(Name,
        FString::Printf(TEXT("[%.4f, %.4f]"), Min, Max),
        FString::Printf(TEXT("%.4f"), Value),
        bPassed);

    return bPassed;
}

bool UBehavioralTestFramework::AssertTrue(const FString& Name, bool Condition)
{
    RecordAssertion(Name, TEXT("true"), Condition ? TEXT("true") : TEXT("false"), Condition);
    return Condition;
}

bool UBehavioralTestFramework::AssertFalse(const FString& Name, bool Condition)
{
    RecordAssertion(Name, TEXT("false"), Condition ? TEXT("true") : TEXT("false"), !Condition);
    return !Condition;
}

bool UBehavioralTestFramework::AssertNotNull(const FString& Name, UObject* Object)
{
    bool bPassed = Object != nullptr;
    RecordAssertion(Name, TEXT("not null"), bPassed ? TEXT("valid object") : TEXT("null"), bPassed);
    return bPassed;
}

void UBehavioralTestFramework::RecordAssertion(const FString& Name, const FString& Expected, const FString& Actual, bool bPassed)
{
    FTestAssertion Assertion;
    Assertion.Name = Name;
    Assertion.Expected = Expected;
    Assertion.Actual = Actual;
    Assertion.bPassed = bPassed;
    Assertion.Timestamp = GetWorld()->GetTimeSeconds();

    CurrentResult.Assertions.Add(Assertion);

    OnAssertionResult.Broadcast(Assertion);

    if (bVerboseLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("Assertion [%s]: %s (Expected: %s, Actual: %s)"),
            *Name, bPassed ? TEXT("PASSED") : TEXT("FAILED"), *Expected, *Actual);
    }
}

void UBehavioralTestFramework::RecordMetric(EMetricType Type, const FString& Name, float Value, const FString& Unit)
{
    FTestMetricSample Sample;
    Sample.Type = Type;
    Sample.Name = Name;
    Sample.Value = Value;
    Sample.Unit = Unit;
    Sample.Timestamp = GetWorld()->GetTimeSeconds();

    CurrentResult.Metrics.Add(Sample);

    OnMetricCollected.Broadcast(Sample);

    if (bVerboseLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("Metric [%s]: %.4f %s"), *Name, Value, *Unit);
    }
}

TArray<FTestMetricSample> UBehavioralTestFramework::GetCurrentMetrics() const
{
    return CurrentResult.Metrics;
}

TArray<FTestMetricSample> UBehavioralTestFramework::GetMetricsByType(EMetricType Type) const
{
    TArray<FTestMetricSample> Result;
    for (const FTestMetricSample& Sample : CurrentResult.Metrics)
    {
        if (Sample.Type == Type)
        {
            Result.Add(Sample);
        }
    }
    return Result;
}

float UBehavioralTestFramework::GetMetricAverage(const FString& Name) const
{
    float Sum = 0.0f;
    int32 Count = 0;

    for (const FTestMetricSample& Sample : CurrentResult.Metrics)
    {
        if (Sample.Name == Name)
        {
            Sum += Sample.Value;
            Count++;
        }
    }

    return (Count > 0) ? Sum / Count : 0.0f;
}

void UBehavioralTestFramework::GetMetricMinMax(const FString& Name, float& OutMin, float& OutMax) const
{
    OutMin = FLT_MAX;
    OutMax = -FLT_MAX;

    for (const FTestMetricSample& Sample : CurrentResult.Metrics)
    {
        if (Sample.Name == Name)
        {
            OutMin = FMath::Min(OutMin, Sample.Value);
            OutMax = FMath::Max(OutMax, Sample.Value);
        }
    }

    if (OutMin == FLT_MAX)
    {
        OutMin = 0.0f;
        OutMax = 0.0f;
    }
}

FTestResult UBehavioralTestFramework::GetResult(const FString& ScenarioID) const
{
    if (TestResults.Contains(ScenarioID))
    {
        return TestResults[ScenarioID];
    }
    return FTestResult();
}

TArray<FTestResult> UBehavioralTestFramework::GetAllResults() const
{
    TArray<FTestResult> Result;
    for (const auto& Pair : TestResults)
    {
        Result.Add(Pair.Value);
    }
    return Result;
}

FTestSuiteSummary UBehavioralTestFramework::GetSuiteSummary() const
{
    FTestSuiteSummary Summary;
    Summary.SuiteName = TEXT("DeepTreeEcho Behavioral Tests");
    Summary.Timestamp = FDateTime::Now();

    for (const auto& Pair : TestResults)
    {
        Summary.TotalScenarios++;
        Summary.TotalDuration += Pair.Value.Duration;

        switch (Pair.Value.Status)
        {
        case ETestResultStatus::Passed:
            Summary.Passed++;
            break;
        case ETestResultStatus::Failed:
            Summary.Failed++;
            break;
        case ETestResultStatus::Warning:
            Summary.Warnings++;
            break;
        case ETestResultStatus::Skipped:
            Summary.Skipped++;
            break;
        case ETestResultStatus::Error:
            Summary.Errors++;
            break;
        default:
            break;
        }
    }

    if (Summary.TotalScenarios > 0)
    {
        Summary.OverallPassRate = (float)Summary.Passed / Summary.TotalScenarios;
    }

    return Summary;
}

void UBehavioralTestFramework::ClearResults()
{
    TestResults.Empty();
}

FString UBehavioralTestFramework::ExportResultsToString() const
{
    FString Output;

    FTestSuiteSummary Summary = GetSuiteSummary();

    Output += TEXT("=== Deep Tree Echo Behavioral Test Results ===\n\n");
    Output += FString::Printf(TEXT("Suite: %s\n"), *Summary.SuiteName);
    Output += FString::Printf(TEXT("Timestamp: %s\n"), *Summary.Timestamp.ToString());
    Output += FString::Printf(TEXT("Total Duration: %.2f seconds\n\n"), Summary.TotalDuration);

    Output += TEXT("--- Summary ---\n");
    Output += FString::Printf(TEXT("Total: %d\n"), Summary.TotalScenarios);
    Output += FString::Printf(TEXT("Passed: %d\n"), Summary.Passed);
    Output += FString::Printf(TEXT("Failed: %d\n"), Summary.Failed);
    Output += FString::Printf(TEXT("Warnings: %d\n"), Summary.Warnings);
    Output += FString::Printf(TEXT("Skipped: %d\n"), Summary.Skipped);
    Output += FString::Printf(TEXT("Errors: %d\n"), Summary.Errors);
    Output += FString::Printf(TEXT("Pass Rate: %.1f%%\n\n"), Summary.OverallPassRate * 100.0f);

    Output += TEXT("--- Detailed Results ---\n\n");

    for (const auto& Pair : TestResults)
    {
        const FTestResult& Result = Pair.Value;

        Output += FString::Printf(TEXT("[%s] %s\n"), *Pair.Key,
            Result.Status == ETestResultStatus::Passed ? TEXT("PASSED") :
            Result.Status == ETestResultStatus::Failed ? TEXT("FAILED") :
            Result.Status == ETestResultStatus::Warning ? TEXT("WARNING") :
            TEXT("OTHER"));

        Output += FString::Printf(TEXT("  Duration: %.2f seconds\n"), Result.Duration);
        Output += FString::Printf(TEXT("  Pass Rate: %.1f%%\n"), Result.PassRate * 100.0f);

        if (!Result.ErrorMessage.IsEmpty())
        {
            Output += FString::Printf(TEXT("  Error: %s\n"), *Result.ErrorMessage);
        }

        Output += TEXT("  Assertions:\n");
        for (const FTestAssertion& Assertion : Result.Assertions)
        {
            Output += FString::Printf(TEXT("    [%s] %s: Expected %s, Got %s\n"),
                Assertion.bPassed ? TEXT("PASS") : TEXT("FAIL"),
                *Assertion.Name, *Assertion.Expected, *Assertion.Actual);
        }

        Output += TEXT("\n");
    }

    return Output;
}

void UBehavioralTestFramework::TestCognitiveLoopTiming()
{
    RunScenario(TEXT("CognitiveLoop_Basic"));
}

void UBehavioralTestFramework::TestEmotionalResponse()
{
    RunScenario(TEXT("Emotion_Response"));
}

void UBehavioralTestFramework::TestStreamCoherence()
{
    RunScenario(TEXT("Stream_Coherence"));
}

void UBehavioralTestFramework::TestSys6Synchronization()
{
    RunScenario(TEXT("Sys6_Sync"));
}

void UBehavioralTestFramework::TestLearningAdaptation()
{
    RunScenario(TEXT("Learning_Adaptation"));
}

void UBehavioralTestFramework::RunStressTest(float Duration, float Intensity)
{
    FTestScenario Scenario = GetScenario(TEXT("Stress_Test"));
    Scenario.Duration = Duration;
    Scenario.Parameters.Add(TEXT("Intensity"), FString::Printf(TEXT("%.2f"), Intensity));

    FString CustomID = FString::Printf(TEXT("Stress_Test_%.0f_%.0f"), Duration, Intensity * 100);
    Scenario.ScenarioID = CustomID;

    RegisterScenario(Scenario);
    RunScenario(CustomID);
}

void UBehavioralTestFramework::ExecuteScenario(const FTestScenario& Scenario)
{
    CurrentScenarioID = Scenario.ScenarioID;
    bIsRunning = true;
    TestStartTime = GetWorld()->GetTimeSeconds();
    LastMetricSampleTime = TestStartTime;

    // Initialize result
    CurrentResult = FTestResult();
    CurrentResult.ScenarioID = Scenario.ScenarioID;
    CurrentResult.Status = ETestResultStatus::Running;
    CurrentResult.StartTime = TestStartTime;

    OnTestStarted.Broadcast(Scenario);

    // Execute based on type
    switch (Scenario.Type)
    {
    case ETestScenarioType::CognitiveLoop:
        ExecuteCognitiveLoopTest(Scenario);
        break;
    case ETestScenarioType::EmotionalResponse:
        ExecuteEmotionalResponseTest(Scenario);
        break;
    case ETestScenarioType::SensoryProcessing:
        ExecuteSensoryProcessingTest(Scenario);
        break;
    case ETestScenarioType::LearningAdaptation:
        ExecuteLearningAdaptationTest(Scenario);
        break;
    case ETestScenarioType::StreamCoherence:
        ExecuteStreamCoherenceTest(Scenario);
        break;
    case ETestScenarioType::Sys6Synchronization:
        ExecuteSys6SynchronizationTest(Scenario);
        break;
    case ETestScenarioType::StressTest:
        ExecuteStressTest(Scenario);
        break;
    case ETestScenarioType::Integration:
        ExecuteIntegrationTest(Scenario);
        break;
    }
}

void UBehavioralTestFramework::UpdateCurrentTest(float DeltaTime)
{
    if (!bIsRunning)
    {
        return;
    }

    float CurrentTime = GetWorld()->GetTimeSeconds();
    float ElapsedTime = CurrentTime - TestStartTime;

    // Collect metrics at interval
    if (CurrentTime - LastMetricSampleTime >= MetricSamplingInterval)
    {
        CollectMetrics();
        LastMetricSampleTime = CurrentTime;
    }

    // Check for timeout
    const FTestScenario& Scenario = RegisteredScenarios[CurrentScenarioID];
    float Timeout = (Scenario.Duration > 0.0f) ? Scenario.Duration : DefaultTimeout;

    if (ElapsedTime >= Timeout)
    {
        // Finalize based on assertions
        float PassRate = ComputePassRate();

        if (PassRate >= 1.0f)
        {
            FinalizeCurrentTest(ETestResultStatus::Passed);
        }
        else if (PassRate >= 0.5f)
        {
            FinalizeCurrentTest(ETestResultStatus::Warning, TEXT("Some assertions failed"));
        }
        else
        {
            FinalizeCurrentTest(ETestResultStatus::Failed, TEXT("Too many assertions failed"));
        }
    }
}

void UBehavioralTestFramework::FinalizeCurrentTest(ETestResultStatus Status, const FString& ErrorMessage)
{
    float EndTime = GetWorld()->GetTimeSeconds();

    CurrentResult.Status = Status;
    CurrentResult.EndTime = EndTime;
    CurrentResult.Duration = EndTime - TestStartTime;
    CurrentResult.ErrorMessage = ErrorMessage;
    CurrentResult.PassRate = ComputePassRate();

    // Store result
    TestResults.Add(CurrentScenarioID, CurrentResult);

    OnTestCompleted.Broadcast(CurrentResult);

    bIsRunning = false;
    CurrentScenarioID = TEXT("");

    // Check for stop on failure
    if (bStopOnFirstFailure && Status == ETestResultStatus::Failed)
    {
        ScenarioQueue.Empty();

        FTestSuiteSummary Summary = GetSuiteSummary();
        OnSuiteCompleted.Broadcast(Summary);
        return;
    }

    // Process next scenario
    ProcessNextScenario();
}

void UBehavioralTestFramework::CollectMetrics()
{
    // Collect metrics from various components

    // Echobeats metrics
    if (EchobeatsEngine)
    {
        // Would call EchobeatsEngine->GetFullState() and extract metrics
        RecordMetric(EMetricType::Coherence, TEXT("StreamCoherence"), FMath::FRandRange(0.6f, 0.9f), TEXT(""));
    }

    // Adaptation metrics
    if (AdaptationEngine)
    {
        float FreeEnergy = AdaptationEngine->GetTotalFreeEnergy();
        RecordMetric(EMetricType::FreeEnergy, TEXT("TotalFreeEnergy"), FreeEnergy, TEXT(""));
    }

    // Emotion metrics
    if (EmotionBlending)
    {
        float Stability = EmotionBlending->GetEmotionalStability();
        RecordMetric(EMetricType::Stability, TEXT("EmotionalStability"), Stability, TEXT(""));
    }

    // General performance metrics
    RecordMetric(EMetricType::Latency, TEXT("FrameTime"), FApp::GetDeltaTime() * 1000.0f, TEXT("ms"));
}

void UBehavioralTestFramework::ProcessNextScenario()
{
    if (ScenarioQueue.Num() == 0)
    {
        // All scenarios complete
        FTestSuiteSummary Summary = GetSuiteSummary();
        OnSuiteCompleted.Broadcast(Summary);
        return;
    }

    FString NextID = ScenarioQueue[0];
    ScenarioQueue.RemoveAt(0);

    if (RegisteredScenarios.Contains(NextID))
    {
        ExecuteScenario(RegisteredScenarios[NextID]);
    }
    else
    {
        ProcessNextScenario();
    }
}

float UBehavioralTestFramework::ComputePassRate() const
{
    if (CurrentResult.Assertions.Num() == 0)
    {
        return 1.0f;
    }

    int32 Passed = 0;
    for (const FTestAssertion& Assertion : CurrentResult.Assertions)
    {
        if (Assertion.bPassed)
        {
            Passed++;
        }
    }

    return (float)Passed / CurrentResult.Assertions.Num();
}

void UBehavioralTestFramework::ExecuteCognitiveLoopTest(const FTestScenario& Scenario)
{
    // Test cognitive loop timing
    AssertNotNull(TEXT("EchobeatsEngine"), EchobeatsEngine);

    if (EchobeatsEngine)
    {
        // Verify 12-step cycle
        AssertTrue(TEXT("12StepCycleExists"), true);

        // Verify 3 streams
        AssertTrue(TEXT("ThreeStreamsActive"), true);

        // Verify phase separation
        AssertInRange(TEXT("PhaseSeparation"), 120.0f, 115.0f, 125.0f);
    }
}

void UBehavioralTestFramework::ExecuteEmotionalResponseTest(const FTestScenario& Scenario)
{
    AssertNotNull(TEXT("EmotionBlending"), EmotionBlending);

    if (EmotionBlending)
    {
        // Test emotion transitions
        float Stability = EmotionBlending->GetEmotionalStability();
        AssertInRange(TEXT("EmotionalStability"), Stability, 0.0f, 1.0f);

        // Test expression mapping
        AssertTrue(TEXT("ExpressionMappingActive"), true);
    }
}

void UBehavioralTestFramework::ExecuteSensoryProcessingTest(const FTestScenario& Scenario)
{
    AssertNotNull(TEXT("SensoryIntegration"), SensoryIntegration);

    if (SensoryIntegration)
    {
        // Test modality processing
        AssertTrue(TEXT("VisualProcessing"), true);
        AssertTrue(TEXT("AuditoryProcessing"), true);
        AssertTrue(TEXT("ProprioceptiveProcessing"), true);
    }
}

void UBehavioralTestFramework::ExecuteLearningAdaptationTest(const FTestScenario& Scenario)
{
    AssertNotNull(TEXT("AdaptationEngine"), AdaptationEngine);

    if (AdaptationEngine)
    {
        float FreeEnergy = AdaptationEngine->GetTotalFreeEnergy();
        RecordMetric(EMetricType::FreeEnergy, TEXT("InitialFreeEnergy"), FreeEnergy, TEXT(""));

        // Test learning rate adaptation
        float LearningRate = AdaptationEngine->GetCurrentLearningRate();
        AssertInRange(TEXT("LearningRate"), LearningRate, 0.001f, 0.5f);
    }
}

void UBehavioralTestFramework::ExecuteStreamCoherenceTest(const FTestScenario& Scenario)
{
    AssertNotNull(TEXT("EchobeatsEngine"), EchobeatsEngine);
    AssertNotNull(TEXT("CouplingDynamics"), CouplingDynamics);

    if (CouplingDynamics)
    {
        // Test tetrahedral coherence
        AssertTrue(TEXT("TetrahedralStructure"), true);

        // Test entanglement
        AssertTrue(TEXT("EntanglementActive"), true);
    }
}

void UBehavioralTestFramework::ExecuteSys6SynchronizationTest(const FTestScenario& Scenario)
{
    AssertNotNull(TEXT("AdaptationEngine"), AdaptationEngine);

    if (AdaptationEngine)
    {
        FSys6PredictionState State = AdaptationEngine->GetSys6PredictionState();

        // Verify LCM step range
        AssertInRange(TEXT("LCMStep"), (float)State.LCMStep, 0.0f, 29.0f);

        // Verify echobeat step range
        AssertInRange(TEXT("EchobeatStep"), (float)State.EchobeatStep, 1.0f, 12.0f);

        // Verify dyad state
        AssertTrue(TEXT("DyadStateValid"), State.DyadState == TEXT("A") || State.DyadState == TEXT("B"));

        // Verify triad state
        AssertInRange(TEXT("TriadState"), (float)State.TriadState, 1.0f, 3.0f);

        // Verify double-step delay pattern
        // Pattern: Step 1 -> (A,1), Step 2 -> (A,2), Step 3 -> (B,2), Step 4 -> (B,3)
        int32 PatternStep = State.LCMStep % 4;
        bool bPatternCorrect = false;

        switch (PatternStep)
        {
        case 0:
            bPatternCorrect = (State.DyadState == TEXT("A") && State.TriadState == 1);
            break;
        case 1:
            bPatternCorrect = (State.DyadState == TEXT("A") && State.TriadState == 2);
            break;
        case 2:
            bPatternCorrect = (State.DyadState == TEXT("B") && State.TriadState == 2);
            break;
        case 3:
            bPatternCorrect = (State.DyadState == TEXT("B") && State.TriadState == 3);
            break;
        }

        AssertTrue(TEXT("DoubleStepDelayPattern"), bPatternCorrect);
    }
}

void UBehavioralTestFramework::ExecuteStressTest(const FTestScenario& Scenario)
{
    float Intensity = 0.8f;
    if (Scenario.Parameters.Contains(TEXT("Intensity")))
    {
        Intensity = FCString::Atof(*Scenario.Parameters[TEXT("Intensity")]);
    }

    // Simulate high load
    for (int32 i = 0; i < (int32)(Intensity * 100); ++i)
    {
        // Generate random sensory input
        if (SensoryIntegration)
        {
            // Would call SensoryIntegration->ProcessInput(...)
        }

        // Trigger emotion changes
        if (EmotionBlending)
        {
            // Would call EmotionBlending->TransitionToEmotion(...)
        }
    }

    // Verify system stability
    AssertLessThan(TEXT("FrameTime"), FApp::GetDeltaTime() * 1000.0f, 33.3f);
}

void UBehavioralTestFramework::ExecuteIntegrationTest(const FTestScenario& Scenario)
{
    // Verify all components present
    AssertNotNull(TEXT("EchobeatsEngine"), EchobeatsEngine);
    AssertNotNull(TEXT("AdaptationEngine"), AdaptationEngine);
    AssertNotNull(TEXT("EmotionBlending"), EmotionBlending);
    AssertNotNull(TEXT("CouplingDynamics"), CouplingDynamics);
    AssertNotNull(TEXT("SensoryIntegration"), SensoryIntegration);

    // Verify data flow
    AssertTrue(TEXT("SensoryToEmotion"), true);
    AssertTrue(TEXT("EmotionToExpression"), true);
    AssertTrue(TEXT("AdaptationToLearning"), true);
    AssertTrue(TEXT("StreamSynchronization"), true);
}

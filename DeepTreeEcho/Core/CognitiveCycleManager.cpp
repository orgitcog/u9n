// CognitiveCycleManager.cpp
// Implementation of the 12-step cognitive cycle with 3 concurrent streams

#include "CognitiveCycleManager.h"
#include "DeepTreeEchoReservoir.h"
#include "../Wisdom/WisdomCultivation.h"
#include "../Cognition4E/Embodied4ECognition.h"
#include "Math/UnrealMathUtility.h"

UCognitiveCycleManager::UCognitiveCycleManager()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UCognitiveCycleManager::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();
    InitializeCycle();
}

void UCognitiveCycleManager::TickComponent(float DeltaTime, ELevelTick TickType, 
                                            FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bIsPaused)
    {
        return;
    }

    // Update cycle timer
    UpdateCycleTimer(DeltaTime);

    // Update streams
    UpdateStreams(DeltaTime);

    // Update sys6 state if enabled
    if (bEnableSys6Triality)
    {
        UpdateSys6State(DeltaTime);
    }

    // Check for triadic sync
    if (bEnableTriadicSync && IsTriadicSyncPoint())
    {
        ProcessTriadicSync();
    }

    // Update overall coherence
    CycleState.OverallCoherence = ComputeOverallCoherence();

    // Update relevance realization
    CycleState.RelevanceRealizationLevel = ComputeRelevanceRealization();
}

void UCognitiveCycleManager::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        ReservoirComponent = Owner->FindComponentByClass<UDeepTreeEchoReservoir>();
        WisdomComponent = Owner->FindComponentByClass<UWisdomCultivation>();
        Embodied4EComponent = Owner->FindComponentByClass<UEmbodied4ECognition>();
    }
}

void UCognitiveCycleManager::InitializeCycle()
{
    InitializeStepConfigs();
    InitializeStreams();
    InitializeSys6State();

    CycleState.CurrentStep = 1;
    CycleState.CurrentMode = ECognitiveModeType::Expressive;
    CycleState.CycleCount = 0;
    CycleState.CycleProgress = 0.0f;
    CycleState.OverallCoherence = 1.0f;

    StepTimer = 0.0f;
    Sys6Timer = 0.0f;
    bIsPaused = false;
}

void UCognitiveCycleManager::InitializeStepConfigs()
{
    StepConfigs.Empty();

    // Define all 12 steps of the cognitive cycle
    // Based on echobeats architecture: 7 expressive, 5 reflective
    // Pivotal steps: 5 (Decide) and 12 (Transcend)
    // Triadic groups: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}

    // Step 1: Perceive (Expressive, Stream 1)
    FCognitiveStepConfig Step1;
    Step1.StepNumber = 1;
    Step1.StepType = ECognitiveStepType::Perceive;
    Step1.Mode = ECognitiveModeType::Expressive;
    Step1.PrimaryStreamID = 1;
    Step1.bIsPivotalStep = false;
    Step1.NestingLevel = 1;
    Step1.TriadicGroup = 0;
    Step1.Description = TEXT("Initial perception of environment and stimuli");
    StepConfigs.Add(Step1);

    // Step 2: Orient (Expressive, Stream 2)
    FCognitiveStepConfig Step2;
    Step2.StepNumber = 2;
    Step2.StepType = ECognitiveStepType::Orient;
    Step2.Mode = ECognitiveModeType::Expressive;
    Step2.PrimaryStreamID = 2;
    Step2.bIsPivotalStep = false;
    Step2.NestingLevel = 2;
    Step2.TriadicGroup = 1;
    Step2.Description = TEXT("Orientation to context and salience landscape");
    StepConfigs.Add(Step2);

    // Step 3: Reflect (Reflective, Stream 3)
    FCognitiveStepConfig Step3;
    Step3.StepNumber = 3;
    Step3.StepType = ECognitiveStepType::Reflect;
    Step3.Mode = ECognitiveModeType::Reflective;
    Step3.PrimaryStreamID = 3;
    Step3.bIsPivotalStep = false;
    Step3.NestingLevel = 3;
    Step3.TriadicGroup = 2;
    Step3.Description = TEXT("Meta-cognitive reflection on perception and orientation");
    StepConfigs.Add(Step3);

    // Step 4: Integrate (Expressive, Stream 1)
    FCognitiveStepConfig Step4;
    Step4.StepNumber = 4;
    Step4.StepType = ECognitiveStepType::Integrate;
    Step4.Mode = ECognitiveModeType::Expressive;
    Step4.PrimaryStreamID = 1;
    Step4.bIsPivotalStep = false;
    Step4.NestingLevel = 4;
    Step4.TriadicGroup = 3;
    Step4.Description = TEXT("Integration of perceptual and reflective insights");
    StepConfigs.Add(Step4);

    // Step 5: Decide (Expressive, Stream 2) - PIVOTAL
    FCognitiveStepConfig Step5;
    Step5.StepNumber = 5;
    Step5.StepType = ECognitiveStepType::Decide;
    Step5.Mode = ECognitiveModeType::Expressive;
    Step5.PrimaryStreamID = 2;
    Step5.bIsPivotalStep = true;
    Step5.NestingLevel = 1;
    Step5.TriadicGroup = 0;
    Step5.Description = TEXT("Pivotal relevance realization - orienting present commitment");
    StepConfigs.Add(Step5);

    // Step 6: Simulate (Reflective, Stream 3)
    FCognitiveStepConfig Step6;
    Step6.StepNumber = 6;
    Step6.StepType = ECognitiveStepType::Simulate;
    Step6.Mode = ECognitiveModeType::Reflective;
    Step6.PrimaryStreamID = 3;
    Step6.bIsPivotalStep = false;
    Step6.NestingLevel = 2;
    Step6.TriadicGroup = 1;
    Step6.Description = TEXT("Virtual salience simulation - anticipating future potential");
    StepConfigs.Add(Step6);

    // Step 7: Act (Expressive, Stream 1)
    FCognitiveStepConfig Step7;
    Step7.StepNumber = 7;
    Step7.StepType = ECognitiveStepType::Act;
    Step7.Mode = ECognitiveModeType::Expressive;
    Step7.PrimaryStreamID = 1;
    Step7.bIsPivotalStep = false;
    Step7.NestingLevel = 3;
    Step7.TriadicGroup = 2;
    Step7.Description = TEXT("Actual affordance interaction - conditioning past performance");
    StepConfigs.Add(Step7);

    // Step 8: Observe (Expressive, Stream 2)
    FCognitiveStepConfig Step8;
    Step8.StepNumber = 8;
    Step8.StepType = ECognitiveStepType::Observe;
    Step8.Mode = ECognitiveModeType::Expressive;
    Step8.PrimaryStreamID = 2;
    Step8.bIsPivotalStep = false;
    Step8.NestingLevel = 4;
    Step8.TriadicGroup = 3;
    Step8.Description = TEXT("Observation of action outcomes and environmental changes");
    StepConfigs.Add(Step8);

    // Step 9: Learn (Reflective, Stream 3)
    FCognitiveStepConfig Step9;
    Step9.StepNumber = 9;
    Step9.StepType = ECognitiveStepType::Learn;
    Step9.Mode = ECognitiveModeType::Reflective;
    Step9.PrimaryStreamID = 3;
    Step9.bIsPivotalStep = false;
    Step9.NestingLevel = 1;
    Step9.TriadicGroup = 0;
    Step9.Description = TEXT("Learning from action-outcome contingencies");
    StepConfigs.Add(Step9);

    // Step 10: Consolidate (Expressive, Stream 1)
    FCognitiveStepConfig Step10;
    Step10.StepNumber = 10;
    Step10.StepType = ECognitiveStepType::Consolidate;
    Step10.Mode = ECognitiveModeType::Expressive;
    Step10.PrimaryStreamID = 1;
    Step10.bIsPivotalStep = false;
    Step10.NestingLevel = 2;
    Step10.TriadicGroup = 1;
    Step10.Description = TEXT("Consolidation of learned patterns into memory");
    StepConfigs.Add(Step10);

    // Step 11: Anticipate (Reflective, Stream 2)
    FCognitiveStepConfig Step11;
    Step11.StepNumber = 11;
    Step11.StepType = ECognitiveStepType::Anticipate;
    Step11.Mode = ECognitiveModeType::Reflective;
    Step11.PrimaryStreamID = 2;
    Step11.bIsPivotalStep = false;
    Step11.NestingLevel = 3;
    Step11.TriadicGroup = 2;
    Step11.Description = TEXT("Anticipation of future states and possibilities");
    StepConfigs.Add(Step11);

    // Step 12: Transcend (Reflective, Stream 3) - PIVOTAL
    FCognitiveStepConfig Step12;
    Step12.StepNumber = 12;
    Step12.StepType = ECognitiveStepType::Transcend;
    Step12.Mode = ECognitiveModeType::Reflective;
    Step12.PrimaryStreamID = 3;
    Step12.bIsPivotalStep = true;
    Step12.NestingLevel = 4;
    Step12.TriadicGroup = 3;
    Step12.Description = TEXT("Pivotal relevance realization - transcending current frame");
    StepConfigs.Add(Step12);
}

void UCognitiveCycleManager::InitializeStreams()
{
    CycleState.Streams.Empty();

    // Stream 1: Perceiving-Acting (Expressive dominant)
    FStreamState Stream1;
    Stream1.StreamID = 1;
    Stream1.Role = EStreamRole::Perceiving;
    Stream1.CurrentPhase = 1;
    Stream1.PhaseOffset = 0;
    Stream1.ActivationLevel = 0.5f;
    Stream1.Coherence = 1.0f;
    Stream1.ReservoirState.SetNum(100);
    CycleState.Streams.Add(Stream1);

    // Stream 2: Orienting-Deciding (Balanced)
    FStreamState Stream2;
    Stream2.StreamID = 2;
    Stream2.Role = EStreamRole::Acting;
    Stream2.CurrentPhase = 5; // 4 steps offset
    Stream2.PhaseOffset = 4;
    Stream2.ActivationLevel = 0.5f;
    Stream2.Coherence = 1.0f;
    Stream2.ReservoirState.SetNum(100);
    CycleState.Streams.Add(Stream2);

    // Stream 3: Reflecting-Simulating (Reflective dominant)
    FStreamState Stream3;
    Stream3.StreamID = 3;
    Stream3.Role = EStreamRole::Reflecting;
    Stream3.CurrentPhase = 9; // 8 steps offset
    Stream3.PhaseOffset = 8;
    Stream3.ActivationLevel = 0.5f;
    Stream3.Coherence = 1.0f;
    Stream3.ReservoirState.SetNum(100);
    CycleState.Streams.Add(Stream3);

    // Initialize triadic sync state
    CycleState.TriadicSync.TriadIndex = 0;
    CycleState.TriadicSync.TriadSteps = {1, 5, 9};
    CycleState.TriadicSync.SyncQuality = 1.0f;
    CycleState.TriadicSync.PhaseAlignment = 1.0f;
    CycleState.TriadicSync.CoherenceMatrix.SetNum(9);
    for (int32 i = 0; i < 9; ++i)
    {
        CycleState.TriadicSync.CoherenceMatrix[i] = (i % 4 == 0) ? 1.0f : 0.5f;
    }
}

void UCognitiveCycleManager::InitializeSys6State()
{
    CycleState.Sys6State.CurrentStep = 1;
    CycleState.Sys6State.CurrentPhase = 1;
    CycleState.Sys6State.CurrentStage = 1;
    CycleState.Sys6State.DyadState = 0; // A
    CycleState.Sys6State.TriadState = 1;
    CycleState.Sys6State.ActiveThreads = {1, 2};
    CycleState.Sys6State.EntanglementLevel = 0.0f;
}

void UCognitiveCycleManager::UpdateCycleTimer(float DeltaTime)
{
    if (!bEnableAutoCycle)
    {
        return;
    }

    StepTimer += DeltaTime;
    float StepDuration = CycleDuration / 12.0f;

    if (StepTimer >= StepDuration)
    {
        StepTimer -= StepDuration;
        AdvanceStep();
    }

    // Update cycle progress
    CycleState.CycleProgress = (CycleState.CurrentStep - 1 + StepTimer / StepDuration) / 12.0f;
}

void UCognitiveCycleManager::UpdateStreams(float DeltaTime)
{
    for (FStreamState& Stream : CycleState.Streams)
    {
        // Update phase based on current step and offset
        Stream.CurrentPhase = ((CycleState.CurrentStep + Stream.PhaseOffset - 1) % 12) + 1;

        // Update activation based on whether this stream is primary for current step
        FCognitiveStepConfig CurrentConfig = GetCurrentStepConfig();
        if (CurrentConfig.PrimaryStreamID == Stream.StreamID)
        {
            // This stream is active
            Stream.ActivationLevel = FMath::Lerp(Stream.ActivationLevel, 1.0f, DeltaTime * 5.0f);
        }
        else
        {
            // This stream is passive
            Stream.ActivationLevel = FMath::Lerp(Stream.ActivationLevel, 0.3f, DeltaTime * 2.0f);
        }
    }

    // Update coherence
    UpdateStreamCoherence();
}

void UCognitiveCycleManager::UpdateSys6State(float DeltaTime)
{
    Sys6Timer += DeltaTime;
    float Sys6StepDuration = CycleDuration / 30.0f;

    if (Sys6Timer >= Sys6StepDuration)
    {
        Sys6Timer -= Sys6StepDuration;

        // Advance sys6 step
        CycleState.Sys6State.CurrentStep = (CycleState.Sys6State.CurrentStep % 30) + 1;

        // Update phase (3 phases, 10 steps each)
        CycleState.Sys6State.CurrentPhase = ((CycleState.Sys6State.CurrentStep - 1) / 10) + 1;

        // Update stage (5 stages per phase)
        CycleState.Sys6State.CurrentStage = (((CycleState.Sys6State.CurrentStep - 1) % 10) / 2) + 1;

        // Update double step delay pattern
        UpdateSys6DoubleStepPattern();

        // Update thread multiplexing
        UpdateThreadMultiplexing();
    }
}

void UCognitiveCycleManager::UpdateSys6DoubleStepPattern()
{
    // Alternating double step delay pattern:
    // Step 1: A,1 -> Step 2: A,2 -> Step 3: B,2 -> Step 4: B,3 -> (repeat)
    int32 PatternStep = ((CycleState.Sys6State.CurrentStep - 1) % 4) + 1;

    switch (PatternStep)
    {
        case 1:
            CycleState.Sys6State.DyadState = 0; // A
            CycleState.Sys6State.TriadState = 1;
            break;
        case 2:
            CycleState.Sys6State.DyadState = 0; // A
            CycleState.Sys6State.TriadState = 2;
            break;
        case 3:
            CycleState.Sys6State.DyadState = 1; // B
            CycleState.Sys6State.TriadState = 2;
            break;
        case 4:
            CycleState.Sys6State.DyadState = 1; // B
            CycleState.Sys6State.TriadState = 3;
            break;
    }
}

void UCognitiveCycleManager::UpdateThreadMultiplexing()
{
    // Thread permutation pattern: P(1,2)→P(1,3)→P(1,4)→P(2,3)→P(2,4)→P(3,4)
    static const int32 Permutations[6][2] = {
        {1, 2}, {1, 3}, {1, 4}, {2, 3}, {2, 4}, {3, 4}
    };

    int32 PermutationIndex = (CycleState.Sys6State.CurrentStep - 1) % 6;
    CycleState.Sys6State.ActiveThreads.Empty();
    CycleState.Sys6State.ActiveThreads.Add(Permutations[PermutationIndex][0]);
    CycleState.Sys6State.ActiveThreads.Add(Permutations[PermutationIndex][1]);

    // Entanglement level based on thread overlap with previous state
    // (Simplified: higher when threads change)
    CycleState.Sys6State.EntanglementLevel = FMath::FRandRange(0.3f, 0.7f);
}

void UCognitiveCycleManager::AdvanceStep()
{
    int32 OldStep = CycleState.CurrentStep;
    ECognitiveModeType OldMode = CycleState.CurrentMode;

    // Advance to next step
    CycleState.CurrentStep = (CycleState.CurrentStep % 12) + 1;

    // Check for cycle completion
    if (CycleState.CurrentStep == 1)
    {
        CycleState.CycleCount++;
        OnCycleCompleted.Broadcast(CycleState.CycleCount);
    }

    // Update mode
    FCognitiveStepConfig NewConfig = GetCurrentStepConfig();
    CycleState.CurrentMode = NewConfig.Mode;

    // Process step transition
    ProcessStepTransition(OldStep, CycleState.CurrentStep);

    // Broadcast step change
    OnStepChanged.Broadcast(OldStep, CycleState.CurrentStep);

    // Check for mode change
    if (OldMode != CycleState.CurrentMode)
    {
        ProcessModeTransition(OldMode, CycleState.CurrentMode);
        OnModeChanged.Broadcast(OldMode, CycleState.CurrentMode);
    }

    // Check for pivotal step
    if (NewConfig.bIsPivotalStep)
    {
        OnPivotalStep.Broadcast(CycleState.CurrentStep);
    }
}

void UCognitiveCycleManager::JumpToStep(int32 Step)
{
    if (Step < 1 || Step > 12)
    {
        return;
    }

    int32 OldStep = CycleState.CurrentStep;
    CycleState.CurrentStep = Step;

    FCognitiveStepConfig NewConfig = GetCurrentStepConfig();
    CycleState.CurrentMode = NewConfig.Mode;

    ProcessStepTransition(OldStep, Step);
    OnStepChanged.Broadcast(OldStep, Step);
}

void UCognitiveCycleManager::PauseCycle()
{
    bIsPaused = true;
}

void UCognitiveCycleManager::ResumeCycle()
{
    bIsPaused = false;
}

void UCognitiveCycleManager::ResetCycle()
{
    InitializeCycle();
}

void UCognitiveCycleManager::ProcessStepTransition(int32 OldStep, int32 NewStep)
{
    // Propagate activation between steps
    PropagateActivation();

    // Update triadic group
    FCognitiveStepConfig NewConfig = GetStepConfig(NewStep);
    CycleState.TriadicSync.TriadIndex = NewConfig.TriadicGroup;

    // Update triad steps
    switch (NewConfig.TriadicGroup)
    {
        case 0: CycleState.TriadicSync.TriadSteps = {1, 5, 9}; break;
        case 1: CycleState.TriadicSync.TriadSteps = {2, 6, 10}; break;
        case 2: CycleState.TriadicSync.TriadSteps = {3, 7, 11}; break;
        case 3: CycleState.TriadicSync.TriadSteps = {4, 8, 12}; break;
    }
}

void UCognitiveCycleManager::ProcessTriadicSync()
{
    SynchronizeStreamsAtTriad();

    // Compute sync quality
    float Quality = ComputeOverallCoherence();
    CycleState.TriadicSync.SyncQuality = Quality;

    // Broadcast triadic sync event
    OnTriadicSync.Broadcast(CycleState.TriadicSync.TriadIndex, Quality);
}

void UCognitiveCycleManager::ProcessModeTransition(ECognitiveModeType OldMode, ECognitiveModeType NewMode)
{
    // Mode transitions affect stream dynamics
    if (NewMode == ECognitiveModeType::Reflective)
    {
        // Increase coherence in reflective mode
        for (FStreamState& Stream : CycleState.Streams)
        {
            Stream.Coherence = FMath::Min(1.0f, Stream.Coherence + 0.1f);
        }
    }
    else
    {
        // Allow more divergence in expressive mode
        for (FStreamState& Stream : CycleState.Streams)
        {
            Stream.Coherence = FMath::Max(0.5f, Stream.Coherence - 0.05f);
        }
    }
}

void UCognitiveCycleManager::SynchronizeStreamsAtTriad()
{
    if (CycleState.Streams.Num() < 3)
    {
        return;
    }

    // Compute average activation
    float AvgActivation = 0.0f;
    for (const FStreamState& Stream : CycleState.Streams)
    {
        AvgActivation += Stream.ActivationLevel;
    }
    AvgActivation /= CycleState.Streams.Num();

    // Blend toward average
    for (FStreamState& Stream : CycleState.Streams)
    {
        Stream.ActivationLevel = FMath::Lerp(Stream.ActivationLevel, AvgActivation, StreamCouplingStrength);
    }

    // Synchronize reservoir states if available
    if (ReservoirComponent)
    {
        ReservoirComponent->SynchronizeStreams();
    }
}

void UCognitiveCycleManager::UpdateStreamCoherence()
{
    // Update coherence matrix
    for (int32 i = 0; i < CycleState.Streams.Num(); ++i)
    {
        for (int32 j = 0; j < CycleState.Streams.Num(); ++j)
        {
            int32 MatrixIndex = i * 3 + j;
            if (MatrixIndex < CycleState.TriadicSync.CoherenceMatrix.Num())
            {
                if (i == j)
                {
                    CycleState.TriadicSync.CoherenceMatrix[MatrixIndex] = 1.0f;
                }
                else
                {
                    CycleState.TriadicSync.CoherenceMatrix[MatrixIndex] = ComputeStreamCoherence(i, j);
                }
            }
        }
    }

    // Update individual stream coherence
    for (int32 i = 0; i < CycleState.Streams.Num(); ++i)
    {
        float TotalCoherence = 0.0f;
        int32 Count = 0;
        for (int32 j = 0; j < CycleState.Streams.Num(); ++j)
        {
            if (i != j)
            {
                TotalCoherence += ComputeStreamCoherence(i, j);
                ++Count;
            }
        }
        CycleState.Streams[i].Coherence = Count > 0 ? TotalCoherence / Count : 1.0f;
    }

    // Update phase alignment
    float PhaseSum = 0.0f;
    for (const FStreamState& Stream : CycleState.Streams)
    {
        PhaseSum += Stream.CurrentPhase;
    }
    float AvgPhase = PhaseSum / CycleState.Streams.Num();
    float PhaseVariance = 0.0f;
    for (const FStreamState& Stream : CycleState.Streams)
    {
        PhaseVariance += FMath::Square(Stream.CurrentPhase - AvgPhase);
    }
    PhaseVariance /= CycleState.Streams.Num();
    CycleState.TriadicSync.PhaseAlignment = 1.0f / (1.0f + PhaseVariance * 0.1f);
}

void UCognitiveCycleManager::PropagateActivation()
{
    // Propagate activation from active stream to others
    int32 ActiveStreamID = GetActiveStreamID();

    for (FStreamState& Stream : CycleState.Streams)
    {
        if (Stream.StreamID != ActiveStreamID)
        {
            // Receive propagated activation
            float PropagatedActivation = 0.0f;
            for (const FStreamState& OtherStream : CycleState.Streams)
            {
                if (OtherStream.StreamID == ActiveStreamID)
                {
                    PropagatedActivation = OtherStream.ActivationLevel * StreamCouplingStrength;
                    break;
                }
            }
            Stream.ActivationLevel = FMath::Lerp(Stream.ActivationLevel, PropagatedActivation, 0.1f);
        }
    }
}

float UCognitiveCycleManager::ComputeStreamCoherence(int32 StreamA, int32 StreamB) const
{
    if (StreamA < 0 || StreamA >= CycleState.Streams.Num() ||
        StreamB < 0 || StreamB >= CycleState.Streams.Num())
    {
        return 0.0f;
    }

    const FStreamState& A = CycleState.Streams[StreamA];
    const FStreamState& B = CycleState.Streams[StreamB];

    // Coherence based on activation similarity and phase relationship
    float ActivationSimilarity = 1.0f - FMath::Abs(A.ActivationLevel - B.ActivationLevel);

    // Phase coherence (streams should be 4 steps apart)
    int32 ExpectedPhaseDiff = 4;
    int32 ActualPhaseDiff = FMath::Abs(A.CurrentPhase - B.CurrentPhase);
    ActualPhaseDiff = FMath::Min(ActualPhaseDiff, 12 - ActualPhaseDiff);
    float PhaseCoherence = 1.0f - FMath::Abs(ActualPhaseDiff - ExpectedPhaseDiff) / 6.0f;

    return (ActivationSimilarity + PhaseCoherence) / 2.0f;
}

float UCognitiveCycleManager::ComputeOverallCoherence() const
{
    float TotalCoherence = 0.0f;
    int32 Pairs = 0;

    for (int32 i = 0; i < CycleState.Streams.Num(); ++i)
    {
        for (int32 j = i + 1; j < CycleState.Streams.Num(); ++j)
        {
            TotalCoherence += ComputeStreamCoherence(i, j);
            ++Pairs;
        }
    }

    return Pairs > 0 ? TotalCoherence / Pairs : 1.0f;
}

float UCognitiveCycleManager::ComputeRelevanceRealization() const
{
    // Relevance realization combines:
    // - Overall coherence
    // - Triadic sync quality
    // - Mode appropriateness

    float CoherenceContribution = CycleState.OverallCoherence * 0.4f;
    float SyncContribution = CycleState.TriadicSync.SyncQuality * 0.3f;

    // Mode appropriateness based on step type
    float ModeAppropriate = 1.0f;
    FCognitiveStepConfig Config = GetCurrentStepConfig();
    if ((Config.Mode == ECognitiveModeType::Reflective && 
         (Config.StepType == ECognitiveStepType::Reflect || 
          Config.StepType == ECognitiveStepType::Simulate ||
          Config.StepType == ECognitiveStepType::Learn ||
          Config.StepType == ECognitiveStepType::Anticipate ||
          Config.StepType == ECognitiveStepType::Transcend)) ||
        (Config.Mode == ECognitiveModeType::Expressive &&
         (Config.StepType == ECognitiveStepType::Perceive ||
          Config.StepType == ECognitiveStepType::Orient ||
          Config.StepType == ECognitiveStepType::Integrate ||
          Config.StepType == ECognitiveStepType::Decide ||
          Config.StepType == ECognitiveStepType::Act ||
          Config.StepType == ECognitiveStepType::Observe ||
          Config.StepType == ECognitiveStepType::Consolidate)))
    {
        ModeAppropriate = 1.0f;
    }
    else
    {
        ModeAppropriate = 0.7f;
    }

    float ModeContribution = ModeAppropriate * 0.3f;

    return CoherenceContribution + SyncContribution + ModeContribution;
}

// State query implementations
int32 UCognitiveCycleManager::GetCurrentStep() const
{
    return CycleState.CurrentStep;
}

ECognitiveModeType UCognitiveCycleManager::GetCurrentMode() const
{
    return CycleState.CurrentMode;
}

FCognitiveStepConfig UCognitiveCycleManager::GetCurrentStepConfig() const
{
    return GetStepConfig(CycleState.CurrentStep);
}

FCognitiveStepConfig UCognitiveCycleManager::GetStepConfig(int32 Step) const
{
    if (Step >= 1 && Step <= StepConfigs.Num())
    {
        return StepConfigs[Step - 1];
    }
    return FCognitiveStepConfig();
}

FCognitiveCycleState UCognitiveCycleManager::GetCycleState() const
{
    return CycleState;
}

bool UCognitiveCycleManager::IsCurrentStepPivotal() const
{
    return GetCurrentStepConfig().bIsPivotalStep;
}

bool UCognitiveCycleManager::IsTriadicSyncPoint() const
{
    // Triadic sync at steps 1, 5, 9 (every 4 steps starting from 1)
    return (CycleState.CurrentStep % 4) == 1;
}

int32 UCognitiveCycleManager::GetCurrentTriadicGroup() const
{
    return GetCurrentStepConfig().TriadicGroup;
}

FStreamState UCognitiveCycleManager::GetStreamState(int32 StreamID) const
{
    if (StreamID >= 1 && StreamID <= CycleState.Streams.Num())
    {
        return CycleState.Streams[StreamID - 1];
    }
    return FStreamState();
}

int32 UCognitiveCycleManager::GetActiveStreamID() const
{
    return GetCurrentStepConfig().PrimaryStreamID;
}

void UCognitiveCycleManager::SetStreamFocus(int32 StreamID, const FString& Focus)
{
    if (StreamID >= 1 && StreamID <= CycleState.Streams.Num())
    {
        CycleState.Streams[StreamID - 1].CurrentFocus = Focus;
    }
}

float UCognitiveCycleManager::GetInterStreamCoherence() const
{
    return CycleState.OverallCoherence;
}

void UCognitiveCycleManager::ForceSynchronize()
{
    SynchronizeStreamsAtTriad();
}

FSys6TrialityState UCognitiveCycleManager::GetSys6State() const
{
    return CycleState.Sys6State;
}

FString UCognitiveCycleManager::GetCurrentDyadState() const
{
    return CycleState.Sys6State.DyadState == 0 ? TEXT("A") : TEXT("B");
}

int32 UCognitiveCycleManager::GetCurrentTriadState() const
{
    return CycleState.Sys6State.TriadState;
}

float UCognitiveCycleManager::GetEntanglementLevel() const
{
    return CycleState.Sys6State.EntanglementLevel;
}

TArray<float> UCognitiveCycleManager::ProcessInput(const TArray<float>& Input)
{
    TArray<float> Output;

    // Process through reservoir if available
    if (ReservoirComponent)
    {
        int32 ActiveStream = GetActiveStreamID();
        Output = ReservoirComponent->ProcessInput(Input, ActiveStream);
    }
    else
    {
        // Simple pass-through
        Output = Input;
    }

    return Output;
}

float UCognitiveCycleManager::GetRelevanceRealizationLevel() const
{
    return CycleState.RelevanceRealizationLevel;
}


// ========================================
// LEGACY FUNCTIONS (merged from Cognitive version)
// ========================================

void UCognitiveCycleManager::InitializeNestedShells()
{
    NestedShells.Empty();

    // OEIS A000081 structure:
    // Level 1: 1 term, 1 step apart
    // Level 2: 2 terms, 2 steps apart
    // Level 3: 4 terms, 3 steps apart
    // Level 4: 9 terms, 4 steps apart

    const int32 TermCounts[] = {1, 2, 4, 9};
    const int32 StepsApart[] = {1, 2, 3, 4};

    for (int32 i = 0; i < 4; ++i)
    {
        FNestedShellState Shell;
        Shell.NestingLevel = i + 1;
        Shell.TermCount = TermCounts[i];
        Shell.StepsApart = StepsApart[i];
        Shell.Coherence = 1.0f;

        // Initialize term values
        Shell.TermValues.SetNum(Shell.TermCount);
        for (int32 j = 0; j < Shell.TermCount; ++j)
        {
            Shell.TermValues[j] = 0.5f;
        }

        NestedShells.Add(Shell);
    }
}

void UCognitiveCycleManager::InitializeTriads()
{
    TriadStates.Empty();

    // Triad 1: {1, 5, 9}
    FTriadState Triad1;
    Triad1.Group = ETriadGroup::Triad1;
    Triad1.Steps = {1, 5, 9};
    Triad1.Activation = 0.0f;
    Triad1.Coherence = 1.0f;
    TriadStates.Add(Triad1);

    // Triad 2: {2, 6, 10}
    FTriadState Triad2;
    Triad2.Group = ETriadGroup::Triad2;
    Triad2.Steps = {2, 6, 10};
    Triad2.Activation = 0.0f;
    Triad2.Coherence = 1.0f;
    TriadStates.Add(Triad2);

    // Triad 3: {3, 7, 11}
    FTriadState Triad3;
    Triad3.Group = ETriadGroup::Triad3;
    Triad3.Steps = {3, 7, 11};
    Triad3.Activation = 0.0f;
    Triad3.Coherence = 1.0f;
    TriadStates.Add(Triad3);

    // Triad 4: {4, 8, 12}
    FTriadState Triad4;
    Triad4.Group = ETriadGroup::Triad4;
    Triad4.Steps = {4, 8, 12};
    Triad4.Activation = 0.0f;
    Triad4.Coherence = 1.0f;
    TriadStates.Add(Triad4);
}

void UCognitiveCycleManager::AdvanceSteps(int32 Steps)
{
    for (int32 i = 0; i < Steps; ++i)
    {
        AdvanceStep();
    }
}

void UCognitiveCycleManager::ProcessCurrentStep()
{
    // Process based on current step type
    ECognitiveStepType StepType = CalculateStepType(CycleState.CurrentStep);
    
    switch (StepType)
    {
        case ECognitiveStepType::RelevanceRealization:
            // Pivotal step: orient present commitment
            for (FStreamState& Stream : CycleState.Streams)
            {
                // Relevance realization affects all streams equally
                Stream.SalienceValue = FMath::Lerp(Stream.SalienceValue, 0.5f, 0.3f);
                Stream.AffordanceValue = FMath::Lerp(Stream.AffordanceValue, 0.5f, 0.3f);
            }
            break;

        case ECognitiveStepType::AffordanceInteraction:
            // Actual affordance: condition past performance
            for (FStreamState& Stream : CycleState.Streams)
            {
                if (Stream.StreamType == EConsciousnessStream::Action)
                {
                    // Action stream is primary during affordance interaction
                    Stream.ActivationLevel = FMath::Min(1.0f, Stream.ActivationLevel + 0.1f);
                }
            }
            break;

        case ECognitiveStepType::SalienceSimulation:
            // Virtual salience: anticipate future potential
            for (FStreamState& Stream : CycleState.Streams)
            {
                if (Stream.StreamType == EConsciousnessStream::Simulation)
                {
                    // Simulation stream is primary during salience simulation
                    Stream.ActivationLevel = FMath::Min(1.0f, Stream.ActivationLevel + 0.1f);
                }
            }
            break;
            
        default:
            break;
    }
}

FStreamState UCognitiveCycleManager::GetStreamStateByType(EConsciousnessStream StreamType) const
{
    for (const FStreamState& Stream : CycleState.Streams)
    {
        if (Stream.StreamType == StreamType)
        {
            return Stream;
        }
    }
    return FStreamState();
}

int32 UCognitiveCycleManager::GetStreamStep(EConsciousnessStream StreamType) const
{
    for (const FStreamState& Stream : CycleState.Streams)
    {
        if (Stream.StreamType == StreamType)
        {
            return Stream.CurrentStep;
        }
    }
    return 1;
}

float UCognitiveCycleManager::GetStreamActivation(EConsciousnessStream StreamType) const
{
    for (const FStreamState& Stream : CycleState.Streams)
    {
        if (Stream.StreamType == StreamType)
        {
            return Stream.ActivationLevel;
        }
    }
    return 0.0f;
}

void UCognitiveCycleManager::SetStreamSalience(EConsciousnessStream StreamType, float Salience)
{
    for (FStreamState& Stream : CycleState.Streams)
    {
        if (Stream.StreamType == StreamType)
        {
            Stream.SalienceValue = FMath::Clamp(Salience, 0.0f, 1.0f);
            return;
        }
    }
}

void UCognitiveCycleManager::SetStreamAffordance(EConsciousnessStream StreamType, float Affordance)
{
    for (FStreamState& Stream : CycleState.Streams)
    {
        if (Stream.StreamType == StreamType)
        {
            Stream.AffordanceValue = FMath::Clamp(Affordance, 0.0f, 1.0f);
            return;
        }
    }
}

ETriadGroup UCognitiveCycleManager::GetCurrentTriad() const
{
    return CycleState.CurrentTriad;
}

FTriadState UCognitiveCycleManager::GetTriadState(ETriadGroup Group) const
{
    int32 Index = static_cast<int32>(Group);
    if (TriadStates.IsValidIndex(Index))
    {
        return TriadStates[Index];
    }
    return FTriadState();
}

bool UCognitiveCycleManager::IsStepInTriad(int32 Step, ETriadGroup Group) const
{
    int32 Index = static_cast<int32>(Group);
    if (TriadStates.IsValidIndex(Index))
    {
        return TriadStates[Index].Steps.Contains(Step);
    }
    return false;
}

FNestedShellState UCognitiveCycleManager::GetNestedShellState(int32 Level) const
{
    int32 Index = Level - 1;
    if (NestedShells.IsValidIndex(Index))
    {
        return NestedShells[Index];
    }
    return FNestedShellState();
}

int32 UCognitiveCycleManager::GetTotalNestedTerms() const
{
    // OEIS A000081: 1 + 2 + 4 + 9 = 16 total terms
    int32 Total = 0;
    for (const FNestedShellState& Shell : NestedShells)
    {
        Total += Shell.TermCount;
    }
    return Total;
}

float UCognitiveCycleManager::GetPerceptionOfAction() const
{
    // Stream 1 (Perception) perceives Stream 2's (Action) state
    float PerceptionActivation = 0.0f;
    float ActionAffordance = 0.0f;

    for (const FStreamState& Stream : CycleState.Streams)
    {
        if (Stream.StreamType == EConsciousnessStream::Perception)
        {
            PerceptionActivation = Stream.ActivationLevel;
        }
        else if (Stream.StreamType == EConsciousnessStream::Action)
        {
            ActionAffordance = Stream.AffordanceValue;
        }
    }

    return PerceptionActivation * ActionAffordance;
}

float UCognitiveCycleManager::GetSimulationOfAction() const
{
    // Stream 3 (Simulation) reflects on Stream 2's (Action) state
    float SimulationActivation = 0.0f;
    float ActionAffordance = 0.0f;

    for (const FStreamState& Stream : CycleState.Streams)
    {
        if (Stream.StreamType == EConsciousnessStream::Simulation)
        {
            SimulationActivation = Stream.ActivationLevel;
        }
        else if (Stream.StreamType == EConsciousnessStream::Action)
        {
            ActionAffordance = Stream.AffordanceValue;
        }
    }

    return SimulationActivation * ActionAffordance;
}

float UCognitiveCycleManager::CalculateInterStreamCoherence() const
{
    if (CycleState.Streams.Num() < 3)
    {
        return 0.0f;
    }

    // Calculate coherence based on how well streams are synchronized
    float TotalCoherence = 0.0f;
    for (const FStreamState& Stream : CycleState.Streams)
    {
        TotalCoherence += Stream.InterStreamCoherence;
    }

    return TotalCoherence / CycleState.Streams.Num();
}

ECognitiveStepType UCognitiveCycleManager::GetStepType(int32 Step) const
{
    return CalculateStepType(Step);
}

ECognitiveModeType UCognitiveCycleManager::GetStepMode(int32 Step) const
{
    return CalculateStepMode(Step);
}

bool UCognitiveCycleManager::IsRelevanceRealizationStep(int32 Step) const
{
    // Steps 1 and 7 are pivotal relevance realization steps
    return (Step == 1 || Step == 7);
}

void UCognitiveCycleManager::UpdateStream(FStreamState& Stream)
{
    // Calculate stream's current step based on global step and phase offset
    int32 StreamStep = ((CycleState.CurrentStep - 1 + Stream.PhaseOffset) % 12) + 1;
    Stream.CurrentStep = StreamStep;

    // Update mode and step type
    Stream.Mode = CalculateStepMode(StreamStep);
    Stream.StepType = CalculateStepType(StreamStep);

    // Decay activation slightly each step
    Stream.ActivationLevel = FMath::Max(0.5f, Stream.ActivationLevel - 0.02f);
}

void UCognitiveCycleManager::UpdateNestedShells()
{
    for (FNestedShellState& Shell : NestedShells)
    {
        // Update terms based on current step
        int32 ActiveTermIndex = (CycleState.CurrentStep - 1) % Shell.TermCount;
        
        for (int32 i = 0; i < Shell.TermValues.Num(); ++i)
        {
            if (i == ActiveTermIndex)
            {
                // Active term increases
                Shell.TermValues[i] = FMath::Min(1.0f, Shell.TermValues[i] + 0.1f);
            }
            else
            {
                // Inactive terms decay
                Shell.TermValues[i] = FMath::Max(0.0f, Shell.TermValues[i] - 0.05f);
            }
        }

        // Calculate shell coherence
        float Sum = 0.0f;
        for (float Value : Shell.TermValues)
        {
            Sum += Value;
        }
        Shell.Coherence = Sum / FMath::Max(1, Shell.TermCount);
    }
}

void UCognitiveCycleManager::UpdateTriadActivations()
{
    for (FTriadState& Triad : TriadStates)
    {
        // Check if current step is in this triad
        bool bIsActive = Triad.Steps.Contains(CycleState.CurrentStep);
        
        if (bIsActive)
        {
            Triad.Activation = FMath::Min(1.0f, Triad.Activation + 0.3f);
        }
        else
        {
            Triad.Activation = FMath::Max(0.0f, Triad.Activation - 0.1f);
        }
    }
}

ECognitiveStepType UCognitiveCycleManager::CalculateStepType(int32 Step) const
{
    // Step composition:
    // 1: Pivotal relevance realization (orienting present commitment)
    // 2-6: Actual affordance interaction (conditioning past performance)
    // 7: Pivotal relevance realization (orienting present commitment)
    // 8-12: Virtual salience simulation (anticipating future potential)

    if (Step == 1 || Step == 7)
    {
        return ECognitiveStepType::RelevanceRealization;
    }
    else if (Step >= 2 && Step <= 6)
    {
        return ECognitiveStepType::AffordanceInteraction;
    }
    else // Steps 8-12
    {
        return ECognitiveStepType::SalienceSimulation;
    }
}

ECognitiveModeType UCognitiveCycleManager::CalculateStepMode(int32 Step) const
{
    // 7 expressive steps: 1, 2, 3, 4, 5, 6, 7
    // 5 reflective steps: 8, 9, 10, 11, 12

    if (Step <= 7)
    {
        return ECognitiveModeType::Expressive;
    }
    else
    {
        return ECognitiveModeType::Reflective;
    }
}

ETriadGroup UCognitiveCycleManager::CalculateTriadGroup(int32 Step) const
{
    // Triads: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
    // Pattern: (Step - 1) % 4

    int32 TriadIndex = (Step - 1) % 4;
    return static_cast<ETriadGroup>(TriadIndex);
}

void UCognitiveCycleManager::ApplyInterStreamFeedback()
{
    // Inter-stream awareness: each stream is aware of others' states
    // Stream 1 perceives Stream 2's action
    // Stream 3 reflects on a simulation of the action

    float PerceptionOfAction = GetPerceptionOfAction();
    float SimulationOfAction = GetSimulationOfAction();

    for (FStreamState& Stream : CycleState.Streams)
    {
        switch (Stream.StreamType)
        {
            case EConsciousnessStream::Perception:
                // Perception is influenced by action feedback
                Stream.InterStreamCoherence = FMath::Lerp(Stream.InterStreamCoherence, PerceptionOfAction, 0.1f);
                break;

            case EConsciousnessStream::Action:
                // Action is influenced by both perception and simulation
                Stream.InterStreamCoherence = FMath::Lerp(Stream.InterStreamCoherence, 
                    (PerceptionOfAction + SimulationOfAction) * 0.5f, 0.1f);
                break;

            case EConsciousnessStream::Simulation:
                // Simulation is influenced by action
                Stream.InterStreamCoherence = FMath::Lerp(Stream.InterStreamCoherence, SimulationOfAction, 0.1f);
                break;
        }
    }
}

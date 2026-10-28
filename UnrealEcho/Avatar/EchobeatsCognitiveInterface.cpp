#include "EchobeatsCognitiveInterface.h"
#include "../Neurochemical/NeurochemicalSimulationComponent.h"

UEchobeatsCognitiveInterface::UEchobeatsCognitiveInterface()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    ExpressionSystem = nullptr;
    StateMachine = nullptr;
    ProceduralAnim = nullptr;
    Neurochemistry = nullptr;

    bLoopRunning = false;
    bLoopPaused = false;
    LoopTimer = 0.0f;
    StepDuration = 0.5f;  // 2 steps per second = 6 second cycle
    CurrentStep = 1;
    CycleCount = 0;
    CurrentMode = ECognitiveMode::Expressive;

    // Initialize stream states
    for (int32 i = 0; i < 3; i++)
    {
        StreamStates[i].StreamIndex = i;
        StreamStates[i].Phase = ECognitiveStreamPhase::Perception;
        StreamStates[i].Salience = 0.5f;
        StreamStates[i].Activation = 0.5f;
        StreamStates[i].CurrentStep = 1 + (i * 4);  // Phased 4 steps apart
    }

    bExternalSpeaking = false;
    ExternalSpeakingAmplitude = 0.0f;
    bExternalListening = false;
    bExternalThinking = false;
    ExternalThinkingIntensity = 0.0f;
}

void UEchobeatsCognitiveInterface::BeginPlay()
{
    Super::BeginPlay();

    // Find components on owner
    if (AActor* Owner = GetOwner())
    {
        ExpressionSystem = Owner->FindComponentByClass<UDeepTreeEchoExpressionSystem>();
        StateMachine = Owner->FindComponentByClass<UExpressionStateMachine>();
        ProceduralAnim = Owner->FindComponentByClass<UProceduralAnimationComponent>();
        Neurochemistry = Owner->FindComponentByClass<UNeurochemicalSimulationComponent>();
    }

    InitializeStepConfigs();

    UE_LOG(LogTemp, Log, TEXT("EchobeatsCognitiveInterface initialized with %d step configs (affect source: %s)"),
        StepConfigs.Num(), Neurochemistry ? TEXT("neurochemistry") : TEXT("none"));
}

void UEchobeatsCognitiveInterface::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bLoopRunning || bLoopPaused)
        return;

    // Advance loop timer
    LoopTimer += DeltaTime;

    if (LoopTimer >= StepDuration)
    {
        LoopTimer -= StepDuration;

        // Advance to next step
        int32 NextStep = CurrentStep + 1;
        if (NextStep > 12)
        {
            NextStep = 1;
            CycleCount++;
            OnCognitiveCycleComplete.Broadcast(CycleCount);
        }

        ProcessStepChange(NextStep);
    }

    // Update external state integration
    if (bExternalSpeaking && ProceduralAnim)
    {
        ProceduralAnim->SetSpeakingState(true);
    }
    if (bExternalListening && ProceduralAnim)
    {
        ProceduralAnim->SetListeningState(true);
    }
    if (bExternalThinking && ProceduralAnim)
    {
        ProceduralAnim->SetThinkingState(true);
    }
}

void UEchobeatsCognitiveInterface::InitializeStepConfigs()
{
    // Each step names its phase and mode and sets the emissive level that visualizes the cycle.
    // No step names an expression: the face is re-derived from affect when the step begins.

    StepConfigs.Empty();
    StepConfigs.Reserve(12);

    auto AddStep = [this](int32 StepNumber, const TCHAR* PhaseName, ECognitiveMode Mode, float EmissiveMultiplier)
    {
        FCognitiveStepConfig Config;
        Config.StepNumber = StepNumber;
        Config.PhaseName = PhaseName;
        Config.Mode = Mode;
        Config.EmissiveMultiplier = EmissiveMultiplier;
        StepConfigs.Add(Config);
    };

    // ========== Steps {1,5,9}: Pivotal Relevance Realization (Stream 1) ==========
    // These are the "orienting present commitment" steps
    AddStep(1, TEXT("Pivotal-Perception"),    ECognitiveMode::Expressive, 1.2f);
    AddStep(5, TEXT("Pivotal-Action"),        ECognitiveMode::Expressive, 1.1f);
    AddStep(9, TEXT("Pivotal-Simulation"),    ECognitiveMode::Reflective, 0.9f);

    // ========== Steps {2,6,10}: Affordance Interaction (Stream 2) ==========
    // These are the "conditioning past performance" steps
    AddStep(2, TEXT("Affordance-Perception"), ECognitiveMode::Expressive, 1.15f);
    AddStep(6, TEXT("Affordance-Action"),     ECognitiveMode::Expressive, 1.0f);
    AddStep(10, TEXT("Affordance-Simulation"), ECognitiveMode::Reflective, 0.85f);

    // ========== Steps {3,7,11}: Salience Processing (Stream 3) ==========
    // These are the "anticipating future potential" steps
    AddStep(3, TEXT("Salience-Perception"),   ECognitiveMode::Expressive, 1.3f);
    AddStep(7, TEXT("Salience-Action"),       ECognitiveMode::Expressive, 1.4f);
    AddStep(11, TEXT("Salience-Simulation"),  ECognitiveMode::Reflective, 1.1f);

    // ========== Steps {4,8,12}: Integration/Transition ==========
    // These are the integration points between triads
    AddStep(4, TEXT("Integration-1"),         ECognitiveMode::Expressive, 1.0f);
    AddStep(8, TEXT("Integration-2"),         ECognitiveMode::Reflective, 0.95f);
    AddStep(12, TEXT("Integration-3"),        ECognitiveMode::Reflective, 0.9f);

    // Sort by step number
    StepConfigs.Sort([](const FCognitiveStepConfig& A, const FCognitiveStepConfig& B)
    {
        return A.StepNumber < B.StepNumber;
    });
}

void UEchobeatsCognitiveInterface::ProcessStepChange(int32 NewStep)
{
    CurrentStep = NewStep;

    const FCognitiveStepConfig* Config = GetStepConfig(NewStep);
    if (Config)
    {
        CurrentMode = Config->Mode;
        OnCognitiveStepChanged.Broadcast(NewStep, Config->PhaseName);
    }

    UpdateStreams();
    ReevaluateAffectForStep();

    UE_LOG(LogTemp, Verbose, TEXT("Echobeats step %d: %s"), NewStep, Config ? *Config->PhaseName : TEXT("Unknown"));
}

void UEchobeatsCognitiveInterface::UpdateStreams()
{
    // Update each stream's phase based on current step
    // Streams are phased 4 steps apart: Stream 0 at step 1, Stream 1 at step 5, Stream 2 at step 9

    for (int32 i = 0; i < 3; i++)
    {
        // Calculate this stream's effective step (offset by stream index * 4)
        int32 StreamStep = ((CurrentStep - 1 + (12 - i * 4)) % 12) + 1;
        StreamStates[i].CurrentStep = StreamStep;

        // Determine phase based on step within the stream's cycle
        // Steps 1-4: Perception, Steps 5-8: Action, Steps 9-12: Simulation
        if (StreamStep <= 4)
        {
            StreamStates[i].Phase = ECognitiveStreamPhase::Perception;
        }
        else if (StreamStep <= 8)
        {
            StreamStates[i].Phase = ECognitiveStreamPhase::Action;
        }
        else
        {
            StreamStates[i].Phase = ECognitiveStreamPhase::Simulation;
        }
    }
}

void UEchobeatsCognitiveInterface::ReevaluateAffectForStep()
{
    const FCognitiveStepConfig* Config = GetStepConfig(CurrentStep);
    if (!Config || !ExpressionSystem)
        return;

    // The step is the moment to re-read affect; externally reported speaking/thinking
    // states own the face while they last and are left alone here.
    if (!bExternalSpeaking && !bExternalThinking)
    {
        ExpressionSystem->RefreshExpressionFromAffect(StepDuration * 0.5f);  // Transition in first half of step
    }

    // Emissive level visualizes the cycle (tech marking, not a facial reaction)
    ExpressionSystem->SetEmissiveIntensity(Config->EmissiveMultiplier);
}

const FCognitiveStepConfig* UEchobeatsCognitiveInterface::GetStepConfig(int32 StepNumber) const
{
    for (const FCognitiveStepConfig& Config : StepConfigs)
    {
        if (Config.StepNumber == StepNumber)
        {
            return &Config;
        }
    }
    return nullptr;
}

// ========== Cognitive Loop Control ==========

void UEchobeatsCognitiveInterface::StartCognitiveLoop()
{
    bLoopRunning = true;
    bLoopPaused = false;
    LoopTimer = 0.0f;
    CurrentStep = 1;
    CycleCount = 0;

    ProcessStepChange(1);

    UE_LOG(LogTemp, Log, TEXT("Echobeats cognitive loop started"));
}

void UEchobeatsCognitiveInterface::StopCognitiveLoop()
{
    bLoopRunning = false;
    bLoopPaused = false;

    // Return to neutral
    if (StateMachine)
    {
        StateMachine->ReturnToNeutral(0.5f);
    }

    UE_LOG(LogTemp, Log, TEXT("Echobeats cognitive loop stopped"));
}

void UEchobeatsCognitiveInterface::PauseCognitiveLoop()
{
    bLoopPaused = true;
    UE_LOG(LogTemp, Log, TEXT("Echobeats cognitive loop paused at step %d"), CurrentStep);
}

void UEchobeatsCognitiveInterface::ResumeCognitiveLoop()
{
    bLoopPaused = false;
    UE_LOG(LogTemp, Log, TEXT("Echobeats cognitive loop resumed at step %d"), CurrentStep);
}

void UEchobeatsCognitiveInterface::SetLoopSpeed(float StepsPerSecond)
{
    StepDuration = 1.0f / FMath::Max(StepsPerSecond, 0.1f);
    UE_LOG(LogTemp, Log, TEXT("Echobeats loop speed set to %.2f steps/sec (%.2fs per step)"), StepsPerSecond, StepDuration);
}

void UEchobeatsCognitiveInterface::AdvanceToStep(int32 StepNumber)
{
    StepNumber = FMath::Clamp(StepNumber, 1, 12);
    LoopTimer = 0.0f;
    ProcessStepChange(StepNumber);
}

// ========== Relevance Realization ==========

void UEchobeatsCognitiveInterface::SignalRelevanceRealization(const FRelevanceRealizationEvent& Event)
{
    // Determine which stream to attribute this to based on current step
    int32 StreamIndex = (CurrentStep - 1) % 3;

    // Update stream salience
    StreamStates[StreamIndex].Salience = Event.Salience;

    // Fire event
    OnRelevanceRealization.Broadcast(StreamIndex, Event.Salience);

    // Salience is real attention; it shapes gaze deterministically through the affect mapping
    if (ExpressionSystem)
    {
        ExpressionSystem->SetAttentionLevel(Event.Salience);
    }

    if (Event.bIsInsight)
    {
        SignalInsight(Event.Significance);
    }
    else if (Event.Salience > 0.8f)
    {
        // High salience marks itself in the tech glow
        if (ExpressionSystem)
        {
            ExpressionSystem->PulseEmissives(0.3f, 1.5f + Event.Salience * 0.5f);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("Relevance realization: Stream %d, Salience %.2f, Insight: %s"),
        StreamIndex, Event.Salience, Event.bIsInsight ? TEXT("Yes") : TEXT("No"));
}

void UEchobeatsCognitiveInterface::SignalInsight(float Intensity)
{
    Intensity = FMath::Clamp(Intensity, 0.0f, 1.0f);

    // An insight is a real event: it moves the affect source, and the face follows
    if (Neurochemistry)
    {
        Neurochemistry->TriggerRewardResponse(Intensity);
    }

    if (ExpressionSystem)
    {
        ExpressionSystem->RefreshExpressionFromAffect(0.3f);

        // The model decides what, if anything, shows on the face for this moment
        const FString Event = FString::Printf(
            TEXT("An insight just landed (intensity %.2f of 1): a pattern the character had been working on suddenly made sense."),
            Intensity);
        if (!ExpressionSystem->RequestExpressionForEvent(Event))
        {
            UE_LOG(LogTemp, Warning, TEXT("EchobeatsCognitiveInterface: insight expression not requested, no model bridge; face follows affect only"));
        }

        // Tech marking pulse visualizes the event
        ExpressionSystem->PulseEmissives(0.5f, 2.0f + Intensity);
    }

    UE_LOG(LogTemp, Log, TEXT("Insight signaled with intensity %.2f"), Intensity);
}

// ========== Stream Control ==========

void UEchobeatsCognitiveInterface::SetStreamSalience(int32 StreamIndex, float Salience)
{
    if (StreamIndex >= 0 && StreamIndex < 3)
    {
        StreamStates[StreamIndex].Salience = FMath::Clamp(Salience, 0.0f, 1.0f);
    }
}

void UEchobeatsCognitiveInterface::SetStreamActivation(int32 StreamIndex, float Activation)
{
    if (StreamIndex >= 0 && StreamIndex < 3)
    {
        StreamStates[StreamIndex].Activation = FMath::Clamp(Activation, 0.0f, 1.0f);
    }
}

FCognitiveStreamState UEchobeatsCognitiveInterface::GetStreamState(int32 StreamIndex) const
{
    if (StreamIndex >= 0 && StreamIndex < 3)
    {
        return StreamStates[StreamIndex];
    }
    return FCognitiveStreamState();
}

// ========== External Integration ==========

void UEchobeatsCognitiveInterface::ReceiveCognitiveUpdate(ECognitiveState NewState, float Confidence)
{
    if (ExpressionSystem)
    {
        // Records the state and re-derives the face from affect
        ExpressionSystem->SetCognitiveState(NewState);
    }

    // Adjust emissive based on confidence
    if (ExpressionSystem && Confidence > 0.8f)
    {
        ExpressionSystem->SetEmissiveIntensity(1.0f + (Confidence - 0.8f));
    }
}

void UEchobeatsCognitiveInterface::ReceiveSpeechUpdate(bool bIsSpeaking, float Amplitude)
{
    bExternalSpeaking = bIsSpeaking;
    ExternalSpeakingAmplitude = bIsSpeaking ? Amplitude : 0.0f;

    if (ProceduralAnim)
    {
        ProceduralAnim->SetSpeakingState(bIsSpeaking);
    }

    // The amplitude is the caller's real audio level; it is held only briefly by the
    // expression system, so a speaking caller refreshes it every frame.
    if (ExpressionSystem)
    {
        ExpressionSystem->SetSpeakingAmplitude(ExternalSpeakingAmplitude);
    }
}

void UEchobeatsCognitiveInterface::ReceiveListeningUpdate(bool bIsListening)
{
    bExternalListening = bIsListening;

    if (ProceduralAnim)
    {
        ProceduralAnim->SetListeningState(bIsListening);
    }

    if (bIsListening && StateMachine)
    {
        StateMachine->PlaySequence(TEXT("Listening"));
    }
    else if (!bIsListening && StateMachine)
    {
        StateMachine->StopSequence();
    }
}

void UEchobeatsCognitiveInterface::ReceiveThinkingUpdate(bool bIsThinking, float Intensity)
{
    bExternalThinking = bIsThinking;
    ExternalThinkingIntensity = Intensity;

    if (ProceduralAnim)
    {
        ProceduralAnim->SetThinkingState(bIsThinking);
    }

    if (bIsThinking && StateMachine)
    {
        StateMachine->PlayThinkingSequence();
    }
    else if (!bIsThinking && StateMachine)
    {
        StateMachine->StopSequence();
    }
}

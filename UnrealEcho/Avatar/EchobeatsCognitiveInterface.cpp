#include "EchobeatsCognitiveInterface.h"

UEchobeatsCognitiveInterface::UEchobeatsCognitiveInterface()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;

    ExpressionSystem = nullptr;
    StateMachine = nullptr;
    ProceduralAnim = nullptr;

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
    ExpressionSystem = GetOwner()->FindComponentByClass<UDeepTreeEchoExpressionSystem>();
    StateMachine = GetOwner()->FindComponentByClass<UExpressionStateMachine>();
    ProceduralAnim = GetOwner()->FindComponentByClass<UProceduralAnimationComponent>();

    InitializeStepConfigs();

    UE_LOG(LogTemp, Log, TEXT("EchobeatsCognitiveInterface initialized with %d step configs"), StepConfigs.Num());
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
    StepConfigs.Empty();
    StepConfigs.Reserve(12);

    // ========== Steps {1,5,9}: Pivotal Relevance Realization (Stream 1) ==========
    // These are the "orienting present commitment" steps

    // Step 1: Pivotal-Perception (Stream 1 perceives)
    {
        FCognitiveStepConfig Config;
        Config.StepNumber = 1;
        Config.PhaseName = TEXT("Pivotal-Perception");
        Config.Mode = ECognitiveMode::Expressive;
        Config.ExpressionHint = EExpressionState::AlertGaze;
        Config.EmissiveMultiplier = 1.2f;
        Config.bTriggerMicroExpression = true;
        Config.MicroExpressionType = EMicroExpression::UnilateralBrowRaise;
        StepConfigs.Add(Config);
    }

    // Step 5: Pivotal-Action (Stream 1 acts)
    {
        FCognitiveStepConfig Config;
        Config.StepNumber = 5;
        Config.PhaseName = TEXT("Pivotal-Action");
        Config.Mode = ECognitiveMode::Expressive;
        Config.ExpressionHint = EExpressionState::GentleSmile;
        Config.EmissiveMultiplier = 1.1f;
        StepConfigs.Add(Config);
    }

    // Step 9: Pivotal-Simulation (Stream 1 simulates)
    {
        FCognitiveStepConfig Config;
        Config.StepNumber = 9;
        Config.PhaseName = TEXT("Pivotal-Simulation");
        Config.Mode = ECognitiveMode::Reflective;
        Config.ExpressionHint = EExpressionState::Contemplative;
        Config.EmissiveMultiplier = 0.9f;
        StepConfigs.Add(Config);
    }

    // ========== Steps {2,6,10}: Affordance Interaction (Stream 2) ==========
    // These are the "conditioning past performance" steps

    // Step 2: Affordance-Perception (Stream 2 perceives)
    {
        FCognitiveStepConfig Config;
        Config.StepNumber = 2;
        Config.PhaseName = TEXT("Affordance-Perception");
        Config.Mode = ECognitiveMode::Expressive;
        Config.ExpressionHint = EExpressionState::CuriousGaze;
        Config.EmissiveMultiplier = 1.15f;
        StepConfigs.Add(Config);
    }

    // Step 6: Affordance-Action (Stream 2 acts)
    {
        FCognitiveStepConfig Config;
        Config.StepNumber = 6;
        Config.PhaseName = TEXT("Affordance-Action");
        Config.Mode = ECognitiveMode::Expressive;
        Config.ExpressionHint = EExpressionState::SpeakingVowel;
        Config.EmissiveMultiplier = 1.0f;
        StepConfigs.Add(Config);
    }

    // Step 10: Affordance-Simulation (Stream 2 simulates)
    {
        FCognitiveStepConfig Config;
        Config.StepNumber = 10;
        Config.PhaseName = TEXT("Affordance-Simulation");
        Config.Mode = ECognitiveMode::Reflective;
        Config.ExpressionHint = EExpressionState::Contemplative;
        Config.EmissiveMultiplier = 0.85f;
        StepConfigs.Add(Config);
    }

    // ========== Steps {3,7,11}: Salience Processing (Stream 3) ==========
    // These are the "anticipating future potential" steps

    // Step 3: Salience-Perception (Stream 3 perceives)
    {
        FCognitiveStepConfig Config;
        Config.StepNumber = 3;
        Config.PhaseName = TEXT("Salience-Perception");
        Config.Mode = ECognitiveMode::Expressive;
        Config.ExpressionHint = EExpressionState::Awe;
        Config.EmissiveMultiplier = 1.3f;
        Config.bTriggerMicroExpression = true;
        Config.MicroExpressionType = EMicroExpression::EyelidFlutter;
        StepConfigs.Add(Config);
    }

    // Step 7: Salience-Action (Stream 3 acts)
    {
        FCognitiveStepConfig Config;
        Config.StepNumber = 7;
        Config.PhaseName = TEXT("Salience-Action");
        Config.Mode = ECognitiveMode::Expressive;
        Config.ExpressionHint = EExpressionState::BroadSmile;
        Config.EmissiveMultiplier = 1.4f;
        StepConfigs.Add(Config);
    }

    // Step 11: Salience-Simulation (Stream 3 simulates)
    {
        FCognitiveStepConfig Config;
        Config.StepNumber = 11;
        Config.PhaseName = TEXT("Salience-Simulation");
        Config.Mode = ECognitiveMode::Reflective;
        Config.ExpressionHint = EExpressionState::Blissful;
        Config.EmissiveMultiplier = 1.1f;
        StepConfigs.Add(Config);
    }

    // ========== Steps {4,8,12}: Integration/Transition ==========
    // These are the integration points between triads

    // Step 4: Integration-1 (Triad 1 complete)
    {
        FCognitiveStepConfig Config;
        Config.StepNumber = 4;
        Config.PhaseName = TEXT("Integration-1");
        Config.Mode = ECognitiveMode::Expressive;
        Config.ExpressionHint = EExpressionState::GentleSmile;
        Config.EmissiveMultiplier = 1.0f;
        StepConfigs.Add(Config);
    }

    // Step 8: Integration-2 (Triad 2 complete)
    {
        FCognitiveStepConfig Config;
        Config.StepNumber = 8;
        Config.PhaseName = TEXT("Integration-2");
        Config.Mode = ECognitiveMode::Reflective;
        Config.ExpressionHint = EExpressionState::Neutral;
        Config.EmissiveMultiplier = 0.95f;
        Config.bTriggerMicroExpression = true;
        Config.MicroExpressionType = EMicroExpression::Blink;
        StepConfigs.Add(Config);
    }

    // Step 12: Integration-3 (Cycle complete)
    {
        FCognitiveStepConfig Config;
        Config.StepNumber = 12;
        Config.PhaseName = TEXT("Integration-3");
        Config.Mode = ECognitiveMode::Reflective;
        Config.ExpressionHint = EExpressionState::Contemplative;
        Config.EmissiveMultiplier = 0.9f;
        StepConfigs.Add(Config);
    }

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
    ApplyStepExpression();

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

void UEchobeatsCognitiveInterface::ApplyStepExpression()
{
    const FCognitiveStepConfig* Config = GetStepConfig(CurrentStep);
    if (!Config)
        return;

    // Apply expression hint (unless external state overrides)
    if (!bExternalSpeaking && !bExternalThinking)
    {
        if (StateMachine)
        {
            FExpressionTransitionRequest Request;
            Request.TargetState = Config->ExpressionHint;
            Request.Duration = StepDuration * 0.5f;  // Transition in first half of step
            Request.Priority = EExpressionPriority::Normal;
            Request.Curve = ETransitionCurve::EaseInOut;
            StateMachine->RequestTransition(Request);
        }
        else if (ExpressionSystem)
        {
            ExpressionSystem->SetExpressionState(Config->ExpressionHint, StepDuration * 0.5f);
        }
    }

    // Apply emissive multiplier
    if (ExpressionSystem)
    {
        ExpressionSystem->SetEmissiveIntensity(Config->EmissiveMultiplier);
    }

    // Trigger microexpression if configured
    if (Config->bTriggerMicroExpression && ExpressionSystem)
    {
        ExpressionSystem->TriggerMicroExpression(Config->MicroExpressionType, 0.2f);
    }
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

    // If this is an insight, trigger Wonder-to-Joy
    if (Event.bIsInsight)
    {
        SignalInsight(Event.Significance);
    }
    else if (Event.Salience > 0.8f)
    {
        // High salience triggers emissive pulse
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
    // Trigger the Wonder-to-Joy transition
    if (StateMachine)
    {
        StateMachine->PlayWonderToJoy(1.5f * (2.0f - Intensity));  // Higher intensity = faster
    }
    else if (ExpressionSystem)
    {
        ExpressionSystem->TriggerWonderToJoyTransition(1.5f);
    }

    // Pulse emissives
    if (ExpressionSystem)
    {
        ExpressionSystem->PulseEmissives(0.5f, 2.0f + Intensity);
    }

    // Trigger deliberate blink
    if (ProceduralAnim)
    {
        ProceduralAnim->TriggerDeliberateBlink(0.4f);
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
    ExternalSpeakingAmplitude = Amplitude;

    if (ProceduralAnim)
    {
        ProceduralAnim->SetSpeakingState(bIsSpeaking);
    }

    if (ExpressionSystem && bIsSpeaking)
    {
        ExpressionSystem->SetSpeakingAmplitude(Amplitude);
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

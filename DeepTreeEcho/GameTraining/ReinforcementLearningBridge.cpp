// ReinforcementLearningBridge.cpp
// Implementation of RL Bridge connecting game training with Deep Tree Echo

#include "ReinforcementLearningBridge.h"
#include "../Learning/OnlineLearningSystem.h"
#include "../4ECognition/EmbodiedCognitionComponent.h"
#include "../Core/DeepTreeEchoCore.h"

// ============================================================================
// UReinforcementLearningBridge Implementation
// ============================================================================

UReinforcementLearningBridge::UReinforcementLearningBridge()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UReinforcementLearningBridge::BeginPlay()
{
    Super::BeginPlay();

    FindComponentReferences();
    InitializeActionNames();
    InitializeQTable();
}

void UReinforcementLearningBridge::TickComponent(float DeltaTime, ELevelTick TickType,
                                                  FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bIsTraining)
    {
        return;
    }

    // Update cognitive modulation from DTE state
    if (bUseCognitiveModulation)
    {
        UpdateCognitiveModulation();
    }

    // Periodic learning updates
    StepCounter++;
    if (StepCounter % UpdateFrequency == 0 && ReplayBuffer.Num() >= BatchSize)
    {
        PerformBatchUpdate();
    }
}

void UReinforcementLearningBridge::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (!Owner) return;

    ControllerInterface = Owner->FindComponentByClass<UGameControllerInterface>();
    TrainingEnvironment = Owner->FindComponentByClass<UGameTrainingEnvironment>();
    SkillSystem = Owner->FindComponentByClass<UGameSkillTrainingSystem>();
    LearningSystem = Owner->FindComponentByClass<UOnlineLearningSystem>();
    EmbodimentComponent = Owner->FindComponentByClass<UEmbodiedCognitionComponent>();
    // DTECore = Owner->FindComponentByClass<UDeepTreeEchoCore>();
}

void UReinforcementLearningBridge::InitializeActionNames()
{
    if (ActionNames.Num() == 0)
    {
        // Default action names for common game actions
        ActionNames = {
            TEXT("Idle"),
            TEXT("MoveForward"),
            TEXT("MoveBackward"),
            TEXT("MoveLeft"),
            TEXT("MoveRight"),
            TEXT("Jump"),
            TEXT("Crouch"),
            TEXT("Attack"),
            TEXT("Block"),
            TEXT("Dodge"),
            TEXT("Interact"),
            TEXT("Special1"),
            TEXT("Special2"),
            TEXT("LookLeft"),
            TEXT("LookRight"),
            TEXT("LookUp")
        };
        NumActions = ActionNames.Num();
    }
}

void UReinforcementLearningBridge::InitializeQTable()
{
    // Q-table is lazily initialized as states are encountered
    QTable.Empty();
}

FString UReinforcementLearningBridge::StateToKey(const TArray<float>& State) const
{
    // Discretize state for tabular Q-learning
    FString Key;
    for (int32 i = 0; i < FMath::Min(State.Num(), 20); ++i)
    {
        int32 Bucket = FMath::RoundToInt(State[i] * 10);
        Key += FString::Printf(TEXT("%d_"), Bucket);
    }
    return Key;
}

TArray<float> UReinforcementLearningBridge::GetOrCreateQValues(const FString& StateKey)
{
    if (TArray<float>* Existing = QTable.Find(StateKey))
    {
        return *Existing;
    }

    // Initialize Q-values optimistically
    TArray<float> NewQValues;
    NewQValues.SetNumZeroed(NumActions);
    for (int32 i = 0; i < NumActions; ++i)
    {
        NewQValues[i] = 0.1f;  // Small optimistic initialization
    }
    QTable.Add(StateKey, NewQValues);
    return NewQValues;
}

// ============================================================================
// Action Selection
// ============================================================================

FRLAction UReinforcementLearningBridge::SelectAction(const TArray<float>& State)
{
    FRLAction Action;

    switch (SelectionPolicy)
    {
        case EActionSelectionPolicy::EpsilonGreedy:
            Action = SelectEpsilonGreedy(State);
            break;
        case EActionSelectionPolicy::Softmax:
            Action = SelectSoftmax(State);
            break;
        case EActionSelectionPolicy::UCB:
            Action = SelectUCB(State);
            break;
        case EActionSelectionPolicy::Thompson:
            Action = SelectThompson(State);
            break;
        case EActionSelectionPolicy::ActiveInference:
            Action = SelectActionActiveInference(State);
            break;
        default:
            Action = SelectEpsilonGreedy(State);
            break;
    }

    // Store for learning
    LastState = State;
    LastAction = Action;

    OnActionSelected.Broadcast(Action, UEnum::GetValueAsString(SelectionPolicy));

    return Action;
}

FRLAction UReinforcementLearningBridge::SelectActionFromObservation(const FGameStateObservation& Observation)
{
    TArray<float> State = Observation.ToNormalizedVector();

    // Enhance with reservoir state if enabled
    if (bUseReservoirIntegration)
    {
        TArray<float> EnhancedState = GetReservoirState(State);
        return SelectAction(EnhancedState);
    }

    return SelectAction(State);
}

TArray<float> UReinforcementLearningBridge::GetQValues(const TArray<float>& State) const
{
    FString StateKey = StateToKey(State);
    if (const TArray<float>* Values = QTable.Find(StateKey))
    {
        return *Values;
    }

    // Return default values
    TArray<float> DefaultValues;
    DefaultValues.SetNumZeroed(NumActions);
    return DefaultValues;
}

TArray<float> UReinforcementLearningBridge::GetActionProbabilities(const TArray<float>& State) const
{
    TArray<float> QValues = GetQValues(State);
    TArray<float> Probs;
    Probs.SetNum(NumActions);

    float MaxQ = QValues[0];
    for (float Q : QValues)
    {
        MaxQ = FMath::Max(MaxQ, Q);
    }

    float Sum = 0.0f;
    for (int32 i = 0; i < NumActions; ++i)
    {
        Probs[i] = FMath::Exp((QValues[i] - MaxQ) / SoftmaxTemperature);
        Sum += Probs[i];
    }

    if (Sum > 0.0f)
    {
        for (int32 i = 0; i < NumActions; ++i)
        {
            Probs[i] /= Sum;
        }
    }

    return Probs;
}

FControllerOutputCommand UReinforcementLearningBridge::ActionToControllerOutput(const FRLAction& Action) const
{
    FControllerOutputCommand Output;
    Output.ActionName = Action.ActionName;

    // Map action index to controller state
    switch (Action.ActionIndex)
    {
        case 0:  // Idle
            break;
        case 1:  // MoveForward
            Output.DesiredState.LeftStickY = 1.0f;
            Output.Category = EGameActionCategory::Movement;
            break;
        case 2:  // MoveBackward
            Output.DesiredState.LeftStickY = -1.0f;
            Output.Category = EGameActionCategory::Movement;
            break;
        case 3:  // MoveLeft
            Output.DesiredState.LeftStickX = -1.0f;
            Output.Category = EGameActionCategory::Movement;
            break;
        case 4:  // MoveRight
            Output.DesiredState.LeftStickX = 1.0f;
            Output.Category = EGameActionCategory::Movement;
            break;
        case 5:  // Jump
            Output.DesiredState.PressedButtons.Add(EGamepadButton::FaceBottom);
            Output.Category = EGameActionCategory::Movement;
            break;
        case 6:  // Crouch
            Output.DesiredState.PressedButtons.Add(EGamepadButton::LeftThumb);
            Output.Category = EGameActionCategory::Movement;
            break;
        case 7:  // Attack
            Output.DesiredState.PressedButtons.Add(EGamepadButton::FaceRight);
            Output.Category = EGameActionCategory::Attack;
            break;
        case 8:  // Block
            Output.DesiredState.LeftTrigger = 1.0f;
            Output.Category = EGameActionCategory::Defense;
            break;
        case 9:  // Dodge
            Output.DesiredState.PressedButtons.Add(EGamepadButton::FaceLeft);
            Output.Category = EGameActionCategory::Defense;
            break;
        case 10: // Interact
            Output.DesiredState.PressedButtons.Add(EGamepadButton::FaceTop);
            Output.Category = EGameActionCategory::Interaction;
            break;
        case 11: // Special1
            Output.DesiredState.PressedButtons.Add(EGamepadButton::RightShoulder);
            Output.Category = EGameActionCategory::Special;
            break;
        case 12: // Special2
            Output.DesiredState.PressedButtons.Add(EGamepadButton::LeftShoulder);
            Output.Category = EGameActionCategory::Special;
            break;
        case 13: // LookLeft
            Output.DesiredState.RightStickX = -1.0f;
            Output.Category = EGameActionCategory::Camera;
            break;
        case 14: // LookRight
            Output.DesiredState.RightStickX = 1.0f;
            Output.Category = EGameActionCategory::Camera;
            break;
        case 15: // LookUp
            Output.DesiredState.RightStickY = 1.0f;
            Output.Category = EGameActionCategory::Camera;
            break;
    }

    Output.Duration = 0.1f;
    Output.Priority = 1.0f;
    Output.Confidence = Action.Probability;

    return Output;
}

FRLAction UReinforcementLearningBridge::GetGreedyAction(const TArray<float>& State) const
{
    TArray<float> QValues = GetQValues(State);

    FRLAction Action;
    Action.QValue = QValues[0];
    Action.ActionIndex = 0;

    for (int32 i = 1; i < NumActions; ++i)
    {
        if (QValues[i] > Action.QValue)
        {
            Action.QValue = QValues[i];
            Action.ActionIndex = i;
        }
    }

    if (ActionNames.IsValidIndex(Action.ActionIndex))
    {
        Action.ActionName = ActionNames[Action.ActionIndex];
    }
    Action.Probability = 1.0f;

    return Action;
}

FRLAction UReinforcementLearningBridge::SelectEpsilonGreedy(const TArray<float>& State)
{
    float EffectiveEpsilon = GetEffectiveExplorationRate();

    if (FMath::FRand() < EffectiveEpsilon)
    {
        // Explore: random action
        FRLAction Action;
        Action.ActionIndex = FMath::RandRange(0, NumActions - 1);
        if (ActionNames.IsValidIndex(Action.ActionIndex))
        {
            Action.ActionName = ActionNames[Action.ActionIndex];
        }
        Action.Probability = 1.0f / NumActions;
        return Action;
    }
    else
    {
        // Exploit: greedy action
        return GetGreedyAction(State);
    }
}

FRLAction UReinforcementLearningBridge::SelectSoftmax(const TArray<float>& State)
{
    TArray<float> Probs = GetActionProbabilities(State);

    // Sample from distribution
    float Roll = FMath::FRand();
    float Cumulative = 0.0f;

    FRLAction Action;
    for (int32 i = 0; i < NumActions; ++i)
    {
        Cumulative += Probs[i];
        if (Roll <= Cumulative)
        {
            Action.ActionIndex = i;
            Action.Probability = Probs[i];
            break;
        }
    }

    if (ActionNames.IsValidIndex(Action.ActionIndex))
    {
        Action.ActionName = ActionNames[Action.ActionIndex];
    }

    TArray<float> QValues = GetQValues(State);
    Action.QValue = QValues.IsValidIndex(Action.ActionIndex) ? QValues[Action.ActionIndex] : 0.0f;

    return Action;
}

FRLAction UReinforcementLearningBridge::SelectUCB(const TArray<float>& State)
{
    TArray<float> QValues = GetQValues(State);

    // UCB1 formula: Q(a) + c * sqrt(ln(t) / N(a))
    float c = 2.0f;  // Exploration constant
    float LogT = FMath::Loge(static_cast<float>(TotalSteps + 1));

    FRLAction BestAction;
    float BestUCB = -FLT_MAX;

    for (int32 i = 0; i < NumActions; ++i)
    {
        // Simplified: assume equal visitation for now
        float VisitCount = FMath::Max(1.0f, static_cast<float>(TotalSteps / NumActions));
        float UCBValue = QValues[i] + c * FMath::Sqrt(LogT / VisitCount);

        if (UCBValue > BestUCB)
        {
            BestUCB = UCBValue;
            BestAction.ActionIndex = i;
            BestAction.QValue = QValues[i];
        }
    }

    if (ActionNames.IsValidIndex(BestAction.ActionIndex))
    {
        BestAction.ActionName = ActionNames[BestAction.ActionIndex];
    }
    BestAction.Probability = 1.0f;

    return BestAction;
}

FRLAction UReinforcementLearningBridge::SelectThompson(const TArray<float>& State)
{
    TArray<float> QValues = GetQValues(State);

    // Thompson sampling: sample from posterior
    // Simplified: use Q-value + Gaussian noise scaled by uncertainty
    FRLAction BestAction;
    float BestSample = -FLT_MAX;

    for (int32 i = 0; i < NumActions; ++i)
    {
        float Uncertainty = 1.0f / FMath::Sqrt(static_cast<float>(TotalSteps / NumActions + 1));
        float Sample = QValues[i] + FMath::FRandRange(-1.0f, 1.0f) * Uncertainty;

        if (Sample > BestSample)
        {
            BestSample = Sample;
            BestAction.ActionIndex = i;
            BestAction.QValue = QValues[i];
        }
    }

    if (ActionNames.IsValidIndex(BestAction.ActionIndex))
    {
        BestAction.ActionName = ActionNames[BestAction.ActionIndex];
    }

    return BestAction;
}

// ============================================================================
// Learning
// ============================================================================

void UReinforcementLearningBridge::RecordTransition(const TArray<float>& State, const FRLAction& Action,
                                                     float Reward, const TArray<float>& NextState, bool bTerminal)
{
    FTransition Transition;
    Transition.State = State;
    Transition.Action = Action;
    Transition.Reward = Reward;
    Transition.NextState = NextState;
    Transition.bTerminal = bTerminal;
    Transition.Timestamp = GetWorld()->GetTimeSeconds();

    AddToReplayBuffer(Transition);

    // Apply immediate learning based on algorithm
    if (Algorithm == ELearningAlgorithm::QLearning)
    {
        ApplyQLearningUpdate(Transition);
    }
    else if (Algorithm == ELearningAlgorithm::SARSA)
    {
        ApplySARSAUpdate(Transition);
    }

    // Update statistics
    TotalSteps++;
    TotalReward += Reward;
    RewardCount++;
    if (bTerminal)
    {
        TotalEpisodes++;
    }

    // Sync with cognitive learning system
    SyncWithCognitiveSystem();

    OnTransitionRecorded.Broadcast(Transition);
}

void UReinforcementLearningBridge::RecordGameTransition(const FGameStateObservation& State,
                                                         const FControllerInputState& Input,
                                                         float Reward,
                                                         const FGameStateObservation& NextState,
                                                         bool bTerminal)
{
    TArray<float> StateVec = State.ToNormalizedVector();
    TArray<float> NextStateVec = NextState.ToNormalizedVector();

    FRLAction Action;
    Action.ContinuousAction = Input.ToActionVector();
    // Map input to discrete action (simplified)
    Action.ActionIndex = 0;  // Default to idle
    if (FMath::Abs(Input.LeftStickY) > 0.5f)
    {
        Action.ActionIndex = Input.LeftStickY > 0 ? 1 : 2;  // Forward/Backward
    }
    else if (FMath::Abs(Input.LeftStickX) > 0.5f)
    {
        Action.ActionIndex = Input.LeftStickX > 0 ? 4 : 3;  // Right/Left
    }
    else if (Input.PressedButtons.Contains(EGamepadButton::FaceBottom))
    {
        Action.ActionIndex = 5;  // Jump
    }
    else if (Input.PressedButtons.Contains(EGamepadButton::FaceRight))
    {
        Action.ActionIndex = 7;  // Attack
    }

    if (ActionNames.IsValidIndex(Action.ActionIndex))
    {
        Action.ActionName = ActionNames[Action.ActionIndex];
    }

    RecordTransition(StateVec, Action, Reward, NextStateVec, bTerminal);
}

float UReinforcementLearningBridge::PerformLearningUpdate()
{
    if (ReplayBuffer.Num() == 0)
    {
        return 0.0f;
    }

    return PerformBatchUpdate();
}

float UReinforcementLearningBridge::PerformBatchUpdate()
{
    if (ReplayBuffer.Num() < BatchSize)
    {
        return 0.0f;
    }

    TArray<FTransition> Batch = SampleFromReplayBuffer(BatchSize);

    float TotalLoss = 0.0f;

    for (const FTransition& Transition : Batch)
    {
        if (Algorithm == ELearningAlgorithm::QLearning || Algorithm == ELearningAlgorithm::DQN)
        {
            ApplyQLearningUpdate(Transition);

            // Compute loss (TD error squared)
            float CurrentQ = GetQValues(Transition.State)[Transition.Action.ActionIndex];
            float MaxNextQ = Transition.bTerminal ? 0.0f : GetGreedyAction(Transition.NextState).QValue;
            float Target = Transition.Reward + DiscountFactor * MaxNextQ;
            float TDError = Target - CurrentQ;
            TotalLoss += TDError * TDError;
        }
        else if (Algorithm == ELearningAlgorithm::SARSA)
        {
            ApplySARSAUpdate(Transition);
        }
    }

    float AvgLoss = TotalLoss / Batch.Num();
    float AvgReward = GetAverageReward();

    OnLearningUpdate.Broadcast(AvgLoss, AvgReward);

    return AvgLoss;
}

void UReinforcementLearningBridge::ApplyQLearningUpdate(const FTransition& Transition)
{
    FString StateKey = StateToKey(Transition.State);
    TArray<float> QValues = GetOrCreateQValues(StateKey);

    float CurrentQ = QValues[Transition.Action.ActionIndex];
    float MaxNextQ = Transition.bTerminal ? 0.0f : GetGreedyAction(Transition.NextState).QValue;

    // Add intrinsic reward (curiosity bonus)
    float IntrinsicReward = ComputeIntrinsicReward(Transition.State, Transition.Action.ActionIndex);
    float TotalReward = Transition.Reward + CurrentModulation.Curiosity * IntrinsicReward;

    // TD target
    float Target = TotalReward + DiscountFactor * MaxNextQ;

    // Q-learning update
    float EffectiveLR = GetEffectiveLearningRate();
    float NewQ = CurrentQ + EffectiveLR * (Target - CurrentQ);

    QValues[Transition.Action.ActionIndex] = NewQ;
    QTable.Add(StateKey, QValues);
}

void UReinforcementLearningBridge::ApplySARSAUpdate(const FTransition& Transition)
{
    FString StateKey = StateToKey(Transition.State);
    TArray<float> QValues = GetOrCreateQValues(StateKey);

    float CurrentQ = QValues[Transition.Action.ActionIndex];

    // SARSA uses the action actually taken in next state (on-policy)
    // For simplicity, use epsilon-greedy policy
    FRLAction NextAction = Transition.bTerminal ?
        FRLAction() : SelectEpsilonGreedy(Transition.NextState);
    float NextQ = Transition.bTerminal ? 0.0f : GetQValues(Transition.NextState)[NextAction.ActionIndex];

    // SARSA update
    float Target = Transition.Reward + DiscountFactor * NextQ;
    float EffectiveLR = GetEffectiveLearningRate();
    float NewQ = CurrentQ + EffectiveLR * (Target - CurrentQ);

    QValues[Transition.Action.ActionIndex] = NewQ;
    QTable.Add(StateKey, QValues);
}

void UReinforcementLearningBridge::UpdateQValue(const TArray<float>& State, int32 ActionIndex, float Target)
{
    FString StateKey = StateToKey(State);
    TArray<float> QValues = GetOrCreateQValues(StateKey);

    if (QValues.IsValidIndex(ActionIndex))
    {
        float EffectiveLR = GetEffectiveLearningRate();
        QValues[ActionIndex] += EffectiveLR * (Target - QValues[ActionIndex]);
        QTable.Add(StateKey, QValues);
    }
}

void UReinforcementLearningBridge::ClearReplayBuffer()
{
    ReplayBuffer.Empty();
}

void UReinforcementLearningBridge::AddToReplayBuffer(const FTransition& Transition)
{
    ReplayBuffer.Add(Transition);

    while (ReplayBuffer.Num() > ReplayBufferSize)
    {
        ReplayBuffer.RemoveAt(0);
    }
}

TArray<FTransition> UReinforcementLearningBridge::SampleFromReplayBuffer(int32 Count)
{
    TArray<FTransition> Sample;
    Sample.Reserve(Count);

    for (int32 i = 0; i < Count && ReplayBuffer.Num() > 0; ++i)
    {
        int32 Idx = FMath::RandRange(0, ReplayBuffer.Num() - 1);
        Sample.Add(ReplayBuffer[Idx]);
    }

    return Sample;
}

float UReinforcementLearningBridge::ComputeIntrinsicReward(const TArray<float>& State, int32 ActionIndex) const
{
    // Curiosity-driven intrinsic reward
    // Higher reward for less-visited states
    FString StateKey = StateToKey(State);

    if (!QTable.Contains(StateKey))
    {
        return 1.0f;  // Novel state bonus
    }

    return ComputeCuriosityBonus(State);
}

float UReinforcementLearningBridge::ComputeCuriosityBonus(const TArray<float>& State) const
{
    // Simplified curiosity: inverse of state visitation
    // In full implementation, would use prediction error or count-based exploration
    return 0.1f / FMath::Sqrt(static_cast<float>(TotalSteps + 1));
}

// ============================================================================
// Cognitive Integration
// ============================================================================

FCognitiveModulation UReinforcementLearningBridge::GetCognitiveModulation() const
{
    return CurrentModulation;
}

void UReinforcementLearningBridge::SetCognitiveModulation(const FCognitiveModulation& Modulation)
{
    CurrentModulation = Modulation;
    OnCognitiveModulationChanged.Broadcast(CurrentModulation);
}

void UReinforcementLearningBridge::UpdateCognitiveModulation()
{
    // Update from embodiment component
    if (EmbodimentComponent)
    {
        CurrentModulation.Attention = EmbodimentComponent->Get4EIntegrationScore();
    }

    // Compute arousal from recent performance
    float RecentReward = GetAverageReward();
    CurrentModulation.Arousal = FMath::Clamp(0.5f + RecentReward * 0.1f, 0.0f, 1.0f);

    // Compute confidence from success rate
    if (TotalSteps > 100)
    {
        // Simplified: confidence based on reward trend
        CurrentModulation.Confidence = FMath::Clamp(RecentReward / 10.0f + 0.5f, 0.0f, 1.0f);
    }

    // Compute frustration from consecutive failures
    // (Simplified - would track actual failure streaks)
    CurrentModulation.Frustration = FMath::Max(0.0f, -RecentReward * 0.05f);

    // Compute flow state
    float SuccessRate = RecentReward > 0 ? 0.5f : 0.3f;
    CurrentModulation.FlowState = 1.0f - FMath::Abs(SuccessRate - 0.6f) * 2.0f;
}

float UReinforcementLearningBridge::GetEffectiveLearningRate() const
{
    float Rate = LearningRate;

    if (bUseCognitiveModulation)
    {
        // Higher attention = better learning
        Rate *= (0.5f + CurrentModulation.Attention * 0.5f);

        // Moderate arousal is optimal (Yerkes-Dodson)
        float ArousalMod = 1.0f - FMath::Abs(CurrentModulation.Arousal - 0.6f) * 0.5f;
        Rate *= ArousalMod;

        // Reduce learning when frustrated (prevent bad habits)
        Rate *= (1.0f - CurrentModulation.Frustration * 0.5f);
    }

    return Rate;
}

float UReinforcementLearningBridge::GetEffectiveExplorationRate() const
{
    float Rate = ExplorationRate;

    if (bUseCognitiveModulation)
    {
        // Higher curiosity = more exploration
        Rate *= (0.5f + CurrentModulation.Curiosity);

        // Low confidence = more exploration
        Rate *= (1.5f - CurrentModulation.Confidence * 0.5f);

        // High frustration = more exploration (try new things)
        Rate *= (1.0f + CurrentModulation.Frustration);
    }

    return FMath::Clamp(Rate, MinExplorationRate, 1.0f);
}

void UReinforcementLearningBridge::SyncWithCognitiveSystem()
{
    if (!LearningSystem)
    {
        return;
    }

    // Sync exploration rate
    LearningSystem->SetExplorationRate(GetEffectiveExplorationRate());

    // Record experience in cognitive system
    if (ReplayBuffer.Num() > 0)
    {
        const FTransition& Latest = ReplayBuffer.Last();

        TArray<FString> Tags;
        Tags.Add(TEXT("GameTraining"));
        Tags.Add(Latest.Action.ActionName);

        LearningSystem->RecordExperience(
            StateToKey(Latest.State),
            Latest.Action.ActionName,
            StateToKey(Latest.NextState),
            Latest.Reward,
            Tags,
            Latest.bTerminal
        );

        // Update Q-value in cognitive system
        LearningSystem->UpdateQValue(
            StateToKey(Latest.State),
            Latest.Action.ActionName,
            Latest.Reward,
            StateToKey(Latest.NextState)
        );
    }
}

// ============================================================================
// Reservoir Integration
// ============================================================================

TArray<float> UReinforcementLearningBridge::GetReservoirState(const TArray<float>& Input)
{
    if (!bUseReservoirIntegration)
    {
        return Input;
    }

    // Update reservoir with input
    UpdateReservoir(Input);

    // Concatenate input with echo memory for richer state representation
    TArray<float> EnhancedState = Input;
    EnhancedState.Append(ReservoirState.EchoMemory);

    return EnhancedState;
}

void UReinforcementLearningBridge::UpdateReservoir(const TArray<float>& State)
{
    // Add to temporal buffer
    ReservoirState.TemporalBuffer.Add(State);
    while (ReservoirState.TemporalBuffer.Num() > 30)
    {
        ReservoirState.TemporalBuffer.RemoveAt(0);
    }

    // Simplified echo state: exponential moving average
    if (ReservoirState.EchoMemory.Num() != State.Num())
    {
        ReservoirState.EchoMemory.SetNumZeroed(State.Num());
    }

    float LeakRate = 0.3f;
    for (int32 i = 0; i < State.Num(); ++i)
    {
        ReservoirState.EchoMemory[i] = (1.0f - LeakRate) * ReservoirState.EchoMemory[i] +
                                        LeakRate * State[i];
    }
}

TArray<FString> UReinforcementLearningBridge::GetDetectedPatterns() const
{
    return ReservoirState.DetectedPatterns;
}

TArray<float> UReinforcementLearningBridge::GetEchoMemory() const
{
    return ReservoirState.EchoMemory;
}

// ============================================================================
// Active Inference Integration
// ============================================================================

FRLAction UReinforcementLearningBridge::SelectActionActiveInference(const TArray<float>& State)
{
    // Active inference: minimize expected free energy
    FRLAction BestAction;
    float LowestEFE = FLT_MAX;

    for (int32 i = 0; i < NumActions; ++i)
    {
        float EFE = ComputeExpectedFreeEnergy(State, i);
        if (EFE < LowestEFE)
        {
            LowestEFE = EFE;
            BestAction.ActionIndex = i;
            BestAction.ExpectedFreeEnergy = EFE;
        }
    }

    if (ActionNames.IsValidIndex(BestAction.ActionIndex))
    {
        BestAction.ActionName = ActionNames[BestAction.ActionIndex];
    }

    // Convert EFE to probability (softmax over negative EFE)
    TArray<float> EFEs;
    float MinEFE = FLT_MAX;
    for (int32 i = 0; i < NumActions; ++i)
    {
        float EFE = ComputeExpectedFreeEnergy(State, i);
        EFEs.Add(EFE);
        MinEFE = FMath::Min(MinEFE, EFE);
    }

    float Sum = 0.0f;
    for (int32 i = 0; i < NumActions; ++i)
    {
        Sum += FMath::Exp(-(EFEs[i] - MinEFE) / SoftmaxTemperature);
    }

    BestAction.Probability = FMath::Exp(-(LowestEFE - MinEFE) / SoftmaxTemperature) / Sum;

    return BestAction;
}

float UReinforcementLearningBridge::ComputeExpectedFreeEnergy(const TArray<float>& State, int32 ActionIndex) const
{
    // EFE = Risk + Ambiguity - Epistemic Value - Pragmatic Value

    TArray<float> QValues = GetQValues(State);
    float Q = QValues.IsValidIndex(ActionIndex) ? QValues[ActionIndex] : 0.0f;

    // Risk: divergence from preferred outcomes (use negative Q as proxy)
    float Risk = -Q;

    // Ambiguity: uncertainty about outcomes (higher for less-visited states)
    float Ambiguity = 1.0f / FMath::Sqrt(static_cast<float>(TotalSteps + 1));

    // Epistemic value: information gain (higher for novel actions)
    float EpistemicValue = ComputeCuriosityBonus(State);

    // Pragmatic value: expected reward
    float PragmaticValue = Q;

    // Modulate by cognitive state
    float CuriosityMod = bUseCognitiveModulation ? CurrentModulation.Curiosity : 0.5f;

    return Risk + Ambiguity - CuriosityMod * EpistemicValue - PragmaticValue;
}

void UReinforcementLearningBridge::UpdateBeliefs(const TArray<float>& Observation)
{
    // Update internal model based on observation
    // This integrates with the reservoir for temporal belief updates
    UpdateReservoir(Observation);
}

// ============================================================================
// Training Control
// ============================================================================

void UReinforcementLearningBridge::StartTraining()
{
    bIsTraining = true;
    StepCounter = 0;
}

void UReinforcementLearningBridge::StopTraining()
{
    bIsTraining = false;
}

bool UReinforcementLearningBridge::IsTraining() const
{
    return bIsTraining;
}

void UReinforcementLearningBridge::DecayExploration()
{
    ExplorationRate = FMath::Max(ExplorationRate * ExplorationDecay, MinExplorationRate);

    if (LearningSystem)
    {
        LearningSystem->SetExplorationRate(GetEffectiveExplorationRate());
    }
}

TArray<float> UReinforcementLearningBridge::SavePolicy() const
{
    TArray<float> PolicyData;

    // Serialize Q-table (simplified)
    for (const auto& Pair : QTable)
    {
        PolicyData.Append(Pair.Value);
    }

    return PolicyData;
}

void UReinforcementLearningBridge::LoadPolicy(const TArray<float>& PolicyData)
{
    // In practice, would deserialize properly
    // This is a placeholder for the interface
}

void UReinforcementLearningBridge::ResetLearning()
{
    QTable.Empty();
    ClearReplayBuffer();
    TotalSteps = 0;
    TotalEpisodes = 0;
    TotalReward = 0.0f;
    RewardCount = 0;
    ExplorationRate = 0.3f;
    CurrentModulation = FCognitiveModulation();
}

// ============================================================================
// Statistics
// ============================================================================

float UReinforcementLearningBridge::GetAverageReward() const
{
    return RewardCount > 0 ? TotalReward / RewardCount : 0.0f;
}

int32 UReinforcementLearningBridge::GetTotalSteps() const
{
    return TotalSteps;
}

int32 UReinforcementLearningBridge::GetTotalEpisodes() const
{
    return TotalEpisodes;
}

int32 UReinforcementLearningBridge::GetReplayBufferCount() const
{
    return ReplayBuffer.Num();
}

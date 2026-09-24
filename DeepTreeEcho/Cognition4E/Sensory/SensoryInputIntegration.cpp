// SensoryInputIntegration.cpp
// Implementation of external sensory input integration system

#include "SensoryInputIntegration.h"
#include "Math/UnrealMathUtility.h"
#include "GameFramework/Actor.h"

USensoryInputIntegration::USensoryInputIntegration()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void USensoryInputIntegration::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();

    // Initialize prediction state
    PredictionState.SetNum(FeatureVectorSize);
    for (float& Val : PredictionState)
    {
        Val = 0.0f;
    }

    // Initialize attention state
    State.AttentionState.AttentionBreadth = 0.5f;
    State.AttentionState.AttentionDepth = 0.5f;
    State.AttentionState.VigilanceLevel = 0.5f;
}

void USensoryInputIntegration::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Process inputs to percepts
    ProcessInputsToPercepts();

    // Attempt multimodal binding
    AttemptMultimodalBinding();

    // Update attention
    UpdateAttention(DeltaTime);

    // Predictive processing
    if (bEnablePredictiveProcessing)
    {
        float Error = ComputePredictionError();
        if (Error > 0.5f)
        {
            OnPredictionErrorSpike.Broadcast(Error);
        }
        UpdatePredictionModel();
        GeneratePredictions();
    }

    // Decay old percepts
    DecayPercepts(DeltaTime);

    // Compute sensory load
    ComputeSensoryLoad();
}

void USensoryInputIntegration::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        EchobeatsEngine = Owner->FindComponentByClass<UEchobeatsStreamEngine>();
        CouplingDynamics = Owner->FindComponentByClass<UTensionalCouplingDynamics>();
    }
}

int32 USensoryInputIntegration::RegisterSensoryInput(const FSensoryInputData& InputData)
{
    if (State.ActiveInputs.Num() >= MaxActiveInputs)
    {
        // Remove oldest low-priority input
        int32 LowestPriorityIndex = -1;
        EAttentionPriority LowestPriority = EAttentionPriority::Critical;

        for (int32 i = 0; i < State.ActiveInputs.Num(); ++i)
        {
            if (State.ActiveInputs[i].Priority < LowestPriority)
            {
                LowestPriority = State.ActiveInputs[i].Priority;
                LowestPriorityIndex = i;
            }
        }

        if (LowestPriorityIndex >= 0)
        {
            State.ActiveInputs.RemoveAt(LowestPriorityIndex);
        }
    }

    FSensoryInputData NewInput = InputData;
    NewInput.InputID = NextInputID++;
    NewInput.Timestamp = GetWorld()->GetTimeSeconds();

    State.ActiveInputs.Add(NewInput);

    OnInputReceived.Broadcast(NewInput.InputID, NewInput.Modality);

    return NewInput.InputID;
}

void USensoryInputIntegration::UpdateSensoryInput(int32 InputID, const FSensoryInputData& InputData)
{
    for (FSensoryInputData& Input : State.ActiveInputs)
    {
        if (Input.InputID == InputID)
        {
            Input = InputData;
            Input.InputID = InputID; // Preserve ID
            Input.Timestamp = GetWorld()->GetTimeSeconds();
            break;
        }
    }
}

void USensoryInputIntegration::RemoveSensoryInput(int32 InputID)
{
    State.ActiveInputs.RemoveAll([InputID](const FSensoryInputData& Input) {
        return Input.InputID == InputID;
    });
}

FSensoryInputData USensoryInputIntegration::GetSensoryInput(int32 InputID) const
{
    for (const FSensoryInputData& Input : State.ActiveInputs)
    {
        if (Input.InputID == InputID)
        {
            return Input;
        }
    }
    return FSensoryInputData();
}

TArray<FSensoryInputData> USensoryInputIntegration::GetInputsByModality(ESensoryModality Modality) const
{
    TArray<FSensoryInputData> Result;
    for (const FSensoryInputData& Input : State.ActiveInputs)
    {
        if (Input.Modality == Modality)
        {
            Result.Add(Input);
        }
    }
    return Result;
}

void USensoryInputIntegration::ProcessInputsToPercepts()
{
    // Group inputs by modality and process
    TMap<ESensoryModality, TArray<FSensoryInputData>> ModalityGroups;

    for (const FSensoryInputData& Input : State.ActiveInputs)
    {
        ModalityGroups.FindOrAdd(Input.Modality).Add(Input);
    }

    // Process each modality group
    for (auto& Pair : ModalityGroups)
    {
        // Integrate features within modality
        TArray<float> IntegratedFeatures = IntegrateFeatures(Pair.Value);

        // Find or create percept for this modality
        bool bFoundExisting = false;
        for (FSensoryPercept& Percept : State.Percepts)
        {
            if (Percept.Modality == Pair.Key)
            {
                // Update existing percept
                Percept.IntegratedFeatures = IntegratedFeatures;
                Percept.Salience = ComputeSalience(Pair.Value[0]);
                Percept.GoalRelevance = ComputeGoalRelevance(Pair.Value[0]);
                Percept.AffordancePotential = ComputeAffordancePotential(Pair.Value[0]);
                bFoundExisting = true;
                break;
            }
        }

        if (!bFoundExisting && State.Percepts.Num() < MaxPercepts)
        {
            // Create new percept
            FSensoryPercept NewPercept;
            NewPercept.SourceInputID = Pair.Value[0].InputID;
            NewPercept.Modality = Pair.Key;
            NewPercept.IntegratedFeatures = IntegratedFeatures;
            NewPercept.Salience = ComputeSalience(Pair.Value[0]);
            NewPercept.GoalRelevance = ComputeGoalRelevance(Pair.Value[0]);
            NewPercept.AffordancePotential = ComputeAffordancePotential(Pair.Value[0]);
            NewPercept.Confidence = 1.0f;
            NewPercept.BoundEchobeatStep = State.CurrentEchobeatStep;

            State.Percepts.Add(NewPercept);

            OnPerceptFormed.Broadcast(NextPerceptID++, NewPercept.Salience);

            // Check salience threshold
            if (NewPercept.Salience > SalienceThreshold)
            {
                OnSalienceThresholdCrossed.Broadcast(NewPercept.SourceInputID, NewPercept.Salience);
            }
        }
    }
}

FSensoryPercept USensoryInputIntegration::ProcessInputToPercept(const FSensoryInputData& Input)
{
    FSensoryPercept Percept;
    Percept.SourceInputID = Input.InputID;
    Percept.Modality = Input.Modality;
    Percept.IntegratedFeatures = Input.FeatureVector;
    Percept.IntegratedFeatures.SetNum(FeatureVectorSize);
    Percept.Salience = ComputeSalience(Input);
    Percept.GoalRelevance = ComputeGoalRelevance(Input);
    Percept.AffordancePotential = ComputeAffordancePotential(Input);
    Percept.PredictedValence = Input.EmotionalValence;
    Percept.Confidence = 1.0f;

    return Percept;
}

float USensoryInputIntegration::ComputeSalience(const FSensoryInputData& Input)
{
    float Salience = 0.0f;

    // Base salience from intensity
    Salience += Input.Intensity * 0.3f;

    // Novelty contribution
    Salience += Input.NoveltyScore * 0.3f;

    // Priority contribution
    float PriorityWeight = static_cast<float>(Input.Priority) / 4.0f;
    Salience += PriorityWeight * 0.2f;

    // Emotional valence contribution (absolute value)
    Salience += FMath::Abs(Input.EmotionalValence) * 0.2f;

    // Event type modulation
    if (Input.EventType == ESensoryEventType::Onset)
    {
        Salience *= 1.2f; // Onsets are more salient
    }
    else if (Input.EventType == ESensoryEventType::Change)
    {
        Salience *= 1.1f;
    }

    return FMath::Clamp(Salience, 0.0f, 1.0f);
}

float USensoryInputIntegration::ComputeGoalRelevance(const FSensoryInputData& Input)
{
    // Placeholder - would integrate with goal system
    float Relevance = 0.5f;

    // Modality-based relevance
    switch (Input.Modality)
    {
    case ESensoryModality::Social:
        Relevance += 0.2f; // Social stimuli often goal-relevant
        break;
    case ESensoryModality::Semantic:
        Relevance += 0.15f;
        break;
    default:
        break;
    }

    // Emotional valence increases relevance
    Relevance += FMath::Abs(Input.EmotionalValence) * 0.2f;

    return FMath::Clamp(Relevance, 0.0f, 1.0f);
}

float USensoryInputIntegration::ComputeAffordancePotential(const FSensoryInputData& Input)
{
    float Affordance = 0.3f;

    // Modality-based affordance
    switch (Input.Modality)
    {
    case ESensoryModality::Visual:
        Affordance += 0.2f;
        break;
    case ESensoryModality::Tactile:
        Affordance += 0.3f; // Tactile = direct interaction
        break;
    case ESensoryModality::Proprioceptive:
        Affordance += 0.25f;
        break;
    default:
        break;
    }

    // Proximity increases affordance
    float Distance = Input.WorldLocation.Size();
    if (Distance < 200.0f)
    {
        Affordance += 0.2f * (1.0f - Distance / 200.0f);
    }

    return FMath::Clamp(Affordance, 0.0f, 1.0f);
}

FSensoryPercept USensoryInputIntegration::GetPercept(int32 PerceptID) const
{
    for (const FSensoryPercept& Percept : State.Percepts)
    {
        if (Percept.SourceInputID == PerceptID)
        {
            return Percept;
        }
    }
    return FSensoryPercept();
}

TArray<FSensoryPercept> USensoryInputIntegration::GetMostSalientPercepts(int32 Count) const
{
    TArray<FSensoryPercept> SortedPercepts = State.Percepts;
    SortedPercepts.Sort([](const FSensoryPercept& A, const FSensoryPercept& B) {
        return A.Salience > B.Salience;
    });

    if (SortedPercepts.Num() > Count)
    {
        SortedPercepts.SetNum(Count);
    }

    return SortedPercepts;
}

TArray<FSensoryPercept> USensoryInputIntegration::GetPerceptsByModality(ESensoryModality Modality) const
{
    TArray<FSensoryPercept> Result;
    for (const FSensoryPercept& Percept : State.Percepts)
    {
        if (Percept.Modality == Modality)
        {
            Result.Add(Percept);
        }
    }
    return Result;
}

void USensoryInputIntegration::AttemptMultimodalBinding()
{
    // Find percepts that should be bound
    for (int32 i = 0; i < State.Percepts.Num(); ++i)
    {
        for (int32 j = i + 1; j < State.Percepts.Num(); ++j)
        {
            if (ShouldBindPercepts(State.Percepts[i], State.Percepts[j]))
            {
                // Check if already bound
                bool bAlreadyBound = false;
                for (const FMultimodalBinding& Binding : State.Bindings)
                {
                    if (Binding.BoundPerceptIDs.Contains(State.Percepts[i].SourceInputID) &&
                        Binding.BoundPerceptIDs.Contains(State.Percepts[j].SourceInputID))
                    {
                        bAlreadyBound = true;
                        break;
                    }
                }

                if (!bAlreadyBound)
                {
                    TArray<FSensoryPercept> TooBind = {State.Percepts[i], State.Percepts[j]};
                    FMultimodalBinding NewBinding = CreateBinding(TooBind);
                    State.Bindings.Add(NewBinding);

                    OnBindingFormed.Broadcast(NewBinding.BindingID, NewBinding.BindingStrength);
                }
            }
        }
    }
}

bool USensoryInputIntegration::ShouldBindPercepts(const FSensoryPercept& A, const FSensoryPercept& B)
{
    // Different modalities required for multimodal binding
    if (A.Modality == B.Modality)
    {
        return false;
    }

    // Get source inputs
    FSensoryInputData InputA = GetSensoryInput(A.SourceInputID);
    FSensoryInputData InputB = GetSensoryInput(B.SourceInputID);

    // Temporal coherence check
    float TimeDiff = FMath::Abs(InputA.Timestamp - InputB.Timestamp);
    if (TimeDiff > BindingTemporalWindow)
    {
        return false;
    }

    // Spatial coherence check
    float SpatialDist = FVector::Dist(InputA.WorldLocation, InputB.WorldLocation);
    if (SpatialDist > BindingSpatialThreshold)
    {
        return false;
    }

    return true;
}

FMultimodalBinding USensoryInputIntegration::CreateBinding(const TArray<FSensoryPercept>& Percepts)
{
    FMultimodalBinding Binding;
    Binding.BindingID = NextBindingID++;

    for (const FSensoryPercept& Percept : Percepts)
    {
        Binding.BoundPerceptIDs.Add(Percept.SourceInputID);
        Binding.Modalities.AddUnique(Percept.Modality);
    }

    // Compute binding strength from percept saliences
    float TotalSalience = 0.0f;
    for (const FSensoryPercept& Percept : Percepts)
    {
        TotalSalience += Percept.Salience;
    }
    Binding.BindingStrength = TotalSalience / Percepts.Num();

    // Compute temporal coherence
    float MinTime = FLT_MAX, MaxTime = -FLT_MAX;
    for (const FSensoryPercept& Percept : Percepts)
    {
        FSensoryInputData Input = GetSensoryInput(Percept.SourceInputID);
        MinTime = FMath::Min(MinTime, Input.Timestamp);
        MaxTime = FMath::Max(MaxTime, Input.Timestamp);
    }
    Binding.TemporalCoherence = 1.0f - FMath::Min(1.0f, (MaxTime - MinTime) / BindingTemporalWindow);

    // Compute spatial coherence
    FVector CenterPoint = FVector::ZeroVector;
    for (const FSensoryPercept& Percept : Percepts)
    {
        FSensoryInputData Input = GetSensoryInput(Percept.SourceInputID);
        CenterPoint += Input.WorldLocation;
    }
    CenterPoint /= Percepts.Num();

    float MaxDist = 0.0f;
    for (const FSensoryPercept& Percept : Percepts)
    {
        FSensoryInputData Input = GetSensoryInput(Percept.SourceInputID);
        MaxDist = FMath::Max(MaxDist, FVector::Dist(Input.WorldLocation, CenterPoint));
    }
    Binding.SpatialCoherence = 1.0f - FMath::Min(1.0f, MaxDist / BindingSpatialThreshold);

    // Create unified representation
    Binding.UnifiedRepresentation.SetNum(FeatureVectorSize);
    for (int32 i = 0; i < FeatureVectorSize; ++i)
    {
        float Sum = 0.0f;
        for (const FSensoryPercept& Percept : Percepts)
        {
            if (i < Percept.IntegratedFeatures.Num())
            {
                Sum += Percept.IntegratedFeatures[i];
            }
        }
        Binding.UnifiedRepresentation[i] = Sum / Percepts.Num();
    }

    return Binding;
}

FMultimodalBinding USensoryInputIntegration::GetBinding(int32 BindingID) const
{
    for (const FMultimodalBinding& Binding : State.Bindings)
    {
        if (Binding.BindingID == BindingID)
        {
            return Binding;
        }
    }
    return FMultimodalBinding();
}

TArray<FMultimodalBinding> USensoryInputIntegration::GetActiveBindings() const
{
    return State.Bindings;
}

int32 USensoryInputIntegration::ForceBindPercepts(const TArray<int32>& PerceptIDs)
{
    TArray<FSensoryPercept> PercepstToBind;
    for (int32 ID : PerceptIDs)
    {
        for (const FSensoryPercept& Percept : State.Percepts)
        {
            if (Percept.SourceInputID == ID)
            {
                PercepstToBind.Add(Percept);
                break;
            }
        }
    }

    if (PercepstToBind.Num() >= 2)
    {
        FMultimodalBinding NewBinding = CreateBinding(PercepstToBind);
        State.Bindings.Add(NewBinding);
        return NewBinding.BindingID;
    }

    return -1;
}

void USensoryInputIntegration::ShiftAttentionToPercept(int32 PerceptID)
{
    State.AttentionState.PrimaryFocusID = PerceptID;

    // Get percept location
    FSensoryInputData Input = GetSensoryInput(PerceptID);
    State.AttentionState.FocusLocation = Input.WorldLocation;
    State.AttentionState.FocusDirection = Input.RelativeDirection;
    State.AttentionState.FocusDuration = 0.0f;

    OnAttentionShifted.Broadcast(PerceptID, State.AttentionState.FocusLocation);
}

void USensoryInputIntegration::ShiftAttentionToLocation(const FVector& Location)
{
    State.AttentionState.PrimaryFocusID = -1;
    State.AttentionState.FocusLocation = Location;

    AActor* Owner = GetOwner();
    if (Owner)
    {
        State.AttentionState.FocusDirection = (Location - Owner->GetActorLocation()).GetSafeNormal();
    }

    State.AttentionState.FocusDuration = 0.0f;

    OnAttentionShifted.Broadcast(-1, Location);
}

void USensoryInputIntegration::SetAttentionBreadth(float Breadth)
{
    State.AttentionState.AttentionBreadth = FMath::Clamp(Breadth, 0.0f, 1.0f);
}

void USensoryInputIntegration::SetVigilanceLevel(float Level)
{
    State.AttentionState.VigilanceLevel = FMath::Clamp(Level, 0.0f, 1.0f);
}

FAttentionFocusState USensoryInputIntegration::GetAttentionState() const
{
    return State.AttentionState;
}

void USensoryInputIntegration::UpdateAttention(float DeltaTime)
{
    State.AttentionState.FocusDuration += DeltaTime;

    // Check for attention capture by high-salience percepts
    TArray<FSensoryPercept> SalientPercepts = GetMostSalientPercepts(3);

    for (const FSensoryPercept& Percept : SalientPercepts)
    {
        if (Percept.Salience > SalienceThreshold &&
            Percept.SourceInputID != State.AttentionState.PrimaryFocusID)
        {
            // Consider attention shift based on salience and vigilance
            float ShiftProbability = Percept.Salience * State.AttentionState.VigilanceLevel;

            if (FMath::FRand() < ShiftProbability * DeltaTime)
            {
                ShiftAttentionToPercept(Percept.SourceInputID);
                break;
            }
        }
    }

    // Update secondary focus
    State.AttentionState.SecondaryFocusIDs.Empty();
    for (const FSensoryPercept& Percept : SalientPercepts)
    {
        if (Percept.SourceInputID != State.AttentionState.PrimaryFocusID)
        {
            State.AttentionState.SecondaryFocusIDs.Add(Percept.SourceInputID);
        }
    }
}

void USensoryInputIntegration::GeneratePredictions()
{
    // Simple prediction: expect similar inputs to continue
    for (FSensoryPercept& Percept : State.Percepts)
    {
        // Store current state for next prediction
        PreviousPerceptStates.Add(Percept.SourceInputID, Percept.IntegratedFeatures);

        // Predict slight decay in novelty
        Percept.PredictedValence = Percept.PredictedValence * 0.95f;
    }
}

float USensoryInputIntegration::ComputePredictionError()
{
    float TotalError = 0.0f;
    int32 ErrorCount = 0;

    for (const FSensoryPercept& Percept : State.Percepts)
    {
        TArray<float>* PreviousState = PreviousPerceptStates.Find(Percept.SourceInputID);
        if (PreviousState)
        {
            // Compute difference between predicted and actual
            float Error = 0.0f;
            int32 VecSize = FMath::Min(PreviousState->Num(), Percept.IntegratedFeatures.Num());

            for (int32 i = 0; i < VecSize; ++i)
            {
                float Diff = (*PreviousState)[i] - Percept.IntegratedFeatures[i];
                Error += Diff * Diff;
            }

            Error = FMath::Sqrt(Error / FMath::Max(1, VecSize));
            TotalError += Error;
            ErrorCount++;
        }
    }

    State.PredictionError = (ErrorCount > 0) ? (TotalError / ErrorCount) : 0.0f;
    return State.PredictionError;
}

void USensoryInputIntegration::UpdatePredictionModel()
{
    // Update prediction state based on current percepts
    for (int32 i = 0; i < FeatureVectorSize; ++i)
    {
        float Sum = 0.0f;
        for (const FSensoryPercept& Percept : State.Percepts)
        {
            if (i < Percept.IntegratedFeatures.Num())
            {
                Sum += Percept.IntegratedFeatures[i];
            }
        }

        float Avg = (State.Percepts.Num() > 0) ? (Sum / State.Percepts.Num()) : 0.0f;
        PredictionState[i] = FMath::Lerp(PredictionState[i], Avg, PredictionLearningRate);
    }
}

void USensoryInputIntegration::ProcessForEchobeatStep(int32 Step)
{
    State.CurrentEchobeatStep = Step;

    // Bind percepts to current step
    for (FSensoryPercept& Percept : State.Percepts)
    {
        Percept.BoundEchobeatStep = Step;
    }

    // Modulate attention based on step type
    // Steps 1, 5, 9 (Pivotal) - narrow attention
    // Steps 2-4, 6-8, 10-12 (Affordance/Salience) - broader attention
    int32 StepInTriad = ((Step - 1) % 4);
    if (StepInTriad == 0)
    {
        // Pivotal step - narrow focus
        State.AttentionState.AttentionBreadth = FMath::Lerp(State.AttentionState.AttentionBreadth, 0.3f, 0.2f);
        State.AttentionState.AttentionDepth = FMath::Lerp(State.AttentionState.AttentionDepth, 0.8f, 0.2f);
    }
    else
    {
        // Affordance/Salience steps - broader awareness
        State.AttentionState.AttentionBreadth = FMath::Lerp(State.AttentionState.AttentionBreadth, 0.7f, 0.2f);
        State.AttentionState.AttentionDepth = FMath::Lerp(State.AttentionState.AttentionDepth, 0.5f, 0.2f);
    }
}

TArray<float> USensoryInputIntegration::GetSensoryOutputForStream(ECognitiveStreamType StreamType) const
{
    TArray<float> Output;
    Output.SetNum(FeatureVectorSize);

    switch (StreamType)
    {
    case ECognitiveStreamType::Pivotal:
        // Focus on most salient percept
        if (State.AttentionState.PrimaryFocusID >= 0)
        {
            for (const FSensoryPercept& Percept : State.Percepts)
            {
                if (Percept.SourceInputID == State.AttentionState.PrimaryFocusID)
                {
                    return Percept.IntegratedFeatures;
                }
            }
        }
        break;

    case ECognitiveStreamType::Affordance:
        // Aggregate high-affordance percepts
        {
            int32 Count = 0;
            for (const FSensoryPercept& Percept : State.Percepts)
            {
                if (Percept.AffordancePotential > 0.5f)
                {
                    for (int32 i = 0; i < FMath::Min(FeatureVectorSize, Percept.IntegratedFeatures.Num()); ++i)
                    {
                        Output[i] += Percept.IntegratedFeatures[i];
                    }
                    Count++;
                }
            }
            if (Count > 0)
            {
                for (float& Val : Output)
                {
                    Val /= Count;
                }
            }
        }
        break;

    case ECognitiveStreamType::Salience:
        // Aggregate all salient percepts
        {
            int32 Count = 0;
            for (const FSensoryPercept& Percept : State.Percepts)
            {
                if (Percept.Salience > SalienceThreshold * 0.5f)
                {
                    for (int32 i = 0; i < FMath::Min(FeatureVectorSize, Percept.IntegratedFeatures.Num()); ++i)
                    {
                        Output[i] += Percept.IntegratedFeatures[i] * Percept.Salience;
                    }
                    Count++;
                }
            }
            if (Count > 0)
            {
                for (float& Val : Output)
                {
                    Val /= Count;
                }
            }
        }
        break;
    }

    return Output;
}

ESystem4Term USensoryInputIntegration::MapPerceptToTerm(const FSensoryPercept& Percept) const
{
    // Map based on percept characteristics
    if (Percept.Salience > 0.8f)
    {
        return ESystem4Term::T1_Perception; // High salience = primary perception
    }
    else if (Percept.GoalRelevance > 0.7f)
    {
        return ESystem4Term::T2_IdeaFormation; // Goal-relevant = idea formation
    }
    else if (Percept.AffordancePotential > 0.7f)
    {
        return ESystem4Term::T5_ActionSequence; // High affordance = action potential
    }
    else
    {
        return ESystem4Term::T4_SensoryInput; // Default sensory input
    }
}

FSensoryIntegrationState USensoryInputIntegration::GetIntegrationState() const
{
    return State;
}

float USensoryInputIntegration::GetSensoryLoad() const
{
    return State.SensoryLoad;
}

float USensoryInputIntegration::GetPredictionError() const
{
    return State.PredictionError;
}

void USensoryInputIntegration::DecayPercepts(float DeltaTime)
{
    float DecayRate = 0.1f;

    for (int32 i = State.Percepts.Num() - 1; i >= 0; --i)
    {
        State.Percepts[i].Confidence -= DecayRate * DeltaTime;
        State.Percepts[i].Salience *= (1.0f - DecayRate * DeltaTime * 0.5f);

        if (State.Percepts[i].Confidence <= 0.0f)
        {
            State.Percepts.RemoveAt(i);
        }
    }

    // Decay bindings
    for (int32 i = State.Bindings.Num() - 1; i >= 0; --i)
    {
        State.Bindings[i].BindingStrength -= DecayRate * DeltaTime * 0.5f;

        if (State.Bindings[i].BindingStrength <= 0.0f)
        {
            State.Bindings.RemoveAt(i);
        }
    }
}

void USensoryInputIntegration::ComputeSensoryLoad()
{
    // Compute based on number and intensity of active inputs
    float Load = 0.0f;

    for (const FSensoryInputData& Input : State.ActiveInputs)
    {
        Load += Input.Intensity * (static_cast<float>(Input.Priority) / 4.0f);
    }

    // Normalize by max inputs
    State.SensoryLoad = FMath::Clamp(Load / MaxActiveInputs, 0.0f, 1.0f);
}

TArray<float> USensoryInputIntegration::IntegrateFeatures(const TArray<FSensoryInputData>& Inputs)
{
    TArray<float> Integrated;
    Integrated.SetNum(FeatureVectorSize);

    for (float& Val : Integrated)
    {
        Val = 0.0f;
    }

    if (Inputs.Num() == 0)
    {
        return Integrated;
    }

    // Weight by intensity and priority
    float TotalWeight = 0.0f;

    for (const FSensoryInputData& Input : Inputs)
    {
        float Weight = Input.Intensity * (1.0f + static_cast<float>(Input.Priority) / 4.0f);
        TotalWeight += Weight;

        for (int32 i = 0; i < FMath::Min(FeatureVectorSize, Input.FeatureVector.Num()); ++i)
        {
            Integrated[i] += Input.FeatureVector[i] * Weight;
        }
    }

    if (TotalWeight > 0.0f)
    {
        for (float& Val : Integrated)
        {
            Val /= TotalWeight;
        }
    }

    return Integrated;
}

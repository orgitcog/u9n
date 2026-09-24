// SensorimotorIntegration.cpp
// Implementation of 4E Embodied Cognition Sensorimotor Integration

#include "SensorimotorIntegration.h"
#include "../Core/CognitiveCycleManager.h"
#include "../Wisdom/WisdomCultivation.h"
#include "Math/UnrealMathUtility.h"

USensorimotorIntegration::USensorimotorIntegration()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USensorimotorIntegration::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();

    // Initialize body schema
    EmbodimentState.BodySchema.Position = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
    EmbodimentState.BodySchema.Orientation = GetOwner() ? GetOwner()->GetActorRotation() : FRotator::ZeroRotator;
    EmbodimentState.BodySchema.EnergyLevel = 1.0f;
    EmbodimentState.BodySchema.Stability = 1.0f;

    // Initialize peripersonal space
    EmbodimentState.PeripersonalSpace.ReachRadius = 100.0f;
    EmbodimentState.PeripersonalSpace.GraspRadius = 50.0f;
}

void USensorimotorIntegration::TickComponent(float DeltaTime, ELevelTick TickType, 
                                              FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableSensorimotorLoop)
    {
        return;
    }

    // Process sensory integration
    ProcessSensoryIntegration(DeltaTime);

    // Process motor execution
    ProcessMotorExecution(DeltaTime);

    // Process affordance detection
    if (bEnableAffordanceDetection)
    {
        ProcessAffordanceDetection(DeltaTime);
    }

    // Process predictive coding
    if (bEnablePredictiveCoding)
    {
        ProcessPredictiveCoding(DeltaTime);
    }

    // Update 4E state
    Update4EState(DeltaTime);
}

void USensorimotorIntegration::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        CycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
        WisdomComponent = Owner->FindComponentByClass<UWisdomCultivation>();
    }
}

// ========================================
// SENSORY INPUT
// ========================================

void USensorimotorIntegration::RegisterSensoryInput(const FSensoryInput& Input)
{
    FSensoryInput NewInput = Input;
    NewInput.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    CurrentInputs.Add(NewInput);
    OnSensoryInput.Broadcast(NewInput);

    // Update wisdom component salience if available
    if (WisdomComponent && Input.Salience > 0.5f)
    {
        WisdomComponent->AddSalienceItem(Input.InputID, Input.Salience, Input.Intensity);
    }
}

TArray<FSensoryInput> USensorimotorIntegration::GetCurrentSensoryInputs() const
{
    return CurrentInputs;
}

TArray<FSensoryInput> USensorimotorIntegration::GetInputsByModality(ESensoryModality Modality) const
{
    TArray<FSensoryInput> FilteredInputs;
    for (const FSensoryInput& Input : CurrentInputs)
    {
        if (Input.Modality == Modality)
        {
            FilteredInputs.Add(Input);
        }
    }
    return FilteredInputs;
}

FSensoryInput USensorimotorIntegration::GetMostSalientInput() const
{
    FSensoryInput MostSalient;
    float HighestSalience = -1.0f;

    for (const FSensoryInput& Input : CurrentInputs)
    {
        if (Input.Salience > HighestSalience)
        {
            HighestSalience = Input.Salience;
            MostSalient = Input;
        }
    }

    return MostSalient;
}

void USensorimotorIntegration::ClearOldInputs(float MaxAge)
{
    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    CurrentInputs.RemoveAll([CurrentTime, MaxAge](const FSensoryInput& Input) {
        return (CurrentTime - Input.Timestamp) > MaxAge;
    });
}

// ========================================
// MOTOR OUTPUT
// ========================================

void USensorimotorIntegration::IssueMotorCommand(const FMotorCommand& Command)
{
    // Add to pending commands sorted by priority
    int32 InsertIndex = 0;
    for (int32 i = 0; i < PendingCommands.Num(); ++i)
    {
        if (Command.Priority > PendingCommands[i].Priority)
        {
            InsertIndex = i;
            break;
        }
        InsertIndex = i + 1;
    }

    PendingCommands.Insert(Command, InsertIndex);
    OnMotorCommand.Broadcast(Command);

    // Register affordance selection if available
    if (WisdomComponent)
    {
        WisdomComponent->AddAffordance(Command.CommandID, 
            UEnum::GetValueAsString(Command.ActionType), 1.0f, Command.Priority);
    }
}

TArray<FMotorCommand> USensorimotorIntegration::GetPendingCommands() const
{
    return PendingCommands;
}

void USensorimotorIntegration::CancelCommand(const FString& CommandID)
{
    PendingCommands.RemoveAll([&CommandID](const FMotorCommand& Cmd) {
        return Cmd.CommandID == CommandID;
    });
}

void USensorimotorIntegration::CancelAllCommands()
{
    PendingCommands.Empty();
    CurrentCommand = FMotorCommand();
}

FMotorCommand USensorimotorIntegration::GetCurrentCommand() const
{
    return CurrentCommand;
}

// ========================================
// AFFORDANCE DETECTION
// ========================================

TArray<FDetectedAffordance> USensorimotorIntegration::DetectAffordances()
{
    DetectedAffordances.Empty();

    // Detect affordances based on sensory inputs and body schema
    FVector CurrentPosition = EmbodimentState.BodySchema.Position;

    // For each sensory input, check for potential affordances
    for (const FSensoryInput& Input : CurrentInputs)
    {
        if (Input.Modality == ESensoryModality::Visual)
        {
            float Distance = FVector::Dist(CurrentPosition, Input.SpatialLocation);

            if (Distance <= AffordanceDetectionRadius)
            {
                FDetectedAffordance Affordance;
                Affordance.AffordanceID = GenerateAffordanceID();
                Affordance.ObjectID = Input.InputID;
                Affordance.Location = Input.SpatialLocation;

                // Determine action verb based on distance and input properties
                if (Distance <= EmbodimentState.PeripersonalSpace.GraspRadius)
                {
                    Affordance.ActionVerb = TEXT("grasp");
                    Affordance.Availability = 1.0f;
                }
                else if (Distance <= EmbodimentState.PeripersonalSpace.ReachRadius)
                {
                    Affordance.ActionVerb = TEXT("reach");
                    Affordance.Availability = 0.8f;
                }
                else
                {
                    Affordance.ActionVerb = TEXT("approach");
                    Affordance.Availability = 0.5f;
                }

                // Calculate scores
                Affordance.SkillMatch = 0.8f; // Simplified
                Affordance.ContextualRelevance = Input.Salience;
                Affordance.ExpectedUtility = Input.Intensity * Affordance.Availability;
                Affordance.Risk = FMath::Clamp(Distance / AffordanceDetectionRadius * 0.3f, 0.0f, 1.0f);

                DetectedAffordances.Add(Affordance);
                OnAffordanceDetected.Broadcast(Affordance);
            }
        }
    }

    // Update peripersonal space
    EmbodimentState.PeripersonalSpace.NearbyAffordances = DetectedAffordances;

    return DetectedAffordances;
}

FDetectedAffordance USensorimotorIntegration::GetBestAffordance() const
{
    FDetectedAffordance Best;
    float HighestScore = -1.0f;

    for (const FDetectedAffordance& Aff : DetectedAffordances)
    {
        float Score = Aff.Availability * 0.3f +
                      Aff.SkillMatch * 0.2f +
                      Aff.ContextualRelevance * 0.2f +
                      Aff.ExpectedUtility * 0.2f -
                      Aff.Risk * 0.1f;

        if (Score > HighestScore)
        {
            HighestScore = Score;
            Best = Aff;
        }
    }

    return Best;
}

void USensorimotorIntegration::SelectAffordance(const FString& AffordanceID)
{
    SelectedAffordanceID = AffordanceID;

    // Find the affordance and generate motor commands
    for (const FDetectedAffordance& Aff : DetectedAffordances)
    {
        if (Aff.AffordanceID == AffordanceID)
        {
            // Generate appropriate motor command
            FMotorCommand Command;
            Command.CommandID = FString::Printf(TEXT("CMD_%s"), *AffordanceID);
            Command.TargetLocation = Aff.Location;
            Command.Priority = Aff.ExpectedUtility;

            if (Aff.ActionVerb == TEXT("grasp"))
            {
                Command.ActionType = EMotorActionType::Manipulation;
                Command.Intensity = 0.8f;
            }
            else if (Aff.ActionVerb == TEXT("reach"))
            {
                Command.ActionType = EMotorActionType::Manipulation;
                Command.Intensity = 0.5f;
            }
            else
            {
                Command.ActionType = EMotorActionType::Locomotion;
                Command.Intensity = 0.6f;
            }

            IssueMotorCommand(Command);

            // Update wisdom component
            if (WisdomComponent)
            {
                WisdomComponent->SelectAffordance(AffordanceID);
            }

            break;
        }
    }
}

TArray<FDetectedAffordance> USensorimotorIntegration::GetDetectedAffordances() const
{
    return DetectedAffordances;
}

// ========================================
// BODY SCHEMA
// ========================================

void USensorimotorIntegration::UpdateBodySchema()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    // Update position and orientation
    FVector NewPosition = Owner->GetActorLocation();
    FRotator NewOrientation = Owner->GetActorRotation();

    // Calculate velocity
    float DeltaTime = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.016f;
    if (DeltaTime > 0.0f)
    {
        EmbodimentState.BodySchema.Velocity = (NewPosition - EmbodimentState.BodySchema.Position) / DeltaTime;
    }

    EmbodimentState.BodySchema.Position = NewPosition;
    EmbodimentState.BodySchema.Orientation = NewOrientation;

    // Update stability based on velocity
    float Speed = EmbodimentState.BodySchema.Velocity.Size();
    EmbodimentState.BodySchema.Stability = FMath::Clamp(1.0f - Speed * 0.001f, 0.0f, 1.0f);
}

FBodySchema USensorimotorIntegration::GetBodySchema() const
{
    return EmbodimentState.BodySchema;
}

void USensorimotorIntegration::SetLimbTransform(const FString& LimbName, const FTransform& Transform)
{
    EmbodimentState.BodySchema.LimbTransforms.Add(LimbName, Transform);
}

FTransform USensorimotorIntegration::GetLimbTransform(const FString& LimbName) const
{
    if (EmbodimentState.BodySchema.LimbTransforms.Contains(LimbName))
    {
        return EmbodimentState.BodySchema.LimbTransforms[LimbName];
    }
    return FTransform::Identity;
}

// ========================================
// PERIPERSONAL SPACE
// ========================================

void USensorimotorIntegration::UpdatePeripersonalSpace()
{
    EmbodimentState.PeripersonalSpace.ObjectsInReach.Empty();

    FVector CurrentPosition = EmbodimentState.BodySchema.Position;

    for (const FSensoryInput& Input : CurrentInputs)
    {
        float Distance = FVector::Dist(CurrentPosition, Input.SpatialLocation);
        if (Distance <= EmbodimentState.PeripersonalSpace.ReachRadius)
        {
            EmbodimentState.PeripersonalSpace.ObjectsInReach.Add(Input.InputID);
        }
    }

    // Update crowdedness
    EmbodimentState.PeripersonalSpace.Crowdedness = 
        FMath::Min(1.0f, EmbodimentState.PeripersonalSpace.ObjectsInReach.Num() * 0.1f);
}

FPeripersonalSpace USensorimotorIntegration::GetPeripersonalSpace() const
{
    return EmbodimentState.PeripersonalSpace;
}

bool USensorimotorIntegration::IsObjectInReach(const FString& ObjectID) const
{
    return EmbodimentState.PeripersonalSpace.ObjectsInReach.Contains(ObjectID);
}

// ========================================
// SENSORIMOTOR CONTINGENCIES
// ========================================

void USensorimotorIntegration::LearnContingency(const FMotorCommand& Action, const TArray<FSensoryInput>& Outcome)
{
    FSensorimotorContingency Contingency;
    Contingency.ContingencyID = GenerateContingencyID();
    Contingency.Action = Action;
    Contingency.ExpectedOutcome = Outcome;
    Contingency.ActualOutcome = Outcome;
    Contingency.PredictionError = 0.0f;
    Contingency.Confidence = 0.5f;
    Contingency.ExperienceCount = 1;

    LearnedContingencies.Add(Contingency);
}

TArray<FSensoryInput> USensorimotorIntegration::PredictOutcome(const FMotorCommand& Action) const
{
    // Find matching contingency
    for (const FSensorimotorContingency& Contingency : LearnedContingencies)
    {
        if (Contingency.Action.ActionType == Action.ActionType)
        {
            return Contingency.ExpectedOutcome;
        }
    }

    return TArray<FSensoryInput>();
}

void USensorimotorIntegration::UpdateContingency(const FString& ContingencyID, 
                                                  const TArray<FSensoryInput>& ActualOutcome)
{
    for (FSensorimotorContingency& Contingency : LearnedContingencies)
    {
        if (Contingency.ContingencyID == ContingencyID)
        {
            Contingency.ActualOutcome = ActualOutcome;
            Contingency.PredictionError = ComputeSensoryDistance(Contingency.ExpectedOutcome, ActualOutcome);
            Contingency.ExperienceCount++;

            // Update confidence based on prediction error
            Contingency.Confidence = FMath::Lerp(Contingency.Confidence, 
                                                  1.0f - Contingency.PredictionError, 
                                                  0.1f);

            // Broadcast prediction error if significant
            if (Contingency.PredictionError > 0.3f)
            {
                OnPredictionError.Broadcast(ContingencyID, Contingency.PredictionError);
            }

            break;
        }
    }
}

float USensorimotorIntegration::GetPredictionError(const FString& ContingencyID) const
{
    for (const FSensorimotorContingency& Contingency : LearnedContingencies)
    {
        if (Contingency.ContingencyID == ContingencyID)
        {
            return Contingency.PredictionError;
        }
    }
    return 0.0f;
}

// ========================================
// 4E EMBODIMENT
// ========================================

F4EEmbodimentState USensorimotorIntegration::Get4EState() const
{
    return EmbodimentState;
}

float USensorimotorIntegration::GetEmbodimentLevel(EEmbodimentDimension Dimension) const
{
    switch (Dimension)
    {
        case EEmbodimentDimension::Embodied:
            return EmbodimentState.EmbodiedLevel;
        case EEmbodimentDimension::Embedded:
            return EmbodimentState.EmbeddedLevel;
        case EEmbodimentDimension::Enacted:
            return EmbodimentState.EnactedLevel;
        case EEmbodimentDimension::Extended:
            return EmbodimentState.ExtendedLevel;
        default:
            return 0.0f;
    }
}

void USensorimotorIntegration::RegisterExtendedTool(const FString& ToolID)
{
    if (!EmbodimentState.ExtendedTools.Contains(ToolID))
    {
        EmbodimentState.ExtendedTools.Add(ToolID);
    }
}

void USensorimotorIntegration::UnregisterExtendedTool(const FString& ToolID)
{
    EmbodimentState.ExtendedTools.Remove(ToolID);
}

float USensorimotorIntegration::GetOverallEmbodiment() const
{
    return EmbodimentState.OverallEmbodiment;
}

// ========================================
// INTERNAL PROCESSING
// ========================================

void USensorimotorIntegration::ProcessSensoryIntegration(float DeltaTime)
{
    SensoryTimer += DeltaTime;
    if (SensoryTimer >= SensoryIntegrationRate)
    {
        SensoryTimer -= SensoryIntegrationRate;

        // Update body schema
        UpdateBodySchema();

        // Update peripersonal space
        UpdatePeripersonalSpace();

        // Integrate sensory inputs
        IntegrateSensoryInputs();

        // Clear old inputs
        ClearOldInputs(5.0f);
    }
}

void USensorimotorIntegration::ProcessMotorExecution(float DeltaTime)
{
    MotorTimer += DeltaTime;
    if (MotorTimer >= MotorExecutionRate)
    {
        MotorTimer -= MotorExecutionRate;

        // Execute highest priority command
        if (PendingCommands.Num() > 0)
        {
            CurrentCommand = PendingCommands[0];
            PendingCommands.RemoveAt(0);
            ExecuteMotorCommand(CurrentCommand);
        }
    }
}

void USensorimotorIntegration::ProcessAffordanceDetection(float DeltaTime)
{
    static float AffordanceTimer = 0.0f;
    AffordanceTimer += DeltaTime;

    if (AffordanceTimer >= 0.5f) // Detect every 0.5 seconds
    {
        AffordanceTimer = 0.0f;
        DetectAffordances();
        UpdateAffordanceScores();
    }
}

void USensorimotorIntegration::ProcessPredictiveCoding(float DeltaTime)
{
    // Update active contingencies with current sensory state
    for (FSensorimotorContingency& Contingency : EmbodimentState.ActiveContingencies)
    {
        // Compare predicted vs actual
        float Error = ComputeSensoryDistance(Contingency.ExpectedOutcome, CurrentInputs);
        Contingency.PredictionError = FMath::Lerp(Contingency.PredictionError, Error, 0.1f);
    }
}

void USensorimotorIntegration::IntegrateSensoryInputs()
{
    // Compute integrated salience across modalities
    TMap<ESensoryModality, float> ModalitySalience;

    for (const FSensoryInput& Input : CurrentInputs)
    {
        if (ModalitySalience.Contains(Input.Modality))
        {
            ModalitySalience[Input.Modality] = FMath::Max(ModalitySalience[Input.Modality], Input.Salience);
        }
        else
        {
            ModalitySalience.Add(Input.Modality, Input.Salience);
        }
    }

    // Update wisdom component with integrated salience
    if (WisdomComponent)
    {
        for (auto& Pair : ModalitySalience)
        {
            FString ModalityName = UEnum::GetValueAsString(Pair.Key);
            WisdomComponent->UpdateSalience(ModalityName, Pair.Value * 0.1f);
        }
    }
}

void USensorimotorIntegration::ExecuteMotorCommand(const FMotorCommand& Command)
{
    // Create contingency for this action
    FSensorimotorContingency Contingency;
    Contingency.ContingencyID = GenerateContingencyID();
    Contingency.Action = Command;
    Contingency.ExpectedOutcome = PredictOutcome(Command);
    Contingency.Confidence = 0.5f;

    EmbodimentState.ActiveContingencies.Add(Contingency);

    // Limit active contingencies
    while (EmbodimentState.ActiveContingencies.Num() > 10)
    {
        EmbodimentState.ActiveContingencies.RemoveAt(0);
    }
}

void USensorimotorIntegration::UpdateAffordanceScores()
{
    // Update affordance scores based on current context
    for (FDetectedAffordance& Aff : DetectedAffordances)
    {
        // Update contextual relevance based on wisdom component
        if (WisdomComponent)
        {
            FSalienceLandscape Landscape = WisdomComponent->GetSalienceLandscape();
            if (Landscape.FocusTarget == Aff.ObjectID)
            {
                Aff.ContextualRelevance = 1.0f;
            }
        }

        // Update availability based on body schema
        float Distance = FVector::Dist(EmbodimentState.BodySchema.Position, Aff.Location);
        Aff.Availability = FMath::Clamp(1.0f - Distance / AffordanceDetectionRadius, 0.0f, 1.0f);
    }
}

void USensorimotorIntegration::Update4EState(float DeltaTime)
{
    UpdateEmbodiedLevel();
    UpdateEmbeddedLevel();
    UpdateEnactedLevel();
    UpdateExtendedLevel();

    // Compute overall embodiment
    EmbodimentState.OverallEmbodiment = (EmbodimentState.EmbodiedLevel +
                                          EmbodimentState.EmbeddedLevel +
                                          EmbodimentState.EnactedLevel +
                                          EmbodimentState.ExtendedLevel) / 4.0f;
}

void USensorimotorIntegration::UpdateEmbodiedLevel()
{
    // Embodied level based on body schema coherence
    float StabilityScore = EmbodimentState.BodySchema.Stability;
    float EnergyScore = EmbodimentState.BodySchema.EnergyLevel;
    float LimbCount = EmbodimentState.BodySchema.LimbTransforms.Num() > 0 ? 1.0f : 0.5f;

    EmbodimentState.EmbodiedLevel = (StabilityScore + EnergyScore + LimbCount) / 3.0f;
}

void USensorimotorIntegration::UpdateEmbeddedLevel()
{
    // Embedded level based on environmental coupling
    float AffordanceCount = FMath::Min(1.0f, DetectedAffordances.Num() * 0.1f);
    float ObjectsInReach = FMath::Min(1.0f, EmbodimentState.PeripersonalSpace.ObjectsInReach.Num() * 0.1f);
    float SensoryRichness = FMath::Min(1.0f, CurrentInputs.Num() * 0.05f);

    EmbodimentState.EmbeddedLevel = (AffordanceCount + ObjectsInReach + SensoryRichness) / 3.0f;
}

void USensorimotorIntegration::UpdateEnactedLevel()
{
    // Enacted level based on sensorimotor contingencies
    float ContingencyCount = FMath::Min(1.0f, LearnedContingencies.Num() * 0.05f);

    float AvgConfidence = 0.0f;
    for (const FSensorimotorContingency& C : LearnedContingencies)
    {
        AvgConfidence += C.Confidence;
    }
    if (LearnedContingencies.Num() > 0)
    {
        AvgConfidence /= LearnedContingencies.Num();
    }

    float ActionActivity = PendingCommands.Num() > 0 || CurrentCommand.CommandID.Len() > 0 ? 1.0f : 0.3f;

    EmbodimentState.EnactedLevel = (ContingencyCount + AvgConfidence + ActionActivity) / 3.0f;
}

void USensorimotorIntegration::UpdateExtendedLevel()
{
    // Extended level based on tool use
    float ToolCount = FMath::Min(1.0f, EmbodimentState.ExtendedTools.Num() * 0.2f);

    EmbodimentState.ExtendedLevel = ToolCount;
}

float USensorimotorIntegration::ComputeSensoryDistance(const TArray<FSensoryInput>& A, 
                                                        const TArray<FSensoryInput>& B) const
{
    if (A.Num() == 0 && B.Num() == 0)
    {
        return 0.0f;
    }

    if (A.Num() == 0 || B.Num() == 0)
    {
        return 1.0f;
    }

    // Simple distance based on intensity differences
    float TotalDiff = 0.0f;
    int32 Comparisons = 0;

    for (const FSensoryInput& InputA : A)
    {
        for (const FSensoryInput& InputB : B)
        {
            if (InputA.Modality == InputB.Modality)
            {
                TotalDiff += FMath::Abs(InputA.Intensity - InputB.Intensity);
                ++Comparisons;
            }
        }
    }

    return Comparisons > 0 ? TotalDiff / Comparisons : 1.0f;
}

FString USensorimotorIntegration::GenerateContingencyID() const
{
    return FString::Printf(TEXT("CONT_%d_%d"), ++const_cast<USensorimotorIntegration*>(this)->ContingencyIDCounter, 
                           FMath::RandRange(1000, 9999));
}

FString USensorimotorIntegration::GenerateAffordanceID() const
{
    return FString::Printf(TEXT("AFF_%d_%d"), ++const_cast<USensorimotorIntegration*>(this)->AffordanceIDCounter, 
                           FMath::RandRange(1000, 9999));
}

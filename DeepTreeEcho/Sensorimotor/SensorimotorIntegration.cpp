/**
 * Sensorimotor Integration Implementation
 * 
 * Implements the sensorimotor coupling layer for Deep-Tree-Echo avatar control.
 */

#include "SensorimotorIntegration.h"
#include "../ActiveInference/AXIOMActiveInference.h"

// ========================================
// CONSTRUCTOR
// ========================================

USensorimotorIntegration::USensorimotorIntegration()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f;
}

// ========================================
// LIFECYCLE
// ========================================

void USensorimotorIntegration::BeginPlay()
{
    Super::BeginPlay();
    Initialize();
}

void USensorimotorIntegration::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    AccumulatedTime += DeltaTime;
    
    // Process sensory prediction
    if (Config.bEnablePredictiveControl)
    {
        ProcessSensoryPrediction();
        ProcessMotorPrediction();
    }
    
    // Update attention based on prediction errors
    UpdateAttention();
    
    // Detect affordances
    if (Config.bEnableAffordanceDetection)
    {
        DetectAffordances();
    }
    
    // Adapt body schema if needed
    if (Config.bEnableBodySchemaAdaptation)
    {
        AdaptBodySchema();
    }
    
    // Integrate with cognitive cycle
    if (CognitiveCycleManager)
    {
        int32 GlobalStep = CognitiveCycleManager->CycleState.GlobalStep;
        if (GlobalStep != LastProcessedStep)
        {
            EConsciousnessStream ActiveStream;
            int32 StepInCycle = ((GlobalStep - 1) % 12) + 1;
            
            if (StepInCycle >= 1 && StepInCycle <= 4)
                ActiveStream = EConsciousnessStream::Perception;
            else if (StepInCycle >= 5 && StepInCycle <= 8)
                ActiveStream = EConsciousnessStream::Action;
            else
                ActiveStream = EConsciousnessStream::Simulation;
            
            ProcessCognitiveStep(GlobalStep, ActiveStream);
            LastProcessedStep = GlobalStep;
        }
    }
    
    // Periodically consolidate contingencies
    if (FMath::Fmod(AccumulatedTime, 10.0f) < DeltaTime)
    {
        ConsolidateContingencies();
    }
}

// ========================================
// INITIALIZATION
// ========================================

void USensorimotorIntegration::Initialize()
{
    // Initialize body schema with default values
    CurrentState.BodySchema.State = EBodySchemaState::Calibrating;
    CurrentState.BodySchema.ReachableSpace = FBox(FVector(-100, -100, 0), FVector(100, 100, 200));
    
    // Set default effector capabilities
    CurrentState.BodySchema.EffectorCapabilities.Add(EMotorEffector::Locomotion, 1.0f);
    CurrentState.BodySchema.EffectorCapabilities.Add(EMotorEffector::Manipulation, 1.0f);
    CurrentState.BodySchema.EffectorCapabilities.Add(EMotorEffector::Gaze, 1.0f);
    CurrentState.BodySchema.EffectorCapabilities.Add(EMotorEffector::Vocalization, 1.0f);
    CurrentState.BodySchema.EffectorCapabilities.Add(EMotorEffector::Expression, 1.0f);
    CurrentState.BodySchema.EffectorCapabilities.Add(EMotorEffector::Posture, 1.0f);
    
    // Initialize coupling mode
    CurrentState.CouplingMode = ECouplingMode::Reactive;
    
    UE_LOG(LogTemp, Log, TEXT("Sensorimotor Integration initialized"));
}

void USensorimotorIntegration::Reset()
{
    CurrentState = FSensorimotorState();
    PreviousState = FSensorimotorState();
    Contingencies.Empty();
    PredictedSensory.Empty();
    PredictedMotor.Empty();
    AccumulatedTime = 0.0f;
    
    Initialize();
}

// ========================================
// SENSORY INPUT
// ========================================

void USensorimotorIntegration::RegisterSensoryChannel(FName ChannelID, ESensoryModality Modality, int32 DataDimension)
{
    FSensoryChannel NewChannel;
    NewChannel.ChannelID = ChannelID;
    NewChannel.Modality = Modality;
    NewChannel.RawData.SetNum(DataDimension);
    NewChannel.Features.SetNum(DataDimension);
    NewChannel.AttentionWeight = 1.0f;
    NewChannel.Reliability = 1.0f;
    
    CurrentState.SensoryChannels.Add(NewChannel);
    
    UE_LOG(LogTemp, Log, TEXT("Registered sensory channel: %s"), *ChannelID.ToString());
}

void USensorimotorIntegration::UpdateSensoryChannel(FName ChannelID, const TArray<float>& RawData)
{
    for (FSensoryChannel& Channel : CurrentState.SensoryChannels)
    {
        if (Channel.ChannelID == ChannelID)
        {
            // Store previous for prediction error
            TArray<float> PreviousData = Channel.RawData;
            
            // Update raw data
            Channel.RawData = RawData;
            Channel.LastUpdateTime = AccumulatedTime;
            
            // Extract features (simple normalization for now)
            Channel.Features = NormalizeFeatures(RawData);
            
            // Compute prediction error if we have a prediction
            if (PredictedSensory.Contains(ChannelID))
            {
                const TArray<float>& Predicted = PredictedSensory[ChannelID];
                float Error = 0.0f;
                for (int32 i = 0; i < FMath::Min(RawData.Num(), Predicted.Num()); ++i)
                {
                    Error += FMath::Square(RawData[i] - Predicted[i]);
                }
                Channel.PredictionError = FMath::Sqrt(Error / FMath::Max(1, RawData.Num()));
                
                // Broadcast if error exceeds threshold
                if (Channel.PredictionError > Config.AdaptationThreshold)
                {
                    OnPredictionError.Broadcast(ChannelID, Channel.PredictionError);
                }
            }
            
            break;
        }
    }
}

void USensorimotorIntegration::SetChannelAttention(FName ChannelID, float AttentionWeight)
{
    for (FSensoryChannel& Channel : CurrentState.SensoryChannels)
    {
        if (Channel.ChannelID == ChannelID)
        {
            Channel.AttentionWeight = FMath::Clamp(AttentionWeight, 0.0f, 1.0f);
            break;
        }
    }
}

TArray<float> USensorimotorIntegration::GetIntegratedSensoryFeatures() const
{
    TArray<float> Integrated;
    float TotalWeight = 0.0f;
    
    // Weighted combination of all sensory features
    for (const FSensoryChannel& Channel : CurrentState.SensoryChannels)
    {
        float Weight = Channel.AttentionWeight * Channel.Reliability;
        TotalWeight += Weight;
        
        if (Integrated.Num() == 0)
        {
            Integrated.SetNum(Channel.Features.Num());
            for (int32 i = 0; i < Integrated.Num(); ++i)
                Integrated[i] = 0.0f;
        }
        
        for (int32 i = 0; i < FMath::Min(Integrated.Num(), Channel.Features.Num()); ++i)
        {
            Integrated[i] += Weight * Channel.Features[i];
        }
    }
    
    // Normalize by total weight
    if (TotalWeight > 0.0f)
    {
        for (float& Val : Integrated)
        {
            Val /= TotalWeight;
        }
    }
    
    return Integrated;
}

float USensorimotorIntegration::ComputeSensoryPredictionError(FName ChannelID) const
{
    for (const FSensoryChannel& Channel : CurrentState.SensoryChannels)
    {
        if (Channel.ChannelID == ChannelID)
        {
            return Channel.PredictionError;
        }
    }
    return 0.0f;
}

// ========================================
// MOTOR OUTPUT
// ========================================

void USensorimotorIntegration::RegisterMotorChannel(FName ChannelID, EMotorEffector Effector, int32 CommandDimension)
{
    FMotorChannel NewChannel;
    NewChannel.ChannelID = ChannelID;
    NewChannel.Effector = Effector;
    NewChannel.TargetCommand.SetNum(CommandDimension);
    NewChannel.CurrentState.SetNum(CommandDimension);
    NewChannel.CommandGain = 1.0f;
    NewChannel.Confidence = 1.0f;
    
    CurrentState.MotorChannels.Add(NewChannel);
    
    UE_LOG(LogTemp, Log, TEXT("Registered motor channel: %s"), *ChannelID.ToString());
}

void USensorimotorIntegration::SetMotorCommand(FName ChannelID, const TArray<float>& Command, float Gain)
{
    for (FMotorChannel& Channel : CurrentState.MotorChannels)
    {
        if (Channel.ChannelID == ChannelID)
        {
            Channel.TargetCommand = Command;
            Channel.CommandGain = FMath::Clamp(Gain, 0.0f, 1.0f);
            Channel.bIsExecuting = true;
            Channel.Progress = 0.0f;
            break;
        }
    }
}

void USensorimotorIntegration::UpdateMotorFeedback(FName ChannelID, const TArray<float>& CurrentStateData)
{
    for (FMotorChannel& Channel : CurrentState.MotorChannels)
    {
        if (Channel.ChannelID == ChannelID)
        {
            Channel.CurrentState = CurrentStateData;
            
            // Update progress based on distance to target
            if (Channel.bIsExecuting && Channel.TargetCommand.Num() > 0)
            {
                float Distance = 0.0f;
                float MaxDist = 0.0f;
                for (int32 i = 0; i < FMath::Min(CurrentStateData.Num(), Channel.TargetCommand.Num()); ++i)
                {
                    Distance += FMath::Abs(Channel.TargetCommand[i] - CurrentStateData[i]);
                    MaxDist += FMath::Abs(Channel.TargetCommand[i]);
                }
                
                if (MaxDist > 0.0f)
                {
                    Channel.Progress = 1.0f - FMath::Clamp(Distance / MaxDist, 0.0f, 1.0f);
                }
                
                // Check if execution complete
                if (Channel.Progress > 0.95f)
                {
                    Channel.bIsExecuting = false;
                    Channel.Progress = 1.0f;
                }
            }
            break;
        }
    }
}

TMap<FName, TArray<float>> USensorimotorIntegration::GetAllMotorCommands() const
{
    TMap<FName, TArray<float>> Commands;
    for (const FMotorChannel& Channel : CurrentState.MotorChannels)
    {
        if (Channel.bIsExecuting)
        {
            TArray<float> ScaledCommand;
            ScaledCommand.SetNum(Channel.TargetCommand.Num());
            for (int32 i = 0; i < Channel.TargetCommand.Num(); ++i)
            {
                ScaledCommand[i] = Channel.TargetCommand[i] * Channel.CommandGain;
            }
            Commands.Add(Channel.ChannelID, ScaledCommand);
        }
    }
    return Commands;
}

void USensorimotorIntegration::ExecuteWithPrediction(FName ChannelID)
{
    for (FMotorChannel& Channel : CurrentState.MotorChannels)
    {
        if (Channel.ChannelID == ChannelID)
        {
            // Predict sensory consequences
            TArray<float> PredictedConsequence = PredictSensoryConsequence(Channel.TargetCommand);
            
            // Store prediction for later verification
            PredictedMotor.Add(ChannelID, PredictedConsequence);
            
            // Execute command
            Channel.bIsExecuting = true;
            Channel.Progress = 0.0f;
            
            break;
        }
    }
}

// ========================================
// CONTINGENCY LEARNING
// ========================================

int32 USensorimotorIntegration::LearnContingency(const TArray<float>& ActionPattern, const TArray<float>& SensoryChange)
{
    // Check if similar contingency exists
    int32 ExistingID = FindMatchingContingency(ActionPattern);
    if (ExistingID >= 0)
    {
        // Update existing contingency
        FSensorimotorContingency& Existing = Contingencies[ExistingID];
        
        // Blend sensory change with existing
        float Alpha = Config.ContingencyLearningRate;
        for (int32 i = 0; i < FMath::Min(Existing.ExpectedSensoryChange.Num(), SensoryChange.Num()); ++i)
        {
            Existing.ExpectedSensoryChange[i] = (1.0f - Alpha) * Existing.ExpectedSensoryChange[i] + Alpha * SensoryChange[i];
        }
        
        Existing.ActivationCount++;
        Existing.LastActivationTime = AccumulatedTime;
        
        return ExistingID;
    }
    
    // Create new contingency
    if (Contingencies.Num() >= Config.MaxContingencies)
    {
        // Remove least reliable contingency
        int32 WeakestIdx = 0;
        float WeakestReliability = FLT_MAX;
        for (int32 i = 0; i < Contingencies.Num(); ++i)
        {
            if (Contingencies[i].Reliability < WeakestReliability)
            {
                WeakestReliability = Contingencies[i].Reliability;
                WeakestIdx = i;
            }
        }
        Contingencies.RemoveAt(WeakestIdx);
    }
    
    FSensorimotorContingency NewContingency;
    NewContingency.ContingencyID = Contingencies.Num();
    NewContingency.ActionPattern = ActionPattern;
    NewContingency.ExpectedSensoryChange = SensoryChange;
    NewContingency.Reliability = 0.5f;
    NewContingency.ActivationCount = 1;
    NewContingency.LastActivationTime = AccumulatedTime;
    
    Contingencies.Add(NewContingency);
    
    OnContingencyLearned.Broadcast(NewContingency.ContingencyID);
    
    return NewContingency.ContingencyID;
}

void USensorimotorIntegration::UpdateContingencyReliability(int32 ContingencyID, bool bSuccessful)
{
    if (ContingencyID >= 0 && ContingencyID < Contingencies.Num())
    {
        FSensorimotorContingency& Contingency = Contingencies[ContingencyID];
        
        float Delta = bSuccessful ? 0.1f : -0.1f;
        Contingency.Reliability = FMath::Clamp(Contingency.Reliability + Delta, 0.0f, 1.0f);
    }
}

TArray<float> USensorimotorIntegration::PredictSensoryConsequence(const TArray<float>& ActionPattern) const
{
    TArray<float> Prediction;
    float TotalWeight = 0.0f;
    
    // Weighted combination of matching contingencies
    for (const FSensorimotorContingency& Contingency : Contingencies)
    {
        float Similarity = ComputeFeatureSimilarity(ActionPattern, Contingency.ActionPattern);
        if (Similarity > 0.5f)
        {
            float Weight = Similarity * Contingency.Reliability;
            TotalWeight += Weight;
            
            if (Prediction.Num() == 0)
            {
                Prediction.SetNum(Contingency.ExpectedSensoryChange.Num());
                for (float& Val : Prediction) Val = 0.0f;
            }
            
            for (int32 i = 0; i < FMath::Min(Prediction.Num(), Contingency.ExpectedSensoryChange.Num()); ++i)
            {
                Prediction[i] += Weight * Contingency.ExpectedSensoryChange[i];
            }
        }
    }
    
    if (TotalWeight > 0.0f)
    {
        for (float& Val : Prediction)
        {
            Val /= TotalWeight;
        }
    }
    
    return Prediction;
}

int32 USensorimotorIntegration::FindMatchingContingency(const TArray<float>& ActionPattern) const
{
    int32 BestMatch = -1;
    float BestSimilarity = 0.8f; // Threshold for matching
    
    for (int32 i = 0; i < Contingencies.Num(); ++i)
    {
        float Similarity = ComputeFeatureSimilarity(ActionPattern, Contingencies[i].ActionPattern);
        if (Similarity > BestSimilarity)
        {
            BestSimilarity = Similarity;
            BestMatch = i;
        }
    }
    
    return BestMatch;
}

// ========================================
// AFFORDANCE DETECTION
// ========================================

void USensorimotorIntegration::DetectAffordances()
{
    CurrentState.Affordances.Empty();
    
    // Get integrated sensory features
    TArray<float> Features = GetIntegratedSensoryFeatures();
    if (Features.Num() == 0) return;
    
    // Check each contingency for potential affordance
    for (const FSensorimotorContingency& Contingency : Contingencies)
    {
        if (Contingency.Reliability < 0.3f) continue;
        
        // Check if current sensory state affords this action
        // (simplified: check if expected sensory change would be beneficial)
        float Utility = 0.0f;
        for (float Change : Contingency.ExpectedSensoryChange)
        {
            Utility += Change; // Positive changes are beneficial
        }
        
        if (Utility > 0.0f)
        {
            FAffordance NewAffordance;
            NewAffordance.AffordanceID = CurrentState.Affordances.Num();
            NewAffordance.ActionType = FName(*FString::Printf(TEXT("Action_%d"), Contingency.ContingencyID));
            NewAffordance.Strength = Contingency.Reliability * (Utility / Contingency.ExpectedSensoryChange.Num());
            NewAffordance.ExpectedUtility = Utility;
            NewAffordance.bIsReachable = true;
            
            CurrentState.Affordances.Add(NewAffordance);
            
            OnAffordanceDetected.Broadcast(NewAffordance.ActionType, NewAffordance.Strength);
        }
    }
    
    // Sort by strength
    CurrentState.Affordances.Sort([](const FAffordance& A, const FAffordance& B) {
        return A.Strength > B.Strength;
    });
}

TArray<FAffordance> USensorimotorIntegration::GetAffordancesForAction(FName ActionType) const
{
    TArray<FAffordance> Matching;
    for (const FAffordance& Aff : CurrentState.Affordances)
    {
        if (Aff.ActionType == ActionType)
        {
            Matching.Add(Aff);
        }
    }
    return Matching;
}

FAffordance USensorimotorIntegration::GetStrongestAffordance() const
{
    if (CurrentState.Affordances.Num() > 0)
    {
        return CurrentState.Affordances[0];
    }
    return FAffordance();
}

bool USensorimotorIntegration::IsActionAfforded(FName ActionType) const
{
    for (const FAffordance& Aff : CurrentState.Affordances)
    {
        if (Aff.ActionType == ActionType && Aff.Strength > 0.3f)
        {
            return true;
        }
    }
    return false;
}

// ========================================
// BODY SCHEMA
// ========================================

void USensorimotorIntegration::UpdateBodyPartPosition(FName PartName, FVector Position, FRotator Orientation)
{
    CurrentState.BodySchema.PartPositions.Add(PartName, Position);
    CurrentState.BodySchema.PartOrientations.Add(PartName, Orientation);
    
    // Update reachable space based on part positions
    if (CurrentState.BodySchema.PartPositions.Num() > 0)
    {
        FVector Min(FLT_MAX, FLT_MAX, FLT_MAX);
        FVector Max(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        
        for (const auto& Pair : CurrentState.BodySchema.PartPositions)
        {
            Min = Min.ComponentMin(Pair.Value - FVector(50, 50, 50));
            Max = Max.ComponentMax(Pair.Value + FVector(50, 50, 50));
        }
        
        CurrentState.BodySchema.ReachableSpace = FBox(Min, Max);
    }
}

void USensorimotorIntegration::ExtendBodySchema(FName ToolName, FVector Extension)
{
    CurrentState.BodySchema.CurrentTool = ToolName;
    CurrentState.BodySchema.ToolExtension = Extension;
    CurrentState.BodySchema.State = EBodySchemaState::Extended;
    
    // Expand reachable space
    CurrentState.BodySchema.ReachableSpace = CurrentState.BodySchema.ReachableSpace.ExpandBy(Extension.Size());
    
    OnBodySchemaAdapted.Broadcast(EBodySchemaState::Extended);
}

void USensorimotorIntegration::ResetBodySchema()
{
    CurrentState.BodySchema.CurrentTool = NAME_None;
    CurrentState.BodySchema.ToolExtension = FVector::ZeroVector;
    CurrentState.BodySchema.State = EBodySchemaState::Normal;
    
    OnBodySchemaAdapted.Broadcast(EBodySchemaState::Normal);
}

bool USensorimotorIntegration::IsPositionReachable(FVector Position) const
{
    return CurrentState.BodySchema.ReachableSpace.IsInside(Position);
}

float USensorimotorIntegration::GetEffectorCapability(EMotorEffector Effector) const
{
    if (const float* Capability = CurrentState.BodySchema.EffectorCapabilities.Find(Effector))
    {
        return *Capability;
    }
    return 0.0f;
}

// ========================================
// COGNITIVE CYCLE INTEGRATION
// ========================================

void USensorimotorIntegration::ProcessCognitiveStep(int32 GlobalStep, EConsciousnessStream ActiveStream)
{
    int32 StepInCycle = ((GlobalStep - 1) % 12) + 1;
    
    switch (ActiveStream)
    {
    case EConsciousnessStream::Perception:
        // Steps 1-4: Sensory processing
        if (StepInCycle == 1)
        {
            // Compute salience map
            TArray<float> Salience = GetSalienceMap();
            
            // Update attention based on salience
            for (int32 i = 0; i < CurrentState.SensoryChannels.Num(); ++i)
            {
                if (i < Salience.Num())
                {
                    CurrentState.SensoryChannels[i].AttentionWeight = 
                        0.7f * CurrentState.SensoryChannels[i].AttentionWeight + 0.3f * Salience[i];
                }
            }
        }
        else if (StepInCycle == 4)
        {
            // Integrate sensory features
            TArray<float> Integrated = GetIntegratedSensoryFeatures();
            
            // Feed to AXIOM if available
            if (AXIOMComponent)
            {
                // AXIOM will process this in its perception step
            }
        }
        break;
        
    case EConsciousnessStream::Action:
        // Steps 5-8: Motor processing
        if (StepInCycle == 5)
        {
            // Check action readiness
            float Readiness = GetActionReadiness();
            
            // Update coupling mode based on readiness
            if (Readiness > 0.8f)
            {
                CurrentState.CouplingMode = ECouplingMode::Habitual;
            }
            else if (Readiness > 0.5f)
            {
                CurrentState.CouplingMode = ECouplingMode::Predictive;
            }
            else
            {
                CurrentState.CouplingMode = ECouplingMode::Reactive;
            }
        }
        else if (StepInCycle == 8)
        {
            // Execute motor commands with prediction
            for (FMotorChannel& Channel : CurrentState.MotorChannels)
            {
                if (Channel.bIsExecuting)
                {
                    ExecuteWithPrediction(Channel.ChannelID);
                }
            }
        }
        break;
        
    case EConsciousnessStream::Simulation:
        // Steps 9-12: Prediction and learning
        if (StepInCycle == 9)
        {
            // Process sensory predictions
            ProcessSensoryPrediction();
        }
        else if (StepInCycle == 12)
        {
            // Learn from prediction errors
            for (const FSensoryChannel& Channel : CurrentState.SensoryChannels)
            {
                if (Channel.PredictionError > Config.AdaptationThreshold)
                {
                    // Trigger contingency learning
                    // (actual learning happens when motor command completes)
                }
            }
        }
        break;
    }
}

TArray<float> USensorimotorIntegration::GetSalienceMap() const
{
    TArray<float> Salience;
    Salience.SetNum(CurrentState.SensoryChannels.Num());
    
    for (int32 i = 0; i < CurrentState.SensoryChannels.Num(); ++i)
    {
        const FSensoryChannel& Channel = CurrentState.SensoryChannels[i];
        
        // Salience based on prediction error and reliability
        float ErrorSalience = Channel.PredictionError;
        float ReliabilitySalience = 1.0f - Channel.Reliability;
        
        // Combine factors
        Salience[i] = 0.7f * ErrorSalience + 0.3f * ReliabilitySalience;
        Salience[i] = FMath::Clamp(Salience[i], 0.0f, 1.0f);
    }
    
    return Salience;
}

float USensorimotorIntegration::GetActionReadiness() const
{
    float Readiness = 0.0f;
    int32 Count = 0;
    
    for (const FMotorChannel& Channel : CurrentState.MotorChannels)
    {
        // Readiness based on confidence and capability
        float EffectorCap = GetEffectorCapability(Channel.Effector);
        Readiness += Channel.Confidence * EffectorCap;
        Count++;
    }
    
    if (Count > 0)
    {
        Readiness /= Count;
    }
    
    // Also factor in overall coherence
    Readiness *= CurrentState.Coherence;
    
    return FMath::Clamp(Readiness, 0.0f, 1.0f);
}

// ========================================
// INTERNAL METHODS
// ========================================

void USensorimotorIntegration::ProcessSensoryPrediction()
{
    // Generate predictions for next sensory state based on current motor commands
    for (const FMotorChannel& Channel : CurrentState.MotorChannels)
    {
        if (Channel.bIsExecuting)
        {
            TArray<float> Predicted = PredictSensoryConsequence(Channel.TargetCommand);
            if (Predicted.Num() > 0)
            {
                // Store prediction for verification
                PredictedSensory.Add(Channel.ChannelID, Predicted);
            }
        }
    }
}

void USensorimotorIntegration::ProcessMotorPrediction()
{
    // Predict motor state evolution
    for (FMotorChannel& Channel : CurrentState.MotorChannels)
    {
        if (Channel.bIsExecuting)
        {
            // Simple linear prediction toward target
            TArray<float> Predicted;
            Predicted.SetNum(Channel.CurrentState.Num());
            
            float Alpha = 0.1f; // Prediction step
            for (int32 i = 0; i < Channel.CurrentState.Num(); ++i)
            {
                Predicted[i] = Channel.CurrentState[i] + Alpha * (Channel.TargetCommand[i] - Channel.CurrentState[i]);
            }
            
            PredictedMotor.Add(Channel.ChannelID, Predicted);
        }
    }
}

void USensorimotorIntegration::UpdateAttention()
{
    // Decay attention over time
    for (FSensoryChannel& Channel : CurrentState.SensoryChannels)
    {
        // Increase attention for channels with high prediction error
        if (Channel.PredictionError > Config.AdaptationThreshold)
        {
            Channel.AttentionWeight = FMath::Min(1.0f, Channel.AttentionWeight + 0.1f);
        }
        else
        {
            // Decay toward baseline
            Channel.AttentionWeight = FMath::Max(0.1f, Channel.AttentionWeight - Config.AttentionDecay * 0.01f);
        }
    }
}

void USensorimotorIntegration::ConsolidateContingencies()
{
    // Merge similar contingencies
    for (int32 i = 0; i < Contingencies.Num(); ++i)
    {
        for (int32 j = i + 1; j < Contingencies.Num(); ++j)
        {
            float Similarity = ComputeFeatureSimilarity(Contingencies[i].ActionPattern, Contingencies[j].ActionPattern);
            if (Similarity > 0.9f)
            {
                // Merge j into i
                float Wi = Contingencies[i].ActivationCount;
                float Wj = Contingencies[j].ActivationCount;
                float Total = Wi + Wj;
                
                for (int32 k = 0; k < Contingencies[i].ExpectedSensoryChange.Num(); ++k)
                {
                    Contingencies[i].ExpectedSensoryChange[k] = 
                        (Wi * Contingencies[i].ExpectedSensoryChange[k] + Wj * Contingencies[j].ExpectedSensoryChange[k]) / Total;
                }
                
                Contingencies[i].Reliability = (Wi * Contingencies[i].Reliability + Wj * Contingencies[j].Reliability) / Total;
                Contingencies[i].ActivationCount = Total;
                
                Contingencies.RemoveAt(j);
                j--;
            }
        }
    }
    
    // Prune weak contingencies
    Contingencies.RemoveAll([](const FSensorimotorContingency& C) {
        return C.Reliability < 0.1f && C.ActivationCount < 5;
    });
}

void USensorimotorIntegration::AdaptBodySchema()
{
    // Compute overall prediction error
    float TotalError = 0.0f;
    int32 Count = 0;
    
    for (const FSensoryChannel& Channel : CurrentState.SensoryChannels)
    {
        if (Channel.Modality == ESensoryModality::Proprioceptive)
        {
            TotalError += Channel.PredictionError;
            Count++;
        }
    }
    
    if (Count > 0)
    {
        TotalError /= Count;
    }
    
    // Adapt body schema state based on error
    if (TotalError > Config.AdaptationThreshold)
    {
        if (CurrentState.BodySchema.State != EBodySchemaState::Adapting)
        {
            CurrentState.BodySchema.State = EBodySchemaState::Adapting;
            OnBodySchemaAdapted.Broadcast(EBodySchemaState::Adapting);
        }
    }
    else if (CurrentState.BodySchema.State == EBodySchemaState::Adapting)
    {
        CurrentState.BodySchema.State = EBodySchemaState::Normal;
        OnBodySchemaAdapted.Broadcast(EBodySchemaState::Normal);
    }
    
    // Update coherence
    CurrentState.Coherence = 1.0f - FMath::Clamp(TotalError, 0.0f, 1.0f);
}

float USensorimotorIntegration::ComputeFeatureSimilarity(const TArray<float>& A, const TArray<float>& B) const
{
    if (A.Num() == 0 || B.Num() == 0) return 0.0f;
    
    float DotProduct = 0.0f;
    float NormA = 0.0f;
    float NormB = 0.0f;
    
    int32 MinLen = FMath::Min(A.Num(), B.Num());
    for (int32 i = 0; i < MinLen; ++i)
    {
        DotProduct += A[i] * B[i];
        NormA += A[i] * A[i];
        NormB += B[i] * B[i];
    }
    
    float Denom = FMath::Sqrt(NormA) * FMath::Sqrt(NormB);
    if (Denom < 0.0001f) return 0.0f;
    
    return FMath::Clamp(DotProduct / Denom, 0.0f, 1.0f);
}

TArray<float> USensorimotorIntegration::NormalizeFeatures(const TArray<float>& Features) const
{
    TArray<float> Normalized = Features;
    
    float Sum = 0.0f;
    for (float Val : Features)
    {
        Sum += Val * Val;
    }
    
    float Norm = FMath::Sqrt(Sum);
    if (Norm > 0.0001f)
    {
        for (float& Val : Normalized)
        {
            Val /= Norm;
        }
    }
    
    return Normalized;
}

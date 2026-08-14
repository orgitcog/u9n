/**
 * @file CognitiveCycleManagerEnhanced.cpp
 * @brief Enhanced implementation of the 12-step cognitive cycle with full
 *        echobeats integration, OEIS A000081 nested shells, and 4E embodied cognition
 * 
 * This implementation provides the complete cognitive loop architecture:
 * - 12-step cycle (7 expressive + 5 reflective)
 * - 3 concurrent streams phased 4 steps apart (120 degrees)
 * - Triadic synchronization points at steps 4, 8, 12
 * - OEIS A000081 nested shell structure (1->1, 2->2, 3->4, 4->9 terms)
 * - Integration with 4E embodied cognition framework
 */

#include "CognitiveCycleManager.h"
#include "Math/UnrealMathUtility.h"

// ============================================================================
// Step Configuration Matrix
// ============================================================================

namespace CognitiveConstants
{
    // Step type configuration for the 12-step cycle
    // Step: 1=Pivotal, 2-6=Affordance, 7=Pivotal, 8-12=Salience
    constexpr int StepTypes[12] = {
        0,  // Step 1: Pivotal (Relevance Realization - Present Commitment)
        1,  // Step 2: Affordance (Past Conditioning)
        1,  // Step 3: Affordance
        1,  // Step 4: Affordance
        1,  // Step 5: Affordance
        1,  // Step 6: Affordance
        0,  // Step 7: Pivotal (Relevance Realization - Present Commitment)
        2,  // Step 8: Salience (Future Anticipation)
        2,  // Step 9: Salience
        2,  // Step 10: Salience
        2,  // Step 11: Salience
        2   // Step 12: Salience
    };
    
    // Mode configuration: 0=Expressive, 1=Reflective
    // 7 Expressive (1,2,4,6,7,8,9) + 5 Reflective (3,5,10,11,12)
    constexpr int StepModes[12] = {
        0,  // Step 1: Expressive
        0,  // Step 2: Expressive
        1,  // Step 3: Reflective
        0,  // Step 4: Expressive
        1,  // Step 5: Reflective
        0,  // Step 6: Expressive
        0,  // Step 7: Expressive
        0,  // Step 8: Expressive
        0,  // Step 9: Expressive
        1,  // Step 10: Reflective
        1,  // Step 11: Reflective
        1   // Step 12: Reflective
    };
    
    // Triadic group assignments: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
    constexpr int TriadicGroups[12] = {
        0, 1, 2, 3,  // Steps 1-4
        0, 1, 2, 3,  // Steps 5-8
        0, 1, 2, 3   // Steps 9-12
    };
    
    // Primary stream at each step (0=Pivotal, 1=Affordance, 2=Salience)
    // Streams are phased 4 steps apart
    constexpr int PrimaryStreams[12] = {
        0, 1, 2, 0,  // Steps 1-4
        0, 1, 2, 0,  // Steps 5-8
        0, 1, 2, 0   // Steps 9-12
    };
    
    // OEIS A000081 nested shell term counts
    // Level 1: 1 term, Level 2: 2 terms, Level 3: 4 terms, Level 4: 9 terms
    constexpr int ShellTermCounts[4] = {1, 2, 4, 9};
    
    // Reservoir coupling strengths per step type
    constexpr float PivotalCoupling = 0.9f;
    constexpr float AffordanceCoupling = 0.7f;
    constexpr float SalienceCoupling = 0.5f;
}

// ============================================================================
// UCognitiveCycleManager Implementation
// ============================================================================

UCognitiveCycleManager::UCognitiveCycleManager()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UCognitiveCycleManager::BeginPlay()
{
    Super::BeginPlay();
    
    InitializeStepConfigurations();
    InitializeNestedShells();
    InitializeStreamReservoirs();
    
    // Find and bind to related components
    FindComponentReferences();
    
    bIsInitialized = true;
}

void UCognitiveCycleManager::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    if (!bEnableCognitiveCycle || !bIsInitialized)
    {
        return;
    }
    
    // Accumulate time
    CycleAccumulator += DeltaTime;
    float StepDuration = CycleDuration / 12.0f;
    
    // Check for step advancement
    while (CycleAccumulator >= StepDuration)
    {
        CycleAccumulator -= StepDuration;
        AdvanceStep();
    }
    
    // Process current step
    ProcessCurrentStep(DeltaTime);
    
    // Update stream synchronization
    UpdateStreamSynchronization();
    
    // Propagate through nested shells
    PropagateNestedShells();
    
    // Integrate with 4E cognition if available
    if (Embodied4ECognition)
    {
        Integrate4ECognition();
    }
}

void UCognitiveCycleManager::InitializeStepConfigurations()
{
    StepConfigs.SetNum(12);
    
    for (int32 i = 0; i < 12; ++i)
    {
        FEchobeatStepConfig& Config = StepConfigs[i];
        Config.StepNumber = i + 1;
        
        // Set step type
        switch (CognitiveConstants::StepTypes[i])
        {
            case 0: Config.StepType = EEchobeatStepType::Pivotal; break;
            case 1: Config.StepType = EEchobeatStepType::Affordance; break;
            case 2: Config.StepType = EEchobeatStepType::Salience; break;
        }
        
        // Set mode
        Config.Mode = CognitiveConstants::StepModes[i] == 0 
            ? EEchobeatMode::Expressive 
            : EEchobeatMode::Reflective;
        
        // Set primary stream
        switch (CognitiveConstants::PrimaryStreams[i])
        {
            case 0: Config.PrimaryStream = ECognitiveStreamType::Pivotal; break;
            case 1: Config.PrimaryStream = ECognitiveStreamType::Affordance; break;
            case 2: Config.PrimaryStream = ECognitiveStreamType::Salience; break;
        }
        
        // Set triadic group
        Config.TriadicGroup = CognitiveConstants::TriadicGroups[i];
        
        // Set reservoir coupling based on step type
        switch (Config.StepType)
        {
            case EEchobeatStepType::Pivotal:
                Config.ReservoirCoupling = CognitiveConstants::PivotalCoupling;
                break;
            case EEchobeatStepType::Affordance:
                Config.ReservoirCoupling = CognitiveConstants::AffordanceCoupling;
                break;
            case EEchobeatStepType::Salience:
                Config.ReservoirCoupling = CognitiveConstants::SalienceCoupling;
                break;
        }
        
        // Determine active shell level based on step position in cycle
        // Outer shells active at cycle boundaries, inner shells active mid-cycle
        if (i == 0 || i == 6)
        {
            Config.ActiveShellLevel = 1;  // Root shell at pivotal points
        }
        else if (i == 3 || i == 9)
        {
            Config.ActiveShellLevel = 2;  // Second shell at triadic sync points
        }
        else if (i == 2 || i == 5 || i == 8 || i == 11)
        {
            Config.ActiveShellLevel = 3;  // Third shell
        }
        else
        {
            Config.ActiveShellLevel = 4;  // Deepest shell for detailed processing
        }
    }
}

void UCognitiveCycleManager::InitializeNestedShells()
{
    NestedShellTerms.Empty();
    
    int32 TermID = 0;
    
    // Create terms for each shell level following OEIS A000081
    for (int32 Level = 1; Level <= 4; ++Level)
    {
        int32 TermCount = CognitiveConstants::ShellTermCounts[Level - 1];
        
        for (int32 t = 0; t < TermCount; ++t)
        {
            FNestedShellTerm Term;
            Term.TermID = TermID;
            Term.ShellLevel = Level;
            Term.ActivationLevel = 0.5f;
            
            // Set parent relationships
            if (Level == 1)
            {
                Term.ParentTermID = -1;  // Root has no parent
            }
            else
            {
                // Parent is in the previous level
                // Use modular distribution for balanced tree structure
                int32 ParentLevelStart = 0;
                for (int32 l = 1; l < Level; ++l)
                {
                    ParentLevelStart += CognitiveConstants::ShellTermCounts[l - 1];
                }
                int32 ParentCount = CognitiveConstants::ShellTermCounts[Level - 2];
                Term.ParentTermID = ParentLevelStart - ParentCount + (t % ParentCount);
            }
            
            // Initialize reservoir state for this term
            Term.ReservoirState.ReservoirSize = UnitsPerShellLevel * Level;
            Term.ReservoirState.SpectralRadius = 0.95f - (Level - 1) * 0.05f;
            Term.ReservoirState.LeakRate = 0.1f + (Level - 1) * 0.1f;
            
            NestedShellTerms.Add(Term);
            ++TermID;
        }
    }
    
    // Build child relationships
    for (FNestedShellTerm& Term : NestedShellTerms)
    {
        if (Term.ParentTermID >= 0)
        {
            NestedShellTerms[Term.ParentTermID].ChildTermIDs.Add(Term.TermID);
        }
    }
}

void UCognitiveCycleManager::InitializeStreamReservoirs()
{
    // Initialize 3 concurrent stream reservoirs
    StreamStates.SetNum(3);
    
    for (int32 i = 0; i < 3; ++i)
    {
        FCognitiveStreamState& Stream = StreamStates[i];
        
        switch (i)
        {
            case 0:
                Stream.StreamType = ECognitiveStreamType::Pivotal;
                Stream.ReservoirState.SpectralRadius = 0.95f;
                Stream.ReservoirState.LeakRate = 0.2f;
                break;
            case 1:
                Stream.StreamType = ECognitiveStreamType::Affordance;
                Stream.ReservoirState.SpectralRadius = 0.9f;
                Stream.ReservoirState.LeakRate = 0.3f;
                break;
            case 2:
                Stream.StreamType = ECognitiveStreamType::Salience;
                Stream.ReservoirState.SpectralRadius = 0.85f;
                Stream.ReservoirState.LeakRate = 0.4f;
                break;
        }
        
        Stream.ReservoirState.ReservoirSize = UnitsPerStream;
        Stream.CurrentPhase = i * 4;  // Phased 4 steps apart (120 degrees)
        Stream.ActivationLevel = 0.5f;
    }
    
    // Initialize synchronization state
    SyncState.CoherenceMatrix.SetNum(9);  // 3x3 matrix
    for (int32 i = 0; i < 9; ++i)
    {
        SyncState.CoherenceMatrix[i] = (i % 4 == 0) ? 1.0f : 0.5f;  // Identity + coupling
    }
    
    SyncState.PhaseAlignmentScores.SetNum(3);
    for (int32 i = 0; i < 3; ++i)
    {
        SyncState.PhaseAlignmentScores[i] = 1.0f;
    }
}

void UCognitiveCycleManager::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        Embodied4ECognition = Owner->FindComponentByClass<UEmbodied4ECognition>();
        TetradicIntegration = Owner->FindComponentByClass<UTetradicReservoirIntegration>();
        DeepCognitiveBridge = Owner->FindComponentByClass<UDeepCognitiveBridge>();
        
        // Bind to 4E cognition events if available
        if (Embodied4ECognition)
        {
            Embodied4ECognition->OnDimensionActivated.AddDynamic(
                this, &UCognitiveCycleManager::Handle4EDimensionActivated);
        }
    }
}

void UCognitiveCycleManager::AdvanceStep()
{
    int32 PreviousStep = CurrentStep;
    CurrentStep = (CurrentStep % 12) + 1;
    
    const FEchobeatStepConfig& Config = StepConfigs[CurrentStep - 1];
    
    // Broadcast step change
    OnStepChanged.Broadcast(CurrentStep, Config.StepType, Config.Mode);
    
    // Check for triadic synchronization point
    if (Config.TriadicGroup == 3)  // Steps 4, 8, 12
    {
        PerformTriadicSynchronization();
    }
    
    // Check for relevance realization at pivotal steps
    if (Config.StepType == EEchobeatStepType::Pivotal)
    {
        float Relevance = ComputeRelevanceRealization();
        if (Relevance >= RelevanceThreshold)
        {
            OnRelevanceRealized.Broadcast(CurrentStep, Relevance);
        }
    }
    
    // Update stream phases
    for (FCognitiveStreamState& Stream : StreamStates)
    {
        Stream.CurrentPhase = (Stream.CurrentPhase + 1) % 12;
    }
}

void UCognitiveCycleManager::ProcessCurrentStep(float DeltaTime)
{
    const FEchobeatStepConfig& Config = StepConfigs[CurrentStep - 1];
    
    // Get sensory input (would come from avatar/environment)
    TArray<float> SensoryInput = GatherSensoryInput();
    
    // Process through primary stream
    int32 PrimaryStreamIndex = static_cast<int32>(Config.PrimaryStream);
    if (PrimaryStreamIndex < StreamStates.Num())
    {
        UpdateStreamReservoir(StreamStates[PrimaryStreamIndex], SensoryInput, DeltaTime);
    }
    
    // Process through active shell level
    ProcessShellLevel(Config.ActiveShellLevel, SensoryInput, DeltaTime);
    
    // Apply mode-specific processing
    if (Config.Mode == EEchobeatMode::Expressive)
    {
        ProcessExpressiveMode(DeltaTime);
    }
    else
    {
        ProcessReflectiveMode(DeltaTime);
    }
}

void UCognitiveCycleManager::UpdateStreamReservoir(FCognitiveStreamState& Stream,
                                                    const TArray<float>& Input,
                                                    float DeltaTime)
{
    // Simple leaky integrator reservoir update
    // In full implementation, this would use proper ESN dynamics
    
    float LeakRate = Stream.ReservoirState.LeakRate;
    
    // Compute input influence
    float InputMagnitude = 0.0f;
    for (float Val : Input)
    {
        InputMagnitude += Val * Val;
    }
    InputMagnitude = FMath::Sqrt(InputMagnitude);
    
    // Update activation with leaky integration
    Stream.ActivationLevel = (1.0f - LeakRate) * Stream.ActivationLevel +
                             LeakRate * FMath::Tanh(InputMagnitude);
    
    // Update coherence
    Stream.Coherence = FMath::Lerp(Stream.Coherence, Stream.ActivationLevel, 0.1f);
}

void UCognitiveCycleManager::ProcessShellLevel(int32 Level, const TArray<float>& Input, float DeltaTime)
{
    // Find terms at this shell level
    for (FNestedShellTerm& Term : NestedShellTerms)
    {
        if (Term.ShellLevel == Level)
        {
            // Update term activation
            float InputInfluence = 0.0f;
            for (float Val : Input)
            {
                InputInfluence += Val;
            }
            InputInfluence = InputInfluence / FMath::Max(1.0f, static_cast<float>(Input.Num()));
            
            Term.ActivationLevel = FMath::Lerp(Term.ActivationLevel, 
                                                FMath::Tanh(InputInfluence), 
                                                Term.ReservoirState.LeakRate);
        }
    }
}

void UCognitiveCycleManager::UpdateStreamSynchronization()
{
    // Compute inter-stream coherence
    for (int32 i = 0; i < 3; ++i)
    {
        for (int32 j = 0; j < 3; ++j)
        {
            if (i != j)
            {
                // Compute coherence between streams i and j
                float Coherence = FMath::Abs(StreamStates[i].ActivationLevel - 
                                             StreamStates[j].ActivationLevel);
                Coherence = 1.0f - Coherence;  // Higher similarity = higher coherence
                
                SyncState.CoherenceMatrix[i * 3 + j] = FMath::Lerp(
                    SyncState.CoherenceMatrix[i * 3 + j], Coherence, 0.1f);
            }
        }
    }
    
    // Compute overall sync quality
    float TotalCoherence = 0.0f;
    for (int32 i = 0; i < 9; ++i)
    {
        TotalCoherence += SyncState.CoherenceMatrix[i];
    }
    SyncState.SyncQuality = TotalCoherence / 9.0f;
}

void UCognitiveCycleManager::PropagateNestedShells()
{
    // Outer-to-inner propagation: parent states influence children
    for (FNestedShellTerm& Term : NestedShellTerms)
    {
        if (Term.ParentTermID >= 0)
        {
            const FNestedShellTerm& Parent = NestedShellTerms[Term.ParentTermID];
            Term.ActivationLevel = FMath::Lerp(Term.ActivationLevel, 
                                                Parent.ActivationLevel, 
                                                0.1f);
        }
    }
    
    // Inner-to-outer propagation: child averages feedback to parents
    for (int32 Level = 4; Level >= 1; --Level)
    {
        for (FNestedShellTerm& Term : NestedShellTerms)
        {
            if (Term.ShellLevel == Level && Term.ChildTermIDs.Num() > 0)
            {
                float ChildAverage = 0.0f;
                for (int32 ChildID : Term.ChildTermIDs)
                {
                    ChildAverage += NestedShellTerms[ChildID].ActivationLevel;
                }
                ChildAverage /= Term.ChildTermIDs.Num();
                
                Term.ActivationLevel = FMath::Lerp(Term.ActivationLevel, 
                                                    ChildAverage, 
                                                    0.05f);
            }
        }
    }
}

void UCognitiveCycleManager::PerformTriadicSynchronization()
{
    // Synchronize all three streams at triadic points
    float MeanActivation = 0.0f;
    for (const FCognitiveStreamState& Stream : StreamStates)
    {
        MeanActivation += Stream.ActivationLevel;
    }
    MeanActivation /= 3.0f;
    
    // Pull streams toward mean
    for (FCognitiveStreamState& Stream : StreamStates)
    {
        Stream.ActivationLevel = FMath::Lerp(Stream.ActivationLevel, 
                                              MeanActivation, 
                                              InterStreamCoupling);
    }
    
    SyncState.LastSyncTime = GetWorld()->GetTimeSeconds();
    SyncState.SyncPhase = (SyncState.SyncPhase + 1) % 4;
    
    OnTriadicSync.Broadcast(SyncState.SyncPhase, SyncState.SyncQuality);
}

float UCognitiveCycleManager::ComputeRelevanceRealization()
{
    // Compute relevance based on stream coherence and activation patterns
    float Relevance = 0.0f;
    
    // Factor 1: Stream synchronization quality
    Relevance += SyncState.SyncQuality * 0.3f;
    
    // Factor 2: Activation level of pivotal stream
    Relevance += StreamStates[0].ActivationLevel * 0.3f;
    
    // Factor 3: Nested shell coherence (root to leaf consistency)
    float ShellCoherence = 0.0f;
    if (NestedShellTerms.Num() > 0)
    {
        float RootActivation = NestedShellTerms[0].ActivationLevel;
        for (const FNestedShellTerm& Term : NestedShellTerms)
        {
            ShellCoherence += 1.0f - FMath::Abs(Term.ActivationLevel - RootActivation);
        }
        ShellCoherence /= NestedShellTerms.Num();
    }
    Relevance += ShellCoherence * 0.4f;
    
    return FMath::Clamp(Relevance, 0.0f, 1.0f);
}

void UCognitiveCycleManager::Integrate4ECognition()
{
    if (!Embodied4ECognition)
    {
        return;
    }
    
    // Get 4E state
    F4ECognitionState E4State = Embodied4ECognition->GetCurrentState();
    
    // Modulate stream processing based on 4E coherence
    float E4Coherence = Embodied4ECognition->GetCoherence();
    
    // Embodied dimension affects affordance stream
    StreamStates[1].ActivationLevel = FMath::Lerp(
        StreamStates[1].ActivationLevel,
        E4State.EmbodiedActivation,
        0.1f);
    
    // Extended dimension affects salience stream
    StreamStates[2].ActivationLevel = FMath::Lerp(
        StreamStates[2].ActivationLevel,
        E4State.ExtendedActivation,
        0.1f);
    
    // Overall coherence affects pivotal stream
    StreamStates[0].ActivationLevel = FMath::Lerp(
        StreamStates[0].ActivationLevel,
        E4Coherence,
        0.1f);
}

void UCognitiveCycleManager::ProcessExpressiveMode(float DeltaTime)
{
    // Expressive mode: reactive, action-oriented processing
    // Emphasize motor output and immediate response
    
    for (FCognitiveStreamState& Stream : StreamStates)
    {
        // Increase responsiveness in expressive mode
        Stream.ActivationLevel *= 1.0f + 0.1f * DeltaTime;
        Stream.ActivationLevel = FMath::Clamp(Stream.ActivationLevel, 0.0f, 1.0f);
    }
}

void UCognitiveCycleManager::ProcessReflectiveMode(float DeltaTime)
{
    // Reflective mode: anticipatory, simulation-oriented processing
    // Emphasize internal modeling and prediction
    
    for (FCognitiveStreamState& Stream : StreamStates)
    {
        // Smooth and stabilize in reflective mode
        Stream.ActivationLevel = FMath::Lerp(Stream.ActivationLevel, 0.5f, 0.05f * DeltaTime);
    }
}

TArray<float> UCognitiveCycleManager::GatherSensoryInput()
{
    TArray<float> Input;
    Input.SetNum(10);
    
    // In full implementation, this would gather actual sensory data
    // For now, return normalized placeholder values
    for (int32 i = 0; i < 10; ++i)
    {
        Input[i] = FMath::Sin(GetWorld()->GetTimeSeconds() + i * 0.5f) * 0.5f + 0.5f;
    }
    
    return Input;
}

void UCognitiveCycleManager::Handle4EDimensionActivated(E4EDimension Dimension, float Activation)
{
    // Respond to 4E dimension activation changes
    switch (Dimension)
    {
        case E4EDimension::Embodied:
            // Embodied activation affects body schema processing
            break;
        case E4EDimension::Embedded:
            // Embedded activation affects environmental coupling
            break;
        case E4EDimension::Enacted:
            // Enacted activation affects sensorimotor contingencies
            break;
        case E4EDimension::Extended:
            // Extended activation affects tool integration
            break;
    }
}

// ============================================================================
// Blueprint Callable Functions
// ============================================================================

int32 UCognitiveCycleManager::GetCurrentStep() const
{
    return CurrentStep;
}

FEchobeatStepConfig UCognitiveCycleManager::GetStepConfig(int32 Step) const
{
    if (Step >= 1 && Step <= 12)
    {
        return StepConfigs[Step - 1];
    }
    return FEchobeatStepConfig();
}

float UCognitiveCycleManager::GetStreamActivation(ECognitiveStreamType StreamType) const
{
    int32 Index = static_cast<int32>(StreamType);
    if (Index < StreamStates.Num())
    {
        return StreamStates[Index].ActivationLevel;
    }
    return 0.0f;
}

float UCognitiveCycleManager::GetSyncQuality() const
{
    return SyncState.SyncQuality;
}

TArray<float> UCognitiveCycleManager::GetNestedShellActivations() const
{
    TArray<float> Activations;
    for (const FNestedShellTerm& Term : NestedShellTerms)
    {
        Activations.Add(Term.ActivationLevel);
    }
    return Activations;
}

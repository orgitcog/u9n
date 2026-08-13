// CosmicOrderSystem.cpp - System 5 CNS-ORG Integration Implementation
// Implements Robert Campbell's Cosmic Order framework with 5-Cell Pentachoron geometry

#include "CosmicOrderSystem.h"

UCosmicOrderSystem::UCosmicOrderSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    
    // Initialize step timing
    StepAccumulator = 0.0f;
    StepInterval = 0.1f; // 100ms per step = 1.2s per full cycle
    
    GlobalCycleStep = 1;
    TotalCyclesCompleted = 0;
    
    // Initialize wisdom dimensions
    WisdomUnderstanding = 0.1f;
    WisdomPerspective = 0.1f;
    WisdomIntegration = 0.1f;
    WisdomReflection = 0.1f;
    WisdomCompassion = 0.1f;
    WisdomEquanimity = 0.1f;
    WisdomTranscendence = 0.1f;
}

void UCosmicOrderSystem::BeginPlay()
{
    Super::BeginPlay();
    
    InitializePentachoron();
    InitializeConsciousnessStreams();
}

void UCosmicOrderSystem::TickComponent(float DeltaTime, ELevelTick TickType, 
                                        FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // Accumulate time for step advancement
    StepAccumulator += DeltaTime;
    
    if (StepAccumulator >= StepInterval)
    {
        StepAccumulator -= StepInterval;
        AdvanceCognitiveLoop();
    }
    
    // Process polarities
    ProcessSomaticPolarity(DeltaTime);
    ProcessSympatheticPolarity(DeltaTime);
    ProcessParasympatheticPolarity(DeltaTime);
    
    // Propagate energy through pentachoron
    PropagateEnergy(DeltaTime);
    
    // Update triadic states
    UpdateTriadicStates();
    
    // Update wisdom dimensions
    UpdateWisdomDimensions(DeltaTime);
}

void UCosmicOrderSystem::InitializePentachoron()
{
    TetrahedralCells.Empty();
    
    // Cell 1: Cerebral-Somatic-Autonomic-Integration (A-B-C-E)
    // The Main Cognitive Triad
    {
        FTetrahedralCell Cell1;
        Cell1.CellName = TEXT("MainCognitiveTriad");
        Cell1.Vertices = {
            EPentachoronVertex::Cerebral,
            EPentachoronVertex::Somatic,
            EPentachoronVertex::Autonomic,
            EPentachoronVertex::Integration
        };
        Cell1.CellEnergy = 1.0f;
        Cell1.FunctionalRole = TEXT("Primary cognitive processing - 3 concurrent streams");
        TetrahedralCells.Add(Cell1);
    }
    
    // Cell 2: Cerebral-Somatic-Spinal-Integration (A-B-D-E)
    // The Somatic Polarity Cell
    {
        FTetrahedralCell Cell2;
        Cell2.CellName = TEXT("SomaticPolarityCell");
        Cell2.Vertices = {
            EPentachoronVertex::Cerebral,
            EPentachoronVertex::Somatic,
            EPentachoronVertex::Spinal,
            EPentachoronVertex::Integration
        };
        Cell2.CellEnergy = 1.0f;
        Cell2.FunctionalRole = TEXT("Behavior Technique / Commitment");
        TetrahedralCells.Add(Cell2);
    }
    
    // Cell 3: Cerebral-Autonomic-Spinal-Integration (A-C-D-E)
    // The Parasympathetic Polarity Cell
    {
        FTetrahedralCell Cell3;
        Cell3.CellName = TEXT("ParasympatheticPolarityCell");
        Cell3.Vertices = {
            EPentachoronVertex::Cerebral,
            EPentachoronVertex::Autonomic,
            EPentachoronVertex::Spinal,
            EPentachoronVertex::Integration
        };
        Cell3.CellEnergy = 1.0f;
        Cell3.FunctionalRole = TEXT("Intuitive Feeling / Potential");
        TetrahedralCells.Add(Cell3);
    }
    
    // Cell 4: Somatic-Autonomic-Spinal-Integration (B-C-D-E)
    // The Sympathetic Polarity Cell
    {
        FTetrahedralCell Cell4;
        Cell4.CellName = TEXT("SympatheticPolarityCell");
        Cell4.Vertices = {
            EPentachoronVertex::Somatic,
            EPentachoronVertex::Autonomic,
            EPentachoronVertex::Spinal,
            EPentachoronVertex::Integration
        };
        Cell4.CellEnergy = 1.0f;
        Cell4.FunctionalRole = TEXT("Emotive Technique / Commitment");
        TetrahedralCells.Add(Cell4);
    }
    
    // Cell 5: Cerebral-Somatic-Autonomic-Spinal (A-B-C-D)
    // The Outer Boundary Cell
    {
        FTetrahedralCell Cell5;
        Cell5.CellName = TEXT("OuterBoundaryCell");
        Cell5.Vertices = {
            EPentachoronVertex::Cerebral,
            EPentachoronVertex::Somatic,
            EPentachoronVertex::Autonomic,
            EPentachoronVertex::Spinal
        };
        Cell5.CellEnergy = 1.0f;
        Cell5.FunctionalRole = TEXT("Physical manifestation of nervous system");
        TetrahedralCells.Add(Cell5);
    }
    
    // Initialize base relational wholes
    // R1: Subjective direction - Idea directs routines to give form
    ActiveRelationalWholes.Add(CreateRelationalWhole(
        {EPentachoronVertex::Cerebral, EPentachoronVertex::Somatic, 
         EPentachoronVertex::Autonomic, EPentachoronVertex::Cerebral},
        EPentachoronVertex::Somatic
    ));
    
    // R2: Objective feedback - Form feeds back through sensory routines to idea
    ActiveRelationalWholes.Add(CreateRelationalWhole(
        {EPentachoronVertex::Autonomic, EPentachoronVertex::Somatic,
         EPentachoronVertex::Cerebral, EPentachoronVertex::Autonomic},
        EPentachoronVertex::Somatic
    ));
}

void UCosmicOrderSystem::InitializeConsciousnessStreams()
{
    ConsciousnessStreams.Empty();
    
    // Stream 0: Cerebral Stream (Analytical/Executive)
    {
        FConsciousnessStream CerebralStream;
        CerebralStream.StreamID = 0;
        CerebralStream.StreamName = TEXT("Cerebral");
        CerebralStream.CurrentStep = 1;
        CerebralStream.PhaseOffset = 0;
        CerebralStream.StreamEnergy = 1.0f;
        CerebralStream.StreamState = FVector(1.0f, 0.0f, 0.0f);
        ConsciousnessStreams.Add(CerebralStream);
    }
    
    // Stream 1: Somatic Stream (Behavioral/Motor)
    {
        FConsciousnessStream SomaticStream;
        SomaticStream.StreamID = 1;
        SomaticStream.StreamName = TEXT("Somatic");
        SomaticStream.CurrentStep = 5; // 4 steps behind cerebral
        SomaticStream.PhaseOffset = 4;
        SomaticStream.StreamEnergy = 1.0f;
        SomaticStream.StreamState = FVector(0.0f, 1.0f, 0.0f);
        ConsciousnessStreams.Add(SomaticStream);
    }
    
    // Stream 2: Autonomic Stream (Emotional/Background)
    {
        FConsciousnessStream AutonomicStream;
        AutonomicStream.StreamID = 2;
        AutonomicStream.StreamName = TEXT("Autonomic");
        AutonomicStream.CurrentStep = 9; // 8 steps behind cerebral
        AutonomicStream.PhaseOffset = 8;
        AutonomicStream.StreamEnergy = 1.0f;
        AutonomicStream.StreamState = FVector(0.0f, 0.0f, 1.0f);
        ConsciousnessStreams.Add(AutonomicStream);
    }
}

void UCosmicOrderSystem::AdvanceCognitiveLoop()
{
    // Process each stream
    for (int32 i = 0; i < ConsciousnessStreams.Num(); i++)
    {
        ProcessStreamStep(i);
    }
    
    // Advance global step
    GlobalCycleStep++;
    if (GlobalCycleStep > 12)
    {
        GlobalCycleStep = 1;
        TotalCyclesCompleted++;
    }
}

void UCosmicOrderSystem::ProcessStreamStep(int32 StreamIndex)
{
    if (!ConsciousnessStreams.IsValidIndex(StreamIndex))
        return;
    
    FConsciousnessStream& Stream = ConsciousnessStreams[StreamIndex];
    
    // Create step record
    FCognitiveLoopStep Step;
    Step.StepNumber = Stream.CurrentStep;
    Step.StreamIndex = StreamIndex;
    
    // Determine step type based on 7 expressive / 5 reflective pattern
    // Steps 1,2,3,4,5,6,7 are expressive, 8,9,10,11,12 are reflective
    if (Stream.CurrentStep <= 7)
    {
        Step.StepType = TEXT("Expressive");
        ProcessExpressiveStep(Stream);
    }
    else
    {
        Step.StepType = TEXT("Reflective");
        ProcessReflectiveStep(Stream);
    }
    
    // Determine step function
    // Steps 1 and 7 are pivotal relevance realization
    // Steps 2-6 are affordance interaction
    // Steps 8-12 are salience simulation
    if (Stream.CurrentStep == 1 || Stream.CurrentStep == 7)
    {
        Step.StepFunction = TEXT("Relevance Realization");
    }
    else if (Stream.CurrentStep >= 2 && Stream.CurrentStep <= 6)
    {
        Step.StepFunction = TEXT("Affordance Interaction");
    }
    else
    {
        Step.StepFunction = TEXT("Salience Simulation");
    }
    
    // Map step to active vertex based on stream
    switch (StreamIndex)
    {
        case 0: Step.ActiveVertex = EPentachoronVertex::Cerebral; break;
        case 1: Step.ActiveVertex = EPentachoronVertex::Somatic; break;
        case 2: Step.ActiveVertex = EPentachoronVertex::Autonomic; break;
    }
    
    // Determine active polarity based on step position
    int32 TriadGroup = Stream.GetTriadGroup();
    switch (TriadGroup)
    {
        case 1: Step.ActivePolarity = ESystemPolarity::Somatic; break;
        case 2: Step.ActivePolarity = ESystemPolarity::Sympathetic; break;
        case 3: Step.ActivePolarity = ESystemPolarity::Parasympathetic; break;
        case 4: Step.ActivePolarity = ESystemPolarity::Somatic; break; // Cycle back
    }
    
    Step.Activation = Stream.StreamEnergy;
    
    // Add to history
    Stream.StepHistory.Add(Step);
    if (Stream.StepHistory.Num() > 120) // Keep last 10 cycles
    {
        Stream.StepHistory.RemoveAt(0);
    }
    
    // Advance stream step
    Stream.CurrentStep++;
    if (Stream.CurrentStep > 12)
    {
        Stream.CurrentStep = 1;
    }
}

void UCosmicOrderSystem::ProcessExpressiveStep(FConsciousnessStream& Stream)
{
    // Expressive mode: outward-directed processing
    // Conditioning past performance through affordance interaction
    
    float ExpressiveGain = 0.02f;
    
    switch (Stream.StreamID)
    {
        case 0: // Cerebral
            CerebralTriad.ExecutiveControl += ExpressiveGain;
            CerebralTriad.AppliedTechnique += ExpressiveGain * 0.5f;
            break;
        case 1: // Somatic
            SomaticTriad.MovementReadiness += ExpressiveGain;
            SomaticTriad.MotorActivation += ExpressiveGain * 0.5f;
            break;
        case 2: // Autonomic
            AutonomicTriad.SympatheticActivation += ExpressiveGain;
            AutonomicTriad.EmotionalArousal += ExpressiveGain * 0.5f;
            break;
    }
    
    // Update stream state vector
    Stream.StreamState.X += ExpressiveGain;
    Stream.StreamState.Normalize();
}

void UCosmicOrderSystem::ProcessReflectiveStep(FConsciousnessStream& Stream)
{
    // Reflective mode: inward-directed processing
    // Anticipating future potential through salience simulation
    
    float ReflectiveGain = 0.02f;
    
    switch (Stream.StreamID)
    {
        case 0: // Cerebral
            CerebralTriad.IntuitiveIdea += ReflectiveGain;
            CerebralTriad.CreativeFlow += ReflectiveGain * 0.5f;
            break;
        case 1: // Somatic
            SomaticTriad.ProprioceptiveAwareness += ReflectiveGain;
            SomaticTriad.SensoryIntegration += ReflectiveGain * 0.5f;
            break;
        case 2: // Autonomic
            AutonomicTriad.ParasympatheticActivation += ReflectiveGain;
            AutonomicTriad.InteroceptiveAwareness += ReflectiveGain * 0.5f;
            break;
    }
    
    // Update stream state vector
    Stream.StreamState.Y += ReflectiveGain;
    Stream.StreamState.Normalize();
}

FRelationalWhole UCosmicOrderSystem::CreateRelationalWhole(
    const TArray<EPentachoronVertex>& Path,
    EPentachoronVertex Pivot)
{
    FRelationalWhole RW;
    RW.ID = FString::Printf(TEXT("RW_%d"), ActiveRelationalWholes.Num());
    RW.CircuitPath = Path;
    RW.PivotCenter = Pivot;
    RW.ForwardFlow = 1.0f;
    RW.BackwardFlow = 1.0f;
    RW.bIsBalanced = true;
    return RW;
}

FProjection UCosmicOrderSystem::CreateProjection(
    const TArray<EPentachoronVertex>& Path,
    float ResourceValue)
{
    FProjection Proj;
    Proj.ID = FString::Printf(TEXT("PK_%d"), ActiveProjections.Num());
    Proj.ProjectionPath = Path;
    Proj.ResourceValue = ResourceValue;
    Proj.AccountType = TEXT("Debit");
    return Proj;
}

void UCosmicOrderSystem::UpdateTriadicStates()
{
    // Clamp all values to valid ranges
    CerebralTriad.IntuitiveIdea = FMath::Clamp(CerebralTriad.IntuitiveIdea, 0.0f, 1.0f);
    CerebralTriad.AppliedTechnique = FMath::Clamp(CerebralTriad.AppliedTechnique, 0.0f, 1.0f);
    CerebralTriad.AnalyticalFocus = FMath::Clamp(CerebralTriad.AnalyticalFocus, 0.0f, 1.0f);
    CerebralTriad.CreativeFlow = FMath::Clamp(CerebralTriad.CreativeFlow, 0.0f, 1.0f);
    CerebralTriad.ExecutiveControl = FMath::Clamp(CerebralTriad.ExecutiveControl, 0.0f, 1.0f);
    
    SomaticTriad.SomaticBalance = FMath::Clamp(SomaticTriad.SomaticBalance, 0.0f, 1.0f);
    SomaticTriad.MotorActivation = FMath::Clamp(SomaticTriad.MotorActivation, 0.0f, 1.0f);
    SomaticTriad.MovementReadiness = FMath::Clamp(SomaticTriad.MovementReadiness, 0.0f, 1.0f);
    SomaticTriad.ProprioceptiveAwareness = FMath::Clamp(SomaticTriad.ProprioceptiveAwareness, 0.0f, 1.0f);
    
    AutonomicTriad.EmotiveBalance = FMath::Clamp(AutonomicTriad.EmotiveBalance, 0.0f, 1.0f);
    AutonomicTriad.SympatheticActivation = FMath::Clamp(AutonomicTriad.SympatheticActivation, 0.0f, 1.0f);
    AutonomicTriad.ParasympatheticActivation = FMath::Clamp(AutonomicTriad.ParasympatheticActivation, 0.0f, 1.0f);
    AutonomicTriad.EmotionalValence = FMath::Clamp(AutonomicTriad.EmotionalValence, -1.0f, 1.0f);
    AutonomicTriad.EmotionalArousal = FMath::Clamp(AutonomicTriad.EmotionalArousal, 0.0f, 1.0f);
    
    // Calculate derived states
    // Somatic Balance = average of motor and sensory
    SomaticTriad.SomaticBalance = (SomaticTriad.MotorActivation + SomaticTriad.SensoryIntegration) * 0.5f;
    
    // Emotive Balance = sympathetic - parasympathetic (normalized)
    AutonomicTriad.EmotiveBalance = (AutonomicTriad.SympatheticActivation - 
                                     AutonomicTriad.ParasympatheticActivation + 1.0f) * 0.5f;
    
    // Emotional Valence from balance
    AutonomicTriad.EmotionalValence = AutonomicTriad.EmotiveBalance * 2.0f - 1.0f;
}

FVector UCosmicOrderSystem::GetPolarityBalance() const
{
    // X = Somatic Polarity (Behavior/Commitment)
    // Y = Sympathetic Polarity (Emotive/Commitment)
    // Z = Parasympathetic Polarity (Intuitive/Potential)
    
    float SomaticPolarity = SomaticTriad.SomaticBalance;
    float SympatheticPolarity = AutonomicTriad.SympatheticActivation;
    float ParasympatheticPolarity = AutonomicTriad.ParasympatheticActivation;
    
    return FVector(SomaticPolarity, SympatheticPolarity, ParasympatheticPolarity);
}

float UCosmicOrderSystem::CalculateCountercurrentBalance(const FRelationalWhole& RW) const
{
    if (!RW.IsClosed())
        return 0.0f;
    
    return RW.GetBalanceRatio();
}

void UCosmicOrderSystem::ProcessSomaticPolarity(float DeltaTime)
{
    // Somatic Polarity: Behavior Technique / Commitment
    // Edge B-D in pentachoron
    
    float DecayRate = 0.1f;
    SomaticTriad.MotorActivation -= DecayRate * DeltaTime;
    SomaticTriad.MotorActivation = FMath::Max(0.0f, SomaticTriad.MotorActivation);
}

void UCosmicOrderSystem::ProcessSympatheticPolarity(float DeltaTime)
{
    // Sympathetic Polarity: Emotive Technique / Commitment
    // Edge C-E in pentachoron
    
    float DecayRate = 0.1f;
    AutonomicTriad.SympatheticActivation -= DecayRate * DeltaTime;
    AutonomicTriad.SympatheticActivation = FMath::Max(0.0f, AutonomicTriad.SympatheticActivation);
}

void UCosmicOrderSystem::ProcessParasympatheticPolarity(float DeltaTime)
{
    // Parasympathetic Polarity: Intuitive Feeling / Potential
    // Edge A-C in pentachoron
    
    float DecayRate = 0.05f; // Slower decay for intuitive processes
    AutonomicTriad.ParasympatheticActivation -= DecayRate * DeltaTime;
    AutonomicTriad.ParasympatheticActivation = FMath::Max(0.0f, AutonomicTriad.ParasympatheticActivation);
}

float UCosmicOrderSystem::CalculateVertexEnergy(EPentachoronVertex Vertex) const
{
    switch (Vertex)
    {
        case EPentachoronVertex::Cerebral:
            return (CerebralTriad.IntuitiveIdea + CerebralTriad.AppliedTechnique + 
                    CerebralTriad.ExecutiveControl) / 3.0f;
        case EPentachoronVertex::Somatic:
            return (SomaticTriad.MotorActivation + SomaticTriad.SensoryIntegration + 
                    SomaticTriad.SomaticBalance) / 3.0f;
        case EPentachoronVertex::Autonomic:
            return (AutonomicTriad.SympatheticActivation + AutonomicTriad.ParasympatheticActivation + 
                    AutonomicTriad.EmotiveBalance) / 3.0f;
        case EPentachoronVertex::Spinal:
            return SomaticTriad.ProprioceptiveAwareness;
        case EPentachoronVertex::Integration:
            return (CalculateVertexEnergy(EPentachoronVertex::Cerebral) +
                    CalculateVertexEnergy(EPentachoronVertex::Somatic) +
                    CalculateVertexEnergy(EPentachoronVertex::Autonomic)) / 3.0f;
        default:
            return 0.0f;
    }
}

void UCosmicOrderSystem::PropagateEnergy(float DeltaTime)
{
    // Propagate energy through relational wholes
    for (FRelationalWhole& RW : ActiveRelationalWholes)
    {
        if (RW.CircuitPath.Num() < 2)
            continue;
        
        // Calculate flow based on vertex energies
        float TotalEnergy = 0.0f;
        for (const EPentachoronVertex& V : RW.CircuitPath)
        {
            TotalEnergy += CalculateVertexEnergy(V);
        }
        
        float AvgEnergy = TotalEnergy / RW.CircuitPath.Num();
        
        // Update flows with smoothing
        float FlowSmoothing = 0.9f;
        RW.ForwardFlow = FMath::Lerp(RW.ForwardFlow, AvgEnergy, (1.0f - FlowSmoothing) * DeltaTime * 10.0f);
        RW.BackwardFlow = FMath::Lerp(RW.BackwardFlow, AvgEnergy, (1.0f - FlowSmoothing) * DeltaTime * 10.0f);
        
        // Check balance
        float BalanceRatio = RW.GetBalanceRatio();
        RW.bIsBalanced = FMath::Abs(BalanceRatio - 1.0f) < 0.1f;
    }
}

FDeepTreeEchoCognitiveState UCosmicOrderSystem::MapToDeepTreeEchoState() const
{
    FDeepTreeEchoCognitiveState State;
    
    // Map triadic states to cognitive state
    State.Attention = CerebralTriad.AnalyticalFocus;
    State.Arousal = AutonomicTriad.EmotionalArousal;
    State.Valence = AutonomicTriad.EmotionalValence;
    
    // Map wisdom dimensions
    State.WisdomScore = GetOverallWisdomScore();
    
    return State;
}

FEmotionalState UCosmicOrderSystem::MapToEmotionalState() const
{
    FEmotionalState State;
    
    // Primary emotions from autonomic triad
    State.Joy = FMath::Max(0.0f, AutonomicTriad.EmotionalValence);
    State.Sadness = FMath::Max(0.0f, -AutonomicTriad.EmotionalValence);
    State.Anger = AutonomicTriad.SympatheticActivation * 0.5f;
    State.Fear = AutonomicTriad.SympatheticActivation * 0.3f;
    State.Surprise = CerebralTriad.IntuitiveIdea * 0.5f;
    State.Disgust = 0.0f;
    
    // Arousal and valence
    State.Arousal = AutonomicTriad.EmotionalArousal;
    State.Valence = AutonomicTriad.EmotionalValence;
    
    return State;
}

FCognitiveState UCosmicOrderSystem::MapToCognitiveState() const
{
    FCognitiveState State;
    
    // Map cerebral triad to cognitive state
    State.Focus = CerebralTriad.AnalyticalFocus;
    State.Creativity = CerebralTriad.CreativeFlow;
    State.ExecutiveFunction = CerebralTriad.ExecutiveControl;
    
    // Map somatic triad
    State.MotorReadiness = SomaticTriad.MovementReadiness;
    State.SensoryIntegration = SomaticTriad.SensoryIntegration;
    
    // Processing load from stream energies
    float TotalStreamEnergy = 0.0f;
    for (const FConsciousnessStream& Stream : ConsciousnessStreams)
    {
        TotalStreamEnergy += Stream.StreamEnergy;
    }
    State.ProcessingLoad = TotalStreamEnergy / 3.0f;
    
    return State;
}

void UCosmicOrderSystem::ReceiveDeepTreeEchoInput(const FDeepTreeEchoCognitiveState& State)
{
    // Update triadic states from Deep Tree Echo input
    CerebralTriad.AnalyticalFocus = State.Attention;
    AutonomicTriad.EmotionalArousal = State.Arousal;
    AutonomicTriad.EmotionalValence = State.Valence;
    
    // Propagate to stream energies
    for (FConsciousnessStream& Stream : ConsciousnessStreams)
    {
        Stream.StreamEnergy = FMath::Lerp(Stream.StreamEnergy, State.Attention, 0.1f);
    }
}

void UCosmicOrderSystem::UpdateWisdomDimensions(float DeltaTime)
{
    // Wisdom grows through cognitive loop activity
    float GrowthRate = 0.001f * DeltaTime;
    
    // Understanding grows from cerebral processing
    WisdomUnderstanding += GrowthRate * CerebralTriad.AnalyticalFocus;
    
    // Perspective grows from multiple stream integration
    float StreamDiversity = 0.0f;
    for (const FConsciousnessStream& Stream : ConsciousnessStreams)
    {
        StreamDiversity += Stream.StreamEnergy;
    }
    WisdomPerspective += GrowthRate * (StreamDiversity / 3.0f);
    
    // Integration grows from balanced relational wholes
    float BalanceCount = 0.0f;
    for (const FRelationalWhole& RW : ActiveRelationalWholes)
    {
        if (RW.bIsBalanced) BalanceCount += 1.0f;
    }
    WisdomIntegration += GrowthRate * (BalanceCount / FMath::Max(1.0f, (float)ActiveRelationalWholes.Num()));
    
    // Reflection grows from reflective steps
    WisdomReflection += GrowthRate * CerebralTriad.IntuitiveIdea;
    
    // Compassion grows from autonomic balance
    WisdomCompassion += GrowthRate * AutonomicTriad.EmotiveBalance;
    
    // Equanimity grows from polarity balance
    FVector Polarity = GetPolarityBalance();
    float PolarityBalance = 1.0f - FMath::Abs(Polarity.X - Polarity.Y) - FMath::Abs(Polarity.Y - Polarity.Z);
    WisdomEquanimity += GrowthRate * FMath::Max(0.0f, PolarityBalance);
    
    // Transcendence grows from overall system coherence
    float Coherence = CalculateVertexEnergy(EPentachoronVertex::Integration);
    WisdomTranscendence += GrowthRate * Coherence;
    
    // Apply diminishing returns
    float MaxWisdom = 1.0f;
    WisdomUnderstanding = FMath::Min(MaxWisdom, WisdomUnderstanding);
    WisdomPerspective = FMath::Min(MaxWisdom, WisdomPerspective);
    WisdomIntegration = FMath::Min(MaxWisdom, WisdomIntegration);
    WisdomReflection = FMath::Min(MaxWisdom, WisdomReflection);
    WisdomCompassion = FMath::Min(MaxWisdom, WisdomCompassion);
    WisdomEquanimity = FMath::Min(MaxWisdom, WisdomEquanimity);
    WisdomTranscendence = FMath::Min(MaxWisdom, WisdomTranscendence);
}

float UCosmicOrderSystem::GetOverallWisdomScore() const
{
    return (WisdomUnderstanding + WisdomPerspective + WisdomIntegration +
            WisdomReflection + WisdomCompassion + WisdomEquanimity +
            WisdomTranscendence) / 7.0f;
}

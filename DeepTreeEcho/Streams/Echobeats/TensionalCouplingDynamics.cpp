// TensionalCouplingDynamics.cpp
// Implementation of tetrahedral tensor bundle architecture

#include "TensionalCouplingDynamics.h"
#include "Math/UnrealMathUtility.h"

UTensionalCouplingDynamics::UTensionalCouplingDynamics()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UTensionalCouplingDynamics::BeginPlay()
{
    Super::BeginPlay();

    // Find echobeats engine reference
    AActor* Owner = GetOwner();
    if (Owner)
    {
        EchobeatsEngine = Owner->FindComponentByClass<UEchobeatsStreamEngine>();
    }

    InitializeCouplingSystem();
}

void UTensionalCouplingDynamics::TickComponent(float DeltaTime, ELevelTick TickType,
                                                FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // Auto multiplex
    if (bEnableAutoMultiplex)
    {
        MultiplexTimer += DeltaTime;
        float MultiplexInterval = 1.0f / MultiplexCycleRate;

        if (MultiplexTimer >= MultiplexInterval)
        {
            MultiplexTimer -= MultiplexInterval;
            AdvanceMultiplex();
        }
    }

    // Propagate tensions
    PropagateAllTensions();

    // Update entanglements
    if (bEnableEntanglementDetection)
    {
        UpdateEntanglements(DeltaTime);
    }

    // Compute global metrics
    ComputeTetrahedralCoherence();
    ComputeTensionBalance();
}

void UTensionalCouplingDynamics::InitializeCouplingSystem()
{
    BuildTetrahedralStructure();
    InitializeMultiplexPermutations();
}

void UTensionalCouplingDynamics::BuildTetrahedralStructure()
{
    // Initialize 4 vertices
    State.VertexStates.Empty();
    for (int32 i = 0; i < 4; ++i)
    {
        FThreadVertexState Vertex;
        Vertex.VertexType = static_cast<EThreadVertex>(i);
        Vertex.Activation = 0.0f;
        Vertex.StateVector.SetNum(StateVectorSize);
        Vertex.ProcessingLoad = 0.0f;
        State.VertexStates.Add(Vertex);
    }

    // Initialize 6 edges
    State.EdgeStates.Empty();
    TArray<TPair<EThreadVertex, EThreadVertex>> EdgeVertexPairs = {
        {EThreadVertex::V1_Perception, EThreadVertex::V2_Memory},      // Edge_12
        {EThreadVertex::V1_Perception, EThreadVertex::V3_Assessment},  // Edge_13
        {EThreadVertex::V1_Perception, EThreadVertex::V4_Planning},    // Edge_14
        {EThreadVertex::V2_Memory, EThreadVertex::V3_Assessment},      // Edge_23
        {EThreadVertex::V2_Memory, EThreadVertex::V4_Planning},        // Edge_24
        {EThreadVertex::V3_Assessment, EThreadVertex::V4_Planning}     // Edge_34
    };

    for (int32 i = 0; i < 6; ++i)
    {
        FDyadicEdgeState Edge;
        Edge.EdgeType = static_cast<EDyadicEdge>(i);
        Edge.VertexA = EdgeVertexPairs[i].Key;
        Edge.VertexB = EdgeVertexPairs[i].Value;
        Edge.Tension = 0.5f;
        Edge.FlowDirection = 0.0f;
        Edge.Activation = 0.0f;
        Edge.StateVector.SetNum(StateVectorSize);
        Edge.ResonanceMode = ECouplingResonanceMode::Harmonic;
        State.EdgeStates.Add(Edge);
    }

    // Initialize 4 faces
    State.FaceStates.Empty();
    TArray<TArray<EThreadVertex>> FaceVertices = {
        {EThreadVertex::V1_Perception, EThreadVertex::V2_Memory, EThreadVertex::V3_Assessment},      // Face_123
        {EThreadVertex::V1_Perception, EThreadVertex::V2_Memory, EThreadVertex::V4_Planning},        // Face_124
        {EThreadVertex::V1_Perception, EThreadVertex::V3_Assessment, EThreadVertex::V4_Planning},    // Face_134
        {EThreadVertex::V2_Memory, EThreadVertex::V3_Assessment, EThreadVertex::V4_Planning}         // Face_234
    };

    TArray<TArray<EDyadicEdge>> FaceEdges = {
        {EDyadicEdge::Edge_12, EDyadicEdge::Edge_13, EDyadicEdge::Edge_23},  // Face_123
        {EDyadicEdge::Edge_12, EDyadicEdge::Edge_14, EDyadicEdge::Edge_24},  // Face_124
        {EDyadicEdge::Edge_13, EDyadicEdge::Edge_14, EDyadicEdge::Edge_34},  // Face_134
        {EDyadicEdge::Edge_23, EDyadicEdge::Edge_24, EDyadicEdge::Edge_34}   // Face_234
    };

    TArray<TArray<ETriadicFace>> FaceAdjacencies = {
        {ETriadicFace::Face_124, ETriadicFace::Face_134, ETriadicFace::Face_234},  // Face_123
        {ETriadicFace::Face_123, ETriadicFace::Face_134, ETriadicFace::Face_234},  // Face_124
        {ETriadicFace::Face_123, ETriadicFace::Face_124, ETriadicFace::Face_234},  // Face_134
        {ETriadicFace::Face_123, ETriadicFace::Face_124, ETriadicFace::Face_134}   // Face_234
    };

    for (int32 i = 0; i < 4; ++i)
    {
        FTriadicFaceState Face;
        Face.FaceType = static_cast<ETriadicFace>(i);
        Face.Vertices = FaceVertices[i];
        Face.Edges = FaceEdges[i];
        Face.AdjacentFaces = FaceAdjacencies[i];
        Face.Orientation = (float)i * (2.0f * PI / 4.0f);
        Face.Coherence = 1.0f;
        Face.Activation = 0.0f;
        Face.StateVector.SetNum(StateVectorSize);
        State.FaceStates.Add(Face);
    }

    // Set up vertex edge connections
    for (FThreadVertexState& Vertex : State.VertexStates)
    {
        Vertex.ConnectedEdges = GetEdgesForVertex(Vertex.VertexType);
        for (const FTriadicFaceState& Face : State.FaceStates)
        {
            if (Face.Vertices.Contains(Vertex.VertexType))
            {
                Vertex.ContainingFaces.Add(Face.FaceType);
            }
        }
    }

    // Initialize entanglements (empty)
    State.ActiveEntanglements.Empty();

    // Initialize global state
    State.TetrahedralCoherence = 1.0f;
    State.TensionBalance = 0.5f;
}

void UTensionalCouplingDynamics::InitializeMultiplexPermutations()
{
    // Dyad permutation sequence: P(1,2)→P(1,3)→P(1,4)→P(2,3)→P(2,4)→P(3,4)
    DyadPermutations.Empty();
    DyadPermutations.Add({EThreadVertex::V1_Perception, EThreadVertex::V2_Memory});
    DyadPermutations.Add({EThreadVertex::V1_Perception, EThreadVertex::V3_Assessment});
    DyadPermutations.Add({EThreadVertex::V1_Perception, EThreadVertex::V4_Planning});
    DyadPermutations.Add({EThreadVertex::V2_Memory, EThreadVertex::V3_Assessment});
    DyadPermutations.Add({EThreadVertex::V2_Memory, EThreadVertex::V4_Planning});
    DyadPermutations.Add({EThreadVertex::V3_Assessment, EThreadVertex::V4_Planning});

    // Triad permutation sequence for MP1: P[1,2,3]→P[1,2,4]→P[1,3,4]→P[2,3,4]
    TriadPermutationsMP1.Empty();
    TriadPermutationsMP1.Add({EThreadVertex::V1_Perception, EThreadVertex::V2_Memory, EThreadVertex::V3_Assessment});
    TriadPermutationsMP1.Add({EThreadVertex::V1_Perception, EThreadVertex::V2_Memory, EThreadVertex::V4_Planning});
    TriadPermutationsMP1.Add({EThreadVertex::V1_Perception, EThreadVertex::V3_Assessment, EThreadVertex::V4_Planning});
    TriadPermutationsMP1.Add({EThreadVertex::V2_Memory, EThreadVertex::V3_Assessment, EThreadVertex::V4_Planning});

    // Triad permutation sequence for MP2: P[1,3,4]→P[2,3,4]→P[1,2,3]→P[1,2,4]
    TriadPermutationsMP2.Empty();
    TriadPermutationsMP2.Add({EThreadVertex::V1_Perception, EThreadVertex::V3_Assessment, EThreadVertex::V4_Planning});
    TriadPermutationsMP2.Add({EThreadVertex::V2_Memory, EThreadVertex::V3_Assessment, EThreadVertex::V4_Planning});
    TriadPermutationsMP2.Add({EThreadVertex::V1_Perception, EThreadVertex::V2_Memory, EThreadVertex::V3_Assessment});
    TriadPermutationsMP2.Add({EThreadVertex::V1_Perception, EThreadVertex::V2_Memory, EThreadVertex::V4_Planning});

    // Initialize multiplex state
    State.MultiplexState.PermutationIndex = 0;
    State.MultiplexState.MultiplexPhase = 0;
    State.MultiplexState.CurrentDyadPair = DyadPermutations[0];
    State.MultiplexState.MP1TriadSet = TriadPermutationsMP1[0];
    State.MultiplexState.MP2TriadSet = TriadPermutationsMP2[0];
}

void UTensionalCouplingDynamics::SetEdgeTension(EDyadicEdge Edge, float Tension)
{
    int32 Index = GetEdgeIndex(Edge);
    if (Index >= 0 && Index < State.EdgeStates.Num())
    {
        float OldTension = State.EdgeStates[Index].Tension;
        State.EdgeStates[Index].Tension = FMath::Clamp(Tension, 0.0f, 1.0f);

        if (FMath::Abs(State.EdgeStates[Index].Tension - OldTension) > 0.01f)
        {
            OnEdgeTensionChanged.Broadcast(Edge, State.EdgeStates[Index].Tension);
        }
    }
}

void UTensionalCouplingDynamics::SetEdgeFlow(EDyadicEdge Edge, float FlowDirection)
{
    int32 Index = GetEdgeIndex(Edge);
    if (Index >= 0 && Index < State.EdgeStates.Num())
    {
        State.EdgeStates[Index].FlowDirection = FMath::Clamp(FlowDirection, -1.0f, 1.0f);
    }
}

FDyadicEdgeState UTensionalCouplingDynamics::GetEdgeState(EDyadicEdge Edge) const
{
    int32 Index = GetEdgeIndex(Edge);
    if (Index >= 0 && Index < State.EdgeStates.Num())
    {
        return State.EdgeStates[Index];
    }
    return FDyadicEdgeState();
}

void UTensionalCouplingDynamics::PropagateTension(EDyadicEdge Edge)
{
    int32 EdgeIndex = GetEdgeIndex(Edge);
    if (EdgeIndex < 0 || EdgeIndex >= State.EdgeStates.Num())
    {
        return;
    }

    FDyadicEdgeState& EdgeState = State.EdgeStates[EdgeIndex];
    TArray<EThreadVertex> Vertices = GetVerticesForEdge(Edge);

    if (Vertices.Num() < 2)
    {
        return;
    }

    int32 VertexAIndex = GetVertexIndex(Vertices[0]);
    int32 VertexBIndex = GetVertexIndex(Vertices[1]);

    if (VertexAIndex < 0 || VertexBIndex < 0)
    {
        return;
    }

    FThreadVertexState& VertexA = State.VertexStates[VertexAIndex];
    FThreadVertexState& VertexB = State.VertexStates[VertexBIndex];

    // Propagate based on flow direction and tension
    float FlowAmount = EdgeState.Tension * TensionPropagationRate;

    if (EdgeState.FlowDirection < 0)
    {
        // Flow from A to B
        VertexB.Activation = FMath::Clamp(VertexB.Activation + FlowAmount * FMath::Abs(EdgeState.FlowDirection), 0.0f, 1.0f);
    }
    else if (EdgeState.FlowDirection > 0)
    {
        // Flow from B to A
        VertexA.Activation = FMath::Clamp(VertexA.Activation + FlowAmount * EdgeState.FlowDirection, 0.0f, 1.0f);
    }

    // Apply resonance mode effects
    switch (EdgeState.ResonanceMode)
    {
    case ECouplingResonanceMode::Harmonic:
        // Constructive interference - amplify
        EdgeState.Activation = FMath::Min(EdgeState.Activation + 0.01f, 1.0f);
        break;
    case ECouplingResonanceMode::Dissonant:
        // Destructive interference - dampen
        EdgeState.Activation = FMath::Max(EdgeState.Activation - 0.01f, 0.0f);
        break;
    case ECouplingResonanceMode::Entangled:
        // Quantum-like behavior - maintain correlation
        break;
    case ECouplingResonanceMode::Decoupled:
        // Independent - no propagation
        break;
    }
}

void UTensionalCouplingDynamics::SetEdgeResonanceMode(EDyadicEdge Edge, ECouplingResonanceMode Mode)
{
    int32 Index = GetEdgeIndex(Edge);
    if (Index >= 0 && Index < State.EdgeStates.Num())
    {
        ECouplingResonanceMode OldMode = State.EdgeStates[Index].ResonanceMode;
        State.EdgeStates[Index].ResonanceMode = Mode;

        if (OldMode != Mode)
        {
            OnResonanceModeChanged.Broadcast(Edge, Mode);
        }
    }
}

void UTensionalCouplingDynamics::ActivateFace(ETriadicFace Face, float Activation)
{
    int32 Index = GetFaceIndex(Face);
    if (Index >= 0 && Index < State.FaceStates.Num())
    {
        State.FaceStates[Index].Activation = FMath::Clamp(Activation, 0.0f, 1.0f);

        // Propagate to edges
        for (EDyadicEdge Edge : State.FaceStates[Index].Edges)
        {
            int32 EdgeIndex = GetEdgeIndex(Edge);
            if (EdgeIndex >= 0)
            {
                State.EdgeStates[EdgeIndex].Activation = FMath::Max(
                    State.EdgeStates[EdgeIndex].Activation,
                    Activation * 0.5f
                );
            }
        }

        OnFaceActivated.Broadcast(Face, Activation);
    }
}

FTriadicFaceState UTensionalCouplingDynamics::GetFaceState(ETriadicFace Face) const
{
    int32 Index = GetFaceIndex(Face);
    if (Index >= 0 && Index < State.FaceStates.Num())
    {
        return State.FaceStates[Index];
    }
    return FTriadicFaceState();
}

float UTensionalCouplingDynamics::ComputeFaceCoherence(ETriadicFace Face)
{
    int32 Index = GetFaceIndex(Face);
    if (Index < 0 || Index >= State.FaceStates.Num())
    {
        return 0.0f;
    }

    FTriadicFaceState& FaceState = State.FaceStates[Index];

    // Compute coherence from edge tensions
    float TotalTension = 0.0f;
    float TensionVariance = 0.0f;

    for (EDyadicEdge Edge : FaceState.Edges)
    {
        int32 EdgeIndex = GetEdgeIndex(Edge);
        if (EdgeIndex >= 0)
        {
            TotalTension += State.EdgeStates[EdgeIndex].Tension;
        }
    }

    float MeanTension = TotalTension / FaceState.Edges.Num();

    for (EDyadicEdge Edge : FaceState.Edges)
    {
        int32 EdgeIndex = GetEdgeIndex(Edge);
        if (EdgeIndex >= 0)
        {
            float Diff = State.EdgeStates[EdgeIndex].Tension - MeanTension;
            TensionVariance += Diff * Diff;
        }
    }

    TensionVariance /= FaceState.Edges.Num();

    // Coherence is inverse of variance (normalized)
    FaceState.Coherence = 1.0f / (1.0f + TensionVariance * 10.0f);

    return FaceState.Coherence;
}

void UTensionalCouplingDynamics::SynchronizeAdjacentFaces(ETriadicFace Face)
{
    int32 Index = GetFaceIndex(Face);
    if (Index < 0 || Index >= State.FaceStates.Num())
    {
        return;
    }

    const FTriadicFaceState& FaceState = State.FaceStates[Index];

    for (ETriadicFace AdjacentFace : FaceState.AdjacentFaces)
    {
        int32 AdjIndex = GetFaceIndex(AdjacentFace);
        if (AdjIndex >= 0 && AdjIndex < State.FaceStates.Num())
        {
            // Synchronize orientations
            float OrientationDiff = FaceState.Orientation - State.FaceStates[AdjIndex].Orientation;
            State.FaceStates[AdjIndex].Orientation += OrientationDiff * 0.1f;

            // Propagate activation
            State.FaceStates[AdjIndex].Activation = FMath::Lerp(
                State.FaceStates[AdjIndex].Activation,
                FaceState.Activation,
                0.2f
            );
        }
    }
}

void UTensionalCouplingDynamics::SetVertexActivation(EThreadVertex Vertex, float Activation)
{
    int32 Index = GetVertexIndex(Vertex);
    if (Index >= 0 && Index < State.VertexStates.Num())
    {
        State.VertexStates[Index].Activation = FMath::Clamp(Activation, 0.0f, 1.0f);
    }
}

FThreadVertexState UTensionalCouplingDynamics::GetVertexState(EThreadVertex Vertex) const
{
    int32 Index = GetVertexIndex(Vertex);
    if (Index >= 0 && Index < State.VertexStates.Num())
    {
        return State.VertexStates[Index];
    }
    return FThreadVertexState();
}

void UTensionalCouplingDynamics::ProcessVertex(EThreadVertex Vertex, const TArray<float>& Input)
{
    int32 Index = GetVertexIndex(Vertex);
    if (Index < 0 || Index >= State.VertexStates.Num())
    {
        return;
    }

    FThreadVertexState& VertexState = State.VertexStates[Index];

    // Update state vector from input
    for (int32 i = 0; i < FMath::Min(Input.Num(), VertexState.StateVector.Num()); ++i)
    {
        VertexState.StateVector[i] = FMath::Lerp(VertexState.StateVector[i], Input[i], 0.3f);
    }

    // Update processing load
    float InputMagnitude = 0.0f;
    for (float Val : Input)
    {
        InputMagnitude += Val * Val;
    }
    VertexState.ProcessingLoad = FMath::Sqrt(InputMagnitude / FMath::Max(1, Input.Num()));
}

void UTensionalCouplingDynamics::CreateEntanglement(EDyadicEdge EdgeA, EDyadicEdge EdgeB)
{
    // Check if already entangled
    if (AreEdgesEntangled(EdgeA, EdgeB))
    {
        return;
    }

    FCouplingEntanglement Entanglement;
    Entanglement.EdgeA = EdgeA;
    Entanglement.EdgeB = EdgeB;
    Entanglement.EntanglementStrength = 0.5f;
    Entanglement.PhaseCorrelation = 1.0f;
    Entanglement.bIsEntangled = true;
    Entanglement.SharedMemorySlot = State.ActiveEntanglements.Num();

    State.ActiveEntanglements.Add(Entanglement);

    // Set edges to entangled mode
    SetEdgeResonanceMode(EdgeA, ECouplingResonanceMode::Entangled);
    SetEdgeResonanceMode(EdgeB, ECouplingResonanceMode::Entangled);

    OnEntanglementFormed.Broadcast(EdgeA, EdgeB, Entanglement.EntanglementStrength);
}

void UTensionalCouplingDynamics::BreakEntanglement(EDyadicEdge EdgeA, EDyadicEdge EdgeB)
{
    for (int32 i = State.ActiveEntanglements.Num() - 1; i >= 0; --i)
    {
        const FCouplingEntanglement& Ent = State.ActiveEntanglements[i];
        if ((Ent.EdgeA == EdgeA && Ent.EdgeB == EdgeB) ||
            (Ent.EdgeA == EdgeB && Ent.EdgeB == EdgeA))
        {
            State.ActiveEntanglements.RemoveAt(i);

            // Reset edges to harmonic mode
            SetEdgeResonanceMode(EdgeA, ECouplingResonanceMode::Harmonic);
            SetEdgeResonanceMode(EdgeB, ECouplingResonanceMode::Harmonic);
            break;
        }
    }
}

bool UTensionalCouplingDynamics::AreEdgesEntangled(EDyadicEdge EdgeA, EDyadicEdge EdgeB) const
{
    for (const FCouplingEntanglement& Ent : State.ActiveEntanglements)
    {
        if ((Ent.EdgeA == EdgeA && Ent.EdgeB == EdgeB) ||
            (Ent.EdgeA == EdgeB && Ent.EdgeB == EdgeA))
        {
            return Ent.bIsEntangled;
        }
    }
    return false;
}

FCouplingEntanglement UTensionalCouplingDynamics::GetEntanglementState(EDyadicEdge EdgeA, EDyadicEdge EdgeB) const
{
    for (const FCouplingEntanglement& Ent : State.ActiveEntanglements)
    {
        if ((Ent.EdgeA == EdgeA && Ent.EdgeB == EdgeB) ||
            (Ent.EdgeA == EdgeB && Ent.EdgeB == EdgeA))
        {
            return Ent;
        }
    }
    return FCouplingEntanglement();
}

void UTensionalCouplingDynamics::DetectPotentialEntanglements()
{
    // Check all edge pairs for potential entanglement
    for (int32 i = 0; i < State.EdgeStates.Num(); ++i)
    {
        for (int32 j = i + 1; j < State.EdgeStates.Num(); ++j)
        {
            const FDyadicEdgeState& EdgeA = State.EdgeStates[i];
            const FDyadicEdgeState& EdgeB = State.EdgeStates[j];

            // Check if edges are opposite (don't share vertices)
            TArray<EThreadVertex> VerticesA = GetVerticesForEdge(EdgeA.EdgeType);
            TArray<EThreadVertex> VerticesB = GetVerticesForEdge(EdgeB.EdgeType);

            bool SharesVertex = false;
            for (EThreadVertex VA : VerticesA)
            {
                if (VerticesB.Contains(VA))
                {
                    SharesVertex = true;
                    break;
                }
            }

            // Only opposite edges can be entangled (order-2 qubit)
            if (!SharesVertex)
            {
                // Check tension correlation
                float TensionCorrelation = 1.0f - FMath::Abs(EdgeA.Tension - EdgeB.Tension);

                if (TensionCorrelation > EntanglementThreshold)
                {
                    if (!AreEdgesEntangled(EdgeA.EdgeType, EdgeB.EdgeType))
                    {
                        CreateEntanglement(EdgeA.EdgeType, EdgeB.EdgeType);
                    }
                }
            }
        }
    }
}

void UTensionalCouplingDynamics::AdvanceMultiplex()
{
    int32 OldIndex = State.MultiplexState.PermutationIndex;

    // Advance dyad permutation (6 permutations)
    State.MultiplexState.PermutationIndex = (State.MultiplexState.PermutationIndex + 1) % 6;

    // Update current dyad pair
    State.MultiplexState.CurrentDyadPair = DyadPermutations[State.MultiplexState.PermutationIndex];

    // Update triad sets (4 permutations, cycle at different rate)
    int32 TriadIndex = State.MultiplexState.PermutationIndex % 4;
    State.MultiplexState.MP1TriadSet = TriadPermutationsMP1[TriadIndex];
    State.MultiplexState.MP2TriadSet = TriadPermutationsMP2[TriadIndex];

    // Update multiplex phase
    State.MultiplexState.MultiplexPhase = State.MultiplexState.PermutationIndex;

    OnMultiplexAdvanced.Broadcast(State.MultiplexState.PermutationIndex);
}

FThreadMultiplexState UTensionalCouplingDynamics::GetMultiplexState() const
{
    return State.MultiplexState;
}

void UTensionalCouplingDynamics::SetMultiplexPermutation(int32 PermutationIndex)
{
    State.MultiplexState.PermutationIndex = FMath::Clamp(PermutationIndex, 0, 5);
    State.MultiplexState.CurrentDyadPair = DyadPermutations[State.MultiplexState.PermutationIndex];

    int32 TriadIndex = State.MultiplexState.PermutationIndex % 4;
    State.MultiplexState.MP1TriadSet = TriadPermutationsMP1[TriadIndex];
    State.MultiplexState.MP2TriadSet = TriadPermutationsMP2[TriadIndex];
}

TArray<EThreadVertex> UTensionalCouplingDynamics::GetActiveDyadPair() const
{
    return State.MultiplexState.CurrentDyadPair;
}

void UTensionalCouplingDynamics::GetActiveTriadSets(TArray<EThreadVertex>& OutMP1, TArray<EThreadVertex>& OutMP2) const
{
    OutMP1 = State.MultiplexState.MP1TriadSet;
    OutMP2 = State.MultiplexState.MP2TriadSet;
}

FTetrahedralCouplingState UTensionalCouplingDynamics::GetTetrahedralState() const
{
    return State;
}

float UTensionalCouplingDynamics::GetTetrahedralCoherence() const
{
    return State.TetrahedralCoherence;
}

float UTensionalCouplingDynamics::GetTensionBalance() const
{
    return State.TensionBalance;
}

void UTensionalCouplingDynamics::ProcessEchobeatStep(int32 Step, const FEchobeatsFullState& EchobeatsState)
{
    // Map echobeats streams to vertices
    for (const FCognitiveStreamState& StreamState : EchobeatsState.StreamStates)
    {
        EThreadVertex Vertex = MapStreamToVertex(StreamState.StreamType);
        SetVertexActivation(Vertex, StreamState.ActivationLevel);

        if (StreamState.ProcessingState.Num() > 0)
        {
            ProcessVertex(Vertex, StreamState.ProcessingState);
        }
    }

    // Map tensional couplings to edges
    for (const FTensionalCoupling& Coupling : EchobeatsState.ActiveCouplings)
    {
        if (Coupling.bIsActive)
        {
            EDyadicEdge Edge = MapCouplingToEdge(Coupling);
            SetEdgeTension(Edge, Coupling.CouplingStrength);
        }
    }

    // Activate appropriate face based on step
    // Steps 1-3: Face_123, Steps 4-6: Face_124, Steps 7-9: Face_134, Steps 10-12: Face_234
    int32 FaceIndex = (Step - 1) / 3;
    if (FaceIndex >= 0 && FaceIndex < 4)
    {
        ActivateFace(static_cast<ETriadicFace>(FaceIndex), EchobeatsState.OverallCoherence);
    }
}

EThreadVertex UTensionalCouplingDynamics::MapStreamToVertex(ECognitiveStreamType StreamType) const
{
    switch (StreamType)
    {
    case ECognitiveStreamType::Pivotal:
        return EThreadVertex::V1_Perception;
    case ECognitiveStreamType::Affordance:
        return EThreadVertex::V2_Memory;
    case ECognitiveStreamType::Salience:
        return EThreadVertex::V3_Assessment;
    default:
        return EThreadVertex::V4_Planning;
    }
}

EDyadicEdge UTensionalCouplingDynamics::MapCouplingToEdge(const FTensionalCoupling& Coupling) const
{
    // Map based on term types
    if ((Coupling.TermA == ESystem4Term::T4_SensoryInput && Coupling.TermB == ESystem4Term::T7_MemoryEncoding) ||
        (Coupling.TermA == ESystem4Term::T7_MemoryEncoding && Coupling.TermB == ESystem4Term::T4_SensoryInput))
    {
        return EDyadicEdge::Edge_12; // Perception-Memory
    }
    else if ((Coupling.TermA == ESystem4Term::T1_Perception && Coupling.TermB == ESystem4Term::T2_IdeaFormation) ||
             (Coupling.TermA == ESystem4Term::T2_IdeaFormation && Coupling.TermB == ESystem4Term::T1_Perception))
    {
        return EDyadicEdge::Edge_13; // Perception-Assessment
    }
    else if (Coupling.TermA == ESystem4Term::T8_BalancedResponse || Coupling.TermB == ESystem4Term::T8_BalancedResponse)
    {
        return EDyadicEdge::Edge_34; // Assessment-Planning
    }

    return EDyadicEdge::Edge_14; // Default
}

void UTensionalCouplingDynamics::ComputeTetrahedralCoherence()
{
    float TotalCoherence = 0.0f;

    for (FTriadicFaceState& Face : State.FaceStates)
    {
        TotalCoherence += ComputeFaceCoherence(Face.FaceType);
    }

    State.TetrahedralCoherence = TotalCoherence / State.FaceStates.Num();
}

void UTensionalCouplingDynamics::ComputeTensionBalance()
{
    float TotalTension = 0.0f;

    for (const FDyadicEdgeState& Edge : State.EdgeStates)
    {
        TotalTension += Edge.Tension;
    }

    State.TensionBalance = TotalTension / State.EdgeStates.Num();
}

void UTensionalCouplingDynamics::PropagateAllTensions()
{
    for (int32 i = 0; i < State.EdgeStates.Num(); ++i)
    {
        PropagateTension(static_cast<EDyadicEdge>(i));
    }
}

void UTensionalCouplingDynamics::UpdateEntanglements(float DeltaTime)
{
    // Update existing entanglements
    for (FCouplingEntanglement& Ent : State.ActiveEntanglements)
    {
        int32 IndexA = GetEdgeIndex(Ent.EdgeA);
        int32 IndexB = GetEdgeIndex(Ent.EdgeB);

        if (IndexA >= 0 && IndexB >= 0)
        {
            // Synchronize entangled edge tensions
            float AvgTension = (State.EdgeStates[IndexA].Tension + State.EdgeStates[IndexB].Tension) * 0.5f;
            State.EdgeStates[IndexA].Tension = FMath::Lerp(State.EdgeStates[IndexA].Tension, AvgTension, 0.1f);
            State.EdgeStates[IndexB].Tension = FMath::Lerp(State.EdgeStates[IndexB].Tension, AvgTension, 0.1f);

            // Update phase correlation
            float TensionDiff = FMath::Abs(State.EdgeStates[IndexA].Tension - State.EdgeStates[IndexB].Tension);
            Ent.PhaseCorrelation = 1.0f - TensionDiff;

            // Update entanglement strength
            Ent.EntanglementStrength = Ent.PhaseCorrelation;

            // Break entanglement if correlation drops too low
            if (Ent.PhaseCorrelation < EntanglementThreshold * 0.5f)
            {
                Ent.bIsEntangled = false;
            }
        }
    }

    // Remove broken entanglements
    State.ActiveEntanglements.RemoveAll([](const FCouplingEntanglement& Ent) {
        return !Ent.bIsEntangled;
    });

    // Detect new potential entanglements
    DetectPotentialEntanglements();
}

int32 UTensionalCouplingDynamics::GetEdgeIndex(EDyadicEdge Edge) const
{
    return static_cast<int32>(Edge);
}

int32 UTensionalCouplingDynamics::GetFaceIndex(ETriadicFace Face) const
{
    return static_cast<int32>(Face);
}

int32 UTensionalCouplingDynamics::GetVertexIndex(EThreadVertex Vertex) const
{
    return static_cast<int32>(Vertex);
}

TArray<EDyadicEdge> UTensionalCouplingDynamics::GetEdgesForFace(ETriadicFace Face) const
{
    int32 Index = GetFaceIndex(Face);
    if (Index >= 0 && Index < State.FaceStates.Num())
    {
        return State.FaceStates[Index].Edges;
    }
    return TArray<EDyadicEdge>();
}

TArray<ETriadicFace> UTensionalCouplingDynamics::GetFacesForEdge(EDyadicEdge Edge) const
{
    TArray<ETriadicFace> Faces;
    for (const FTriadicFaceState& Face : State.FaceStates)
    {
        if (Face.Edges.Contains(Edge))
        {
            Faces.Add(Face.FaceType);
        }
    }
    return Faces;
}

TArray<EThreadVertex> UTensionalCouplingDynamics::GetVerticesForEdge(EDyadicEdge Edge) const
{
    int32 Index = GetEdgeIndex(Edge);
    if (Index >= 0 && Index < State.EdgeStates.Num())
    {
        return {State.EdgeStates[Index].VertexA, State.EdgeStates[Index].VertexB};
    }
    return TArray<EThreadVertex>();
}

TArray<EDyadicEdge> UTensionalCouplingDynamics::GetEdgesForVertex(EThreadVertex Vertex) const
{
    TArray<EDyadicEdge> Edges;
    for (const FDyadicEdgeState& Edge : State.EdgeStates)
    {
        if (Edge.VertexA == Vertex || Edge.VertexB == Vertex)
        {
            Edges.Add(Edge.EdgeType);
        }
    }
    return Edges;
}

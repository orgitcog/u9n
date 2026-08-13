#include "TetradicReservoirIntegration.h"

UTetradicReservoirIntegration::UTetradicReservoirIntegration()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.016f; // ~60Hz

    StepDuration = TransformationCycleDuration / 6.0f;
}

void UTetradicReservoirIntegration::BeginPlay()
{
    Super::BeginPlay();
    InitializeComponentReferences();
    InitializeTetradicStructure();
}

void UTetradicReservoirIntegration::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableTetradicIntegration)
        return;

    // Update transformation timer
    TransformationTimer += DeltaTime;

    // Check for step advancement
    if (TransformationTimer >= StepDuration)
    {
        TransformationTimer -= StepDuration;
        AdvanceTransformationStep();
    }

    // Continuous updates
    UpdateAllEdges();
    UpdateAllFaces();
    CheckCoherenceChanges();

    // Map echobeats if bridge available
    if (CognitiveBridge)
    {
        MapEchobeatsToTetradic();
    }
}

void UTetradicReservoirIntegration::InitializeComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        CognitiveBridge = Owner->FindComponentByClass<UDeepCognitiveBridge>();
        BaseReservoir = Owner->FindComponentByClass<UDeepTreeEchoReservoir>();
    }
}

void UTetradicReservoirIntegration::InitializeTetradicStructure()
{
    StepDuration = TransformationCycleDuration / 6.0f;

    InitializeVertexReservoirs();
    InitializeDyadicEdges();
    InitializeTriadicFaces();

    // Initialize coherence tracking
    PreviousFaceCoherences.Add(ETriadicFace::PAC, 0.5f);
    PreviousFaceCoherences.Add(ETriadicFace::PAM, 0.5f);
    PreviousFaceCoherences.Add(ETriadicFace::PCM, 0.5f);
    PreviousFaceCoherences.Add(ETriadicFace::ACM, 0.5f);
}

void UTetradicReservoirIntegration::InitializeVertexReservoirs()
{
    TetrahedralState.Vertices.Empty();

    // V1: Perception
    FVertexReservoirState V1;
    V1.Vertex = ETetradicVertex::Perception;
    V1.ReservoirState.ReservoirID = TEXT("V1_Perception");
    V1.ReservoirState.Units = UnitsPerVertex;
    V1.ReservoirState.SpectralRadius = 0.9f;
    V1.ReservoirState.LeakRate = 0.3f;
    V1.ReservoirState.ActivationState.SetNum(UnitsPerVertex);
    V1.AdjacentCouplings = {InterVertexCoupling, InterVertexCoupling, InterVertexCoupling};
    TetrahedralState.Vertices.Add(V1);

    // V2: Action
    FVertexReservoirState V2;
    V2.Vertex = ETetradicVertex::Action;
    V2.ReservoirState.ReservoirID = TEXT("V2_Action");
    V2.ReservoirState.Units = UnitsPerVertex;
    V2.ReservoirState.SpectralRadius = 0.85f;
    V2.ReservoirState.LeakRate = 0.35f;
    V2.ReservoirState.ActivationState.SetNum(UnitsPerVertex);
    V2.AdjacentCouplings = {InterVertexCoupling, InterVertexCoupling, InterVertexCoupling};
    TetrahedralState.Vertices.Add(V2);

    // V3: Cognition
    FVertexReservoirState V3;
    V3.Vertex = ETetradicVertex::Cognition;
    V3.ReservoirState.ReservoirID = TEXT("V3_Cognition");
    V3.ReservoirState.Units = UnitsPerVertex;
    V3.ReservoirState.SpectralRadius = 0.95f;
    V3.ReservoirState.LeakRate = 0.25f;
    V3.ReservoirState.ActivationState.SetNum(UnitsPerVertex);
    V3.AdjacentCouplings = {InterVertexCoupling, InterVertexCoupling, InterVertexCoupling};
    TetrahedralState.Vertices.Add(V3);

    // V4: Memory
    FVertexReservoirState V4;
    V4.Vertex = ETetradicVertex::Memory;
    V4.ReservoirState.ReservoirID = TEXT("V4_Memory");
    V4.ReservoirState.Units = UnitsPerVertex;
    V4.ReservoirState.SpectralRadius = 0.98f;
    V4.ReservoirState.LeakRate = 0.15f;
    V4.ReservoirState.ActivationState.SetNum(UnitsPerVertex);
    V4.AdjacentCouplings = {InterVertexCoupling, InterVertexCoupling, InterVertexCoupling};
    TetrahedralState.Vertices.Add(V4);
}

void UTetradicReservoirIntegration::InitializeDyadicEdges()
{
    TetrahedralState.Edges.Empty();

    // E1: Perception-Action
    FDyadicEdgeState E1;
    E1.Edge = EDyadicEdge::PA;
    E1.SourceVertex = ETetradicVertex::Perception;
    E1.TargetVertex = ETetradicVertex::Action;
    E1.CouplingStrength = InterVertexCoupling;
    E1.FlowState.SetNum(UnitsPerVertex);
    TetrahedralState.Edges.Add(E1);

    // E2: Perception-Cognition
    FDyadicEdgeState E2;
    E2.Edge = EDyadicEdge::PC;
    E2.SourceVertex = ETetradicVertex::Perception;
    E2.TargetVertex = ETetradicVertex::Cognition;
    E2.CouplingStrength = InterVertexCoupling;
    E2.FlowState.SetNum(UnitsPerVertex);
    TetrahedralState.Edges.Add(E2);

    // E3: Perception-Memory
    FDyadicEdgeState E3;
    E3.Edge = EDyadicEdge::PM;
    E3.SourceVertex = ETetradicVertex::Perception;
    E3.TargetVertex = ETetradicVertex::Memory;
    E3.CouplingStrength = InterVertexCoupling;
    E3.FlowState.SetNum(UnitsPerVertex);
    TetrahedralState.Edges.Add(E3);

    // E4: Action-Cognition
    FDyadicEdgeState E4;
    E4.Edge = EDyadicEdge::AC;
    E4.SourceVertex = ETetradicVertex::Action;
    E4.TargetVertex = ETetradicVertex::Cognition;
    E4.CouplingStrength = InterVertexCoupling;
    E4.FlowState.SetNum(UnitsPerVertex);
    TetrahedralState.Edges.Add(E4);

    // E5: Action-Memory
    FDyadicEdgeState E5;
    E5.Edge = EDyadicEdge::AM;
    E5.SourceVertex = ETetradicVertex::Action;
    E5.TargetVertex = ETetradicVertex::Memory;
    E5.CouplingStrength = InterVertexCoupling;
    E5.FlowState.SetNum(UnitsPerVertex);
    TetrahedralState.Edges.Add(E5);

    // E6: Cognition-Memory
    FDyadicEdgeState E6;
    E6.Edge = EDyadicEdge::CM;
    E6.SourceVertex = ETetradicVertex::Cognition;
    E6.TargetVertex = ETetradicVertex::Memory;
    E6.CouplingStrength = InterVertexCoupling;
    E6.FlowState.SetNum(UnitsPerVertex);
    TetrahedralState.Edges.Add(E6);
}

void UTetradicReservoirIntegration::InitializeTriadicFaces()
{
    TetrahedralState.Faces.Empty();

    // F1: Perception-Action-Cognition (PAC)
    FTriadicFaceState F1;
    F1.Face = ETriadicFace::PAC;
    F1.Vertices = {ETetradicVertex::Perception, ETetradicVertex::Action, ETetradicVertex::Cognition};
    F1.Edges = {EDyadicEdge::PA, EDyadicEdge::PC, EDyadicEdge::AC};
    F1.Orientation = FVector(1.0f, 1.0f, 1.0f).GetSafeNormal();
    F1.CombinedState.SetNum(UnitsPerVertex);
    TetrahedralState.Faces.Add(F1);

    // F2: Perception-Action-Memory (PAM)
    FTriadicFaceState F2;
    F2.Face = ETriadicFace::PAM;
    F2.Vertices = {ETetradicVertex::Perception, ETetradicVertex::Action, ETetradicVertex::Memory};
    F2.Edges = {EDyadicEdge::PA, EDyadicEdge::PM, EDyadicEdge::AM};
    F2.Orientation = FVector(1.0f, 1.0f, -1.0f).GetSafeNormal();
    F2.CombinedState.SetNum(UnitsPerVertex);
    TetrahedralState.Faces.Add(F2);

    // F3: Perception-Cognition-Memory (PCM)
    FTriadicFaceState F3;
    F3.Face = ETriadicFace::PCM;
    F3.Vertices = {ETetradicVertex::Perception, ETetradicVertex::Cognition, ETetradicVertex::Memory};
    F3.Edges = {EDyadicEdge::PC, EDyadicEdge::PM, EDyadicEdge::CM};
    F3.Orientation = FVector(1.0f, -1.0f, 1.0f).GetSafeNormal();
    F3.CombinedState.SetNum(UnitsPerVertex);
    TetrahedralState.Faces.Add(F3);

    // F4: Action-Cognition-Memory (ACM)
    FTriadicFaceState F4;
    F4.Face = ETriadicFace::ACM;
    F4.Vertices = {ETetradicVertex::Action, ETetradicVertex::Cognition, ETetradicVertex::Memory};
    F4.Edges = {EDyadicEdge::AC, EDyadicEdge::AM, EDyadicEdge::CM};
    F4.Orientation = FVector(-1.0f, 1.0f, 1.0f).GetSafeNormal();
    F4.CombinedState.SetNum(UnitsPerVertex);
    TetrahedralState.Faces.Add(F4);
}

void UTetradicReservoirIntegration::UpdateVertex(ETetradicVertex Vertex, const TArray<float>& Input)
{
    int32 Index = GetVertexIndex(Vertex);
    if (Index < 0 || Index >= TetrahedralState.Vertices.Num())
        return;

    FVertexReservoirState& VertexState = TetrahedralState.Vertices[Index];
    FReservoirState& Reservoir = VertexState.ReservoirState;

    // Leaky integration update
    for (int32 i = 0; i < Reservoir.ActivationState.Num() && i < Input.Num(); i++)
    {
        Reservoir.ActivationState[i] = (1.0f - Reservoir.LeakRate) * Reservoir.ActivationState[i]
            + Reservoir.LeakRate * FMath::Tanh(Input[i] * Reservoir.InputScaling);
    }

    // Compute activation level
    float TotalActivation = 0.0f;
    for (float Val : Reservoir.ActivationState)
    {
        TotalActivation += FMath::Abs(Val);
    }
    VertexState.ActivationLevel = TotalActivation / Reservoir.ActivationState.Num();

    Reservoir.bIsInitialized = true;
    Reservoir.LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
}

FVertexReservoirState UTetradicReservoirIntegration::GetVertexState(ETetradicVertex Vertex) const
{
    int32 Index = GetVertexIndex(Vertex);
    if (Index >= 0 && Index < TetrahedralState.Vertices.Num())
    {
        return TetrahedralState.Vertices[Index];
    }
    return FVertexReservoirState();
}

TArray<ETetradicVertex> UTetradicReservoirIntegration::GetVerticesForFace(ETriadicFace Face) const
{
    int32 Index = GetFaceIndex(Face);
    if (Index >= 0 && Index < TetrahedralState.Faces.Num())
    {
        return TetrahedralState.Faces[Index].Vertices;
    }
    return TArray<ETetradicVertex>();
}

void UTetradicReservoirIntegration::UpdateEdge(EDyadicEdge Edge)
{
    int32 Index = GetEdgeIndex(Edge);
    if (Index < 0 || Index >= TetrahedralState.Edges.Num())
        return;

    FDyadicEdgeState& EdgeState = TetrahedralState.Edges[Index];

    // Get source and target vertex states
    FVertexReservoirState SourceState = GetVertexState(EdgeState.SourceVertex);
    FVertexReservoirState TargetState = GetVertexState(EdgeState.TargetVertex);

    // Compute bidirectional flow
    for (int32 i = 0; i < EdgeState.FlowState.Num(); i++)
    {
        float SourceVal = i < SourceState.ReservoirState.ActivationState.Num() ? 
            SourceState.ReservoirState.ActivationState[i] : 0.0f;
        float TargetVal = i < TargetState.ReservoirState.ActivationState.Num() ? 
            TargetState.ReservoirState.ActivationState[i] : 0.0f;

        // Flow is difference weighted by coupling
        EdgeState.FlowState[i] = EdgeState.CouplingStrength * (SourceVal - TargetVal);
    }
}

FDyadicEdgeState UTetradicReservoirIntegration::GetEdgeState(EDyadicEdge Edge) const
{
    int32 Index = GetEdgeIndex(Edge);
    if (Index >= 0 && Index < TetrahedralState.Edges.Num())
    {
        return TetrahedralState.Edges[Index];
    }
    return FDyadicEdgeState();
}

TArray<EDyadicEdge> UTetradicReservoirIntegration::GetEdgesForFace(ETriadicFace Face) const
{
    int32 Index = GetFaceIndex(Face);
    if (Index >= 0 && Index < TetrahedralState.Faces.Num())
    {
        return TetrahedralState.Faces[Index].Edges;
    }
    return TArray<EDyadicEdge>();
}

TArray<float> UTetradicReservoirIntegration::ComputeEdgeFlow(EDyadicEdge Edge) const
{
    FDyadicEdgeState EdgeState = GetEdgeState(Edge);
    return EdgeState.FlowState;
}

void UTetradicReservoirIntegration::UpdateFace(ETriadicFace Face)
{
    int32 Index = GetFaceIndex(Face);
    if (Index < 0 || Index >= TetrahedralState.Faces.Num())
        return;

    FTriadicFaceState& FaceState = TetrahedralState.Faces[Index];

    // Combine vertex states
    for (int32 i = 0; i < FaceState.CombinedState.Num(); i++)
    {
        float Sum = 0.0f;
        for (ETetradicVertex Vertex : FaceState.Vertices)
        {
            FVertexReservoirState VertexState = GetVertexState(Vertex);
            if (i < VertexState.ReservoirState.ActivationState.Num())
            {
                Sum += VertexState.ReservoirState.ActivationState[i];
            }
        }
        FaceState.CombinedState[i] = Sum / 3.0f;
    }

    // Update coherence
    FaceState.Coherence = ComputeFaceCoherence(Face);
}

FTriadicFaceState UTetradicReservoirIntegration::GetFaceState(ETriadicFace Face) const
{
    int32 Index = GetFaceIndex(Face);
    if (Index >= 0 && Index < TetrahedralState.Faces.Num())
    {
        return TetrahedralState.Faces[Index];
    }
    return FTriadicFaceState();
}

float UTetradicReservoirIntegration::ComputeFaceCoherence(ETriadicFace Face) const
{
    TArray<ETetradicVertex> Vertices = GetVerticesForFace(Face);
    if (Vertices.Num() < 3)
        return 0.0f;

    // Compute pairwise coherence between vertices
    float TotalCoherence = 0.0f;
    int32 PairCount = 0;

    for (int32 i = 0; i < Vertices.Num(); i++)
    {
        for (int32 j = i + 1; j < Vertices.Num(); j++)
        {
            FVertexReservoirState V1 = GetVertexState(Vertices[i]);
            FVertexReservoirState V2 = GetVertexState(Vertices[j]);

            const TArray<float>& A = V1.ReservoirState.ActivationState;
            const TArray<float>& B = V2.ReservoirState.ActivationState;

            float DotProduct = 0.0f;
            float NormA = 0.0f;
            float NormB = 0.0f;

            int32 MinSize = FMath::Min(A.Num(), B.Num());
            for (int32 k = 0; k < MinSize; k++)
            {
                DotProduct += A[k] * B[k];
                NormA += A[k] * A[k];
                NormB += B[k] * B[k];
            }

            if (NormA > 0.0f && NormB > 0.0f)
            {
                TotalCoherence += DotProduct / (FMath::Sqrt(NormA) * FMath::Sqrt(NormB));
            }
            PairCount++;
        }
    }

    return PairCount > 0 ? TotalCoherence / PairCount : 0.0f;
}

void UTetradicReservoirIntegration::AlignFaceOrientations()
{
    // Ensure face orientations are mutually orthogonal
    // Tetrahedral faces have orientations pointing to opposite vertices
    if (TetrahedralState.Faces.Num() < 4)
        return;

    // Standard tetrahedral face normals
    TetrahedralState.Faces[0].Orientation = FVector(1.0f, 1.0f, 1.0f).GetSafeNormal();
    TetrahedralState.Faces[1].Orientation = FVector(1.0f, -1.0f, -1.0f).GetSafeNormal();
    TetrahedralState.Faces[2].Orientation = FVector(-1.0f, 1.0f, -1.0f).GetSafeNormal();
    TetrahedralState.Faces[3].Orientation = FVector(-1.0f, -1.0f, 1.0f).GetSafeNormal();
}

void UTetradicReservoirIntegration::AdvanceTransformationStep()
{
    // Advance step (1-6 cycle)
    TetrahedralState.TransformationStep = (TetrahedralState.TransformationStep % 6) + 1;

    // Apply transformation
    ApplyTransformation();

    // Broadcast event
    OnTransformationAdvanced.Broadcast(TetrahedralState.TransformationStep);
}

void UTetradicReservoirIntegration::ApplyTransformation()
{
    // 6-step transformation cycle
    // Each step activates different face combinations
    switch (TetrahedralState.TransformationStep)
    {
        case 1: // PAC primary
            UpdateFace(ETriadicFace::PAC);
            break;
        case 2: // PAM primary
            UpdateFace(ETriadicFace::PAM);
            break;
        case 3: // PCM primary
            UpdateFace(ETriadicFace::PCM);
            break;
        case 4: // ACM primary
            UpdateFace(ETriadicFace::ACM);
            break;
        case 5: // Synchronize all
            SynchronizeTetrahedralState();
            break;
        case 6: // Global coherence check
            TetrahedralState.GlobalCoherence = ComputeGlobalCoherence();
            break;
    }
}

float UTetradicReservoirIntegration::ComputeGlobalCoherence() const
{
    // Average coherence across all faces
    float TotalCoherence = 0.0f;
    for (const FTriadicFaceState& Face : TetrahedralState.Faces)
    {
        TotalCoherence += Face.Coherence;
    }
    return TetrahedralState.Faces.Num() > 0 ? TotalCoherence / TetrahedralState.Faces.Num() : 0.0f;
}

void UTetradicReservoirIntegration::SynchronizeTetrahedralState()
{
    // Compute global average state
    TArray<float> GlobalAverage;
    GlobalAverage.SetNum(UnitsPerVertex);

    for (const FVertexReservoirState& Vertex : TetrahedralState.Vertices)
    {
        for (int32 i = 0; i < GlobalAverage.Num() && i < Vertex.ReservoirState.ActivationState.Num(); i++)
        {
            GlobalAverage[i] += Vertex.ReservoirState.ActivationState[i] / 4.0f;
        }
    }

    // Apply synchronization with coupling
    for (FVertexReservoirState& Vertex : TetrahedralState.Vertices)
    {
        for (int32 i = 0; i < Vertex.ReservoirState.ActivationState.Num() && i < GlobalAverage.Num(); i++)
        {
            Vertex.ReservoirState.ActivationState[i] = FMath::Lerp(
                Vertex.ReservoirState.ActivationState[i],
                GlobalAverage[i],
                InterVertexCoupling * 0.5f
            );
        }
    }

    // Update global coherence
    TetrahedralState.GlobalCoherence = ComputeGlobalCoherence();
}

void UTetradicReservoirIntegration::MapEchobeatsToTetradic()
{
    if (!CognitiveBridge)
        return;

    // Map echobeats streams to tetradic vertices
    // Pivotal -> Perception
    // Affordance -> Action
    // Salience -> Cognition
    // Combined -> Memory

    FConcurrentStreamState PivotalState = CognitiveBridge->GetStreamState(ECognitiveStreamType::Pivotal);
    FConcurrentStreamState AffordanceState = CognitiveBridge->GetStreamState(ECognitiveStreamType::Affordance);
    FConcurrentStreamState SalienceState = CognitiveBridge->GetStreamState(ECognitiveStreamType::Salience);

    // Update vertices with stream states
    if (PivotalState.ReservoirState.ActivationState.Num() > 0)
    {
        UpdateVertex(ETetradicVertex::Perception, PivotalState.ReservoirState.ActivationState);
    }

    if (AffordanceState.ReservoirState.ActivationState.Num() > 0)
    {
        UpdateVertex(ETetradicVertex::Action, AffordanceState.ReservoirState.ActivationState);
    }

    if (SalienceState.ReservoirState.ActivationState.Num() > 0)
    {
        UpdateVertex(ETetradicVertex::Cognition, SalienceState.ReservoirState.ActivationState);
    }

    // Memory is combination of all three
    TArray<float> MemoryInput;
    MemoryInput.SetNum(UnitsPerVertex);
    for (int32 i = 0; i < MemoryInput.Num(); i++)
    {
        float P = i < PivotalState.ReservoirState.ActivationState.Num() ? 
            PivotalState.ReservoirState.ActivationState[i] : 0.0f;
        float A = i < AffordanceState.ReservoirState.ActivationState.Num() ? 
            AffordanceState.ReservoirState.ActivationState[i] : 0.0f;
        float S = i < SalienceState.ReservoirState.ActivationState.Num() ? 
            SalienceState.ReservoirState.ActivationState[i] : 0.0f;
        MemoryInput[i] = (P + A + S) / 3.0f;
    }
    UpdateVertex(ETetradicVertex::Memory, MemoryInput);
}

ETriadicFace UTetradicReservoirIntegration::GetFaceForEchobeatStep(int32 Step) const
{
    // Map 12-step echobeats to 4 triadic faces
    // Steps 1-3 -> PAC
    // Steps 4-6 -> PAM
    // Steps 7-9 -> PCM
    // Steps 10-12 -> ACM
    int32 FaceIndex = ((Step - 1) / 3) % 4;
    switch (FaceIndex)
    {
        case 0: return ETriadicFace::PAC;
        case 1: return ETriadicFace::PAM;
        case 2: return ETriadicFace::PCM;
        case 3: return ETriadicFace::ACM;
        default: return ETriadicFace::PAC;
    }
}

int32 UTetradicReservoirIntegration::GetVertexIndex(ETetradicVertex Vertex) const
{
    return static_cast<int32>(Vertex);
}

int32 UTetradicReservoirIntegration::GetEdgeIndex(EDyadicEdge Edge) const
{
    return static_cast<int32>(Edge);
}

int32 UTetradicReservoirIntegration::GetFaceIndex(ETriadicFace Face) const
{
    return static_cast<int32>(Face);
}

void UTetradicReservoirIntegration::ComputeVertexCoupling(ETetradicVertex Source, ETetradicVertex Target)
{
    FVertexReservoirState SourceState = GetVertexState(Source);
    int32 TargetIndex = GetVertexIndex(Target);

    if (TargetIndex < 0 || TargetIndex >= TetrahedralState.Vertices.Num())
        return;

    FVertexReservoirState& TargetState = TetrahedralState.Vertices[TargetIndex];

    // Apply coupling influence
    for (int32 i = 0; i < TargetState.ReservoirState.ActivationState.Num() && 
         i < SourceState.ReservoirState.ActivationState.Num(); i++)
    {
        TargetState.ReservoirState.ActivationState[i] += 
            InterVertexCoupling * SourceState.ReservoirState.ActivationState[i] * 0.1f;
    }
}

void UTetradicReservoirIntegration::UpdateAllEdges()
{
    for (int32 i = 0; i < TetrahedralState.Edges.Num(); i++)
    {
        UpdateEdge(TetrahedralState.Edges[i].Edge);
    }
}

void UTetradicReservoirIntegration::UpdateAllFaces()
{
    for (int32 i = 0; i < TetrahedralState.Faces.Num(); i++)
    {
        UpdateFace(TetrahedralState.Faces[i].Face);
    }
}

void UTetradicReservoirIntegration::CheckCoherenceChanges()
{
    const float CoherenceChangeThreshold = 0.1f;

    for (const FTriadicFaceState& Face : TetrahedralState.Faces)
    {
        float* PreviousCoherence = PreviousFaceCoherences.Find(Face.Face);
        if (PreviousCoherence)
        {
            if (FMath::Abs(Face.Coherence - *PreviousCoherence) > CoherenceChangeThreshold)
            {
                OnFaceCoherenceChanged.Broadcast(Face.Face, Face.Coherence);
                *PreviousCoherence = Face.Coherence;
            }
        }
    }
}

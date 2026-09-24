#include "System5CognitiveIntegration.h"

/**
 * System 5 Cognitive Integration Implementation
 * 
 * Implements the tetradic/tetrahedral cognitive architecture following
 * the System 5 Deterministic State Transition Model with OEIS A000081 nesting.
 */

USystem5CognitiveIntegration::USystem5CognitiveIntegration()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void USystem5CognitiveIntegration::BeginPlay()
{
    Super::BeginPlay();
    
    InitializeTetrahedralStructure();
    InitializeNestedShells();
}

void USystem5CognitiveIntegration::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableTetradicProcessing)
    {
        return;
    }

    StepTimer += DeltaTime;
    if (StepTimer >= StepDuration)
    {
        StepTimer = 0.0f;
        ExecuteTransformationStep();
    }
}

// ========================================
// INITIALIZATION
// ========================================

void USystem5CognitiveIntegration::InitializeTetrahedralStructure()
{
    // Initialize 4 monadic vertices
    TetrahedralState.Vertices.SetNum(4);
    for (int32 i = 0; i < 4; ++i)
    {
        TetrahedralState.Vertices[i].VertexId = i;
        TetrahedralState.Vertices[i].Activation = 0.0f;
        TetrahedralState.Vertices[i].StateVector.SetNum(9); // 9 terms at max nesting
    }

    // Initialize 6 dyadic edges
    // Edge connectivity: 0-1, 0-2, 0-3, 1-2, 1-3, 2-3
    TetrahedralState.Edges.SetNum(6);
    
    int32 EdgeIndex = 0;
    for (int32 i = 0; i < 4; ++i)
    {
        for (int32 j = i + 1; j < 4; ++j)
        {
            TetrahedralState.Edges[EdgeIndex].EdgeId = EdgeIndex;
            TetrahedralState.Edges[EdgeIndex].Vertex1 = i;
            TetrahedralState.Edges[EdgeIndex].Vertex2 = j;
            TetrahedralState.Edges[EdgeIndex].Weight = 1.0f;
            EdgeIndex++;
        }
    }

    // Initialize 4 triadic faces
    // Face 0: vertices 0,1,2 (excludes vertex 3)
    // Face 1: vertices 0,1,3 (excludes vertex 2)
    // Face 2: vertices 0,2,3 (excludes vertex 1)
    // Face 3: vertices 1,2,3 (excludes vertex 0)
    TetrahedralState.Faces.SetNum(4);
    
    // Face 0: 0,1,2
    TetrahedralState.Faces[0].FaceId = 0;
    TetrahedralState.Faces[0].Vertices = {0, 1, 2};
    TetrahedralState.Faces[0].Edges = {0, 3, 1}; // 0-1, 1-2, 0-2
    TetrahedralState.Faces[0].AdjacentFaces = {1, 2, 3};
    
    // Face 1: 0,1,3
    TetrahedralState.Faces[1].FaceId = 1;
    TetrahedralState.Faces[1].Vertices = {0, 1, 3};
    TetrahedralState.Faces[1].Edges = {0, 4, 2}; // 0-1, 1-3, 0-3
    TetrahedralState.Faces[1].AdjacentFaces = {0, 2, 3};
    
    // Face 2: 0,2,3
    TetrahedralState.Faces[2].FaceId = 2;
    TetrahedralState.Faces[2].Vertices = {0, 2, 3};
    TetrahedralState.Faces[2].Edges = {1, 5, 2}; // 0-2, 2-3, 0-3
    TetrahedralState.Faces[2].AdjacentFaces = {0, 1, 3};
    
    // Face 3: 1,2,3
    TetrahedralState.Faces[3].FaceId = 3;
    TetrahedralState.Faces[3].Vertices = {1, 2, 3};
    TetrahedralState.Faces[3].Edges = {3, 5, 4}; // 1-2, 2-3, 1-3
    TetrahedralState.Faces[3].AdjacentFaces = {0, 1, 2};

    BuildTetrahedralConnectivity();
    AlignFaceOrientations();

    UE_LOG(LogTemp, Log, TEXT("System5CognitiveIntegration: Tetrahedral structure initialized"));
}

void USystem5CognitiveIntegration::InitializeNestedShells()
{
    // OEIS A000081: Number of rooted trees with n nodes
    // Level 1: 1 term
    // Level 2: 2 terms
    // Level 3: 4 terms
    // Level 4: 9 terms
    
    NestedShells.SetNum(4);
    
    // Level 1: 1 term (global context)
    NestedShells[0].Level = 1;
    NestedShells[0].TermCount = 1;
    NestedShells[0].Terms = {TEXT("Global")};
    NestedShells[0].StepsApart = 1;
    
    // Level 2: 2 terms (local context)
    NestedShells[1].Level = 2;
    NestedShells[1].TermCount = 2;
    NestedShells[1].Terms = {TEXT("Universal"), TEXT("Particular")};
    NestedShells[1].StepsApart = 2;
    
    // Level 3: 4 terms (detailed processing)
    NestedShells[2].Level = 3;
    NestedShells[2].TermCount = 4;
    NestedShells[2].Terms = {TEXT("Discretion"), TEXT("Means"), TEXT("Goals"), TEXT("Consequence")};
    NestedShells[2].StepsApart = 3;
    
    // Level 4: 9 terms (fine-grained)
    NestedShells[3].Level = 4;
    NestedShells[3].TermCount = 9;
    NestedShells[3].Terms = {
        TEXT("Perception"), TEXT("Action"), TEXT("Reflection"),
        TEXT("Embodied"), TEXT("Embedded"), TEXT("Enacted"),
        TEXT("Extended"), TEXT("Salience"), TEXT("Relevance")
    };
    NestedShells[3].StepsApart = 4;

    CurrentNestingLevel = 1;

    UE_LOG(LogTemp, Log, TEXT("System5CognitiveIntegration: Nested shells initialized (OEIS A000081)"));
}

void USystem5CognitiveIntegration::BuildTetrahedralConnectivity()
{
    // Update vertex connectivity
    for (int32 i = 0; i < 4; ++i)
    {
        TetrahedralState.Vertices[i].ConnectedEdges.Reset();
        TetrahedralState.Vertices[i].ParticipatingFaces.Reset();
    }

    // Map edges to vertices
    for (int32 e = 0; e < 6; ++e)
    {
        int32 v1 = TetrahedralState.Edges[e].Vertex1;
        int32 v2 = TetrahedralState.Edges[e].Vertex2;
        
        TetrahedralState.Vertices[v1].ConnectedEdges.Add(e);
        TetrahedralState.Vertices[v2].ConnectedEdges.Add(e);
    }

    // Map faces to vertices and edges
    for (int32 f = 0; f < 4; ++f)
    {
        for (int32 v : TetrahedralState.Faces[f].Vertices)
        {
            TetrahedralState.Vertices[v].ParticipatingFaces.Add(f);
        }
        
        for (int32 e : TetrahedralState.Faces[f].Edges)
        {
            TetrahedralState.Edges[e].ParticipatingFaces.AddUnique(f);
        }
    }
}

// ========================================
// TETRAHEDRAL OPERATIONS
// ========================================

void USystem5CognitiveIntegration::ActivateVertex(int32 VertexId, float Activation)
{
    if (VertexId < 0 || VertexId >= 4)
    {
        return;
    }

    TetrahedralState.Vertices[VertexId].Activation = FMath::Clamp(Activation, 0.0f, 1.0f);
    
    // Propagate activation through connected edges
    PropagateActivation();
}

void USystem5CognitiveIntegration::UpdateEdgeWeight(int32 EdgeId, float Weight)
{
    if (EdgeId < 0 || EdgeId >= 6)
    {
        return;
    }

    TetrahedralState.Edges[EdgeId].Weight = FMath::Clamp(Weight, 0.0f, 2.0f);
}

void USystem5CognitiveIntegration::ProcessTriadicTensor(int32 FaceId, const TArray<float>& InputTensor)
{
    if (FaceId < 0 || FaceId >= 4)
    {
        return;
    }

    FTriadicFace& Face = TetrahedralState.Faces[FaceId];
    
    // Process tensor through the triadic structure
    Face.TensorState = InputTensor;
    
    // Update face coherence based on tensor properties
    float TensorMagnitude = 0.0f;
    for (float Val : InputTensor)
    {
        TensorMagnitude += Val * Val;
    }
    TensorMagnitude = FMath::Sqrt(TensorMagnitude);
    
    // Coherence is high when tensor is normalized
    Face.Coherence = FMath::Clamp(1.0f - FMath::Abs(1.0f - TensorMagnitude), 0.0f, 1.0f);
}

void USystem5CognitiveIntegration::ExecuteTransformationStep()
{
    TetrahedralState.TransformationStep++;
    
    // Update tensor bundles
    UpdateTensorBundles();
    
    // Align face orientations
    AlignFaceOrientations();
    
    // Compute global coherence
    TetrahedralState.GlobalCoherence = ComputeGlobalCoherence();
    
    // Check and update symmetry alignment
    TetrahedralState.SymmetryAlignment = CheckOrthogonalSymmetry() ? 1.0f : 0.5f;
    
    // Advance nesting level based on step
    if (bEnableNestedShells && (TetrahedralState.TransformationStep % 4 == 0))
    {
        AdvanceNestingLevel();
    }
}

void USystem5CognitiveIntegration::AlignFaceOrientations()
{
    // Compute face normals to ensure mutually orthogonal symmetries
    for (int32 f = 0; f < 4; ++f)
    {
        TetrahedralState.Faces[f].Normal = ComputeFaceNormal(TetrahedralState.Faces[f]);
    }
    
    // Compute edge orientations
    for (int32 e = 0; e < 6; ++e)
    {
        TetrahedralState.Edges[e].Orientation = ComputeEdgeOrientation(TetrahedralState.Edges[e]);
    }
}

float USystem5CognitiveIntegration::ComputeGlobalCoherence() const
{
    // Average face coherence
    float FaceCoherence = 0.0f;
    for (const FTriadicFace& Face : TetrahedralState.Faces)
    {
        FaceCoherence += Face.Coherence;
    }
    FaceCoherence /= 4.0f;
    
    // Average vertex activation balance
    float ActivationBalance = 0.0f;
    float TotalActivation = 0.0f;
    for (const FMonadicVertex& Vertex : TetrahedralState.Vertices)
    {
        TotalActivation += Vertex.Activation;
    }
    float MeanActivation = TotalActivation / 4.0f;
    
    float ActivationVariance = 0.0f;
    for (const FMonadicVertex& Vertex : TetrahedralState.Vertices)
    {
        float Diff = Vertex.Activation - MeanActivation;
        ActivationVariance += Diff * Diff;
    }
    ActivationVariance /= 4.0f;
    ActivationBalance = 1.0f - FMath::Clamp(ActivationVariance, 0.0f, 1.0f);
    
    // Edge weight balance
    float EdgeBalance = 0.0f;
    float TotalWeight = 0.0f;
    for (const FDyadicEdge& Edge : TetrahedralState.Edges)
    {
        TotalWeight += Edge.Weight;
    }
    float MeanWeight = TotalWeight / 6.0f;
    
    float WeightVariance = 0.0f;
    for (const FDyadicEdge& Edge : TetrahedralState.Edges)
    {
        float Diff = Edge.Weight - MeanWeight;
        WeightVariance += Diff * Diff;
    }
    WeightVariance /= 6.0f;
    EdgeBalance = 1.0f - FMath::Clamp(WeightVariance, 0.0f, 1.0f);
    
    // Combined coherence
    return (FaceCoherence * 0.4f) + (ActivationBalance * 0.3f) + (EdgeBalance * 0.3f);
}

// ========================================
// NESTED SHELLS
// ========================================

int32 USystem5CognitiveIntegration::GetTermCountAtLevel(int32 Level) const
{
    return ComputeOEISTermCount(Level);
}

void USystem5CognitiveIntegration::AdvanceNestingLevel()
{
    CurrentNestingLevel = (CurrentNestingLevel % 4) + 1;
}

TArray<FString> USystem5CognitiveIntegration::GetTermsAtCurrentLevel() const
{
    if (CurrentNestingLevel >= 1 && CurrentNestingLevel <= 4)
    {
        return NestedShells[CurrentNestingLevel - 1].Terms;
    }
    return TArray<FString>();
}

// ========================================
// COGNITIVE LOOP INTEGRATION
// ========================================

void USystem5CognitiveIntegration::MapCycleStepToTetrahedralState(int32 CycleStep)
{
    // Map 12-step cycle to tetrahedral structure
    // Triadic synchronization points: {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
    
    // Determine which face is active based on cycle step
    int32 ActiveFace = GetActiveFaceForCycleStep(CycleStep);
    
    // Activate vertices of the active face
    for (int32 v = 0; v < 4; ++v)
    {
        bool IsInActiveFace = TetrahedralState.Faces[ActiveFace].Vertices.Contains(v);
        float TargetActivation = IsInActiveFace ? 1.0f : 0.3f;
        
        // Smooth transition
        float CurrentActivation = TetrahedralState.Vertices[v].Activation;
        TetrahedralState.Vertices[v].Activation = FMath::Lerp(CurrentActivation, TargetActivation, 0.3f);
    }
    
    // Update nesting level based on cycle step
    // Steps 1-3: Level 1, Steps 4-6: Level 2, Steps 7-9: Level 3, Steps 10-12: Level 4
    CurrentNestingLevel = ((CycleStep - 1) / 3) + 1;
}

int32 USystem5CognitiveIntegration::GetActiveFaceForCycleStep(int32 CycleStep) const
{
    // Map cycle steps to faces
    // Face 0: steps 1, 5, 9 (perceiving triad)
    // Face 1: steps 2, 6, 10 (acting triad)
    // Face 2: steps 3, 7, 11 (reflecting triad)
    // Face 3: steps 4, 8, 12 (integrating triad)
    
    int32 StepMod = ((CycleStep - 1) % 4);
    return StepMod;
}

TArray<int32> USystem5CognitiveIntegration::GetTriadicSynchronizationPoints() const
{
    // Return the triadic synchronization points
    // {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
    return TArray<int32>({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12});
}

// ========================================
// INTERNAL METHODS
// ========================================

FVector USystem5CognitiveIntegration::ComputeFaceNormal(const FTriadicFace& Face) const
{
    // Compute normal from vertex positions (using idealized tetrahedron)
    // Vertices at: (1,1,1), (1,-1,-1), (-1,1,-1), (-1,-1,1)
    
    static const FVector VertexPositions[4] = {
        FVector(1, 1, 1),
        FVector(1, -1, -1),
        FVector(-1, 1, -1),
        FVector(-1, -1, 1)
    };
    
    if (Face.Vertices.Num() < 3)
    {
        return FVector::UpVector;
    }
    
    FVector V0 = VertexPositions[Face.Vertices[0]];
    FVector V1 = VertexPositions[Face.Vertices[1]];
    FVector V2 = VertexPositions[Face.Vertices[2]];
    
    FVector Edge1 = V1 - V0;
    FVector Edge2 = V2 - V0;
    
    return FVector::CrossProduct(Edge1, Edge2).GetSafeNormal();
}

FVector USystem5CognitiveIntegration::ComputeEdgeOrientation(const FDyadicEdge& Edge) const
{
    static const FVector VertexPositions[4] = {
        FVector(1, 1, 1),
        FVector(1, -1, -1),
        FVector(-1, 1, -1),
        FVector(-1, -1, 1)
    };
    
    FVector V1 = VertexPositions[Edge.Vertex1];
    FVector V2 = VertexPositions[Edge.Vertex2];
    
    return (V2 - V1).GetSafeNormal();
}

bool USystem5CognitiveIntegration::CheckOrthogonalSymmetry() const
{
    // Check if face normals maintain proper orthogonal relationships
    // In a regular tetrahedron, opposite edges are perpendicular
    
    // Check edge pairs: (0,5), (1,4), (2,3) should be perpendicular
    float Dot05 = FVector::DotProduct(TetrahedralState.Edges[0].Orientation, TetrahedralState.Edges[5].Orientation);
    float Dot14 = FVector::DotProduct(TetrahedralState.Edges[1].Orientation, TetrahedralState.Edges[4].Orientation);
    float Dot23 = FVector::DotProduct(TetrahedralState.Edges[2].Orientation, TetrahedralState.Edges[3].Orientation);
    
    // Should be close to 0 for perpendicular
    float Tolerance = 0.1f;
    return (FMath::Abs(Dot05) < Tolerance) && (FMath::Abs(Dot14) < Tolerance) && (FMath::Abs(Dot23) < Tolerance);
}

void USystem5CognitiveIntegration::PropagateActivation()
{
    // Propagate activation through edges
    for (const FDyadicEdge& Edge : TetrahedralState.Edges)
    {
        float Activation1 = TetrahedralState.Vertices[Edge.Vertex1].Activation;
        float Activation2 = TetrahedralState.Vertices[Edge.Vertex2].Activation;
        
        // Bidirectional propagation with edge weight
        float Propagation = (Activation1 + Activation2) * 0.5f * Edge.Weight * 0.1f;
        
        TetrahedralState.Vertices[Edge.Vertex1].Activation = 
            FMath::Clamp(TetrahedralState.Vertices[Edge.Vertex1].Activation + Propagation * (Activation2 - Activation1), 0.0f, 1.0f);
        TetrahedralState.Vertices[Edge.Vertex2].Activation = 
            FMath::Clamp(TetrahedralState.Vertices[Edge.Vertex2].Activation + Propagation * (Activation1 - Activation2), 0.0f, 1.0f);
    }
}

void USystem5CognitiveIntegration::UpdateTensorBundles()
{
    // Update tensor state for each face based on vertex activations
    for (FTriadicFace& Face : TetrahedralState.Faces)
    {
        Face.TensorState.SetNum(3);
        
        for (int32 i = 0; i < 3; ++i)
        {
            int32 VertexId = Face.Vertices[i];
            Face.TensorState[i] = TetrahedralState.Vertices[VertexId].Activation;
        }
        
        // Compute face coherence
        float Sum = 0.0f;
        for (float Val : Face.TensorState)
        {
            Sum += Val;
        }
        float Mean = Sum / 3.0f;
        
        float Variance = 0.0f;
        for (float Val : Face.TensorState)
        {
            float Diff = Val - Mean;
            Variance += Diff * Diff;
        }
        Variance /= 3.0f;
        
        Face.Coherence = 1.0f - FMath::Clamp(Variance, 0.0f, 1.0f);
    }
}

int32 USystem5CognitiveIntegration::ComputeOEISTermCount(int32 Level) const
{
    // OEIS A000081: Number of rooted trees with n nodes
    // a(1) = 1, a(2) = 1, a(3) = 2, a(4) = 4, a(5) = 9, ...
    // For our purposes, we use: Level 1 -> 1, Level 2 -> 2, Level 3 -> 4, Level 4 -> 9
    
    switch (Level)
    {
    case 1: return 1;
    case 2: return 2;
    case 3: return 4;
    case 4: return 9;
    default: return 1;
    }
}

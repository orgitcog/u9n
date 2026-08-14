#pragma once

/**
 * System 5 Cognitive Integration
 * 
 * Implements the tetradic/tetrahedral cognitive architecture with:
 * - 4 tensor bundles (monadic vertices)
 * - 3 dyadic edges per triadic face
 * - 6 shared dyadic edges across tetrahedron
 * - OEIS A000081 nested shell structure
 * 
 * Architecture follows the System 5 Deterministic State Transition Model:
 * - 4 threads corresponding to 4 monadic vertices
 * - Each triad contains 3 of 4 threads for full complementarity
 * - Mutually orthogonal symmetries align triadic faces and dyadic edges
 * 
 * @see DeepTreeEchoCore for cognitive loop integration
 * @see RecursiveMutualAwarenessSystem for consciousness streams
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "System5CognitiveIntegration.generated.h"

/**
 * Monadic Vertex - Fundamental cognitive unit
 * Represents one of 4 threads in the tetrahedral structure
 */
USTRUCT(BlueprintType)
struct FMonadicVertex
{
    GENERATED_BODY()

    /** Vertex identifier (0-3) */
    UPROPERTY(BlueprintReadWrite)
    int32 VertexId = 0;

    /** Current activation state */
    UPROPERTY(BlueprintReadWrite)
    float Activation = 0.0f;

    /** Thread state vector */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> StateVector;

    /** Connected dyadic edges (indices) */
    UPROPERTY(BlueprintReadWrite)
    TArray<int32> ConnectedEdges;

    /** Participating triadic faces (indices) */
    UPROPERTY(BlueprintReadWrite)
    TArray<int32> ParticipatingFaces;
};

/**
 * Dyadic Edge - Relational connection between vertices
 * 6 edges shared across the tetrahedron
 */
USTRUCT(BlueprintType)
struct FDyadicEdge
{
    GENERATED_BODY()

    /** Edge identifier (0-5) */
    UPROPERTY(BlueprintReadWrite)
    int32 EdgeId = 0;

    /** First vertex index */
    UPROPERTY(BlueprintReadWrite)
    int32 Vertex1 = 0;

    /** Second vertex index */
    UPROPERTY(BlueprintReadWrite)
    int32 Vertex2 = 0;

    /** Edge weight/strength */
    UPROPERTY(BlueprintReadWrite)
    float Weight = 1.0f;

    /** Orientation alignment (for symmetry) */
    UPROPERTY(BlueprintReadWrite)
    FVector Orientation = FVector::ForwardVector;

    /** Participating triadic faces */
    UPROPERTY(BlueprintReadWrite)
    TArray<int32> ParticipatingFaces;
};

/**
 * Triadic Face - Tensor bundle containing 3 dyadic edges
 * 4 faces forming the tetrahedral structure
 */
USTRUCT(BlueprintType)
struct FTriadicFace
{
    GENERATED_BODY()

    /** Face identifier (0-3) */
    UPROPERTY(BlueprintReadWrite)
    int32 FaceId = 0;

    /** Three vertices forming this face */
    UPROPERTY(BlueprintReadWrite)
    TArray<int32> Vertices; // Size 3

    /** Three edges forming this face */
    UPROPERTY(BlueprintReadWrite)
    TArray<int32> Edges; // Size 3

    /** Face normal (for orientation alignment) */
    UPROPERTY(BlueprintReadWrite)
    FVector Normal = FVector::UpVector;

    /** Tensor bundle state */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> TensorState;

    /** Face coherence (how well-integrated) */
    UPROPERTY(BlueprintReadWrite)
    float Coherence = 1.0f;

    /** Adjacent faces (shares 1 edge each) */
    UPROPERTY(BlueprintReadWrite)
    TArray<int32> AdjacentFaces;
};

/**
 * Tetrahedral State - Complete system state
 */
USTRUCT(BlueprintType)
struct FTetrahedralState
{
    GENERATED_BODY()

    /** 4 monadic vertices */
    UPROPERTY(BlueprintReadWrite)
    TArray<FMonadicVertex> Vertices; // Size 4

    /** 6 dyadic edges */
    UPROPERTY(BlueprintReadWrite)
    TArray<FDyadicEdge> Edges; // Size 6

    /** 4 triadic faces */
    UPROPERTY(BlueprintReadWrite)
    TArray<FTriadicFace> Faces; // Size 4

    /** Global coherence (tetrahedral integrity) */
    UPROPERTY(BlueprintReadWrite)
    float GlobalCoherence = 1.0f;

    /** Current transformation step */
    UPROPERTY(BlueprintReadWrite)
    int32 TransformationStep = 0;

    /** Symmetry alignment score */
    UPROPERTY(BlueprintReadWrite)
    float SymmetryAlignment = 1.0f;
};

/**
 * OEIS A000081 Nesting Level
 * Defines the nested shell structure
 */
USTRUCT(BlueprintType)
struct FNestedShellLevel
{
    GENERATED_BODY()

    /** Nesting level (1-4) */
    UPROPERTY(BlueprintReadWrite)
    int32 Level = 1;

    /** Number of terms at this level (1, 2, 4, 9) */
    UPROPERTY(BlueprintReadWrite)
    int32 TermCount = 1;

    /** Terms at this level */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> Terms;

    /** Steps apart from previous level */
    UPROPERTY(BlueprintReadWrite)
    int32 StepsApart = 1;
};

/**
 * System 5 Cognitive Integration Component
 * 
 * Implements the complete tetradic cognitive architecture with
 * 4 tensor bundles, 6 dyadic edges, and OEIS A000081 nesting.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class USystem5CognitiveIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    USystem5CognitiveIntegration();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable tetradic processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "System5|Config")
    bool bEnableTetradicProcessing = true;

    /** Enable OEIS A000081 nesting */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "System5|Config")
    bool bEnableNestedShells = true;

    /** Transformation step duration (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "System5|Config", meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float StepDuration = 1.0f;

    // ========================================
    // STATE
    // ========================================

    /** Current tetrahedral state */
    UPROPERTY(BlueprintReadOnly, Category = "System5|State")
    FTetrahedralState TetrahedralState;

    /** Nested shell levels */
    UPROPERTY(BlueprintReadOnly, Category = "System5|State")
    TArray<FNestedShellLevel> NestedShells;

    /** Current nesting level (1-4) */
    UPROPERTY(BlueprintReadOnly, Category = "System5|State")
    int32 CurrentNestingLevel = 1;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize the tetrahedral structure */
    UFUNCTION(BlueprintCallable, Category = "System5")
    void InitializeTetrahedralStructure();

    /** Initialize nested shell structure */
    UFUNCTION(BlueprintCallable, Category = "System5")
    void InitializeNestedShells();

    // ========================================
    // PUBLIC API - TETRAHEDRAL OPERATIONS
    // ========================================

    /** Activate a monadic vertex */
    UFUNCTION(BlueprintCallable, Category = "System5|Tetrahedron")
    void ActivateVertex(int32 VertexId, float Activation);

    /** Update dyadic edge weight */
    UFUNCTION(BlueprintCallable, Category = "System5|Tetrahedron")
    void UpdateEdgeWeight(int32 EdgeId, float Weight);

    /** Process triadic face tensor */
    UFUNCTION(BlueprintCallable, Category = "System5|Tetrahedron")
    void ProcessTriadicTensor(int32 FaceId, const TArray<float>& InputTensor);

    /** Execute state transformation step */
    UFUNCTION(BlueprintCallable, Category = "System5|Tetrahedron")
    void ExecuteTransformationStep();

    /** Align triadic face orientations */
    UFUNCTION(BlueprintCallable, Category = "System5|Tetrahedron")
    void AlignFaceOrientations();

    /** Compute global coherence */
    UFUNCTION(BlueprintCallable, Category = "System5|Tetrahedron")
    float ComputeGlobalCoherence() const;

    // ========================================
    // PUBLIC API - NESTED SHELLS
    // ========================================

    /** Get current nesting level */
    UFUNCTION(BlueprintCallable, Category = "System5|Nesting")
    int32 GetCurrentNestingLevel() const { return CurrentNestingLevel; }

    /** Get term count at nesting level */
    UFUNCTION(BlueprintCallable, Category = "System5|Nesting")
    int32 GetTermCountAtLevel(int32 Level) const;

    /** Advance to next nesting level */
    UFUNCTION(BlueprintCallable, Category = "System5|Nesting")
    void AdvanceNestingLevel();

    /** Get terms at current level */
    UFUNCTION(BlueprintCallable, Category = "System5|Nesting")
    TArray<FString> GetTermsAtCurrentLevel() const;

    // ========================================
    // PUBLIC API - COGNITIVE LOOP INTEGRATION
    // ========================================

    /** Map 12-step cycle to tetrahedral state */
    UFUNCTION(BlueprintCallable, Category = "System5|CognitiveLoop")
    void MapCycleStepToTetrahedralState(int32 CycleStep);

    /** Get active triadic face for cycle step */
    UFUNCTION(BlueprintCallable, Category = "System5|CognitiveLoop")
    int32 GetActiveFaceForCycleStep(int32 CycleStep) const;

    /** Get triadic synchronization points */
    UFUNCTION(BlueprintCallable, Category = "System5|CognitiveLoop")
    TArray<int32> GetTriadicSynchronizationPoints() const;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Step timer */
    float StepTimer = 0.0f;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Build tetrahedral connectivity */
    void BuildTetrahedralConnectivity();

    /** Compute face normal from vertices */
    FVector ComputeFaceNormal(const FTriadicFace& Face) const;

    /** Compute edge orientation */
    FVector ComputeEdgeOrientation(const FDyadicEdge& Edge) const;

    /** Check orthogonal symmetry */
    bool CheckOrthogonalSymmetry() const;

    /** Propagate activation through edges */
    void PropagateActivation();

    /** Update tensor bundles */
    void UpdateTensorBundles();

    /** Compute OEIS A000081 term count */
    int32 ComputeOEISTermCount(int32 Level) const;
};

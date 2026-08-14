#pragma once

/**
 * Tetradic Reservoir Integration
 * 
 * Implements the System 5 tetradic/tetrahedral cognitive architecture
 * with reservoir computing substrate. Maps 4 tensor bundles to reservoir
 * dynamics with triadic face alignment and dyadic edge coupling.
 * 
 * Architecture:
 * - 4 monadic vertices (threads) corresponding to 4 reservoir pools
 * - 6 dyadic edges shared between triadic faces
 * - 4 triadic faces (fiber bundles) with 3 threads each
 * - Tetrahedral symmetry with mutually orthogonal orientations
 * 
 * Vertex-Thread Mapping:
 * - V1: Perception thread (sensory input)
 * - V2: Action thread (motor output)
 * - V3: Cognition thread (internal processing)
 * - V4: Memory thread (temporal integration)
 * 
 * Triadic Faces (each contains 3 of 4 threads):
 * - F1 (V1,V2,V3): Perception-Action-Cognition
 * - F2 (V1,V2,V4): Perception-Action-Memory
 * - F3 (V1,V3,V4): Perception-Cognition-Memory
 * - F4 (V2,V3,V4): Action-Cognition-Memory
 * 
 * @see DeepCognitiveBridge for echobeats integration
 * @see ReservoirCognitiveIntegration for base reservoir coupling
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepTreeEchoReservoir.h"
#include "DeepCognitiveBridge.h"
#include "TetradicReservoirIntegration.generated.h"

/**
 * Tetradic Vertex (Monadic Thread)
 */
UENUM(BlueprintType)
enum class ETetradicVertex : uint8
{
    /** V1: Perception thread */
    Perception UMETA(DisplayName = "Perception"),
    
    /** V2: Action thread */
    Action UMETA(DisplayName = "Action"),
    
    /** V3: Cognition thread */
    Cognition UMETA(DisplayName = "Cognition"),
    
    /** V4: Memory thread */
    Memory UMETA(DisplayName = "Memory")
};

/**
 * Triadic Face (Fiber Bundle)
 */
UENUM(BlueprintType)
enum class ETriadicFace : uint8
{
    /** F1: Perception-Action-Cognition */
    PAC UMETA(DisplayName = "Perception-Action-Cognition"),
    
    /** F2: Perception-Action-Memory */
    PAM UMETA(DisplayName = "Perception-Action-Memory"),
    
    /** F3: Perception-Cognition-Memory */
    PCM UMETA(DisplayName = "Perception-Cognition-Memory"),
    
    /** F4: Action-Cognition-Memory */
    ACM UMETA(DisplayName = "Action-Cognition-Memory")
};

/**
 * Dyadic Edge
 */
UENUM(BlueprintType)
enum class EDyadicEdge : uint8
{
    /** E1: Perception-Action */
    PA UMETA(DisplayName = "Perception-Action"),
    
    /** E2: Perception-Cognition */
    PC UMETA(DisplayName = "Perception-Cognition"),
    
    /** E3: Perception-Memory */
    PM UMETA(DisplayName = "Perception-Memory"),
    
    /** E4: Action-Cognition */
    AC UMETA(DisplayName = "Action-Cognition"),
    
    /** E5: Action-Memory */
    AM UMETA(DisplayName = "Action-Memory"),
    
    /** E6: Cognition-Memory */
    CM UMETA(DisplayName = "Cognition-Memory")
};

/**
 * Vertex Reservoir State
 */
USTRUCT(BlueprintType)
struct FVertexReservoirState
{
    GENERATED_BODY()

    /** Vertex type */
    UPROPERTY(BlueprintReadWrite)
    ETetradicVertex Vertex = ETetradicVertex::Perception;

    /** Reservoir state */
    UPROPERTY(BlueprintReadWrite)
    FReservoirState ReservoirState;

    /** Thread activation level */
    UPROPERTY(BlueprintReadWrite)
    float ActivationLevel = 0.5f;

    /** Connections to adjacent vertices */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> AdjacentCouplings;
};

/**
 * Dyadic Edge State
 */
USTRUCT(BlueprintType)
struct FDyadicEdgeState
{
    GENERATED_BODY()

    /** Edge type */
    UPROPERTY(BlueprintReadWrite)
    EDyadicEdge Edge = EDyadicEdge::PA;

    /** Source vertex */
    UPROPERTY(BlueprintReadWrite)
    ETetradicVertex SourceVertex = ETetradicVertex::Perception;

    /** Target vertex */
    UPROPERTY(BlueprintReadWrite)
    ETetradicVertex TargetVertex = ETetradicVertex::Action;

    /** Coupling strength */
    UPROPERTY(BlueprintReadWrite)
    float CouplingStrength = 0.5f;

    /** Bidirectional flow state */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> FlowState;

    /** Edge is active */
    UPROPERTY(BlueprintReadWrite)
    bool bIsActive = true;
};

/**
 * Triadic Face State (Fiber Bundle)
 */
USTRUCT(BlueprintType)
struct FTriadicFaceState
{
    GENERATED_BODY()

    /** Face type */
    UPROPERTY(BlueprintReadWrite)
    ETriadicFace Face = ETriadicFace::PAC;

    /** Vertices in this face */
    UPROPERTY(BlueprintReadWrite)
    TArray<ETetradicVertex> Vertices;

    /** Edges in this face */
    UPROPERTY(BlueprintReadWrite)
    TArray<EDyadicEdge> Edges;

    /** Face orientation (normal vector) */
    UPROPERTY(BlueprintReadWrite)
    FVector Orientation = FVector::UpVector;

    /** Face coherence */
    UPROPERTY(BlueprintReadWrite)
    float Coherence = 0.5f;

    /** Combined reservoir state for face */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> CombinedState;
};

/**
 * Tetrahedral State
 */
USTRUCT(BlueprintType)
struct FTetrahedralState
{
    GENERATED_BODY()

    /** All 4 vertex states */
    UPROPERTY(BlueprintReadWrite)
    TArray<FVertexReservoirState> Vertices;

    /** All 6 edge states */
    UPROPERTY(BlueprintReadWrite)
    TArray<FDyadicEdgeState> Edges;

    /** All 4 face states */
    UPROPERTY(BlueprintReadWrite)
    TArray<FTriadicFaceState> Faces;

    /** Global coherence */
    UPROPERTY(BlueprintReadWrite)
    float GlobalCoherence = 0.5f;

    /** Current transformation step (1-6) */
    UPROPERTY(BlueprintReadWrite)
    int32 TransformationStep = 1;
};

/**
 * Tetradic Reservoir Integration Component
 * 
 * Implements System 5 tetradic cognitive architecture with reservoir computing.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UTetradicReservoirIntegration : public UActorComponent
{
    GENERATED_BODY()

public:
    UTetradicReservoirIntegration();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable tetradic integration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tetradic|Config")
    bool bEnableTetradicIntegration = true;

    /** Reservoir units per vertex */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tetradic|Config", meta = (ClampMin = "10", ClampMax = "500"))
    int32 UnitsPerVertex = 100;

    /** Inter-vertex coupling strength */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tetradic|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float InterVertexCoupling = 0.3f;

    /** Transformation cycle duration (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tetradic|Config", meta = (ClampMin = "1.0", ClampMax = "30.0"))
    float TransformationCycleDuration = 6.0f;

    // ========================================
    // COMPONENT REFERENCES
    // ========================================

    /** Reference to cognitive bridge */
    UPROPERTY(BlueprintReadOnly, Category = "Tetradic|Components")
    UDeepCognitiveBridge* CognitiveBridge;

    /** Reference to base reservoir */
    UPROPERTY(BlueprintReadOnly, Category = "Tetradic|Components")
    UDeepTreeEchoReservoir* BaseReservoir;

    // ========================================
    // STATE
    // ========================================

    /** Current tetrahedral state */
    UPROPERTY(BlueprintReadOnly, Category = "Tetradic|State")
    FTetrahedralState TetrahedralState;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize tetradic structure */
    UFUNCTION(BlueprintCallable, Category = "Tetradic")
    void InitializeTetradicStructure();

    /** Initialize vertex reservoirs */
    UFUNCTION(BlueprintCallable, Category = "Tetradic")
    void InitializeVertexReservoirs();

    /** Initialize dyadic edges */
    UFUNCTION(BlueprintCallable, Category = "Tetradic")
    void InitializeDyadicEdges();

    /** Initialize triadic faces */
    UFUNCTION(BlueprintCallable, Category = "Tetradic")
    void InitializeTriadicFaces();

    // ========================================
    // PUBLIC API - VERTEX OPERATIONS
    // ========================================

    /** Update vertex with input */
    UFUNCTION(BlueprintCallable, Category = "Tetradic|Vertex")
    void UpdateVertex(ETetradicVertex Vertex, const TArray<float>& Input);

    /** Get vertex state */
    UFUNCTION(BlueprintCallable, Category = "Tetradic|Vertex")
    FVertexReservoirState GetVertexState(ETetradicVertex Vertex) const;

    /** Get vertices for face */
    UFUNCTION(BlueprintCallable, Category = "Tetradic|Vertex")
    TArray<ETetradicVertex> GetVerticesForFace(ETriadicFace Face) const;

    // ========================================
    // PUBLIC API - EDGE OPERATIONS
    // ========================================

    /** Update edge coupling */
    UFUNCTION(BlueprintCallable, Category = "Tetradic|Edge")
    void UpdateEdge(EDyadicEdge Edge);

    /** Get edge state */
    UFUNCTION(BlueprintCallable, Category = "Tetradic|Edge")
    FDyadicEdgeState GetEdgeState(EDyadicEdge Edge) const;

    /** Get edges for face */
    UFUNCTION(BlueprintCallable, Category = "Tetradic|Edge")
    TArray<EDyadicEdge> GetEdgesForFace(ETriadicFace Face) const;

    /** Compute edge flow */
    UFUNCTION(BlueprintCallable, Category = "Tetradic|Edge")
    TArray<float> ComputeEdgeFlow(EDyadicEdge Edge) const;

    // ========================================
    // PUBLIC API - FACE OPERATIONS
    // ========================================

    /** Update face state */
    UFUNCTION(BlueprintCallable, Category = "Tetradic|Face")
    void UpdateFace(ETriadicFace Face);

    /** Get face state */
    UFUNCTION(BlueprintCallable, Category = "Tetradic|Face")
    FTriadicFaceState GetFaceState(ETriadicFace Face) const;

    /** Compute face coherence */
    UFUNCTION(BlueprintCallable, Category = "Tetradic|Face")
    float ComputeFaceCoherence(ETriadicFace Face) const;

    /** Align face orientations */
    UFUNCTION(BlueprintCallable, Category = "Tetradic|Face")
    void AlignFaceOrientations();

    // ========================================
    // PUBLIC API - TRANSFORMATION
    // ========================================

    /** Advance transformation step */
    UFUNCTION(BlueprintCallable, Category = "Tetradic|Transform")
    void AdvanceTransformationStep();

    /** Get current transformation step */
    UFUNCTION(BlueprintCallable, Category = "Tetradic|Transform")
    int32 GetTransformationStep() const { return TetrahedralState.TransformationStep; }

    /** Apply transformation to state */
    UFUNCTION(BlueprintCallable, Category = "Tetradic|Transform")
    void ApplyTransformation();

    // ========================================
    // PUBLIC API - COHERENCE
    // ========================================

    /** Compute global coherence */
    UFUNCTION(BlueprintCallable, Category = "Tetradic|Coherence")
    float ComputeGlobalCoherence() const;

    /** Synchronize tetrahedral state */
    UFUNCTION(BlueprintCallable, Category = "Tetradic|Coherence")
    void SynchronizeTetrahedralState();

    // ========================================
    // PUBLIC API - ECHOBEATS MAPPING
    // ========================================

    /** Map echobeats streams to tetradic structure */
    UFUNCTION(BlueprintCallable, Category = "Tetradic|Echobeats")
    void MapEchobeatsToTetradic();

    /** Get face for echobeats step */
    UFUNCTION(BlueprintCallable, Category = "Tetradic|Echobeats")
    ETriadicFace GetFaceForEchobeatStep(int32 Step) const;

    // ========================================
    // DELEGATES
    // ========================================

    /** Called when transformation step advances */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTransformationAdvanced, int32, NewStep);
    UPROPERTY(BlueprintAssignable, Category = "Tetradic|Events")
    FOnTransformationAdvanced OnTransformationAdvanced;

    /** Called when face coherence changes significantly */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFaceCoherenceChanged, ETriadicFace, Face, float, NewCoherence);
    UPROPERTY(BlueprintAssignable, Category = "Tetradic|Events")
    FOnFaceCoherenceChanged OnFaceCoherenceChanged;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Transformation timer */
    float TransformationTimer = 0.0f;

    /** Step duration */
    float StepDuration = 1.0f;

    /** Previous face coherences for change detection */
    TMap<ETriadicFace, float> PreviousFaceCoherences;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Initialize component references */
    void InitializeComponentReferences();

    /** Get vertex index */
    int32 GetVertexIndex(ETetradicVertex Vertex) const;

    /** Get edge index */
    int32 GetEdgeIndex(EDyadicEdge Edge) const;

    /** Get face index */
    int32 GetFaceIndex(ETriadicFace Face) const;

    /** Compute vertex coupling */
    void ComputeVertexCoupling(ETetradicVertex Source, ETetradicVertex Target);

    /** Update all edges */
    void UpdateAllEdges();

    /** Update all faces */
    void UpdateAllFaces();

    /** Check for coherence changes */
    void CheckCoherenceChanges();
};

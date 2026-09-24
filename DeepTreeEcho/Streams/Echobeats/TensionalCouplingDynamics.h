// TensionalCouplingDynamics.h
// Advanced tensional coupling dynamics for stream processing
// Implements tetrahedral tensor bundle architecture with dyadic edge sharing

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EchobeatsStreamEngine.h"
#include "TensionalCouplingDynamics.generated.h"

/**
 * Dyadic edge type for tetrahedral structure
 * 6 edges connecting 4 vertices (threads)
 */
UENUM(BlueprintType)
enum class EDyadicEdge : uint8
{
    Edge_12  UMETA(DisplayName = "Edge 1-2 (Perception-Memory)"),
    Edge_13  UMETA(DisplayName = "Edge 1-3 (Perception-Assessment)"),
    Edge_14  UMETA(DisplayName = "Edge 1-4 (Perception-Planning)"),
    Edge_23  UMETA(DisplayName = "Edge 2-3 (Memory-Assessment)"),
    Edge_24  UMETA(DisplayName = "Edge 2-4 (Memory-Planning)"),
    Edge_34  UMETA(DisplayName = "Edge 3-4 (Assessment-Planning)")
};

/**
 * Triadic face type for tetrahedral structure
 * 4 faces, each containing 3 vertices (threads)
 */
UENUM(BlueprintType)
enum class ETriadicFace : uint8
{
    Face_123  UMETA(DisplayName = "Face 1-2-3 (Perception-Memory-Assessment)"),
    Face_124  UMETA(DisplayName = "Face 1-2-4 (Perception-Memory-Planning)"),
    Face_134  UMETA(DisplayName = "Face 1-3-4 (Perception-Assessment-Planning)"),
    Face_234  UMETA(DisplayName = "Face 2-3-4 (Memory-Assessment-Planning)")
};

/**
 * Thread vertex type (4 monadic vertices)
 */
UENUM(BlueprintType)
enum class EThreadVertex : uint8
{
    V1_Perception  UMETA(DisplayName = "V1: Perception Thread"),
    V2_Memory      UMETA(DisplayName = "V2: Memory Thread"),
    V3_Assessment  UMETA(DisplayName = "V3: Assessment Thread"),
    V4_Planning    UMETA(DisplayName = "V4: Planning Thread")
};

/**
 * Coupling resonance mode
 */
UENUM(BlueprintType)
enum class ECouplingResonanceMode : uint8
{
    Harmonic      UMETA(DisplayName = "Harmonic - Constructive interference"),
    Dissonant     UMETA(DisplayName = "Dissonant - Destructive interference"),
    Entangled     UMETA(DisplayName = "Entangled - Quantum-like superposition"),
    Decoupled     UMETA(DisplayName = "Decoupled - Independent operation")
};

/**
 * Dyadic edge state
 */
USTRUCT(BlueprintType)
struct FDyadicEdgeState
{
    GENERATED_BODY()

    /** Edge type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EDyadicEdge EdgeType = EDyadicEdge::Edge_12;

    /** First vertex */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EThreadVertex VertexA = EThreadVertex::V1_Perception;

    /** Second vertex */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EThreadVertex VertexB = EThreadVertex::V2_Memory;

    /** Edge tension (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Tension = 0.5f;

    /** Edge flow direction (-1 to 1, negative = A to B, positive = B to A) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FlowDirection = 0.0f;

    /** Edge activation level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Activation = 0.0f;

    /** Edge state vector */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> StateVector;

    /** Resonance mode */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ECouplingResonanceMode ResonanceMode = ECouplingResonanceMode::Harmonic;
};

/**
 * Triadic face state
 */
USTRUCT(BlueprintType)
struct FTriadicFaceState
{
    GENERATED_BODY()

    /** Face type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ETriadicFace FaceType = ETriadicFace::Face_123;

    /** Vertices in this face */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<EThreadVertex> Vertices;

    /** Edges in this face */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<EDyadicEdge> Edges;

    /** Face orientation (0-2*PI) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Orientation = 0.0f;

    /** Face coherence */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Coherence = 1.0f;

    /** Face activation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Activation = 0.0f;

    /** Face state vector */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> StateVector;

    /** Adjacent faces */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<ETriadicFace> AdjacentFaces;
};

/**
 * Thread vertex state
 */
USTRUCT(BlueprintType)
struct FThreadVertexState
{
    GENERATED_BODY()

    /** Vertex type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EThreadVertex VertexType = EThreadVertex::V1_Perception;

    /** Vertex activation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Activation = 0.0f;

    /** Vertex state vector */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> StateVector;

    /** Connected edges */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<EDyadicEdge> ConnectedEdges;

    /** Containing faces */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<ETriadicFace> ContainingFaces;

    /** Thread processing load */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ProcessingLoad = 0.0f;
};

/**
 * Coupling entanglement state (order 2 qubit)
 */
USTRUCT(BlueprintType)
struct FCouplingEntanglement
{
    GENERATED_BODY()

    /** First edge in entanglement */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EDyadicEdge EdgeA = EDyadicEdge::Edge_12;

    /** Second edge in entanglement */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EDyadicEdge EdgeB = EDyadicEdge::Edge_34;

    /** Entanglement strength */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EntanglementStrength = 0.0f;

    /** Phase correlation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PhaseCorrelation = 0.0f;

    /** Is currently entangled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsEntangled = false;

    /** Shared memory address (conceptual) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SharedMemorySlot = -1;
};

/**
 * Thread multiplexing state
 */
USTRUCT(BlueprintType)
struct FThreadMultiplexState
{
    GENERATED_BODY()

    /** Current permutation index */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PermutationIndex = 0;

    /** Current dyad pair */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<EThreadVertex> CurrentDyadPair;

    /** Current triad set for MP1 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<EThreadVertex> MP1TriadSet;

    /** Current triad set for MP2 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<EThreadVertex> MP2TriadSet;

    /** Multiplexing phase (0-5 for dyad, 0-3 for triad) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MultiplexPhase = 0;
};

/**
 * Full tetrahedral coupling state
 */
USTRUCT(BlueprintType)
struct FTetrahedralCouplingState
{
    GENERATED_BODY()

    /** Vertex states (4) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FThreadVertexState> VertexStates;

    /** Edge states (6) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FDyadicEdgeState> EdgeStates;

    /** Face states (4) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FTriadicFaceState> FaceStates;

    /** Active entanglements */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FCouplingEntanglement> ActiveEntanglements;

    /** Multiplexing state */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FThreadMultiplexState MultiplexState;

    /** Overall tetrahedral coherence */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TetrahedralCoherence = 1.0f;

    /** Global tension balance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TensionBalance = 0.5f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEdgeTensionChanged, EDyadicEdge, Edge, float, NewTension);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnFaceActivated, ETriadicFace, Face, float, Activation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEntanglementFormed, EDyadicEdge, EdgeA, EDyadicEdge, EdgeB, float, Strength);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMultiplexAdvanced, int32, NewPermutationIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnResonanceModeChanged, EDyadicEdge, Edge, ECouplingResonanceMode, NewMode);

/**
 * Tensional Coupling Dynamics Component
 * 
 * Implements the tetrahedral tensor bundle architecture with:
 * - 4 monadic vertices (threads)
 * - 6 dyadic edges (couplings)
 * - 4 triadic faces (complementarity sets)
 * - Thread-level multiplexing with permutation cycling
 * - Order-2 qubit entanglement (2 processes, 1 memory address)
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UTensionalCouplingDynamics : public UActorComponent
{
    GENERATED_BODY()

public:
    UTensionalCouplingDynamics();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** State vector size per element */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coupling|Config")
    int32 StateVectorSize = 32;

    /** Tension propagation rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coupling|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TensionPropagationRate = 0.1f;

    /** Entanglement threshold */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coupling|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float EntanglementThreshold = 0.7f;

    /** Enable automatic multiplexing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coupling|Config")
    bool bEnableAutoMultiplex = true;

    /** Multiplex cycle rate (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coupling|Config")
    float MultiplexCycleRate = 2.0f;

    /** Enable entanglement detection */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coupling|Config")
    bool bEnableEntanglementDetection = true;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Coupling|Events")
    FOnEdgeTensionChanged OnEdgeTensionChanged;

    UPROPERTY(BlueprintAssignable, Category = "Coupling|Events")
    FOnFaceActivated OnFaceActivated;

    UPROPERTY(BlueprintAssignable, Category = "Coupling|Events")
    FOnEntanglementFormed OnEntanglementFormed;

    UPROPERTY(BlueprintAssignable, Category = "Coupling|Events")
    FOnMultiplexAdvanced OnMultiplexAdvanced;

    UPROPERTY(BlueprintAssignable, Category = "Coupling|Events")
    FOnResonanceModeChanged OnResonanceModeChanged;

    // ========================================
    // COMPONENT REFERENCES
    // ========================================

    UPROPERTY(BlueprintReadOnly, Category = "Coupling|Components")
    UEchobeatsStreamEngine* EchobeatsEngine;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize the coupling dynamics system */
    UFUNCTION(BlueprintCallable, Category = "Coupling")
    void InitializeCouplingSystem();

    /** Build tetrahedral structure */
    UFUNCTION(BlueprintCallable, Category = "Coupling")
    void BuildTetrahedralStructure();

    /** Initialize multiplexing permutations */
    UFUNCTION(BlueprintCallable, Category = "Coupling")
    void InitializeMultiplexPermutations();

    // ========================================
    // PUBLIC API - EDGE OPERATIONS
    // ========================================

    /** Set edge tension */
    UFUNCTION(BlueprintCallable, Category = "Coupling|Edges")
    void SetEdgeTension(EDyadicEdge Edge, float Tension);

    /** Set edge flow direction */
    UFUNCTION(BlueprintCallable, Category = "Coupling|Edges")
    void SetEdgeFlow(EDyadicEdge Edge, float FlowDirection);

    /** Get edge state */
    UFUNCTION(BlueprintPure, Category = "Coupling|Edges")
    FDyadicEdgeState GetEdgeState(EDyadicEdge Edge) const;

    /** Propagate tension along edge */
    UFUNCTION(BlueprintCallable, Category = "Coupling|Edges")
    void PropagateTension(EDyadicEdge Edge);

    /** Set edge resonance mode */
    UFUNCTION(BlueprintCallable, Category = "Coupling|Edges")
    void SetEdgeResonanceMode(EDyadicEdge Edge, ECouplingResonanceMode Mode);

    // ========================================
    // PUBLIC API - FACE OPERATIONS
    // ========================================

    /** Activate triadic face */
    UFUNCTION(BlueprintCallable, Category = "Coupling|Faces")
    void ActivateFace(ETriadicFace Face, float Activation);

    /** Get face state */
    UFUNCTION(BlueprintPure, Category = "Coupling|Faces")
    FTriadicFaceState GetFaceState(ETriadicFace Face) const;

    /** Compute face coherence from edges */
    UFUNCTION(BlueprintCallable, Category = "Coupling|Faces")
    float ComputeFaceCoherence(ETriadicFace Face);

    /** Synchronize adjacent faces */
    UFUNCTION(BlueprintCallable, Category = "Coupling|Faces")
    void SynchronizeAdjacentFaces(ETriadicFace Face);

    // ========================================
    // PUBLIC API - VERTEX OPERATIONS
    // ========================================

    /** Set vertex activation */
    UFUNCTION(BlueprintCallable, Category = "Coupling|Vertices")
    void SetVertexActivation(EThreadVertex Vertex, float Activation);

    /** Get vertex state */
    UFUNCTION(BlueprintPure, Category = "Coupling|Vertices")
    FThreadVertexState GetVertexState(EThreadVertex Vertex) const;

    /** Process vertex (thread) */
    UFUNCTION(BlueprintCallable, Category = "Coupling|Vertices")
    void ProcessVertex(EThreadVertex Vertex, const TArray<float>& Input);

    // ========================================
    // PUBLIC API - ENTANGLEMENT
    // ========================================

    /** Create entanglement between edges */
    UFUNCTION(BlueprintCallable, Category = "Coupling|Entanglement")
    void CreateEntanglement(EDyadicEdge EdgeA, EDyadicEdge EdgeB);

    /** Break entanglement */
    UFUNCTION(BlueprintCallable, Category = "Coupling|Entanglement")
    void BreakEntanglement(EDyadicEdge EdgeA, EDyadicEdge EdgeB);

    /** Check if edges are entangled */
    UFUNCTION(BlueprintPure, Category = "Coupling|Entanglement")
    bool AreEdgesEntangled(EDyadicEdge EdgeA, EDyadicEdge EdgeB) const;

    /** Get entanglement state */
    UFUNCTION(BlueprintPure, Category = "Coupling|Entanglement")
    FCouplingEntanglement GetEntanglementState(EDyadicEdge EdgeA, EDyadicEdge EdgeB) const;

    /** Detect potential entanglements */
    UFUNCTION(BlueprintCallable, Category = "Coupling|Entanglement")
    void DetectPotentialEntanglements();

    // ========================================
    // PUBLIC API - MULTIPLEXING
    // ========================================

    /** Advance multiplex permutation */
    UFUNCTION(BlueprintCallable, Category = "Coupling|Multiplex")
    void AdvanceMultiplex();

    /** Get current multiplex state */
    UFUNCTION(BlueprintPure, Category = "Coupling|Multiplex")
    FThreadMultiplexState GetMultiplexState() const;

    /** Set multiplex permutation index */
    UFUNCTION(BlueprintCallable, Category = "Coupling|Multiplex")
    void SetMultiplexPermutation(int32 PermutationIndex);

    /** Get active dyad pair for current permutation */
    UFUNCTION(BlueprintPure, Category = "Coupling|Multiplex")
    TArray<EThreadVertex> GetActiveDyadPair() const;

    /** Get active triad sets for current permutation */
    UFUNCTION(BlueprintPure, Category = "Coupling|Multiplex")
    void GetActiveTriadSets(TArray<EThreadVertex>& OutMP1, TArray<EThreadVertex>& OutMP2) const;

    // ========================================
    // PUBLIC API - STATE QUERIES
    // ========================================

    /** Get full tetrahedral state */
    UFUNCTION(BlueprintPure, Category = "Coupling|State")
    FTetrahedralCouplingState GetTetrahedralState() const;

    /** Get tetrahedral coherence */
    UFUNCTION(BlueprintPure, Category = "Coupling|State")
    float GetTetrahedralCoherence() const;

    /** Get tension balance */
    UFUNCTION(BlueprintPure, Category = "Coupling|State")
    float GetTensionBalance() const;

    // ========================================
    // PUBLIC API - ECHOBEATS INTEGRATION
    // ========================================

    /** Process echobeats step */
    UFUNCTION(BlueprintCallable, Category = "Coupling|Integration")
    void ProcessEchobeatStep(int32 Step, const FEchobeatsFullState& EchobeatsState);

    /** Map stream to vertex */
    UFUNCTION(BlueprintPure, Category = "Coupling|Integration")
    EThreadVertex MapStreamToVertex(ECognitiveStreamType StreamType) const;

    /** Map tensional coupling to edge */
    UFUNCTION(BlueprintPure, Category = "Coupling|Integration")
    EDyadicEdge MapCouplingToEdge(const FTensionalCoupling& Coupling) const;

protected:
    /** Current tetrahedral state */
    FTetrahedralCouplingState State;

    /** Multiplex timer */
    float MultiplexTimer = 0.0f;

    /** Dyad permutation sequence: P(1,2)→P(1,3)→P(1,4)→P(2,3)→P(2,4)→P(3,4) */
    TArray<TArray<EThreadVertex>> DyadPermutations;

    /** Triad permutation sequence for MP1: P[1,2,3]→P[1,2,4]→P[1,3,4]→P[2,3,4] */
    TArray<TArray<EThreadVertex>> TriadPermutationsMP1;

    /** Triad permutation sequence for MP2: P[1,3,4]→P[2,3,4]→P[1,2,3]→P[1,2,4] */
    TArray<TArray<EThreadVertex>> TriadPermutationsMP2;

    // Internal methods
    void ComputeTetrahedralCoherence();
    void ComputeTensionBalance();
    void PropagateAllTensions();
    void UpdateEntanglements(float DeltaTime);
    int32 GetEdgeIndex(EDyadicEdge Edge) const;
    int32 GetFaceIndex(ETriadicFace Face) const;
    int32 GetVertexIndex(EThreadVertex Vertex) const;
    TArray<EDyadicEdge> GetEdgesForFace(ETriadicFace Face) const;
    TArray<ETriadicFace> GetFacesForEdge(EDyadicEdge Edge) const;
    TArray<EThreadVertex> GetVerticesForEdge(EDyadicEdge Edge) const;
    TArray<EDyadicEdge> GetEdgesForVertex(EThreadVertex Vertex) const;
};

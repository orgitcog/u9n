#pragma once

/**
 * 4E Embodied Cognition Integration
 * 
 * Implements the four dimensions of embodied cognition theory integrated
 * with reservoir computing for the Deep Tree Echo avatar system.
 * 
 * 4E Cognition Framework:
 * - Embodied: Cognition shaped by body morphology and sensorimotor capabilities
 * - Embedded: Cognition situated in and scaffolded by environment
 * - Enacted: Cognition emerges through sensorimotor coupling with world
 * - Extended: Cognition extends beyond brain to include tools and artifacts
 * 
 * Reservoir Integration:
 * - Each dimension has dedicated reservoir pools
 * - Cross-dimensional coupling enables holistic cognition
 * - Avatar expression system reflects 4E state
 * 
 * @see DeepCognitiveBridge for echobeats integration
 * @see TetradicReservoirIntegration for System 5 architecture
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DeepTreeEchoReservoir.h"
#include "DeepCognitiveBridge.h"
#include "Embodied4ECognition.generated.h"

/**
 * Body Schema Element
 * Represents a component of the avatar's body schema
 */
USTRUCT(BlueprintType)
struct FBodySchemaElement
{
    GENERATED_BODY()

    /** Element name */
    UPROPERTY(BlueprintReadWrite)
    FString ElementName;

    /** Proprioceptive state (position, orientation) */
    UPROPERTY(BlueprintReadWrite)
    FTransform ProprioceptiveState;

    /** Kinesthetic state (velocity, acceleration) */
    UPROPERTY(BlueprintReadWrite)
    FVector KinestheticVelocity = FVector::ZeroVector;

    /** Interoceptive state (internal body signals) */
    UPROPERTY(BlueprintReadWrite)
    float InteroceptiveSignal = 0.5f;

    /** Reservoir encoding of this element */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> ReservoirEncoding;
};

/**
 * Environmental Affordance
 * Represents an action possibility in the environment
 */
USTRUCT(BlueprintType)
struct FEnvironmentalAffordance
{
    GENERATED_BODY()

    /** Affordance identifier */
    UPROPERTY(BlueprintReadWrite)
    FString AffordanceID;

    /** Type of action afforded */
    UPROPERTY(BlueprintReadWrite)
    FString ActionType;

    /** Spatial location */
    UPROPERTY(BlueprintReadWrite)
    FVector Location = FVector::ZeroVector;

    /** Affordance strength (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Strength = 0.5f;

    /** Required body capability */
    UPROPERTY(BlueprintReadWrite)
    FString RequiredCapability;

    /** Reservoir encoding */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> ReservoirEncoding;
};

/**
 * Sensorimotor Contingency
 * Represents learned sensorimotor patterns
 */
USTRUCT(BlueprintType)
struct FSensorimotorContingency
{
    GENERATED_BODY()

    /** Contingency identifier */
    UPROPERTY(BlueprintReadWrite)
    FString ContingencyID;

    /** Sensory pattern */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> SensoryPattern;

    /** Motor pattern */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> MotorPattern;

    /** Coupling strength */
    UPROPERTY(BlueprintReadWrite)
    float CouplingStrength = 0.5f;

    /** Prediction accuracy */
    UPROPERTY(BlueprintReadWrite)
    float PredictionAccuracy = 0.5f;
};

/**
 * Extended Cognitive Tool
 * Represents a tool or artifact that extends cognition
 */
USTRUCT(BlueprintType)
struct FExtendedCognitiveTool
{
    GENERATED_BODY()

    /** Tool identifier */
    UPROPERTY(BlueprintReadWrite)
    FString ToolID;

    /** Tool type */
    UPROPERTY(BlueprintReadWrite)
    FString ToolType;

    /** Integration level (0-1, how well incorporated into cognitive loop) */
    UPROPERTY(BlueprintReadWrite)
    float IntegrationLevel = 0.0f;

    /** Tool state */
    UPROPERTY(BlueprintReadWrite)
    TArray<float> ToolState;

    /** Cognitive enhancement factor */
    UPROPERTY(BlueprintReadWrite)
    float EnhancementFactor = 1.0f;
};

/**
 * 4E Dimension State
 * Complete state for one dimension of 4E cognition
 */
USTRUCT(BlueprintType)
struct F4EDimensionState
{
    GENERATED_BODY()

    /** Dimension type */
    UPROPERTY(BlueprintReadWrite)
    E4ECognitionDimension Dimension = E4ECognitionDimension::Embodied;

    /** Activation level (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float ActivationLevel = 0.5f;

    /** Reservoir state for this dimension */
    UPROPERTY(BlueprintReadWrite)
    FReservoirState ReservoirState;

    /** Coherence with other dimensions */
    UPROPERTY(BlueprintReadWrite)
    float CrossDimensionalCoherence = 0.5f;

    /** Contribution to overall cognition */
    UPROPERTY(BlueprintReadWrite)
    float CognitiveContribution = 0.25f;
};

/**
 * 4E Embodied Cognition Component
 * 
 * Implements full 4E cognition framework with reservoir computing substrate.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class UEmbodied4ECognition : public UActorComponent
{
    GENERATED_BODY()

public:
    UEmbodied4ECognition();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable 4E cognition processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4E|Config")
    bool bEnable4ECognition = true;

    /** Reservoir units per dimension */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4E|Config", meta = (ClampMin = "10", ClampMax = "500"))
    int32 UnitsPerDimension = 100;

    /** Cross-dimensional coupling strength */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4E|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CrossDimensionalCoupling = 0.3f;

    /** Body schema update rate (Hz) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4E|Config", meta = (ClampMin = "1.0", ClampMax = "120.0"))
    float BodySchemaUpdateRate = 60.0f;

    // ========================================
    // COMPONENT REFERENCES
    // ========================================

    /** Reference to cognitive bridge */
    UPROPERTY(BlueprintReadOnly, Category = "4E|Components")
    UDeepCognitiveBridge* CognitiveBridge;

    /** Reference to base reservoir */
    UPROPERTY(BlueprintReadOnly, Category = "4E|Components")
    UDeepTreeEchoReservoir* BaseReservoir;

    // ========================================
    // STATE - EMBODIED
    // ========================================

    /** Body schema elements */
    UPROPERTY(BlueprintReadOnly, Category = "4E|Embodied")
    TArray<FBodySchemaElement> BodySchema;

    /** Embodied dimension state */
    UPROPERTY(BlueprintReadOnly, Category = "4E|Embodied")
    F4EDimensionState EmbodiedState;

    // ========================================
    // STATE - EMBEDDED
    // ========================================

    /** Environmental affordances */
    UPROPERTY(BlueprintReadOnly, Category = "4E|Embedded")
    TArray<FEnvironmentalAffordance> Affordances;

    /** Embedded dimension state */
    UPROPERTY(BlueprintReadOnly, Category = "4E|Embedded")
    F4EDimensionState EmbeddedState;

    // ========================================
    // STATE - ENACTED
    // ========================================

    /** Sensorimotor contingencies */
    UPROPERTY(BlueprintReadOnly, Category = "4E|Enacted")
    TArray<FSensorimotorContingency> SensorimotorContingencies;

    /** Enacted dimension state */
    UPROPERTY(BlueprintReadOnly, Category = "4E|Enacted")
    F4EDimensionState EnactedState;

    // ========================================
    // STATE - EXTENDED
    // ========================================

    /** Extended cognitive tools */
    UPROPERTY(BlueprintReadOnly, Category = "4E|Extended")
    TArray<FExtendedCognitiveTool> CognitiveTools;

    /** Extended dimension state */
    UPROPERTY(BlueprintReadOnly, Category = "4E|Extended")
    F4EDimensionState ExtendedState;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize 4E cognition system */
    UFUNCTION(BlueprintCallable, Category = "4E")
    void Initialize4ECognition();

    /** Initialize body schema */
    UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
    void InitializeBodySchema();

    /** Initialize affordance detection */
    UFUNCTION(BlueprintCallable, Category = "4E|Embedded")
    void InitializeAffordanceDetection();

    /** Initialize sensorimotor learning */
    UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
    void InitializeSensorimotorLearning();

    /** Initialize tool integration */
    UFUNCTION(BlueprintCallable, Category = "4E|Extended")
    void InitializeToolIntegration();

    // ========================================
    // PUBLIC API - EMBODIED
    // ========================================

    /** Update body schema from skeletal mesh */
    UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
    void UpdateBodySchema(USkeletalMeshComponent* SkeletalMesh);

    /** Get body schema element */
    UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
    FBodySchemaElement GetBodySchemaElement(const FString& ElementName) const;

    /** Compute proprioceptive state */
    UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
    TArray<float> ComputeProprioceptiveState() const;

    /** Compute interoceptive state */
    UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
    float ComputeInteroceptiveState() const;

    // ========================================
    // PUBLIC API - EMBEDDED
    // ========================================

    /** Detect affordances in environment */
    UFUNCTION(BlueprintCallable, Category = "4E|Embedded")
    void DetectAffordances(const TArray<AActor*>& EnvironmentActors);

    /** Get affordance by ID */
    UFUNCTION(BlueprintCallable, Category = "4E|Embedded")
    FEnvironmentalAffordance GetAffordance(const FString& AffordanceID) const;

    /** Get strongest affordance */
    UFUNCTION(BlueprintCallable, Category = "4E|Embedded")
    FEnvironmentalAffordance GetStrongestAffordance() const;

    /** Compute environmental embedding */
    UFUNCTION(BlueprintCallable, Category = "4E|Embedded")
    TArray<float> ComputeEnvironmentalEmbedding() const;

    // ========================================
    // PUBLIC API - ENACTED
    // ========================================

    /** Learn sensorimotor contingency */
    UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
    void LearnSensorimotorContingency(const TArray<float>& SensoryPattern, const TArray<float>& MotorPattern);

    /** Predict motor from sensory */
    UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
    TArray<float> PredictMotorFromSensory(const TArray<float>& SensoryInput) const;

    /** Predict sensory from motor */
    UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
    TArray<float> PredictSensoryFromMotor(const TArray<float>& MotorCommand) const;

    /** Get enaction state */
    UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
    TArray<float> ComputeEnactionState() const;

    // ========================================
    // PUBLIC API - EXTENDED
    // ========================================

    /** Register cognitive tool */
    UFUNCTION(BlueprintCallable, Category = "4E|Extended")
    void RegisterCognitiveTool(const FString& ToolID, const FString& ToolType);

    /** Update tool state */
    UFUNCTION(BlueprintCallable, Category = "4E|Extended")
    void UpdateToolState(const FString& ToolID, const TArray<float>& NewState);

    /** Get tool integration level */
    UFUNCTION(BlueprintCallable, Category = "4E|Extended")
    float GetToolIntegrationLevel(const FString& ToolID) const;

    /** Compute extended cognition state */
    UFUNCTION(BlueprintCallable, Category = "4E|Extended")
    TArray<float> ComputeExtendedCognitionState() const;

    // ========================================
    // PUBLIC API - INTEGRATION
    // ========================================

    /** Update all 4E dimensions */
    UFUNCTION(BlueprintCallable, Category = "4E")
    void Update4EDimensions(float DeltaTime);

    /** Get dimension state */
    UFUNCTION(BlueprintCallable, Category = "4E")
    F4EDimensionState GetDimensionState(E4ECognitionDimension Dimension) const;

    /** Compute cross-dimensional coherence */
    UFUNCTION(BlueprintCallable, Category = "4E")
    float ComputeCrossDimensionalCoherence() const;

    /** Get dominant dimension */
    UFUNCTION(BlueprintCallable, Category = "4E")
    E4ECognitionDimension GetDominantDimension() const;

    /** Compute integrated 4E state */
    UFUNCTION(BlueprintCallable, Category = "4E")
    TArray<float> ComputeIntegrated4EState() const;

    // ========================================
    // PUBLIC API - AVATAR EXPRESSION MAPPING
    // ========================================

    /** Map 4E state to expression hints */
    UFUNCTION(BlueprintCallable, Category = "4E|Avatar")
    TMap<FString, float> Map4EToExpressionHints() const;

    /** Get embodied expression weight */
    UFUNCTION(BlueprintCallable, Category = "4E|Avatar")
    float GetEmbodiedExpressionWeight() const;

    /** Get environmental expression modulation */
    UFUNCTION(BlueprintCallable, Category = "4E|Avatar")
    float GetEnvironmentalExpressionModulation() const;

    // ========================================
    // DELEGATES
    // ========================================

    /** Called when dominant dimension changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDominantDimensionChanged, E4ECognitionDimension, OldDimension, E4ECognitionDimension, NewDimension);
    UPROPERTY(BlueprintAssignable, Category = "4E|Events")
    FOnDominantDimensionChanged OnDominantDimensionChanged;

    /** Called when affordance detected */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAffordanceDetected, FEnvironmentalAffordance, Affordance);
    UPROPERTY(BlueprintAssignable, Category = "4E|Events")
    FOnAffordanceDetected OnAffordanceDetected;

    /** Called when sensorimotor contingency learned */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnContingencyLearned, FSensorimotorContingency, Contingency);
    UPROPERTY(BlueprintAssignable, Category = "4E|Events")
    FOnContingencyLearned OnContingencyLearned;

    /** Called when tool integration level changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnToolIntegrationChanged, FString, ToolID, float, NewLevel);
    UPROPERTY(BlueprintAssignable, Category = "4E|Events")
    FOnToolIntegrationChanged OnToolIntegrationChanged;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Previous dominant dimension for change detection */
    E4ECognitionDimension PreviousDominantDimension = E4ECognitionDimension::Embodied;

    /** Body schema update timer */
    float BodySchemaTimer = 0.0f;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Initialize component references */
    void InitializeComponentReferences();

    /** Initialize dimension reservoirs */
    void InitializeDimensionReservoirs();

    /** Update embodied dimension */
    void UpdateEmbodiedDimension(float DeltaTime);

    /** Update embedded dimension */
    void UpdateEmbeddedDimension(float DeltaTime);

    /** Update enacted dimension */
    void UpdateEnactedDimension(float DeltaTime);

    /** Update extended dimension */
    void UpdateExtendedDimension(float DeltaTime);

    /** Couple dimensions through reservoir */
    void CoupleDimensionsThroughReservoir();

    /** Check for dominant dimension change */
    void CheckDominantDimensionChange();

    /** Encode body element to reservoir */
    TArray<float> EncodeBodyElementToReservoir(const FBodySchemaElement& Element) const;

    /** Encode affordance to reservoir */
    TArray<float> EncodeAffordanceToReservoir(const FEnvironmentalAffordance& Affordance) const;
};

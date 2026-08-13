#pragma once

/**
 * 4E Embodied Cognition Component
 * 
 * Implements the four dimensions of embodied cognition:
 * - Embodied: Cognition shaped by body morphology and sensorimotor capabilities
 * - Embedded: Cognition situated in and coupled with the environment
 * - Enacted: Cognition emerges through action-perception loops
 * - Extended: Cognition extends beyond brain to include tools and environment
 * 
 * Integrates with Deep Tree Echo for avatar-based cognitive embodiment.
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EmbodiedCognitionComponent.generated.h"

/**
 * Body Schema - Internal representation of body structure
 */
USTRUCT(BlueprintType)
struct FBodySchema
{
    GENERATED_BODY()

    /** Body part positions (relative to root) */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, FVector> BodyPartPositions;

    /** Body part orientations */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, FRotator> BodyPartOrientations;

    /** Joint angles */
    UPROPERTY(BlueprintReadWrite)
    TMap<FString, float> JointAngles;

    /** Peripersonal space (reachable area) */
    UPROPERTY(BlueprintReadWrite)
    float PeripersonalRadius = 100.0f;

    /** Body schema coherence */
    UPROPERTY(BlueprintReadWrite)
    float SchemaCoherence = 1.0f;
};

/**
 * Sensorimotor Contingency - Action-perception relationship
 */
USTRUCT(BlueprintType)
struct FSensorimotorContingency
{
    GENERATED_BODY()

    /** Contingency ID */
    UPROPERTY(BlueprintReadWrite)
    FString ContingencyID;

    /** Action that triggers the contingency */
    UPROPERTY(BlueprintReadWrite)
    FString TriggeringAction;

    /** Expected sensory outcome */
    UPROPERTY(BlueprintReadWrite)
    FString ExpectedOutcome;

    /** Actual sensory outcome */
    UPROPERTY(BlueprintReadWrite)
    FString ActualOutcome;

    /** Prediction error */
    UPROPERTY(BlueprintReadWrite)
    float PredictionError = 0.0f;

    /** Contingency strength (learned reliability) */
    UPROPERTY(BlueprintReadWrite)
    float Strength = 0.5f;
};

/**
 * Environmental Affordance - Action possibility in environment
 */
USTRUCT(BlueprintType)
struct FEnvironmentalAffordance
{
    GENERATED_BODY()

    /** Affordance ID */
    UPROPERTY(BlueprintReadWrite)
    FString AffordanceID;

    /** Object or surface providing affordance */
    UPROPERTY(BlueprintReadWrite)
    FString Provider;

    /** Type of affordance (graspable, sittable, walkable, etc.) */
    UPROPERTY(BlueprintReadWrite)
    FString AffordanceType;

    /** Location in world space */
    UPROPERTY(BlueprintReadWrite)
    FVector Location;

    /** Affordance salience (how noticeable) */
    UPROPERTY(BlueprintReadWrite)
    float Salience = 0.5f;

    /** Affordance accessibility (can we use it now) */
    UPROPERTY(BlueprintReadWrite)
    float Accessibility = 1.0f;

    /** Required body capability */
    UPROPERTY(BlueprintReadWrite)
    FString RequiredCapability;
};

/**
 * Cognitive Tool - External cognitive resource
 */
USTRUCT(BlueprintType)
struct FCognitiveTool
{
    GENERATED_BODY()

    /** Tool ID */
    UPROPERTY(BlueprintReadWrite)
    FString ToolID;

    /** Tool name */
    UPROPERTY(BlueprintReadWrite)
    FString ToolName;

    /** Tool type (memory aid, calculation, communication, etc.) */
    UPROPERTY(BlueprintReadWrite)
    FString ToolType;

    /** Cognitive function extended */
    UPROPERTY(BlueprintReadWrite)
    FString ExtendedFunction;

    /** Integration level (how well incorporated into cognition) */
    UPROPERTY(BlueprintReadWrite)
    float IntegrationLevel = 0.0f;

    /** Tool is currently active */
    UPROPERTY(BlueprintReadWrite)
    bool bIsActive = false;
};

/**
 * Somatic Marker - Body-based emotional signal
 */
USTRUCT(BlueprintType)
struct FSomaticMarker
{
    GENERATED_BODY()

    /** Marker ID */
    UPROPERTY(BlueprintReadWrite)
    FString MarkerID;

    /** Associated stimulus/situation */
    UPROPERTY(BlueprintReadWrite)
    FString AssociatedStimulus;

    /** Body region affected */
    UPROPERTY(BlueprintReadWrite)
    FString BodyRegion;

    /** Valence (-1 to 1, negative to positive) */
    UPROPERTY(BlueprintReadWrite)
    float Valence = 0.0f;

    /** Arousal (0 to 1, calm to excited) */
    UPROPERTY(BlueprintReadWrite)
    float Arousal = 0.5f;

    /** Marker strength */
    UPROPERTY(BlueprintReadWrite)
    float Strength = 0.5f;
};

/**
 * 4E Embodied Cognition Component
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UEmbodiedCognitionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UEmbodiedCognitionComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable embodied processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4E|Config")
    bool bEnableEmbodied = true;

    /** Enable embedded processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4E|Config")
    bool bEnableEmbedded = true;

    /** Enable enacted processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4E|Config")
    bool bEnableEnacted = true;

    /** Enable extended processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4E|Config")
    bool bEnableExtended = true;

    /** Sensorimotor learning rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "4E|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SensorimotorLearningRate = 0.1f;

    // ========================================
    // EMBODIED STATE
    // ========================================

    /** Current body schema */
    UPROPERTY(BlueprintReadOnly, Category = "4E|Embodied")
    FBodySchema BodySchema;

    /** Active somatic markers */
    UPROPERTY(BlueprintReadOnly, Category = "4E|Embodied")
    TArray<FSomaticMarker> ActiveSomaticMarkers;

    // ========================================
    // EMBEDDED STATE
    // ========================================

    /** Detected affordances */
    UPROPERTY(BlueprintReadOnly, Category = "4E|Embedded")
    TArray<FEnvironmentalAffordance> DetectedAffordances;

    /** Current environmental niche */
    UPROPERTY(BlueprintReadOnly, Category = "4E|Embedded")
    FString CurrentNiche;

    // ========================================
    // ENACTED STATE
    // ========================================

    /** Learned sensorimotor contingencies */
    UPROPERTY(BlueprintReadOnly, Category = "4E|Enacted")
    TArray<FSensorimotorContingency> LearnedContingencies;

    /** Current action-perception loop state */
    UPROPERTY(BlueprintReadOnly, Category = "4E|Enacted")
    FString CurrentActionPerceptionState;

    // ========================================
    // EXTENDED STATE
    // ========================================

    /** Available cognitive tools */
    UPROPERTY(BlueprintReadOnly, Category = "4E|Extended")
    TArray<FCognitiveTool> AvailableTools;

    /** External memory references */
    UPROPERTY(BlueprintReadOnly, Category = "4E|Extended")
    TArray<FString> ExternalMemoryRefs;

    // ========================================
    // PUBLIC API - EMBODIED
    // ========================================

    /** Update body schema from skeletal mesh */
    UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
    void UpdateBodySchema(const TMap<FString, FVector>& BodyPartPositions, const TMap<FString, FRotator>& BodyPartOrientations);

    /** Add somatic marker */
    UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
    void AddSomaticMarker(const FString& Stimulus, const FString& BodyRegion, float Valence, float Arousal);

    /** Get somatic marker for stimulus */
    UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
    FSomaticMarker GetSomaticMarkerForStimulus(const FString& Stimulus) const;

    /** Check if position is in peripersonal space */
    UFUNCTION(BlueprintCallable, Category = "4E|Embodied")
    bool IsInPeripersonalSpace(const FVector& Position) const;

    // ========================================
    // PUBLIC API - EMBEDDED
    // ========================================

    /** Detect affordances in environment */
    UFUNCTION(BlueprintCallable, Category = "4E|Embedded")
    void DetectAffordances(const TArray<AActor*>& NearbyActors);

    /** Add affordance */
    UFUNCTION(BlueprintCallable, Category = "4E|Embedded")
    void AddAffordance(const FString& Provider, const FString& Type, const FVector& Location, float Salience);

    /** Get most salient affordance */
    UFUNCTION(BlueprintCallable, Category = "4E|Embedded")
    FEnvironmentalAffordance GetMostSalientAffordance() const;

    /** Set current niche */
    UFUNCTION(BlueprintCallable, Category = "4E|Embedded")
    void SetCurrentNiche(const FString& Niche);

    // ========================================
    // PUBLIC API - ENACTED
    // ========================================

    /** Learn sensorimotor contingency */
    UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
    void LearnContingency(const FString& Action, const FString& ExpectedOutcome, const FString& ActualOutcome);

    /** Predict outcome for action */
    UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
    FString PredictOutcome(const FString& Action) const;

    /** Get prediction error for last action */
    UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
    float GetLastPredictionError() const;

    /** Update action-perception state */
    UFUNCTION(BlueprintCallable, Category = "4E|Enacted")
    void UpdateActionPerceptionState(const FString& State);

    // ========================================
    // PUBLIC API - EXTENDED
    // ========================================

    /** Register cognitive tool */
    UFUNCTION(BlueprintCallable, Category = "4E|Extended")
    void RegisterCognitiveTool(const FString& ToolName, const FString& ToolType, const FString& ExtendedFunction);

    /** Activate cognitive tool */
    UFUNCTION(BlueprintCallable, Category = "4E|Extended")
    void ActivateTool(const FString& ToolID);

    /** Deactivate cognitive tool */
    UFUNCTION(BlueprintCallable, Category = "4E|Extended")
    void DeactivateTool(const FString& ToolID);

    /** Add external memory reference */
    UFUNCTION(BlueprintCallable, Category = "4E|Extended")
    void AddExternalMemoryRef(const FString& Reference);

    /** Get tool integration level */
    UFUNCTION(BlueprintCallable, Category = "4E|Extended")
    float GetToolIntegrationLevel(const FString& ToolID) const;

    // ========================================
    // PUBLIC API - INTEGRATION
    // ========================================

    /** Get overall 4E integration score */
    UFUNCTION(BlueprintCallable, Category = "4E")
    float Get4EIntegrationScore() const;

    /** Get embodied score */
    UFUNCTION(BlueprintCallable, Category = "4E")
    float GetEmbodiedScore() const;

    /** Get embedded score */
    UFUNCTION(BlueprintCallable, Category = "4E")
    float GetEmbeddedScore() const;

    /** Get enacted score */
    UFUNCTION(BlueprintCallable, Category = "4E")
    float GetEnactedScore() const;

    /** Get extended score */
    UFUNCTION(BlueprintCallable, Category = "4E")
    float GetExtendedScore() const;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Last prediction error */
    float LastPredictionError = 0.0f;

    /** Contingency ID counter */
    int32 ContingencyIDCounter = 0;

    /** Tool ID counter */
    int32 ToolIDCounter = 0;

    /** Marker ID counter */
    int32 MarkerIDCounter = 0;

    /** Affordance ID counter */
    int32 AffordanceIDCounter = 0;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Update somatic markers decay */
    void UpdateSomaticMarkerDecay(float DeltaTime);

    /** Update affordance salience */
    void UpdateAffordanceSalience(float DeltaTime);

    /** Update contingency strengths */
    void UpdateContingencyStrengths(float DeltaTime);

    /** Update tool integration */
    void UpdateToolIntegration(float DeltaTime);

    /** Generate unique IDs */
    FString GenerateContingencyID();
    FString GenerateToolID();
    FString GenerateMarkerID();
    FString GenerateAffordanceID();
};

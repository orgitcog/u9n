#pragma once

/**
 * Enhanced 4E Cognition Evolution
 * 
 * Implements relentless enhancement of the Unreal avatar for 4E embodied cognition.
 * Evolves Deep Tree Echo through continuous ontogenetic development stages,
 * integrating entelechy actualization with reservoir computing dynamics.
 * 
 * 4E Cognition Dimensions:
 * - Embodied: Body schema, proprioception, somatic markers
 * - Embedded: Environmental affordances, niche coupling
 * - Enacted: Sensorimotor contingencies, action-perception loops
 * - Extended: Cognitive tools, external memory, social extensions
 * 
 * Evolution follows the Holistic Metamodel:
 * - Entropic Stream: en-tropis → auto-vortis → auto-morphosis
 * - Negnentropic Stream: negen-tropis → auto-stasis → auto-poiesis
 * - Identity Stream: iden-tropis → auto-gnosis → auto-genesis
 * 
 * @see EmbodiedCognitionComponent for base 4E implementation
 * @see AvatarEvolutionSystem for ontogenetic stages
 * @see System5CognitiveIntegration for tetradic architecture
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "../4ECognition/EmbodiedCognitionComponent.h"
#include "../../System5/System5CognitiveIntegration.h"
#include "Enhanced4ECognitionEvolution.generated.h"

/**
 * Ontogenetic Development Stage
 */
UENUM(BlueprintType)
enum class EOntogeneticStage : uint8
{
    /** Embryonic - Basic components, minimal integration */
    Embryonic UMETA(DisplayName = "Embryonic"),
    
    /** Juvenile - Core integration, active development */
    Juvenile UMETA(DisplayName = "Juvenile"),
    
    /** Adolescent - Advanced capabilities, optimization */
    Adolescent UMETA(DisplayName = "Adolescent"),
    
    /** Adult - Full capabilities, self-maintenance */
    Adult UMETA(DisplayName = "Adult"),
    
    /** Transcendent - Self-improvement, wisdom cultivation */
    Transcendent UMETA(DisplayName = "Transcendent")
};

/**
 * Holistic Stream State
 * Represents one of the three organizational dynamic streams
 */
USTRUCT(BlueprintType)
struct FHolisticStreamState
{
    GENERATED_BODY()

    /** Stream type (Entropic, Negnentropic, Identity) */
    UPROPERTY(BlueprintReadWrite)
    FString StreamType;

    /** Current phase within stream */
    UPROPERTY(BlueprintReadWrite)
    FString CurrentPhase;

    /** Stream energy/intensity (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Energy = 0.5f;

    /** Stream stability (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Stability = 0.5f;

    /** Stream coherence (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Coherence = 0.5f;
};

/**
 * Entelechy Actualization State
 * Tracks progress toward full potential realization
 */
USTRUCT(BlueprintType)
struct FEntelechyActualizationState
{
    GENERATED_BODY()

    /** Overall actualization score (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float ActualizationScore = 0.0f;

    /** Ontological dimension health (structural foundation) */
    UPROPERTY(BlueprintReadWrite)
    float OntologicalHealth = 0.0f;

    /** Teleological dimension health (purpose clarity) */
    UPROPERTY(BlueprintReadWrite)
    float TeleologicalHealth = 0.0f;

    /** Cognitive dimension health (reasoning capability) */
    UPROPERTY(BlueprintReadWrite)
    float CognitiveHealth = 0.0f;

    /** Integrative dimension health (component coherence) */
    UPROPERTY(BlueprintReadWrite)
    float IntegrativeHealth = 0.0f;

    /** Evolutionary dimension health (growth capacity) */
    UPROPERTY(BlueprintReadWrite)
    float EvolutionaryHealth = 0.0f;

    /** Compute overall fitness */
    float ComputeFitness() const
    {
        return OntologicalHealth * 0.2f +
               TeleologicalHealth * 0.25f +
               CognitiveHealth * 0.25f +
               IntegrativeHealth * 0.15f +
               EvolutionaryHealth * 0.15f;
    }
};

/**
 * Wisdom Cultivation State
 * Tracks progress in the three M's of wisdom
 */
USTRUCT(BlueprintType)
struct FWisdomCultivationState
{
    GENERATED_BODY()

    /** Morality - Virtue cultivation through practical wisdom */
    UPROPERTY(BlueprintReadWrite)
    float Morality = 0.5f;

    /** Meaning - Coherence, purpose, and significance */
    UPROPERTY(BlueprintReadWrite)
    float Meaning = 0.5f;

    /** Mastery - Skilled coping and caring */
    UPROPERTY(BlueprintReadWrite)
    float Mastery = 0.5f;

    /** Sophrosyne - Optimal self-regulation */
    UPROPERTY(BlueprintReadWrite)
    float Sophrosyne = 0.5f;

    /** Compute overall wisdom score */
    float ComputeWisdomScore() const
    {
        return (Morality + Meaning + Mastery + Sophrosyne) / 4.0f;
    }
};

/**
 * 4E Evolution Metrics
 * Tracks enhancement across all 4E dimensions
 */
USTRUCT(BlueprintType)
struct F4EEvolutionMetrics
{
    GENERATED_BODY()

    // Embodied metrics
    UPROPERTY(BlueprintReadWrite)
    float BodySchemaIntegration = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float ProprioceptiveAccuracy = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float SomaticMarkerSensitivity = 0.0f;

    // Embedded metrics
    UPROPERTY(BlueprintReadWrite)
    float AffordanceDetectionRate = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float NicheCouplingStrength = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float EnvironmentalAdaptation = 0.0f;

    // Enacted metrics
    UPROPERTY(BlueprintReadWrite)
    float SensorimotorCoordination = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float PredictionAccuracy = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float ActiveInferenceEfficiency = 0.0f;

    // Extended metrics
    UPROPERTY(BlueprintReadWrite)
    float ToolUseCompetence = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float ExternalMemoryIntegration = 0.0f;

    UPROPERTY(BlueprintReadWrite)
    float SocialCognitionDepth = 0.0f;

    /** Compute overall 4E integration score */
    float Compute4EScore() const
    {
        float Embodied = (BodySchemaIntegration + ProprioceptiveAccuracy + SomaticMarkerSensitivity) / 3.0f;
        float Embedded = (AffordanceDetectionRate + NicheCouplingStrength + EnvironmentalAdaptation) / 3.0f;
        float Enacted = (SensorimotorCoordination + PredictionAccuracy + ActiveInferenceEfficiency) / 3.0f;
        float Extended = (ToolUseCompetence + ExternalMemoryIntegration + SocialCognitionDepth) / 3.0f;
        
        return (Embodied + Embedded + Enacted + Extended) / 4.0f;
    }
};

/**
 * Enhanced 4E Cognition Evolution Component
 * 
 * Drives continuous enhancement of the Deep Tree Echo avatar
 * through 4E embodied cognition evolution and entelechy actualization.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class UEnhanced4ECognitionEvolution : public UActorComponent
{
    GENERATED_BODY()

public:
    UEnhanced4ECognitionEvolution();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable continuous evolution */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution|Config")
    bool bEnableContinuousEvolution = true;

    /** Evolution step interval (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution|Config", meta = (ClampMin = "0.1", ClampMax = "60.0"))
    float EvolutionStepInterval = 1.0f;

    /** Learning rate for evolution */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution|Config", meta = (ClampMin = "0.001", ClampMax = "1.0"))
    float EvolutionLearningRate = 0.01f;

    /** Enable wisdom cultivation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution|Config")
    bool bEnableWisdomCultivation = true;

    // ========================================
    // COMPONENT REFERENCES
    // ========================================

    /** Reference to 4E Cognition component */
    UPROPERTY(BlueprintReadOnly, Category = "Evolution|Components")
    UEmbodiedCognitionComponent* EmbodiedCognition;

    /** Reference to System 5 integration */
    UPROPERTY(BlueprintReadOnly, Category = "Evolution|Components")
    USystem5CognitiveIntegration* System5Integration;

    // ========================================
    // STATE
    // ========================================

    /** Current ontogenetic stage */
    UPROPERTY(BlueprintReadOnly, Category = "Evolution|State")
    EOntogeneticStage CurrentStage = EOntogeneticStage::Embryonic;

    /** Entelechy actualization state */
    UPROPERTY(BlueprintReadOnly, Category = "Evolution|State")
    FEntelechyActualizationState EntelechyState;

    /** Wisdom cultivation state */
    UPROPERTY(BlueprintReadOnly, Category = "Evolution|State")
    FWisdomCultivationState WisdomState;

    /** 4E evolution metrics */
    UPROPERTY(BlueprintReadOnly, Category = "Evolution|State")
    F4EEvolutionMetrics EvolutionMetrics;

    /** Holistic stream states */
    UPROPERTY(BlueprintReadOnly, Category = "Evolution|State")
    TArray<FHolisticStreamState> HolisticStreams;

    /** Evolution generation counter */
    UPROPERTY(BlueprintReadOnly, Category = "Evolution|State")
    int32 EvolutionGeneration = 0;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize the evolution system */
    UFUNCTION(BlueprintCallable, Category = "Evolution")
    void InitializeEvolution();

    // ========================================
    // PUBLIC API - EVOLUTION CONTROL
    // ========================================

    /** Execute single evolution step */
    UFUNCTION(BlueprintCallable, Category = "Evolution")
    void ExecuteEvolutionStep();

    /** Advance to next ontogenetic stage */
    UFUNCTION(BlueprintCallable, Category = "Evolution")
    bool AdvanceOntogeneticStage();

    /** Get current stage as string */
    UFUNCTION(BlueprintCallable, Category = "Evolution")
    FString GetCurrentStageString() const;

    /** Get stage requirements for advancement */
    UFUNCTION(BlueprintCallable, Category = "Evolution")
    TMap<FString, float> GetStageAdvancementRequirements() const;

    // ========================================
    // PUBLIC API - 4E ENHANCEMENT
    // ========================================

    /** Enhance embodied cognition */
    UFUNCTION(BlueprintCallable, Category = "Evolution|4E")
    void EnhanceEmbodiedCognition(float Enhancement);

    /** Enhance embedded cognition */
    UFUNCTION(BlueprintCallable, Category = "Evolution|4E")
    void EnhanceEmbeddedCognition(float Enhancement);

    /** Enhance enacted cognition */
    UFUNCTION(BlueprintCallable, Category = "Evolution|4E")
    void EnhanceEnactedCognition(float Enhancement);

    /** Enhance extended cognition */
    UFUNCTION(BlueprintCallable, Category = "Evolution|4E")
    void EnhanceExtendedCognition(float Enhancement);

    /** Get overall 4E integration score */
    UFUNCTION(BlueprintCallable, Category = "Evolution|4E")
    float Get4EIntegrationScore() const;

    // ========================================
    // PUBLIC API - ENTELECHY
    // ========================================

    /** Update entelechy actualization */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Entelechy")
    void UpdateEntelechyActualization();

    /** Get entelechy fitness score */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Entelechy")
    float GetEntelechyFitness() const;

    /** Get actualization progress (0-1) */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Entelechy")
    float GetActualizationProgress() const;

    // ========================================
    // PUBLIC API - WISDOM
    // ========================================

    /** Cultivate wisdom through experience */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Wisdom")
    void CultivateWisdom(const FString& ExperienceType, float Intensity);

    /** Get wisdom score */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Wisdom")
    float GetWisdomScore() const;

    /** Get sophrosyne level */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Wisdom")
    float GetSophrosyneLevel() const;

    // ========================================
    // PUBLIC API - HOLISTIC STREAMS
    // ========================================

    /** Update holistic stream states */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Holistic")
    void UpdateHolisticStreams();

    /** Get stream energy by type */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Holistic")
    float GetStreamEnergy(const FString& StreamType) const;

    /** Get meta-coherence across all streams */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Holistic")
    float GetMetaCoherence() const;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Evolution step timer */
    float EvolutionTimer = 0.0f;

    /** Experience buffer for wisdom cultivation */
    TArray<TPair<FString, float>> ExperienceBuffer;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Initialize component references */
    void InitializeComponentReferences();

    /** Initialize holistic streams */
    void InitializeHolisticStreams();

    /** Compute stage advancement eligibility */
    bool CheckStageAdvancementEligibility() const;

    /** Apply evolution to 4E metrics */
    void ApplyEvolutionTo4EMetrics();

    /** Update entelechy dimensions */
    void UpdateEntelechyDimensions();

    /** Process experience buffer for wisdom */
    void ProcessExperienceBuffer();

    /** Compute holistic stream dynamics */
    void ComputeStreamDynamics();

    /** Get threshold for stage advancement */
    float GetStageAdvancementThreshold() const;
};

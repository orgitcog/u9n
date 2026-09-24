#pragma once

/**
 * Wisdom Cultivation - Relevance Realization Framework
 * 
 * Implements John Vervaeke's Relevance Realization theory:
 * - Salience Landscape: What stands out as important
 * - Affordance Landscape: What actions are available
 * - Opponent Processing: Balancing exploration/exploitation
 * - Perspectival Knowing: Knowing from a point of view
 * - Participatory Knowing: Knowing through engagement
 * - Procedural Knowing: Knowing how to do
 * - Propositional Knowing: Knowing that (facts)
 * 
 * Wisdom Dimensions:
 * - Sophia: Theoretical wisdom (understanding reality)
 * - Phronesis: Practical wisdom (right action in context)
 * - Gnosis: Transformative wisdom (self-knowledge)
 * - Techne: Craft wisdom (skillful making)
 * 
 * Cognitive Virtues:
 * - Attention: Capacity to focus relevantly
 * - Insight: Capacity to restructure understanding
 * - Inference: Capacity to draw valid conclusions
 * - Imagination: Capacity to generate possibilities
 * - Integration: Capacity to unify perspectives
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WisdomCultivation.generated.h"

// ========================================
// ENUMERATIONS
// ========================================

/**
 * Wisdom Dimension
 */
UENUM(BlueprintType)
enum class EWisdomDimension : uint8
{
    /** Theoretical wisdom - understanding reality */
    Sophia UMETA(DisplayName = "Sophia (Theoretical)"),
    
    /** Practical wisdom - right action in context */
    Phronesis UMETA(DisplayName = "Phronesis (Practical)"),
    
    /** Transformative wisdom - self-knowledge */
    Gnosis UMETA(DisplayName = "Gnosis (Transformative)"),
    
    /** Craft wisdom - skillful making */
    Techne UMETA(DisplayName = "Techne (Craft)")
};

/**
 * Knowing Type
 */
UENUM(BlueprintType)
enum class EKnowingType : uint8
{
    /** Knowing that - propositional facts */
    Propositional UMETA(DisplayName = "Propositional (That)"),
    
    /** Knowing how - procedural skills */
    Procedural UMETA(DisplayName = "Procedural (How)"),
    
    /** Knowing from - perspectival viewpoint */
    Perspectival UMETA(DisplayName = "Perspectival (From)"),
    
    /** Knowing through - participatory engagement */
    Participatory UMETA(DisplayName = "Participatory (Through)")
};

/**
 * Cognitive Virtue
 */
UENUM(BlueprintType)
enum class ECognitiveVirtue : uint8
{
    /** Capacity to focus relevantly */
    Attention UMETA(DisplayName = "Attention"),
    
    /** Capacity to restructure understanding */
    Insight UMETA(DisplayName = "Insight"),
    
    /** Capacity to draw valid conclusions */
    Inference UMETA(DisplayName = "Inference"),
    
    /** Capacity to generate possibilities */
    Imagination UMETA(DisplayName = "Imagination"),
    
    /** Capacity to unify perspectives */
    Integration UMETA(DisplayName = "Integration")
};

/**
 * Opponent Process Type
 */
UENUM(BlueprintType)
enum class EOpponentProcess : uint8
{
    /** Exploration vs Exploitation */
    ExplorationExploitation UMETA(DisplayName = "Exploration-Exploitation"),
    
    /** Focus vs Diffusion */
    FocusDiffusion UMETA(DisplayName = "Focus-Diffusion"),
    
    /** Assimilation vs Accommodation */
    AssimilationAccommodation UMETA(DisplayName = "Assimilation-Accommodation"),
    
    /** Abstraction vs Concretization */
    AbstractionConcretization UMETA(DisplayName = "Abstraction-Concretization"),
    
    /** Analysis vs Synthesis */
    AnalysisSynthesis UMETA(DisplayName = "Analysis-Synthesis")
};

// ========================================
// STRUCTURES
// ========================================

/**
 * Salience Item - Something that stands out
 */
USTRUCT(BlueprintType)
struct FSalienceItem
{
    GENERATED_BODY()

    /** Item identifier */
    UPROPERTY(BlueprintReadWrite)
    FString Id;

    /** Salience value (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Salience = 0.0f;

    /** Relevance to current goals (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Relevance = 0.0f;

    /** Urgency (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Urgency = 0.0f;

    /** Novelty (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Novelty = 0.0f;

    /** Time since last attention */
    UPROPERTY(BlueprintReadWrite)
    float TimeSinceAttention = 0.0f;
};

/**
 * Salience Landscape State
 */
USTRUCT(BlueprintType)
struct FSalienceLandscape
{
    GENERATED_BODY()

    /** Items in the landscape */
    UPROPERTY(BlueprintReadWrite)
    TArray<FSalienceItem> Items;

    /** Current focus target */
    UPROPERTY(BlueprintReadWrite)
    FString FocusTarget;

    /** Landscape coherence (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Coherence = 1.0f;

    /** Landscape complexity (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Complexity = 0.5f;

    /** Total salience energy */
    UPROPERTY(BlueprintReadWrite)
    float TotalSalience = 0.0f;
};

/**
 * Affordance - Available action possibility
 */
USTRUCT(BlueprintType)
struct FAffordance
{
    GENERATED_BODY()

    /** Affordance identifier */
    UPROPERTY(BlueprintReadWrite)
    FString Id;

    /** Action description */
    UPROPERTY(BlueprintReadWrite)
    FString Action;

    /** Availability (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Availability = 0.0f;

    /** Expected utility (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float ExpectedUtility = 0.0f;

    /** Risk level (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Risk = 0.0f;

    /** Skill requirement (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float SkillRequirement = 0.0f;

    /** Contextual fit (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float ContextualFit = 0.0f;
};

/**
 * Affordance Landscape State
 */
USTRUCT(BlueprintType)
struct FAffordanceLandscape
{
    GENERATED_BODY()

    /** Available affordances */
    UPROPERTY(BlueprintReadWrite)
    TArray<FAffordance> Affordances;

    /** Currently selected affordance */
    UPROPERTY(BlueprintReadWrite)
    FString SelectedAffordance;

    /** Landscape richness (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Richness = 0.5f;

    /** Action readiness (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float ActionReadiness = 0.5f;
};

/**
 * Opponent Process State
 */
USTRUCT(BlueprintType)
struct FOpponentProcessState
{
    GENERATED_BODY()

    /** Process type */
    UPROPERTY(BlueprintReadWrite)
    EOpponentProcess ProcessType = EOpponentProcess::ExplorationExploitation;

    /** Balance value (-1 to 1, 0 = balanced) */
    UPROPERTY(BlueprintReadWrite)
    float Balance = 0.0f;

    /** Pole A strength (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float PoleAStrength = 0.5f;

    /** Pole B strength (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float PoleBStrength = 0.5f;

    /** Oscillation frequency */
    UPROPERTY(BlueprintReadWrite)
    float OscillationFrequency = 1.0f;

    /** Current phase (0-2π) */
    UPROPERTY(BlueprintReadWrite)
    float Phase = 0.0f;
};

/**
 * Knowing State
 */
USTRUCT(BlueprintType)
struct FKnowingState
{
    GENERATED_BODY()

    /** Knowing type */
    UPROPERTY(BlueprintReadWrite)
    EKnowingType Type = EKnowingType::Propositional;

    /** Strength (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Strength = 0.5f;

    /** Integration with other knowing types (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Integration = 0.5f;

    /** Active engagement level (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Engagement = 0.5f;
};

/**
 * Cognitive Virtue State
 */
USTRUCT(BlueprintType)
struct FCognitiveVirtueState
{
    GENERATED_BODY()

    /** Virtue type */
    UPROPERTY(BlueprintReadWrite)
    ECognitiveVirtue Virtue = ECognitiveVirtue::Attention;

    /** Current strength (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Strength = 0.5f;

    /** Development level (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Development = 0.0f;

    /** Active exercise (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Exercise = 0.0f;
};

/**
 * Wisdom Dimension State
 */
USTRUCT(BlueprintType)
struct FWisdomDimensionState
{
    GENERATED_BODY()

    /** Dimension type */
    UPROPERTY(BlueprintReadWrite)
    EWisdomDimension Dimension = EWisdomDimension::Sophia;

    /** Current level (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Level = 0.0f;

    /** Growth rate */
    UPROPERTY(BlueprintReadWrite)
    float GrowthRate = 0.0f;

    /** Integration with other dimensions (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Integration = 0.0f;
};

/**
 * Relevance Realization State
 */
USTRUCT(BlueprintType)
struct FRelevanceRealizationState
{
    GENERATED_BODY()

    /** Overall relevance realization capacity (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Capacity = 0.5f;

    /** Current relevance accuracy (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Accuracy = 0.5f;

    /** Relevance efficiency (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Efficiency = 0.5f;

    /** Adaptability to context changes (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float Adaptability = 0.5f;
};

/**
 * Complete Wisdom State
 */
USTRUCT(BlueprintType)
struct FWisdomState
{
    GENERATED_BODY()

    /** Salience landscape */
    UPROPERTY(BlueprintReadWrite)
    FSalienceLandscape SalienceLandscape;

    /** Affordance landscape */
    UPROPERTY(BlueprintReadWrite)
    FAffordanceLandscape AffordanceLandscape;

    /** Opponent processes */
    UPROPERTY(BlueprintReadWrite)
    TArray<FOpponentProcessState> OpponentProcesses;

    /** Knowing states */
    UPROPERTY(BlueprintReadWrite)
    TArray<FKnowingState> KnowingStates;

    /** Cognitive virtues */
    UPROPERTY(BlueprintReadWrite)
    TArray<FCognitiveVirtueState> CognitiveVirtues;

    /** Wisdom dimensions */
    UPROPERTY(BlueprintReadWrite)
    TArray<FWisdomDimensionState> WisdomDimensions;

    /** Relevance realization */
    UPROPERTY(BlueprintReadWrite)
    FRelevanceRealizationState RelevanceRealization;

    /** Overall wisdom level (0-1) */
    UPROPERTY(BlueprintReadWrite)
    float OverallWisdom = 0.0f;
};

/**
 * Wisdom Cultivation Component
 * 
 * Implements relevance realization and wisdom cultivation for the
 * Deep-Tree-Echo cognitive framework.
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UWisdomCultivation : public UActorComponent
{
    GENERATED_BODY()

public:
    UWisdomCultivation();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable automatic wisdom processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wisdom|Config")
    bool bEnableAutoProcessing = true;

    /** Processing interval in seconds */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wisdom|Config", meta = (ClampMin = "0.01", ClampMax = "10.0"))
    float ProcessingInterval = 0.1f;

    /** Salience decay rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wisdom|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SalienceDecayRate = 0.05f;

    /** Opponent process oscillation speed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wisdom|Config", meta = (ClampMin = "0.1", ClampMax = "10.0"))
    float OpponentOscillationSpeed = 1.0f;

    // ========================================
    // STATE
    // ========================================

    /** Current wisdom state */
    UPROPERTY(BlueprintReadOnly, Category = "Wisdom|State")
    FWisdomState CurrentState;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /** Initialize the wisdom cultivation system */
    UFUNCTION(BlueprintCallable, Category = "Wisdom")
    void InitializeWisdom();

    /** Reset to initial state */
    UFUNCTION(BlueprintCallable, Category = "Wisdom")
    void ResetWisdom();

    // ========================================
    // PUBLIC API - SALIENCE
    // ========================================

    /** Add item to salience landscape */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Salience")
    void AddSalienceItem(const FString& Id, float InitialSalience, float Relevance);

    /** Update item salience */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Salience")
    void UpdateSalience(const FString& Id, float DeltaSalience);

    /** Get most salient item */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Salience")
    FSalienceItem GetMostSalientItem() const;

    /** Focus attention on item */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Salience")
    void FocusAttention(const FString& Id);

    /** Get salience landscape */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Salience")
    FSalienceLandscape GetSalienceLandscape() const;

    // ========================================
    // PUBLIC API - AFFORDANCES
    // ========================================

    /** Add affordance to landscape */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Affordance")
    void AddAffordance(const FString& Id, const FString& Action, float Availability, float ExpectedUtility);

    /** Update affordance availability */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Affordance")
    void UpdateAffordanceAvailability(const FString& Id, float Availability);

    /** Get best affordance */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Affordance")
    FAffordance GetBestAffordance() const;

    /** Select affordance for action */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Affordance")
    void SelectAffordance(const FString& Id);

    /** Get affordance landscape */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Affordance")
    FAffordanceLandscape GetAffordanceLandscape() const;

    // ========================================
    // PUBLIC API - OPPONENT PROCESSES
    // ========================================

    /** Get opponent process state */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Opponent")
    FOpponentProcessState GetOpponentProcessState(EOpponentProcess ProcessType) const;

    /** Set opponent process balance */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Opponent")
    void SetOpponentBalance(EOpponentProcess ProcessType, float Balance);

    /** Get exploration-exploitation balance */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Opponent")
    float GetExplorationExploitationBalance() const;

    // ========================================
    // PUBLIC API - KNOWING
    // ========================================

    /** Get knowing state */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Knowing")
    FKnowingState GetKnowingState(EKnowingType Type) const;

    /** Update knowing strength */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Knowing")
    void UpdateKnowingStrength(EKnowingType Type, float Strength);

    /** Integrate knowing types */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Knowing")
    void IntegrateKnowing();

    // ========================================
    // PUBLIC API - COGNITIVE VIRTUES
    // ========================================

    /** Get cognitive virtue state */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Virtue")
    FCognitiveVirtueState GetCognitiveVirtueState(ECognitiveVirtue Virtue) const;

    /** Exercise cognitive virtue */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Virtue")
    void ExerciseCognitiveVirtue(ECognitiveVirtue Virtue, float Intensity);

    /** Get overall virtue development */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Virtue")
    float GetOverallVirtueDevelopment() const;

    // ========================================
    // PUBLIC API - WISDOM DIMENSIONS
    // ========================================

    /** Get wisdom dimension state */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Dimension")
    FWisdomDimensionState GetWisdomDimensionState(EWisdomDimension Dimension) const;

    /** Cultivate wisdom dimension */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Dimension")
    void CultivateWisdomDimension(EWisdomDimension Dimension, float Growth);

    /** Get overall wisdom level */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Dimension")
    float GetOverallWisdomLevel() const;

    // ========================================
    // PUBLIC API - RELEVANCE REALIZATION
    // ========================================

    /** Realize relevance for current context */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Relevance")
    void RealizeRelevance();

    /** Get relevance realization state */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Relevance")
    FRelevanceRealizationState GetRelevanceRealizationState() const;

    /** Assess relevance accuracy */
    UFUNCTION(BlueprintCallable, Category = "Wisdom|Relevance")
    float AssessRelevanceAccuracy() const;

protected:
    virtual void BeginPlay() override;

private:
    /** Accumulated time for processing */
    float AccumulatedTime = 0.0f;

    /** Initialize opponent processes */
    void InitializeOpponentProcesses();

    /** Initialize knowing states */
    void InitializeKnowingStates();

    /** Initialize cognitive virtues */
    void InitializeCognitiveVirtues();

    /** Initialize wisdom dimensions */
    void InitializeWisdomDimensions();

    /** Process salience landscape */
    void ProcessSalienceLandscape(float DeltaTime);

    /** Process affordance landscape */
    void ProcessAffordanceLandscape(float DeltaTime);

    /** Process opponent dynamics */
    void ProcessOpponentDynamics(float DeltaTime);

    /** Update relevance realization */
    void UpdateRelevanceRealization();

    /** Calculate overall wisdom */
    void CalculateOverallWisdom();
};

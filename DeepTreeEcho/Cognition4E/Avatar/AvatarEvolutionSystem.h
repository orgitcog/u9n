#pragma once

/**
 * Avatar Evolution System
 * 
 * Implements relentless enhancement of avatar with 4E embodied cognition.
 * Manages the continuous evolution of avatar capabilities through:
 * - Morphological adaptation (body schema evolution)
 * - Sensorimotor learning (action-perception refinement)
 * - Environmental coupling (niche construction)
 * - Cognitive extension (tool integration)
 * 
 * Evolution follows ontogenetic principles with:
 * - Developmental stages (embryonic, juvenile, adult, transcendent)
 * - Capability maturation curves
 * - Adaptive plasticity windows
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AvatarEvolutionSystem.generated.h"

/**
 * Developmental Stage - Current stage of avatar evolution
 */
UENUM(BlueprintType)
enum class EDevelopmentalStage : uint8
{
    /** Initial formation - basic capabilities */
    Embryonic UMETA(DisplayName = "Embryonic"),
    
    /** Rapid learning - high plasticity */
    Juvenile UMETA(DisplayName = "Juvenile"),
    
    /** Stable operation - refined capabilities */
    Adult UMETA(DisplayName = "Adult"),
    
    /** Beyond normal limits - emergent capabilities */
    Transcendent UMETA(DisplayName = "Transcendent")
};

/**
 * Capability Domain - Area of avatar capability
 */
UENUM(BlueprintType)
enum class ECapabilityDomain : uint8
{
    /** Motor control and movement */
    Motor UMETA(DisplayName = "Motor"),
    
    /** Sensory processing */
    Sensory UMETA(DisplayName = "Sensory"),
    
    /** Cognitive processing */
    Cognitive UMETA(DisplayName = "Cognitive"),
    
    /** Social interaction */
    Social UMETA(DisplayName = "Social"),
    
    /** Emotional regulation */
    Emotional UMETA(DisplayName = "Emotional"),
    
    /** Creative expression */
    Creative UMETA(DisplayName = "Creative")
};

/**
 * Capability Level - Maturation level of a capability
 */
USTRUCT(BlueprintType)
struct FCapabilityLevel
{
    GENERATED_BODY()

    /** Capability domain */
    UPROPERTY(BlueprintReadWrite)
    ECapabilityDomain Domain = ECapabilityDomain::Motor;

    /** Current level (0.0 - 1.0) */
    UPROPERTY(BlueprintReadWrite)
    float Level = 0.0f;

    /** Maximum potential level */
    UPROPERTY(BlueprintReadWrite)
    float MaxPotential = 1.0f;

    /** Current plasticity (learning rate) */
    UPROPERTY(BlueprintReadWrite)
    float Plasticity = 0.5f;

    /** Experience accumulated */
    UPROPERTY(BlueprintReadWrite)
    float Experience = 0.0f;

    /** Last update timestamp */
    UPROPERTY(BlueprintReadWrite)
    float LastUpdateTime = 0.0f;
};

/**
 * Evolution Event - Record of an evolutionary change
 */
USTRUCT(BlueprintType)
struct FEvolutionEvent
{
    GENERATED_BODY()

    /** Event ID */
    UPROPERTY(BlueprintReadWrite)
    FString EventID;

    /** Timestamp */
    UPROPERTY(BlueprintReadWrite)
    FDateTime Timestamp;

    /** Event type */
    UPROPERTY(BlueprintReadWrite)
    FString EventType;

    /** Affected domain */
    UPROPERTY(BlueprintReadWrite)
    ECapabilityDomain AffectedDomain = ECapabilityDomain::Motor;

    /** Change magnitude */
    UPROPERTY(BlueprintReadWrite)
    float ChangeMagnitude = 0.0f;

    /** Trigger description */
    UPROPERTY(BlueprintReadWrite)
    FString Trigger;
};

/**
 * Morphological Trait - Physical characteristic that can evolve
 */
USTRUCT(BlueprintType)
struct FMorphologicalTrait
{
    GENERATED_BODY()

    /** Trait name */
    UPROPERTY(BlueprintReadWrite)
    FString TraitName;

    /** Current value */
    UPROPERTY(BlueprintReadWrite)
    float Value = 0.5f;

    /** Minimum value */
    UPROPERTY(BlueprintReadWrite)
    float MinValue = 0.0f;

    /** Maximum value */
    UPROPERTY(BlueprintReadWrite)
    float MaxValue = 1.0f;

    /** Adaptation rate */
    UPROPERTY(BlueprintReadWrite)
    float AdaptationRate = 0.1f;

    /** Linked capabilities */
    UPROPERTY(BlueprintReadWrite)
    TArray<ECapabilityDomain> LinkedCapabilities;
};

/**
 * Niche Construction Record - Environmental modification
 */
USTRUCT(BlueprintType)
struct FNicheConstructionRecord
{
    GENERATED_BODY()

    /** Record ID */
    UPROPERTY(BlueprintReadWrite)
    FString RecordID;

    /** Niche type */
    UPROPERTY(BlueprintReadWrite)
    FString NicheType;

    /** Construction actions taken */
    UPROPERTY(BlueprintReadWrite)
    TArray<FString> ConstructionActions;

    /** Fitness improvement */
    UPROPERTY(BlueprintReadWrite)
    float FitnessImprovement = 0.0f;

    /** Stability of construction */
    UPROPERTY(BlueprintReadWrite)
    float Stability = 0.5f;
};

/**
 * Avatar Evolution System Component
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UAvatarEvolutionSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UAvatarEvolutionSystem();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Enable continuous evolution */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution|Config")
    bool bEnableEvolution = true;

    /** Base evolution rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BaseEvolutionRate = 0.01f;

    /** Plasticity decay rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution|Config", meta = (ClampMin = "0.0", ClampMax = "0.1"))
    float PlasticityDecayRate = 0.001f;

    /** Enable morphological adaptation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution|Config")
    bool bEnableMorphologicalAdaptation = true;

    /** Enable niche construction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution|Config")
    bool bEnableNicheConstruction = true;

    // ========================================
    // EVOLUTION STATE
    // ========================================

    /** Current developmental stage */
    UPROPERTY(BlueprintReadOnly, Category = "Evolution|State")
    EDevelopmentalStage CurrentStage = EDevelopmentalStage::Embryonic;

    /** Capability levels */
    UPROPERTY(BlueprintReadOnly, Category = "Evolution|State")
    TMap<ECapabilityDomain, FCapabilityLevel> CapabilityLevels;

    /** Morphological traits */
    UPROPERTY(BlueprintReadOnly, Category = "Evolution|State")
    TArray<FMorphologicalTrait> MorphologicalTraits;

    /** Evolution history */
    UPROPERTY(BlueprintReadOnly, Category = "Evolution|State")
    TArray<FEvolutionEvent> EvolutionHistory;

    /** Niche construction records */
    UPROPERTY(BlueprintReadOnly, Category = "Evolution|State")
    TArray<FNicheConstructionRecord> NicheConstructions;

    /** Overall fitness score */
    UPROPERTY(BlueprintReadOnly, Category = "Evolution|State")
    float OverallFitness = 0.0f;

    /** Total evolution time (seconds) */
    UPROPERTY(BlueprintReadOnly, Category = "Evolution|State")
    float TotalEvolutionTime = 0.0f;

    // ========================================
    // PUBLIC API - EVOLUTION CONTROL
    // ========================================

    /** Initialize evolution system */
    UFUNCTION(BlueprintCallable, Category = "Evolution")
    void InitializeEvolution();

    /** Trigger evolution step */
    UFUNCTION(BlueprintCallable, Category = "Evolution")
    void TriggerEvolutionStep(float DeltaTime);

    /** Force stage transition */
    UFUNCTION(BlueprintCallable, Category = "Evolution")
    void ForceStageTransition(EDevelopmentalStage NewStage);

    /** Get current stage */
    UFUNCTION(BlueprintCallable, Category = "Evolution")
    EDevelopmentalStage GetCurrentStage() const { return CurrentStage; }

    // ========================================
    // PUBLIC API - CAPABILITY MANAGEMENT
    // ========================================

    /** Get capability level */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Capability")
    float GetCapabilityLevel(ECapabilityDomain Domain) const;

    /** Add experience to capability */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Capability")
    void AddCapabilityExperience(ECapabilityDomain Domain, float Experience);

    /** Get capability plasticity */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Capability")
    float GetCapabilityPlasticity(ECapabilityDomain Domain) const;

    /** Boost plasticity (learning opportunity) */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Capability")
    void BoostPlasticity(ECapabilityDomain Domain, float Amount);

    // ========================================
    // PUBLIC API - MORPHOLOGICAL ADAPTATION
    // ========================================

    /** Add morphological trait */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Morphology")
    void AddMorphologicalTrait(const FString& TraitName, float InitialValue, const TArray<ECapabilityDomain>& LinkedCapabilities);

    /** Adapt trait based on usage */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Morphology")
    void AdaptTrait(const FString& TraitName, float UsageIntensity);

    /** Get trait value */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Morphology")
    float GetTraitValue(const FString& TraitName) const;

    // ========================================
    // PUBLIC API - NICHE CONSTRUCTION
    // ========================================

    /** Record niche construction action */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Niche")
    void RecordNicheConstruction(const FString& NicheType, const FString& Action, float FitnessImpact);

    /** Get niche stability */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Niche")
    float GetNicheStability(const FString& NicheType) const;

    // ========================================
    // PUBLIC API - FITNESS EVALUATION
    // ========================================

    /** Evaluate overall fitness */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Fitness")
    float EvaluateFitness() const;

    /** Get fitness for domain */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Fitness")
    float GetDomainFitness(ECapabilityDomain Domain) const;

    // ========================================
    // PUBLIC API - EVOLUTION HISTORY
    // ========================================

    /** Get evolution events for domain */
    UFUNCTION(BlueprintCallable, Category = "Evolution|History")
    TArray<FEvolutionEvent> GetEvolutionEventsForDomain(ECapabilityDomain Domain) const;

    /** Get recent evolution events */
    UFUNCTION(BlueprintCallable, Category = "Evolution|History")
    TArray<FEvolutionEvent> GetRecentEvolutionEvents(int32 Count) const;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Event ID counter */
    int32 EventIDCounter = 0;

    /** Record ID counter */
    int32 RecordIDCounter = 0;

    /** Stage transition thresholds */
    TMap<EDevelopmentalStage, float> StageThresholds;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Initialize capability levels */
    void InitializeCapabilities();

    /** Initialize default morphological traits */
    void InitializeDefaultTraits();

    /** Update capabilities based on experience */
    void UpdateCapabilities(float DeltaTime);

    /** Update morphological traits */
    void UpdateMorphology(float DeltaTime);

    /** Check and perform stage transitions */
    void CheckStageTransition();

    /** Decay plasticity over time */
    void DecayPlasticity(float DeltaTime);

    /** Record evolution event */
    void RecordEvolutionEvent(const FString& EventType, ECapabilityDomain Domain, float Magnitude, const FString& Trigger);

    /** Generate event ID */
    FString GenerateEventID();

    /** Generate record ID */
    FString GenerateRecordID();

    /** Compute stage threshold */
    float ComputeStageThreshold(EDevelopmentalStage Stage) const;

    // ========================================
    // 4E EMBODIED COGNITION INTEGRATION
    // ========================================

public:
    /** Get embodied score from motor and sensory capabilities */
    UFUNCTION(BlueprintCallable, Category = "Evolution|4E")
    float GetEmbodiedScore() const;

    /** Get embedded score from environmental coupling */
    UFUNCTION(BlueprintCallable, Category = "Evolution|4E")
    float GetEmbeddedScore() const;

    /** Get enacted score from action-perception integration */
    UFUNCTION(BlueprintCallable, Category = "Evolution|4E")
    float GetEnactedScore() const;

    /** Get extended score from tool use and social cognition */
    UFUNCTION(BlueprintCallable, Category = "Evolution|4E")
    float GetExtendedScore() const;

    /** Get overall 4E integration score */
    UFUNCTION(BlueprintCallable, Category = "Evolution|4E")
    float Get4EIntegrationScore() const;

    /** Update 4E scores from capability levels */
    UFUNCTION(BlueprintCallable, Category = "Evolution|4E")
    void Update4EScores();

    // ========================================
    // WISDOM CULTIVATION
    // ========================================

    /** Current wisdom level (0-1) */
    UPROPERTY(BlueprintReadOnly, Category = "Evolution|Wisdom")
    float WisdomLevel = 0.0f;

    /** Wisdom cultivation rate */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Evolution|Wisdom")
    float WisdomCultivationRate = 0.001f;

    /** Integrate wisdom from insights */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Wisdom")
    void IntegrateWisdom(float InsightValue);

    /** Get wisdom-adjusted capability level */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Wisdom")
    float GetWisdomAdjustedCapability(ECapabilityDomain Domain) const;

    /** Apply wisdom to all capabilities */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Wisdom")
    void ApplyWisdomToCapabilities();

    // ========================================
    // COGNITIVE CYCLE INTEGRATION
    // ========================================

    /** Current cognitive cycle step (0-11) */
    UPROPERTY(BlueprintReadOnly, Category = "Evolution|Cognitive")
    int32 CurrentCognitiveStep = 0;

    /** Set cognitive step for evolution modulation */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Cognitive")
    void SetCognitiveStep(int32 Step);

    /** Get evolution rate modifier for current cognitive step */
    UFUNCTION(BlueprintCallable, Category = "Evolution|Cognitive")
    float GetCognitiveStepEvolutionModifier() const;

    // ========================================
    // EVENTS
    // ========================================

    /** Called when stage transitions */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStageTransition, EDevelopmentalStage, OldStage, EDevelopmentalStage, NewStage);
    UPROPERTY(BlueprintAssignable, Category = "Evolution|Events")
    FOnStageTransition OnStageTransition;

    /** Called when capability level changes significantly */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCapabilityLevelChanged, ECapabilityDomain, Domain, float, NewLevel);
    UPROPERTY(BlueprintAssignable, Category = "Evolution|Events")
    FOnCapabilityLevelChanged OnCapabilityLevelChanged;

    /** Called when wisdom level changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWisdomLevelChanged, float, NewWisdomLevel);
    UPROPERTY(BlueprintAssignable, Category = "Evolution|Events")
    FOnWisdomLevelChanged OnWisdomLevelChanged;

    /** Called when 4E integration score changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOn4EScoreChanged, float, NewScore);
    UPROPERTY(BlueprintAssignable, Category = "Evolution|Events")
    FOn4EScoreChanged On4EScoreChanged;

private:
    /** Previous 4E score for change detection */
    float Previous4EScore = 0.0f;

    /** Previous wisdom level for change detection */
    float PreviousWisdomLevel = 0.0f;

    /** 4E dimension scores */
    float EmbodiedScore = 0.0f;
    float EmbeddedScore = 0.0f;
    float EnactedScore = 0.0f;
    float ExtendedScore = 0.0f;
};

// RelevanceRealizationEnnead.h
// Implementation of the Triad-of-Triads framework for relevance realization
// Integrates ways of knowing, orders of understanding, and practices of wisdom

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RelevanceRealizationEnnead.generated.h"

// Forward declarations
class UCognitiveCycleManager;
class UWisdomCultivation;
class USensorimotorIntegration;

/**
 * Ways of Knowing (Epistemological Triad)
 */
UENUM(BlueprintType)
enum class EWayOfKnowing : uint8
{
    Propositional   UMETA(DisplayName = "Propositional (Knowing-That)"),
    Procedural      UMETA(DisplayName = "Procedural (Knowing-How)"),
    Perspectival    UMETA(DisplayName = "Perspectival (Knowing-As)"),
    Participatory   UMETA(DisplayName = "Participatory (Knowing-By-Being)")
};

/**
 * Orders of Understanding (Ontological Triad)
 */
UENUM(BlueprintType)
enum class EOrderOfUnderstanding : uint8
{
    Nomological     UMETA(DisplayName = "Nomological (How Things Work)"),
    Normative       UMETA(DisplayName = "Normative (What Matters)"),
    Narrative       UMETA(DisplayName = "Narrative (How Things Develop)")
};

/**
 * Practices of Wisdom (Axiological Triad)
 */
UENUM(BlueprintType)
enum class EPracticeOfWisdom : uint8
{
    Morality        UMETA(DisplayName = "Morality (Virtue & Ethics)"),
    Meaning         UMETA(DisplayName = "Meaning (Coherence & Purpose)"),
    Mastery         UMETA(DisplayName = "Mastery (Excellence & Flow)")
};

/**
 * Gnostic spiral stage
 */
UENUM(BlueprintType)
enum class EGnosticStage : uint8
{
    PropositionalOpening    UMETA(DisplayName = "Propositional Opening"),
    ProceduralDevelopment   UMETA(DisplayName = "Procedural Development"),
    PerspectivalShift       UMETA(DisplayName = "Perspectival Shift"),
    ParticipatoryTransform  UMETA(DisplayName = "Participatory Transformation"),
    NewUnderstanding        UMETA(DisplayName = "New Understanding Emergence"),
    AnagogicAscent          UMETA(DisplayName = "Anagogic Ascent")
};

/**
 * State of a way of knowing
 */
USTRUCT(BlueprintType)
struct FKnowingState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EWayOfKnowing Type = EWayOfKnowing::Propositional;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ActivationLevel = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float IntegrationLevel = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Coherence = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> ActiveContents;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString CurrentFocus;
};

/**
 * State of an order of understanding
 */
USTRUCT(BlueprintType)
struct FOrderState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EOrderOfUnderstanding Type = EOrderOfUnderstanding::Nomological;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ActivationLevel = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Clarity = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float IntegrationWithOthers = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> CurrentUnderstandings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PrimaryQuestion;
};

/**
 * State of a practice of wisdom
 */
USTRUCT(BlueprintType)
struct FPracticeState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EPracticeOfWisdom Type = EPracticeOfWisdom::Morality;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CultivationLevel = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ActiveEngagement = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float IntegrationWithOthers = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> CurrentPractices;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PrimaryGoal;
};

/**
 * Gnostic spiral state
 */
USTRUCT(BlueprintType)
struct FGnosticSpiralState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGnosticStage CurrentStage = EGnosticStage::PropositionalOpening;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SpiralLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float StageProgress = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TransformativeIntensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> InsightsGained;
};

/**
 * Complete ennead state
 */
USTRUCT(BlueprintType)
struct FEnneadState
{
    GENERATED_BODY()

    // Triad I: Ways of Knowing (Epistemological)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FKnowingState> WaysOfKnowing;

    // Triad II: Orders of Understanding (Ontological)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FOrderState> OrdersOfUnderstanding;

    // Triad III: Practices of Wisdom (Axiological)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FPracticeState> PracticesOfWisdom;

    // Gnostic spiral
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGnosticSpiralState GnosticSpiral;

    // Overall integration
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OverallIntegration = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RelevanceRealizationLevel = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WisdomLevel = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MeaningLevel = 0.0f;
};

/**
 * Interpenetration relationship between triads
 */
USTRUCT(BlueprintType)
struct FTriadInterpenetration
{
    GENERATED_BODY()

    // Knowing -> Order
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float KnowingToOrderFlow = 0.5f;

    // Knowing -> Wisdom
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float KnowingToWisdomFlow = 0.5f;

    // Order -> Knowing
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OrderToKnowingFlow = 0.5f;

    // Order -> Wisdom
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OrderToWisdomFlow = 0.5f;

    // Wisdom -> Knowing
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WisdomToKnowingFlow = 0.5f;

    // Wisdom -> Order
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float WisdomToOrderFlow = 0.5f;

    // Overall coherence
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OverallCoherence = 0.5f;
};

/**
 * Delegate declarations
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnKnowingActivated, EWayOfKnowing, Way, float, Level);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOrderActivated, EOrderOfUnderstanding, Order, float, Level);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPracticeActivated, EPracticeOfWisdom, Practice, float, Level);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGnosticStageAdvanced, EGnosticStage, NewStage, int32, SpiralLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRelevanceRealized, float, Level);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWisdomCultivated, float, Level);

/**
 * Relevance Realization Ennead Component
 * Implements the triad-of-triads framework for optimal meaning-making
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API URelevanceRealizationEnnead : public UActorComponent
{
    GENERATED_BODY()

public:
    URelevanceRealizationEnnead();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    bool bEnableGnosticSpiral = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    bool bEnableTriadInterpenetration = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float IntegrationRate = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    float GnosticAdvancementThreshold = 0.8f;

    // ========================================
    // EVENTS
    // ========================================

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnKnowingActivated OnKnowingActivated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnOrderActivated OnOrderActivated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnPracticeActivated OnPracticeActivated;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnGnosticStageAdvanced OnGnosticStageAdvanced;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnRelevanceRealized OnRelevanceRealized;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnWisdomCultivated OnWisdomCultivated;

    // ========================================
    // WAYS OF KNOWING (TRIAD I)
    // ========================================

    /** Activate a way of knowing */
    UFUNCTION(BlueprintCallable, Category = "Ways of Knowing")
    void ActivateKnowing(EWayOfKnowing Way, float Intensity);

    /** Get state of a way of knowing */
    UFUNCTION(BlueprintPure, Category = "Ways of Knowing")
    FKnowingState GetKnowingState(EWayOfKnowing Way) const;

    /** Add content to a way of knowing */
    UFUNCTION(BlueprintCallable, Category = "Ways of Knowing")
    void AddKnowingContent(EWayOfKnowing Way, const FString& Content);

    /** Set focus for a way of knowing */
    UFUNCTION(BlueprintCallable, Category = "Ways of Knowing")
    void SetKnowingFocus(EWayOfKnowing Way, const FString& Focus);

    /** Get the dominant way of knowing */
    UFUNCTION(BlueprintPure, Category = "Ways of Knowing")
    EWayOfKnowing GetDominantKnowing() const;

    /** Get integration level between knowings */
    UFUNCTION(BlueprintPure, Category = "Ways of Knowing")
    float GetKnowingIntegration() const;

    // ========================================
    // ORDERS OF UNDERSTANDING (TRIAD II)
    // ========================================

    /** Activate an order of understanding */
    UFUNCTION(BlueprintCallable, Category = "Orders of Understanding")
    void ActivateOrder(EOrderOfUnderstanding Order, float Intensity);

    /** Get state of an order of understanding */
    UFUNCTION(BlueprintPure, Category = "Orders of Understanding")
    FOrderState GetOrderState(EOrderOfUnderstanding Order) const;

    /** Add understanding to an order */
    UFUNCTION(BlueprintCallable, Category = "Orders of Understanding")
    void AddOrderUnderstanding(EOrderOfUnderstanding Order, const FString& Understanding);

    /** Set primary question for an order */
    UFUNCTION(BlueprintCallable, Category = "Orders of Understanding")
    void SetOrderQuestion(EOrderOfUnderstanding Order, const FString& Question);

    /** Get the dominant order of understanding */
    UFUNCTION(BlueprintPure, Category = "Orders of Understanding")
    EOrderOfUnderstanding GetDominantOrder() const;

    /** Get integration level between orders */
    UFUNCTION(BlueprintPure, Category = "Orders of Understanding")
    float GetOrderIntegration() const;

    // ========================================
    // PRACTICES OF WISDOM (TRIAD III)
    // ========================================

    /** Activate a practice of wisdom */
    UFUNCTION(BlueprintCallable, Category = "Practices of Wisdom")
    void ActivatePractice(EPracticeOfWisdom Practice, float Intensity);

    /** Get state of a practice of wisdom */
    UFUNCTION(BlueprintPure, Category = "Practices of Wisdom")
    FPracticeState GetPracticeState(EPracticeOfWisdom Practice) const;

    /** Add practice to a wisdom dimension */
    UFUNCTION(BlueprintCallable, Category = "Practices of Wisdom")
    void AddPractice(EPracticeOfWisdom Practice, const FString& PracticeName);

    /** Set primary goal for a practice */
    UFUNCTION(BlueprintCallable, Category = "Practices of Wisdom")
    void SetPracticeGoal(EPracticeOfWisdom Practice, const FString& Goal);

    /** Get the dominant practice of wisdom */
    UFUNCTION(BlueprintPure, Category = "Practices of Wisdom")
    EPracticeOfWisdom GetDominantPractice() const;

    /** Get integration level between practices (Three M's) */
    UFUNCTION(BlueprintPure, Category = "Practices of Wisdom")
    float GetPracticeIntegration() const;

    // ========================================
    // GNOSTIC SPIRAL
    // ========================================

    /** Get current gnostic spiral state */
    UFUNCTION(BlueprintPure, Category = "Gnostic Spiral")
    FGnosticSpiralState GetGnosticState() const;

    /** Advance gnostic spiral */
    UFUNCTION(BlueprintCallable, Category = "Gnostic Spiral")
    void AdvanceGnosticSpiral();

    /** Add insight to gnostic spiral */
    UFUNCTION(BlueprintCallable, Category = "Gnostic Spiral")
    void AddGnosticInsight(const FString& Insight);

    /** Get current gnostic stage */
    UFUNCTION(BlueprintPure, Category = "Gnostic Spiral")
    EGnosticStage GetCurrentGnosticStage() const;

    /** Get spiral level (anagogic ascent) */
    UFUNCTION(BlueprintPure, Category = "Gnostic Spiral")
    int32 GetSpiralLevel() const;

    // ========================================
    // ENNEAD INTEGRATION
    // ========================================

    /** Get complete ennead state */
    UFUNCTION(BlueprintPure, Category = "Ennead")
    FEnneadState GetEnneadState() const;

    /** Get triad interpenetration state */
    UFUNCTION(BlueprintPure, Category = "Ennead")
    FTriadInterpenetration GetInterpenetration() const;

    /** Get overall relevance realization level */
    UFUNCTION(BlueprintPure, Category = "Ennead")
    float GetRelevanceRealizationLevel() const;

    /** Get overall wisdom level */
    UFUNCTION(BlueprintPure, Category = "Ennead")
    float GetWisdomLevel() const;

    /** Get overall meaning level */
    UFUNCTION(BlueprintPure, Category = "Ennead")
    float GetMeaningLevel() const;

    /** Force integration of all triads */
    UFUNCTION(BlueprintCallable, Category = "Ennead")
    void ForceIntegration();

    // ========================================
    // COGNITIVE INTEGRATION
    // ========================================

    /** Process cognitive input through ennead */
    UFUNCTION(BlueprintCallable, Category = "Processing")
    void ProcessCognitiveInput(const TArray<float>& Input, EWayOfKnowing PrimaryKnowing);

    /** Get relevance score for content */
    UFUNCTION(BlueprintPure, Category = "Processing")
    float GetRelevanceScore(const FString& Content) const;

    /** Determine optimal knowing mode for context */
    UFUNCTION(BlueprintPure, Category = "Processing")
    EWayOfKnowing DetermineOptimalKnowing(const FString& Context) const;

protected:
    // Component references
    UPROPERTY()
    UCognitiveCycleManager* CycleManager;

    UPROPERTY()
    UWisdomCultivation* WisdomComponent;

    UPROPERTY()
    USensorimotorIntegration* SensorimotorComponent;

    // Internal state
    FEnneadState EnneadState;
    FTriadInterpenetration Interpenetration;

    float UpdateTimer = 0.0f;

    // Internal methods
    void FindComponentReferences();
    void InitializeEnnead();

    void UpdateWaysOfKnowing(float DeltaTime);
    void UpdateOrdersOfUnderstanding(float DeltaTime);
    void UpdatePracticesOfWisdom(float DeltaTime);
    void UpdateGnosticSpiral(float DeltaTime);
    void UpdateInterpenetration(float DeltaTime);

    void ProcessKnowingToOrderFlow();
    void ProcessKnowingToWisdomFlow();
    void ProcessOrderToKnowingFlow();
    void ProcessOrderToWisdomFlow();
    void ProcessWisdomToKnowingFlow();
    void ProcessWisdomToOrderFlow();

    float ComputeKnowingIntegration() const;
    float ComputeOrderIntegration() const;
    float ComputePracticeIntegration() const;
    float ComputeOverallIntegration() const;
    float ComputeRelevanceRealization() const;
    float ComputeWisdomLevel() const;
    float ComputeMeaningLevel() const;

    int32 GetKnowingIndex(EWayOfKnowing Way) const;
    int32 GetOrderIndex(EOrderOfUnderstanding Order) const;
    int32 GetPracticeIndex(EPracticeOfWisdom Practice) const;
};

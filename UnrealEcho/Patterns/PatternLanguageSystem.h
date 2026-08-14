#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PatternLanguageSystem.generated.h"

// Forward declarations
class UAvatarAtomSpaceClient;
class UPersonalityTraitSystem;
class UNeurochemicalSimulationComponent;

/**
 * Christopher Alexander's 15 Morphological Properties
 */
UENUM(BlueprintType)
enum class EMorphologicalProperty : uint8
{
    LevelsOfScale UMETA(DisplayName = "Levels of Scale"),
    StrongCenters UMETA(DisplayName = "Strong Centers"),
    Boundaries UMETA(DisplayName = "Boundaries"),
    AlternatingRepetition UMETA(DisplayName = "Alternating Repetition"),
    PositiveSpace UMETA(DisplayName = "Positive Space"),
    GoodShape UMETA(DisplayName = "Good Shape"),
    LocalSymmetries UMETA(DisplayName = "Local Symmetries"),
    DeepInterlock UMETA(DisplayName = "Deep Interlock and Ambiguity"),
    Contrast UMETA(DisplayName = "Contrast"),
    Gradients UMETA(DisplayName = "Gradients"),
    Roughness UMETA(DisplayName = "Roughness"),
    Echoes UMETA(DisplayName = "Echoes"),
    TheVoid UMETA(DisplayName = "The Void"),
    Simplicity UMETA(DisplayName = "Simplicity and Inner Calm"),
    NotSeparateness UMETA(DisplayName = "Not-Separateness")
};

/**
 * Pattern Category Types
 */
UENUM(BlueprintType)
enum class EPatternCategory : uint8
{
    Behavioral UMETA(DisplayName = "Behavioral Pattern"),
    Cognitive UMETA(DisplayName = "Cognitive Pattern"),
    Emotional UMETA(DisplayName = "Emotional Pattern"),
    Social UMETA(DisplayName = "Social Pattern"),
    Temporal UMETA(DisplayName = "Temporal Pattern"),
    Spatial UMETA(DisplayName = "Spatial Pattern"),
    Communicative UMETA(DisplayName = "Communicative Pattern"),
    Developmental UMETA(DisplayName = "Developmental Pattern")
};

/**
 * Pattern structure following Alexander's format
 */
USTRUCT(BlueprintType)
struct FPattern
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString PatternId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PatternNumber; // Following Alexander's numbering

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EPatternCategory Category;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Context; // When to apply

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Problem; // What conflict it resolves

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Solution; // The pattern's core instruction

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Forces; // Competing requirements

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> RelatedPatterns; // Pattern IDs

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<EMorphologicalProperty> ImplementsProperties;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Confidence; // 0-1, how well established

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ApplicationCount;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float SuccessRate;

    FPattern()
        : PatternNumber(0)
        , Category(EPatternCategory::Behavioral)
        , Confidence(0.5f)
        , ApplicationCount(0)
        , SuccessRate(0.5f)
    {}
};

/**
 * Quality metric for living structure assessment
 */
USTRUCT(BlueprintType)
struct FQualityMetric
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EMorphologicalProperty Property;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Score; // 0-1

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Evidence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime MeasuredAt;

    FQualityMetric()
        : Property(EMorphologicalProperty::StrongCenters)
        , Score(0.5f)
        , MeasuredAt(FDateTime::Now())
    {}
};

/**
 * Living Structure Assessment
 */
USTRUCT(BlueprintType)
struct FLivingStructureAssessment
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AssessmentId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FQualityMetric> PropertyScores;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float OverallQuality; // "Quality Without a Name" score

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Wholeness;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Aliveness;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> Recommendations;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime AssessedAt;

    FLivingStructureAssessment()
        : OverallQuality(0.5f)
        , Wholeness(0.5f)
        , Aliveness(0.5f)
        , AssessedAt(FDateTime::Now())
    {}
};

/**
 * Behavior History Entry for pattern recognition
 */
USTRUCT(BlueprintType)
struct FBehaviorHistoryEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString EntryId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString BehaviorType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Context;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Outcome;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EmotionalValence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Success;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FDateTime Timestamp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, FString> Metadata;

    FBehaviorHistoryEntry()
        : EmotionalValence(0.0f)
        , Success(0.5f)
        , Timestamp(FDateTime::Now())
    {}
};

/**
 * Delegates for pattern events
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPatternRecognized, const FPattern&, Pattern);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPatternApplied, const FString&, PatternId, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQualityAssessed, const FLivingStructureAssessment&, Assessment);

/**
 * Pattern Language System Component
 * Implements Christopher Alexander's pattern language principles for avatar behavior
 *
 * Core Concepts:
 * - Patterns as recurring solutions to behavioral/cognitive/emotional problems
 * - 15 morphological properties for assessing "living structure"
 * - Quality Without a Name (QWAN) as the measure of wholeness and aliveness
 * - Pattern mining from behavior history
 * - Pattern application to improve avatar quality
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API UPatternLanguageSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UPatternLanguageSystem();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    // ===== Pattern Catalog =====

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Catalog")
    void InitializePatternCatalog();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Catalog")
    void AddPattern(const FPattern& Pattern);

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Catalog")
    FPattern GetPattern(const FString& PatternId);

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Catalog")
    TArray<FPattern> GetPatternsByCategory(EPatternCategory Category);

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Catalog")
    TArray<FPattern> GetPatternsImplementingProperty(EMorphologicalProperty Property);

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Catalog")
    TArray<FPattern> GetRelatedPatterns(const FString& PatternId);

    // ===== Pattern Recognition =====

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Recognition")
    TArray<FPattern> RecognizePatterns(const TArray<FBehaviorHistoryEntry>& History);

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Recognition")
    void RecordBehavior(const FBehaviorHistoryEntry& Entry);

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Recognition")
    TArray<FBehaviorHistoryEntry> GetBehaviorHistory(int32 Count = 100);

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Recognition")
    void MineNewPatterns(int32 MinSupport = 3);

    // ===== Pattern Application =====

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Application")
    void ApplyPattern(const FPattern& Pattern);

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Application")
    FPattern SuggestPattern(const FString& CurrentContext);

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Application")
    void UpdatePatternSuccess(const FString& PatternId, bool bSuccess);

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Application")
    TArray<FPattern> GetApplicablePatterns(const FString& Context);

    // ===== Morphological Properties =====

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Morphology")
    float CalculateLevelsOfScale();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Morphology")
    float CalculateStrongCenters();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Morphology")
    float CalculateBoundaries();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Morphology")
    float CalculateAlternatingRepetition();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Morphology")
    float CalculatePositiveSpace();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Morphology")
    float CalculateGoodShape();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Morphology")
    float CalculateLocalSymmetries();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Morphology")
    float CalculateDeepInterlock();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Morphology")
    float CalculateContrast();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Morphology")
    float CalculateGradients();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Morphology")
    float CalculateRoughness();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Morphology")
    float CalculateEchoes();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Morphology")
    float CalculateTheVoid();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Morphology")
    float CalculateSimplicity();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Morphology")
    float CalculateNotSeparateness();

    // ===== Living Structure Assessment =====

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Quality")
    FLivingStructureAssessment AssessLivingStructure();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Quality")
    float ValidateQualityWithoutAName();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Quality")
    float CalculateWholeness();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Quality")
    float CalculateAliveness();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Quality")
    TArray<FString> GenerateImprovementRecommendations();

    // ===== Pattern-Guided Development =====

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Development")
    void EnablePatternGuidedDevelopment(bool bEnable);

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Development")
    FPattern GetNextDevelopmentalPattern();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|Development")
    void TrackDevelopmentalProgress();

    // ===== AtomSpace Integration =====

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|AtomSpace")
    void StorePatternsInAtomSpace();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|AtomSpace")
    void LoadPatternsFromAtomSpace();

    UFUNCTION(BlueprintCallable, Category = "PatternLanguage|AtomSpace")
    void SharePatternWithAgents(const FPattern& Pattern);

    // ===== Events =====

    UPROPERTY(BlueprintAssignable, Category = "PatternLanguage|Events")
    FOnPatternRecognized OnPatternRecognized;

    UPROPERTY(BlueprintAssignable, Category = "PatternLanguage|Events")
    FOnPatternApplied OnPatternApplied;

    UPROPERTY(BlueprintAssignable, Category = "PatternLanguage|Events")
    FOnQualityAssessed OnQualityAssessed;

protected:
    // ===== Internal Methods =====

    void InitializeCorePatterns();
    float CalculateMorphologicalProperty(EMorphologicalProperty Property);
    TArray<FString> FindFrequentSequences(const TArray<FBehaviorHistoryEntry>& History, int32 MinSupport);

private:
    // ===== Component References =====

    UPROPERTY()
    UAvatarAtomSpaceClient* AtomSpaceClient;

    UPROPERTY()
    UPersonalityTraitSystem* PersonalitySystem;

    UPROPERTY()
    UNeurochemicalSimulationComponent* NeurochemicalSystem;

    // ===== Pattern Catalog =====

    UPROPERTY()
    TMap<FString, FPattern> PatternCatalog;

    // ===== Behavior History =====

    UPROPERTY()
    TArray<FBehaviorHistoryEntry> BehaviorHistory;

    // ===== Assessment Cache =====

    UPROPERTY()
    FLivingStructureAssessment LastAssessment;

    UPROPERTY()
    float AssessmentCacheTime;

    // ===== Configuration =====

    UPROPERTY(EditAnywhere, Category = "PatternLanguage|Configuration")
    int32 MaxHistorySize;

    UPROPERTY(EditAnywhere, Category = "PatternLanguage|Configuration")
    float AssessmentInterval;

    UPROPERTY(EditAnywhere, Category = "PatternLanguage|Configuration")
    bool bEnablePatternGuidedDevelopment;

    UPROPERTY(EditAnywhere, Category = "PatternLanguage|Configuration")
    bool bVerboseLogging;

    // ===== Timers =====

    float AssessmentTimer;
    float PatternMiningTimer;
};

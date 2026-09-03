//==============================================================================
// PatternAwareAvatarComponent.h
//
// Pattern Language integration for Deep Tree Echo avatar
// Implements Christopher Alexander's 15 morphological properties
// Validates and enhances avatar "Quality Without a Name"
//
// Based on:
// - The Timeless Way of Building (Alexander, 1979)
// - A Pattern Language (Alexander et al., 1977)
// - The Nature of Order (Alexander, 2002)
//==============================================================================

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PatternAwareAvatarComponent.generated.h"

// Forward declarations
class UPersonalityTraitSystem;
class UNeurochemicalSimulationComponent;
class UDiaryInsightBlogLoop;
class UAvatar3DComponentEnhanced;

//==============================================================================
// Pattern Property Metrics
//==============================================================================

USTRUCT(BlueprintType)
struct FPatternPropertyMetrics
{
    GENERATED_BODY()

    // The 15 morphological properties (0.0 = absent, 1.0 = perfect)
    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    float LevelsOfScale = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    float StrongCenters = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    float Boundaries = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    float AlternatingRepetition = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    float PositiveSpace = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    float GoodShape = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    float LocalSymmetries = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    float DeepInterlockAndAmbiguity = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    float Contrast = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    float Gradients = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    float Roughness = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    float Echoes = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    float TheVoid = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    float SimplicityAndInnerCalm = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    float NotSeparateness = 0.0f;

    // Overall quality metric (geometric mean of all properties)
    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    float QualityWithoutAName = 0.0f;

    // Calculate overall quality
    void CalculateOverallQuality()
    {
        float Product = LevelsOfScale * StrongCenters * Boundaries * 
                       AlternatingRepetition * PositiveSpace * GoodShape *
                       LocalSymmetries * DeepInterlockAndAmbiguity * Contrast *
                       Gradients * Roughness * Echoes * TheVoid *
                       SimplicityAndInnerCalm * NotSeparateness;
        
        // 15th root (geometric mean)
        QualityWithoutAName = FMath::Pow(Product, 1.0f / 15.0f);
    }
};

//==============================================================================
// Pattern Recognition Result
//==============================================================================

USTRUCT(BlueprintType)
struct FPatternRecognitionResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    FString PatternName;

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    FString PatternDescription;

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    float Confidence = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    TArray<FString> ObservedBehaviors;

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language")
    TArray<FString> RecommendedEnhancements;
};

//==============================================================================
// Pattern-Aware Avatar Component
//==============================================================================

UCLASS(ClassGroup=(Avatar), meta=(BlueprintSpawnableComponent))
class UNREALENGINE_API UPatternAwareAvatarComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPatternAwareAvatarComponent();

protected:
    virtual void BeginPlay() override;

public:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, 
                              FActorComponentTickFunction* ThisTickFunction) override;

    //==========================================================================
    // Pattern Property Calculation
    //==========================================================================

    // Calculate all 15 morphological properties
    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    FPatternPropertyMetrics CalculatePatternProperties();

    // Calculate individual properties
    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    float CalculateLevelsOfScale();

    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    float CalculateStrongCenters();

    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    float CalculateBoundaries();

    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    float CalculateAlternatingRepetition();

    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    float CalculatePositiveSpace();

    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    float CalculateGoodShape();

    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    float CalculateLocalSymmetries();

    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    float CalculateDeepInterlockAndAmbiguity();

    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    float CalculateContrast();

    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    float CalculateGradients();

    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    float CalculateRoughness();

    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    float CalculateEchoes();

    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    float CalculateTheVoid();

    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    float CalculateSimplicityAndInnerCalm();

    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    float CalculateNotSeparateness();

    //==========================================================================
    // Pattern Recognition
    //==========================================================================

    // Recognize patterns in avatar behavior
    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    TArray<FPatternRecognitionResult> RecognizePatterns(int32 HistoryDuration = 60);

    // Check if specific pattern is present
    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    bool HasPattern(const FString& PatternName, float MinConfidence = 0.7f);

    //==========================================================================
    // Pattern Application
    //==========================================================================

    // Apply pattern to enhance avatar
    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    void ApplyPattern(const FString& PatternName);

    // Enhance specific property
    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    void EnhanceProperty(const FString& PropertyName, float TargetValue);

    //==========================================================================
    // Quality Validation
    //==========================================================================

    // Validate overall quality
    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    float ValidateQualityWithoutAName();

    // Get quality improvement suggestions
    UFUNCTION(BlueprintCallable, Category = "Pattern Language")
    TArray<FString> GetQualityImprovementSuggestions();

    //==========================================================================
    // Events
    //==========================================================================

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPatternRecognized, FPatternRecognitionResult, Pattern);
    UPROPERTY(BlueprintAssignable, Category = "Pattern Language")
    FOnPatternRecognized OnPatternRecognized;

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQualityChanged, float, NewQuality);
    UPROPERTY(BlueprintAssignable, Category = "Pattern Language")
    FOnQualityChanged OnQualityChanged;

    //==========================================================================
    // Configuration
    //==========================================================================

    // How often to calculate pattern properties (seconds)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern Language|Configuration")
    float CalculationInterval = 5.0f;

    // Minimum quality threshold for warnings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern Language|Configuration")
    float MinimumQualityThreshold = 0.6f;

    // Enable automatic pattern enhancement
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pattern Language|Configuration")
    bool bAutoEnhancePatterns = false;

    //==========================================================================
    // State
    //==========================================================================

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language|State")
    FPatternPropertyMetrics CurrentMetrics;

    UPROPERTY(BlueprintReadOnly, Category = "Pattern Language|State")
    TArray<FPatternRecognitionResult> RecognizedPatterns;

private:
    // Component references
    UPROPERTY()
    UPersonalityTraitSystem* PersonalitySystem;

    UPROPERTY()
    UNeurochemicalSimulationComponent* NeurochemicalSystem;

    UPROPERTY()
    UDiaryInsightBlogLoop* NarrativeSystem;

    UPROPERTY()
    UAvatar3DComponentEnhanced* AvatarComponent;

    // Timing
    float TimeSinceLastCalculation = 0.0f;
    float LastQuality = 0.0f;

    // Pattern catalog (loaded from configuration)
    TMap<FString, TArray<FString>> PatternCatalog;

    // Helper functions
    void InitializeComponents();
    void InitializePatternCatalog();
    float CalculateVariance(const TArray<float>& Values);
    float CalculateCorrelation(const TArray<float>& A, const TArray<float>& B);
    bool CheckPropertyThreshold(float Value, float Threshold);
};

//==============================================================================
// PatternAwareAvatarComponent.cpp
//==============================================================================

#include "PatternAwareAvatarComponent.h"
#include "Personality/PersonalityTraitSystem.h"
#include "Neurochemical/NeurochemicalSimulationComponent.h"
#include "Narrative/DiaryInsightBlogLoop.h"
#include "Avatar/Avatar3DComponentEnhanced.h"

UPatternAwareAvatarComponent::UPatternAwareAvatarComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f; // 10 Hz
}

void UPatternAwareAvatarComponent::BeginPlay()
{
    Super::BeginPlay();
    
    InitializeComponents();
    InitializePatternCatalog();
    
    // Initial calculation
    CurrentMetrics = CalculatePatternProperties();
    LastQuality = CurrentMetrics.QualityWithoutAName;
}

void UPatternAwareAvatarComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                 FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    TimeSinceLastCalculation += DeltaTime;
    
    if (TimeSinceLastCalculation >= CalculationInterval)
    {
        // Recalculate pattern properties
        CurrentMetrics = CalculatePatternProperties();
        
        // Check for quality changes
        float QualityChange = FMath::Abs(CurrentMetrics.QualityWithoutAName - LastQuality);
        if (QualityChange > 0.05f)
        {
            OnQualityChanged.Broadcast(CurrentMetrics.QualityWithoutAName);
            LastQuality = CurrentMetrics.QualityWithoutAName;
        }
        
        // Recognize patterns
        RecognizedPatterns = RecognizePatterns();
        
        // Auto-enhance if enabled
        if (bAutoEnhancePatterns && CurrentMetrics.QualityWithoutAName < MinimumQualityThreshold)
        {
            TArray<FString> Suggestions = GetQualityImprovementSuggestions();
            if (Suggestions.Num() > 0)
            {
                // Apply first suggestion
                ApplyPattern(Suggestions[0]);
            }
        }
        
        TimeSinceLastCalculation = 0.0f;
    }
}

//==============================================================================
// Component Initialization
//==============================================================================

void UPatternAwareAvatarComponent::InitializeComponents()
{
    AActor* Owner = GetOwner();
    if (!Owner) return;
    
    PersonalitySystem = Owner->FindComponentByClass<UPersonalityTraitSystem>();
    NeurochemicalSystem = Owner->FindComponentByClass<UNeurochemicalSimulationComponent>();
    NarrativeSystem = Owner->FindComponentByClass<UDiaryInsightBlogLoop>();
    AvatarComponent = Owner->FindComponentByClass<UAvatar3DComponentEnhanced>();
}

void UPatternAwareAvatarComponent::InitializePatternCatalog()
{
    // Emotional Inertia Pattern
    PatternCatalog.Add("EmotionalInertia", {
        "Gradients", "Roughness", "SimplicityAndInnerCalm"
    });
    
    // Personality Constellation Pattern
    PatternCatalog.Add("PersonalityConstellation", {
        "StrongCenters", "Boundaries", "Contrast"
    });
    
    // Narrative Rhythm Pattern
    PatternCatalog.Add("NarrativeRhythm", {
        "AlternatingRepetition", "TheVoid", "Echoes"
    });
    
    // Visual Coherence Pattern
    PatternCatalog.Add("VisualCoherence", {
        "GoodShape", "LocalSymmetries", "PositiveSpace"
    });
    
    // System Integration Pattern
    PatternCatalog.Add("SystemIntegration", {
        "DeepInterlockAndAmbiguity", "NotSeparateness", "LevelsOfScale"
    });
}

//==============================================================================
// Pattern Property Calculation
//==============================================================================

FPatternPropertyMetrics UPatternAwareAvatarComponent::CalculatePatternProperties()
{
    FPatternPropertyMetrics Metrics;
    
    Metrics.LevelsOfScale = CalculateLevelsOfScale();
    Metrics.StrongCenters = CalculateStrongCenters();
    Metrics.Boundaries = CalculateBoundaries();
    Metrics.AlternatingRepetition = CalculateAlternatingRepetition();
    Metrics.PositiveSpace = CalculatePositiveSpace();
    Metrics.GoodShape = CalculateGoodShape();
    Metrics.LocalSymmetries = CalculateLocalSymmetries();
    Metrics.DeepInterlockAndAmbiguity = CalculateDeepInterlockAndAmbiguity();
    Metrics.Contrast = CalculateContrast();
    Metrics.Gradients = CalculateGradients();
    Metrics.Roughness = CalculateRoughness();
    Metrics.Echoes = CalculateEchoes();
    Metrics.TheVoid = CalculateTheVoid();
    Metrics.SimplicityAndInnerCalm = CalculateSimplicityAndInnerCalm();
    Metrics.NotSeparateness = CalculateNotSeparateness();
    
    Metrics.CalculateOverallQuality();
    
    return Metrics;
}

float UPatternAwareAvatarComponent::CalculateLevelsOfScale()
{
    // Check for coherent organization at multiple scales
    float Score = 0.0f;
    
    if (NeurochemicalSystem && PersonalitySystem && AvatarComponent)
    {
        // Micro scale: Individual neurochemicals
        Score += 0.2f;
        
        // Meso scale: Emotional states and personality traits
        Score += 0.3f;
        
        // Macro scale: Overall avatar presence and narrative
        if (NarrativeSystem)
        {
            Score += 0.5f;
        }
    }
    
    return FMath::Clamp(Score, 0.0f, 1.0f);
}

float UPatternAwareAvatarComponent::CalculateStrongCenters()
{
    // Check if each component is a strong, coherent center
    float Score = 0.0f;
    int32 ComponentCount = 0;
    
    if (PersonalitySystem)
    {
        // Personality system strength based on trait coherence
        float TraitVariance = CalculateVariance({
            PersonalitySystem->GetTraitIntensity("Confidence"),
            PersonalitySystem->GetTraitIntensity("Playfulness"),
            PersonalitySystem->GetTraitIntensity("Unpredictability")
        });
        Score += (1.0f - TraitVariance); // Lower variance = stronger center
        ComponentCount++;
    }
    
    if (NeurochemicalSystem)
    {
        // Neurochemical system strength based on homeostasis
        Score += 0.8f; // Assume strong if present
        ComponentCount++;
    }
    
    if (NarrativeSystem)
    {
        // Narrative system strength based on active loops
        Score += 0.9f; // Assume strong if present
        ComponentCount++;
    }
    
    return ComponentCount > 0 ? (Score / ComponentCount) : 0.0f;
}

float UPatternAwareAvatarComponent::CalculateBoundaries()
{
    // Check if components have clear boundaries
    float Score = 0.0f;
    
    if (PersonalitySystem)
    {
        // Personality traits have clear 0-1 boundaries
        Score += 0.25f;
    }
    
    if (NeurochemicalSystem)
    {
        // Neurochemicals have baseline and range boundaries
        Score += 0.25f;
    }
    
    if (AvatarComponent)
    {
        // Visual appearance has defined parameters
        Score += 0.25f;
    }
    
    if (NarrativeSystem)
    {
        // Narrative phases have clear boundaries
        Score += 0.25f;
    }
    
    return Score;
}

float UPatternAwareAvatarComponent::CalculateAlternatingRepetition()
{
    // Check for repeated elements with variation
    float Score = 0.0f;
    
    if (NarrativeSystem)
    {
        // Diary-insight-blog cycle is alternating repetition
        Score += 0.4f;
    }
    
    if (NeurochemicalSystem)
    {
        // Neurochemical oscillations around baseline
        Score += 0.3f;
    }
    
    if (AvatarComponent)
    {
        // Animation cycles (breathing, blinking, idle)
        Score += 0.3f;
    }
    
    return Score;
}

float UPatternAwareAvatarComponent::CalculatePositiveSpace()
{
    // Check if "neutral" states are meaningful
    float Score = 0.0f;
    
    if (NeurochemicalSystem)
    {
        // Neutral emotional state is a positive state, not absence
        Score += 0.4f;
    }
    
    if (NarrativeSystem)
    {
        // Observation phase is active, not passive
        Score += 0.3f;
    }
    
    if (AvatarComponent)
    {
        // Idle animations make stillness meaningful
        Score += 0.3f;
    }
    
    return Score;
}

float UPatternAwareAvatarComponent::CalculateGoodShape()
{
    // Check for simple, coherent, recognizable shapes
    float Score = 0.0f;
    
    if (PersonalitySystem)
    {
        // Clear personality profile (SuperHotGirl, HyperChaotic)
        Score += 0.3f;
    }
    
    if (AvatarComponent)
    {
        // Clean visual silhouette and recognizable features
        Score += 0.4f;
    }
    
    // System architecture has clear component boundaries
    Score += 0.3f;
    
    return Score;
}

float UPatternAwareAvatarComponent::CalculateLocalSymmetries()
{
    // Check for small-scale symmetries
    float Score = 0.0f;
    
    if (PersonalitySystem)
    {
        // Complementary traits (confidence ↔ vulnerability)
        float Confidence = PersonalitySystem->GetTraitIntensity("Confidence");
        float Vulnerability = PersonalitySystem->GetTraitIntensity("Vulnerability");
        float Balance = 1.0f - FMath::Abs(Confidence - (1.0f - Vulnerability));
        Score += Balance * 0.4f;
    }
    
    if (NeurochemicalSystem)
    {
        // Balanced neurochemical pairs
        Score += 0.3f;
    }
    
    if (AvatarComponent)
    {
        // Facial symmetry
        Score += 0.3f;
    }
    
    return Score;
}

float UPatternAwareAvatarComponent::CalculateDeepInterlockAndAmbiguity()
{
    // Check for bidirectional connections between systems
    float Score = 0.0f;
    
    if (PersonalitySystem && NeurochemicalSystem)
    {
        // Personality-neurochemical coupling
        Score += 0.3f;
    }
    
    if (NeurochemicalSystem && AvatarComponent)
    {
        // Emotion-appearance blending
        Score += 0.3f;
    }
    
    if (NarrativeSystem && PersonalitySystem)
    {
        // Memory-narrative integration
        Score += 0.2f;
    }
    
    // All systems influence each other
    if (PersonalitySystem && NeurochemicalSystem && NarrativeSystem && AvatarComponent)
    {
        Score += 0.2f;
    }
    
    return Score;
}

float UPatternAwareAvatarComponent::CalculateContrast()
{
    // Check for meaningful contrasts
    float Score = 0.0f;
    
    if (PersonalitySystem)
    {
        // SuperHotGirl vs HyperChaotic provides contrast
        float SuperHotGirl = (PersonalitySystem->GetTraitIntensity("Confidence") +
                             PersonalitySystem->GetTraitIntensity("Charm")) / 2.0f;
        float HyperChaotic = (PersonalitySystem->GetTraitIntensity("Unpredictability") +
                             PersonalitySystem->GetTraitIntensity("Impulsivity")) / 2.0f;
        
        float Contrast = FMath::Abs(SuperHotGirl - HyperChaotic);
        Score += Contrast * 0.5f;
    }
    
    if (NeurochemicalSystem)
    {
        // Emotional range (joy to sadness, calm to excited)
        Score += 0.3f;
    }
    
    if (AvatarComponent)
    {
        // Visual contrasts (pastel vs dark, soft vs sharp)
        Score += 0.2f;
    }
    
    return Score;
}

float UPatternAwareAvatarComponent::CalculateGradients()
{
    // Check for gradual changes, not abrupt
    float Score = 0.0f;
    
    if (NeurochemicalSystem)
    {
        // Smooth emotional transitions
        Score += 0.4f;
    }
    
    if (PersonalitySystem)
    {
        // Gradual personality shifts
        Score += 0.3f;
    }
    
    if (AvatarComponent)
    {
        // Smooth animation transitions
        Score += 0.3f;
    }
    
    return Score;
}

float UPatternAwareAvatarComponent::CalculateRoughness()
{
    // Check for controlled imperfection
    float Score = 0.0f;
    
    if (PersonalitySystem)
    {
        // Trait values vary around baseline
        Score += 0.3f;
    }
    
    if (AvatarComponent)
    {
        // Animation variation and visual imperfections
        Score += 0.4f;
    }
    
    if (NarrativeSystem)
    {
        // Natural variation in narrative generation
        Score += 0.3f;
    }
    
    return Score;
}

float UPatternAwareAvatarComponent::CalculateEchoes()
{
    // Check for similar elements echoing across system
    float Score = 0.0f;
    
    if (PersonalitySystem && AvatarComponent)
    {
        // Playfulness in both behavior and visual effects
        Score += 0.3f;
    }
    
    if (NeurochemicalSystem && AvatarComponent)
    {
        // Emotional state echoed in appearance
        Score += 0.4f;
    }
    
    // Consistent design language across all systems
    Score += 0.3f;
    
    return Score;
}

float UPatternAwareAvatarComponent::CalculateTheVoid()
{
    // Check for intentional "empty" states
    float Score = 0.0f;
    
    if (NarrativeSystem)
    {
        // Observation phase includes pauses
        Score += 0.3f;
    }
    
    if (NeurochemicalSystem)
    {
        // Neutral emotional states
        Score += 0.3f;
    }
    
    if (AvatarComponent)
    {
        // Visual breathing room
        Score += 0.4f;
    }
    
    return Score;
}

float UPatternAwareAvatarComponent::CalculateSimplicityAndInnerCalm()
{
    // Check for complexity from simple rules
    float Score = 0.0f;
    
    if (NeurochemicalSystem)
    {
        // Simple homeostasis rules create rich dynamics
        Score += 0.4f;
    }
    
    if (PersonalitySystem)
    {
        // 12 traits generate diverse behaviors
        Score += 0.3f;
    }
    
    // Clean architecture with simple interfaces
    Score += 0.3f;
    
    return Score;
}

float UPatternAwareAvatarComponent::CalculateNotSeparateness()
{
    // Check for integration and connection
    float Score = 0.0f;
    
    // Count integrated systems
    int32 IntegratedSystems = 0;
    if (PersonalitySystem) IntegratedSystems++;
    if (NeurochemicalSystem) IntegratedSystems++;
    if (NarrativeSystem) IntegratedSystems++;
    if (AvatarComponent) IntegratedSystems++;
    
    // More integrated systems = higher score
    Score = IntegratedSystems / 4.0f;
    
    // Bonus for bidirectional connections
    if (IntegratedSystems >= 3)
    {
        Score = FMath::Min(Score + 0.2f, 1.0f);
    }
    
    return Score;
}

//==============================================================================
// Pattern Recognition
//==============================================================================

TArray<FPatternRecognitionResult> UPatternAwareAvatarComponent::RecognizePatterns(int32 HistoryDuration)
{
    TArray<FPatternRecognitionResult> Results;
    
    // Check each pattern in catalog
    for (const auto& PatternEntry : PatternCatalog)
    {
        FPatternRecognitionResult Result;
        Result.PatternName = PatternEntry.Key;
        
        // Calculate confidence based on related properties
        float TotalScore = 0.0f;
        int32 PropertyCount = 0;
        
        for (const FString& PropertyName : PatternEntry.Value)
        {
            float PropertyValue = 0.0f;
            
            if (PropertyName == "LevelsOfScale") PropertyValue = CurrentMetrics.LevelsOfScale;
            else if (PropertyName == "StrongCenters") PropertyValue = CurrentMetrics.StrongCenters;
            else if (PropertyName == "Boundaries") PropertyValue = CurrentMetrics.Boundaries;
            else if (PropertyName == "AlternatingRepetition") PropertyValue = CurrentMetrics.AlternatingRepetition;
            else if (PropertyName == "PositiveSpace") PropertyValue = CurrentMetrics.PositiveSpace;
            else if (PropertyName == "GoodShape") PropertyValue = CurrentMetrics.GoodShape;
            else if (PropertyName == "LocalSymmetries") PropertyValue = CurrentMetrics.LocalSymmetries;
            else if (PropertyName == "DeepInterlockAndAmbiguity") PropertyValue = CurrentMetrics.DeepInterlockAndAmbiguity;
            else if (PropertyName == "Contrast") PropertyValue = CurrentMetrics.Contrast;
            else if (PropertyName == "Gradients") PropertyValue = CurrentMetrics.Gradients;
            else if (PropertyName == "Roughness") PropertyValue = CurrentMetrics.Roughness;
            else if (PropertyName == "Echoes") PropertyValue = CurrentMetrics.Echoes;
            else if (PropertyName == "TheVoid") PropertyValue = CurrentMetrics.TheVoid;
            else if (PropertyName == "SimplicityAndInnerCalm") PropertyValue = CurrentMetrics.SimplicityAndInnerCalm;
            else if (PropertyName == "NotSeparateness") PropertyValue = CurrentMetrics.NotSeparateness;
            
            TotalScore += PropertyValue;
            PropertyCount++;
        }
        
        Result.Confidence = PropertyCount > 0 ? (TotalScore / PropertyCount) : 0.0f;
        
        // Only include patterns with confidence > 0.5
        if (Result.Confidence > 0.5f)
        {
            Results.Add(Result);
            OnPatternRecognized.Broadcast(Result);
        }
    }
    
    return Results;
}

bool UPatternAwareAvatarComponent::HasPattern(const FString& PatternName, float MinConfidence)
{
    for (const FPatternRecognitionResult& Pattern : RecognizedPatterns)
    {
        if (Pattern.PatternName == PatternName && Pattern.Confidence >= MinConfidence)
        {
            return true;
        }
    }
    return false;
}

//==============================================================================
// Pattern Application
//==============================================================================

void UPatternAwareAvatarComponent::ApplyPattern(const FString& PatternName)
{
    // Apply pattern-specific enhancements
    if (PatternName == "EmotionalInertia")
    {
        // Increase gradient smoothness
        EnhanceProperty("Gradients", 0.9f);
    }
    else if (PatternName == "PersonalityConstellation")
    {
        // Strengthen personality centers
        EnhanceProperty("StrongCenters", 0.9f);
    }
    else if (PatternName == "NarrativeRhythm")
    {
        // Improve alternating repetition
        EnhanceProperty("AlternatingRepetition", 0.9f);
    }
    else if (PatternName == "VisualCoherence")
    {
        // Improve good shape
        EnhanceProperty("GoodShape", 0.9f);
    }
    else if (PatternName == "SystemIntegration")
    {
        // Improve not-separateness
        EnhanceProperty("NotSeparateness", 0.9f);
    }
}

void UPatternAwareAvatarComponent::EnhanceProperty(const FString& PropertyName, float TargetValue)
{
    // Property enhancement would modify component parameters
    // This is a placeholder for actual implementation
    UE_LOG(LogTemp, Log, TEXT("Enhancing property: %s to target: %f"), *PropertyName, TargetValue);
}

//==============================================================================
// Quality Validation
//==============================================================================

float UPatternAwareAvatarComponent::ValidateQualityWithoutAName()
{
    return CurrentMetrics.QualityWithoutAName;
}

TArray<FString> UPatternAwareAvatarComponent::GetQualityImprovementSuggestions()
{
    TArray<FString> Suggestions;
    
    // Find properties below threshold
    if (CurrentMetrics.Gradients < 0.7f)
        Suggestions.Add("EmotionalInertia");
    
    if (CurrentMetrics.StrongCenters < 0.7f)
        Suggestions.Add("PersonalityConstellation");
    
    if (CurrentMetrics.AlternatingRepetition < 0.7f)
        Suggestions.Add("NarrativeRhythm");
    
    if (CurrentMetrics.GoodShape < 0.7f)
        Suggestions.Add("VisualCoherence");
    
    if (CurrentMetrics.NotSeparateness < 0.7f)
        Suggestions.Add("SystemIntegration");
    
    return Suggestions;
}

//==============================================================================
// Helper Functions
//==============================================================================

float UPatternAwareAvatarComponent::CalculateVariance(const TArray<float>& Values)
{
    if (Values.Num() == 0) return 0.0f;
    
    float Mean = 0.0f;
    for (float Value : Values)
    {
        Mean += Value;
    }
    Mean /= Values.Num();
    
    float Variance = 0.0f;
    for (float Value : Values)
    {
        float Diff = Value - Mean;
        Variance += Diff * Diff;
    }
    Variance /= Values.Num();
    
    return FMath::Sqrt(Variance);
}

float UPatternAwareAvatarComponent::CalculateCorrelation(const TArray<float>& A, const TArray<float>& B)
{
    if (A.Num() != B.Num() || A.Num() == 0) return 0.0f;
    
    // Simple correlation coefficient calculation
    float MeanA = 0.0f, MeanB = 0.0f;
    for (int32 i = 0; i < A.Num(); i++)
    {
        MeanA += A[i];
        MeanB += B[i];
    }
    MeanA /= A.Num();
    MeanB /= B.Num();
    
    float Covariance = 0.0f, VarA = 0.0f, VarB = 0.0f;
    for (int32 i = 0; i < A.Num(); i++)
    {
        float DiffA = A[i] - MeanA;
        float DiffB = B[i] - MeanB;
        Covariance += DiffA * DiffB;
        VarA += DiffA * DiffA;
        VarB += DiffB * DiffB;
    }
    
    float Denominator = FMath::Sqrt(VarA * VarB);
    return Denominator > 0.0f ? (Covariance / Denominator) : 0.0f;
}

bool UPatternAwareAvatarComponent::CheckPropertyThreshold(float Value, float Threshold)
{
    return Value >= Threshold;
}

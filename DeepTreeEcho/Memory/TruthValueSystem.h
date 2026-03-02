// TruthValueSystem.h
// Feature F1.4.4: Probabilistic Truth Value System for Deep Tree Echo
// Implements strength-confidence based uncertain reasoning following OpenCog conventions
// Copyright (c) 2025-2026 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TruthValueSystem.generated.h"

// Forward declarations
class UHypergraphMemorySystem;

// ========================================
// TRUTH VALUE ENUMERATIONS
// ========================================

/**
 * Truth value type classification
 * Based on OpenCog TruthValue types
 */
UENUM(BlueprintType)
enum class ETruthValueType : uint8
{
    /** Simple strength-confidence pair */
    Simple          UMETA(DisplayName = "Simple Truth Value"),
    /** Count-based with observation tracking */
    Count           UMETA(DisplayName = "Count Truth Value"),
    /** Indefinite probability with lower/upper bounds */
    Indefinite      UMETA(DisplayName = "Indefinite Truth Value"),
    /** Fuzzy logic with degree of membership */
    Fuzzy           UMETA(DisplayName = "Fuzzy Truth Value"),
    /** Full probability distribution */
    Probabilistic   UMETA(DisplayName = "Probabilistic Truth Value"),
    /** Evidence-based with positive/negative counts */
    Evidence        UMETA(DisplayName = "Evidence Truth Value")
};

/**
 * Truth value merge strategy
 * How to combine multiple truth values
 */
UENUM(BlueprintType)
enum class ETruthValueMergeStrategy : uint8
{
    /** Use higher confidence value */
    HigherConfidence    UMETA(DisplayName = "Higher Confidence Wins"),
    /** Use lower confidence (conservative) */
    LowerConfidence     UMETA(DisplayName = "Lower Confidence (Conservative)"),
    /** Average weighted by confidence */
    WeightedAverage     UMETA(DisplayName = "Weighted Average"),
    /** Bayesian revision */
    BayesianRevision    UMETA(DisplayName = "Bayesian Revision"),
    /** Dempster-Shafer combination */
    DempsterShafer      UMETA(DisplayName = "Dempster-Shafer"),
    /** Maximum strength */
    MaxStrength         UMETA(DisplayName = "Maximum Strength"),
    /** Minimum strength */
    MinStrength         UMETA(DisplayName = "Minimum Strength")
};

/**
 * Inference rule type for truth value propagation
 */
UENUM(BlueprintType)
enum class ETruthValueInferenceRule : uint8
{
    /** Deduction: A→B, B→C ⊢ A→C */
    Deduction           UMETA(DisplayName = "Deduction"),
    /** Induction: A→B, A→C ⊢ B→C */
    Induction           UMETA(DisplayName = "Induction"),
    /** Abduction: A→B, C→B ⊢ A→C */
    Abduction           UMETA(DisplayName = "Abduction"),
    /** Modus Ponens: A→B, A ⊢ B */
    ModusPonens         UMETA(DisplayName = "Modus Ponens"),
    /** And: A, B ⊢ A∧B */
    Conjunction         UMETA(DisplayName = "Conjunction (AND)"),
    /** Or: A, B ⊢ A∨B */
    Disjunction         UMETA(DisplayName = "Disjunction (OR)"),
    /** Negation: A ⊢ ¬A */
    Negation            UMETA(DisplayName = "Negation"),
    /** Revision: update belief with new evidence */
    Revision            UMETA(DisplayName = "Revision"),
    /** Similarity: A≈B based on shared properties */
    Similarity          UMETA(DisplayName = "Similarity")
};

// ========================================
// TRUTH VALUE STRUCTURES
// ========================================

/**
 * Simple Truth Value - basic strength/confidence pair
 * Most common truth value type for general reasoning
 */
USTRUCT(BlueprintType)
struct FSimpleTruthValue
{
    GENERATED_BODY()

    /** Strength: probability/degree of truth (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Strength = 1.0f;

    /** Confidence: certainty in the strength value (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Confidence = 0.9f;

    FSimpleTruthValue() = default;
    
    FSimpleTruthValue(float InStrength, float InConfidence)
        : Strength(FMath::Clamp(InStrength, 0.0f, 1.0f))
        , Confidence(FMath::Clamp(InConfidence, 0.0f, 1.0f))
    {}

    /** Check if truth value indicates true (strength > threshold) */
    bool IsTrue(float Threshold = 0.5f) const { return Strength > Threshold; }

    /** Check if truth value is confident (confidence > threshold) */
    bool IsConfident(float Threshold = 0.5f) const { return Confidence > Threshold; }

    /** Calculate mean expected value (strength weighted by confidence) */
    float GetMeanValue() const { return Strength * Confidence; }

    /** Get uncertainty (1 - confidence) */
    float GetUncertainty() const { return 1.0f - Confidence; }

    bool operator==(const FSimpleTruthValue& Other) const
    {
        return FMath::IsNearlyEqual(Strength, Other.Strength) &&
               FMath::IsNearlyEqual(Confidence, Other.Confidence);
    }
};

/**
 * Count Truth Value - includes observation count for evidence tracking
 * Used when counting evidence for/against a proposition
 */
USTRUCT(BlueprintType)
struct FCountTruthValue
{
    GENERATED_BODY()

    /** Strength: probability/degree of truth (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Strength = 1.0f;

    /** Confidence: certainty in the strength value (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Confidence = 0.9f;

    /** Count: number of observations/evidence samples */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue", meta = (ClampMin = "0"))
    int32 Count = 1;

    FCountTruthValue() = default;
    
    FCountTruthValue(float InStrength, float InConfidence, int32 InCount = 1)
        : Strength(FMath::Clamp(InStrength, 0.0f, 1.0f))
        , Confidence(FMath::Clamp(InConfidence, 0.0f, 1.0f))
        , Count(FMath::Max(0, InCount))
    {}

    /** Calculate confidence from count (confidence = count / (count + k)) */
    static float ConfidenceFromCount(int32 InCount, float K = 800.0f)
    {
        return static_cast<float>(InCount) / (static_cast<float>(InCount) + K);
    }

    /** Convert to simple truth value */
    FSimpleTruthValue ToSimple() const { return FSimpleTruthValue(Strength, Confidence); }
};

/**
 * Indefinite Truth Value - probability interval with lower/upper bounds
 * Used when exact probability cannot be determined
 */
USTRUCT(BlueprintType)
struct FIndefiniteTruthValue
{
    GENERATED_BODY()

    /** Lower bound of probability interval (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LowerBound = 0.0f;

    /** Upper bound of probability interval (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float UpperBound = 1.0f;

    /** Confidence in the interval (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Confidence = 0.5f;

    FIndefiniteTruthValue() = default;

    FIndefiniteTruthValue(float InLower, float InUpper, float InConfidence = 0.5f)
        : LowerBound(FMath::Clamp(InLower, 0.0f, 1.0f))
        , UpperBound(FMath::Clamp(InUpper, 0.0f, 1.0f))
        , Confidence(FMath::Clamp(InConfidence, 0.0f, 1.0f))
    {
        // Ensure lower <= upper
        if (LowerBound > UpperBound)
        {
            float Temp = LowerBound;
            LowerBound = UpperBound;
            UpperBound = Temp;
        }
    }

    /** Get interval width (uncertainty range) */
    float GetIntervalWidth() const { return UpperBound - LowerBound; }

    /** Get midpoint of interval */
    float GetMidpoint() const { return (LowerBound + UpperBound) * 0.5f; }

    /** Convert to simple truth value using midpoint */
    FSimpleTruthValue ToSimple() const 
    {
        return FSimpleTruthValue(GetMidpoint(), Confidence * (1.0f - GetIntervalWidth()));
    }
};

/**
 * Fuzzy Truth Value - degree of membership in fuzzy sets
 * Used for vague predicates (tall, hot, fast, etc.)
 */
USTRUCT(BlueprintType)
struct FFuzzyTruthValue
{
    GENERATED_BODY()

    /** Membership degree (0-1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Membership = 0.5f;

    /** Hedge factor: 1.0 = normal, <1.0 = very, >1.0 = somewhat */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue", meta = (ClampMin = "0.1", ClampMax = "3.0"))
    float Hedge = 1.0f;

    /** Confidence in membership assessment */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Confidence = 0.9f;

    FFuzzyTruthValue() = default;

    FFuzzyTruthValue(float InMembership, float InConfidence = 0.9f, float InHedge = 1.0f)
        : Membership(FMath::Clamp(InMembership, 0.0f, 1.0f))
        , Hedge(FMath::Clamp(InHedge, 0.1f, 3.0f))
        , Confidence(FMath::Clamp(InConfidence, 0.0f, 1.0f))
    {}

    /** Get hedged membership value */
    float GetHedgedMembership() const 
    {
        return FMath::Pow(Membership, Hedge);
    }

    /** Apply "very" hedge (square) */
    void ApplyVery() { Hedge = 0.5f; }

    /** Apply "somewhat" hedge (square root) */
    void ApplySomewhat() { Hedge = 2.0f; }

    /** Convert to simple truth value */
    FSimpleTruthValue ToSimple() const 
    {
        return FSimpleTruthValue(GetHedgedMembership(), Confidence);
    }
};

/**
 * Evidence Truth Value - positive/negative evidence counts
 * Used for Bayesian-style reasoning with explicit evidence
 */
USTRUCT(BlueprintType)
struct FEvidenceTruthValue
{
    GENERATED_BODY()

    /** Positive evidence count */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue", meta = (ClampMin = "0"))
    int32 PositiveCount = 0;

    /** Negative evidence count */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue", meta = (ClampMin = "0"))
    int32 NegativeCount = 0;

    /** Prior strength (default assumption before evidence) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PriorStrength = 0.5f;

    FEvidenceTruthValue() = default;

    FEvidenceTruthValue(int32 InPositive, int32 InNegative, float InPrior = 0.5f)
        : PositiveCount(FMath::Max(0, InPositive))
        , NegativeCount(FMath::Max(0, InNegative))
        , PriorStrength(FMath::Clamp(InPrior, 0.0f, 1.0f))
    {}

    /** Get total evidence count */
    int32 GetTotalCount() const { return PositiveCount + NegativeCount; }

    /** Calculate strength from evidence (positive / total) */
    float CalculateStrength() const
    {
        int32 Total = GetTotalCount();
        if (Total == 0) return PriorStrength;
        return static_cast<float>(PositiveCount) / static_cast<float>(Total);
    }

    /** Calculate confidence from evidence count */
    float CalculateConfidence(float K = 800.0f) const
    {
        return FCountTruthValue::ConfidenceFromCount(GetTotalCount(), K);
    }

    /** Add positive evidence */
    void AddPositiveEvidence(int32 Count = 1) { PositiveCount += FMath::Max(0, Count); }

    /** Add negative evidence */
    void AddNegativeEvidence(int32 Count = 1) { NegativeCount += FMath::Max(0, Count); }

    /** Convert to simple truth value */
    FSimpleTruthValue ToSimple(float K = 800.0f) const
    {
        return FSimpleTruthValue(CalculateStrength(), CalculateConfidence(K));
    }
};

/**
 * Probabilistic Truth Value - full probability distribution
 * Most general but computationally expensive truth value type
 */
USTRUCT(BlueprintType)
struct FProbabilisticTruthValue
{
    GENERATED_BODY()

    /** Probability distribution (histogram with NumBins bins from 0 to 1) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue")
    TArray<float> Distribution;

    /** Number of bins in the distribution */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue", meta = (ClampMin = "2", ClampMax = "100"))
    int32 NumBins = 10;

    FProbabilisticTruthValue() 
    {
        InitializeUniform();
    }

    FProbabilisticTruthValue(int32 InNumBins)
        : NumBins(FMath::Clamp(InNumBins, 2, 100))
    {
        InitializeUniform();
    }

    /** Initialize with uniform distribution */
    void InitializeUniform()
    {
        Distribution.SetNum(NumBins);
        float UniformProb = 1.0f / static_cast<float>(NumBins);
        for (int32 i = 0; i < NumBins; i++)
        {
            Distribution[i] = UniformProb;
        }
    }

    /** Initialize with peaked distribution at given value */
    void InitializePeaked(float PeakValue, float Spread = 0.1f)
    {
        Distribution.SetNum(NumBins);
        float Total = 0.0f;
        for (int32 i = 0; i < NumBins; i++)
        {
            float BinCenter = (static_cast<float>(i) + 0.5f) / static_cast<float>(NumBins);
            float Diff = BinCenter - PeakValue;
            Distribution[i] = FMath::Exp(-Diff * Diff / (2.0f * Spread * Spread));
            Total += Distribution[i];
        }
        // Normalize
        if (Total > 0.0f)
        {
            for (int32 i = 0; i < NumBins; i++)
            {
                Distribution[i] /= Total;
            }
        }
    }

    /** Get expected value (mean of distribution) */
    float GetExpectedValue() const
    {
        if (Distribution.Num() == 0) return 0.5f;
        float Expected = 0.0f;
        for (int32 i = 0; i < Distribution.Num(); i++)
        {
            float BinCenter = (static_cast<float>(i) + 0.5f) / static_cast<float>(Distribution.Num());
            Expected += BinCenter * Distribution[i];
        }
        return Expected;
    }

    /** Get variance of distribution */
    float GetVariance() const
    {
        if (Distribution.Num() == 0) return 0.0f;
        float Mean = GetExpectedValue();
        float Variance = 0.0f;
        for (int32 i = 0; i < Distribution.Num(); i++)
        {
            float BinCenter = (static_cast<float>(i) + 0.5f) / static_cast<float>(Distribution.Num());
            float Diff = BinCenter - Mean;
            Variance += Diff * Diff * Distribution[i];
        }
        return Variance;
    }

    /** Get entropy of distribution */
    float GetEntropy() const
    {
        float Entropy = 0.0f;
        for (int32 i = 0; i < Distribution.Num(); i++)
        {
            if (Distribution[i] > 0.0001f)
            {
                Entropy -= Distribution[i] * FMath::Loge(Distribution[i]);
            }
        }
        return Entropy;
    }

    /** Convert to simple truth value */
    FSimpleTruthValue ToSimple() const
    {
        float Strength = GetExpectedValue();
        // Confidence derived from inverse variance (lower variance = higher confidence)
        float MaxVariance = 0.25f; // Maximum variance for uniform distribution
        float Confidence = 1.0f - FMath::Sqrt(GetVariance() / MaxVariance);
        return FSimpleTruthValue(Strength, FMath::Clamp(Confidence, 0.0f, 1.0f));
    }
};

/**
 * Universal Truth Value container
 * Can hold any truth value type with conversion utilities
 */
USTRUCT(BlueprintType)
struct FTruthValue
{
    GENERATED_BODY()

    /** Truth value type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue")
    ETruthValueType Type = ETruthValueType::Simple;

    /** Simple truth value (always populated) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue")
    FSimpleTruthValue Simple;

    /** Count truth value (if Type == Count) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue")
    FCountTruthValue Count;

    /** Indefinite truth value (if Type == Indefinite) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue")
    FIndefiniteTruthValue Indefinite;

    /** Fuzzy truth value (if Type == Fuzzy) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue")
    FFuzzyTruthValue Fuzzy;

    /** Evidence truth value (if Type == Evidence) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue")
    FEvidenceTruthValue Evidence;

    /** Probabilistic truth value (if Type == Probabilistic) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue")
    FProbabilisticTruthValue Probabilistic;

    FTruthValue() = default;

    /** Construct from simple strength/confidence */
    FTruthValue(float Strength, float Confidence)
        : Type(ETruthValueType::Simple)
        , Simple(Strength, Confidence)
    {}

    /** Construct from simple truth value */
    FTruthValue(const FSimpleTruthValue& InSimple)
        : Type(ETruthValueType::Simple)
        , Simple(InSimple)
    {}

    /** Get strength (always available via conversion) */
    float GetStrength() const
    {
        switch (Type)
        {
        case ETruthValueType::Simple: return Simple.Strength;
        case ETruthValueType::Count: return Count.Strength;
        case ETruthValueType::Indefinite: return Indefinite.GetMidpoint();
        case ETruthValueType::Fuzzy: return Fuzzy.GetHedgedMembership();
        case ETruthValueType::Evidence: return Evidence.CalculateStrength();
        case ETruthValueType::Probabilistic: return Probabilistic.GetExpectedValue();
        default: return 0.5f;
        }
    }

    /** Get confidence (always available via conversion) */
    float GetConfidence() const
    {
        switch (Type)
        {
        case ETruthValueType::Simple: return Simple.Confidence;
        case ETruthValueType::Count: return Count.Confidence;
        case ETruthValueType::Indefinite: return Indefinite.Confidence * (1.0f - Indefinite.GetIntervalWidth());
        case ETruthValueType::Fuzzy: return Fuzzy.Confidence;
        case ETruthValueType::Evidence: return Evidence.CalculateConfidence();
        case ETruthValueType::Probabilistic: return Probabilistic.ToSimple().Confidence;
        default: return 0.0f;
        }
    }

    /** Convert to simple truth value */
    FSimpleTruthValue ToSimple() const
    {
        return FSimpleTruthValue(GetStrength(), GetConfidence());
    }

    /** Check if indicates true */
    bool IsTrue(float Threshold = 0.5f) const { return GetStrength() > Threshold; }

    /** Check if confident */
    bool IsConfident(float Threshold = 0.5f) const { return GetConfidence() > Threshold; }
};

/**
 * Inference result from truth value operations
 */
USTRUCT(BlueprintType)
struct FTruthValueInferenceResult
{
    GENERATED_BODY()

    /** Resulting truth value */
    UPROPERTY(BlueprintReadWrite, Category = "TruthValue")
    FTruthValue Result;

    /** Inference rule applied */
    UPROPERTY(BlueprintReadWrite, Category = "TruthValue")
    ETruthValueInferenceRule RuleApplied = ETruthValueInferenceRule::Deduction;

    /** Confidence in the inference itself */
    UPROPERTY(BlueprintReadWrite, Category = "TruthValue")
    float InferenceConfidence = 1.0f;

    /** Source truth value IDs used */
    UPROPERTY(BlueprintReadWrite, Category = "TruthValue")
    TArray<int64> SourceIDs;

    /** Timestamp of inference */
    UPROPERTY(BlueprintReadWrite, Category = "TruthValue")
    double InferenceTime = 0.0;
};

/**
 * Truth value system statistics
 */
USTRUCT(BlueprintType)
struct FTruthValueSystemStats
{
    GENERATED_BODY()

    /** Total truth values in system */
    UPROPERTY(BlueprintReadOnly, Category = "TruthValue")
    int32 TotalTruthValues = 0;

    /** Average strength across all values */
    UPROPERTY(BlueprintReadOnly, Category = "TruthValue")
    float AverageStrength = 0.0f;

    /** Average confidence across all values */
    UPROPERTY(BlueprintReadOnly, Category = "TruthValue")
    float AverageConfidence = 0.0f;

    /** Count by type */
    UPROPERTY(BlueprintReadOnly, Category = "TruthValue")
    TMap<ETruthValueType, int32> CountByType;

    /** Total inferences performed */
    UPROPERTY(BlueprintReadOnly, Category = "TruthValue")
    int64 TotalInferences = 0;

    /** Average inference time (ms) */
    UPROPERTY(BlueprintReadOnly, Category = "TruthValue")
    float AverageInferenceTimeMs = 0.0f;
};

// ========================================
// DELEGATE DECLARATIONS
// ========================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTruthValueCreated, int64, TruthValueID, const FTruthValue&, TruthValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTruthValueUpdated, int64, TruthValueID, const FTruthValue&, NewTruthValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTruthValueDeleted, int64, TruthValueID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInferencePerformed, ETruthValueInferenceRule, Rule, const FTruthValueInferenceResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTruthValueConflict, int64, TruthValueA, int64, TruthValueB);

// ========================================
// TRUTH VALUE SYSTEM COMPONENT
// ========================================

/**
 * Truth Value System Component
 * 
 * Implements probabilistic truth values with strength and confidence for
 * uncertain reasoning in the Deep Tree Echo cognitive architecture.
 * 
 * Features:
 * - Multiple truth value types (Simple, Count, Indefinite, Fuzzy, Evidence, Probabilistic)
 * - Truth value inference operations (Deduction, Induction, Abduction, etc.)
 * - Truth value revision and merging
 * - Integration with HypergraphMemorySystem
 * - Evidence accumulation and Bayesian updating
 * 
 * Based on OpenCog TruthValue semantics:
 * - Strength: probability or degree of truth (0-1)
 * - Confidence: certainty in the strength value (0-1)
 * 
 * Confidence can be interpreted as:
 * - Confidence = n/(n+k) where n is evidence count and k is a constant
 * - Higher confidence means more certain, not more true
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UTruthValueSystem : public UActorComponent
{
    GENERATED_BODY()

public:
    UTruthValueSystem();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Default confidence value for new truth values */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DefaultConfidence = 0.9f;

    /** K constant for count-to-confidence conversion */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue|Config", meta = (ClampMin = "1.0", ClampMax = "10000.0"))
    float ConfidenceK = 800.0f;

    /** Default merge strategy */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue|Config")
    ETruthValueMergeStrategy DefaultMergeStrategy = ETruthValueMergeStrategy::BayesianRevision;

    /** Minimum confidence threshold for truth value acceptance */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinConfidenceThreshold = 0.1f;

    /** Enable automatic decay of confidence over time */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue|Config")
    bool bEnableConfidenceDecay = false;

    /** Confidence decay rate per second */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TruthValue|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ConfidenceDecayRate = 0.001f;

    // ========================================
    // SUBSYSTEM REFERENCES
    // ========================================

    /** Reference to HypergraphMemorySystem for integration */
    UPROPERTY(BlueprintReadOnly, Category = "TruthValue|Subsystems")
    UHypergraphMemorySystem* HypergraphMemory;

    // ========================================
    // EVENTS
    // ========================================

    /** Fired when a truth value is created */
    UPROPERTY(BlueprintAssignable, Category = "TruthValue|Events")
    FOnTruthValueCreated OnTruthValueCreated;

    /** Fired when a truth value is updated */
    UPROPERTY(BlueprintAssignable, Category = "TruthValue|Events")
    FOnTruthValueUpdated OnTruthValueUpdated;

    /** Fired when a truth value is deleted */
    UPROPERTY(BlueprintAssignable, Category = "TruthValue|Events")
    FOnTruthValueDeleted OnTruthValueDeleted;

    /** Fired when an inference is performed */
    UPROPERTY(BlueprintAssignable, Category = "TruthValue|Events")
    FOnInferencePerformed OnInferencePerformed;

    /** Fired when conflicting truth values are detected */
    UPROPERTY(BlueprintAssignable, Category = "TruthValue|Events")
    FOnTruthValueConflict OnTruthValueConflict;

    // ========================================
    // PUBLIC API - TRUTH VALUE CREATION
    // ========================================

    /** Initialize the truth value system */
    UFUNCTION(BlueprintCallable, Category = "TruthValue")
    void InitializeSystem();

    /** Create a simple truth value */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Creation")
    int64 CreateSimpleTruthValue(float Strength, float Confidence);

    /** Create a count truth value */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Creation")
    int64 CreateCountTruthValue(float Strength, int32 Count);

    /** Create an evidence truth value */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Creation")
    int64 CreateEvidenceTruthValue(int32 PositiveCount, int32 NegativeCount, float Prior = 0.5f);

    /** Create an indefinite truth value */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Creation")
    int64 CreateIndefiniteTruthValue(float LowerBound, float UpperBound, float Confidence);

    /** Create a fuzzy truth value */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Creation")
    int64 CreateFuzzyTruthValue(float Membership, float Confidence, float Hedge = 1.0f);

    /** Create a probabilistic truth value */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Creation")
    int64 CreateProbabilisticTruthValue(float PeakValue, float Spread, int32 NumBins = 10);

    // ========================================
    // PUBLIC API - TRUTH VALUE ACCESS
    // ========================================

    /** Get truth value by ID */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Access")
    FTruthValue GetTruthValue(int64 TruthValueID) const;

    /** Get simple truth value (converted if necessary) */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Access")
    FSimpleTruthValue GetSimpleTruthValue(int64 TruthValueID) const;

    /** Check if truth value exists */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Access")
    bool TruthValueExists(int64 TruthValueID) const;

    /** Get strength of truth value */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Access")
    float GetStrength(int64 TruthValueID) const;

    /** Get confidence of truth value */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Access")
    float GetConfidence(int64 TruthValueID) const;

    /** Delete a truth value */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Access")
    bool DeleteTruthValue(int64 TruthValueID);

    // ========================================
    // PUBLIC API - TRUTH VALUE OPERATIONS
    // ========================================

    /** Update truth value strength */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Operations")
    void SetStrength(int64 TruthValueID, float NewStrength);

    /** Update truth value confidence */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Operations")
    void SetConfidence(int64 TruthValueID, float NewConfidence);

    /** Add evidence to an evidence truth value */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Operations")
    void AddEvidence(int64 TruthValueID, bool bPositive, int32 Count = 1);

    /** Merge two truth values using specified strategy */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Operations")
    FTruthValue MergeTruthValues(int64 TruthValueA, int64 TruthValueB, ETruthValueMergeStrategy Strategy);

    /** Revise a truth value with new evidence */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Operations")
    FTruthValue ReviseTruthValue(int64 TruthValueID, const FTruthValue& NewEvidence);

    // ========================================
    // PUBLIC API - INFERENCE
    // ========================================

    /** Perform deduction: A→B, B→C ⊢ A→C */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Inference")
    FTruthValueInferenceResult Deduction(int64 PremiseAB, int64 PremiseBC);

    /** Perform induction: A→B, A→C ⊢ B→C */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Inference")
    FTruthValueInferenceResult Induction(int64 PremiseAB, int64 PremiseAC);

    /** Perform abduction: A→B, C→B ⊢ A→C */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Inference")
    FTruthValueInferenceResult Abduction(int64 PremiseAB, int64 PremiseCB);

    /** Perform modus ponens: A→B, A ⊢ B */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Inference")
    FTruthValueInferenceResult ModusPonens(int64 ImplicationAB, int64 PremiseA);

    /** Compute conjunction: A, B ⊢ A∧B */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Inference")
    FTruthValueInferenceResult Conjunction(int64 TruthValueA, int64 TruthValueB);

    /** Compute disjunction: A, B ⊢ A∨B */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Inference")
    FTruthValueInferenceResult Disjunction(int64 TruthValueA, int64 TruthValueB);

    /** Compute negation: A ⊢ ¬A */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Inference")
    FTruthValueInferenceResult Negation(int64 TruthValueA);

    /** Compute similarity strength */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Inference")
    float ComputeSimilarity(int64 TruthValueA, int64 TruthValueB);

    // ========================================
    // PUBLIC API - STATISTICS
    // ========================================

    /** Get system statistics */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Stats")
    FTruthValueSystemStats GetStatistics() const;

    /** Get count of truth values */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Stats")
    int32 GetTruthValueCount() const;

    /** Clear all truth values */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Stats")
    void ClearAllTruthValues();

    /** Prune low-confidence truth values */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Stats")
    int32 PruneLowConfidenceValues(float ConfidenceThreshold);

    // ========================================
    // PUBLIC API - UTILITY
    // ========================================

    /** Convert strength to natural language description */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Utility")
    FString StrengthToDescription(float Strength) const;

    /** Convert confidence to natural language description */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Utility")
    FString ConfidenceToDescription(float Confidence) const;

    /** Get diagnostic report */
    UFUNCTION(BlueprintCallable, Category = "TruthValue|Utility")
    TArray<FString> GenerateDiagnosticReport() const;

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Truth value storage */
    UPROPERTY()
    TMap<int64, FTruthValue> TruthValues;

    /** Next truth value ID */
    int64 NextTruthValueID = 1;

    /** Inference counter for statistics */
    int64 InferenceCount = 0;

    /** Total inference time for statistics */
    double TotalInferenceTime = 0.0;

    /** Is system initialized */
    bool bInitialized = false;

    // ========================================
    // INTERNAL METHODS
    // ========================================

    /** Generate unique truth value ID */
    int64 GenerateTruthValueID();

    /** Discover and link subsystems */
    void DiscoverSubsystems();

    /** Apply confidence decay */
    void ApplyConfidenceDecay(float DeltaTime);

    /** Internal merge implementation */
    FSimpleTruthValue MergeSimpleTruthValues(const FSimpleTruthValue& A, const FSimpleTruthValue& B, ETruthValueMergeStrategy Strategy) const;

    /** Bayesian revision formula */
    FSimpleTruthValue BayesianRevision(const FSimpleTruthValue& Prior, const FSimpleTruthValue& Evidence) const;

    /** Dempster-Shafer combination */
    FSimpleTruthValue DempsterShaferCombination(const FSimpleTruthValue& A, const FSimpleTruthValue& B) const;

    /** Deduction formula (PLN-style) */
    FSimpleTruthValue DeductionFormula(const FSimpleTruthValue& AB, const FSimpleTruthValue& BC) const;

    /** Induction formula (PLN-style) */
    FSimpleTruthValue InductionFormula(const FSimpleTruthValue& AB, const FSimpleTruthValue& AC) const;

    /** Abduction formula (PLN-style) */
    FSimpleTruthValue AbductionFormula(const FSimpleTruthValue& AB, const FSimpleTruthValue& CB) const;

    /** Record inference for statistics */
    void RecordInference(double InferenceTimeMs);
};

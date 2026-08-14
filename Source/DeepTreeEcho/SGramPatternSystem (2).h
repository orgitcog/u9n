// SGramPatternSystem.h
// Deep Tree Echo - S-Gram (Second Power N-Gram) Pattern Recognition System
// Based on Sigurd Andersen's mathematical framework
// Copyright (c) 2026 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include <vector>
#include <map>
#include <array>
#include <cmath>
#include <functional>

/**
 * S-Gram Pattern System for Deep Tree Echo
 * 
 * Implements Sigurd Andersen's Second Power N-Grams (S-Grams) for:
 * - Cyclic pattern recognition in cognitive processing
 * - Mapping repeating decimal patterns to cognitive states
 * - Integrating with the 12-step cognitive loop architecture
 * - Providing mathematical foundation for triadic consciousness streams
 */

namespace DeepTreeEcho
{
namespace SGram
{

//=============================================================================
// CORE MATHEMATICAL STRUCTURES
//=============================================================================

/**
 * Represents a single vertex on an N-Gram circle
 * Vertices are positioned at equal angles around the circumference
 */
struct FNGramVertex
{
    int32 Index;           // 0 to (Base-1)
    float Angle;           // Radians from top (0 = top, π = bottom)
    FVector2D Position;    // Normalized 2D position on unit circle
    bool bIsZeroPoint;     // True if this is the special 0/(Base-1) point
    
    FNGramVertex() : Index(0), Angle(0.0f), bIsZeroPoint(true) {}
    
    FNGramVertex(int32 InIndex, int32 Base)
        : Index(InIndex)
        , bIsZeroPoint(InIndex == 0)
    {
        // Position vertices starting from top, going clockwise
        Angle = (2.0f * FMath::PI * InIndex) / Base;
        Position = FVector2D(FMath::Sin(Angle), -FMath::Cos(Angle));
    }
};

/**
 * Represents an edge (connection) between two vertices in an N-Gram pattern
 */
struct FNGramEdge
{
    int32 FromVertex;
    int32 ToVertex;
    int32 PatternIndex;    // Which pattern this edge belongs to
    bool bIsResolving;     // True = resolving pattern, False = informing pattern
    
    FNGramEdge() : FromVertex(0), ToVertex(0), PatternIndex(0), bIsResolving(true) {}
    
    FNGramEdge(int32 From, int32 To, int32 Pattern, bool Resolving = true)
        : FromVertex(From), ToVertex(To), PatternIndex(Pattern), bIsResolving(Resolving) {}
};

/**
 * A single repeating decimal pattern within an N-Gram
 * e.g., 1/7 in base 10 = 0.142857... creates pattern [1,4,2,8,5,7]
 */
struct FDecimalPattern
{
    int32 Numerator;
    int32 Denominator;
    TArray<int32> Digits;           // The repeating digit sequence
    TArray<FNGramEdge> Edges;       // Edges connecting consecutive digits
    int32 PatternLength;            // Length of repeating cycle
    bool bIsResolving;              // Resolving (active) vs Informing (reference)
    
    FDecimalPattern() : Numerator(0), Denominator(1), PatternLength(0), bIsResolving(true) {}
    
    // Calculate the repeating decimal pattern for Numerator/Denominator in given Base
    void Calculate(int32 Num, int32 Denom, int32 Base);
    
    // Get the pattern as a string for debugging
    FString ToString() const;
};

/**
 * An atomic N-Gram: all patterns formed by division by a single divisor in a given base
 */
struct FAtomicNGram
{
    int32 Base;                     // The numeric base (e.g., 10 for decimal)
    int32 Divisor;                  // The divisor (e.g., 7 for 1/7, 2/7, etc.)
    TArray<FNGramVertex> Vertices;  // All vertices around the circle
    TArray<FDecimalPattern> Patterns; // All patterns for this divisor
    bool bIsGray;                   // True if no repeating patterns (common factors)
    bool bIsPartialGray;            // True if some patterns don't repeat
    
    FAtomicNGram() : Base(10), Divisor(1), bIsGray(false), bIsPartialGray(false) {}
    
    // Generate all patterns for this N-Gram
    void Generate(int32 InBase, int32 InDivisor);
    
    // Check if divisor shares prime factors with base
    bool HasCommonFactors() const;
};

/**
 * A paired N-Gram: two atomic N-Grams that together cover all digits
 * The divisors are complementary: Divisor1 + Divisor2 = Base - 1
 */
struct FPairedNGram
{
    int32 Base;
    int32 Divisor1;                 // Smaller divisor
    int32 Divisor2;                 // Larger divisor (Base - 1 - Divisor1)
    FAtomicNGram NGram1;            // First atomic N-Gram
    FAtomicNGram NGram2;            // Second atomic N-Gram
    
    FPairedNGram() : Base(10), Divisor1(3), Divisor2(7) {}
    
    // Generate the paired N-Gram
    void Generate(int32 InBase);
    
    // Get all edges from both N-Grams
    TArray<FNGramEdge> GetAllEdges() const;
};

//=============================================================================
// S-GRAM (SECOND POWER N-GRAM) SYSTEM
//=============================================================================

/**
 * S-Gram: A Second Power N-Gram
 * S(n) has n² points, uses base (n²+1), with divisors n and (n²-n+1)
 * 
 * S1: 1 point,  base 2,   divisors 1 & 1
 * S2: 4 points, base 5,   divisors 2 & 3
 * S3: 9 points, base 10,  divisors 3 & 7  (Enneagram)
 * S4: 16 points, base 17, divisors 4 & 13
 * S5: 25 points, base 26, divisors 5 & 21
 * ...
 */
struct FSGram
{
    int32 Order;                    // S-Gram order (1, 2, 3, ...)
    int32 NumPoints;                // n² points
    int32 Base;                     // n² + 1
    int32 Divisor1;                 // n
    int32 Divisor2;                 // n² - n + 1
    FPairedNGram PairedNGram;       // The underlying paired N-Gram
    
    // Cognitive mapping
    TArray<int32> ResolvingSteps;   // Steps in the resolving pattern
    TArray<int32> InformingSteps;   // Steps in the informing pattern
    
    FSGram() : Order(3), NumPoints(9), Base(10), Divisor1(3), Divisor2(7) {}
    
    // Generate S-Gram of given order
    void Generate(int32 InOrder);
    
    // Get the S-Gram formula values
    static void GetSGramValues(int32 Order, int32& OutPoints, int32& OutBase, 
                               int32& OutDivisor1, int32& OutDivisor2);
    
    // Map S-Gram step to cognitive loop step
    int32 MapToCognitiveStep(int32 SGramStep, int32 CognitiveLoopSize = 12) const;
};

//=============================================================================
// COORDINATE SYSTEMS (Zero-Sum Vector Measures)
//=============================================================================

/**
 * 2M₁: 1-Dimensional Zero-Sum Coordinate System
 * Each point has coordinates (a, b) where a + b = 0
 */
struct F2M1Coordinate
{
    float A;  // Positive axis
    float B;  // Negative axis (B = -A)
    
    F2M1Coordinate() : A(0.0f), B(0.0f) {}
    F2M1Coordinate(float InA) : A(InA), B(-InA) {}
    
    float GetValue() const { return A; }
    bool IsValid() const { return FMath::IsNearlyEqual(A + B, 0.0f, 0.001f); }
};

/**
 * 3M₂: 2-Dimensional Zero-Sum Coordinate System
 * Each point has coordinates (a, b, c) where a + b + c = 0
 * Axes at 120° angles
 */
struct F3M2Coordinate
{
    float A;  // Axis A (0°)
    float B;  // Axis B (120°)
    float C;  // Axis C (240°)
    
    F3M2Coordinate() : A(0.0f), B(0.0f), C(0.0f) {}
    F3M2Coordinate(float InA, float InB) : A(InA), B(InB), C(-(InA + InB)) {}
    
    FVector2D ToCartesian() const;
    static F3M2Coordinate FromCartesian(const FVector2D& Point);
    bool IsValid() const { return FMath::IsNearlyEqual(A + B + C, 0.0f, 0.001f); }
};

/**
 * 4M₃: 3-Dimensional Zero-Sum Coordinate System (Tetrahedral)
 * Each point has coordinates (a, b, c, d) where a + b + c + d = 0
 * Maps to the 4 tensor bundles in Deep Tree Echo
 */
struct F4M3Coordinate
{
    float A;  // Vertex A
    float B;  // Vertex B
    float C;  // Vertex C
    float D;  // Vertex D
    
    F4M3Coordinate() : A(0.0f), B(0.0f), C(0.0f), D(0.0f) {}
    F4M3Coordinate(float InA, float InB, float InC) 
        : A(InA), B(InB), C(InC), D(-(InA + InB + InC)) {}
    
    FVector ToCartesian() const;
    static F4M3Coordinate FromCartesian(const FVector& Point);
    bool IsValid() const { return FMath::IsNearlyEqual(A + B + C + D, 0.0f, 0.001f); }
    
    // Map to tensor bundle indices
    TArray<float> ToTensorBundleWeights() const;
};

//=============================================================================
// COGNITIVE INTEGRATION
//=============================================================================

/**
 * Maps S-Gram patterns to the 3 concurrent cognitive streams
 */
struct FCognitiveStreamMapping
{
    int32 StreamIndex;              // 0, 1, or 2
    TArray<int32> AssignedSteps;    // Steps in the 12-step loop
    FSGram* SourceSGram;            // Reference to source S-Gram
    int32 PhaseOffset;              // Phase offset in degrees (0, 120, 240)
    
    FCognitiveStreamMapping() : StreamIndex(0), SourceSGram(nullptr), PhaseOffset(0) {}
    
    // Get the current step for this stream given global step
    int32 GetCurrentStep(int32 GlobalStep) const;
    
    // Check if this stream is active at given global step
    bool IsActiveAtStep(int32 GlobalStep) const;
};

/**
 * Pattern recognition result from S-Gram analysis
 */
struct FSGramPatternMatch
{
    int32 SGramOrder;               // Which S-Gram matched
    int32 PatternIndex;             // Index within the S-Gram
    float Confidence;               // Match confidence (0-1)
    TArray<int32> MatchedSequence;  // The matched digit sequence
    bool bIsResolving;              // Resolving vs Informing pattern
    
    FSGramPatternMatch() : SGramOrder(0), PatternIndex(0), Confidence(0.0f), bIsResolving(true) {}
};

//=============================================================================
// MAIN S-GRAM PATTERN SYSTEM CLASS
//=============================================================================

/**
 * USGramPatternSystem
 * 
 * Main class for S-Gram pattern recognition and cognitive integration.
 * Provides methods for:
 * - Generating S-Grams of any order
 * - Pattern matching against input sequences
 * - Mapping patterns to cognitive loop steps
 * - Integrating with the triadic consciousness architecture
 */
UCLASS(BlueprintType, Blueprintable)
class DEEPTREEECHO_API USGramPatternSystem : public UObject
{
    GENERATED_BODY()

public:
    USGramPatternSystem();
    virtual ~USGramPatternSystem();

    //-------------------------------------------------------------------------
    // INITIALIZATION
    //-------------------------------------------------------------------------
    
    /** Initialize the S-Gram system with specified maximum order */
    UFUNCTION(BlueprintCallable, Category = "SGram")
    void Initialize(int32 MaxOrder = 11);
    
    /** Reset the system to initial state */
    UFUNCTION(BlueprintCallable, Category = "SGram")
    void Reset();

    //-------------------------------------------------------------------------
    // S-GRAM GENERATION
    //-------------------------------------------------------------------------
    
    /** Generate an S-Gram of specified order */
    UFUNCTION(BlueprintCallable, Category = "SGram")
    FSGram GenerateSGram(int32 Order);
    
    /** Get a pre-generated S-Gram by order */
    UFUNCTION(BlueprintCallable, Category = "SGram")
    FSGram GetSGram(int32 Order) const;
    
    /** Generate all S-Grams up to MaxOrder */
    UFUNCTION(BlueprintCallable, Category = "SGram")
    void GenerateAllSGrams();

    //-------------------------------------------------------------------------
    // PATTERN RECOGNITION
    //-------------------------------------------------------------------------
    
    /** Match an input sequence against all S-Gram patterns */
    UFUNCTION(BlueprintCallable, Category = "SGram")
    TArray<FSGramPatternMatch> MatchPattern(const TArray<int32>& InputSequence);
    
    /** Find the best matching S-Gram for a cyclic pattern */
    UFUNCTION(BlueprintCallable, Category = "SGram")
    FSGramPatternMatch FindBestMatch(const TArray<int32>& InputSequence);
    
    /** Check if a sequence matches a specific S-Gram pattern */
    UFUNCTION(BlueprintCallable, Category = "SGram")
    bool MatchesPattern(const TArray<int32>& InputSequence, int32 SGramOrder, int32 PatternIndex);

    //-------------------------------------------------------------------------
    // COGNITIVE LOOP INTEGRATION
    //-------------------------------------------------------------------------
    
    /** Map S-Gram patterns to the 12-step cognitive loop */
    UFUNCTION(BlueprintCallable, Category = "SGram|Cognitive")
    void MapToCognitiveLoop();
    
    /** Get the cognitive stream mapping for a given stream index */
    UFUNCTION(BlueprintCallable, Category = "SGram|Cognitive")
    FCognitiveStreamMapping GetStreamMapping(int32 StreamIndex) const;
    
    /** Get the active pattern for current cognitive step */
    UFUNCTION(BlueprintCallable, Category = "SGram|Cognitive")
    int32 GetActivePatternAtStep(int32 GlobalStep, int32 StreamIndex) const;
    
    /** Advance the cognitive loop by one step */
    UFUNCTION(BlueprintCallable, Category = "SGram|Cognitive")
    void AdvanceCognitiveStep();
    
    /** Get current cognitive loop step */
    UFUNCTION(BlueprintCallable, Category = "SGram|Cognitive")
    int32 GetCurrentCognitiveStep() const { return CurrentCognitiveStep; }

    //-------------------------------------------------------------------------
    // COORDINATE SYSTEM OPERATIONS
    //-------------------------------------------------------------------------
    
    /** Convert S-Gram vertex to 3M₂ coordinate */
    UFUNCTION(BlueprintCallable, Category = "SGram|Coordinates")
    F3M2Coordinate VertexTo3M2(const FNGramVertex& Vertex) const;
    
    /** Convert cognitive state to 4M₃ tetrahedral coordinate */
    UFUNCTION(BlueprintCallable, Category = "SGram|Coordinates")
    F4M3Coordinate StateTo4M3(const TArray<float>& StreamActivations) const;
    
    /** Get the zero-sum coordinate for current cognitive state */
    UFUNCTION(BlueprintCallable, Category = "SGram|Coordinates")
    F4M3Coordinate GetCurrentStateCoordinate() const;

    //-------------------------------------------------------------------------
    // VISUALIZATION SUPPORT
    //-------------------------------------------------------------------------
    
    /** Get vertices for drawing an S-Gram */
    UFUNCTION(BlueprintCallable, Category = "SGram|Visualization")
    TArray<FVector2D> GetSGramVertices(int32 Order, float Radius = 100.0f) const;
    
    /** Get edges for drawing an S-Gram */
    UFUNCTION(BlueprintCallable, Category = "SGram|Visualization")
    TArray<TPair<FVector2D, FVector2D>> GetSGramEdges(int32 Order, float Radius = 100.0f) const;

    //-------------------------------------------------------------------------
    // EVENTS
    //-------------------------------------------------------------------------
    
    /** Called when a pattern is recognized */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPatternRecognized, const FSGramPatternMatch&, Match);
    UPROPERTY(BlueprintAssignable, Category = "SGram|Events")
    FOnPatternRecognized OnPatternRecognized;
    
    /** Called when cognitive step advances */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCognitiveStepAdvanced, int32, NewStep, int32, ActiveStream);
    UPROPERTY(BlueprintAssignable, Category = "SGram|Events")
    FOnCognitiveStepAdvanced OnCognitiveStepAdvanced;

protected:
    //-------------------------------------------------------------------------
    // INTERNAL STATE
    //-------------------------------------------------------------------------
    
    /** Maximum S-Gram order to generate */
    UPROPERTY()
    int32 MaxSGramOrder;
    
    /** Pre-generated S-Grams */
    TMap<int32, FSGram> SGrams;
    
    /** Cognitive stream mappings */
    TArray<FCognitiveStreamMapping> StreamMappings;
    
    /** Current step in the 12-step cognitive loop */
    UPROPERTY()
    int32 CurrentCognitiveStep;
    
    /** Stream activations for current state */
    TArray<float> StreamActivations;

    //-------------------------------------------------------------------------
    // INTERNAL METHODS
    //-------------------------------------------------------------------------
    
    /** Calculate repeating decimal pattern */
    TArray<int32> CalculateRepeatingDecimal(int32 Numerator, int32 Denominator, int32 Base);
    
    /** Find greatest common divisor */
    int32 GCD(int32 A, int32 B) const;
    
    /** Check if two numbers are coprime */
    bool AreCoprime(int32 A, int32 B) const;
    
    /** Get prime factors of a number */
    TArray<int32> GetPrimeFactors(int32 N) const;
};

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

namespace SGramUtils
{
    /** Calculate S-Gram order from base */
    inline int32 BaseToOrder(int32 Base)
    {
        // Base = n² + 1, so n = sqrt(Base - 1)
        return FMath::RoundToInt(FMath::Sqrt(static_cast<float>(Base - 1)));
    }
    
    /** Calculate base from S-Gram order */
    inline int32 OrderToBase(int32 Order)
    {
        return Order * Order + 1;
    }
    
    /** Calculate divisor pair from order */
    inline void OrderToDivisors(int32 Order, int32& OutDiv1, int32& OutDiv2)
    {
        OutDiv1 = Order;
        OutDiv2 = Order * Order - Order + 1;
    }
    
    /** Check if this is a "pattern of 2" S-Gram (S2, S5, S8, S11, ...) */
    inline bool HasPatternOfTwo(int32 Order)
    {
        return (Order - 2) % 3 == 0;
    }
    
    /** Get the triad step group for a given step (0-11) */
    inline int32 GetTriadGroup(int32 Step)
    {
        // Groups: {1,5,9}=0, {2,6,10}=1, {3,7,11}=2, {4,8,12}=3
        return Step % 4;
    }
    
    /** Get the stream index for a given cognitive step */
    inline int32 GetStreamForStep(int32 Step)
    {
        // Streams are phased 4 steps apart
        // Stream 0: steps 0, 4, 8
        // Stream 1: steps 1, 5, 9
        // Stream 2: steps 2, 6, 10
        // Integration: steps 3, 7, 11
        return Step % 4;
    }
}

} // namespace SGram
} // namespace DeepTreeEcho

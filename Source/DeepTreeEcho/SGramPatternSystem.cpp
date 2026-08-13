// SGramPatternSystem.cpp
// Deep Tree Echo - S-Gram (Second Power N-Gram) Pattern Recognition System Implementation
// Based on Sigurd Andersen's mathematical framework
// Copyright (c) 2026 Deep Tree Echo Project

#include "SGramPatternSystem.h"

namespace DeepTreeEcho
{
namespace SGram
{

//=============================================================================
// FDecimalPattern Implementation
//=============================================================================

void FDecimalPattern::Calculate(int32 Num, int32 Denom, int32 Base)
{
    Numerator = Num;
    Denominator = Denom;
    Digits.Empty();
    Edges.Empty();
    
    if (Denom == 0) return;
    
    // Normalize numerator
    int32 Remainder = Num % Denom;
    if (Remainder == 0)
    {
        // No repeating pattern (terminates)
        PatternLength = 0;
        return;
    }
    
    // Track remainders to detect cycle
    TMap<int32, int32> RemainderPositions;
    TArray<int32> TempDigits;
    
    int32 Position = 0;
    while (Remainder != 0 && !RemainderPositions.Contains(Remainder))
    {
        RemainderPositions.Add(Remainder, Position);
        Remainder *= Base;
        int32 Digit = Remainder / Denom;
        TempDigits.Add(Digit);
        Remainder = Remainder % Denom;
        Position++;
    }
    
    if (Remainder == 0)
    {
        // Terminates, no repeating pattern
        PatternLength = 0;
        return;
    }
    
    // Extract the repeating portion
    int32 CycleStart = RemainderPositions[Remainder];
    for (int32 i = CycleStart; i < TempDigits.Num(); i++)
    {
        Digits.Add(TempDigits[i]);
    }
    
    PatternLength = Digits.Num();
    
    // Create edges for the pattern
    for (int32 i = 0; i < PatternLength; i++)
    {
        int32 FromVertex = Digits[i];
        int32 ToVertex = Digits[(i + 1) % PatternLength];
        Edges.Add(FNGramEdge(FromVertex, ToVertex, 0, bIsResolving));
    }
}

FString FDecimalPattern::ToString() const
{
    FString Result = FString::Printf(TEXT("%d/%d = 0."), Numerator, Denominator);
    for (int32 Digit : Digits)
    {
        Result += FString::Printf(TEXT("%d"), Digit);
    }
    Result += TEXT("...");
    return Result;
}

//=============================================================================
// FAtomicNGram Implementation
//=============================================================================

void FAtomicNGram::Generate(int32 InBase, int32 InDivisor)
{
    Base = InBase;
    Divisor = InDivisor;
    Vertices.Empty();
    Patterns.Empty();
    
    // Generate vertices
    for (int32 i = 0; i < Base; i++)
    {
        Vertices.Add(FNGramVertex(i, Base));
    }
    
    // Check for common factors
    bIsGray = HasCommonFactors();
    bIsPartialGray = false;
    
    if (bIsGray)
    {
        return; // No patterns to generate
    }
    
    // Generate patterns for all numerators
    TSet<int32> UsedDigits;
    for (int32 Num = 1; Num < Divisor; Num++)
    {
        // Skip if this numerator's pattern was already covered
        bool bAlreadyCovered = false;
        for (const FDecimalPattern& Existing : Patterns)
        {
            if (Existing.Digits.Num() > 0)
            {
                for (int32 Digit : Existing.Digits)
                {
                    if (Digit == (Num * Base / Divisor) % Base)
                    {
                        bAlreadyCovered = true;
                        break;
                    }
                }
            }
            if (bAlreadyCovered) break;
        }
        
        if (!bAlreadyCovered)
        {
            FDecimalPattern Pattern;
            Pattern.bIsResolving = true;
            Pattern.Calculate(Num, Divisor, Base);
            
            if (Pattern.PatternLength > 0)
            {
                Patterns.Add(Pattern);
                for (int32 Digit : Pattern.Digits)
                {
                    UsedDigits.Add(Digit);
                }
            }
        }
    }
    
    // Check if only some patterns exist (partial gray)
    if (UsedDigits.Num() < Base - 1 && Patterns.Num() > 0)
    {
        bIsPartialGray = true;
    }
}

bool FAtomicNGram::HasCommonFactors() const
{
    // Simple GCD check
    int32 A = Base;
    int32 B = Divisor;
    while (B != 0)
    {
        int32 Temp = B;
        B = A % B;
        A = Temp;
    }
    return A > 1;
}

//=============================================================================
// FPairedNGram Implementation
//=============================================================================

void FPairedNGram::Generate(int32 InBase)
{
    Base = InBase;
    
    // For paired N-Grams, divisors are complementary
    // Find the divisor pair that covers all digits
    Divisor1 = 0;
    Divisor2 = 0;
    
    // For S-Grams, use the formula
    int32 Order = SGramUtils::BaseToOrder(Base);
    SGramUtils::OrderToDivisors(Order, Divisor1, Divisor2);
    
    NGram1.Generate(Base, Divisor1);
    NGram2.Generate(Base, Divisor2);
}

TArray<FNGramEdge> FPairedNGram::GetAllEdges() const
{
    TArray<FNGramEdge> AllEdges;
    
    for (const FDecimalPattern& Pattern : NGram1.Patterns)
    {
        AllEdges.Append(Pattern.Edges);
    }
    
    for (const FDecimalPattern& Pattern : NGram2.Patterns)
    {
        for (FNGramEdge Edge : Pattern.Edges)
        {
            Edge.bIsResolving = false; // Mark as informing pattern
            AllEdges.Add(Edge);
        }
    }
    
    return AllEdges;
}

//=============================================================================
// FSGram Implementation
//=============================================================================

void FSGram::Generate(int32 InOrder)
{
    Order = InOrder;
    GetSGramValues(Order, NumPoints, Base, Divisor1, Divisor2);
    
    PairedNGram.Generate(Base);
    
    // Extract resolving and informing steps
    ResolvingSteps.Empty();
    InformingSteps.Empty();
    
    for (const FDecimalPattern& Pattern : PairedNGram.NGram1.Patterns)
    {
        ResolvingSteps.Append(Pattern.Digits);
    }
    
    for (const FDecimalPattern& Pattern : PairedNGram.NGram2.Patterns)
    {
        InformingSteps.Append(Pattern.Digits);
    }
}

void FSGram::GetSGramValues(int32 InOrder, int32& OutPoints, int32& OutBase, 
                            int32& OutDivisor1, int32& OutDivisor2)
{
    OutPoints = InOrder * InOrder;
    OutBase = OutPoints + 1;
    OutDivisor1 = InOrder;
    OutDivisor2 = OutPoints - InOrder + 1;
}

int32 FSGram::MapToCognitiveStep(int32 SGramStep, int32 CognitiveLoopSize) const
{
    // Map S-Gram step to cognitive loop step
    // S3 (Enneagram) maps directly to 9 of the 12 steps
    // Other S-Grams need scaling
    
    if (NumPoints == 0) return 0;
    
    float Scale = static_cast<float>(CognitiveLoopSize) / NumPoints;
    return FMath::RoundToInt(SGramStep * Scale) % CognitiveLoopSize;
}

//=============================================================================
// F3M2Coordinate Implementation
//=============================================================================

FVector2D F3M2Coordinate::ToCartesian() const
{
    // Convert 3M₂ (a,b,c) where a+b+c=0 to Cartesian (x,y)
    // Using 120° axes
    const float Sqrt3Over2 = 0.866025403784f;
    
    float X = A - 0.5f * B - 0.5f * C;
    float Y = Sqrt3Over2 * B - Sqrt3Over2 * C;
    
    return FVector2D(X, Y);
}

F3M2Coordinate F3M2Coordinate::FromCartesian(const FVector2D& Point)
{
    // Convert Cartesian to 3M₂
    const float Sqrt3Over2 = 0.866025403784f;
    const float OneOverSqrt3 = 0.577350269189f;
    
    float A = Point.X;
    float B = -0.5f * Point.X + Sqrt3Over2 * Point.Y;
    float C = -(A + B);
    
    return F3M2Coordinate(A, B);
}

//=============================================================================
// F4M3Coordinate Implementation
//=============================================================================

FVector F4M3Coordinate::ToCartesian() const
{
    // Convert 4M₃ tetrahedral coordinates to Cartesian
    // Vertices of regular tetrahedron centered at origin
    const float Sqrt2Over3 = 0.816496580928f;
    const float Sqrt6Over3 = 0.816496580928f;
    const float OneOver3 = 0.333333333333f;
    
    // Tetrahedral vertex positions
    FVector VA(1.0f, 0.0f, -1.0f / FMath::Sqrt(2.0f));
    FVector VB(-1.0f, 0.0f, -1.0f / FMath::Sqrt(2.0f));
    FVector VC(0.0f, 1.0f, 1.0f / FMath::Sqrt(2.0f));
    FVector VD(0.0f, -1.0f, 1.0f / FMath::Sqrt(2.0f));
    
    // Barycentric interpolation
    return A * VA + B * VB + C * VC + D * VD;
}

F4M3Coordinate F4M3Coordinate::FromCartesian(const FVector& Point)
{
    // Convert Cartesian to 4M₃ (inverse barycentric)
    // This is approximate for points not on the tetrahedron surface
    
    FVector VA(1.0f, 0.0f, -1.0f / FMath::Sqrt(2.0f));
    FVector VB(-1.0f, 0.0f, -1.0f / FMath::Sqrt(2.0f));
    FVector VC(0.0f, 1.0f, 1.0f / FMath::Sqrt(2.0f));
    FVector VD(0.0f, -1.0f, 1.0f / FMath::Sqrt(2.0f));
    
    // Use distances to vertices as weights
    float DA = FVector::Dist(Point, VA);
    float DB = FVector::Dist(Point, VB);
    float DC = FVector::Dist(Point, VC);
    float DD = FVector::Dist(Point, VD);
    
    float Total = DA + DB + DC + DD;
    if (Total < 0.0001f) return F4M3Coordinate();
    
    // Invert distances (closer = higher weight)
    float WA = (Total - DA) / Total;
    float WB = (Total - DB) / Total;
    float WC = (Total - DC) / Total;
    
    // Normalize to sum to zero
    float Sum = WA + WB + WC;
    float Offset = Sum / 4.0f;
    
    return F4M3Coordinate(WA - Offset, WB - Offset, WC - Offset);
}

TArray<float> F4M3Coordinate::ToTensorBundleWeights() const
{
    // Convert to 4 tensor bundle weights (normalized positive values)
    float MinVal = FMath::Min(FMath::Min(A, B), FMath::Min(C, D));
    
    TArray<float> Weights;
    Weights.Add(A - MinVal);
    Weights.Add(B - MinVal);
    Weights.Add(C - MinVal);
    Weights.Add(D - MinVal);
    
    // Normalize
    float Sum = Weights[0] + Weights[1] + Weights[2] + Weights[3];
    if (Sum > 0.0001f)
    {
        for (float& W : Weights)
        {
            W /= Sum;
        }
    }
    
    return Weights;
}

//=============================================================================
// FCognitiveStreamMapping Implementation
//=============================================================================

int32 FCognitiveStreamMapping::GetCurrentStep(int32 GlobalStep) const
{
    // Apply phase offset and wrap
    int32 LocalStep = (GlobalStep - PhaseOffset / 30) % 12;
    if (LocalStep < 0) LocalStep += 12;
    return LocalStep;
}

bool FCognitiveStreamMapping::IsActiveAtStep(int32 GlobalStep) const
{
    // Check if this stream is active at the given global step
    for (int32 Step : AssignedSteps)
    {
        if (Step == GlobalStep % 12)
        {
            return true;
        }
    }
    return false;
}

//=============================================================================
// USGramPatternSystem Implementation
//=============================================================================

USGramPatternSystem::USGramPatternSystem()
    : MaxSGramOrder(11)
    , CurrentCognitiveStep(0)
{
    StreamActivations.SetNum(3);
    for (float& Activation : StreamActivations)
    {
        Activation = 0.0f;
    }
}

USGramPatternSystem::~USGramPatternSystem()
{
}

void USGramPatternSystem::Initialize(int32 MaxOrder)
{
    MaxSGramOrder = MaxOrder;
    GenerateAllSGrams();
    MapToCognitiveLoop();
}

void USGramPatternSystem::Reset()
{
    CurrentCognitiveStep = 0;
    for (float& Activation : StreamActivations)
    {
        Activation = 0.0f;
    }
}

FSGram USGramPatternSystem::GenerateSGram(int32 Order)
{
    FSGram NewSGram;
    NewSGram.Generate(Order);
    SGrams.Add(Order, NewSGram);
    return NewSGram;
}

FSGram USGramPatternSystem::GetSGram(int32 Order) const
{
    if (const FSGram* Found = SGrams.Find(Order))
    {
        return *Found;
    }
    return FSGram();
}

void USGramPatternSystem::GenerateAllSGrams()
{
    SGrams.Empty();
    for (int32 Order = 1; Order <= MaxSGramOrder; Order++)
    {
        GenerateSGram(Order);
    }
}

TArray<FSGramPatternMatch> USGramPatternSystem::MatchPattern(const TArray<int32>& InputSequence)
{
    TArray<FSGramPatternMatch> Matches;
    
    if (InputSequence.Num() == 0) return Matches;
    
    // Try to match against all S-Gram patterns
    for (const auto& Pair : SGrams)
    {
        const FSGram& SGram = Pair.Value;
        
        // Check resolving patterns
        int32 PatternIdx = 0;
        for (const FDecimalPattern& Pattern : SGram.PairedNGram.NGram2.Patterns)
        {
            if (Pattern.PatternLength == 0) continue;
            
            // Check if input matches this pattern
            bool bMatches = true;
            int32 MatchCount = 0;
            
            for (int32 i = 0; i < InputSequence.Num() && i < Pattern.Digits.Num(); i++)
            {
                if (InputSequence[i] == Pattern.Digits[i])
                {
                    MatchCount++;
                }
                else
                {
                    bMatches = false;
                }
            }
            
            if (MatchCount > 0)
            {
                FSGramPatternMatch Match;
                Match.SGramOrder = SGram.Order;
                Match.PatternIndex = PatternIdx;
                Match.Confidence = static_cast<float>(MatchCount) / 
                    FMath::Max(InputSequence.Num(), Pattern.Digits.Num());
                Match.MatchedSequence = Pattern.Digits;
                Match.bIsResolving = true;
                Matches.Add(Match);
            }
            
            PatternIdx++;
        }
    }
    
    // Sort by confidence
    Matches.Sort([](const FSGramPatternMatch& A, const FSGramPatternMatch& B) {
        return A.Confidence > B.Confidence;
    });
    
    return Matches;
}

FSGramPatternMatch USGramPatternSystem::FindBestMatch(const TArray<int32>& InputSequence)
{
    TArray<FSGramPatternMatch> Matches = MatchPattern(InputSequence);
    
    if (Matches.Num() > 0)
    {
        return Matches[0];
    }
    
    return FSGramPatternMatch();
}

bool USGramPatternSystem::MatchesPattern(const TArray<int32>& InputSequence, 
                                          int32 SGramOrder, int32 PatternIndex)
{
    const FSGram* SGram = SGrams.Find(SGramOrder);
    if (!SGram) return false;
    
    if (PatternIndex >= SGram->PairedNGram.NGram2.Patterns.Num()) return false;
    
    const FDecimalPattern& Pattern = SGram->PairedNGram.NGram2.Patterns[PatternIndex];
    
    if (InputSequence.Num() != Pattern.Digits.Num()) return false;
    
    for (int32 i = 0; i < InputSequence.Num(); i++)
    {
        if (InputSequence[i] != Pattern.Digits[i])
        {
            return false;
        }
    }
    
    return true;
}

void USGramPatternSystem::MapToCognitiveLoop()
{
    StreamMappings.Empty();
    StreamMappings.SetNum(3);
    
    // Map 3 streams to the 12-step cognitive loop
    // Streams are phased 120° (4 steps) apart
    
    // Stream 0 (Cerebral): Steps 0, 4, 8
    StreamMappings[0].StreamIndex = 0;
    StreamMappings[0].PhaseOffset = 0;
    StreamMappings[0].AssignedSteps = {0, 4, 8};
    
    // Stream 1 (Somatic): Steps 1, 5, 9
    StreamMappings[1].StreamIndex = 1;
    StreamMappings[1].PhaseOffset = 120;
    StreamMappings[1].AssignedSteps = {1, 5, 9};
    
    // Stream 2 (Autonomic): Steps 2, 6, 10
    StreamMappings[2].StreamIndex = 2;
    StreamMappings[2].PhaseOffset = 240;
    StreamMappings[2].AssignedSteps = {2, 6, 10};
    
    // Steps 3, 7, 11 are integration points (all streams)
    
    // Link to S3 (Enneagram) as the primary cognitive S-Gram
    if (FSGram* S3 = SGrams.Find(3))
    {
        for (FCognitiveStreamMapping& Mapping : StreamMappings)
        {
            Mapping.SourceSGram = S3;
        }
    }
}

FCognitiveStreamMapping USGramPatternSystem::GetStreamMapping(int32 StreamIndex) const
{
    if (StreamIndex >= 0 && StreamIndex < StreamMappings.Num())
    {
        return StreamMappings[StreamIndex];
    }
    return FCognitiveStreamMapping();
}

int32 USGramPatternSystem::GetActivePatternAtStep(int32 GlobalStep, int32 StreamIndex) const
{
    if (StreamIndex < 0 || StreamIndex >= StreamMappings.Num()) return -1;
    
    const FCognitiveStreamMapping& Mapping = StreamMappings[StreamIndex];
    
    if (!Mapping.IsActiveAtStep(GlobalStep)) return -1;
    
    // Map to S-Gram pattern index
    if (Mapping.SourceSGram)
    {
        int32 LocalStep = Mapping.GetCurrentStep(GlobalStep);
        return LocalStep % Mapping.SourceSGram->PairedNGram.NGram2.Patterns.Num();
    }
    
    return 0;
}

void USGramPatternSystem::AdvanceCognitiveStep()
{
    CurrentCognitiveStep = (CurrentCognitiveStep + 1) % 12;
    
    // Update stream activations
    int32 ActiveStream = SGramUtils::GetStreamForStep(CurrentCognitiveStep);
    
    for (int32 i = 0; i < 3; i++)
    {
        if (i == ActiveStream || CurrentCognitiveStep % 4 == 3)
        {
            // Active stream or integration step
            StreamActivations[i] = FMath::Min(1.0f, StreamActivations[i] + 0.3f);
        }
        else
        {
            // Decay inactive streams
            StreamActivations[i] = FMath::Max(0.0f, StreamActivations[i] - 0.1f);
        }
    }
    
    // Broadcast event
    OnCognitiveStepAdvanced.Broadcast(CurrentCognitiveStep, ActiveStream);
}

F3M2Coordinate USGramPatternSystem::VertexTo3M2(const FNGramVertex& Vertex) const
{
    // Map N-Gram vertex position to 3M₂ coordinate
    return F3M2Coordinate::FromCartesian(Vertex.Position);
}

F4M3Coordinate USGramPatternSystem::StateTo4M3(const TArray<float>& InStreamActivations) const
{
    if (InStreamActivations.Num() < 3) return F4M3Coordinate();
    
    // Map 3 stream activations to 4M₃ tetrahedral coordinate
    // Fourth coordinate is the integration state
    float Integration = (InStreamActivations[0] + InStreamActivations[1] + InStreamActivations[2]) / 3.0f;
    
    // Normalize to zero-sum
    float Mean = (InStreamActivations[0] + InStreamActivations[1] + InStreamActivations[2] + Integration) / 4.0f;
    
    return F4M3Coordinate(
        InStreamActivations[0] - Mean,
        InStreamActivations[1] - Mean,
        InStreamActivations[2] - Mean
    );
}

F4M3Coordinate USGramPatternSystem::GetCurrentStateCoordinate() const
{
    return StateTo4M3(StreamActivations);
}

TArray<FVector2D> USGramPatternSystem::GetSGramVertices(int32 Order, float Radius) const
{
    TArray<FVector2D> Vertices;
    
    const FSGram* SGram = SGrams.Find(Order);
    if (!SGram) return Vertices;
    
    for (const FNGramVertex& Vertex : SGram->PairedNGram.NGram1.Vertices)
    {
        Vertices.Add(Vertex.Position * Radius);
    }
    
    return Vertices;
}

TArray<TPair<FVector2D, FVector2D>> USGramPatternSystem::GetSGramEdges(int32 Order, float Radius) const
{
    TArray<TPair<FVector2D, FVector2D>> Edges;
    
    const FSGram* SGram = SGrams.Find(Order);
    if (!SGram) return Edges;
    
    TArray<FNGramEdge> AllEdges = SGram->PairedNGram.GetAllEdges();
    
    for (const FNGramEdge& Edge : AllEdges)
    {
        if (Edge.FromVertex < SGram->PairedNGram.NGram1.Vertices.Num() &&
            Edge.ToVertex < SGram->PairedNGram.NGram1.Vertices.Num())
        {
            FVector2D From = SGram->PairedNGram.NGram1.Vertices[Edge.FromVertex].Position * Radius;
            FVector2D To = SGram->PairedNGram.NGram1.Vertices[Edge.ToVertex].Position * Radius;
            Edges.Add(TPair<FVector2D, FVector2D>(From, To));
        }
    }
    
    return Edges;
}

TArray<int32> USGramPatternSystem::CalculateRepeatingDecimal(int32 Numerator, int32 Denominator, int32 Base)
{
    TArray<int32> Result;
    
    if (Denominator == 0) return Result;
    
    int32 Remainder = Numerator % Denominator;
    TMap<int32, int32> RemainderPositions;
    
    int32 Position = 0;
    while (Remainder != 0 && !RemainderPositions.Contains(Remainder))
    {
        RemainderPositions.Add(Remainder, Position);
        Remainder *= Base;
        Result.Add(Remainder / Denominator);
        Remainder = Remainder % Denominator;
        Position++;
    }
    
    if (Remainder != 0)
    {
        // Extract only the repeating portion
        int32 CycleStart = RemainderPositions[Remainder];
        TArray<int32> Repeating;
        for (int32 i = CycleStart; i < Result.Num(); i++)
        {
            Repeating.Add(Result[i]);
        }
        return Repeating;
    }
    
    return Result;
}

int32 USGramPatternSystem::GCD(int32 A, int32 B) const
{
    while (B != 0)
    {
        int32 Temp = B;
        B = A % B;
        A = Temp;
    }
    return A;
}

bool USGramPatternSystem::AreCoprime(int32 A, int32 B) const
{
    return GCD(A, B) == 1;
}

TArray<int32> USGramPatternSystem::GetPrimeFactors(int32 N) const
{
    TArray<int32> Factors;
    
    // Factor out 2s
    while (N % 2 == 0)
    {
        if (!Factors.Contains(2))
        {
            Factors.Add(2);
        }
        N /= 2;
    }
    
    // Factor out odd numbers
    for (int32 i = 3; i * i <= N; i += 2)
    {
        while (N % i == 0)
        {
            if (!Factors.Contains(i))
            {
                Factors.Add(i);
            }
            N /= i;
        }
    }
    
    // If N is still greater than 1, it's a prime factor
    if (N > 1)
    {
        Factors.Add(N);
    }
    
    return Factors;
}

} // namespace SGram
} // namespace DeepTreeEcho

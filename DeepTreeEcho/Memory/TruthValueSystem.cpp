// TruthValueSystem.cpp
// Feature F1.4.4: Probabilistic Truth Value System Implementation
// Copyright (c) 2025-2026 Deep Tree Echo Project

#include "TruthValueSystem.h"
#include "HypergraphMemorySystem.h"

UTruthValueSystem::UTruthValueSystem()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.1f; // Tick every 100ms
}

void UTruthValueSystem::BeginPlay()
{
    Super::BeginPlay();
    InitializeSystem();
}

void UTruthValueSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bEnableConfidenceDecay)
    {
        ApplyConfidenceDecay(DeltaTime);
    }
}

// ========================================
// INITIALIZATION
// ========================================

void UTruthValueSystem::InitializeSystem()
{
    if (bInitialized)
    {
        return;
    }

    DiscoverSubsystems();
    TruthValues.Empty();
    NextTruthValueID = 1;
    InferenceCount = 0;
    TotalInferenceTime = 0.0;
    bInitialized = true;

    UE_LOG(LogTemp, Log, TEXT("TruthValueSystem: Initialized with K=%f, DefaultConfidence=%f"), 
           ConfidenceK, DefaultConfidence);
}

void UTruthValueSystem::DiscoverSubsystems()
{
    if (AActor* Owner = GetOwner())
    {
        HypergraphMemory = Owner->FindComponentByClass<UHypergraphMemorySystem>();
        if (HypergraphMemory)
        {
            UE_LOG(LogTemp, Log, TEXT("TruthValueSystem: Found HypergraphMemorySystem"));
        }
    }
}

// ========================================
// TRUTH VALUE CREATION
// ========================================

int64 UTruthValueSystem::GenerateTruthValueID()
{
    return NextTruthValueID++;
}

int64 UTruthValueSystem::CreateSimpleTruthValue(float Strength, float Confidence)
{
    int64 ID = GenerateTruthValueID();
    
    FTruthValue TV;
    TV.Type = ETruthValueType::Simple;
    TV.Simple = FSimpleTruthValue(Strength, Confidence);
    
    TruthValues.Add(ID, TV);
    OnTruthValueCreated.Broadcast(ID, TV);
    
    return ID;
}

int64 UTruthValueSystem::CreateCountTruthValue(float Strength, int32 Count)
{
    int64 ID = GenerateTruthValueID();
    
    float Confidence = FCountTruthValue::ConfidenceFromCount(Count, ConfidenceK);
    
    FTruthValue TV;
    TV.Type = ETruthValueType::Count;
    TV.Count = FCountTruthValue(Strength, Confidence, Count);
    TV.Simple = TV.Count.ToSimple();
    
    TruthValues.Add(ID, TV);
    OnTruthValueCreated.Broadcast(ID, TV);
    
    return ID;
}

int64 UTruthValueSystem::CreateEvidenceTruthValue(int32 PositiveCount, int32 NegativeCount, float Prior)
{
    int64 ID = GenerateTruthValueID();
    
    FTruthValue TV;
    TV.Type = ETruthValueType::Evidence;
    TV.Evidence = FEvidenceTruthValue(PositiveCount, NegativeCount, Prior);
    TV.Simple = TV.Evidence.ToSimple(ConfidenceK);
    
    TruthValues.Add(ID, TV);
    OnTruthValueCreated.Broadcast(ID, TV);
    
    return ID;
}

int64 UTruthValueSystem::CreateIndefiniteTruthValue(float LowerBound, float UpperBound, float Confidence)
{
    int64 ID = GenerateTruthValueID();
    
    FTruthValue TV;
    TV.Type = ETruthValueType::Indefinite;
    TV.Indefinite = FIndefiniteTruthValue(LowerBound, UpperBound, Confidence);
    TV.Simple = TV.Indefinite.ToSimple();
    
    TruthValues.Add(ID, TV);
    OnTruthValueCreated.Broadcast(ID, TV);
    
    return ID;
}

int64 UTruthValueSystem::CreateFuzzyTruthValue(float Membership, float Confidence, float Hedge)
{
    int64 ID = GenerateTruthValueID();
    
    FTruthValue TV;
    TV.Type = ETruthValueType::Fuzzy;
    TV.Fuzzy = FFuzzyTruthValue(Membership, Confidence, Hedge);
    TV.Simple = TV.Fuzzy.ToSimple();
    
    TruthValues.Add(ID, TV);
    OnTruthValueCreated.Broadcast(ID, TV);
    
    return ID;
}

int64 UTruthValueSystem::CreateProbabilisticTruthValue(float PeakValue, float Spread, int32 NumBins)
{
    int64 ID = GenerateTruthValueID();
    
    FTruthValue TV;
    TV.Type = ETruthValueType::Probabilistic;
    TV.Probabilistic = FProbabilisticTruthValue(NumBins);
    TV.Probabilistic.InitializePeaked(PeakValue, Spread);
    TV.Simple = TV.Probabilistic.ToSimple();
    
    TruthValues.Add(ID, TV);
    OnTruthValueCreated.Broadcast(ID, TV);
    
    return ID;
}

// ========================================
// TRUTH VALUE ACCESS
// ========================================

FTruthValue UTruthValueSystem::GetTruthValue(int64 TruthValueID) const
{
    if (const FTruthValue* TV = TruthValues.Find(TruthValueID))
    {
        return *TV;
    }
    return FTruthValue();
}

FSimpleTruthValue UTruthValueSystem::GetSimpleTruthValue(int64 TruthValueID) const
{
    if (const FTruthValue* TV = TruthValues.Find(TruthValueID))
    {
        return TV->ToSimple();
    }
    return FSimpleTruthValue();
}

bool UTruthValueSystem::TruthValueExists(int64 TruthValueID) const
{
    return TruthValues.Contains(TruthValueID);
}

float UTruthValueSystem::GetStrength(int64 TruthValueID) const
{
    if (const FTruthValue* TV = TruthValues.Find(TruthValueID))
    {
        return TV->GetStrength();
    }
    return 0.0f;
}

float UTruthValueSystem::GetConfidence(int64 TruthValueID) const
{
    if (const FTruthValue* TV = TruthValues.Find(TruthValueID))
    {
        return TV->GetConfidence();
    }
    return 0.0f;
}

bool UTruthValueSystem::DeleteTruthValue(int64 TruthValueID)
{
    if (TruthValues.Remove(TruthValueID) > 0)
    {
        OnTruthValueDeleted.Broadcast(TruthValueID);
        return true;
    }
    return false;
}

// ========================================
// TRUTH VALUE OPERATIONS
// ========================================

void UTruthValueSystem::SetStrength(int64 TruthValueID, float NewStrength)
{
    if (FTruthValue* TV = TruthValues.Find(TruthValueID))
    {
        NewStrength = FMath::Clamp(NewStrength, 0.0f, 1.0f);
        
        switch (TV->Type)
        {
        case ETruthValueType::Simple:
            TV->Simple.Strength = NewStrength;
            break;
        case ETruthValueType::Count:
            TV->Count.Strength = NewStrength;
            TV->Simple = TV->Count.ToSimple();
            break;
        case ETruthValueType::Fuzzy:
            TV->Fuzzy.Membership = NewStrength;
            TV->Simple = TV->Fuzzy.ToSimple();
            break;
        default:
            TV->Simple.Strength = NewStrength;
            break;
        }
        
        OnTruthValueUpdated.Broadcast(TruthValueID, *TV);
    }
}

void UTruthValueSystem::SetConfidence(int64 TruthValueID, float NewConfidence)
{
    if (FTruthValue* TV = TruthValues.Find(TruthValueID))
    {
        NewConfidence = FMath::Clamp(NewConfidence, 0.0f, 1.0f);
        
        switch (TV->Type)
        {
        case ETruthValueType::Simple:
            TV->Simple.Confidence = NewConfidence;
            break;
        case ETruthValueType::Count:
            TV->Count.Confidence = NewConfidence;
            TV->Simple = TV->Count.ToSimple();
            break;
        case ETruthValueType::Indefinite:
            TV->Indefinite.Confidence = NewConfidence;
            TV->Simple = TV->Indefinite.ToSimple();
            break;
        case ETruthValueType::Fuzzy:
            TV->Fuzzy.Confidence = NewConfidence;
            TV->Simple = TV->Fuzzy.ToSimple();
            break;
        default:
            TV->Simple.Confidence = NewConfidence;
            break;
        }
        
        OnTruthValueUpdated.Broadcast(TruthValueID, *TV);
    }
}

void UTruthValueSystem::AddEvidence(int64 TruthValueID, bool bPositive, int32 Count)
{
    if (FTruthValue* TV = TruthValues.Find(TruthValueID))
    {
        if (TV->Type == ETruthValueType::Evidence)
        {
            if (bPositive)
            {
                TV->Evidence.AddPositiveEvidence(Count);
            }
            else
            {
                TV->Evidence.AddNegativeEvidence(Count);
            }
            TV->Simple = TV->Evidence.ToSimple(ConfidenceK);
            OnTruthValueUpdated.Broadcast(TruthValueID, *TV);
        }
        else if (TV->Type == ETruthValueType::Count)
        {
            TV->Count.Count += Count;
            TV->Count.Confidence = FCountTruthValue::ConfidenceFromCount(TV->Count.Count, ConfidenceK);
            TV->Simple = TV->Count.ToSimple();
            OnTruthValueUpdated.Broadcast(TruthValueID, *TV);
        }
    }
}

FTruthValue UTruthValueSystem::MergeTruthValues(int64 TruthValueA, int64 TruthValueB, ETruthValueMergeStrategy Strategy)
{
    FTruthValue Result;
    
    const FTruthValue* TVA = TruthValues.Find(TruthValueA);
    const FTruthValue* TVB = TruthValues.Find(TruthValueB);
    
    if (!TVA || !TVB)
    {
        return Result;
    }
    
    FSimpleTruthValue SimpleA = TVA->ToSimple();
    FSimpleTruthValue SimpleB = TVB->ToSimple();
    
    Result.Type = ETruthValueType::Simple;
    Result.Simple = MergeSimpleTruthValues(SimpleA, SimpleB, Strategy);
    
    return Result;
}

FSimpleTruthValue UTruthValueSystem::MergeSimpleTruthValues(const FSimpleTruthValue& A, const FSimpleTruthValue& B, ETruthValueMergeStrategy Strategy) const
{
    switch (Strategy)
    {
    case ETruthValueMergeStrategy::HigherConfidence:
        return (A.Confidence > B.Confidence) ? A : B;
        
    case ETruthValueMergeStrategy::LowerConfidence:
        return (A.Confidence < B.Confidence) ? A : B;
        
    case ETruthValueMergeStrategy::WeightedAverage:
        {
            float TotalWeight = A.Confidence + B.Confidence;
            if (TotalWeight < 0.0001f)
            {
                return FSimpleTruthValue((A.Strength + B.Strength) * 0.5f, 0.0f);
            }
            float Strength = (A.Strength * A.Confidence + B.Strength * B.Confidence) / TotalWeight;
            float Confidence = FMath::Max(A.Confidence, B.Confidence);
            return FSimpleTruthValue(Strength, Confidence);
        }
        
    case ETruthValueMergeStrategy::BayesianRevision:
        return BayesianRevision(A, B);
        
    case ETruthValueMergeStrategy::DempsterShafer:
        return DempsterShaferCombination(A, B);
        
    case ETruthValueMergeStrategy::MaxStrength:
        return (A.Strength > B.Strength) ? A : B;
        
    case ETruthValueMergeStrategy::MinStrength:
        return (A.Strength < B.Strength) ? A : B;
        
    default:
        return A;
    }
}

FSimpleTruthValue UTruthValueSystem::BayesianRevision(const FSimpleTruthValue& Prior, const FSimpleTruthValue& Evidence) const
{
    // PLN-style revision formula
    // Combines two truth values, giving more weight to the one with higher confidence
    
    float c1 = Prior.Confidence;
    float c2 = Evidence.Confidence;
    float s1 = Prior.Strength;
    float s2 = Evidence.Strength;
    
    // Avoid division by zero
    float denom = c1 + c2 - c1 * c2;
    if (denom < 0.0001f)
    {
        return FSimpleTruthValue((s1 + s2) * 0.5f, 0.0f);
    }
    
    // Revised strength
    float sNew = (s1 * c1 * (1.0f - c2) + s2 * c2 * (1.0f - c1) + s1 * s2 * c1 * c2) / denom;
    
    // Revised confidence (increases with combination)
    float cNew = denom;
    
    return FSimpleTruthValue(sNew, cNew);
}

FSimpleTruthValue UTruthValueSystem::DempsterShaferCombination(const FSimpleTruthValue& A, const FSimpleTruthValue& B) const
{
    // Simplified Dempster-Shafer combination
    // Treats strength as belief and (1-strength) as disbelief
    
    float bel1 = A.Strength * A.Confidence;
    float dis1 = (1.0f - A.Strength) * A.Confidence;
    float unc1 = 1.0f - A.Confidence;
    
    float bel2 = B.Strength * B.Confidence;
    float dis2 = (1.0f - B.Strength) * B.Confidence;
    float unc2 = 1.0f - B.Confidence;
    
    // Conflict factor
    float K = bel1 * dis2 + dis1 * bel2;
    
    if (K > 0.99f)
    {
        // Total conflict - return uncertain
        return FSimpleTruthValue(0.5f, 0.0f);
    }
    
    float Normalizer = 1.0f - K;
    
    // Combined belief
    float belCombined = (bel1 * bel2 + bel1 * unc2 + unc1 * bel2) / Normalizer;
    float disCombined = (dis1 * dis2 + dis1 * unc2 + unc1 * dis2) / Normalizer;
    
    float Strength = belCombined / (belCombined + disCombined + 0.0001f);
    float Confidence = 1.0f - (unc1 * unc2) / Normalizer;
    
    return FSimpleTruthValue(Strength, Confidence);
}

FTruthValue UTruthValueSystem::ReviseTruthValue(int64 TruthValueID, const FTruthValue& NewEvidence)
{
    FTruthValue Result;
    
    if (FTruthValue* TV = TruthValues.Find(TruthValueID))
    {
        FSimpleTruthValue Current = TV->ToSimple();
        FSimpleTruthValue Evidence = NewEvidence.ToSimple();
        
        Result.Type = ETruthValueType::Simple;
        Result.Simple = BayesianRevision(Current, Evidence);
        
        // Update the stored truth value
        TV->Simple = Result.Simple;
        OnTruthValueUpdated.Broadcast(TruthValueID, *TV);
    }
    
    return Result;
}

// ========================================
// INFERENCE
// ========================================

FTruthValueInferenceResult UTruthValueSystem::Deduction(int64 PremiseAB, int64 PremiseBC)
{
    double StartTime = FPlatformTime::Seconds();
    
    FTruthValueInferenceResult Result;
    Result.RuleApplied = ETruthValueInferenceRule::Deduction;
    Result.SourceIDs.Add(PremiseAB);
    Result.SourceIDs.Add(PremiseBC);
    Result.InferenceTime = FPlatformTime::Seconds();
    
    const FTruthValue* TVAB = TruthValues.Find(PremiseAB);
    const FTruthValue* TVBC = TruthValues.Find(PremiseBC);
    
    if (TVAB && TVBC)
    {
        FSimpleTruthValue AB = TVAB->ToSimple();
        FSimpleTruthValue BC = TVBC->ToSimple();
        
        Result.Result.Type = ETruthValueType::Simple;
        Result.Result.Simple = DeductionFormula(AB, BC);
        Result.InferenceConfidence = FMath::Min(AB.Confidence, BC.Confidence);
    }
    
    double EndTime = FPlatformTime::Seconds();
    RecordInference((EndTime - StartTime) * 1000.0);
    
    OnInferencePerformed.Broadcast(ETruthValueInferenceRule::Deduction, Result);
    
    return Result;
}

FSimpleTruthValue UTruthValueSystem::DeductionFormula(const FSimpleTruthValue& AB, const FSimpleTruthValue& BC) const
{
    // PLN deduction formula: P(A→C) from P(A→B) and P(B→C)
    // sAC = sAB * sBC + (1-sAB) * (sC - sBC * sB) / (1-sB)
    // Simplified: sAC ≈ sAB * sBC
    
    float sAB = AB.Strength;
    float sBC = BC.Strength;
    
    // Simple product approximation
    float sAC = sAB * sBC;
    
    // Confidence decreases through inference chain
    float cAC = AB.Confidence * BC.Confidence;
    
    return FSimpleTruthValue(sAC, cAC);
}

FTruthValueInferenceResult UTruthValueSystem::Induction(int64 PremiseAB, int64 PremiseAC)
{
    double StartTime = FPlatformTime::Seconds();
    
    FTruthValueInferenceResult Result;
    Result.RuleApplied = ETruthValueInferenceRule::Induction;
    Result.SourceIDs.Add(PremiseAB);
    Result.SourceIDs.Add(PremiseAC);
    Result.InferenceTime = FPlatformTime::Seconds();
    
    const FTruthValue* TVAB = TruthValues.Find(PremiseAB);
    const FTruthValue* TVAC = TruthValues.Find(PremiseAC);
    
    if (TVAB && TVAC)
    {
        FSimpleTruthValue AB = TVAB->ToSimple();
        FSimpleTruthValue AC = TVAC->ToSimple();
        
        Result.Result.Type = ETruthValueType::Simple;
        Result.Result.Simple = InductionFormula(AB, AC);
        Result.InferenceConfidence = FMath::Min(AB.Confidence, AC.Confidence) * 0.8f; // Induction is weaker
    }
    
    double EndTime = FPlatformTime::Seconds();
    RecordInference((EndTime - StartTime) * 1000.0);
    
    OnInferencePerformed.Broadcast(ETruthValueInferenceRule::Induction, Result);
    
    return Result;
}

FSimpleTruthValue UTruthValueSystem::InductionFormula(const FSimpleTruthValue& AB, const FSimpleTruthValue& AC) const
{
    // Induction: if A implies B and A implies C, then B might imply C
    // Weaker than deduction - based on shared antecedent
    
    float sAB = AB.Strength;
    float sAC = AC.Strength;
    
    // B→C strength based on shared relationship through A
    float sBC = sAB * sAC + (1.0f - sAB) * (1.0f - sAC);
    
    // Lower confidence for induction
    float cBC = AB.Confidence * AC.Confidence * 0.5f;
    
    return FSimpleTruthValue(sBC, cBC);
}

FTruthValueInferenceResult UTruthValueSystem::Abduction(int64 PremiseAB, int64 PremiseCB)
{
    double StartTime = FPlatformTime::Seconds();
    
    FTruthValueInferenceResult Result;
    Result.RuleApplied = ETruthValueInferenceRule::Abduction;
    Result.SourceIDs.Add(PremiseAB);
    Result.SourceIDs.Add(PremiseCB);
    Result.InferenceTime = FPlatformTime::Seconds();
    
    const FTruthValue* TVAB = TruthValues.Find(PremiseAB);
    const FTruthValue* TVCB = TruthValues.Find(PremiseCB);
    
    if (TVAB && TVCB)
    {
        FSimpleTruthValue AB = TVAB->ToSimple();
        FSimpleTruthValue CB = TVCB->ToSimple();
        
        Result.Result.Type = ETruthValueType::Simple;
        Result.Result.Simple = AbductionFormula(AB, CB);
        Result.InferenceConfidence = FMath::Min(AB.Confidence, CB.Confidence) * 0.6f; // Abduction is weakest
    }
    
    double EndTime = FPlatformTime::Seconds();
    RecordInference((EndTime - StartTime) * 1000.0);
    
    OnInferencePerformed.Broadcast(ETruthValueInferenceRule::Abduction, Result);
    
    return Result;
}

FSimpleTruthValue UTruthValueSystem::AbductionFormula(const FSimpleTruthValue& AB, const FSimpleTruthValue& CB) const
{
    // Abduction: if A implies B and C implies B, then A might relate to C
    // Weakest form of inference - based on shared consequent
    
    float sAB = AB.Strength;
    float sCB = CB.Strength;
    
    // A→C strength based on shared relationship through B
    float sAC = sAB * sCB;
    
    // Lowest confidence for abduction
    float cAC = AB.Confidence * CB.Confidence * 0.3f;
    
    return FSimpleTruthValue(sAC, cAC);
}

FTruthValueInferenceResult UTruthValueSystem::ModusPonens(int64 ImplicationAB, int64 PremiseA)
{
    double StartTime = FPlatformTime::Seconds();
    
    FTruthValueInferenceResult Result;
    Result.RuleApplied = ETruthValueInferenceRule::ModusPonens;
    Result.SourceIDs.Add(ImplicationAB);
    Result.SourceIDs.Add(PremiseA);
    Result.InferenceTime = FPlatformTime::Seconds();
    
    const FTruthValue* TVAB = TruthValues.Find(ImplicationAB);
    const FTruthValue* TVA = TruthValues.Find(PremiseA);
    
    if (TVAB && TVA)
    {
        FSimpleTruthValue AB = TVAB->ToSimple();
        FSimpleTruthValue A = TVA->ToSimple();
        
        // P(B) = P(A) * P(A→B) + P(¬A) * P(B|¬A)
        // Simplified: P(B) ≈ P(A) * P(A→B)
        float sB = A.Strength * AB.Strength;
        float cB = FMath::Min(A.Confidence, AB.Confidence);
        
        Result.Result.Type = ETruthValueType::Simple;
        Result.Result.Simple = FSimpleTruthValue(sB, cB);
        Result.InferenceConfidence = cB;
    }
    
    double EndTime = FPlatformTime::Seconds();
    RecordInference((EndTime - StartTime) * 1000.0);
    
    OnInferencePerformed.Broadcast(ETruthValueInferenceRule::ModusPonens, Result);
    
    return Result;
}

FTruthValueInferenceResult UTruthValueSystem::Conjunction(int64 TruthValueA, int64 TruthValueB)
{
    double StartTime = FPlatformTime::Seconds();
    
    FTruthValueInferenceResult Result;
    Result.RuleApplied = ETruthValueInferenceRule::Conjunction;
    Result.SourceIDs.Add(TruthValueA);
    Result.SourceIDs.Add(TruthValueB);
    Result.InferenceTime = FPlatformTime::Seconds();
    
    const FTruthValue* TVA = TruthValues.Find(TruthValueA);
    const FTruthValue* TVB = TruthValues.Find(TruthValueB);
    
    if (TVA && TVB)
    {
        FSimpleTruthValue A = TVA->ToSimple();
        FSimpleTruthValue B = TVB->ToSimple();
        
        // P(A∧B) = P(A) * P(B) (assuming independence)
        float sAB = A.Strength * B.Strength;
        float cAB = FMath::Min(A.Confidence, B.Confidence);
        
        Result.Result.Type = ETruthValueType::Simple;
        Result.Result.Simple = FSimpleTruthValue(sAB, cAB);
        Result.InferenceConfidence = cAB;
    }
    
    double EndTime = FPlatformTime::Seconds();
    RecordInference((EndTime - StartTime) * 1000.0);
    
    OnInferencePerformed.Broadcast(ETruthValueInferenceRule::Conjunction, Result);
    
    return Result;
}

FTruthValueInferenceResult UTruthValueSystem::Disjunction(int64 TruthValueA, int64 TruthValueB)
{
    double StartTime = FPlatformTime::Seconds();
    
    FTruthValueInferenceResult Result;
    Result.RuleApplied = ETruthValueInferenceRule::Disjunction;
    Result.SourceIDs.Add(TruthValueA);
    Result.SourceIDs.Add(TruthValueB);
    Result.InferenceTime = FPlatformTime::Seconds();
    
    const FTruthValue* TVA = TruthValues.Find(TruthValueA);
    const FTruthValue* TVB = TruthValues.Find(TruthValueB);
    
    if (TVA && TVB)
    {
        FSimpleTruthValue A = TVA->ToSimple();
        FSimpleTruthValue B = TVB->ToSimple();
        
        // P(A∨B) = P(A) + P(B) - P(A) * P(B) (assuming independence)
        float sAB = A.Strength + B.Strength - A.Strength * B.Strength;
        float cAB = FMath::Min(A.Confidence, B.Confidence);
        
        Result.Result.Type = ETruthValueType::Simple;
        Result.Result.Simple = FSimpleTruthValue(sAB, cAB);
        Result.InferenceConfidence = cAB;
    }
    
    double EndTime = FPlatformTime::Seconds();
    RecordInference((EndTime - StartTime) * 1000.0);
    
    OnInferencePerformed.Broadcast(ETruthValueInferenceRule::Disjunction, Result);
    
    return Result;
}

FTruthValueInferenceResult UTruthValueSystem::Negation(int64 TruthValueA)
{
    double StartTime = FPlatformTime::Seconds();
    
    FTruthValueInferenceResult Result;
    Result.RuleApplied = ETruthValueInferenceRule::Negation;
    Result.SourceIDs.Add(TruthValueA);
    Result.InferenceTime = FPlatformTime::Seconds();
    
    const FTruthValue* TVA = TruthValues.Find(TruthValueA);
    
    if (TVA)
    {
        FSimpleTruthValue A = TVA->ToSimple();
        
        // P(¬A) = 1 - P(A)
        float sNotA = 1.0f - A.Strength;
        float cNotA = A.Confidence; // Confidence preserved
        
        Result.Result.Type = ETruthValueType::Simple;
        Result.Result.Simple = FSimpleTruthValue(sNotA, cNotA);
        Result.InferenceConfidence = cNotA;
    }
    
    double EndTime = FPlatformTime::Seconds();
    RecordInference((EndTime - StartTime) * 1000.0);
    
    OnInferencePerformed.Broadcast(ETruthValueInferenceRule::Negation, Result);
    
    return Result;
}

float UTruthValueSystem::ComputeSimilarity(int64 TruthValueA, int64 TruthValueB)
{
    const FTruthValue* TVA = TruthValues.Find(TruthValueA);
    const FTruthValue* TVB = TruthValues.Find(TruthValueB);
    
    if (!TVA || !TVB)
    {
        return 0.0f;
    }
    
    FSimpleTruthValue A = TVA->ToSimple();
    FSimpleTruthValue B = TVB->ToSimple();
    
    // Similarity based on distance in (strength, confidence) space
    float StrengthDiff = FMath::Abs(A.Strength - B.Strength);
    float ConfidenceDiff = FMath::Abs(A.Confidence - B.Confidence);
    
    float Distance = FMath::Sqrt(StrengthDiff * StrengthDiff + ConfidenceDiff * ConfidenceDiff);
    float MaxDistance = FMath::Sqrt(2.0f); // Diagonal of unit square
    
    return 1.0f - (Distance / MaxDistance);
}

// ========================================
// STATISTICS
// ========================================

FTruthValueSystemStats UTruthValueSystem::GetStatistics() const
{
    FTruthValueSystemStats Stats;
    Stats.TotalTruthValues = TruthValues.Num();
    Stats.TotalInferences = InferenceCount;
    
    if (InferenceCount > 0)
    {
        Stats.AverageInferenceTimeMs = static_cast<float>(TotalInferenceTime / static_cast<double>(InferenceCount));
    }
    
    float TotalStrength = 0.0f;
    float TotalConfidence = 0.0f;
    
    for (const auto& Pair : TruthValues)
    {
        const FTruthValue& TV = Pair.Value;
        TotalStrength += TV.GetStrength();
        TotalConfidence += TV.GetConfidence();
        
        int32& TypeCount = Stats.CountByType.FindOrAdd(TV.Type);
        TypeCount++;
    }
    
    if (Stats.TotalTruthValues > 0)
    {
        Stats.AverageStrength = TotalStrength / static_cast<float>(Stats.TotalTruthValues);
        Stats.AverageConfidence = TotalConfidence / static_cast<float>(Stats.TotalTruthValues);
    }
    
    return Stats;
}

int32 UTruthValueSystem::GetTruthValueCount() const
{
    return TruthValues.Num();
}

void UTruthValueSystem::ClearAllTruthValues()
{
    TruthValues.Empty();
    NextTruthValueID = 1;
    InferenceCount = 0;
    TotalInferenceTime = 0.0;
    
    UE_LOG(LogTemp, Log, TEXT("TruthValueSystem: Cleared all truth values"));
}

int32 UTruthValueSystem::PruneLowConfidenceValues(float ConfidenceThreshold)
{
    TArray<int64> ToRemove;
    
    for (const auto& Pair : TruthValues)
    {
        if (Pair.Value.GetConfidence() < ConfidenceThreshold)
        {
            ToRemove.Add(Pair.Key);
        }
    }
    
    for (int64 ID : ToRemove)
    {
        DeleteTruthValue(ID);
    }
    
    return ToRemove.Num();
}

// ========================================
// UTILITY
// ========================================

FString UTruthValueSystem::StrengthToDescription(float Strength) const
{
    if (Strength >= 0.95f) return TEXT("Definitely True");
    if (Strength >= 0.8f) return TEXT("Very Likely True");
    if (Strength >= 0.6f) return TEXT("Probably True");
    if (Strength >= 0.4f) return TEXT("Uncertain");
    if (Strength >= 0.2f) return TEXT("Probably False");
    if (Strength >= 0.05f) return TEXT("Very Likely False");
    return TEXT("Definitely False");
}

FString UTruthValueSystem::ConfidenceToDescription(float Confidence) const
{
    if (Confidence >= 0.95f) return TEXT("Certain");
    if (Confidence >= 0.8f) return TEXT("Very Confident");
    if (Confidence >= 0.6f) return TEXT("Confident");
    if (Confidence >= 0.4f) return TEXT("Somewhat Confident");
    if (Confidence >= 0.2f) return TEXT("Uncertain");
    if (Confidence >= 0.05f) return TEXT("Very Uncertain");
    return TEXT("No Confidence");
}

TArray<FString> UTruthValueSystem::GenerateDiagnosticReport() const
{
    TArray<FString> Report;
    
    FTruthValueSystemStats Stats = GetStatistics();
    
    Report.Add(TEXT("=== Truth Value System Diagnostic Report ==="));
    Report.Add(FString::Printf(TEXT("Total Truth Values: %d"), Stats.TotalTruthValues));
    Report.Add(FString::Printf(TEXT("Average Strength: %.3f (%s)"), 
               Stats.AverageStrength, *StrengthToDescription(Stats.AverageStrength)));
    Report.Add(FString::Printf(TEXT("Average Confidence: %.3f (%s)"), 
               Stats.AverageConfidence, *ConfidenceToDescription(Stats.AverageConfidence)));
    Report.Add(FString::Printf(TEXT("Total Inferences: %lld"), Stats.TotalInferences));
    Report.Add(FString::Printf(TEXT("Average Inference Time: %.3f ms"), Stats.AverageInferenceTimeMs));
    
    Report.Add(TEXT(""));
    Report.Add(TEXT("Count by Type:"));
    for (const auto& Pair : Stats.CountByType)
    {
        FString TypeName;
        switch (Pair.Key)
        {
        case ETruthValueType::Simple: TypeName = TEXT("Simple"); break;
        case ETruthValueType::Count: TypeName = TEXT("Count"); break;
        case ETruthValueType::Indefinite: TypeName = TEXT("Indefinite"); break;
        case ETruthValueType::Fuzzy: TypeName = TEXT("Fuzzy"); break;
        case ETruthValueType::Evidence: TypeName = TEXT("Evidence"); break;
        case ETruthValueType::Probabilistic: TypeName = TEXT("Probabilistic"); break;
        default: TypeName = TEXT("Unknown"); break;
        }
        Report.Add(FString::Printf(TEXT("  %s: %d"), *TypeName, Pair.Value));
    }
    
    Report.Add(TEXT(""));
    Report.Add(FString::Printf(TEXT("Configuration:")));
    Report.Add(FString::Printf(TEXT("  Default Confidence: %.3f"), DefaultConfidence));
    Report.Add(FString::Printf(TEXT("  Confidence K: %.1f"), ConfidenceK));
    Report.Add(FString::Printf(TEXT("  Min Confidence Threshold: %.3f"), MinConfidenceThreshold));
    Report.Add(FString::Printf(TEXT("  Confidence Decay: %s (Rate: %.4f/s)"), 
               bEnableConfidenceDecay ? TEXT("Enabled") : TEXT("Disabled"), ConfidenceDecayRate));
    
    return Report;
}

// ========================================
// INTERNAL METHODS
// ========================================

void UTruthValueSystem::ApplyConfidenceDecay(float DeltaTime)
{
    float DecayAmount = ConfidenceDecayRate * DeltaTime;
    
    for (auto& Pair : TruthValues)
    {
        FTruthValue& TV = Pair.Value;
        float NewConfidence = FMath::Max(0.0f, TV.GetConfidence() - DecayAmount);
        
        if (TV.Type == ETruthValueType::Simple)
        {
            TV.Simple.Confidence = NewConfidence;
        }
    }
}

void UTruthValueSystem::RecordInference(double InferenceTimeMs)
{
    InferenceCount++;
    TotalInferenceTime += InferenceTimeMs;
}

// NeuralToSymbolicTranslator.cpp
// Neural-to-Symbolic Translation Layer Implementation
// Feature F1.1.1 - Copyright (c) 2025 Deep Tree Echo Project

#include "NeuralToSymbolicTranslator.h"
#include "NeuroSymbolicBridge.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/DateTime.h"

UNeuralToSymbolicTranslator::UNeuralToSymbolicTranslator()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 1.0f; // Update metrics every second
}

void UNeuralToSymbolicTranslator::BeginPlay()
{
    Super::BeginPlay();
    Initialize();
}

void UNeuralToSymbolicTranslator::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UpdateMetrics();
}

// ========================================
// INITIALIZATION
// ========================================

void UNeuralToSymbolicTranslator::Initialize()
{
    DiscoverBridge();
    ResetMetrics();
    
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("NeuralToSymbolicTranslator: Initialized with threshold=%.3f, bins=%d"),
            Config.ActivationThreshold, Config.DiscretizationBins);
    }
}

void UNeuralToSymbolicTranslator::DiscoverBridge()
{
    if (AActor* Owner = GetOwner())
    {
        Bridge = Owner->FindComponentByClass<UNeuroSymbolicBridge>();
        
        if (bEnableDebugLogging)
        {
            UE_LOG(LogTemp, Log, TEXT("NeuralToSymbolicTranslator: Bridge %s"),
                Bridge ? TEXT("found") : TEXT("not found"));
        }
    }
}

void UNeuralToSymbolicTranslator::ResetMetrics()
{
    Metrics = FTranslationMetrics();
    LatencySamples.Empty();
}

// ========================================
// CORE TRANSLATION API (Feature F1.1.1)
// ========================================

FSymbolicAtom UNeuralToSymbolicTranslator::TranslateTensor(const TArray<float>& Tensor)
{
    // Start latency measurement
    double StartTime = FPlatformTime::Seconds();
    
    FSymbolicAtom Atom;
    
    if (Tensor.Num() == 0)
    {
        RecordLatency(0.0f);
        return Atom;
    }
    
    // Find dominant activation
    float MaxActivation = -FLT_MAX;
    int32 MaxIndex = 0;
    float TotalEnergy = 0.0f;
    
    for (int32 i = 0; i < Tensor.Num(); i++)
    {
        float AbsVal = FMath::Abs(Tensor[i]);
        TotalEnergy += AbsVal;
        
        if (Tensor[i] > MaxActivation)
        {
            MaxActivation = Tensor[i];
            MaxIndex = i;
        }
    }
    
    // Create atom if activation is significant
    if (ShouldCreateAtom(MaxActivation))
    {
        Atom.AtomID = GenerateAtomID();
        Atom.AtomType = TEXT("TensorConcept");
        Atom.ActivationValue = MaxActivation;
        Atom.SourceFeatureIndex = MaxIndex;
        Atom.Confidence = CalculateConfidence(MaxActivation);
        Atom.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        
        // Add metadata
        Atom.Properties.Add(TEXT("TensorDimension"), FString::FromInt(Tensor.Num()));
        Atom.Properties.Add(TEXT("TotalEnergy"), FString::SanitizeFloat(TotalEnergy));
        Atom.Properties.Add(TEXT("MaxActivation"), FString::SanitizeFloat(MaxActivation));
        Atom.Properties.Add(TEXT("DiscreteBin"), FString::FromInt(DiscretizeActivation(MaxActivation)));
        
        Metrics.TotalAtomsCreated++;
    }
    
    // Record latency
    double EndTime = FPlatformTime::Seconds();
    float LatencyMs = (float)((EndTime - StartTime) * 1000.0);
    RecordLatency(LatencyMs);
    
    Metrics.TotalTranslations++;
    
    return Atom;
}

TArray<FPredicate> UNeuralToSymbolicTranslator::TranslateActivations(const FActivationMap& Activations)
{
    double StartTime = FPlatformTime::Seconds();
    
    TArray<FPredicate> Predicates;
    
    // Create atoms from each activation vector
    TArray<FSymbolicAtom> AllAtoms;
    
    for (const auto& ActivationPair : Activations.Activations)
    {
        const FString& ActivationName = ActivationPair.Key;
        const TArray<float>& ActivationVector = ActivationPair.Value;
        
        // Get confidence for this activation
        float Confidence = 1.0f;
        if (Activations.ConfidenceScores.Contains(ActivationName))
        {
            Confidence = Activations.ConfidenceScores[ActivationName];
        }
        
        // Create atoms from significant activations
        TArray<FSymbolicAtom> Atoms = CreateAtomsFromActivationVector(ActivationVector, ActivationName);
        
        // Apply confidence to atoms
        for (FSymbolicAtom& Atom : Atoms)
        {
            Atom.Confidence *= Confidence;
        }
        
        AllAtoms.Append(Atoms);
    }
    
    // Infer predicates from atoms
    Predicates = InferPredicatesFromAtoms(AllAtoms);
    
    // Record latency
    double EndTime = FPlatformTime::Seconds();
    float LatencyMs = (float)((EndTime - StartTime) * 1000.0);
    RecordLatency(LatencyMs);
    
    Metrics.TotalTranslations++;
    Metrics.TotalPredicatesCreated += Predicates.Num();
    
    return Predicates;
}

FSymbolicState UNeuralToSymbolicTranslator::TranslateNeuralState(const FNeuralState& State)
{
    double StartTime = FPlatformTime::Seconds();
    
    FSymbolicState SymbolicState;
    SymbolicState.StateID = FString::Printf(TEXT("SymbolicState_%s"), *State.StateID);
    SymbolicState.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    
    // Translate primary activations to atoms
    if (State.Activations.Num() > 0)
    {
        TArray<FSymbolicAtom> PrimaryAtoms = CreateAtomsFromActivationVector(State.Activations, TEXT("Primary"));
        SymbolicState.Atoms.Append(PrimaryAtoms);
    }
    
    // Translate hidden state if present
    if (State.HiddenState.Num() > 0)
    {
        TArray<FSymbolicAtom> HiddenAtoms = CreateAtomsFromActivationVector(State.HiddenState, TEXT("Hidden"));
        SymbolicState.Atoms.Append(HiddenAtoms);
    }
    
    // Translate layer activations
    for (int32 LayerIdx = 0; LayerIdx < State.LayerActivations.Num(); LayerIdx++)
    {
        const TArray<float>& LayerAct = State.LayerActivations[LayerIdx];
        FString LayerType = FString::Printf(TEXT("Layer%d"), LayerIdx);
        TArray<FSymbolicAtom> LayerAtoms = CreateAtomsFromActivationVector(LayerAct, LayerType);
        SymbolicState.Atoms.Append(LayerAtoms);
    }
    
    // Infer predicates from atoms
    SymbolicState.Predicates = InferPredicatesFromAtoms(SymbolicState.Atoms);
    
    // Propagate uncertainty
    if (Config.bPropagateUncertainty)
    {
        float PropagatedConfidence = PropagateUncertainty(State.Confidence, SymbolicState.Atoms.Num());
        SymbolicState.Confidence = PropagatedConfidence;
        
        // Apply to all atoms
        for (FSymbolicAtom& Atom : SymbolicState.Atoms)
        {
            Atom.Confidence *= PropagatedConfidence;
        }
    }
    else
    {
        SymbolicState.Confidence = State.Confidence;
    }
    
    // Record latency
    double EndTime = FPlatformTime::Seconds();
    float LatencyMs = (float)((EndTime - StartTime) * 1000.0);
    RecordLatency(LatencyMs);
    
    Metrics.TotalTranslations++;
    Metrics.TotalAtomsCreated += SymbolicState.Atoms.Num();
    Metrics.TotalPredicatesCreated += SymbolicState.Predicates.Num();
    
    return SymbolicState;
}

// ========================================
// BATCH TRANSLATION
// ========================================

TArray<FSymbolicAtom> UNeuralToSymbolicTranslator::BatchTranslateTensors(const TArray<TArray<float>>& Tensors)
{
    double StartTime = FPlatformTime::Seconds();
    
    TArray<FSymbolicAtom> AllAtoms;
    AllAtoms.Reserve(Tensors.Num());
    
    if (Config.bEnableBatchProcessing)
    {
        // Process in batches for efficiency
        int32 BatchCount = FMath::CeilToInt((float)Tensors.Num() / Config.BatchSize);
        
        for (int32 BatchIdx = 0; BatchIdx < BatchCount; BatchIdx++)
        {
            int32 BatchStart = BatchIdx * Config.BatchSize;
            int32 BatchEnd = FMath::Min(BatchStart + Config.BatchSize, Tensors.Num());
            
            for (int32 i = BatchStart; i < BatchEnd; i++)
            {
                FSymbolicAtom Atom = TranslateTensor(Tensors[i]);
                if (!Atom.AtomID.IsEmpty())
                {
                    AllAtoms.Add(Atom);
                }
            }
        }
        
        // Calculate batch efficiency
        double EndTime = FPlatformTime::Seconds();
        float TotalTime = (float)((EndTime - StartTime) * 1000.0);
        float SingleLatency = Metrics.AverageLatency;
        float ExpectedTime = SingleLatency * Tensors.Num();
        Metrics.BatchEfficiency = (ExpectedTime > 0.0f) ? (TotalTime / ExpectedTime) * 100.0f : 100.0f;
    }
    else
    {
        // Sequential processing
        for (const TArray<float>& Tensor : Tensors)
        {
            FSymbolicAtom Atom = TranslateTensor(Tensor);
            if (!Atom.AtomID.IsEmpty())
            {
                AllAtoms.Add(Atom);
            }
        }
    }
    
    return AllAtoms;
}

TArray<FSymbolicState> UNeuralToSymbolicTranslator::BatchTranslateStates(const TArray<FNeuralState>& States)
{
    TArray<FSymbolicState> SymbolicStates;
    SymbolicStates.Reserve(States.Num());
    
    for (const FNeuralState& State : States)
    {
        FSymbolicState SymState = TranslateNeuralState(State);
        SymbolicStates.Add(SymState);
    }
    
    return SymbolicStates;
}

// ========================================
// ATOM FACTORY
// ========================================

FSymbolicAtom UNeuralToSymbolicTranslator::CreateAtomFromActivation(float ActivationValue, int32 FeatureIndex, const FString& AtomType)
{
    FSymbolicAtom Atom;
    
    if (ShouldCreateAtom(ActivationValue))
    {
        Atom.AtomID = GenerateAtomID();
        Atom.AtomType = AtomType;
        Atom.ActivationValue = ActivationValue;
        Atom.SourceFeatureIndex = FeatureIndex;
        Atom.Confidence = CalculateConfidence(ActivationValue);
        Atom.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
        
        Atom.Properties.Add(TEXT("FeatureIndex"), FString::FromInt(FeatureIndex));
        Atom.Properties.Add(TEXT("DiscreteBin"), FString::FromInt(DiscretizeActivation(ActivationValue)));
    }
    
    return Atom;
}

TArray<FSymbolicAtom> UNeuralToSymbolicTranslator::CreateAtomsFromActivationVector(const TArray<float>& Activations, const FString& AtomType)
{
    TArray<FSymbolicAtom> Atoms;
    
    // Extract dominant features
    TArray<int32> DominantFeatures = ExtractDominantFeatures(Activations, Config.MaxAtomsPerTranslation);
    
    // Create atom for each dominant feature
    for (int32 FeatureIdx : DominantFeatures)
    {
        FSymbolicAtom Atom = CreateAtomFromActivation(Activations[FeatureIdx], FeatureIdx, AtomType);
        if (!Atom.AtomID.IsEmpty())
        {
            Atoms.Add(Atom);
        }
    }
    
    return Atoms;
}

FPredicate UNeuralToSymbolicTranslator::CreatePredicateFromAtoms(const FSymbolicAtom& Atom1, const FSymbolicAtom& Atom2, const FString& PredicateName)
{
    FPredicate Predicate;
    Predicate.PredicateID = GeneratePredicateID();
    Predicate.PredicateName = PredicateName;
    Predicate.Arguments.Add(Atom1.AtomID);
    Predicate.Arguments.Add(Atom2.AtomID);
    
    // Calculate truth value based on atom confidences
    Predicate.Confidence = (Atom1.Confidence + Atom2.Confidence) / 2.0f;
    Predicate.TruthValue = Predicate.Confidence;
    
    Predicate.SourceFeatureIndices.Add(Atom1.SourceFeatureIndex);
    Predicate.SourceFeatureIndices.Add(Atom2.SourceFeatureIndex);
    
    return Predicate;
}

// ========================================
// DISCRETIZATION
// ========================================

int32 UNeuralToSymbolicTranslator::DiscretizeActivation(float ActivationValue) const
{
    // Map activation value to discrete bin
    float NormalizedValue = FMath::Clamp((ActivationValue + 1.0f) / 2.0f, 0.0f, 1.0f);
    int32 Bin = FMath::FloorToInt(NormalizedValue * (Config.DiscretizationBins - 1));
    return FMath::Clamp(Bin, 0, Config.DiscretizationBins - 1);
}

bool UNeuralToSymbolicTranslator::ShouldCreateAtom(float ActivationValue) const
{
    return FMath::Abs(ActivationValue) >= Config.ActivationThreshold;
}

float UNeuralToSymbolicTranslator::CalculateConfidence(float ActivationValue) const
{
    // Sigmoid-like mapping for confidence
    float AbsValue = FMath::Abs(ActivationValue);
    float Confidence = AbsValue / (1.0f + AbsValue);
    return FMath::Clamp(Confidence, 0.0f, 1.0f);
}

// ========================================
// UNCERTAINTY PROPAGATION
// ========================================

float UNeuralToSymbolicTranslator::PropagateUncertainty(float NeuralConfidence, int32 FeatureCount) const
{
    if (FeatureCount <= 0)
    {
        return 0.0f;
    }
    
    // Uncertainty increases with number of features (information loss)
    float UncertaintyFactor = 1.0f / FMath::Sqrt((float)FeatureCount);
    float PropagatedConfidence = NeuralConfidence * UncertaintyFactor;
    
    return FMath::Clamp(PropagatedConfidence, 0.0f, 1.0f);
}

float UNeuralToSymbolicTranslator::CalculatePredicateUncertainty(const TArray<FSymbolicAtom>& InputAtoms) const
{
    if (InputAtoms.Num() == 0)
    {
        return 0.0f;
    }
    
    // Geometric mean of atom confidences
    float Product = 1.0f;
    for (const FSymbolicAtom& Atom : InputAtoms)
    {
        Product *= Atom.Confidence;
    }
    
    float GeometricMean = FMath::Pow(Product, 1.0f / InputAtoms.Num());
    return GeometricMean;
}

// ========================================
// METRICS
// ========================================

FTranslationMetrics UNeuralToSymbolicTranslator::GetMetrics() const
{
    return Metrics;
}

bool UNeuralToSymbolicTranslator::IsMeetingLatencyTarget() const
{
    // Target: <0.5ms per translation
    return Metrics.AverageLatency < 0.5f;
}

TArray<FString> UNeuralToSymbolicTranslator::GenerateDiagnosticReport() const
{
    TArray<FString> Report;
    
    Report.Add(TEXT("=== Neural-to-Symbolic Translator Diagnostic Report ==="));
    Report.Add(FString::Printf(TEXT("Total Translations: %lld"), Metrics.TotalTranslations));
    Report.Add(FString::Printf(TEXT("Total Atoms Created: %lld"), Metrics.TotalAtomsCreated));
    Report.Add(FString::Printf(TEXT("Total Predicates Created: %lld"), Metrics.TotalPredicatesCreated));
    Report.Add(FString::Printf(TEXT("Average Latency: %.4f ms"), Metrics.AverageLatency));
    Report.Add(FString::Printf(TEXT("Peak Latency: %.4f ms"), Metrics.PeakLatency));
    Report.Add(FString::Printf(TEXT("Average Atoms per Translation: %.2f"), Metrics.AverageAtomsPerTranslation));
    Report.Add(FString::Printf(TEXT("Batch Efficiency: %.1f%%"), Metrics.BatchEfficiency));
    Report.Add(FString::Printf(TEXT("Meeting Latency Target (<0.5ms): %s"), 
        IsMeetingLatencyTarget() ? TEXT("YES") : TEXT("NO")));
    
    Report.Add(TEXT("--- Configuration ---"));
    Report.Add(FString::Printf(TEXT("Activation Threshold: %.3f"), Config.ActivationThreshold));
    Report.Add(FString::Printf(TEXT("Confidence Threshold: %.3f"), Config.ConfidenceThreshold));
    Report.Add(FString::Printf(TEXT("Max Atoms per Translation: %d"), Config.MaxAtomsPerTranslation));
    Report.Add(FString::Printf(TEXT("Discretization Bins: %d"), Config.DiscretizationBins));
    Report.Add(FString::Printf(TEXT("Batch Processing: %s"), Config.bEnableBatchProcessing ? TEXT("Enabled") : TEXT("Disabled")));
    Report.Add(FString::Printf(TEXT("Batch Size: %d"), Config.BatchSize));
    Report.Add(FString::Printf(TEXT("Uncertainty Propagation: %s"), Config.bPropagateUncertainty ? TEXT("Enabled") : TEXT("Disabled")));
    
    Report.Add(TEXT("--- Subsystems ---"));
    Report.Add(FString::Printf(TEXT("Neuro-Symbolic Bridge: %s"), Bridge ? TEXT("Connected") : TEXT("Not Found")));
    
    return Report;
}

// ========================================
// INTERNAL METHODS
// ========================================

FString UNeuralToSymbolicTranslator::GenerateAtomID()
{
    return FString::Printf(TEXT("Atom_%d_%d"), AtomIDCounter++, FMath::Rand() % 10000);
}

FString UNeuralToSymbolicTranslator::GeneratePredicateID()
{
    return FString::Printf(TEXT("Pred_%d_%d"), PredicateIDCounter++, FMath::Rand() % 10000);
}

void UNeuralToSymbolicTranslator::RecordLatency(float LatencyMs)
{
    if (!bEnablePerformanceMonitoring)
    {
        return;
    }
    
    LatencySamples.Add(LatencyMs);
    
    // Keep only recent samples
    if (LatencySamples.Num() > MaxLatencySamples)
    {
        LatencySamples.RemoveAt(0);
    }
    
    // Update peak latency
    Metrics.PeakLatency = FMath::Max(Metrics.PeakLatency, LatencyMs);
}

void UNeuralToSymbolicTranslator::UpdateMetrics()
{
    if (LatencySamples.Num() > 0)
    {
        float Sum = 0.0f;
        for (float Sample : LatencySamples)
        {
            Sum += Sample;
        }
        Metrics.AverageLatency = Sum / LatencySamples.Num();
    }
    
    if (Metrics.TotalTranslations > 0)
    {
        Metrics.AverageAtomsPerTranslation = (float)Metrics.TotalAtomsCreated / Metrics.TotalTranslations;
    }
}

TArray<FPredicate> UNeuralToSymbolicTranslator::InferPredicatesFromAtoms(const TArray<FSymbolicAtom>& Atoms)
{
    TArray<FPredicate> Predicates;
    
    // Infer "CoActivated" predicates for atoms with similar activation values
    for (int32 i = 0; i < Atoms.Num(); i++)
    {
        for (int32 j = i + 1; j < Atoms.Num(); j++)
        {
            float ActivationDiff = FMath::Abs(Atoms[i].ActivationValue - Atoms[j].ActivationValue);
            
            // If activations are similar, create co-activation predicate
            if (ActivationDiff < 0.2f)
            {
                FPredicate Pred = CreatePredicateFromAtoms(Atoms[i], Atoms[j], TEXT("CoActivated"));
                Predicates.Add(Pred);
            }
        }
    }
    
    // Limit predicates to avoid explosion
    if (Predicates.Num() > Config.MaxAtomsPerTranslation)
    {
        // Sort by confidence and keep top predicates
        Predicates.Sort([](const FPredicate& A, const FPredicate& B) {
            return A.Confidence > B.Confidence;
        });
        Predicates.SetNum(Config.MaxAtomsPerTranslation);
    }
    
    return Predicates;
}

TArray<int32> UNeuralToSymbolicTranslator::ExtractDominantFeatures(const TArray<float>& Activations, int32 MaxFeatures) const
{
    TArray<int32> Features;
    
    // Create index-value pairs
    TArray<TPair<int32, float>> IndexedActivations;
    IndexedActivations.Reserve(Activations.Num());
    
    for (int32 i = 0; i < Activations.Num(); i++)
    {
        if (ShouldCreateAtom(Activations[i]))
        {
            IndexedActivations.Add(TPair<int32, float>(i, FMath::Abs(Activations[i])));
        }
    }
    
    // Sort by activation magnitude (descending)
    IndexedActivations.Sort([](const TPair<int32, float>& A, const TPair<int32, float>& B) {
        return A.Value > B.Value;
    });
    
    // Extract top indices
    int32 Count = FMath::Min(MaxFeatures, IndexedActivations.Num());
    for (int32 i = 0; i < Count; i++)
    {
        Features.Add(IndexedActivations[i].Key);
    }
    
    return Features;
}

void UNeuralToSymbolicTranslator::CalculateActivationStats(const TArray<float>& Activations, float& OutMean, float& OutStdDev) const
{
    if (Activations.Num() == 0)
    {
        OutMean = 0.0f;
        OutStdDev = 0.0f;
        return;
    }
    
    // Calculate mean
    float Sum = 0.0f;
    for (float Val : Activations)
    {
        Sum += Val;
    }
    OutMean = Sum / Activations.Num();
    
    // Calculate standard deviation
    float VarianceSum = 0.0f;
    for (float Val : Activations)
    {
        float Diff = Val - OutMean;
        VarianceSum += Diff * Diff;
    }
    OutStdDev = FMath::Sqrt(VarianceSum / Activations.Num());
}

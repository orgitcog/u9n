// NeuroSymbolicBridge.cpp
// Deep Tree Echo - Neuro-Symbolic Integration Bridge Implementation
// Copyright (c) 2025 Deep Tree Echo Project

#include "NeuroSymbolicBridge.h"
#include "DeepTreeEchoCore.h"
#include "CognitiveCycleManager.h"
#include "../Reservoir/ReservoirCognitiveIntegration.h"
#include "../Entelechy/EntelechyFramework.h"

UNeuroSymbolicBridge::UNeuroSymbolicBridge()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.033f; // ~30Hz
}

void UNeuroSymbolicBridge::BeginPlay()
{
    Super::BeginPlay();
    InitializeBridge();
}

void UNeuroSymbolicBridge::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bEnableAutoMaintenance)
    {
        AccumulatedTime += DeltaTime;
        
        // Maintenance every 100ms
        if (AccumulatedTime >= 0.1f)
        {
            MaintainBindings(AccumulatedTime);
            AccumulatedTime = 0.0f;
        }
    }

    // Bidirectional inference on every tick
    BidirectionalInferenceCycle(DeltaTime);
    
    // Update metrics
    UpdateMetrics();
}

void UNeuroSymbolicBridge::InitializeBridge()
{
    DiscoverSubsystems();
    ResetBindings();
    
    UE_LOG(LogTemp, Log, TEXT("NeuroSymbolicBridge: Initialized with %d dimension patterns"), NeuralPatternDimension);
}

void UNeuroSymbolicBridge::DiscoverSubsystems()
{
    if (AActor* Owner = GetOwner())
    {
        DeepTreeEchoCore = Owner->FindComponentByClass<UDeepTreeEchoCore>();
        CognitiveCycleManager = Owner->FindComponentByClass<UCognitiveCycleManager>();
        ReservoirIntegration = Owner->FindComponentByClass<UReservoirCognitiveIntegration>();
        EntelechyFramework = Owner->FindComponentByClass<UEntelechyFramework>();
        
        UE_LOG(LogTemp, Log, TEXT("NeuroSymbolicBridge: Discovered subsystems - Core:%s, Cycle:%s, Reservoir:%s, Entelechy:%s"),
            DeepTreeEchoCore ? TEXT("Yes") : TEXT("No"),
            CognitiveCycleManager ? TEXT("Yes") : TEXT("No"),
            ReservoirIntegration ? TEXT("Yes") : TEXT("No"),
            EntelechyFramework ? TEXT("Yes") : TEXT("No"));
    }
}

void UNeuroSymbolicBridge::ResetBindings()
{
    Bindings.Empty();
    BindingIDCounter = 0;
    Metrics = FNeuroSymbolicMetrics();
}

// ========================================
// NEURAL TO SYMBOLIC
// ========================================

FSymbolicRepresentation UNeuroSymbolicBridge::GroundNeuralPattern(const FNeuralActivationPattern& Pattern)
{
    FSymbolicRepresentation Result;
    Result.SymbolID = FString::Printf(TEXT("Symbol_%s"), *Pattern.PatternID);
    Result.SymbolType = TEXT("GroundedConcept");
    Result.TruthValue = Pattern.Confidence;
    Result.AttentionValue = 0.5f;
    
    // Extract dominant features from neural pattern
    if (Pattern.Activations.Num() > 0)
    {
        float MaxActivation = 0.0f;
        int32 MaxIndex = 0;
        float TotalActivation = 0.0f;
        
        for (int32 i = 0; i < Pattern.Activations.Num(); i++)
        {
            TotalActivation += Pattern.Activations[i];
            if (Pattern.Activations[i] > MaxActivation)
            {
                MaxActivation = Pattern.Activations[i];
                MaxIndex = i;
            }
        }
        
        // Store feature properties
        Result.Properties.Add(TEXT("DominantFeature"), FString::FromInt(MaxIndex));
        Result.Properties.Add(TEXT("ActivationStrength"), FString::SanitizeFloat(MaxActivation));
        Result.Properties.Add(TEXT("TotalEnergy"), FString::SanitizeFloat(TotalActivation));
        Result.Properties.Add(TEXT("PatternDimension"), FString::FromInt(Pattern.Activations.Num()));
        
        // Attention based on activation strength
        Result.AttentionValue = FMath::Clamp(MaxActivation, 0.0f, 1.0f);
    }
    
    // Add temporal context if available
    if (Pattern.TemporalContext.Num() > 0)
    {
        float TemporalEnergy = 0.0f;
        for (float Val : Pattern.TemporalContext)
        {
            TemporalEnergy += FMath::Abs(Val);
        }
        Result.Properties.Add(TEXT("TemporalEnergy"), FString::SanitizeFloat(TemporalEnergy));
    }
    
    return Result;
}

TArray<FSymbolicRepresentation> UNeuroSymbolicBridge::ExtractSymbolicEntities(const TArray<float>& NeuralActivation)
{
    TArray<FSymbolicRepresentation> Entities;
    
    if (NeuralActivation.Num() == 0)
    {
        return Entities;
    }
    
    // Segment activation into regions and extract entities
    const int32 SegmentSize = FMath::Max(1, NeuralActivation.Num() / 8);
    
    for (int32 Segment = 0; Segment < 8 && Segment * SegmentSize < NeuralActivation.Num(); Segment++)
    {
        float SegmentEnergy = 0.0f;
        float MaxVal = 0.0f;
        
        for (int32 i = Segment * SegmentSize; i < FMath::Min((Segment + 1) * SegmentSize, NeuralActivation.Num()); i++)
        {
            SegmentEnergy += FMath::Abs(NeuralActivation[i]);
            MaxVal = FMath::Max(MaxVal, NeuralActivation[i]);
        }
        
        // Only create entity if segment has significant activation
        if (SegmentEnergy > 0.1f * SegmentSize)
        {
            FSymbolicRepresentation Entity;
            Entity.SymbolID = FString::Printf(TEXT("Entity_Seg%d_%d"), Segment, FMath::RandRange(0, 9999));
            Entity.SymbolType = TEXT("ExtractedEntity");
            Entity.Properties.Add(TEXT("Segment"), FString::FromInt(Segment));
            Entity.Properties.Add(TEXT("Energy"), FString::SanitizeFloat(SegmentEnergy));
            Entity.TruthValue = FMath::Clamp(SegmentEnergy / SegmentSize, 0.0f, 1.0f);
            Entity.AttentionValue = MaxVal;
            
            Entities.Add(Entity);
        }
    }
    
    return Entities;
}

TArray<FSymbolicRepresentation> UNeuroSymbolicBridge::InferSymbolicRelations(const TArray<FNeuralActivationPattern>& Patterns)
{
    TArray<FSymbolicRepresentation> Relations;
    
    // Infer relations between pattern pairs based on similarity
    for (int32 i = 0; i < Patterns.Num(); i++)
    {
        for (int32 j = i + 1; j < Patterns.Num(); j++)
        {
            float Similarity = CalculateNeuralSimilarity(Patterns[i].Activations, Patterns[j].Activations);
            
            if (Similarity > 0.5f)
            {
                FSymbolicRepresentation Relation;
                Relation.SymbolID = FString::Printf(TEXT("Relation_%s_%s"), *Patterns[i].PatternID, *Patterns[j].PatternID);
                Relation.SymbolType = TEXT("InferredRelation");
                Relation.Properties.Add(TEXT("Subject"), Patterns[i].PatternID);
                Relation.Properties.Add(TEXT("Object"), Patterns[j].PatternID);
                Relation.Properties.Add(TEXT("RelationType"), Similarity > 0.8f ? TEXT("StrongSimilarity") : TEXT("WeakSimilarity"));
                Relation.TruthValue = Similarity;
                Relation.AttentionValue = (Patterns[i].Confidence + Patterns[j].Confidence) / 2.0f;
                
                Relation.Relations.Add(Patterns[i].PatternID);
                Relation.Relations.Add(Patterns[j].PatternID);
                
                Relations.Add(Relation);
            }
        }
    }
    
    return Relations;
}

// ========================================
// SYMBOLIC TO NEURAL
// ========================================

FNeuralActivationPattern UNeuroSymbolicBridge::EncodeSymbolicRepresentation(const FSymbolicRepresentation& Symbol)
{
    FNeuralActivationPattern Pattern;
    Pattern.PatternID = FString::Printf(TEXT("Neural_%s"), *Symbol.SymbolID);
    Pattern.Confidence = Symbol.TruthValue;
    Pattern.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    
    // Initialize activation vector
    Pattern.Activations.SetNum(NeuralPatternDimension);
    for (int32 i = 0; i < NeuralPatternDimension; i++)
    {
        Pattern.Activations[i] = 0.0f;
    }
    
    // Encode symbol type as base pattern
    uint32 TypeHash = GetTypeHash(Symbol.SymbolType);
    for (int32 i = 0; i < FMath::Min(32, NeuralPatternDimension); i++)
    {
        Pattern.Activations[i] = ((TypeHash >> i) & 1) ? 0.5f : 0.0f;
    }
    
    // Encode properties
    int32 PropIndex = 32;
    for (const auto& Prop : Symbol.Properties)
    {
        if (PropIndex >= NeuralPatternDimension) break;
        
        uint32 PropHash = GetTypeHash(Prop.Key);
        Pattern.Activations[PropIndex % NeuralPatternDimension] += (PropHash % 100) / 100.0f;
        PropIndex++;
    }
    
    // Encode attention as overall activation scaling
    for (float& Activation : Pattern.Activations)
    {
        Activation *= (0.5f + 0.5f * Symbol.AttentionValue);
    }
    
    // Normalize
    float MaxAct = 0.0f;
    for (float Act : Pattern.Activations)
    {
        MaxAct = FMath::Max(MaxAct, FMath::Abs(Act));
    }
    if (MaxAct > 0.0f)
    {
        for (float& Act : Pattern.Activations)
        {
            Act /= MaxAct;
        }
    }
    
    return Pattern;
}

TArray<float> UNeuroSymbolicBridge::GenerateNeuralContext(const TArray<FSymbolicRepresentation>& GameState)
{
    TArray<float> Context;
    Context.SetNum(NeuralPatternDimension);
    
    for (int32 i = 0; i < NeuralPatternDimension; i++)
    {
        Context[i] = 0.0f;
    }
    
    // Aggregate all symbolic representations into context
    for (const FSymbolicRepresentation& Symbol : GameState)
    {
        FNeuralActivationPattern Encoded = EncodeSymbolicRepresentation(Symbol);
        
        // Weighted addition based on attention
        for (int32 i = 0; i < FMath::Min(Encoded.Activations.Num(), NeuralPatternDimension); i++)
        {
            Context[i] += Encoded.Activations[i] * Symbol.AttentionValue;
        }
    }
    
    // Normalize context
    float MaxVal = 0.0f;
    for (float Val : Context)
    {
        MaxVal = FMath::Max(MaxVal, FMath::Abs(Val));
    }
    if (MaxVal > 0.0f)
    {
        for (float& Val : Context)
        {
            Val /= MaxVal;
        }
    }
    
    return Context;
}

TArray<float> UNeuroSymbolicBridge::CreateQueryEmbedding(const FString& SymbolicQuery)
{
    TArray<float> Embedding;
    Embedding.SetNum(NeuralPatternDimension);
    
    // Hash-based embedding of query string
    uint32 QueryHash = GetTypeHash(SymbolicQuery);
    
    for (int32 i = 0; i < NeuralPatternDimension; i++)
    {
        // Pseudo-random but deterministic embedding
        uint32 Seed = QueryHash ^ (i * 2654435761);
        Embedding[i] = (float)(Seed % 1000) / 1000.0f;
    }
    
    // Normalize
    float Norm = 0.0f;
    for (float Val : Embedding)
    {
        Norm += Val * Val;
    }
    Norm = FMath::Sqrt(Norm);
    if (Norm > 0.0f)
    {
        for (float& Val : Embedding)
        {
            Val /= Norm;
        }
    }
    
    return Embedding;
}

// ========================================
// BINDING MANAGEMENT
// ========================================

FNeuroSymbolicBinding UNeuroSymbolicBridge::CreateBinding(const FNeuralActivationPattern& Neural, const FSymbolicRepresentation& Symbolic, const FString& BindingType)
{
    FNeuroSymbolicBinding Binding;
    Binding.BindingID = GenerateBindingID();
    Binding.NeuralPattern = Neural;
    Binding.SymbolicRep = Symbolic;
    Binding.BindingType = BindingType;
    Binding.BindingStrength = FMath::Min(Neural.Confidence, Symbolic.TruthValue);
    Binding.CreationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Binding.LastActivationTime = Binding.CreationTime;
    Binding.ActivationCount = 1;
    
    // Add to bindings if under limit
    if (Bindings.Num() < MaxBindings)
    {
        Bindings.Add(Binding);
    }
    else
    {
        // Replace weakest binding
        int32 WeakestIndex = 0;
        float WeakestStrength = Bindings[0].BindingStrength;
        
        for (int32 i = 1; i < Bindings.Num(); i++)
        {
            if (Bindings[i].BindingStrength < WeakestStrength)
            {
                WeakestStrength = Bindings[i].BindingStrength;
                WeakestIndex = i;
            }
        }
        
        if (Binding.BindingStrength > WeakestStrength)
        {
            Bindings[WeakestIndex] = Binding;
        }
    }
    
    return Binding;
}

void UNeuroSymbolicBridge::StrengthenBinding(const FString& BindingID, float Amount)
{
    for (FNeuroSymbolicBinding& Binding : Bindings)
    {
        if (Binding.BindingID == BindingID)
        {
            Binding.BindingStrength = FMath::Clamp(Binding.BindingStrength + Amount, 0.0f, 1.0f);
            Binding.LastActivationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
            Binding.ActivationCount++;
            break;
        }
    }
}

TArray<FNeuroSymbolicBinding> UNeuroSymbolicBridge::FindBindingsByNeuralSimilarity(const FNeuralActivationPattern& Pattern, float Threshold)
{
    TArray<FNeuroSymbolicBinding> Results;
    
    for (const FNeuroSymbolicBinding& Binding : Bindings)
    {
        float Similarity = CalculateNeuralSimilarity(Pattern.Activations, Binding.NeuralPattern.Activations);
        if (Similarity >= Threshold)
        {
            Results.Add(Binding);
        }
    }
    
    // Sort by similarity (descending)
    Results.Sort([&Pattern, this](const FNeuroSymbolicBinding& A, const FNeuroSymbolicBinding& B)
    {
        float SimA = CalculateNeuralSimilarity(Pattern.Activations, A.NeuralPattern.Activations);
        float SimB = CalculateNeuralSimilarity(Pattern.Activations, B.NeuralPattern.Activations);
        return SimA > SimB;
    });
    
    return Results;
}

TArray<FNeuroSymbolicBinding> UNeuroSymbolicBridge::FindBindingsBySymbolicMatch(const FSymbolicRepresentation& Symbol)
{
    TArray<FNeuroSymbolicBinding> Results;
    
    for (const FNeuroSymbolicBinding& Binding : Bindings)
    {
        float Similarity = CalculateSymbolicSimilarity(Symbol, Binding.SymbolicRep);
        if (Similarity > 0.5f)
        {
            Results.Add(Binding);
        }
    }
    
    return Results;
}

TArray<FNeuroSymbolicBinding> UNeuroSymbolicBridge::GetActiveBindings() const
{
    TArray<FNeuroSymbolicBinding> Active;
    
    for (const FNeuroSymbolicBinding& Binding : Bindings)
    {
        if (Binding.BindingStrength >= MinBindingStrength)
        {
            Active.Add(Binding);
        }
    }
    
    return Active;
}

// ========================================
// INFERENCE
// ========================================

TArray<FSymbolicRepresentation> UNeuroSymbolicBridge::NeuralGuidedInference(const FNeuralActivationPattern& Pattern, const TArray<FSymbolicRepresentation>& KnowledgeBase)
{
    TArray<FSymbolicRepresentation> Inferred;
    
    // Find relevant bindings
    TArray<FNeuroSymbolicBinding> RelevantBindings = FindBindingsByNeuralSimilarity(Pattern, 0.6f);
    
    // Use bindings to guide inference over knowledge base
    for (const FSymbolicRepresentation& Knowledge : KnowledgeBase)
    {
        float Relevance = 0.0f;
        
        for (const FNeuroSymbolicBinding& Binding : RelevantBindings)
        {
            float SymSim = CalculateSymbolicSimilarity(Knowledge, Binding.SymbolicRep);
            Relevance = FMath::Max(Relevance, SymSim * Binding.BindingStrength);
        }
        
        if (Relevance > 0.3f)
        {
            FSymbolicRepresentation InferredKnowledge = Knowledge;
            InferredKnowledge.TruthValue *= Relevance;
            InferredKnowledge.AttentionValue = Relevance;
            Inferred.Add(InferredKnowledge);
        }
    }
    
    return Inferred;
}

FNeuralActivationPattern UNeuroSymbolicBridge::SymbolicConstrainedPrediction(const FSymbolicRepresentation& Constraint, const TArray<float>& CurrentState)
{
    FNeuralActivationPattern Prediction;
    Prediction.PatternID = FString::Printf(TEXT("Prediction_%s"), *Constraint.SymbolID);
    Prediction.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    
    // Encode constraint
    FNeuralActivationPattern ConstraintPattern = EncodeSymbolicRepresentation(Constraint);
    
    // Combine current state with constraint
    Prediction.Activations.SetNum(NeuralPatternDimension);
    
    for (int32 i = 0; i < NeuralPatternDimension; i++)
    {
        float StateVal = (i < CurrentState.Num()) ? CurrentState[i] : 0.0f;
        float ConstraintVal = (i < ConstraintPattern.Activations.Num()) ? ConstraintPattern.Activations[i] : 0.0f;
        
        // Weighted combination favoring constraint
        Prediction.Activations[i] = 0.3f * StateVal + 0.7f * ConstraintVal;
    }
    
    Prediction.Confidence = Constraint.TruthValue;
    
    return Prediction;
}

void UNeuroSymbolicBridge::BidirectionalInferenceCycle(float DeltaTime)
{
    // This is called every tick to maintain coherence between neural and symbolic representations
    
    // 1. Strengthen bindings that are co-activated
    for (FNeuroSymbolicBinding& Binding : Bindings)
    {
        // Hebbian-like strengthening: bindings that are frequently used together get stronger
        float TimeSinceActivation = (GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f) - Binding.LastActivationTime;
        
        if (TimeSinceActivation < 1.0f && Binding.ActivationCount > 1)
        {
            // Recent and frequent activation strengthens binding
            Binding.BindingStrength = FMath::Min(1.0f, Binding.BindingStrength + 0.01f * DeltaTime);
        }
    }
    
    // 2. Update metrics
    UpdateMetrics();
}

// ========================================
// METRICS
// ========================================

FNeuroSymbolicMetrics UNeuroSymbolicBridge::GetMetrics() const
{
    return Metrics;
}

float UNeuroSymbolicBridge::CalculateIntegrationCoherence() const
{
    if (Bindings.Num() == 0)
    {
        return 0.0f;
    }
    
    // Coherence based on average binding strength and activation patterns
    float TotalStrength = 0.0f;
    int32 ActiveCount = 0;
    
    for (const FNeuroSymbolicBinding& Binding : Bindings)
    {
        TotalStrength += Binding.BindingStrength;
        if (Binding.BindingStrength >= MinBindingStrength)
        {
            ActiveCount++;
        }
    }
    
    float AvgStrength = TotalStrength / Bindings.Num();
    float ActiveRatio = (float)ActiveCount / Bindings.Num();
    
    return (AvgStrength + ActiveRatio) / 2.0f;
}

TArray<FString> UNeuroSymbolicBridge::GenerateDiagnosticReport() const
{
    TArray<FString> Report;
    
    Report.Add(TEXT("=== NeuroSymbolicBridge Diagnostic Report ==="));
    Report.Add(FString::Printf(TEXT("Total Bindings: %d"), Bindings.Num()));
    Report.Add(FString::Printf(TEXT("Active Bindings: %d"), Metrics.ActiveBindings));
    Report.Add(FString::Printf(TEXT("Average Binding Strength: %.3f"), Metrics.AverageBindingStrength));
    Report.Add(FString::Printf(TEXT("Integration Coherence: %.3f"), Metrics.IntegrationCoherence));
    Report.Add(FString::Printf(TEXT("Neural Pattern Dimension: %d"), NeuralPatternDimension));
    Report.Add(FString::Printf(TEXT("Max Bindings: %d"), MaxBindings));
    Report.Add(FString::Printf(TEXT("Binding Decay Rate: %.3f"), BindingDecayRate));
    
    // Subsystem status
    Report.Add(TEXT("--- Subsystem Status ---"));
    Report.Add(FString::Printf(TEXT("DeepTreeEchoCore: %s"), DeepTreeEchoCore ? TEXT("Connected") : TEXT("Not Found")));
    Report.Add(FString::Printf(TEXT("CognitiveCycleManager: %s"), CognitiveCycleManager ? TEXT("Connected") : TEXT("Not Found")));
    Report.Add(FString::Printf(TEXT("ReservoirIntegration: %s"), ReservoirIntegration ? TEXT("Connected") : TEXT("Not Found")));
    Report.Add(FString::Printf(TEXT("EntelechyFramework: %s"), EntelechyFramework ? TEXT("Connected") : TEXT("Not Found")));
    
    return Report;
}

// ========================================
// PRIVATE HELPERS
// ========================================

void UNeuroSymbolicBridge::MaintainBindings(float DeltaTime)
{
    // Apply decay to all bindings
    for (FNeuroSymbolicBinding& Binding : Bindings)
    {
        Binding.BindingStrength -= BindingDecayRate * DeltaTime;
        Binding.BindingStrength = FMath::Max(0.0f, Binding.BindingStrength);
    }
    
    // Prune weak bindings
    PruneWeakBindings();
}

void UNeuroSymbolicBridge::PruneWeakBindings()
{
    Bindings.RemoveAll([this](const FNeuroSymbolicBinding& Binding)
    {
        return Binding.BindingStrength < MinBindingStrength * 0.5f;
    });
}

void UNeuroSymbolicBridge::UpdateMetrics()
{
    Metrics.TotalBindings = Bindings.Num();
    Metrics.ActiveBindings = 0;
    Metrics.AverageBindingStrength = 0.0f;
    
    if (Bindings.Num() > 0)
    {
        float TotalStrength = 0.0f;
        
        for (const FNeuroSymbolicBinding& Binding : Bindings)
        {
            TotalStrength += Binding.BindingStrength;
            if (Binding.BindingStrength >= MinBindingStrength)
            {
                Metrics.ActiveBindings++;
            }
        }
        
        Metrics.AverageBindingStrength = TotalStrength / Bindings.Num();
    }
    
    Metrics.IntegrationCoherence = CalculateIntegrationCoherence();
}

float UNeuroSymbolicBridge::CalculateNeuralSimilarity(const TArray<float>& A, const TArray<float>& B) const
{
    if (A.Num() == 0 || B.Num() == 0)
    {
        return 0.0f;
    }
    
    // Cosine similarity
    float DotProduct = 0.0f;
    float NormA = 0.0f;
    float NormB = 0.0f;
    
    int32 MinLen = FMath::Min(A.Num(), B.Num());
    
    for (int32 i = 0; i < MinLen; i++)
    {
        DotProduct += A[i] * B[i];
        NormA += A[i] * A[i];
        NormB += B[i] * B[i];
    }
    
    NormA = FMath::Sqrt(NormA);
    NormB = FMath::Sqrt(NormB);
    
    if (NormA < KINDA_SMALL_NUMBER || NormB < KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }
    
    return FMath::Clamp(DotProduct / (NormA * NormB), 0.0f, 1.0f);
}

float UNeuroSymbolicBridge::CalculateSymbolicSimilarity(const FSymbolicRepresentation& A, const FSymbolicRepresentation& B) const
{
    float Similarity = 0.0f;
    float TotalWeight = 0.0f;
    
    // Type match (weight 0.3)
    if (A.SymbolType == B.SymbolType)
    {
        Similarity += 0.3f;
    }
    TotalWeight += 0.3f;
    
    // Property overlap (weight 0.4)
    int32 CommonProps = 0;
    for (const auto& PropA : A.Properties)
    {
        if (B.Properties.Contains(PropA.Key))
        {
            CommonProps++;
        }
    }
    
    int32 TotalProps = A.Properties.Num() + B.Properties.Num();
    if (TotalProps > 0)
    {
        Similarity += 0.4f * (2.0f * CommonProps / TotalProps);
    }
    TotalWeight += 0.4f;
    
    // Relation overlap (weight 0.3)
    int32 CommonRels = 0;
    for (const FString& RelA : A.Relations)
    {
        if (B.Relations.Contains(RelA))
        {
            CommonRels++;
        }
    }
    
    int32 TotalRels = A.Relations.Num() + B.Relations.Num();
    if (TotalRels > 0)
    {
        Similarity += 0.3f * (2.0f * CommonRels / TotalRels);
    }
    TotalWeight += 0.3f;
    
    return Similarity / TotalWeight;
}

FString UNeuroSymbolicBridge::GenerateBindingID()
{
    return FString::Printf(TEXT("Binding_%d_%d"), BindingIDCounter++, FMath::RandRange(0, 9999));
}

uint32 UNeuroSymbolicBridge::HashNeuralPattern(const TArray<float>& Pattern) const
{
    uint32 Hash = 0;
    for (int32 i = 0; i < FMath::Min(16, Pattern.Num()); i++)
    {
        Hash ^= GetTypeHash(Pattern[i]) << (i % 32);
    }
    return Hash;
}

uint32 UNeuroSymbolicBridge::HashSymbolicRep(const FSymbolicRepresentation& Symbol) const
{
    return GetTypeHash(Symbol.SymbolID) ^ GetTypeHash(Symbol.SymbolType);
}

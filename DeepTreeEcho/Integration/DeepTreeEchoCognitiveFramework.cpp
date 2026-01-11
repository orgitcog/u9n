/**
 * @file DeepTreeEchoCognitiveFramework.cpp
 * @brief Implementation of the Deep Tree Echo cognitive framework
 */

#include "DeepTreeEchoCognitiveFramework.h"
#include "Math/UnrealMathUtility.h"

// ============================================================================
// OEIS A000081 Nested Shell Constants
// ============================================================================
namespace NestedShellConstants
{
    // Term counts per level: 1, 2, 4, 9, 20, 48, 115, 286, 719, 1842...
    constexpr int32 ShellTermCounts[] = {1, 2, 4, 9, 20, 48, 115, 286};
    constexpr int32 MaxShellLevels = 8;
    
    // Coupling strengths per shell level (outer to inner)
    constexpr float ShellCouplingStrengths[] = {1.0f, 0.9f, 0.8f, 0.7f, 0.6f, 0.5f, 0.4f, 0.3f};
}

// ============================================================================
// Holistic Metamodel Constants
// ============================================================================
namespace MetamodelConstants
{
    // 4 phases of self-stabilizing cycles
    constexpr int32 NumPhases = 4;
    
    // 7 steps in triad production
    constexpr int32 NumTriadSteps = 7;
    
    // 9 aspects of ennead meta-systems
    constexpr int32 NumEnneadAspects = 9;
    
    // 11 stages of evolutionary helix
    constexpr int32 NumHelixStages = 11;
    
    // Ennead aspect groupings (creativity, stability, drift)
    constexpr int32 CreativityAspects[] = {0, 1, 5};  // 1, 2, 6
    constexpr int32 StabilityAspects[] = {2, 4, 8};   // 3, 5, 9
    constexpr int32 DriftAspects[] = {3, 6, 7};       // 4, 7, 8
}

// ============================================================================
// Constructor
// ============================================================================
UDeepTreeEchoCognitiveFramework::UDeepTreeEchoCognitiveFramework()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    
    // Initialize ennead aspects
    MetamodelState.EnneadAspects.SetNum(MetamodelConstants::NumEnneadAspects);
    for (int32 i = 0; i < MetamodelConstants::NumEnneadAspects; ++i)
    {
        MetamodelState.EnneadAspects[i] = 0.5f;
    }
}

// ============================================================================
// BeginPlay
// ============================================================================
void UDeepTreeEchoCognitiveFramework::BeginPlay()
{
    Super::BeginPlay();
    
    InitializeMembranes();
    InitializeMetamodel();
    
    UE_LOG(LogTemp, Log, TEXT("DeepTreeEchoCognitiveFramework initialized"));
    UE_LOG(LogTemp, Log, TEXT("  Spectral Radius: %f"), SpectralRadius);
    UE_LOG(LogTemp, Log, TEXT("  Input Scaling: %f"), InputScaling);
    UE_LOG(LogTemp, Log, TEXT("  Leak Rate: %f"), LeakRate);
    UE_LOG(LogTemp, Log, TEXT("  Relevance Threshold: %f"), RelevanceThreshold);
}

// ============================================================================
// TickComponent
// ============================================================================
void UDeepTreeEchoCognitiveFramework::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // Update dynamic streams
    UpdateDynamicStreams(DeltaTime);
    
    // Update cross-dimensional coherence
    CognitionState.CrossDimensionalCoherence = ComputeCrossDimensionalCoherence();
    CognitionState.DominantDimension = IdentifyDominantDimension();
    
    // Process nested shells
    ProcessNestedShells();
    
    // Autognosis (self-reflection)
    if (bEnableAutognosis)
    {
        float Relevance = ComputeRelevanceRealization();
        if (Relevance >= RelevanceThreshold)
        {
            OnRelevanceRealized.Broadcast(CurrentCognitiveStep, Relevance);
        }
    }
}

// ============================================================================
// InitializeMembranes
// ============================================================================
void UDeepTreeEchoCognitiveFramework::InitializeMembranes()
{
    // Root membrane (system boundary)
    FMembraneState RootMembrane;
    RootMembrane.Type = EMembraneType::Root;
    RootMembrane.Permeability = 0.3f;
    RootMembrane.Coherence = 1.0f;
    RootMembrane.ChildMembranes = {1, 2, 3}; // Cognitive, Extension, Security
    Membranes.Add(EMembraneType::Root, RootMembrane);
    
    // Cognitive membrane (core processing)
    FMembraneState CognitiveMembrane;
    CognitiveMembrane.Type = EMembraneType::Cognitive;
    CognitiveMembrane.Permeability = 0.5f;
    CognitiveMembrane.Coherence = 1.0f;
    CognitiveMembrane.ChildMembranes = {4, 5, 6}; // Memory, Reasoning, Grammar
    Membranes.Add(EMembraneType::Cognitive, CognitiveMembrane);
    
    // Memory membrane
    FMembraneState MemoryMembrane;
    MemoryMembrane.Type = EMembraneType::Memory;
    MemoryMembrane.Permeability = 0.7f;
    MemoryMembrane.Coherence = 1.0f;
    Membranes.Add(EMembraneType::Memory, MemoryMembrane);
    
    // Reasoning membrane
    FMembraneState ReasoningMembrane;
    ReasoningMembrane.Type = EMembraneType::Reasoning;
    ReasoningMembrane.Permeability = 0.6f;
    ReasoningMembrane.Coherence = 1.0f;
    Membranes.Add(EMembraneType::Reasoning, ReasoningMembrane);
    
    // Grammar membrane
    FMembraneState GrammarMembrane;
    GrammarMembrane.Type = EMembraneType::Grammar;
    GrammarMembrane.Permeability = 0.5f;
    GrammarMembrane.Coherence = 1.0f;
    Membranes.Add(EMembraneType::Grammar, GrammarMembrane);
    
    // Extension membrane
    FMembraneState ExtensionMembrane;
    ExtensionMembrane.Type = EMembraneType::Extension;
    ExtensionMembrane.Permeability = 0.8f;
    ExtensionMembrane.Coherence = 1.0f;
    Membranes.Add(EMembraneType::Extension, ExtensionMembrane);
    
    // Security membrane
    FMembraneState SecurityMembrane;
    SecurityMembrane.Type = EMembraneType::Security;
    SecurityMembrane.Permeability = 0.2f;
    SecurityMembrane.Coherence = 1.0f;
    Membranes.Add(EMembraneType::Security, SecurityMembrane);
}

// ============================================================================
// InitializeMetamodel
// ============================================================================
void UDeepTreeEchoCognitiveFramework::InitializeMetamodel()
{
    // Initialize to primordial unity state
    MetamodelState.MonadCoherence = 1.0f;
    MetamodelState.DualTension = 0.0f;
    MetamodelState.TriadicBalance = 0.5f;
    MetamodelState.CurrentPhase = 1; // Emergence
    MetamodelState.TriadProductionStep = 1; // Initial Differentiation
    MetamodelState.EvolutionaryHelixStage = 1; // Primordial Unity
    MetamodelState.SpiralLevel = 1;
    
    // Initialize ennead aspects to balanced state
    for (int32 i = 0; i < MetamodelConstants::NumEnneadAspects; ++i)
    {
        MetamodelState.EnneadAspects[i] = 0.5f;
    }
    
    // Initialize dynamic streams
    StreamState.EntropicEnergy = 0.5f;
    StreamState.NegnentropicStability = 0.5f;
    StreamState.IdentityCoherence = 0.5f;
    StreamState.DominantStream = EDynamicStream::Identity;
}

// ============================================================================
// UpdateDynamicStreams
// ============================================================================
void UDeepTreeEchoCognitiveFramework::UpdateDynamicStreams(float DeltaTime)
{
    // Entropic stream: en-tropis → auto-vortis → auto-morphosis
    // Driven by cognitive activity and novelty
    float CognitiveActivity = (CognitionState.EmbodiedActivation + 
                               CognitionState.EnactedActivation) * 0.5f;
    StreamState.EntropicEnergy = FMath::Lerp(StreamState.EntropicEnergy, 
                                              CognitiveActivity, 
                                              DeltaTime * 0.5f);
    
    // Negnentropic stream: negen-tropis → auto-stasis → auto-poiesis
    // Driven by stability and coherence
    float StabilityFactor = (CognitionState.CrossDimensionalCoherence + 
                             MetamodelState.MonadCoherence) * 0.5f;
    StreamState.NegnentropicStability = FMath::Lerp(StreamState.NegnentropicStability,
                                                     StabilityFactor,
                                                     DeltaTime * 0.3f);
    
    // Identity stream: iden-tropis → auto-gnosis → auto-genesis
    // Driven by self-reflection and boundary definition
    float IdentityFactor = (CognitionState.EmbeddedActivation + 
                            CognitionState.ExtendedActivation) * 0.5f;
    StreamState.IdentityCoherence = FMath::Lerp(StreamState.IdentityCoherence,
                                                 IdentityFactor,
                                                 DeltaTime * 0.4f);
    
    // Determine dominant stream
    if (StreamState.EntropicEnergy > StreamState.NegnentropicStability &&
        StreamState.EntropicEnergy > StreamState.IdentityCoherence)
    {
        StreamState.DominantStream = EDynamicStream::Entropic;
    }
    else if (StreamState.NegnentropicStability > StreamState.IdentityCoherence)
    {
        StreamState.DominantStream = EDynamicStream::Negnentropic;
    }
    else
    {
        StreamState.DominantStream = EDynamicStream::Identity;
    }
}

// ============================================================================
// ComputeCrossDimensionalCoherence
// ============================================================================
float UDeepTreeEchoCognitiveFramework::ComputeCrossDimensionalCoherence() const
{
    // Compute pairwise cosine similarities between 4E dimensions
    TArray<float> Activations = {
        CognitionState.EmbodiedActivation,
        CognitionState.EmbeddedActivation,
        CognitionState.EnactedActivation,
        CognitionState.ExtendedActivation
    };
    
    float TotalCoherence = 0.0f;
    int32 PairCount = 0;
    
    for (int32 i = 0; i < 4; ++i)
    {
        for (int32 j = i + 1; j < 4; ++j)
        {
            // Simplified coherence: 1 - |difference|
            float Coherence = 1.0f - FMath::Abs(Activations[i] - Activations[j]);
            TotalCoherence += Coherence;
            ++PairCount;
        }
    }
    
    return PairCount > 0 ? TotalCoherence / PairCount : 0.0f;
}

// ============================================================================
// IdentifyDominantDimension
// ============================================================================
ECognitiveDimension UDeepTreeEchoCognitiveFramework::IdentifyDominantDimension() const
{
    float MaxActivation = CognitionState.EmbodiedActivation;
    ECognitiveDimension Dominant = ECognitiveDimension::Embodied;
    
    if (CognitionState.EmbeddedActivation > MaxActivation)
    {
        MaxActivation = CognitionState.EmbeddedActivation;
        Dominant = ECognitiveDimension::Embedded;
    }
    if (CognitionState.EnactedActivation > MaxActivation)
    {
        MaxActivation = CognitionState.EnactedActivation;
        Dominant = ECognitiveDimension::Enacted;
    }
    if (CognitionState.ExtendedActivation > MaxActivation)
    {
        Dominant = ECognitiveDimension::Extended;
    }
    
    return Dominant;
}

// ============================================================================
// ProcessSensoryInput
// ============================================================================
void UDeepTreeEchoCognitiveFramework::ProcessSensoryInput(const TArray<float>& SensoryData)
{
    // Integrate with reservoir computing
    IntegrateWithReservoir(SensoryData);
    
    // Execute cognitive step
    ExecuteCognitiveStep();
}

// ============================================================================
// ExecuteCognitiveStep
// ============================================================================
void UDeepTreeEchoCognitiveFramework::ExecuteCognitiveStep()
{
    // 12-step cognitive cycle
    // Steps 1, 7: Pivotal (relevance realization)
    // Steps 2-6: Affordance (past conditioning)
    // Steps 8-12: Salience (future anticipation)
    
    bool bIsPivotalStep = (CurrentCognitiveStep == 1 || CurrentCognitiveStep == 7);
    
    if (bIsPivotalStep)
    {
        float Relevance = ComputeRelevanceRealization();
        if (Relevance >= RelevanceThreshold)
        {
            OnRelevanceRealized.Broadcast(CurrentCognitiveStep, Relevance);
        }
    }
    
    // Advance to next step
    CurrentCognitiveStep = (CurrentCognitiveStep % 12) + 1;
    
    // Triadic synchronization at steps 4, 8, 12
    if (CurrentCognitiveStep == 4 || CurrentCognitiveStep == 8 || CurrentCognitiveStep == 12)
    {
        // Synchronize all three streams
        ProcessNestedShells();
    }
}

// ============================================================================
// ComputeRelevanceRealization
// ============================================================================
float UDeepTreeEchoCognitiveFramework::ComputeRelevanceRealization()
{
    // Relevance = f(4E coherence, metamodel coherence, stream balance)
    float Coherence4E = CognitionState.CrossDimensionalCoherence;
    float CoherenceMetamodel = MetamodelState.MonadCoherence;
    float StreamBalance = 1.0f - FMath::Abs(StreamState.EntropicEnergy - StreamState.NegnentropicStability);
    
    // Weighted combination
    float Relevance = 0.4f * Coherence4E + 0.3f * CoherenceMetamodel + 0.3f * StreamBalance;
    
    // Store in history
    FRelevanceEvent Event;
    Event.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Event.RelevanceScore = Relevance;
    Event.Context = FString::Printf(TEXT("Step %d"), CurrentCognitiveStep);
    RelevanceHistory.Add(Event);
    
    // Keep history bounded
    if (RelevanceHistory.Num() > 100)
    {
        RelevanceHistory.RemoveAt(0);
    }
    
    return Relevance;
}

// ============================================================================
// CultivateWisdom
// ============================================================================
void UDeepTreeEchoCognitiveFramework::CultivateWisdom()
{
    // Analyze relevance history for patterns
    if (RelevanceHistory.Num() < 10)
    {
        return; // Need sufficient history
    }
    
    // Compute average relevance
    float TotalRelevance = 0.0f;
    for (const FRelevanceEvent& Event : RelevanceHistory)
    {
        TotalRelevance += Event.RelevanceScore;
    }
    float AvgRelevance = TotalRelevance / RelevanceHistory.Num();
    
    // Wisdom cultivation based on sustained high relevance
    if (AvgRelevance >= RelevanceThreshold)
    {
        FString InsightType = TEXT("RelevancePattern");
        float WisdomScore = AvgRelevance;
        FString Description = FString::Printf(
            TEXT("Sustained relevance realization (avg: %.2f) over %d events"),
            AvgRelevance, RelevanceHistory.Num());
        
        OnWisdomCultivated.Broadcast(InsightType, WisdomScore, Description);
        
        // Advance metamodel
        AdvanceMetamodelPhase();
    }
}

// ============================================================================
// 4E Cognition Interface
// ============================================================================
void UDeepTreeEchoCognitiveFramework::UpdateEmbodiedDimension(float BodyTension, float Groundedness)
{
    CognitionState.EmbodiedActivation = (BodyTension + Groundedness) * 0.5f;
}

void UDeepTreeEchoCognitiveFramework::UpdateEmbeddedDimension(float EnvironmentalEngagement, const TArray<FString>& Affordances)
{
    CognitionState.EmbeddedActivation = EnvironmentalEngagement;
    // Store affordances in memory membrane
    if (FMembraneState* MemMembrane = Membranes.Find(EMembraneType::Memory))
    {
        for (const FString& Affordance : Affordances)
        {
            MemMembrane->Contents.Add(Affordance, 1.0f);
        }
    }
}

void UDeepTreeEchoCognitiveFramework::UpdateEnactedDimension(float ActionReadiness, float SensorimotorCoupling)
{
    CognitionState.EnactedActivation = (ActionReadiness + SensorimotorCoupling) * 0.5f;
}

void UDeepTreeEchoCognitiveFramework::UpdateExtendedDimension(float ToolIntegration, const TArray<FString>& ExtendedTools)
{
    CognitionState.ExtendedActivation = ToolIntegration;
    // Store tools in extension membrane
    if (FMembraneState* ExtMembrane = Membranes.Find(EMembraneType::Extension))
    {
        for (const FString& Tool : ExtendedTools)
        {
            ExtMembrane->Contents.Add(Tool, ToolIntegration);
        }
    }
}

// ============================================================================
// Membrane Interface
// ============================================================================
FMembraneState UDeepTreeEchoCognitiveFramework::GetMembraneState(EMembraneType Type) const
{
    if (const FMembraneState* State = Membranes.Find(Type))
    {
        return *State;
    }
    return FMembraneState();
}

void UDeepTreeEchoCognitiveFramework::SetMembranePermeability(EMembraneType Type, float Permeability)
{
    if (FMembraneState* State = Membranes.Find(Type))
    {
        State->Permeability = FMath::Clamp(Permeability, 0.0f, 1.0f);
    }
}

void UDeepTreeEchoCognitiveFramework::PropagateMembraneContents(EMembraneType FromMembrane, EMembraneType ToMembrane, const FString& ContentKey)
{
    FMembraneState* From = Membranes.Find(FromMembrane);
    FMembraneState* To = Membranes.Find(ToMembrane);
    
    if (From && To)
    {
        if (float* Value = From->Contents.Find(ContentKey))
        {
            // Propagate with permeability attenuation
            float PropagatedValue = *Value * From->Permeability * To->Permeability;
            To->Contents.Add(ContentKey, PropagatedValue);
        }
    }
}

// ============================================================================
// Metamodel Interface
// ============================================================================
void UDeepTreeEchoCognitiveFramework::AdvanceMetamodelPhase()
{
    int32 OldPhase = MetamodelState.CurrentPhase;
    MetamodelState.CurrentPhase = (MetamodelState.CurrentPhase % MetamodelConstants::NumPhases) + 1;
    
    // Update monad coherence based on phase
    switch (MetamodelState.CurrentPhase)
    {
        case 1: // Emergence
            MetamodelState.MonadCoherence = 0.8f;
            break;
        case 2: // Development
            MetamodelState.MonadCoherence = 0.6f;
            break;
        case 3: // Integration
            MetamodelState.MonadCoherence = 0.9f;
            break;
        case 4: // Transcendence
            MetamodelState.MonadCoherence = 1.0f;
            ProcessTriadProductionStep();
            break;
    }
}

void UDeepTreeEchoCognitiveFramework::ProcessTriadProductionStep()
{
    MetamodelState.TriadProductionStep = (MetamodelState.TriadProductionStep % MetamodelConstants::NumTriadSteps) + 1;
    
    // Update triadic balance based on step
    switch (MetamodelState.TriadProductionStep)
    {
        case 1: // Initial Differentiation
            MetamodelState.TriadicBalance = 0.3f;
            MetamodelState.DualTension = 0.2f;
            break;
        case 2: // Polar Tension
            MetamodelState.TriadicBalance = 0.2f;
            MetamodelState.DualTension = 0.8f;
            break;
        case 3: // Dynamic Interaction
            MetamodelState.TriadicBalance = 0.4f;
            MetamodelState.DualTension = 0.6f;
            break;
        case 4: // Synthetic Emergence
            MetamodelState.TriadicBalance = 0.5f;
            MetamodelState.DualTension = 0.4f;
            break;
        case 5: // Triadic Stabilization
            MetamodelState.TriadicBalance = 0.7f;
            MetamodelState.DualTension = 0.2f;
            break;
        case 6: // Recursive Elaboration
            MetamodelState.TriadicBalance = 0.8f;
            MetamodelState.DualTension = 0.1f;
            break;
        case 7: // Transcendent Integration
            MetamodelState.TriadicBalance = 1.0f;
            MetamodelState.DualTension = 0.0f;
            EvolveOnHelix();
            break;
    }
}

void UDeepTreeEchoCognitiveFramework::EvolveOnHelix()
{
    int32 OldStage = MetamodelState.EvolutionaryHelixStage;
    MetamodelState.EvolutionaryHelixStage = (MetamodelState.EvolutionaryHelixStage % MetamodelConstants::NumHelixStages) + 1;
    
    // Cosmic return (stage 11 → 1) advances spiral level
    if (OldStage == MetamodelConstants::NumHelixStages)
    {
        MetamodelState.SpiralLevel++;
        
        FString InsightType = TEXT("SpiralAscension");
        float WisdomScore = static_cast<float>(MetamodelState.SpiralLevel) / 10.0f;
        FString Description = FString::Printf(
            TEXT("Cosmic return: ascending to spiral level %d"),
            MetamodelState.SpiralLevel);
        
        OnWisdomCultivated.Broadcast(InsightType, WisdomScore, Description);
    }
}

// ============================================================================
// ProcessNestedShells
// ============================================================================
void UDeepTreeEchoCognitiveFramework::ProcessNestedShells()
{
    // OEIS A000081 nested shell structure
    // Level 1: 1 term, Level 2: 2 terms, Level 3: 4 terms, Level 4: 9 terms
    
    // Outer-to-inner propagation: parent states influence children
    // Inner-to-outer feedback: child averages feed back to parents
    
    // This is integrated with the reservoir computing system
    // through the EchobeatsReservoirDynamics component
}

// ============================================================================
// IntegrateWithReservoir
// ============================================================================
void UDeepTreeEchoCognitiveFramework::IntegrateWithReservoir(const TArray<float>& Input)
{
    // Echo state network update with leak rate
    // x(t+1) = (1 - LeakRate) * x(t) + LeakRate * tanh(W_in * u(t) + W * x(t))
    
    // This integration is handled by the UDeepTreeEchoReservoir component
    // Here we just update the cognitive state based on input magnitude
    
    if (Input.Num() > 0)
    {
        float InputMagnitude = 0.0f;
        for (float Val : Input)
        {
            InputMagnitude += FMath::Abs(Val);
        }
        InputMagnitude /= Input.Num();
        
        // Modulate 4E dimensions based on input
        CognitionState.EmbodiedActivation = FMath::Lerp(
            CognitionState.EmbodiedActivation,
            InputMagnitude,
            LeakRate);
    }
}

// ============================================================================
// Map4EToExpressionHints
// ============================================================================
TMap<FString, float> UDeepTreeEchoCognitiveFramework::Map4EToExpressionHints() const
{
    TMap<FString, float> Hints;
    
    // Map 4E state to avatar expression parameters
    Hints.Add(TEXT("BodyTension"), CognitionState.EmbodiedActivation);
    Hints.Add(TEXT("Groundedness"), CognitionState.EmbodiedActivation * 0.8f);
    Hints.Add(TEXT("EnvironmentalEngagement"), CognitionState.EmbeddedActivation);
    Hints.Add(TEXT("ActionReadiness"), CognitionState.EnactedActivation);
    Hints.Add(TEXT("ToolIntegration"), CognitionState.ExtendedActivation);
    Hints.Add(TEXT("4ECoherence"), CognitionState.CrossDimensionalCoherence);
    
    // Add metamodel-derived hints
    Hints.Add(TEXT("MonadCoherence"), MetamodelState.MonadCoherence);
    Hints.Add(TEXT("TriadicBalance"), MetamodelState.TriadicBalance);
    Hints.Add(TEXT("EvolutionaryMomentum"), 
              static_cast<float>(MetamodelState.EvolutionaryHelixStage) / MetamodelConstants::NumHelixStages);
    
    // Add stream-derived hints
    Hints.Add(TEXT("EntropicEnergy"), StreamState.EntropicEnergy);
    Hints.Add(TEXT("NegnentropicStability"), StreamState.NegnentropicStability);
    Hints.Add(TEXT("IdentityCoherence"), StreamState.IdentityCoherence);
    
    return Hints;
}

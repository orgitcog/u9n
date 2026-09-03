# Feature F1.1.1: Neural-to-Symbolic Translator - Usage Examples

## Quick Start

```cpp
// Attach the translator to your actor (alongside other DTE components)
UNeuralToSymbolicTranslator* Translator = CreateDefaultSubobject<UNeuralToSymbolicTranslator>(TEXT("NeuralTranslator"));
```

---

## 1. Basic Tensor Translation

Convert a raw activation tensor from the reservoir into a symbolic atom:

```cpp
// Get activation from the Echo State Network
TArray<float> ReservoirOutput = ReservoirComponent->GetCurrentActivation();

// Translate to symbolic representation
FSymbolicAtom Atom = Translator->TranslateTensor(ReservoirOutput);

if (!Atom.AtomID.IsEmpty())
{
    UE_LOG(LogDTE, Log, TEXT("Created atom: %s (type=%s, confidence=%.3f, feature=%d)"),
        *Atom.AtomID, *Atom.AtomType, Atom.Confidence, Atom.SourceFeatureIndex);
    
    // Use the atom in game logic
    KnowledgeBase->AddAtom(Atom);
}
```

---

## 2. Activation Map Processing

Handle multiple named activation streams (e.g., from different sensory modalities):

```cpp
FActivationMap SensoryActivations;

// Visual processing output
SensoryActivations.Activations.Add(TEXT("Visual"), VisualCortex->GetActivation());
SensoryActivations.ConfidenceScores.Add(TEXT("Visual"), 0.92f);

// Audio processing output
SensoryActivations.Activations.Add(TEXT("Audio"), AudioProcessor->GetActivation());
SensoryActivations.ConfidenceScores.Add(TEXT("Audio"), 0.78f);

// Proprioceptive output
SensoryActivations.Activations.Add(TEXT("Proprioception"), BodySchema->GetState());
SensoryActivations.ConfidenceScores.Add(TEXT("Proprioception"), 0.99f);

// Translate - produces predicates connecting atoms across modalities
TArray<FPredicate> Relations = Translator->TranslateActivations(SensoryActivations);

for (const FPredicate& Pred : Relations)
{
    UE_LOG(LogDTE, Log, TEXT("Predicate: %s(%s, %s) truth=%.3f"),
        *Pred.PredicateName,
        *Pred.Arguments[0], *Pred.Arguments[1],
        Pred.TruthValue);
}
```

---

## 3. Full Neural State Translation

Translate a complete neural state (activations + hidden state + layers):

```cpp
FNeuralState CurrentState;
CurrentState.StateID = TEXT("Frame_") + FString::FromInt(GFrameCounter);
CurrentState.Activations = Reservoir->GetOutputActivation();
CurrentState.HiddenState = Reservoir->GetHiddenState();
CurrentState.LayerActivations = Reservoir->GetAllLayerActivations();
CurrentState.Confidence = Reservoir->GetStateConfidence();

FSymbolicState SymState = Translator->TranslateNeuralState(CurrentState);

UE_LOG(LogDTE, Log, TEXT("Translated state '%s': %d atoms, %d predicates, confidence=%.3f"),
    *SymState.StateID,
    SymState.Atoms.Num(),
    SymState.Predicates.Num(),
    SymState.Confidence);

// Feed into the NeuroSymbolicBridge binding system
for (const FSymbolicAtom& Atom : SymState.Atoms)
{
    Bridge->GroundNeuralPatternToAtom(Atom);
}
```

---

## 4. Batch Processing

Process multiple tensors efficiently (e.g., from parallel reservoir streams):

```cpp
// Collect activations from all 3 consciousness streams
TArray<TArray<float>> StreamActivations;
StreamActivations.Add(PerceptionStream->GetActivation());
StreamActivations.Add(ActionStream->GetActivation());
StreamActivations.Add(SimulationStream->GetActivation());

// Batch translate (optimized for throughput)
TArray<FSymbolicAtom> AllAtoms = Translator->BatchTranslateTensors(StreamActivations);

// Or batch translate complete states
TArray<FNeuralState> HistoricalStates = MemorySystem->GetRecentStates(10);
TArray<FSymbolicState> SymbolicHistory = Translator->BatchTranslateStates(HistoricalStates);
```

---

## 5. Atom Factory - Direct Creation

Use the factory when you already know what type of atom to create:

```cpp
// Create atom from a specific feature activation
float ActivationValue = Reservoir->GetFeatureActivation(42);
FSymbolicAtom Atom = Translator->CreateAtomFromActivation(ActivationValue, 42, TEXT("VisualFeature"));

// Create multiple atoms from an activation vector
TArray<float> LayerOutput = Network->GetLayerOutput(3);
TArray<FSymbolicAtom> LayerAtoms = Translator->CreateAtomsFromActivationVector(LayerOutput, TEXT("Layer3"));

// Create a predicate connecting two atoms
FSymbolicAtom SubjectAtom = Translator->CreateAtomFromActivation(0.9f, 0, TEXT("Subject"));
FSymbolicAtom ObjectAtom = Translator->CreateAtomFromActivation(0.7f, 1, TEXT("Object"));
FPredicate Interaction = Translator->CreatePredicateFromAtoms(SubjectAtom, ObjectAtom, TEXT("Interacts"));
```

---

## 6. Discretization and Confidence

Use discretization for categorical reasoning:

```cpp
// Check if an activation should produce a symbolic atom
float Activation = 0.45f;
if (Translator->ShouldCreateAtom(Activation))
{
    // Get discrete bin for categorical processing
    int32 Bin = Translator->DiscretizeActivation(Activation);
    
    // Get confidence (sigmoid-mapped)
    float Confidence = Translator->CalculateConfidence(Activation);
    
    UE_LOG(LogDTE, Log, TEXT("Activation %.3f -> Bin %d, Confidence %.3f"),
        Activation, Bin, Confidence);
}
```

---

## 7. Uncertainty Propagation

Track confidence degradation through translation:

```cpp
// When translating a neural state with many features,
// uncertainty increases (more information loss)
float NeuralConfidence = 0.9f;
int32 FeatureCount = 64;

float PropagatedConfidence = Translator->PropagateUncertainty(NeuralConfidence, FeatureCount);
// Result: 0.9 * (1/sqrt(64)) = 0.9 * 0.125 = 0.1125

// Calculate combined uncertainty from multiple atoms
TArray<FSymbolicAtom> Atoms = Translator->CreateAtomsFromActivationVector(Activations, TEXT("Test"));
float PredicateUncertainty = Translator->CalculatePredicateUncertainty(Atoms);
// Uses geometric mean of atom confidences
```

---

## 8. Configuration Tuning

Adjust parameters for different use cases:

```cpp
// High-sensitivity mode (more atoms, lower threshold)
Translator->Config.ActivationThreshold = 0.1f;
Translator->Config.MaxAtomsPerTranslation = 500;
Translator->Config.DiscretizationBins = 20;

// High-precision mode (fewer, more confident atoms)
Translator->Config.ActivationThreshold = 0.6f;
Translator->Config.ConfidenceThreshold = 0.8f;
Translator->Config.MaxAtomsPerTranslation = 20;

// Real-time mode (optimize for latency)
Translator->Config.bEnableBatchProcessing = true;
Translator->Config.BatchSize = 64;
Translator->Config.MaxAtomsPerTranslation = 50;
```

---

## 9. Performance Monitoring

Monitor translation performance in real-time:

```cpp
// Get metrics
FTranslationMetrics Metrics = Translator->GetMetrics();

UE_LOG(LogDTE, Log, TEXT("Translator Performance:"));
UE_LOG(LogDTE, Log, TEXT("  Translations: %lld"), Metrics.TotalTranslations);
UE_LOG(LogDTE, Log, TEXT("  Avg Latency: %.4f ms"), Metrics.AverageLatency);
UE_LOG(LogDTE, Log, TEXT("  Peak Latency: %.4f ms"), Metrics.PeakLatency);
UE_LOG(LogDTE, Log, TEXT("  Atoms/Translation: %.1f"), Metrics.AverageAtomsPerTranslation);
UE_LOG(LogDTE, Log, TEXT("  Batch Efficiency: %.1f%%"), Metrics.BatchEfficiency);

// Check latency compliance
if (!Translator->IsMeetingLatencyTarget())
{
    UE_LOG(LogDTE, Warning, TEXT("Translator exceeding 0.5ms target! Avg: %.3f ms"),
        Metrics.AverageLatency);
    
    // Auto-tune: reduce atom cap
    Translator->Config.MaxAtomsPerTranslation = 
        FMath::Max(10, Translator->Config.MaxAtomsPerTranslation / 2);
}

// Full diagnostic report
TArray<FString> Report = Translator->GenerateDiagnosticReport();
for (const FString& Line : Report)
{
    UE_LOG(LogDTE, Log, TEXT("%s"), *Line);
}
```

---

## 10. Integration with NeuroSymbolicBridge

The translator works alongside the bridge for complete neuro-symbolic processing:

```cpp
// In your actor's BeginPlay:
void AMyActor::BeginPlay()
{
    Super::BeginPlay();
    
    // Components auto-discover each other via FindComponentByClass
    // Just ensure they're all attached to the same actor:
    // - UNeuroSymbolicBridge
    // - UNeuralToSymbolicTranslator (F1.1.1)
    // - USymbolicToNeuralEncoder (F1.1.2)
    // - UBidirectionalMessageProtocol (F1.1.3)
    // - UNeuralSymbolicSyncManager (F1.1.4)
}

// In your cognitive tick:
void AMyActor::ProcessCognitiveCycle(float DeltaTime)
{
    // 1. Get neural state from reservoir
    FNeuralState NeuralState = CollectNeuralState();
    
    // 2. Translate to symbolic (F1.1.1)
    FSymbolicState SymState = Translator->TranslateNeuralState(NeuralState);
    
    // 3. Feed into bridge for binding management
    for (const FSymbolicAtom& Atom : SymState.Atoms)
    {
        FNeuralActivationPattern Pattern;
        Pattern.PatternID = Atom.AtomID;
        Pattern.Activations = NeuralState.Activations;
        Pattern.Confidence = Atom.Confidence;
        
        FSymbolicRepresentation SymRep;
        SymRep.SymbolID = Atom.AtomID;
        SymRep.SymbolType = Atom.AtomType;
        SymRep.TruthValue = Atom.Confidence;
        
        Bridge->CreateBinding(Pattern, SymRep, TEXT("Grounding"));
    }
}
```

---

*Feature F1.1.1 - Neural-to-Symbolic Translation Layer*  
*Phase 1.1 - Neural-Symbolic Bridge Architecture*  
*Epic E1 - Foundation & Core Integration*

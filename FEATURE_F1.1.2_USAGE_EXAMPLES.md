# Feature F1.1.2 Usage Examples

## Quick Start

### Basic Setup
```cpp
// Create the encoder component
USymbolicToNeuralEncoder* Encoder = CreateDefaultSubobject<USymbolicToNeuralEncoder>(TEXT("SymbolicEncoder"));

// Initialize with default configuration
Encoder->Initialize();
```

## Example 1: Encode a Single Entity

```cpp
// Create entity properties
FGameEntityProperties Entity;
Entity.EntityID = TEXT("Player_001");
Entity.EntityType = TEXT("PlayerCharacter");
Entity.Location = FVector(100.0f, 200.0f, 50.0f);
Entity.Rotation = FRotator(0.0f, 90.0f, 0.0f);
Entity.Velocity = FVector(5.0f, 0.0f, 0.0f);

// Add categorical properties
Entity.CategoricalProperties.Add(TEXT("Team"), TEXT("Blue"));
Entity.CategoricalProperties.Add(TEXT("Class"), TEXT("Warrior"));
Entity.CategoricalProperties.Add(TEXT("State"), TEXT("Combat"));

// Add continuous properties
Entity.ContinuousProperties.Add(TEXT("Health"), 75.0f);
Entity.ContinuousProperties.Add(TEXT("Mana"), 50.0f);
Entity.ContinuousProperties.Add(TEXT("Stamina"), 80.0f);

// Encode to neural representation
FTensorEmbedding EntityEmbedding = Encoder->EncodeEntity(Entity);

// Use the embedding
UE_LOG(LogTemp, Log, TEXT("Entity encoded: %s, Dim: %d, Confidence: %.2f"), 
    *EntityEmbedding.EmbeddingID, 
    EntityEmbedding.Vector.Num(), 
    EntityEmbedding.Confidence);
```

## Example 2: Encode Game Events

```cpp
// Create a combat event
FGameEvent CombatEvent;
CombatEvent.EventID = TEXT("Event_Combat_001");
CombatEvent.EventType = TEXT("CombatEngaged");
CombatEvent.InvolvedEntities.Add(TEXT("Player_001"));
CombatEvent.InvolvedEntities.Add(TEXT("Enemy_023"));
CombatEvent.Magnitude = 0.8f;
CombatEvent.Timestamp = GetWorld()->GetTimeSeconds();
CombatEvent.Properties.Add(TEXT("Weapon"), TEXT("Sword"));
CombatEvent.Properties.Add(TEXT("Location"), TEXT("Forest"));

// Encode the event
FTensorEmbedding EventEmbedding = Encoder->EncodeEvent(CombatEvent);

// Process the encoded event
ProcessNeuralEvent(EventEmbedding);
```

## Example 3: Encode Entity Relations

```cpp
// Define a relation between player and enemy
FEntityRelation Relation;
Relation.SourceEntityID = TEXT("Player_001");
Relation.TargetEntityID = TEXT("Enemy_023");
Relation.RelationType = TEXT("Attacking");
Relation.RelationStrength = 0.9f;
Relation.bIsBidirectional = false;

// Encode the relation
FTensorEmbedding RelationEmbedding = Encoder->EncodeRelation(Relation);

// Use for relational reasoning
UpdateEntityContext(RelationEmbedding);
```

## Example 4: Encode Complete Game State

```cpp
// Capture current game state
FGameStateSnapshot GameState;
GameState.SnapshotID = TEXT("State_") + FString::FromInt(FrameNumber);
GameState.Timestamp = GetWorld()->GetTimeSeconds();

// Gather all entities
TArray<AActor*> AllActors;
UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACharacter::StaticClass(), AllActors);

for (AActor* Actor : AllActors)
{
    FGameEntityProperties EntityProps = CaptureEntityProperties(Actor);
    GameState.Entities.Add(EntityProps);
}

// Add recent events
GameState.Events = GetRecentGameEvents();

// Add relations
GameState.Relations = ComputeEntityRelations(GameState.Entities);

// Encode complete state
FEncodedNeuralState EncodedState = Encoder->EncodeGameState(GameState);

// Feed to cognitive system
CognitiveCore->ProcessEncodedState(EncodedState);
```

## Example 5: Streaming Mode for Real-Time Updates

```cpp
// Enable streaming mode
Encoder->Config.bEnableStreamingMode = true;

void AMyGameMode::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Stream encode entity updates
    for (AActor* Actor : UpdatedActors)
    {
        FGameEntityProperties EntityProps = CaptureEntityProperties(Actor);
        Encoder->StreamEncodeEntityUpdate(EntityProps);
    }
    
    // Stream encode events
    for (const FGameEvent& Event : NewEvents)
    {
        Encoder->StreamEncodeEvent(Event);
    }
    
    // Flush buffer every 10 frames
    if (FrameCounter % 10 == 0)
    {
        FEncodedNeuralState StreamState = Encoder->FlushStreamingBuffer();
        ProcessStreamingState(StreamState);
        
        UE_LOG(LogTemp, Log, TEXT("Streaming buffer flushed: %d embeddings"), 
            StreamState.EntityEmbeddings.Num() + StreamState.EventEmbeddings.Num());
    }
    
    FrameCounter++;
}
```

## Example 6: Batch Processing

```cpp
// Collect multiple entities
TArray<FGameEntityProperties> EntitiesToEncode;
for (AActor* Actor : AllActors)
{
    EntitiesToEncode.Add(CaptureEntityProperties(Actor));
}

// Batch encode (optimized for parallel processing)
Encoder->Config.bEnableBatchProcessing = true;
Encoder->Config.BatchSize = 64;

TArray<FTensorEmbedding> BatchedEmbeddings = Encoder->BatchEncodeEntities(EntitiesToEncode);

UE_LOG(LogTemp, Log, TEXT("Batch encoded %d entities"), BatchedEmbeddings.Num());

// Process all embeddings
for (const FTensorEmbedding& Embedding : BatchedEmbeddings)
{
    ReservoirSystem->ProcessEmbedding(Embedding);
}
```

## Example 7: Custom Configuration

```cpp
// Configure categorical encoding
Encoder->Config.CategoricalConfig.bUseOneHot = false; // Use learned embeddings
Encoder->Config.CategoricalConfig.MaxCategories = 200;
Encoder->Config.CategoricalConfig.bEnableCategoryLearning = true;

// Configure continuous encoding
Encoder->Config.ContinuousConfig.NormalizationMethod = TEXT("StandardScaling");
Encoder->Config.ContinuousConfig.bEnableAdaptiveNormalization = true;
Encoder->Config.ContinuousConfig.OutlierClampStdDev = 3.0f;

// Configure relational encoding
Encoder->Config.RelationalConfig.EncodingMethod = TEXT("Attention");
Encoder->Config.RelationalConfig.AttentionHeads = 8;
Encoder->Config.RelationalConfig.MaxRelationsPerEntity = 20;

// Configure temporal encoding
Encoder->Config.TemporalConfig.bEnableTemporalSmoothing = true;
Encoder->Config.TemporalConfig.SmoothingFactor = 0.2f;
Encoder->Config.TemporalConfig.TemporalWindowSize = 15;
Encoder->Config.TemporalConfig.bEncodeVelocity = true;

// Apply configuration
Encoder->Initialize();
```

## Example 8: Register Custom Entity Type Embeddings

```cpp
// Create custom embedding for a specific entity type
TArray<float> CustomWarriorEmbedding;
CustomWarriorEmbedding.SetNum(128);

// Initialize with specific pattern
for (int32 i = 0; i < 128; i++)
{
    CustomWarriorEmbedding[i] = FMath::Sin(i * 0.1f);
}

// Register the custom embedding
Encoder->RegisterEntityTypeEmbedding(TEXT("WarriorClass"), CustomWarriorEmbedding);

// Now entities of type "WarriorClass" will use this embedding
FGameEntityProperties Warrior;
Warrior.EntityType = TEXT("WarriorClass");
FTensorEmbedding WarriorEmbedding = Encoder->EncodeEntity(Warrior);
```

## Example 9: Performance Monitoring

```cpp
// Enable performance monitoring
Encoder->bEnablePerformanceMonitoring = true;

// Check if meeting latency targets
if (Encoder->IsMeetingLatencyTarget())
{
    UE_LOG(LogTemp, Log, TEXT("✓ Encoding latency target met"));
}
else
{
    UE_LOG(LogTemp, Warning, TEXT("✗ Encoding latency exceeded target"));
}

// Get detailed metrics
FEncodingMetrics Metrics = Encoder->GetMetrics();
UE_LOG(LogTemp, Log, TEXT("Encoding Metrics:"));
UE_LOG(LogTemp, Log, TEXT("  Total Encodings: %lld"), Metrics.TotalEncodings);
UE_LOG(LogTemp, Log, TEXT("  Avg Latency: %.3f ms"), Metrics.AverageLatency);
UE_LOG(LogTemp, Log, TEXT("  Peak Latency: %.3f ms"), Metrics.PeakLatency);
UE_LOG(LogTemp, Log, TEXT("  Throughput: %.2f enc/s"), Metrics.StreamingThroughput);

// Generate diagnostic report
TArray<FString> Report = Encoder->GenerateDiagnosticReport();
for (const FString& Line : Report)
{
    UE_LOG(LogTemp, Log, TEXT("%s"), *Line);
}
```

## Example 10: Integration with NeuroSymbolicBridge

```cpp
// Complete bidirectional encoding/decoding pipeline

// 1. Get components
USymbolicToNeuralEncoder* Encoder = GetEncoder();
UNeuralToSymbolicTranslator* Translator = GetTranslator();
UNeuroSymbolicBridge* Bridge = GetBridge();

// 2. Encode symbolic game state → neural
FGameStateSnapshot GameState = CaptureGameState();
FEncodedNeuralState EncodedState = Encoder->EncodeGameState(GameState);

// 3. Process in neural domain (reservoir computing)
ReservoirSystem->ProcessInput(EncodedState.EncodedVector);
TArray<float> ReservoirOutput = ReservoirSystem->GetOutput();

// 4. Translate neural output → symbolic
FNeuralState NeuralOutput;
NeuralOutput.Activations = ReservoirOutput;
FSymbolicState SymbolicResult = Translator->TranslateNeuralState(NeuralOutput);

// 5. Create neuro-symbolic binding
for (const FSymbolicAtom& Atom : SymbolicResult.Atoms)
{
    // Find matching entity embedding
    for (const FTensorEmbedding& Embedding : EncodedState.EntityEmbeddings)
    {
        FNeuralActivationPattern Pattern;
        Pattern.Activations = Embedding.Vector;
        Pattern.Confidence = Embedding.Confidence;
        
        FSymbolicRepresentation Symbol;
        Symbol.SymbolID = Atom.AtomID;
        Symbol.SymbolType = Atom.AtomType;
        
        // Create binding
        FNeuroSymbolicBinding Binding = Bridge->CreateBinding(Pattern, Symbol, TEXT("Grounding"));
    }
}

// 6. Use bindings for cognitive reasoning
TArray<FNeuroSymbolicBinding> ActiveBindings = Bridge->GetActiveBindings();
for (const FNeuroSymbolicBinding& Binding : ActiveBindings)
{
    ProcessCognitiveBinding(Binding);
}
```

## Example 11: Temporal Sequence Encoding

```cpp
// Maintain history of game states
TArray<FGameStateSnapshot> StateHistory;

void UpdateStateHistory(const FGameStateSnapshot& CurrentState)
{
    StateHistory.Add(CurrentState);
    
    // Keep only recent history (10 frames)
    if (StateHistory.Num() > 10)
    {
        StateHistory.RemoveAt(0);
    }
    
    // Encode temporal sequence
    if (StateHistory.Num() >= 3)
    {
        TArray<float> TemporalEncoding = Encoder->EncodeTemporalSequence(StateHistory);
        
        // Use for prediction/anticipation
        CognitiveCore->UpdateTemporalContext(TemporalEncoding);
    }
    
    // Encode velocity (change rate)
    if (StateHistory.Num() >= 2)
    {
        FGameStateSnapshot& Previous = StateHistory[StateHistory.Num() - 2];
        FGameStateSnapshot& Current = StateHistory[StateHistory.Num() - 1];
        
        for (int32 i = 0; i < Current.Entities.Num(); i++)
        {
            if (i < Previous.Entities.Num())
            {
                TArray<float> VelocityEncoding = Encoder->EncodeVelocity(
                    Current.Entities[i].Location,
                    Previous.Entities[i].Location,
                    GetWorld()->GetDeltaSeconds()
                );
                
                ProcessVelocityInfo(Current.Entities[i].EntityID, VelocityEncoding);
            }
        }
    }
}
```

## Common Patterns

### Pattern 1: Entity State Tracking
```cpp
TMap<FString, FTensorEmbedding> EntityEmbeddingCache;

void TrackEntity(const FGameEntityProperties& Entity)
{
    FTensorEmbedding Embedding = Encoder->EncodeEntity(Entity);
    EntityEmbeddingCache.Add(Entity.EntityID, Embedding);
}

void UpdateEntityState(const FString& EntityID, const FGameEntityProperties& NewState)
{
    FTensorEmbedding OldEmbedding = EntityEmbeddingCache[EntityID];
    FTensorEmbedding NewEmbedding = Encoder->EncodeEntity(NewState);
    
    // Compute change
    float ChangeAmount = ComputeEmbeddingDistance(OldEmbedding.Vector, NewEmbedding.Vector);
    
    if (ChangeAmount > 0.1f)
    {
        UE_LOG(LogTemp, Log, TEXT("Significant state change for %s: %.3f"), *EntityID, ChangeAmount);
    }
    
    EntityEmbeddingCache[EntityID] = NewEmbedding;
}
```

### Pattern 2: Event Detection and Encoding
```cpp
void DetectAndEncodeEvents()
{
    // Detect events from game state changes
    TArray<FGameEvent> DetectedEvents = EventDetector->DetectEvents();
    
    // Encode all events
    TArray<FTensorEmbedding> EventEmbeddings;
    for (const FGameEvent& Event : DetectedEvents)
    {
        FTensorEmbedding Embedding = Encoder->EncodeEvent(Event);
        EventEmbeddings.Add(Embedding);
        
        // Log significant events
        if (Event.Magnitude > 0.7f)
        {
            UE_LOG(LogTemp, Log, TEXT("High-magnitude event: %s (%.2f)"), 
                *Event.EventType, Event.Magnitude);
        }
    }
    
    // Feed to cognitive system
    CognitiveCore->ProcessEventSequence(EventEmbeddings);
}
```

## Debugging Tips

### Enable Debug Logging
```cpp
Encoder->bEnableDebugLogging = true;
```

### Check Streaming Buffer Size
```cpp
int32 BufferSize = Encoder->GetStreamingBufferSize();
UE_LOG(LogTemp, Log, TEXT("Streaming buffer: %d/%d"), BufferSize, 1000);
```

### Validate Embeddings
```cpp
bool IsValidEmbedding(const FTensorEmbedding& Embedding)
{
    if (Embedding.Vector.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("Empty embedding vector!"));
        return false;
    }
    
    if (Embedding.Confidence < 0.0f || Embedding.Confidence > 1.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid confidence: %.2f"), Embedding.Confidence);
        return false;
    }
    
    return true;
}
```

### Monitor Performance
```cpp
void MonitorEncoderPerformance()
{
    FEncodingMetrics Metrics = Encoder->GetMetrics();
    
    if (Metrics.AverageLatency > 0.5f)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠ Encoding latency high: %.3f ms"), Metrics.AverageLatency);
    }
    
    if (Metrics.StreamingThroughput < 2000.0f)
    {
        UE_LOG(LogTemp, Warning, TEXT("⚠ Low throughput: %.2f enc/s"), Metrics.StreamingThroughput);
    }
}
```

// SymbolicToNeuralEncoder.cpp
// Symbolic-to-Neural Encoding Interface Implementation
// Feature F1.1.2: Converts Unreal Engine symbolic game state into neural-compatible representations
// Copyright (c) 2025 Deep Tree Echo Project

#include "SymbolicToNeuralEncoder.h"
#include "NeuroSymbolicBridge.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include <chrono>

USymbolicToNeuralEncoder::USymbolicToNeuralEncoder()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void USymbolicToNeuralEncoder::BeginPlay()
{
    Super::BeginPlay();
    Initialize();
}

void USymbolicToNeuralEncoder::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    if (bEnablePerformanceMonitoring)
    {
        UpdateMetrics();
    }
}

// ========================================
// INITIALIZATION
// ========================================

void USymbolicToNeuralEncoder::Initialize()
{
    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("SymbolicToNeuralEncoder: Initializing encoder..."));
    }

    DiscoverBridge();
    ResetMetrics();

    // Initialize default embeddings
    EntityTypeEmbeddings.Empty();
    CategoryMappings.Empty();
    ContinuousStats.Empty();
    StreamingBuffer.Empty();

    if (bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("SymbolicToNeuralEncoder: Initialization complete"));
    }
}

void USymbolicToNeuralEncoder::DiscoverBridge()
{
    if (AActor* Owner = GetOwner())
    {
        Bridge = Owner->FindComponentByClass<UNeuroSymbolicBridge>();
        
        if (Bridge)
        {
            if (bEnableDebugLogging)
            {
                UE_LOG(LogTemp, Log, TEXT("SymbolicToNeuralEncoder: Connected to NeuroSymbolicBridge"));
            }
        }
        else if (bEnableDebugLogging)
        {
            UE_LOG(LogTemp, Warning, TEXT("SymbolicToNeuralEncoder: NeuroSymbolicBridge not found on owner"));
        }
    }
}

void USymbolicToNeuralEncoder::ResetEncoder()
{
    ResetMetrics();
    ClearEmbeddings();
    StreamingBuffer.Empty();
    PreviousGameState = FGameStateSnapshot();
}

void USymbolicToNeuralEncoder::ResetMetrics()
{
    Metrics = FEncodingMetrics();
    LatencySamples.Empty();
}

// ========================================
// CORE ENCODING API
// ========================================

FTensorEmbedding USymbolicToNeuralEncoder::EncodeEntity(const FGameEntityProperties& Entity)
{
    auto StartTime = std::chrono::high_resolution_clock::now();

    FTensorEmbedding Result;
    Result.EmbeddingID = GenerateEmbeddingID();
    Result.EmbeddingType = TEXT("Entity");
    Result.SourceID = Entity.EntityID;
    Result.Timestamp = Entity.Timestamp;
    Result.Confidence = 1.0f;

    // Get entity type embedding
    TArray<float> TypeEmbedding = GetEntityTypeEmbedding(Entity.EntityType);

    // Encode spatial properties
    TArray<float> SpatialEncoding = EncodeContinuousVector(Entity.Location, TEXT("Location"));
    SpatialEncoding.Append(EncodeContinuousVector(FVector(Entity.Rotation.Pitch, Entity.Rotation.Yaw, Entity.Rotation.Roll), TEXT("Rotation")));
    SpatialEncoding.Append(EncodeContinuousVector(Entity.Velocity, TEXT("Velocity")));

    // Encode categorical properties
    TArray<float> CategoricalEncoding;
    for (const auto& Prop : Entity.CategoricalProperties)
    {
        TArray<float> PropEncoding = GetCategoryEmbedding(Prop.Value, Prop.Key);
        CategoricalEncoding.Append(PropEncoding);
    }

    // Encode continuous properties
    TArray<float> ContinuousEncoding = EncodeContinuousProperties(Entity.ContinuousProperties);

    // Concatenate all encodings
    Result.Vector = TypeEmbedding;
    Result.Vector.Append(SpatialEncoding);
    Result.Vector.Append(CategoricalEncoding);
    Result.Vector.Append(ContinuousEncoding);

    // Pad or truncate to target dimension
    while (Result.Vector.Num() < Config.EntityEmbeddingDim)
    {
        Result.Vector.Add(0.0f);
    }
    if (Result.Vector.Num() > Config.EntityEmbeddingDim)
    {
        Result.Vector.SetNum(Config.EntityEmbeddingDim);
    }

    // Record latency
    auto EndTime = std::chrono::high_resolution_clock::now();
    float LatencyMs = std::chrono::duration<float, std::milli>(EndTime - StartTime).count();
    RecordLatency(LatencyMs);

    Metrics.TotalEntityEmbeddings++;

    return Result;
}

FTensorEmbedding USymbolicToNeuralEncoder::EncodeEvent(const FGameEvent& Event)
{
    auto StartTime = std::chrono::high_resolution_clock::now();

    FTensorEmbedding Result;
    Result.EmbeddingID = GenerateEmbeddingID();
    Result.EmbeddingType = TEXT("Event");
    Result.SourceID = Event.EventID;
    Result.Timestamp = Event.Timestamp;
    Result.Confidence = 1.0f;

    // Encode event type
    TArray<float> TypeEncoding = GetCategoryEmbedding(Event.EventType, TEXT("EventType"));

    // Encode event magnitude
    float EncodedMagnitude = EncodeContinuousValue(Event.Magnitude, TEXT("EventMagnitude"));
    TArray<float> MagnitudeVector = { EncodedMagnitude };

    // Encode involved entities (aggregate their embeddings)
    TArray<TArray<float>> EntityVectors;
    for (const FString& EntityID : Event.InvolvedEntities)
    {
        // Create placeholder embedding for entity ID
        TArray<float> EntityVec = CreateRandomEmbedding(32);
        EntityVectors.Add(EntityVec);
    }
    TArray<float> AggregatedEntities = EntityVectors.Num() > 0 ? ComputeVectorMean(EntityVectors) : CreateZeroVector(32);

    // Concatenate encodings
    Result.Vector = TypeEncoding;
    Result.Vector.Append(MagnitudeVector);
    Result.Vector.Append(AggregatedEntities);

    // Pad or truncate to target dimension
    while (Result.Vector.Num() < Config.EventEmbeddingDim)
    {
        Result.Vector.Add(0.0f);
    }
    if (Result.Vector.Num() > Config.EventEmbeddingDim)
    {
        Result.Vector.SetNum(Config.EventEmbeddingDim);
    }

    // Record latency
    auto EndTime = std::chrono::high_resolution_clock::now();
    float LatencyMs = std::chrono::duration<float, std::milli>(EndTime - StartTime).count();
    RecordLatency(LatencyMs);

    Metrics.TotalEventEmbeddings++;

    return Result;
}

FTensorEmbedding USymbolicToNeuralEncoder::EncodeRelation(const FEntityRelation& Relation)
{
    auto StartTime = std::chrono::high_resolution_clock::now();

    FTensorEmbedding Result;
    Result.EmbeddingID = GenerateEmbeddingID();
    Result.EmbeddingType = TEXT("Relation");
    Result.SourceID = Relation.SourceEntityID + TEXT("->") + Relation.TargetEntityID;
    Result.Timestamp = 0.0f;
    Result.Confidence = Relation.RelationStrength;

    // Encode relation type
    TArray<float> TypeEncoding = GetCategoryEmbedding(Relation.RelationType, TEXT("RelationType"));

    // Encode relation strength
    float EncodedStrength = EncodeContinuousValue(Relation.RelationStrength, TEXT("RelationStrength"));
    TArray<float> StrengthVector = { EncodedStrength };

    // Encode bidirectional flag
    TArray<float> DirectionalVector = { Relation.bIsBidirectional ? 1.0f : 0.0f };

    // Concatenate encodings
    Result.Vector = TypeEncoding;
    Result.Vector.Append(StrengthVector);
    Result.Vector.Append(DirectionalVector);

    // Pad to relation embedding dimension
    while (Result.Vector.Num() < Config.RelationalConfig.RelationEmbeddingDim)
    {
        Result.Vector.Add(0.0f);
    }
    if (Result.Vector.Num() > Config.RelationalConfig.RelationEmbeddingDim)
    {
        Result.Vector.SetNum(Config.RelationalConfig.RelationEmbeddingDim);
    }

    // Record latency
    auto EndTime = std::chrono::high_resolution_clock::now();
    float LatencyMs = std::chrono::duration<float, std::milli>(EndTime - StartTime).count();
    RecordLatency(LatencyMs);

    Metrics.TotalRelationalEmbeddings++;

    return Result;
}

FEncodedNeuralState USymbolicToNeuralEncoder::EncodeGameState(const FGameStateSnapshot& GameState)
{
    auto StartTime = std::chrono::high_resolution_clock::now();

    FEncodedNeuralState Result;
    Result.StateID = GameState.SnapshotID;
    Result.Timestamp = GameState.Timestamp;
    Result.Confidence = 1.0f;

    // Encode all entities
    for (const FGameEntityProperties& Entity : GameState.Entities)
    {
        FTensorEmbedding EntityEmbed = EncodeEntity(Entity);
        Result.EntityEmbeddings.Add(EntityEmbed);
    }

    // Encode all events
    for (const FGameEvent& Event : GameState.Events)
    {
        FTensorEmbedding EventEmbed = EncodeEvent(Event);
        Result.EventEmbeddings.Add(EventEmbed);
    }

    // Encode all relations
    for (const FEntityRelation& Relation : GameState.Relations)
    {
        FTensorEmbedding RelationEmbed = EncodeRelation(Relation);
        Result.RelationalEmbeddings.Add(RelationEmbed);
    }

    // Compute temporal context if we have previous state
    if (Config.TemporalConfig.bEnableTemporalSmoothing && PreviousGameState.Entities.Num() > 0)
    {
        Result.TemporalContext = EncodeStateDelta(GameState, PreviousGameState);
    }
    else
    {
        Result.TemporalContext = CreateZeroVector(64);
    }

    // Aggregate all embeddings into primary encoded vector
    TArray<TArray<float>> AllVectors;
    for (const FTensorEmbedding& Embed : Result.EntityEmbeddings)
    {
        AllVectors.Add(Embed.Vector);
    }
    for (const FTensorEmbedding& Embed : Result.EventEmbeddings)
    {
        AllVectors.Add(Embed.Vector);
    }
    Result.EncodedVector = AllVectors.Num() > 0 ? ComputeVectorMean(AllVectors) : CreateZeroVector(Config.EntityEmbeddingDim);

    // Store as previous state for next delta encoding
    PreviousGameState = GameState;

    // Record latency
    auto EndTime = std::chrono::high_resolution_clock::now();
    float LatencyMs = std::chrono::duration<float, std::milli>(EndTime - StartTime).count();
    RecordLatency(LatencyMs);

    Metrics.TotalEncodings++;

    return Result;
}

// ========================================
// CATEGORICAL ENCODING
// ========================================

TArray<float> USymbolicToNeuralEncoder::EncodeCategoricalOneHot(const FString& Category, const FString& PropertyName)
{
    int32 CategoryIndex = GetOrCreateCategoryIndex(Category, PropertyName);
    
    TMap<FString, int32>& PropCategories = CategoryMappings.FindOrAdd(PropertyName);
    int32 MaxIndex = PropCategories.Num();
    
    // Clamp to max categories
    if (MaxIndex > Config.CategoricalConfig.MaxCategories)
    {
        MaxIndex = Config.CategoricalConfig.MaxCategories;
    }
    
    TArray<float> OneHot;
    OneHot.SetNumZeroed(MaxIndex);
    
    if (CategoryIndex < MaxIndex)
    {
        OneHot[CategoryIndex] = 1.0f;
    }
    
    return OneHot;
}

TArray<float> USymbolicToNeuralEncoder::EncodeCategoricalMultiHot(const TArray<FString>& Categories, const FString& PropertyName)
{
    TMap<FString, int32>& PropCategories = CategoryMappings.FindOrAdd(PropertyName);
    int32 MaxIndex = FMath::Min(PropCategories.Num(), Config.CategoricalConfig.MaxCategories);
    
    TArray<float> MultiHot;
    MultiHot.SetNumZeroed(MaxIndex);
    
    for (const FString& Category : Categories)
    {
        int32 CategoryIndex = GetOrCreateCategoryIndex(Category, PropertyName);
        if (CategoryIndex < MaxIndex)
        {
            MultiHot[CategoryIndex] = 1.0f;
        }
    }
    
    return MultiHot;
}

TArray<float> USymbolicToNeuralEncoder::GetCategoryEmbedding(const FString& Category, const FString& PropertyName)
{
    TMap<FString, TArray<float>>& PropEmbeddings = CategoryEmbeddings.FindOrAdd(PropertyName);
    
    if (TArray<float>* Embedding = PropEmbeddings.Find(Category))
    {
        return *Embedding;
    }
    
    // Create new embedding
    TArray<float> NewEmbedding = CreateRandomEmbedding(16);
    PropEmbeddings.Add(Category, NewEmbedding);
    
    return NewEmbedding;
}

// ========================================
// CONTINUOUS ENCODING
// ========================================

float USymbolicToNeuralEncoder::EncodeContinuousValue(float Value, const FString& PropertyName)
{
    if (Config.ContinuousConfig.bEnableAdaptiveNormalization)
    {
        UpdateContinuousStats(PropertyName, Value);
    }
    
    return NormalizeContinuous(Value, PropertyName);
}

TArray<float> USymbolicToNeuralEncoder::EncodeContinuousVector(const FVector& Vector, const FString& PropertyName)
{
    TArray<float> Result;
    Result.Add(EncodeContinuousValue(Vector.X, PropertyName + TEXT("_X")));
    Result.Add(EncodeContinuousValue(Vector.Y, PropertyName + TEXT("_Y")));
    Result.Add(EncodeContinuousValue(Vector.Z, PropertyName + TEXT("_Z")));
    return Result;
}

TArray<float> USymbolicToNeuralEncoder::EncodeContinuousProperties(const TMap<FString, float>& Properties)
{
    TArray<float> Result;
    
    for (const auto& Prop : Properties)
    {
        float EncodedValue = EncodeContinuousValue(Prop.Value, Prop.Key);
        Result.Add(EncodedValue);
    }
    
    return Result;
}

// ========================================
// RELATIONAL ENCODING
// ========================================

TArray<float> USymbolicToNeuralEncoder::EncodeGraphStructure(const TArray<FEntityRelation>& Relations)
{
    // Simplified graph encoding: count relations by type
    TMap<FString, int32> RelationCounts;
    
    for (const FEntityRelation& Relation : Relations)
    {
        int32& Count = RelationCounts.FindOrAdd(Relation.RelationType, 0);
        Count++;
    }
    
    TArray<float> Result;
    for (const auto& TypeCount : RelationCounts)
    {
        Result.Add(static_cast<float>(TypeCount.Value));
    }
    
    // Pad to consistent size
    while (Result.Num() < 16)
    {
        Result.Add(0.0f);
    }
    
    return Result;
}

TArray<float> USymbolicToNeuralEncoder::EncodeEntityContext(const FString& EntityID, const TArray<FEntityRelation>& Relations)
{
    // Filter relations involving this entity
    TArray<FEntityRelation> RelevantRelations;
    for (const FEntityRelation& Relation : Relations)
    {
        if (Relation.SourceEntityID == EntityID || Relation.TargetEntityID == EntityID)
        {
            RelevantRelations.Add(Relation);
        }
    }
    
    // Encode the filtered relations
    return EncodeGraphStructure(RelevantRelations);
}

TArray<float> USymbolicToNeuralEncoder::CreateRelationalEmbedding(const FString& SourceID, const FString& TargetID, const FString& RelationType)
{
    // Simple concatenation of entity ID hashes and relation type
    TArray<float> Result;
    
    uint32 SourceHash = GetTypeHash(SourceID);
    uint32 TargetHash = GetTypeHash(TargetID);
    
    Result.Add(static_cast<float>(SourceHash % 1000) / 1000.0f);
    Result.Add(static_cast<float>(TargetHash % 1000) / 1000.0f);
    
    TArray<float> TypeEmbed = GetCategoryEmbedding(RelationType, TEXT("RelationType"));
    Result.Append(TypeEmbed);
    
    return Result;
}

// ========================================
// TEMPORAL ENCODING
// ========================================

TArray<float> USymbolicToNeuralEncoder::EncodeTemporalSequence(const TArray<FGameStateSnapshot>& StateSequence)
{
    if (StateSequence.Num() == 0)
    {
        return CreateZeroVector(Config.EntityEmbeddingDim);
    }
    
    // Encode each state and compute running average
    TArray<TArray<float>> EncodedStates;
    for (const FGameStateSnapshot& State : StateSequence)
    {
        FEncodedNeuralState EncodedState = EncodeGameState(State);
        EncodedStates.Add(EncodedState.EncodedVector);
    }
    
    return ComputeVectorMean(EncodedStates);
}

TArray<float> USymbolicToNeuralEncoder::EncodeStateDelta(const FGameStateSnapshot& CurrentState, const FGameStateSnapshot& PreviousState)
{
    TArray<float> Delta;
    
    // Compute entity count delta
    float EntityCountDelta = static_cast<float>(CurrentState.Entities.Num() - PreviousState.Entities.Num());
    Delta.Add(EntityCountDelta);
    
    // Compute event count delta
    float EventCountDelta = static_cast<float>(CurrentState.Events.Num() - PreviousState.Events.Num());
    Delta.Add(EventCountDelta);
    
    // Compute relation count delta
    float RelationCountDelta = static_cast<float>(CurrentState.Relations.Num() - PreviousState.Relations.Num());
    Delta.Add(RelationCountDelta);
    
    // Pad to consistent size
    while (Delta.Num() < 64)
    {
        Delta.Add(0.0f);
    }
    
    return Delta;
}

TArray<float> USymbolicToNeuralEncoder::EncodeVelocity(const FVector& CurrentPos, const FVector& PreviousPos, float DeltaTime)
{
    TArray<float> Velocity;
    
    if (DeltaTime > 0.0f)
    {
        FVector VelocityVec = (CurrentPos - PreviousPos) / DeltaTime;
        Velocity.Add(EncodeContinuousValue(VelocityVec.X, TEXT("Velocity_X")));
        Velocity.Add(EncodeContinuousValue(VelocityVec.Y, TEXT("Velocity_Y")));
        Velocity.Add(EncodeContinuousValue(VelocityVec.Z, TEXT("Velocity_Z")));
    }
    else
    {
        Velocity.Add(0.0f);
        Velocity.Add(0.0f);
        Velocity.Add(0.0f);
    }
    
    return Velocity;
}

// ========================================
// STREAMING ENCODER
// ========================================

void USymbolicToNeuralEncoder::StreamEncodeEntityUpdate(const FGameEntityProperties& Entity)
{
    FTensorEmbedding Embedding = EncodeEntity(Entity);
    StreamingBuffer.Add(Embedding);
    
    // Trim buffer if it exceeds max size
    if (StreamingBuffer.Num() > MaxStreamingBufferSize)
    {
        StreamingBuffer.RemoveAt(0);
    }
}

void USymbolicToNeuralEncoder::StreamEncodeEvent(const FGameEvent& Event)
{
    FTensorEmbedding Embedding = EncodeEvent(Event);
    StreamingBuffer.Add(Embedding);
    
    // Trim buffer if it exceeds max size
    if (StreamingBuffer.Num() > MaxStreamingBufferSize)
    {
        StreamingBuffer.RemoveAt(0);
    }
}

FEncodedNeuralState USymbolicToNeuralEncoder::FlushStreamingBuffer()
{
    FEncodedNeuralState Result;
    Result.StateID = TEXT("StreamingState_") + FString::FromInt(EmbeddingIDCounter++);
    Result.Timestamp = 0.0f;
    Result.Confidence = 1.0f;
    
    // Categorize embeddings
    for (const FTensorEmbedding& Embedding : StreamingBuffer)
    {
        if (Embedding.EmbeddingType == TEXT("Entity"))
        {
            Result.EntityEmbeddings.Add(Embedding);
        }
        else if (Embedding.EmbeddingType == TEXT("Event"))
        {
            Result.EventEmbeddings.Add(Embedding);
        }
        else if (Embedding.EmbeddingType == TEXT("Relation"))
        {
            Result.RelationalEmbeddings.Add(Embedding);
        }
    }
    
    // Aggregate into primary vector
    TArray<TArray<float>> AllVectors;
    for (const FTensorEmbedding& Embedding : StreamingBuffer)
    {
        AllVectors.Add(Embedding.Vector);
    }
    Result.EncodedVector = AllVectors.Num() > 0 ? ComputeVectorMean(AllVectors) : CreateZeroVector(Config.EntityEmbeddingDim);
    
    // Clear buffer
    StreamingBuffer.Empty();
    
    return Result;
}

int32 USymbolicToNeuralEncoder::GetStreamingBufferSize() const
{
    return StreamingBuffer.Num();
}

// ========================================
// BATCH ENCODING
// ========================================

TArray<FTensorEmbedding> USymbolicToNeuralEncoder::BatchEncodeEntities(const TArray<FGameEntityProperties>& Entities)
{
    TArray<FTensorEmbedding> Results;
    Results.Reserve(Entities.Num());
    
    for (const FGameEntityProperties& Entity : Entities)
    {
        Results.Add(EncodeEntity(Entity));
    }
    
    return Results;
}

TArray<FTensorEmbedding> USymbolicToNeuralEncoder::BatchEncodeEvents(const TArray<FGameEvent>& Events)
{
    TArray<FTensorEmbedding> Results;
    Results.Reserve(Events.Num());
    
    for (const FGameEvent& Event : Events)
    {
        Results.Add(EncodeEvent(Event));
    }
    
    return Results;
}

TArray<FEncodedNeuralState> USymbolicToNeuralEncoder::BatchEncodeGameStates(const TArray<FGameStateSnapshot>& States)
{
    TArray<FEncodedNeuralState> Results;
    Results.Reserve(States.Num());
    
    for (const FGameStateSnapshot& State : States)
    {
        Results.Add(EncodeGameState(State));
    }
    
    return Results;
}

// ========================================
// EMBEDDING MANAGEMENT
// ========================================

void USymbolicToNeuralEncoder::RegisterEntityTypeEmbedding(const FString& EntityType, const TArray<float>& Embedding)
{
    EntityTypeEmbeddings.Add(EntityType, Embedding);
}

TArray<float> USymbolicToNeuralEncoder::GetEntityTypeEmbedding(const FString& EntityType)
{
    if (TArray<float>* Embedding = EntityTypeEmbeddings.Find(EntityType))
    {
        return *Embedding;
    }
    
    return InitializeEntityEmbedding(EntityType);
}

void USymbolicToNeuralEncoder::ClearEmbeddings()
{
    EntityTypeEmbeddings.Empty();
    CategoryMappings.Empty();
    CategoryEmbeddings.Empty();
    ContinuousStats.Empty();
}

// ========================================
// METRICS
// ========================================

FEncodingMetrics USymbolicToNeuralEncoder::GetMetrics() const
{
    return Metrics;
}

bool USymbolicToNeuralEncoder::IsMeetingLatencyTarget() const
{
    return Metrics.AverageLatency < 0.5f;
}

TArray<FString> USymbolicToNeuralEncoder::GenerateDiagnosticReport() const
{
    TArray<FString> Report;
    
    Report.Add(TEXT("=== Symbolic-to-Neural Encoder Diagnostics ==="));
    Report.Add(FString::Printf(TEXT("Total Encodings: %lld"), Metrics.TotalEncodings));
    Report.Add(FString::Printf(TEXT("Average Latency: %.3f ms"), Metrics.AverageLatency));
    Report.Add(FString::Printf(TEXT("Peak Latency: %.3f ms"), Metrics.PeakLatency));
    Report.Add(FString::Printf(TEXT("Latency Target Met: %s"), IsMeetingLatencyTarget() ? TEXT("YES") : TEXT("NO")));
    Report.Add(TEXT(""));
    
    Report.Add(TEXT("Embedding Counts:"));
    Report.Add(FString::Printf(TEXT("  Entity Embeddings: %lld"), Metrics.TotalEntityEmbeddings));
    Report.Add(FString::Printf(TEXT("  Event Embeddings: %lld"), Metrics.TotalEventEmbeddings));
    Report.Add(FString::Printf(TEXT("  Relational Embeddings: %lld"), Metrics.TotalRelationalEmbeddings));
    Report.Add(TEXT(""));
    
    Report.Add(TEXT("Configuration:"));
    Report.Add(FString::Printf(TEXT("  Entity Embedding Dim: %d"), Config.EntityEmbeddingDim));
    Report.Add(FString::Printf(TEXT("  Event Embedding Dim: %d"), Config.EventEmbeddingDim));
    Report.Add(FString::Printf(TEXT("  Batch Processing: %s"), Config.bEnableBatchProcessing ? TEXT("Enabled") : TEXT("Disabled")));
    Report.Add(FString::Printf(TEXT("  Streaming Mode: %s"), Config.bEnableStreamingMode ? TEXT("Enabled") : TEXT("Disabled")));
    Report.Add(TEXT(""));
    
    Report.Add(TEXT("Learned Embeddings:"));
    Report.Add(FString::Printf(TEXT("  Entity Types: %d"), EntityTypeEmbeddings.Num()));
    Report.Add(FString::Printf(TEXT("  Category Mappings: %d"), CategoryMappings.Num()));
    Report.Add(FString::Printf(TEXT("  Continuous Stats: %d"), ContinuousStats.Num()));
    Report.Add(TEXT(""));
    
    if (Config.bEnableStreamingMode)
    {
        Report.Add(TEXT("Streaming:"));
        Report.Add(FString::Printf(TEXT("  Buffer Size: %d / %d"), StreamingBuffer.Num(), MaxStreamingBufferSize));
        Report.Add(FString::Printf(TEXT("  Throughput: %.2f enc/s"), Metrics.StreamingThroughput));
    }
    
    return Report;
}

// ========================================
// INTERNAL METHODS
// ========================================

FString USymbolicToNeuralEncoder::GenerateEmbeddingID()
{
    return FString::Printf(TEXT("Embedding_%d"), EmbeddingIDCounter++);
}

TArray<float> USymbolicToNeuralEncoder::InitializeEntityEmbedding(const FString& EntityType)
{
    TArray<float> Embedding = CreateRandomEmbedding(Config.EntityEmbeddingDim);
    EntityTypeEmbeddings.Add(EntityType, Embedding);
    return Embedding;
}

void USymbolicToNeuralEncoder::UpdateContinuousStats(const FString& PropertyName, float Value)
{
    TPair<float, float>& Stats = ContinuousStats.FindOrAdd(PropertyName, TPair<float, float>(0.0f, 1.0f));
    
    // Simple running average (could be improved with Welford's algorithm)
    float& Mean = Stats.Key;
    float& StdDev = Stats.Value;
    
    const float Alpha = 0.01f; // Learning rate
    Mean = Mean * (1.0f - Alpha) + Value * Alpha;
    
    float Deviation = FMath::Abs(Value - Mean);
    StdDev = StdDev * (1.0f - Alpha) + Deviation * Alpha;
    
    // Ensure StdDev is never zero
    StdDev = FMath::Max(StdDev, 0.001f);
}

float USymbolicToNeuralEncoder::NormalizeContinuous(float Value, const FString& PropertyName)
{
    if (ContinuousStats.Contains(PropertyName))
    {
        const TPair<float, float>& Stats = ContinuousStats[PropertyName];
        float Mean = Stats.Key;
        float StdDev = Stats.Value;
        
        if (Config.ContinuousConfig.NormalizationMethod == TEXT("StandardScaling"))
        {
            float Normalized = (Value - Mean) / StdDev;
            
            // Clamp outliers
            float ClampRange = Config.ContinuousConfig.OutlierClampStdDev;
            Normalized = FMath::Clamp(Normalized, -ClampRange, ClampRange);
            
            return Normalized;
        }
        else if (Config.ContinuousConfig.NormalizationMethod == TEXT("MinMax"))
        {
            // Simple min-max to [0, 1] assuming mean ± 3*stddev range
            float MinVal = Mean - 3.0f * StdDev;
            float MaxVal = Mean + 3.0f * StdDev;
            return FMath::Clamp((Value - MinVal) / (MaxVal - MinVal), 0.0f, 1.0f);
        }
    }
    
    return Value; // No normalization if stats not available
}

TArray<float> USymbolicToNeuralEncoder::ApplyTemporalSmoothing(const TArray<float>& Current, const TArray<float>& Previous)
{
    if (Current.Num() != Previous.Num())
    {
        return Current;
    }
    
    TArray<float> Smoothed;
    Smoothed.Reserve(Current.Num());
    
    float Alpha = Config.TemporalConfig.SmoothingFactor;
    
    for (int32 i = 0; i < Current.Num(); i++)
    {
        float SmoothedValue = Previous[i] * Alpha + Current[i] * (1.0f - Alpha);
        Smoothed.Add(SmoothedValue);
    }
    
    return Smoothed;
}

TArray<float> USymbolicToNeuralEncoder::ComputeAttentionWeights(const TArray<FTensorEmbedding>& Embeddings, const TArray<float>& QueryEmbedding)
{
    TArray<float> Weights;
    Weights.Reserve(Embeddings.Num());
    
    float TotalWeight = 0.0f;
    
    for (const FTensorEmbedding& Embedding : Embeddings)
    {
        // Simplified dot product attention
        float DotProduct = 0.0f;
        int32 MinDim = FMath::Min(Embedding.Vector.Num(), QueryEmbedding.Num());
        
        for (int32 i = 0; i < MinDim; i++)
        {
            DotProduct += Embedding.Vector[i] * QueryEmbedding[i];
        }
        
        float Weight = FMath::Exp(DotProduct / FMath::Sqrt(static_cast<float>(MinDim)));
        Weights.Add(Weight);
        TotalWeight += Weight;
    }
    
    // Normalize weights
    if (TotalWeight > 0.0f)
    {
        for (float& Weight : Weights)
        {
            Weight /= TotalWeight;
        }
    }
    
    return Weights;
}

TArray<float> USymbolicToNeuralEncoder::AggregateWithAttention(const TArray<FTensorEmbedding>& Embeddings, const TArray<float>& AttentionWeights)
{
    if (Embeddings.Num() == 0 || Embeddings.Num() != AttentionWeights.Num())
    {
        return CreateZeroVector(Config.EntityEmbeddingDim);
    }
    
    TArray<float> Aggregated;
    Aggregated.SetNumZeroed(Embeddings[0].Vector.Num());
    
    for (int32 i = 0; i < Embeddings.Num(); i++)
    {
        float Weight = AttentionWeights[i];
        const TArray<float>& Vector = Embeddings[i].Vector;
        
        for (int32 j = 0; j < FMath::Min(Aggregated.Num(), Vector.Num()); j++)
        {
            Aggregated[j] += Vector[j] * Weight;
        }
    }
    
    return Aggregated;
}

void USymbolicToNeuralEncoder::RecordLatency(float LatencyMs)
{
    LatencySamples.Add(LatencyMs);
    
    if (LatencySamples.Num() > MaxLatencySamples)
    {
        LatencySamples.RemoveAt(0);
    }
    
    // Update metrics
    Metrics.PeakLatency = FMath::Max(Metrics.PeakLatency, LatencyMs);
}

void USymbolicToNeuralEncoder::UpdateMetrics()
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
    
    if (Metrics.TotalEncodings > 0)
    {
        Metrics.AverageEmbeddingsPerEncoding = 
            static_cast<float>(Metrics.TotalEntityEmbeddings + Metrics.TotalEventEmbeddings + Metrics.TotalRelationalEmbeddings) / 
            static_cast<float>(Metrics.TotalEncodings);
    }
    
    // Calculate streaming throughput (encodings per second)
    if (Config.bEnableStreamingMode && Metrics.AverageLatency > 0.0f)
    {
        Metrics.StreamingThroughput = 1000.0f / Metrics.AverageLatency;
    }
}

int32 USymbolicToNeuralEncoder::GetOrCreateCategoryIndex(const FString& Category, const FString& PropertyName)
{
    TMap<FString, int32>& PropCategories = CategoryMappings.FindOrAdd(PropertyName);
    
    if (int32* Index = PropCategories.Find(Category))
    {
        return *Index;
    }
    
    // Create new category index
    if (!Config.CategoricalConfig.bEnableCategoryLearning && PropCategories.Num() >= Config.CategoricalConfig.MaxCategories)
    {
        // Return last index as "unknown" category
        return Config.CategoricalConfig.MaxCategories - 1;
    }
    
    int32 NewIndex = PropCategories.Num();
    PropCategories.Add(Category, NewIndex);
    
    return NewIndex;
}

TArray<float> USymbolicToNeuralEncoder::CreateZeroVector(int32 Dimension) const
{
    TArray<float> Vector;
    Vector.SetNumZeroed(Dimension);
    return Vector;
}

TArray<float> USymbolicToNeuralEncoder::CreateRandomEmbedding(int32 Dimension) const
{
    TArray<float> Embedding;
    Embedding.Reserve(Dimension);
    
    for (int32 i = 0; i < Dimension; i++)
    {
        // Xavier/Glorot initialization
        float Value = FMath::FRandRange(-1.0f, 1.0f) / FMath::Sqrt(static_cast<float>(Dimension));
        Embedding.Add(Value);
    }
    
    return Embedding;
}

TArray<float> USymbolicToNeuralEncoder::ConcatenateVectors(const TArray<TArray<float>>& Vectors) const
{
    TArray<float> Result;
    
    for (const TArray<float>& Vector : Vectors)
    {
        Result.Append(Vector);
    }
    
    return Result;
}

TArray<float> USymbolicToNeuralEncoder::ComputeVectorMean(const TArray<TArray<float>>& Vectors) const
{
    if (Vectors.Num() == 0)
    {
        return TArray<float>();
    }
    
    int32 Dimension = Vectors[0].Num();
    TArray<float> Mean;
    Mean.SetNumZeroed(Dimension);
    
    for (const TArray<float>& Vector : Vectors)
    {
        for (int32 i = 0; i < FMath::Min(Dimension, Vector.Num()); i++)
        {
            Mean[i] += Vector[i];
        }
    }
    
    float Count = static_cast<float>(Vectors.Num());
    for (float& Value : Mean)
    {
        Value /= Count;
    }
    
    return Mean;
}

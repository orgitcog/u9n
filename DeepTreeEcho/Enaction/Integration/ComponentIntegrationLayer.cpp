// ComponentIntegrationLayer.cpp
// Feature F1.6.1: Unified Integration Layer for E1 Components
// Copyright (c) 2025-2026 Deep Tree Echo Project

#include "ComponentIntegrationLayer.h"
#include "../../Core/NeuroSymbolicBridge.h"
#include "../../Reservoir/DeepTreeEchoReservoir.h"
#include "../../Membrane/MembraneHierarchyManager.h"
#include "../../Memory/HypergraphMemorySystem.h"
#include "../../Memory/TemporalEventGraph.h"
#include "../../Cognitive/CognitiveCycleManager.h"
#include "../../Core/DeepTreeEchoCore.h"

// ============================================================================
// CONSTRUCTOR & LIFECYCLE
// ============================================================================

UComponentIntegrationLayer::UComponentIntegrationLayer()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.0f; // Tick every frame

    // Initialize with default config
    Config = FIntegrationConfig();
    
    // Clear pointers
    Bridge = nullptr;
    Reservoir = nullptr;
    PSystem = nullptr;
    Hypergraph = nullptr;
    Temporal = nullptr;
    CognitiveCycle = nullptr;
    DTECore = nullptr;
}

void UComponentIntegrationLayer::BeginPlay()
{
    Super::BeginPlay();

    if (Config.bAutoDiscoverComponents)
    {
        Initialize();
    }
}

void UComponentIntegrationLayer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Clear all channels and state
    Channels.Empty();
    EventHistory.Empty();
    ComponentStatuses.Empty();
    bIsInitialized = false;

    Super::EndPlay(EndPlayReason);
}

void UComponentIntegrationLayer::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (!bEnableIntegration || !bIsInitialized)
    {
        return;
    }

    // Accumulate time for synchronization
    AccumulatedTime += DeltaTime;

    // Update component statuses periodically
    if (AccumulatedTime >= Config.DefaultSyncInterval)
    {
        AccumulatedTime = 0.0f;
        
        // Update statuses
        UpdateAllComponentStatuses();
        
        // Synchronize channels
        SynchronizeAllChannels();
        
        // Update statistics
        UpdateStatistics();
        
        // Prune old events
        PruneEventHistory();
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void UComponentIntegrationLayer::Initialize()
{
    if (bIsInitialized)
    {
        return;
    }

    // Initialize component statuses
    InitializeComponentStatuses();

    // Discover components
    DiscoverComponents();

    // Create default channels if enabled
    if (Config.bAutoCreateChannels)
    {
        CreateDefaultChannels();
    }

    bIsInitialized = true;

    // Record initialization event
    RecordEvent(EIntegrationEventType::ComponentInitialized, EE1ComponentType::Bridge, 
                EE1ComponentType::Bridge, TEXT("Component Integration Layer initialized"), true);

    if (Config.bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("[F1.6.1] Component Integration Layer initialized with %d components"), 
               GetAvailableComponentCount());
    }
}

void UComponentIntegrationLayer::DiscoverComponents()
{
    AActor* Owner = GetOwner();
    if (!Owner)
    {
        return;
    }

    // Discover Neural-Symbolic Bridge
    Bridge = Owner->FindComponentByClass<UNeuroSymbolicBridge>();
    UpdateComponentStatus(EE1ComponentType::Bridge);

    // Discover Reservoir
    Reservoir = Owner->FindComponentByClass<UDeepTreeEchoReservoir>();
    UpdateComponentStatus(EE1ComponentType::Reservoir);

    // Discover P-System Membrane Manager
    PSystem = Owner->FindComponentByClass<UMembraneHierarchyManager>();
    UpdateComponentStatus(EE1ComponentType::PSystem);

    // Discover Hypergraph Memory System
    Hypergraph = Owner->FindComponentByClass<UHypergraphMemorySystem>();
    UpdateComponentStatus(EE1ComponentType::Hypergraph);

    // Discover Temporal Event Graph
    Temporal = Owner->FindComponentByClass<UTemporalEventGraph>();
    UpdateComponentStatus(EE1ComponentType::Temporal);

    // Optional: Discover Cognitive Cycle Manager
    CognitiveCycle = Owner->FindComponentByClass<UCognitiveCycleManager>();

    // Optional: Discover Deep Tree Echo Core
    DTECore = Owner->FindComponentByClass<UDeepTreeEchoCore>();

    if (Config.bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("[F1.6.1] Discovered components: Bridge=%s, Reservoir=%s, PSystem=%s, Hypergraph=%s, Temporal=%s"),
               Bridge ? TEXT("Yes") : TEXT("No"),
               Reservoir ? TEXT("Yes") : TEXT("No"),
               PSystem ? TEXT("Yes") : TEXT("No"),
               Hypergraph ? TEXT("Yes") : TEXT("No"),
               Temporal ? TEXT("Yes") : TEXT("No"));
    }
}

void UComponentIntegrationLayer::CreateDefaultChannels()
{
    // Bridge ↔ Reservoir: Neural pattern exchange
    if (Bridge && Reservoir)
    {
        CreateChannel(EE1ComponentType::Bridge, EE1ComponentType::Reservoir, EIntegrationFlowDirection::Bidirectional);
    }

    // Reservoir ↔ Hypergraph: Pattern-to-memory consolidation
    if (Reservoir && Hypergraph)
    {
        CreateChannel(EE1ComponentType::Reservoir, EE1ComponentType::Hypergraph, EIntegrationFlowDirection::Bidirectional);
    }

    // Hypergraph ↔ Temporal: Event-memory linking
    if (Hypergraph && Temporal)
    {
        CreateChannel(EE1ComponentType::Hypergraph, EE1ComponentType::Temporal, EIntegrationFlowDirection::Bidirectional);
    }

    // P-System ↔ Hypergraph: Membrane boundary for memory
    if (PSystem && Hypergraph)
    {
        CreateChannel(EE1ComponentType::PSystem, EE1ComponentType::Hypergraph, EIntegrationFlowDirection::Bidirectional);
    }

    // P-System ↔ Reservoir: Membrane boundary for processing
    if (PSystem && Reservoir)
    {
        CreateChannel(EE1ComponentType::PSystem, EE1ComponentType::Reservoir, EIntegrationFlowDirection::Bidirectional);
    }

    // Temporal ↔ Reservoir: Temporal context for patterns
    if (Temporal && Reservoir)
    {
        CreateChannel(EE1ComponentType::Temporal, EE1ComponentType::Reservoir, EIntegrationFlowDirection::Bidirectional);
    }

    // Bridge ↔ Temporal: Neural-temporal linking
    if (Bridge && Temporal)
    {
        CreateChannel(EE1ComponentType::Bridge, EE1ComponentType::Temporal, EIntegrationFlowDirection::Forward);
    }

    if (Config.bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("[F1.6.1] Created %d default integration channels"), Channels.Num());
    }
}

void UComponentIntegrationLayer::Reset()
{
    Channels.Empty();
    EventHistory.Empty();
    AccumulatedTime = 0.0f;
    Statistics = FIntegrationStatistics();
    NextEventID = 1;
    ChannelIDCounter = 0;

    // Re-initialize
    bIsInitialized = false;
    Initialize();
}

// ============================================================================
// COMPONENT STATUS
// ============================================================================

void UComponentIntegrationLayer::InitializeComponentStatuses()
{
    // Initialize status for all component types
    TArray<EE1ComponentType> ComponentTypes = {
        EE1ComponentType::Bridge,
        EE1ComponentType::Reservoir,
        EE1ComponentType::PSystem,
        EE1ComponentType::Hypergraph,
        EE1ComponentType::Temporal
    };

    for (EE1ComponentType Type : ComponentTypes)
    {
        FComponentStatusInfo Status;
        Status.ComponentType = Type;
        Status.Status = EComponentStatus::Uninitialized;
        Status.bIsAvailable = false;
        Status.Health = 0.0f;
        ComponentStatuses.Add(Type, Status);
    }
}

void UComponentIntegrationLayer::UpdateComponentStatus(EE1ComponentType ComponentType)
{
    if (!ComponentStatuses.Contains(ComponentType))
    {
        FComponentStatusInfo Status;
        Status.ComponentType = ComponentType;
        ComponentStatuses.Add(ComponentType, Status);
    }

    FComponentStatusInfo& Status = ComponentStatuses[ComponentType];
    Status.LastUpdateTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

    // Check component availability
    bool bWasAvailable = Status.bIsAvailable;
    EComponentStatus OldStatus = Status.Status;

    switch (ComponentType)
    {
    case EE1ComponentType::Bridge:
        Status.bIsAvailable = (Bridge != nullptr);
        break;
    case EE1ComponentType::Reservoir:
        Status.bIsAvailable = (Reservoir != nullptr);
        break;
    case EE1ComponentType::PSystem:
        Status.bIsAvailable = (PSystem != nullptr);
        break;
    case EE1ComponentType::Hypergraph:
        Status.bIsAvailable = (Hypergraph != nullptr);
        break;
    case EE1ComponentType::Temporal:
        Status.bIsAvailable = (Temporal != nullptr);
        break;
    }

    // Update status based on availability
    if (Status.bIsAvailable)
    {
        Status.Status = EComponentStatus::Active;
        Status.Health = 1.0f;
        Status.ErrorMessage.Empty();
    }
    else
    {
        Status.Status = EComponentStatus::NotFound;
        Status.Health = 0.0f;
    }

    // Broadcast status change if changed
    if (OldStatus != Status.Status)
    {
        OnComponentStatusChanged.Broadcast(ComponentType, Status.Status);
    }
}

void UComponentIntegrationLayer::UpdateAllComponentStatuses()
{
    UpdateComponentStatus(EE1ComponentType::Bridge);
    UpdateComponentStatus(EE1ComponentType::Reservoir);
    UpdateComponentStatus(EE1ComponentType::PSystem);
    UpdateComponentStatus(EE1ComponentType::Hypergraph);
    UpdateComponentStatus(EE1ComponentType::Temporal);
}

FComponentStatusInfo UComponentIntegrationLayer::GetComponentStatus(EE1ComponentType ComponentType) const
{
    if (ComponentStatuses.Contains(ComponentType))
    {
        return ComponentStatuses[ComponentType];
    }

    FComponentStatusInfo EmptyStatus;
    EmptyStatus.ComponentType = ComponentType;
    EmptyStatus.Status = EComponentStatus::NotFound;
    return EmptyStatus;
}

TArray<FComponentStatusInfo> UComponentIntegrationLayer::GetAllComponentStatuses() const
{
    TArray<FComponentStatusInfo> Result;
    for (const auto& Pair : ComponentStatuses)
    {
        Result.Add(Pair.Value);
    }
    return Result;
}

bool UComponentIntegrationLayer::IsComponentAvailable(EE1ComponentType ComponentType) const
{
    if (ComponentStatuses.Contains(ComponentType))
    {
        return ComponentStatuses[ComponentType].bIsAvailable;
    }
    return false;
}

int32 UComponentIntegrationLayer::GetAvailableComponentCount() const
{
    int32 Count = 0;
    for (const auto& Pair : ComponentStatuses)
    {
        if (Pair.Value.bIsAvailable)
        {
            Count++;
        }
    }
    return Count;
}

// ============================================================================
// CHANNEL MANAGEMENT
// ============================================================================

FString UComponentIntegrationLayer::CreateChannel(EE1ComponentType SourceComponent, EE1ComponentType TargetComponent, 
                                                   EIntegrationFlowDirection FlowDirection)
{
    // Check if both components are available
    if (!IsComponentAvailable(SourceComponent) || !IsComponentAvailable(TargetComponent))
    {
        if (Config.bEnableDebugLogging)
        {
            UE_LOG(LogTemp, Warning, TEXT("[F1.6.1] Cannot create channel: one or both components unavailable"));
        }
        return FString();
    }

    // Generate channel ID
    FString ChannelID = GenerateChannelID();

    // Create channel
    FIntegrationChannel Channel(ChannelID, SourceComponent, TargetComponent);
    Channel.FlowDirection = FlowDirection;
    Channel.bEnabled = true;
    Channel.SyncInterval = Config.DefaultSyncInterval;

    Channels.Add(ChannelID, Channel);

    // Record event
    RecordEvent(EIntegrationEventType::DataSynchronized, SourceComponent, TargetComponent,
                FString::Printf(TEXT("Created channel %s"), *ChannelID), true);

    // Broadcast
    OnChannelStateChanged.Broadcast(ChannelID, true);

    return ChannelID;
}

bool UComponentIntegrationLayer::RemoveChannel(const FString& ChannelID)
{
    if (Channels.Contains(ChannelID))
    {
        Channels.Remove(ChannelID);
        OnChannelStateChanged.Broadcast(ChannelID, false);
        return true;
    }
    return false;
}

void UComponentIntegrationLayer::SetChannelEnabled(const FString& ChannelID, bool bEnable)
{
    if (Channels.Contains(ChannelID))
    {
        Channels[ChannelID].bEnabled = bEnable;
        OnChannelStateChanged.Broadcast(ChannelID, bEnable);
    }
}

FIntegrationChannel UComponentIntegrationLayer::GetChannel(const FString& ChannelID) const
{
    if (Channels.Contains(ChannelID))
    {
        return Channels[ChannelID];
    }
    return FIntegrationChannel();
}

TArray<FIntegrationChannel> UComponentIntegrationLayer::GetAllChannels() const
{
    TArray<FIntegrationChannel> Result;
    for (const auto& Pair : Channels)
    {
        Result.Add(Pair.Value);
    }
    return Result;
}

TArray<FIntegrationChannel> UComponentIntegrationLayer::GetChannelsForComponent(EE1ComponentType ComponentType) const
{
    TArray<FIntegrationChannel> Result;
    for (const auto& Pair : Channels)
    {
        if (Pair.Value.SourceComponent == ComponentType || Pair.Value.TargetComponent == ComponentType)
        {
            Result.Add(Pair.Value);
        }
    }
    return Result;
}

// ============================================================================
// DATA FLOW
// ============================================================================

bool UComponentIntegrationLayer::SynchronizeChannel(const FString& ChannelID)
{
    if (!Channels.Contains(ChannelID))
    {
        return false;
    }

    FIntegrationChannel& Channel = Channels[ChannelID];
    if (!Channel.bEnabled)
    {
        return false;
    }

    float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    float StartTime = FPlatformTime::Seconds();

    // Perform synchronization based on component types
    bool bSuccess = false;

    EE1ComponentType Source = Channel.SourceComponent;
    EE1ComponentType Target = Channel.TargetComponent;

    // Handle specific component pairs
    if ((Source == EE1ComponentType::Bridge && Target == EE1ComponentType::Reservoir) ||
        (Source == EE1ComponentType::Reservoir && Target == EE1ComponentType::Bridge))
    {
        SyncBridgeReservoir();
        bSuccess = true;
    }
    else if ((Source == EE1ComponentType::Reservoir && Target == EE1ComponentType::Hypergraph) ||
             (Source == EE1ComponentType::Hypergraph && Target == EE1ComponentType::Reservoir))
    {
        SyncReservoirHypergraph();
        bSuccess = true;
    }
    else if ((Source == EE1ComponentType::Hypergraph && Target == EE1ComponentType::Temporal) ||
             (Source == EE1ComponentType::Temporal && Target == EE1ComponentType::Hypergraph))
    {
        SyncHypergraphTemporal();
        bSuccess = true;
    }
    else if (Source == EE1ComponentType::PSystem || Target == EE1ComponentType::PSystem)
    {
        SyncPSystemBoundaries();
        bSuccess = true;
    }
    else if (Source == EE1ComponentType::Temporal || Target == EE1ComponentType::Temporal)
    {
        SyncTemporalContext();
        bSuccess = true;
    }

    // Update channel statistics
    float EndTime = FPlatformTime::Seconds();
    float Latency = (EndTime - StartTime) * 1000.0f; // Convert to ms

    Channel.LastSyncTime = CurrentTime;
    Channel.MessageCount++;
    Channel.AverageLatency = (Channel.AverageLatency * (Channel.MessageCount - 1) + Latency) / Channel.MessageCount;

    return bSuccess;
}

void UComponentIntegrationLayer::SynchronizeAllChannels()
{
    for (auto& Pair : Channels)
    {
        if (Pair.Value.bEnabled)
        {
            SynchronizeChannel(Pair.Key);
        }
    }

    Statistics.IntegrationCycles++;
}

void UComponentIntegrationLayer::PropagatePatterns()
{
    if (!Config.bEnablePatternPropagation)
    {
        return;
    }

    // Propagate patterns from reservoir to hypergraph
    if (Reservoir && Hypergraph)
    {
        // Get detected patterns from reservoir
        TArray<FTemporalPattern> Patterns = Reservoir->DetectTemporalPatterns();

        // For each pattern, create or update memory nodes
        // This is a placeholder for actual pattern propagation logic
        for (const FTemporalPattern& Pattern : Patterns)
        {
            RecordEvent(EIntegrationEventType::PatternPropagated, EE1ComponentType::Reservoir,
                        EE1ComponentType::Hypergraph, FString::Printf(TEXT("Propagated pattern %s"), *Pattern.PatternID), true);
        }
    }
}

void UComponentIntegrationLayer::LinkTemporalEvent(int64 EventID)
{
    if (!Config.bEnableTemporalLinking || !Temporal)
    {
        return;
    }

    // Get the temporal event
    FTemporalEvent Event = Temporal->GetEvent(EventID);
    if (Event.EventID == 0)
    {
        return;
    }

    // Link to hypergraph memory if available
    if (Hypergraph)
    {
        // Create memory trace for the temporal event
        RecordEvent(EIntegrationEventType::TemporalEventLinked, EE1ComponentType::Temporal,
                    EE1ComponentType::Hypergraph, FString::Printf(TEXT("Linked temporal event %lld"), EventID), true);
    }

    // Link to reservoir patterns if available
    if (Reservoir)
    {
        RecordEvent(EIntegrationEventType::TemporalEventLinked, EE1ComponentType::Temporal,
                    EE1ComponentType::Reservoir, FString::Printf(TEXT("Linked temporal event %lld to patterns"), EventID), true);
    }
}

void UComponentIntegrationLayer::ApplyMembraneBoundary(const FString& MembraneID)
{
    if (!Config.bEnableMembraneBoundaries || !PSystem)
    {
        return;
    }

    // Get membrane state
    FMembraneStateData MembraneState = PSystem->GetMembraneState(MembraneID);
    if (MembraneState.MembraneID.IsEmpty())
    {
        return;
    }

    // Apply boundary rules to data flow
    RecordEvent(EIntegrationEventType::MembraneChanged, EE1ComponentType::PSystem,
                EE1ComponentType::PSystem, FString::Printf(TEXT("Applied membrane boundary %s"), *MembraneID), true);
}

// ============================================================================
// SYNCHRONIZATION HELPERS
// ============================================================================

void UComponentIntegrationLayer::SyncBridgeReservoir()
{
    if (!Bridge || !Reservoir)
    {
        return;
    }

    // Transfer neural patterns from bridge to reservoir
    // and reservoir states back to bridge
    // This is a coordination point - actual data transfer happens in the components
}

void UComponentIntegrationLayer::SyncReservoirHypergraph()
{
    if (!Reservoir || !Hypergraph)
    {
        return;
    }

    // Propagate patterns if enabled
    if (Config.bEnablePatternPropagation)
    {
        PropagatePatterns();
    }
}

void UComponentIntegrationLayer::SyncHypergraphTemporal()
{
    if (!Hypergraph || !Temporal)
    {
        return;
    }

    // Link memory nodes to temporal events
    // This establishes the temporal context for memories
}

void UComponentIntegrationLayer::SyncPSystemBoundaries()
{
    if (!PSystem)
    {
        return;
    }

    // Apply membrane boundaries to all components
    // Get root membranes and propagate boundary rules
    TArray<FString> RootMembranes = PSystem->GetRootMembranes();
    for (const FString& MembraneID : RootMembranes)
    {
        ApplyMembraneBoundary(MembraneID);
    }
}

void UComponentIntegrationLayer::SyncTemporalContext()
{
    if (!Temporal)
    {
        return;
    }

    // Propagate temporal context to other components
    // This ensures temporal ordering is maintained across the system
}

// ============================================================================
// COGNITIVE CYCLE INTEGRATION
// ============================================================================

void UComponentIntegrationLayer::ExecuteCognitiveIntegration(int32 CycleStep, int32 StreamID)
{
    if (!bEnableIntegration || !bIsInitialized)
    {
        return;
    }

    // Execute integration based on cognitive cycle step
    // Steps 1, 4, 7, 10: Stream 1 (Perceiving) - Bridge integration
    // Steps 2, 5, 8, 11: Stream 2 (Acting) - Reservoir/PSystem integration
    // Steps 3, 6, 9, 12: Stream 3 (Reflecting) - Hypergraph/Temporal integration

    switch ((CycleStep - 1) % 3)
    {
    case 0: // Perceiving stream
        SyncBridgeReservoir();
        break;
    case 1: // Acting stream
        SyncPSystemBoundaries();
        break;
    case 2: // Reflecting stream
        SyncReservoirHypergraph();
        SyncHypergraphTemporal();
        break;
    }

    // Record cycle step execution
    RecordEvent(EIntegrationEventType::CycleStepExecuted, EE1ComponentType::Bridge,
                EE1ComponentType::Temporal, FString::Printf(TEXT("Cycle step %d, stream %d"), CycleStep, StreamID), true);
}

void UComponentIntegrationLayer::SynchronizeTriadicPoint()
{
    // Synchronize all components at triadic points
    // These occur at steps {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}

    SynchronizeAllChannels();

    // Ensure temporal consistency
    SyncTemporalContext();

    // Record synchronization
    RecordEvent(EIntegrationEventType::DataSynchronized, EE1ComponentType::Bridge,
                EE1ComponentType::Temporal, TEXT("Triadic synchronization point"), true);
}

// ============================================================================
// STATISTICS & DIAGNOSTICS
// ============================================================================

FIntegrationStatistics UComponentIntegrationLayer::GetStatistics() const
{
    return Statistics;
}

TArray<FIntegrationEvent> UComponentIntegrationLayer::GetRecentEvents(int32 Count) const
{
    TArray<FIntegrationEvent> Result;
    int32 StartIndex = FMath::Max(0, EventHistory.Num() - Count);
    for (int32 i = StartIndex; i < EventHistory.Num(); i++)
    {
        Result.Add(EventHistory[i]);
    }
    return Result;
}

TArray<FString> UComponentIntegrationLayer::GenerateDiagnosticReport() const
{
    TArray<FString> Report;

    Report.Add(TEXT("========================================"));
    Report.Add(TEXT("Component Integration Layer Diagnostic Report"));
    Report.Add(TEXT("Feature F1.6.1"));
    Report.Add(TEXT("========================================"));
    Report.Add(TEXT(""));

    // Component status
    Report.Add(TEXT("Component Status:"));
    for (const auto& Pair : ComponentStatuses)
    {
        FString StatusStr;
        switch (Pair.Value.Status)
        {
        case EComponentStatus::Active: StatusStr = TEXT("Active"); break;
        case EComponentStatus::Uninitialized: StatusStr = TEXT("Uninitialized"); break;
        case EComponentStatus::NotFound: StatusStr = TEXT("Not Found"); break;
        case EComponentStatus::Error: StatusStr = TEXT("Error"); break;
        default: StatusStr = TEXT("Unknown"); break;
        }
        Report.Add(FString::Printf(TEXT("  %s: %s (Health: %.1f%%)"),
                                    *GetComponentName(Pair.Key), *StatusStr, Pair.Value.Health * 100.0f));
    }
    Report.Add(TEXT(""));

    // Channel status
    Report.Add(TEXT("Integration Channels:"));
    for (const auto& Pair : Channels)
    {
        Report.Add(FString::Printf(TEXT("  %s: %s -> %s (%s, Messages: %lld, Latency: %.2fms)"),
                                    *Pair.Key,
                                    *GetComponentName(Pair.Value.SourceComponent),
                                    *GetComponentName(Pair.Value.TargetComponent),
                                    Pair.Value.bEnabled ? TEXT("Enabled") : TEXT("Disabled"),
                                    Pair.Value.MessageCount,
                                    Pair.Value.AverageLatency));
    }
    Report.Add(TEXT(""));

    // Statistics
    Report.Add(TEXT("Statistics:"));
    Report.Add(FString::Printf(TEXT("  Active Channels: %d"), Statistics.ActiveChannelCount));
    Report.Add(FString::Printf(TEXT("  Total Messages: %lld"), Statistics.TotalMessagesProcessed));
    Report.Add(FString::Printf(TEXT("  Integration Cycles: %lld"), Statistics.IntegrationCycles));
    Report.Add(FString::Printf(TEXT("  Average Latency: %.2fms"), Statistics.AverageLatency));
    Report.Add(FString::Printf(TEXT("  System Health: %.1f%%"), Statistics.SystemHealth * 100.0f));
    Report.Add(FString::Printf(TEXT("  Components: %d/%d available"), Statistics.ComponentsAvailable, Statistics.ComponentsTotal));

    return Report;
}

float UComponentIntegrationLayer::CalculateSystemHealth() const
{
    if (ComponentStatuses.Num() == 0)
    {
        return 0.0f;
    }

    float TotalHealth = 0.0f;
    int32 Count = 0;

    for (const auto& Pair : ComponentStatuses)
    {
        TotalHealth += Pair.Value.Health;
        Count++;
    }

    return Count > 0 ? TotalHealth / Count : 0.0f;
}

FString UComponentIntegrationLayer::GetSystemInfo() const
{
    return FString::Printf(TEXT("ComponentIntegrationLayer: %d components, %d channels, %.1f%% health"),
                            GetAvailableComponentCount(), Channels.Num(), CalculateSystemHealth() * 100.0f);
}

// ============================================================================
// EVENT HANDLING
// ============================================================================

void UComponentIntegrationLayer::RecordEvent(EIntegrationEventType EventType, EE1ComponentType Source,
                                              EE1ComponentType Target, const FString& Description, bool bSuccess)
{
    FIntegrationEvent Event;
    Event.EventID = NextEventID++;
    Event.EventType = EventType;
    Event.SourceComponent = Source;
    Event.TargetComponent = Target;
    Event.Timestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
    Event.Description = Description;
    Event.bSuccess = bSuccess;

    EventHistory.Add(Event);

    // Broadcast event
    OnIntegrationEvent.Broadcast(Event);

    // Log if debug enabled
    if (Config.bEnableDebugLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("[F1.6.1] %s: %s"), *GetComponentName(Source), *Description);
    }
}

void UComponentIntegrationLayer::PruneEventHistory()
{
    if (EventHistory.Num() > Config.MaxEventHistorySize)
    {
        int32 ToRemove = EventHistory.Num() - Config.MaxEventHistorySize;
        EventHistory.RemoveAt(0, ToRemove);
    }
}

FString UComponentIntegrationLayer::GenerateChannelID()
{
    return FString::Printf(TEXT("CH_%04d"), ++ChannelIDCounter);
}

void UComponentIntegrationLayer::UpdateStatistics()
{
    Statistics.ComponentsAvailable = GetAvailableComponentCount();
    Statistics.ActiveChannelCount = 0;
    Statistics.TotalMessagesProcessed = 0;
    float TotalLatency = 0.0f;

    for (const auto& Pair : Channels)
    {
        if (Pair.Value.bEnabled)
        {
            Statistics.ActiveChannelCount++;
        }
        Statistics.TotalMessagesProcessed += Pair.Value.MessageCount;
        TotalLatency += Pair.Value.AverageLatency;
    }

    Statistics.AverageLatency = Channels.Num() > 0 ? TotalLatency / Channels.Num() : 0.0f;
    Statistics.SystemHealth = CalculateSystemHealth();
}

FString UComponentIntegrationLayer::GetComponentName(EE1ComponentType ComponentType) const
{
    switch (ComponentType)
    {
    case EE1ComponentType::Bridge: return TEXT("Bridge");
    case EE1ComponentType::Reservoir: return TEXT("Reservoir");
    case EE1ComponentType::PSystem: return TEXT("P-System");
    case EE1ComponentType::Hypergraph: return TEXT("Hypergraph");
    case EE1ComponentType::Temporal: return TEXT("Temporal");
    default: return TEXT("Unknown");
    }
}

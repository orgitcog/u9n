// ComponentIntegrationLayer.h
// Feature F1.6.1: Unified Integration Layer for E1 Components
// Connects bridge, reservoir, P-system, hypergraph, and temporal components
// Copyright (c) 2025-2026 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ComponentIntegrationLayer.generated.h"

// Forward declarations for E1 components
class UNeuroSymbolicBridge;
class UDeepTreeEchoReservoir;
class UMembraneHierarchyManager;
class UHypergraphMemorySystem;
class UTemporalEventGraph;
class UCognitiveCycleManager;
class UDeepTreeEchoCore;

// ============================================================================
// ENUMERATIONS
// ============================================================================

/**
 * E1 Component Types
 */
UENUM(BlueprintType)
enum class EE1ComponentType : uint8
{
    /** Neural-Symbolic Bridge (F1.1) */
    Bridge          UMETA(DisplayName = "Bridge"),
    
    /** Reservoir Computing (F1.2) */
    Reservoir       UMETA(DisplayName = "Reservoir"),
    
    /** P-System Membrane (F1.3) */
    PSystem         UMETA(DisplayName = "P-System"),
    
    /** Hypergraph Memory (F1.4) */
    Hypergraph      UMETA(DisplayName = "Hypergraph"),
    
    /** Temporal Event Graph (F1.5) */
    Temporal        UMETA(DisplayName = "Temporal")
};

/**
 * Integration Data Flow Direction
 */
UENUM(BlueprintType)
enum class EIntegrationFlowDirection : uint8
{
    /** Data flows from source to target */
    Forward         UMETA(DisplayName = "Forward"),
    
    /** Data flows from target to source */
    Backward        UMETA(DisplayName = "Backward"),
    
    /** Bidirectional data flow */
    Bidirectional   UMETA(DisplayName = "Bidirectional")
};

/**
 * Integration Event Types
 */
UENUM(BlueprintType)
enum class EIntegrationEventType : uint8
{
    /** Component initialized */
    ComponentInitialized    UMETA(DisplayName = "Component Initialized"),
    
    /** Data synchronized between components */
    DataSynchronized        UMETA(DisplayName = "Data Synchronized"),
    
    /** Pattern detected and propagated */
    PatternPropagated       UMETA(DisplayName = "Pattern Propagated"),
    
    /** Memory consolidated across systems */
    MemoryConsolidated      UMETA(DisplayName = "Memory Consolidated"),
    
    /** Temporal event linked */
    TemporalEventLinked     UMETA(DisplayName = "Temporal Event Linked"),
    
    /** Membrane state changed */
    MembraneChanged         UMETA(DisplayName = "Membrane Changed"),
    
    /** Cognitive cycle step executed */
    CycleStepExecuted       UMETA(DisplayName = "Cycle Step Executed"),
    
    /** Integration error occurred */
    IntegrationError        UMETA(DisplayName = "Integration Error")
};

/**
 * Component Status
 */
UENUM(BlueprintType)
enum class EComponentStatus : uint8
{
    /** Component not initialized */
    Uninitialized   UMETA(DisplayName = "Uninitialized"),
    
    /** Component initializing */
    Initializing    UMETA(DisplayName = "Initializing"),
    
    /** Component ready and active */
    Active          UMETA(DisplayName = "Active"),
    
    /** Component suspended */
    Suspended       UMETA(DisplayName = "Suspended"),
    
    /** Component error state */
    Error           UMETA(DisplayName = "Error"),
    
    /** Component not found */
    NotFound        UMETA(DisplayName = "Not Found")
};

// ============================================================================
// STRUCTURES
// ============================================================================

/**
 * Integration Channel - Connection between two E1 components
 */
USTRUCT(BlueprintType)
struct FIntegrationChannel
{
    GENERATED_BODY()

    /** Unique channel identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Channel")
    FString ChannelID;

    /** Source component type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Channel")
    EE1ComponentType SourceComponent = EE1ComponentType::Bridge;

    /** Target component type */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Channel")
    EE1ComponentType TargetComponent = EE1ComponentType::Reservoir;

    /** Data flow direction */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Channel")
    EIntegrationFlowDirection FlowDirection = EIntegrationFlowDirection::Bidirectional;

    /** Channel enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Channel")
    bool bEnabled = true;

    /** Synchronization interval (seconds, 0 = every tick) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Channel", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float SyncInterval = 0.0f;

    /** Last synchronization time */
    UPROPERTY(BlueprintReadOnly, Category = "Channel")
    float LastSyncTime = 0.0f;

    /** Messages transferred count */
    UPROPERTY(BlueprintReadOnly, Category = "Channel")
    int64 MessageCount = 0;

    /** Average latency (ms) */
    UPROPERTY(BlueprintReadOnly, Category = "Channel")
    float AverageLatency = 0.0f;

    FIntegrationChannel() = default;

    FIntegrationChannel(const FString& InID, EE1ComponentType InSource, EE1ComponentType InTarget)
        : ChannelID(InID)
        , SourceComponent(InSource)
        , TargetComponent(InTarget)
    {}
};

/**
 * Component Status Info
 */
USTRUCT(BlueprintType)
struct FComponentStatusInfo
{
    GENERATED_BODY()

    /** Component type */
    UPROPERTY(BlueprintReadOnly, Category = "Status")
    EE1ComponentType ComponentType = EE1ComponentType::Bridge;

    /** Current status */
    UPROPERTY(BlueprintReadOnly, Category = "Status")
    EComponentStatus Status = EComponentStatus::Uninitialized;

    /** Is component available */
    UPROPERTY(BlueprintReadOnly, Category = "Status")
    bool bIsAvailable = false;

    /** Component health (0.0 - 1.0) */
    UPROPERTY(BlueprintReadOnly, Category = "Status")
    float Health = 0.0f;

    /** Last update timestamp */
    UPROPERTY(BlueprintReadOnly, Category = "Status")
    float LastUpdateTime = 0.0f;

    /** Error message (if any) */
    UPROPERTY(BlueprintReadOnly, Category = "Status")
    FString ErrorMessage;

    /** Processing latency (ms) */
    UPROPERTY(BlueprintReadOnly, Category = "Status")
    float ProcessingLatency = 0.0f;
};

/**
 * Integration Event
 */
USTRUCT(BlueprintType)
struct FIntegrationEvent
{
    GENERATED_BODY()

    /** Event ID */
    UPROPERTY(BlueprintReadWrite, Category = "Event")
    int64 EventID = 0;

    /** Event type */
    UPROPERTY(BlueprintReadWrite, Category = "Event")
    EIntegrationEventType EventType = EIntegrationEventType::ComponentInitialized;

    /** Source component */
    UPROPERTY(BlueprintReadWrite, Category = "Event")
    EE1ComponentType SourceComponent = EE1ComponentType::Bridge;

    /** Target component (if applicable) */
    UPROPERTY(BlueprintReadWrite, Category = "Event")
    EE1ComponentType TargetComponent = EE1ComponentType::Bridge;

    /** Event timestamp */
    UPROPERTY(BlueprintReadWrite, Category = "Event")
    float Timestamp = 0.0f;

    /** Event description */
    UPROPERTY(BlueprintReadWrite, Category = "Event")
    FString Description;

    /** Event data (key-value pairs) */
    UPROPERTY(BlueprintReadWrite, Category = "Event")
    TMap<FString, FString> EventData;

    /** Success status */
    UPROPERTY(BlueprintReadWrite, Category = "Event")
    bool bSuccess = true;
};

/**
 * Integration Statistics
 */
USTRUCT(BlueprintType)
struct FIntegrationStatistics
{
    GENERATED_BODY()

    /** Total active channels */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 ActiveChannelCount = 0;

    /** Total messages processed */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int64 TotalMessagesProcessed = 0;

    /** Average integration latency (ms) */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float AverageLatency = 0.0f;

    /** Integration cycles completed */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int64 IntegrationCycles = 0;

    /** Components available */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 ComponentsAvailable = 0;

    /** Components total */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 ComponentsTotal = 5; // Bridge, Reservoir, PSystem, Hypergraph, Temporal

    /** Overall system health (0.0 - 1.0) */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    float SystemHealth = 0.0f;

    /** Errors in last minute */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int32 RecentErrorCount = 0;

    /** Memory usage (bytes) */
    UPROPERTY(BlueprintReadOnly, Category = "Stats")
    int64 MemoryUsage = 0;
};

/**
 * Integration Configuration
 */
USTRUCT(BlueprintType)
struct FIntegrationConfig
{
    GENERATED_BODY()

    /** Enable automatic component discovery */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bAutoDiscoverComponents = true;

    /** Enable automatic channel creation */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bAutoCreateChannels = true;

    /** Default synchronization interval (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "0.0", ClampMax = "10.0"))
    float DefaultSyncInterval = 0.1f;

    /** Enable pattern propagation between reservoir and hypergraph */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bEnablePatternPropagation = true;

    /** Enable temporal linking of events */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bEnableTemporalLinking = true;

    /** Enable membrane boundary integration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bEnableMembraneBoundaries = true;

    /** Maximum event history size */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (ClampMin = "100", ClampMax = "100000"))
    int32 MaxEventHistorySize = 10000;

    /** Enable debug logging */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bEnableDebugLogging = false;
};

// ============================================================================
// DELEGATES
// ============================================================================

/** Delegate for integration events */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnIntegrationEvent, const FIntegrationEvent&, Event);

/** Delegate for component status changes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnComponentStatusChanged, EE1ComponentType, Component, EComponentStatus, NewStatus);

/** Delegate for channel state changes */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnChannelStateChanged, const FString&, ChannelID, bool, bEnabled);

// ============================================================================
// COMPONENT INTEGRATION LAYER
// ============================================================================

/**
 * Component Integration Layer
 *
 * Feature F1.6.1: Unified integration layer that connects all E1 components:
 * - Neural-Symbolic Bridge (F1.1)
 * - Reservoir Computing (F1.2)
 * - P-System Membrane (F1.3)
 * - Hypergraph Memory (F1.4)
 * - Temporal Event Graph (F1.5)
 *
 * Key Features:
 * - Automatic component discovery and linking
 * - Bidirectional data flow channels
 * - Pattern propagation between reservoir and memory systems
 * - Temporal event linking across components
 * - Membrane boundary coordination
 * - Cognitive cycle synchronization
 * - Health monitoring and diagnostics
 *
 * Integration Architecture:
 * - Bridge ↔ Reservoir: Neural pattern exchange
 * - Reservoir ↔ Hypergraph: Pattern-to-memory consolidation
 * - Hypergraph ↔ Temporal: Event-memory linking
 * - P-System ↔ All: Membrane boundary isolation
 * - Temporal ↔ All: Temporal context propagation
 *
 * @author Deep Tree Echo Team
 * @date March 2026
 * @version 1.0.0
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UComponentIntegrationLayer : public UActorComponent
{
    GENERATED_BODY()

public:
    UComponentIntegrationLayer();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Integration configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration|Config")
    FIntegrationConfig Config;

    /** Enable integration processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Integration|Config")
    bool bEnableIntegration = true;

    // ========================================
    // COMPONENT REFERENCES
    // ========================================

    /** Neural-Symbolic Bridge reference */
    UPROPERTY(BlueprintReadOnly, Category = "Integration|Components")
    UNeuroSymbolicBridge* Bridge;

    /** Reservoir Computing reference */
    UPROPERTY(BlueprintReadOnly, Category = "Integration|Components")
    UDeepTreeEchoReservoir* Reservoir;

    /** P-System Membrane Manager reference */
    UPROPERTY(BlueprintReadOnly, Category = "Integration|Components")
    UMembraneHierarchyManager* PSystem;

    /** Hypergraph Memory System reference */
    UPROPERTY(BlueprintReadOnly, Category = "Integration|Components")
    UHypergraphMemorySystem* Hypergraph;

    /** Temporal Event Graph reference */
    UPROPERTY(BlueprintReadOnly, Category = "Integration|Components")
    UTemporalEventGraph* Temporal;

    /** Cognitive Cycle Manager reference (optional) */
    UPROPERTY(BlueprintReadOnly, Category = "Integration|Components")
    UCognitiveCycleManager* CognitiveCycle;

    /** Deep Tree Echo Core reference (optional) */
    UPROPERTY(BlueprintReadOnly, Category = "Integration|Components")
    UDeepTreeEchoCore* DTECore;

    // ========================================
    // STATE
    // ========================================

    /** Current integration statistics */
    UPROPERTY(BlueprintReadOnly, Category = "Integration|State")
    FIntegrationStatistics Statistics;

    // ========================================
    // EVENTS
    // ========================================

    /** Called when an integration event occurs */
    UPROPERTY(BlueprintAssignable, Category = "Integration|Events")
    FOnIntegrationEvent OnIntegrationEvent;

    /** Called when a component status changes */
    UPROPERTY(BlueprintAssignable, Category = "Integration|Events")
    FOnComponentStatusChanged OnComponentStatusChanged;

    /** Called when a channel state changes */
    UPROPERTY(BlueprintAssignable, Category = "Integration|Events")
    FOnChannelStateChanged OnChannelStateChanged;

    // ========================================
    // PUBLIC API - INITIALIZATION
    // ========================================

    /**
     * Initialize the integration layer
     * Discovers components and creates channels
     */
    UFUNCTION(BlueprintCallable, Category = "Integration")
    void Initialize();

    /**
     * Discover and link E1 components on the owner actor
     */
    UFUNCTION(BlueprintCallable, Category = "Integration")
    void DiscoverComponents();

    /**
     * Create default integration channels between components
     */
    UFUNCTION(BlueprintCallable, Category = "Integration")
    void CreateDefaultChannels();

    /**
     * Reset integration state
     */
    UFUNCTION(BlueprintCallable, Category = "Integration")
    void Reset();

    // ========================================
    // PUBLIC API - COMPONENT ACCESS
    // ========================================

    /**
     * Get component status
     * @param ComponentType Component to query
     * @return Component status info
     */
    UFUNCTION(BlueprintCallable, Category = "Integration|Components")
    FComponentStatusInfo GetComponentStatus(EE1ComponentType ComponentType) const;

    /**
     * Get all component statuses
     * @return Array of status info for all components
     */
    UFUNCTION(BlueprintCallable, Category = "Integration|Components")
    TArray<FComponentStatusInfo> GetAllComponentStatuses() const;

    /**
     * Check if a component is available
     * @param ComponentType Component to check
     * @return True if component is available and active
     */
    UFUNCTION(BlueprintPure, Category = "Integration|Components")
    bool IsComponentAvailable(EE1ComponentType ComponentType) const;

    /**
     * Get number of available components
     */
    UFUNCTION(BlueprintPure, Category = "Integration|Components")
    int32 GetAvailableComponentCount() const;

    // ========================================
    // PUBLIC API - CHANNEL MANAGEMENT
    // ========================================

    /**
     * Create an integration channel between two components
     * @param SourceComponent Source component type
     * @param TargetComponent Target component type
     * @param FlowDirection Data flow direction
     * @return Channel ID
     */
    UFUNCTION(BlueprintCallable, Category = "Integration|Channels")
    FString CreateChannel(EE1ComponentType SourceComponent, EE1ComponentType TargetComponent, 
                          EIntegrationFlowDirection FlowDirection = EIntegrationFlowDirection::Bidirectional);

    /**
     * Remove an integration channel
     * @param ChannelID Channel to remove
     * @return True if removed
     */
    UFUNCTION(BlueprintCallable, Category = "Integration|Channels")
    bool RemoveChannel(const FString& ChannelID);

    /**
     * Enable or disable a channel
     * @param ChannelID Channel to modify
     * @param bEnable Enable state
     */
    UFUNCTION(BlueprintCallable, Category = "Integration|Channels")
    void SetChannelEnabled(const FString& ChannelID, bool bEnable);

    /**
     * Get channel info
     * @param ChannelID Channel to query
     * @return Channel info
     */
    UFUNCTION(BlueprintCallable, Category = "Integration|Channels")
    FIntegrationChannel GetChannel(const FString& ChannelID) const;

    /**
     * Get all active channels
     * @return Array of active channels
     */
    UFUNCTION(BlueprintCallable, Category = "Integration|Channels")
    TArray<FIntegrationChannel> GetAllChannels() const;

    /**
     * Get channels for a specific component
     * @param ComponentType Component to query
     * @return Channels connected to the component
     */
    UFUNCTION(BlueprintCallable, Category = "Integration|Channels")
    TArray<FIntegrationChannel> GetChannelsForComponent(EE1ComponentType ComponentType) const;

    // ========================================
    // PUBLIC API - DATA FLOW
    // ========================================

    /**
     * Synchronize data between components via a channel
     * @param ChannelID Channel to use
     * @return True if synchronization succeeded
     */
    UFUNCTION(BlueprintCallable, Category = "Integration|DataFlow")
    bool SynchronizeChannel(const FString& ChannelID);

    /**
     * Synchronize all active channels
     */
    UFUNCTION(BlueprintCallable, Category = "Integration|DataFlow")
    void SynchronizeAllChannels();

    /**
     * Propagate patterns from reservoir to hypergraph memory
     */
    UFUNCTION(BlueprintCallable, Category = "Integration|DataFlow")
    void PropagatePatterns();

    /**
     * Link temporal events across components
     * @param EventID Temporal event ID
     */
    UFUNCTION(BlueprintCallable, Category = "Integration|DataFlow")
    void LinkTemporalEvent(int64 EventID);

    /**
     * Apply membrane boundaries to data flow
     * @param MembraneID Membrane to apply
     */
    UFUNCTION(BlueprintCallable, Category = "Integration|DataFlow")
    void ApplyMembraneBoundary(const FString& MembraneID);

    // ========================================
    // PUBLIC API - COGNITIVE CYCLE INTEGRATION
    // ========================================

    /**
     * Execute integration for current cognitive cycle step
     * @param CycleStep Current step (1-12)
     * @param StreamID Active stream (1-3)
     */
    UFUNCTION(BlueprintCallable, Category = "Integration|Cognitive")
    void ExecuteCognitiveIntegration(int32 CycleStep, int32 StreamID);

    /**
     * Synchronize at triadic point
     * Called at steps {1,5,9}, {2,6,10}, {3,7,11}, {4,8,12}
     */
    UFUNCTION(BlueprintCallable, Category = "Integration|Cognitive")
    void SynchronizeTriadicPoint();

    // ========================================
    // PUBLIC API - STATISTICS & DIAGNOSTICS
    // ========================================

    /**
     * Get integration statistics
     */
    UFUNCTION(BlueprintPure, Category = "Integration|Stats")
    FIntegrationStatistics GetStatistics() const;

    /**
     * Get recent integration events
     * @param Count Number of events to return
     * @return Recent events
     */
    UFUNCTION(BlueprintCallable, Category = "Integration|Stats")
    TArray<FIntegrationEvent> GetRecentEvents(int32 Count = 100) const;

    /**
     * Generate diagnostic report
     * @return Array of diagnostic messages
     */
    UFUNCTION(BlueprintCallable, Category = "Integration|Stats")
    TArray<FString> GenerateDiagnosticReport() const;

    /**
     * Calculate system health
     * @return Overall health (0.0 - 1.0)
     */
    UFUNCTION(BlueprintPure, Category = "Integration|Stats")
    float CalculateSystemHealth() const;

    /**
     * Get system info as string
     */
    UFUNCTION(BlueprintCallable, Category = "Integration|Stats")
    FString GetSystemInfo() const;

protected:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Component status map */
    TMap<EE1ComponentType, FComponentStatusInfo> ComponentStatuses;

    /** Integration channels */
    TMap<FString, FIntegrationChannel> Channels;

    /** Event history */
    TArray<FIntegrationEvent> EventHistory;

    /** Next event ID */
    int64 NextEventID = 1;

    /** Channel ID counter */
    int32 ChannelIDCounter = 0;

    /** Accumulated time for synchronization */
    float AccumulatedTime = 0.0f;

    /** System initialized flag */
    bool bIsInitialized = false;

    // ========================================
    // INTERNAL METHODS - INITIALIZATION
    // ========================================

    /** Initialize component statuses */
    void InitializeComponentStatuses();

    /** Update component status */
    void UpdateComponentStatus(EE1ComponentType ComponentType);

    /** Update all component statuses */
    void UpdateAllComponentStatuses();

    // ========================================
    // INTERNAL METHODS - SYNCHRONIZATION
    // ========================================

    /** Synchronize bridge and reservoir */
    void SyncBridgeReservoir();

    /** Synchronize reservoir and hypergraph */
    void SyncReservoirHypergraph();

    /** Synchronize hypergraph and temporal */
    void SyncHypergraphTemporal();

    /** Synchronize P-system boundaries */
    void SyncPSystemBoundaries();

    /** Synchronize temporal context */
    void SyncTemporalContext();

    // ========================================
    // INTERNAL METHODS - EVENT HANDLING
    // ========================================

    /** Record an integration event */
    void RecordEvent(EIntegrationEventType EventType, EE1ComponentType Source, 
                     EE1ComponentType Target, const FString& Description, bool bSuccess = true);

    /** Prune old events from history */
    void PruneEventHistory();

    /** Generate unique channel ID */
    FString GenerateChannelID();

    /** Update statistics */
    void UpdateStatistics();

    /** Get component name as string */
    FString GetComponentName(EE1ComponentType ComponentType) const;
};

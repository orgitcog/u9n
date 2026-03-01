#pragma once

/**
 * Membrane Communication Channels - P-System Symport/Antiport Transport
 * 
 * Feature F1.3.4: Implements symport/antiport communication channels between 
 * adjacent membranes in the P-System membrane architecture.
 * 
 * Symport: Co-transport of objects in the same direction across a membrane
 * Antiport: Counter-transport of objects in opposite directions across a membrane
 * 
 * Scientific Foundation:
 * - Păun, G. (2002): "Membrane Computing: An Introduction"
 * - Paun, G. & Rozenberg, G. (2002): "A Guide to Membrane Computing"
 * - Bernardini, F. & Gheorghe, M. (2004): "Population P Systems"
 * 
 * Integration with Deep Tree Echo:
 * - Provides inter-membrane communication for P-System architecture
 * - Supports hierarchical boundary management in cognitive processing
 * - Enables selective transport based on rules and affinities
 * - Compatible with 12-step cognitive cycle
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MembraneCommChannels.generated.h"

/**
 * Channel Transport Direction
 */
UENUM(BlueprintType)
enum class EChannelDirection : uint8
{
    /** Transport objects inward (parent -> child membrane) */
    Inward   UMETA(DisplayName = "Inward"),
    
    /** Transport objects outward (child -> parent membrane) */
    Outward  UMETA(DisplayName = "Outward"),
    
    /** Bidirectional transport based on gradient */
    Bidirectional UMETA(DisplayName = "Bidirectional")
};

/**
 * Channel Type - Symport or Antiport
 */
UENUM(BlueprintType)
enum class EChannelType : uint8
{
    /** Symport: Co-transport of objects in same direction */
    Symport   UMETA(DisplayName = "Symport"),
    
    /** Antiport: Counter-transport of objects in opposite directions */
    Antiport  UMETA(DisplayName = "Antiport"),
    
    /** Uniport: Single object transport (simple diffusion) */
    Uniport   UMETA(DisplayName = "Uniport")
};

/**
 * Channel State - Current operational status
 */
UENUM(BlueprintType)
enum class EChannelState : uint8
{
    /** Channel is closed, no transport possible */
    Closed    UMETA(DisplayName = "Closed"),
    
    /** Channel is open and ready for transport */
    Open      UMETA(DisplayName = "Open"),
    
    /** Channel is in refractory period */
    Refractory UMETA(DisplayName = "Refractory"),
    
    /** Channel is saturated, transport rate limited */
    Saturated UMETA(DisplayName = "Saturated"),
    
    /** Channel is blocked by inhibitor */
    Blocked   UMETA(DisplayName = "Blocked")
};

/**
 * Membrane Object - Item that can be transported across membranes
 */
USTRUCT(BlueprintType)
struct FMembraneObject
{
    GENERATED_BODY()

    /** Unique identifier for this object */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ObjectID;

    /** Object type/symbol (e.g., "a", "b", "signal_A") */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Symbol;

    /** Object multiplicity (count of identical objects) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Multiplicity = 1;

    /** Object priority for transport */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Priority = 1.0f;

    /** Energy cost for transport */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnergyCost = 1.0f;

    /** Payload data associated with this object */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<float> Payload;

    /** Timestamp when object was created */
    UPROPERTY(BlueprintReadOnly)
    float CreationTime = 0.0f;

    /** Is this object a catalyst (not consumed in reactions) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsCatalyst = false;
};

/**
 * Transport Rule - Defines conditions for symport/antiport transport
 */
USTRUCT(BlueprintType)
struct FTransportRule
{
    GENERATED_BODY()

    /** Rule identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString RuleID;

    /** Objects required on the source side (consumed) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> SourceObjects;

    /** Objects required on the target side (for antiport) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> TargetObjects;

    /** Objects produced on the target side */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> ProducedOnTarget;

    /** Objects produced on the source side (for antiport) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> ProducedOnSource;

    /** Channel type this rule applies to */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EChannelType ChannelType = EChannelType::Symport;

    /** Direction of primary transport */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EChannelDirection Direction = EChannelDirection::Outward;

    /** Priority of this rule (higher = applied first) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Priority = 0;

    /** Is this rule enabled */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnabled = true;

    /** Required catalyst symbols (not consumed) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> RequiredCatalysts;

    /** Minimum energy required for transport */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MinimumEnergy = 0.0f;

    /** Rule application probability (stochastic rules) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ApplicationProbability = 1.0f;
};

/**
 * Transport Event - Record of a transport operation
 */
USTRUCT(BlueprintType)
struct FTransportEvent
{
    GENERATED_BODY()

    /** Event ID */
    UPROPERTY(BlueprintReadOnly)
    FString EventID;

    /** Rule that triggered this transport */
    UPROPERTY(BlueprintReadOnly)
    FString RuleID;

    /** Channel ID through which transport occurred */
    UPROPERTY(BlueprintReadOnly)
    FString ChannelID;

    /** Source membrane ID */
    UPROPERTY(BlueprintReadOnly)
    FString SourceMembraneID;

    /** Target membrane ID */
    UPROPERTY(BlueprintReadOnly)
    FString TargetMembraneID;

    /** Objects transported from source to target */
    UPROPERTY(BlueprintReadOnly)
    TArray<FMembraneObject> TransportedToTarget;

    /** Objects transported from target to source (antiport) */
    UPROPERTY(BlueprintReadOnly)
    TArray<FMembraneObject> TransportedToSource;

    /** Timestamp of transport */
    UPROPERTY(BlueprintReadOnly)
    float Timestamp = 0.0f;

    /** Energy consumed by transport */
    UPROPERTY(BlueprintReadOnly)
    float EnergyConsumed = 0.0f;

    /** Was transport successful */
    UPROPERTY(BlueprintReadOnly)
    bool bSuccessful = false;
};

/**
 * Communication Channel - Single channel between two adjacent membranes
 */
USTRUCT(BlueprintType)
struct FCommunicationChannel
{
    GENERATED_BODY()

    /** Unique channel identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ChannelID;

    /** Channel type (symport/antiport/uniport) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EChannelType ChannelType = EChannelType::Symport;

    /** Current channel state */
    UPROPERTY(BlueprintReadOnly)
    EChannelState State = EChannelState::Closed;

    /** Default direction for transport */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EChannelDirection DefaultDirection = EChannelDirection::Outward;

    /** ID of membrane on one side (parent/outer) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MembraneA_ID;

    /** ID of membrane on other side (child/inner) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MembraneB_ID;

    /** Maximum transport rate (objects per second) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxTransportRate = 10.0f;

    /** Current transport rate */
    UPROPERTY(BlueprintReadOnly)
    float CurrentTransportRate = 0.0f;

    /** Channel selectivity - which object symbols can pass */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> SelectiveFor;

    /** Channel affinity map (symbol -> affinity strength) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TMap<FString, float> AffinityMap;

    /** Transport rules for this channel */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FTransportRule> TransportRules;

    /** Queue of objects waiting to be transported */
    UPROPERTY(BlueprintReadOnly)
    TArray<FMembraneObject> TransportQueue;

    /** Refractory period after transport (seconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float RefractoryPeriod = 0.1f;

    /** Time remaining in refractory period */
    UPROPERTY(BlueprintReadOnly)
    float RefractoryTimer = 0.0f;

    /** Total objects transported through this channel */
    UPROPERTY(BlueprintReadOnly)
    int32 TotalTransportCount = 0;

    /** Time of last transport */
    UPROPERTY(BlueprintReadOnly)
    float LastTransportTime = 0.0f;
};

/**
 * Membrane State - State of a single membrane in the hierarchy
 */
USTRUCT(BlueprintType)
struct FMembraneState
{
    GENERATED_BODY()

    /** Unique membrane identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString MembraneID;

    /** Parent membrane ID (empty for skin membrane) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ParentMembraneID;

    /** Child membrane IDs */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> ChildMembraneIDs;

    /** Objects contained within this membrane */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FMembraneObject> ContainedObjects;

    /** Communication channels on this membrane's boundary */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FString> BoundaryChannelIDs;

    /** Membrane energy level (for active transport) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnergyLevel = 100.0f;

    /** Maximum energy capacity */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxEnergy = 100.0f;

    /** Energy regeneration rate per second */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnergyRegenRate = 1.0f;

    /** Is membrane active */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsActive = true;

    /** Membrane nesting depth (0 = skin, 1 = first level, etc.) */
    UPROPERTY(BlueprintReadOnly)
    int32 NestingDepth = 0;
};

/**
 * Channel Manager Configuration
 */
USTRUCT(BlueprintType)
struct FChannelManagerConfig
{
    GENERATED_BODY()

    /** Maximum number of channels allowed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxChannels = 100;

    /** Maximum number of membranes allowed */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxMembranes = 50;

    /** Default transport rate limit */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DefaultTransportRate = 10.0f;

    /** Enable stochastic rule application */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableStochasticRules = true;

    /** Enable energy-based transport constraints */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableEnergyConstraints = true;

    /** Global energy regeneration multiplier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float EnergyRegenMultiplier = 1.0f;

    /** Enable transport event logging */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bEnableEventLogging = true;

    /** Maximum transport events to retain */
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxEventLogSize = 1000;
};

/**
 * Membrane Communication Channels Component
 * Manages symport/antiport communication channels between adjacent membranes
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UMembraneCommChannels : public UActorComponent
{
    GENERATED_BODY()

public:
    UMembraneCommChannels();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ========================================
    // CONFIGURATION
    // ========================================

    /** Channel manager configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MembraneChannels|Config")
    FChannelManagerConfig Config;

    /** Enable membrane channel processing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MembraneChannels|Config")
    bool bEnableChannelProcessing = true;

    // ========================================
    // STATE
    // ========================================

    /** All registered membranes */
    UPROPERTY(BlueprintReadOnly, Category = "MembraneChannels|State")
    TMap<FString, FMembraneState> Membranes;

    /** All communication channels */
    UPROPERTY(BlueprintReadOnly, Category = "MembraneChannels|State")
    TMap<FString, FCommunicationChannel> Channels;

    /** Recent transport events */
    UPROPERTY(BlueprintReadOnly, Category = "MembraneChannels|State")
    TArray<FTransportEvent> TransportEventLog;

    /** Current simulation time */
    UPROPERTY(BlueprintReadOnly, Category = "MembraneChannels|State")
    float CurrentTime = 0.0f;

    // ========================================
    // DELEGATES
    // ========================================

    /** Called when transport occurs */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTransportEvent, const FTransportEvent&, Event);
    UPROPERTY(BlueprintAssignable, Category = "MembraneChannels|Events")
    FOnTransportEvent OnTransportOccurred;

    /** Called when channel state changes */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnChannelStateChanged, const FString&, ChannelID, EChannelState, NewState);
    UPROPERTY(BlueprintAssignable, Category = "MembraneChannels|Events")
    FOnChannelStateChanged OnChannelStateChanged;

    /** Called when membrane energy depleted */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMembraneEnergyDepleted, const FString&, MembraneID);
    UPROPERTY(BlueprintAssignable, Category = "MembraneChannels|Events")
    FOnMembraneEnergyDepleted OnMembraneEnergyDepleted;

    // ========================================
    // PUBLIC API - MEMBRANE MANAGEMENT
    // ========================================

    /** Create a new membrane */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Membranes")
    FString CreateMembrane(const FString& ParentMembraneID = TEXT(""));

    /** Remove a membrane and redistribute its contents */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Membranes")
    bool RemoveMembrane(const FString& MembraneID);

    /** Get membrane state by ID */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Membranes")
    bool GetMembraneState(const FString& MembraneID, FMembraneState& OutState) const;

    /** Add object to membrane */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Membranes")
    bool AddObjectToMembrane(const FString& MembraneID, const FMembraneObject& Object);

    /** Remove object from membrane */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Membranes")
    bool RemoveObjectFromMembrane(const FString& MembraneID, const FString& ObjectID);

    /** Get objects in membrane by symbol */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Membranes")
    TArray<FMembraneObject> GetObjectsBySymbol(const FString& MembraneID, const FString& Symbol) const;

    /** Count objects of a given symbol in membrane */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Membranes")
    int32 CountObjectsInMembrane(const FString& MembraneID, const FString& Symbol) const;

    // ========================================
    // PUBLIC API - CHANNEL MANAGEMENT
    // ========================================

    /** Create a symport channel between two adjacent membranes */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Channels")
    FString CreateSymportChannel(const FString& MembraneA_ID, const FString& MembraneB_ID,
                                 const TArray<FString>& SelectiveFor,
                                 EChannelDirection Direction = EChannelDirection::Outward);

    /** Create an antiport channel between two adjacent membranes */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Channels")
    FString CreateAntiportChannel(const FString& MembraneA_ID, const FString& MembraneB_ID,
                                  const TArray<FString>& SelectiveFor,
                                  EChannelDirection Direction = EChannelDirection::Bidirectional);

    /** Create a uniport channel (simple diffusion) */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Channels")
    FString CreateUniportChannel(const FString& MembraneA_ID, const FString& MembraneB_ID,
                                 const TArray<FString>& SelectiveFor);

    /** Remove a channel */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Channels")
    bool RemoveChannel(const FString& ChannelID);

    /** Open a channel */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Channels")
    bool OpenChannel(const FString& ChannelID);

    /** Close a channel */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Channels")
    bool CloseChannel(const FString& ChannelID);

    /** Get channel state */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Channels")
    bool GetChannelState(const FString& ChannelID, FCommunicationChannel& OutChannel) const;

    /** Add transport rule to channel */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Channels")
    bool AddTransportRule(const FString& ChannelID, const FTransportRule& Rule);

    /** Remove transport rule from channel */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Channels")
    bool RemoveTransportRule(const FString& ChannelID, const FString& RuleID);

    // ========================================
    // PUBLIC API - TRANSPORT OPERATIONS
    // ========================================

    /** Attempt symport transport through a channel */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Transport")
    bool ExecuteSymport(const FString& ChannelID, const TArray<FString>& ObjectSymbols);

    /** Attempt antiport transport through a channel */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Transport")
    bool ExecuteAntiport(const FString& ChannelID, 
                         const TArray<FString>& SourceObjectSymbols,
                         const TArray<FString>& TargetObjectSymbols);

    /** Queue an object for transport */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Transport")
    bool QueueObjectForTransport(const FString& ChannelID, const FMembraneObject& Object);

    /** Process all pending transports */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Transport")
    int32 ProcessPendingTransports();

    /** Check if transport is possible given current state */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Transport")
    bool CanTransport(const FString& ChannelID, const TArray<FString>& ObjectSymbols) const;

    // ========================================
    // PUBLIC API - ANALYSIS
    // ========================================

    /** Get transport statistics for a channel */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Analysis")
    float GetChannelTransportRate(const FString& ChannelID) const;

    /** Get total objects in system */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Analysis")
    int32 GetTotalObjectCount() const;

    /** Get membrane hierarchy depth */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Analysis")
    int32 GetMaxNestingDepth() const;

    /** Get channels connecting to a membrane */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Analysis")
    TArray<FString> GetChannelsForMembrane(const FString& MembraneID) const;

    /** Get transport events for a channel */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels|Analysis")
    TArray<FTransportEvent> GetRecentTransportEvents(const FString& ChannelID, int32 MaxEvents = 10) const;

    /** Reset channel manager */
    UFUNCTION(BlueprintCallable, Category = "MembraneChannels")
    void ResetChannelManager();

protected:
    virtual void BeginPlay() override;

private:
    // ========================================
    // INTERNAL STATE
    // ========================================

    /** Counter for generating unique IDs */
    int32 IDCounter = 0;

    /** Generate unique ID with prefix */
    FString GenerateUniqueID(const FString& Prefix);

    // ========================================
    // INTERNAL METHODS - MEMBRANE OPERATIONS
    // ========================================

    /** Validate membrane adjacency for channel creation */
    bool AreMembranesAdjacent(const FString& MembraneA_ID, const FString& MembraneB_ID) const;

    /** Update membrane energy levels */
    void UpdateMembraneEnergy(float DeltaTime);

    /** Consume energy from membrane for transport */
    bool ConsumeMembraneEnergy(const FString& MembraneID, float Amount);

    // ========================================
    // INTERNAL METHODS - CHANNEL OPERATIONS
    // ========================================

    /** Update channel states (refractory timers, etc.) */
    void UpdateChannelStates(float DeltaTime);

    /** Set channel state and broadcast event */
    void SetChannelState(const FString& ChannelID, EChannelState NewState);

    /** Find applicable transport rule */
    const FTransportRule* FindApplicableRule(const FCommunicationChannel& Channel,
                                              const TArray<FString>& SourceObjects,
                                              const TArray<FString>& TargetObjects) const;

    // ========================================
    // INTERNAL METHODS - TRANSPORT OPERATIONS
    // ========================================

    /** Execute transport based on rule */
    FTransportEvent ExecuteTransportRule(const FString& ChannelID, 
                                          const FTransportRule& Rule,
                                          const FString& SourceMembraneID,
                                          const FString& TargetMembraneID);

    /** Move objects between membranes */
    bool MoveObjectsBetweenMembranes(const FString& SourceID, 
                                      const FString& TargetID,
                                      const TArray<FString>& Symbols,
                                      TArray<FMembraneObject>& OutMovedObjects);

    /** Check if objects are available in membrane */
    bool AreObjectsAvailable(const FString& MembraneID, const TArray<FString>& Symbols) const;

    /** Check if catalysts are present */
    bool AreCatalystsPresent(const FString& MembraneID, const TArray<FString>& CatalystSymbols) const;

    /** Log transport event */
    void LogTransportEvent(const FTransportEvent& Event);

    // ========================================
    // INTERNAL METHODS - RULE APPLICATION
    // ========================================

    /** Apply stochastic probability to rule */
    bool ShouldApplyRule(const FTransportRule& Rule) const;

    /** Process queued transports for a channel */
    int32 ProcessChannelQueue(const FString& ChannelID);
};

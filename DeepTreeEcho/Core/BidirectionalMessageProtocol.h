// BidirectionalMessageProtocol.h
// Bidirectional Message Protocol for Deep Tree Echo
// Feature F1.1.3: Zero-copy shared memory with lock-free queues
// Copyright (c) 2025 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Messages/LockFreeMessageQueue.h"
#include "BidirectionalMessageProtocol.generated.h"

// Forward declarations
class UNeuroSymbolicBridge;

// ============================================================================
// MESSAGE TYPES (Matching FlatBuffers schema)
// ============================================================================

/**
 * Message type enum
 */
UENUM(BlueprintType)
enum class EMessageType : uint8
{
    Command     UMETA(DisplayName = "Command"),
    Query       UMETA(DisplayName = "Query"),
    Event       UMETA(DisplayName = "Event"),
    StateUpdate UMETA(DisplayName = "State Update"),
    Response    UMETA(DisplayName = "Response")
};

/**
 * Message priority levels
 */
UENUM(BlueprintType)
enum class EMessagePriority : uint8
{
    Critical UMETA(DisplayName = "Critical"),
    High     UMETA(DisplayName = "High"),
    Normal   UMETA(DisplayName = "Normal"),
    Low      UMETA(DisplayName = "Low")
};

/**
 * Command types
 */
UENUM(BlueprintType)
enum class ECommandType : uint8
{
    StartProcessing      UMETA(DisplayName = "Start Processing"),
    StopProcessing       UMETA(DisplayName = "Stop Processing"),
    ResetState           UMETA(DisplayName = "Reset State"),
    UpdateConfiguration  UMETA(DisplayName = "Update Configuration"),
    FlushBuffer          UMETA(DisplayName = "Flush Buffer")
};

/**
 * Query types
 */
UENUM(BlueprintType)
enum class EQueryType : uint8
{
    GetState         UMETA(DisplayName = "Get State"),
    GetMetrics       UMETA(DisplayName = "Get Metrics"),
    GetConfiguration UMETA(DisplayName = "Get Configuration"),
    GetHistory       UMETA(DisplayName = "Get History")
};

/**
 * Event types
 */
UENUM(BlueprintType)
enum class EEventType : uint8
{
    StateChanged         UMETA(DisplayName = "State Changed"),
    ThresholdExceeded    UMETA(DisplayName = "Threshold Exceeded"),
    ErrorOccurred        UMETA(DisplayName = "Error Occurred"),
    ProcessingComplete   UMETA(DisplayName = "Processing Complete"),
    ConfigurationUpdated UMETA(DisplayName = "Configuration Updated")
};

// ============================================================================
// MESSAGE STRUCTURES
// ============================================================================

/**
 * Lightweight message structure for lock-free queue
 * Contains only essential data to minimize copying
 */
USTRUCT(BlueprintType)
struct FMessage
{
    GENERATED_BODY()

    /** Unique message identifier */
    UPROPERTY(BlueprintReadWrite, Category = "Message")
    FString MessageID;

    /** Message type */
    UPROPERTY(BlueprintReadWrite, Category = "Message")
    EMessageType MessageType = EMessageType::Event;

    /** Message priority */
    UPROPERTY(BlueprintReadWrite, Category = "Message")
    EMessagePriority Priority = EMessagePriority::Normal;

    /** Timestamp (microseconds) */
    UPROPERTY(BlueprintReadWrite, Category = "Message")
    int64 Timestamp = 0;

    /** Sender component identifier */
    UPROPERTY(BlueprintReadWrite, Category = "Message")
    FString SenderID;

    /** Recipient component identifier */
    UPROPERTY(BlueprintReadWrite, Category = "Message")
    FString RecipientID;

    /** Routing topic */
    UPROPERTY(BlueprintReadWrite, Category = "Message")
    FString Topic;

    /** Correlation ID for request-response pairing */
    UPROPERTY(BlueprintReadWrite, Category = "Message")
    FString CorrelationID;

    /** Serialized payload data (FlatBuffers buffer) */
    UPROPERTY(BlueprintReadWrite, Category = "Message")
    TArray<uint8> PayloadData;

    /** Enqueue time for latency tracking */
    UPROPERTY(BlueprintReadWrite, Category = "Message")
    int64 EnqueueTime = 0;

    /** Optional deadline for time-critical messages */
    UPROPERTY(BlueprintReadWrite, Category = "Message")
    int64 Deadline = 0;

    /** Whether acknowledgment is required */
    UPROPERTY(BlueprintReadWrite, Category = "Message")
    bool bRequiresAck = false;

    /** Whether this is part of a batch */
    UPROPERTY(BlueprintReadWrite, Category = "Message")
    bool bIsBatched = false;

    /** Batch index (if batched) */
    UPROPERTY(BlueprintReadWrite, Category = "Message")
    int32 BatchIndex = 0;

    /** Total batch size (if batched) */
    UPROPERTY(BlueprintReadWrite, Category = "Message")
    int32 BatchSize = 1;

    FMessage()
        : MessageType(EMessageType::Event)
        , Priority(EMessagePriority::Normal)
        , Timestamp(0)
        , EnqueueTime(0)
        , Deadline(0)
        , bRequiresAck(false)
        , bIsBatched(false)
        , BatchIndex(0)
        , BatchSize(1)
    {}
};

/**
 * Message batch for efficient processing
 */
USTRUCT(BlueprintType)
struct FMessageBatch
{
    GENERATED_BODY()

    /** Batch identifier */
    UPROPERTY(BlueprintReadWrite, Category = "Batch")
    FString BatchID;

    /** Messages in this batch */
    UPROPERTY(BlueprintReadWrite, Category = "Batch")
    TArray<FMessage> Messages;

    /** Creation time */
    UPROPERTY(BlueprintReadWrite, Category = "Batch")
    int64 CreationTime = 0;

    FMessageBatch()
        : CreationTime(0)
    {}
};

/**
 * Message queue metrics
 */
USTRUCT(BlueprintType)
struct FMessageQueueMetrics
{
    GENERATED_BODY()

    /** Current queue depth */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    int32 QueueDepth = 0;

    /** Total messages enqueued */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    int64 TotalEnqueued = 0;

    /** Total messages dequeued */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    int64 TotalDequeued = 0;

    /** Total messages dropped (queue full) */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    int64 TotalDropped = 0;

    /** Average latency (microseconds) */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    float AverageLatencyUs = 0.0f;

    /** Peak latency (microseconds) */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    float PeakLatencyUs = 0.0f;

    /** Throughput (messages per second) */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    float ThroughputMPS = 0.0f;

    /** Timestamp of last update */
    UPROPERTY(BlueprintReadWrite, Category = "Metrics")
    int64 LastUpdateTime = 0;

    FMessageQueueMetrics()
        : QueueDepth(0)
        , TotalEnqueued(0)
        , TotalDequeued(0)
        , TotalDropped(0)
        , AverageLatencyUs(0.0f)
        , PeakLatencyUs(0.0f)
        , ThroughputMPS(0.0f)
        , LastUpdateTime(0)
    {}
};

// ============================================================================
// ROUTING CONFIGURATION
// ============================================================================

/**
 * Message routing configuration
 */
USTRUCT(BlueprintType)
struct FMessageRoutingConfig
{
    GENERATED_BODY()

    /** Enable topic-based routing */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Routing")
    bool bEnableTopicRouting = true;

    /** Enable priority-based scheduling */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Routing")
    bool bEnablePriorityScheduling = true;

    /** Enable message batching */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Routing")
    bool bEnableBatching = true;

    /** Batch size threshold */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Routing")
    int32 BatchSizeThreshold = 16;

    /** Batch timeout (microseconds) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Routing")
    int64 BatchTimeoutUs = 1000; // 1ms default

    /** Maximum queue size per priority level */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Routing")
    int32 MaxQueueSizePerPriority = 1024;

    FMessageRoutingConfig()
        : bEnableTopicRouting(true)
        , bEnablePriorityScheduling(true)
        , bEnableBatching(true)
        , BatchSizeThreshold(16)
        , BatchTimeoutUs(1000)
        , MaxQueueSizePerPriority(1024)
    {}
};

// ============================================================================
// MESSAGE SUBSCRIPTION
// ============================================================================

/**
 * Message subscription callback signature
 */
DECLARE_DYNAMIC_DELEGATE_OneParam(FMessageSubscriptionCallback, const FMessage&, Message);

/**
 * Topic subscription for message routing
 */
USTRUCT(BlueprintType)
struct FTopicSubscription
{
    GENERATED_BODY()

    /** Topic pattern (supports wildcards) */
    UPROPERTY(BlueprintReadWrite, Category = "Subscription")
    FString TopicPattern;

    /** Subscriber component ID */
    UPROPERTY(BlueprintReadWrite, Category = "Subscription")
    FString SubscriberID;

    /** Minimum priority level to receive */
    UPROPERTY(BlueprintReadWrite, Category = "Subscription")
    EMessagePriority MinPriority = EMessagePriority::Normal;

    /** Callback function (native only) */
    FMessageSubscriptionCallback Callback;

    FTopicSubscription()
        : MinPriority(EMessagePriority::Normal)
    {}
};

// ============================================================================
// BIDIRECTIONAL MESSAGE PROTOCOL COMPONENT
// ============================================================================

/**
 * Bidirectional Message Protocol Component
 * Manages zero-copy message passing between neural and symbolic subsystems
 */
UCLASS(ClassGroup=(DeepTreeEcho), meta=(BlueprintSpawnableComponent))
class DEEPTREEECHO_API UBidirectionalMessageProtocol : public UActorComponent
{
    GENERATED_BODY()

public:
    UBidirectionalMessageProtocol();

    //~ Begin UActorComponent Interface
    virtual void InitializeComponent() override;
    virtual void UninitializeComponent() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
    //~ End UActorComponent Interface

    // ========================================================================
    // MESSAGE SENDING API
    // ========================================================================

    /**
     * Send a message through the protocol
     * @param Message The message to send
     * @return true if message was enqueued successfully
     */
    UFUNCTION(BlueprintCallable, Category = "Message Protocol")
    bool SendMessage(const FMessage& Message);

    /**
     * Send a batch of messages
     * @param Batch The message batch to send
     * @return Number of messages successfully enqueued
     */
    UFUNCTION(BlueprintCallable, Category = "Message Protocol")
    int32 SendBatch(const FMessageBatch& Batch);

    /**
     * Send a command message
     * @param RecipientID Target component
     * @param CommandType Command type
     * @param Priority Message priority
     * @return Message ID if successful, empty string otherwise
     */
    UFUNCTION(BlueprintCallable, Category = "Message Protocol")
    FString SendCommand(const FString& RecipientID, ECommandType CommandType, EMessagePriority Priority = EMessagePriority::Normal);

    /**
     * Send a query message
     * @param RecipientID Target component
     * @param QueryType Query type
     * @param CorrelationID Correlation ID for response tracking
     * @return Message ID if successful, empty string otherwise
     */
    UFUNCTION(BlueprintCallable, Category = "Message Protocol")
    FString SendQuery(const FString& RecipientID, EQueryType QueryType, const FString& CorrelationID);

    /**
     * Send an event message
     * @param Topic Event topic
     * @param EventType Event type
     * @param Priority Message priority
     * @return Message ID if successful, empty string otherwise
     */
    UFUNCTION(BlueprintCallable, Category = "Message Protocol")
    FString SendEvent(const FString& Topic, EEventType EventType, EMessagePriority Priority = EMessagePriority::Normal);

    // ========================================================================
    // MESSAGE RECEIVING API
    // ========================================================================

    /**
     * Receive the next available message
     * @param OutMessage Output message
     * @return true if a message was received
     */
    UFUNCTION(BlueprintCallable, Category = "Message Protocol")
    bool ReceiveMessage(FMessage& OutMessage);

    /**
     * Receive multiple messages (batch receive)
     * @param MaxMessages Maximum number of messages to receive
     * @param OutMessages Output array of messages
     * @return Number of messages received
     */
    UFUNCTION(BlueprintCallable, Category = "Message Protocol")
    int32 ReceiveMessages(int32 MaxMessages, TArray<FMessage>& OutMessages);

    /**
     * Peek at the next message without removing it
     * @param OutMessage Output message
     * @return true if a message is available
     */
    UFUNCTION(BlueprintCallable, Category = "Message Protocol")
    bool PeekMessage(FMessage& OutMessage) const;

    // ========================================================================
    // SUBSCRIPTION API
    // ========================================================================

    /**
     * Subscribe to a topic
     * @param TopicPattern Topic pattern (supports wildcards like "neural.*")
     * @param SubscriberID Subscriber component ID
     * @param MinPriority Minimum priority level to receive
     * @return Subscription ID
     */
    UFUNCTION(BlueprintCallable, Category = "Message Protocol")
    FString Subscribe(const FString& TopicPattern, const FString& SubscriberID, EMessagePriority MinPriority = EMessagePriority::Normal);

    /**
     * Unsubscribe from a topic
     * @param SubscriptionID Subscription ID from Subscribe()
     */
    UFUNCTION(BlueprintCallable, Category = "Message Protocol")
    void Unsubscribe(const FString& SubscriptionID);

    /**
     * Unsubscribe all subscriptions for a subscriber
     * @param SubscriberID Subscriber component ID
     */
    UFUNCTION(BlueprintCallable, Category = "Message Protocol")
    void UnsubscribeAll(const FString& SubscriberID);

    // ========================================================================
    // METRICS AND DIAGNOSTICS API
    // ========================================================================

    /**
     * Get current message queue metrics
     * @return Queue metrics
     */
    UFUNCTION(BlueprintCallable, Category = "Message Protocol")
    FMessageQueueMetrics GetMetrics() const;

    /**
     * Get metrics for a specific priority level
     * @param Priority Priority level
     * @return Queue metrics for that priority
     */
    UFUNCTION(BlueprintCallable, Category = "Message Protocol")
    FMessageQueueMetrics GetMetricsForPriority(EMessagePriority Priority) const;

    /**
     * Reset all metrics counters
     */
    UFUNCTION(BlueprintCallable, Category = "Message Protocol")
    void ResetMetrics();

    /**
     * Get number of active subscriptions
     * @return Number of active subscriptions
     */
    UFUNCTION(BlueprintCallable, Category = "Message Protocol")
    int32 GetSubscriptionCount() const;

    // ========================================================================
    // CONFIGURATION API
    // ========================================================================

    /**
     * Get current routing configuration
     * @return Routing configuration
     */
    UFUNCTION(BlueprintCallable, Category = "Message Protocol")
    FMessageRoutingConfig GetRoutingConfig() const { return RoutingConfig; }

    /**
     * Update routing configuration
     * @param NewConfig New routing configuration
     */
    UFUNCTION(BlueprintCallable, Category = "Message Protocol")
    void SetRoutingConfig(const FMessageRoutingConfig& NewConfig);

protected:
    /** Routing configuration */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    FMessageRoutingConfig RoutingConfig;

    /** Component identifier */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration")
    FString ComponentID;

private:
    // Lock-free message queues per priority level
    TLockFreeMPSCQueue<FMessage, 1024>* PriorityQueues[4]; // One per priority level

    // Topic subscriptions
    TArray<FTopicSubscription> Subscriptions;
    mutable FCriticalSection SubscriptionLock;

    // Batching state
    TMap<FString, FMessageBatch> PendingBatches;
    TMap<FString, int64> BatchStartTimes;

    // Metrics per priority level
    FMessageQueueMetrics Metrics[4];

    // Helper functions
    void ProcessIncomingMessages(float DeltaTime);
    void ProcessBatching(float DeltaTime);
    void RouteMessage(const FMessage& Message);
    bool MatchesTopicPattern(const FString& Topic, const FString& Pattern) const;
    void UpdateMetrics(EMessagePriority Priority, const FMessage& Message, bool bEnqueued);
    FString GenerateMessageID() const;
    int64 GetCurrentTimeMicroseconds() const;
};

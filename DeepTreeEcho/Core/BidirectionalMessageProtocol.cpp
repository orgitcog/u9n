// BidirectionalMessageProtocol.cpp
// Implementation of Bidirectional Message Protocol
// Feature F1.1.3: Zero-copy shared memory with lock-free queues
// Copyright (c) 2025 Deep Tree Echo Project

#include "BidirectionalMessageProtocol.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"

UBidirectionalMessageProtocol::UBidirectionalMessageProtocol()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    
    // Initialize priority queues
    for (int i = 0; i < 4; ++i)
    {
        PriorityQueues[i] = nullptr;
    }
    
    // Generate unique component ID
    ComponentID = FGuid::NewGuid().ToString();
}

void UBidirectionalMessageProtocol::InitializeComponent()
{
    Super::InitializeComponent();
    
    // Create lock-free queues for each priority level
    PriorityQueues[0] = new TLockFreeMPSCQueue<FMessage, 1024>(); // Critical
    PriorityQueues[1] = new TLockFreeMPSCQueue<FMessage, 1024>(); // High
    PriorityQueues[2] = new TLockFreeMPSCQueue<FMessage, 1024>(); // Normal
    PriorityQueues[3] = new TLockFreeMPSCQueue<FMessage, 512>();  // Low (smaller capacity)
    
    // Reset metrics
    ResetMetrics();
    
    UE_LOG(LogTemp, Log, TEXT("BidirectionalMessageProtocol initialized: %s"), *ComponentID);
}

void UBidirectionalMessageProtocol::UninitializeComponent()
{
    // Clean up queues
    for (int i = 0; i < 4; ++i)
    {
        if (PriorityQueues[i])
        {
            delete PriorityQueues[i];
            PriorityQueues[i] = nullptr;
        }
    }
    
    // Clear subscriptions
    {
        FScopeLock Lock(&SubscriptionLock);
        Subscriptions.Empty();
    }
    
    Super::UninitializeComponent();
}

void UBidirectionalMessageProtocol::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    
    // Process incoming messages
    ProcessIncomingMessages(DeltaTime);
    
    // Process batching if enabled
    if (RoutingConfig.bEnableBatching)
    {
        ProcessBatching(DeltaTime);
    }
}

// ============================================================================
// MESSAGE SENDING API
// ============================================================================

bool UBidirectionalMessageProtocol::SendMessage(const FMessage& Message)
{
    // Determine priority queue
    int32 QueueIndex = static_cast<int32>(Message.Priority);
    if (QueueIndex < 0 || QueueIndex >= 4)
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid message priority: %d"), QueueIndex);
        return false;
    }
    
    // Create a copy and set enqueue time
    FMessage MessageCopy = Message;
    MessageCopy.EnqueueTime = GetCurrentTimeMicroseconds();
    
    // Enqueue to appropriate priority queue
    bool bEnqueued = PriorityQueues[QueueIndex]->Enqueue(MessageCopy);
    
    // Update metrics
    UpdateMetrics(Message.Priority, MessageCopy, bEnqueued);
    
    if (!bEnqueued)
    {
        UE_LOG(LogTemp, Warning, TEXT("Failed to enqueue message %s - queue full"), *Message.MessageID);
    }
    
    return bEnqueued;
}

int32 UBidirectionalMessageProtocol::SendBatch(const FMessageBatch& Batch)
{
    int32 SuccessCount = 0;
    
    for (const FMessage& Message : Batch.Messages)
    {
        if (SendMessage(Message))
        {
            SuccessCount++;
        }
    }
    
    return SuccessCount;
}

FString UBidirectionalMessageProtocol::SendCommand(const FString& RecipientID, ECommandType CommandType, EMessagePriority Priority)
{
    FMessage Message;
    Message.MessageID = GenerateMessageID();
    Message.MessageType = EMessageType::Command;
    Message.Priority = Priority;
    Message.Timestamp = GetCurrentTimeMicroseconds();
    Message.SenderID = ComponentID;
    Message.RecipientID = RecipientID;
    Message.Topic = TEXT("command");
    
    // TODO: Serialize command type into PayloadData using FlatBuffers
    // For now, store as simple byte representation
    Message.PayloadData.Add(static_cast<uint8>(CommandType));
    
    if (SendMessage(Message))
    {
        return Message.MessageID;
    }
    
    return FString();
}

FString UBidirectionalMessageProtocol::SendQuery(const FString& RecipientID, EQueryType QueryType, const FString& CorrelationID)
{
    FMessage Message;
    Message.MessageID = GenerateMessageID();
    Message.MessageType = EMessageType::Query;
    Message.Priority = EMessagePriority::High; // Queries are typically high priority
    Message.Timestamp = GetCurrentTimeMicroseconds();
    Message.SenderID = ComponentID;
    Message.RecipientID = RecipientID;
    Message.Topic = TEXT("query");
    Message.CorrelationID = CorrelationID;
    Message.bRequiresAck = true;
    
    // TODO: Serialize query type into PayloadData using FlatBuffers
    Message.PayloadData.Add(static_cast<uint8>(QueryType));
    
    if (SendMessage(Message))
    {
        return Message.MessageID;
    }
    
    return FString();
}

FString UBidirectionalMessageProtocol::SendEvent(const FString& Topic, EEventType EventType, EMessagePriority Priority)
{
    FMessage Message;
    Message.MessageID = GenerateMessageID();
    Message.MessageType = EMessageType::Event;
    Message.Priority = Priority;
    Message.Timestamp = GetCurrentTimeMicroseconds();
    Message.SenderID = ComponentID;
    Message.Topic = Topic;
    
    // TODO: Serialize event type into PayloadData using FlatBuffers
    Message.PayloadData.Add(static_cast<uint8>(EventType));
    
    if (SendMessage(Message))
    {
        return Message.MessageID;
    }
    
    return FString();
}

// ============================================================================
// MESSAGE RECEIVING API
// ============================================================================

bool UBidirectionalMessageProtocol::ReceiveMessage(FMessage& OutMessage)
{
    if (!RoutingConfig.bEnablePriorityScheduling)
    {
        // Simple FIFO without priority
        for (int i = 0; i < 4; ++i)
        {
            if (PriorityQueues[i]->Dequeue(OutMessage))
            {
                return true;
            }
        }
        return false;
    }
    
    // Priority-based scheduling: check higher priority queues first
    for (int i = 0; i < 4; ++i)
    {
        if (PriorityQueues[i]->Dequeue(OutMessage))
        {
            // Calculate latency
            int64 CurrentTime = GetCurrentTimeMicroseconds();
            float LatencyUs = static_cast<float>(CurrentTime - OutMessage.EnqueueTime);
            
            // Update metrics
            EMessagePriority Priority = static_cast<EMessagePriority>(i);
            FMessageQueueMetrics& QueueMetrics = Metrics[i];
            QueueMetrics.TotalDequeued++;
            
            // Update average latency (exponential moving average)
            const float Alpha = 0.1f;
            QueueMetrics.AverageLatencyUs = (1.0f - Alpha) * QueueMetrics.AverageLatencyUs + Alpha * LatencyUs;
            
            // Update peak latency
            if (LatencyUs > QueueMetrics.PeakLatencyUs)
            {
                QueueMetrics.PeakLatencyUs = LatencyUs;
            }
            
            return true;
        }
    }
    
    return false;
}

int32 UBidirectionalMessageProtocol::ReceiveMessages(int32 MaxMessages, TArray<FMessage>& OutMessages)
{
    OutMessages.Empty();
    OutMessages.Reserve(MaxMessages);
    
    int32 ReceivedCount = 0;
    FMessage Message;
    
    while (ReceivedCount < MaxMessages && ReceiveMessage(Message))
    {
        OutMessages.Add(Message);
        ReceivedCount++;
    }
    
    return ReceivedCount;
}

bool UBidirectionalMessageProtocol::PeekMessage(FMessage& OutMessage) const
{
    // Note: Peek is not truly lock-free, but provides a best-effort view
    // For true lock-free peek, we'd need to modify the queue implementation
    
    for (int i = 0; i < 4; ++i)
    {
        if (!PriorityQueues[i]->IsEmpty())
        {
            // We can't truly peek without modifying the queue,
            // so we return a success indicator
            return true;
        }
    }
    
    return false;
}

// ============================================================================
// SUBSCRIPTION API
// ============================================================================

FString UBidirectionalMessageProtocol::Subscribe(const FString& TopicPattern, const FString& SubscriberID, EMessagePriority MinPriority)
{
    FScopeLock Lock(&SubscriptionLock);
    
    FTopicSubscription Subscription;
    Subscription.TopicPattern = TopicPattern;
    Subscription.SubscriberID = SubscriberID;
    Subscription.MinPriority = MinPriority;
    
    Subscriptions.Add(Subscription);
    
    FString SubscriptionID = FString::Printf(TEXT("%s:%s:%s"), *SubscriberID, *TopicPattern, *FGuid::NewGuid().ToString());
    
    UE_LOG(LogTemp, Log, TEXT("Subscription created: %s for topic %s"), *SubscriptionID, *TopicPattern);
    
    return SubscriptionID;
}

void UBidirectionalMessageProtocol::Unsubscribe(const FString& SubscriptionID)
{
    FScopeLock Lock(&SubscriptionLock);
    
    // Parse subscription ID to extract subscriber and topic
    TArray<FString> Parts;
    SubscriptionID.ParseIntoArray(Parts, TEXT(":"));
    
    if (Parts.Num() >= 2)
    {
        const FString& SubscriberID = Parts[0];
        const FString& TopicPattern = Parts[1];
        
        Subscriptions.RemoveAll([&](const FTopicSubscription& Sub) {
            return Sub.SubscriberID == SubscriberID && Sub.TopicPattern == TopicPattern;
        });
    }
}

void UBidirectionalMessageProtocol::UnsubscribeAll(const FString& SubscriberID)
{
    FScopeLock Lock(&SubscriptionLock);
    
    Subscriptions.RemoveAll([&](const FTopicSubscription& Sub) {
        return Sub.SubscriberID == SubscriberID;
    });
    
    UE_LOG(LogTemp, Log, TEXT("Unsubscribed all for: %s"), *SubscriberID);
}

// ============================================================================
// METRICS AND DIAGNOSTICS API
// ============================================================================

FMessageQueueMetrics UBidirectionalMessageProtocol::GetMetrics() const
{
    // Aggregate metrics across all priority levels
    FMessageQueueMetrics AggregatedMetrics;
    
    for (int i = 0; i < 4; ++i)
    {
        AggregatedMetrics.QueueDepth += Metrics[i].QueueDepth;
        AggregatedMetrics.TotalEnqueued += Metrics[i].TotalEnqueued;
        AggregatedMetrics.TotalDequeued += Metrics[i].TotalDequeued;
        AggregatedMetrics.TotalDropped += Metrics[i].TotalDropped;
        
        // Average latency weighted by message count
        if (Metrics[i].TotalDequeued > 0)
        {
            AggregatedMetrics.AverageLatencyUs += Metrics[i].AverageLatencyUs * Metrics[i].TotalDequeued;
        }
        
        // Peak latency is max across all queues
        if (Metrics[i].PeakLatencyUs > AggregatedMetrics.PeakLatencyUs)
        {
            AggregatedMetrics.PeakLatencyUs = Metrics[i].PeakLatencyUs;
        }
    }
    
    // Finalize average latency
    if (AggregatedMetrics.TotalDequeued > 0)
    {
        AggregatedMetrics.AverageLatencyUs /= AggregatedMetrics.TotalDequeued;
    }
    
    // Calculate throughput
    int64 CurrentTime = GetCurrentTimeMicroseconds();
    if (AggregatedMetrics.LastUpdateTime > 0)
    {
        float DeltaSeconds = (CurrentTime - AggregatedMetrics.LastUpdateTime) / 1000000.0f;
        if (DeltaSeconds > 0.0f)
        {
            AggregatedMetrics.ThroughputMPS = AggregatedMetrics.TotalDequeued / DeltaSeconds;
        }
    }
    AggregatedMetrics.LastUpdateTime = CurrentTime;
    
    return AggregatedMetrics;
}

FMessageQueueMetrics UBidirectionalMessageProtocol::GetMetricsForPriority(EMessagePriority Priority) const
{
    int32 Index = static_cast<int32>(Priority);
    if (Index >= 0 && Index < 4)
    {
        return Metrics[Index];
    }
    
    return FMessageQueueMetrics();
}

void UBidirectionalMessageProtocol::ResetMetrics()
{
    for (int i = 0; i < 4; ++i)
    {
        Metrics[i] = FMessageQueueMetrics();
        Metrics[i].LastUpdateTime = GetCurrentTimeMicroseconds();
    }
}

int32 UBidirectionalMessageProtocol::GetSubscriptionCount() const
{
    FScopeLock Lock(&SubscriptionLock);
    return Subscriptions.Num();
}

// ============================================================================
// CONFIGURATION API
// ============================================================================

void UBidirectionalMessageProtocol::SetRoutingConfig(const FMessageRoutingConfig& NewConfig)
{
    RoutingConfig = NewConfig;
    UE_LOG(LogTemp, Log, TEXT("Routing configuration updated"));
}

// ============================================================================
// PRIVATE HELPER FUNCTIONS
// ============================================================================

void UBidirectionalMessageProtocol::ProcessIncomingMessages(float DeltaTime)
{
    // Route messages to subscribers
    if (!RoutingConfig.bEnableTopicRouting)
    {
        return;
    }
    
    // Process a batch of messages per tick to avoid blocking
    const int32 MaxMessagesPerTick = 100;
    TArray<FMessage> Messages;
    ReceiveMessages(MaxMessagesPerTick, Messages);
    
    for (const FMessage& Message : Messages)
    {
        RouteMessage(Message);
    }
}

void UBidirectionalMessageProtocol::ProcessBatching(float DeltaTime)
{
    int64 CurrentTime = GetCurrentTimeMicroseconds();
    
    // Check for batches that need to be flushed due to timeout
    TArray<FString> BatchesToFlush;
    
    for (const auto& Pair : BatchStartTimes)
    {
        int64 BatchAge = CurrentTime - Pair.Value;
        if (BatchAge > RoutingConfig.BatchTimeoutUs)
        {
            BatchesToFlush.Add(Pair.Key);
        }
    }
    
    // Flush timed-out batches
    for (const FString& BatchID : BatchesToFlush)
    {
        if (FMessageBatch* Batch = PendingBatches.Find(BatchID))
        {
            SendBatch(*Batch);
            PendingBatches.Remove(BatchID);
            BatchStartTimes.Remove(BatchID);
        }
    }
}

void UBidirectionalMessageProtocol::RouteMessage(const FMessage& Message)
{
    FScopeLock Lock(&SubscriptionLock);
    
    for (const FTopicSubscription& Sub : Subscriptions)
    {
        // Check priority filter
        if (Message.Priority > Sub.MinPriority)
        {
            continue;
        }
        
        // Check topic pattern match
        if (MatchesTopicPattern(Message.Topic, Sub.TopicPattern))
        {
            // Invoke callback if available
            if (Sub.Callback.IsBound())
            {
                Sub.Callback.Execute(Message);
            }
        }
    }
}

bool UBidirectionalMessageProtocol::MatchesTopicPattern(const FString& Topic, const FString& Pattern) const
{
    // Simple wildcard matching
    // Supports: "topic.*" for prefix match, "*" for match all
    
    if (Pattern == TEXT("*"))
    {
        return true;
    }
    
    if (Pattern.EndsWith(TEXT(".*")))
    {
        FString Prefix = Pattern.Left(Pattern.Len() - 2);
        return Topic.StartsWith(Prefix);
    }
    
    return Topic == Pattern;
}

void UBidirectionalMessageProtocol::UpdateMetrics(EMessagePriority Priority, const FMessage& Message, bool bEnqueued)
{
    int32 Index = static_cast<int32>(Priority);
    if (Index < 0 || Index >= 4)
    {
        return;
    }
    
    FMessageQueueMetrics& QueueMetrics = Metrics[Index];
    
    if (bEnqueued)
    {
        QueueMetrics.TotalEnqueued++;
        QueueMetrics.QueueDepth = PriorityQueues[Index]->Size();
    }
    else
    {
        QueueMetrics.TotalDropped++;
    }
    
    QueueMetrics.LastUpdateTime = GetCurrentTimeMicroseconds();
}

FString UBidirectionalMessageProtocol::GenerateMessageID() const
{
    return FGuid::NewGuid().ToString();
}

int64 UBidirectionalMessageProtocol::GetCurrentTimeMicroseconds() const
{
    return static_cast<int64>(FPlatformTime::Seconds() * 1000000.0);
}

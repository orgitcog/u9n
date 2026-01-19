// BidirectionalMessageProtocolTests.cpp
// Unit tests for Bidirectional Message Protocol
// Feature F1.1.3: Testing lock-free queues, routing, and batching
// Copyright (c) 2025 Deep Tree Echo Project

#include "Misc/AutomationTest.h"
#include "DeepTreeEcho/Core/BidirectionalMessageProtocol.h"
#include "DeepTreeEcho/Core/Messages/LockFreeMessageQueue.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// LOCK-FREE QUEUE TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLockFreeSPSCQueueBasicTest, 
    "DeepTreeEcho.MessageProtocol.LockFreeQueue.SPSC.Basic", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLockFreeSPSCQueueBasicTest::RunTest(const FString& Parameters)
{
    TLockFreeSPSCQueue<int32, 16> Queue;
    
    // Test initial state
    TestTrue(TEXT("Queue should be empty initially"), Queue.IsEmpty());
    TestFalse(TEXT("Queue should not be full initially"), Queue.IsFull());
    TestEqual(TEXT("Queue size should be 0"), Queue.Size(), (size_t)0);
    
    // Test enqueue
    TestTrue(TEXT("Should enqueue item 1"), Queue.Enqueue(42));
    TestFalse(TEXT("Queue should not be empty after enqueue"), Queue.IsEmpty());
    TestEqual(TEXT("Queue size should be 1"), Queue.Size(), (size_t)1);
    
    // Test dequeue
    int32 Value;
    TestTrue(TEXT("Should dequeue item"), Queue.Dequeue(Value));
    TestEqual(TEXT("Dequeued value should be 42"), Value, 42);
    TestTrue(TEXT("Queue should be empty after dequeue"), Queue.IsEmpty());
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLockFreeSPSCQueueCapacityTest, 
    "DeepTreeEcho.MessageProtocol.LockFreeQueue.SPSC.Capacity", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLockFreeSPSCQueueCapacityTest::RunTest(const FString& Parameters)
{
    TLockFreeSPSCQueue<int32, 8> Queue; // Capacity 8, but one slot reserved
    
    // Fill queue to capacity
    for (int32 i = 0; i < 7; ++i) // 7 items (capacity - 1)
    {
        TestTrue(FString::Printf(TEXT("Should enqueue item %d"), i), Queue.Enqueue(i));
    }
    
    TestTrue(TEXT("Queue should be full"), Queue.IsFull());
    TestFalse(TEXT("Should not enqueue when full"), Queue.Enqueue(999));
    
    // Drain queue
    int32 Value;
    for (int32 i = 0; i < 7; ++i)
    {
        TestTrue(FString::Printf(TEXT("Should dequeue item %d"), i), Queue.Dequeue(Value));
        TestEqual(FString::Printf(TEXT("Value should be %d"), i), Value, i);
    }
    
    TestTrue(TEXT("Queue should be empty after draining"), Queue.IsEmpty());
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLockFreeSPSCQueueMoveTest, 
    "DeepTreeEcho.MessageProtocol.LockFreeQueue.SPSC.MoveSemantics", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLockFreeSPSCQueueMoveTest::RunTest(const FString& Parameters)
{
    TLockFreeSPSCQueue<FString, 16> Queue;
    
    // Test move semantics
    FString TestString = TEXT("TestMessage");
    TestTrue(TEXT("Should enqueue with move"), Queue.Enqueue(MoveTemp(TestString)));
    
    FString OutString;
    TestTrue(TEXT("Should dequeue"), Queue.Dequeue(OutString));
    TestEqual(TEXT("Dequeued string should match"), OutString, TEXT("TestMessage"));
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLockFreeMPSCQueueBasicTest, 
    "DeepTreeEcho.MessageProtocol.LockFreeQueue.MPSC.Basic", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FLockFreeMPSCQueueBasicTest::RunTest(const FString& Parameters)
{
    TLockFreeMPSCQueue<int32, 16> Queue;
    
    // Test basic operations
    TestTrue(TEXT("Queue should be empty initially"), Queue.IsEmpty());
    
    TestTrue(TEXT("Should enqueue item 1"), Queue.Enqueue(10));
    TestTrue(TEXT("Should enqueue item 2"), Queue.Enqueue(20));
    TestTrue(TEXT("Should enqueue item 3"), Queue.Enqueue(30));
    
    TestFalse(TEXT("Queue should not be empty"), Queue.IsEmpty());
    TestEqual(TEXT("Queue size should be 3"), Queue.Size(), (size_t)3);
    
    int32 Value;
    TestTrue(TEXT("Should dequeue first item"), Queue.Dequeue(Value));
    TestEqual(TEXT("First value should be 10"), Value, 10);
    
    TestTrue(TEXT("Should dequeue second item"), Queue.Dequeue(Value));
    TestEqual(TEXT("Second value should be 20"), Value, 20);
    
    TestTrue(TEXT("Should dequeue third item"), Queue.Dequeue(Value));
    TestEqual(TEXT("Third value should be 30"), Value, 30);
    
    TestTrue(TEXT("Queue should be empty after draining"), Queue.IsEmpty());
    
    return true;
}

// ============================================================================
// MESSAGE PROTOCOL TESTS
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMessageProtocolInitTest, 
    "DeepTreeEcho.MessageProtocol.Protocol.Initialization", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMessageProtocolInitTest::RunTest(const FString& Parameters)
{
    // Create a mock actor for the component
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }
    
    AActor* TestActor = World->SpawnActor<AActor>();
    if (!TestActor)
    {
        AddError(TEXT("Failed to spawn test actor"));
        return false;
    }
    
    UBidirectionalMessageProtocol* Protocol = NewObject<UBidirectionalMessageProtocol>(TestActor);
    TestNotNull(TEXT("Protocol should be created"), Protocol);
    
    Protocol->RegisterComponent();
    Protocol->InitializeComponent();
    
    // Test initial state
    FMessageQueueMetrics Metrics = Protocol->GetMetrics();
    TestEqual(TEXT("Initial queue depth should be 0"), Metrics.QueueDepth, 0);
    TestEqual(TEXT("Initial enqueued count should be 0"), Metrics.TotalEnqueued, (int64)0);
    TestEqual(TEXT("Initial dequeued count should be 0"), Metrics.TotalDequeued, (int64)0);
    TestEqual(TEXT("Initial dropped count should be 0"), Metrics.TotalDropped, (int64)0);
    
    Protocol->UninitializeComponent();
    TestActor->Destroy();
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMessageProtocolSendReceiveTest, 
    "DeepTreeEcho.MessageProtocol.Protocol.SendReceive", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMessageProtocolSendReceiveTest::RunTest(const FString& Parameters)
{
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }
    
    AActor* TestActor = World->SpawnActor<AActor>();
    UBidirectionalMessageProtocol* Protocol = NewObject<UBidirectionalMessageProtocol>(TestActor);
    Protocol->RegisterComponent();
    Protocol->InitializeComponent();
    
    // Create and send a message
    FMessage Message;
    Message.MessageID = TEXT("test-msg-001");
    Message.MessageType = EMessageType::Event;
    Message.Priority = EMessagePriority::Normal;
    Message.Topic = TEXT("test.event");
    Message.SenderID = TEXT("TestSender");
    Message.RecipientID = TEXT("TestRecipient");
    
    TestTrue(TEXT("Should send message successfully"), Protocol->SendMessage(Message));
    
    // Receive the message
    FMessage ReceivedMessage;
    TestTrue(TEXT("Should receive message"), Protocol->ReceiveMessage(ReceivedMessage));
    
    // Verify message content
    TestEqual(TEXT("Message ID should match"), ReceivedMessage.MessageID, Message.MessageID);
    TestEqual(TEXT("Message type should match"), ReceivedMessage.MessageType, Message.MessageType);
    TestEqual(TEXT("Priority should match"), ReceivedMessage.Priority, Message.Priority);
    TestEqual(TEXT("Topic should match"), ReceivedMessage.Topic, Message.Topic);
    TestEqual(TEXT("Sender ID should match"), ReceivedMessage.SenderID, Message.SenderID);
    
    Protocol->UninitializeComponent();
    TestActor->Destroy();
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMessageProtocolPriorityTest, 
    "DeepTreeEcho.MessageProtocol.Protocol.PriorityScheduling", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMessageProtocolPriorityTest::RunTest(const FString& Parameters)
{
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }
    
    AActor* TestActor = World->SpawnActor<AActor>();
    UBidirectionalMessageProtocol* Protocol = NewObject<UBidirectionalMessageProtocol>(TestActor);
    Protocol->RegisterComponent();
    Protocol->InitializeComponent();
    
    // Send messages in reverse priority order
    FMessage LowPriorityMsg;
    LowPriorityMsg.MessageID = TEXT("low-priority");
    LowPriorityMsg.Priority = EMessagePriority::Low;
    LowPriorityMsg.MessageType = EMessageType::Event;
    Protocol->SendMessage(LowPriorityMsg);
    
    FMessage NormalPriorityMsg;
    NormalPriorityMsg.MessageID = TEXT("normal-priority");
    NormalPriorityMsg.Priority = EMessagePriority::Normal;
    NormalPriorityMsg.MessageType = EMessageType::Event;
    Protocol->SendMessage(NormalPriorityMsg);
    
    FMessage HighPriorityMsg;
    HighPriorityMsg.MessageID = TEXT("high-priority");
    HighPriorityMsg.Priority = EMessagePriority::High;
    HighPriorityMsg.MessageType = EMessageType::Event;
    Protocol->SendMessage(HighPriorityMsg);
    
    FMessage CriticalPriorityMsg;
    CriticalPriorityMsg.MessageID = TEXT("critical-priority");
    CriticalPriorityMsg.Priority = EMessagePriority::Critical;
    CriticalPriorityMsg.MessageType = EMessageType::Event;
    Protocol->SendMessage(CriticalPriorityMsg);
    
    // Receive messages - should come out in priority order
    FMessage Received1, Received2, Received3, Received4;
    
    Protocol->ReceiveMessage(Received1);
    TestEqual(TEXT("First message should be critical"), Received1.MessageID, TEXT("critical-priority"));
    
    Protocol->ReceiveMessage(Received2);
    TestEqual(TEXT("Second message should be high"), Received2.MessageID, TEXT("high-priority"));
    
    Protocol->ReceiveMessage(Received3);
    TestEqual(TEXT("Third message should be normal"), Received3.MessageID, TEXT("normal-priority"));
    
    Protocol->ReceiveMessage(Received4);
    TestEqual(TEXT("Fourth message should be low"), Received4.MessageID, TEXT("low-priority"));
    
    Protocol->UninitializeComponent();
    TestActor->Destroy();
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMessageProtocolBatchTest, 
    "DeepTreeEcho.MessageProtocol.Protocol.Batching", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMessageProtocolBatchTest::RunTest(const FString& Parameters)
{
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }
    
    AActor* TestActor = World->SpawnActor<AActor>();
    UBidirectionalMessageProtocol* Protocol = NewObject<UBidirectionalMessageProtocol>(TestActor);
    Protocol->RegisterComponent();
    Protocol->InitializeComponent();
    
    // Create a batch of messages
    FMessageBatch Batch;
    Batch.BatchID = TEXT("test-batch-001");
    
    for (int32 i = 0; i < 10; ++i)
    {
        FMessage Message;
        Message.MessageID = FString::Printf(TEXT("batch-msg-%d"), i);
        Message.MessageType = EMessageType::StateUpdate;
        Message.Priority = EMessagePriority::Normal;
        Message.bIsBatched = true;
        Message.BatchIndex = i;
        Message.BatchSize = 10;
        Batch.Messages.Add(Message);
    }
    
    // Send batch
    int32 SuccessCount = Protocol->SendBatch(Batch);
    TestEqual(TEXT("All batch messages should be sent"), SuccessCount, 10);
    
    // Receive batch
    TArray<FMessage> ReceivedMessages;
    int32 ReceivedCount = Protocol->ReceiveMessages(10, ReceivedMessages);
    TestEqual(TEXT("Should receive all batch messages"), ReceivedCount, 10);
    
    // Verify batch messages
    for (int32 i = 0; i < ReceivedMessages.Num(); ++i)
    {
        TestTrue(TEXT("Message should be marked as batched"), ReceivedMessages[i].bIsBatched);
        TestEqual(TEXT("Batch size should be 10"), ReceivedMessages[i].BatchSize, 10);
    }
    
    Protocol->UninitializeComponent();
    TestActor->Destroy();
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMessageProtocolSubscriptionTest, 
    "DeepTreeEcho.MessageProtocol.Protocol.Subscription", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMessageProtocolSubscriptionTest::RunTest(const FString& Parameters)
{
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }
    
    AActor* TestActor = World->SpawnActor<AActor>();
    UBidirectionalMessageProtocol* Protocol = NewObject<UBidirectionalMessageProtocol>(TestActor);
    Protocol->RegisterComponent();
    Protocol->InitializeComponent();
    
    // Subscribe to topics
    FString Sub1 = Protocol->Subscribe(TEXT("neural.*"), TEXT("Subscriber1"), EMessagePriority::Normal);
    FString Sub2 = Protocol->Subscribe(TEXT("symbolic.*"), TEXT("Subscriber2"), EMessagePriority::High);
    FString Sub3 = Protocol->Subscribe(TEXT("*"), TEXT("Subscriber3"), EMessagePriority::Low);
    
    TestNotEqual(TEXT("Subscription 1 should have valid ID"), Sub1, TEXT(""));
    TestNotEqual(TEXT("Subscription 2 should have valid ID"), Sub2, TEXT(""));
    TestNotEqual(TEXT("Subscription 3 should have valid ID"), Sub3, TEXT(""));
    TestEqual(TEXT("Should have 3 active subscriptions"), Protocol->GetSubscriptionCount(), 3);
    
    // Unsubscribe
    Protocol->Unsubscribe(Sub1);
    TestEqual(TEXT("Should have 2 active subscriptions after unsubscribe"), Protocol->GetSubscriptionCount(), 2);
    
    Protocol->UnsubscribeAll(TEXT("Subscriber2"));
    TestEqual(TEXT("Should have 1 active subscription after unsubscribe all"), Protocol->GetSubscriptionCount(), 1);
    
    Protocol->UninitializeComponent();
    TestActor->Destroy();
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMessageProtocolCommandTest, 
    "DeepTreeEcho.MessageProtocol.Protocol.Commands", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMessageProtocolCommandTest::RunTest(const FString& Parameters)
{
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }
    
    AActor* TestActor = World->SpawnActor<AActor>();
    UBidirectionalMessageProtocol* Protocol = NewObject<UBidirectionalMessageProtocol>(TestActor);
    Protocol->RegisterComponent();
    Protocol->InitializeComponent();
    
    // Send command
    FString MsgID = Protocol->SendCommand(TEXT("TargetComponent"), ECommandType::StartProcessing, EMessagePriority::High);
    TestNotEqual(TEXT("Command should have valid message ID"), MsgID, TEXT(""));
    
    // Receive command
    FMessage ReceivedMsg;
    TestTrue(TEXT("Should receive command message"), Protocol->ReceiveMessage(ReceivedMsg));
    TestEqual(TEXT("Message type should be Command"), ReceivedMsg.MessageType, EMessageType::Command);
    TestEqual(TEXT("Priority should be High"), ReceivedMsg.Priority, EMessagePriority::High);
    TestEqual(TEXT("Recipient should match"), ReceivedMsg.RecipientID, TEXT("TargetComponent"));
    
    Protocol->UninitializeComponent();
    TestActor->Destroy();
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMessageProtocolQueryTest, 
    "DeepTreeEcho.MessageProtocol.Protocol.Queries", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMessageProtocolQueryTest::RunTest(const FString& Parameters)
{
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }
    
    AActor* TestActor = World->SpawnActor<AActor>();
    UBidirectionalMessageProtocol* Protocol = NewObject<UBidirectionalMessageProtocol>(TestActor);
    Protocol->RegisterComponent();
    Protocol->InitializeComponent();
    
    // Send query
    FString CorrelationID = TEXT("query-001");
    FString MsgID = Protocol->SendQuery(TEXT("TargetComponent"), EQueryType::GetState, CorrelationID);
    TestNotEqual(TEXT("Query should have valid message ID"), MsgID, TEXT(""));
    
    // Receive query
    FMessage ReceivedMsg;
    TestTrue(TEXT("Should receive query message"), Protocol->ReceiveMessage(ReceivedMsg));
    TestEqual(TEXT("Message type should be Query"), ReceivedMsg.MessageType, EMessageType::Query);
    TestEqual(TEXT("Correlation ID should match"), ReceivedMsg.CorrelationID, CorrelationID);
    TestTrue(TEXT("Query should require acknowledgment"), ReceivedMsg.bRequiresAck);
    
    Protocol->UninitializeComponent();
    TestActor->Destroy();
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMessageProtocolEventTest, 
    "DeepTreeEcho.MessageProtocol.Protocol.Events", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMessageProtocolEventTest::RunTest(const FString& Parameters)
{
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }
    
    AActor* TestActor = World->SpawnActor<AActor>();
    UBidirectionalMessageProtocol* Protocol = NewObject<UBidirectionalMessageProtocol>(TestActor);
    Protocol->RegisterComponent();
    Protocol->InitializeComponent();
    
    // Send event
    FString MsgID = Protocol->SendEvent(TEXT("neural.statechange"), EEventType::StateChanged, EMessagePriority::Normal);
    TestNotEqual(TEXT("Event should have valid message ID"), MsgID, TEXT(""));
    
    // Receive event
    FMessage ReceivedMsg;
    TestTrue(TEXT("Should receive event message"), Protocol->ReceiveMessage(ReceivedMsg));
    TestEqual(TEXT("Message type should be Event"), ReceivedMsg.MessageType, EMessageType::Event);
    TestEqual(TEXT("Topic should match"), ReceivedMsg.Topic, TEXT("neural.statechange"));
    
    Protocol->UninitializeComponent();
    TestActor->Destroy();
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMessageProtocolMetricsTest, 
    "DeepTreeEcho.MessageProtocol.Protocol.Metrics", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMessageProtocolMetricsTest::RunTest(const FString& Parameters)
{
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }
    
    AActor* TestActor = World->SpawnActor<AActor>();
    UBidirectionalMessageProtocol* Protocol = NewObject<UBidirectionalMessageProtocol>(TestActor);
    Protocol->RegisterComponent();
    Protocol->InitializeComponent();
    
    // Send multiple messages
    for (int32 i = 0; i < 5; ++i)
    {
        FMessage Message;
        Message.MessageID = FString::Printf(TEXT("metric-test-%d"), i);
        Message.MessageType = EMessageType::Event;
        Message.Priority = EMessagePriority::Normal;
        Protocol->SendMessage(Message);
    }
    
    // Check metrics
    FMessageQueueMetrics Metrics = Protocol->GetMetrics();
    TestEqual(TEXT("Total enqueued should be 5"), Metrics.TotalEnqueued, (int64)5);
    
    // Receive messages
    FMessage ReceivedMsg;
    for (int32 i = 0; i < 5; ++i)
    {
        Protocol->ReceiveMessage(ReceivedMsg);
    }
    
    // Check updated metrics
    Metrics = Protocol->GetMetrics();
    TestEqual(TEXT("Total dequeued should be 5"), Metrics.TotalDequeued, (int64)5);
    TestEqual(TEXT("Queue should be empty"), Metrics.QueueDepth, 0);
    
    // Reset metrics
    Protocol->ResetMetrics();
    Metrics = Protocol->GetMetrics();
    TestEqual(TEXT("Total enqueued should be 0 after reset"), Metrics.TotalEnqueued, (int64)0);
    TestEqual(TEXT("Total dequeued should be 0 after reset"), Metrics.TotalDequeued, (int64)0);
    
    Protocol->UninitializeComponent();
    TestActor->Destroy();
    
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMessageProtocolLatencyTest, 
    "DeepTreeEcho.MessageProtocol.Protocol.Latency", 
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FMessageProtocolLatencyTest::RunTest(const FString& Parameters)
{
    UWorld* World = GetTestWorld();
    if (!World)
    {
        AddError(TEXT("Failed to get test world"));
        return false;
    }
    
    AActor* TestActor = World->SpawnActor<AActor>();
    UBidirectionalMessageProtocol* Protocol = NewObject<UBidirectionalMessageProtocol>(TestActor);
    Protocol->RegisterComponent();
    Protocol->InitializeComponent();
    
    // Send and receive messages to measure latency
    for (int32 i = 0; i < 100; ++i)
    {
        FMessage Message;
        Message.MessageID = FString::Printf(TEXT("latency-test-%d"), i);
        Message.MessageType = EMessageType::Event;
        Message.Priority = EMessagePriority::Normal;
        Protocol->SendMessage(Message);
        
        FMessage ReceivedMsg;
        Protocol->ReceiveMessage(ReceivedMsg);
    }
    
    // Check latency metrics
    FMessageQueueMetrics Metrics = Protocol->GetMetrics();
    
    // Average latency should be very low (microseconds)
    TestTrue(TEXT("Average latency should be < 100us"), Metrics.AverageLatencyUs < 100.0f);
    UE_LOG(LogTemp, Log, TEXT("Average latency: %.2f us"), Metrics.AverageLatencyUs);
    UE_LOG(LogTemp, Log, TEXT("Peak latency: %.2f us"), Metrics.PeakLatencyUs);
    
    Protocol->UninitializeComponent();
    TestActor->Destroy();
    
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

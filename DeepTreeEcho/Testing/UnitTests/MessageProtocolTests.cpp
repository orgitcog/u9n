/**
 * @file MessageProtocolTests.cpp
 * @brief GTest-based standalone tests for Bidirectional Message Protocol
 *
 * Feature F1.1.2: Tests the message protocol logic — send/receive,
 * priority scheduling, batch operations, topic routing, subscriptions,
 * metrics tracking, command/query/event helpers, and edge cases.
 *
 * Follows the project's established mock-UE pattern: all UE types are
 * replicated as plain C++ structs and the protocol logic is tested
 * through a standalone MockBidirectionalMessageProtocol class.
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <atomic>

// ============================================================================
// Mock UE types for standalone testing
// ============================================================================

#ifndef WITH_UNREAL_ENGINE
using int32 = int;
using int64 = long long;
using uint8 = unsigned char;
#define TEXT(x) x
#endif

// ============================================================================
// Standalone SPSC queue for protocol internals
// ============================================================================

template<typename T, size_t Capacity = 1024>
class MockSPSCQueue
{
public:
    MockSPSCQueue() : Head(0), Tail(0)
    {
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
        Buffer.resize(Capacity);
    }

    bool Enqueue(const T& Item)
    {
        size_t next = (Tail + 1) & (Capacity - 1);
        if (next == Head) return false;
        Buffer[Tail] = Item;
        Tail = next;
        return true;
    }

    bool Dequeue(T& OutItem)
    {
        if (Head == Tail) return false;
        OutItem = std::move(Buffer[Head]);
        Head = (Head + 1) & (Capacity - 1);
        return true;
    }

    size_t Size() const
    {
        return (Tail >= Head) ? (Tail - Head) : (Capacity - (Head - Tail));
    }

    bool IsEmpty() const { return Head == Tail; }

private:
    size_t Head;
    size_t Tail;
    std::vector<T> Buffer;
};

// ============================================================================
// Mock message types (mirror production enums/structs)
// ============================================================================

enum class MockMessageType : uint8
{
    Command     = 0,
    Query       = 1,
    Event       = 2,
    StateUpdate = 3,
    Response    = 4
};

enum class MockMessagePriority : uint8
{
    Critical = 0,
    High     = 1,
    Normal   = 2,
    Low      = 3
};

enum class MockCommandType : uint8
{
    StartProcessing = 0,
    StopProcessing  = 1,
    ResetState      = 2,
    UpdateConfig    = 3,
    FlushBuffer     = 4
};

enum class MockQueryType : uint8
{
    GetState         = 0,
    GetMetrics       = 1,
    GetConfiguration = 2,
    GetHistory       = 3
};

enum class MockEventType : uint8
{
    StateChanged       = 0,
    ThresholdExceeded  = 1,
    ErrorOccurred      = 2,
    ProcessingComplete = 3,
    ConfigUpdated      = 4
};

struct MockMessage
{
    std::string MessageID;
    MockMessageType MessageType = MockMessageType::Event;
    MockMessagePriority Priority = MockMessagePriority::Normal;
    int64 Timestamp = 0;
    std::string SenderID;
    std::string RecipientID;
    std::string Topic;
    std::string CorrelationID;
    std::vector<uint8> PayloadData;
    int64 EnqueueTime = 0;
    int64 Deadline = 0;
    bool bRequiresAck = false;
    bool bIsBatched = false;
    int32 BatchIndex = 0;
    int32 BatchSize = 1;
};

struct MockMessageBatch
{
    std::string BatchID;
    std::vector<MockMessage> Messages;
    int64 CreationTime = 0;
};

struct MockQueueMetrics
{
    int32 QueueDepth = 0;
    int64 TotalEnqueued = 0;
    int64 TotalDequeued = 0;
    int64 TotalDropped = 0;
    float AverageLatencyUs = 0.0f;
    float PeakLatencyUs = 0.0f;
};

struct MockRoutingConfig
{
    bool bEnableTopicRouting = true;
    bool bEnablePriorityScheduling = true;
    bool bEnableBatching = true;
    int32 BatchSizeThreshold = 16;
    int32 MaxQueueSizePerPriority = 1024;
};

struct MockTopicSubscription
{
    std::string TopicPattern;
    std::string SubscriberID;
    MockMessagePriority MinPriority = MockMessagePriority::Normal;
    std::function<void(const MockMessage&)> Callback;
};

// ============================================================================
// MockBidirectionalMessageProtocol
// Replicates the production protocol's logic for standalone testing
// ============================================================================

class MockBidirectionalMessageProtocol
{
public:
    MockBidirectionalMessageProtocol()
    {
        ComponentID = "MockProtocol-001";
        NextMessageCounter = 0;
        for (int i = 0; i < 4; ++i)
            Metrics[i] = MockQueueMetrics{};
    }

    // --- Sending ---

    bool SendMessage(const MockMessage& Message)
    {
        int32 idx = static_cast<int32>(Message.Priority);
        if (idx < 0 || idx >= 4) return false;

        MockMessage copy = Message;
        copy.EnqueueTime = NowMicroseconds();

        bool ok = PriorityQueues[idx].Enqueue(copy);
        if (ok)
        {
            Metrics[idx].TotalEnqueued++;
            Metrics[idx].QueueDepth = static_cast<int32>(PriorityQueues[idx].Size());
        }
        else
        {
            Metrics[idx].TotalDropped++;
        }
        return ok;
    }

    int32 SendBatch(const MockMessageBatch& Batch)
    {
        int32 ok = 0;
        for (auto& m : Batch.Messages)
            if (SendMessage(m)) ++ok;
        return ok;
    }

    std::string SendCommand(const std::string& RecipientID, MockCommandType CmdType,
                            MockMessagePriority Priority = MockMessagePriority::Normal)
    {
        MockMessage m;
        m.MessageID = GenerateID();
        m.MessageType = MockMessageType::Command;
        m.Priority = Priority;
        m.Timestamp = NowMicroseconds();
        m.SenderID = ComponentID;
        m.RecipientID = RecipientID;
        m.Topic = "command";
        m.PayloadData.push_back(static_cast<uint8>(CmdType));
        return SendMessage(m) ? m.MessageID : "";
    }

    std::string SendQuery(const std::string& RecipientID, MockQueryType QType,
                          const std::string& CorrelationID)
    {
        MockMessage m;
        m.MessageID = GenerateID();
        m.MessageType = MockMessageType::Query;
        m.Priority = MockMessagePriority::High;
        m.Timestamp = NowMicroseconds();
        m.SenderID = ComponentID;
        m.RecipientID = RecipientID;
        m.Topic = "query";
        m.CorrelationID = CorrelationID;
        m.bRequiresAck = true;
        m.PayloadData.push_back(static_cast<uint8>(QType));
        return SendMessage(m) ? m.MessageID : "";
    }

    std::string SendEvent(const std::string& Topic, MockEventType EvType,
                          MockMessagePriority Priority = MockMessagePriority::Normal)
    {
        MockMessage m;
        m.MessageID = GenerateID();
        m.MessageType = MockMessageType::Event;
        m.Priority = Priority;
        m.Timestamp = NowMicroseconds();
        m.SenderID = ComponentID;
        m.Topic = Topic;
        m.PayloadData.push_back(static_cast<uint8>(EvType));
        return SendMessage(m) ? m.MessageID : "";
    }

    // --- Receiving ---

    bool ReceiveMessage(MockMessage& OutMessage)
    {
        if (!Config.bEnablePriorityScheduling)
        {
            for (int i = 0; i < 4; ++i)
                if (PriorityQueues[i].Dequeue(OutMessage))
                    return true;
            return false;
        }

        // Priority order: Critical(0) > High(1) > Normal(2) > Low(3)
        for (int i = 0; i < 4; ++i)
        {
            if (PriorityQueues[i].Dequeue(OutMessage))
            {
                int64 now = NowMicroseconds();
                float latency = static_cast<float>(now - OutMessage.EnqueueTime);
                Metrics[i].TotalDequeued++;
                const float Alpha = 0.1f;
                Metrics[i].AverageLatencyUs = (1.0f - Alpha) * Metrics[i].AverageLatencyUs + Alpha * latency;
                if (latency > Metrics[i].PeakLatencyUs)
                    Metrics[i].PeakLatencyUs = latency;
                return true;
            }
        }
        return false;
    }

    int32 ReceiveMessages(int32 MaxMessages, std::vector<MockMessage>& OutMessages)
    {
        OutMessages.clear();
        OutMessages.reserve(MaxMessages);
        MockMessage m;
        int32 count = 0;
        while (count < MaxMessages && ReceiveMessage(m))
        {
            OutMessages.push_back(std::move(m));
            ++count;
        }
        return count;
    }

    // --- Subscriptions ---

    std::string Subscribe(const std::string& TopicPattern, const std::string& SubscriberID,
                          MockMessagePriority MinPriority = MockMessagePriority::Normal)
    {
        MockTopicSubscription sub;
        sub.TopicPattern = TopicPattern;
        sub.SubscriberID = SubscriberID;
        sub.MinPriority = MinPriority;
        Subscriptions.push_back(sub);
        return SubscriberID + ":" + TopicPattern;
    }

    void Unsubscribe(const std::string& SubscriptionID)
    {
        auto colon = SubscriptionID.find(':');
        if (colon == std::string::npos) return;
        std::string subID = SubscriptionID.substr(0, colon);
        std::string pattern = SubscriptionID.substr(colon + 1);
        Subscriptions.erase(
            std::remove_if(Subscriptions.begin(), Subscriptions.end(),
                [&](const MockTopicSubscription& s) {
                    return s.SubscriberID == subID && s.TopicPattern == pattern;
                }),
            Subscriptions.end());
    }

    void UnsubscribeAll(const std::string& SubscriberID)
    {
        Subscriptions.erase(
            std::remove_if(Subscriptions.begin(), Subscriptions.end(),
                [&](const MockTopicSubscription& s) { return s.SubscriberID == SubscriberID; }),
            Subscriptions.end());
    }

    int32 GetSubscriptionCount() const
    {
        return static_cast<int32>(Subscriptions.size());
    }

    // --- Metrics ---

    MockQueueMetrics GetAggregatedMetrics() const
    {
        MockQueueMetrics agg;
        for (int i = 0; i < 4; ++i)
        {
            agg.TotalEnqueued += Metrics[i].TotalEnqueued;
            agg.TotalDequeued += Metrics[i].TotalDequeued;
            agg.TotalDropped += Metrics[i].TotalDropped;
            agg.QueueDepth += Metrics[i].QueueDepth;
            if (Metrics[i].PeakLatencyUs > agg.PeakLatencyUs)
                agg.PeakLatencyUs = Metrics[i].PeakLatencyUs;
        }
        return agg;
    }

    MockQueueMetrics GetMetricsForPriority(MockMessagePriority p) const
    {
        int32 idx = static_cast<int32>(p);
        if (idx >= 0 && idx < 4) return Metrics[idx];
        return MockQueueMetrics{};
    }

    void ResetMetrics()
    {
        for (int i = 0; i < 4; ++i)
            Metrics[i] = MockQueueMetrics{};
    }

    // --- Configuration ---

    MockRoutingConfig GetRoutingConfig() const { return Config; }
    void SetRoutingConfig(const MockRoutingConfig& c) { Config = c; }

    // --- Topic matching (exposed for testing) ---

    bool MatchesTopicPattern(const std::string& Topic, const std::string& Pattern) const
    {
        if (Pattern == "*") return true;
        // "prefix.*" matches topics starting with "prefix."
        if (Pattern.size() >= 2 && Pattern.substr(Pattern.size() - 2) == ".*")
        {
            std::string prefix = Pattern.substr(0, Pattern.size() - 2);
            return Topic.substr(0, prefix.size()) == prefix;
        }
        return Topic == Pattern;
    }

private:
    std::string GenerateID()
    {
        return "msg-" + std::to_string(++NextMessageCounter);
    }

    int64 NowMicroseconds() const
    {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
    }

    std::string ComponentID;
    std::atomic<int64> NextMessageCounter;
    MockSPSCQueue<MockMessage, 1024> PriorityQueues[4];
    std::vector<MockTopicSubscription> Subscriptions;
    MockQueueMetrics Metrics[4];
    MockRoutingConfig Config;
};

// ============================================================================
// Test Fixture
// ============================================================================

class MessageProtocolTest : public ::testing::Test
{
protected:
    MockBidirectionalMessageProtocol Protocol;
};

// ============================================================================
// Basic Send / Receive
// ============================================================================

TEST_F(MessageProtocolTest, SendAndReceiveSingleMessage)
{
    MockMessage msg;
    msg.MessageID = "test-001";
    msg.MessageType = MockMessageType::Event;
    msg.Priority = MockMessagePriority::Normal;
    msg.Topic = "test.event";
    msg.SenderID = "sender";
    msg.RecipientID = "recipient";

    EXPECT_TRUE(Protocol.SendMessage(msg));

    MockMessage received;
    EXPECT_TRUE(Protocol.ReceiveMessage(received));
    EXPECT_EQ(received.MessageID, "test-001");
    EXPECT_EQ(received.Topic, "test.event");
    EXPECT_EQ(received.SenderID, "sender");
}

TEST_F(MessageProtocolTest, EmptyReceiveReturnsFalse)
{
    MockMessage m;
    EXPECT_FALSE(Protocol.ReceiveMessage(m));
}

TEST_F(MessageProtocolTest, FIFOOrderWithinPriority)
{
    for (int i = 0; i < 5; ++i)
    {
        MockMessage msg;
        msg.MessageID = "msg-" + std::to_string(i);
        msg.Priority = MockMessagePriority::Normal;
        Protocol.SendMessage(msg);
    }

    for (int i = 0; i < 5; ++i)
    {
        MockMessage out;
        EXPECT_TRUE(Protocol.ReceiveMessage(out));
        EXPECT_EQ(out.MessageID, "msg-" + std::to_string(i));
    }
}

// ============================================================================
// Priority Scheduling
// ============================================================================

TEST_F(MessageProtocolTest, PriorityOrdering)
{
    // Enqueue in reverse priority order
    MockMessage low;  low.MessageID = "low";   low.Priority = MockMessagePriority::Low;
    MockMessage norm; norm.MessageID = "norm";  norm.Priority = MockMessagePriority::Normal;
    MockMessage high; high.MessageID = "high";  high.Priority = MockMessagePriority::High;
    MockMessage crit; crit.MessageID = "crit";  crit.Priority = MockMessagePriority::Critical;

    Protocol.SendMessage(low);
    Protocol.SendMessage(norm);
    Protocol.SendMessage(high);
    Protocol.SendMessage(crit);

    MockMessage r1, r2, r3, r4;
    EXPECT_TRUE(Protocol.ReceiveMessage(r1));
    EXPECT_TRUE(Protocol.ReceiveMessage(r2));
    EXPECT_TRUE(Protocol.ReceiveMessage(r3));
    EXPECT_TRUE(Protocol.ReceiveMessage(r4));

    EXPECT_EQ(r1.MessageID, "crit");
    EXPECT_EQ(r2.MessageID, "high");
    EXPECT_EQ(r3.MessageID, "norm");
    EXPECT_EQ(r4.MessageID, "low");
}

TEST_F(MessageProtocolTest, PriorityDisabled)
{
    MockRoutingConfig cfg;
    cfg.bEnablePriorityScheduling = false;
    Protocol.SetRoutingConfig(cfg);

    MockMessage low;  low.MessageID = "low";   low.Priority = MockMessagePriority::Low;
    MockMessage crit; crit.MessageID = "crit";  crit.Priority = MockMessagePriority::Critical;

    Protocol.SendMessage(low);
    Protocol.SendMessage(crit);

    // Without priority scheduling, still dequeues from queues in order 0..3
    MockMessage r1, r2;
    EXPECT_TRUE(Protocol.ReceiveMessage(r1));
    EXPECT_TRUE(Protocol.ReceiveMessage(r2));
    EXPECT_EQ(r1.MessageID, "crit"); // queue 0 checked first
    EXPECT_EQ(r2.MessageID, "low");
}

// ============================================================================
// Batch Operations
// ============================================================================

TEST_F(MessageProtocolTest, BatchSendAndReceive)
{
    MockMessageBatch batch;
    batch.BatchID = "batch-001";
    for (int i = 0; i < 10; ++i)
    {
        MockMessage msg;
        msg.MessageID = "b-" + std::to_string(i);
        msg.MessageType = MockMessageType::StateUpdate;
        msg.Priority = MockMessagePriority::Normal;
        msg.bIsBatched = true;
        msg.BatchIndex = i;
        msg.BatchSize = 10;
        batch.Messages.push_back(msg);
    }

    EXPECT_EQ(Protocol.SendBatch(batch), 10);

    std::vector<MockMessage> received;
    EXPECT_EQ(Protocol.ReceiveMessages(10, received), 10);

    for (int i = 0; i < 10; ++i)
    {
        EXPECT_TRUE(received[i].bIsBatched);
        EXPECT_EQ(received[i].BatchSize, 10);
    }
}

TEST_F(MessageProtocolTest, ReceiveMessagesMaxLimit)
{
    for (int i = 0; i < 20; ++i)
    {
        MockMessage msg;
        msg.MessageID = "x-" + std::to_string(i);
        msg.Priority = MockMessagePriority::Normal;
        Protocol.SendMessage(msg);
    }

    std::vector<MockMessage> out;
    EXPECT_EQ(Protocol.ReceiveMessages(5, out), 5);
    EXPECT_EQ(out.size(), 5u);
}

// ============================================================================
// Command / Query / Event helpers
// ============================================================================

TEST_F(MessageProtocolTest, SendCommand)
{
    std::string id = Protocol.SendCommand("target", MockCommandType::StartProcessing,
                                          MockMessagePriority::High);
    EXPECT_FALSE(id.empty());

    MockMessage r;
    EXPECT_TRUE(Protocol.ReceiveMessage(r));
    EXPECT_EQ(r.MessageType, MockMessageType::Command);
    EXPECT_EQ(r.Priority, MockMessagePriority::High);
    EXPECT_EQ(r.RecipientID, "target");
    EXPECT_EQ(r.PayloadData.size(), 1u);
    EXPECT_EQ(r.PayloadData[0], static_cast<uint8>(MockCommandType::StartProcessing));
}

TEST_F(MessageProtocolTest, SendQuery)
{
    std::string id = Protocol.SendQuery("target", MockQueryType::GetState, "corr-42");
    EXPECT_FALSE(id.empty());

    MockMessage r;
    EXPECT_TRUE(Protocol.ReceiveMessage(r));
    EXPECT_EQ(r.MessageType, MockMessageType::Query);
    EXPECT_EQ(r.CorrelationID, "corr-42");
    EXPECT_TRUE(r.bRequiresAck);
    EXPECT_EQ(r.Priority, MockMessagePriority::High); // queries default to high
}

TEST_F(MessageProtocolTest, SendEvent)
{
    std::string id = Protocol.SendEvent("neural.statechange", MockEventType::StateChanged,
                                        MockMessagePriority::Normal);
    EXPECT_FALSE(id.empty());

    MockMessage r;
    EXPECT_TRUE(Protocol.ReceiveMessage(r));
    EXPECT_EQ(r.MessageType, MockMessageType::Event);
    EXPECT_EQ(r.Topic, "neural.statechange");
}

// ============================================================================
// Subscriptions
// ============================================================================

TEST_F(MessageProtocolTest, SubscribeAndUnsubscribe)
{
    std::string sub1 = Protocol.Subscribe("neural.*", "sub1");
    std::string sub2 = Protocol.Subscribe("symbolic.*", "sub2", MockMessagePriority::High);
    std::string sub3 = Protocol.Subscribe("*", "sub3", MockMessagePriority::Low);

    EXPECT_FALSE(sub1.empty());
    EXPECT_FALSE(sub2.empty());
    EXPECT_EQ(Protocol.GetSubscriptionCount(), 3);

    Protocol.Unsubscribe(sub1);
    EXPECT_EQ(Protocol.GetSubscriptionCount(), 2);

    Protocol.UnsubscribeAll("sub2");
    EXPECT_EQ(Protocol.GetSubscriptionCount(), 1);

    Protocol.UnsubscribeAll("sub3");
    EXPECT_EQ(Protocol.GetSubscriptionCount(), 0);
}

// ============================================================================
// Topic Pattern Matching
// ============================================================================

TEST_F(MessageProtocolTest, TopicWildcardMatchAll)
{
    EXPECT_TRUE(Protocol.MatchesTopicPattern("anything", "*"));
    EXPECT_TRUE(Protocol.MatchesTopicPattern("neural.state", "*"));
}

TEST_F(MessageProtocolTest, TopicPrefixWildcard)
{
    EXPECT_TRUE(Protocol.MatchesTopicPattern("neural.state", "neural.*"));
    EXPECT_TRUE(Protocol.MatchesTopicPattern("neural.output", "neural.*"));
    EXPECT_FALSE(Protocol.MatchesTopicPattern("symbolic.state", "neural.*"));
}

TEST_F(MessageProtocolTest, TopicExactMatch)
{
    EXPECT_TRUE(Protocol.MatchesTopicPattern("neural.state", "neural.state"));
    EXPECT_FALSE(Protocol.MatchesTopicPattern("neural.state", "neural.output"));
}

// ============================================================================
// Metrics
// ============================================================================

TEST_F(MessageProtocolTest, MetricsTracking)
{
    // Send 5 messages
    for (int i = 0; i < 5; ++i)
    {
        MockMessage msg;
        msg.MessageID = "m-" + std::to_string(i);
        msg.Priority = MockMessagePriority::Normal;
        Protocol.SendMessage(msg);
    }

    auto metrics = Protocol.GetAggregatedMetrics();
    EXPECT_EQ(metrics.TotalEnqueued, 5);
    EXPECT_EQ(metrics.TotalDropped, 0);

    // Receive 3
    for (int i = 0; i < 3; ++i)
    {
        MockMessage out;
        Protocol.ReceiveMessage(out);
    }

    metrics = Protocol.GetAggregatedMetrics();
    EXPECT_EQ(metrics.TotalDequeued, 3);
}

TEST_F(MessageProtocolTest, MetricsPerPriority)
{
    MockMessage high; high.Priority = MockMessagePriority::High;
    MockMessage norm; norm.Priority = MockMessagePriority::Normal;
    Protocol.SendMessage(high);
    Protocol.SendMessage(norm);
    Protocol.SendMessage(norm);

    auto hm = Protocol.GetMetricsForPriority(MockMessagePriority::High);
    auto nm = Protocol.GetMetricsForPriority(MockMessagePriority::Normal);
    EXPECT_EQ(hm.TotalEnqueued, 1);
    EXPECT_EQ(nm.TotalEnqueued, 2);
}

TEST_F(MessageProtocolTest, MetricsReset)
{
    MockMessage msg; msg.Priority = MockMessagePriority::Normal;
    Protocol.SendMessage(msg);
    Protocol.ResetMetrics();

    auto m = Protocol.GetAggregatedMetrics();
    EXPECT_EQ(m.TotalEnqueued, 0);
    EXPECT_EQ(m.TotalDequeued, 0);
}

// ============================================================================
// Configuration
// ============================================================================

TEST_F(MessageProtocolTest, ConfigurationUpdate)
{
    MockRoutingConfig cfg;
    cfg.bEnableTopicRouting = false;
    cfg.bEnableBatching = false;
    cfg.BatchSizeThreshold = 32;
    Protocol.SetRoutingConfig(cfg);

    auto retrieved = Protocol.GetRoutingConfig();
    EXPECT_FALSE(retrieved.bEnableTopicRouting);
    EXPECT_FALSE(retrieved.bEnableBatching);
    EXPECT_EQ(retrieved.BatchSizeThreshold, 32);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(MessageProtocolTest, UniqueMessageIDs)
{
    std::string id1 = Protocol.SendCommand("a", MockCommandType::ResetState);
    std::string id2 = Protocol.SendCommand("b", MockCommandType::FlushBuffer);
    EXPECT_NE(id1, id2);
}

TEST_F(MessageProtocolTest, MultipleMessageTypes)
{
    Protocol.SendCommand("t", MockCommandType::StartProcessing, MockMessagePriority::Critical);
    Protocol.SendQuery("t", MockQueryType::GetMetrics, "q1");
    Protocol.SendEvent("test.topic", MockEventType::ProcessingComplete);

    MockMessage r;
    // Critical command comes first
    EXPECT_TRUE(Protocol.ReceiveMessage(r));
    EXPECT_EQ(r.MessageType, MockMessageType::Command);

    // High-priority query next
    EXPECT_TRUE(Protocol.ReceiveMessage(r));
    EXPECT_EQ(r.MessageType, MockMessageType::Query);

    // Normal event last
    EXPECT_TRUE(Protocol.ReceiveMessage(r));
    EXPECT_EQ(r.MessageType, MockMessageType::Event);
}

TEST_F(MessageProtocolTest, LargePayload)
{
    MockMessage msg;
    msg.Priority = MockMessagePriority::Normal;
    msg.PayloadData.resize(4096, 0xAB);
    EXPECT_TRUE(Protocol.SendMessage(msg));

    MockMessage out;
    EXPECT_TRUE(Protocol.ReceiveMessage(out));
    EXPECT_EQ(out.PayloadData.size(), 4096u);
    EXPECT_EQ(out.PayloadData[0], 0xAB);
    EXPECT_EQ(out.PayloadData[4095], 0xAB);
}

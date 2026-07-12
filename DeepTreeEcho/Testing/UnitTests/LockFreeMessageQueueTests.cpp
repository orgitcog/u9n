/**
 * @file LockFreeMessageQueueTests.cpp
 * @brief GTest-based standalone tests for lock-free message queues
 *
 * Feature F1.1.2 / F1.1.3: Tests cover SPSC and MPSC queue implementations
 * used by the Bidirectional Message Protocol.
 *
 * Tests:
 *  - SPSC basic enqueue/dequeue
 *  - SPSC capacity and overflow
 *  - SPSC fill/drain cycles
 *  - SPSC move semantics
 *  - SPSC wraparound correctness
 *  - MPSC basic enqueue/dequeue
 *  - MPSC capacity and overflow
 *  - MPSC sequential multi-producer pattern
 *  - Edge cases (empty dequeue, size tracking)
 */

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <atomic>
#include <thread>

// ============================================================================
// Standalone copies of lock-free queue templates
// The production headers include UE-specific headers (CoreMinimal.h,
// HAL/Platform.h) that are unavailable outside the engine. We replicate
// the template logic here for standalone GTest testing — the same pattern
// used by every other GTest file in the repo.
// ============================================================================

template<typename T, size_t Capacity = 1024>
class TestSPSCQueue
{
public:
    TestSPSCQueue() : Head(0), Tail(0)
    {
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
        Buffer = new T[Capacity];
    }

    ~TestSPSCQueue() { delete[] Buffer; }

    bool Enqueue(const T& Item)
    {
        const size_t CurrentTail = Tail.load(std::memory_order_relaxed);
        const size_t NextTail = Increment(CurrentTail);
        if (NextTail == Head.load(std::memory_order_acquire))
            return false;
        Buffer[CurrentTail] = Item;
        Tail.store(NextTail, std::memory_order_release);
        return true;
    }

    bool Enqueue(T&& Item)
    {
        const size_t CurrentTail = Tail.load(std::memory_order_relaxed);
        const size_t NextTail = Increment(CurrentTail);
        if (NextTail == Head.load(std::memory_order_acquire))
            return false;
        Buffer[CurrentTail] = std::move(Item);
        Tail.store(NextTail, std::memory_order_release);
        return true;
    }

    bool Dequeue(T& OutItem)
    {
        const size_t CurrentHead = Head.load(std::memory_order_relaxed);
        if (CurrentHead == Tail.load(std::memory_order_acquire))
            return false;
        OutItem = std::move(Buffer[CurrentHead]);
        Head.store(Increment(CurrentHead), std::memory_order_release);
        return true;
    }

    size_t Size() const
    {
        const size_t t = Tail.load(std::memory_order_acquire);
        const size_t h = Head.load(std::memory_order_acquire);
        return (t >= h) ? (t - h) : (Capacity - (h - t));
    }

    bool IsEmpty() const
    {
        return Head.load(std::memory_order_acquire) == Tail.load(std::memory_order_acquire);
    }

    bool IsFull() const
    {
        return Increment(Tail.load(std::memory_order_acquire)) == Head.load(std::memory_order_acquire);
    }

    size_t GetCapacity() const { return Capacity - 1; }

private:
    size_t Increment(size_t Index) const { return (Index + 1) & (Capacity - 1); }

    alignas(64) std::atomic<size_t> Head;
    alignas(64) std::atomic<size_t> Tail;
    T* Buffer;
};

template<typename T, size_t Capacity = 1024>
class TestMPSCQueue
{
public:
    TestMPSCQueue() : Head(0), Tail(0)
    {
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
        Buffer = new T[Capacity];
        Sequence = new std::atomic<size_t>[Capacity];
        for (size_t i = 0; i < Capacity; ++i)
            Sequence[i].store(i, std::memory_order_relaxed);
    }

    ~TestMPSCQueue()
    {
        delete[] Buffer;
        delete[] Sequence;
    }

    bool Enqueue(const T& Item)
    {
        size_t Pos;
        while (true)
        {
            Pos = Tail.load(std::memory_order_relaxed);
            const size_t Seq = Sequence[Pos & (Capacity - 1)].load(std::memory_order_acquire);
            const intptr_t Diff = static_cast<intptr_t>(Seq) - static_cast<intptr_t>(Pos);
            if (Diff == 0)
            {
                if (Tail.compare_exchange_weak(Pos, Pos + 1, std::memory_order_relaxed))
                    break;
            }
            else if (Diff < 0)
                return false;
        }
        Buffer[Pos & (Capacity - 1)] = Item;
        Sequence[Pos & (Capacity - 1)].store(Pos + 1, std::memory_order_release);
        return true;
    }

    bool Enqueue(T&& Item)
    {
        size_t Pos;
        while (true)
        {
            Pos = Tail.load(std::memory_order_relaxed);
            const size_t Seq = Sequence[Pos & (Capacity - 1)].load(std::memory_order_acquire);
            const intptr_t Diff = static_cast<intptr_t>(Seq) - static_cast<intptr_t>(Pos);
            if (Diff == 0)
            {
                if (Tail.compare_exchange_weak(Pos, Pos + 1, std::memory_order_relaxed))
                    break;
            }
            else if (Diff < 0)
                return false;
        }
        Buffer[Pos & (Capacity - 1)] = std::move(Item);
        Sequence[Pos & (Capacity - 1)].store(Pos + 1, std::memory_order_release);
        return true;
    }

    bool Dequeue(T& OutItem)
    {
        size_t Pos = Head.load(std::memory_order_relaxed);
        while (true)
        {
            const size_t Seq = Sequence[Pos & (Capacity - 1)].load(std::memory_order_acquire);
            const intptr_t Diff = static_cast<intptr_t>(Seq) - static_cast<intptr_t>(Pos + 1);
            if (Diff == 0)
            {
                OutItem = std::move(Buffer[Pos & (Capacity - 1)]);
                Sequence[Pos & (Capacity - 1)].store(Pos + Capacity, std::memory_order_release);
                Head.store(Pos + 1, std::memory_order_release);
                return true;
            }
            else if (Diff < 0)
                return false;
            Pos = Head.load(std::memory_order_relaxed);
        }
    }

    size_t Size() const
    {
        const size_t t = Tail.load(std::memory_order_acquire);
        const size_t h = Head.load(std::memory_order_acquire);
        return (t >= h) ? (t - h) : (Capacity - (h - t));
    }

    bool IsEmpty() const
    {
        const size_t h = Head.load(std::memory_order_acquire);
        const size_t Seq = Sequence[h & (Capacity - 1)].load(std::memory_order_acquire);
        return static_cast<intptr_t>(Seq) - static_cast<intptr_t>(h + 1) < 0;
    }

    size_t GetCapacity() const { return Capacity; }

private:
    alignas(64) std::atomic<size_t> Head;
    alignas(64) std::atomic<size_t> Tail;
    T* Buffer;
    std::atomic<size_t>* Sequence;
};

// ============================================================================
// SPSC Queue Tests
// ============================================================================

class SPSCQueueTest : public ::testing::Test {};

TEST_F(SPSCQueueTest, InitialState)
{
    TestSPSCQueue<int, 16> q;
    EXPECT_TRUE(q.IsEmpty());
    EXPECT_FALSE(q.IsFull());
    EXPECT_EQ(q.Size(), 0u);
    EXPECT_EQ(q.GetCapacity(), 15u); // one slot reserved
}

TEST_F(SPSCQueueTest, BasicEnqueueDequeue)
{
    TestSPSCQueue<int, 16> q;
    EXPECT_TRUE(q.Enqueue(42));
    EXPECT_FALSE(q.IsEmpty());
    EXPECT_EQ(q.Size(), 1u);

    int val = 0;
    EXPECT_TRUE(q.Dequeue(val));
    EXPECT_EQ(val, 42);
    EXPECT_TRUE(q.IsEmpty());
}

TEST_F(SPSCQueueTest, MultipleItems)
{
    TestSPSCQueue<int, 16> q;
    for (int i = 1; i <= 5; ++i)
        EXPECT_TRUE(q.Enqueue(i));

    EXPECT_EQ(q.Size(), 5u);

    for (int i = 1; i <= 5; ++i)
    {
        int val = 0;
        EXPECT_TRUE(q.Dequeue(val));
        EXPECT_EQ(val, i);
    }
    EXPECT_TRUE(q.IsEmpty());
}

TEST_F(SPSCQueueTest, CapacityLimit)
{
    TestSPSCQueue<int, 8> q; // capacity 8, usable 7
    for (int i = 0; i < 7; ++i)
        EXPECT_TRUE(q.Enqueue(i));

    EXPECT_TRUE(q.IsFull());
    EXPECT_FALSE(q.Enqueue(999)); // overflow rejected
}

TEST_F(SPSCQueueTest, FillDrainCycle)
{
    TestSPSCQueue<int, 8> q;

    // Fill completely
    for (int i = 0; i < 7; ++i)
        EXPECT_TRUE(q.Enqueue(i));
    EXPECT_TRUE(q.IsFull());

    // Drain completely
    for (int i = 0; i < 7; ++i)
    {
        int val = -1;
        EXPECT_TRUE(q.Dequeue(val));
        EXPECT_EQ(val, i);
    }
    EXPECT_TRUE(q.IsEmpty());

    // Refill — verifies wraparound
    for (int i = 100; i < 107; ++i)
        EXPECT_TRUE(q.Enqueue(i));
    EXPECT_TRUE(q.IsFull());

    for (int i = 100; i < 107; ++i)
    {
        int val = -1;
        EXPECT_TRUE(q.Dequeue(val));
        EXPECT_EQ(val, i);
    }
    EXPECT_TRUE(q.IsEmpty());
}

TEST_F(SPSCQueueTest, EmptyDequeue)
{
    TestSPSCQueue<int, 16> q;
    int val = 0;
    EXPECT_FALSE(q.Dequeue(val));
}

TEST_F(SPSCQueueTest, MoveSemantics)
{
    TestSPSCQueue<std::string, 16> q;
    std::string input = "hello world";
    EXPECT_TRUE(q.Enqueue(std::move(input)));

    std::string output;
    EXPECT_TRUE(q.Dequeue(output));
    EXPECT_EQ(output, "hello world");
}

TEST_F(SPSCQueueTest, WraparoundCorrectness)
{
    TestSPSCQueue<int, 4> q; // tiny: capacity 4, usable 3

    // Fill and drain multiple times to exercise wraparound
    for (int round = 0; round < 10; ++round)
    {
        for (int i = 0; i < 3; ++i)
            EXPECT_TRUE(q.Enqueue(round * 10 + i));

        for (int i = 0; i < 3; ++i)
        {
            int val = -1;
            EXPECT_TRUE(q.Dequeue(val));
            EXPECT_EQ(val, round * 10 + i);
        }
        EXPECT_TRUE(q.IsEmpty());
    }
}

TEST_F(SPSCQueueTest, InterleavedOps)
{
    TestSPSCQueue<int, 16> q;

    // Interleave enqueue and dequeue
    EXPECT_TRUE(q.Enqueue(1));
    EXPECT_TRUE(q.Enqueue(2));

    int val = 0;
    EXPECT_TRUE(q.Dequeue(val));
    EXPECT_EQ(val, 1);

    EXPECT_TRUE(q.Enqueue(3));
    EXPECT_TRUE(q.Dequeue(val));
    EXPECT_EQ(val, 2);
    EXPECT_TRUE(q.Dequeue(val));
    EXPECT_EQ(val, 3);
    EXPECT_TRUE(q.IsEmpty());
}

// ============================================================================
// MPSC Queue Tests
// ============================================================================

class MPSCQueueTest : public ::testing::Test {};

TEST_F(MPSCQueueTest, InitialState)
{
    TestMPSCQueue<int, 16> q;
    EXPECT_TRUE(q.IsEmpty());
    EXPECT_EQ(q.Size(), 0u);
}

TEST_F(MPSCQueueTest, BasicEnqueueDequeue)
{
    TestMPSCQueue<int, 16> q;
    EXPECT_TRUE(q.Enqueue(10));
    EXPECT_TRUE(q.Enqueue(20));
    EXPECT_TRUE(q.Enqueue(30));

    EXPECT_FALSE(q.IsEmpty());
    EXPECT_EQ(q.Size(), 3u);

    int val = 0;
    EXPECT_TRUE(q.Dequeue(val)); EXPECT_EQ(val, 10);
    EXPECT_TRUE(q.Dequeue(val)); EXPECT_EQ(val, 20);
    EXPECT_TRUE(q.Dequeue(val)); EXPECT_EQ(val, 30);
    EXPECT_TRUE(q.IsEmpty());
}

TEST_F(MPSCQueueTest, CapacityLimit)
{
    TestMPSCQueue<int, 8> q;
    int accepted = 0;
    for (int i = 0; i < 16; ++i)
    {
        if (q.Enqueue(i))
            ++accepted;
    }
    // MPSC uses all slots via sequence numbers, capacity = 8
    EXPECT_EQ(accepted, 8);
}

TEST_F(MPSCQueueTest, MoveSemantics)
{
    TestMPSCQueue<std::string, 16> q;
    std::string s = "test message";
    EXPECT_TRUE(q.Enqueue(std::move(s)));

    std::string out;
    EXPECT_TRUE(q.Dequeue(out));
    EXPECT_EQ(out, "test message");
}

TEST_F(MPSCQueueTest, SequentialMultiProducerPattern)
{
    // Simulate multiple producers sequentially (validates the CAS logic path)
    TestMPSCQueue<int, 64> q;

    // "Producer 1" enqueues evens
    for (int i = 0; i < 10; i += 2)
        EXPECT_TRUE(q.Enqueue(i));

    // "Producer 2" enqueues odds
    for (int i = 1; i < 10; i += 2)
        EXPECT_TRUE(q.Enqueue(i));

    EXPECT_EQ(q.Size(), 10u);

    // Consume all — order should be evens then odds
    std::vector<int> results;
    int val;
    while (q.Dequeue(val))
        results.push_back(val);

    EXPECT_EQ(results.size(), 10u);
    // Evens first
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(results[i], i * 2);
    // Odds next
    for (int i = 0; i < 5; ++i)
        EXPECT_EQ(results[5 + i], i * 2 + 1);
}

TEST_F(MPSCQueueTest, FillDrainCycle)
{
    TestMPSCQueue<int, 8> q;

    // Fill
    for (int i = 0; i < 8; ++i)
        EXPECT_TRUE(q.Enqueue(i));
    EXPECT_FALSE(q.Enqueue(99)); // overflow

    // Drain
    for (int i = 0; i < 8; ++i)
    {
        int val = -1;
        EXPECT_TRUE(q.Dequeue(val));
        EXPECT_EQ(val, i);
    }
    EXPECT_TRUE(q.IsEmpty());

    // Refill after drain
    for (int i = 100; i < 108; ++i)
        EXPECT_TRUE(q.Enqueue(i));
    for (int i = 100; i < 108; ++i)
    {
        int val = -1;
        EXPECT_TRUE(q.Dequeue(val));
        EXPECT_EQ(val, i);
    }
    EXPECT_TRUE(q.IsEmpty());
}

TEST_F(MPSCQueueTest, EmptyDequeue)
{
    TestMPSCQueue<int, 16> q;
    int val = 0;
    EXPECT_FALSE(q.Dequeue(val));
}

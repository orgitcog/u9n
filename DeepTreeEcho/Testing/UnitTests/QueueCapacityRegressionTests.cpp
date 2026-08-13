// QueueCapacityRegressionTests.cpp
// Regression tests for queue capacity fix (issue #609)
// Verifies that high-throughput messaging does not drop messages
// Copyright (c) 2025 Deep Tree Echo Project

#include "Misc/AutomationTest.h"
#include "DeepTreeEcho/Core/Messages/LockFreeMessageQueue.h"

#if WITH_DEV_AUTOMATION_TESTS

// ============================================================================
// REGRESSION: Queue capacity must not silently drop messages (#609)
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueueCapacityRegressionBasicTest,
    "DeepTreeEcho.Regression.QueueCapacity.BasicFillDrain",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FQueueCapacityRegressionBasicTest::RunTest(const FString& Parameters)
{
    // A queue of capacity N should accept exactly N-1 items (one slot reserved)
    // and return false -- not silently drop -- on overflow.
    constexpr int32 Capacity = 64;
    TLockFreeSPSCQueue<int32, Capacity> Queue;

    int32 Accepted = 0;
    for (int32 i = 0; i < Capacity; ++i)
    {
        if (Queue.Enqueue(i))
        {
            ++Accepted;
        }
    }

    TestEqual(TEXT("Accepted count should be Capacity - 1"), Accepted, Capacity - 1);
    TestTrue(TEXT("Queue should be full"), Queue.IsFull());

    // Drain and verify every accepted item is present in order
    for (int32 i = 0; i < Accepted; ++i)
    {
        int32 Value = -1;
        TestTrue(FString::Printf(TEXT("Dequeue item %d"), i), Queue.Dequeue(Value));
        TestEqual(FString::Printf(TEXT("Item %d value"), i), Value, i);
    }

    TestTrue(TEXT("Queue should be empty after full drain"), Queue.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueueCapacityRegressionOverflowTest,
    "DeepTreeEcho.Regression.QueueCapacity.OverflowReportsFailure",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FQueueCapacityRegressionOverflowTest::RunTest(const FString& Parameters)
{
    constexpr int32 Capacity = 16;
    TLockFreeSPSCQueue<int32, Capacity> Queue;

    // Fill to capacity
    for (int32 i = 0; i < Capacity - 1; ++i)
    {
        Queue.Enqueue(i);
    }

    // Overflow attempts must return false, not silently succeed
    for (int32 i = 0; i < 10; ++i)
    {
        TestFalse(
            FString::Printf(TEXT("Overflow enqueue %d must return false"), i),
            Queue.Enqueue(999 + i));
    }

    // Queue contents must be intact despite overflow attempts
    for (int32 i = 0; i < Capacity - 1; ++i)
    {
        int32 Value = -1;
        TestTrue(FString::Printf(TEXT("Dequeue after overflow %d"), i), Queue.Dequeue(Value));
        TestEqual(FString::Printf(TEXT("Value intact after overflow %d"), i), Value, i);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQueueCapacityRegressionBurstTest,
    "DeepTreeEcho.Regression.QueueCapacity.BurstProduceDrain",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FQueueCapacityRegressionBurstTest::RunTest(const FString& Parameters)
{
    // Simulate the high-throughput burst pattern from #609:
    // produce a burst, drain partially, produce another burst, drain all.
    constexpr int32 Capacity = 32;
    TLockFreeSPSCQueue<int32, Capacity> Queue;

    int32 TotalProduced = 0;
    int32 TotalConsumed = 0;

    // First burst: fill halfway
    constexpr int32 FirstBurst = (Capacity - 1) / 2;
    for (int32 i = 0; i < FirstBurst; ++i)
    {
        TestTrue(FString::Printf(TEXT("Burst1 enqueue %d"), i), Queue.Enqueue(TotalProduced++));
    }

    // Drain half of the first burst
    constexpr int32 FirstDrain = FirstBurst / 2;
    for (int32 i = 0; i < FirstDrain; ++i)
    {
        int32 Value = -1;
        TestTrue(FString::Printf(TEXT("Drain1 dequeue %d"), i), Queue.Dequeue(Value));
        TestEqual(FString::Printf(TEXT("Drain1 value %d"), i), Value, TotalConsumed++);
    }

    // Second burst: fill remaining capacity
    int32 SecondBurstAccepted = 0;
    for (int32 i = 0; i < Capacity; ++i)
    {
        if (Queue.Enqueue(TotalProduced))
        {
            ++TotalProduced;
            ++SecondBurstAccepted;
        }
        else
        {
            break; // queue full, stop producing
        }
    }

    TestTrue(TEXT("Second burst should have accepted some items"), SecondBurstAccepted > 0);

    // Final drain: consume everything remaining
    int32 FinalDrained = 0;
    int32 Value = -1;
    while (Queue.Dequeue(Value))
    {
        TestEqual(FString::Printf(TEXT("Final drain value %d"), FinalDrained), Value, TotalConsumed++);
        ++FinalDrained;
    }

    TestEqual(TEXT("Total consumed must equal total produced"),
        TotalConsumed, TotalProduced);
    TestTrue(TEXT("Queue must be empty at end"), Queue.IsEmpty());

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

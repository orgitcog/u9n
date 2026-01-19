// LockFreeMessageQueue.h
// Lock-free message queue for Deep Tree Echo Bidirectional Message Protocol
// Feature F1.1.3: High-performance zero-contention message passing
// Copyright (c) 2025 Deep Tree Echo Project

#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include <atomic>
#include <memory>

/**
 * Lock-free Single-Producer Single-Consumer (SPSC) queue
 * Optimized for zero contention between one writer and one reader
 * Uses memory ordering for synchronization without locks
 */
template<typename T, size_t Capacity = 1024>
class TLockFreeSPSCQueue
{
public:
    TLockFreeSPSCQueue()
        : Head(0)
        , Tail(0)
    {
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
        Buffer = new T[Capacity];
    }

    ~TLockFreeSPSCQueue()
    {
        delete[] Buffer;
    }

    /**
     * Try to enqueue an item (producer side)
     * @param Item The item to enqueue
     * @return true if enqueued successfully, false if queue is full
     */
    bool Enqueue(const T& Item)
    {
        const size_t CurrentTail = Tail.load(std::memory_order_relaxed);
        const size_t NextTail = Increment(CurrentTail);
        
        if (NextTail == Head.load(std::memory_order_acquire))
        {
            // Queue is full
            return false;
        }
        
        Buffer[CurrentTail] = Item;
        Tail.store(NextTail, std::memory_order_release);
        return true;
    }

    /**
     * Try to enqueue an item with move semantics (producer side)
     * @param Item The item to enqueue
     * @return true if enqueued successfully, false if queue is full
     */
    bool Enqueue(T&& Item)
    {
        const size_t CurrentTail = Tail.load(std::memory_order_relaxed);
        const size_t NextTail = Increment(CurrentTail);
        
        if (NextTail == Head.load(std::memory_order_acquire))
        {
            // Queue is full
            return false;
        }
        
        Buffer[CurrentTail] = std::move(Item);
        Tail.store(NextTail, std::memory_order_release);
        return true;
    }

    /**
     * Try to dequeue an item (consumer side)
     * @param OutItem Output parameter for the dequeued item
     * @return true if dequeued successfully, false if queue is empty
     */
    bool Dequeue(T& OutItem)
    {
        const size_t CurrentHead = Head.load(std::memory_order_relaxed);
        
        if (CurrentHead == Tail.load(std::memory_order_acquire))
        {
            // Queue is empty
            return false;
        }
        
        OutItem = std::move(Buffer[CurrentHead]);
        Head.store(Increment(CurrentHead), std::memory_order_release);
        return true;
    }

    /**
     * Get the current size of the queue
     * @return Approximate number of items in queue
     */
    size_t Size() const
    {
        const size_t CurrentTail = Tail.load(std::memory_order_acquire);
        const size_t CurrentHead = Head.load(std::memory_order_acquire);
        
        if (CurrentTail >= CurrentHead)
        {
            return CurrentTail - CurrentHead;
        }
        else
        {
            return Capacity - (CurrentHead - CurrentTail);
        }
    }

    /**
     * Check if queue is empty
     * @return true if empty
     */
    bool IsEmpty() const
    {
        return Head.load(std::memory_order_acquire) == Tail.load(std::memory_order_acquire);
    }

    /**
     * Check if queue is full
     * @return true if full
     */
    bool IsFull() const
    {
        const size_t CurrentTail = Tail.load(std::memory_order_acquire);
        const size_t NextTail = Increment(CurrentTail);
        return NextTail == Head.load(std::memory_order_acquire);
    }

    /**
     * Get the maximum capacity
     * @return Maximum capacity of the queue
     */
    size_t GetCapacity() const
    {
        return Capacity - 1; // One slot is always kept empty
    }

private:
    /**
     * Increment index with wrapping
     */
    size_t Increment(size_t Index) const
    {
        return (Index + 1) & (Capacity - 1);
    }

    // Cache-line aligned atomics to prevent false sharing
    alignas(64) std::atomic<size_t> Head;
    alignas(64) std::atomic<size_t> Tail;
    
    T* Buffer;
};

/**
 * Lock-free Multi-Producer Single-Consumer (MPSC) queue
 * Allows multiple writers and one reader using atomic operations
 */
template<typename T, size_t Capacity = 1024>
class TLockFreeMPSCQueue
{
public:
    TLockFreeMPSCQueue()
        : Head(0)
        , Tail(0)
    {
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
        Buffer = new T[Capacity];
        Sequence = new std::atomic<size_t>[Capacity];
        
        for (size_t i = 0; i < Capacity; ++i)
        {
            Sequence[i].store(i, std::memory_order_relaxed);
        }
    }

    ~TLockFreeMPSCQueue()
    {
        delete[] Buffer;
        delete[] Sequence;
    }

    /**
     * Try to enqueue an item (producer side, thread-safe)
     * @param Item The item to enqueue
     * @return true if enqueued successfully, false if queue is full
     */
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
                // Try to claim this slot
                if (Tail.compare_exchange_weak(Pos, Pos + 1, std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if (Diff < 0)
            {
                // Queue is full
                return false;
            }
        }
        
        Buffer[Pos & (Capacity - 1)] = Item;
        Sequence[Pos & (Capacity - 1)].store(Pos + 1, std::memory_order_release);
        return true;
    }

    /**
     * Try to enqueue an item with move semantics (producer side, thread-safe)
     * @param Item The item to enqueue
     * @return true if enqueued successfully, false if queue is full
     */
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
                // Try to claim this slot
                if (Tail.compare_exchange_weak(Pos, Pos + 1, std::memory_order_relaxed))
                {
                    break;
                }
            }
            else if (Diff < 0)
            {
                // Queue is full
                return false;
            }
        }
        
        Buffer[Pos & (Capacity - 1)] = std::move(Item);
        Sequence[Pos & (Capacity - 1)].store(Pos + 1, std::memory_order_release);
        return true;
    }

    /**
     * Try to dequeue an item (consumer side)
     * @param OutItem Output parameter for the dequeued item
     * @return true if dequeued successfully, false if queue is empty
     */
    bool Dequeue(T& OutItem)
    {
        size_t Pos = Head.load(std::memory_order_relaxed);
        
        while (true)
        {
            const size_t Seq = Sequence[Pos & (Capacity - 1)].load(std::memory_order_acquire);
            const intptr_t Diff = static_cast<intptr_t>(Seq) - static_cast<intptr_t>(Pos + 1);
            
            if (Diff == 0)
            {
                // Item is ready to be consumed
                OutItem = std::move(Buffer[Pos & (Capacity - 1)]);
                Sequence[Pos & (Capacity - 1)].store(Pos + Capacity, std::memory_order_release);
                Head.store(Pos + 1, std::memory_order_release);
                return true;
            }
            else if (Diff < 0)
            {
                // Queue is empty
                return false;
            }
            
            Pos = Head.load(std::memory_order_relaxed);
        }
    }

    /**
     * Get the approximate current size of the queue
     * @return Approximate number of items in queue
     */
    size_t Size() const
    {
        const size_t CurrentTail = Tail.load(std::memory_order_acquire);
        const size_t CurrentHead = Head.load(std::memory_order_acquire);
        
        if (CurrentTail >= CurrentHead)
        {
            return CurrentTail - CurrentHead;
        }
        else
        {
            return Capacity - (CurrentHead - CurrentTail);
        }
    }

    /**
     * Check if queue is empty
     * @return true if empty
     */
    bool IsEmpty() const
    {
        const size_t CurrentHead = Head.load(std::memory_order_acquire);
        const size_t Seq = Sequence[CurrentHead & (Capacity - 1)].load(std::memory_order_acquire);
        return static_cast<intptr_t>(Seq) - static_cast<intptr_t>(CurrentHead + 1) < 0;
    }

    /**
     * Get the maximum capacity
     * @return Maximum capacity of the queue
     */
    size_t GetCapacity() const
    {
        return Capacity;
    }

private:
    // Cache-line aligned atomics to prevent false sharing
    alignas(64) std::atomic<size_t> Head;
    alignas(64) std::atomic<size_t> Tail;
    
    T* Buffer;
    std::atomic<size_t>* Sequence;
};

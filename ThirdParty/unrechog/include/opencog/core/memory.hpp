#pragma once
/**
 * @file memory.hpp
 * @brief Memory management utilities for Modern OpenCog
 *
 * Provides:
 * - Pool allocators for atoms
 * - SIMD-aligned allocation
 * - Lock-free memory pools
 * - Arena allocators for temporary operations
 */

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <memory_resource>
#include <new>
#include <span>
#include <vector>

#ifdef OPENCOG_USE_MIMALLOC
#include <mimalloc.h>
#endif

namespace opencog {

// ============================================================================
// Alignment Constants
// ============================================================================

inline constexpr size_t CACHE_LINE_SIZE = 64;
inline constexpr size_t SIMD_ALIGNMENT = 32;  // AVX2

// ============================================================================
// Aligned Allocation
// ============================================================================

/**
 * @brief Allocate aligned memory
 */
[[nodiscard]] inline void* aligned_alloc(size_t alignment, size_t size) {
#ifdef OPENCOG_USE_MIMALLOC
    return mi_aligned_alloc(alignment, size);
#elif defined(_MSC_VER)
    return _aligned_malloc(size, alignment);
#else
    return std::aligned_alloc(alignment, size);
#endif
}

/**
 * @brief Free aligned memory
 */
inline void aligned_free(void* ptr) {
#ifdef OPENCOG_USE_MIMALLOC
    mi_free(ptr);
#elif defined(_MSC_VER)
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}

/**
 * @brief RAII wrapper for aligned memory
 */
template<typename T>
struct AlignedDeleter {
    void operator()(T* ptr) const noexcept {
        if (ptr) {
            ptr->~T();
            aligned_free(ptr);
        }
    }
};

template<typename T>
using AlignedPtr = std::unique_ptr<T, AlignedDeleter<T>>;

template<typename T, typename... Args>
[[nodiscard]] AlignedPtr<T> make_aligned(size_t alignment, Args&&... args) {
    void* mem = aligned_alloc(alignment, sizeof(T));
    if (!mem) throw std::bad_alloc{};
    try {
        return AlignedPtr<T>{new(mem) T(std::forward<Args>(args)...)};
    } catch (...) {
        aligned_free(mem);
        throw;
    }
}

// ============================================================================
// Slot-based Pool Allocator
// ============================================================================

/**
 * @brief Lock-free slot pool for fixed-size allocations
 *
 * Uses a free list with generation counters to prevent ABA problems.
 * Ideal for atom storage where items are frequently created/destroyed.
 */
template<typename T, size_t BlockSize = 4096>
class SlotPool {
    static_assert(sizeof(T) >= sizeof(uint32_t), "Type must be at least 4 bytes");

    struct Block {
        alignas(CACHE_LINE_SIZE) std::array<std::byte, sizeof(T) * BlockSize> storage;
        std::atomic<Block*> next{nullptr};
    };

    struct FreeSlot {
        uint32_t next_free;  // Index of next free slot
    };

    std::atomic<Block*> blocks_{nullptr};
    std::atomic<uint64_t> free_head_{make_free_head(0, 0)};  // packed: [generation:32][index:32]
    std::atomic<uint32_t> capacity_{0};
    std::atomic<uint32_t> size_{0};

    static constexpr uint64_t make_free_head(uint32_t gen, uint32_t idx) noexcept {
        return (static_cast<uint64_t>(gen) << 32) | idx;
    }

    static constexpr uint32_t head_gen(uint64_t h) noexcept {
        return static_cast<uint32_t>(h >> 32);
    }

    static constexpr uint32_t head_idx(uint64_t h) noexcept {
        return static_cast<uint32_t>(h);
    }

public:
    SlotPool() { grow(); }

    ~SlotPool() {
        Block* b = blocks_.load(std::memory_order_relaxed);
        while (b) {
            Block* next = b->next.load(std::memory_order_relaxed);
            delete b;
            b = next;
        }
    }

    SlotPool(const SlotPool&) = delete;
    SlotPool& operator=(const SlotPool&) = delete;

    /**
     * @brief Allocate a slot and construct T in-place
     * @return Pointer to constructed object, or nullptr if allocation fails
     */
    template<typename... Args>
    [[nodiscard]] T* allocate(Args&&... args) {
        uint64_t head = free_head_.load(std::memory_order_acquire);

        while (true) {
            uint32_t idx = head_idx(head);
            uint32_t gen = head_gen(head);

            if (idx == 0xFFFFFFFF) {
                // No free slots, grow and retry
                grow();
                head = free_head_.load(std::memory_order_acquire);
                continue;
            }

            T* slot = get_slot(idx);
            auto* fs = reinterpret_cast<FreeSlot*>(slot);
            uint32_t next_idx = fs->next_free;

            uint64_t new_head = make_free_head(gen + 1, next_idx);
            if (free_head_.compare_exchange_weak(head, new_head,
                    std::memory_order_release, std::memory_order_acquire)) {
                size_.fetch_add(1, std::memory_order_relaxed);
                return new(slot) T(std::forward<Args>(args)...);
            }
            // CAS failed, head was updated, retry
        }
    }

    /**
     * @brief Deallocate a slot
     */
    void deallocate(T* ptr) noexcept {
        if (!ptr) return;

        ptr->~T();
        uint32_t idx = get_index(ptr);

        uint64_t head = free_head_.load(std::memory_order_acquire);
        while (true) {
            auto* fs = reinterpret_cast<FreeSlot*>(ptr);
            fs->next_free = head_idx(head);

            uint64_t new_head = make_free_head(head_gen(head) + 1, idx);
            if (free_head_.compare_exchange_weak(head, new_head,
                    std::memory_order_release, std::memory_order_acquire)) {
                size_.fetch_sub(1, std::memory_order_relaxed);
                return;
            }
        }
    }

    [[nodiscard]] size_t size() const noexcept {
        return size_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] size_t capacity() const noexcept {
        return capacity_.load(std::memory_order_relaxed);
    }

private:
    void grow() {
        auto* new_block = new Block{};

        // Link new block
        Block* old_head = blocks_.load(std::memory_order_relaxed);
        do {
            new_block->next.store(old_head, std::memory_order_relaxed);
        } while (!blocks_.compare_exchange_weak(old_head, new_block,
                std::memory_order_release, std::memory_order_relaxed));

        // Initialize free list for new block
        uint32_t base = capacity_.fetch_add(BlockSize, std::memory_order_relaxed);
        for (size_t i = 0; i < BlockSize - 1; ++i) {
            auto* slot = reinterpret_cast<FreeSlot*>(&new_block->storage[i * sizeof(T)]);
            slot->next_free = base + i + 1;
        }
        auto* last = reinterpret_cast<FreeSlot*>(&new_block->storage[(BlockSize - 1) * sizeof(T)]);

        // Link to existing free list
        uint64_t head = free_head_.load(std::memory_order_acquire);
        while (true) {
            last->next_free = head_idx(head);
            uint64_t new_head = make_free_head(head_gen(head) + 1, base);
            if (free_head_.compare_exchange_weak(head, new_head,
                    std::memory_order_release, std::memory_order_acquire)) {
                break;
            }
        }
    }

    [[nodiscard]] T* get_slot(uint32_t index) noexcept {
        uint32_t block_idx = index / BlockSize;
        uint32_t slot_idx = index % BlockSize;

        Block* b = blocks_.load(std::memory_order_acquire);
        for (uint32_t i = 0; i < block_idx && b; ++i) {
            b = b->next.load(std::memory_order_relaxed);
        }

        if (!b) return nullptr;
        return reinterpret_cast<T*>(&b->storage[slot_idx * sizeof(T)]);
    }

    [[nodiscard]] uint32_t get_index(T* ptr) const noexcept {
        Block* b = blocks_.load(std::memory_order_acquire);
        uint32_t block_num = 0;

        while (b) {
            auto* start = reinterpret_cast<T*>(&b->storage[0]);
            auto* end = reinterpret_cast<T*>(&b->storage[BlockSize * sizeof(T)]);

            if (ptr >= start && ptr < end) {
                return block_num * BlockSize + static_cast<uint32_t>(ptr - start);
            }

            b = b->next.load(std::memory_order_relaxed);
            ++block_num;
        }

        return 0xFFFFFFFF;  // Invalid
    }
};

// ============================================================================
// Arena Allocator
// ============================================================================

/**
 * @brief Fast arena allocator for temporary allocations
 *
 * All memory is freed at once when the arena is cleared or destroyed.
 * Perfect for pattern matching temporary results.
 */
class Arena {
    struct Block {
        std::unique_ptr<std::byte[]> memory;
        size_t size;
        size_t used;
        std::unique_ptr<Block> next;

        explicit Block(size_t sz)
            : memory(new std::byte[sz]), size(sz), used(0) {}
    };

    std::unique_ptr<Block> current_;
    size_t default_block_size_;

public:
    explicit Arena(size_t block_size = 64 * 1024)
        : default_block_size_(block_size) {
        current_ = std::make_unique<Block>(block_size);
    }

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    Arena(Arena&&) noexcept = default;
    Arena& operator=(Arena&&) noexcept = default;

    /**
     * @brief Allocate memory from the arena
     */
    [[nodiscard]] void* allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
        // Align current position
        size_t aligned_used = (current_->used + alignment - 1) & ~(alignment - 1);

        if (aligned_used + size > current_->size) {
            // Need new block
            size_t new_size = std::max(default_block_size_, size + alignment);
            auto new_block = std::make_unique<Block>(new_size);
            new_block->next = std::move(current_);
            current_ = std::move(new_block);
            aligned_used = 0;
        }

        void* ptr = current_->memory.get() + aligned_used;
        current_->used = aligned_used + size;
        return ptr;
    }

    /**
     * @brief Allocate and construct an object
     */
    template<typename T, typename... Args>
    [[nodiscard]] T* create(Args&&... args) {
        void* mem = allocate(sizeof(T), alignof(T));
        return new(mem) T(std::forward<Args>(args)...);
    }

    /**
     * @brief Allocate an array
     */
    template<typename T>
    [[nodiscard]] std::span<T> allocate_array(size_t count) {
        void* mem = allocate(sizeof(T) * count, alignof(T));
        return std::span<T>{static_cast<T*>(mem), count};
    }

    /**
     * @brief Reset the arena, reusing memory
     */
    void clear() noexcept {
        // Keep only the first block
        while (current_->next) {
            current_ = std::move(current_->next);
        }
        current_->used = 0;
    }

    [[nodiscard]] size_t bytes_used() const noexcept {
        size_t total = 0;
        for (Block* b = current_.get(); b; b = b->next.get()) {
            total += b->used;
        }
        return total;
    }
};

// ============================================================================
// SIMD-aligned vector
// ============================================================================

/**
 * @brief Vector with SIMD-aligned storage
 */
template<typename T>
class alignas(SIMD_ALIGNMENT) SimdVector {
    T* data_{nullptr};
    size_t size_{0};
    size_t capacity_{0};

public:
    SimdVector() = default;

    explicit SimdVector(size_t n) {
        reserve(n);
        size_ = n;
        for (size_t i = 0; i < n; ++i) {
            new(&data_[i]) T{};
        }
    }

    ~SimdVector() {
        clear();
        if (data_) aligned_free(data_);
    }

    SimdVector(const SimdVector& other) {
        reserve(other.size_);
        for (size_t i = 0; i < other.size_; ++i) {
            new(&data_[i]) T(other.data_[i]);
        }
        size_ = other.size_;
    }

    SimdVector(SimdVector&& other) noexcept
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    SimdVector& operator=(SimdVector other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
        return *this;
    }

    void reserve(size_t n) {
        if (n <= capacity_) return;

        // Round up to SIMD alignment
        size_t alloc_size = ((n * sizeof(T) + SIMD_ALIGNMENT - 1) / SIMD_ALIGNMENT) * SIMD_ALIGNMENT;
        T* new_data = static_cast<T*>(aligned_alloc(SIMD_ALIGNMENT, alloc_size));

        if (data_) {
            for (size_t i = 0; i < size_; ++i) {
                new(&new_data[i]) T(std::move(data_[i]));
                data_[i].~T();
            }
            aligned_free(data_);
        }

        data_ = new_data;
        capacity_ = alloc_size / sizeof(T);
    }

    void push_back(const T& value) {
        if (size_ >= capacity_) {
            reserve(capacity_ ? capacity_ * 2 : 16);
        }
        new(&data_[size_++]) T(value);
    }

    void push_back(T&& value) {
        if (size_ >= capacity_) {
            reserve(capacity_ ? capacity_ * 2 : 16);
        }
        new(&data_[size_++]) T(std::move(value));
    }

    void clear() noexcept {
        for (size_t i = 0; i < size_; ++i) {
            data_[i].~T();
        }
        size_ = 0;
    }

    [[nodiscard]] T& operator[](size_t i) noexcept { return data_[i]; }
    [[nodiscard]] const T& operator[](size_t i) const noexcept { return data_[i]; }

    [[nodiscard]] T* data() noexcept { return data_; }
    [[nodiscard]] const T* data() const noexcept { return data_; }

    [[nodiscard]] size_t size() const noexcept { return size_; }
    [[nodiscard]] size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    [[nodiscard]] T* begin() noexcept { return data_; }
    [[nodiscard]] T* end() noexcept { return data_ + size_; }
    [[nodiscard]] const T* begin() const noexcept { return data_; }
    [[nodiscard]] const T* end() const noexcept { return data_ + size_; }
};

} // namespace opencog

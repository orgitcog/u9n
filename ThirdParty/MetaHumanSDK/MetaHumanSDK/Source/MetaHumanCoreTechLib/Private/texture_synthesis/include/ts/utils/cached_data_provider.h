// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <map>
#include <vector>
#include <memory>
#include <atomic>
#include <carbon/common/Defs.h>
#include <ts/model_data_provider_interface.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::ts)

/* 
* Bespoke data container and memory manager for model data used by TS
* This class manages memory allocated for matrices etc and each instance provides information similar to NPY arrays: rows, cols, word_size
*
* Memory managed by this class is either cached or uncached; 
* * Uncached model data is intended for data used by TS regardless of the specific character map being synthesised, i.e. PCA and mask data
* * Cached model data is intended for use by the large HF arrays specific to a character map being synthesised. This data is keyed (and can therefore be re-used without reloading) and can also 
*   be purged to reduce memory usage with the Model_Data::trim_cache function
* 
* See main_texturesynthesis.cpp and texture_model.cpp for examples of how this is used
*/
struct CachedModelData : public Model_Data
{
    using key_t = uint64_t;
    static constexpr key_t nullkey = 0UL;
    static const CachedModelData InvalidCachedData;

    CachedModelData()
        : Model_Data()
        , _allocation_size(0)
        , _loaded(nullptr)
    {}

    CachedModelData(const CachedModelData& rhs) = default;

    // Write access 
    template <typename T>
    constexpr T* data()
    {
        return reinterpret_cast<T*>(_data);
    }

    static const CachedModelData& get_cached(key_t key_base);

    // allocate an uncached Model_Data instance
    static const CachedModelData& allocate(int32_t cols, int32_t rows, int32_t word_size, int32_t channels);

    /// <summary>
    /// allocate a cached Model_Data instance identified by the given key
    /// </summary>
    /// <param name="key_base">unique hash key</param>
    /// <param name="cols"></param>
    /// <param name="rows"></param>
    /// <param name="word_size"></param>
    /// <param name="channels"></param>
    /// <returns>a valid Model_Data object, or an empty one (test with bool operator)</returns>
    ///
    /// NOTE: if the keyed entry already exists in the cache it is returned but if the shape of the stored data mismatches the return is an empty Model_Data instance
    /// TODO: this could be an assert, but we don't want to bomb out on it in here, or perhaps include carbon for CARBON_ASSERT?
    static const CachedModelData& allocate_cached(key_t key_base, int32_t cols, int32_t rows, int32_t word_size, int32_t channels);

    /// <summary>
    /// free all allocated memory
    /// </summary>
    /// after this call any Model_Data instance is invalid and access to its data will crash
    static void free_all();

    /// <summary>
    /// clears out cached items, if available, to drive the size of cached allocations down to or below the given ceiling
    /// </summary>
    /// NOTE: Only call this when no threads are using data, i.e. this can be done in serialized portions of the caller code ONLY
    /// <param name="max_cached_allocation_size"></param>
    static void trim_cache(size_t max_cached_allocation_size);

    void make_available();

    bool is_available() const;

    constexpr size_t allocation_size() const
    {
        return _allocation_size;
    }

private:
    size_t _allocation_size;
    using shared_loaded_flag = std::shared_ptr<std::atomic<bool>>;
    shared_loaded_flag _loaded;

    using cached_allocations_t = std::map<CachedModelData::key_t, CachedModelData>;
    using allocations_t = std::vector<CachedModelData>;

    static allocations_t _allocated;
    static cached_allocations_t _allocated_cached;
    static size_t _cached_allocation_size;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::ts)

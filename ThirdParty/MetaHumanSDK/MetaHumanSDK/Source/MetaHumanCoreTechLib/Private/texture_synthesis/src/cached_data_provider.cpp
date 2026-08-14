// Copyright Epic Games, Inc. All Rights Reserved.

#include <ts/utils/cached_data_provider.h>

#include <carbon/Common.h>
#include <algorithm>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::ts)

CachedModelData::allocations_t CachedModelData::_allocated;
CachedModelData::cached_allocations_t CachedModelData::_allocated_cached;
size_t CachedModelData::_cached_allocation_size = 0;
const CachedModelData CachedModelData::InvalidCachedData {};

const CachedModelData& CachedModelData::get_cached(key_t key_base)
{
    if (_allocated_cached.find(key_base) != _allocated_cached.end())
    {
        return _allocated_cached.at(key_base);
    }
    return InvalidCachedData;
}

const CachedModelData& CachedModelData::allocate(int32_t cols, int32_t rows, int32_t word_size, int32_t channels)
{
    const size_t allocation_size = size_t(cols) * size_t(rows) * size_t(word_size) * size_t(channels);
    CachedModelData result;
    {
        result._cols = cols;
        result._rows = rows;
        result._word_size = word_size;
        result._channels = channels;
        result._allocation_size = allocation_size;
        result._data = new uint8_t[allocation_size];
    }
    _allocated.push_back(result);
    return _allocated.back();
}

const CachedModelData& CachedModelData::allocate_cached(key_t key_base, int32_t cols, int32_t rows, int32_t word_size, int32_t channels)
{

    if (_allocated_cached.find(key_base) != _allocated_cached.end())
    {
        return _allocated_cached.at(key_base);
    }

    const size_t allocation_size = size_t(cols) * size_t(rows) * size_t(word_size) * size_t(channels);
    CachedModelData result;
    {
        result._cols = cols;
        result._rows = rows;
        result._word_size = word_size;
        result._channels = channels;
        result._allocation_size = allocation_size;
        result._data = new uint8_t[allocation_size];
    }
    _cached_allocation_size += allocation_size;
    return _allocated_cached.emplace(key_base, result).first->second;
}

void CachedModelData::free_all()
{
    for (auto& entry : _allocated)
    {
        delete[] entry._data;
    }
    _allocated.clear();
    for (auto& entry : _allocated_cached)
    {
        delete[] entry.second._data;
    }
    _allocated_cached.clear();
}

void CachedModelData::trim_cache(size_t max_cached_allocation_size)
{
    if (_cached_allocation_size > max_cached_allocation_size)
    {

        if (_allocated_cached.size() == 1)
        {
            _cached_allocation_size -= _allocated_cached.begin()->second.allocation_size();
            delete[] _allocated_cached.begin()->second._data;
            _allocated_cached.clear();
            return;
        }

        struct purge_candidate
        {
            key_t key;
            size_t size;
        };
        std::vector<purge_candidate> purge_candidates;
        for (const auto& entry : _allocated_cached)
        {
            purge_candidates.push_back({ entry.first, entry.second.allocation_size() });
        }
        std::sort(purge_candidates.begin(), purge_candidates.end(), [](const purge_candidate& lhs, const purge_candidate& rhs)
            { return lhs.size > rhs.size; });

        for (const auto& to_purge : purge_candidates)
        {
            _cached_allocation_size -= to_purge.size;
            _allocated_cached[to_purge.key]._loaded->store(false);
            delete[] _allocated_cached[to_purge.key]._data;
            _allocated_cached.erase(to_purge.key);
            if (_cached_allocation_size < max_cached_allocation_size)
            {
                break;
            }
        }
    }
}

void CachedModelData::make_available()
{
    if (_loaded)
    {
        _loaded->store(true);
    }
}

bool CachedModelData::is_available() const
{
    if (_loaded)
    {
        return _loaded->load();
    }
    return true;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::ts)

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>

#include <algorithm>
#include <set>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

//! Convenience function finding an item in a vector. @return index of item if found, -1 otherwise.
template <class T>
int GetItemIndex(const std::vector<T>& vec, const T& item)
{
    auto it = std::find(vec.begin(), vec.end(), item);
    if (it != vec.end()) {
        return (int)std::distance(vec.begin(), it);
    }
    return -1;
}

/**
 * Concatenates two vectors with undown direction along one matching end point (removing the duplicate).
 * Fails if there is no matching end point or if either vector is empty.
 */
template <class T>
bool ConcatenateVectorsWithMatchingEndPointsAndUnknownDirection(const std::vector<T>& vector1, const std::vector<T>& vector2, std::vector<T>& mergedVector)
{
    if (vector1.empty() || vector2.empty())
    {
        // no matching of vectors if any is empty
        return false;
    }

    std::vector<T> newMergedVector;
    newMergedVector.reserve(vector1.size() + vector2.size() - 1);
    bool success = false;

    if (vector2.front() == vector1.front())
    {
        newMergedVector.insert(newMergedVector.begin(), vector1.rbegin(), vector1.rend());
        newMergedVector.insert(newMergedVector.end(), vector2.begin() + 1, vector2.end());
        success = true;
    }
    else if (vector2.front() == vector1.back())
    {
        newMergedVector.insert(newMergedVector.begin(), vector1.begin(), vector1.end());
        newMergedVector.insert(newMergedVector.end(), vector2.begin() + 1, vector2.end());
        success = true;
    }
    else if (vector2.back() == vector1.front())
    {
        newMergedVector.insert(newMergedVector.begin(), vector2.begin(), vector2.end());
        newMergedVector.insert(newMergedVector.end(), vector1.begin() + 1, vector1.end());
        success = true;
    }
    else if (vector2.back() == vector1.back())
    {
        newMergedVector.insert(newMergedVector.begin(), vector2.begin(), vector2.end());
        newMergedVector.insert(newMergedVector.end(), vector1.rbegin() + 1, vector1.rend());
        success = true;
    }

    if (success)
    {
        mergedVector.swap(newMergedVector);
    }
    return success;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

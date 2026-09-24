// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/joints/cpu/utils/LODRegion.h"

namespace rl4 {

class JointBehaviorFilter;

struct JointGroupOptimizer {
    static void defragment(const JointBehaviorFilter& source,
                           std::uint16_t jointGroupIndex,
                           Vector<float>& values,
                           Vector<std::uint16_t>& inputIndices,
                           Vector<std::uint16_t>& outputIndices,
                           ArrayView<LODRegion> lodRegions,
                           float translationPruningThreshold,
                           float rotationPruningThreshold,
                           float scalePruningThreshold);

};

}  // namespace rl4

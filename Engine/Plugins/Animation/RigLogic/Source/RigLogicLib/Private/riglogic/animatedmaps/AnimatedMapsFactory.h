// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/animatedmaps/AnimatedMaps.h"

namespace rl4 {

struct Configuration;
struct RigMetrics;
class Controls;

struct AnimatedMapsFactory {
    static AnimatedMaps::Pointer create(const Configuration& config,
                                        const dna::BehaviorReader* reader,
                                        Controls* controls,
                                        MemoryResource* memRes);
    static AnimatedMaps::Pointer create(const Configuration& config, const RigMetrics& metrics, MemoryResource* memRes);
};

}  // namespace rl4

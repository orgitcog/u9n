// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/joints/Joints.h"

namespace rl4 {

struct Configuration;
struct RigMetrics;
class Controls;

struct JointsFactory {
    static Joints::Pointer create(const Configuration& config,
                                  const dna::Reader* reader,
                                  Controls* controls,
                                  MemoryResource* memRes);
    static Joints::Pointer create(const Configuration& config, const RigMetrics& metrics, MemoryResource* memRes);

};

}  // namespace rl4

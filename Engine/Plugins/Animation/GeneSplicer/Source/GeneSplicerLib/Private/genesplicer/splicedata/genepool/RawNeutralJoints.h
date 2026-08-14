// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "genesplicer/TypeDefs.h"
#include "genesplicer/dna/Aliases.h"

#include <functional>

namespace gs4 {

struct RawNeutralJoints {
    RawNeutralJoints(MemoryResource* memRes) :
        translations{memRes},
        rotations{memRes} {
    }

    RawNeutralJoints(const Reader* dna, MemoryResource* memRes) :
        translations{dna->getNeutralJointTranslationXs(),
                          dna->getNeutralJointTranslationYs(),
                          dna->getNeutralJointTranslationZs(),
                          memRes},
        rotations{dna->getNeutralJointRotationXs(),
                  dna->getNeutralJointRotationYs(),
                  dna->getNeutralJointRotationZs(),
                  memRes} {

        auto toRadians = [](float& r) {
                r = tdm::radians(r);
            };
        std::for_each(rotations.xs.begin(), rotations.xs.end(), toRadians);
        std::for_each(rotations.ys.begin(), rotations.ys.end(), toRadians);
        std::for_each(rotations.zs.begin(), rotations.zs.end(), toRadians);
    }

    template<class Archive>
    void serialize(Archive& archive) {
        archive(translations, rotations);
    }

    RawVector3Vector translations;
    RawVector3Vector rotations;
};

void toWorldSpace(std::function<std::uint16_t(std::uint16_t)> getJointParentIndex, RawNeutralJoints& neutralJoints);
void toLocalSpace(std::function<std::uint16_t(std::uint16_t)> getJointParentIndex, RawNeutralJoints& neutralJoints);

}  // namespace gs4

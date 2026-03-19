// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "genesplicer/TypeDefs.h"
#include "genesplicer/neutraljointsplicer/JointAttribute.h"
#include "genesplicer/splicedata/genepool/RawNeutralJoints.h"
#include "genesplicer/types/BlockStorage.h"

namespace gs4 {

class NeutralJointPool {
    public:
        NeutralJointPool(MemoryResource* memRes);
        NeutralJointPool(const Reader* deltaArchetype, ConstArrayView<const Reader*> dnas, MemoryResource* memRes);
        template<JointAttribute JT>
        const XYZTiledMatrix<16u>& getDNAData() const;

        std::uint16_t getJointCount() const;

        Vector3 getDNANeutralJointWorldTranslation(std::uint16_t dnaIndex, std::uint16_t jointIndex) const;
        Vector3 getArchetypeNeutralJointWorldTranslation(std::uint16_t jointIndex) const;
        Vector3 getDNANeutralJointWorldRotation(std::uint16_t dnaIndex, std::uint16_t jointIndex) const;
        Vector3 getArchetypeNeutralJointWorldRotation(std::uint16_t jointIndex) const;

        template<class Archive>
        void serialize(Archive& archive) {
            archive(dnaTranslations, dnaRotations, archJoints);
        }

    private:
        XYZTiledMatrix<16u> dnaTranslations;
        XYZTiledMatrix<16u> dnaRotations;
        RawNeutralJoints archJoints;
};

template<>
const XYZTiledMatrix<16u>& NeutralJointPool::getDNAData<JointAttribute::Translation>() const;

template<>
const XYZTiledMatrix<16u>& NeutralJointPool::getDNAData<JointAttribute::Rotation>() const;

}  // namespace gs4

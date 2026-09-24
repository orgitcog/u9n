// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/controls/Controls.h"
#include "riglogic/joints/JointBehaviorFilter.h"
#include "riglogic/joints/JointsBuilder.h"
#include "riglogic/joints/cpu/quaternions/QuaternionJointsEvaluator.h"
#include "riglogic/joints/cpu/quaternions/RotationAdapters.h"
#include "riglogic/joints/cpu/utils/JointGroupOptimizer.h"
#include "riglogic/riglogic/Configuration.h"
#include "riglogic/types/bpcm/Optimizer.h"
#include "riglogic/utils/Extd.h"

#include <tdm/Quat.h>

namespace rl4 {

template<typename TValue, typename TFVec256, typename TFVec128>
class QuaternionJointsBuilder : public JointsBuilder {
    public:
        QuaternionJointsBuilder(const Configuration& config_, MemoryResource* memRes_);

        void computeStorageRequirements(const RigMetrics& source) override;
        void computeStorageRequirements(const JointBehaviorFilter& source) override;
        void allocateStorage(const JointBehaviorFilter& source) override;
        void fillStorage(const JointBehaviorFilter& source) override;
        void registerControls(Controls* controls) override;
        JointsEvaluator::Pointer build() override;

    private:
        void setInputIndices(JointGroup<TValue>& group, ConstArrayView<std::uint16_t> inputIndices);
        void setOutputIndices(JointGroup<TValue>& group, ConstArrayView<std::uint16_t> outputIndices);
        void setValues(JointGroup<TValue>& group,
                       ConstArrayView<float> eulers,
                       ConstArrayView<std::uint16_t> inputIndices,
                       ConstArrayView<std::uint16_t> outputIndices);
        void setLODs(JointGroup<TValue>& group, ConstArrayView<std::uint16_t> outputIndices);
        void remapOutputIndices(JointGroup<TValue>& group);

        static constexpr std::uint32_t BlockHeight() {
            return 32u;
        }

        static constexpr std::uint32_t PadTo() {
            return 16u;
        }

        static constexpr std::uint32_t Stride() {
            return 4u;
        }

    private:
        Configuration config;
        MemoryResource* memRes;
        Vector<JointGroup<TValue> > jointGroups;
        dna::RotationUnit rotationUnit;
};

template<typename TValue, typename TFVec256, typename TFVec128>
QuaternionJointsBuilder<TValue, TFVec256, TFVec128>::QuaternionJointsBuilder(const Configuration& config_,
                                                                             MemoryResource* memRes_) :
    config{config_},
    memRes{memRes_},
    jointGroups{memRes_},
    rotationUnit{} {
}

template<typename TValue, typename TFVec256, typename TFVec128>
void QuaternionJointsBuilder<TValue, TFVec256, TFVec128>::computeStorageRequirements(const RigMetrics&  /*unused*/) {
}

template<typename TValue, typename TFVec256, typename TFVec128>
void QuaternionJointsBuilder<TValue, TFVec256, TFVec128>::computeStorageRequirements(const JointBehaviorFilter&  /*unused*/) {
}

template<typename TValue, typename TFVec256, typename TFVec128>
void QuaternionJointsBuilder<TValue, TFVec256, TFVec128>::allocateStorage(const JointBehaviorFilter& source) {
    jointGroups.resize(source.getJointGroupCount(), JointGroup<TValue>{memRes});
    for (auto& group : jointGroups) {
        group.lods.resize(source.getLODCount());
    }
}

template<typename TValue, typename TFVec256, typename TFVec128>
void QuaternionJointsBuilder<TValue, TFVec256, TFVec128>::setInputIndices(JointGroup<TValue>& group,
                                                                          ConstArrayView<std::uint16_t> inputIndices) {
    group.inputIndices.assign(inputIndices.begin(), inputIndices.end());
}

template<typename TValue, typename TFVec256, typename TFVec128>
void QuaternionJointsBuilder<TValue, TFVec256, TFVec128>::setOutputIndices(JointGroup<TValue>& group,
                                                                           ConstArrayView<std::uint16_t> outputIndices) {
    // Given any rotation indices (rx, ry, rz), return only rx indices for all joints in the group
    #if !defined(__clang__) && defined(__GNUC__)
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wattributes"
    #endif
    auto deduplicate = [this](Vector<std::uint16_t>& v) {
            UnorderedSet<std::uint16_t> deduplicator{memRes};
            v.erase(v.rend().base(), std::remove_if(v.rbegin(), v.rend(), [&deduplicator](const std::uint16_t value) {
                return !deduplicator.insert(value).second;
            }).base());
        };
    #if !defined(__clang__) && defined(__GNUC__)
        #pragma GCC diagnostic pop
    #endif
    Vector<std::uint16_t> outputRotationBaseIndices{memRes};
    outputRotationBaseIndices.reserve(outputIndices.size() / 3ul);
    std::transform(outputIndices.begin(),
                   outputIndices.end(),
                   std::back_inserter(outputRotationBaseIndices),
                   [](std::uint16_t outputIndex) {
            return static_cast<std::uint16_t>((outputIndex / 9) * 9);
        });
    deduplicate(outputRotationBaseIndices);
    // Expand output rotation base indices (qx) into (qx, qy, qz, qw) for all rotations
    group.outputIndices.reserve(outputRotationBaseIndices.size() * static_cast<std::uint8_t>(RotationType::Quaternions));
    for (const auto baseIndex : outputRotationBaseIndices) {
        // Remap output indices from 9-attribute joints to 10-attribute joints rx -> qx
        const auto jointIndex = static_cast<std::uint16_t>(baseIndex / 9u);
        const auto remappedBaseIndex = static_cast<std::uint16_t>(jointIndex * 10u);
        group.outputIndices.push_back(static_cast<std::uint16_t>(remappedBaseIndex + 3));
        group.outputIndices.push_back(static_cast<std::uint16_t>(remappedBaseIndex + 4));
        group.outputIndices.push_back(static_cast<std::uint16_t>(remappedBaseIndex + 5));
        group.outputIndices.push_back(static_cast<std::uint16_t>(remappedBaseIndex + 6));
    }
}

template<typename TValue, typename TFVec256, typename TFVec128>
void QuaternionJointsBuilder<TValue, TFVec256, TFVec128>::setValues(JointGroup<TValue>& group,
                                                                    ConstArrayView<float> eulers,
                                                                    ConstArrayView<std::uint16_t> inputIndices,
                                                                    ConstArrayView<std::uint16_t> outputIndices) {
    // Convert euler angles to quaternions
    std::function<tdm::frad(float)> angConv;
    if (rotationUnit == dna::RotationUnit::degrees) {
        angConv = [](float angle) {
                return tdm::frad{tdm::fdeg{angle}};
            };
    } else {
        angConv = [](float angle) {
                return tdm::frad{angle};
            };
    }

    const auto colCount = static_cast<std::uint16_t>(inputIndices.size());
    const auto rowCount = static_cast<std::uint16_t>(outputIndices.size());

    Vector<float> quaternions(static_cast<std::size_t>(group.outputIndices.size() * colCount), {}, memRes);
    for (std::size_t col = {}; col < colCount; ++col) {
        for (std::size_t row = {}, quatIndex = {}; row < rowCount; ++quatIndex) {
            tdm::frad3 angles;
            const std::uint16_t jointIndex = static_cast<std::uint16_t>(outputIndices[row] / 9u);
            while ((row < rowCount) && (jointIndex == static_cast<std::uint16_t>(outputIndices[row] / 9u))) {
                const auto relAttrIndex = static_cast<std::uint16_t>(outputIndices[row] % 9u);
                // 0 = rx, 1 = ry, 2 = rz
                const auto relRotAttrIndex = static_cast<std::uint16_t>(relAttrIndex % 3u);
                angles[relRotAttrIndex] = angConv(eulers[row * colCount + col]);
                ++row;
            }
            tdm::fquat q{angles, static_cast<tdm::rot_seq>(config.rotationOrder)};
            quaternions[((quatIndex * 4) + 0ul) * colCount + col] = q.x;
            quaternions[((quatIndex * 4) + 1ul) * colCount + col] = q.y;
            quaternions[((quatIndex * 4) + 2ul) * colCount + col] = q.z;
            quaternions[((quatIndex * 4) + 3ul) * colCount + col] = q.w;
        }
    }

    // 8 quaternions x 4 floats per quat = 32
    const auto newRowCount = static_cast<std::uint32_t>(group.outputIndices.size());
    const auto paddedRowCount = extd::roundUp(newRowCount, PadTo());
    group.values.resize(paddedRowCount * colCount);
    group.colCount = colCount;
    group.rowCount = paddedRowCount;
    using BPCMOptimizer = bpcm::Optimizer<TFVec256, BlockHeight(), PadTo(), Stride()>;
    BPCMOptimizer::optimize(group.values.data(), quaternions.data(), {newRowCount, colCount});
}

template<typename TValue, typename TFVec256, typename TFVec128>
void QuaternionJointsBuilder<TValue, TFVec256, TFVec128>::setLODs(JointGroup<TValue>& group,
                                                                  ConstArrayView<std::uint16_t> outputIndices) {
    const auto maxRemappedRotationIndex = [](std::uint16_t absRotAttrIndex) {
            const auto jointIndex = static_cast<std::uint16_t>(absRotAttrIndex / 9u);
            const auto newAttrBase = static_cast<std::uint16_t>(jointIndex * 10u);
            // Only rotation indices are inputs, and since the goal is to find the maximum rotation index,
            // the last quaternion attribute index is used, which is 6 based on [tx, ty, tz, qx, qy, qz, qw, sx, sy, sz]
            return static_cast<std::uint16_t>(newAttrBase + 6);
        };

    const auto newRowCount = static_cast<std::uint32_t>(group.outputIndices.size());
    const auto paddedRowCount = extd::roundUp(newRowCount, PadTo());
    const auto lodCount = static_cast<std::uint16_t>(group.lods.size());
    for (std::uint16_t lod = {}; lod < lodCount; ++lod) {
        const std::uint32_t oldLODRowCount = group.lods[lod].outputLODs.size;
        std::uint32_t newLODRowCount = {};
        if (oldLODRowCount != 0) {
            const auto qwRotationIndexAtOldLODRowCount = maxRemappedRotationIndex(outputIndices[oldLODRowCount - 1ul]);
            auto it = std::find(group.outputIndices.begin(), group.outputIndices.end(), qwRotationIndexAtOldLODRowCount);
            assert(it != group.outputIndices.end());
            newLODRowCount = static_cast<std::uint16_t>(std::distance(group.outputIndices.begin(), it) + 1);
        }
        RowLOD outputLOD{newLODRowCount, paddedRowCount, BlockHeight(), PadTo()};
        group.lods[lod].outputLODs = outputLOD;
    }
}

template<typename TValue, typename TFVec256, typename TFVec128>
void QuaternionJointsBuilder<TValue, TFVec256, TFVec128>::remapOutputIndices(JointGroup<TValue>& group) {
    for (auto& outputIndex : group.outputIndices) {
        const auto jointIndex = static_cast<std::uint16_t>(outputIndex / 10u);
        const auto relAttrIndex = static_cast<std::uint16_t>(outputIndex % 10u);
        const auto newAttrBase = static_cast<std::uint16_t>(jointIndex * 9u);
        // Only rotations are among output indices (no translation or scale)
        // qx, qy, qz are kept, qw is ignored
        outputIndex = (relAttrIndex == 6) ? std::uint16_t{} : static_cast<std::uint16_t>(newAttrBase + relAttrIndex);
    }
}

template<typename TValue, typename TFVec256, typename TFVec128>
void QuaternionJointsBuilder<TValue, TFVec256, TFVec128>::fillStorage(const JointBehaviorFilter& source) {
    rotationUnit = source.getRotationUnit();

    for (std::uint16_t jgi = {}; jgi < static_cast<std::uint16_t>(jointGroups.size()); ++jgi) {
        auto rowCount = source.getRowCount(jgi);
        auto colCount = source.getColumnCount(jgi);
        if ((rowCount == 0u) || (colCount == 0u)) {
            continue;
        }

        JointGroup<TValue>& group = jointGroups[jgi];

        Vector<float> eulers(static_cast<std::size_t>(rowCount) * static_cast<std::size_t>(colCount), {}, memRes);
        source.copyValues(jgi, eulers);

        Vector<std::uint16_t> inputIndices{colCount, {}, memRes};
        source.copyInputIndices(jgi, inputIndices);

        Vector<std::uint16_t> outputIndices{rowCount, {}, memRes};
        source.copyOutputIndices(jgi, outputIndices);

        JointGroupOptimizer::defragment(source,
                                        jgi,
                                        eulers,
                                        inputIndices,
                                        outputIndices,
                                        group.lods,
                                        config.translationPruningThreshold,
                                        config.rotationPruningThreshold,
                                        config.scalePruningThreshold);

        setInputIndices(group, inputIndices);
        setOutputIndices(group, outputIndices);
        setValues(group, eulers, inputIndices, outputIndices);
        setLODs(group, outputIndices);

        // If the selected RigLogic output is in quaternions, then the output indices are already setup as needed.
        // But if Euler angles were requested, the output indices need to be mapped back to 9-attribute joint output indices
        if (config.rotationType == RotationType::EulerAngles) {
            remapOutputIndices(group);
        }
    }
}

template<typename TValue, typename TFVec256, typename TFVec128>
void QuaternionJointsBuilder<TValue, TFVec256, TFVec128>::registerControls(Controls* controls) {
    for (const auto& group : jointGroups) {
        for (std::uint16_t lod = {}; lod < static_cast<std::uint16_t>(group.lods.size()); ++lod) {
            ConstArrayView<std::uint16_t> inputIndicesForLOD(group.inputIndices.data(), group.lods[lod].inputLODs.size);
            controls->registerControls(lod, inputIndicesForLOD);
        }
    }
}

template<typename T, typename TFVec256, typename TFVec128>
typename UniqueInstance<JointGroupQuaternionCalculationStrategy<T> >::PointerType createJointGroupQuaternionStrategy(
    RotationType rotationType,
    RotationOrder rotationOrder,
    dna::RotationUnit rotationUnit,
    MemoryResource* memRes) {

    if (rotationType == RotationType::Quaternions) {
        using CalculationStrategy = VectorizedJointGroupQuaternionCalculationStrategy<T, TFVec256, TFVec128, PassthroughAdapter>;
        return UniqueInstance<CalculationStrategy, JointGroupQuaternionCalculationStrategy<T> >::with(memRes).create();
    }

    #ifdef RL_BUILD_WITH_XYZ_ROTATION_ORDER
        if (rotationOrder == RotationOrder::XYZ) {
            if (rotationUnit == dna::RotationUnit::degrees) {
                using Q2E = QuaternionsToEulerAngles<tdm::fdeg, tdm::rot_seq::xyz>;
                using CalculationStrategy = VectorizedJointGroupQuaternionCalculationStrategy<T, TFVec256, TFVec128, Q2E>;
                return UniqueInstance<CalculationStrategy, JointGroupQuaternionCalculationStrategy<T> >::with(memRes).create();
            } else {
                using Q2E = QuaternionsToEulerAngles<tdm::frad, tdm::rot_seq::xyz>;
                using CalculationStrategy = VectorizedJointGroupQuaternionCalculationStrategy<T, TFVec256, TFVec128, Q2E>;
                return UniqueInstance<CalculationStrategy, JointGroupQuaternionCalculationStrategy<T> >::with(memRes).create();
            }
        }
    #endif  // RL_BUILD_WITH_XYZ_ROTATION_ORDER

    #ifdef RL_BUILD_WITH_XZY_ROTATION_ORDER
        if (rotationOrder == RotationOrder::XZY) {
            if (rotationUnit == dna::RotationUnit::degrees) {
                using Q2E = QuaternionsToEulerAngles<tdm::fdeg, tdm::rot_seq::xzy>;
                using CalculationStrategy = VectorizedJointGroupQuaternionCalculationStrategy<T, TFVec256, TFVec128, Q2E>;
                return UniqueInstance<CalculationStrategy, JointGroupQuaternionCalculationStrategy<T> >::with(memRes).create();
            } else {
                using Q2E = QuaternionsToEulerAngles<tdm::frad, tdm::rot_seq::xzy>;
                using CalculationStrategy = VectorizedJointGroupQuaternionCalculationStrategy<T, TFVec256, TFVec128, Q2E>;
                return UniqueInstance<CalculationStrategy, JointGroupQuaternionCalculationStrategy<T> >::with(memRes).create();
            }
        }
    #endif  // RL_BUILD_WITH_XZY_ROTATION_ORDER

    #ifdef RL_BUILD_WITH_YXZ_ROTATION_ORDER
        if (rotationOrder == RotationOrder::YXZ) {
            if (rotationUnit == dna::RotationUnit::degrees) {
                using Q2E = QuaternionsToEulerAngles<tdm::fdeg, tdm::rot_seq::yxz>;
                using CalculationStrategy = VectorizedJointGroupQuaternionCalculationStrategy<T, TFVec256, TFVec128, Q2E>;
                return UniqueInstance<CalculationStrategy, JointGroupQuaternionCalculationStrategy<T> >::with(memRes).create();
            } else {
                using Q2E = QuaternionsToEulerAngles<tdm::frad, tdm::rot_seq::yxz>;
                using CalculationStrategy = VectorizedJointGroupQuaternionCalculationStrategy<T, TFVec256, TFVec128, Q2E>;
                return UniqueInstance<CalculationStrategy, JointGroupQuaternionCalculationStrategy<T> >::with(memRes).create();
            }
        }
    #endif  // RL_BUILD_WITH_YXZ_ROTATION_ORDER

    #ifdef RL_BUILD_WITH_YZX_ROTATION_ORDER
        if (rotationOrder == RotationOrder::YZX) {
            if (rotationUnit == dna::RotationUnit::degrees) {
                using Q2E = QuaternionsToEulerAngles<tdm::fdeg, tdm::rot_seq::yzx>;
                using CalculationStrategy = VectorizedJointGroupQuaternionCalculationStrategy<T, TFVec256, TFVec128, Q2E>;
                return UniqueInstance<CalculationStrategy, JointGroupQuaternionCalculationStrategy<T> >::with(memRes).create();
            } else {
                using Q2E = QuaternionsToEulerAngles<tdm::frad, tdm::rot_seq::yzx>;
                using CalculationStrategy = VectorizedJointGroupQuaternionCalculationStrategy<T, TFVec256, TFVec128, Q2E>;
                return UniqueInstance<CalculationStrategy, JointGroupQuaternionCalculationStrategy<T> >::with(memRes).create();
            }
        }
    #endif  // RL_BUILD_WITH_YZX_ROTATION_ORDER

    #ifdef RL_BUILD_WITH_ZXY_ROTATION_ORDER
        if (rotationOrder == RotationOrder::ZXY) {
            if (rotationUnit == dna::RotationUnit::degrees) {
                using Q2E = QuaternionsToEulerAngles<tdm::fdeg, tdm::rot_seq::zxy>;
                using CalculationStrategy = VectorizedJointGroupQuaternionCalculationStrategy<T, TFVec256, TFVec128, Q2E>;
                return UniqueInstance<CalculationStrategy, JointGroupQuaternionCalculationStrategy<T> >::with(memRes).create();
            } else {
                using Q2E = QuaternionsToEulerAngles<tdm::frad, tdm::rot_seq::zxy>;
                using CalculationStrategy = VectorizedJointGroupQuaternionCalculationStrategy<T, TFVec256, TFVec128, Q2E>;
                return UniqueInstance<CalculationStrategy, JointGroupQuaternionCalculationStrategy<T> >::with(memRes).create();
            }
        }
    #endif  // RL_BUILD_WITH_ZXY_ROTATION_ORDER

    #ifdef RL_BUILD_WITH_ZYX_ROTATION_ORDER
        if (rotationOrder == RotationOrder::ZYX) {
            if (rotationUnit == dna::RotationUnit::degrees) {
                using Q2E = QuaternionsToEulerAngles<tdm::fdeg, tdm::rot_seq::zyx>;
                using CalculationStrategy = VectorizedJointGroupQuaternionCalculationStrategy<T, TFVec256, TFVec128, Q2E>;
                return UniqueInstance<CalculationStrategy, JointGroupQuaternionCalculationStrategy<T> >::with(memRes).create();
            } else {
                using Q2E = QuaternionsToEulerAngles<tdm::frad, tdm::rot_seq::zyx>;
                using CalculationStrategy = VectorizedJointGroupQuaternionCalculationStrategy<T, TFVec256, TFVec128, Q2E>;
                return UniqueInstance<CalculationStrategy, JointGroupQuaternionCalculationStrategy<T> >::with(memRes).create();
            }
        }
    #endif  // RL_BUILD_WITH_ZYX_ROTATION_ORDER

    return nullptr;
}

template<typename TValue, typename TFVec256, typename TFVec128>
JointsEvaluator::Pointer QuaternionJointsBuilder<TValue, TFVec256, TFVec128>::build() {
    auto factory = UniqueInstance<QuaternionJointsEvaluator<TValue>, JointsEvaluator>::with(memRes);
    auto strategy = createJointGroupQuaternionStrategy<TValue, TFVec256, TFVec128>(config.rotationType,
                                                                                   config.rotationOrder,
                                                                                   rotationUnit,
                                                                                   memRes);
    return factory.create(std::move(strategy), std::move(jointGroups), nullptr, memRes);
}

}  // namespace rl4

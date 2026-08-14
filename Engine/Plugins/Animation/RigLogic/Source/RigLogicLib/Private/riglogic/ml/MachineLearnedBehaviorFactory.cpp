// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/ml/MachineLearnedBehaviorFactory.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/utils/Extd.h"
#include "riglogic/ml/MachineLearnedBehaviorEvaluator.h"
#include "riglogic/ml/MachineLearnedBehaviorNullEvaluator.h"
#include "riglogic/ml/cpu/CPUMachineLearnedBehaviorFactory.h"
#include "riglogic/riglogic/Configuration.h"
#include "riglogic/riglogic/RigMetrics.h"
#include "riglogic/system/simd/SIMD.h"

#include <cstdint>

namespace rl4 {

MachineLearnedBehaviorEvaluator::Pointer createMLEvaluator(const Configuration& config,
                                                           const dna::MachineLearnedBehaviorReader* reader,
                                                           MemoryResource* memRes) {
    auto features = trimd::getCPUFeatures();
    RL_UNUSED(features);
    RL_UNUSED(config);
    #ifdef RL_BUILD_WITH_SSE
        #ifdef RL_DISABLE_RUNTIME_FEATURE_DETECTION
            features.SSE2 = true;
        #endif  // RL_DISABLE_RUNTIME_FEATURE_DETECTION
        if (features.SSE2 &&
            ((config.calculationType == CalculationType::SSE) || (config.calculationType == CalculationType::AnyVector))) {
            #ifdef RL_BUILD_WITH_HALF_FLOATS
                #ifdef RL_DISABLE_RUNTIME_FEATURE_DETECTION
                    features.F16C = true;
                #endif  // RL_DISABLE_RUNTIME_FEATURE_DETECTION
                if (features.F16C) {
                    return ml::cpu::Factory<std::uint16_t, trimd::sse::F256, trimd::sse::F128>::create(reader, memRes);
                }
            #endif  // RL_BUILD_WITH_HALF_FLOATS
            return ml::cpu::Factory<float, trimd::sse::F256, trimd::sse::F128>::create(reader, memRes);
        }
    #endif  // RL_BUILD_WITH_SSE
    #ifdef RL_BUILD_WITH_AVX
        #ifdef RL_DISABLE_RUNTIME_FEATURE_DETECTION
            features.AVX = true;
        #endif  // RL_DISABLE_RUNTIME_FEATURE_DETECTION
        if (features.AVX &&
            ((config.calculationType == CalculationType::AVX) || (config.calculationType == CalculationType::AnyVector))) {
            // Use 256-bit AVX registers and whatever 128-bit width type is available
            #ifdef RL_BUILD_WITH_HALF_FLOATS
                #ifdef RL_DISABLE_RUNTIME_FEATURE_DETECTION
                    features.F16C = true;
                #endif  // RL_DISABLE_RUNTIME_FEATURE_DETECTION
                if (features.F16C) {
                    return ml::cpu::Factory<std::uint16_t, trimd::avx::F256, trimd::sse::F128>::create(reader, memRes);
                }
            #endif  // RL_BUILD_WITH_HALF_FLOATS
            return ml::cpu::Factory<float, trimd::avx::F256, trimd::sse::F128>::create(reader, memRes);
        }
    #endif  // RL_BUILD_WITH_AVX
    #ifdef RL_BUILD_WITH_NEON
        #ifdef RL_DISABLE_RUNTIME_FEATURE_DETECTION
            features.NEON = true;
        #endif  // RL_DISABLE_RUNTIME_FEATURE_DETECTION
        if (features.NEON &&
            ((config.calculationType == CalculationType::NEON) || (config.calculationType == CalculationType::AnyVector))) {
            #ifdef RL_BUILD_WITH_HALF_FLOATS
                #ifdef RL_DISABLE_RUNTIME_FEATURE_DETECTION
                    features.FP16 = true;
                #endif  // RL_DISABLE_RUNTIME_FEATURE_DETECTION
                if (features.FP16) {
                    return ml::cpu::Factory<std::uint16_t, trimd::neon::F256, trimd::neon::F128>::create(reader, memRes);
                }
            #endif  // RL_BUILD_WITH_HALF_FLOATS
            return ml::cpu::Factory<float, trimd::neon::F256, trimd::neon::F128>::create(reader, memRes);
        }
    #endif  // RL_BUILD_WITH_NEON
    return ml::cpu::Factory<float, trimd::scalar::F256, trimd::scalar::F128>::create(reader, memRes);
}

MachineLearnedBehavior::Pointer MachineLearnedBehaviorFactory::create(const Configuration& config,
                                                                      const dna::MachineLearnedBehaviorReader* reader,
                                                                      MemoryResource* memRes) {
    auto moduleFactory = UniqueInstance<MachineLearnedBehavior>::with(memRes);
    if (!config.loadMachineLearnedBehavior || (reader->getNeuralNetworkCount() == 0u)) {
        auto evaluator =
            UniqueInstance<MachineLearnedBehaviorNullEvaluator, MachineLearnedBehaviorEvaluator>::with(memRes).create();
        return moduleFactory.create(std::move(evaluator), memRes);
    }

    Vector<Matrix<std::uint16_t> > neuralNetworkIndicesPerMeshRegion{memRes};
    neuralNetworkIndicesPerMeshRegion.resize(reader->getMeshCount());
    for (std::uint16_t meshIdx = {}; meshIdx < neuralNetworkIndicesPerMeshRegion.size(); ++meshIdx) {
        neuralNetworkIndicesPerMeshRegion[meshIdx].resize(reader->getMeshRegionCount(meshIdx));
        for (std::uint16_t regionIdx = {}; regionIdx < neuralNetworkIndicesPerMeshRegion[meshIdx].size(); ++regionIdx) {
            const auto netIndices = reader->getNeuralNetworkIndicesForMeshRegion(meshIdx, regionIdx);
            neuralNetworkIndicesPerMeshRegion[meshIdx][regionIdx].assign(netIndices.begin(), netIndices.end());
        }
    }

    return moduleFactory.create(createMLEvaluator(config, reader, memRes), std::move(neuralNetworkIndicesPerMeshRegion));
}

MachineLearnedBehavior::Pointer MachineLearnedBehaviorFactory::create(const Configuration& config,
                                                                      const RigMetrics& metrics,
                                                                      MemoryResource* memRes) {
    auto moduleFactory = UniqueInstance<MachineLearnedBehavior>::with(memRes);
    if (!config.loadMachineLearnedBehavior || (metrics.neuralNetworkCount == 0u)) {
        auto evaluator =
            UniqueInstance<MachineLearnedBehaviorNullEvaluator, MachineLearnedBehaviorEvaluator>::with(memRes).create();
        return moduleFactory.create(std::move(evaluator), memRes);
    }
    return moduleFactory.create(createMLEvaluator(config, nullptr, memRes), memRes);
}

}  // namespace rl4

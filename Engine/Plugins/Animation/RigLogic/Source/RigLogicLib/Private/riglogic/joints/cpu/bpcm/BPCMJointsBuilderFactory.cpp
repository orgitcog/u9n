// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/joints/cpu/bpcm/BPCMJointsBuilderFactory.h"

#include "riglogic/joints/cpu/bpcm/BPCMJointsBuilder.h"
#include "riglogic/system/simd/Detect.h"

namespace rl4 {

UniqueInstance<JointsBuilder>::PointerType BPCMJointsBuilderFactory::create(const Configuration& config, MemoryResource* memRes) {
    auto features = trimd::getCPUFeatures();
    RL_UNUSED(features);
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
                    using SSEBPCMJointsBuilder = bpcm::BPCMJointsBuilder<std::uint16_t, trimd::sse::F128>;
                    return UniqueInstance<SSEBPCMJointsBuilder, JointsBuilder>::with(memRes).create(config, memRes);
                }
            #endif  // RL_BUILD_WITH_HALF_FLOATS
            using SSEBPCMJointsBuilder = bpcm::BPCMJointsBuilder<float, trimd::sse::F128>;
            return UniqueInstance<SSEBPCMJointsBuilder, JointsBuilder>::with(memRes).create(config, memRes);
        }
    #endif  // RL_BUILD_WITH_SSE
    #ifdef RL_BUILD_WITH_AVX
        #ifdef RL_DISABLE_RUNTIME_FEATURE_DETECTION
            features.AVX = true;
        #endif  // RL_DISABLE_RUNTIME_FEATURE_DETECTION
        if (features.AVX &&
            ((config.calculationType == CalculationType::AVX) || (config.calculationType == CalculationType::AnyVector))) {
            #ifdef RL_BUILD_WITH_HALF_FLOATS
                #ifdef RL_DISABLE_RUNTIME_FEATURE_DETECTION
                    features.F16C = true;
                #endif  // RL_DISABLE_RUNTIME_FEATURE_DETECTION
                if (features.F16C) {
                    using AVXBPCMJointsBuilder = bpcm::BPCMJointsBuilder<std::uint16_t, trimd::avx::F256>;
                    return UniqueInstance<AVXBPCMJointsBuilder, JointsBuilder>::with(memRes).create(config, memRes);
                }
            #endif  // RL_BUILD_WITH_HALF_FLOATS
            using AVXBPCMJointsBuilder = bpcm::BPCMJointsBuilder<float, trimd::avx::F256>;
            return UniqueInstance<AVXBPCMJointsBuilder, JointsBuilder>::with(memRes).create(config, memRes);
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
                    using NEONBPCMJointsBuilder = bpcm::BPCMJointsBuilder<std::uint16_t, trimd::neon::F128>;
                    return UniqueInstance<NEONBPCMJointsBuilder, JointsBuilder>::with(memRes).create(config, memRes);
                }
            #endif  // RL_BUILD_WITH_HALF_FLOATS
            using NEONBPCMJointsBuilder = bpcm::BPCMJointsBuilder<float, trimd::neon::F128>;
            return UniqueInstance<NEONBPCMJointsBuilder, JointsBuilder>::with(memRes).create(config, memRes);
        }
    #endif  // RL_BUILD_WITH_NEON
    using ScalarBPCMJointsBuilder = bpcm::BPCMJointsBuilder<float, trimd::scalar::F128>;
    return UniqueInstance<ScalarBPCMJointsBuilder, JointsBuilder>::with(memRes).create(config, memRes);
}

}  // namespace rl4

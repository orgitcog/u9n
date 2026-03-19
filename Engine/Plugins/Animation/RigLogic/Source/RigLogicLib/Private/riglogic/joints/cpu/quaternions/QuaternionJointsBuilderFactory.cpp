// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/joints/cpu/quaternions/QuaternionJointsBuilderFactory.h"

#include "riglogic/joints/cpu/quaternions/QuaternionJointsBuilder.h"
#include "riglogic/system/simd/Detect.h"

namespace rl4 {

UniqueInstance<JointsBuilder>::PointerType QuaternionJointsBuilderFactory::create(const Configuration& config,
                                                                                  MemoryResource* memRes) {
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
                    using SSEQuaternionJointsBuilder = QuaternionJointsBuilder<std::uint16_t, trimd::sse::F256, trimd::sse::F128>;
                    return UniqueInstance<SSEQuaternionJointsBuilder, JointsBuilder>::with(memRes).create(config, memRes);
                }
            #endif  // RL_BUILD_WITH_HALF_FLOATS
            using SSEQuaternionJointsBuilder = QuaternionJointsBuilder<float, trimd::sse::F256, trimd::sse::F128>;
            return UniqueInstance<SSEQuaternionJointsBuilder, JointsBuilder>::with(memRes).create(config, memRes);
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
                    using AVXQuaternionJointsBuilder = QuaternionJointsBuilder<std::uint16_t, trimd::avx::F256, trimd::sse::F128>;
                    return UniqueInstance<AVXQuaternionJointsBuilder, JointsBuilder>::with(memRes).create(config, memRes);
                }
            #endif  // RL_BUILD_WITH_HALF_FLOATS
            using AVXQuaternionJointsBuilder = QuaternionJointsBuilder<float, trimd::avx::F256, trimd::sse::F128>;
            return UniqueInstance<AVXQuaternionJointsBuilder, JointsBuilder>::with(memRes).create(config, memRes);
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
                    using NEONQuaternionJointsBuilder = QuaternionJointsBuilder<std::uint16_t, trimd::neon::F256,
                                                                                trimd::neon::F128>;
                    return UniqueInstance<NEONQuaternionJointsBuilder, JointsBuilder>::with(memRes).create(config, memRes);
                }
            #endif  // RL_BUILD_WITH_HALF_FLOATS
            using NEONQuaternionJointsBuilder = QuaternionJointsBuilder<float, trimd::neon::F256, trimd::neon::F128>;
            return UniqueInstance<NEONQuaternionJointsBuilder, JointsBuilder>::with(memRes).create(config, memRes);
        }
    #endif  // RL_BUILD_WITH_NEON
    using ScalarQuaternionJointsBuilder = QuaternionJointsBuilder<float, trimd::scalar::F256, trimd::scalar::F128>;
    return UniqueInstance<ScalarQuaternionJointsBuilder, JointsBuilder>::with(memRes).create(config, memRes);
}

}  // namespace rl4

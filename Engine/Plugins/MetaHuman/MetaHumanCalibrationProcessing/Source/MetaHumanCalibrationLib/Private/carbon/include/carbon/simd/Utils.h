// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Common.h>
#include <carbon/simd/SimdeWarnings.h>

CARBON_DISABLE_SIMDE_WARNINGS
#if defined CARBON_ENABLE_SSE && CARBON_ENABLE_SSE
#include <simde/x86/sse2.h>
#include <simde/x86/avx.h>
#endif
#if defined CARBON_ENABLE_AVX && CARBON_ENABLE_AVX
#include <simde/x86/avx.h>
#include <simde/x86/avx2.h>
#endif
CARBON_REENABLE_SIMDE_WARNINGS

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

#if defined CARBON_ENABLE_SSE && CARBON_ENABLE_SSE
//! \return value[0] + value[1] + value[2] + value[3]
inline float HorizontalSum(simde__m128 value)
{
    const simde__m128 perm1 = simde_mm_shuffle_ps(value, value, SIMDE_MM_SHUFFLE(3, 3, 1, 1));
    const simde__m128 sum1 = simde_mm_add_ps(value, perm1);
    const simde__m128 perm2 = simde_mm_movehl_ps(perm1, sum1);
    const simde__m128 sum2 = simde_mm_add_ss(sum1, perm2);
    return simde_mm_cvtss_f32(sum2);
}

inline simde__m128i mul_epi32_sse2(const simde__m128i& a, const simde__m128i& b)
{
#if defined(__clang__) && !defined(__SSE4_1__)
	simde__m128i tmp1 = simde_mm_mul_epu32(a, b); /* mul 2,0*/
	simde__m128i tmp2 = simde_mm_mul_epu32(simde_mm_srli_si128(a, 4), simde_mm_srli_si128(b, 4)); /* mul 3,1 */
	return simde_mm_unpacklo_epi32(simde_mm_shuffle_epi32(tmp1, SIMDE_MM_SHUFFLE(0, 0, 2, 0)), simde_mm_shuffle_epi32(tmp2, SIMDE_MM_SHUFFLE(0, 0, 2, 0))); /* shuffle results to [63..0] and pack */
#else
	return simde_mm_mullo_epi32(a, b);
#endif
}

#endif

#if defined CARBON_ENABLE_AVX && CARBON_ENABLE_AVX
//! \return value[0] + value[1] + ... + value[7]
inline float HorizontalSum(simde__m256 value) {
    const simde__m128 first = simde_mm256_castps256_ps128(value);
    const simde__m128 second = simde_mm256_extractf128_ps(value, 1);
    return HorizontalSum(simde_mm_add_ps(first, second));
}

#endif

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

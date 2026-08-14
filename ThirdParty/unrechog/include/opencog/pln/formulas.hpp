#pragma once
/**
 * @file formulas.hpp
 * @brief PLN inference formulas with SIMD optimization
 *
 * Implements the core PLN truth value formulas:
 * - Deduction
 * - Inversion
 * - Revision
 * - Abduction
 * - And/Or/Not
 *
 * SIMD versions process multiple truth values in parallel.
 */

#include <opencog/core/types.hpp>
#include <opencog/core/memory.hpp>

#include <algorithm>
#include <cmath>
#include <span>

// SIMD intrinsics
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #include <immintrin.h>
    #define OPENCOG_HAS_SSE 1
    #if defined(__AVX2__)
        #define OPENCOG_HAS_AVX2 1
    #endif
#elif defined(__aarch64__) || defined(_M_ARM64)
    #include <arm_neon.h>
    #define OPENCOG_HAS_NEON 1
#endif

namespace opencog::pln {

// ============================================================================
// Constants
// ============================================================================

/// Default "lookahead" parameter for PLN formulas
inline constexpr float DEFAULT_K = 800.0f;

/// Small epsilon to avoid division by zero
inline constexpr float EPSILON = 1e-10f;

// ============================================================================
// Scalar Formulas (single truth value operations)
// ============================================================================

/**
 * @brief Deduction formula: (A->B, B->C) => A->C
 *
 * sAC = sAB * sBC + (1 - sAB) * (sC - sBC * sB) / (1 - sB)
 */
[[nodiscard]] inline TruthValue deduction(
    TruthValue ab,  // A -> B
    TruthValue bc,  // B -> C
    float sB,       // P(B)
    float sC        // P(C)
) noexcept {
    float sAB = ab.strength;
    float sBC = bc.strength;

    // Strength calculation
    float term1 = sAB * sBC;
    float term2 = (1.0f - sAB) * (sC - sBC * sB) / (1.0f - sB + EPSILON);
    float sAC = term1 + term2;

    // Clamp to [0, 1]
    sAC = std::clamp(sAC, 0.0f, 1.0f);

    // Confidence: geometric mean of inputs, reduced
    float cAC = ab.confidence * bc.confidence * 0.9f;

    return TruthValue{sAC, cAC};
}

/**
 * @brief Inversion formula: (A->B) => B->A
 *
 * sBA = sAB * sA / sB
 */
[[nodiscard]] inline TruthValue inversion(
    TruthValue ab,  // A -> B
    float sA,       // P(A)
    float sB        // P(B)
) noexcept {
    float sBA = ab.strength * sA / (sB + EPSILON);
    sBA = std::clamp(sBA, 0.0f, 1.0f);

    // Confidence reduced due to less direct evidence
    float cBA = ab.confidence * 0.7f;

    return TruthValue{sBA, cBA};
}

/**
 * @brief Revision formula: merge two truth values for the same statement
 *
 * This is the key formula for combining evidence.
 */
[[nodiscard]] inline TruthValue revision(TruthValue tv1, TruthValue tv2) noexcept {
    // Convert to count representation
    float n1 = tv1.count();
    float n2 = tv2.count();

    // Weighted merge
    float s = (tv1.strength * n1 + tv2.strength * n2) / (n1 + n2 + EPSILON);
    float n = n1 + n2;

    return TruthValue::from_count(s, n);
}

/**
 * @brief Abduction formula: (A->B, C->B) => A->C
 *
 * sAC = sAB * sC / sB + (1 - sAB) * sCB * (1 - sC) / (1 - sB)
 */
[[nodiscard]] inline TruthValue abduction(
    TruthValue ab,  // A -> B
    TruthValue cb,  // C -> B
    float sB,       // P(B)
    float sC        // P(C)
) noexcept {
    float sAB = ab.strength;
    float sCB = cb.strength;

    // Invert CB to get BC (approximately)
    float sBC = sCB * sC / (sB + EPSILON);

    // Then apply deduction-like formula
    float term1 = sAB * sC / (sB + EPSILON);
    float term2 = (1.0f - sAB) * sCB * (1.0f - sC) / (1.0f - sB + EPSILON);
    float sAC = term1 + term2;
    sAC = std::clamp(sAC, 0.0f, 1.0f);

    // Lower confidence for abductive reasoning
    float cAC = ab.confidence * cb.confidence * 0.6f;

    return TruthValue{sAC, cAC};
}

/**
 * @brief Modus ponens: (A, A->B) => B
 */
[[nodiscard]] inline TruthValue modus_ponens(
    TruthValue a,   // P(A)
    TruthValue ab   // A -> B
) noexcept {
    float sB = a.strength * ab.strength + (1.0f - a.strength) * 0.5f;  // Assuming independence
    float cB = a.confidence * ab.confidence * 0.9f;
    return TruthValue{sB, cB};
}

// ============================================================================
// Logical Operations
// ============================================================================

/**
 * @brief Fuzzy AND (conjunction)
 */
[[nodiscard]] inline TruthValue fuzzy_and(TruthValue a, TruthValue b) noexcept {
    float s = a.strength * b.strength;
    float c = std::min(a.confidence, b.confidence);
    return TruthValue{s, c};
}

/**
 * @brief Fuzzy OR (disjunction)
 */
[[nodiscard]] inline TruthValue fuzzy_or(TruthValue a, TruthValue b) noexcept {
    float s = a.strength + b.strength - a.strength * b.strength;
    float c = std::min(a.confidence, b.confidence);
    return TruthValue{s, c};
}

/**
 * @brief Fuzzy NOT (negation)
 */
[[nodiscard]] inline TruthValue fuzzy_not(TruthValue a) noexcept {
    return TruthValue{1.0f - a.strength, a.confidence};
}

/**
 * @brief N-ary AND
 */
[[nodiscard]] inline TruthValue fuzzy_and_n(std::span<const TruthValue> tvs) noexcept {
    if (tvs.empty()) return TruthValue::default_tv();

    float s = 1.0f;
    float c = 1.0f;
    for (const auto& tv : tvs) {
        s *= tv.strength;
        c = std::min(c, tv.confidence);
    }
    return TruthValue{s, c};
}

/**
 * @brief N-ary OR
 */
[[nodiscard]] inline TruthValue fuzzy_or_n(std::span<const TruthValue> tvs) noexcept {
    if (tvs.empty()) return TruthValue::default_tv();

    float s = 0.0f;
    float c = 1.0f;
    for (const auto& tv : tvs) {
        s = s + tv.strength - s * tv.strength;
        c = std::min(c, tv.confidence);
    }
    return TruthValue{s, c};
}

// ============================================================================
// SIMD Batch Operations
// ============================================================================

#ifdef OPENCOG_HAS_AVX2

/**
 * @brief Batch deduction using AVX2 (8 operations at once)
 *
 * Processes 8 deduction operations in parallel.
 */
inline void batch_deduction_avx2(
    const float* sAB, const float* cAB,  // Input: A->B
    const float* sBC, const float* cBC,  // Input: B->C
    const float* sB,                      // Input: P(B)
    const float* sC,                      // Input: P(C)
    float* sAC, float* cAC,              // Output: A->C
    size_t count                          // Must be multiple of 8
) {
    const __m256 ones = _mm256_set1_ps(1.0f);
    const __m256 zeros = _mm256_setzero_ps();
    const __m256 eps = _mm256_set1_ps(EPSILON);
    const __m256 conf_factor = _mm256_set1_ps(0.9f);

    for (size_t i = 0; i < count; i += 8) {
        // Load inputs
        __m256 v_sAB = _mm256_load_ps(sAB + i);
        __m256 v_cAB = _mm256_load_ps(cAB + i);
        __m256 v_sBC = _mm256_load_ps(sBC + i);
        __m256 v_cBC = _mm256_load_ps(cBC + i);
        __m256 v_sB = _mm256_load_ps(sB + i);
        __m256 v_sC = _mm256_load_ps(sC + i);

        // term1 = sAB * sBC
        __m256 term1 = _mm256_mul_ps(v_sAB, v_sBC);

        // term2 = (1 - sAB) * (sC - sBC * sB) / (1 - sB + eps)
        __m256 one_minus_sAB = _mm256_sub_ps(ones, v_sAB);
        __m256 sBC_times_sB = _mm256_mul_ps(v_sBC, v_sB);
        __m256 numerator = _mm256_sub_ps(v_sC, sBC_times_sB);
        numerator = _mm256_mul_ps(one_minus_sAB, numerator);

        __m256 one_minus_sB = _mm256_sub_ps(ones, v_sB);
        __m256 denominator = _mm256_add_ps(one_minus_sB, eps);
        __m256 term2 = _mm256_div_ps(numerator, denominator);

        // sAC = clamp(term1 + term2, 0, 1)
        __m256 v_sAC = _mm256_add_ps(term1, term2);
        v_sAC = _mm256_max_ps(v_sAC, zeros);
        v_sAC = _mm256_min_ps(v_sAC, ones);

        // cAC = cAB * cBC * 0.9
        __m256 v_cAC = _mm256_mul_ps(v_cAB, v_cBC);
        v_cAC = _mm256_mul_ps(v_cAC, conf_factor);

        // Store outputs
        _mm256_store_ps(sAC + i, v_sAC);
        _mm256_store_ps(cAC + i, v_cAC);
    }
}

/**
 * @brief Batch revision using AVX2
 */
inline void batch_revision_avx2(
    const float* s1, const float* c1,  // Input: TV1
    const float* s2, const float* c2,  // Input: TV2
    float* s_out, float* c_out,        // Output
    size_t count
) {
    const __m256 k = _mm256_set1_ps(DEFAULT_K);
    const __m256 ones = _mm256_set1_ps(1.0f);
    const __m256 eps = _mm256_set1_ps(EPSILON);

    for (size_t i = 0; i < count; i += 8) {
        __m256 v_s1 = _mm256_load_ps(s1 + i);
        __m256 v_c1 = _mm256_load_ps(c1 + i);
        __m256 v_s2 = _mm256_load_ps(s2 + i);
        __m256 v_c2 = _mm256_load_ps(c2 + i);

        // n = c * K / (1 - c + eps)
        __m256 one_minus_c1 = _mm256_sub_ps(ones, v_c1);
        __m256 one_minus_c2 = _mm256_sub_ps(ones, v_c2);
        __m256 denom1 = _mm256_add_ps(one_minus_c1, eps);
        __m256 denom2 = _mm256_add_ps(one_minus_c2, eps);

        __m256 n1 = _mm256_div_ps(_mm256_mul_ps(v_c1, k), denom1);
        __m256 n2 = _mm256_div_ps(_mm256_mul_ps(v_c2, k), denom2);

        // s = (s1 * n1 + s2 * n2) / (n1 + n2 + eps)
        __m256 n_sum = _mm256_add_ps(n1, n2);
        __m256 weighted = _mm256_add_ps(
            _mm256_mul_ps(v_s1, n1),
            _mm256_mul_ps(v_s2, n2)
        );
        __m256 v_s = _mm256_div_ps(weighted, _mm256_add_ps(n_sum, eps));

        // c = n / (n + K)
        __m256 v_c = _mm256_div_ps(n_sum, _mm256_add_ps(n_sum, k));

        _mm256_store_ps(s_out + i, v_s);
        _mm256_store_ps(c_out + i, v_c);
    }
}

/**
 * @brief Batch fuzzy AND using AVX2
 */
inline void batch_fuzzy_and_avx2(
    const float* s1, const float* c1,
    const float* s2, const float* c2,
    float* s_out, float* c_out,
    size_t count
) {
    for (size_t i = 0; i < count; i += 8) {
        __m256 v_s1 = _mm256_load_ps(s1 + i);
        __m256 v_c1 = _mm256_load_ps(c1 + i);
        __m256 v_s2 = _mm256_load_ps(s2 + i);
        __m256 v_c2 = _mm256_load_ps(c2 + i);

        __m256 v_s = _mm256_mul_ps(v_s1, v_s2);
        __m256 v_c = _mm256_min_ps(v_c1, v_c2);

        _mm256_store_ps(s_out + i, v_s);
        _mm256_store_ps(c_out + i, v_c);
    }
}

/**
 * @brief Batch fuzzy OR using AVX2
 */
inline void batch_fuzzy_or_avx2(
    const float* s1, const float* c1,
    const float* s2, const float* c2,
    float* s_out, float* c_out,
    size_t count
) {
    const __m256 ones = _mm256_set1_ps(1.0f);

    for (size_t i = 0; i < count; i += 8) {
        __m256 v_s1 = _mm256_load_ps(s1 + i);
        __m256 v_c1 = _mm256_load_ps(c1 + i);
        __m256 v_s2 = _mm256_load_ps(s2 + i);
        __m256 v_c2 = _mm256_load_ps(c2 + i);

        // s = s1 + s2 - s1 * s2
        __m256 product = _mm256_mul_ps(v_s1, v_s2);
        __m256 sum = _mm256_add_ps(v_s1, v_s2);
        __m256 v_s = _mm256_sub_ps(sum, product);
        __m256 v_c = _mm256_min_ps(v_c1, v_c2);

        _mm256_store_ps(s_out + i, v_s);
        _mm256_store_ps(c_out + i, v_c);
    }
}

#endif // OPENCOG_HAS_AVX2

// ============================================================================
// Portable Batch Operations (fallback)
// ============================================================================

/**
 * @brief Portable batch deduction (scalar fallback)
 */
inline void batch_deduction(
    std::span<const TruthValue> ab,
    std::span<const TruthValue> bc,
    std::span<const float> sB,
    std::span<const float> sC,
    std::span<TruthValue> out
) {
    size_t n = std::min({ab.size(), bc.size(), sB.size(), sC.size(), out.size()});

#ifdef OPENCOG_HAS_AVX2
    // Use SIMD for aligned, multiple-of-8 data
    if (n >= 8 && n % 8 == 0) {
        // Convert SoA for SIMD (ideally data would already be in SoA format)
        SimdVector<float> sAB_vec(n), cAB_vec(n);
        SimdVector<float> sBC_vec(n), cBC_vec(n);
        SimdVector<float> sAC_vec(n), cAC_vec(n);

        for (size_t i = 0; i < n; ++i) {
            sAB_vec[i] = ab[i].strength;
            cAB_vec[i] = ab[i].confidence;
            sBC_vec[i] = bc[i].strength;
            cBC_vec[i] = bc[i].confidence;
        }

        batch_deduction_avx2(
            sAB_vec.data(), cAB_vec.data(),
            sBC_vec.data(), cBC_vec.data(),
            sB.data(), sC.data(),
            sAC_vec.data(), cAC_vec.data(),
            n
        );

        for (size_t i = 0; i < n; ++i) {
            out[i] = TruthValue{sAC_vec[i], cAC_vec[i]};
        }
        return;
    }
#endif

    // Scalar fallback
    for (size_t i = 0; i < n; ++i) {
        out[i] = deduction(ab[i], bc[i], sB[i], sC[i]);
    }
}

/**
 * @brief Portable batch revision
 */
inline void batch_revision(
    std::span<const TruthValue> tv1,
    std::span<const TruthValue> tv2,
    std::span<TruthValue> out
) {
    size_t n = std::min({tv1.size(), tv2.size(), out.size()});

    for (size_t i = 0; i < n; ++i) {
        out[i] = revision(tv1[i], tv2[i]);
    }
}

} // namespace opencog::pln

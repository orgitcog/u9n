// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Common.h>
#include <carbon/simd/Utils.h>

#include <algorithm>
#include <cmath>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <typename T, int C>
struct SimdTypeBase
{
    inline static constexpr int Alignment() { return sizeof(T) * C; }
    inline static constexpr int Size() { return C; }
    inline static int Pad(int n) { return (n % Size()) ? (n + Size() - (n % Size())) : n; }
};

template <typename T, int C>
struct _SimdType;

template <class T, int C> inline _SimdType<T, C> Abs(const _SimdType<T, C>& value);
template <class T, int C> inline _SimdType<T, C> Min(const _SimdType<T, C>& value1, const _SimdType<T, C>& value2);
template <class T, int C> inline _SimdType<T, C> Max(const _SimdType<T, C>& value1, const _SimdType<T, C>& value2);
template <class T, int C> inline _SimdType<T, C> Sqrt(const _SimdType<T, C>& value);

template <>
struct _SimdType<bool, 1> : public SimdTypeBase<int, 1>
{
    bool vec;
    _SimdType(bool value) : vec(value) {}
    inline void StoreAligned(int* ptr) const { ptr[0] = int(vec); }
    inline bool Any() const { return vec; }
};
template <>
struct _SimdType<int, 1> : public SimdTypeBase<int, 1>
{
    int vec;

    _SimdType() = default;
    explicit _SimdType(int value) : vec(value) {}
    _SimdType(const _SimdType& other) = default;
    _SimdType& operator=(const _SimdType& other) = default;
    _SimdType(_SimdType&& vec) = default;
    _SimdType& operator=(_SimdType&& other) = default;
    ~_SimdType() = default;

    inline static _SimdType Zero() { return _SimdType(0); }
    inline void SetZero() { vec = 0; }
    inline void Set(int value) { vec = value; }
    inline void LoadAligned(const int* ptr) { vec = ptr[0]; }
    inline void StoreAligned(int* ptr) const { *ptr = vec; }
    inline _SimdType operator+(const _SimdType& other) const { return _SimdType{ vec + other.vec }; }
    inline _SimdType operator-(const _SimdType& other) const { return _SimdType{ vec - other.vec }; }
    inline _SimdType operator*(const _SimdType& other) const { return _SimdType{ vec * other.vec }; }
    inline _SimdType operator/(const _SimdType& other) const { return _SimdType{ vec / other.vec }; }
    inline _SimdType& operator+=(const _SimdType& other) { vec += other.vec; return *this; }
    inline _SimdType& operator-=(const _SimdType& other) { vec -= other.vec; return *this; }
    inline _SimdType& operator*=(const _SimdType& other) { vec *= other.vec; return *this; }
    inline _SimdType& operator/=(const _SimdType& other) { vec /= other.vec; return *this; }

    template <typename S>
    inline _SimdType<S, 1> ValueCast() const;

    template <typename S>
    inline _SimdType<S, 1> BitwiseCast() const;
};

template <>
struct _SimdType<float, 1> : public SimdTypeBase<float, 1>
{
    float vec;

    _SimdType() = default;
    explicit _SimdType(float value) : vec(value) {}
    _SimdType(const _SimdType& other) = default;
    _SimdType& operator=(const _SimdType& other) = default;
    _SimdType(_SimdType&& vec) = default;
    _SimdType& operator=(_SimdType&& other) = default;
    ~_SimdType() = default;

    inline static _SimdType Zero() { return _SimdType{ 0.0f }; }
    inline void SetZero() { vec = 0.0f; }
    inline void Set(float value) { vec = value; }
    inline void LoadAligned(const float* ptr) { vec = *ptr; }
    inline void StoreAligned(float* ptr) const { *ptr = vec; }
    inline _SimdType operator+(const _SimdType& other) const { return _SimdType(vec + other.vec); }
    inline _SimdType operator-(const _SimdType& other) const { return _SimdType(vec - other.vec); }
    inline _SimdType operator*(const _SimdType& other) const { return _SimdType(vec * other.vec); }
    inline _SimdType operator/(const _SimdType& other) const { return _SimdType(vec / other.vec); }
    inline _SimdType& operator+=(const _SimdType& other) { vec += other.vec; return *this; }
    inline _SimdType& operator-=(const _SimdType& other) { vec -= other.vec; return *this; }
    inline _SimdType& operator*=(const _SimdType& other) { vec *= other.vec; return *this; }
    inline _SimdType& operator/=(const _SimdType& other) { vec /= other.vec; return *this; }
    inline _SimdType Reciprocal() const { return _SimdType{ 1.0f / vec }; }
    inline float HorizontalSum() const { return vec; }
    inline _SimdType Square() const { return _SimdType{ vec * vec }; }
    inline _SimdType<float, 1> ConditionalMove(const _SimdType<bool, 1>& mask) const
    {
        return mask.vec ? _SimdType<float, 1>{ vec } : _SimdType<float, 1>(0.0f);
    }

    template <typename S>
    inline _SimdType<S, 1> ValueCast() const;

    template <typename S>
    inline _SimdType<S, 1> BitwiseCast() const;
};

template <> inline _SimdType<float, 1> _SimdType<int, 1>::ValueCast<float>() const { return _SimdType<float, 1>{ static_cast<float>(vec) }; }
template <> inline _SimdType<int, 1> _SimdType<float, 1>::ValueCast<int>() const { return _SimdType<int, 1>{ static_cast<int>(vec) }; }

#if !defined(__APPLE__) && defined(__unix__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif
template <> inline _SimdType<float, 1> _SimdType<int, 1>::BitwiseCast<float>() const { return _SimdType<float, 1>{ *(reinterpret_cast<const float*>(&vec)) }; }
template <> inline _SimdType<int, 1> _SimdType<float, 1>::BitwiseCast<int>() const { return _SimdType<int, 1>{ *(reinterpret_cast<const int*>(&vec)) }; }
#if !defined(__APPLE__) && defined(__unix__)
#pragma GCC diagnostic pop
#endif

inline _SimdType<bool, 1> operator&&(const _SimdType<bool, 1>& a, const _SimdType<bool, 1>& b) { return _SimdType<bool, 1>{ a.vec && b.vec }; }
inline _SimdType<bool, 1> operator||(const _SimdType<bool, 1>& a, const _SimdType<bool, 1>& b) { return _SimdType<bool, 1>{ a.vec || b.vec }; }

template <typename T> inline _SimdType<bool, 1> operator==(const _SimdType<T, 1>& a, const _SimdType<T, 1>& b) { return _SimdType<bool, 1>{ a.vec == b.vec }; }
template <typename T> inline _SimdType<bool, 1> operator!=(const _SimdType<T, 1>& a, const _SimdType<T, 1>& b) { return _SimdType<bool, 1>{ a.vec != b.vec }; }
template <typename T> inline _SimdType<bool, 1> operator<=(const _SimdType<T, 1>& a, const _SimdType<T, 1>& b) { return _SimdType<bool, 1>{ a.vec <= b.vec }; }
template <typename T> inline _SimdType<bool, 1> operator<(const _SimdType<T, 1>& a, const _SimdType<T, 1>& b) { return _SimdType<bool, 1>{ a.vec < b.vec }; }
template <typename T> inline _SimdType<bool, 1> operator>(const _SimdType<T, 1>& a, const _SimdType<T, 1>& b) { return _SimdType<bool, 1>{ a.vec > b.vec }; }

template <> inline _SimdType<float, 1> Abs(const _SimdType<float, 1>& value) { return _SimdType<float, 1>{ std::abs(value.vec) }; }
template <> inline _SimdType<float, 1> Min(const _SimdType<float, 1>& value1, const _SimdType<float, 1>& value2)
{
    return _SimdType<float, 1>{ std::min(
                                    value1.vec,
                                    value2.vec) };
}

template <> inline _SimdType<float, 1> Max(const _SimdType<float, 1>& value1, const _SimdType<float, 1>& value2)
{
    return _SimdType<float, 1>{ std::max(
                                    value1.vec,
                                    value2.vec) };
}

template <> inline _SimdType<float, 1> Sqrt(const _SimdType<float, 1>& value) { return _SimdType<float, 1>{ std::sqrt(value.vec) }; }


#if defined CARBON_ENABLE_SSE && CARBON_ENABLE_SSE

template <>
struct _SimdType<bool, 4> : public SimdTypeBase<int, 4>
{
    simde__m128i vec;
    _SimdType(simde__m128i&& vec) : vec(vec) {}
    inline void StoreAligned(int* ptr) const { simde_mm_store_si128((simde__m128i*)ptr, vec); }
    inline bool Any() const { return static_cast<bool>(simde_mm_movemask_ps(simde_mm_castsi128_ps(vec))); }
};

template <>
struct _SimdType<int, 4> : public SimdTypeBase<int, 4>
{
    simde__m128i vec;

    _SimdType() = default;
    _SimdType(simde__m128i&& vec) : vec(vec) {}
    explicit _SimdType(int value) : vec(simde_mm_set1_epi32(value)) {}
    _SimdType(const _SimdType& other) = default;
    _SimdType& operator=(const _SimdType& other) = default;
    _SimdType(_SimdType&& vec) = default;
    _SimdType& operator=(_SimdType&& other) = default;
    ~_SimdType() = default;

    inline static _SimdType Zero() { return { simde_mm_setzero_si128() }; }
    inline void SetZero() { vec = simde_mm_setzero_si128(); }
    inline void Set(int value) { vec = simde_mm_set1_epi32 (value); }
    inline void LoadAligned(const int* ptr) { vec = simde_mm_load_si128((const simde__m128i*)ptr); }
    inline void StoreAligned(int* ptr) const { simde_mm_store_si128((simde__m128i*)ptr, vec); }
    inline _SimdType operator+(const _SimdType& other) const { return { simde_mm_add_epi32(vec, other.vec) }; }
    inline _SimdType operator-(const _SimdType& other) const { return { simde_mm_sub_epi32(vec, other.vec) }; }
    inline _SimdType operator*(const _SimdType& other) const { return { mul_epi32_sse2(vec, other.vec) }; }
    // inline _SimdType operator/(const _SimdType& other) const { return {_mm_div_epi32(vec, other.vec)}; }
    inline _SimdType& operator+=(const _SimdType& other) { vec = simde_mm_add_epi32(vec, other.vec); return *this; }
    inline _SimdType& operator-=(const _SimdType& other) { vec = simde_mm_sub_epi32(vec, other.vec); return *this; }
    inline _SimdType& operator*=(const _SimdType& other) { vec = mul_epi32_sse2(vec, other.vec); return *this; }
    // inline _SimdType& operator/=(const _SimdType& other) { vec = _mm_div_epi32(vec, other.vec); return *this; }

    template <typename S>
    inline _SimdType<S, 4> ValueCast() const;

    template <typename S>
    inline _SimdType<S, 4> BitwiseCast() const;
};

template <>
struct _SimdType<float, 4> : public SimdTypeBase<float, 4>
{
    simde__m128 vec;

    _SimdType() = default;
    _SimdType(simde__m128&& vec) : vec(vec) {}
    explicit _SimdType(float value) : vec(simde_mm_set1_ps(value)) {}
    _SimdType(const _SimdType& other) = default;
    _SimdType& operator=(const _SimdType& other) = default;
    _SimdType(_SimdType&& vec) = default;
    _SimdType& operator=(_SimdType&& other) = default;
    ~_SimdType() = default;

    inline static _SimdType Zero() { return { simde_mm_setzero_ps() }; }
    inline void SetZero() { vec = simde_mm_setzero_ps(); }
    inline void Set(float value) { vec = simde_mm_set1_ps(value); }
    inline void LoadAligned(const float* ptr) { vec = simde_mm_load_ps(ptr); }
    inline void StoreAligned(float* ptr) const { simde_mm_store_ps(ptr, vec); }
    inline _SimdType operator+(const _SimdType& other) const { return { simde_mm_add_ps(vec, other.vec) }; }
    inline _SimdType operator-(const _SimdType& other) const { return { simde_mm_sub_ps(vec, other.vec) }; }
    inline _SimdType operator*(const _SimdType& other) const { return { simde_mm_mul_ps(vec, other.vec) }; }
    inline _SimdType operator/(const _SimdType& other) const { return { simde_mm_div_ps(vec, other.vec) }; }
    inline _SimdType& operator+=(const _SimdType& other) { vec = simde_mm_add_ps(vec, other.vec); return *this; }
    inline _SimdType& operator-=(const _SimdType& other) { vec = simde_mm_sub_ps(vec, other.vec); return *this; }
    inline _SimdType& operator*=(const _SimdType& other) { vec = simde_mm_mul_ps(vec, other.vec); return *this; }
    inline _SimdType& operator/=(const _SimdType& other) { vec = simde_mm_div_ps(vec, other.vec); return *this; }
    inline _SimdType Reciprocal() const { return { simde_mm_rcp_ps(vec) }; }
    inline float HorizontalSum() const { return TITAN_NAMESPACE::HorizontalSum(vec); }
    inline _SimdType Square() const { return simde_mm_mul_ps(vec, vec); }
    inline _SimdType<float, 4> ConditionalMove(const _SimdType<bool, 4>& mask) const { return { simde_mm_and_ps(simde_mm_castsi128_ps(mask.vec), vec) }; }
    inline _SimdType<float, 4> Abs() const { return _SimdType<float, 4>{ simde_mm_max_ps(simde_mm_sub_ps(simde_mm_setzero_ps(), vec), vec) }; }

    template <typename S>
    inline _SimdType<S, 4> ValueCast() const;

    template <typename S>
    inline _SimdType<S, 4> BitwiseCast() const;
};


template <> inline _SimdType<float, 4> _SimdType<int, 4>::ValueCast<float>() const { return _SimdType<float, 4>{ simde_mm_cvtepi32_ps(vec) }; }
template <> inline _SimdType<int, 4> _SimdType<float, 4>::ValueCast<int>() const
{
    // requires removal of half pixel as _mm256_cvtps_epi32 rounds to nearest
    return _SimdType<int, 4>{ simde_mm_cvtps_epi32(((*this) - _SimdType(0.5f)).vec) };
}

template <> inline _SimdType<float, 4> _SimdType<int, 4>::BitwiseCast<float>() const { return _SimdType<float, 4>{ simde_mm_castsi128_ps(vec) }; }
template <> inline _SimdType<int, 4> _SimdType<float, 4>::BitwiseCast<int>() const { return _SimdType<int, 4>{ simde_mm_castps_si128(vec) }; }

inline _SimdType<bool, 4> operator&&(const _SimdType<bool, 4>& a, const _SimdType<bool, 4>& b) { return { simde_mm_and_si128(a.vec, b.vec) }; }
inline _SimdType<bool, 4> operator||(const _SimdType<bool, 4>& a, const _SimdType<bool, 4>& b) { return { simde_mm_or_si128(a.vec, b.vec) }; }

inline _SimdType<bool, 4> operator==(const _SimdType<int, 4>& a, const _SimdType<int, 4>& b) { return _SimdType<bool, 4>{ simde_mm_cmpeq_epi32(a.vec, b.vec) }; }
inline _SimdType<bool, 4> operator>(const _SimdType<int, 4>& a, const _SimdType<int, 4>& b) { return _SimdType<bool, 4>{ simde_mm_cmpgt_epi32(a.vec, b.vec) }; }
inline _SimdType<bool, 4> operator<=(const _SimdType<int, 4>& a, const _SimdType<int, 4>& b) { return (b > a) || (a == b); }
inline _SimdType<bool, 4> operator<(const _SimdType<int, 4>& a, const _SimdType<int, 4>& b) { return (b > a); }

inline _SimdType<bool, 4> operator==(const _SimdType<float, 4>& a,
                                     const _SimdType<float, 4>& b) { return _SimdType<bool, 4>{ simde_mm_castps_si128(simde_mm_cmpeq_ps(a.vec, b.vec)) };
}
inline _SimdType<bool, 4> operator!=(const _SimdType<float, 4>& a,
                                     const _SimdType<float, 4>& b) { return _SimdType<bool, 4>{ simde_mm_castps_si128(simde_mm_cmpneq_ps(a.vec, b.vec)) };
}
inline _SimdType<bool, 4> operator<=(const _SimdType<float, 4>& a,
                                     const _SimdType<float, 4>& b) { return _SimdType<bool, 4>{ simde_mm_castps_si128(simde_mm_cmple_ps(a.vec, b.vec)) };
}
inline _SimdType<bool, 4> operator<(const _SimdType<float, 4>& a,
                                    const _SimdType<float, 4>& b) { return _SimdType<bool, 4>{ simde_mm_castps_si128(simde_mm_cmplt_ps(a.vec, b.vec)) };
}
inline _SimdType<bool, 4> operator>(const _SimdType<float, 4>& a,
                                    const _SimdType<float, 4>& b) { return _SimdType<bool, 4>{ simde_mm_castps_si128(simde_mm_cmpgt_ps(a.vec, b.vec)) };
}

template <> inline _SimdType<float, 4> Abs(const _SimdType<float, 4>& value) { return value.Abs(); }
template <> inline _SimdType<float, 4> Min(const _SimdType<float, 4>& value1, const _SimdType<float, 4>& value2)
{
    return _SimdType<float, 4>{ simde_mm_min_ps(
                                    value1.vec,
                                    value2.vec) };
}

template <> inline _SimdType<float, 4> Max(const _SimdType<float, 4>& value1, const _SimdType<float, 4>& value2)
{
    return _SimdType<float, 4>{ simde_mm_max_ps(
                                    value1.vec,
                                    value2.vec) };
}

template <> inline _SimdType<float, 4> Sqrt(const _SimdType<float, 4>& value) { return _SimdType<float, 4>{ simde_mm_sqrt_ps(value.vec) }; }

#endif

#if defined CARBON_ENABLE_AVX && CARBON_ENABLE_AVX

template <>
struct _SimdType<bool, 8> : public SimdTypeBase<int, 8>
{
    simde__m256i vec;
    _SimdType(simde__m256i&& vec) : vec(vec) {}
    inline void StoreAligned(int* ptr) const { simde_mm256_store_si256((simde__m256i*)ptr, vec); }
    inline bool Any() const { return simde_mm256_movemask_ps(simde_mm256_castsi256_ps(vec)); }
};

template <>
struct _SimdType<int, 8> : public SimdTypeBase<int, 8>
{
    simde__m256i vec;

    _SimdType() = default;
    _SimdType(simde__m256i&& vec) : vec(vec) {}
    explicit _SimdType(int value) : vec(simde_mm256_set1_epi32(value)) {}
    _SimdType(const _SimdType& other) = default;
    _SimdType& operator=(const _SimdType& other) = default;
    _SimdType(_SimdType&& vec) = default;
    _SimdType& operator=(_SimdType&& other) = default;
    ~_SimdType() = default;

    inline static _SimdType Zero() { return { simde_mm256_setzero_si256() }; }
    inline void SetZero() { vec = simde_mm256_setzero_si256(); }
    inline void Set(int value) { vec = simde_mm256_set1_epi32 (value); }
    inline void LoadAligned(const int* ptr) { vec = simde_mm256_load_si256((const simde__m256i*)ptr); }
    inline void StoreAligned(int* ptr) const { simde_mm256_store_si256((simde__m256i*)ptr, vec); }
    inline _SimdType operator+(const _SimdType& other) const { return { simde_mm256_add_epi32(vec, other.vec) }; }
    inline _SimdType operator-(const _SimdType& other) const { return { simde_mm256_sub_epi32(vec, other.vec) }; }
    inline _SimdType operator*(const _SimdType& other) const { return { simde_mm256_mullo_epi32(vec, other.vec) }; }
    // inline _SimdType operator/(const _SimdType& other) const { return {_mm256_div_epi32(vec, other.vec)}; }
    inline _SimdType& operator+=(const _SimdType& other) { vec = simde_mm256_add_epi32(vec, other.vec); return *this; }
    inline _SimdType& operator-=(const _SimdType& other) { vec = simde_mm256_sub_epi32(vec, other.vec); return *this; }
    inline _SimdType& operator*=(const _SimdType& other) { vec = simde_mm256_mullo_epi32(vec, other.vec); return *this; }
    // inline _SimdType& operator/=(const _SimdType& other) { vec = _mm256_div_epi32(vec, other.vec); return *this; }

    template <typename S>
    inline _SimdType<S, 8> ValueCast() const;

    template <typename S>
    inline _SimdType<S, 8> BitwiseCast() const;
};

template <>
struct _SimdType<float, 8> : public SimdTypeBase<float, 8>
{
    simde__m256 vec;

    _SimdType() = default;
    _SimdType(simde__m256&& vec) : vec(vec) {}
    explicit _SimdType(float value) : vec(simde_mm256_set1_ps(value)) {}
    _SimdType(const _SimdType& other) = default;
    _SimdType& operator=(const _SimdType& other) = default;
    _SimdType(_SimdType&& vec) = default;
    _SimdType& operator=(_SimdType&& other) = default;
    ~_SimdType() = default;

    inline static _SimdType Zero() { return { simde_mm256_setzero_ps() }; }
    inline void SetZero() { vec = simde_mm256_setzero_ps(); }
    inline void Set(float value) { vec = simde_mm256_set1_ps(value); }
    inline void LoadAligned(const float* ptr) { vec = simde_mm256_load_ps(ptr); }
    inline void StoreAligned(float* ptr) const { simde_mm256_store_ps(ptr, vec); }
    inline _SimdType operator+(const _SimdType& other) const { return { simde_mm256_add_ps(vec, other.vec) }; }
    inline _SimdType operator-(const _SimdType& other) const { return { simde_mm256_sub_ps(vec, other.vec) }; }
    inline _SimdType operator*(const _SimdType& other) const { return { simde_mm256_mul_ps(vec, other.vec) }; }
    inline _SimdType operator/(const _SimdType& other) const { return { simde_mm256_div_ps(vec, other.vec) }; }
    inline _SimdType& operator+=(const _SimdType& other) { vec = simde_mm256_add_ps(vec, other.vec); return *this; }
    inline _SimdType& operator-=(const _SimdType& other) { vec = simde_mm256_sub_ps(vec, other.vec); return *this; }
    inline _SimdType& operator*=(const _SimdType& other) { vec = simde_mm256_mul_ps(vec, other.vec); return *this; }
    inline _SimdType& operator/=(const _SimdType& other) { vec = simde_mm256_div_ps(vec, other.vec); return *this; }
    inline _SimdType Reciprocal() const { return { simde_mm256_rcp_ps(vec) }; }
    inline float HorizontalSum() const { return TITAN_NAMESPACE::HorizontalSum(vec); }
    inline _SimdType Square() const { return simde_mm256_mul_ps(vec, vec); }
    inline _SimdType<float, 8> ConditionalMove(const _SimdType<bool, 8>& mask) const { return { simde_mm256_and_ps(simde_mm256_castsi256_ps(mask.vec), vec) }; }
    inline _SimdType<float, 8> Abs() const { return _SimdType<float, 8>{simde_mm256_max_ps( simde_mm256_sub_ps(simde_mm256_setzero_ps(), vec), vec) }; }

    template <typename S>
    inline _SimdType<S, 8> ValueCast() const;

    template <typename S>
    inline _SimdType<S, 8> BitwiseCast() const;
};

template <> inline _SimdType<float, 8> _SimdType<int, 8>::ValueCast<float>() const { return _SimdType<float, 8>{ simde_mm256_cvtepi32_ps(vec) }; }
template <> inline _SimdType<int, 8> _SimdType<float, 8>::ValueCast<int>() const
{
    // requires removal of half pixel as _mm256_cvtps_epi32 rounds to nearest
    return _SimdType<int, 8>{ simde_mm256_cvtps_epi32(((*this) - _SimdType(0.5f)).vec) };
}

template <> inline _SimdType<float, 8> _SimdType<int, 8>::BitwiseCast<float>() const { return _SimdType<float, 8>{ simde_mm256_castsi256_ps (vec) }; }
template <> inline _SimdType<int, 8> _SimdType<float, 8>::BitwiseCast<int>() const { return _SimdType<int, 8>{ simde_mm256_castps_si256(vec) }; }

inline _SimdType<bool, 8> operator&&(const _SimdType<bool, 8>& a, const _SimdType<bool, 8>& b) { return _SimdType<bool, 8>{ simde_mm256_and_si256(a.vec, b.vec) }; }
inline _SimdType<bool, 8> operator||(const _SimdType<bool, 8>& a, const _SimdType<bool, 8>& b) { return _SimdType<bool, 8>{ simde_mm256_or_si256(a.vec, b.vec) }; }

inline _SimdType<bool, 8> operator==(const _SimdType<int, 8>& a, const _SimdType<int, 8>& b) { return _SimdType<bool, 8>{ simde_mm256_cmpeq_epi32(a.vec, b.vec) }; }
inline _SimdType<bool, 8> operator>(const _SimdType<int, 8>& a, const _SimdType<int, 8>& b) { return _SimdType<bool, 8>{ simde_mm256_cmpgt_epi32(a.vec, b.vec) }; }
inline _SimdType<bool, 8> operator<=(const _SimdType<int, 8>& a, const _SimdType<int, 8>& b) { return ((b > a) || (a == b)); }
inline _SimdType<bool, 8> operator<(const _SimdType<int, 8>& a, const _SimdType<int, 8>& b) { return (b > a); }

inline _SimdType<bool, 8> operator==(const _SimdType<float, 8>& a,
                                     const _SimdType<float, 8>& b) { return _SimdType<bool, 8>{ simde_mm256_castps_si256(simde_mm256_cmp_ps(a.vec, b.vec, SIMDE_CMP_EQ_OS)) };
}
inline _SimdType<bool, 8> operator!=(const _SimdType<float, 8>& a,
                                     const _SimdType<float, 8>& b) { return _SimdType<bool, 8>{ simde_mm256_castps_si256(simde_mm256_cmp_ps(a.vec, b.vec, SIMDE_CMP_NEQ_OS)) };
}
inline _SimdType<bool, 8> operator<=(const _SimdType<float, 8>& a,
                                     const _SimdType<float, 8>& b) { return _SimdType<bool, 8>{ simde_mm256_castps_si256(simde_mm256_cmp_ps(a.vec, b.vec, SIMDE_CMP_LE_OS)) };
}
inline _SimdType<bool, 8> operator<(const _SimdType<float, 8>& a,
                                    const _SimdType<float, 8>& b) { return _SimdType<bool, 8>{ simde_mm256_castps_si256(simde_mm256_cmp_ps(a.vec, b.vec, SIMDE_CMP_LT_OS)) };
}
inline _SimdType<bool, 8> operator>(const _SimdType<float, 8>& a,
                                    const _SimdType<float, 8>& b) { return _SimdType<bool, 8>{ simde_mm256_castps_si256(simde_mm256_cmp_ps(a.vec, b.vec, SIMDE_CMP_GT_OS)) };
}

template <> inline _SimdType<float, 8> Abs(const _SimdType<float, 8>& value) { return value.Abs(); }
template <> inline _SimdType<float, 8> Min(const _SimdType<float, 8>& value1, const _SimdType<float, 8>& value2)
{
    return _SimdType<float, 8>{ simde_mm256_min_ps(
                                    value1.vec,
                                    value2.vec) };
}

template <> inline _SimdType<float, 8> Max(const _SimdType<float, 8>& value1, const _SimdType<float, 8>& value2)
{
    return _SimdType<float, 8>{ simde_mm256_max_ps(
                                    value1.vec,
                                    value2.vec) };
}

template <> inline _SimdType<float, 8> Sqrt(const _SimdType<float, 8>& value) { return _SimdType<float, 8>{ simde_mm256_sqrt_ps(value.vec) }; }

#endif


#if defined CARBON_ENABLE_AVX && CARBON_ENABLE_AVX
typedef _SimdType<float, 8> SimdType;
typedef _SimdType<int, 8> SimdTypei;
typedef _SimdType<bool, 8> SimdTypeb;
#elif defined CARBON_ENABLE_SSE && CARBON_ENABLE_SSE
typedef _SimdType<float, 4> SimdType;
typedef _SimdType<int, 4> SimdTypei;
typedef _SimdType<bool, 4> SimdTypeb;
#else
typedef _SimdType<float, 1> SimdType;
typedef _SimdType<int, 1> SimdTypei;
typedef _SimdType<bool, 1> SimdTypeb;
#endif

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

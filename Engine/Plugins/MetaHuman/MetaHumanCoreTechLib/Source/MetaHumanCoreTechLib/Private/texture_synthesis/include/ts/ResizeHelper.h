// Copyright Epic Games, Inc. All Rights Reserved.

#include <carbon/Math.h>
#include <carbon/utils/TaskThreadPool.h>

#if defined(TS_SSE_SUPPORT) && defined(_WIN32)
#define TS_SSE_SUPPORT_RESIZE
#include <immintrin.h>
#endif // TS_SSE_SUPPORT

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::ts)

//! cubic interpolation based on implementation in OpenCV
template <typename T>
inline Eigen::Vector4<T> CubicCoeffs(T t)
{
    const T A = T(-0.75);
    Eigen::Vector4<T> coeffs;
    coeffs[0] = ((A * (t + T(1)) - T(5) * A) * (t + 1) + T(8) * A) * (t + T(1)) - T(4) * A;
    coeffs[1] = ((A + T(2)) * t - (A + T(3))) * t * t + T(1);
    coeffs[2] = ((A + T(2)) * (T(1) - t) - (A + T(3))) * (T(1) - t) * (T(1) - t) + T(1);
    coeffs[3] = T(1) - coeffs[0] - coeffs[1] - coeffs[2];
    return coeffs;
}

Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> ResizeImageCubic3fSimple(
    Eigen::Ref<const Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> input,
    int rows,
    int cols,
    Eigen::Vector3f offset,
    TaskThreadPool* taskThreadPool);

#ifdef TS_SSE_SUPPORT_RESIZE
Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> ResizeImageCubic3fSSE(
    Eigen::Ref<const Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> input,
    int rows,
    int cols,
    Eigen::Vector3f offset,
    TaskThreadPool* taskThreadPool);
#endif // TS_SSE_SUPPORT_RESIZE

Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> ResizeImageCubic3f(
    Eigen::Ref<const Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> input,
    int rows,
    int cols,
    Eigen::Vector3f offset,
    TaskThreadPool* taskThreadPool);

CARBON_NAMESPACE_END(TITAN_NAMESPACE::ts)

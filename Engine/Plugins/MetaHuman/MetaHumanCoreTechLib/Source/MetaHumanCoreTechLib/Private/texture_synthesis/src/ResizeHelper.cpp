// Copyright Epic Games, Inc. All Rights Reserved.

#include <ts/ResizeHelper.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::ts)

Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> ResizeImageCubic3fSimple(
    Eigen::Ref<const Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> input,
    int rows,
    int cols,
    Eigen::Vector3f offset,
    TaskThreadPool* taskThreadPool)
{
    // filter along x direction
    Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> tmp(input.rows(), cols);
    Eigen::Vector4i indexOffset(-1, 0, 1, 2);
    auto filterX = [&](int start, int end)
    {
        for (int y = start; y < end; ++y)
        {
            for (int x = 0; x < cols; ++x)
            {
                const float fx = (float(x) + 0.5f) * float(input.cols()) / float(cols) - 0.5f;
                const int ix = (int)std::floor(fx);
                Eigen::Vector4f xCoeffs = CubicCoeffs<float>(fx - std::floor(fx));
                const Eigen::Vector4i xIndices = (Eigen::Vector4i::Constant(ix) + indexOffset).array().min((int)input.cols() - 1).max(0);

                tmp(y, x) = xCoeffs[0] * input(y, xIndices[0]) +
                            xCoeffs[1] * input(y, xIndices[1]) +
                            xCoeffs[2] * input(y, xIndices[2]) +
                            xCoeffs[3] * input(y, xIndices[3]) +
                            offset;
            }
        }
    };
    if (taskThreadPool) taskThreadPool->AddTaskRangeAndWait((int)input.rows(), filterX);
    else filterX(0, (int)input.rows());

    // filter along y direction
    Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> output(rows, cols);
    auto filterY = [&](int start, int end)
    {
        for (int y = start; y < end; ++y)
        {
            const float fy = (float(y) + 0.5f) * float(input.rows()) / float(rows) - 0.5f;
            const int iy = (int)std::floor(fy);
            Eigen::Vector4f yCoeffs = CubicCoeffs<float>(fy - std::floor(fy));
            const Eigen::Vector4i yIndices = (Eigen::Vector4i::Constant(iy) + indexOffset).array().min((int)input.rows() - 1).max(0);

            const Eigen::Vector3f* inPtr0 = tmp.row(yIndices[0]).data();
            const Eigen::Vector3f* inPtr1 = tmp.row(yIndices[1]).data();
            const Eigen::Vector3f* inPtr2 = tmp.row(yIndices[2]).data();
            const Eigen::Vector3f* inPtr3 = tmp.row(yIndices[3]).data();
            Eigen::Vector3f* outPtr = output.row(y).data();
            for (int k = 0; k < (int)tmp.cols(); ++k)
            {
                outPtr[k] = yCoeffs[0] * inPtr0[k] +
                            yCoeffs[1] * inPtr1[k] +
                            yCoeffs[2] * inPtr2[k] +
                            yCoeffs[3] * inPtr3[k];
            }
        }
    };
    if (taskThreadPool) taskThreadPool->AddTaskRangeAndWait(rows, filterY);
    else filterY(0, rows);

    return output;
}

#ifdef TS_SSE_SUPPORT_RESIZE
Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> ResizeImageCubic3fSSE(
    Eigen::Ref<const Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> input,
    int rows,
    int cols,
    Eigen::Vector3f offset,
    TaskThreadPool* taskThreadPool)
{
    // filter along x direction
    Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> tmp(input.rows(), cols);
    auto filterX = [&](int start, int end)
    {
        Eigen::Vector4i indexOffset(-1, 0, 1, 2);
        for (int y = start; y < end; ++y)
        {
            for (int x = 0; x < cols; ++x)
            {
                const float fx = (float(x) + 0.5f) * float(input.cols()) / float(cols) - 0.5f;
                const int ix = (int)std::floor(fx);
                Eigen::Vector4f xCoeffs = CubicCoeffs<float>(fx - std::floor(fx));
                const Eigen::Vector4i xIndices = (Eigen::Vector4i::Constant(ix) + indexOffset).array().min((int)input.cols() - 1).max(0);

                tmp(y, x) = xCoeffs[0] * input(y, xIndices[0]) +
                            xCoeffs[1] * input(y, xIndices[1]) +
                            xCoeffs[2] * input(y, xIndices[2]) +
                            xCoeffs[3] * input(y, xIndices[3]) +
                            offset;
            }
        }
    };
    if (taskThreadPool) taskThreadPool->AddTaskRangeAndWait((int)input.rows(), filterX, 8);
    else filterX(0, (int)input.rows());

    // filter along y direction
    Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> output(rows, cols);
    auto filterY = [&](int start, int end)
    {
        Eigen::Vector4i indexOffset(-1, 0, 1, 2);
        for (int y = start; y < end; ++y)
        {
            const float fy = (float(y) + 0.5f) * float(input.rows()) / float(rows) - 0.5f;
            const int iy = (int)std::floor(fy);
            Eigen::Vector4f yCoeffs = CubicCoeffs<float>(fy - std::floor(fy));
            Eigen::Vector4i yIndices = (Eigen::Vector4i::Constant(iy) + indexOffset).array().min((int)input.rows() - 1).max(0);

            const __m128* inPtr0 = (const __m128*)tmp.row(yIndices[0]).data();
            const __m128* inPtr1 = (const __m128*)tmp.row(yIndices[1]).data();
            const __m128* inPtr2 = (const __m128*)tmp.row(yIndices[2]).data();
            const __m128* inPtr3 = (const __m128*)tmp.row(yIndices[3]).data();
            __m128 yCoeffs0 = _mm_set1_ps(yCoeffs[0]);
            __m128 yCoeffs1 = _mm_set1_ps(yCoeffs[1]);
            __m128 yCoeffs2 = _mm_set1_ps(yCoeffs[2]);
            __m128 yCoeffs3 = _mm_set1_ps(yCoeffs[3]);
            __m128* outPtr = (__m128*)output.row(y).data();
            for (int k = 0; k < (int)tmp.cols() * 3 / 4; ++k)
            {
                outPtr[k] = _mm_add_ps(
                                _mm_add_ps(_mm_mul_ps(yCoeffs0, inPtr0[k]), _mm_mul_ps(yCoeffs1, inPtr1[k])),
                                _mm_add_ps(_mm_mul_ps(yCoeffs2, inPtr2[k]), _mm_mul_ps(yCoeffs3, inPtr3[k])));
            }
        }
    };
    if (taskThreadPool) taskThreadPool->AddTaskRangeAndWait(rows, filterY, 8);
    else filterY(0, rows);

    return output;
}
#endif // TS_SSE_SUPPORT_RESIZE

Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> ResizeImageCubic3f(
    Eigen::Ref<const Eigen::Matrix<Eigen::Vector3f, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>> input,
    int rows,
    int cols,
    Eigen::Vector3f offset,
    TaskThreadPool* taskThreadPool)
{
#ifdef TS_SSE_SUPPORT_RESIZE
    if ((cols % 4) == 0)
    {
        return ResizeImageCubic3fSSE(input, rows, cols, offset, taskThreadPool);
    }
#endif // TS_SSE_SUPPORT_RESIZE
    return ResizeImageCubic3fSimple(input, rows, cols, offset, taskThreadPool);
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::ts)

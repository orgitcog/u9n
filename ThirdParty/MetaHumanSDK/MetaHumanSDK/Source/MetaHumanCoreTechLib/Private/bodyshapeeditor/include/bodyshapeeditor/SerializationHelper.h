// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/math/Math.h>
#include <terse/archives/binary/InputArchive.h>
#include <terse/archives/binary/OutputArchive.h>
#include <trio/Stream.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

using MHCBinaryInputArchive = terse::BinaryInputArchive<trio::BoundedIOStream, /*TSize=*/std::uint32_t, /*TOffset=*/std::uint32_t, /*EByteOrder=*/terse::Endianness::Little>;
using MHCBinaryOutputArchive = terse::BinaryOutputArchive<trio::BoundedIOStream, /*TSize=*/std::uint32_t, /*TOffset=*/std::uint32_t, /*EByteOrder=*/terse::Endianness::Little>;

template <typename T, int R, int C>
void SerializeEigenMatrix(MHCBinaryOutputArchive& archive, trio::BoundedIOStream* OutputStream, const Eigen::Matrix<T, R, C>& mat)
{
    const MHCBinaryOutputArchive::SizeType rows = (MHCBinaryOutputArchive::SizeType)mat.rows();
    const MHCBinaryOutputArchive::SizeType cols = (MHCBinaryOutputArchive::SizeType)mat.cols();
    archive(rows);
    archive(cols);
#ifdef TARGET_LITTLE_ENDIAN
    if (archive.endianness() == terse::Endianness::Little)
#else
    if (archive.endianness() == terse::Endianness::Big)
#endif
    {
        OutputStream->write(reinterpret_cast<const char*>(mat.data()), sizeof(T) * rows * cols);
    }
    else
    {
        for (int r = 0; r < mat.rows(); ++r)
        {
            for (int c = 0; c < mat.cols(); ++c)
            {
                        archive(mat(r, c));
            }
        }
    }
}

template <typename T, int R, int C>
void DeserializeEigenMatrix(MHCBinaryInputArchive& archive, trio::BoundedIOStream* InputStream, Eigen::Matrix<T, R, C>& mat)
{
    MHCBinaryInputArchive::SizeType rows{};
    MHCBinaryInputArchive::SizeType cols{};
    archive(rows);
    archive(cols);
    if constexpr (R >= 0) {
        if (rows != (MHCBinaryInputArchive::SizeType)R)
        {
            CARBON_CRITICAL("Invalid row size for eigen matrix. expected {} but got {}", R, rows);
        }
    }
    if constexpr (C >= 0) {
        if (cols != (MHCBinaryInputArchive::SizeType)C)
        {
            CARBON_CRITICAL("Invalid col size for eigen matrix. expected {} but got {}", C, cols);
        }
    }
    mat.resize(rows, cols);
#ifdef TARGET_LITTLE_ENDIAN
    if (archive.endianness() == terse::Endianness::Little)
#else
    if (archive.endianness() == terse::Endianness::Big)
#endif
    {
        InputStream->read(reinterpret_cast<char*>(mat.data()), sizeof(T) * rows * cols);
    }
    else
    {
        for (int r = 0; r < mat.rows(); ++r)
        {
            for (int c = 0; c < mat.cols(); ++c)
            {
                        archive(mat(r, c));
            }
        }
    }
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

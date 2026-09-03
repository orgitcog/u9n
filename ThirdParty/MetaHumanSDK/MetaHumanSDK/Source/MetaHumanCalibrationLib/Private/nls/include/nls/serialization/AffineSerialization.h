// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/io/JsonIO.h>

#include <nls/geometry/Affine.h>
#include <nls/geometry/DiffDataAffine.h>
#include <nls/serialization/BinarySerialization.h>
#include <nls/serialization/EigenSerialization.h>


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

//! affine serialization as homogenous 4x4 matrix (see Eigen json serialization)
template <class T, int R, int C>
JsonElement ToJson(const Affine<T, R, C>& aff) { return io::ToJson(aff.Matrix()); }

template <class T, int R, int C>
JsonElement ToJson(const std::vector<Affine<T, R, C>>& aff) {
    JsonElement j(JsonElement::JsonType::Array);
    for (const auto& a : aff)
    {
        j.Append(ToJson(a));
    }

    return j;
}

//! deserializes an affine matrix from json format
template <class T, int R, int C>
void FromJson(const JsonElement& j, Affine<T, R, C>& aff)
{
    aff.SetMatrix(io::FromJson<Eigen::Matrix<T, R + 1, C + 1>>(j));
}

template <class T, int R, int C>
void FromJson(const JsonElement& j, std::vector<Affine<T, R, C>>& aff)
{
    aff.clear();
    for (auto &&affineJson : j.Array())
    {
        Affine<T, R, C> mat;
        mat.SetMatrix(io::FromJson<Eigen::Matrix<T, R + 1, C + 1>>(affineJson));
        aff.push_back(mat);
    }
}

//! Serializes an affine matrix to binary format
template <class T, int R, int C>
bool ToBinaryFile(FILE* pFile, const Affine<T, R, C>& mat)
{
    bool success = true;

    success &= io::ToBinaryFile(pFile, mat.Translation());
    success &= io::ToBinaryFile(pFile, mat.Linear());

    return success;
}

//! Deserializes an affine matrix from binary format
template <class T, int R, int C>
bool FromBinaryFile(FILE* pFile, Affine<T, R, C>& mat)
{
    bool success = true;
    Eigen::Vector<T, R> trans;
    Eigen::Matrix<T, R, C> linear;

    success &= io::FromBinaryFile(pFile, trans);
    success &= io::FromBinaryFile(pFile, linear);

    if (success == true)
    {
        mat.SetTranslation(trans);
        mat.SetLinear(linear);
    }

    return success;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

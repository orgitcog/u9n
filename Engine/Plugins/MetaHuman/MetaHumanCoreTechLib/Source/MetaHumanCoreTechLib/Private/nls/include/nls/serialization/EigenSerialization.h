// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/math/Math.h>
#include <carbon/io/JsonIO.h>

#include <stdio.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::io)

/**
 * Serializes an Eigen matrix mat to json format in column major format.
 * Vectors are serialized to an array [values...]
 * Matrices are serialized to {
 *  "rows" : rows
 *  "cols" : cols
 *  "data" : [values ...] (in column major layout)
 * }
 */
template <class T, int R, int C>
JsonElement ToJson(const Eigen::Matrix<T, R, C>& mat)
{
    if constexpr ((R == 1) || (C == 1))
    {
        // vector type is directly written as a single number array
        JsonElement j(JsonElement::JsonType::Array);
        for (int i = 0; i < int(mat.size()); i++)
        {
            j.Append(JsonElement(*(mat.data() + i)));
        }
        return j;
    }
    else
    {
        JsonElement matObject(JsonElement::JsonType::Array);
        for (int c = 0; c < int(mat.cols()); c++)
        {
            for (int r = 0; r < int(mat.rows()); r++)
            {
                matObject.Append(JsonElement(mat(r, c)));
            }
        }
        JsonElement j(JsonElement::JsonType::Object);
        j.Insert("rows", JsonElement(int(mat.rows())));
        j.Insert("cols", JsonElement(int(mat.cols())));
        j.Insert("data", std::move(matObject));
        return j;
    }
}

//! deserializes a matrix from json format (see @ToJson)
template <class T, int R, int C>
void FromJson(const JsonElement& j, Eigen::Matrix<T, R, C>& mat)
{
    if (j.IsArray())
    {
        const int size = int(j.Size());

        // we only row and colum vectors to be read directly from an array
        const bool isColumnVector = (C == 1 && (R < 0 || R == size));
        const bool isRowVector = (R == 1 && (C < 0 || C == size));
        if (isColumnVector)
        {
            mat.resize(size, 1);
        }
        else if (isRowVector)
        {
            mat.resize(1, size);
        }
        else
        {
            CARBON_CRITICAL("invalid size {} for matrix of size {}x{}", size, R, C);
        }
        for (int i = 0; i < size; i++)
        {
            *(mat.data() + i) = j[i].template Get<T>();
        }
    }
    else
    {
        int rows = j["rows"].Get<int>();
        int cols = j["cols"].Get<int>();
        if ((rows < 0) || (cols < 0))
        {
            CARBON_CRITICAL("invalid number of rows and/or columns: {}x{}", rows, cols);
        }
        if constexpr (R >= 0) { assert(R == rows); }
        if constexpr (C >= 0) { assert(C == cols); }
        mat.resize(rows, cols);

        const JsonElement& jsonObject = j["data"];
        if (!jsonObject.IsArray() || (static_cast<int>(jsonObject.Size()) != rows * cols))
        {
            CARBON_CRITICAL("object is not an array or not of the right size");
        }
        for (int c = 0; c < cols; c++)
        {
            for (int r = 0; r < rows; r++)
            {
                mat(r, c) = jsonObject[c * rows + r].template Get<T>();
            }
        }
    }
}

//! Serializes a matrix to json format.
template <class MatrixType>
MatrixType FromJson(const JsonElement& j)
{
    MatrixType mat;
    FromJson(j, mat);
    return mat;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::io)

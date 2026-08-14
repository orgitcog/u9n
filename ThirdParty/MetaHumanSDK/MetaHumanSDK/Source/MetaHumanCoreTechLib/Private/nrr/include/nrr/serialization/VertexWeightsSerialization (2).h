// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/serialization/BinarySerialization.h>
#include <nrr/VertexWeights.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

//! Serializes VertexWeights to binary format
template<class T>
bool ToBinaryFile(FILE *pFile, const VertexWeights<T> &weights)
{
    return io::ToBinaryFile(pFile, static_cast<Eigen::Matrix<T, -1, -1>>(weights.Weights()));
}

//! Deserializes VertexWeights from binary format
template<class T>
bool FromBinaryFile(FILE *pFile, VertexWeights<T> &vertexWeights)
{
    bool success = true;

    Eigen::VectorX<T> weights;
    Eigen::Matrix<T, -1, -1> weightsMat;

    success &= io::FromBinaryFile(pFile, weightsMat);

    weights = weightsMat;

    vertexWeights = VertexWeights(weights);

    return success;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <carbon/io/JsonIO.h>
#include <nls/math/Math.h>
#include <nls/serialization/BinarySerialization.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Representation of a simple configuration for snapping vertices on one mesh to corresponding vertices on another mesh
 */
template <class T>
struct SnapConfig
{
    static constexpr int32_t version = 1;
    std::string sourceMesh;
    std::vector<int> sourceVertexIndices;
    std::vector<int> targetVertexIndices;

    //! read the snap config from a json element and return true if successful, false otherwise
    bool ReadJson(const JsonElement& element);

    /**
     * Apply the snap config to 'snap' the target vertices to the same positions as the source vertices
     * @pre indices in the SnapConfig must be in range for the source and target vertices
     */ 
    void Apply(const Eigen::Matrix<T, 3, -1>& sourceVertices, Eigen::Matrix<T, 3, -1>& targetVertices) const;

    //! is the SnapConfig valid to apply for the supplied source and target vertices ie source and target vertex indices in range
    bool IsValid(const Eigen::Matrix<T, 3, -1>& sourceVertices, const Eigen::Matrix<T, 3, -1>& targetVertices) const;
    
    //! Write the SnapConfig to Json
    void WriteJson(JsonElement& json) const;
};


template <class T>
bool ToBinaryFile(FILE* pFile, const SnapConfig<T>& snapConfig)
{
    bool success = true;
    success &= io::ToBinaryFile(pFile, snapConfig.version);
    success &= io::ToBinaryFile(pFile, snapConfig.sourceMesh);
    success &= io::ToBinaryFile(pFile, snapConfig.sourceVertexIndices);
    success &= io::ToBinaryFile(pFile, snapConfig.targetVertexIndices);
    return success;
}

template <class T>
bool FromBinaryFile(FILE* pFile, SnapConfig<T>& snapConfig)
{
    bool success = true;
    int32_t version;
    success &= io::FromBinaryFile(pFile, version);
    if (success && version == 1)
    {
        success &= io::FromBinaryFile(pFile, snapConfig.sourceMesh);
        success &= io::FromBinaryFile(pFile, snapConfig.sourceVertexIndices);
        success &= io::FromBinaryFile(pFile, snapConfig.targetVertexIndices);
    }
    else
    {
        success = false;
    }
    return success;
}



CARBON_NAMESPACE_END(TITAN_NAMESPACE)

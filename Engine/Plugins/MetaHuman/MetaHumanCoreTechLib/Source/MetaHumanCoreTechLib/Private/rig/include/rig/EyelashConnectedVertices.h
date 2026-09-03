// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/geometry/Mesh.h>
#include <nls/serialization/AffineSerialization.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

// a little struct to represent eyelash connected vertices and how they should be transformed relative to the head mesh
template <class T>
struct EyelashConnectedVertices
{
    static constexpr int32_t version = 1;

    bool valid = false;
    std::vector<int> indices;
    Affine<T, 3, 3> affine;
    std::vector<int> headvIDs;

    //! initialize the eyelash mapping between the eyelashes mesh and head mesh, returning the mapping as a vector of eyelash connected vertices. Returns true if initialized successfully, false otherwise (eg if eyelashRoots indices are out of range)
    static bool InitializeEyelashMapping(const Mesh<T>& headMesh, const Mesh<T>& eyelashesMesh, const std::vector<std::pair<int, T>>& eyelashRoots, std::vector<std::shared_ptr<EyelashConnectedVertices<T>>>& eyelashConnectedVertices);

    //! apply the eyelash mapping using the supplied head and eyelash meshes and eyelash connected vertices, returning the results in updatedEyelashVertices. Will fail with a CARBON_CRITICAL if the head meshes do not contain a consistent number of verticesm or the 
    //! eyelash mesh and eyelashConnectedVertices are inconsistent with each other or with the head mesh
    static void ApplyEyelashMapping(const Mesh<T>& srcHeadMesh, const Eigen::Matrix<T, 3, -1>& targetHeadMeshVertices, const Mesh<T>& srcEyelashesMesh,
        const std::vector<std::shared_ptr<EyelashConnectedVertices<T>>>& eyelashConnectedVertices, Eigen::Matrix<T, 3, -1>& updatedEyelashVertices);

    //! reduce the number of eyelashes to segments
    static void Reduce(std::vector<std::shared_ptr<EyelashConnectedVertices<T>>>& eyelashConnectedVertices);
};


template <class T>
bool ToBinaryFile(FILE* pFile, const EyelashConnectedVertices<T>& eyelashConnectedVertices);

template <class T>
bool FromBinaryFile(FILE* pFile, EyelashConnectedVertices<T>& eyelashConnectedVertices);


CARBON_NAMESPACE_END(TITAN_NAMESPACE)

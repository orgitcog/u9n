// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <rig/EyelashConnectedVertices.h>
#include <nls/geometry/Mesh.h>
#include <carbon/io/JsonIO.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/*
* Parameters for eyelashes generation
*/
template <class T>
struct EyelashesGeneratorParams
{
    static constexpr int32_t version = 1;
    std::vector<std::pair<int, T>> eyelashesRoots;

    bool ReadJson(const JsonElement& element);
};

/**
 * A class which can generate eyelashes from an example driver and eyelashes mesh, and then apply this to an updated driver mesh to generate eyelashes for
 * that mesh. Similar in concept to a 'wrap deformer', it finds the mean translation for each eyelash root, and then applies this to each eyelash in turn.
 * Only a translation rather than a full affine transform is applied currently.
 */
template <class T>
class EyelashesGenerator
{
public:
    EyelashesGenerator() { }

    /* 
     * set the driver mesh and eyelashes mesh. Note that this does NOT 
     * re-initialize the other internals of the class and can be used as a means to (re)set the meshes if they have been stored separately from the class
     */
    void SetMeshes(const std::shared_ptr<const Mesh<T>>& driverMesh, const std::shared_ptr<const Mesh<T>>& eyelashesMesh);

    //! initialize the generator from the driver mesh, the eyelashes mesh, and parameters. Returns true if initialized successfully, false otherwise
    bool Init(const std::shared_ptr<const Mesh<T>>& driverMesh, const std::shared_ptr<const Mesh<T>>& eyelashesMesh, const EyelashesGeneratorParams<T>& params);

    //! apply the generator to the driver mesh vertices to give deformedEyelashesMeshVertices as a result; deformedDriverMeshVertices must contain the correct number of vertices
    void Apply(const Eigen::Matrix<T, 3, -1>& deformedDriverMeshVertices, Eigen::Matrix<T, 3, -1>& deformedEyelashesMeshVertices) const;

    template <class U>
    friend bool ToBinaryFile(FILE* pFile, const EyelashesGenerator<U>& eyelashesGenerator);
    template <class U>
    friend bool FromBinaryFile(FILE* pFile, EyelashesGenerator<U>& eyelashesGenerator);

private:
    static constexpr int32_t m_version = 1;
    std::shared_ptr<const Mesh<T>> m_driverMesh;
    std::shared_ptr<const Mesh<T>> m_eyelashesMesh;
    std::vector<std::shared_ptr<EyelashConnectedVertices<T>>> m_eyelashesConnectedVertices;
};

template <class T>
bool ToBinaryFile(FILE* pFile, const EyelashesGeneratorParams<T>& params);

template <class T>
bool FromBinaryFile(FILE* pFile, EyelashesGeneratorParams<T>& params);

template <class T>
bool ToBinaryFile(FILE* pFile, const EyelashesGenerator<T>& eyelashesGenerator);

template <class T>
bool FromBinaryFile(FILE* pFile, EyelashesGenerator<T>& eyelashesGenerator);

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

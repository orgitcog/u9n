// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <carbon/io/JsonIO.h>
#include <carbon/geometry/AABBTree.h>
#include <nls/math/Math.h>
#include <nls/serialization/BinarySerialization.h>
#include <nls/geometry/ClosestPointData.h>
#include <nls/geometry/Mesh.h>
#include <memory>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)


/*
* Parameters for a wrap deformer (see below)
*/
template <class T>
struct WrapDeformerParams
{
    enum class FalloffType
    {
        Volume,
        Surface // TODO Surface falloff is not currently implemented; not sure what surface distance means; perhaps geodesic distance from closest point?
    };

    static constexpr int32_t version = 2;
    bool bExclusiveBind = true;
    FalloffType falloffType = FalloffType::Volume;
    T maxDistance = std::numeric_limits<T>::max();
    T weightThreshold = 0;
    bool bAutoWeightThreshold = true;
    T normalOffset = 0; // optional parameter; if non-zero, wrapping results will be displaced by the normalOffset along the local reference frame normals, allowing a wrapped result to be displaced slightly from its driver mesh
    Eigen::VectorXi wrappedMeshVertexIndicesToApplyTo; // optional parameter; if empty, the wrap deformer will be applied to all mesh indices; if contains values, it will only be applied to the specified mesh vertices

    bool ReadJson(const JsonElement& element);
};

/**
 * An implementation of a 'wrap-deformer'. A method for deforming one 'wrapped' mesh to follow the deformations of another 'driver' mesh using 
 * local nearest point to surface geometry, defined in the local coordinate space of the mesh. 
 * This is similar to the Maya Wrap Deformer in concept and results, although we don't know exactly how that is implemented.
 * There are a couple of minor additions to the vanilla Maya implementation: i) you can offset the wrapped mesh along the driver mesh normals, 
 * ii) you can specify a subset of wrapped mesh vertices to apply the wrapping to.
 */
template <class T>
class WrapDeformer
{
public:
    WrapDeformer() { }

    /* 
     * set the driver and wrapped meshes without initializing. Note that the driver mesh must contain triangles only. Note that this does NOT 
     * re-initialize the other internals of the class and can be used as a means to (re)set the meshes if they have been stored separately from the class
     */
    void SetMeshes(const std::shared_ptr<const Mesh<T>>& driverMesh, const std::shared_ptr<const Mesh<T>>& wrappedMesh);
    
    //! initialize the deformer from parameters. The driver and wrapped meshes must have been set already.
    void Init(const WrapDeformerParams<T>& params);

    //! initialize the deformer from the driver mesh, the wrapped mesh, and parameters. Note that the driver mesh must contain triangles only, and if specified, wrappedMeshIndicesToApplyTo must be in range
    void Init(const std::shared_ptr<const Mesh<T>>& driverMesh, const std::shared_ptr<const Mesh<T>>& wrappedMesh, const WrapDeformerParams<T>& params);

    /* 
     * apply the deformer to the driver mesh vertices to give deformedWrappedMeshVertices as a result; deformedDriverMeshVertices must contain the correct number of vertices. 
     * Note that if you have specified wrappedMeshIndicesToApplyTo, if the input deformedWrappedMeshVertices contains vertices and is of the correct size, any vertices not specified in 
     * wrappedMeshIndicesToApplyTo will be set to the values of those in deformedWrappedMeshVertices
     */
    void Deform(const Eigen::Matrix<T, 3, -1>& deformedDriverMeshVertices, Eigen::Matrix<T, 3, -1>& deformedWrappedMeshVertices) const;
    
    /*
     * once initialized, get the barycentric coordinates of the closest point on the driver mesh to each point on the wrapped mesh ie the highest weighted in the wrap deformer
     */
    void GetDriverMeshClosestPointBarycentricCoordinates(std::vector<BarycentricCoordinates<T>>& driverMeshClosestPointBarycentricCoordinates) const;

    const WrapDeformerParams<T>& Params() const;

    template<class U>
    friend bool ToBinaryFile(FILE* pFile, const WrapDeformer<U>& wrapDeformer);
    template <class U>
    friend bool FromBinaryFile(FILE* pFile, WrapDeformer<U>& wrapDeformer);

private:
    static constexpr int32_t m_version = 2;
    std::shared_ptr<const Mesh<T>> m_driverMesh;
    std::shared_ptr<const Mesh<T>> m_wrappedMesh;
    std::vector<std::vector<ClosestPointData<T>>> m_driverMeshCorrespondenceClosestPointData;
    WrapDeformerParams<T> m_wrappingParams;
};

template <class T>
bool ToBinaryFile(FILE* pFile, const WrapDeformerParams<T>& params);

template <class T>
bool FromBinaryFile(FILE* pFile, WrapDeformerParams<T>& params);

template <class T>
bool ToBinaryFile(FILE* pFile, const WrapDeformer<T>& wrapDeformer);

template <class T>
bool FromBinaryFile(FILE* pFile, WrapDeformer<T>& wrapDeformer);

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

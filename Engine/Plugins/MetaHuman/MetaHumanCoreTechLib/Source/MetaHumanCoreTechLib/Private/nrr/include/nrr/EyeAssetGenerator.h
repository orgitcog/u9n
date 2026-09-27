// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/geometry/Mesh.h>
#include <nls/geometry/WrapDeformer.h>
#include <carbon/io/JsonIO.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/*
* Parameters for eyelashes generation
*/
template <class T>
struct EyeAssetGeneratorParams
{
    static constexpr int32_t version = 2;

    WrapDeformerParams<T> wrapDeformerParams;
    T eyeballNormalDisplacement = 0.01;
    T faceLockDistanceThreshold = 0.1;
    T eyeDistanceThreshold = 0.2;
    int numSolverIterations = 2;
    int numSolverCGIterations = 20;
    int leftRightSplitIndex = 0;
    bool optimizePose = true;
    Eigen::VectorXi wrapDeformOnlyVertexIndices; 
    T deformationModelVertexWeight = 1.0;
    T pointToPointConstraintWeight = 2.0;
    T eyeConstraintWeight = 5.0;
    Eigen::VectorXi caruncleVertexIndices;
    T caruncleMultiplier = 0.5;

    bool ReadJson(const JsonElement& element);
};

/**
 * A class which can generate an eye asset (either eyeshell or eyeEdge) from a head mesh, left and right eye meshes, and various parameters.
 * It uses a combination of wrapDeformers plus optimization to ensure that the eye asset does not intersect with the eye mesh and also that it
 * is close in shape to the archetype eye asset shape.
 */
template <class T>
class EyeAssetGenerator
{
public:
    EyeAssetGenerator();

    /* 
     * set the archetype (LOD0) head mesh, left and right eye meshes, asset mesh. Note that this does NOT 
     * re-initialize the other internals of the class and can be used as a means to (re)set the meshes if they have been stored separately from the class
     */
    void SetMeshes(const std::shared_ptr<const Mesh<T>>& headMesh, const std::shared_ptr<const Mesh<T>>& eyeLeftMesh, const std::shared_ptr<const Mesh<T>>& eyeRightMesh,
        const std::shared_ptr<const Mesh<T>>& eyeAssetMesh);

    //! Set a threadpool for parallelization of eye asset generation tasks (if not set, the default global threadpool will be used)
    void SetThreadPool(const std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool>& taskThreadPool);


    //! initialize the generator from the archetype (LOD0) head mesh, left and right eye meshes, asset mesh, plus parameters. headMesh and eye asset mesh must be triangulated
    void Init(const std::shared_ptr<const Mesh<T>>& headMesh, const std::shared_ptr<const Mesh<T>>& eyeLeftMesh, const std::shared_ptr<const Mesh<T>>& eyeRightMesh,
        const std::shared_ptr<const Mesh<T>>& eyeAssetMesh, const EyeAssetGeneratorParams<T>& params);

    //! apply the generator to the driver mesh vertices to give deformedEyeAssetMeshVertices as a result; deformedHeadMeshVertices, deformedEyeLeftMeshVertices, deformedEyeRightMeshVertices, deformedCartilageMeshVertices  must contain the correct number of vertices
    void Apply(const Eigen::Matrix<T, 3, -1>& deformedHeadMeshVertices, const Eigen::Matrix<T, 3, -1>& deformedEyeLeftMeshVertices,
        const Eigen::Matrix<T, 3, -1>& deformedEyeRightMeshVertices,  Eigen::Matrix<T, 3, -1>& deformedEyeAssetMeshVertices) const;

    /*
     * simple heuristic helper function which helps fix any intersections between the caruncle regions of an eyeshell and an eyeEdge asset.
     * this is not perfect but is very simple and uses simple user-defined vertex correspondence and vertex normals for the eyeshell asset to define whether the meshes are overlapping
     */
    static void FixCaruncleIntersection(const EyeAssetGenerator<T>& eyeshellAsset, const EyeAssetGenerator<T>& eyeEdgeAsset,
        Eigen::Matrix<T, 3, -1>& deformedEyeshellAssetMeshVertices, Eigen::Matrix<T, 3, -1>& deformedEyeEdgeAssetMeshVertices);

    template <class U>
    friend bool ToBinaryFile(FILE* pFile, const EyeAssetGenerator<U>& eyeAssetGenerator);
    template <class U>
    friend bool FromBinaryFile(FILE* pFile, EyeAssetGenerator<U>& eyeAssetGenerator);

private:

    void InitializeAsset();
    void ApplyAsset(const Eigen::Matrix<T, 3, -1>& deformedHeadMeshVertices, const Eigen::Matrix<T, 3, -1>& deformedEyeLeftMeshVertices, const Eigen::Matrix<T, 3, -1>& deformedEyeRightMeshVertices, 
        Eigen::Matrix<T, 3, -1>& deformedAssetVertices) const;
    bool CheckAssetVertexIndices(const Eigen::VectorXi& vertexIndices) const;

    static constexpr int32_t m_version = 1;
    std::shared_ptr<const Mesh<T>> m_headMesh;
    std::shared_ptr<const Mesh<T>> m_eyeRightMesh;
    std::shared_ptr<const Mesh<T>> m_eyeLeftMesh;
    std::shared_ptr<const Mesh<T>> m_eyeAssetMesh;

    Eigen::VectorX<T> m_eyeLeftWeights;
    Eigen::VectorX<T> m_eyeRightWeights;
    WrapDeformer<T> m_wrapDeformer;
    EyeAssetGeneratorParams<T> m_params;

    Eigen::VectorXi m_rightAssetVertexIndices;
    Eigen::Matrix<T, 3, -1> m_rightAssetVertices;
    Eigen::Matrix<int, 3, -1> m_rightAssetTriangles;
    Eigen::VectorXi m_rightFaceCorrespondenceIndices;

    Eigen::VectorXi m_leftAssetVertexIndices;
    Eigen::Matrix<T, 3, -1> m_leftAssetVertices;
    Eigen::Matrix<int, 3, -1> m_leftAssetTriangles;
    Eigen::VectorXi m_leftFaceCorrespondenceIndices;

    std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> m_taskThreadPool;

};

template <class T>
bool ToBinaryFile(FILE* pFile, const EyeAssetGeneratorParams<T>& params);

template <class T>
bool FromBinaryFile(FILE* pFile, EyeAssetGeneratorParams<T>& params);

template <class T>
bool ToBinaryFile(FILE* pFile, const EyeAssetGenerator<T>& eyeAssetGenerator);

template <class T>
bool FromBinaryFile(FILE* pFile, EyeAssetGenerator<T>& eyeAssetGenerator);

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/geometry/Affine.h>
#include <nrr/TemplateDescription.h>

namespace dna
{
    class Reader;
    class Writer;
}

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class MeshMorphModule
{
public:
    static Eigen::Matrix<T, 3, -1> Morph(const Eigen::Matrix<T, 3, -1>& sourceMeshVerticesStart,
                                         const Eigen::Matrix<T, 3, -1>& sourceMeshVerticesEnd,
                                         const Eigen::Matrix<T, 3, -1>& targetMeshVerticesStart,
                                         const VertexWeights<T>& targetVerticesMask,
                                         int gridSize = 128);
};

template <class T>
class RigMorphModule
{
public:
    // perform the volumetric morph assuming that input target meshes are in "rig" space
    static bool Morph(dna::Reader* reader,
                      dna::Writer* writer,
                      const std::map<std::string, Eigen::Matrix<T, 3, -1>>& targetVertices,
                      const std::vector<std::string>& drivingMeshNames,
                      const std::vector<std::string>& inactiveJointNames,
                      const std::map<std::string,
                                     std::vector<std::string>>& drivenJointNames,
                      const std::map<std::string, std::vector<std::string>>& dependentJointNames,
                      const std::vector<std::string>& jointsToOptimize,
                      const std::map<std::string, std::vector<std::string>>& deltaTransferMeshNames,
                      const std::map<std::string, std::vector<std::string>>& rigidTransformMeshNames,
                      const std::map<std::string, std::vector<std::string>>& uvProjectionMeshNames,
                      const VertexWeights<T>& mainMeshGridDeformMask,
                      int gridSize = 128,
                      bool inParallel = true);

    // update teeth in dna
    // @ teethMesh is assumed to be in original space
    static void UpdateTeeth(dna::Reader* reader,
                            dna::Writer* writer,
                            const Eigen::Matrix<T, 3, -1>& teethMeshVertices,
                            const std::string& teethMeshName,
                            const std::string& headMeshName,
                            const std::vector<std::string>& drivenJointNames,
                            const std::vector<std::string>& deltaTransferMeshNames,
                            const std::vector<std::string>& rigidTransformMeshNames,
                            const std::vector<std::string>& uvProjectionMeshNames,
                            const VertexWeights<T>& mouthSocketVertices,
                            int gridSize = 128,
                            bool inParallel = true);


    static std::map<std::string, std::tuple<std::string, std::vector<int>, std::vector<std::vector<T>>>> CollectDeltaTransferCorrespondences(
        dna::Reader* reader,
        const std::map<std::string, std::vector<std::string>>& deltaTransferMeshNames);

    // apply rigid transform to the internal rig
    static void ApplyRigidTransform(dna::Reader* reader, dna::Writer* writer, const Affine<T, 3, 3>& rigidTransform, bool inParallel = true);

    // apply scale to the internal rig
    static void ApplyScale(dna::Reader* reader, dna::Writer* writer, const T scale, const Eigen::Vector3<T>& scalingPivot, bool inParallel = true);
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

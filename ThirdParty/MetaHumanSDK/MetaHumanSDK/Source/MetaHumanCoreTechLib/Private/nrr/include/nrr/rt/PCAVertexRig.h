// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/math/Math.h>
#include <nls/geometry/Mesh.h>
#include <rig/RigLogicDNAResource.h>
#include <nrr/rt/HeadVertexState.h>
#include <nrr/rt/LinearEyeModel.h>
#include <nrr/rt/PCARig.h>

#include <string>

namespace dna
{

class Reader;

} // namespace dna

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::rt)

struct PCAVertexRig
{
    LinearVertexModel<float> facePCA;
    LinearVertexModel<float> teethPCA;
    LinearVertexModel<float> eyeLeftPCA;
    LinearVertexModel<float> eyeRightPCA;

    //! global root bind pose
    Eigen::Transform<float, 3, Eigen::Affine> rootBindPose;

    //! meshes of face, teeth, eye left, and eye right
    Mesh<float> meshes[4];

    Eigen::ColPivHouseholderQR<Eigen::Matrix<float, -1, -1>> solver;

    int NumCoeffs() const { return static_cast<int>(facePCA.NumPCAModes()); }

    HeadVertexState<float> EvaluatePCARig(const Eigen::VectorX<float>& pcaCoeffs) const;

    Eigen::VectorX<float> Project(const HeadVertexState<float>& headVertexState, const Eigen::VectorX<float>& coeffs) const;

    //! Translates the rig globally, moving the vertices and the bind pose
    void Translate(const Eigen::Vector3f& translation);

    //! @returns the midepoint between the eyes
    Eigen::Vector3f EyesMidpoint() const;

    void SaveAsDNA(const std::string& filename) const;
    bool LoadFromDNA(const std::string& filename);
    bool LoadFromDNA(dna::Reader* reader);
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::rt)

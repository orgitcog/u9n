// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/math/Math.h>
#include <nls/geometry/Affine.h>
#include <nls/geometry/Mesh.h>
#include <nrr/rt/HeadVertexState.h>
#include <nrr/rt/LinearEyeModel.h>
#include <nrr/LinearVertexModel.h>

namespace dna
{

class Reader;
class Writer;

} // namespace dna

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::rt)

const char* EyeLeftJointName();
const char* EyeRightJointName();
const char* FacialRootJointName();

const char* HeadMeshName();
const char* TeethMeshName();
const char* EyeLeftMeshName();
const char* EyeRightMeshName();

// disabling warning about padded structure due to use of Eigen::Vector3f which is padded
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
struct PCARig
{
    //! PCA model of the face vertices
    LinearVertexModel<float> facePCA;

    //! PCA model of the teeth vertices
    LinearVertexModel<float> teethPCA;

    //! PCA model of the parameters of the left eye (rotation, translation)
    LinearEyeModel<float> eyeLeftTransformPCA;

    //! PCA model of the parameters of the right eye (rotation, translation)
    LinearEyeModel<float> eyeRightTransformPCA;

    //! PCA model of the face vertices
    LinearVertexModel<float> neckPCA;

    //! global root bind pose (required to save the eye joints relative to the root)
    Eigen::Transform<float, 3, Eigen::Affine> rootBindPose;

    //! meshes of face, teeth, eye left, and eye right
    Mesh<float> meshes[4];

    //! offset that was applied to the pca model @see Translate()
    Eigen::Vector3f offset = Eigen::Vector3f::Zero();

    //! @returns the affine transformation that moves the pca rig back to the original position (removes the offset)
    Affine<float, 3, 3> ToOriginalPosition() const { return Affine<float, 3, 3>::FromTranslation(-offset); }

    int NumCoeffs() const { return static_cast<int>(facePCA.NumPCAModes()); }
    int NumCoeffsNeck() const { return static_cast<int>(neckPCA.NumPCAModes()); }
    HeadVertexState<float> EvaluatePCARig(const Eigen::VectorX<float>& pcaCoeffs) const;

    Eigen::Matrix3Xf EvaluatePCARigNeck(const Eigen::VectorX<float>& pcaCoeffs) const;

    Eigen::VectorX<float> Project(const HeadVertexState<float>& headVertexState, const Eigen::VectorX<float>& coeffs) const;

    Eigen::VectorX<float> ProjectNeck(const HeadVertexState<float>& headVertexState, const HeadVertexState<float>& neutralHeadVertexState, const Eigen::VectorX<float>& coeffs) const;

    //! Translates the rig globally, moving the vertices and the bind poses
    void Translate(const Eigen::Vector3f& translation);

    //! @returns the midepoint between the eyes
    Eigen::Vector3f EyesMidpoint() const;

    void SaveAsDNA(const std::string& filename) const;
    void SaveAsDNA(dna::Writer* writer) const;

    //! Save the pca model as a large npy matrix including face, teeth, and eyes
    void SaveAsNpy(const std::string& filename) const;

    bool LoadFromDNA(const std::string& filename);
    bool LoadFromDNA(dna::Reader* reader);
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

CARBON_NAMESPACE_END(TITAN_NAMESPACE::rt)

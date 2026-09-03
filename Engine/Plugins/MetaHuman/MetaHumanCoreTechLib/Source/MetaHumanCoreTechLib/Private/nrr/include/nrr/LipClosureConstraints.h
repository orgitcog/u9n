// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Pimpl.h>
#include <nls/math/Math.h>
#include <nls/geometry/BarycentricCoordinates.h>
#include <nls/geometry/Mesh.h>
#include <nls/geometry/VertexConstraints.h>
#include <nls/utils/Configuration.h>
#include <nrr/landmarks/LipClosure.h>

#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
struct LipClosureConstraintsData
{
    std::vector<int> srcIDs;
    std::vector<BarycentricCoordinates<T, 3>> targetBCs;
    std::vector<Eigen::Vector3<T>> normals;
    std::vector<T> weights;

    void Clear()
    {
        srcIDs.clear();
        targetBCs.clear();
        normals.clear();
        weights.clear();
    }

    int NumConstraints() const { return static_cast<int>(srcIDs.size()); }
};

/**
 * Class to evaluate collision constraints
 */
template <class T>
class LipClosureConstraints
{
public:
    static constexpr const char* ConfigName() { return "Lip Closure Constraints"; }

public:
    LipClosureConstraints();
    ~LipClosureConstraints();
    LipClosureConstraints(LipClosureConstraints&&);
    LipClosureConstraints(LipClosureConstraints&) = delete;
    LipClosureConstraints& operator=(LipClosureConstraints&&);
    LipClosureConstraints& operator=(const LipClosureConstraints&) = delete;

    //! Set the topology with mask for lower and upper lips
    void SetTopology(const Mesh<T>& mesh,
                     const std::vector<int>& upperLipMask,
                     const std::vector<std::vector<int>>& upperContourLines,
                     const std::vector<int>& lowerLipMask,
                     const std::vector<std::vector<int>>& lowerContourLines,
                     TITAN_NAMESPACE::TaskThreadPool* taskThreadPool = nullptr);

    //! Set the lip closure data (i.e. determine lip closure from landmarks)
    void SetLipClosure(const LipClosure3D<T>& lipClosure3D);

    //! Reset the lip closure data
    void ResetLipClosure();

    //! @returns True if the lip closure constraint can be calculated using lip closure landmarks
    bool ValidLipClosure() const;

    //! Calculate lip closure constraints.
    void CalculateLipClosureData(const Eigen::Matrix<T, 3, -1>& vertices,
                                 const Eigen::Matrix<T, 3, -1>& normals,
                                 const Eigen::Transform<T, 3, Eigen::Affine>& toCamerasTransform,
                                 bool useLandmarks,
                                 const Eigen::Transform<T, 3, Eigen::Affine>& toFaceTransform,
                                 TITAN_NAMESPACE::TaskThreadPool* taskThreadPool = nullptr);

    //! Evaluate simple lip closure constraints.
    void EvaluateLipClosure(const Eigen::Matrix<T, 3, -1>& vertices, VertexConstraints<T, 3, 1>& lipClosureVertexConstraints) const;
    void EvaluateLipClosure(const Eigen::Matrix<T, 3, -1>& vertices, VertexConstraints<T, 3, 4>& lipClosureVertexConstraints) const;

    //! Retrieve current closure
    void GetLipClosureData(LipClosureConstraintsData<T>& lipClosureConstraintsData) const;

    /* */Configuration& Config()/* */;
    const Configuration& Config() const;

private:
    struct Private;
    TITAN_NAMESPACE::Pimpl<Private> m;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

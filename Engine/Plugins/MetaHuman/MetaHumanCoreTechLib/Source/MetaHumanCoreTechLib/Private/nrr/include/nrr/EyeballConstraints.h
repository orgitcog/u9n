// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Pimpl.h>
#include <nls/DiffDataMatrix.h>
#include <nls/geometry/Mesh.h>
#include <nls/geometry/VertexConstraints.h>
#include <nls/utils/ConfigurationParameter.h>
#include <nrr/VertexWeights.h>


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Class to evaluate constraints relative to the eyeballs
 */
template <class T>
class EyeballConstraints
{
public:
    struct EyeballMatch
    {
        int64_t tID; //! The triangle id on the eyeball match
        Eigen::Vector3<T> eyeballPosition; //! The vertex position on the eyeball
        Eigen::Vector3<T> eyeballNormal; //! The normal on the eyeball
        Eigen::Vector3<T> dir; //! The direction pointing outside the eyeball
        T dist; //! The distance between the query and the eyeball position. Query is inside the eyeball if negative.
    };

public:
    EyeballConstraints();
    ~EyeballConstraints();
    EyeballConstraints(EyeballConstraints&&);
    EyeballConstraints(EyeballConstraints&) = delete;
    EyeballConstraints& operator=(EyeballConstraints&&);
    EyeballConstraints& operator=(const EyeballConstraints&) = delete;

    const Configuration& GetConfiguration() const;

    void SetConfiguration(const Configuration& config);

    //! Set the eyeball mesh.
    void SetEyeballMesh(const Mesh<T>& eyeballMesh);

    [[deprecated("deprecated, use SetInterfaceVertices() instead")]]
    void SetTargetVertexWeights(const VertexWeights<T>& interfaceVertexWeights) { SetInterfaceVertices(interfaceVertexWeights); }

    //! Set the vertices that are influenced by the eyeball.
    void SetInfluenceVertices(const VertexWeights<T>& influenceVertexWeights);
    const VertexWeights<T>& InfluenceVertices() const;

    //! Set the vertices that lie on the eyeball.
    void SetInterfaceVertices(const VertexWeights<T>& interfaceVertexWeights);
    const VertexWeights<T>& InterfaceVertices() const;

    //! Calculate for each vertex the distance to the eyeball
    std::vector<T> GetEyeballDistances(const Eigen::Matrix<T, 3, -1>& vertices) const;

    //! Calculate for each vertex the distance to the eyeball
    std::vector<T> GetEyeballDistances(const Eigen::Matrix<T, 3, -1>& vertices, const std::vector<int>& vIDs) const;

    //! Set the eyeball rest pose. This is only required for influence vertices.
    void SetRestPose(const Eigen::Matrix<T, 3, -1>& eyeballVertices, const Eigen::Matrix<T, 3, -1>& targetVertices);

    //! Set the current eyeball pose.
    void SetEyeballPose(const Eigen::Matrix<T, 3, -1>& eyeballVertices);

    //! Evaluate the eyeball constraints.
    DiffData<T> EvaluateEyeballConstraints(const DiffDataMatrix<T, 3, -1>& eyeballVertices) const;

    //! Setup the eyeball constraints
    void SetupEyeballConstraints(const Eigen::Matrix<T, 3, -1>& vertices, VertexConstraints<T, 1, 1>& vertexConstraints) const;

    //! Setup the eyeball interface constraints
    void SetupEyeballInterfaceConstraints(const Eigen::Matrix<T, 3, -1>& vertices,
                                          const VertexWeights<T>& interfaceVertexWeights,
                                          T eyeballWeight,
                                          VertexConstraints<T, 1, 1>& vertexConstraints) const;

    //! Setup the eyeball influence constraints
    void SetupEyeballInfluenceConstraints(const Eigen::Matrix<T, 3, -1>& vertices,
                                          const VertexWeights<T>& influenceVertexWeights,
                                          const std::vector<T>& influenceEyeballDistances,
                                          T eyeballWeight,
                                          VertexConstraints<T, 1, 1>& vertexConstraints) const;

    //! Project the vertices that are interface vertices onto the eyeball, the influence vertices are projected to the reference distance
    Eigen::Matrix<T, 3, -1> Project(const Eigen::Matrix<T, 3, -1>& vertices) const;

    //! Project the vertices onto the eyeball
    Eigen::Matrix<T, 3, -1> ProjectOntoEyeball(const Eigen::Matrix<T, 3, -1>& vertices) const;

    //! Project the vertices onto the eyeball
    Eigen::Matrix<T, 3, -1> ProjectOntoEyeball(const Eigen::Matrix<T, 3, -1>& vertices, const std::vector<int>& vIDs) const;

    //! Set the distances of the influence vertices to the eyeball
    void SetInfluenceEyeballDistances(const std::vector<T>& distances);

    //! @return the distances of the influence vertices to the eyeball
    const std::vector<T>& InfluenceEyeballDistances() const;

    /**
     * @brief Get the closest eyeball position to @p vertex.
     *
     * @param vertex The query vertex.
     * @param useClosestEyeballPosition   Whether to use the true closest point, otherwise the correspondence is found by
     *                                    intersecting the ray from the eyeball center to the vertex with the eyeball mesh.
     * @return EyeballMatch  The found correspondence.
     */
    EyeballMatch GetClosestEyeballPosition(const Eigen::Vector3<T>& vertex, bool useClosestEyeballPosition) const;

private:
    struct Private;
    TITAN_NAMESPACE::Pimpl<Private> m;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

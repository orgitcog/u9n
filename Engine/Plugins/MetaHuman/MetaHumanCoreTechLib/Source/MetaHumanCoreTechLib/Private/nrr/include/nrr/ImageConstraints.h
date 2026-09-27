// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/common/Pimpl.h>
#include <nls/Cost.h>
#include <nls/DiffDataMatrix.h>
#include <nls/geometry/Camera.h>
#include <nls/geometry/VertexConstraints.h>
#include <nrr/VertexWeights.h>

#include <map>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class ImageConstraintsData
{
public:
    Camera<T> camera;
    Eigen::Matrix<T, 2, -1> targetPositions;
    Eigen::Matrix<T, 2, -1> targetNormals;
    Eigen::VectorX<T> weights;
    Eigen::VectorXi vertexIndices;
};

/**
 * Class to evaluate flow constraints (2D point2point)
 */
template <class T>
class ImageConstraints
{
public:
    ImageConstraints();
    ~ImageConstraints();
    ImageConstraints(ImageConstraints&& other);
    ImageConstraints(const ImageConstraints& other) = delete;
    ImageConstraints& operator=(ImageConstraints&& other);
    ImageConstraints& operator=(const ImageConstraints& other) = delete;

    //! Set Image constraints data
    void SetImageConstraintsData(const std::map<std::string, std::shared_ptr<const ImageConstraintsData<T>>>& imageConstraintsData);

    //! Set the weight for image constraints
    void SetWeight(T weight);

    Cost<T> Evaluate(const DiffDataMatrix<T, 3, -1>& vertices,
                     std::map<std::string, std::shared_ptr<const ImageConstraintsData<T>>>* debugImageConstraints = nullptr);

    void SetupImageConstraints(const Eigen::Transform<T, 3, Eigen::Affine>& rigidTransform,
                               const Eigen::Matrix<T, 3, -1>& vertices,
                               VertexConstraints<T, 1, 1>& flowConstraints) const;

private:
    struct Private;
    TITAN_NAMESPACE::Pimpl<Private> m;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

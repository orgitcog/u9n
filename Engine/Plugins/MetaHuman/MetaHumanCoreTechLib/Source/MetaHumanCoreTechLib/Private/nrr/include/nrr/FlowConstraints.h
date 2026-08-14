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
class FlowConstraintsData
{
public:
    Camera<T> camera;
    Eigen::Matrix<T, 2, -1> targetPositions;
    Eigen::VectorX<T> weights;
    Eigen::VectorXi vertexIndices;
};

/**
 * Class to evaluate flow constraints (2D point2point)
 */
template <class T>
class FlowConstraints
{
public:
    FlowConstraints();
    ~FlowConstraints();
    FlowConstraints(FlowConstraints&& other);
    FlowConstraints(const FlowConstraints& other) = delete;
    FlowConstraints& operator=(FlowConstraints&& other);
    FlowConstraints& operator=(const FlowConstraints& other) = delete;

    //! Set FlowData
    void SetFlowData(const std::map<std::string, std::shared_ptr<const FlowConstraintsData<T>>>& flowConstraintsData);

    //! Set the flow weight
    void SetFlowWeight(T weight);
    T FlowWeight() const;

    Cost<T> Evaluate(const DiffDataMatrix<T, 3, -1>& vertices,
                     std::map<std::string, std::shared_ptr<const FlowConstraintsData<T>>>* debugFlowConstraints = nullptr);

    void SetupFlowConstraints(const Eigen::Transform<T, 3, Eigen::Affine>& rigidTransform,
                              const Eigen::Matrix<T, 3, -1>& vertices,
                              VertexConstraints<T, 2, 1>& flowConstraints) const;

    const std::map<std::string, std::shared_ptr<const FlowConstraintsData<T>>>& FlowData() const;

private:
    struct Private;
    TITAN_NAMESPACE::Pimpl<Private> m;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

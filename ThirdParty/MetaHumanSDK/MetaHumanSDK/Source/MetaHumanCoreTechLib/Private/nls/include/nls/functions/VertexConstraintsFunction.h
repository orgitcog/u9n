// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/DiffDataMatrix.h>
#include <nls/geometry/VertexConstraints.h>
#include <carbon/utils/Timer.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Applies the vertex constraints @p vertexConstraints to @p vertices. VertexConstraints store the
 * constraints as constraint = residual + Jacobian * dx. Hence we need to pass @p baseVertices
 * so that the correct residual for the vertex constraints can be calculated.
 */
template <class T, int ResidualSize, int NumConstraintVertices>
DiffData<T> ApplyVertexConstraints(const DiffDataMatrix<T, 3, -1>& vertices,
                                   const Eigen::Matrix<T, 3, -1>& baseVertices,
                                   const VertexConstraints<T,
                                                           ResidualSize,
                                                           NumConstraintVertices>& vertexConstraints)
{
    Vector<T> output = vertexConstraints.EvaluateResidual(vertices.Matrix(), baseVertices);

    JacobianConstPtr<T> jacobian;
    if (vertices.HasJacobian())
    {
        jacobian = vertices.Jacobian().Premultiply(vertexConstraints.SparseJacobian(vertices.Cols()));
    }
    return DiffData<T>(std::move(output), jacobian);
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

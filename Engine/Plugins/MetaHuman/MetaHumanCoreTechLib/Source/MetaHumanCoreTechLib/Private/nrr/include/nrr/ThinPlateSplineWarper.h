// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/math/Math.h>


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)


/**
 * Thin Plate Spline Warper class, templated on both scalar type and dimensionality of the input data
 *  https://user.engineering.uiowa.edu/~aip/papers/bookstein-89.pdf 
 */
template <typename T, int D = 3>
class ThinPlateSplineWarper
{
public:
    /*
    * Construct the warper from source points (D x n) and target points (D x n)
    */ 
    ThinPlateSplineWarper(const Eigen::Matrix<T, D, -1>& sourcePoints, const Eigen::Matrix<T, D, -1>& targetPoints);

    //! Warp a set of points
    Eigen::Matrix<T, D, -1> Apply(const Eigen::Matrix<T, D, -1>& points) const;

private:
    Eigen::Matrix<T, D, -1> m_sourcePoints;
    Eigen::Matrix<T, D, -1> m_targetPoints;
    Eigen::Matrix<T, -1, -1> m_weights; 

    void ComputeWeights();
    Eigen::Matrix<T, -1, -1> ComputeKernel(const Eigen::Matrix<T, D, -1>& A, const Eigen::Matrix<T, D, -1>& B) const;
};


CARBON_NAMESPACE_END(TITAN_NAMESPACE)

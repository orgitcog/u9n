// Copyright Epic Games, Inc. All Rights Reserved.

#include <nrr/ThinPlateSplineWarper.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T, int D>
ThinPlateSplineWarper<T, D>::ThinPlateSplineWarper(const Eigen::Matrix<T, D, -1>& sourcePoints, const Eigen::Matrix<T, D, -1>& targetPoints)
    : m_sourcePoints(sourcePoints)
    , m_targetPoints(targetPoints)
{
    if (sourcePoints.cols() != targetPoints.cols() || sourcePoints.rows() != D || targetPoints.rows() != D)
    {
        CARBON_CRITICAL("Source and target points must be {} contain the same number of points.", D);
    }

    ComputeWeights();
}

template <class T, int D>
Eigen::Matrix<T, D, -1> ThinPlateSplineWarper<T, D>::Apply(const Eigen::Matrix<T, D, -1>& points) const
{
    const int numPoints = int(points.cols());

    // compute kernel matrix U for the input points relative to the source points
    Eigen::Matrix<T, -1, -1> U = ComputeKernel(points, m_sourcePoints);

    // apply the warping
    Eigen::Matrix<T, D, -1> warpedPoints(D, numPoints);
    for (int i = 0; i < D; ++i)
    {
        warpedPoints.row(i) = U * m_weights.col(i).head(m_sourcePoints.cols()) + points.transpose() * m_weights.col(i).segment(m_sourcePoints.cols(), D) + Vector<T>::Ones(numPoints) * m_weights.col(i)(m_sourcePoints.cols() + D);
    }

    return warpedPoints;
}

template <class T, int D>
void ThinPlateSplineWarper<T,D>::ComputeWeights()
{
    // compute the TPS weights
    
    // calculate K matrix (kernel function applied to pairwise distances)
    Eigen::Matrix<T, -1, -1> K = ComputeKernel(m_sourcePoints, m_sourcePoints);

    // P matrix (affine component)
    Eigen::Matrix<T, -1, -1> P(m_sourcePoints.cols(), D + 1);
    P.block(0, 0, m_sourcePoints.cols(), D) = m_sourcePoints.transpose();
    P.col(D) = Vector<T>::Ones(m_sourcePoints.cols());

    // L matrix
    Eigen::Matrix<T, -1, -1> L(m_sourcePoints.cols() + D + 1, m_sourcePoints.cols() + D + 1);
    L.setZero();
    L.block(0, 0, m_sourcePoints.cols(), m_sourcePoints.cols()) = K;
    L.block(0, m_sourcePoints.cols(), m_sourcePoints.cols(), D + 1) = P;
    L.block(m_sourcePoints.cols(), 0, D + 1, m_sourcePoints.cols()) = P.transpose();

    // Y matrix
    Eigen::Matrix<T, -1, -1> Y(m_sourcePoints.cols() + D + 1, D);
    Y.setZero();
    Y.block(0, 0, m_sourcePoints.cols(), D) = m_targetPoints.transpose();

    // solve linear equation to get the weights
    m_weights = L.fullPivLu().solve(Y);
}

template <class T, int D>
Eigen::Matrix<T, -1, -1> ThinPlateSplineWarper<T, D>::ComputeKernel(const Eigen::Matrix<T, D, -1>& A, const Eigen::Matrix<T, D, -1>& B) const
{
    // compute pairwise distance kernel matrix U(r) = r^2 * log(r^2) 
    Eigen::Matrix<T, -1, -1> U(int(A.cols()), int(B.cols()));

    for (int i = 0; i < int(A.cols()); ++i)
    {
        for (int j = 0; j < int(B.cols()); ++j)
        {
            T r2 = (A.col(i) - B.col(j)).squaredNorm();
            U(i, j) = (r2 > T(1e-10)) ? r2 * std::log(r2) : T(0); // handle the case near 0 to avoid log(0)
        }
    }
    return U;
}

template class ThinPlateSplineWarper<float, 2>;
template class ThinPlateSplineWarper<double, 2>;

template class ThinPlateSplineWarper<float, 3>;
template class ThinPlateSplineWarper<double, 3>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

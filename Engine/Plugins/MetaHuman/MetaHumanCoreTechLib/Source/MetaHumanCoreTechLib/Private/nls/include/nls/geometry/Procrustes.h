// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/math/Math.h>
#include <nls/geometry/Affine.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Procrustes Analsysis
 * see https://en.wikipedia.org/wiki/Procrustes_analysis, https://en.wikipedia.org/wiki/Orthogonal_Procrustes_problem
 */
template <class T, int C>
class Procrustes
{
public:
    /**
     * Calculate optimal rigid transformation to align src and target: min_affine || target - affine * src ||
     */
    static Affine<T, C, C> AlignRigid(const Eigen::Matrix<T, C, -1>& src, const Eigen::Matrix<T, C, -1>& target, bool withRotation = true)
    {
        Eigen::Vector<T, C> meanSrc = src.rowwise().mean();
        Eigen::Vector<T, C> meanTarget = target.rowwise().mean();
        if (withRotation)
        {
            Eigen::Matrix<T, C, C> M = (target.colwise() - meanTarget) * (src.colwise() - meanSrc).transpose();
            const Eigen::JacobiSVD<Eigen::Matrix<T, C, C>> svd(M, Eigen::ComputeFullU | Eigen::ComputeFullV);
            Eigen::Matrix<T, C, C> R;
            if (svd.matrixU().determinant() * svd.matrixV().determinant() < 0)
            {
                Eigen::DiagonalMatrix<T, C> diag;
                diag.setIdentity();
                diag.diagonal()[C - 1] = T(-1);
                R = svd.matrixU() * diag * svd.matrixV().transpose();
            }
            else
            {
                R = svd.matrixU() * svd.matrixV().transpose();
            }
            Affine<T, C, C> aff;
            aff.SetLinear(R);
            aff.SetTranslation(meanTarget - R * meanSrc);
            return aff;
        }
        else
        {
            return Affine<T, C, C>::FromTranslation(meanTarget - meanSrc);
        }
    }

    /**
     * Calculate optimal rigid transformation to align src and target: min_affine || (target - affine * src) * weight ||
     */
    static Affine<T, C, C> AlignRigidWeighted(const Eigen::Matrix<T, C, -1>& src,
        const Eigen::Matrix<T, C, -1>& target,
        const Eigen::VectorX<T>& weights,
        bool withRotation = true) {
        Eigen::Matrix<T, C, -1> wSrc = src.array().rowwise() * weights.transpose().array();
        Eigen::Matrix<T, C, -1> wTgt = target.array().rowwise() * weights.transpose().array();
        Eigen::Vector<T, C> meanSrc = wSrc.rowwise().sum() / weights.sum();
        Eigen::Vector<T, C> meanTarget = wTgt.rowwise().sum() / weights.sum();
        if (withRotation)
        {
            Eigen::Matrix<T, C, C> M = ((target.colwise() - meanTarget).array().rowwise() * weights.transpose().array()).matrix() * ((src.colwise() - meanSrc).array().rowwise() * weights.transpose().array()).matrix().transpose();
            const Eigen::JacobiSVD<Eigen::Matrix<T, C, C>> svd(M, Eigen::ComputeFullU | Eigen::ComputeFullV);
            Eigen::Matrix<T, C, C> R;
            if (svd.matrixU().determinant() * svd.matrixV().determinant() < 0) {
                Eigen::DiagonalMatrix<T, C> diag;
                diag.setIdentity();
                diag.diagonal()[C - 1] = T(-1);
                R = svd.matrixU() * diag * svd.matrixV().transpose();
            }
            else {
                R = svd.matrixU() * svd.matrixV().transpose();
            }
            Affine<T, C, C> aff;
            aff.SetLinear(R);
            aff.SetTranslation(meanTarget - R * meanSrc);
            return aff;
        }
        else
        {
            return Affine<T, C, C>::FromTranslation(meanTarget - meanSrc);
        }
    }

    /**
     * Calculate optimal rigid transformation and scale to align src and target: min_affine || target - affine * scale * src ||
     */
    static std::pair<T, Affine<T, C, C>> AlignRigidAndScale(const Eigen::Matrix<T, C, -1>& src, const Eigen::Matrix<T, C, -1>& target, bool withRotation = true)
    {
        Eigen::Vector<T, C> meanSrc = src.rowwise().mean();
        Eigen::Vector<T, C> meanTarget = target.rowwise().mean();
        Eigen::Matrix<T, C, -1> movedSrc = (src.colwise() - meanSrc);
        Eigen::Matrix<T, C, -1> movedTarget = (target.colwise() - meanTarget);
        const T scaleSrc = sqrt(movedSrc.squaredNorm() / T(movedSrc.cols()));
        const T scaleTarget = sqrt(movedTarget.squaredNorm() / T(movedTarget.cols()));
        const T scale = scaleTarget / scaleSrc;
        if (withRotation)
        {
            movedSrc /= scaleSrc;
            movedTarget /= scaleTarget;
            Eigen::Matrix<T, C, C> M = movedTarget * movedSrc.transpose();
            const Eigen::JacobiSVD<Eigen::Matrix<T, C, C>> svd(M, Eigen::ComputeFullU | Eigen::ComputeFullV);
            Eigen::Matrix<T, C, C> R;
            if (svd.matrixU().determinant() * svd.matrixV().determinant() < 0)
            {
                Eigen::DiagonalMatrix<T, C> diag;
                diag.setIdentity();
                diag.diagonal()[C - 1] = T(-1);
                R = svd.matrixU() * diag * svd.matrixV().transpose();
            }
            else
            {
                R = svd.matrixU() * svd.matrixV().transpose();
            }
            Affine<T, C, C> aff;
            aff.SetLinear(R);
            aff.SetTranslation(meanTarget - R * scale * meanSrc);
            return std::pair<T, Affine<T, C, C>>(scale, aff);
        }
        else
        {
            return std::pair<T, Affine<T, C, C>>(scale, Affine<T, C, C>::FromTranslation(meanTarget - scale * meanSrc));
        }
    }

private:
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

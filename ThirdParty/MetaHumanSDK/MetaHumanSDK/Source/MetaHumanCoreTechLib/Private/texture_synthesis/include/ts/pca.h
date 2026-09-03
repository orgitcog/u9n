// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Math.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::ts)

template <typename MatrixType>
class PCA
{
    typedef typename MatrixType::Scalar Scalar;

    public:
        PCA() {}
        PCA(const Eigen::VectorX<Scalar>& mu, const MatrixType& S, const MatrixType& T)
            : m_mu(mu)
            , m_S(S)
            , m_T(T)
        {}
        PCA(Eigen::VectorX<Scalar>&& mu, MatrixType&& S, MatrixType&& T)
            : m_mu(std::move(mu))
            , m_S(std::move(S))
            , m_T(std::move(T))
        {}

        int Size() const { return (int)m_mu.size(); }
        int NumCoeffs() const { return (int)m_T.cols(); }

        const Eigen::VectorX<Scalar>& Mu() const { return m_mu; }
        const MatrixType& S() const { return m_S; }
        const MatrixType& T() const { return m_T; }

        Eigen::VectorX<Scalar> Reconstruct(const Eigen::VectorX<Scalar>& v) const { return Mu() + T() * v; }

    private:
        Eigen::VectorX<Scalar> m_mu;
        MatrixType m_S;
        MatrixType m_T;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::ts)
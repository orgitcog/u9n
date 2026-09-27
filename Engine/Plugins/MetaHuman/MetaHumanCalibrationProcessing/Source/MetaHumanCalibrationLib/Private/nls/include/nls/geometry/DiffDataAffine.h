// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/DiffDataMatrix.h>
#include <nls/functions/ColwiseAddFunction.h>
#include <nls/functions/MatrixMultiplyFunction.h>
#include <nls/functions/AddFunction.h>
#include <nls/geometry/Affine.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Convenience function for differentiable affine transformation Ax + b
 */
template <class T, int R, int C, typename DISCARD = typename std::enable_if<(R > 0 && C > 0), void>::type>
class DiffDataAffine
{
public:
    using ScalarType = T;
    using MatrixType = Eigen::Matrix<T, R + 1, C + 1>;

public:
    DiffDataAffine()
        : m_A(Eigen::Matrix<T, R, C>::Identity())
        , m_b(Eigen::Vector<T, R>::Zero())
    {}

    DiffDataAffine(const Eigen::Matrix<T, R + 1, C + 1>& mat)
        : m_A(mat.template topLeftCorner<R, C>())
        , m_b(mat.template topRightCorner<R, 1>())
    {}

    DiffDataAffine(const DiffDataMatrix<T, R, C>& A, const DiffDataMatrix<T, R, 1>& b)
        : m_A(A)
        , m_b(b)
    {}

    DiffDataAffine(DiffDataMatrix<T, R, C>&& A, DiffDataMatrix<T, R, 1>&& b)
        : m_A(std::move(A))
        , m_b(std::move(b))
    {}

    DiffDataAffine(const Affine<T, R, C>& aff)
        : DiffDataAffine(aff.Matrix())
    {}

    DiffDataAffine(const DiffDataAffine& aff) = delete;
    DiffDataAffine(DiffDataAffine&& aff) = default;
    DiffDataAffine& operator=(const DiffDataAffine& aff) = delete;
    DiffDataAffine& operator=(DiffDataAffine&& aff) = default;

    //! explicitly clone the diff data affine object as copy constructor is disabled (implementation may change in the future)
    DiffDataAffine Clone() const { return DiffDataAffine(m_A.Clone(), m_b.Clone()); }


    //! Multiplies two affine transformations A1 (A2 x + b2) + b1 = (A1 A2) x + (A1 b2 + b2)
    DiffDataAffine Multiply(const DiffDataAffine& other) const
    {
        MatrixMultiplyFunction<T> multiplyFunction;

        // (A1 A2)
        DiffDataMatrix<T, R, C> newA = multiplyFunction.DenseMatrixMatrixMultiply(m_A, other.m_A);
        // (A1 b2 + b2)
        DiffDataMatrix<T, R, 1> newb = multiplyFunction.DenseMatrixMatrixMultiply(m_A, other.m_b) + m_b;
        return DiffDataAffine(std::move(newA), std::move(newb));
    }

    DiffDataAffine operator*(const DiffDataAffine& aff) const
    {
        return Multiply(aff);
    }

    static DiffDataAffine FromTranslation(const DiffDataMatrix<T, R, 1>& translation)
    {
        DiffDataMatrix<T, R, C> linear(Eigen::Matrix<T, R, C>::Identity());
        DiffDataMatrix<T, R, 1> translationMatrix(R, 1, DiffData<T>(std::move(translation.Value()), translation.JacobianPtr()));

        return DiffDataAffine(std::move(linear), std::move(translationMatrix));
    }

    //! Applies the affine transformation to some input data y = Ax + b
    DiffDataMatrix<T, R, -1> Transform(const DiffDataMatrix<T, C, -1>& matX) const
    {
        MatrixMultiplyFunction<T> multiplyFunction;
        ColwiseAddFunction<T> colwiseAddFunction;
        // Ax
        DiffDataMatrix<T, R, -1> matY = multiplyFunction.DenseMatrixMatrixMultiply(m_A, matX);
        // + b
        return colwiseAddFunction.colwiseAddFunction(matY, m_b);
    }

    //! Returns the full "homogeneous" matrix
    Eigen::Matrix<T, R + 1, C + 1> Matrix() const
    {
        Eigen::Matrix<T, R + 1, C + 1> mat;
        mat.template topLeftCorner<R, C>() = m_A.Matrix();
        mat.template topRightCorner<R, 1>() = m_b.Matrix();
        mat.bottomRows(1).setZero();
        mat(R, C) = T(1.0);
        return mat;
    }

    TITAN_NAMESPACE::Affine<T, R, C> Affine() const
    {
        TITAN_NAMESPACE::Affine<T, R, C> aff;
        aff.SetLinear(m_A.Matrix());
        aff.SetTranslation(m_b.Matrix());

        return aff;
    }

    bool HasJacobian() const
    {
        return m_A.HasJacobian() || m_b.HasJacobian();
    }

    const DiffDataMatrix<T, R, C>& Linear() const { return m_A; }
    /* */DiffDataMatrix<T, R, C>& Linear()/* */{ return m_A; }
    const DiffDataMatrix<T, R, 1>& Translation() const { return m_b; }
    /* */DiffDataMatrix<T, R, 1>& Translation()/* */{ return m_b; }

private:
    DiffDataMatrix<T, R, C> m_A;
    DiffDataMatrix<T, R, 1> m_b;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

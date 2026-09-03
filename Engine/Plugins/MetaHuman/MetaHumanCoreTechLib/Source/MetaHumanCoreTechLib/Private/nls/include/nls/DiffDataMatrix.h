// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/DiffData.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class MatrixMultiplyFunction;

/**
 * Differentiable data but with f(x) being a matrix and not a vector.
 * @see DiffData
 */
template <class T, int R = -1, int C = -1>
class DiffDataMatrix : public DiffData<T>
{
public:
    template <bool TMP = true>
    DiffDataMatrix(DiffData<T>&& o, typename std::enable_if<TMP && ((R >= 0) && (C >= 0))>::type* = 0)
        : DiffData<T>(std::move(o))
        , m_rows(R)
        , m_cols(C)
    {
        CARBON_PRECONDITION(m_rows >= 0, "invalid row size");
        CARBON_PRECONDITION(m_cols >= 0, "invalid column size");
        CARBON_PRECONDITION(m_rows * m_cols == int(this->Value().size()), "the number of rows and columns need to match the DiffData size");
    }

    DiffDataMatrix(int rows, int cols, DiffData<T>&& o)
        : DiffData<T>(std::move(o))
        , m_rows(rows)
        , m_cols(cols)
    {
        CARBON_PRECONDITION(m_rows >= 0 && (m_rows == R || R < 0), "invalid row size");
        CARBON_PRECONDITION(m_cols >= 0 && (m_cols == C || C < 0), "invalid column size");
        CARBON_PRECONDITION(m_rows * m_cols == int(this->Value().size()), "the number of rows and columns need to match the DiffData size");
    }

    template <bool TMP = true>
    DiffDataMatrix(const Eigen::Matrix<T, R, C>& mat,
                   const JacobianConstPtr<T>& jacobian = nullptr,
                   typename std::enable_if<TMP && ((R >= 0) && (C >= 0))>::type* = 0)
        : DiffDataMatrix(DiffData<T>(mat.data(), int(mat.size()), jacobian)) {}

    template <bool TMP = true>
    DiffDataMatrix(const Eigen::Matrix<T, R, C>& mat,
                   const JacobianConstPtr<T>& jacobian = nullptr,
                   typename std::enable_if<TMP && !((R >= 0) && (C >= 0))>::type* = 0)
        : DiffDataMatrix(int(mat.rows()), int(mat.cols()), DiffData<T>(mat.data(), int(mat.size()), jacobian)) {}

    int Rows() const { return m_rows; }
    int Cols() const { return m_cols; }

    //! Convenience function returning the value data as a matrix
    Eigen::Map<const Eigen::Matrix<T, R, C>> Matrix() const
    {
        return Eigen::Map<const Eigen::Matrix<T, R, C>>(this->Value().data(), Rows(), Cols());
    }

    /**
     * Convenience function returning the value data as a matrix.
     * @warning Use with care as any copy of DiffDataMatrix will have its value modified as well.
     */
    Eigen::Map<Eigen::Matrix<T, R, C>> MutableMatrix() { return Eigen::Map<Eigen::Matrix<T, R, C>>(this->MutableValue().data(), Rows(), Cols()); }

    //! Multiplies two matrices C = A * B
    template <int C2>
    DiffDataMatrix<T, R, C2> Multiply(const DiffDataMatrix<T, C, C2>& B) const
    {
        MatrixMultiplyFunction<T> multiplyFunction;
        return multiplyFunction.DenseMatrixMatrixMultiply(*this, B);
    }

    //! Multiplies a matrix and a vector c = A * b
    DiffData<T> Multiply(const DiffData<T>& b) const
    {
        MatrixMultiplyFunction<T> multiplyFunction;
        return multiplyFunction.DenseMatrixVectorMultiply(*this, b);
    }

private:
    int m_rows;
    int m_cols;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

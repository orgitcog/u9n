// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/DiffData.h>
#include <nls/math/Math.h>
#include <iostream>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class MatrixMultiplyFunction;

/**
 * Differentiable data but with f(x) being a sparse matrix and not a vector.
 * Values of the underlying data vector are the dense nonzero entries and will always be in the sparse matrix's storage order.
 * @see DiffData
 */
template <class T, int R = -1, int C = -1>
class DiffDataSparseMatrix : public DiffData<T>
{
public:
    /**
     * @warning Index arrays follow Eigen's CSC sparse format. len(outerIndices) = R + 1 and len(innerIndices) = o.Size()
     */
    template <bool TMP = true>
    DiffDataSparseMatrix(DiffData<T>&& o, const Eigen::VectorX<int>& outerIndices, const Eigen::VectorX<int>& innerIndices,
                         typename std::enable_if<TMP && ((R >= 0) && (C >= 0))>::type* = 0)
        : DiffData<T>(std::move(o))
        , m_rows(R)
        , m_cols(C)
        , m_outerIndices(outerIndices)
        , m_innerIndices(innerIndices)
    {
        CARBON_PRECONDITION(m_rows >= 0, "invalid row size");
        CARBON_PRECONDITION(m_cols >= 0, "invalid column size");
        CARBON_PRECONDITION(int(outerIndices.size()) == int(Rows()) + 1, "the number of outer indices needs to equal Rows + 1");
        CARBON_PRECONDITION(int(innerIndices.size()) == int(this->Size()), "the number of inner indices needs to match DiffData size");

        //SanityCheck(updateJacobian);
    }

    DiffDataSparseMatrix(int rows, int cols, DiffData<T>&& o, const Eigen::VectorX<int>& outerIndices, const Eigen::VectorX<int>& innerIndices)
        : DiffData<T>(std::move(o))
        , m_rows(rows)
        , m_cols(cols)
        , m_outerIndices(outerIndices)
        , m_innerIndices(innerIndices)
    {
        CARBON_PRECONDITION(m_rows >= 0 && (m_rows == R || R < 0), "invalid row size");
        CARBON_PRECONDITION(m_cols >= 0 && (m_cols == C || C < 0), "invalid column size");
        CARBON_PRECONDITION(int(outerIndices.size()) == int(Rows()) + 1, "the number of outer indices needs to equal Rows + 1");
        CARBON_PRECONDITION(int(innerIndices.size()) == int(this->Size()), "the number of inner indices needs to match DiffData size");

        //SanityCheck(updateJacobian);
    }

    /**
     * @warning This constructor uses 'mat' ONLY for the sparsity structure for convenience. The nonzero data and jacobian comes from DiffData input
     */
    template <bool TMP = true>
    DiffDataSparseMatrix(DiffData<T>&& o,
                         const SparseMatrix<T>& mat,
                         typename std::enable_if<TMP && ((R >= 0) && (C >= 0))>::type* = 0)
        : DiffDataSparseMatrix(std::move(o),
                               Eigen::Map<const Eigen::VectorX<int>>(mat.outerIndexPtr(), mat.outerSize() + 1),
                               Eigen::Map<const Eigen::VectorX<int>>(mat.innerIndexPtr(), mat.nonZeros())) {}

    /**
     * @warning This constructor uses 'mat' ONLY for the sparsity structure for convenience. The nonzero data and jacobian comes from DiffData input
     */
    template <bool TMP = true>
    DiffDataSparseMatrix(DiffData<T>&& o,
                         const SparseMatrix<T>& mat,
                         typename std::enable_if<TMP && !((R >= 0) && (C >= 0))>::type* = 0)
        : DiffDataSparseMatrix(int(mat.rows()),
                               int(mat.cols()),
                               std::move(o),
                               Eigen::Map<const Eigen::VectorX<int>>(mat.outerIndexPtr(), mat.outerSize() + 1),
                               Eigen::Map<const Eigen::VectorX<int>>(mat.innerIndexPtr(), mat.nonZeros())) {}

    template <bool TMP = true>
    DiffDataSparseMatrix(const SparseMatrix<T>& mat,
                         const JacobianConstPtr<T>& jacobian = nullptr,
                         typename std::enable_if<TMP && ((R >= 0) && (C >= 0))>::type* = 0)
        : DiffDataSparseMatrix(DiffData<T>(mat.valuePtr(), int(mat.nonZeros()), jacobian),
                               Eigen::Map<const Eigen::VectorX<int>>(mat.outerIndexPtr(), mat.outerSize() + 1),
                               Eigen::Map<const Eigen::VectorX<int>>(mat.innerIndexPtr(), mat.nonZeros())) {}

    template <bool TMP = true>
    DiffDataSparseMatrix(const SparseMatrix<T>& mat,
                         const JacobianConstPtr<T>& jacobian = nullptr,
                         typename std::enable_if<TMP && !((R >= 0) && (C >= 0))>::type* = 0)
        : DiffDataSparseMatrix(int(mat.rows()),
                               int(mat.cols()),
                               DiffData<T>(mat.valuePtr(),int(mat.nonZeros()), jacobian),
                               Eigen::Map<const Eigen::VectorX<int>>(mat.outerIndexPtr(), mat.outerSize() + 1),
                               Eigen::Map<const Eigen::VectorX<int>>(mat.innerIndexPtr(), mat.nonZeros())) {}


    int Rows() const { return m_rows; }
    int Cols() const { return m_cols; }

    const Eigen::VectorX<int>& OuterIndices() const { return m_outerIndices; }
    const Eigen::VectorX<int>& InnerIndices() const { return m_innerIndices; }

    //! Convenience function returning the value data as a sparse matrix
    Eigen::Map<const SparseMatrix<T>> Matrix() const
    {
         return Eigen::Map<const SparseMatrix<T>>(Rows(), Cols(), this->Size(), m_outerIndices.data(), m_innerIndices.data(), this->Value().data());
    }

    /**
     * Convenience function returning the value data as a sparse matrix.
     * @warning Use with care as any copy of DiffDataSparseMatrix will have its value modified as well.
     */
    Eigen::Map<SparseMatrix<T>> MutableMatrix()
    {
        return Eigen::Map<SparseMatrix<T>>(Rows(), Cols(), this->Size(), m_outerIndices.data(), m_innerIndices.data(), this->Value().data());
    }

    //! Multiplies a matrix and a vector c = A * b
    DiffData<T> Multiply(const DiffData<T>& b) const
    {
        MatrixMultiplyFunction<T> multiplyFunction;
        return multiplyFunction.SparseMatrixVectorMultiply(*this, b);
    }

private:
    int m_rows;
    int m_cols;
    Eigen::VectorX<int> m_outerIndices;
    Eigen::VectorX<int> m_innerIndices;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

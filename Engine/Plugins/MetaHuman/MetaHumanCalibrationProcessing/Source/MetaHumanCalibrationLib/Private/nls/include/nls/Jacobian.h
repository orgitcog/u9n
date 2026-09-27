// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <carbon/utils/TaskThreadPool.h>
#include <nls/math/Math.h>
#include <nls/BlockJacobian.h>

#include <memory>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

#define USE_BLOCK_JACOBIAN 0

#if USE_BLOCK_JACOBIAN
template <class T>
using JacobianPtr = std::shared_ptr<BlockJacobian<T>>;

template <class T>
using JacobianConstPtr = std::shared_ptr<const BlockJacobian<T>>;

template <class T>
using Jacobian = BlockJacobian<T>;

template <class T>
using DenseJacobian = BlockJacobian<T>;
template <class T>
using SparseJacobian = BlockJacobian<T>;

#else // USE_BLOCK_JACOBIAN

template <class T>
class Jacobian;

template <class T>
using JacobianPtr = std::shared_ptr<Jacobian<T>>;

template <class T>
using JacobianConstPtr = std::shared_ptr<const Jacobian<T>>;

/**
 * Abstract Jacobian class
 */
template <class T>
class Jacobian
{
public:
    Jacobian() : m_rows(0), m_startCol(0), m_endCol(0) {}
    Jacobian(int rows, int startCol, int endCol) : m_rows(rows), m_startCol(startCol), m_endCol(endCol) {}
    virtual ~Jacobian() = default;

    //! @returns the number of rows of the Jacobian matrix
    int Rows() const { return m_rows; }

    //! @returns the number of columns of the Jacobian matrix
    int Cols() const { return m_endCol; }

    //! @returns the first non-zero column of the Jacobian matrix
    int StartCol() const { return m_startCol; }

    //! @returns True if the Jacobian matrix is sparse
    virtual bool IsSparse() const = 0;

    //! @returns the number of non zeros in the Jacobian matrix
    virtual int NonZeros() const = 0;

    //! @returns the Jacobian as a Sparse Matrix
    virtual SparseMatrixConstPtr<T> AsSparseMatrix() const = 0;

    //! @brief Copies a range of the jacobian to a dense matrix.
    virtual void CopyToDenseMatrix(Eigen::Ref<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> dense) const = 0;

    //! @returns a new Jacobian by premultiplying the current Jacobian with sparseMat
    virtual JacobianConstPtr<T> Premultiply(const SparseMatrix<T>& sparseMat) const = 0;

    /**
     * Adds another Jacobian to this Jacobian and returns it as a new Jacobian.
     * @precondition: other.Rows() == Rows() and other.Cols() == Cols()
     */
    virtual JacobianConstPtr<T> Add(JacobianConstPtr<T> other) const = 0;

    /**
     * Subtracts another Jacobian from this Jacobian and returns it as a new Jacobian.
     * @precondition: other.Rows() == Rows() and other.Cols() == Cols()
     */
    virtual JacobianConstPtr<T> Subtract(JacobianConstPtr<T> other) const = 0;

    //! Scales the Jacobian by @p scale and returns it as a new Jacobian
    virtual JacobianConstPtr<T> Scale(T scale) const = 0;

    /**
     * Selects blockSize rows from the Jacobian and returns it as a new Jacobian
     *
     * newJacobian(blockSize  * i : blockSize * (i + 1), :) = thisJacobian(blockSize * blockIndices[i], blockSize * (blockIndices[i] + 1), :)
     */
    virtual JacobianConstPtr<T> RowGather(const Eigen::VectorX<int>& blockIndices, int blockSize = 1) const = 0;

    /**
     * Scatters the blockSize rows from this Jacobian to outputsize based on the blockIndices and returns it as a new Jacobian
     *
     * newJacobian(blockSize * blockIndices[i] : blockSize * (blockIndices[i] + 1), :) = thisJacobian(blockSize * i : blockSize * (i + 1), :)
     */
    virtual JacobianConstPtr<T> RowScatter(int outputSize, const Eigen::VectorX<int>& blockIndices, int blockSize = 1) const = 0;

    //! Repeats the rows of the Jacobian @p N times.
    virtual JacobianConstPtr<T> Repeat(int N) const = 0;

    //! Extract a single row of the Jacobian
    virtual Eigen::SparseVector<T> Row(int row) const = 0;

    //! result += scale * Jacobian() * vec
    virtual void AddJx(Eigen::Ref<Eigen::VectorX<T>> result, const Eigen::Ref<const Eigen::VectorX<T>>& x, const T scale) const = 0;

    //! result += scale * Jacobian().transpose() * vec
    virtual void AddJtx(Eigen::Ref<Eigen::VectorX<T>> result, const Eigen::Ref<const Eigen::VectorX<T>>& x, const T scale) const = 0;

    //! JtJ += scale * Jacobian().transpose() * Jacobian() using triplets
    virtual void AddSparseJtJLower(std::vector<Eigen::Triplet<T>>& triplets, const T scale) const = 0;

    //! JtJ += scale * Jacobian().transpose() * Jacobian()
    virtual void AddSparseJtJLower(SparseMatrix<T>& JtJ, const T scale) const = 0;

    //! JtJ += scale * Jacobian().transpose() * Jacobian()
    virtual void AddDenseJtJLower(Eigen::Ref<Eigen::Matrix<T, -1, -1>> JtJ, const T scale, TaskThreadPool* threadPool) const = 0;

private:
    Jacobian(const Jacobian&) = delete;
    Jacobian& operator=(const Jacobian&) = delete;

private:
    int m_rows;
    int m_startCol;
    int m_endCol;
};


/**
 * Default Sparse Jacobian class using a row-major Eigen sparse matrix
 */
template <class T>
class SparseJacobian : public Jacobian<T>
{
public:
    using SparseMatrixType = Eigen::SparseMatrix<T, Eigen::RowMajor>;

public:
    SparseJacobian(SparseMatrixConstPtr<T>&& sparseMatrix, int startCol)
        : Jacobian<T>(int(sparseMatrix->rows()), startCol, int(sparseMatrix->cols()))
        , m_sparseMatrix(std::move(sparseMatrix)) {}
    SparseJacobian(const SparseMatrixConstPtr<T>& sparseMatrix, int startCol)
        : Jacobian<T>(int(sparseMatrix->rows()), startCol, int(sparseMatrix->cols()))
        , m_sparseMatrix(sparseMatrix) {}
    virtual ~SparseJacobian() {}

    virtual bool IsSparse() const final override { return true; }

    virtual int NonZeros() const final override { return int(m_sparseMatrix->nonZeros()); }

    virtual SparseMatrixConstPtr<T> AsSparseMatrix() const final override { return m_sparseMatrix; }

    virtual void CopyToDenseMatrix(Eigen::Ref<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> dense) const final override;

    virtual JacobianConstPtr<T> Premultiply(const SparseMatrix<T>& sparseMat) const final override;

    virtual JacobianConstPtr<T> Add(JacobianConstPtr<T> other) const final override;

    virtual JacobianConstPtr<T> Subtract(JacobianConstPtr<T> other) const final override;

    virtual JacobianConstPtr<T> Scale(T scale) const final override;

    virtual JacobianConstPtr<T> RowGather(const Eigen::VectorX<int>& blockIndices, int blockSize) const final override;

    virtual JacobianConstPtr<T> RowScatter(int outputSize, const Eigen::VectorX<int>& blockIndices, int blockSize) const final override;

    virtual JacobianConstPtr<T> Repeat(int N) const final override;

    virtual Eigen::SparseVector<T> Row(int row) const final override;

    //! result += scale * Jacobian() * vec
    virtual void AddJx(Eigen::Ref<Eigen::VectorX<T>> result, const Eigen::Ref<const Eigen::VectorX<T>>& x, const T scale) const final override;

    //! result += scale * Jacobian().transpose() * vec
    virtual void AddJtx(Eigen::Ref<Eigen::VectorX<T>> result, const Eigen::Ref<const Eigen::VectorX<T>>& x, const T scale) const final override;

    //! JtJ += scale * Jacobian().transpose() * Jacobian()
    virtual void AddSparseJtJLower(SparseMatrix<T>& JtJ, const T scale) const final override;

    //! JtJ += scale * Jacobian().transpose() * Jacobian() using triplets
    virtual void AddSparseJtJLower(std::vector<Eigen::Triplet<T>>& triplets, const T scale) const final override;

    //! JtJ += scale * Jacobian().transpose() * Jacobian()
    virtual void AddDenseJtJLower(Eigen::Ref<Eigen::Matrix<T, -1, -1>> JtJ, const T scale, TaskThreadPool* threadPool) const final override;

private:
    SparseMatrixConstPtr<T> m_sparseMatrix;
};

template <class T>
using SparseJacobianPtr = std::shared_ptr<SparseJacobian<T>>;

template <class T>
using SparseJacobianConstPtr = std::shared_ptr<const SparseJacobian<T>>;


/**
 * Default Dense Jacobian class using a row-major Eigen dense matrix as block specifying the non-zero values.
 */
template <class T>
class DenseJacobian : public Jacobian<T>
{
public:
    using DenseMatrixType = Eigen::Matrix<T, -1, -1, Eigen::RowMajor>;

public:
    DenseJacobian(std::shared_ptr<const DenseMatrixType>&& denseMatrix, int startCol)
        : Jacobian<T>(int(denseMatrix->rows()), startCol, startCol + int(denseMatrix->cols()))
        , m_denseMatrix(std::move(denseMatrix)) {}
    DenseJacobian(const std::shared_ptr<const DenseMatrixType>& denseMatrix, int startCol)
        : Jacobian<T>(int(denseMatrix->rows()), startCol, startCol + int(denseMatrix->cols()))
        , m_denseMatrix(denseMatrix) {}
    virtual ~DenseJacobian() {}

    virtual bool IsSparse() const final override { return false; }

    virtual int NonZeros() const final override { return int(m_denseMatrix->size()); }

    virtual SparseMatrixConstPtr<T> AsSparseMatrix() const final override;

    virtual void CopyToDenseMatrix(Eigen::Ref<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> dense) const final override;

    virtual JacobianConstPtr<T> Premultiply(const SparseMatrix<T>& sparseMat) const final override;

    virtual JacobianConstPtr<T> Add(JacobianConstPtr<T> other) const final override;

    virtual JacobianConstPtr<T> Subtract(JacobianConstPtr<T> other) const final override;

    virtual JacobianConstPtr<T> Scale(T scale) const final override;

    virtual JacobianConstPtr<T> RowGather(const Eigen::VectorX<int>& blockIndices, int blockSize) const final override;

    virtual JacobianConstPtr<T> RowScatter(int outputSize, const Eigen::VectorX<int>& blockIndices, int blockSize) const final override;

    virtual JacobianConstPtr<T> Repeat(int N) const final override;

    virtual Eigen::SparseVector<T> Row(int row) const final override;

    //! result += scale * Jacobian() * vec
    virtual void AddJx(Eigen::Ref<Eigen::VectorX<T>> result, const Eigen::Ref<const Eigen::VectorX<T>>& x, const T scale) const final override;

    //! result += scale * Jacobian().transpose() * vec
    virtual void AddJtx(Eigen::Ref<Eigen::VectorX<T>> result, const Eigen::Ref<const Eigen::VectorX<T>>& x, const T scale) const final override;

    //! JtJ += scale * Jacobian().transpose() * Jacobian()
    virtual void AddSparseJtJLower(SparseMatrix<T>& JtJ, const T scale) const final override;

    //! JtJ += scale * Jacobian().transpose() * Jacobian() using triplets
    virtual void AddSparseJtJLower(std::vector<Eigen::Triplet<T>>& triplets, const T scale) const final override;

    //! JtJ += scale * Jacobian().transpose() * Jacobian()
    virtual void AddDenseJtJLower(Eigen::Ref<Eigen::Matrix<T, -1, -1>> JtJ, const T scale, TaskThreadPool* threadPool) const final override;

private:
    std::shared_ptr<const DenseMatrixType> m_denseMatrix;
};

#endif

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

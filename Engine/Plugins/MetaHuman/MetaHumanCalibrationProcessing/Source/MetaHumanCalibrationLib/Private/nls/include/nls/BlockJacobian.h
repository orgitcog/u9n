// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <carbon/utils/TaskThreadPool.h>
#include <nls/math/Math.h>

#include <memory>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Jacobian class using block layout with each block being either a sparse or dense matrix.
 */
template <class T>
class BlockJacobian
{
public:
    using DenseMatrixType = Eigen::Matrix<T, -1, -1, Eigen::RowMajor>;
    using SparseMatrixType = Eigen::SparseMatrix<T, Eigen::RowMajor>;

public:
    BlockJacobian() = default;
    /**
     * @brief Create a block jacobian with a sparse matrix of size [rows, cols].
     *        The block jacobian then has size [rows, cols].
     *        @p startCol should be the first non-zero column of the sparse matrix.
     */
    BlockJacobian(const std::shared_ptr<const SparseMatrixType>& sparseMatrix, int startCol);
    //! Create a block jacobian with a dense matrix of size [rows, cols]. The block jacobian then has size [rows, startCol + cols]
    BlockJacobian(const std::shared_ptr<const DenseMatrixType>& denseMatrix, int startCol);
    ~BlockJacobian() = default;
    BlockJacobian(BlockJacobian&&) = default;
    BlockJacobian(const BlockJacobian&) = default;
    BlockJacobian& operator=(BlockJacobian&&) = default;
    BlockJacobian& operator=(const BlockJacobian&) = default;

    //! @returns the number of rows of the Jacobian matrix
    [[nodiscard]] int Rows() const;

    //! @returns the number of columns of the Jacobian matrix
    [[nodiscard]] int Cols() const;

    //! @returns the first non-zero column of the Jacobian matrix
    [[nodiscard]] int StartCol() const;

    //! @returns the total number of non zeros
    [[nodiscard]] int NonZeros() const;

    //! @returns the number of non zeros for row @p r
    [[nodiscard]] int NonZeros(int r) const;

    //! @returns the block jacobian as a sparse matrix of size [Rows(), Cols()]
    [[nodiscard]] std::shared_ptr<const SparseMatrixType> AsSparseMatrix() const;

    //! Copies the block jacobian to dense matrix @p dense (of size [Rows(), Cols() - StartCol()])
    void CopyToDenseMatrix(Eigen::Ref<DenseMatrixType> dense) const;

    [[nodiscard]] std::shared_ptr<const BlockJacobian> Premultiply(const SparseMatrixType& sparseMat) const;

    [[nodiscard]] std::shared_ptr<const BlockJacobian> Add(const BlockJacobian<T>& other) const;
    [[nodiscard]] std::shared_ptr<const BlockJacobian> Add(const std::shared_ptr<const BlockJacobian<T>>& other) const { return Add(*other); }

    [[nodiscard]] std::shared_ptr<const BlockJacobian> Subtract(const BlockJacobian<T>& other) const;
    [[nodiscard]] std::shared_ptr<const BlockJacobian> Subtract(const std::shared_ptr<const BlockJacobian<T>>& other) const { return Subtract(*other); }

    [[nodiscard]] std::shared_ptr<const BlockJacobian> Scale(T scale) const;

    [[nodiscard]] std::shared_ptr<const BlockJacobian> RowGather(const Eigen::VectorX<int>& blockIndices, int blockSize) const;

    [[nodiscard]] std::shared_ptr<const BlockJacobian> RowScatter(int outputSize, const Eigen::VectorX<int>& blockIndices, int blockSize) const;

    [[nodiscard]] std::shared_ptr<const BlockJacobian> Repeat(int N) const;

    [[nodiscard]] Eigen::SparseVector<T> Row(int row) const;

    //! result += scale * Jacobian() * vec
    void AddJx(Eigen::Ref<Eigen::VectorX<T>> result, const Eigen::Ref<const Eigen::VectorX<T>>& x, const T scale) const;

    //! result += scale * Jacobian().transpose() * vec
    void AddJtx(Eigen::Ref<Eigen::VectorX<T>> result, const Eigen::Ref<const Eigen::VectorX<T>>& x, const T scale) const;

    //! JtJ += scale * Jacobian().transpose() * Jacobian()
    void AddSparseJtJLower(std::vector<Eigen::Triplet<T>>& JtJ, const T scale) const;

    //! JtJ += scale * Jacobian().transpose() * Jacobian()
    void AddDenseJtJLower(Eigen::Ref<Eigen::Matrix<T, -1, -1>> JtJ, const T scale, TaskThreadPool* threadPool) const;

private:
    struct Block
    {
        std::shared_ptr<const SparseMatrixType> sparseMatrix;
        std::shared_ptr<const DenseMatrixType> denseMatrix;
        int startCol;
        int endCol;
        T scale;
    };
    std::vector<Block> m_blocks;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

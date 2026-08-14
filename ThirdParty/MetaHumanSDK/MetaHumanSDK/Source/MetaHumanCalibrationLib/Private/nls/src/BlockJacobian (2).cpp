// Copyright Epic Games, Inc. All Rights Reserved.

#include <nls/BlockJacobian.h>
#include <nls/math/ParallelBLAS.h>
#include <nls/math/SparseMatrixMultiply.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
BlockJacobian<T>::BlockJacobian(const std::shared_ptr<const SparseMatrixType>& sparseMatrix, int startCol)
    : m_blocks({ { sparseMatrix, std::shared_ptr<const DenseMatrixType>(), startCol, int(sparseMatrix->cols()), T(1) } })
{}

template <class T>
BlockJacobian<T>::BlockJacobian(const std::shared_ptr<const DenseMatrixType>& denseMatrix, int startCol)
    : m_blocks({ { std::shared_ptr<const SparseMatrixType>(), denseMatrix, startCol, int(denseMatrix->cols()) + startCol, T(1) } })
{}

template <class T>
int BlockJacobian<T>::Rows() const
{
    return int(m_blocks.front().sparseMatrix ? m_blocks.front().sparseMatrix->rows() : m_blocks.front().denseMatrix->rows());
}

template <class T>
int BlockJacobian<T>::Cols() const
{
    return m_blocks.back().endCol;
}

template <class T>
int BlockJacobian<T>::StartCol() const
{
    return m_blocks.front().startCol;
}

template <class T>
int BlockJacobian<T>::NonZeros() const
{
    Eigen::Index nonzeros = 0;
    for (const Block& block : m_blocks)
    {
        if (block.sparseMatrix)
        {
            nonzeros += block.sparseMatrix->nonZeros();
        }
        else
        {
            nonzeros += block.denseMatrix->rows() * block.denseMatrix->cols();
        }
    }
    return int(nonzeros);
}

template <class T>
int BlockJacobian<T>::NonZeros(int r) const
{
    Eigen::Index nonzeros = 0;
    for (const Block& block : m_blocks)
    {
        if (block.sparseMatrix)
        {
            nonzeros += block.sparseMatrix->outerIndexPtr()[r + 1] - block.sparseMatrix->outerIndexPtr()[r];
        }
        else
        {
            nonzeros += block.denseMatrix->cols();
        }
    }
    return int(nonzeros);
}

template <class T>
std::shared_ptr<const typename BlockJacobian<T>::SparseMatrixType> BlockJacobian<T>::AsSparseMatrix() const
{
    if ((m_blocks.size() == 1) && m_blocks.front().sparseMatrix && (m_blocks.front().scale == T(1)))
    {
        return m_blocks.front().sparseMatrix;
    }

    auto smat = std::make_shared<SparseMatrixType>(Rows(), Cols());
    smat->reserve(NonZeros());
    for (int r = 0; r < Rows(); ++r)
    {
        smat->startVec(r);
        for (const Block& block : m_blocks)
        {
            if (block.sparseMatrix)
            {
                for (typename SparseMatrixType::InnerIterator it(*block.sparseMatrix, r); it; ++it)
                {
                    smat->insertBackByOuterInner(r, it.col()) = block.scale * it.value();
                }
            }
            else
            {
                for (int c = 0; c < int(block.denseMatrix->cols()); ++c)
                {
                    smat->insertBackByOuterInner(r, c + block.startCol) = block.scale * (*block.denseMatrix)(r, c);
                }
            }
        }
    }
    smat->finalize();
    return smat;
}

template <class T>
void BlockJacobian<T>::CopyToDenseMatrix(Eigen::Ref<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> dense) const
{
    if (Cols() - StartCol() != int(dense.cols()))
    {
        CARBON_CRITICAL("number of columns do not match: {} vs {}", Cols() - StartCol(), dense.cols());
    }
    if (int(dense.rows()) != Rows())
    {
        CARBON_CRITICAL("number of rows do not match");
    }
    const int startCol = StartCol();
    const int rows = Rows();
    for (size_t i = 0; i < m_blocks.size(); ++i)
    {
        if (i > 0)
        {
            const int zeroCols = m_blocks[i].startCol - m_blocks[i - 1].endCol;
            dense.block(0, m_blocks[i].startCol - startCol, Rows(), zeroCols).setZero();
        }
        const int dataCols = m_blocks[i].endCol - m_blocks[i].startCol;
        if (m_blocks[i].sparseMatrix)
        {
            dense.block(0, m_blocks[i].startCol - startCol, rows, dataCols) = m_blocks[i].scale * m_blocks[i].sparseMatrix->block(0,
                                                                                                                                  m_blocks[i].startCol,
                                                                                                                                  Rows(),
                                                                                                                                  m_blocks[i].endCol -
                                                                                                                                  m_blocks[i].startCol);
        }
        else
        {
            dense.block(0, m_blocks[i].startCol - startCol, rows, dataCols) = m_blocks[i].scale * (*m_blocks[i].denseMatrix);
        }
    }
}

template <class T>
std::shared_ptr<const BlockJacobian<T>> BlockJacobian<T>::Premultiply(const SparseMatrixType& sparseMat) const
{
    std::shared_ptr<BlockJacobian<T>> newBlockJacobian = std::make_shared<BlockJacobian<T>>();
    auto& newBlocks = newBlockJacobian->m_blocks;
    newBlocks.reserve(m_blocks.size());
    for (const Block& block : m_blocks)
    {
        if (block.sparseMatrix)
        {
            std::shared_ptr<SparseMatrixType> sparseMatrix = std::make_shared<SparseMatrixType>();
            SparseMatrixMultiply(sparseMat, false, *block.sparseMatrix, false, *sparseMatrix);
            newBlocks.emplace_back(Block{ sparseMatrix, std::shared_ptr<const DenseMatrixType>(), block.startCol, block.endCol, block.scale });
        }
        else
        {
            newBlocks.emplace_back(Block{ std::shared_ptr<const SparseMatrixType>(), std::make_shared<const DenseMatrixType>(
                                              sparseMat * *block.denseMatrix), block.startCol, block.endCol, block.scale });
        }
    }
    return newBlockJacobian;
}

template <class T>
std::shared_ptr<const BlockJacobian<T>> BlockJacobian<T>::Add(const BlockJacobian<T>& other) const
{
    if (other.Rows() != Rows())
    {
        CARBON_CRITICAL("number of rows are not matching: {} vs {}", Rows(), other.Rows());
    }
    std::shared_ptr<BlockJacobian<T>> newBlockJacobian = std::make_shared<BlockJacobian<T>>();
    auto& newBlocks = newBlockJacobian->m_blocks;
    newBlocks.reserve(m_blocks.size() + other.m_blocks.size());
    size_t i0 = 0;
    size_t i1 = 0;
    while (i0 < m_blocks.size() && i1 < other.m_blocks.size())
    {
        const auto& thisBlock = m_blocks[i0];
        const auto& otherBlock = other.m_blocks[i1];
        if (thisBlock.endCol <= otherBlock.startCol)
        {
            newBlocks.push_back(thisBlock);
            ++i0;
        }
        else if (otherBlock.endCol <= thisBlock.startCol)
        {
            newBlocks.push_back(otherBlock);
            ++i1;
        }
        else if ((thisBlock.startCol == otherBlock.startCol) && (thisBlock.endCol == otherBlock.endCol))
        {
            if (thisBlock.sparseMatrix && otherBlock.sparseMatrix)
            {
                // add sparse jacobians
                SparseMatrixType sum = thisBlock.scale * (*thisBlock.sparseMatrix) + otherBlock.scale * (*otherBlock.sparseMatrix);
                newBlocks.push_back(Block{ std::make_shared<SparseMatrixType>(std::move(sum)),
                                           std::shared_ptr<const DenseMatrixType>(), thisBlock.startCol, thisBlock.endCol, T(1) });
            }
            else if (thisBlock.denseMatrix && otherBlock.denseMatrix)
            {
                // create a joint dense jacobian
                DenseMatrixType sum = thisBlock.scale * (*thisBlock.denseMatrix) + otherBlock.scale * (*otherBlock.denseMatrix);
                newBlocks.push_back(Block{ std::shared_ptr<const SparseMatrixType>(), std::make_shared<DenseMatrixType>(std::move(
                                                                                                                            sum)), thisBlock.startCol, thisBlock.endCol, T(
                                               1) });
            }
            else if (thisBlock.sparseMatrix && otherBlock.denseMatrix)
            {
                // create a joint dense jacobian
                DenseMatrixType sum = thisBlock.scale *
                    (thisBlock.sparseMatrix->block(0,
                                                   thisBlock.startCol,
                                                   Rows(),
                                                   thisBlock.endCol - thisBlock.startCol)) + otherBlock.scale * (*otherBlock.denseMatrix);
                newBlocks.push_back(Block{ std::shared_ptr<const SparseMatrixType>(), std::make_shared<DenseMatrixType>(std::move(
                                                                                                                            sum)), thisBlock.startCol, thisBlock.endCol, T(
                                               1) });
            }
            else if (thisBlock.denseMatrix && otherBlock.sparseMatrix)
            {
                // create a joint dense jacobian
                DenseMatrixType sum = thisBlock.scale * (*thisBlock.denseMatrix) + otherBlock.scale *
                    (otherBlock.sparseMatrix->block(0,
                                                    otherBlock.startCol,
                                                    Rows(),
                                                    otherBlock.endCol - otherBlock.startCol));
                newBlocks.push_back(Block{ std::shared_ptr<const SparseMatrixType>(), std::make_shared<DenseMatrixType>(std::move(
                                                                                                                            sum)), thisBlock.startCol, thisBlock.endCol, T(
                                               1) });
            }
            ++i0;
            ++i1;
        }
        else
        {
            CARBON_CRITICAL("block jacobians do not support partially overlapping jacobian blocks");
        }
    }
    while (i0 < m_blocks.size())
    {
        newBlocks.push_back(m_blocks[i0++]);
    }
    while (i1 < other.m_blocks.size())
    {
        newBlocks.push_back(other.m_blocks[i1++]);
    }
    return newBlockJacobian;
}

template <class T>
std::shared_ptr<const BlockJacobian<T>> BlockJacobian<T>::Subtract(const BlockJacobian<T>& other) const
{
    return Add(*other.Scale(T(-1)));
}

template <class T>
std::shared_ptr<const BlockJacobian<T>> BlockJacobian<T>::Scale(T scale) const
{
    std::shared_ptr<BlockJacobian<T>> newBlockJacobian = std::make_shared<BlockJacobian<T>>(*this);
    for (Block& block : newBlockJacobian->m_blocks)
    {
        block.scale *= scale;
    }
    return newBlockJacobian;
}

template <class T>
std::shared_ptr<const BlockJacobian<T>> BlockJacobian<T>::RowGather(const Eigen::VectorX<int>& blockIndices, int blockSize) const
{
    std::shared_ptr<BlockJacobian<T>> newBlockJacobian = std::make_shared<BlockJacobian<T>>();
    auto& newBlocks = newBlockJacobian->m_blocks;
    newBlocks.reserve(m_blocks.size());
    for (const Block& block : m_blocks)
    {
        if (block.sparseMatrix)
        {
            std::shared_ptr<SparseMatrixType> jacobian = std::make_shared<SparseMatrixType>();
            *jacobian = TITAN_NAMESPACE::RowGather(*block.sparseMatrix, blockIndices, blockSize);
            newBlocks.emplace_back(Block{ std::move(jacobian), std::shared_ptr<const DenseMatrixType>(), block.startCol, block.endCol, block.scale });
        }
        else
        {
            std::shared_ptr<DenseMatrixType> jacobian = std::make_shared<DenseMatrixType>((int)blockIndices.size() * blockSize, block.endCol - block.startCol);
            for (int i = 0; i < (int)blockIndices.size(); ++i)
            {
                for (int j = 0; j < blockSize; ++j)
                {
                    jacobian->row(i * blockSize + j) = block.denseMatrix->row(blockIndices[i] * blockSize + j);
                }
            }
            newBlocks.emplace_back(Block{ std::shared_ptr<const SparseMatrixType>(), std::move(jacobian), block.startCol, block.endCol, block.scale });
        }
    }
    return newBlockJacobian;
}

template <class T>
std::shared_ptr<const BlockJacobian<T>> BlockJacobian<T>::RowScatter(int outputSize, const Eigen::VectorX<int>& blockIndices, int blockSize) const
{
    std::shared_ptr<BlockJacobian<T>> newBlockJacobian = std::make_shared<BlockJacobian<T>>();
    auto& newBlocks = newBlockJacobian->m_blocks;
    newBlocks.reserve(m_blocks.size());
    for (const Block& block : m_blocks)
    {
        if (block.sparseMatrix)
        {
            std::shared_ptr<SparseMatrixType> jacobian = std::make_shared<SparseMatrixType>();
            *jacobian = TITAN_NAMESPACE::RowScatter(*block.sparseMatrix, outputSize, blockIndices, blockSize);
            newBlocks.emplace_back(Block{ std::move(jacobian), std::shared_ptr<const DenseMatrixType>(), block.startCol, block.endCol, block.scale });
        }
        else
        {
            std::shared_ptr<DenseMatrixType> jacobian = std::make_shared<DenseMatrixType>(outputSize, block.endCol - block.startCol);
            jacobian->setZero();
            for (int i = 0; i < (int)blockIndices.size(); ++i)
            {
                for (int j = 0; j < blockSize; ++j)
                {
                    jacobian->row(blockIndices[i] * blockSize + j) = block.denseMatrix->row(i * blockSize + j);
                }
            }
            newBlocks.emplace_back(Block{ std::shared_ptr<const SparseMatrixType>(), std::move(jacobian), block.startCol, block.endCol, block.scale });
        }
    }
    return newBlockJacobian;
}

template <class T>
std::shared_ptr<const BlockJacobian<T>> BlockJacobian<T>::Repeat(int N) const
{
    std::shared_ptr<BlockJacobian<T>> newBlockJacobian = std::make_shared<BlockJacobian<T>>();
    auto& newBlocks = newBlockJacobian->m_blocks;
    newBlocks.reserve(m_blocks.size());
    for (const Block& block : m_blocks)
    {
        if (block.sparseMatrix)
        {
            std::shared_ptr<SparseMatrixType> repeatedJacobian = std::make_shared<SparseMatrixType>();
            RepeatRowsOfSparseMatrix<T>(*block.sparseMatrix, *repeatedJacobian, N);
            newBlocks.emplace_back(Block{ std::move(repeatedJacobian), std::shared_ptr<const DenseMatrixType>(), block.startCol, block.endCol, block.scale });
        }
        else
        {
            auto repeated = std::make_shared<DenseMatrixType>(block.denseMatrix->replicate(N, 1));
            newBlocks.emplace_back(Block{ std::shared_ptr<const SparseMatrixType>(), std::move(repeated), block.startCol, block.endCol, block.scale });
        }
    }
    return newBlockJacobian;
}

template <class T>
Eigen::SparseVector<T> BlockJacobian<T>::Row(int row) const
{
    Eigen::SparseVector<T> svec(this->Cols());
    svec.reserve(NonZeros(row));
    svec.startVec(0);
    for (const Block& block : m_blocks)
    {
        if (block.sparseMatrix)
        {
            for (int idx = block.sparseMatrix->outerIndexPtr()[row]; idx < block.sparseMatrix->outerIndexPtr()[row + 1]; ++idx)
            {
                svec.insertBack(block.sparseMatrix->innerIndexPtr()[idx]) = block.scale * block.sparseMatrix->valuePtr()[idx];
            }
        }
        else
        {
            for (Eigen::Index i = 0; i < block.denseMatrix->cols(); ++i)
            {
                svec.insertBack(block.startCol + i) = block.scale * (*block.denseMatrix)(row, i);
            }
        }
    }
    svec.finalize();
    return svec;
}

template <class T>
void BlockJacobian<T>::AddJx(Eigen::Ref<Eigen::VectorX<T>> result, const Eigen::Ref<const Eigen::VectorX<T>>& x, const T scale) const
{
    CARBON_PRECONDITION(int(result.size()) == this->Rows(), "the output vector must match the number of rows");
    CARBON_PRECONDITION(int(x.size()) == this->Cols(), "the input vector must match the number of columns");

    for (const Block& block : m_blocks)
    {
        if (block.sparseMatrix)
        {
            result += (block.scale * scale) * ((*block.sparseMatrix) * x);
        }
        else
        {
            result += (block.scale * scale) * ((*block.denseMatrix) * x.segment(block.startCol, block.endCol - block.startCol));
        }
    }
}

template <class T>
void BlockJacobian<T>::AddJtx(Eigen::Ref<Eigen::VectorX<T>> result, const Eigen::Ref<const Eigen::VectorX<T>>& x, const T scale) const
{
    CARBON_PRECONDITION(int(result.size()) == this->Cols(), "the output vector must match the number of columns");
    CARBON_PRECONDITION(int(x.size()) == this->Rows(), "the input vector must match the number of rows");

    for (const Block& block : m_blocks)
    {
        if (block.sparseMatrix)
        {
            result.head(block.endCol) += (block.scale * scale) * (block.sparseMatrix->transpose() * x);
        }
        else
        {
            result.segment(block.startCol, block.endCol - block.startCol) += (block.scale * scale) * (block.denseMatrix->transpose() * x);
        }
    }
}

template <class T>
void BlockJacobian<T>::AddSparseJtJLower(std::vector<Eigen::Triplet<T>>& JtJ, const T scale) const
{
    for (size_t i = 0; i < m_blocks.size(); ++i)
    {
        for (size_t j = 0; j <= i; ++j)
        {
            if (i == j)
            {
                const auto& block = m_blocks[i];
                const T totScale = block.scale * block.scale * scale;
                if (block.sparseMatrix)
                {
                    for (int r = 0; r < (int)block.sparseMatrix->rows(); ++r)
                    {
                        for (typename SparseMatrix<T>::InnerIterator it(*block.sparseMatrix, r); it; ++it)
                        {
                            for (typename SparseMatrix<T>::InnerIterator it2(*block.sparseMatrix, r); it2; ++it2)
                            {
                                if (it2.col() <= it.col())
                                {
                                    JtJ.push_back(Eigen::Triplet<T>((int)it.col(), (int)it2.col(), totScale * it.value() * it2.value()));
                                }
                                else
                                {
                                    break;
                                }
                            }
                        }
                    }
                }
                else
                {
                    const Eigen::Matrix<T, -1, -1,
                                        Eigen::RowMajor> res = (block.denseMatrix->transpose() * (*block.denseMatrix)).template triangularView<Eigen::Lower>();
                    for (int r = 0; r < (int)res.rows(); ++r)
                    {
                        for (int c = 0; c <= r; ++c)
                        {
                            JtJ.push_back(Eigen::Triplet<T>(block.startCol + r, block.startCol + c, totScale * res(r, c)));
                        }
                    }
                }
            }
            else
            {
                const auto& b1 = m_blocks[i];
                const auto& b2 = m_blocks[j];
                const T totScale = b1.scale * b2.scale * scale;
                if (b1.sparseMatrix)
                {
                    if (b2.sparseMatrix)
                    {
                        for (int r = 0; r < (int)b1.sparseMatrix->rows(); ++r)
                        {
                            for (typename SparseMatrix<T>::InnerIterator it(*b1.sparseMatrix, r); it; ++it)
                            {
                                for (typename SparseMatrix<T>::InnerIterator it2(*b2.sparseMatrix, r); it2; ++it2)
                                {
                                    JtJ.push_back(Eigen::Triplet<T>((int)it.col(), (int)it2.col(), totScale * it.value() * it2.value()));
                                }
                            }
                        }
                    }
                    else
                    {
                        const Eigen::Matrix<T, -1, -1,
                                            Eigen::RowMajor> res =
                            b1.sparseMatrix->block(0, b1.startCol, Rows(), b1.endCol - b1.startCol).transpose() * (*b2.denseMatrix);
                        for (int r = 0; r < (int)res.rows(); ++r)
                        {
                            for (int c = 0; c < (int)res.cols(); ++c)
                            {
                                JtJ.push_back(Eigen::Triplet<T>(b1.startCol + r, b2.startCol + c, totScale * res(r, c)));
                            }
                        }
                    }
                }
                else
                {
                    if (b2.sparseMatrix)
                    {
                        const Eigen::Matrix<T, -1, -1, Eigen::RowMajor> res = b1.denseMatrix->transpose() * b2.sparseMatrix->block(0,
                                                                                                                                   b2.startCol,
                                                                                                                                   Rows(),
                                                                                                                                   b2.endCol - b2.startCol);
                        for (int r = 0; r < (int)res.rows(); ++r)
                        {
                            for (int c = 0; c < (int)res.cols(); ++c)
                            {
                                JtJ.push_back(Eigen::Triplet<T>(b1.startCol + r, b2.startCol + c, totScale * res(r, c)));
                            }
                        }
                    }
                    else
                    {
                        const Eigen::Matrix<T, -1, -1, Eigen::RowMajor> res = b1.denseMatrix->transpose() * (*b2.denseMatrix);
                        for (int r = 0; r < (int)res.rows(); ++r)
                        {
                            for (int c = 0; c < (int)res.cols(); ++c)
                            {
                                JtJ.push_back(Eigen::Triplet<T>(b1.startCol + r, b2.startCol + c, totScale * res(r, c)));
                            }
                        }
                    }
                }
            }
        }
    }
}

template <class T>
void BlockJacobian<T>::AddDenseJtJLower(Eigen::Ref<Eigen::Matrix<T, -1, -1>> JtJ, const T scale, TaskThreadPool* threadPool) const
{
    if ((int(JtJ.cols()) != this->Cols()) || (int(JtJ.rows()) != this->Cols()))
    {
        CARBON_CRITICAL("JtJ must match the number of columns");
    }

    for (size_t i = 0; i < m_blocks.size(); ++i)
    {
        for (size_t j = 0; j <= i; ++j)
        {
            if (i == j)
            {
                const auto& block = m_blocks[i];
                if (block.sparseMatrix)
                {
                    JtJ.block(0, 0, block.endCol,
                              block.endCol) += (block.scale * block.scale * scale) *
                        (block.sparseMatrix->transpose() * (*block.sparseMatrix)).template triangularView<Eigen::Lower>();
                }
                else
                {
                    const int cols = block.endCol - block.startCol;
                    ParallelAtALowerAdd<T>(JtJ.block(block.startCol, block.startCol, cols, cols), *block.denseMatrix, (block.scale * block.scale * scale), threadPool);
                }
            }
            else
            {
                const auto& b1 = m_blocks[i];
                const auto& b2 = m_blocks[j];
                const T totScale = b1.scale * b2.scale * scale;
                if (b1.sparseMatrix)
                {
                    if (b2.sparseMatrix)
                    {
                        auto JtJblock = JtJ.block(0, 0, b1.endCol, b2.endCol);
                        JtJblock += totScale * (b1.sparseMatrix->transpose() * (*b2.sparseMatrix));
                    }
                    else
                    {
                        auto JtJblock = JtJ.block(0, b2.startCol, b1.endCol, b2.endCol - b2.startCol);
                        JtJblock += totScale * (b1.sparseMatrix->transpose() * (*b2.denseMatrix));
                    }
                }
                else
                {
                    if (b2.sparseMatrix)
                    {
                        auto JtJblock = JtJ.block(b1.startCol, 0, b1.endCol - b1.startCol, b2.endCol);
                        JtJblock += totScale * (b1.denseMatrix->transpose() * (*b2.sparseMatrix));
                    }
                    else
                    {
                        auto JtJblock = JtJ.block(b1.startCol, b2.startCol, b1.endCol - b1.startCol, b2.endCol - b2.startCol);
                        JtJblock += totScale * (b1.denseMatrix->transpose() * (*b2.denseMatrix));
                    }
                }
            }
        }
    }
}

template class BlockJacobian<float>;
template class BlockJacobian<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

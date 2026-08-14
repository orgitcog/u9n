// Copyright Epic Games, Inc. All Rights Reserved.

#include <nls/Jacobian.h>
#include <nls/math/ParallelBLAS.h>
#include <nls/math/SparseMatrixMultiply.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

#if !USE_BLOCK_JACOBIAN

template <class T>
JacobianConstPtr<T> SparseJacobian<T>::Premultiply(const SparseMatrix<T>& sparseMat) const
{
    SparseMatrixPtr<T> sparseMatrix = std::make_shared<SparseMatrix<T>>();
    SparseMatrixMultiply(sparseMat, false, *m_sparseMatrix, false, *sparseMatrix);
    return std::make_shared<SparseJacobian<T>>(std::move(sparseMatrix), this->StartCol());
}

template <class T>
void SparseJacobian<T>::CopyToDenseMatrix(Eigen::Ref<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> dense) const
{
    if (this->Cols() - this->StartCol() != int(dense.cols()))
    {
        CARBON_CRITICAL("number of columns do not match");
    }
    if (int(dense.rows()) != this->Rows())
    {
        CARBON_CRITICAL("number of rows do not match");
    }
    dense = m_sparseMatrix->block(0, this->StartCol(), dense.rows(), dense.cols());
}

template <class T>
JacobianConstPtr<T> SparseJacobian<T>::Add(JacobianConstPtr<T> other) const
{
    SparseMatrixPtr<T> J = std::make_shared<SparseMatrix<T>>();
    *J = AddSparseMatricesAndPadColumns<T>(*m_sparseMatrix, *(other->AsSparseMatrix()), /*squeeze=*/false);
    return std::make_shared<SparseJacobian<T>>(std::move(J), std::min<int>(this->StartCol(), other->StartCol()));
}

template <class T>
JacobianConstPtr<T> SparseJacobian<T>::Subtract(JacobianConstPtr<T> other) const
{
    SparseMatrixPtr<T> J = std::make_shared<SparseMatrix<T>>();
    *J = AddSparseMatricesAndPadColumns<T>(*m_sparseMatrix, -*(other->AsSparseMatrix()), /*squeeze=*/false);
    return std::make_shared<SparseJacobian<T>>(std::move(J), std::min<int>(this->StartCol(), other->StartCol()));
}

template <class T>
JacobianConstPtr<T> SparseJacobian<T>::Scale(T scale) const
{
    return std::make_shared<SparseJacobian<T>>(std::make_shared<SparseMatrix<T>>(scale * *m_sparseMatrix), this->StartCol());
}

template <class T>
JacobianConstPtr<T> SparseJacobian<T>::RowGather(const Eigen::VectorX<int>& blockIndices, int blockSize) const
{
    SparseMatrixPtr<T> jacobian = std::make_shared<SparseMatrix<T>>();
    *jacobian = TITAN_NAMESPACE::RowGather(*m_sparseMatrix, blockIndices, blockSize);
    return std::make_shared<SparseJacobian<T>>(std::move(jacobian), this->StartCol());
}

template <class T>
JacobianConstPtr<T> SparseJacobian<T>::RowScatter(int outputSize, const Eigen::VectorX<int>& blockIndices, int blockSize) const
{
    SparseMatrixPtr<T> jacobian = std::make_shared<SparseMatrix<T>>();
    *jacobian = TITAN_NAMESPACE::RowScatter(*m_sparseMatrix, outputSize, blockIndices, blockSize);
    return std::make_shared<SparseJacobian<T>>(std::move(jacobian), this->StartCol());
}

template <class T>
JacobianConstPtr<T> SparseJacobian<T>::Repeat(int N) const
{
    SparseMatrixPtr<T> repeatedJacobian = std::make_shared<SparseMatrix<T>>();
    RepeatRowsOfSparseMatrix<T>(*m_sparseMatrix, *repeatedJacobian, N);
    return std::make_shared<SparseJacobian<T>>(std::move(repeatedJacobian), this->StartCol());
}

template <class T>
Eigen::SparseVector<T> SparseJacobian<T>::Row(int row) const
{
    return m_sparseMatrix->row(row);
}

template <class T>
void SparseJacobian<T>::AddJx(Eigen::Ref<Eigen::VectorX<T>> result, const Eigen::Ref<const Eigen::VectorX<T>>& x, const T scale) const
{
    if (int(x.size()) != this->Cols())
    {
        CARBON_CRITICAL("the input vector must match the number of columns");
    }
    result += scale * ((*m_sparseMatrix) * x);
}

template <class T>
void SparseJacobian<T>::AddJtx(Eigen::Ref<Eigen::VectorX<T>> result, const Eigen::Ref<const Eigen::VectorX<T>>& x, const T scale) const
{
    if (int(x.size()) != this->Rows())
    {
        CARBON_CRITICAL("the input vector must match the number of rows");
    }
    result += scale * (m_sparseMatrix->transpose() * x);
}

template <class T>
void SparseJacobian<T>::AddSparseJtJLower(SparseMatrix<T>& JtJ, const T scale) const
{
    #ifdef EIGEN_USE_MKL_ALL
    Eigen::SparseMatrix<T, Eigen::RowMajor> tmpJtJ;
    mkl::ComputeAtA(*m_sparseMatrix, tmpJtJ);
    if (tmpJtJ.cols() < JtJ.cols())
    {
        tmpJtJ.conservativeResize(JtJ.rows(), JtJ.cols());
    }
    JtJ += scale * tmpJtJ.template triangularView<Eigen::Lower>();
    #else
    Eigen::SparseMatrix<T, Eigen::RowMajor> result = scale * (m_sparseMatrix->transpose() * (*m_sparseMatrix));
    if (result.cols() < JtJ.cols())
    {
        result.conservativeResize(JtJ.rows(), JtJ.cols());
    }
    JtJ += result.template triangularView<Eigen::Lower>();
    #endif
}

template <class T>
void SparseJacobian<T>::AddSparseJtJLower(std::vector<Eigen::Triplet<T>>& JtJ, const T scale) const
{
    for (int r = 0; r < (int)m_sparseMatrix->rows(); ++r)
    {
        for (typename SparseMatrix<T>::InnerIterator it(*m_sparseMatrix, r); it; ++it)
        {
            for (typename SparseMatrix<T>::InnerIterator it2(*m_sparseMatrix, r); it2; ++it2)
            {
                if (it2.col() <= it.col())
                {
                    JtJ.push_back(Eigen::Triplet<T>((int)it.col(), (int)it2.col(), scale * it.value() * it2.value()));
                }
                else
                {
                    break;
                }
            }
        }
    }
}

template <class T>
void SparseJacobian<T>::AddDenseJtJLower(Eigen::Ref<Eigen::Matrix<T, -1, -1>> JtJ, const T scale, TaskThreadPool* threadPool) const
{
    (void)threadPool;
    if ((int(JtJ.cols()) != this->Cols()) || (int(JtJ.rows()) != this->Cols()))
    {
        CARBON_CRITICAL("JtJ must match the number of columns");
    }
    // ignore if there's no non zeros, otherwise this may crash the following calls
    if (m_sparseMatrix->nonZeros() == 0) {
        return;
    }
    #ifdef EIGEN_USE_MKL_ALL
    Eigen::SparseMatrix<T, Eigen::RowMajor> tmpJtJ;
    mkl::ComputeAtA(*m_sparseMatrix, tmpJtJ);
    JtJ += scale * tmpJtJ.template triangularView<Eigen::Lower>();
    #else
    JtJ += scale * (m_sparseMatrix->transpose() * (*m_sparseMatrix)).template triangularView<Eigen::Lower>();
    #endif
}

template class SparseJacobian<float>;
template class SparseJacobian<double>;


template <class T>
SparseMatrixConstPtr<T> DenseJacobian<T>::AsSparseMatrix() const
{
    LOG_WARNING("this method is inefficient and should not be called");
    auto smat = std::make_shared<SparseMatrix<T>>();
    Dense2Sparse(*m_denseMatrix, *smat, 0, this->StartCol());
    // return smat;
    return std::make_shared<SparseMatrix<T>>(std::move(smat->pruned()));
}

template <class T>
void DenseJacobian<T>::CopyToDenseMatrix(Eigen::Ref<Eigen::Matrix<T, -1, -1, Eigen::RowMajor>> dense) const
{
    if (this->Cols() - this->StartCol() != int(dense.cols()))
    {
        CARBON_CRITICAL("number of columns do not match");
    }
    if (int(dense.rows()) != this->Rows())
    {
        CARBON_CRITICAL("number of rows do not match");
    }
    dense = *m_denseMatrix;
}

template <class T>
JacobianConstPtr<T> DenseJacobian<T>::Premultiply(const SparseMatrix<T>& sparseMat) const
{
    auto result = std::make_shared<DenseMatrixType>(sparseMat * (*m_denseMatrix));
    return std::make_shared<DenseJacobian<T>>(std::move(result), this->StartCol());
}

template <class T>
JacobianConstPtr<T> DenseJacobian<T>::Add(JacobianConstPtr<T> /*other*/) const
{
    CARBON_CRITICAL("not implemented");
}

template <class T>
JacobianConstPtr<T> DenseJacobian<T>::Subtract(JacobianConstPtr<T> /*other*/) const
{
    CARBON_CRITICAL("not implemented");
}

template <class T>
JacobianConstPtr<T> DenseJacobian<T>::Scale(T scale) const
{
    return std::make_shared<DenseJacobian<T>>(std::make_unique<DenseMatrixType>(scale * (*m_denseMatrix)), this->StartCol());
}

template <class T>
JacobianConstPtr<T> DenseJacobian<T>::RowGather(const Eigen::VectorX<int>&/*blockIndices*/, int /*blockSize*/) const
{
    CARBON_CRITICAL("not implemented");
}

template <class T>
JacobianConstPtr<T> DenseJacobian<T>::RowScatter(int /*outputSize*/, const Eigen::VectorX<int>&/*blockIndices*/, int /*blockSize*/) const
{
    CARBON_CRITICAL("not implemented");
}

template <class T>
JacobianConstPtr<T> DenseJacobian<T>::Repeat(int N) const
{
    auto repeated = std::make_unique<DenseMatrixType>(m_denseMatrix->replicate(N, 1));
    return std::make_shared<DenseJacobian<T>>(std::move(repeated), this->StartCol());
}

template <class T>
Eigen::SparseVector<T> DenseJacobian<T>::Row(int row) const
{
    Eigen::SparseVector<T> svec(this->Cols());
    svec.reserve(m_denseMatrix->cols());
    svec.startVec(0);
    for (Eigen::Index i = 0; i < m_denseMatrix->cols(); ++i)
    {
        svec.insertBack(this->StartCol() + i) = (*m_denseMatrix)(row, i);
    }
    svec.finalize();
    return svec;
}

template <class T>
void DenseJacobian<T>::AddJx(Eigen::Ref<Eigen::VectorX<T>> result, const Eigen::Ref<const Eigen::VectorX<T>>& x, const T scale) const
{
    CARBON_PRECONDITION(int(x.size()) == this->Cols(), "the input vector must match the number of columns");
    if (int(x.size()) != this->Cols())
    {
        CARBON_CRITICAL("the input vector must match the number of columns");
    }
    result += scale * ((*m_denseMatrix) * x.segment(this->StartCol(), m_denseMatrix->cols()));
}

template <class T>
void DenseJacobian<T>::AddJtx(Eigen::Ref<Eigen::VectorX<T>> result, const Eigen::Ref<const Eigen::VectorX<T>>& x, const T scale) const
{
    CARBON_PRECONDITION(int(x.size()) == this->Rows(), "the input vector must match the number of rows");
    if (int(x.size()) != this->Rows())
    {
        CARBON_CRITICAL("the input vector must match the number of rows");
    }
    result.segment(this->StartCol(), m_denseMatrix->cols()) += scale * (m_denseMatrix->transpose() * x);
}

template <class T>
void DenseJacobian<T>::AddSparseJtJLower(SparseMatrix<T>& JtJ, const T scale) const
{
    LOG_WARNING("this method is inefficient and should not be called");
    Eigen::Matrix<T, -1, -1, Eigen::RowMajor> dense = (scale * (m_denseMatrix->transpose() * (*m_denseMatrix))).template triangularView<Eigen::Lower>();
    SparseMatrix<T> smat;
    Dense2Sparse(dense, smat, this->StartCol(), this->StartCol());
    JtJ += smat;
}

template <class T>
void DenseJacobian<T>::AddSparseJtJLower(std::vector<Eigen::Triplet<T>>& JtJ, const T scale) const
{
    for (int r = 0; r < (int)m_denseMatrix->cols(); ++r)
    {
        for (int c = 0; c <= r; ++c)
        {
            T val = 0;
            for (int k = 0; k < (int)m_denseMatrix->rows(); ++k)
            {
                val += (*m_denseMatrix)(k, r) * (*m_denseMatrix)(k, c);
            }
            JtJ.push_back(Eigen::Triplet<T>(this->StartCol() + r, this->StartCol() + c, scale * val));
        }
    }
}

template <class T>
void DenseJacobian<T>::AddDenseJtJLower(Eigen::Ref<Eigen::Matrix<T, -1, -1>> JtJ, const T scale, TaskThreadPool* threadPool) const
{
    CARBON_PRECONDITION(int(JtJ.cols()) == this->Cols() && int(JtJ.rows()) == this->Cols(), "JtJ must match the number of columns");
    if ((int(JtJ.cols()) != this->Cols()) || (int(JtJ.rows()) != this->Cols()))
    {
        CARBON_CRITICAL("JtJ must match the number of columns");
    }
    ParallelAtALowerAdd<T>(JtJ.block(this->StartCol(), this->StartCol(), m_denseMatrix->cols(), m_denseMatrix->cols()), *m_denseMatrix, scale, threadPool);
}

template class DenseJacobian<float>;
template class DenseJacobian<double>;

#endif

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

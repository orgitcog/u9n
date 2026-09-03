// Copyright Epic Games, Inc. All Rights Reserved.

#include <nls/math/ParallelBLAS.h>
#include <carbon/utils/TaskThreadPool.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
void ParallelNoAliasGEMV(Eigen::Ref<Eigen::VectorX<T>> out,
                         const Eigen::Ref<const Eigen::Matrix<T, -1, -1, Eigen::RowMajor>, 0, Eigen::OuterStride<-1>>& A,
                         const Eigen::Ref<const Eigen::VectorX<T>>& x,
                         TaskThreadPool* taskThreadPool)
{
    CARBON_PRECONDITION(A.rows() == out.size(), "Number of rows of A ({}) does not match size of output ({})", A.rows(), out.size());
    CARBON_PRECONDITION(A.cols() == x.size(), "Number of columns of A ({}) does not match size of x ({}).", A.cols(), x.size());

    #if defined(EIGEN_USE_BLAS)
    // use BLAS if available
    out.noalias() = A * x;
    #else
    if (taskThreadPool && (A.rows() > 1000))
    {
        const int numAvailableThreads = std::max(int(taskThreadPool->NumThreads()), 1);
        const int blockSize = int(A.rows()) / numAvailableThreads;
        const int lastBlockSize = int(A.rows()) - (numAvailableThreads - 1) * blockSize;

        TaskFutures taskFutures;
        taskFutures.Reserve(numAvailableThreads);
        for (int ri = 0; ri < numAvailableThreads; ++ri)
        {
            taskFutures.Add(taskThreadPool->AddTask(std::bind([&](int r){
                    const int rb = (r == numAvailableThreads - 1) ? lastBlockSize : blockSize;
                    out.segment(r * blockSize, rb).noalias() = A.block(r * blockSize, 0, rb, A.cols()) * x;
                }, ri)));
        }
        taskFutures.Wait();
    }
    else
    {
        out.noalias() = A * x;
    }
    #endif
}

/**
 * Method to calculate out = A * x + b with explicit multi-threading. It is only used in case MKL is not available, as MKL will
 * perform multi-threading for matrix multiplication.
 */
template <class T>
void ParallelNoAliasGEMV(Eigen::Ref<Eigen::VectorX<T>> out,
                         const Eigen::Ref<const Eigen::VectorX<T>>& b,
                         const Eigen::Ref<const Eigen::Matrix<T, -1, -1,
                                                              Eigen::RowMajor>,
                                          0, Eigen::OuterStride<-1>>& A,
                         const Eigen::Ref<const Eigen::VectorX<T>>& x,
                         TaskThreadPool* taskThreadPool)
{
    CARBON_PRECONDITION(out.size() == b.size(), "Size of output ({}) does not match size of b ({})", out.size(), b.size());
    CARBON_PRECONDITION(A.rows() == b.size(), "Number of rows of A ({}) does not match size of b ({})", A.rows(), b.size());
    CARBON_PRECONDITION(A.cols() == x.size(), "Number of columns of A ({}) does not match size of x ({}).", A.cols(), x.size());

    #if defined(EIGEN_USE_BLAS)
    // use BLAS if available
    out.noalias() = A * x + b;
    #else

    if (taskThreadPool && (A.rows() > 1000))
    {
        const int numAvailableThreads = std::max(int(taskThreadPool->NumThreads()), 1);
        const int blockSize = int(A.rows()) / numAvailableThreads;
        const int lastBlockSize = int(A.rows()) - (numAvailableThreads - 1) * blockSize;

        TaskFutures taskFutures;
        taskFutures.Reserve(numAvailableThreads);
        for (int ri = 0; ri < numAvailableThreads; ++ri)
        {
            taskFutures.Add(taskThreadPool->AddTask(std::bind([&](int r){
                    const int rb = (r == numAvailableThreads - 1) ? lastBlockSize : blockSize;
                    out.segment(r * blockSize, rb).noalias() = b.segment(r * blockSize, rb) + A.block(r * blockSize, 0, rb, A.cols()) * x;
                }, ri)));
        }
        taskFutures.Wait();
    }
    else
    {
        out.noalias() = A * x + b;
    }
    #endif
}

// Method to calculate out = A * x for a sparse matrix with explicit multi-threading.
template <class T>
void ParallelNoAliasGEMV(Eigen::Ref<Eigen::VectorX<T>> out,
                         const Eigen::SparseMatrix<T, Eigen::RowMajor>& A,
                         const Eigen::Ref<const Eigen::VectorX<T>>& x,
                         TaskThreadPool* taskThreadPool)
{
    CARBON_PRECONDITION(A.rows() == out.size(), "Number of rows of A ({}) does not match size of output ({})", A.rows(), out.size());
    CARBON_PRECONDITION(A.cols() == x.size(), "Number of columns of A ({}) does not match size of x ({}).", A.cols(), x.size());


    if (taskThreadPool && (A.rows() > 1000))
    {
        auto func = [&](int start, int end) {
                out.segment(start, end - start).noalias() = A.block(start, 0, end - start, A.cols()) * x;
            };
        taskThreadPool->AddTaskRangeAndWait((int)A.rows(), func);
    }
    else
    {
        out.noalias() = A * x;
    }
}

/**
 * Method to calculate the lower triangular matrix of AtA with multi-threading. It is only used in case MKL is not available, as MKL will
 * perform multi-threading for matrix multiplication.
 * AtA.triangularView<Eigen::Lower>() += scale * A.transpose() * A
 */
template <class T>
void ParallelAtALowerAdd(Eigen::Ref<Eigen::Matrix<T, -1, -1>> AtA,
                         const Eigen::Ref<const Eigen::Matrix<T, -1, -1, Eigen::RowMajor>, 0, Eigen::OuterStride<-1>>& A,
                         TaskThreadPool* taskThreadPool)
{
    CARBON_PRECONDITION(AtA.rows() == AtA.cols(), "AtA is not square: {}x{}", AtA.rows(), AtA.cols());
    CARBON_PRECONDITION(AtA.rows() == A.cols(), "Number of columns of A ({}) does not match AtA ({})", A.cols(), AtA.cols());

    #if defined(EIGEN_USE_BLAS)
    // use BLAS if available
    AtA.template triangularView<Eigen::Lower>() += A.transpose() * A;
    #else

    if (taskThreadPool && (A.rows() > 1000))
    {
        const int numAvailableThreads = std::max(int(taskThreadPool->NumThreads()), 1);
        // to calculate the lower triangular matrix in blocks we require n * (n + 1) / 2 threads, where n is the split in row/col of AtA
        const int numSplits = std::max(int((-1.0f + sqrt(1.0f + 8.0f * float(numAvailableThreads))) / 2.0f), 1);
        const int blockSize = int(AtA.rows()) / numSplits;
        const int lastBlockSize = int(AtA.rows()) - (numSplits - 1) * blockSize;

        TaskFutures taskFutures;
        taskFutures.Reserve(numSplits * (numSplits + 1) / 2);
        for (int ri = 0; ri < numSplits; ++ri)
        {
            for (int ci = 0; ci <= ri; ++ci)
            {
                taskFutures.Add(taskThreadPool->AddTask(std::bind([&](int r, int c){
                        const int rb = (r == numSplits - 1) ? lastBlockSize : blockSize;
                        const int cb = (c == numSplits - 1) ? lastBlockSize : blockSize;
                        if (r == c)
                        {
                            AtA.block(r * blockSize, c * blockSize, rb,
                                      cb).template triangularView<Eigen::Lower>() +=
                                A.block(0, r * blockSize, A.rows(), rb).transpose() * A.block(0, c * blockSize, A.rows(), cb);
                        }
                        else
                        {
                            AtA.block(r * blockSize, c * blockSize, rb,
                                      cb).noalias() += A.block(0, r * blockSize, A.rows(), rb).transpose() * A.block(0, c * blockSize, A.rows(), cb);
                        }
                    }, ri, ci)));
            }
        }
        taskFutures.Wait();
    }
    else
    {
        AtA.template triangularView<Eigen::Lower>() += A.transpose() * A;
    }
    #endif
}

/**
 * Method to calculate the lower triangular matrix of scaled AtA with multi-threading. It is only used in case MKL is not available, as MKL will
 * perform multi-threading for matrix multiplication.
 * AtA.triangularView<Eigen::Lower>() += scale * A.transpose() * A
 */
template <class T>
void ParallelAtALowerAdd(Eigen::Ref<Eigen::Matrix<T, -1, -1>> AtA,
                         const Eigen::Ref<const Eigen::Matrix<T, -1, -1, Eigen::RowMajor>, 0, Eigen::OuterStride<-1>>& A,
                         T scale,
                         TaskThreadPool* taskThreadPool)
{
    CARBON_PRECONDITION(AtA.rows() == AtA.cols(), "AtA is not square: {}x{}", AtA.rows(), AtA.cols());
    CARBON_PRECONDITION(AtA.rows() == A.cols(), "Number of columns of A ({}) does not match AtA ({})", A.cols(), AtA.cols());

    #if defined(EIGEN_USE_BLAS)
    // use BLAS if available
    AtA.template triangularView<Eigen::Lower>() += scale * (A.transpose() * A);
    #else

    if (taskThreadPool && (A.rows() > 1000))
    {
        const int numAvailableThreads = std::max(int(taskThreadPool->NumThreads()), 1);
        // to calculate the lower triangular matrix in blocks we require n * (n + 1) / 2 threads, where n is the split in row/col of AtA
        const int numSplits = std::max(int((-1.0f + sqrt(1.0f + 8.0f * float(numAvailableThreads))) / 2.0f), 1);
        const int blockSize = int(AtA.rows()) / numSplits;
        const int lastBlockSize = int(AtA.rows()) - (numSplits - 1) * blockSize;

        TaskFutures taskFutures;
        taskFutures.Reserve(numSplits * (numSplits + 1) / 2);
        for (int ri = 0; ri < numSplits; ++ri)
        {
            for (int ci = 0; ci <= ri; ++ci)
            {
                taskFutures.Add(taskThreadPool->AddTask(std::bind([&](int r, int c){
                        const int rb = (r == numSplits - 1) ? lastBlockSize : blockSize;
                        const int cb = (c == numSplits - 1) ? lastBlockSize : blockSize;
                        if (r == c)
                        {
                            AtA.block(r * blockSize, c * blockSize, rb,
                                      cb).template triangularView<Eigen::Lower>() += scale *
                                (A.block(0, r * blockSize, A.rows(), rb).transpose() * A.block(0, c * blockSize, A.rows(), cb));
                        }
                        else
                        {
                            AtA.block(r * blockSize, c * blockSize, rb,
                                      cb).noalias() += scale * (A.block(0, r * blockSize, A.rows(), rb).transpose() * A.block(0, c * blockSize, A.rows(), cb));
                        }
                    }, ri, ci)));
            }
        }
        taskFutures.Wait();
    }
    else
    {
        AtA.template triangularView<Eigen::Lower>() += scale * (A.transpose() * A);
    }
    #endif
}

template <class T>
void ParallelAtALower(Eigen::Ref<Eigen::Matrix<T, -1, -1>> AtA,
                      const Eigen::Ref<const Eigen::Matrix<T, -1, -1, Eigen::RowMajor>, 0, Eigen::OuterStride<-1>>& A,
                      TaskThreadPool* taskThreadPool)
{
    CARBON_PRECONDITION(AtA.rows() == AtA.cols(), "AtA is not square: {}x{}", AtA.rows(), AtA.cols());
    CARBON_PRECONDITION(AtA.rows() == A.cols(), "Number of columns of A ({}) does not match AtA ({})", A.cols(), AtA.cols());

    #if defined(EIGEN_USE_BLAS)
    // use BLAS if available
    AtA.template triangularView<Eigen::Lower>() = A.transpose() * A;
    #else

    if (taskThreadPool && (A.rows() > 1000))
    {
        const int numAvailableThreads = std::max(int(taskThreadPool->NumThreads()), 1);
        // to calculate the lower triangular matrix in blocks we require n * (n + 1) / 2 threads, where n is the split in row/col of AtA
        const int numSplits = std::max(int((-1.0f + sqrt(1.0f + 8.0f * float(numAvailableThreads))) / 2.0f), 1);
        const int blockSize = int(AtA.rows()) / numSplits;
        const int lastBlockSize = int(AtA.rows()) - (numSplits - 1) * blockSize;

        TaskFutures taskFutures;
        taskFutures.Reserve(numSplits * (numSplits + 1) / 2);
        for (int ri = 0; ri < numSplits; ++ri)
        {
            for (int ci = 0; ci <= ri; ++ci)
            {
                taskFutures.Add(taskThreadPool->AddTask(std::bind([&](int r, int c){
                        const int rb = (r == numSplits - 1) ? lastBlockSize : blockSize;
                        const int cb = (c == numSplits - 1) ? lastBlockSize : blockSize;
                        if (r == c)
                        {
                            AtA.block(r * blockSize, c * blockSize, rb,
                                      cb).template triangularView<Eigen::Lower>() =
                                A.block(0, r * blockSize, A.rows(), rb).transpose() * A.block(0, c * blockSize, A.rows(), cb);
                        }
                        else
                        {
                            AtA.block(r * blockSize, c * blockSize, rb,
                                      cb).noalias() = A.block(0, r * blockSize, A.rows(), rb).transpose() * A.block(0, c * blockSize, A.rows(), cb);
                        }
                    }, ri, ci)));
            }
        }
        taskFutures.Wait();
    }
    else
    {
        AtA.template triangularView<Eigen::Lower>() = A.transpose() * A;
    }
    #endif
}

template void ParallelNoAliasGEMV(Eigen::Ref<Eigen::VectorX<float>> out, const Eigen::Ref<const Eigen::Matrix<float, -1, -1, Eigen::RowMajor>, 0,
                                                                                          Eigen::OuterStride<-1>>& A,
                                  const Eigen::Ref<const Eigen::VectorX<float>>& x, TaskThreadPool* taskThreadPool);
template void ParallelNoAliasGEMV(Eigen::Ref<Eigen::VectorX<double>> out, const Eigen::Ref<const Eigen::Matrix<double, -1, -1, Eigen::RowMajor>, 0,
                                                                                           Eigen::OuterStride<-1>>& A,
                                  const Eigen::Ref<const Eigen::VectorX<double>>& x, TaskThreadPool* taskThreadPool);
template void ParallelNoAliasGEMV(Eigen::Ref<Eigen::VectorX<float>> out, const Eigen::SparseMatrix<float, Eigen::RowMajor>& A,
                                  const Eigen::Ref<const Eigen::VectorX<float>>& x, TaskThreadPool* taskThreadPool);
template void ParallelNoAliasGEMV(Eigen::Ref<Eigen::VectorX<double>> out, const Eigen::SparseMatrix<double, Eigen::RowMajor>& A,
                                  const Eigen::Ref<const Eigen::VectorX<double>>& x, TaskThreadPool* taskThreadPool);
template void ParallelNoAliasGEMV(Eigen::Ref<Eigen::VectorX<float>> out, const Eigen::Ref<const Eigen::VectorX<float>>& b,
                                  const Eigen::Ref<const Eigen::Matrix<float, -1, -1, Eigen::RowMajor>, 0, Eigen::OuterStride<-1>>& A,
                                  const Eigen::Ref<const Eigen::VectorX<float>>& x, TaskThreadPool* taskThreadPool);
template void ParallelNoAliasGEMV(Eigen::Ref<Eigen::VectorX<double>> out, const Eigen::Ref<const Eigen::VectorX<double>>& b,
                                  const Eigen::Ref<const Eigen::Matrix<double, -1, -1, Eigen::RowMajor>, 0, Eigen::OuterStride<-1>>& A,
                                  const Eigen::Ref<const Eigen::VectorX<double>>& x, TaskThreadPool* taskThreadPool);
template void ParallelAtALowerAdd(Eigen::Ref<Eigen::Matrix<float, -1, -1>> AtA, const Eigen::Ref<const Eigen::Matrix<float, -1, -1, Eigen::RowMajor>, 0,
                                                                                                 Eigen::OuterStride<-1>>& A, TaskThreadPool* taskThreadPool);
template void ParallelAtALowerAdd(Eigen::Ref<Eigen::Matrix<double, -1, -1>> AtA, const Eigen::Ref<const Eigen::Matrix<double, -1, -1, Eigen::RowMajor>, 0,
                                                                                                  Eigen::OuterStride<-1>>& A, TaskThreadPool* taskThreadPool);
template void ParallelAtALowerAdd(Eigen::Ref<Eigen::Matrix<float, -1, -1>> AtA, const Eigen::Ref<const Eigen::Matrix<float, -1, -1, Eigen::RowMajor>, 0,
                                                                                                 Eigen::OuterStride<-1>>& A, float scale, TaskThreadPool* taskThreadPool);
template void ParallelAtALowerAdd(Eigen::Ref<Eigen::Matrix<double, -1, -1>> AtA, const Eigen::Ref<const Eigen::Matrix<double, -1, -1, Eigen::RowMajor>, 0,
                                                                                                  Eigen::OuterStride<-1>>& A, double scale, TaskThreadPool* taskThreadPool);
template void ParallelAtALower(Eigen::Ref<Eigen::Matrix<float, -1, -1>> AtA, const Eigen::Ref<const Eigen::Matrix<float, -1, -1, Eigen::RowMajor>, 0,
                                                                                              Eigen::OuterStride<-1>>& A, TaskThreadPool* taskThreadPool);
template void ParallelAtALower(Eigen::Ref<Eigen::Matrix<double, -1, -1>> AtA, const Eigen::Ref<const Eigen::Matrix<double, -1, -1, Eigen::RowMajor>, 0,
                                                                                               Eigen::OuterStride<-1>>& A, TaskThreadPool* taskThreadPool);

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

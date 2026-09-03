// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/math/Math.h>
#include <nls/math/ParallelBLAS.h>
#include <carbon/utils/TaskThreadPool.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Preconditioned Conjugate Gradient that uses multi-threading for matrix vector multiplication.
 * Only supports row-major matrices (both dense and sparse).
 */
template <typename MatrixType>
class PCG {
    using T = typename MatrixType::Scalar;

public:
    PCG(int size = 0, const std::shared_ptr<TaskThreadPool>& threadPool = TaskThreadPool::GlobalInstance(false))
        : m_xrpqzc(size, 6)
        , m_threadPool(threadPool)
    {
    }

    // Solve Ax = b
    Eigen::Ref<Eigen::VectorX<T>> Solve(int iterations, const MatrixType& A, const Eigen::VectorX<T>& rhs, const Eigen::VectorX<T>& xInit)
    {
        const int size = (int)A.cols();
        if ((int)m_xrpqzc.rows() != size) {
            m_xrpqzc.resize(size, 6);
        }

        c() = Eigen::DiagonalPreconditioner<T>(A).solve(Eigen::VectorX<T>::Ones(A.cols()));

        if (xInit.size() > 0) {
            if (xInit.size() != x().size()) {
                CARBON_CRITICAL("invalid size for initial x: {} instead of {}", xInit.size(), x().size());
            }
            x() = xInit;
            // r() = rhs - A * x()
            ParallelNoAliasGEMV<T>(r(), A, x(), m_threadPool.get());
            r() = rhs - r();
        } else {
            x().setZero();
            r() = rhs;
        }

        const T bNormSquared = rhs.squaredNorm();
        if (bNormSquared == 0)
        {
            return x();
        }
        T residualNormSquared = r().squaredNorm();
        z() = c().array() * r().array(); //!< apply preconditioner
        p() = z();
        T delta = r().dot(p());

        m_tolError = T(1);
        m_resError = sqrt(residualNormSquared);
        const T threshold = std::max<T>(std::numeric_limits<T>::min(), bNormSquared * std::numeric_limits<T>::epsilon() * std::numeric_limits<T>::epsilon());

        if(residualNormSquared < threshold) {
            return x();
        }

        for (int iter = 0; iter < iterations; ++iter) {
            // q() = A * p()
            ParallelNoAliasGEMV<T>(q(), A, p(), m_threadPool.get());
            T alpha = delta / p().dot(q());
            x() += alpha * p();
            r() -= alpha * q();
            residualNormSquared = r().squaredNorm();
            if(residualNormSquared < threshold) {
                m_tolError = sqrt(residualNormSquared / bNormSquared);
                m_resError = sqrt(residualNormSquared);
                break;
            }
            z() = c().array() * r().array();  //!< apply preconditioner
            T deltaOld = delta;
            delta = r().dot(z());
            T beta = delta / deltaOld;
            p() = z() + beta * p();
            m_tolError = sqrt(residualNormSquared / bNormSquared);
            m_resError = sqrt(residualNormSquared);
        }

        return x();
    }

    Eigen::Ref<Eigen::VectorX<T>> x() { return m_xrpqzc.col(0); }
    Eigen::Ref<Eigen::VectorX<T>> r() { return m_xrpqzc.col(1); }
    Eigen::Ref<Eigen::VectorX<T>> p() { return m_xrpqzc.col(2); }
    Eigen::Ref<Eigen::VectorX<T>> q() { return m_xrpqzc.col(3); }
    Eigen::Ref<Eigen::VectorX<T>> z() { return m_xrpqzc.col(4); }
    Eigen::Ref<Eigen::VectorX<T>> c() { return m_xrpqzc.col(5); }

    Eigen::Ref<const Eigen::VectorX<T>> x() const { return m_xrpqzc.col(0); }
    Eigen::Ref<const Eigen::VectorX<T>> r() const { return m_xrpqzc.col(1); }
    Eigen::Ref<const Eigen::VectorX<T>> p() const { return m_xrpqzc.col(2); }
    Eigen::Ref<const Eigen::VectorX<T>> q() const { return m_xrpqzc.col(3); }
    Eigen::Ref<const Eigen::VectorX<T>> z() const { return m_xrpqzc.col(4); }
    Eigen::Ref<const Eigen::VectorX<T>> c() const { return m_xrpqzc.col(5); }

    const T TolError() const { return m_tolError; }
    const T ResError() const { return m_resError; }
    Eigen::Ref<const Eigen::VectorX<T>> Result() const { return x(); }

private:
    Eigen::Matrix<T, -1, 6> m_xrpqzc;

    T m_tolError{};
    T m_resError{};

    std::shared_ptr<TaskThreadPool> m_threadPool;
};


/**
 * Class to solve a ConjugateGradient Problem where the matrix multiplication residual = rhs - A * x is calculated
 * in multiple segments by a user provide CGProblem type i.e. residual = rhs - sum_i A[i] * x.
 * As opposed
 *
 * @tparam CGProblem  Needs to define all functions to solve the conjugate gradient problem including
 * - Rhs(); which returns the right-hand side of the problem residual = Rhs() - A x()
 * - DiagonalPreconditioner(): the diagonal preconditioner for matrix A
 * - NumSegments(): the number partial matrix multiplications
 * - MatrixMultiply(): the partial multiplication A(i) * x, where A * x = sum_i A(i) x
 */
template <typename CGProblem>
class ParallelPCG {
    using T = typename CGProblem::ScalarType;
public:
    ParallelPCG(const std::shared_ptr<TaskThreadPool>& threadPool)
    : m_threadPool(threadPool) {
        if (!m_threadPool) {
            CARBON_CRITICAL("parallel pcg requires a thread pool");
        }
    }

    // Solve Ax = b
    Eigen::Ref<Eigen::VectorX<T>> Solve(int iterations, const CGProblem& problem, const Eigen::VectorX<T>& xInit = Eigen::VectorX<T>())
    {
        const int size = (int)problem.Rhs().size();
        if ((int)m_xrpzc.rows() != size) {
            m_xrpzc.resize(size, 5);
        }
        if ((int)m_qs.cols() != problem.NumSegments() || (int)m_qs.rows() != size) {
            m_qs.resize(size, problem.NumSegments());
        }

        c() = problem.DiagonalPreconditioner();

        if (xInit.size() > 0) {
            if (xInit.size() != x().size()) {
                CARBON_CRITICAL("invalid size for initial x: {} instead of {}", xInit.size(), x().size());
            }
            x() = xInit;
            // r() = rhs - A * x()
            TaskFutures taskFutures;
            for (int i = 0; i < problem.NumSegments(); ++i) {
                taskFutures.Add(m_threadPool->AddTask(std::bind([&](int segmentId) {
                    problem.MatrixMultiply(q(segmentId), segmentId, x());
                }, i)));
            }
            taskFutures.Wait();
            for (int segmentId = 1; segmentId < problem.NumSegments(); ++segmentId) {
                q(0) += q(segmentId);
            }
            r() = problem.Rhs() - q(0);
        } else {
            x().setZero();
            r() = problem.Rhs();
        }


        const T bNormSquared = problem.Rhs().squaredNorm();
        if (bNormSquared == 0)
        {
            return x();
        }

        T residualNormSquared = r().squaredNorm();

        m_tolError = T(1);
        m_resError = sqrt(residualNormSquared);

        const T threshold = std::max<T>(std::numeric_limits<T>::min(), bNormSquared * std::numeric_limits<T>::epsilon() * std::numeric_limits<T>::epsilon());

        if(residualNormSquared < threshold) {
            return x();
        }

        z() = c().array() * r().array(); //!< apply preconditioner
        p() = z();
        T delta = r().dot(p());

        for (int iter = 0; iter < iterations; ++iter) {
            {
                // m_q.noalias() = A * m_p;
                TaskFutures taskFutures;
                for (int i = 0; i < problem.NumSegments(); ++i) {
                    taskFutures.Add(m_threadPool->AddTask(std::bind([&](int segmentId) {
			            problem.MatrixMultiply(q(segmentId), segmentId, p());
			        }, i)));
                }
                taskFutures.Wait();
                for (int segmentId = 1; segmentId < problem.NumSegments(); ++segmentId) {
                    q(0) += q(segmentId);
                }
            }
            T alpha = delta / p().dot(q(0));
            x() += alpha * p();
            r() -= alpha * q(0);
            residualNormSquared = r().squaredNorm();
            if(residualNormSquared < threshold) {
                m_tolError = sqrt(residualNormSquared / bNormSquared);
                m_resError = sqrt(residualNormSquared);
                break;
            }
            z() = c().array() * r().array();  //!< apply preconditioner
            T deltaOld = delta;
            delta = r().dot(z());
            T beta = delta / deltaOld;
            p() = z() + beta * p();
            m_tolError = sqrt(residualNormSquared / bNormSquared);
            m_resError = sqrt(residualNormSquared);
        }

        return x();
    }

    Eigen::Ref<Eigen::VectorX<T>> x() { return m_xrpzc.col(0); }
    Eigen::Ref<Eigen::VectorX<T>> r() { return m_xrpzc.col(1); }
    Eigen::Ref<Eigen::VectorX<T>> p() { return m_xrpzc.col(2); }
    Eigen::Ref<Eigen::VectorX<T>> z() { return m_xrpzc.col(3); }
    Eigen::Ref<Eigen::VectorX<T>> c() { return m_xrpzc.col(4); }

    Eigen::Ref<const Eigen::VectorX<T>> x() const { return m_xrpzc.col(0); }
    Eigen::Ref<const Eigen::VectorX<T>> r() const { return m_xrpzc.col(1); }
    Eigen::Ref<const Eigen::VectorX<T>> p() const { return m_xrpzc.col(2); }
    Eigen::Ref<const Eigen::VectorX<T>> z() const { return m_xrpzc.col(3); }
    Eigen::Ref<const Eigen::VectorX<T>> c() const { return m_xrpzc.col(4); }

    Eigen::Ref<Eigen::VectorX<T>> q(int i) { return m_qs.col(i); }
    Eigen::Ref<const Eigen::VectorX<T>> q(int i) const { return m_qs.col(i); }

    T TolError() const { return m_tolError; }
    T ResidualError() const { return m_resError; }

private:
    Eigen::Matrix<T, -1, 5> m_xrpzc;
    Eigen::Matrix<T, -1, -1> m_qs;

    T m_tolError;
    T m_resError;

    std::shared_ptr<TaskThreadPool> m_threadPool;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

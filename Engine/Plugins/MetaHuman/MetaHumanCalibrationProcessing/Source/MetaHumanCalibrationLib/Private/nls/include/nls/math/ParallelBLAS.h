// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/math/Math.h>
#include <carbon/utils/TaskThreadPool.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

//! Method to calculate out = A * x with explicit multi-threading.
template <class T>
void ParallelNoAliasGEMV(Eigen::Ref<Eigen::VectorX<T>> out,
                         const Eigen::Ref<const Eigen::Matrix<T, -1, -1, Eigen::RowMajor>, 0, Eigen::OuterStride<-1>>& A,
                         const Eigen::Ref<const Eigen::VectorX<T>>& x,
                         TaskThreadPool* taskThreadPool);

//! Method to calculate out = A * x for a sparse matrix with explicit multi-threading.
template <class T>
void ParallelNoAliasGEMV(Eigen::Ref<Eigen::VectorX<T>> out,
                         const Eigen::SparseMatrix<T, Eigen::RowMajor>& A,
                         const Eigen::Ref<const Eigen::VectorX<T>>& x,
                         TaskThreadPool* taskThreadPool);

/**
 * Method to calculate out = A * x + b with explicit multi-threading. It is only used in case MKL is not available, as MKL will
 * perform multi-threading for matrix multiplication.
 */
template <class T>
void ParallelNoAliasGEMV(Eigen::Ref<Eigen::VectorX<T>> out,
                         const Eigen::Ref<const Eigen::VectorX<T>>& b,
                         const Eigen::Ref<const Eigen::Matrix<T, -1, -1, Eigen::RowMajor>, 0, Eigen::OuterStride<-1>>& A,
                         const Eigen::Ref<const Eigen::VectorX<T>>& x,
                         TaskThreadPool* taskThreadPool);

/**
 * Method to calculate the lower triangular matrix of AtA with multi-threading. It is only used in case MKL is not available, as MKL will
 * perform multi-threading for matrix multiplication.
 * AtA.triangularView<Eigen::Lower>() += A.transpose() * A
 */
template <class T>
void ParallelAtALowerAdd(Eigen::Ref<Eigen::Matrix<T, -1, -1>> AtA,
                         const Eigen::Ref<const Eigen::Matrix<T, -1, -1, Eigen::RowMajor>, 0, Eigen::OuterStride<-1>>& A,
                         TaskThreadPool* taskThreadPool);

/**
 * Method to calculate the lower triangular matrix of scaled AtA with multi-threading. It is only used in case MKL is not available, as MKL will
 * perform multi-threading for matrix multiplication.
 * AtA.triangularView<Eigen::Lower>() += scale * A.transpose() * A
 */
template <class T>
void ParallelAtALowerAdd(Eigen::Ref<Eigen::Matrix<T, -1, -1>> AtA,
                         const Eigen::Ref<const Eigen::Matrix<T, -1, -1, Eigen::RowMajor>, 0, Eigen::OuterStride<-1>>& A,
                         T scale,
                         TaskThreadPool* taskThreadPool);

/**
 * Method to calculate the lower triangular matrix of AtA with multi-threading. It is only used in case MKL is not available, as MKL will
 * perform multi-threading for matrix multiplication.
 * AtA.triangularView<Eigen::Lower>() = A.transpose() * A
 */
template <class T>
void ParallelAtALower(Eigen::Ref<Eigen::Matrix<T, -1, -1>> AtA,
                      const Eigen::Ref<const Eigen::Matrix<T, -1, -1, Eigen::RowMajor>, 0, Eigen::OuterStride<-1>>& A,
                      TaskThreadPool* taskThreadPool);

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

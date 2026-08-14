// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/math/Math.h>

#ifdef EIGEN_USE_MKL_ALL
    #include <nls/math/MKLWrapper.h>
#endif

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T, int Options>
void SparseMatrixReorder(Eigen::SparseMatrix<T, Options>& A)
{
    #ifdef EIGEN_USE_MKL_ALL
    mkl::SparseMatrixReorder(A);
    #else
    // use eigen to reorder by tranposing the matrix twice
    SparseMatrix<T> other = A.transpose();
    A = other.transpose();
    #endif
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

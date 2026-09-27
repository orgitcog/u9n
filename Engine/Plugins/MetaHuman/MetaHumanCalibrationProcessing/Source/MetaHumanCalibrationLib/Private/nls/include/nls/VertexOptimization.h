// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <carbon/utils/TaskThreadPool.h>
#include <carbon/utils/Timer.h>
#include <nls/solver/GaussNewtonSolver.h>
#include <nls/geometry/Mesh.h>
#include <nls/geometry/VertexConstraints.h>
#include <nls/geometry/VertexLaplacian.h>
#include <nls/math/ParallelBLAS.h>
#include <nls/math/PCG.h>

#include <numeric>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

//! Parallel evaluation of laplacian multiplication including extending the multiplication to multiple dimensions
template <typename T, int C>
void LaplacianMultiplication(Eigen::Ref<Eigen::VectorX<T>> out,
                             const Eigen::SparseMatrix<T, Eigen::RowMajor>& A,
                             const Eigen::Ref<const Eigen::VectorX<T>>& x,
                             int rows,
                             int cols)
{
    CARBON_PRECONDITION(rows <= (int)A.rows(), "Number of rows ({}) to multiply is larger than size of matrix ({})", rows, A.rows());
    CARBON_PRECONDITION(rows * C == out.size(), "Number of rows ({}) does not match size of output ({})", rows, out.size());
    CARBON_PRECONDITION(cols * C == x.size(), "Number of columns of A ({}) does not match size of x ({}).", cols * C, x.size());

    auto func = [&](int start, int end) {
            const int* rowIndices = A.outerIndexPtr();
            const int* colIndices = A.innerIndexPtr();
            const T* values = A.valuePtr();
            for (int i = start; i < end; ++i)
            {
                Eigen::Vector<T, C> result = Eigen::Vector<T, C>::Zero();
                for (int k = rowIndices[i]; k < rowIndices[i + 1]; ++k)
                {
                    result += values[k] * x.segment(C * colIndices[k], C);
                }
                out.segment(C * i, C) = result;
            }
        };

    std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> globalThreadPool = TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(/*createIfNotAvailable=*/false);
    if (globalThreadPool && (rows > 1000))
    {
        globalThreadPool->AddTaskRangeAndWait(rows, func, 8);
    }
    else
    {
        func(0, rows);
    }
}

//! Class supporting vertex optimization problems
template <class T>
struct VertexOptimization
{
    using ScalarType = T;

    int numVertices;

    //! Base Laplacian
    Eigen::SparseMatrix<T, Eigen::RowMajor> baseLtL;
    //! Temporary laplacian with removed "fixed" rows
    Eigen::SparseMatrix<T, Eigen::RowMajor> tmpLtL;
    //! Final laplacian with removed "fixed" rows and columns
    Eigen::SparseMatrix<T, Eigen::RowMajor> LtL;

    std::vector<Eigen::Matrix<T, 3, 3>> diagonalBlocks;
    std::vector<bool> validDiagonal;
    std::vector<int> vertexMap;

    std::vector<Eigen::Matrix<T, 3, 3>> offDiagonalBlocks;
    std::vector<std::pair<int, int>> offDiagonalIndices;

    Eigen::VectorX<T> Jtb;

    std::shared_ptr<TITAN_NAMESPACE::TaskThreadPool> threadPool;

    int NumUnknowns() const { return (int)Jtb.size(); }
    int NumVariableVertices() const { return NumUnknowns() / 3; }
    int NumVertices() const { return numVertices; }
    bool HasFixedVertices() const { return (NumVariableVertices() != NumVertices()); }

    void SetTopology(const Mesh<T>& mesh)
    {
        const Eigen::SparseMatrix<T,
                                  Eigen::RowMajor> vertexLaplacian = VertexLaplacian<T>::LaplacianMatrix(mesh, VertexLaplacian<T>::Type::MEANVALUE, /*dim=*/1);
        baseLtL = vertexLaplacian.transpose() * vertexLaplacian;
        baseLtL.makeCompressed();
        tmpLtL = baseLtL;
        LtL = baseLtL;
        numVertices = mesh.NumVertices();
        diagonalBlocks.resize(numVertices);
        validDiagonal = std::vector<bool>(numVertices, false);
        vertexMap = std::vector<int>(numVertices);
        std::iota(vertexMap.begin(), vertexMap.end(), 0);
        offDiagonalBlocks.clear();
        offDiagonalIndices.clear();
        Jtb = Eigen::VectorX<T>::Zero(numVertices * 3);

        threadPool = TITAN_NAMESPACE::TaskThreadPool::GlobalInstance(/*createIfNotAvailable=*/true);
    }

    void Clear(const std::vector<int>& fixVertices = {})
    {
        std::fill(validDiagonal.begin(), validDiagonal.end(), false);
        offDiagonalBlocks.clear();
        offDiagonalIndices.clear();
        if (fixVertices.size() > 0)
        {
            std::vector<bool> isFixed(NumVertices(), false);
            for (int vID : fixVertices)
            {
                isFixed[vID] = true;
            }
            int mappedID = 0;
            for (size_t i = 0; i < isFixed.size(); ++i)
            {
                if (isFixed[i])
                {
                    vertexMap[i] = -1;
                }
                else
                {
                    vertexMap[i] = mappedID++;
                }
            }
            Jtb = Eigen::VectorX<T>::Zero(mappedID * 3);
            if (NumVariableVertices() != mappedID)
            {
                CARBON_CRITICAL("logic error in vertex problem");
            }
        }
        else
        {
            if ((int)Jtb.size() == numVertices * 3)
            {
                Jtb.setZero();
            }
            else
            {
                std::iota(vertexMap.begin(), vertexMap.end(), 0);
                Jtb = Eigen::VectorX<T>::Zero(numVertices * 3);
            }
        }
    }

    void Setup(const Eigen::Matrix<T, 3, -1>& vertexOffsets, T laplacianRegularization, T offsetRegularization, T updateRegularization)
    {
        Eigen::VectorX<T> res(NumUnknowns());
        if (HasFixedVertices())
        {
            {
                // update the LtL matrix with the laplacian and the offset regularization
                // remove the rows of the LtL matrix that are fixed
                // rows of the LtL matrix that are relevant
                const int* rowIndices = baseLtL.outerIndexPtr();
                const int* colIndices = baseLtL.innerIndexPtr();
                const T* values = baseLtL.valuePtr();
                int* modRowIndices = tmpLtL.outerIndexPtr();
                int* modColIndices = tmpLtL.innerIndexPtr();
                T* modValues = tmpLtL.valuePtr();
                int outIdx = 0;
                int lastvID = -1;
                for (int i = 0; i < NumVertices(); ++i)
                {
                    const int vID = vertexMap[i];
                    if (vID >= 0)
                    {
                        if (vID != lastvID + 1)
                        {
                            CARBON_CRITICAL("vids not matching");
                        }
                        modRowIndices[vID] = outIdx;
                        for (int inIdx = rowIndices[i]; inIdx < rowIndices[i + 1]; ++inIdx)
                        {
                            modColIndices[outIdx] = colIndices[inIdx];
                            modValues[outIdx] = laplacianRegularization * values[inIdx];
                            if (colIndices[inIdx] == i)
                            {
                                // add the offset regularization which is an addition to the diagonal of the LtL matrix
                                // loss: sum_i 0.5 offsetRegularization *  || vertexOffset_i + dx_i ||^2
                                modValues[outIdx] += offsetRegularization;
                            }
                            outIdx++;
                        }
                        modRowIndices[vID + 1] = outIdx;
                        lastvID = vID;
                    }
                }
            }
            LaplacianMultiplication<T, 3>(res, tmpLtL, Eigen::Map<const Eigen::VectorX<T>>(vertexOffsets.data(), vertexOffsets.size()), NumVariableVertices(), NumVertices());
            {
                // remove the columns of the LtL matrix that are fixed
                const int* rowIndices = tmpLtL.outerIndexPtr();
                const int* colIndices = tmpLtL.innerIndexPtr();
                const T* values = tmpLtL.valuePtr();
                int* modRowIndices = LtL.outerIndexPtr();
                int* modColIndices = LtL.innerIndexPtr();
                T* modValues = LtL.valuePtr();
                int outIdx = 0;
                for (int i = 0; i < NumVariableVertices(); ++i)
                {
                    modRowIndices[i] = outIdx;
                    for (int inIdx = rowIndices[i]; inIdx < rowIndices[i + 1]; ++inIdx)
                    {
                        const int loc = vertexMap[colIndices[inIdx]];
                        if (loc >= 0)
                        {
                            modColIndices[outIdx] = loc;
                            modValues[outIdx] = values[inIdx];
                            outIdx++;
                        }
                    }
                    modRowIndices[i + 1] = outIdx;
                }
            }
        }
        else
        {
            // update the LtL matrix with the laplacian and the offset regularization
            const int* rowIndices = baseLtL.outerIndexPtr();
            const int* colIndices = baseLtL.innerIndexPtr();
            const T* values = baseLtL.valuePtr();
            T* modValues = LtL.valuePtr();
            for (int i = 0; i < NumVertices(); ++i)
            {
                for (int j = rowIndices[i]; j < rowIndices[i + 1]; ++j)
                {
                    modValues[j] = laplacianRegularization * values[j];
                    if (colIndices[j] == i)
                    {
                        // add the offset regularization which is an addition to the diagonal of the LtL matrix
                        // loss: sum_i 0.5 offsetRegularization *  || vertexOffset_i + dx_i ||^2
                        modValues[j] += offsetRegularization;
                    }
                }
            }
            LaplacianMultiplication<T, 3>(res, LtL, Eigen::Map<const Eigen::VectorX<T>>(vertexOffsets.data(), vertexOffsets.size()), (int)LtL.rows(), NumVertices());
        }

        Jtb -= res;

        // add the update regularization which is an addition to the diagonal of the LtL matrix
        // loss: sum_i 0.5 updateRegularization *  || dx_i ||^2
        for (int i = 0; i < NumVariableVertices(); ++i)
        {
            for (typename Eigen::SparseMatrix<T, Eigen::RowMajor>::InnerIterator it(LtL, i); it; ++it)
            {
                if (it.col() == i)
                {
                    it.valueRef() += updateRegularization;
                }
            }
        }

        // for (int i = 0; i < NumVariableVertices(); ++i) {
        // for (int j = rowIndices[i]; j < rowIndices[i + 1]; ++j) {
        // if (colIndices[j] == i) {
        // modValues[j] += updateRegularization;
        // }
        // }
        // }
    }

    template <int ResidualSize, int NumConstraintVertices>
    void AddConstraints(const VertexConstraints<T, ResidualSize, NumConstraintVertices>& vertexConstraints)
    {
        for (int i = 0; i < vertexConstraints.NumberOfConstraints(); ++i)
        {
            const auto& j = vertexConstraints.Jacobians()[i];
            const auto& weights = vertexConstraints.WeightsPerVertex()[i];
            const auto& vIDs = vertexConstraints.VertexIDs()[i];
            const Eigen::Matrix<T, 3, 3> jtj = j.transpose() * j;
            for (int e = 0; e < NumConstraintVertices; ++e)
            {
                const int vID = vIDs[e];
                const int loc = vertexMap[vID];
                if (loc < 0) { continue; }
                const T weightSquared = weights[e] * weights[e];
                if (validDiagonal[loc])
                {
                    diagonalBlocks[loc] += weightSquared * jtj;
                }
                else
                {
                    diagonalBlocks[loc] = weightSquared * jtj;
                    validDiagonal[loc] = true;
                }
                Jtb.segment(3 * loc, 3) += -weights[e] * j.transpose() * vertexConstraints.Residual().segment(ResidualSize * i, ResidualSize);
            }

            for (int e1 = 0; e1 < NumConstraintVertices; ++e1)
            {
                const int loc1 = vertexMap[vIDs[e1]];
                if (loc1 < 0) { continue; }
                for (int e2 = e1 + 1; e2 < NumConstraintVertices; ++e2)
                {
                    const int loc2 = vertexMap[vIDs[e2]];
                    if (loc2 < 0) { continue; }
                    offDiagonalBlocks.push_back(weights[e1] * weights[e2] * jtj);
                    offDiagonalIndices.push_back({ loc1, loc2 });
                }
            }
        }
    }

    const Eigen::VectorX<T>& Rhs() const { return Jtb; }

    Eigen::VectorX<T> DiagonalPreconditioner() const
    {
        Eigen::VectorX<T> diag = Eigen::VectorX<T>::Ones(NumUnknowns());
        const int* rowIndices = LtL.outerIndexPtr();
        const int* colIndices = LtL.innerIndexPtr();
        const T* values = LtL.valuePtr();
        for (int i = 0; i < NumVariableVertices(); ++i)
        {
            for (int j = rowIndices[i]; j < rowIndices[i + 1]; ++j)
            {
                if (colIndices[j] == i)
                {
                    diag.segment(3 * i, 3).fill(values[j]);
                }
            }
        }

        for (int i = 0; i < NumVariableVertices(); ++i)
        {
            if (validDiagonal[i])
            {
                diag.segment(3 * i, 3) += diagonalBlocks[i].diagonal();
            }
        }

        for (int i = 0; i < (int)diag.size(); ++i)
        {
            if (diag[i] != 0)
            {
                diag[i] = T(1) / diag[i];
            }
        }
        return diag;
    }

    int NumSegments() const { return 2; }

    void MatrixMultiply(Eigen::Ref<Eigen::VectorX<T>> out, int segmentId, Eigen::Ref<const Eigen::VectorX<T>> x) const
    {
        if (segmentId == 0)
        {
            out.setZero();
            for (int i = 0; i < NumVariableVertices(); ++i)
            {
                if (validDiagonal[i])
                {
                    out.segment(3 * i, 3) += diagonalBlocks[i] * x.segment(3 * i, 3);
                }
            }
            for (size_t i = 0; i < offDiagonalBlocks.size(); ++i)
            {
                const int vID1 = offDiagonalIndices[i].first;
                const int vID2 = offDiagonalIndices[i].second;
                out.segment(3 * vID2, 3) += offDiagonalBlocks[i] * x.segment(3 * vID1, 3);
                out.segment(3 * vID1, 3) += offDiagonalBlocks[i] * x.segment(3 * vID2, 3);
            }
        }
        else
        {
            LaplacianMultiplication<T, 3>(out, LtL, x, NumVariableVertices(), NumVariableVertices());
        }
    }

    Eigen::Matrix<T, 3, -1> Solve(int cgIterations) const
    {
        ParallelPCG<VertexOptimization<T>> solver(threadPool);
        const Eigen::VectorX<T> dx = solver.Solve(cgIterations, *this);
        if (HasFixedVertices())
        {
            // the solve contains fixed vertices, and hence we remap the result back
            Eigen::Matrix<T, 3, -1> result = Eigen::Matrix<T, 3, -1>::Zero(3, NumVertices());
            for (int vID = 0; vID < NumVertices(); ++vID)
            {
                const int loc = vertexMap[vID];
                if (loc >= 0)
                {
                    result.col(vID) = dx.segment(3 * loc, 3);
                }
            }
            return result;
        }
        else
        {
            // all vertices are solved, so we can simply return the result
            return Eigen::Map<const Eigen::Matrix<T, 3, -1>>(dx.data(), 3, dx.size() / 3);
        }
    }
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

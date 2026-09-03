// Copyright Epic Games, Inc. All Rights Reserved.

#include <nls/Cost.h>
#include <nls/math/Math.h>

#include <limits>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
Eigen::VectorX<T> Cost<T>::Value() const
{
    if (m_costTerms.size() == 0)
    {
        return Vector<T>();
    }
    else if (m_costTerms.size() == 1)
    {
        if (m_costTerms[0].weight == T(1))
        {
            return m_costTerms[0].diffdata.Value();
        }
        const CostTerm& costTerm = m_costTerms[0];
        const T sqrtWeight = std::sqrt(costTerm.weight);
        return sqrtWeight * costTerm.diffdata.Value();
    }
    else
    {
        int offset = 0;
        Vector<T> values(Size());
        for (int i = 0; i < NumTerms(); ++i)
        {
            const int rows = int(m_costTerms[i].diffdata.Value().size());
            values.segment(offset, rows) = std::sqrt(m_costTerms[i].weight) * m_costTerms[i].diffdata.Value();
            offset += rows;
        }
        return values;
    }
}

template <class T>
DiffData<T> Cost<T>::CostToDiffData() const
{
    if (m_costTerms.size() == 0)
    {
        return DiffData<T>(Vector<T>());
    }

    if (m_costTerms.size() == 1)
    {
        if (m_costTerms[0].weight == T(1))
        {
            return m_costTerms[0].diffdata.Clone();
        }
        const CostTerm& costTerm = m_costTerms[0];
        const T sqrtWeight = std::sqrt(costTerm.weight);
        if (costTerm.diffdata.HasJacobian())
        {
            return DiffData<T>(sqrtWeight * costTerm.diffdata.Value(), costTerm.diffdata.Jacobian().Scale(sqrtWeight));
        }
        else
        {
            return DiffData<T>(sqrtWeight * costTerm.diffdata.Value());
        }
    }

    // Get total number of rows and the maximum column size. Remember that not all Jacobians
    // must have the same column size. However each column represents the same variable assuming
    // the mapping has been done with the same Context.
    int totalRows = 0;
    int startCol = std::numeric_limits<int>::max();
    int maxCols = 0;
    std::vector<int> rowOffsets;
    bool anyJacobian = false;
    for (const CostTerm& costTerm : m_costTerms)
    {
        rowOffsets.push_back(totalRows);
        totalRows += costTerm.diffdata.Size();
        if (costTerm.diffdata.HasJacobian())
        {
            maxCols = std::max<int>(costTerm.diffdata.Jacobian().Cols(), maxCols);
            startCol = std::min<int>(costTerm.diffdata.Jacobian().StartCol(), startCol);
            anyJacobian = true;
        }
    }

    // scale the values
    Vector<T> values(totalRows);
    for (int i = 0; i < NumTerms(); ++i)
    {
        values.segment(rowOffsets[i], m_costTerms[i].diffdata.Value().size()) = std::sqrt(m_costTerms[i].weight) * m_costTerms[i].diffdata.Value();
    }

    JacobianConstPtr<T> Jacobian;

    if (anyJacobian)
    {
        // we only need to concatenate the jacobians if there is any jacobian in the terms
        std::vector<SparseMatrixConstPtr<T>> jacobians;
        for (int i = 0; i < NumTerms(); ++i)
        {
            const CostTerm& costTerm = m_costTerms[i];
            const int rows = costTerm.diffdata.Size();
            if (costTerm.diffdata.HasJacobian())
            {
                SparseMatrixConstPtr<T> resizedJacobian;
                if (costTerm.weight == T(1))
                {
                    if (costTerm.diffdata.Jacobian().Cols() != maxCols)
                    {
                        // just resize
                        SparseMatrixPtr<T> jacobian = std::make_shared<SparseMatrix<T>>(*costTerm.diffdata.Jacobian().AsSparseMatrix());
                        // resize so that cols match for all jacobians
                        jacobian->conservativeResize(rows, maxCols);
                        resizedJacobian = jacobian;
                    }
                    else
                    {
                        resizedJacobian = costTerm.diffdata.Jacobian().AsSparseMatrix();
                    }
                }
                else
                {
                    // resize and scale
                    SparseMatrixPtr<T> scaledJacobian = std::make_shared<SparseMatrix<T>>(*costTerm.diffdata.Jacobian().Scale(std::sqrt(
                                                                                                                                  costTerm.weight))->AsSparseMatrix());
                    CARBON_ASSERT(scaledJacobian->cols() <= maxCols,
                                  "number of columns of resize jacobian need to be smaller or equal the total number of columns");
                    // resize so that cols match for all jacobians
                    scaledJacobian->conservativeResize(rows, maxCols);
                    resizedJacobian = scaledJacobian;
                }
                CARBON_ASSERT(resizedJacobian->rows() == rows, "rows of resized jacobian need to match value size");
                CARBON_ASSERT(resizedJacobian->cols() == maxCols, "number of columns of resized jacobian need to be equal the total number of columns");
                jacobians.push_back(resizedJacobian);
            }
            else
            {
                jacobians.push_back(std::make_shared<SparseMatrix<T>>(rows, maxCols));
            }
        }

        // concatenate the jacobians
        std::vector<std::reference_wrapper<const SparseMatrix<T>>> jacobianRefs;
        for (int i = 0; i < int(jacobians.size()); i++)
        {
            jacobianRefs.push_back(*jacobians[i]);
        }
        SparseMatrixPtr<T> concatenatedJacobian = std::make_shared<SparseMatrix<T>>();
        // TODO: potential optimization - put scaling and resizing into ConcatenateSparseMatricesAlongRowDimension
        ConcatenateSparseMatricesAlongRowDimension<T>(jacobianRefs, *concatenatedJacobian);
        Jacobian = std::make_shared<SparseJacobian<T>>(concatenatedJacobian, startCol);
    }

    return DiffData<T>(values, Jacobian);
}

template <class T>
bool Cost<T>::HasJacobian() const
{
    for (const auto& costTerm : m_costTerms)
    {
        if (!costTerm.diffdata.HasJacobian())
        {
            return false;
        }
    }
    return true;
}

template <class T>
int Cost<T>::Cols() const
{
    if (!HasJacobian())
    {
        CARBON_CRITICAL("cost terms do not contain a Jacobian");
    }
    int cols = 0;
    for (const auto& costTerm : m_costTerms)
    {
        cols = std::max<int>(cols, costTerm.diffdata.Jacobian().Cols());
    }
    return cols;
}

template <class T>
void Cost<T>::AddJx(Eigen::Ref<Eigen::VectorX<T>> result, const Eigen::Ref<const Eigen::VectorX<T>>& x, const T scale) const
{
    if (!HasJacobian())
    {
        CARBON_CRITICAL("cost terms do not contain a Jacobian");
    }
    if (int(result.size()) != Rows())
    {
        CARBON_CRITICAL("the result vector must match the size of the Cost terms");
    }
    if (int(x.size()) != Cols())
    {
        CARBON_CRITICAL("the input vector must match the number of variables of the cost terms");
    }

    int offset = 0;
    for (const auto& costTerm : m_costTerms)
    {
        const int rows = costTerm.diffdata.Jacobian().Rows();
        const int cols = costTerm.diffdata.Jacobian().Cols();
        costTerm.diffdata.Jacobian().AddJx(result.segment(offset, rows), x.segment(0, cols), (scale * std::sqrt(costTerm.weight)));
        offset += rows;
    }
}

template <class T>
void Cost<T>::AddJtx(Eigen::Ref<Eigen::VectorX<T>> result, const Eigen::Ref<const Eigen::VectorX<T>>& x, const T scale) const
{
    if (!HasJacobian())
    {
        CARBON_CRITICAL("cost terms do not contain a Jacobian");
    }
    if (int(x.size()) != Rows())
    {
        CARBON_CRITICAL("the result vector must match the number of variables of the cost terms");
    }
    if (int(result.size()) != Cols())
    {
        CARBON_CRITICAL("the input vector must match the size of the Cost terms: {} vs {}", int(result.size()), Cols());
    }

    int offset = 0;
    for (const auto& costTerm : m_costTerms)
    {
        const int rows = costTerm.diffdata.Jacobian().Rows();
        const int cols = costTerm.diffdata.Jacobian().Cols();
        costTerm.diffdata.Jacobian().AddJtx(result.segment(0, cols), x.segment(offset, rows), scale * std::sqrt(costTerm.weight));
        offset += rows;
    }
}

template <class T>
void Cost<T>::AddDenseJtJLower(Eigen::Ref<Eigen::Matrix<T, -1, -1>> JtJ, const T scale, TaskThreadPool* threadPool) const
{
    if (!HasJacobian())
    {
        CARBON_CRITICAL("cost terms do not contain a Jacobian");
    }

    for (const auto& costTerm : m_costTerms)
    {
        const int cols = costTerm.diffdata.Jacobian().Cols();
        costTerm.diffdata.Jacobian().AddDenseJtJLower(JtJ.block(0, 0, cols, cols), scale * costTerm.weight, threadPool);
    }
}

template <class T>
void Cost<T>::AddSparseJtJLower(std::vector<Eigen::Triplet<T>>& JtJ, const T scale) const
{
    if (!HasJacobian())
    {
        CARBON_CRITICAL("cost terms do not contain a Jacobian");
    }

    for (const auto& costTerm : m_costTerms)
    {
        costTerm.diffdata.Jacobian().AddSparseJtJLower(JtJ, scale * costTerm.weight);
    }
}

template class Cost<float>;
template class Cost<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

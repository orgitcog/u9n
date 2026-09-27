// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <carbon/utils/TaskThreadPool.h>
#include <nls/DiffData.h>

#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Cost
 */
template <class T>
class Cost
{
public:
    Cost() = default;
    ~Cost() = default;
    Cost(Cost&&) = default;
    Cost(const Cost&) = delete;
    Cost& operator=(Cost&&) = default;
    Cost& operator=(const Cost&) = delete;

    /**
     * @brief Add a least squares cost term: weight * || f(x) ||^2
     *
     * @param fx       The f(x) value (with or without Jacobian).
     * @param weight   The weight for the cost term.
     * @param name     An optional name for the cost term.
     * @param average  If True, then the cost term is calculating the average: weight * || f(x) ||^2 / size(f(x)).
     */
    void Add(DiffData<T>&& fx, T weight, const std::string& name = std::string(), bool average = false)
    {
        if ((fx.Size() > 0) && (weight > 0))
        {
            weight = average ? weight / T(fx.Size()) : weight;
            m_costTerms.push_back({ weight, std::move(fx), name });
        }
    }

    /**
     * @brief Add another cost term to this cost term: weight * cost(x)
     *
     * @param other    The other cost term.
     * @param weight   A weighting of the other cost term.
     * @param average  If True, then the cost term is added as an average: weight * cost(x) / size(f(x))
     */
    void Add(Cost&& other, T weight = T(1), bool average = false)
    {
        if ((other.NumTerms() > 0) && (weight > 0))
        {
            weight = average ? weight / T(other.Size()) : weight;
            for (int i = 0; i < other.NumTerms(); ++i)
            {
                m_costTerms.push_back({ weight* other.m_costTerms[i].weight, std::move(other.m_costTerms[i].diffdata), other.m_costTerms[i].name });
            }
        }
    }

    Eigen::VectorX<T> Value() const;

    DiffData<T> CostToDiffData() const;

    int NumTerms() const { return static_cast<int>(m_costTerms.size()); }

    int Size() const
    {
        int size = 0;
        for (const auto& costTerm : m_costTerms)
        {
            size += costTerm.diffdata.Size();
        }
        return size;
    }

    int Rows() const { return Size(); }

    //! @returns the number of columns of the Jacobian. @pre HasJacobian() is True.
    int Cols() const;

    //! result += scale * cost.Jacobian() * vec
    void AddJx(Eigen::Ref<Eigen::VectorX<T>> result, const Eigen::Ref<const Eigen::VectorX<T>>& x, const T scale) const;

    //! result += scale * cost.Jacobian().transpose() * vec
    void AddJtx(Eigen::Ref<Eigen::VectorX<T>> result, const Eigen::Ref<const Eigen::VectorX<T>>& x, const T scale) const;

    //! Calculates JtJ of the cost term and accumulates it into JtJ with additional scaling @p scale.
    void AddDenseJtJLower(Eigen::Ref<Eigen::Matrix<T, -1, -1>> JtJ, const T scale, TaskThreadPool* threadPool) const;

    //! Calculates sparse JtJ of the cost term and accumulates it as triplets with additional scaling @p scale.
    void AddSparseJtJLower(std::vector<Eigen::Triplet<T>>& JtJ, const T scale) const;

    //! @returns True if all cost terms have a Jacobian.
    bool HasJacobian() const;

private:
    struct CostTerm
    {
        T weight;
        DiffData<T> diffdata;
        std::string name;
    };

    std::vector<CostTerm> m_costTerms;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

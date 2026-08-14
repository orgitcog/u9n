// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/VectorVariable.h>

#include <limits>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * BoundedVectorVariable is a VectorVariable where each dimenion is bounded by some user defined bounds.
 * Note that the Jacobian is correct in the sense that clamping due to the bounds is not taken into account i.e. the Jacobian is the same as VectorVariable.
 */
template <class T>
class BoundedVectorVariable : public VectorVariable<T>
{
public:
    BoundedVectorVariable(int size) : VectorVariable<T>(size), m_enforceBounds(true)
    {
        SetUnbounded();
        SetRegularizationScaling(Eigen::VectorX<T>::Ones(size));
    }

    BoundedVectorVariable(const Vector<T>& vector) : VectorVariable<T>(vector), m_enforceBounds(true)
    {
        SetUnbounded();
        SetRegularizationScaling(Eigen::VectorX<T>::Ones(vector.size()));
    }

    BoundedVectorVariable(BoundedVectorVariable&& other) = default;
    BoundedVectorVariable(const BoundedVectorVariable& other) = default;
    BoundedVectorVariable& operator=(BoundedVectorVariable&& other) = default;
    BoundedVectorVariable& operator=(const BoundedVectorVariable& other) = default;

    /**
     * Change whether the bounds are enforced or not. If the bounds are not enforced then the variable behaves the same as a VectorVariable.
     */
    void EnforceBounds(bool enforce)
    {
        m_enforceBounds = enforce;
        if (m_enforceBounds)
        {
            // immediately set the value and therefore project the values to the bounds
            this->Set(this->Value());
        }
    }

    //! @return True if the bounds are enforced when setting/updating the value
    bool BoundsAreEnforced() const { return m_enforceBounds; }

    const Eigen::Matrix<T, 2, -1> Bounds() const { return m_bounds; }

    void SetBounds(const Eigen::Matrix<T, 2, -1>& bounds)
    {
        if (bounds.cols() != m_bounds.cols())
        {
            CARBON_CRITICAL("bounds matrix does not have the correct size");
        }
        m_bounds = bounds;
        if (m_enforceBounds)
        {
            // immediately set the value and therefore project the values to the bounds
            this->Set(this->Value());
        }
    }

    void SetBounds(int index, T minValue, T maxValue)
    {
        if ((index < 0) || (index >= this->Size()))
        {
            CARBON_CRITICAL("index out of bounds");
        }
        if (minValue > maxValue)
        {
            CARBON_CRITICAL("minimum bounds value needs to be smaller or equal to the maximum value");
        }
        m_bounds(0, index) = minValue;
        m_bounds(1, index) = maxValue;
    }

    void SetRegularizationScaling(const Eigen::VectorX<T>& regularizationScaling)
    {
        if ((int)regularizationScaling.size() != this->Size())
        {
            CARBON_CRITICAL("regularization scaling does not match variable size: {} instead of {}", regularizationScaling.size(), this->Size());
        }
        m_regularizationScaling = regularizationScaling;
    }

    const Eigen::VectorX<T>& RegularizationScaling() const { return m_regularizationScaling; }

private:
    //! Project the variables to the bounds
    virtual void ProjectToManifold(Vector<T>& value) override final
    {
        if (m_enforceBounds)
        {
            for (int i = 0; i < this->Size(); i++)
            {
                value[i] = clamp<T>(value[i], m_bounds(0, i), m_bounds(1, i));
            }
        }
    }

    //! Set the internal bounds to unbounded i.e. the range is to the minimum and maximum value
    void SetUnbounded()
    {
        m_bounds.resize(2, this->Size());
        m_bounds.row(0).setConstant(std::numeric_limits<T>::lowest());
        m_bounds.row(1).setConstant(std::numeric_limits<T>::max());
    }

private:
    bool m_enforceBounds;
    Eigen::Matrix<T, 2, -1> m_bounds;
    Eigen::VectorX<T> m_regularizationScaling;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

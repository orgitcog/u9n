// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/Jacobian.h>
#include <nls/math/Math.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Base data representation required for differentiable data with a vector of values and a Jacobian
 * values: f(x)
 * jacobian: df/dx
 */
template <class T>
class DiffData
{
public:
    DiffData(Vector<T>&& value, JacobianConstPtr<T>&& jacobian) : m_value(std::move(value)), m_jacobian(std::move(jacobian))
    {
        SanityCheck();
    }

    DiffData(Vector<T>&& value, const JacobianConstPtr<T>& jacobian) : m_value(std::move(value)), m_jacobian(jacobian)
    {
        SanityCheck();
    }

    DiffData(Vector<T>&& value) : DiffData(std::move(value), JacobianConstPtr<T>()) {}

    DiffData(const Vector<T>& value, const JacobianConstPtr<T>& jacobian) : m_value(value), m_jacobian(jacobian)
    {
        SanityCheck();
    }

    DiffData(const Vector<T>& value) : DiffData(value, JacobianConstPtr<T>()) {}

    //! constructor that explicitly copies the data
    DiffData(const T* values, int size) : DiffData(Eigen::Map<const Vector<T>>(values, size)) {}
    DiffData(const T* values, int size, const JacobianConstPtr<T>& jacobian) : DiffData(Eigen::Map<const Vector<T>>(values, size), jacobian) {}

    //! constructor that explicitly copies the data
    template <class S, int R, int C, typename DISCARD = typename std::enable_if<std::is_same<S, T>::value, void>::type>
    DiffData(const Eigen::Matrix<S, R, C>& mat) : DiffData((const T*)mat.data(), (int)mat.size()) {}

    //! constructor that explicitly copies the data
    template <class S, int R, int C, typename DISCARD = typename std::enable_if<std::is_same<S, T>::value, void>::type>
    DiffData(const Eigen::Matrix<S, R, C>& mat, const JacobianConstPtr<T>& jacobian) : DiffData((const T*)mat.data(), (int)mat.size(), jacobian) {}

    DiffData(const DiffData&) = delete;
    DiffData& operator=(const DiffData&) = delete;
    DiffData(DiffData&&) = default;
    DiffData& operator=(DiffData&&) = default;

    void SanityCheck() const
    {
        if (m_jacobian)
        {
            CARBON_PRECONDITION(m_jacobian->Rows() == int(m_value.size()), "jacobian needs to match value vector size");
        }
    }

    //! explicitly clone the diff data object as copy constructor is disabled (implementation may change in the future)
    DiffData<T> Clone() const { return DiffData<T>(Value(), m_jacobian); }

    int Size() const { return int(Value().size()); }
    const Vector<T>& Value() const { return m_value; }

    bool HasJacobian() const { return bool(m_jacobian.get()); }
    const JacobianConstPtr<T>& JacobianPtr() const { return m_jacobian; }
    const TITAN_NAMESPACE::Jacobian<T>& Jacobian() const { return *m_jacobian; }

    void SetJacobianPtr(const JacobianConstPtr<T>& jacobian) { m_jacobian = jacobian; }

    /**
     * Convenience function returning a reference to the value.
     * @warning Use with care as any copy of DiffData will have its value modified as well.
     */
    Vector<T>& MutableValue() { return m_value; }

    /**
     * Convenience function returning a reference to the jacobian ptr.
     */
    JacobianConstPtr<T>& MutableJacobianPtr() { return m_jacobian; }

private:
    Vector<T> m_value;
    JacobianConstPtr<T> m_jacobian;
};

//! Sparse multiplication (linear map) of input @p x.
template <class T>
DiffData<T> operator*(const SparseMatrix<T>& A, const DiffData<T>& x)
{
    CARBON_PRECONDITION((int)A.cols() == x.Size(), "mismatch of dimensions: {} vs {}", A.cols(), x.Size());
    return DiffData<T>(A * x.Value(), x.HasJacobian() ? x.Jacobian().Premultiply(A) : nullptr);
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

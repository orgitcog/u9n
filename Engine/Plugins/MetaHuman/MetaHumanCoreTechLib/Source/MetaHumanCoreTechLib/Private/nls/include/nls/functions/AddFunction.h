// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/DiffData.h>
#include <nls/DiffDataMatrix.h>
#include <nls/math/Math.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Function to add two value vectors: f(x) = a(x) + b(x)
 */
template <class T>
class AddFunction
{
public:
    AddFunction() {}

    DiffData<T> Evaluate(const DiffData<T>& a, const DiffData<T>& b) const
    {
        CARBON_PRECONDITION(a.Value().size() == b.Value().size(), "dimensions need to match for DiffData addition");

        JacobianConstPtr<T> Jacobian;
        if (a.HasJacobian() && b.HasJacobian())
        {
            // merge a and b jacobians i.e. add the two jacobians
            Jacobian = a.Jacobian().Add(b.JacobianPtr());
        }
        else if (a.HasJacobian())
        {
            Jacobian = a.JacobianPtr();
        }
        else if (b.HasJacobian())
        {
            Jacobian = b.JacobianPtr();
        }
        return DiffData<T>(a.Value() + b.Value(), Jacobian);
    }

    template <int R, int C>
    DiffDataMatrix<T, R, C> Evaluate(const DiffDataMatrix<T, R, C>& a, const DiffDataMatrix<T, R, C>& b) const
    {
        CARBON_PRECONDITION(a.Rows() == b.Rows(), "dimensions need to match for DiffData addition");
        CARBON_PRECONDITION(a.Cols() == b.Cols(), "dimensions need to match for DiffData addition");
        return DiffDataMatrix<T, R, C>(a.Rows(), a.Cols(), Evaluate((const DiffData<T>&)a, (const DiffData<T>&)b));
    }
};

template <class T>
DiffData<T> operator+(const DiffData<T>& a, const DiffData<T>& b) { return AddFunction<T>().Evaluate(a, b); }

template <class T, int R, int C>
DiffDataMatrix<T, R, C> operator+(const DiffDataMatrix<T, R, C>& a, const DiffDataMatrix<T, R, C>& b) { return AddFunction<T>().Evaluate(a, b); }

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/DiffData.h>
#include <nls/DiffDataMatrix.h>
#include <nls/math/Math.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * Function to subtract two value vectors: f(x) = a(x) - b(x)
 */
template <class T>
class SubtractFunction
{
public:
    SubtractFunction() {}

    DiffData<T> Evaluate(const DiffData<T>& a, const DiffData<T>& b) const
    {
        CARBON_PRECONDITION(a.Value().size() == b.Value().size(), "dimensions need to match for DiffData subtraction");

        JacobianConstPtr<T> Jacobian;
        if (a.HasJacobian() && b.HasJacobian())
        {
            // merge a and b jacobians i.e. subtract the two jacobians
            Jacobian = a.Jacobian().Subtract(b.JacobianPtr());
        }
        else if (a.HasJacobian())
        {
            Jacobian = a.JacobianPtr();
        }
        else if (b.HasJacobian())
        {
            // TODO: optimization - negate?
            Jacobian = b.Jacobian().Scale(T(-1));
        }
        return DiffData<T>(a.Value() - b.Value(), Jacobian);
    }

    template <int R, int C>
    DiffDataMatrix<T, R, C> Evaluate(const DiffDataMatrix<T, R, C>& a, const DiffDataMatrix<T, R, C>& b) const
    {
        CARBON_PRECONDITION(a.Rows() == b.Rows(), "dimensions need to match for DiffData subtraction");
        CARBON_PRECONDITION(a.Cols() == b.Cols(), "dimensions need to match for DiffData subtraction");
        return DiffDataMatrix<T, R, C>(a.Rows(), a.Cols(), Evaluate((const DiffData<T>&)a, (const DiffData<T>&)(b)));
    }
};


template <class T>
DiffData<T> operator-(const DiffData<T>& a, const DiffData<T>& b) { return SubtractFunction<T>().Evaluate(a, b); }

template <class T, int R, int C>
DiffDataMatrix<T, R, C> operator-(const DiffDataMatrix<T, R, C>& a, const DiffDataMatrix<T, R, C>& b) { return SubtractFunction<T>().Evaluate(a, b); }

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/DiffData.h>
#include <nls/DiffDataMatrix.h>
#include <nls/math/Math.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
DiffData<T> operator*(const DiffData<T>& a, const T scale)
{
    if (a.HasJacobian())
    {
        return DiffData<T>(scale * a.Value(), a.Jacobian().Scale(scale));
    }
    else
    {
        return DiffData<T>(scale * a.Value());
    }
}

template <class T>
DiffData<T> operator*(const T scale, const DiffData<T>& a) { return a * scale; }

template <class T, int R, int C>
DiffDataMatrix<T, R, C> operator*(const DiffDataMatrix<T, R, C>& a, const T scale)
{
    return DiffDataMatrix<T, R, C>(a.Rows(), a.Cols(), ((DiffData<T>)a) * scale);
}

template <class T, int R, int C>
DiffDataMatrix<T, R, C> operator*(const T scale, const DiffDataMatrix<T, R, C>& a) { return a * scale; }

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

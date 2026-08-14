// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/Context.h>
#include <nls/DiffDataMatrix.h>
#include <nls/math/Math.h>
#include <nls/MatrixVariable.h>
#include <nls/geometry/Affine.h>
#include <nls/geometry/DiffDataAffine.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

/**
 * An AffineTransformationVariable represents an affine transformation of a set of vertices.
 */
template <class LinearTransformationVariable>
class AffineVariable
{
public:
    typedef typename LinearTransformationVariable::value_type T;
    static constexpr int R = LinearTransformationVariable::ROWS;
    static constexpr int C = LinearTransformationVariable::COLS;

public:
    AffineVariable()
    {
        m_linearTransformationVariable.SetIdentity();
        m_translationVariable.SetZero();
    }

    DiffDataAffine<T, R, C> EvaluateAffine(Context<T>* context)
    {
        // rotation:
        DiffDataMatrix<T, R, C> rot = m_linearTransformationVariable.EvaluateMatrix(context);
        DiffDataMatrix<T, R, 1> t = m_translationVariable.EvaluateMatrix(context);
        return DiffDataAffine<T, R, C>(std::move(rot), std::move(t));
    }

    void SetAffine(const ::TITAN_NAMESPACE::Affine<T, R, C>& A)
    {
        m_linearTransformationVariable.SetMatrix(A.Linear());
        m_translationVariable.SetMatrix(A.Translation());
    }

    TITAN_NAMESPACE::Affine<T, R, C> Affine() const
    {
        ::TITAN_NAMESPACE::Affine<T, R, C> A;
        A.SetLinear(m_linearTransformationVariable.Matrix());
        A.SetTranslation(m_translationVariable.Matrix());
        return A;
    }

    /**
     * Creates a valid affine transformation matrix. Valid means the linear part of the transformation is on the manifold of the underlying
     * LinearTransformationVariable.
     */
    static ::TITAN_NAMESPACE::Affine<T, R, C> Random()
    {
        ::TITAN_NAMESPACE::Affine<T, R, C> aff;
        aff.SetLinear(Eigen::Matrix<T, R, C>::Random());
        aff.SetTranslation(Eigen::Vector<T, R>::Random());
        AffineVariable<LinearTransformationVariable> var;
        var.SetAffine(aff);
        return var.Affine();
    }

    void MakeConstant(bool makeLinearConstant, bool makeTranslationConstant)
    {
        if (makeLinearConstant)
        {
            m_linearTransformationVariable.MakeConstant();
        }
        else
        {
            m_linearTransformationVariable.MakeMutable();
        }

        if (makeTranslationConstant)
        {
            m_translationVariable.MakeConstant();
        }
        else
        {
            m_translationVariable.MakeMutable();
        }
    }

    LinearTransformationVariable& LinearVariable() { return m_linearTransformationVariable; }
    MatrixVariable<T, R, 1>& TranslationVariable() { return m_translationVariable; }

private:
    LinearTransformationVariable m_linearTransformationVariable;
    MatrixVariable<T, R, 1> m_translationVariable;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

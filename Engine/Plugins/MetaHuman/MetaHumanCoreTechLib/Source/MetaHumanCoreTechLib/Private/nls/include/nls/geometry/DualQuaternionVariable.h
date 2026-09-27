// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

// #include <carbon/Common.h>
// #include <nls/math/Math.h>
#include <nls/VectorVariable.h>
#include <nls/geometry/DualQuaternion.h>
#include <nls/functions/DualQuaternionFunctions.h>
#include <nls/geometry/Jacobians.h>
// #include <nls/geometry/Quaternion.h>
// #include <nls/geometry/RotationManifold.h>

CARBON_DISABLE_EIGEN_WARNINGS
#include <Eigen/Geometry>
CARBON_RENABLE_WARNINGS

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class DualQuaternionVariable : public VectorVariable<T>
{
public:
    DualQuaternionVariable()
        : VectorVariable<T>(/*size=*/8)
    {}

    DualQuaternionVariable(const Vector<T>& value)
        : VectorVariable<T>(/*size=*/8)
    {
        CARBON_ASSERT(value.rows() == 8, "Dual quaternion must have 8 elements");
        this->Set(value);
    }

    virtual bool RealJacobian() override
    {
        return true;
    }

    void SetIdentity()
    {
        Vector<T> value(8);
        value << T(0), T(0), T(0), T(1), T(0), T(0), T(0), T(0);
        Set(value);
    }

private:
    virtual SparseMatrixConstPtr<T> CalculateLocalJacobianMatrix() override
    {
        if (this->ConstantIndices().size() > 0)
        {
            CARBON_CRITICAL("DualQuaternionVariable does not support partial contant indices");
        }

        const int Rows = this->OutputDimension();
        const int Cols = this->UpdateDimension();
        CARBON_ASSERT(Rows == 8, "invalid jacobian row size");
        CARBON_ASSERT(Cols == 8, "invalid jacobian column size");

        return std::make_shared<SparseMatrix<T>>(DualQuaternionNormalizationJacobian<T>(this->Value()));
    }

    virtual void ProjectToManifold(Vector<T>& value) override
    {
        value = DualQuaternionNormalize(value);
    }
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

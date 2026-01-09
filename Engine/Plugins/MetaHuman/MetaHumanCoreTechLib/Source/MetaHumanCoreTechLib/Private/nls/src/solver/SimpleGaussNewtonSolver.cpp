// Copyright Epic Games, Inc. All Rights Reserved.

#include <nls/solver/SimpleGaussNewtonSolver.h>
#include <nls/Context.h>
#include <nls/Variable.h>
#include <algorithm>
#include <iostream>

#include <chrono>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
bool SimpleGaussNewtonSolver<T>::Solve(const std::function<Cost<T>(Context<T>*)>& costFunction,
                        Context<T>& context,
                        const int iterations,
                        const T regularization,
                        const T epsilon1,
                        const T epsilon2,
                        TaskThreadPool* threadPool) const
{
    bool found = false;
    Eigen::MatrixX<T> JtJ;
    Vector<T> Jtb;
    int k = 0;
    while (!found && k < iterations) {
        k += 1;

        Cost<T> cost = costFunction(&context);
        Vector<T> fx = cost.Value();
        Vector<T> currX = context.Value(); //initial iteration
        T normCurrX = currX.norm();

        if (regularization <= T(0.0)) {
            JtJ = Eigen::MatrixX<T>::Identity(currX.rows(), currX.rows()) * fx.cwiseAbs().maxCoeff();
        }
        else {
            JtJ = Eigen::MatrixX<T>::Identity(currX.rows(), currX.rows()) * regularization;
        }
        Jtb = Vector<T>::Zero(currX.rows());
        cost.AddDenseJtJLower(JtJ, T(1), threadPool);
        cost.AddJtx(Jtb, fx, T(-1));

        T gradNorm = Jtb.squaredNorm(); // gradient norm in initial iteration x_0
        found = (gradNorm <= epsilon1);

        const Vector<T> dx = JtJ.template selfadjointView<Eigen::Lower>().llt().solve(Jtb);
        const T normStep = dx.norm();

        if (normStep <= epsilon2 * (normCurrX + epsilon2)) {
            // almost no further change in norm possible, so stop the optimization
            return true;
        }

        context.Update(dx); // we have new point: x_{k+1}= x_k + dx
    }

    return true;
}

template class SimpleGaussNewtonSolver<float>;
template class SimpleGaussNewtonSolver<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

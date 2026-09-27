// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/Context.h>
#include <nls/DiffData.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class LMSolver
{
public:
    LMSolver() = default;

    // epsilon1: stopping criterion for gradient norm
    // epsilon2: stopping criterion for step size
    bool Solve(const std::function<DiffData<T>(Context<T>*)>& evaluationFunction, int iterations = 1, const T epsilon1 = T(1e-8),
               const T epsilon2 = T(1e-8)) const
    {
        Context<T> context;
        return Solve(evaluationFunction, context, iterations, epsilon1, epsilon2);
    }

    // epsilon1: stopping criterion for gradient norm
    // epsilon2: stopping criterion for step size
    bool Solve(const std::function<DiffData<T>(Context<T>*)>& evaluationFunction,
               Context<T>& context,
               const int iterations = 1,
               const T epsilon1 = T(1e-8),
               const T epsilon2 = T(1e-8)) const;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

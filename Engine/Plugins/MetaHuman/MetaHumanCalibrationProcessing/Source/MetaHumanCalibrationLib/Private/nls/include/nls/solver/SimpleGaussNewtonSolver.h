// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <carbon/utils/TaskThreadPool.h>
#include <nls/Context.h>
#include <nls/Cost.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
class SimpleGaussNewtonSolver
{
public:
	SimpleGaussNewtonSolver() = default;

    // epsilon1: stopping criterion for gradient norm
    // epsilon2: stopping criterion for step size
	bool Solve(const std::function<Cost<T>(Context<T>*)>& costFunction,
			   int iterations,
			   const T regularization, // = T(1e-1),
			   const T epsilon1, // = T(1e-8),
			   const T epsilon2, // = T(1e-8),
			   TaskThreadPool* threadPool) const
	{
		Context<T> context;
		return Solve(costFunction, context, iterations, regularization, epsilon1, epsilon2, threadPool);
	}

    // epsilon1: stopping criterion for gradient norm
    // epsilon2: stopping criterion for step size
	bool Solve(const std::function<Cost<T>(Context<T>*)>& costFunction,
			   Context<T>& context,
			   const int iterations,
			   const T regularization, // = T(1e-1),
			   const T epsilon1, // = T(1e-8),
			   const T epsilon2, // = T(1e-8),
			   TaskThreadPool* threadPool) const;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

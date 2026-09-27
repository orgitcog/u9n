// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <carbon/utils/TaskThreadPool.h>
#include <nls/BoundedVectorVariable.h>
#include <nls/Context.h>
#include <nls/Cost.h>
#include <nls/DiffData.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
struct BoundedCoordinateDescentSolverSettings
{
    int iterations = 10;
    int coordinateDescentIterations = 100;
    int maxLineSearchIterations = 10;
    T l1reg = 0;
    bool useSaturatedL1 = false;
    T saturatedL1_m = 2;
};

/**
 * Special coordinate descent solver with bounds and L1 regularization i.e. LASSO with box constraints.
 * see also https://www.stat.cmu.edu/~ryantibs/convexopt-S15/lectures/22-coord-desc.pdf.
 * https://www.jstatsoft.org/article/view/v033i01/v33i01.pdf
 *
 * Used to solve the function:
 *              min 1/2 || evaluationFunction() ||_2^2 + sum_i | l1Reg boundedVectorVariable_i |_1 (optionally using saturated L1)
 */
template <class T>
class BoundedCoordinateDescentSolver
{
public:
    /**
     * Run the solve on the cost function with additional L1 regularization.
     * @param costFunction            The least squares evaluation function.
     * @param context                             The context that maps variables to the columns of the Jacobian of the evaluation function
     * @param boundedVectorVariables  A set of vector variable for which the bounds are applied. The costFunction needs to use these bounded variables.
     * @param settings				  The settings for the solver.
     */
    static bool SolveCostFunction(const std::function<Cost<T>(Context<T>*)>& costFunction,
                                  Context<T>& context,
                                  const std::vector<BoundedVectorVariable<T>*>& boundedVectorVariables,
                                  const BoundedCoordinateDescentSolverSettings<T>& settings,
                                  TaskThreadPool* threadPool);

    //! Evaluate the total energy for the function: 1/2 || costFunction() ||_2^2 + sum_i | l1Reg boundedVectorVariable_i |_1 (optionally using saturated L1)
    static T EvaluateCostFunction(const std::function<Cost<T>(Context<T>*)>& costFunction,
                                  const std::vector<BoundedVectorVariable<T>*>& boundedVectorVariables,
                                  const BoundedCoordinateDescentSolverSettings<T>& settings);

    /**
     * Run the solve on the evaluation function with additional L1 regularization.
     * @param evaluationFunction      The least squares evaluation function.
     * @param context                             The context that maps variables to the columns of the Jacobian of the evaluation function
     * @param boundedVectorVariables  A set of vector variable for which the bounds are applied. The evaluationFunction needs to use these bounded variables.
     * @param settings				  The settings for the solver.
     */
    static bool Solve(const std::function<DiffData<T>(Context<T>*)>& evaluationFunction,
                      Context<T>& context,
                      const std::vector<BoundedVectorVariable<T>*>& boundedVectorVariables,
                      const BoundedCoordinateDescentSolverSettings<T>& settings,
                      TaskThreadPool* threadPool);

    //! Evaluate the total energy for the function: 1/2 || evaluationFunction() ||_2^2 + sum_i | l1Reg boundedVectorVariable_i |_1 (optionally using saturated
    //! L1)
    static T Evaluate(const std::function<DiffData<T>(Context<T>*)>& evaluationFunction,
                      const std::vector<BoundedVectorVariable<T>*>& boundedVectorVariables,
                      const BoundedCoordinateDescentSolverSettings<T>& settings);
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

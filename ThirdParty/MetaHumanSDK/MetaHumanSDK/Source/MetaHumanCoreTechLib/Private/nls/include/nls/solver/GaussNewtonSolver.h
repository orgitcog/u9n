// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/Context.h>
#include <nls/DiffData.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T> T DiagonalRegularization();
template <class T> T ResidualErrorStoppingCriterion();
template <class T> T PredictionReductionErrorStoppingCriterion();

template <class T>
class GaussNewtonSolver
{
public:
    struct Settings
    {
        int iterations = 1;
        T reg = DiagonalRegularization<T>();
        int cgIterations = 200; //! the number of cg iterations (if pcg is used)
        int maxLineSearchIterations = 10;
        T residualErrorStoppingCriterion = ResidualErrorStoppingCriterion<T>();
        T predictionReductionStoppingCriterion = PredictionReductionErrorStoppingCriterion<T>();
        bool optimizeForRectangularDenseJacobian = false;
    };

    GaussNewtonSolver() = default;

    bool Solve(const std::function<DiffData<T>(Context<T>*)>& evaluationFunction, int iterations = 1, T reg = DiagonalRegularization<T>()) const
    {
        Context<T> context;
        return Solve(evaluationFunction, context, iterations, reg);
    }

    bool Solve(const std::function<DiffData<T>(Context<T> *)> &evaluationFunction, const Settings &settings) const
    {
        Context<T> context;
        return Solve(evaluationFunction, context, settings);
    }

    bool Solve(const std::function<DiffData<T>(Context<T>*)>& evaluationFunction, Context<T>& context, int iterations = 1,
               T reg = DiagonalRegularization<T>()) const
    {
        Settings settings;
        settings.iterations = iterations;
        settings.reg = reg;
        return Solve(evaluationFunction, context, settings);
    }

    bool Solve(const std::function<DiffData<T>(Context<T>*)>& evaluationFunction, Context<T>& context, const Settings& settings) const;

private:
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

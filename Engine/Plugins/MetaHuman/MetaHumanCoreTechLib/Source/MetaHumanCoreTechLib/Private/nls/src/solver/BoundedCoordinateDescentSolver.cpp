// Copyright Epic Games, Inc. All Rights Reserved.

#include <nls/solver/BoundedCoordinateDescentSolver.h>
#include <nls/Context.h>
#include <nls/PatternCheck.h>
#include <nls/Variable.h>


CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T> T ResidualErrorStoppingCriterion();
template <class T> T PredictionReductionErrorStoppingCriterion();

template <class T>
inline T L1(const T x) { return std::abs(x); }

template <class T>
inline T SaturatedL1(const T x, const T m) { return std::tanh(m * std::abs(x)) / m; }

template <class T>
inline T SaturatedL1Gradient(const T x, const T m)
{
    if (x > 0)
    {
        const T h = std::tanh(m * x);
        const T g = (T(1) - h * h);
        return g;
    }
    else
    {
        const T h = std::tanh(-m * x);
        const T g = -(T(1) - h * h);
        return g;
    }
}

/**
 * @brief Coordinate descent solve with L1 regularization
 *
 * @param currX  The current value of the coordinate
 * @param b      The right-hand side of the solve (see below)
 * @param JtJ    The diagonal entry of JtJ for the current coordinate
 * @param l1Reg  The l1 regularization
 * @return The solved value for the coordinate
 *
 * L1(x[i]) = l1Reg * abs(x[i])
 * gradient(L1(x[i])) = sgn(x[i]) * l1Reg
 *
 * Coordinate descent wit GaussNewton for coordinate i:
 *   JtJ.row(i).dot(dx) + (Jt * f)[i] + g = 0
 *   where g = sgn(x[i]) * l1Reg
 *   JtJ.row(i).dot(dx') + JtJ(i,i) * dx[i] + Jtf[i] + g = 0
 *   where dx' is dx with dx[i]=0
 *   JtJ.row(i).dot(dx') + JtJ(i,i) * (x[i] - x_curr[i]) + Jtf[i] + g = 0
 *   JtJ(i,i) * (x[i] - x_curr[i]) + Jtf[i] + g = - JtJ.row(i).dot(dx') - Jtf[i]
 *   JtJ(i,i) * (x[i] - x_curr[i]) + g = b
 *   b = - JtJ.row(i).dot(dx') - Jtf[i]
 *   x[i] = (b + JtJ(i,i) * x_curr[i] - g) / JtJ(i)
 *
 *   Note that g = sgn(x[i]) * l1Reg, and therefore the solution needs to ensure that x[i] and g are consistent or otherwise x[i] = 0
 */
template <class T>
inline T L1CoordinateSolve(const T currX, const T b, const T JtJ, const T l1Reg)
{
    T newXj = 0;
    const T acc2 = b + currX * JtJ;
    if (acc2 > l1Reg)
    {
        // newXj is positive
        const T g = l1Reg;
        newXj = (acc2 - g) / JtJ;
    }
    else if (acc2 < -l1Reg)
    {
        // newXj is negative
        const T g = -l1Reg;
        newXj = (acc2 - g) / JtJ;
    }
    else
    {
        // no gradient "consistent" solution
        newXj = 0;
    }
    return newXj;
}

/**
 * @brief Coordinate descent solve with saturated L1 regularization
 *
 * @param currX  The current value of the coordinate
 * @param b      The right-hand side of the solve
 * @param JtJ    The diagonal entry of JtJ for the current coordinate
 * @param l1Reg  The l1 regularization
 * @param m      The parameter for saturated L1
 * @return The solved value for the coordinate
 *
 * saturatedL1(x[i]) = tanh(m * abs(x[i])) / m
 * gradient(saturatedL1(x[i])) = sgn(x[i]) * (1 - tanh(m abs(x[i]))^2) * l1Reg
 *
 * SaturatedL1 approximates L1 around zero. Given that the gradient of saturated L1
 * depends on x[i], we would need to iterate to recalculate the gradient at the current x[i]. However,
 * SaturatedL1CoordinateSolve is called many times in the solve loop, and therefore we can use
 * the current x[i] + dx[i] to initialize the gradient. Note that x[i] will be smallest with normal L1,
 * hence the checks whether x[i] shall be positive or negative is the same as for L1.
 */
template <class T>
inline T SaturatedL1CoordinateSolve(const T currX, const T currDx, const T b, const T JtJ, const T l1Reg, const T m)
{
    T newXj = 0;
    const T acc2 = b + currX * JtJ;
    if (acc2 > l1Reg)
    {
        // newXj is positive
        const T g = (currX + currDx > 0) ? SaturatedL1Gradient<T>(currX + currDx, m) * l1Reg : l1Reg;
        newXj = (acc2 - g) / JtJ;
    }
    else if (acc2 < -l1Reg)
    {
        // newXj is negative
        const T g = (currX + currDx < 0) ? SaturatedL1Gradient<T>(currX + currDx, m) * l1Reg : -l1Reg;
        newXj = (acc2 - g) / JtJ;
    }
    else
    {
        // no gradient "consistent" solution
        newXj = 0;
    }
    return newXj;
}

namespace
{

template <class T>
struct Bounds
{
    bool valid = false;
    T lowerBound = std::numeric_limits<T>::lowest();
    T upperBound = std::numeric_limits<T>::max();
    int valueIndex{};
    T regScaling = T(1);
};

} // namespace

template <class T>
Vector<T> SolveCoordinateDescent(const Eigen::Matrix<T, -1, -1>& JtJ,
                                 const Eigen::VectorX<T>& Jtb,
                                 const Eigen::VectorX<T>& currX,
                                 const std::vector<Bounds<T>>& bounds,
                                 const BoundedCoordinateDescentSolverSettings<T>& settings)
{
    Vector<T> dx = Vector<T>::Zero(Jtb.size());
    for (int gsIter = 0; gsIter < settings.coordinateDescentIterations; ++gsIter)
    {
        // Vector<T> dxOld = dx;
        for (int r = 0; r < int(Jtb.size()); ++r)
        {
            if (JtJ(r, r) == 0) { continue; }
            const T acc = Jtb[r] - JtJ.col(r).dot(dx) + JtJ(r, r) * dx[r];
            if (bounds[r].valid)
            {
                // bounded variable => apply first L1, then bounds
                const int idx = bounds[r].valueIndex;
                const T newXj = settings.useSaturatedL1
                              ? SaturatedL1CoordinateSolve<T>(currX[idx],
                                                              dx[r],
                                                              acc,
                                                              JtJ(r, r),
                                                              settings.l1reg * bounds[r].regScaling,
                                                              settings.saturatedL1_m * bounds[r].regScaling)
                              : L1CoordinateSolve<T>(currX[idx], acc, JtJ(r, r), settings.l1reg * bounds[r].regScaling);
                dx[r] = clamp<T>(newXj, bounds[r].lowerBound, bounds[r].upperBound) - currX[idx];
            }
            else
            {
                dx[r] = acc / JtJ(r, r);
            }
        }
        // LOG_INFO("max change {}: {}", gsIter, (dxOld-dx).cwiseAbs().maxCoeff());
    }
    return dx;
}

template <class T>
T EvaluateRegularization(const Eigen::VectorX<T>& x,
                         const Eigen::VectorX<T>& dx,
                         const std::vector<Bounds<T>>& bounds,
                         const BoundedCoordinateDescentSolverSettings<T>& settings)
{
    if (settings.l1reg == 0) { return 0; }

    T cost = 0;
    if (settings.useSaturatedL1)
    {
        for (size_t i = 0; i < bounds.size(); ++i)
        {
            if (bounds[i].valid)
            {
                const int idx = bounds[i].valueIndex;
                cost += SaturatedL1<T>((x[idx] + dx[i]) * bounds[i].regScaling, settings.saturatedL1_m * bounds[i].regScaling);
            }
        }
    }
    else
    {
        for (size_t i = 0; i < bounds.size(); ++i)
        {
            if (bounds[i].valid)
            {
                const int idx = bounds[i].valueIndex;
                cost += L1<T>((x[idx] + dx[i]) * bounds[i].regScaling);
            }
        }
    }
    return settings.l1reg * cost;
}

template <class T>
T EvaluateRegularization(const std::vector<BoundedVectorVariable<T>*>& boundedVectorVariables, const BoundedCoordinateDescentSolverSettings<T>& settings)
{
    if (settings.l1reg == 0) { return 0; }

    T cost = 0;
    for (auto boundedVectorVariable : boundedVectorVariables)
    {
        if (boundedVectorVariable)
        {
            const auto& vec = boundedVectorVariable->Value();
            if (settings.useSaturatedL1)
            {
                for (Eigen::Index i = 0; i < vec.size(); ++i)
                {
                    const T regScale = boundedVectorVariable->RegularizationScaling()[i];
                    cost += SaturatedL1<T>(vec[i] * regScale, settings.saturatedL1_m * regScale);
                }
            }
            else
            {
                for (Eigen::Index i = 0; i < vec.size(); ++i)
                {
                    const T regScale = boundedVectorVariable->RegularizationScaling()[i];
                    cost += L1<T>(vec[i] * regScale);
                }
            }
        }
    }
    return settings.l1reg * cost;
}

template <class T>
void UpdateBounds(std::vector<Bounds<T>>& bounds, const Context<T>& context, const std::vector<BoundedVectorVariable<T>*>& boundedVectorVariables)
{
    const int numTotalVariables = context.UpdateSize();
    if (static_cast<int>(bounds.size()) != numTotalVariables)
    {
        std::vector<Bounds<T>> newBounds(numTotalVariables);
        for (size_t j = 0; j < boundedVectorVariables.size(); ++j)
        {
            const auto* boundedVectorVariable = boundedVectorVariables[j];
            if (boundedVectorVariable)
            {
                const typename Context<T>::VariableInfo& variableInfo = context.GetVariableInfo(boundedVectorVariable);
                if (boundedVectorVariable->BoundsAreEnforced())
                {
                    // L1 and bounds are applied
                    for (int k = 0; k < variableInfo.cols; ++k)
                    {
                        newBounds[variableInfo.colOffset + k].valid = true;
                        newBounds[variableInfo.colOffset + k].lowerBound = boundedVectorVariable->Bounds()(0, k);
                        newBounds[variableInfo.colOffset + k].upperBound = boundedVectorVariable->Bounds()(1, k);
                        newBounds[variableInfo.colOffset + k].valueIndex = variableInfo.rowOffset + k;
                        newBounds[variableInfo.colOffset + k].regScaling = boundedVectorVariable->RegularizationScaling()[k];
                    }
                }
                else
                {
                    // only L1 is applied
                    for (int k = 0; k < variableInfo.cols; ++k)
                    {
                        newBounds[variableInfo.colOffset + k].valid = true;
                        newBounds[variableInfo.colOffset + k].valueIndex = variableInfo.rowOffset + k;
                        newBounds[variableInfo.colOffset + k].regScaling = boundedVectorVariable->RegularizationScaling()[k];
                    }
                }
            }
        }
        std::swap(bounds, newBounds);
    }
}

template <class T>
bool BoundedCoordinateDescentSolver<T>::SolveCostFunction(const std::function<Cost<T>(Context<T>*)>& costFunction,
                                                          Context<T>& context,
                                                          const std::vector<BoundedVectorVariable<T>*>& boundedVectorVariables,
                                                          const BoundedCoordinateDescentSolverSettings<T>& settings,
                                                          TaskThreadPool* threadPool)
{
    // stops when the residual error goes below this point
    const T residualErrorStoppingCriterion = ResidualErrorStoppingCriterion<T>();

    // stops when predicted reduction goes below this point
    const T predictionReductionStoppingCriterion = PredictionReductionErrorStoppingCriterion<T>();

    // bounds for each variable
    std::vector<Bounds<T>> bounds;

    for (int iter = 0; iter < settings.iterations; ++iter)
    {
        Cost<T> cost = costFunction(&context);

        if (!cost.HasJacobian())
        {
            CARBON_CRITICAL("data is missing the jacobian");
        }

        // update the bounds information
        UpdateBounds<T>(bounds, context, boundedVectorVariables);

        const Vector<T> currX = context.Value();
        const Vector<T> fx = cost.Value();
        const T regularizationError = EvaluateRegularization<T>(boundedVectorVariables, settings);
        const T residualError = T(0.5) * cost.Value().squaredNorm() + regularizationError;

        // LOG_INFO("{}: energy: {}", iter, residualError);

        if (residualError < residualErrorStoppingCriterion)
        {
            return true;
        }

        //// || fx + Jdx || = 0
        //// JtJ dx = - Jt fx

        Eigen::VectorX<T> Jtb = Eigen::VectorX<T>::Zero(cost.Cols());
        // Jtb = - Jt fx
        cost.AddJtx(Jtb, fx, T(-1));
        Eigen::Matrix<T, -1, -1> JtJlower = Eigen::Matrix<T, -1, -1>::Zero(cost.Cols(), cost.Cols());
        cost.AddDenseJtJLower(JtJlower, T(1), threadPool);
        const Eigen::Matrix<T, -1, -1> JtJ = JtJlower.template selfadjointView<Eigen::Lower>();

        // gauss seidel iterations to solve for new dx
        const Eigen::VectorX<T> dx = SolveCoordinateDescent(JtJ, Jtb, currX, bounds, settings);
        // calculate new regularization error
        const T newRegularizationError = EvaluateRegularization<T>(currX, dx, bounds, settings);
        // get the predicted residual from the linearization
        // const T predictedResidual = T(0.5) * (fx + Jtd.transpose() * dx).squaredNorm() + newRegularizationError;
        Eigen::VectorX<T> dval = fx;
        cost.AddJx(dval, dx, T(1));
        const T predictedResidual = T(0.5) * dval.squaredNorm() + newRegularizationError;
        const T predictedReduction = residualError - predictedResidual;

        // a way to look at the angle between the update and the gradient descent direction
        // const T angle = acos(Jtb.normalized().dot(dx.normalized())) / T(CARBON_PI) * T(180.0);
        // LOG_INFO("angle: {}", angle);

        // LOG_INFO("{}: predicted reduction: {}", iter, predictedReduction);

        if (predictedReduction / residualError < predictionReductionStoppingCriterion)
        {
            // almost no further change in residual possible, so stop the optimization
            return true;
        }

        context.Update(dx);

        // based on description in http://sites.science.oregonstate.edu/~gibsonn/optpart2.pdf
        T newResidualError = EvaluateCostFunction(costFunction, boundedVectorVariables, settings);
        T actualReduction = residualError - newResidualError;
        // LOG_INFO("{}: actual reduction: {} ({}) - {} ({}) = {}", iter, residualError, regularizationError, newResidualError, newRegularizationError,
        // actualReduction);

        const T alpha = T(0.25);
        T currStep = T(1.0);
        int lineSearchIter = 0;
        // T armijo = 1.0;

        while (actualReduction < alpha * currStep * predictedReduction && lineSearchIter < settings.maxLineSearchIterations)
        {
            // error has increased or not decreased signficantly
            lineSearchIter++;

            // Options to reduce the error:
            // 1) Armijo rules to reduce the error
            // armijo *= 0.5;
            // const T nextStep = armijo - accumulatedStep;

            // 2) Quadratric interpolation on the residuals to calculate the optimal step
            const T quadraticStep = -(-predictedReduction) / (2 * (newResidualError - residualError - (-predictedReduction)));
            if ((quadraticStep <= 0) || (quadraticStep >= 1.0))
            {
                CARBON_CRITICAL("Bounded Coordinate Descent: quadratic interpolation in line search has failed.");
            }
            // use quadratic interpolation only if interpoalted value is meaningful and not reducing the steps size too much
            const T nextStep = (quadraticStep > T(0.05)) ? quadraticStep * currStep : T(0.25) * currStep;

            // set and update to always start from the original position or otherwise there may be an accumulation of error
            context.Set(currX);
            context.Update(nextStep * dx);

            newResidualError = EvaluateCostFunction(costFunction, boundedVectorVariables, settings);
            const T newActualReduction = residualError - newResidualError;
            // LOG_INFO("{}: - new actual reduction: {} (step {})", iter, newActualReduction, nextStep);

            if (newActualReduction < actualReduction)
            {
                // results get worse, so we need to invert the last step and stop
                context.Set(currX);
                context.Update(currStep * dx);
                break;
            }
            actualReduction = newActualReduction;
            currStep = nextStep;
        }

        if (actualReduction <= 0)
        {
            // LOG_INFO("Bounded Coordinate Descent: failed to further reduce the error, despite no stopping criterion was hit.");
            return true;
        }
    }

    return true;
}

template <class T>
bool BoundedCoordinateDescentSolver<T>::Solve(const std::function<DiffData<T>(Context<T>*)>& evaluationFunction,
                                              Context<T>& context,
                                              const std::vector<BoundedVectorVariable<T>*>& boundedVectorVariables,
                                              const BoundedCoordinateDescentSolverSettings<T>& settings,
                                              TaskThreadPool* threadPool)
{
    auto costFunction = [&](Context<T>* context) -> Cost<T> {
            Cost<T> cost;
            cost.Add(evaluationFunction(context), T(1));
            return cost;
        };
    return SolveCostFunction(costFunction, context, boundedVectorVariables, settings, threadPool);
}

template <class T>
T BoundedCoordinateDescentSolver<T>::EvaluateCostFunction(const std::function<Cost<T>(Context<T>*)>& costFunction,
                                                          const std::vector<BoundedVectorVariable<T>*>& boundedVectorVariables,
                                                          const BoundedCoordinateDescentSolverSettings<T>& settings)
{
    return T(0.5) * costFunction(nullptr).Value().squaredNorm() + EvaluateRegularization(boundedVectorVariables, settings);
}

template <class T>
T BoundedCoordinateDescentSolver<T>::Evaluate(const std::function<DiffData<T>(Context<T>*)>& evaluationFunction,
                                              const std::vector<BoundedVectorVariable<T>*>& boundedVectorVariables,
                                              const BoundedCoordinateDescentSolverSettings<T>& settings)
{
    return T(0.5) * evaluationFunction(nullptr).Value().squaredNorm() + EvaluateRegularization(boundedVectorVariables, settings);
}

template class BoundedCoordinateDescentSolver<float>;
template class BoundedCoordinateDescentSolver<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

// Copyright Epic Games, Inc. All Rights Reserved.

#include <nls/solver/GaussNewtonSolver.h>
#include <nls/Context.h>
#include <nls/PatternCheck.h>
#include <nls/Variable.h>
#include <nls/math/PCG.h>
#include <nls/math/ParallelBLAS.h>

CARBON_DISABLE_EIGEN_WARNINGS
#ifdef EIGEN_USE_MKL_ALL
    #include <Eigen/PardisoSupport>
    #include <nls/math/MKLWrapper.h>
#else
    #include <Eigen/IterativeLinearSolvers>
#endif
CARBON_RENABLE_WARNINGS

#include <chrono>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <> float DiagonalRegularization() { return float(1e-3); }
template <> double DiagonalRegularization() { return double(1e-7); }

template <> float ResidualErrorStoppingCriterion() { return float(1e-8); }
template <> double ResidualErrorStoppingCriterion() { return double(1e-12); }

template <> float PredictionReductionErrorStoppingCriterion() { return float(1e-6); }
template <> double PredictionReductionErrorStoppingCriterion() { return double(1e-8); }


template <class T>
bool GaussNewtonSolver<T>::Solve(const std::function<DiffData<T>(Context<T>*)>& evaluationFunction, Context<T>& context, const Settings& settings) const
{
    // stops when the residual error goes below this point
    const T residualErrorStoppingCriterion = ResidualErrorStoppingCriterion<T>();

    // stops when predicted reduction goes below this point
    const T predictionReductionStoppingCriterion = PredictionReductionErrorStoppingCriterion<T>();

    #ifdef EIGEN_USE_MKL_ALL
    Eigen::PardisoLDLT<Eigen::SparseMatrix<T, Eigen::RowMajor>> solver;
    #else
    // Eigen::ConjugateGradient<Eigen::SparseMatrix<T, Eigen::RowMajor>, Eigen::Lower|Eigen::Upper> solver;
    PCG<Eigen::SparseMatrix<T, Eigen::RowMajor>> solver;
    // TODO: check if we want to switch to LeastSquaresConjugateGradient
    // Eigen::LeastSquaresConjugateGradient<Eigen::SparseMatrix<T, Eigen::RowMajor>> solver;
    #endif

    PatternCheck<T> jtjPatternChecker;

    for (int iter = 0; iter < settings.iterations; ++iter)
    {
        DiffData<T> diffData = evaluationFunction(&context);
        if (!diffData.HasJacobian())
        {
            CARBON_CRITICAL("data is missing the jacobian");
        }
        const Vector<T> fx = diffData.Value();
        const T residualError = diffData.Value().squaredNorm();

        const Vector<T> currX = context.Value();

        // LOG_VERBOSE("{}: energy: {}", iter, residualError);

        if (residualError < residualErrorStoppingCriterion)
        {
            return true;
        }

        // || fx + Jdx || = 0
        // JtJ dx = - Jt fx
        const int numTotalVariables = context.UpdateSize();
        SparseMatrix<T> Jt = diffData.Jacobian().AsSparseMatrix()->transpose();

        // Remove empty columns of J (so empty rows of Jt). Empty columns are variables that are constant or the
        // solution is not depending on these variables.
        // Note that not removing the variables would mean that the regularization further below gets very slow as it will
        // add new diagonal coefficients into the sparse matrix
        Eigen::VectorXi mapping;
        DiscardEmptyInnerDimensions(Jt, mapping);

        Eigen::SparseMatrix<T, Eigen::RowMajor> JtJ;
        if (settings.optimizeForRectangularDenseJacobian)
        {
            Eigen::Matrix<T, -1, -1, Eigen::RowMajor> J(*diffData.Jacobian().AsSparseMatrix());
            Eigen::Matrix<T, -1, -1> JtJDense = Eigen::Matrix<T, -1, -1>::Zero(J.cols(), J.cols());

            ParallelAtALowerAdd<T>(JtJDense, J, TaskThreadPool::GlobalInstance(true).get());

            Eigen::Matrix<T, -1, -1, Eigen::RowMajor> temp = JtJDense.template selfadjointView<Eigen::Lower>();
            JtJ = Eigen::SparseMatrix<T, Eigen::RowMajor>(temp.sparseView());  
        }
        else
        {
            #ifdef EIGEN_USE_MKL_ALL
            mkl::ComputeAAt(Jt, JtJ);
            #else
            JtJ = Jt * Jt.transpose();
            #endif
        }

        if (settings.reg > 0)
        {
            for (int i = 0; i < JtJ.cols(); i++)
            {
                // only fast if the entry already exists, which is the case whenever the variable is used in the Jacobian (see DiscardEmptyInnerDimensions
                // above)
                JtJ.coeffRef(i, i) += settings.reg;
            }
        }

        const Vector<T> Jtb = -Jt * fx;

        #ifdef EIGEN_USE_MKL_ALL
        if (jtjPatternChecker.checkAndUpdatePattern(JtJ))
        {
            solver.analyzePattern(JtJ);
        }

        solver.factorize(JtJ);

        if (solver.info() != Eigen::Success)
        {
            CARBON_CRITICAL("Gauss Newton: failed to analyze matrix - singularity?");
        }

        const Vector<T> dx = solver.solve(Jtb);
        #else
        const Vector<T> dx = solver.Solve(settings.cgIterations, JtJ, Jtb, Eigen::VectorX<T>());
        #endif
        const T normStep = dx.norm();
        const T stepRegCost = normStep * normStep * settings.reg;
        const T predictedReduction = Jtb.dot(dx) - stepRegCost;

        // a way to look at the angle between the update and the gradient descent direction
        // const T angle = acos(Jtb.normalized().dot(dx.normalized())) / T(CARBON_PI) * T(180.0);
        // LOG_VERBOSE("angle: {} ({})", angle, Jtb.normalized().dot(dx.normalized()));

        // LOG_VERBOSE("{}: predicted reduction: {}", iter, predictedReduction);

        if (predictedReduction / residualError < predictionReductionStoppingCriterion)
        {
            // almost no further change in residual possible, so stop the optimization
            return true;
        }

        Vector<T> finalDx;
        if (numTotalVariables > int(dx.size()))
        {
            // remapping - some variables are constant and not optimized
            finalDx = Vector<T>::Zero(numTotalVariables);
            for (int i = 0; i < int(mapping.size()); i++)
            {
                if (mapping[i] >= 0)
                {
                    finalDx[i] = dx[mapping[i]];
                }
            }
        }
        else
        {
            finalDx = dx;
        }
        context.Update(finalDx);

        // based on description in http://sites.science.oregonstate.edu/~gibsonn/optpart2.pdf
        T newResidualError = evaluationFunction(nullptr).Value().squaredNorm() + stepRegCost;
        // LOG_VERBOSE("new residual: {}", newResidualError);
        T actualReduction = residualError - newResidualError;
        // LOG_VERBOSE("{}: actual reduction: {}", iter, actualReduction);

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
                CARBON_CRITICAL("Gauss Newton: quadratic interpolation has failed.");
            }
            // use quadratic interpolation only if interpoalted value is meaningful and not reducing the steps size too much
            const T nextStep = (quadraticStep > T(0.05)) ? quadraticStep * currStep : T(0.25) * currStep;

            // set and update to always start from the original position or otherwise there may be an accumulation of error
            context.Set(currX);
            context.Update(nextStep * finalDx);

            newResidualError = evaluationFunction(nullptr).Value().squaredNorm() + nextStep * nextStep * stepRegCost;
            T newActualReduction = residualError - newResidualError;
            // LOG_VERBOSE("{}:     new actual reduction: {} (step {}) ({} {} - {})", iter, newActualReduction, nextStep);

            if (newActualReduction < actualReduction)
            {
                // results get worse, so we need to invert the last step and stop
                context.Set(currX);
                context.Update(currStep * finalDx);
                break;
            }
            else
            {
                actualReduction = newActualReduction;
                currStep = nextStep;
            }
        }

        if (actualReduction <= 0)
        {
            if (predictedReduction / residualError > T(10) * predictionReductionStoppingCriterion)
            {
                LOG_VERBOSE("Gauss Newton - last info: {} {} {}", normStep, predictedReduction / residualError, predictionReductionStoppingCriterion);
                LOG_INFO("Gauss Newton, iteration {}: failed to further reduce the error, despite no stopping criterion was hit.", iter);
            }
            return true;
        }
    }

    return true;
}

template class GaussNewtonSolver<float>;
template class GaussNewtonSolver<double>;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

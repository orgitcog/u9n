// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <rig/rbfs/RBFSolverBase.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::rbfs)

class InterpolativeRBFSolver : public RBFSolverBase {
    public:
        InterpolativeRBFSolver(const RBFSolverRecipe& recipe, pma::MemoryResource* memRes);
        explicit InterpolativeRBFSolver(InterpolativeRBFSolver* other);
        explicit InterpolativeRBFSolver(pma::MemoryResource* memRes);


        RBFSolverType getSolverType() const override;
        void solve(dna::ArrayView<float> input, dna::ArrayView<float> intermediateWeights, dna::ArrayView<float> outputWeights) const override;

        const Matrix<float>& getCoefficients() const;

    private:
        Matrix<float> coefficients;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::rbfs)

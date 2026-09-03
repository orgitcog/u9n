// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <rig/rbfs/RBFSolverBase.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::rbfs)

class AdditiveRBFSolver : public RBFSolverBase {
    public:
        explicit AdditiveRBFSolver(pma::MemoryResource* memRes);
        AdditiveRBFSolver(const RBFSolverRecipe& recipe, pma::MemoryResource* memRes);
        explicit AdditiveRBFSolver(const RBFSolverBase* other);


        RBFSolverType getSolverType() const override;
        void solve(dna::ArrayView<float> input, dna::ArrayView<float> intermediateWeights, dna::ArrayView<float> outputWeights) const override;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::rbfs)

// Copyright Epic Games, Inc. All Rights Reserved.

#include <rig/rbfs/AdditiveRBFSolver.h>
#include <rig/rbfs/RBFSolverBase.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::rbfs)

AdditiveRBFSolver::AdditiveRBFSolver(pma::MemoryResource* memRes) : RBFSolverBase(memRes) {
}

AdditiveRBFSolver::AdditiveRBFSolver(const RBFSolverRecipe& recipe, pma::MemoryResource* memRes) : RBFSolverBase(recipe, memRes) {
}

AdditiveRBFSolver::AdditiveRBFSolver(const RBFSolverBase* other) :
    RBFSolverBase(*other) {

}

RBFSolverType AdditiveRBFSolver::getSolverType() const {
    return RBFSolverType::Additive;
}

void AdditiveRBFSolver::solve(dna::ArrayView<float> input, dna::ArrayView<float>  /*unused*/, dna::ArrayView<float> outputWeights) const {
    convertInput(input);
    getDistanceWeight(targets, input, outputWeights, radius);

    normalizeAndCutOff(outputWeights);
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::rbfs)

// Copyright Epic Games, Inc. All Rights Reserved.

#include <rig/rbfs/RBFSolver.h>
#include <rig/rbfs/AdditiveRBFSolver.h>
#include <rig/rbfs/InterpolativeRBFSolver.h>
#include <rig/rbfs/RBFSolverBase.h>

#include <pma/PolyAllocator.h>

#include <cassert>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::rbfs)

RBFSolver::~RBFSolver() = default;

RBFSolver* RBFSolver::create(RBFSolverRecipe recipe, pma::MemoryResource* memRes) {
    const auto solverType = recipe.solverType;

    if (solverType == RBFSolverType::Interpolative) {
        pma::PolyAllocator<InterpolativeRBFSolver> alloc{memRes};
        return alloc.newObject(recipe, memRes);
    }
    pma::PolyAllocator<AdditiveRBFSolver> alloc{memRes};
    return alloc.newObject(recipe, memRes);
}

RBFSolver* RBFSolver::create(pma::MemoryResource* memRes) {
    pma::PolyAllocator<InterpolativeRBFSolver> alloc{memRes};
    return alloc.newObject(memRes);
}

RBFSolver* RBFSolver::create(RBFSolver* other) {
    const auto solverType = other->getSolverType();
    auto memRes = static_cast<RBFSolverBase*>(other)->getMemoryResource();
    if (solverType == RBFSolverType::Interpolative) {
        pma::PolyAllocator<InterpolativeRBFSolver> alloc{memRes};
        return alloc.newObject(static_cast<InterpolativeRBFSolver*>(other));
    }
    pma::PolyAllocator<AdditiveRBFSolver> alloc{memRes};
    return alloc.newObject(static_cast<AdditiveRBFSolver*>(other));
}

void RBFSolver::destroy(RBFSolver* solverInstance) {
    auto instance = static_cast<RBFSolverBase*>(solverInstance);
    if (instance->getSolverType() == RBFSolverType::Interpolative) {
        pma::PolyAllocator<InterpolativeRBFSolver> alloc{instance->getMemoryResource()};
        alloc.deleteObject(static_cast<InterpolativeRBFSolver*>(instance));
    } else if (instance->getSolverType() == RBFSolverType::Additive) {
        pma::PolyAllocator<AdditiveRBFSolver> alloc{instance->getMemoryResource()};
        alloc.deleteObject(static_cast<AdditiveRBFSolver*>(instance));
    }
    // Unreachable code
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE::rbfs)

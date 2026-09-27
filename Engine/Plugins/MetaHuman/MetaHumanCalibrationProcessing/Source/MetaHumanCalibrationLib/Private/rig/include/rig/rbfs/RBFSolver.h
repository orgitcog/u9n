// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <dna/Reader.h>
#include <pma/TypeDefs.h>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <cstdint>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::rbfs)

enum class RBFSolverType {
    /** The additive solver sums up contributions from each target. It's faster
        but may require more targets for a good coverage, and requires the
        normalization step to be performed for smooth results.
    */
    Additive,
    /** The interpolative solver interpolates the values from each target based
        on distance. As long as the input values are within the area bounded by
        the targets, the interpolation is well-behaved and returns weight values
        within the 0% - 100% limit with no normalization required.
        Interpolation also gives smoother results, with fewer targets than
        additive solver, but at a higher computational cost.
    */
    Interpolative
};

enum class RBFFunctionType {
    Gaussian,
    Exponential,
    Linear,
    Cubic,
    Quintic,
};

enum class RBFDistanceMethod {
    // Standard n-dimensional distance measure
    Euclidean,
    // Treat inputs as quaternion
    Quaternion,
    // Treat inputs as quaternion, and find distance between rotated TwistAxis direction
    SwingAngle,
    // Treat inputs as half quaternion, and find distance between rotations around the TwistAxis direction
    TwistAngle,
};

enum class RBFNormalizeMethod {
    OnlyNormalizeAboveOne,
    AlwaysNormalize
};

enum class AutomaticRadius {
    On,
    Off
};

enum class TwistAxis {
    X,
    Y,
    Z
};

struct RBFSolverRecipe {
    RBFSolverType solverType;
    RBFDistanceMethod distanceMethod;
    RBFFunctionType weightFunction;
    RBFNormalizeMethod normalizeMethod;
    TwistAxis twistAxis;
    bool isAutomaticRadius;
    float radius;
    float weightThreshold;
    std::uint16_t rawControlCount;
    dna::ConstArrayView<float> targetValues;
    dna::ConstArrayView<float> targetScales;
};

class RBFSolver {
    public:
        static RBFSolver* create(RBFSolverRecipe recipe, pma::MemoryResource* memRes = nullptr);
        static RBFSolver* create(pma::MemoryResource* memRes = nullptr);
        static RBFSolver* create(RBFSolver* other);

        static void destroy(RBFSolver* instance);

    public:
        virtual ~RBFSolver();

        virtual RBFSolverType getSolverType() const = 0;
        virtual void solve(dna::ArrayView<float> input, dna::ArrayView<float> intermediateWeights,
                           dna::ArrayView<float> outputWeights) const = 0;

        virtual dna::ConstArrayView<float> getTarget(std::uint16_t targetIndex) const = 0;
        virtual std::uint16_t getTargetCount() const = 0;

        virtual dna::ConstArrayView<float> getTargetScales() const = 0;
        virtual float getRadius() const = 0;
        virtual float getWeightThreshold() const = 0;
        virtual RBFDistanceMethod getDistanceMethod() const = 0;
        virtual RBFFunctionType getWeightFunction() const = 0;
        virtual RBFNormalizeMethod getNormalizeMethod() const = 0;
        virtual TwistAxis getTwistAxis() const = 0;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::rbfs)

namespace pma {

template<>
struct DefaultInstanceCreator<TITAN_NAMESPACE::rbfs::RBFSolver> {
    using type = FactoryCreate<TITAN_NAMESPACE::rbfs::RBFSolver>;
};

template<>
struct DefaultInstanceDestroyer<TITAN_NAMESPACE::rbfs::RBFSolver> {
    using type = FactoryDestroy<TITAN_NAMESPACE::rbfs::RBFSolver>;
};

}  // namespace pma

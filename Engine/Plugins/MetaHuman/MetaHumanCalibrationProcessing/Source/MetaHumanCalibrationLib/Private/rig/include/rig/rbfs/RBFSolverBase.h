// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <pma/MemoryResource.h>
#include <rig/rbfs/RBFSolver.h>

#include <dna/Reader.h>
#include <pma/TypeDefs.h>
#include <pma/resources/AlignedMemoryResource.h>

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <cstdint>
#include <functional>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::rbfs)

static constexpr std::size_t cacheLineAlignment = 64ul;

template<typename T>
using AlignedAllocator = pma::PolyAllocator<T, cacheLineAlignment, pma::AlignedMemoryResource>;

template<typename T>
using AlignedVector = pma::Vector<T, AlignedAllocator<T>>;

template<typename T, typename Allocator = pma::PolyAllocator<pma::Vector<T> > >
using Matrix = pma::Vector<pma::Vector<T>, Allocator>;


class RBFSolverBase : public RBFSolver {

    public:
        using DistanceWeightFun = std::function<void (dna::ConstArrayView<AlignedVector<float> >, dna::ConstArrayView<float>,
                                                      dna::ArrayView<float>,
                                                      float)>;
        using InputConvertFun = std::function<void (dna::ArrayView<float>)>;

        explicit RBFSolverBase(dna::MemoryResource* memRes);
        RBFSolverBase(const RBFSolverRecipe& recipe, dna::MemoryResource* memRes);

        virtual ~RBFSolverBase() = 0;

        RBFSolverBase& operator=(const RBFSolverBase&) = default;
        RBFSolverBase(const RBFSolverBase&) = default;


        dna::MemoryResource* getMemoryResource() const;

        dna::ConstArrayView<float> getTarget(std::uint16_t targetIndex) const override;
        std::uint16_t getTargetCount() const override;

        dna::ConstArrayView<float> getTargetScales() const override;
        float getRadius() const override;
        float getWeightThreshold() const override;
        RBFDistanceMethod getDistanceMethod() const override;
        RBFFunctionType getWeightFunction() const override;
        RBFNormalizeMethod getNormalizeMethod() const override;
        TwistAxis getTwistAxis() const override;

    protected:
        void normalizeAndCutOff(dna::ArrayView<float> outputWeights) const;

    protected:
        dna::Vector<AlignedVector<float> > targets;
        dna::Vector<float> targetScale;
        DistanceWeightFun getDistanceWeight;
        InputConvertFun convertInput;
        float radius;
        float weightThreshold;
        RBFDistanceMethod distanceMethod;
        RBFFunctionType weightFunction;
        RBFNormalizeMethod normalizeMethod;
        TwistAxis twistAxis;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::rbfs)

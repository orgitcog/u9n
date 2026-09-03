// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorRefinementUtils.h"
#include <carbon/Common.h>
#include <dna/Reader.h>
#include <dna/Writer.h>
#include <functional>

namespace TITAN_API_NAMESPACE
{

struct OperationFactory
{
    using OpFunc = std::function<dna::Position (const dna::Position&, const dna::Position&, float)>;

    static OpFunc create(Operation operation)
    {
        switch (operation)
        {
            case Operation::Add:
            {
                return [](const dna::Position& a, const dna::Position& b, float weight){
                           return a + b * weight;
                };
            }
            case Operation::Substract:
            {
                return [](const dna::Position& a, const dna::Position& b, float weight){
                           return (b - a) * weight;
                };
            }
            default:
            {
                CARBON_CRITICAL("Invalid operation.");
            }
        }
    }
};

void ApplyDNAInternal(dna::Reader* dna1, dna::Reader* dna2, dna::Writer* resultDna, Operation operation,
                      const std::vector<float>& mask)
{
    std::vector<float> processedMask = mask.size() > 0 ? mask : std::vector<float>(dna1->getVertexPositionCount(0), 1.0f);
    if (processedMask.size() != dna1->getVertexPositionCount(0))
    {
        CARBON_CRITICAL("Invalid mask size. Should be the same as number of vertices.");
    }

    const std::uint16_t dna1MeshCount = dna1->getMeshCount();
    const std::uint16_t dna2MeshCount = dna2->getMeshCount();
    if (dna1MeshCount != dna2MeshCount)
    {
        CARBON_CRITICAL("Different topology - mesh count: {} vs {}", dna1MeshCount, dna2MeshCount);
    }

    const std::uint16_t dna1JointCount = dna1->getJointCount();
    // const std::uint16_t dna2JointCount = dna2->getJointCount();
    // if (dna1JointCount != dna2JointCount){
    // CARBON_CRITICAL("Different topology - joint count: {} vs {}", dna1JointCount, dna2JointCount);
    // }

    if (operation == Operation::Add)
    {
        resultDna->setFrom(dna1);
    }
    else
    {
        resultDna->setLODCount(dna1->getLODCount());
        for (std::uint16_t i = 0; i < dna1->getMeshCount(); ++i)
        {
            resultDna->setMeshName(i, dna1->getMeshName(i).c_str());
        }
        for (std::uint16_t i = 0; i < dna1JointCount; ++i)
        {
            resultDna->setJointName(i, dna1->getJointName(i).data());
        }
    }

    OperationFactory::OpFunc funcToApply = OperationFactory::create(operation);

    auto UpdateVertices = [&](uint16_t mIdx, const std::vector<float>& locMask) {
            const std::uint32_t size1 = dna1->getVertexPositionCount(mIdx);
            const std::uint32_t size2 = dna2->getVertexPositionCount(mIdx);
            if (size1 != size2)
            {
                CARBON_CRITICAL("Different topology - vertex count for mesh {}: {} vs {}", dna1->getMeshName(mIdx).data(), size1, size2);
            }
            std::vector<float> maskToUse = mIdx == 0 ? locMask : std::vector<float>(static_cast<std::size_t>(size1), 1.0f);
            std::vector<dna::Position> result;
            result.reserve(size1);

            for (std::uint32_t i = 0; i < size1; ++i)
            {
                dna::Position res = funcToApply(dna1->getVertexPosition(mIdx, i), dna2->getVertexPosition(mIdx, i), maskToUse[i]);
                result.push_back(res);
            }
            resultDna->setVertexPositions(mIdx, result.data(), static_cast<std::uint16_t>(size1));
        };

    for (uint16_t meshIdx = 0; meshIdx < dna1->getMeshCount(); ++meshIdx)
    {
        UpdateVertices(meshIdx, processedMask);
    }

    const std::uint16_t jointsCount = dna1->getJointCount();
    std::vector<dna::Position> translations;
    translations.reserve(jointsCount);

    for (std::uint16_t i = 0; i < jointsCount; ++i)
    {
        dna::Position res = funcToApply(dna1->getNeutralJointTranslation(i), dna2->getNeutralJointTranslation(i), 1.0f);
        translations.push_back(res);
    }
    resultDna->setNeutralJointTranslations(translations.data(), jointsCount);

    std::vector<dna::Position> rotations;
    rotations.reserve(jointsCount);

    for (std::uint16_t i = 0; i < jointsCount; ++i)
    {
        dna::Position res = funcToApply(dna1->getNeutralJointRotation(i), dna2->getNeutralJointRotation(i), 1.0f);
        rotations.push_back(res);
    }
    resultDna->setNeutralJointRotations(rotations.data(), jointsCount);
}

} // namespace TITAN_API_NAMESPACE

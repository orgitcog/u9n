// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/controls/ControlsFactory.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/conditionaltable/ConditionalTable.h"
#include "riglogic/controls/Controls.h"
#include "riglogic/controls/instances/StandardControlsInputInstance.h"
#include "riglogic/controls/psdnet/PSDNet.h"
#include "riglogic/riglogic/Configuration.h"
#include "riglogic/riglogic/RigMetrics.h"
#include "riglogic/utils/Extd.h"

#include <cstdint>

namespace rl4 {

static ConditionalTable createConditionalTable(const dna::Reader* reader, MemoryResource* memRes) {
    Vector<std::uint16_t> inputIndices{memRes};
    Vector<std::uint16_t> outputIndices{memRes};
    Vector<float> fromValues{memRes};
    Vector<float> toValues{memRes};
    Vector<float> slopeValues{memRes};
    Vector<float> cutValues{memRes};
    extd::copy(reader->getGUIToRawInputIndices(), inputIndices);
    extd::copy(reader->getGUIToRawOutputIndices(), outputIndices);
    extd::copy(reader->getGUIToRawFromValues(), fromValues);
    extd::copy(reader->getGUIToRawToValues(), toValues);
    extd::copy(reader->getGUIToRawSlopeValues(), slopeValues);
    extd::copy(reader->getGUIToRawCutValues(), cutValues);
    // DNAs may contain these parameters in reverse order
    // i.e. the `from` value is actually larger than the `to` value
    assert(fromValues.size() == toValues.size());
    for (std::size_t i = 0ul; i < fromValues.size(); ++i) {
        if (fromValues[i] > toValues[i]) {
            std::swap(fromValues[i], toValues[i]);
        }
    }

    const auto guiControlCount = reader->getGUIControlCount();
    const auto rawControlCount = reader->getRawControlCount();

    return ConditionalTable{std::move(inputIndices),
                            std::move(outputIndices),
                            std::move(fromValues),
                            std::move(toValues),
                            std::move(slopeValues),
                            std::move(cutValues),
                            guiControlCount,
                            rawControlCount,
                            memRes};
}

static PSDNet createPSDNet(const dna::Reader* reader, MemoryResource* memRes) {
    Matrix<std::uint16_t> inputLODs{memRes};
    Matrix<std::uint16_t> outputLODs{memRes};
    Vector<PSD> psds{memRes};
    Vector<std::uint16_t> inputIndicesPerPSD{memRes};

    const auto lodCount = reader->getLODCount();
    const auto rawControlCount = reader->getRawControlCount();
    const auto psdCount = reader->getPSDCount();
    const auto psdRows = reader->getPSDRowIndices();
    const auto psdCols = reader->getPSDColumnIndices();
    const auto psdWeights = reader->getPSDValues();
    auto minPSD = rawControlCount;
    auto maxPSD = static_cast<std::uint16_t>(0);
    if (psdCount > 0u) {
        maxPSD = static_cast<std::uint16_t>(static_cast<std::uint32_t>(minPSD) + static_cast<std::uint32_t>(psdCount) - 1u);
    }

    inputLODs.resize(lodCount);
    outputLODs.resize(lodCount);
    for (auto& indices : inputLODs) {
        indices.reserve(rawControlCount);
    }
    for (auto& indices : outputLODs) {
        indices.reserve(psdCount);
    }
    psds.resize(psdCount);
    inputIndicesPerPSD.reserve(psdCols.size());
    for (std::size_t start = {}; start < psdRows.size(); ++start) {
        const auto psdOutputIndex = psdRows[start];
        PSD& psd = psds[static_cast<std::size_t>(psdOutputIndex) - static_cast<std::size_t>(rawControlCount)];
        if (psd.size != 0ul) {
            continue;
        }

        psd.weight = 1.0f;
        psd.offset = inputIndicesPerPSD.size();
        for (std::size_t i = start; i < psdRows.size(); ++i) {
            if (psdRows[i] == psdOutputIndex) {
                inputIndicesPerPSD.push_back(psdCols[i]);
                psd.weight *= psdWeights[i];
            }
        }
        psd.size = inputIndicesPerPSD.size() - psd.offset;
    }

    return PSDNet{std::move(inputLODs),
                  std::move(outputLODs),
                  std::move(inputIndicesPerPSD),
                  std::move(psds),
                  minPSD,
                  maxPSD};
}

static Vector<ControlInitializer> createInitialControlValues(const dna::Reader* reader, MemoryResource* memRes) {
    auto endsWith = [](StringView str, StringView suffix) {
            #if __cplusplus >= 201402L || (defined(_MSC_VER) && _MSC_VER >= 1900)
                return (suffix.size() <= str.size()) && std::equal(extd::advanced(str.begin(), str.size() - suffix.size()),
                                                                   str.end(),
                                                                   suffix.begin(),
                                                                   suffix.end());
            #else
                return (suffix.size() <= str.size()) && std::equal(extd::advanced(str.begin(), str.size() - suffix.size()),
                                                                   str.end(),
                                                                   suffix.begin());
            #endif
        };
    auto makeSuffix = [](const char* str) {
            return StringView{str, std::strlen(str)};
        };
    const auto qwSuffix = makeSuffix(".w");
    const auto rawControlCount = static_cast<std::size_t>(reader->getRawControlCount());
    const auto psdControlCount = static_cast<std::size_t>(reader->getPSDCount());
    const auto mlControlCount = static_cast<std::size_t>(reader->getMLControlCount());
    const auto rbfControlCount = static_cast<std::size_t>(reader->getRBFPoseControlCount());
    const auto controlCount = rawControlCount + psdControlCount + mlControlCount + rbfControlCount;
    Vector<ControlInitializer> initialValues{memRes};
    initialValues.reserve(controlCount - rawControlCount);
    for (std::uint16_t solverIndex = {}; solverIndex < reader->getRBFSolverCount(); ++solverIndex) {
        const auto solverRawControlIndices = reader->getRBFSolverRawControlIndices(solverIndex);
        for (const auto rawControlIndex : solverRawControlIndices) {
            const auto rawControlName = reader->getRawControlName(rawControlIndex);
            if (endsWith(rawControlName, qwSuffix)) {
                initialValues.push_back({rawControlIndex, 1.0f});
            }
        }
    }
    return initialValues;
}

static ControlsInputInstance::Factory createInstanceFactory(const Configuration&  /*unused*/,
                                                            std::uint16_t guiControlCount,
                                                            std::uint16_t rawControlCount,
                                                            std::uint16_t psdControlCount,
                                                            std::uint16_t mlControlCount,
                                                            std::uint16_t rbfControlCount) {
    return [ = ](ConstArrayView<ControlInitializer> initialValues, MemoryResource* memRes) {
               auto factory = UniqueInstance<StandardControlsInputInstance, ControlsInputInstance>::with(memRes);
               return factory.create(guiControlCount,
                                     rawControlCount,
                                     psdControlCount,
                                     mlControlCount,
                                     rbfControlCount,
                                     initialValues,
                                     memRes);
    };
}

Controls::Pointer ControlsFactory::create(const Configuration& config, const RigMetrics& metrics, MemoryResource* memRes) {
    auto instanceFactory = createInstanceFactory(config,
                                                 metrics.guiControlCount,
                                                 metrics.rawControlCount,
                                                 metrics.psdControlCount,
                                                 metrics.mlControlCount,
                                                 metrics.rbfControlCount);
    return UniqueInstance<Controls>::with(memRes).create(ConditionalTable{memRes},
                                                         PSDNet{memRes},
                                                         Vector<ControlInitializer>{memRes},
                                                         instanceFactory);
}

Controls::Pointer ControlsFactory::create(const Configuration& config, const dna::Reader* reader, MemoryResource* memRes) {
    const auto guiControlCount = reader->getGUIControlCount();
    const auto rawControlCount = reader->getRawControlCount();
    const auto psdControlCount = reader->getPSDCount();
    const auto mlControlCount = reader->getMLControlCount();
    const auto rbfControlCount = reader->getRBFPoseControlCount();

    ConditionalTable conditionals = createConditionalTable(reader, memRes);
    PSDNet psds = createPSDNet(reader, memRes);
    Vector<ControlInitializer> initialValues = createInitialControlValues(reader, memRes);
    auto instanceFactory = createInstanceFactory(config,
                                                 guiControlCount,
                                                 rawControlCount,
                                                 psdControlCount,
                                                 mlControlCount,
                                                 rbfControlCount);

    return UniqueInstance<Controls>::with(memRes).create(std::move(conditionals),
                                                         std::move(psds),
                                                         std::move(initialValues),
                                                         instanceFactory);
}

}  // namespace rl4

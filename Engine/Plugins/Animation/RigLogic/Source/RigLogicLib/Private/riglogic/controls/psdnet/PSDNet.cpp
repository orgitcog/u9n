// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/controls/psdnet/PSDNet.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/utils/Extd.h"

#include <cstddef>

namespace rl4 {

namespace {

constexpr float minPSDValue = 0.0f;
constexpr float maxPSDValue = 1.0f;

}  // namespace

PSDNet::PSDNet(MemoryResource* memRes) :
    inputLODs{memRes},
    outputLODs{memRes},
    inputIndicesPerPSD{memRes},
    psds{memRes},
    psdMinIndex{},
    psdMaxIndex{} {
}

PSDNet::PSDNet(Matrix<std::uint16_t>&& inputLODs_,
               Matrix<std::uint16_t>&& outputLODs_,
               Vector<std::uint16_t>&& inputIndicesPerPSD_,
               Vector<PSD>&& psds_,
               std::uint16_t psdMinIndex_,
               std::uint16_t psdMaxIndex_) :
    inputLODs{inputLODs_},
    outputLODs{outputLODs_},
    inputIndicesPerPSD{std::move(inputIndicesPerPSD_)},
    psds{std::move(psds_)},
    psdMinIndex{psdMinIndex_},
    psdMaxIndex{psdMaxIndex_} {
}

void PSDNet::registerControls(std::uint16_t lod, ConstArrayView<std::uint16_t> controlIndices) {
    if ((controlIndices.size() == 0ul) || (psds.size() == 0ul)) {
        return;
    }

    // Memory already reserved in both buffers
    auto& inputIndices = inputLODs[lod];
    auto& outputIndices = outputLODs[lod];

    for (auto controlIndex : controlIndices) {
        if ((controlIndex >= psdMinIndex) && (controlIndex <= psdMaxIndex)) {
            outputIndices.push_back(controlIndex);
            const PSD psd = psds[static_cast<std::size_t>(controlIndex) - static_cast<std::size_t>(psdMinIndex)];
            for (std::size_t i = psd.offset; i < psd.offset + psd.size; ++i) {
                inputIndices.push_back(inputIndicesPerPSD[i]);
            }
        }
    }

    std::sort(inputIndices.begin(), inputIndices.end());
    std::sort(outputIndices.begin(), outputIndices.end());
    inputIndices.erase(std::unique(inputIndices.begin(), inputIndices.end()), inputIndices.end());
    outputIndices.erase(std::unique(outputIndices.begin(), outputIndices.end()), outputIndices.end());
}

std::uint16_t PSDNet::getPSDCount() const {
    return static_cast<std::uint16_t>(psds.size());
}

ConstArrayView<std::uint16_t> PSDNet::getPSDInputIndicesForLOD(std::uint16_t lod) const {
    assert(lod < inputLODs.size());
    return inputLODs[lod];
}

ConstArrayView<std::uint16_t> PSDNet::getPSDOutputIndicesForLOD(std::uint16_t lod) const {
    assert(lod < outputLODs.size());
    return outputLODs[lod];
}

void PSDNet::calculate(ArrayView<float> inputs, ArrayView<float> clampBuffer, std::uint16_t lod) const {
    ConstArrayView<std::uint16_t> inputIndices = inputLODs[lod];
    ConstArrayView<std::uint16_t> outputIndices = outputLODs[lod];

    for (auto inputIndex : inputIndices) {
        clampBuffer[inputIndex] = extd::clamp(inputs[inputIndex], minPSDValue, maxPSDValue);
    }

    for (auto outputIndex : outputIndices) {
        const PSD psd = psds[static_cast<std::size_t>(outputIndex) - static_cast<std::size_t>(psdMinIndex)];
        float psdOutput = psd.weight;
        for (std::size_t i = psd.offset; i < psd.offset + psd.size; ++i) {
            const float input = clampBuffer[inputIndicesPerPSD[i]];
            psdOutput *= input;
        }
        inputs[outputIndex] = std::min(maxPSDValue, psdOutput);
    }
}

}  // namespace rl4

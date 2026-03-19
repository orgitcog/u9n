// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"

#include <cstddef>

namespace rl4 {

struct PSD {
    std::size_t offset;
    std::size_t size;
    float weight;

    template<class Archive>
    void serialize(Archive& archive) {
        archive(offset, size, weight);
    }

};

class PSDNet {
    public:
        explicit PSDNet(MemoryResource* memRes);
        PSDNet(Matrix<std::uint16_t>&& inputLODs_,
               Matrix<std::uint16_t>&& outputLODs_,
               Vector<std::uint16_t>&& inputIndicesPerPSD_,
               Vector<PSD>&& psds_,
               std::uint16_t psdMinIndex_,
               std::uint16_t psdMaxIndex_);

        void registerControls(std::uint16_t lod, ConstArrayView<std::uint16_t> controlIndices);
        std::uint16_t getPSDCount() const;
        ConstArrayView<std::uint16_t> getPSDInputIndicesForLOD(std::uint16_t lod) const;
        ConstArrayView<std::uint16_t> getPSDOutputIndicesForLOD(std::uint16_t lod) const;
        void calculate(ArrayView<float> inputs, ArrayView<float> clampBuffer, std::uint16_t lod) const;

        template<class Archive>
        void serialize(Archive& archive) {
            archive(inputLODs, outputLODs, inputIndicesPerPSD, psds, psdMinIndex, psdMaxIndex);
        }

    private:
        Matrix<std::uint16_t> inputLODs;
        Matrix<std::uint16_t> outputLODs;
        Vector<std::uint16_t> inputIndicesPerPSD;
        Vector<PSD> psds;
        std::uint16_t psdMinIndex;
        std::uint16_t psdMaxIndex;
};

}  // namespace rl4

// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/controls/instances/StandardControlsInputInstance.h"

#include "riglogic/utils/Extd.h"

#include <cstdint>
#include <cstddef>

namespace rl4 {

StandardControlsInputInstance::StandardControlsInputInstance(std::uint16_t guiControlCount_,
                                                             std::uint16_t rawControlCount_,
                                                             std::uint16_t psdControlCount_,
                                                             std::uint16_t mlControlCount_,
                                                             std::uint16_t rbfControlCount_,
                                                             ConstArrayView<ControlInitializer> initialValues_,
                                                             MemoryResource* memRes) :
    guiControlBuffer{guiControlCount_, {}, memRes},
    inputBuffer{static_cast<std::size_t>(rawControlCount_) +
                static_cast<std::size_t>(psdControlCount_) +
                static_cast<std::size_t>(mlControlCount_) +
                static_cast<std::size_t>(rbfControlCount_), {}, memRes},
    clampBuffer{inputBuffer.size(), {}, memRes},
    initialValues{initialValues_.begin(), initialValues_.end(), memRes},
    guiControlCount{guiControlCount_},
    rawControlCount{rawControlCount_},
    psdControlCount{psdControlCount_},
    mlControlCount{mlControlCount_},
    rbfControlCount{rbfControlCount_} {

    resetInternalInputBuffer();
}

void StandardControlsInputInstance::resetInternalInputBuffer() {
    std::fill(extd::advanced(inputBuffer.begin(), rawControlCount), inputBuffer.end(), 0.0f);
    for (const auto& initializer : initialValues) {
        inputBuffer[initializer.index] = initializer.value;
    }
}

ArrayView<float> StandardControlsInputInstance::getGUIControlBuffer() {
    return ArrayView<float>{guiControlBuffer};
}

ArrayView<float> StandardControlsInputInstance::getInputBuffer() {
    return ArrayView<float>{inputBuffer};
}

ArrayView<float> StandardControlsInputInstance::getClampBuffer() {
    return ArrayView<float>{clampBuffer};
}

ConstArrayView<float> StandardControlsInputInstance::getGUIControlBuffer() const {
    return ConstArrayView<float>{guiControlBuffer};
}

ConstArrayView<float> StandardControlsInputInstance::getInputBuffer() const {
    return ConstArrayView<float>{inputBuffer};
}

ConstArrayView<float> StandardControlsInputInstance::getClampBuffer() const {
    return ConstArrayView<float>{clampBuffer};
}

std::uint16_t StandardControlsInputInstance::getGUIControlCount() const {
    return guiControlCount;
}

std::uint16_t StandardControlsInputInstance::getRawControlCount() const {
    return rawControlCount;
}

std::uint16_t StandardControlsInputInstance::getPSDControlCount() const {
    return psdControlCount;
}

std::uint16_t StandardControlsInputInstance::getMLControlCount() const {
    return mlControlCount;
}

std::uint16_t StandardControlsInputInstance::getRBFControlCount() const {
    return rbfControlCount;
}

}  // namespace rl4

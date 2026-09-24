// Copyright Epic Games, Inc. All Rights Reserved.

#include "riglogic/controls/Controls.h"

#include "riglogic/conditionaltable/ConditionalTable.h"
#include "riglogic/controls/ControlsInputInstance.h"
#include "riglogic/controls/psdnet/PSDNet.h"

#include <cassert>
#include <cstdint>

namespace rl4 {

Controls::Controls(ConditionalTable&& guiToRawMapping_,
                   PSDNet&& psds_,
                   Vector<ControlInitializer>&& initialValues_,
                   ControlsInputInstance::Factory instanceFactory_) :
    guiToRawMapping{std::move(guiToRawMapping_)},
    psds{std::move(psds_)},
    initialValues{std::move(initialValues_)},
    instanceFactory{instanceFactory_} {
}

ControlsInputInstance::Pointer Controls::createInstance(MemoryResource* instanceMemRes) const {
    return instanceFactory(initialValues, instanceMemRes);
}

void Controls::registerControls(std::uint16_t lod, ConstArrayView<std::uint16_t> controlIndices) {
    psds.registerControls(lod, controlIndices);
}

ConstArrayView<std::uint16_t> Controls::getPSDIndicesForLOD(std::uint16_t lod) const {
    return psds.getPSDOutputIndicesForLOD(lod);
}

void Controls::mapGUIToRaw(ControlsInputInstance* instance) const {
    auto guiControlBuffer = instance->getGUIControlBuffer();
    assert(guiControlBuffer.size() == guiToRawMapping.getInputCount());
    auto inputBuffer = instance->getInputBuffer();
    guiToRawMapping.calculateForward(guiControlBuffer.data(), inputBuffer.data());
}

void Controls::mapRawToGUI(ControlsInputInstance* instance) const {
    auto guiControlBuffer = instance->getGUIControlBuffer();
    assert(guiControlBuffer.size() == guiToRawMapping.getInputCount());
    auto inputBuffer = instance->getInputBuffer();
    guiToRawMapping.calculateReverse(guiControlBuffer.data(), inputBuffer.data());
}

void Controls::calculate(ControlsInputInstance* instance, std::uint16_t lod) const {
    psds.calculate(instance->getInputBuffer(), instance->getClampBuffer(), lod);
}

}  // namespace rl4

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "riglogic/TypeDefs.h"
#include "riglogic/conditionaltable/ConditionalTable.h"
#include "riglogic/controls/ControlsInputInstance.h"
#include "riglogic/controls/psdnet/PSDNet.h"

#include <cstdint>

namespace rl4 {

class Controls {
    public:
        using Pointer = UniqueInstance<Controls>::PointerType;

    public:
        Controls(ConditionalTable&& guiToRawMapping_,
                 PSDNet&& psds_,
                 Vector<ControlInitializer>&& initialValues_,
                 ControlsInputInstance::Factory instanceFactory_);

        ControlsInputInstance::Pointer createInstance(MemoryResource* instanceMemRes) const;
        void registerControls(std::uint16_t lod, ConstArrayView<std::uint16_t> controlIndices);
        ConstArrayView<std::uint16_t> getPSDIndicesForLOD(std::uint16_t lod) const;
        void mapGUIToRaw(ControlsInputInstance* instance) const;
        void mapRawToGUI(ControlsInputInstance* instance) const;
        void calculate(ControlsInputInstance* instance, std::uint16_t lod) const;

        template<class Archive>
        void serialize(Archive& archive) {
            archive(guiToRawMapping, psds, initialValues);
        }

    private:
        ConditionalTable guiToRawMapping;
        PSDNet psds;
        Vector<ControlInitializer> initialValues;
        ControlsInputInstance::Factory instanceFactory;

};

}  // namespace rl4

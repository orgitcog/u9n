// Copyright Epic Games, Inc. All Rights Reserved.

#include "rltests/Defs.h"
#include "rltests/conditionaltable/ConditionalTableFixtures.h"
#include "rltests/controls/ControlFixtures.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/controls/Controls.h"
#include "riglogic/controls/psdnet/PSDNet.h"

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <array>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

TEST(ControlsTest, GUIToRawMapping) {
    pma::AlignedMemoryResource amr;
    auto conditionals = ConditionalTableFactory::withMultipleIODefaults(&amr);

    rl4::PSDNet psds{{}, {}, {}, {}, {}, {}};
    const std::uint16_t guiControlCount = conditionals.getInputCount();
    const std::uint16_t rawControlCount = conditionals.getOutputCount();
    const std::uint16_t psdControlCount = psds.getPSDCount();
    auto instanceFactory = ControlsFactory::getInstanceFactory(guiControlCount, rawControlCount, psdControlCount, 0u, 0u);

    rl4::Vector<rl4::ControlInitializer> initialValues;
    rl4::Controls controls{std::move(conditionals), std::move(psds), std::move(initialValues), instanceFactory};

    const rl4::Vector<float> guiControls{0.1f, 0.2f};
    const rl4::Vector<float> expected{0.3f, 0.6f};

    auto instance = controls.createInstance(&amr);
    auto guiBuffer = instance->getGUIControlBuffer();
    auto rawBuffer = instance->getInputBuffer();
    std::copy(guiControls.begin(), guiControls.end(), guiBuffer.begin());
    controls.mapGUIToRaw(instance.get());

    ASSERT_EQ(rawBuffer.size(), expected.size());
    ASSERT_ELEMENTS_EQ(rawBuffer, expected, expected.size());
}

TEST(ControlsTest, PSDsAppendToOutput) {
    pma::AlignedMemoryResource amr;
    auto conditionals = ConditionalTableFactory::withMultipleIODefaults(&amr);

    rl4::Vector<float> rawControls{0.1f, 0.2f};
    const rl4::Vector<float> expected{0.1f, 0.2f, 0.24f, 0.02f};

    rl4::Matrix<std::uint16_t> inputLODs{{0u, 1u}};
    rl4::Matrix<std::uint16_t> outputLODs{{2u, 3u}};
    rl4::Vector<std::uint16_t> cols{0u, 1u, 0u, 1u};
    // PSD weights {4.0f, 3.0f, 0.5f, 2.0f}
    rl4::Vector<rl4::PSD> psds{{0u, 2u, 12.0f}, {2u, 2u, 1.0f}};
    rl4::PSDNet psdNet{std::move(inputLODs),
                       std::move(outputLODs),
                       std::move(cols),
                       std::move(psds),
                       2u,
                       3u};

    const std::uint16_t guiControlCount = conditionals.getInputCount();
    const std::uint16_t rawControlCount = conditionals.getOutputCount();
    const std::uint16_t psdControlCount = psdNet.getPSDCount();
    auto instanceFactory = ControlsFactory::getInstanceFactory(guiControlCount, rawControlCount, psdControlCount, 0u, 0u);
    rl4::Vector<rl4::ControlInitializer> initialValues;
    rl4::Controls controls{std::move(conditionals), std::move(psdNet), std::move(initialValues), instanceFactory};
    auto instance = controls.createInstance(&amr);
    auto buffer = instance->getInputBuffer();
    std::copy(rawControls.begin(), rawControls.end(), buffer.begin());

    controls.calculate(instance.get(), 0u);

    ASSERT_EQ(buffer.size(), expected.size());
    ASSERT_ELEMENTS_NEAR(buffer, expected, expected.size(), 0.0001f);
}

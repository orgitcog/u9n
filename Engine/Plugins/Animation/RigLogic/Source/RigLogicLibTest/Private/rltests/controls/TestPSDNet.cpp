// Copyright Epic Games, Inc. All Rights Reserved.

#include "rltests/Defs.h"

#include "riglogic/TypeDefs.h"
#include "riglogic/controls/psdnet/PSDNet.h"

TEST(PSDNetTest, OutputsAreClamped) {
    pma::AlignedMemoryResource amr;

    rl4::Matrix<std::uint16_t> inputLODs{{0u}};
    rl4::Matrix<std::uint16_t> outputLODs{{1u}};
    rl4::Vector<std::uint16_t> cols{0u};
    // PSD weights {100.0f}
    rl4::Vector<rl4::PSD> psds{{0u, 1u, 100.0f}};
    rl4::PSDNet PSDNet{std::move(inputLODs),
                       std::move(outputLODs),
                       std::move(cols),
                       std::move(psds),
                       1u,
                       1u};

    float inputs[] = {0.1f, 0.0f};
    float clampBuffer[] = {0.0f, 0.0f};
    const float expected[] = {1.0f};

    PSDNet.calculate({inputs, 2ul}, {clampBuffer, 2ul}, 0u);
    ASSERT_ELEMENTS_EQ((inputs + 1ul), expected, 1);
}

TEST(PSDNetTest, OutputsKeepExistingProduct) {
    pma::AlignedMemoryResource amr;

    rl4::Matrix<std::uint16_t> inputLODs{{0u, 1u}};
    rl4::Matrix<std::uint16_t> outputLODs{{2u}};
    rl4::Vector<std::uint16_t> cols{0u, 1u};
    // PSD weights {4.0f, 10.0f}
    rl4::Vector<rl4::PSD> psds{{0u, 2u, 40.0f}};
    rl4::PSDNet PSDNet{std::move(inputLODs),
                       std::move(outputLODs),
                       std::move(cols),
                       std::move(psds),
                       2u,
                       2u};

    float inputs[] = {0.1f, 0.2f, 0.0f};
    float clampBuffer[] = {0.0f, 0.0f, 0.0f};
    const float expected[] = {0.8f};

    PSDNet.calculate({inputs, 3ul}, {clampBuffer, 3ul}, 0ul);
    ASSERT_ELEMENTS_EQ((inputs + 2ul), expected, 1);
}

TEST(PSDNetTest, RowsSpecifyDestinationIndex) {
    pma::AlignedMemoryResource amr;

    rl4::Matrix<std::uint16_t> inputLODs{{0u, 1u}};
    rl4::Matrix<std::uint16_t> outputLODs{{2u, 3u}};
    rl4::Vector<std::uint16_t> cols{0u, 1u};
    // PSD weights {4.0f, 3.0f}
    rl4::Vector<rl4::PSD> psds{{0u, 1u, 4.0f}, {1u, 1u, 3.0f}};
    rl4::PSDNet PSDNet{std::move(inputLODs),
                       std::move(outputLODs),
                       std::move(cols),
                       std::move(psds),
                       2u,
                       2u};

    float inputs[] = {0.1f, 0.2f, 0.0f, 0.0f};
    float clampBuffer[] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float expected[] = {0.4f, 0.6f};

    PSDNet.calculate({inputs, 4ul}, {clampBuffer, 4ul}, 0ul);
    ASSERT_ELEMENTS_EQ((inputs + 2ul), expected, 2);
}

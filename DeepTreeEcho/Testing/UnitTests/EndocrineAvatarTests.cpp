/**
 * @file EndocrineAvatarTests.cpp
 * @brief Unit tests for the DeepTreeEcho/Avatar endocrine integration
 *        (Gap E of the 32-repo integration): the vendored elizaos-cpp
 *        virtual endocrine system (EndocrineBus.h/.cpp) and the
 *        EndocrineBusAdapter that maps hormone state to MetaHuman Rig
 *        Logic curves and somatic-marker channels.
 */

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

#include "Avatar/EndocrineBus.cpp"
#include "Avatar/EndocrineBusAdapter.h"

// ============================================================================
// Vendored endocrine core
// ============================================================================

TEST(EndocrineBusTest, HormoneBusClampsAndDecays)
{
    dte::HormoneBus bus;
    bus.reset(0.5);
    EXPECT_DOUBLE_EQ(bus.get(dte::HormoneId::Dopamine), 0.5);

    bus.set(dte::HormoneId::Dopamine, 1.7);
    EXPECT_DOUBLE_EQ(bus.get(dte::HormoneId::Dopamine), 1.0); // clamped

    bus.set(dte::HormoneId::Cortisol, -0.3);
    EXPECT_DOUBLE_EQ(bus.get(dte::HormoneId::Cortisol), 0.0); // clamped

    // Exponential decay toward baseline.
    bus.set(dte::HormoneId::Dopamine, 1.0);
    for (int i = 0; i < 50; ++i)
        bus.decay(dte::HormoneId::Dopamine, 0.5, 0.1);
    EXPECT_NEAR(bus.get(dte::HormoneId::Dopamine), 0.5, 0.01);
}

TEST(EndocrineBusTest, StimulusShiftsHormonesAndAffect)
{
    dte::EndocrineSystem sys;
    const double base_cortisol = sys.hormoneLevel(dte::HormoneId::Cortisol);

    // Threat stimulus should raise cortisol via the HPA axis gland
    // (routing source "threat" in the vendored stimulus router).
    sys.submitStimulus(dte::Stimulus("threat", 1.0));
    sys.tick();
    EXPECT_GT(sys.hormoneLevel(dte::HormoneId::Cortisol), base_cortisol);

    // Valence/arousal must stay in [-1, 1].
    const auto& va = sys.valenceArousal();
    EXPECT_GE(va.valence, -1.0);
    EXPECT_LE(va.valence, 1.0);
    EXPECT_GE(va.arousal, -1.0);
    EXPECT_LE(va.arousal, 1.0);
}

TEST(EndocrineBusTest, RewardRaisesDopamineAndPositiveValence)
{
    dte::EndocrineSystem sys;
    for (int i = 0; i < 5; ++i) {
        sys.submitStimulus(dte::Stimulus("action_success", 1.0));
        sys.tick();
    }
    EXPECT_GT(sys.hormoneLevel(dte::HormoneId::Dopamine), 0.5);
    EXPECT_GT(sys.valenceArousal().valence, 0.0);
}

TEST(EndocrineBusTest, ResetRestoresHomeostasis)
{
    // Each gland has a distinct homeostatic baseline (cortisol 0.3,
    // dopamine 0.4, serotonin 0.5, cytokine 0.2, ...). reset() must
    // restore exactly the same levels as a freshly constructed system.
    dte::EndocrineSystem fresh;
    dte::EndocrineSystem sys;
    sys.submitStimulus(dte::Stimulus("threat", 1.0));
    sys.tick();
    sys.reset();
    for (std::size_t i = 0; i < dte::HORMONE_COUNT; ++i) {
        const auto id = static_cast<dte::HormoneId>(i);
        EXPECT_DOUBLE_EQ(sys.hormoneLevel(id), fresh.hormoneLevel(id))
            << dte::hormoneName(id);
    }
    // Serotonin's configured baseline is 0.5 — spot-check one exact value.
    EXPECT_DOUBLE_EQ(sys.hormoneLevel(dte::HormoneId::Serotonin), 0.5);
}

// ============================================================================
// EndocrineBusAdapter — Rig Logic curve mapping
// ============================================================================

TEST(EndocrineBusAdapterTest, SampleMirrorsAllTenHormones)
{
    dte::EndocrineSystem sys;
    auto sample = dte::sampleEndocrineSystem(sys);
    EXPECT_EQ(sample.channels.size(), dte::HORMONE_COUNT);
    EXPECT_FALSE(sample.cognitive_mode.empty());
    EXPECT_GE(sample.dominance, 0.0);
    EXPECT_LE(sample.dominance, 1.0);

    // Names should match the vendored hormone names.
    EXPECT_EQ(sample.channels[0].name,
              std::string(dte::hormoneName(dte::HormoneId::Cortisol)));
}

TEST(EndocrineBusAdapterTest, StressDrivesBrowAndJawCurves)
{
    dte::EndocrineSystem sys;
    for (int i = 0; i < 6; ++i) {
        sys.submitStimulus(dte::Stimulus("threat", 1.0));
        sys.tick();
    }

    auto curves = dte::mapHormonesToRigCurves(sys);
    auto find = [&](const std::string& name) -> double {
        for (const auto& [n, w] : curves)
            if (n == name) return w;
        return -1.0;
    };

    // Elevated cortisol should express as brow-down tension.
    EXPECT_GT(find("CTRL_L_brow_down"), 0.0);
    EXPECT_GT(find("CTRL_R_brow_down"), 0.0);
    EXPECT_GT(find("CTRL_C_jaw_clench"), 0.0);

    // All curve weights must be normalized in [0, 1].
    for (const auto& [name, w] : curves) {
        EXPECT_GE(w, 0.0) << name;
        EXPECT_LE(w, 1.0) << name;
    }
}

TEST(EndocrineBusAdapterTest, HomeostasisProducesNeutralFace)
{
    dte::EndocrineSystem sys; // all hormones at baseline 0.5
    auto curves = dte::mapHormonesToRigCurves(sys);
    for (const auto& [name, w] : curves)
        EXPECT_NEAR(w, 0.0, 1e-9) << name;
}

// ============================================================================
// Somatic-marker channels (parity with GTAngel EsnReservoirPipeline)
// ============================================================================

TEST(EndocrineBusAdapterTest, SomaticMarkerChannelsExactOrderAndRange)
{
    dte::EndocrineSystem sys;
    sys.submitStimulus(dte::Stimulus("action_success", 0.8));
    sys.tick();

    auto ch = dte::somaticMarkerChannels(sys);
    // Order parity with C# side: dopamine, serotonin, cortisol, oxytocin,
    // valence, arousal.
    EXPECT_DOUBLE_EQ(ch[0], sys.hormoneLevel(dte::HormoneId::Dopamine));
    EXPECT_DOUBLE_EQ(ch[1], sys.hormoneLevel(dte::HormoneId::Serotonin));
    EXPECT_DOUBLE_EQ(ch[2], sys.hormoneLevel(dte::HormoneId::Cortisol));
    EXPECT_DOUBLE_EQ(ch[3], sys.hormoneLevel(dte::HormoneId::Oxytocin));

    for (double v : ch) {
        EXPECT_GE(v, 0.0);
        EXPECT_LE(v, 1.0);
    }
}

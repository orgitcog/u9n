/**
 * @file CognitiveActionArbiterTests.cpp
 * @brief Unit tests for DeepTreeEcho::CognitiveActionArbiter
 *
 * Pure C++ / Eigen, no UE dependencies — compiled by the root CMake glob
 * at DeepTreeEcho/Testing/UnitTests/*.cpp.
 */

#include <gtest/gtest.h>
#include <Eigen/Core>
#include <vector>
#include <cmath>

// ---------------------------------------------------------------------------
// Include the class under test.  The CMake include path adds
// ${CMAKE_SOURCE_DIR}/DeepTreeEcho, so we can use the relative path from
// that root.
// ---------------------------------------------------------------------------
#include "UnrealBridge/CognitiveActionArbiter.h"

using namespace DeepTreeEcho;

// ===========================================================================
// Helpers
// ===========================================================================

static std::vector<float> make_salience(std::initializer_list<float> vals)
{
    return std::vector<float>(vals);
}

// ===========================================================================
// Basic construction
// ===========================================================================

TEST(CognitiveActionArbiterTest, DefaultConstruction)
{
    CognitiveActionArbiter arb;
    EXPECT_FLOAT_EQ(arb.config().movement_threshold, 0.6f);
    EXPECT_FLOAT_EQ(arb.config().focus_threshold, 0.4f);
}

TEST(CognitiveActionArbiterTest, CustomConfig)
{
    ArbiterConfig cfg;
    cfg.movement_threshold = 0.7f;
    cfg.focus_threshold    = 0.3f;
    CognitiveActionArbiter arb(cfg);
    EXPECT_FLOAT_EQ(arb.config().movement_threshold, 0.7f);
    EXPECT_FLOAT_EQ(arb.config().focus_threshold, 0.3f);
}

// ===========================================================================
// Empty salience vector
// ===========================================================================

TEST(CognitiveActionArbiterTest, EmptySalienceReturnsNoAction)
{
    CognitiveActionArbiter arb;
    auto action = arb.compute(std::vector<float>{});

    ASSERT_EQ(action.size(), 3);
    EXPECT_FLOAT_EQ(action[0], 0.0f);  // no movement urgency
    EXPECT_FLOAT_EQ(action[1], 0.0f);  // no focus urgency
    EXPECT_FLOAT_EQ(action[2], -1.0f); // no target
}

// ===========================================================================
// Movement / focus thresholds
// ===========================================================================

TEST(CognitiveActionArbiterTest, LowSalienceNoMovement)
{
    CognitiveActionArbiter arb;
    // All salience values below movement threshold (0.6)
    auto action = arb.compute(make_salience({0.1f, 0.2f, 0.3f}));

    EXPECT_LT(action[0], 0.6f);
    EXPECT_FALSE(CognitiveActionArbiter::should_move(action));
    EXPECT_EQ(CognitiveActionArbiter::target_index(action), -1);
}

TEST(CognitiveActionArbiterTest, HighSalienceTriggersMoveAndFocus)
{
    CognitiveActionArbiter arb;
    auto action = arb.compute(make_salience({0.1f, 0.9f, 0.3f}));

    // Peak at index 1 with salience 0.9 in Reactive mode → movement urgency ≈ 0.9
    EXPECT_GE(action[0], 0.6f);
    EXPECT_TRUE(CognitiveActionArbiter::should_move(action));
    EXPECT_TRUE(CognitiveActionArbiter::should_focus(action));
    EXPECT_EQ(CognitiveActionArbiter::target_index(action), 1);
}

TEST(CognitiveActionArbiterTest, BestTargetIndexIsMaxSalience)
{
    CognitiveActionArbiter arb;
    auto action = arb.compute(make_salience({0.5f, 0.3f, 0.8f, 0.6f}));

    // Index 2 has the highest salience (0.8)
    EXPECT_EQ(CognitiveActionArbiter::target_index(action), 2);
}

// ===========================================================================
// Cognitive mode bias
// ===========================================================================

TEST(CognitiveActionArbiterTest, ReactiveModeMaxBias)
{
    CognitiveActionArbiter arb;
    const float salience = 0.8f;
    auto action_reactive = arb.compute(make_salience({salience}), CognitiveMode::Reactive);
    auto action_reflect  = arb.compute(make_salience({salience}), CognitiveMode::Reflective);

    // Reactive mode should produce higher movement urgency than Reflective
    EXPECT_GT(action_reactive[0], action_reflect[0]);
}

TEST(CognitiveActionArbiterTest, ReflectiveModeReducesMovementUrgency)
{
    CognitiveActionArbiter arb;
    const float salience = 0.65f;  // Just above default threshold in Reactive

    auto action = arb.compute(make_salience({salience}), CognitiveMode::Reflective);
    // Reflective bias = 0.5 → urgency = 0.65 * 0.5 = 0.325 < threshold 0.6
    EXPECT_LT(action[0], 0.6f);
    EXPECT_FALSE(CognitiveActionArbiter::should_move(action));
}

TEST(CognitiveActionArbiterTest, AllModeBiasesAreValid)
{
    ArbiterConfig cfg;
    for (auto mode : {CognitiveMode::Reactive,
                      CognitiveMode::Deliberative,
                      CognitiveMode::Reflective,
                      CognitiveMode::Creative,
                      CognitiveMode::Integrative})
    {
        float bias = cfg.mode_movement_bias(mode);
        EXPECT_GE(bias, 0.0f) << "Bias must be non-negative";
        EXPECT_LE(bias, 1.0f) << "Bias must not exceed 1";
    }
}

// ===========================================================================
// Action vector clamping (urgency stays in [0, 1])
// ===========================================================================

TEST(CognitiveActionArbiterTest, MovementUrgencyClampedToOne)
{
    CognitiveActionArbiter arb;
    // Even if salience is exactly 1.0, urgency must not exceed 1.0
    auto action = arb.compute(make_salience({1.0f}));
    EXPECT_LE(action[0], 1.0f);
    EXPECT_GE(action[0], 0.0f);
}

TEST(CognitiveActionArbiterTest, FocusUrgencyClampedToOne)
{
    CognitiveActionArbiter arb;
    auto action = arb.compute(make_salience({1.0f}));
    EXPECT_LE(action[1], 1.0f);
    EXPECT_GE(action[1], 0.0f);
}

// ===========================================================================
// Eigen-vector overload
// ===========================================================================

TEST(CognitiveActionArbiterTest, EigenVectorOverload)
{
    CognitiveActionArbiter arb;
    Eigen::VectorXf sv(3);
    sv << 0.2f, 0.7f, 0.4f;

    auto action = arb.compute(sv, CognitiveMode::Reactive);

    // Index 1 (0.7) is the peak → should trigger movement
    EXPECT_TRUE(CognitiveActionArbiter::should_move(action));
    EXPECT_EQ(CognitiveActionArbiter::target_index(action), 1);
}

// ===========================================================================
// Utility function edge cases
// ===========================================================================

TEST(CognitiveActionArbiterTest, ShouldMoveReturnsFalseForShortVector)
{
    Eigen::VectorXf empty(0);
    EXPECT_FALSE(CognitiveActionArbiter::should_move(empty));
}

TEST(CognitiveActionArbiterTest, ShouldFocusReturnsFalseForShortVector)
{
    Eigen::VectorXf single(1);
    single[0] = 1.0f;
    EXPECT_FALSE(CognitiveActionArbiter::should_focus(single));
}

TEST(CognitiveActionArbiterTest, TargetIndexReturnsMinus1ForShortVector)
{
    Eigen::VectorXf two(2);
    two << 0.8f, 0.8f;
    EXPECT_EQ(CognitiveActionArbiter::target_index(two), -1);
}

// ===========================================================================
// Multi-actor scenario
// ===========================================================================

TEST(CognitiveActionArbiterTest, MultipleActorsSelectsMostSalient)
{
    CognitiveActionArbiter arb;
    // Five actors with varying salience
    auto action = arb.compute(make_salience({0.3f, 0.55f, 0.85f, 0.72f, 0.1f}));

    EXPECT_EQ(CognitiveActionArbiter::target_index(action), 2);
}

TEST(CognitiveActionArbiterTest, TieBreakSelectsFirstMaxElement)
{
    CognitiveActionArbiter arb;
    // Two actors with equal salience — std::max_element picks first
    auto action = arb.compute(make_salience({0.9f, 0.9f}));
    EXPECT_EQ(CognitiveActionArbiter::target_index(action), 0);
}

// ===========================================================================
// Deliberative mode produces moderate responses
// ===========================================================================

TEST(CognitiveActionArbiterTest, DeliberativeModeModerateResponse)
{
    CognitiveActionArbiter arb;
    const float salience = 0.8f;

    auto reactive     = arb.compute(make_salience({salience}), CognitiveMode::Reactive);
    auto deliberative = arb.compute(make_salience({salience}), CognitiveMode::Deliberative);
    auto reflective   = arb.compute(make_salience({salience}), CognitiveMode::Reflective);

    // Reactive >= Deliberative >= Reflective in movement urgency
    EXPECT_GE(reactive[0],     deliberative[0]);
    EXPECT_GE(deliberative[0], reflective[0]);
}

// ===========================================================================
// Focus threshold gating (config_.focus_threshold)
// ===========================================================================

TEST(CognitiveActionArbiterTest, FocusBelowThresholdSuppressedToZero)
{
    CognitiveActionArbiter arb;  // default focus_threshold = 0.4
    // Peak salience 0.4 → raw focus urgency 0.36 < 0.4 → suppressed to zero
    auto action = arb.compute(make_salience({0.4f}));
    EXPECT_FLOAT_EQ(action[1], 0.0f);
    EXPECT_FALSE(CognitiveActionArbiter::should_focus(action));
}

TEST(CognitiveActionArbiterTest, FocusAboveThresholdPassesThrough)
{
    CognitiveActionArbiter arb;
    // Peak salience 0.8 → raw focus urgency 0.72 >= 0.4 → passes through
    auto action = arb.compute(make_salience({0.8f}));
    EXPECT_NEAR(action[1], 0.72f, 1e-5f);
    EXPECT_TRUE(CognitiveActionArbiter::should_focus(action));
}

TEST(CognitiveActionArbiterTest, CustomFocusThresholdRespected)
{
    ArbiterConfig cfg;
    cfg.focus_threshold = 0.8f;
    CognitiveActionArbiter arb(cfg);

    // Raw focus urgency = 0.85 * 0.9 = 0.765 < 0.8 → suppressed by strict config
    auto strict_action = arb.compute(make_salience({0.85f}));
    EXPECT_FLOAT_EQ(strict_action[1], 0.0f);

    // The default config (0.4) lets the same salience through
    CognitiveActionArbiter default_arb;
    auto default_action = default_arb.compute(make_salience({0.85f}));
    EXPECT_GT(default_action[1], 0.0f);
}

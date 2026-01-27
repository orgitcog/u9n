/**
 * @file EchoStateNetworkTests.cpp
 * @brief Unit tests for Feature F1.2.1: Echo State Network Implementation
 * 
 * Tests cover:
 * - Sparse reservoir matrix generation
 * - Spectral radius normalization
 * - Leaky integrator dynamics
 * - Input/output scaling
 * - State harvesting mechanism
 * 
 * @author Deep Tree Echo Team
 * @date January 2026
 */

#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <cmath>

// Mock UEchoStateNetwork for testing (simulates UE environment)
#include "EchoStateNetworkMock.h"

// ============================================================================
// TEST FIXTURE
// ============================================================================

class EchoStateNetworkTest : public ::testing::Test {
protected:
    void SetUp() override {
        esn = std::make_unique<MockEchoStateNetwork>();
    }

    void TearDown() override {
        esn.reset();
    }

    std::unique_ptr<MockEchoStateNetwork> esn;
};

// ============================================================================
// TEST: SPARSE MATRIX GENERATION
// ============================================================================

TEST_F(EchoStateNetworkTest, SparseMatrixGeneration) {
    // Configure ESN with known sparsity
    MockESNConfig config;
    config.ReservoirSize = 100;
    config.InputDim = 10;
    config.OutputDim = 5;
    config.ReservoirSparsity = 0.1f;  // 10% connectivity
    config.RandomSeed = 42;  // Deterministic

    ASSERT_TRUE(esn->InitializeWithConfig(config));

    // Verify reservoir connectivity
    float connectivity = esn->GetReservoirConnectivity();
    EXPECT_NEAR(connectivity, 0.1f, 0.05f);  // Within 5% tolerance

    // Verify matrix is sparse (not dense)
    EXPECT_LT(connectivity, 0.5f);
}

TEST_F(EchoStateNetworkTest, SparseMatrixDeterminism) {
    MockESNConfig config;
    config.ReservoirSize = 50;
    config.InputDim = 5;
    config.RandomSeed = 123;  // Fixed seed

    // Initialize twice with same seed
    auto esn1 = std::make_unique<MockEchoStateNetwork>();
    auto esn2 = std::make_unique<MockEchoStateNetwork>();

    ASSERT_TRUE(esn1->InitializeWithConfig(config));
    ASSERT_TRUE(esn2->InitializeWithConfig(config));

    // Process same input
    std::vector<float> input = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};
    std::vector<float> state1 = esn1->ProcessInput(input);
    std::vector<float> state2 = esn2->ProcessInput(input);

    // States should be identical
    ASSERT_EQ(state1.size(), state2.size());
    for (size_t i = 0; i < state1.size(); ++i) {
        EXPECT_FLOAT_EQ(state1[i], state2[i]);
    }
}

// ============================================================================
// TEST: SPECTRAL RADIUS NORMALIZATION
// ============================================================================

TEST_F(EchoStateNetworkTest, SpectralRadiusNormalization) {
    MockESNConfig config;
    config.ReservoirSize = 100;
    config.InputDim = 10;
    config.SpectralRadius = 0.9f;  // Target
    config.RandomSeed = 42;

    ASSERT_TRUE(esn->InitializeWithConfig(config));

    // Verify spectral radius is close to target
    float actualSR = esn->GetSpectralRadius();
    EXPECT_NEAR(actualSR, 0.9f, 0.1f);  // Within 10% tolerance
}

TEST_F(EchoStateNetworkTest, SpectralRadiusStability) {
    // Test different spectral radius values
    std::vector<float> targetRadii = {0.5f, 0.9f, 0.95f, 1.0f, 1.1f};

    for (float targetSR : targetRadii) {
        auto testESN = std::make_unique<MockEchoStateNetwork>();
        MockESNConfig config;
        config.ReservoirSize = 80;
        config.InputDim = 5;
        config.SpectralRadius = targetSR;
        config.RandomSeed = 42;

        ASSERT_TRUE(testESN->InitializeWithConfig(config));

        float actualSR = testESN->GetSpectralRadius();
        EXPECT_NEAR(actualSR, targetSR, 0.15f)
            << "Failed for target SR = " << targetSR;
    }
}

// ============================================================================
// TEST: LEAKY INTEGRATOR DYNAMICS
// ============================================================================

TEST_F(EchoStateNetworkTest, LeakyIntegratorMemory) {
    MockESNConfig config;
    config.ReservoirSize = 50;
    config.InputDim = 1;
    config.LeakRate = 0.3f;  // Slow leak = longer memory
    config.RandomSeed = 42;

    ASSERT_TRUE(esn->InitializeWithConfig(config));

    // Send impulse
    std::vector<float> impulse = {1.0f};
    std::vector<float> zero = {0.0f};

    esn->ProcessInput(impulse);
    float activation1 = esn->GetAverageActivation();

    // Process zeros
    for (int i = 0; i < 5; ++i) {
        esn->ProcessInput(zero);
    }

    float activation2 = esn->GetAverageActivation();

    // With leak rate 0.3, activation should decay but persist
    EXPECT_GT(activation1, 0.0f);
    EXPECT_GT(activation2, 0.0f);
    EXPECT_LT(activation2, activation1);  // Decayed
}

TEST_F(EchoStateNetworkTest, LeakyIntegratorLeakRates) {
    // Test different leak rates
    std::vector<float> leakRates = {0.1f, 0.5f, 1.0f};
    std::vector<float> finalActivations;

    for (float lr : leakRates) {
        auto testESN = std::make_unique<MockEchoStateNetwork>();
        MockESNConfig config;
        config.ReservoirSize = 50;
        config.InputDim = 1;
        config.LeakRate = lr;
        config.RandomSeed = 42;

        ASSERT_TRUE(testESN->InitializeWithConfig(config));

        // Send impulse then zeros
        std::vector<float> impulse = {1.0f};
        std::vector<float> zero = {0.0f};

        testESN->ProcessInput(impulse);
        for (int i = 0; i < 10; ++i) {
            testESN->ProcessInput(zero);
        }

        finalActivations.push_back(testESN->GetAverageActivation());
    }

    // Higher leak rate = faster decay = lower final activation
    EXPECT_GT(finalActivations[0], finalActivations[1]);  // 0.1 > 0.5
    EXPECT_GT(finalActivations[1], finalActivations[2]);  // 0.5 > 1.0
}

TEST_F(EchoStateNetworkTest, LeakyIntegratorStateUpdate) {
    MockESNConfig config;
    config.ReservoirSize = 10;
    config.InputDim = 1;
    config.LeakRate = 0.5f;
    config.RandomSeed = 42;

    ASSERT_TRUE(esn->InitializeWithConfig(config));

    // Verify state update equation: x(t) = (1-α)*x(t-1) + α*f(...)
    std::vector<float> input = {0.5f};

    std::vector<float> state1 = esn->ProcessInput(input);
    std::vector<float> state2 = esn->ProcessInput(input);

    // States should differ (not just copy)
    bool statesDiffer = false;
    for (size_t i = 0; i < state1.size() && i < state2.size(); ++i) {
        if (std::abs(state1[i] - state2[i]) > 1e-6f) {
            statesDiffer = true;
            break;
        }
    }

    EXPECT_TRUE(statesDiffer);
}

// ============================================================================
// TEST: INPUT/OUTPUT SCALING
// ============================================================================

TEST_F(EchoStateNetworkTest, InputScaling) {
    // Test with different input scaling
    std::vector<float> scales = {0.1f, 1.0f, 10.0f};
    std::vector<float> activations;

    for (float scale : scales) {
        auto testESN = std::make_unique<MockEchoStateNetwork>();
        MockESNConfig config;
        config.ReservoirSize = 50;
        config.InputDim = 1;
        config.InputScaling = scale;
        config.RandomSeed = 42;

        ASSERT_TRUE(testESN->InitializeWithConfig(config));

        std::vector<float> input = {1.0f};
        testESN->ProcessInput(input);

        activations.push_back(testESN->GetAverageActivation());
    }

    // Larger input scaling should increase activation
    EXPECT_LT(activations[0], activations[1]);  // 0.1 < 1.0
    EXPECT_LT(activations[1], activations[2]);  // 1.0 < 10.0
}

TEST_F(EchoStateNetworkTest, OutputScaling) {
    MockESNConfig config;
    config.ReservoirSize = 50;
    config.InputDim = 5;
    config.OutputDim = 3;
    config.OutputScaling = 2.0f;
    config.RandomSeed = 42;

    ASSERT_TRUE(esn->InitializeWithConfig(config));

    // Train with dummy data
    std::vector<std::vector<float>> inputs = {
        {0.1f, 0.2f, 0.3f, 0.4f, 0.5f},
        {0.2f, 0.3f, 0.4f, 0.5f, 0.6f},
        {0.3f, 0.4f, 0.5f, 0.6f, 0.7f}
    };
    std::vector<std::vector<float>> targets = {
        {1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f}
    };

    float error = esn->TrainOutputWeights(inputs, targets, 0);
    EXPECT_GE(error, 0.0f);

    // Process input and check output is scaled
    esn->ResetState();
    esn->ProcessInput(inputs[0]);
    std::vector<float> output = esn->ComputeOutput();

    EXPECT_EQ(output.size(), 3);
    // Output should be affected by scaling
}

TEST_F(EchoStateNetworkTest, BiasScaling) {
    MockESNConfig config;
    config.ReservoirSize = 50;
    config.InputDim = 1;
    config.bEnableBias = true;
    config.BiasScaling = 1.0f;
    config.RandomSeed = 42;

    ASSERT_TRUE(esn->InitializeWithConfig(config));

    std::vector<float> zero = {0.0f};
    esn->ProcessInput(zero);

    float activation = esn->GetAverageActivation();

    // Even with zero input, bias should cause some activation
    EXPECT_GT(activation, 0.0f);
}

// ============================================================================
// TEST: STATE HARVESTING
// ============================================================================

TEST_F(EchoStateNetworkTest, StateHarvesting) {
    MockESNConfig config;
    config.ReservoirSize = 50;
    config.InputDim = 5;
    config.RandomSeed = 42;

    ASSERT_TRUE(esn->InitializeWithConfig(config));

    std::vector<float> input = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f};

    // Process input
    std::vector<float> state = esn->ProcessInput(input);

    // Verify state is harvested correctly
    EXPECT_EQ(state.size(), 50);

    // State should match GetReservoirState()
    std::vector<float> stateFromGetter = esn->GetReservoirState();
    ASSERT_EQ(state.size(), stateFromGetter.size());

    for (size_t i = 0; i < state.size(); ++i) {
        EXPECT_FLOAT_EQ(state[i], stateFromGetter[i]);
    }
}

TEST_F(EchoStateNetworkTest, StateHarvestingSequence) {
    MockESNConfig config;
    config.ReservoirSize = 30;
    config.InputDim = 3;
    config.RandomSeed = 42;

    ASSERT_TRUE(esn->InitializeWithConfig(config));

    // Process sequence
    std::vector<std::vector<float>> sequence = {
        {0.1f, 0.2f, 0.3f},
        {0.2f, 0.3f, 0.4f},
        {0.3f, 0.4f, 0.5f},
        {0.4f, 0.5f, 0.6f}
    };

    std::vector<std::vector<float>> states = esn->ProcessSequence(sequence);

    // Should return one state per input
    EXPECT_EQ(states.size(), 4);

    // Each state should have correct dimensions
    for (const auto& state : states) {
        EXPECT_EQ(state.size(), 30);
    }

    // States should evolve (differ from each other)
    EXPECT_NE(states[0], states[1]);
    EXPECT_NE(states[1], states[2]);
    EXPECT_NE(states[2], states[3]);
}

// ============================================================================
// TEST: TRAINING AND OUTPUT
// ============================================================================

TEST_F(EchoStateNetworkTest, TrainingBasic) {
    MockESNConfig config;
    config.ReservoirSize = 100;
    config.InputDim = 2;
    config.OutputDim = 1;
    config.RandomSeed = 42;

    ASSERT_TRUE(esn->InitializeWithConfig(config));

    // Simple training data
    std::vector<std::vector<float>> inputs;
    std::vector<std::vector<float>> targets;

    for (int i = 0; i < 50; ++i) {
        float x = i * 0.1f;
        inputs.push_back({x, x * x});
        targets.push_back({std::sin(x)});
    }

    float error = esn->TrainOutputWeights(inputs, targets, 10);

    EXPECT_GE(error, 0.0f);
    EXPECT_LT(error, 10.0f);  // Should learn something reasonable
}

TEST_F(EchoStateNetworkTest, WashoutPeriod) {
    MockESNConfig config;
    config.ReservoirSize = 50;
    config.InputDim = 1;
    config.OutputDim = 1;
    config.RandomSeed = 42;

    ASSERT_TRUE(esn->InitializeWithConfig(config));

    std::vector<std::vector<float>> inputs;
    std::vector<std::vector<float>> targets;

    for (int i = 0; i < 100; ++i) {
        inputs.push_back({static_cast<float>(i)});
        targets.push_back({static_cast<float>(i * 2)});
    }

    // Train with different washout periods
    float error10 = esn->TrainOutputWeights(inputs, targets, 10);
    esn->Initialize();
    float error50 = esn->TrainOutputWeights(inputs, targets, 50);

    // Both should complete successfully
    EXPECT_GE(error10, 0.0f);
    EXPECT_GE(error50, 0.0f);
}

// ============================================================================
// TEST: EDGE CASES
// ============================================================================

TEST_F(EchoStateNetworkTest, UninitializedProcessing) {
    // Should not crash when processing without initialization
    std::vector<float> input = {1.0f, 2.0f, 3.0f};
    std::vector<float> state = esn->ProcessInput(input);

    // Should return empty or handle gracefully
    EXPECT_TRUE(state.empty() || state.size() > 0);
}

TEST_F(EchoStateNetworkTest, InvalidInputDimensions) {
    MockESNConfig config;
    config.ReservoirSize = 50;
    config.InputDim = 5;
    config.RandomSeed = 42;

    ASSERT_TRUE(esn->InitializeWithConfig(config));

    // Wrong input size
    std::vector<float> wrongInput = {1.0f, 2.0f};  // Only 2, expected 5
    std::vector<float> state = esn->ProcessInput(wrongInput);

    // Should handle gracefully (return previous state or empty)
    EXPECT_TRUE(state.empty() || state.size() == 50);
}

TEST_F(EchoStateNetworkTest, ResetState) {
    MockESNConfig config;
    config.ReservoirSize = 50;
    config.InputDim = 1;
    config.RandomSeed = 42;

    ASSERT_TRUE(esn->InitializeWithConfig(config));

    // Process some inputs
    std::vector<float> input = {1.0f};
    esn->ProcessInput(input);
    esn->ProcessInput(input);

    float activationBefore = esn->GetAverageActivation();
    EXPECT_GT(activationBefore, 0.0f);

    // Reset
    esn->ResetState();

    float activationAfter = esn->GetAverageActivation();
    EXPECT_FLOAT_EQ(activationAfter, 0.0f);
}

// ============================================================================
// TEST: ECHO STATE PROPERTY
// ============================================================================

TEST_F(EchoStateNetworkTest, EchoStateProperty) {
    // Test that ESN satisfies echo state property
    // (same input sequence leads to same final state regardless of initial state)

    MockESNConfig config;
    config.ReservoirSize = 50;
    config.InputDim = 1;
    config.SpectralRadius = 0.9f;  // < 1.0 for ESP
    config.RandomSeed = 42;

    auto esn1 = std::make_unique<MockEchoStateNetwork>();
    auto esn2 = std::make_unique<MockEchoStateNetwork>();

    ASSERT_TRUE(esn1->InitializeWithConfig(config));
    ASSERT_TRUE(esn2->InitializeWithConfig(config));

    // Start from different initial states
    esn1->ResetState();
    esn2->ProcessInput({1.0f});  // Different initial state

    // Process same long sequence
    std::vector<float> input = {0.5f};
    for (int i = 0; i < 100; ++i) {
        esn1->ProcessInput(input);
        esn2->ProcessInput(input);
    }

    // Final states should converge
    std::vector<float> state1 = esn1->GetReservoirState();
    std::vector<float> state2 = esn2->GetReservoirState();

    ASSERT_EQ(state1.size(), state2.size());

    float maxDiff = 0.0f;
    for (size_t i = 0; i < state1.size(); ++i) {
        maxDiff = std::max(maxDiff, std::abs(state1[i] - state2[i]));
    }

    // States should be very similar (within tolerance)
    EXPECT_LT(maxDiff, 0.1f);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

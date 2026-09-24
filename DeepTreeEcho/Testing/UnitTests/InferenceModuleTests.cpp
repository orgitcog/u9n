/**
 * @file InferenceModuleTests.cpp
 * @brief Unit tests for the DeepTreeEcho/Inference module (Gap A of the
 *        32-repo integration): DTEReservoirCore (opencog-esn port),
 *        DTERidgeReadout (RidgeNode port), DTEMLPEncoder (coggml DTE-MLP),
 *        and LlamaCppBridge (elizaos-cpp reference engine).
 *
 * Since the unit-test target globs only Testing/UnitTests/*.cpp, module
 * implementations are included directly (translation-unit inclusion),
 * matching how other standalone modules are exercised.
 */

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "Inference/DTEReservoirCore.cpp"
#include "Inference/DTERidgeReadout.cpp"
#include "Inference/DTEMLPEncoder.h"

// ============================================================================
// DTEReservoirCore — leaky integrator ESN (opencog-esn ReservoirNode port)
// ============================================================================

TEST(DTEReservoirCoreTest, InitializesWithCorrectDimensions)
{
    dte::DTEReservoirCore res("test:reservoir");
    res.initialize(3, 50, 0.9, 0.3, 0.9, 1.0, 42);

    EXPECT_TRUE(res.is_initialized());
    EXPECT_EQ(res.get_input_dim(), 3u);
    EXPECT_EQ(res.get_reservoir_dim(), 50u);
    EXPECT_EQ(res.get_input_weights().rows(), 50);
    EXPECT_EQ(res.get_input_weights().cols(), 3);
    EXPECT_EQ(res.get_recurrent_weights().rows(), 50);
    EXPECT_EQ(res.get_recurrent_weights().cols(), 50);
    EXPECT_EQ(res.get_state().size(), 50);
    EXPECT_DOUBLE_EQ(res.get_state().norm(), 0.0);
}

TEST(DTEReservoirCoreTest, SpectralRadiusIsScaledToTarget)
{
    dte::DTEReservoirCore res;
    res.initialize(2, 80, 0.85, 0.3, 0.8, 1.0, 7);
    // Measured spectral radius should match target within numerical tolerance.
    EXPECT_NEAR(res.measured_spectral_radius(), 0.85, 1e-6);
}

TEST(DTEReservoirCoreTest, LeakyIntegratorUpdateMatchesEquation)
{
    dte::DTEReservoirCore res;
    res.initialize(2, 10, 0.9, 0.3, 0.5, 1.0, 42);

    Eigen::VectorXd x(2);
    x << 0.5, -0.2;

    // Manual computation of the update from a zero state:
    // s[1] = (1-a)*0 + a*tanh(W_in*x + W*0) = a*tanh(W_in*x)
    Eigen::VectorXd expected =
        0.3 * (res.get_input_weights() * x).array().tanh().matrix();

    Eigen::VectorXd s1 = res.update(x);
    EXPECT_NEAR((s1 - expected).norm(), 0.0, 1e-12);
}

TEST(DTEReservoirCoreTest, StateIsBoundedByTanh)
{
    dte::DTEReservoirCore res;
    res.initialize(1, 30, 0.95, 0.5, 0.7, 2.0, 99);

    Eigen::VectorXd x(1);
    for (int t = 0; t < 200; ++t) {
        x << std::sin(0.1 * t) * 10.0; // large inputs
        Eigen::VectorXd s = res.update(x);
        EXPECT_LE(s.cwiseAbs().maxCoeff(), 1.0 + 1e-12);
    }
}

TEST(DTEReservoirCoreTest, EchoStateFadingMemory)
{
    // Two reservoirs with identical weights but different initial inputs
    // should converge when driven by the same subsequent sequence
    // (echo state property under sub-unit spectral radius).
    dte::DTEReservoirCore a, b;
    a.initialize(1, 40, 0.8, 0.3, 0.8, 1.0, 5);
    b.initialize(1, 40, 0.8, 0.3, 0.8, 1.0, 5);

    Eigen::VectorXd xa(1), xb(1), common(1);
    xa << 1.0;
    xb << -1.0;
    a.update(xa);
    b.update(xb);

    for (int t = 0; t < 500; ++t) {
        common << std::sin(0.05 * t);
        a.update(common);
        b.update(common);
    }
    EXPECT_LT((a.get_state() - b.get_state()).norm(), 1e-6);
}

TEST(DTEReservoirCoreTest, ResetAndSerializationRoundTrip)
{
    dte::DTEReservoirCore res;
    res.initialize(2, 20, 0.9, 0.3, 0.9, 1.0, 42);
    std::vector<double> x{0.3, 0.7};
    res.update(x);
    res.update(x);

    auto snap = res.serialize();
    EXPECT_EQ(snap.reservoir_dim, 20u);
    EXPECT_EQ(snap.state.size(), 20u);

    dte::DTEReservoirCore restored;
    restored.deserialize(snap);
    EXPECT_TRUE(restored.is_initialized());
    EXPECT_NEAR((restored.get_state() - res.get_state()).norm(), 0.0, 1e-15);

    // Same next-step behavior after restore.
    auto s1 = res.update(x);
    auto s2 = restored.update(x);
    for (std::size_t i = 0; i < s1.size(); ++i)
        EXPECT_NEAR(s1[i], s2[i], 1e-15);

    res.reset_state();
    EXPECT_DOUBLE_EQ(res.get_state().norm(), 0.0);
}

TEST(DTEReservoirCoreTest, ThrowsOnDimensionMismatchAndUninitialized)
{
    dte::DTEReservoirCore res;
    Eigen::VectorXd x(3);
    x.setZero();
    EXPECT_THROW(res.update(x), std::logic_error);

    res.initialize(2, 10);
    EXPECT_THROW(res.update(x), std::invalid_argument);
}

// ============================================================================
// DTERidgeReadout — Cholesky ridge regression (opencog-esn RidgeNode port)
// ============================================================================

TEST(DTERidgeReadoutTest, LearnsLinearMapExactly)
{
    // Target: y = M*s for a fixed matrix M. Ridge with tiny lambda should
    // recover M nearly exactly given enough samples.
    Eigen::MatrixXd M(2, 4);
    M << 1.0, -0.5, 0.25, 2.0,
         0.0,  1.5, -1.0, 0.5;

    dte::DTERidgeReadout ridge;
    ridge.initialize(4, 2, 1e-8);

    std::mt19937 gen(123);
    std::uniform_real_distribution<> uni(-1.0, 1.0);
    for (int n = 0; n < 200; ++n) {
        Eigen::VectorXd s(4);
        for (int i = 0; i < 4; ++i) s(i) = uni(gen);
        ridge.collect_sample(s, M * s);
    }
    EXPECT_EQ(ridge.num_samples(), 200u);
    ridge.train();
    EXPECT_TRUE(ridge.is_trained());

    Eigen::VectorXd probe(4);
    probe << 0.1, -0.9, 0.4, 0.6;
    Eigen::VectorXd y = ridge.predict(probe);
    EXPECT_NEAR((y - M * probe).norm(), 0.0, 1e-5);
}

TEST(DTERidgeReadoutTest, RegularizationShrinksWeights)
{
    Eigen::MatrixXd S = Eigen::MatrixXd::Random(3, 50);
    Eigen::MatrixXd Y = Eigen::MatrixXd::Random(1, 50);

    dte::DTERidgeReadout weak, strong;
    weak.initialize(3, 1, 1e-8);
    strong.initialize(3, 1, 100.0);
    weak.train(S, Y);
    strong.train(S, Y);

    EXPECT_LT(strong.get_output_weights().norm(),
              weak.get_output_weights().norm());
}

TEST(DTERidgeReadoutTest, ThrowsWithoutTrainingOrSamples)
{
    dte::DTERidgeReadout ridge;
    ridge.initialize(3, 1);
    Eigen::VectorXd s(3);
    s.setZero();
    EXPECT_THROW(ridge.predict(s), std::logic_error);
    EXPECT_THROW(ridge.train(), std::logic_error);
}

TEST(DTERidgeReadoutTest, SerializationRoundTrip)
{
    dte::DTERidgeReadout ridge;
    ridge.initialize(3, 2, 1e-6);
    Eigen::MatrixXd S = Eigen::MatrixXd::Random(3, 40);
    Eigen::MatrixXd Y = Eigen::MatrixXd::Random(2, 40);
    ridge.train(S, Y);

    auto snap = ridge.serialize();
    dte::DTERidgeReadout restored;
    restored.deserialize(snap);
    EXPECT_TRUE(restored.is_trained());

    Eigen::VectorXd probe = Eigen::VectorXd::Random(3);
    EXPECT_NEAR((restored.predict(probe) - ridge.predict(probe)).norm(),
                0.0, 1e-15);
}

// ============================================================================
// Reservoir -> Ridge pipeline (ESN readout end-to-end)
// ============================================================================

TEST(InferencePipelineTest, ReservoirRidgeLearnsSineDelay)
{
    // Classic ESN task: reproduce a delayed sine from reservoir states.
    dte::DTEReservoirCore res;
    res.initialize(1, 100, 0.9, 0.3, 0.9, 0.5, 42);

    dte::DTERidgeReadout ridge;
    ridge.initialize(100, 1, 1e-6);

    const int washout = 100, train_n = 500, delay = 3;
    std::vector<double> signal;
    for (int t = 0; t < washout + train_n + 100; ++t)
        signal.push_back(std::sin(0.2 * t));

    Eigen::VectorXd x(1), y(1);
    for (int t = 0; t < washout + train_n; ++t) {
        x << signal[t];
        Eigen::VectorXd s = res.update(x);
        if (t >= washout && t >= delay) {
            y << signal[t - delay];
            ridge.collect_sample(s, y);
        }
    }
    ridge.train();

    // Evaluate on the following segment.
    double mse = 0.0;
    int n_eval = 0;
    for (int t = washout + train_n; t < washout + train_n + 100; ++t) {
        x << signal[t];
        Eigen::VectorXd s = res.update(x);
        double pred = ridge.predict(s)(0);
        double target = signal[t - delay];
        mse += (pred - target) * (pred - target);
        ++n_eval;
    }
    mse /= n_eval;
    EXPECT_LT(mse, 1e-3);
}

// ============================================================================
// DTEMLPEncoder — coggml DTE-MLP identity encoder (vendored)
// ============================================================================

TEST(DTEMLPEncoderTest, IdentityVectorRoundTrip)
{
    dte::IdentityVector id(32, 64);
    EXPECT_EQ(id.identity_dim, 29u + 32u + 64u);

    id.agent = {0.9f, 0.5f, 0.3f, 0.7f, 0.6f};
    id.level = dte::AutonomyLevel::EMBODIED;
    auto flat = id.to_vector();
    EXPECT_EQ(flat.size(), id.identity_dim);

    dte::IdentityVector id2(32, 64);
    id2.from_vector(flat.data(), flat.size());
    EXPECT_FLOAT_EQ(id2.agent[0], 0.9f);
    EXPECT_FLOAT_EQ(id2.agent[3], 0.7f);
}

TEST(DTEMLPEncoderTest, ReservoirStateBridge)
{
    dte::DTEReservoirCore res;
    res.initialize(2, 64, 0.9, 0.3, 0.9, 1.0, 42);
    std::vector<double> x{0.5, -0.5};
    res.update(x);

    dte::IdentityVector id(32, 64);
    std::vector<double> state(res.get_state().data(),
                              res.get_state().data() + res.get_state().size());
    dte::encode_reservoir_state(id, state);

    auto decoded = dte::decode_reservoir_state(id);
    ASSERT_EQ(decoded.size(), 64u);
    for (std::size_t i = 0; i < 64; ++i)
        EXPECT_NEAR(decoded[i], state[i], 1e-6);
}

TEST(DTEMLPEncoderTest, RecoveryCoreAutoencoderLearnsIdentity)
{
    dte::IdentityVector id(32, 64);
    for (std::size_t i = 0; i < 5; ++i) {
        id.agent[i] = 0.1f * static_cast<float>(i + 1);
        id.arena[i] = 0.05f * static_cast<float>(i + 1);
    }
    auto flat = id.to_vector();

    dte::mlp::RecoveryCoreMLP mlp(id.identity_dim, 32, 64);
    float initial_mse = mlp.reconstruction_mse(flat.data());
    mlp.train(flat.data(), 1, 300, 0.005f);
    float trained_mse = mlp.reconstruction_mse(flat.data());

    EXPECT_TRUE(mlp.trained());
    EXPECT_LT(trained_mse, initial_mse);
    EXPECT_LT(trained_mse, 0.01f);
}

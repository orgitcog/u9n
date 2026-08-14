#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// EchoReservoirNode — NanEcho ESN Reservoir for Unreal Engine
// Ported from echoself/NanEcho/dte_nodes/echo_reservoir.py
// Maps to: Arena in AAR (Agent-Arena-Relation) architecture
// cogpy layer: L1 coglow
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include "DeepTreeEcho/Core/EigenToUEConverter.h"
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <random>
#include <cmath>

/**
 * Configuration for the Echo State Network reservoir.
 * Spectral radius controls the echo state property (must be < 1 for stability).
 * Sparsity controls memory usage and computational cost.
 */
struct FEchoReservoirConfig
{
    /** Dimensionality of input signals */
    int32 InputDim = 32;

    /** Number of reservoir neurons */
    int32 ReservoirSize = 512;

    /** Spectral radius of the recurrent weight matrix (< 1.0 for ESP) */
    float SpectralRadius = 0.95f;

    /** Fraction of non-zero connections in reservoir */
    float Sparsity = 0.1f;

    /** Leaking rate for leaky integrator neurons */
    float LeakingRate = 0.3f;

    /** Input scaling factor */
    float InputScaling = 1.0f;

    /** Bias scaling factor */
    float BiasScaling = 0.1f;

    /** Random seed for reproducibility */
    int32 Seed = 42;
};

/**
 * EchoReservoirNode — The Arena in the AAR cognitive architecture.
 *
 * Implements a sparse Echo State Network with leaky integrator neurons.
 * The reservoir transforms temporal input sequences into rich, high-dimensional
 * state representations that capture temporal dependencies through the echo
 * state property.
 *
 * Key properties:
 * - Sparse recurrent connections (CSR format) for memory efficiency
 * - Spectral radius normalization ensures the echo state property
 * - Leaky integration provides controllable memory timescale
 * - State vector serves as the "arena" — the manifold on which cognition acts
 */
class FEchoReservoirNode
{
public:
    FEchoReservoirNode() = default;

    /** Initialize reservoir with given configuration */
    void Initialize(const FEchoReservoirConfig& Config)
    {
        Cfg = Config;
        std::mt19937 Rng(Cfg.Seed);
        std::uniform_real_distribution<float> Dist(-1.0f, 1.0f);
        std::uniform_real_distribution<float> SparseDist(0.0f, 1.0f);

        // Initialize input weight matrix W_in: ReservoirSize x InputDim
        W_in = Eigen::MatrixXf::Zero(Cfg.ReservoirSize, Cfg.InputDim);
        for (int i = 0; i < Cfg.ReservoirSize; ++i)
            for (int j = 0; j < Cfg.InputDim; ++j)
                W_in(i, j) = Dist(Rng) * Cfg.InputScaling;

        // Initialize sparse reservoir weight matrix W_res
        typedef Eigen::Triplet<float> T;
        std::vector<T> Triplets;
        for (int i = 0; i < Cfg.ReservoirSize; ++i)
        {
            for (int j = 0; j < Cfg.ReservoirSize; ++j)
            {
                if (SparseDist(Rng) < Cfg.Sparsity)
                {
                    Triplets.push_back(T(i, j, Dist(Rng)));
                }
            }
        }
        W_res.resize(Cfg.ReservoirSize, Cfg.ReservoirSize);
        W_res.setFromTriplets(Triplets.begin(), Triplets.end());

        // Normalize to spectral radius
        NormalizeSpectralRadius();

        // Initialize bias vector
        Bias = Eigen::VectorXf::Zero(Cfg.ReservoirSize);
        for (int i = 0; i < Cfg.ReservoirSize; ++i)
            Bias(i) = Dist(Rng) * Cfg.BiasScaling;

        // Initialize state to zero
        State = Eigen::VectorXf::Zero(Cfg.ReservoirSize);
        bInitialized = true;
    }

    /**
     * Run one step of the reservoir.
     * Implements: state(t) = (1-α)·state(t-1) + α·tanh(W_in·input + W_res·state(t-1) + bias)
     * where α is the leaking rate.
     *
     * @param Input - Input vector of dimension InputDim
     * @return Current reservoir state vector of dimension ReservoirSize
     */
    Eigen::VectorXf Step(const Eigen::VectorXf& Input)
    {
        check(bInitialized);
        check(Input.size() == Cfg.InputDim);

        // Pre-activation: W_in * input + W_res * state + bias
        Eigen::VectorXf PreAct = W_in * Input + W_res * State + Bias;

        // Apply tanh activation
        Eigen::VectorXf NewState = PreAct.array().tanh().matrix();

        // Leaky integration
        State = (1.0f - Cfg.LeakingRate) * State + Cfg.LeakingRate * NewState;

        return State;
    }

    /** Reset reservoir state to zero */
    void Reset()
    {
        if (bInitialized)
            State = Eigen::VectorXf::Zero(Cfg.ReservoirSize);
    }

    /** Get current state vector (read-only) */
    const Eigen::VectorXf& GetState() const { return State; }

    /** Get reservoir size */
    int32 GetReservoirSize() const { return Cfg.ReservoirSize; }

    /** Check if initialized */
    bool IsInitialized() const { return bInitialized; }

    /** Export state for backup (L3 layer) */
    TArray<float> ExportState() const
    {
        TArray<float> Result;
        Result.SetNum(State.size());
        for (int i = 0; i < State.size(); ++i)
            Result[i] = State(i);
        return Result;
    }

    /** Import state from backup */
    void ImportState(const TArray<float>& InState)
    {
        check(InState.Num() == Cfg.ReservoirSize);
        for (int i = 0; i < Cfg.ReservoirSize; ++i)
            State(i) = InState[i];
    }

private:
    void NormalizeSpectralRadius()
    {
        // Power iteration to estimate largest eigenvalue
        Eigen::VectorXf v = Eigen::VectorXf::Random(Cfg.ReservoirSize);
        v.normalize();
        float Lambda = 0.0f;
        for (int iter = 0; iter < 100; ++iter)
        {
            Eigen::VectorXf Av = W_res * v;
            Lambda = Av.norm();
            if (Lambda > 1e-10f)
                v = Av / Lambda;
        }
        // Scale reservoir weights
        if (Lambda > 1e-10f)
            W_res *= (Cfg.SpectralRadius / Lambda);
    }

    FEchoReservoirConfig Cfg;
    Eigen::MatrixXf W_in;                    // Input weights
    Eigen::SparseMatrix<float> W_res;        // Sparse reservoir weights
    Eigen::VectorXf Bias;                    // Bias vector
    Eigen::VectorXf State;                   // Current reservoir state
    bool bInitialized = false;
};

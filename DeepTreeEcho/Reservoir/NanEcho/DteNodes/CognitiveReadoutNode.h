#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// CognitiveReadoutNode — NanEcho Readout Layer for Unreal Engine
// Ported from echoself/NanEcho/dte_nodes/cognitive_readout.py
// Maps to: Agent in AAR (Agent-Arena-Relation) architecture
// cogpy layer: L0 coggml (dense tensor operations)
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <Eigen/Dense>
#include <cmath>

/**
 * Configuration for the Cognitive Readout layer.
 */
struct FCognitiveReadoutConfig
{
    /** Dimensionality of reservoir state input */
    int32 InputDim = 512;

    /** Dimensionality of output (action preferences, emotional updates, style) */
    int32 OutputDim = 30;

    /** Ridge regression regularization parameter */
    float RidgeAlpha = 1e-4f;

    /** Enable online learning via RLS (Recursive Least Squares) */
    bool bOnlineLearning = true;

    /** RLS forgetting factor (1.0 = no forgetting) */
    float RLSForgettingFactor = 0.999f;
};

/**
 * CognitiveReadoutNode — The Agent in the AAR cognitive architecture.
 *
 * Implements a trainable linear readout from the reservoir state space
 * to the action/decision space. Supports both batch training via ridge
 * regression and online adaptation via Recursive Least Squares (RLS).
 *
 * The readout maps the high-dimensional reservoir state (Arena) into
 * actionable decisions — the "urge-to-act" that drives behavior.
 *
 * Output decomposition (30D):
 *   [0..15]  Action preferences (16D)
 *   [16..21] Emotional update (6D: joy, sadness, anger, fear, surprise, disgust)
 *   [22..29] Communication style modulation (8D)
 */
class FCognitiveReadoutNode
{
public:
    FCognitiveReadoutNode() = default;

    /** Initialize readout with given configuration */
    void Initialize(const FCognitiveReadoutConfig& Config)
    {
        Cfg = Config;

        // Initialize output weights to zero (untrained)
        W_out = Eigen::MatrixXf::Zero(Cfg.OutputDim, Cfg.InputDim);

        // Initialize RLS inverse correlation matrix
        if (Cfg.bOnlineLearning)
        {
            P = Eigen::MatrixXf::Identity(Cfg.InputDim, Cfg.InputDim) * 1000.0f;
        }

        bInitialized = true;
    }

    /**
     * Forward pass: compute output from reservoir state.
     * output = W_out * state
     *
     * @param ReservoirState - Current state from EchoReservoirNode
     * @return Output vector decomposed into actions, emotions, style
     */
    Eigen::VectorXf Forward(const Eigen::VectorXf& ReservoirState) const
    {
        check(bInitialized);
        check(ReservoirState.size() == Cfg.InputDim);
        return W_out * ReservoirState;
    }

    /**
     * Batch training via ridge regression.
     * W_out = Y * X^T * (X * X^T + α*I)^{-1}
     *
     * @param States - Matrix of collected reservoir states (InputDim x NumSamples)
     * @param Targets - Matrix of target outputs (OutputDim x NumSamples)
     */
    void TrainBatch(const Eigen::MatrixXf& States, const Eigen::MatrixXf& Targets)
    {
        check(bInitialized);
        check(States.rows() == Cfg.InputDim);
        check(Targets.rows() == Cfg.OutputDim);
        check(States.cols() == Targets.cols());

        Eigen::MatrixXf XXT = States * States.transpose();
        Eigen::MatrixXf Reg = Eigen::MatrixXf::Identity(Cfg.InputDim, Cfg.InputDim) * Cfg.RidgeAlpha;
        Eigen::MatrixXf YXT = Targets * States.transpose();

        // Solve: W_out = Y*X^T * (X*X^T + αI)^{-1}
        W_out = YXT * (XXT + Reg).inverse();
    }

    /**
     * Online learning step via Recursive Least Squares (RLS).
     * Updates weights from a single observation.
     *
     * @param State - Current reservoir state
     * @param Target - Desired output
     * @param Error - Prediction error (Target - Predicted)
     */
    void UpdateOnline(const Eigen::VectorXf& State, const Eigen::VectorXf& Target)
    {
        check(bInitialized && Cfg.bOnlineLearning);

        Eigen::VectorXf Predicted = Forward(State);
        Eigen::VectorXf Error = Target - Predicted;

        // RLS update
        Eigen::VectorXf Px = P * State;
        float Denom = Cfg.RLSForgettingFactor + State.dot(Px);
        Eigen::VectorXf K = Px / Denom;

        // Update weights: W_out += Error * K^T
        W_out += Error * K.transpose();

        // Update inverse correlation matrix
        P = (P - K * Px.transpose()) / Cfg.RLSForgettingFactor;
    }

    /** Get action preferences (first 16 dims of output) */
    static TArray<float> ExtractActionPreferences(const Eigen::VectorXf& Output)
    {
        TArray<float> Actions;
        Actions.SetNum(16);
        for (int i = 0; i < 16 && i < Output.size(); ++i)
            Actions[i] = Output(i);
        return Actions;
    }

    /** Get emotional update (dims 16-21 of output) */
    static TArray<float> ExtractEmotionalUpdate(const Eigen::VectorXf& Output)
    {
        TArray<float> Emotions;
        Emotions.SetNum(6);
        for (int i = 0; i < 6 && (i + 16) < Output.size(); ++i)
            Emotions[i] = Output(i + 16);
        return Emotions;
    }

    /** Get style modulation (dims 22-29 of output) */
    static TArray<float> ExtractStyleModulation(const Eigen::VectorXf& Output)
    {
        TArray<float> Style;
        Style.SetNum(8);
        for (int i = 0; i < 8 && (i + 22) < Output.size(); ++i)
            Style[i] = Output(i + 22);
        return Style;
    }

    /** Export weights for backup (L0/L3 layer) */
    TArray<float> ExportWeights() const
    {
        TArray<float> Result;
        Result.SetNum(W_out.rows() * W_out.cols());
        int Idx = 0;
        for (int r = 0; r < W_out.rows(); ++r)
            for (int c = 0; c < W_out.cols(); ++c)
                Result[Idx++] = W_out(r, c);
        return Result;
    }

    /** Import weights from backup */
    void ImportWeights(const TArray<float>& Weights)
    {
        check(Weights.Num() == W_out.rows() * W_out.cols());
        int Idx = 0;
        for (int r = 0; r < W_out.rows(); ++r)
            for (int c = 0; c < W_out.cols(); ++c)
                W_out(r, c) = Weights[Idx++];
    }

    bool IsInitialized() const { return bInitialized; }

private:
    FCognitiveReadoutConfig Cfg;
    Eigen::MatrixXf W_out;  // Output weight matrix
    Eigen::MatrixXf P;      // RLS inverse correlation matrix
    bool bInitialized = false;
};

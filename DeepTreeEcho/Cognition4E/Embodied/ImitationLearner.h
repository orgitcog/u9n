#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// ImitationLearner — Behavioral Cloning for DTE Avatar Control
//
// Implements Stage 2 of the DTE training strategy:
//   Record human gameplay → Train readout to predict human actions
//
// Following SIMA 2's approach, the agent learns from demonstrations:
//   1. RECORD: Capture (observation, action) pairs during human play
//   2. ENCODE: Process observations through the vision system + reservoir
//   3. TRAIN: Fit readout weights via Ridge Regression / RLS
//   4. EVALUATE: Measure prediction accuracy on held-out data
//
// The key insight from reservoir computing is that the reservoir
// (Echo State Network) provides a rich, high-dimensional temporal
// encoding of the visual stream WITHOUT needing backpropagation.
// Only the readout layer needs training — making this extremely
// efficient compared to end-to-end deep learning.
//
// Data format: JSONL files with timestamped observation-action pairs
// Compatible with the echoself NanEcho training pipeline
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <Eigen/Dense>
#include <vector>
#include <fstream>

/** A single demonstration frame */
struct FDemonstrationFrame
{
    /** Timestamp */
    double Timestamp = 0.0;

    /** Reservoir state at this frame */
    Eigen::VectorXf ReservoirState;

    /** Human action at this frame */
    Eigen::VectorXf Action;

    /** Reward signal (if available) */
    float Reward = 0.0f;

    /** Episode ID */
    int32 EpisodeId = 0;
};

/** Training statistics */
struct FTrainingStats
{
    float TrainLoss = 0.0f;
    float ValidLoss = 0.0f;
    float ActionAccuracy = 0.0f;
    int32 NumSamples = 0;
    int32 NumEpisodes = 0;
    float RegularizationStrength = 1e-4f;
};

/**
 * ImitationLearner — Trains DTE to mimic human gameplay.
 *
 * The learning pipeline:
 *
 * ┌─────────────────────────────────────────────────────────────┐
 * │                    Human Gameplay                           │
 * │  ┌──────────┐    ┌──────────┐    ┌──────────┐             │
 * │  │ Frame 1  │───▶│ Frame 2  │───▶│ Frame 3  │───▶ ...     │
 * │  │ (obs,act)│    │ (obs,act)│    │ (obs,act)│             │
 * │  └──────────┘    └──────────┘    └──────────┘             │
 * └────────────────────────┬────────────────────────────────────┘
 *                          ▼
 * ┌─────────────────────────────────────────────────────────────┐
 * │              Vision System + Reservoir                      │
 * │  obs → VisionSystem.ProcessFrame() → Reservoir.Step()      │
 * │  → reservoir_state (high-dimensional temporal encoding)     │
 * └────────────────────────┬────────────────────────────────────┘
 *                          ▼
 * ┌─────────────────────────────────────────────────────────────┐
 * │              Ridge Regression Training                      │
 * │  W = (X^T X + λI)^{-1} X^T Y                              │
 * │  X = [reservoir_states], Y = [human_actions]               │
 * │  → Readout weights that predict human actions               │
 * └─────────────────────────────────────────────────────────────┘
 *
 * Why this works:
 * - The ESN reservoir creates a rich nonlinear temporal expansion
 *   of the visual input sequence (like a kernel trick in time)
 * - Ridge regression finds the linear mapping from this expanded
 *   space to actions — no backprop needed
 * - Training is FAST: O(N * D^2) where N=samples, D=reservoir_dim
 * - The reservoir's echo state property ensures temporal context
 *   is naturally captured without explicit frame stacking
 */
class FImitationLearner
{
public:
    FImitationLearner() = default;

    /**
     * Initialize the learner.
     */
    void Initialize(int32 ReservoirDim, int32 ActionDim, float Lambda = 1e-4f)
    {
        ResDim = ReservoirDim;
        ActDim = ActionDim;
        RegLambda = Lambda;

        // Initialize readout weights to zero
        ReadoutWeights = Eigen::MatrixXf::Zero(ActDim, ResDim);

        bInitialized = true;
    }

    /**
     * Add a demonstration frame to the training buffer.
     */
    void AddDemonstration(const Eigen::VectorXf& ReservoirState,
                          const Eigen::VectorXf& Action,
                          float Reward = 0.0f,
                          int32 EpisodeId = 0)
    {
        FDemonstrationFrame Frame;
        Frame.Timestamp = FPlatformTime::Seconds();
        Frame.ReservoirState = ReservoirState;
        Frame.Action = Action;
        Frame.Reward = Reward;
        Frame.EpisodeId = EpisodeId;
        DemoBuffer.push_back(Frame);
    }

    /**
     * Train the readout weights using Ridge Regression.
     *
     * W = (X^T X + λI)^{-1} X^T Y
     *
     * @param ValidationSplit - Fraction of data for validation (0.0 - 0.5)
     * @return Training statistics
     */
    FTrainingStats Train(float ValidationSplit = 0.1f)
    {
        check(bInitialized);
        FTrainingStats Stats;
        Stats.NumSamples = DemoBuffer.size();

        if (DemoBuffer.size() < 10)
        {
            UE_LOG(LogTemp, Warning, TEXT("ImitationLearner: Not enough data (%d samples)"),
                   (int32)DemoBuffer.size());
            return Stats;
        }

        // Split into train/validation
        int32 ValSize = FMath::Max(1, (int32)(DemoBuffer.size() * ValidationSplit));
        int32 TrainSize = DemoBuffer.size() - ValSize;

        // Build matrices
        Eigen::MatrixXf X(TrainSize, ResDim);
        Eigen::MatrixXf Y(TrainSize, ActDim);

        for (int32 i = 0; i < TrainSize; ++i)
        {
            X.row(i) = DemoBuffer[i].ReservoirState.transpose();
            Y.row(i) = DemoBuffer[i].Action.transpose();
        }

        // Ridge Regression: W = (X^T X + λI)^{-1} X^T Y
        Eigen::MatrixXf XtX = X.transpose() * X;
        XtX += RegLambda * Eigen::MatrixXf::Identity(ResDim, ResDim);
        Eigen::MatrixXf XtY = X.transpose() * Y;

        // Solve via Cholesky decomposition (fast, numerically stable)
        Eigen::LLT<Eigen::MatrixXf> Solver(XtX);
        ReadoutWeights = Solver.solve(XtY).transpose();

        // Compute training loss
        Eigen::MatrixXf Predictions = X * ReadoutWeights.transpose();
        Eigen::MatrixXf Errors = Predictions - Y;
        Stats.TrainLoss = Errors.squaredNorm() / TrainSize;

        // Compute validation loss
        if (ValSize > 0)
        {
            Eigen::MatrixXf XVal(ValSize, ResDim);
            Eigen::MatrixXf YVal(ValSize, ActDim);
            for (int32 i = 0; i < ValSize; ++i)
            {
                XVal.row(i) = DemoBuffer[TrainSize + i].ReservoirState.transpose();
                YVal.row(i) = DemoBuffer[TrainSize + i].Action.transpose();
            }
            Eigen::MatrixXf ValPred = XVal * ReadoutWeights.transpose();
            Eigen::MatrixXf ValErr = ValPred - YVal;
            Stats.ValidLoss = ValErr.squaredNorm() / ValSize;

            // Discrete action accuracy
            int32 Correct = 0;
            for (int32 i = 0; i < ValSize; ++i)
            {
                bool Match = true;
                for (int32 j = 4; j < ActDim; ++j) // Skip continuous axes
                {
                    bool PredAction = ValPred(i, j) > 0.5f;
                    bool TrueAction = YVal(i, j) > 0.5f;
                    if (PredAction != TrueAction) { Match = false; break; }
                }
                if (Match) Correct++;
            }
            Stats.ActionAccuracy = (float)Correct / ValSize;
        }

        Stats.RegularizationStrength = RegLambda;

        // Count unique episodes
        TSet<int32> Episodes;
        for (const auto& F : DemoBuffer) Episodes.Add(F.EpisodeId);
        Stats.NumEpisodes = Episodes.Num();

        UE_LOG(LogTemp, Log, TEXT("ImitationLearner: Trained on %d samples. "
            "TrainLoss=%.4f, ValidLoss=%.4f, Accuracy=%.2f%%"),
            TrainSize, Stats.TrainLoss, Stats.ValidLoss, Stats.ActionAccuracy * 100.0f);

        return Stats;
    }

    /**
     * Predict action from reservoir state.
     */
    Eigen::VectorXf Predict(const Eigen::VectorXf& ReservoirState) const
    {
        return ReadoutWeights * ReservoirState;
    }

    /**
     * Online update (Recursive Least Squares).
     * Updates weights incrementally without retraining from scratch.
     */
    void OnlineUpdate(const Eigen::VectorXf& ReservoirState,
                      const Eigen::VectorXf& TargetAction,
                      float LearningRate = 0.01f)
    {
        // Simple gradient descent update
        Eigen::VectorXf Prediction = ReadoutWeights * ReservoirState;
        Eigen::VectorXf Error = TargetAction - Prediction;

        // W += lr * error * state^T
        ReadoutWeights += LearningRate * Error * ReservoirState.transpose();
    }

    /**
     * Save demonstrations to JSONL file (compatible with echoself pipeline).
     */
    void SaveDemonstrations(const FString& FilePath) const
    {
        std::ofstream File(TCHAR_TO_UTF8(*FilePath));
        if (!File.is_open()) return;

        for (const auto& Frame : DemoBuffer)
        {
            File << "{\"timestamp\":" << Frame.Timestamp
                 << ",\"episode\":" << Frame.EpisodeId
                 << ",\"reward\":" << Frame.Reward
                 << ",\"reservoir_state\":[";
            for (int32 i = 0; i < Frame.ReservoirState.size(); ++i)
            {
                if (i > 0) File << ",";
                File << Frame.ReservoirState(i);
            }
            File << "],\"action\":[";
            for (int32 i = 0; i < Frame.Action.size(); ++i)
            {
                if (i > 0) File << ",";
                File << Frame.Action(i);
            }
            File << "]}\n";
        }
    }

    /**
     * Get the trained readout weights (for transfer to CognitiveReadoutNode).
     */
    const Eigen::MatrixXf& GetReadoutWeights() const { return ReadoutWeights; }

    /** Clear the demonstration buffer */
    void ClearBuffer() { DemoBuffer.clear(); }

    int32 GetBufferSize() const { return DemoBuffer.size(); }
    bool IsInitialized() const { return bInitialized; }

private:
    int32 ResDim = 0;
    int32 ActDim = 0;
    float RegLambda = 1e-4f;

    Eigen::MatrixXf ReadoutWeights;
    std::vector<FDemonstrationFrame> DemoBuffer;

    bool bInitialized = false;
};

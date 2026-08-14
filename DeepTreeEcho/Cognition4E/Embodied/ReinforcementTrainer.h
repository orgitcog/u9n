#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// ReinforcementTrainer — DTE's Self-Improvement Engine
//
// Implements Stage 3-4 of the DTE training strategy:
//   Stage 3: Goal-directed RL with extrinsic + intrinsic rewards
//   Stage 4: Open-ended self-improvement (autonomous task generation)
//
// Following SIMA 2's self-improvement paradigm, DTE can:
//   1. Generate its own tasks via the LLM (IntrospectionNode)
//   2. Attempt tasks using current policy
//   3. Evaluate success via the LLM (reward model)
//   4. Update readout weights based on reward signal
//
// The RL algorithm is REINFORCE with reservoir computing:
//   - The reservoir provides the state representation
//   - The readout provides the policy (action probabilities)
//   - Policy gradient updates the readout weights only
//   - No backprop through the reservoir (echo state property)
//
// Intrinsic motivation signals:
//   - Curiosity: prediction error of a forward model
//   - Competence: improvement rate on current task
//   - Novelty: distance from visited states in reservoir space
//   - Somatic: emotional valence from SomaticDecisionEngine
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <Eigen/Dense>
#include <vector>
#include <deque>
#include <random>

/** Reward component types */
enum class ERewardType : uint8
{
    EXTRINSIC,    // Game-provided reward (score, health, etc.)
    CURIOSITY,    // Prediction error of forward model
    COMPETENCE,   // Improvement rate on current task
    NOVELTY,      // Distance from visited states
    SOMATIC,      // Emotional valence from somatic markers
    SOCIAL        // Reward from social interaction (humor landing, etc.)
};

/** A single experience tuple */
struct FExperience
{
    Eigen::VectorXf State;       // Reservoir state
    Eigen::VectorXf Action;      // Action taken
    float Reward = 0.0f;         // Total reward
    Eigen::VectorXf NextState;   // Next reservoir state
    bool bDone = false;          // Episode terminated
    float LogProb = 0.0f;        // Log probability of action under policy
};

/** A self-generated task */
struct FSelfTask
{
    FString Description;         // Natural language description
    FString SuccessCriteria;     // How to evaluate success
    float Difficulty = 0.5f;     // Estimated difficulty (0-1)
    int32 MaxSteps = 1000;       // Maximum steps to attempt
    float BestReward = -1e9f;    // Best reward achieved
    int32 AttemptCount = 0;      // Number of attempts
};

/** Forward model for curiosity-driven exploration */
struct FForwardModel
{
    Eigen::MatrixXf Weights;
    int32 StateDim = 0;
    int32 ActionDim = 0;

    void Initialize(int32 InStateDim, int32 InActionDim)
    {
        StateDim = InStateDim;
        ActionDim = InActionDim;
        // Simple linear forward model: next_state = W * [state; action]
        Weights = Eigen::MatrixXf::Random(StateDim, StateDim + ActionDim) * 0.01f;
    }

    Eigen::VectorXf Predict(const Eigen::VectorXf& State, const Eigen::VectorXf& Action) const
    {
        Eigen::VectorXf Input(StateDim + ActionDim);
        Input.head(StateDim) = State;
        Input.tail(ActionDim) = Action;
        return Weights * Input;
    }

    float GetPredictionError(const Eigen::VectorXf& State, const Eigen::VectorXf& Action,
                              const Eigen::VectorXf& NextState) const
    {
        Eigen::VectorXf Predicted = Predict(State, Action);
        return (Predicted - NextState).squaredNorm();
    }

    void Update(const Eigen::VectorXf& State, const Eigen::VectorXf& Action,
                const Eigen::VectorXf& NextState, float LearningRate = 0.001f)
    {
        Eigen::VectorXf Input(StateDim + ActionDim);
        Input.head(StateDim) = State;
        Input.tail(ActionDim) = Action;
        Eigen::VectorXf Error = NextState - Weights * Input;
        Weights += LearningRate * Error * Input.transpose();
    }
};

/**
 * ReinforcementTrainer — DTE's self-improvement engine.
 *
 * Combines reservoir computing with policy gradient RL:
 *
 * ┌─────────────────────────────────────────────────────────────┐
 * │                    Environment (Game)                       │
 * │  ┌──────────┐    ┌──────────┐    ┌──────────┐             │
 * │  │ State s  │───▶│ Action a │───▶│ Reward r │             │
 * │  └────┬─────┘    └────┬─────┘    └────┬─────┘             │
 * └───────┼───────────────┼───────────────┼─────────────────────┘
 *         │               │               │
 *         ▼               ▼               ▼
 * ┌─────────────────────────────────────────────────────────────┐
 * │              Experience Buffer                              │
 * │  [(s₁,a₁,r₁), (s₂,a₂,r₂), ..., (sₙ,aₙ,rₙ)]            │
 * └────────────────────────┬────────────────────────────────────┘
 *                          ▼
 * ┌─────────────────────────────────────────────────────────────┐
 * │              Policy Gradient Update                         │
 * │  ∇J = Σ ∇log π(aₜ|sₜ) * Gₜ                               │
 * │  W_readout += α * ∇J                                       │
 * │  (Only readout weights updated — reservoir is fixed)        │
 * └─────────────────────────────────────────────────────────────┘
 *
 * Intrinsic motivation adds exploration bonuses:
 * ┌─────────────────────────────────────────────────────────────┐
 * │  r_total = r_extrinsic                                      │
 * │         + β₁ * r_curiosity   (forward model error)          │
 * │         + β₂ * r_competence  (improvement rate)             │
 * │         + β₃ * r_novelty     (state space coverage)         │
 * │         + β₄ * r_somatic     (emotional valence)            │
 * └─────────────────────────────────────────────────────────────┘
 */
class FReinforcementTrainer
{
public:
    FReinforcementTrainer() = default;

    /**
     * Initialize the trainer.
     */
    void Initialize(int32 StateDim, int32 ActionDim,
                    float LearningRate = 0.001f, float DiscountFactor = 0.99f)
    {
        StateD = StateDim;
        ActionD = ActionDim;
        Alpha = LearningRate;
        Gamma = DiscountFactor;

        // Initialize forward model for curiosity
        ForwardMdl.Initialize(StateDim, ActionDim);

        // Intrinsic reward weights
        CuriosityWeight = 0.1f;
        CompetenceWeight = 0.05f;
        NoveltyWeight = 0.05f;
        SomaticWeight = 0.2f;

        // Visited state buffer for novelty computation
        VisitedStates.clear();
        MaxVisitedStates = 10000;

        bInitialized = true;
    }

    /**
     * Store an experience tuple.
     */
    void StoreExperience(const Eigen::VectorXf& State, const Eigen::VectorXf& Action,
                         float ExtrinsicReward, const Eigen::VectorXf& NextState,
                         bool bDone, float SomaticValence = 0.0f)
    {
        FExperience Exp;
        Exp.State = State;
        Exp.Action = Action;
        Exp.NextState = NextState;
        Exp.bDone = bDone;

        // Compute intrinsic rewards
        float CuriosityReward = ForwardMdl.GetPredictionError(State, Action, NextState);
        float NoveltyReward = ComputeNovelty(NextState);
        float CompetenceReward = ComputeCompetence();

        // Total reward
        Exp.Reward = ExtrinsicReward
                   + CuriosityWeight * CuriosityReward
                   + NoveltyWeight * NoveltyReward
                   + CompetenceWeight * CompetenceReward
                   + SomaticWeight * SomaticValence;

        ExperienceBuffer.push_back(Exp);

        // Update forward model
        ForwardMdl.Update(State, Action, NextState);

        // Track visited states
        if (VisitedStates.size() < MaxVisitedStates)
            VisitedStates.push_back(NextState);

        // Track reward history for competence
        RewardHistory.push_back(Exp.Reward);
        if (RewardHistory.size() > 1000)
            RewardHistory.pop_front();
    }

    /**
     * Compute policy gradient and return weight update.
     *
     * Uses REINFORCE algorithm:
     *   ∇J = Σ ∇log π(aₜ|sₜ) * Gₜ
     *
     * Where Gₜ = Σ γ^k * r_{t+k} is the discounted return.
     *
     * @param PolicyWeights - Current readout weights (ActionDim x StateDim)
     * @return Weight update matrix to be added to readout weights
     */
    Eigen::MatrixXf ComputePolicyGradient(const Eigen::MatrixXf& PolicyWeights)
    {
        check(bInitialized);

        if (ExperienceBuffer.size() < 10)
            return Eigen::MatrixXf::Zero(ActionD, StateD);

        // Compute discounted returns
        std::vector<float> Returns(ExperienceBuffer.size(), 0.0f);
        float G = 0.0f;
        for (int32 t = ExperienceBuffer.size() - 1; t >= 0; --t)
        {
            if (ExperienceBuffer[t].bDone)
                G = 0.0f;
            G = ExperienceBuffer[t].Reward + Gamma * G;
            Returns[t] = G;
        }

        // Normalize returns (baseline subtraction)
        float MeanReturn = 0.0f;
        for (float R : Returns) MeanReturn += R;
        MeanReturn /= Returns.size();
        float StdReturn = 0.0f;
        for (float R : Returns) StdReturn += (R - MeanReturn) * (R - MeanReturn);
        StdReturn = FMath::Sqrt(StdReturn / Returns.size() + 1e-8f);

        // Policy gradient accumulator
        Eigen::MatrixXf Gradient = Eigen::MatrixXf::Zero(ActionD, StateD);

        for (size_t t = 0; t < ExperienceBuffer.size(); ++t)
        {
            float Advantage = (Returns[t] - MeanReturn) / StdReturn;

            // ∇log π(a|s) for linear policy: ∇W = (a - W*s) * s^T
            Eigen::VectorXf PolicyOutput = PolicyWeights * ExperienceBuffer[t].State;
            Eigen::VectorXf ActionError = ExperienceBuffer[t].Action - PolicyOutput;

            // Accumulate gradient
            Gradient += Advantage * ActionError * ExperienceBuffer[t].State.transpose();
        }

        Gradient /= ExperienceBuffer.size();

        // Clear buffer after update
        ExperienceBuffer.clear();

        return Alpha * Gradient;
    }

    /**
     * Generate a self-improvement task.
     * In production, this would use the LLM via IntrospectionNode.
     * Here we implement heuristic task generation.
     */
    FSelfTask GenerateSelfTask() const
    {
        static const TArray<FString> TaskTemplates = {
            TEXT("Navigate to the nearest visible landmark"),
            TEXT("Collect the closest item"),
            TEXT("Explore an area not visited in the last 100 steps"),
            TEXT("Interact with the nearest NPC or object"),
            TEXT("Survive for 60 seconds without taking damage"),
            TEXT("Reach the highest visible point"),
            TEXT("Follow the path to its end"),
            TEXT("Find and open a door or container")
        };

        FSelfTask Task;
        int32 Idx = FMath::RandRange(0, TaskTemplates.Num() - 1);
        Task.Description = TaskTemplates[Idx];
        Task.SuccessCriteria = TEXT("LLM evaluates from visual observation");
        Task.Difficulty = 0.3f + 0.5f * FMath::FRand();
        Task.MaxSteps = 500 + FMath::RandRange(0, 1000);
        return Task;
    }

    /** Get training statistics */
    float GetAverageReward() const
    {
        if (RewardHistory.empty()) return 0.0f;
        float Sum = 0.0f;
        for (float R : RewardHistory) Sum += R;
        return Sum / RewardHistory.size();
    }

    float GetRewardTrend() const
    {
        if (RewardHistory.size() < 100) return 0.0f;
        float Recent = 0.0f, Old = 0.0f;
        int32 Half = RewardHistory.size() / 2;
        int32 i = 0;
        for (float R : RewardHistory)
        {
            if (i < Half) Old += R;
            else Recent += R;
            i++;
        }
        Old /= Half;
        Recent /= (RewardHistory.size() - Half);
        return Recent - Old;
    }

    bool IsInitialized() const { return bInitialized; }

private:
    float ComputeNovelty(const Eigen::VectorXf& State) const
    {
        if (VisitedStates.empty()) return 1.0f;

        // Average distance to k-nearest visited states
        float MinDist = 1e9f;
        int32 K = FMath::Min(5, (int32)VisitedStates.size());
        std::vector<float> Distances;
        for (const auto& VS : VisitedStates)
            Distances.push_back((State - VS).squaredNorm());

        std::partial_sort(Distances.begin(), Distances.begin() + K, Distances.end());
        float AvgDist = 0.0f;
        for (int32 i = 0; i < K; ++i) AvgDist += Distances[i];
        return AvgDist / K;
    }

    float ComputeCompetence() const
    {
        // Competence = positive reward trend
        return FMath::Max(0.0f, GetRewardTrend());
    }

    int32 StateD = 0;
    int32 ActionD = 0;
    float Alpha = 0.001f;
    float Gamma = 0.99f;

    float CuriosityWeight = 0.1f;
    float CompetenceWeight = 0.05f;
    float NoveltyWeight = 0.05f;
    float SomaticWeight = 0.2f;

    FForwardModel ForwardMdl;
    std::vector<FExperience> ExperienceBuffer;
    std::vector<Eigen::VectorXf> VisitedStates;
    std::deque<float> RewardHistory;
    size_t MaxVisitedStates = 10000;

    bool bInitialized = false;
};

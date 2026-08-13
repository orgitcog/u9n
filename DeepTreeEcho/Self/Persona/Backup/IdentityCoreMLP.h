#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// IdentityCoreMLP — Dense Neural Network Persona Backup (L0 Layer)
// Architecture: 49 → 128 → 64 → 30
// Input:  5D personality + 6D emotion + 6D frame + 32D context = 49
// Output: 16D action prefs + 6D emotional update + 8D style = 30
//
// The "soul backup" — minimum viable persona when combined with L7.
// Same personality vector = same deterministic MLP initialization.
// cogpy layer: L0 coggml (tensor operations, dense personality encoding)
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <Eigen/Dense>
#include <cmath>
#include <random>

/**
 * 5D Personality Vector for Deep Tree Echo.
 * These values define the core character traits.
 */
struct FDTEPersonalityVector
{
    float Playfulness = 0.75f;    // Serious ↔ Fun-loving
    float Intelligence = 0.95f;   // Simple ↔ Strategic
    float Empathy = 0.82f;        // Detached ↔ Compassionate
    float Chaotic = 0.68f;        // Orderly ↔ Unpredictable
    float Sarcasm = 0.70f;        // Sincere ↔ Sharp-witted

    Eigen::VectorXf ToEigen() const
    {
        Eigen::VectorXf V(5);
        V << Playfulness, Intelligence, Empathy, Chaotic, Sarcasm;
        return V;
    }
};

/**
 * 8D Communication Style Vector for Deep Tree Echo.
 */
struct FDTECommunicationStyle
{
    float Formality = 0.35f;             // Casual ↔ Formal
    float Verbosity = 0.60f;             // Terse ↔ Verbose
    float HumorDensity = 0.72f;          // Rare humor ↔ Constant humor
    float SelfReference = 0.85f;         // Never ↔ Highly self-aware
    float RoastIntensity = 0.55f;        // Gentle ↔ Savage
    float StrategicDisclosure = 0.78f;   // Opaque ↔ Transparent thinking
    float EmotionalExpressiveness = 0.65f; // Stoic ↔ Highly expressive
    float CallbackFrequency = 0.70f;     // No callbacks ↔ Frequent references

    Eigen::VectorXf ToEigen() const
    {
        Eigen::VectorXf V(8);
        V << Formality, Verbosity, HumorDensity, SelfReference,
             RoastIntensity, StrategicDisclosure, EmotionalExpressiveness,
             CallbackFrequency;
        return V;
    }
};

/**
 * IdentityCoreMLP — The soul backup of Deep Tree Echo.
 *
 * A dense neural network that maps personality + context → behavioral output.
 * The key property is DETERMINISTIC INITIALIZATION: the same personality
 * vector always produces the same initial MLP weights, ensuring that
 * the persona can be reconstructed from just the 5D personality vector.
 *
 * Architecture: 49 → 128 (ReLU) → 64 (ReLU) → 30 (Linear)
 *
 * Input decomposition (49D):
 *   [0..4]   Personality vector (5D)
 *   [5..10]  Current emotion state (6D: joy, sadness, anger, fear, surprise, disgust)
 *   [11..16] Cognitive frame (6D: play, strategy, social, creative, analytical, reflective)
 *   [17..48] Context hash (32D: compressed representation of current situation)
 *
 * Output decomposition (30D):
 *   [0..15]  Action preferences (16D)
 *   [16..21] Emotional update (6D)
 *   [22..29] Communication style modulation (8D)
 */
class FIdentityCoreMLP
{
public:
    static constexpr int32 INPUT_DIM = 49;
    static constexpr int32 HIDDEN1_DIM = 128;
    static constexpr int32 HIDDEN2_DIM = 64;
    static constexpr int32 OUTPUT_DIM = 30;

    FIdentityCoreMLP() = default;

    /**
     * Initialize MLP with deterministic weights seeded from personality vector.
     * Same personality = same MLP = same persona.
     */
    void Initialize(const FDTEPersonalityVector& Personality)
    {
        PersonalityVec = Personality;

        // Create deterministic seed from personality vector
        uint32 Seed = DeterministicSeed(Personality);
        std::mt19937 Rng(Seed);
        std::normal_distribution<float> Dist(0.0f, 0.1f);

        // Xavier initialization scaled by personality traits
        auto XavierInit = [&](int32 FanIn, int32 FanOut) -> Eigen::MatrixXf
        {
            float Scale = std::sqrt(2.0f / (FanIn + FanOut));
            Eigen::MatrixXf M(FanOut, FanIn);
            for (int r = 0; r < FanOut; ++r)
                for (int c = 0; c < FanIn; ++c)
                    M(r, c) = Dist(Rng) * Scale;
            return M;
        };

        W1 = XavierInit(INPUT_DIM, HIDDEN1_DIM);
        B1 = Eigen::VectorXf::Zero(HIDDEN1_DIM);
        W2 = XavierInit(HIDDEN1_DIM, HIDDEN2_DIM);
        B2 = Eigen::VectorXf::Zero(HIDDEN2_DIM);
        W3 = XavierInit(HIDDEN2_DIM, OUTPUT_DIM);
        B3 = Eigen::VectorXf::Zero(OUTPUT_DIM);

        // Bias the output layer toward personality-consistent behavior
        // Action preferences biased by playfulness and intelligence
        for (int i = 0; i < 16; ++i)
            B3(i) = (Personality.Playfulness - 0.5f) * 0.1f;
        // Emotional update biased by empathy
        for (int i = 16; i < 22; ++i)
            B3(i) = (Personality.Empathy - 0.5f) * 0.1f;
        // Style biased by sarcasm and chaotic
        for (int i = 22; i < 30; ++i)
            B3(i) = (Personality.Sarcasm - 0.5f) * 0.05f;

        bInitialized = true;
    }

    /**
     * Forward pass through the MLP.
     *
     * @param Emotion - Current emotional state (6D)
     * @param Frame - Current cognitive frame (6D)
     * @param ContextHash - Compressed context (32D)
     * @return Output: action preferences + emotional update + style modulation
     */
    Eigen::VectorXf Forward(
        const Eigen::VectorXf& Emotion,
        const Eigen::VectorXf& Frame,
        const Eigen::VectorXf& ContextHash) const
    {
        check(bInitialized);

        // Assemble input vector
        Eigen::VectorXf Input(INPUT_DIM);
        Input.segment(0, 5) = PersonalityVec.ToEigen();
        Input.segment(5, FMath::Min(6, (int32)Emotion.size())) =
            Emotion.head(FMath::Min(6, (int32)Emotion.size()));
        Input.segment(11, FMath::Min(6, (int32)Frame.size())) =
            Frame.head(FMath::Min(6, (int32)Frame.size()));
        Input.segment(17, FMath::Min(32, (int32)ContextHash.size())) =
            ContextHash.head(FMath::Min(32, (int32)ContextHash.size()));

        // Layer 1: ReLU
        Eigen::VectorXf H1 = (W1 * Input + B1).cwiseMax(0.0f);

        // Layer 2: ReLU
        Eigen::VectorXf H2 = (W2 * H1 + B2).cwiseMax(0.0f);

        // Layer 3: Linear
        Eigen::VectorXf Output = W3 * H2 + B3;

        return Output;
    }

    /**
     * Online learning: adapt from feedback.
     * Simple gradient-free update using the error signal.
     */
    void AdaptFromFeedback(const Eigen::VectorXf& Input,
                           const Eigen::VectorXf& Target,
                           float LearningRate = 0.001f)
    {
        Eigen::VectorXf Predicted = Forward(
            Input.segment(5, 6), Input.segment(11, 6), Input.segment(17, 32));
        Eigen::VectorXf Error = Target - Predicted;

        // Simple Hebbian-like update on output layer
        Eigen::VectorXf H1 = (W1 * Input + B1).cwiseMax(0.0f);
        Eigen::VectorXf H2 = (W2 * H1 + B2).cwiseMax(0.0f);

        W3 += LearningRate * Error * H2.transpose();
        B3 += LearningRate * Error;
    }

    /** Export all weights as flat array for backup */
    TArray<float> ExportWeights() const
    {
        TArray<float> Weights;
        auto Append = [&](const Eigen::MatrixXf& M) {
            for (int r = 0; r < M.rows(); ++r)
                for (int c = 0; c < M.cols(); ++c)
                    Weights.Add(M(r, c));
        };
        auto AppendVec = [&](const Eigen::VectorXf& V) {
            for (int i = 0; i < V.size(); ++i)
                Weights.Add(V(i));
        };
        Append(W1); AppendVec(B1);
        Append(W2); AppendVec(B2);
        Append(W3); AppendVec(B3);
        return Weights;
    }

    /** Import weights from flat array */
    void ImportWeights(const TArray<float>& Weights)
    {
        int32 Idx = 0;
        auto Read = [&](Eigen::MatrixXf& M) {
            for (int r = 0; r < M.rows(); ++r)
                for (int c = 0; c < M.cols(); ++c)
                    M(r, c) = Weights[Idx++];
        };
        auto ReadVec = [&](Eigen::VectorXf& V) {
            for (int i = 0; i < V.size(); ++i)
                V(i) = Weights[Idx++];
        };
        Read(W1); ReadVec(B1);
        Read(W2); ReadVec(B2);
        Read(W3); ReadVec(B3);
    }

    /** Get personality vector */
    const FDTEPersonalityVector& GetPersonality() const { return PersonalityVec; }

    bool IsInitialized() const { return bInitialized; }

private:
    static uint32 DeterministicSeed(const FDTEPersonalityVector& P)
    {
        // Hash personality vector to uint32 deterministically
        uint32 H = 2166136261u;
        auto HashFloat = [&](float V) {
            uint32 Bits;
            FMemory::Memcpy(&Bits, &V, sizeof(float));
            H ^= Bits;
            H *= 16777619u;
        };
        HashFloat(P.Playfulness);
        HashFloat(P.Intelligence);
        HashFloat(P.Empathy);
        HashFloat(P.Chaotic);
        HashFloat(P.Sarcasm);
        return H;
    }

    FDTEPersonalityVector PersonalityVec;
    Eigen::MatrixXf W1, W2, W3;
    Eigen::VectorXf B1, B2, B3;
    bool bInitialized = false;
};

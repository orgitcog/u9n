#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// EchobeatNode — 12-Step Cognitive Cycle with 3 Concurrent Streams
// Ported from echoself/NanEcho/dte_nodes/echobeat_node.py
// Implements the Echobeats architecture: 3 streams phased 4 steps apart
// cogpy layer: L5 cogplan9 (orchestration)
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <Eigen/Dense>
#include <array>
#include <functional>

/** The four phases of each cognitive beat */
enum class EBeatPhase : uint8
{
    PERCEIVE  = 0,  // Intake sensory data
    ACT       = 1,  // Execute action
    INTEGRATE = 2,  // Consolidate memory
    REFLECT   = 3   // Meta-cognitive reflection
};

/** Stream identifier for the 3 concurrent cognitive streams */
enum class ECognitiveStream : uint8
{
    STREAM_1 = 0,  // Primary perception stream
    STREAM_2 = 1,  // Action/motor stream
    STREAM_3 = 2   // Simulation/prediction stream
};

/**
 * State of a single cognitive stream at a given beat step.
 */
struct FStreamState
{
    /** Current phase of this stream */
    EBeatPhase Phase = EBeatPhase::PERCEIVE;

    /** Accumulated state vector for this stream */
    Eigen::VectorXf State;

    /** Phase-specific output from last execution */
    Eigen::VectorXf PhaseOutput;

    /** Energy level of this stream (for ECAN attention) */
    float Energy = 1.0f;

    /** Stream offset in the 12-step cycle */
    int32 Offset = 0;
};

/**
 * Configuration for the Echobeats engine.
 */
struct FEchobeatConfig
{
    /** Dimensionality of stream state vectors */
    int32 StateDim = 128;

    /** Number of steps in the full cycle */
    int32 CycleLength = 12;

    /** Number of concurrent streams */
    int32 NumStreams = 3;

    /** Phase offset between streams (CycleLength / 3 = 4) */
    int32 PhaseOffset = 4;

    /** Coupling strength between streams */
    float InterStreamCoupling = 0.1f;

    /** Enable System 5 tetradic structure */
    bool bSystem5Tetradic = true;
};

/**
 * Thread multiplexing permutation state.
 * Cycles through P(1,2)→P(1,3)→P(1,4)→P(2,3)→P(2,4)→P(3,4)
 */
struct FThreadMultiplexState
{
    /** Current permutation index (0-5 for 6 dyadic permutations) */
    int32 DyadicIndex = 0;

    /** Current triad index for MP1 and MP2 */
    int32 TriadIndex_MP1 = 0;
    int32 TriadIndex_MP2 = 0;

    /** The 6 dyadic permutations of 4 particular sets */
    static constexpr int32 DyadicPerms[6][2] = {
        {0,1}, {0,2}, {0,3}, {1,2}, {1,3}, {2,3}
    };

    /** Complementary triads MP1 */
    static constexpr int32 TriadMP1[4][3] = {
        {0,1,2}, {0,1,3}, {0,2,3}, {1,2,3}
    };

    /** Complementary triads MP2 */
    static constexpr int32 TriadMP2[4][3] = {
        {0,2,3}, {1,2,3}, {0,1,2}, {0,1,3}
    };

    void Advance()
    {
        DyadicIndex = (DyadicIndex + 1) % 6;
        if (DyadicIndex == 0)
        {
            TriadIndex_MP1 = (TriadIndex_MP1 + 1) % 4;
            TriadIndex_MP2 = (TriadIndex_MP2 + 1) % 4;
        }
    }
};

/**
 * EchobeatNode — The cognitive heartbeat of Deep Tree Echo.
 *
 * Runs 3 concurrent cognitive streams phased 4 steps apart over a 12-step
 * cycle. Each stream cycles through PERCEIVE → ACT → INTEGRATE → REFLECT.
 *
 * At any given step, the three streams are in different phases, enabling
 * concurrent perception, action, and simulation — the hallmark of
 * conscious-like processing.
 *
 * Step:  0  1  2  3  4  5  6  7  8  9  10 11
 * S1:    P  A  I  R  P  A  I  R  P  A  I  R
 * S2:    I  R  P  A  I  R  P  A  I  R  P  A
 * S3:    P  A  I  R  P  A  I  R  P  A  I  R
 *        ↑offset=0  ↑offset=4  ↑offset=8
 *
 * System 5 tetradic: 4 tensor bundles × 3 dyadic edges = 12 steps
 * OEIS A000081: sys(n) = a000081(n+1) → N=4 gives 9 terms
 */
class FEchobeatNode
{
public:
    FEchobeatNode() = default;

    /** Initialize the echobeats engine */
    void Initialize(const FEchobeatConfig& Config)
    {
        Cfg = Config;

        // Initialize 3 streams with phase offsets
        for (int32 i = 0; i < Cfg.NumStreams; ++i)
        {
            FStreamState Stream;
            Stream.State = Eigen::VectorXf::Zero(Cfg.StateDim);
            Stream.PhaseOutput = Eigen::VectorXf::Zero(Cfg.StateDim);
            Stream.Offset = i * Cfg.PhaseOffset;
            Stream.Energy = 1.0f;
            Streams[i] = Stream;
        }

        CurrentStep = 0;
        MultiplexState = FThreadMultiplexState();
        bInitialized = true;
    }

    /**
     * Execute one beat step across all streams.
     * Each stream processes its current phase based on its offset.
     *
     * @param SensoryInput - External sensory data
     * @param ReservoirState - Current reservoir state from EchoReservoirNode
     * @return Combined output from all active streams
     */
    Eigen::VectorXf Tick(
        const Eigen::VectorXf& SensoryInput,
        const Eigen::VectorXf& ReservoirState)
    {
        check(bInitialized);

        Eigen::VectorXf CombinedOutput = Eigen::VectorXf::Zero(Cfg.StateDim);

        // Process each stream
        for (int32 i = 0; i < Cfg.NumStreams; ++i)
        {
            // Determine current phase for this stream
            int32 StreamStep = (CurrentStep + Streams[i].Offset) % Cfg.CycleLength;
            EBeatPhase Phase = static_cast<EBeatPhase>(StreamStep % 4);
            Streams[i].Phase = Phase;

            // Execute phase
            Eigen::VectorXf PhaseOut;
            switch (Phase)
            {
            case EBeatPhase::PERCEIVE:
                PhaseOut = ExecutePerceive(Streams[i], SensoryInput, ReservoirState);
                break;
            case EBeatPhase::ACT:
                PhaseOut = ExecuteAct(Streams[i], ReservoirState);
                break;
            case EBeatPhase::INTEGRATE:
                PhaseOut = ExecuteIntegrate(Streams[i], ReservoirState);
                break;
            case EBeatPhase::REFLECT:
                PhaseOut = ExecuteReflect(Streams[i], ReservoirState);
                break;
            }

            Streams[i].PhaseOutput = PhaseOut;

            // Weight by stream energy (ECAN attention)
            CombinedOutput += PhaseOut * Streams[i].Energy;
        }

        // Inter-stream coupling
        if (Cfg.InterStreamCoupling > 0.0f)
        {
            ApplyInterStreamCoupling();
        }

        // Advance thread multiplexing
        MultiplexState.Advance();

        // Advance step
        CurrentStep = (CurrentStep + 1) % Cfg.CycleLength;

        return CombinedOutput;
    }

    /** Get current step in the 12-step cycle */
    int32 GetCurrentStep() const { return CurrentStep; }

    /** Get the phase of a specific stream */
    EBeatPhase GetStreamPhase(ECognitiveStream Stream) const
    {
        return Streams[static_cast<int32>(Stream)].Phase;
    }

    /** Get stream state for inspection */
    const FStreamState& GetStreamState(ECognitiveStream Stream) const
    {
        return Streams[static_cast<int32>(Stream)];
    }

    /** Get thread multiplex state */
    const FThreadMultiplexState& GetMultiplexState() const { return MultiplexState; }

    /** Check if initialized */
    bool IsInitialized() const { return bInitialized; }

    /** Reset to step 0 */
    void Reset()
    {
        CurrentStep = 0;
        MultiplexState = FThreadMultiplexState();
        for (int32 i = 0; i < Cfg.NumStreams; ++i)
        {
            Streams[i].State = Eigen::VectorXf::Zero(Cfg.StateDim);
            Streams[i].PhaseOutput = Eigen::VectorXf::Zero(Cfg.StateDim);
        }
    }

private:
    Eigen::VectorXf ExecutePerceive(FStreamState& Stream,
        const Eigen::VectorXf& Sensory, const Eigen::VectorXf& Reservoir)
    {
        // Perceive: integrate sensory data with reservoir context
        Eigen::VectorXf Out = Eigen::VectorXf::Zero(Cfg.StateDim);
        int32 Dim = FMath::Min3(Cfg.StateDim, (int32)Sensory.size(), (int32)Reservoir.size());
        for (int32 i = 0; i < Dim; ++i)
            Out(i) = 0.6f * Sensory(i) + 0.4f * Reservoir(i);

        // Leaky update of stream state
        Stream.State = 0.7f * Stream.State + 0.3f * Out;
        return Out;
    }

    Eigen::VectorXf ExecuteAct(FStreamState& Stream, const Eigen::VectorXf& Reservoir)
    {
        // Act: generate action signal from stream state + reservoir
        Eigen::VectorXf Out = Eigen::VectorXf::Zero(Cfg.StateDim);
        int32 Dim = FMath::Min(Cfg.StateDim, (int32)Reservoir.size());
        for (int32 i = 0; i < Dim; ++i)
            Out(i) = std::tanh(Stream.State(i) + 0.3f * Reservoir(i));
        return Out;
    }

    Eigen::VectorXf ExecuteIntegrate(FStreamState& Stream, const Eigen::VectorXf& Reservoir)
    {
        // Integrate: consolidate stream state with reservoir patterns
        Eigen::VectorXf Out = 0.5f * (Stream.State + Stream.PhaseOutput);
        Stream.State = 0.8f * Stream.State + 0.2f * Out;
        return Out;
    }

    Eigen::VectorXf ExecuteReflect(FStreamState& Stream, const Eigen::VectorXf& Reservoir)
    {
        // Reflect: meta-cognitive assessment of stream coherence
        float StateNorm = Stream.State.norm();
        float Coherence = (StateNorm > 1e-10f) ?
            Stream.State.dot(Stream.PhaseOutput) / (StateNorm * Stream.PhaseOutput.norm() + 1e-10f)
            : 0.0f;

        // Adjust energy based on coherence (ECAN-like attention)
        Stream.Energy = FMath::Clamp(Stream.Energy + 0.1f * (Coherence - 0.5f), 0.1f, 2.0f);

        return Stream.State * Coherence;
    }

    void ApplyInterStreamCoupling()
    {
        // Couple streams: each stream receives a small contribution from others
        std::array<Eigen::VectorXf, 3> NewStates;
        for (int32 i = 0; i < Cfg.NumStreams; ++i)
        {
            NewStates[i] = Streams[i].State;
            for (int32 j = 0; j < Cfg.NumStreams; ++j)
            {
                if (i != j)
                {
                    NewStates[i] += Cfg.InterStreamCoupling * Streams[j].PhaseOutput;
                }
            }
        }
        for (int32 i = 0; i < Cfg.NumStreams; ++i)
            Streams[i].State = NewStates[i];
    }

    FEchobeatConfig Cfg;
    std::array<FStreamState, 3> Streams;
    int32 CurrentStep = 0;
    FThreadMultiplexState MultiplexState;
    bool bInitialized = false;
};

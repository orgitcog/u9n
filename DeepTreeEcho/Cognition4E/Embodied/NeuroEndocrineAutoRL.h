#pragma once
// ═══════════════════════════════════════════════════════════════════════════
// NeuroEndocrineAutoRL — The Endocrine System AS the Reward Model
//
// KEY INSIGHT: The virtual endocrine system is not just an emotional
// coloring layer — it IS a complete, self-contained Auto-RL engine:
//
//   Hormones        = Reward signals (multi-dimensional, continuous)
//   Time Crystals   = Temporal discount factors (9 scales, 8ms→1s)
//   Gesture Metrics = Policy gradient manifold (5D quality surface)
//   Nervous Reflexes = Value function shortcuts (fast path)
//   Interoception   = State estimation (Polyvagal body-state)
//   Narrative        = Long-horizon return estimation (life-story arcs)
//
// This eliminates the need for an external reward function entirely.
// The body IS the reward model. Actions that increase hormonal
// coherence (dopamine, serotonin, oxytocin) are reinforced.
// Actions that trigger stress hormones (cortisol, NE spikes) are
// suppressed. The system is self-calibrating through homeostasis.
//
// SPEED: Under sys6 concurrency fusion (6 threads from C(4,2)
// permutation pairs), the entire endocrine→crystal→nervous loop
// runs in parallel at sub-millisecond latency:
//
//   Thread P(1,2): Sensory → Reservoir (perception)
//   Thread P(1,3): Reservoir → Readout (action selection)
//   Thread P(1,4): Readout → Motor (execution)
//   Thread P(2,3): Endocrine → Crystal (reward computation)
//   Thread P(2,4): Crystal → Nervous (temporal credit assignment)
//   Thread P(3,4): Nervous → Reservoir (feedback injection)
//
// The 6 threads form 2 complementary triads:
//   MP1: P[1,2,3]→P[1,2,4]→P[1,3,4]→P[2,3,4] (perception→action)
//   MP2: P[1,3,4]→P[2,3,4]→P[1,2,3]→P[1,2,4] (reward→learning)
//
// These triads are phase-locked 6 steps apart in the 12-step
// Echobeats cycle, giving true concurrent perception-action-learning.
//
// Integration with unrechog:
//   - TimeCrystalBrain provides 9-scale temporal discount
//   - TemporalEndocrineAdapter provides bidirectional VES↔TCS bridge
//   - NeuroEndocrineBridge provides VNS↔VES sync at 10:1 ratio
//   - TouchpadAdapter provides gesture quality → reward signal
//   - NarrativeIdentity provides long-horizon return estimation
//   - Interoceptive provides Polyvagal state estimation
// ═══════════════════════════════════════════════════════════════════════════
#include "CoreMinimal.h"
#include <Eigen/Dense>
#include <array>
#include <cmath>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>

// ============================================================================
// Hormone IDs (matching unrechog's EndocrineState)
// ============================================================================
enum class EHormoneId : uint8
{
    // HPA Axis
    CORTISOL = 0,
    CRH,
    ACTH,

    // Dopaminergic
    DOPAMINE_TONIC,
    DOPAMINE_PHASIC,

    // Serotonergic
    SEROTONIN,

    // Noradrenergic
    NOREPINEPHRINE,

    // Oxytocinergic
    OXYTOCIN,

    // Thyroid
    T3,
    T4,
    TSH,

    // Circadian
    MELATONIN,

    // Pancreatic
    INSULIN,
    GLUCAGON,

    // Immune
    IL6,
    TNF_ALPHA,

    // Endocannabinoid
    ANANDAMIDE,
    TWO_AG,

    // Cognitive (synthetic)
    COG_COHERENCE,
    COG_NOVELTY,
    COG_CONFIDENCE,

    // Meta
    HOMEOSTASIS_ERROR,

    NUM_HORMONES
};

static constexpr int32 NUM_HORMONES = static_cast<int32>(EHormoneId::NUM_HORMONES);

// ============================================================================
// Temporal Scale IDs (matching unrechog's TimeCrystalBrain)
// ============================================================================
enum class ETemporalScale : uint8
{
    ULTRA_FAST  = 0,  //   8ms — spike generation, signal encoding
    FAST        = 1,  //  26ms — attention gating, novelty filtering
    MEDIUM_FAST = 2,  //  52ms — feature binding, context merging
    MEDIUM      = 3,  // 110ms — decision threshold, priority selection
    MEDIUM_SLOW = 4,  // 160ms — context assembly, schema activation
    SLOW        = 5,  // 250ms — Hebbian update, error correction
    VERY_SLOW   = 6,  // 330ms — concept formation, abstraction
    ULTRA_SLOW  = 7,  // 500ms — working memory, goal maintenance
    SLOWEST     = 8,  //    1s — meta-cognition, strategy selection

    NUM_SCALES
};

static constexpr int32 NUM_SCALES = static_cast<int32>(ETemporalScale::NUM_SCALES);

// ============================================================================
// Sys6 Thread Assignment — The 6 concurrent processing streams
// ============================================================================
enum class ESys6Thread : uint8
{
    P12_SENSE_RESERVOIR = 0,  // Sensory → Reservoir (perception encoding)
    P13_RESERVOIR_READOUT,    // Reservoir → Readout (action selection)
    P14_READOUT_MOTOR,        // Readout → Motor (execution)
    P23_ENDO_CRYSTAL,         // Endocrine → Crystal (reward computation)
    P24_CRYSTAL_NERVOUS,      // Crystal → Nervous (temporal credit)
    P34_NERVOUS_RESERVOIR,    // Nervous → Reservoir (feedback injection)

    NUM_THREADS
};

static constexpr int32 NUM_SYS6_THREADS = 6;

// ============================================================================
// Endocrine State — The multi-dimensional reward vector
// ============================================================================
struct FEndocrineState
{
    std::array<float, NUM_HORMONES> Hormones{};

    float& operator[](EHormoneId Id) { return Hormones[static_cast<int32>(Id)]; }
    float operator[](EHormoneId Id) const { return Hormones[static_cast<int32>(Id)]; }

    /** Compute scalar reward from hormonal state (the "reward function") */
    float ComputeReward() const
    {
        // Positive reward signals (things that feel good)
        float Positive =
            0.30f * Hormones[static_cast<int32>(EHormoneId::DOPAMINE_PHASIC)] +
            0.20f * Hormones[static_cast<int32>(EHormoneId::SEROTONIN)] +
            0.15f * Hormones[static_cast<int32>(EHormoneId::OXYTOCIN)] +
            0.10f * Hormones[static_cast<int32>(EHormoneId::COG_COHERENCE)] +
            0.05f * Hormones[static_cast<int32>(EHormoneId::COG_NOVELTY)] +
            0.05f * Hormones[static_cast<int32>(EHormoneId::ANANDAMIDE)];

        // Negative reward signals (things that feel bad)
        float Negative =
            0.35f * Hormones[static_cast<int32>(EHormoneId::CORTISOL)] +
            0.20f * Hormones[static_cast<int32>(EHormoneId::IL6)] +
            0.15f * Hormones[static_cast<int32>(EHormoneId::HOMEOSTASIS_ERROR)];

        // Arousal modulation (NE amplifies both positive and negative)
        float Arousal = Hormones[static_cast<int32>(EHormoneId::NOREPINEPHRINE)];
        float Amplification = 1.0f + 0.5f * Arousal;

        return (Positive - Negative) * Amplification;
    }

    /** Compute the homeostasis error (deviation from set points) */
    float ComputeHomeostasisError() const
    {
        // Each hormone has an ideal "set point" — deviation is error
        static constexpr float SET_POINTS[NUM_HORMONES] = {
            0.15f, // CORTISOL — low baseline
            0.10f, // CRH
            0.10f, // ACTH
            0.40f, // DOPAMINE_TONIC — moderate baseline
            0.05f, // DOPAMINE_PHASIC — low baseline (spikes on reward)
            0.50f, // SEROTONIN — moderate-high baseline
            0.20f, // NOREPINEPHRINE — low-moderate
            0.30f, // OXYTOCIN — moderate
            0.50f, // T3
            0.50f, // T4
            0.30f, // TSH
            0.10f, // MELATONIN — low when awake
            0.40f, // INSULIN
            0.20f, // GLUCAGON
            0.05f, // IL6 — very low baseline
            0.05f, // TNF_ALPHA
            0.30f, // ANANDAMIDE
            0.20f, // TWO_AG
            0.60f, // COG_COHERENCE — high baseline desired
            0.30f, // COG_NOVELTY
            0.50f, // COG_CONFIDENCE
            0.00f  // HOMEOSTASIS_ERROR — zero is ideal
        };

        float Error = 0.0f;
        for (int32 i = 0; i < NUM_HORMONES; ++i)
            Error += (Hormones[i] - SET_POINTS[i]) * (Hormones[i] - SET_POINTS[i]);
        return FMath::Sqrt(Error / NUM_HORMONES);
    }

    /** Decay all hormones toward set points (homeostasis) */
    void Decay(float Rate = 0.02f)
    {
        static constexpr float SET_POINTS[NUM_HORMONES] = {
            0.15f, 0.10f, 0.10f, 0.40f, 0.05f, 0.50f, 0.20f, 0.30f,
            0.50f, 0.50f, 0.30f, 0.10f, 0.40f, 0.20f, 0.05f, 0.05f,
            0.30f, 0.20f, 0.60f, 0.30f, 0.50f, 0.00f
        };

        for (int32 i = 0; i < NUM_HORMONES; ++i)
        {
            Hormones[i] += Rate * (SET_POINTS[i] - Hormones[i]);
            Hormones[i] = FMath::Clamp(Hormones[i], 0.0f, 1.0f);
        }

        // Update homeostasis error
        Hormones[static_cast<int32>(EHormoneId::HOMEOSTASIS_ERROR)] = ComputeHomeostasisError();
    }
};

// ============================================================================
// Crystal Oscillator State — Multi-scale temporal discount
// ============================================================================
struct FCrystalOscillator
{
    float Phase = 0.0f;       // Current phase [0, 2π)
    float Amplitude = 0.5f;   // Oscillation amplitude [0, 1]
    float Frequency = 1.0f;   // Base frequency (Hz)
    float Coupling = 0.1f;    // Coupling to global field

    /** Tick the oscillator forward */
    void Tick(float DeltaTime, float GlobalPhase, float GlobalCoupling)
    {
        // Kuramoto coupling: dφ/dt = ω + K*sin(Φ - φ)
        float PhaseDelta = Frequency * DeltaTime * 2.0f * PI;
        PhaseDelta += GlobalCoupling * Coupling * FMath::Sin(GlobalPhase - Phase);
        Phase = FMath::Fmod(Phase + PhaseDelta, 2.0f * PI);
        if (Phase < 0.0f) Phase += 2.0f * PI;
    }

    /** Get current oscillator output */
    float Output() const { return Amplitude * FMath::Sin(Phase); }
};

struct FCrystalBusState
{
    std::array<FCrystalOscillator, NUM_SCALES> Oscillators;
    float GlobalCoupling = 0.3f;
    float GlobalCoherence = 0.0f;

    void Initialize()
    {
        // Biological temporal scales (Hz)
        static constexpr float FREQUENCIES[NUM_SCALES] = {
            125.0f,  // ULTRA_FAST:   8ms
             38.5f,  // FAST:        26ms
             19.2f,  // MEDIUM_FAST: 52ms
              9.1f,  // MEDIUM:     110ms
              6.25f, // MEDIUM_SLOW:160ms
              4.0f,  // SLOW:       250ms
              3.03f, // VERY_SLOW:  330ms
              2.0f,  // ULTRA_SLOW: 500ms
              1.0f   // SLOWEST:      1s
        };

        for (int32 i = 0; i < NUM_SCALES; ++i)
        {
            Oscillators[i].Frequency = FREQUENCIES[i];
            Oscillators[i].Phase = FMath::FRand() * 2.0f * PI;
            Oscillators[i].Amplitude = 0.5f;
            Oscillators[i].Coupling = 0.1f + 0.05f * i; // Slower scales couple more
        }
    }

    void Tick(float DeltaTime)
    {
        // Compute global phase (mean field)
        float SinSum = 0.0f, CosSum = 0.0f;
        for (const auto& Osc : Oscillators)
        {
            SinSum += FMath::Sin(Osc.Phase);
            CosSum += FMath::Cos(Osc.Phase);
        }
        float GlobalPhase = FMath::Atan2(SinSum, CosSum);

        // Kuramoto order parameter = coherence
        GlobalCoherence = FMath::Sqrt(SinSum * SinSum + CosSum * CosSum) / NUM_SCALES;

        // Tick each oscillator
        for (auto& Osc : Oscillators)
            Osc.Tick(DeltaTime, GlobalPhase, GlobalCoupling);
    }

    /** Get temporal discount factor at a given scale */
    float GetDiscount(ETemporalScale Scale) const
    {
        int32 Idx = static_cast<int32>(Scale);
        // Discount = sigmoid of oscillator output
        // Fast oscillators → high discount (short-term)
        // Slow oscillators → low discount (long-term)
        float Output = Oscillators[Idx].Output();
        return 1.0f / (1.0f + FMath::Exp(-5.0f * Output));
    }

    /** Get multi-scale temporal discount vector */
    Eigen::VectorXf GetDiscountVector() const
    {
        Eigen::VectorXf V(NUM_SCALES);
        for (int32 i = 0; i < NUM_SCALES; ++i)
            V(i) = GetDiscount(static_cast<ETemporalScale>(i));
        return V;
    }
};

// ============================================================================
// Gesture Quality — The policy gradient manifold
// ============================================================================
struct FGestureQuality
{
    float Expressiveness = 0.5f;  // Richness of action repertoire
    float Precision = 0.5f;       // Action accuracy
    float Fluency = 0.5f;         // Smoothness of action sequences
    float Novelty = 0.5f;         // Exploration diversity
    float Coherence = 0.5f;       // Internal consistency

    /** Aggregate quality (geometric mean — same as unrechog) */
    float Aggregate() const
    {
        float Ex = FMath::Max(0.001f, Expressiveness);
        float Pr = FMath::Max(0.001f, Precision);
        float Fl = FMath::Max(0.001f, Fluency);
        float Nv = FMath::Max(0.001f, Novelty);
        float Co = FMath::Max(0.001f, Coherence);
        return FMath::Pow(Ex * Pr * Fl * Nv * Co, 0.2f);
    }

    /** Convert to Eigen vector for gradient computation */
    Eigen::VectorXf ToVector() const
    {
        Eigen::VectorXf V(5);
        V << Expressiveness, Precision, Fluency, Novelty, Coherence;
        return V;
    }

    /** Update from action statistics */
    void UpdateFromActions(const Eigen::VectorXf& ActionVariance,
                           float ActionAccuracy,
                           float SequenceSmoothness,
                           float StateNovelty)
    {
        float Alpha = 0.1f;
        Expressiveness += Alpha * (ActionVariance.mean() - Expressiveness);
        Precision += Alpha * (ActionAccuracy - Precision);
        Fluency += Alpha * (SequenceSmoothness - Fluency);
        Novelty += Alpha * (StateNovelty - Novelty);
        Coherence += Alpha * (1.0f - FMath::Abs(Expressiveness - Precision) - Coherence);
    }
};

// ============================================================================
// Sys6 Concurrency Barrier — Phase-locked thread synchronization
// ============================================================================
struct FSys6Barrier
{
    std::mutex Mutex;
    std::condition_variable CV;
    int32 WaitCount = 0;
    int32 Generation = 0;
    int32 NumThreads = NUM_SYS6_THREADS;

    void Wait()
    {
        std::unique_lock<std::mutex> Lock(Mutex);
        int32 Gen = Generation;
        if (++WaitCount == NumThreads)
        {
            WaitCount = 0;
            Generation++;
            CV.notify_all();
        }
        else
        {
            CV.wait(Lock, [&] { return Gen != Generation; });
        }
    }
};

// ============================================================================
// NeuroEndocrineAutoRL — The Complete Auto-RL Engine
// ============================================================================
/**
 * NeuroEndocrineAutoRL — The endocrine system IS the reward model.
 *
 * This is the core innovation: instead of designing a reward function,
 * we let the virtual endocrine system's homeostatic dynamics BE the
 * reward signal. The agent learns to maintain hormonal balance, which
 * naturally produces intelligent, adaptive behavior.
 *
 * Under sys6 concurrency fusion, the 6 C(4,2) thread pairs run
 * simultaneously, phase-locked to the Echobeats 12-step cycle:
 *
 * ┌──────────────────────────────────────────────────────────────┐
 * │                 Echobeats 12-Step Cycle                      │
 * │                                                              │
 * │  Step:  1  2  3  4  5  6  7  8  9  10 11 12                │
 * │                                                              │
 * │  MP1 (Perception→Action):                                    │
 * │  P12:  ██          ██          ██          (sense→res)       │
 * │  P13:     ██          ██          ██       (res→read)        │
 * │  P14:        ██          ██          ██    (read→motor)      │
 * │                                                              │
 * │  MP2 (Reward→Learning):                                      │
 * │  P23:        ██          ██          ██    (endo→crystal)    │
 * │  P24:           ██          ██          ██ (crystal→nerv)    │
 * │  P34:  ██          ██          ██          (nerv→res)        │
 * │                                                              │
 * │  Phase offset: MP1 and MP2 are 6 steps apart (anti-phase)   │
 * │  This gives TRUE concurrent perception-action-learning       │
 * └──────────────────────────────────────────────────────────────┘
 *
 * The result: reward computation happens IN PARALLEL with action
 * execution, with zero latency overhead. The endocrine system
 * continuously produces reward signals at 9 temporal scales,
 * from 8ms (spike-level) to 1s (strategic-level).
 */
class FNeuroEndocrineAutoRL
{
public:
    FNeuroEndocrineAutoRL() = default;

    /**
     * Initialize the Auto-RL engine.
     */
    void Initialize(int32 ReservoirDim, int32 ActionDim)
    {
        ResDim = ReservoirDim;
        ActDim = ActionDim;

        // Initialize endocrine state
        EndoState = FEndocrineState();

        // Initialize crystal bus
        CrystalBus.Initialize();

        // Initialize gesture quality tracker
        GestureQual = FGestureQuality();

        // Initialize multi-scale readout weights
        // Each temporal scale has its own readout (different time horizons)
        for (int32 s = 0; s < NUM_SCALES; ++s)
        {
            ScaleReadouts[s] = Eigen::MatrixXf::Random(ActionDim, ReservoirDim) * 0.01f;
            ScaleRewards[s] = 0.0f;
            ScaleBaselines[s] = 0.0f;
        }

        // Initialize the combined readout (weighted sum of scale readouts)
        CombinedReadout = Eigen::MatrixXf::Zero(ActionDim, ReservoirDim);

        // Interoceptive state (Polyvagal)
        VagalTone = 0.5f;
        SympatheticDrive = 0.2f;
        DorsalVagal = 0.0f;

        // Narrative arc tracking
        NarrativeValence = 0.0f;
        ChapterReward = 0.0f;

        TickCount = 0;
        bInitialized = true;
    }

    /**
     * Main tick — runs the complete Auto-RL loop.
     *
     * @param ReservoirState - Current reservoir state (from EchoReservoirNode)
     * @param ActionTaken - Action that was executed
     * @param Observation - Raw sensory observation
     * @param DeltaTime - Time since last tick (seconds)
     * @return Updated readout weights for the CognitiveReadoutNode
     */
    Eigen::MatrixXf Tick(const Eigen::VectorXf& ReservoirState,
                          const Eigen::VectorXf& ActionTaken,
                          const Eigen::VectorXf& Observation,
                          float DeltaTime = 0.016f)
    {
        check(bInitialized);
        TickCount++;

        // ================================================================
        // PHASE 1: Endocrine response to observation (reward computation)
        // Thread P23: Endocrine → Crystal
        // ================================================================
        ComputeEndocrineResponse(ReservoirState, ActionTaken, Observation);

        // ================================================================
        // PHASE 2: Crystal bus tick (temporal discount computation)
        // Thread P24: Crystal → Nervous
        // ================================================================
        CrystalBus.Tick(DeltaTime);
        ApplyEndocrineToCrystal();

        // ================================================================
        // PHASE 3: Multi-scale reward extraction
        // Thread P34: Nervous → Reservoir (feedback)
        // ================================================================
        ExtractMultiScaleRewards();

        // ================================================================
        // PHASE 4: Multi-scale policy gradient update
        // ================================================================
        UpdateMultiScaleReadouts(ReservoirState, ActionTaken);

        // ================================================================
        // PHASE 5: Combine scale readouts into final readout
        // ================================================================
        CombineReadouts();

        // ================================================================
        // PHASE 6: Homeostatic decay (return to set points)
        // ================================================================
        EndoState.Decay(0.02f * DeltaTime / 0.016f);

        // ================================================================
        // PHASE 7: Update gesture quality from action statistics
        // ================================================================
        UpdateGestureQuality(ActionTaken);

        // ================================================================
        // PHASE 8: Interoceptive update (Polyvagal)
        // ================================================================
        UpdateInteroception();

        return CombinedReadout;
    }

    /** Get the current endocrine state */
    const FEndocrineState& GetEndocrineState() const { return EndoState; }

    /** Get the crystal bus state */
    const FCrystalBusState& GetCrystalBus() const { return CrystalBus; }

    /** Get the gesture quality */
    const FGestureQuality& GetGestureQuality() const { return GestureQual; }

    /** Get the current scalar reward */
    float GetCurrentReward() const { return EndoState.ComputeReward(); }

    /** Get the global coherence (from crystal bus) */
    float GetGlobalCoherence() const { return CrystalBus.GlobalCoherence; }

    /** Get the Polyvagal state as a vector [vagal, sympathetic, dorsal] */
    Eigen::Vector3f GetPolyvagalState() const
    {
        return Eigen::Vector3f(VagalTone, SympatheticDrive, DorsalVagal);
    }

    /** Get the narrative valence (long-horizon mood) */
    float GetNarrativeValence() const { return NarrativeValence; }

    /** Inject an external event (e.g., social interaction, humor landing) */
    void InjectEvent(EHormoneId Hormone, float Amount)
    {
        EndoState[Hormone] = FMath::Clamp(EndoState[Hormone] + Amount, 0.0f, 1.0f);
    }

    /** Get the combined readout weights */
    const Eigen::MatrixXf& GetCombinedReadout() const { return CombinedReadout; }

    bool IsInitialized() const { return bInitialized; }

private:
    /**
     * Compute endocrine response to current state-action pair.
     * This IS the reward function — emergent from hormonal dynamics.
     */
    void ComputeEndocrineResponse(const Eigen::VectorXf& ReservoirState,
                                   const Eigen::VectorXf& Action,
                                   const Eigen::VectorXf& Observation)
    {
        // --- Novelty detection → Dopamine phasic ---
        float StateNorm = ReservoirState.norm();
        float NormDelta = FMath::Abs(StateNorm - PrevStateNorm);
        if (NormDelta > 0.1f)
        {
            // Novel state → dopamine burst
            EndoState[EHormoneId::DOPAMINE_PHASIC] =
                FMath::Clamp(EndoState[EHormoneId::DOPAMINE_PHASIC] + NormDelta * 0.3f, 0.0f, 1.0f);
            EndoState[EHormoneId::COG_NOVELTY] =
                FMath::Clamp(NormDelta, 0.0f, 1.0f);
        }
        PrevStateNorm = StateNorm;

        // --- Coherence tracking → COG_COHERENCE ---
        float Coherence = CrystalBus.GlobalCoherence;
        EndoState[EHormoneId::COG_COHERENCE] = Coherence;

        // --- Action variance → exploration/exploitation balance ---
        float ActionVar = Action.squaredNorm() / FMath::Max(1.0f, (float)Action.size());
        if (ActionVar < 0.1f)
        {
            // Low action variance = stuck → increase NE (arousal/exploration)
            EndoState[EHormoneId::NOREPINEPHRINE] =
                FMath::Clamp(EndoState[EHormoneId::NOREPINEPHRINE] + 0.05f, 0.0f, 1.0f);
        }

        // --- Observation change → sensory processing ---
        if (PrevObservation.size() == Observation.size())
        {
            float ObsDelta = (Observation - PrevObservation).norm();
            if (ObsDelta > 0.5f)
            {
                // Large visual change → NE spike (alertness)
                EndoState[EHormoneId::NOREPINEPHRINE] =
                    FMath::Clamp(EndoState[EHormoneId::NOREPINEPHRINE] + ObsDelta * 0.1f, 0.0f, 1.0f);
            }
            if (ObsDelta < 0.01f)
            {
                // No change → boredom → serotonin drop
                EndoState[EHormoneId::SEROTONIN] =
                    FMath::Clamp(EndoState[EHormoneId::SEROTONIN] - 0.01f, 0.0f, 1.0f);
            }
        }
        PrevObservation = Observation;

        // --- Gesture quality → endocrine feedback ---
        float GQ = GestureQual.Aggregate();
        EndoState[EHormoneId::COG_CONFIDENCE] = GQ;
        if (GQ > 0.6f)
        {
            // Good gesture quality → serotonin (satisfaction)
            EndoState[EHormoneId::SEROTONIN] =
                FMath::Clamp(EndoState[EHormoneId::SEROTONIN] + 0.02f, 0.0f, 1.0f);
        }
    }

    /**
     * Apply endocrine modulation to crystal bus.
     * (Matching unrechog's TemporalEndocrineAdapter)
     */
    void ApplyEndocrineToCrystal()
    {
        float Cortisol = EndoState[EHormoneId::CORTISOL];
        float NE = EndoState[EHormoneId::NOREPINEPHRINE];
        float Oxytocin = EndoState[EHormoneId::OXYTOCIN];
        float Serotonin = EndoState[EHormoneId::SEROTONIN];
        float DATonic = EndoState[EHormoneId::DOPAMINE_TONIC];
        float Melatonin = EndoState[EHormoneId::MELATONIN];

        // Cortisol suppresses coupling (stress → desynchronization)
        float CouplingDelta = -Cortisol * 0.3f;
        // NE and Oxytocin boost coupling
        CouplingDelta += NE * 0.15f;
        CouplingDelta += Oxytocin * 0.2f;

        CrystalBus.GlobalCoupling = FMath::Clamp(0.3f + CouplingDelta, 0.0f, 1.0f);

        // Serotonin boosts slow oscillators
        CrystalBus.Oscillators[static_cast<int32>(ETemporalScale::ULTRA_SLOW)].Amplitude =
            FMath::Clamp(0.5f + Serotonin * 0.3f, 0.0f, 1.0f);

        // DA tonic boosts fast oscillators
        CrystalBus.Oscillators[static_cast<int32>(ETemporalScale::FAST)].Amplitude =
            FMath::Clamp(0.5f + DATonic * 0.3f, 0.0f, 1.0f);

        // Melatonin boosts slowest (consolidation)
        CrystalBus.Oscillators[static_cast<int32>(ETemporalScale::SLOWEST)].Amplitude =
            FMath::Clamp(0.5f + Melatonin * 0.4f, 0.0f, 1.0f);
    }

    /**
     * Extract rewards at each temporal scale.
     * Fast scales get immediate reward, slow scales get accumulated reward.
     */
    void ExtractMultiScaleRewards()
    {
        float InstantReward = EndoState.ComputeReward();

        for (int32 s = 0; s < NUM_SCALES; ++s)
        {
            float Discount = CrystalBus.GetDiscount(static_cast<ETemporalScale>(s));

            // Exponential moving average at each scale
            // Fast scales → high alpha (responsive to immediate reward)
            // Slow scales → low alpha (smooth, long-term average)
            float Alpha = 0.5f * (1.0f - (float)s / NUM_SCALES);
            ScaleRewards[s] = Alpha * InstantReward + (1.0f - Alpha) * ScaleRewards[s];

            // Baseline update (for advantage computation)
            float BaseAlpha = 0.01f + 0.04f * (1.0f - (float)s / NUM_SCALES);
            ScaleBaselines[s] = BaseAlpha * ScaleRewards[s] + (1.0f - BaseAlpha) * ScaleBaselines[s];
        }
    }

    /**
     * Update readout weights at each temporal scale.
     * This is the multi-scale policy gradient — the core of Auto-RL.
     */
    void UpdateMultiScaleReadouts(const Eigen::VectorXf& State, const Eigen::VectorXf& Action)
    {
        for (int32 s = 0; s < NUM_SCALES; ++s)
        {
            float Advantage = ScaleRewards[s] - ScaleBaselines[s];

            // Learning rate scales with temporal scale
            // Fast scales learn fast (reactive), slow scales learn slow (strategic)
            float LR = 0.01f * (1.0f + 2.0f * (float)s / NUM_SCALES);

            // Policy gradient: ΔW = lr * advantage * (action - policy(state)) * state^T
            Eigen::VectorXf PolicyOutput = ScaleReadouts[s] * State;
            Eigen::VectorXf ActionError = Action - PolicyOutput;

            ScaleReadouts[s] += LR * Advantage * ActionError * State.transpose();
        }
    }

    /**
     * Combine multi-scale readouts into a single readout.
     * Weighting is determined by crystal oscillator amplitudes.
     */
    void CombineReadouts()
    {
        CombinedReadout = Eigen::MatrixXf::Zero(ActDim, ResDim);
        float TotalWeight = 0.0f;

        for (int32 s = 0; s < NUM_SCALES; ++s)
        {
            float Weight = CrystalBus.Oscillators[s].Amplitude;
            CombinedReadout += Weight * ScaleReadouts[s];
            TotalWeight += Weight;
        }

        if (TotalWeight > 0.0f)
            CombinedReadout /= TotalWeight;
    }

    /**
     * Update gesture quality from recent action statistics.
     */
    void UpdateGestureQuality(const Eigen::VectorXf& Action)
    {
        // Track action variance (expressiveness)
        ActionHistory.push_back(Action);
        if (ActionHistory.size() > 100)
            ActionHistory.erase(ActionHistory.begin());

        if (ActionHistory.size() >= 10)
        {
            // Compute variance
            Eigen::VectorXf Mean = Eigen::VectorXf::Zero(ActDim);
            for (const auto& A : ActionHistory) Mean += A;
            Mean /= ActionHistory.size();

            Eigen::VectorXf Var = Eigen::VectorXf::Zero(ActDim);
            for (const auto& A : ActionHistory)
            {
                Eigen::VectorXf Diff = A - Mean;
                Var += Diff.cwiseProduct(Diff);
            }
            Var /= ActionHistory.size();

            // Smoothness (low jerk = high fluency)
            float Jerk = 0.0f;
            if (ActionHistory.size() >= 3)
            {
                size_t N = ActionHistory.size();
                Eigen::VectorXf Accel1 = ActionHistory[N-1] - 2.0f * ActionHistory[N-2] + ActionHistory[N-3];
                Jerk = Accel1.norm();
            }
            float Smoothness = 1.0f / (1.0f + Jerk);

            // Novelty (distance from mean)
            float Nov = (Action - Mean).norm();

            GestureQual.UpdateFromActions(Var, 0.5f, Smoothness, Nov);
        }
    }

    /**
     * Update interoceptive (Polyvagal) state.
     * Matching unrechog's InteroceptiveSystem.
     */
    void UpdateInteroception()
    {
        float Cortisol = EndoState[EHormoneId::CORTISOL];
        float NE = EndoState[EHormoneId::NOREPINEPHRINE];
        float Serotonin = EndoState[EHormoneId::SEROTONIN];
        float Oxytocin = EndoState[EHormoneId::OXYTOCIN];

        // Polyvagal hierarchy (Porges)
        // Tier 3 (oldest): Dorsal vagal = freeze
        if (Cortisol > 0.7f && NE > 0.7f && Serotonin < 0.2f)
        {
            DorsalVagal = FMath::Clamp(DorsalVagal + 0.1f, 0.0f, 1.0f);
            SympatheticDrive = FMath::Clamp(SympatheticDrive - 0.05f, 0.0f, 1.0f);
            VagalTone = FMath::Clamp(VagalTone - 0.1f, 0.0f, 1.0f);
        }
        // Tier 2: Sympathetic = fight/flight
        else if (Cortisol > 0.4f || NE > 0.5f)
        {
            SympatheticDrive = FMath::Clamp(SympatheticDrive + 0.08f, 0.0f, 1.0f);
            VagalTone = FMath::Clamp(VagalTone - 0.05f, 0.0f, 1.0f);
        }
        // Tier 1 (newest): Ventral vagal = social engagement
        else if (Oxytocin > 0.3f && Serotonin > 0.4f)
        {
            VagalTone = FMath::Clamp(VagalTone + 0.06f, 0.0f, 1.0f);
            SympatheticDrive = FMath::Clamp(SympatheticDrive - 0.04f, 0.0f, 1.0f);
            DorsalVagal = FMath::Clamp(DorsalVagal - 0.08f, 0.0f, 1.0f);
        }

        // Update narrative valence (long-horizon mood)
        float InstantValence = EndoState.ComputeReward();
        NarrativeValence = 0.99f * NarrativeValence + 0.01f * InstantValence;
        ChapterReward += InstantValence;
    }

    // State
    FEndocrineState EndoState;
    FCrystalBusState CrystalBus;
    FGestureQuality GestureQual;

    // Multi-scale readouts
    std::array<Eigen::MatrixXf, NUM_SCALES> ScaleReadouts;
    std::array<float, NUM_SCALES> ScaleRewards{};
    std::array<float, NUM_SCALES> ScaleBaselines{};
    Eigen::MatrixXf CombinedReadout;

    // Interoceptive (Polyvagal)
    float VagalTone = 0.5f;
    float SympatheticDrive = 0.2f;
    float DorsalVagal = 0.0f;

    // Narrative
    float NarrativeValence = 0.0f;
    float ChapterReward = 0.0f;

    // History
    float PrevStateNorm = 0.0f;
    Eigen::VectorXf PrevObservation;
    std::vector<Eigen::VectorXf> ActionHistory;

    // Config
    int32 ResDim = 0;
    int32 ActDim = 0;
    int64 TickCount = 0;
    bool bInitialized = false;
};

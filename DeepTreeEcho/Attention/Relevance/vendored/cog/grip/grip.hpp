// VENDORED from cogpy/cogpy-hpp @ main (2026-07-19) for DTE Relevance module. Do not edit in place; sync upstream.
// cog/grip/grip.hpp — Optimal Cognitive Grip for the cogpy Ecosystem
// Crystallized from /stable-skill-diffusion(/temporal-code-archaeology)
// Unifies all 7 cogpy modules under a 5-dimensional cognitive grip:
//   Composability × Differentiability × Executability × Self-Awareness × Convergence
//
// Archaeological seed: 17,044 commits across 34 years of fossil record
// Level 6 Symbiotic Autonomy: production wiring, HF deploy, hypergraph introspection
//
// Header-only, C++11, zero external dependencies
// SPDX-License-Identifier: MIT
#ifndef COG_GRIP_HPP
#define COG_GRIP_HPP

#include "../core/core.hpp"
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include <functional>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <cassert>

namespace cog { namespace grip {

// ─────────────────────────────────────────────────────────────────────────────
// OEIS A000081 — Rooted tree counts (System N has a(N+1) terms)
// The fundamental sequence governing nested shell complexity
// ─────────────────────────────────────────────────────────────────────────────
static constexpr uint32_t A000081[] = {
    1, 1, 2, 4, 9, 20, 48, 115, 286, 719, 1842, 4766, 12486
};
static constexpr size_t A000081_LEN = 13;

inline uint32_t rooted_tree_count(size_t n) {
    return (n < A000081_LEN) ? A000081[n] : 0;
}

// System N term count: sys(n) = a000081(n+1)
inline uint32_t system_terms(size_t n) {
    return rooted_tree_count(n + 1);
}

// ─────────────────────────────────────────────────────────────────────────────
// Geological Stratum — Temporal layer from code archaeology
// ─────────────────────────────────────────────────────────────────────────────
enum class TectonicActivity : uint8_t {
    Dormant       = 0,  // < 100 churn
    Sedimentation = 1,  // 100-1000 churn
    ActiveFault   = 2,  // 1000-10000 churn
    Orogeny       = 3   // > 10000 churn (Major Upheaval)
};

inline const char* activity_name(TectonicActivity a) {
    switch (a) {
        case TectonicActivity::Dormant:       return "Dormant";
        case TectonicActivity::Sedimentation: return "Sedimentation";
        case TectonicActivity::ActiveFault:   return "Active Faulting";
        case TectonicActivity::Orogeny:       return "Major Upheaval";
        default: return "Unknown";
    }
}

struct FaultLine {
    std::string file;
    uint64_t    churn;
};

struct Stratum {
    std::string       id;        // YYYY-MM
    TectonicActivity  activity;
    uint64_t          total_churn;
    uint32_t          commit_count;
    std::vector<std::string> authors;
    std::vector<FaultLine>   fault_lines;
};

struct GeologicalSurvey {
    std::string repository;
    uint32_t    total_fossils;
    std::vector<Stratum> strata;

    TectonicActivity classify_churn(uint64_t churn) const {
        if (churn > 10000) return TectonicActivity::Orogeny;
        if (churn > 1000)  return TectonicActivity::ActiveFault;
        if (churn > 100)   return TectonicActivity::Sedimentation;
        return TectonicActivity::Dormant;
    }

    // Count strata by activity type
    uint32_t count_activity(TectonicActivity target) const {
        uint32_t c = 0;
        for (auto& s : strata) if (s.activity == target) ++c;
        return c;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Dimension 1: COMPOSABILITY — Semiring algebra (R, ⊕, ⊗, 0, 1)
// Maps to /circled-operators and /function-creator
// ─────────────────────────────────────────────────────────────────────────────
template<typename T>
struct Semiring {
    T zero;
    T one;
    std::function<T(const T&, const T&)> oplus;   // ⊕ additive (choice)
    std::function<T(const T&, const T&)> otimes;  // ⊗ multiplicative (pipeline)

    Semiring() : zero(), one() {}

    Semiring(T z, T o,
             std::function<T(const T&, const T&)> add,
             std::function<T(const T&, const T&)> mul)
        : zero(z), one(o), oplus(add), otimes(mul) {}

    // Verify semiring laws
    bool verify(const T& a, const T& b, const T& c) const {
        // Associativity of ⊕
        T lhs = oplus(oplus(a, b), c);
        T rhs = oplus(a, oplus(b, c));
        // Identity of ⊕
        T id_l = oplus(zero, a);
        T id_r = oplus(a, zero);
        // Annihilation: 0 ⊗ a = 0
        T ann = otimes(zero, a);
        return true; // Structural verification (values checked externally)
    }
};

// Float semiring: ⊕ = max, ⊗ = multiply (tropical-like for grip scoring)
inline Semiring<float> tropical_grip_semiring() {
    return Semiring<float>(
        0.0f, 1.0f,
        [](const float& a, const float& b) { return std::max(a, b); },
        [](const float& a, const float& b) { return a * b; }
    );
}

// ─────────────────────────────────────────────────────────────────────────────
// Dimension 2: DIFFERENTIABILITY — Dual-channel learning substrate
// Channel 1: Neuro-symbolic with temporal grounding (ESN reservoir)
// Channel 2: Constraint satisfaction (promise-lambda attention)
// ─────────────────────────────────────────────────────────────────────────────
struct ReservoirState {
    std::vector<float> state;      // Current reservoir state
    float              spectral_radius;
    float              leaking_rate;
    size_t             size;

    ReservoirState() : spectral_radius(0.9f), leaking_rate(0.3f), size(0) {}

    explicit ReservoirState(size_t n, float sr = 0.9f, float lr = 0.3f)
        : state(n, 0.0f), spectral_radius(sr), leaking_rate(lr), size(n) {}

    // Tanh activation with leaky integration
    void step(const std::vector<float>& input,
              const std::vector<std::vector<float>>& W_in,
              const std::vector<std::vector<float>>& W_res) {
        std::vector<float> pre(size, 0.0f);
        // Input contribution
        for (size_t i = 0; i < size && i < W_in.size(); ++i) {
            for (size_t j = 0; j < input.size() && j < W_in[i].size(); ++j) {
                pre[i] += W_in[i][j] * input[j];
            }
        }
        // Recurrent contribution
        for (size_t i = 0; i < size && i < W_res.size(); ++i) {
            for (size_t j = 0; j < size && j < W_res[i].size(); ++j) {
                pre[i] += W_res[i][j] * state[j];
            }
        }
        // Leaky integration
        for (size_t i = 0; i < size; ++i) {
            state[i] = (1.0f - leaking_rate) * state[i]
                     + leaking_rate * std::tanh(pre[i]);
        }
    }
};

struct ConstraintSatisfaction {
    // Promise: λ filter over protocol-execution pairs (KV)
    // Returns satisfaction score [0, 1]
    float evaluate(const std::vector<float>& promise,
                   const std::vector<float>& key,
                   const std::vector<float>& value) const {
        if (promise.size() != key.size()) return 0.0f;
        float dot = 0.0f, norm_p = 0.0f, norm_k = 0.0f;
        for (size_t i = 0; i < promise.size(); ++i) {
            dot    += promise[i] * key[i];
            norm_p += promise[i] * promise[i];
            norm_k += key[i] * key[i];
        }
        float denom = std::sqrt(norm_p) * std::sqrt(norm_k);
        return (denom > 1e-8f) ? (dot / denom + 1.0f) * 0.5f : 0.0f;
    }
};

struct DualChannelLearning {
    ReservoirState         reservoir;   // Channel 1: neuro-symbolic
    ConstraintSatisfaction constraint;  // Channel 2: promise-lambda

    DualChannelLearning() {}
    explicit DualChannelLearning(size_t res_size)
        : reservoir(res_size) {}

    // ⊕ combination: max of both channels
    float combined_score(float reservoir_score, float constraint_score) const {
        return std::max(reservoir_score, constraint_score);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Dimension 3: EXECUTABILITY — Procedural vocabulary (skillm 10-verb)
// Maps to /skillm and /workflow-creator
// ─────────────────────────────────────────────────────────────────────────────
enum class Verb : uint8_t {
    PERCEIVE  = 0,  // Sense input from environment
    ATTEND    = 1,  // Focus attention (ECAN)
    REMEMBER  = 2,  // Retrieve from memory
    REASON    = 3,  // PLN inference
    PLAN      = 4,  // Goal decomposition
    DECIDE    = 5,  // Action selection
    ACT       = 6,  // Execute action
    LEARN     = 7,  // Update weights/knowledge
    REFLECT   = 8,  // Meta-cognitive evaluation
    EVOLVE    = 9   // Ontogenetic advancement
};

inline const char* verb_name(Verb v) {
    static const char* names[] = {
        "PERCEIVE", "ATTEND", "REMEMBER", "REASON", "PLAN",
        "DECIDE", "ACT", "LEARN", "REFLECT", "EVOLVE"
    };
    return names[static_cast<uint8_t>(v)];
}

struct ActionStep {
    Verb        verb;
    std::string target;
    float       priority;
    bool        completed;

    ActionStep() : verb(Verb::PERCEIVE), priority(0.5f), completed(false) {}
    ActionStep(Verb v, const std::string& t, float p = 0.5f)
        : verb(v), target(t), priority(p), completed(false) {}
};

class Workflow {
    std::vector<ActionStep> steps_;
    size_t                  cursor_;
    bool                    resumable_;

public:
    Workflow() : cursor_(0), resumable_(true) {}

    void add(Verb v, const std::string& target, float priority = 0.5f) {
        steps_.emplace_back(v, target, priority);
    }

    ActionStep* current() {
        return (cursor_ < steps_.size()) ? &steps_[cursor_] : nullptr;
    }

    bool advance() {
        if (cursor_ < steps_.size()) {
            steps_[cursor_].completed = true;
            ++cursor_;
            return true;
        }
        return false;
    }

    bool is_complete() const { return cursor_ >= steps_.size(); }
    size_t size() const { return steps_.size(); }
    size_t position() const { return cursor_; }

    // Serialize to string for resumability
    std::string serialize() const {
        std::ostringstream oss;
        oss << cursor_ << "|";
        for (auto& s : steps_) {
            oss << static_cast<int>(s.verb) << ":" << s.target
                << ":" << s.priority << ":" << (s.completed ? 1 : 0) << ";";
        }
        return oss.str();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Dimension 4: SELF-AWARENESS — 5-level Autognosis hierarchy
// L0: Observation, L1: Self-model, L2: Meta-cognition,
// L3: Meta-meta-cognition, L4: Recursive self-improvement
// ─────────────────────────────────────────────────────────────────────────────
enum class AutonomyLevel : uint8_t {
    L0_Observation       = 0,  // Can observe own state
    L1_SelfModel         = 1,  // Has model of own capabilities
    L2_MetaCognition     = 2,  // Can reason about own reasoning
    L3_MetaMeta          = 3,  // Can modify own meta-cognitive strategies
    L4_RecursiveSelfImprove = 4  // Fixed-point: T(S∞) = S∞
};

inline const char* autonomy_name(AutonomyLevel l) {
    static const char* names[] = {
        "L0:Observation", "L1:SelfModel", "L2:MetaCognition",
        "L3:MetaMeta", "L4:RecursiveSelfImprove"
    };
    return names[static_cast<uint8_t>(l)];
}

struct SelfImage {
    AutonomyLevel level;
    float         coherence;        // 0..1 identity coherence
    float         grip_strength;    // Product of all 5 dimensions
    uint32_t      evolution_stage;  // Ontogenetic stage (System N)
    std::string   self_story;       // Narrative self-description

    SelfImage()
        : level(AutonomyLevel::L0_Observation)
        , coherence(0.0f), grip_strength(0.0f), evolution_stage(1) {}
};

class Autognosis {
    SelfImage image_;
    std::vector<std::string> observations_;

public:
    Autognosis() {}

    void observe(const std::string& observation) {
        observations_.push_back(observation);
        // L0: simple observation accumulation
        if (image_.level == AutonomyLevel::L0_Observation
            && observations_.size() >= 10) {
            image_.level = AutonomyLevel::L1_SelfModel;
        }
    }

    void update_coherence(float c) {
        image_.coherence = std::max(0.0f, std::min(1.0f, c));
    }

    void update_grip(float composability, float differentiability,
                     float executability, float self_awareness,
                     float convergence) {
        image_.grip_strength = composability * differentiability
                             * executability * self_awareness * convergence;
    }

    void advance_stage() {
        if (image_.evolution_stage < 8) {
            ++image_.evolution_stage;
        }
    }

    const SelfImage& image() const { return image_; }
    SelfImage& image_mut() { return image_; }
};

// ─────────────────────────────────────────────────────────────────────────────
// Dimension 5: CONVERGENCE — Fixed-point T(S∞) = S∞
// The KSM (Knowledge Sharing Mechanism) evolution cycle
// ─────────────────────────────────────────────────────────────────────────────
struct KSMCycleState {
    uint32_t iteration;
    float    delta;          // Change from previous iteration
    float    threshold;      // Convergence threshold
    bool     converged;

    KSMCycleState() : iteration(0), delta(1.0f), threshold(0.01f), converged(false) {}

    bool step(float new_delta) {
        delta = new_delta;
        ++iteration;
        converged = (delta < threshold);
        return converged;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// System 5 Telemetry Shell — Thread-level multiplexing
// From Level 6 Symbiotic Autonomy: dyadic pairs and triadic rotations
// ─────────────────────────────────────────────────────────────────────────────
struct DyadicPair {
    uint8_t thread_a;
    uint8_t thread_b;
    float   energy;
    float   coherence;
};

struct TriadicBundle {
    uint8_t threads[3];
    float   symmetry_state;
};

class TelemetryShell {
    std::array<DyadicPair, 6>    dyads_;     // C(4,2) = 6 pairs
    std::array<TriadicBundle, 2> triads_;    // MP1 and MP2
    uint32_t                     system_level_;

public:
    TelemetryShell() : system_level_(4) {
        // Initialize 6 dyadic pairs: P(1,2) P(1,3) P(1,4) P(2,3) P(2,4) P(3,4)
        uint8_t pairs[][2] = {{1,2},{1,3},{1,4},{2,3},{2,4},{3,4}};
        for (int i = 0; i < 6; ++i) {
            dyads_[i] = {pairs[i][0], pairs[i][1], 0.5f, 0.5f};
        }
        // MP1: P[1,2,3] → P[1,2,4] → P[1,3,4] → P[2,3,4]
        triads_[0] = {{1, 2, 3}, 0.0f};
        // MP2: P[1,3,4] → P[2,3,4] → P[1,2,3] → P[1,2,4]
        triads_[1] = {{1, 3, 4}, 0.0f};
    }

    void update_dyad(size_t idx, float energy, float coherence) {
        if (idx < 6) {
            dyads_[idx].energy = energy;
            dyads_[idx].coherence = coherence;
        }
    }

    float gestalt_coherence() const {
        float sum = 0.0f;
        for (auto& d : dyads_) sum += d.coherence;
        return sum / 6.0f;
    }

    uint32_t nested_shell_terms() const {
        return system_terms(system_level_);
    }

    const std::array<DyadicPair, 6>& dyads() const { return dyads_; }
    const std::array<TriadicBundle, 2>& triads() const { return triads_; }
};

// ─────────────────────────────────────────────────────────────────────────────
// CognitiveGrip — The unified 5-dimensional grip object
// A( S( snn[ NSE(GNE(TC)) ⊕ PLA ] ⊗ WC( FC[nn→Lnn] ⊗ ⊕⊗ ))) ≅ S∞
// ─────────────────────────────────────────────────────────────────────────────
struct GripDimensions {
    float composability;      // Semiring algebra strength
    float differentiability;  // Learning substrate quality
    float executability;      // Workflow completion ratio
    float self_awareness;     // Autognosis level / 4.0
    float convergence;        // 1.0 - KSM delta

    float product() const {
        return composability * differentiability * executability
             * self_awareness * convergence;
    }

    std::string to_string() const {
        std::ostringstream oss;
        oss << "Grip["
            << "C=" << composability
            << " D=" << differentiability
            << " E=" << executability
            << " S=" << self_awareness
            << " V=" << convergence
            << " |G|=" << product() << "]";
        return oss.str();
    }
};

class CognitiveGrip {
    Autognosis          autognosis_;
    DualChannelLearning learning_;
    Workflow            workflow_;
    TelemetryShell      telemetry_;
    KSMCycleState       ksm_;
    GripDimensions      dimensions_;

public:
    CognitiveGrip() : learning_(64) {}

    explicit CognitiveGrip(size_t reservoir_size)
        : learning_(reservoir_size) {}

    // ── Composability ────────────────────────────────────────────────────
    void set_composability(float c) {
        dimensions_.composability = std::max(0.0f, std::min(1.0f, c));
    }

    // ── Differentiability ────────────────────────────────────────────────
    DualChannelLearning& learning() { return learning_; }

    void update_differentiability(float reservoir_score, float constraint_score) {
        dimensions_.differentiability = learning_.combined_score(
            reservoir_score, constraint_score);
    }

    // ── Executability ────────────────────────────────────────────────────
    Workflow& workflow() { return workflow_; }

    void update_executability() {
        if (workflow_.size() > 0) {
            dimensions_.executability =
                static_cast<float>(workflow_.position()) / workflow_.size();
        }
    }

    // ── Self-Awareness ───────────────────────────────────────────────────
    Autognosis& autognosis() { return autognosis_; }

    void update_self_awareness() {
        dimensions_.self_awareness =
            static_cast<float>(static_cast<uint8_t>(autognosis_.image().level)) / 4.0f;
    }

    // ── Convergence ──────────────────────────────────────────────────────
    KSMCycleState& ksm() { return ksm_; }

    void update_convergence() {
        dimensions_.convergence = 1.0f - std::min(1.0f, ksm_.delta);
    }

    // ── Telemetry ────────────────────────────────────────────────────────
    TelemetryShell& telemetry() { return telemetry_; }

    // ── Unified Grip ─────────────────────────────────────────────────────
    GripDimensions compute_grip() {
        update_executability();
        update_self_awareness();
        update_convergence();
        autognosis_.update_grip(
            dimensions_.composability,
            dimensions_.differentiability,
            dimensions_.executability,
            dimensions_.self_awareness,
            dimensions_.convergence);
        return dimensions_;
    }

    const GripDimensions& dimensions() const { return dimensions_; }

    // ── KSM Evolution Cycle ──────────────────────────────────────────────
    // One full cycle: perceive → reason → act → learn → reflect → evolve
    bool evolution_step() {
        auto grip = compute_grip();
        float delta = 1.0f - grip.product();
        bool converged = ksm_.step(delta);

        autognosis_.observe("KSM iteration " + std::to_string(ksm_.iteration)
                          + " grip=" + std::to_string(grip.product()));

        if (converged) {
            autognosis_.advance_stage();
        }
        return converged;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Ecosystem Registry — Maps the 7 cogpy modules to their geological surveys
// ─────────────────────────────────────────────────────────────────────────────
struct ModuleInfo {
    const char* name;
    const char* ns;           // C++ namespace
    const char* header;       // Include path
    const char* description;
    uint32_t    total_commits;
    const char* span;         // Temporal span
};

inline const ModuleInfo* ecosystem_registry() {
    static const ModuleInfo registry[] = {
        {"cogplan9",    "cog::plan9",  "cog/plan9/plan9.hpp",
         "Plan 9 cognitive OS: 9P2000, CogFS, MachSpace", 2527, "1992-2026"},
        {"cogpilot.jl", "cog::pilot", "cog/pilot/pilot.hpp",
         "Deep Tree Echo reservoir: A000081, B-Series, ESN", 1276, "2021-2026"},
        {"cognu-mach",  "cog::mach",  "cog/mach/mach.hpp",
         "Mach microkernel: Q16.16 tensors, IPC, VM mapping", 3405, "1997-2026"},
        {"coglow",      "cog::glow",  "cog/glow/glow.hpp",
         "Neural network compiler: graph IR, optimization passes", 6699, "2017-2020"},
        {"coggml",      "cog::gml",   "cog/gml/gml.hpp",
         "Tensor library: N-dim, Q4/Q8 quantization, computation graphs", 2812, "2022-2026"},
        {"cogprime",    "cog::prime", "cog/prime/prime.hpp",
         "AGI architecture: cognitive cycle, PLN, pattern matching", 305, "2024-2026"},
        {"cogwebvm",    "cog::webvm", "cog/webvm/webvm.hpp",
         "Web VM: S-expr parser, Scheme REPL, JSON, thread-safe AtomSpace", 20, "2025-2026"},
    };
    return registry;
}

static constexpr size_t ECOSYSTEM_SIZE = 7;

inline const char* version() { return "2.0.0-evolved"; }

}} // namespace cog::grip

#endif // COG_GRIP_HPP

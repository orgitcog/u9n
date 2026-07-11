#pragma once
/**
 * @file time_crystal_neuron.hpp
 * @brief Time Crystal Neuron (TCN) model — Nanobrain Figure 6.14
 *
 * Implements the generalized neuron model with notation [a,b,c,d]:
 *   a = spatial domains (default 3: dendrite, soma, axon)
 *   b = functional layers per domain
 *   c = temporal scales per layer
 *   d = component types per scale
 *
 * Cognitive domain mapping for [3,4,3,3]:
 *   Domain 0 (Perception): sensory encoding, attention gating, feature binding
 *   Domain 1 (Processing):  integration, decision, learning, concept formation
 *   Domain 2 (Action):      working memory, meta-cognition, motor output
 *
 * Each component oscillates at its characteristic frequency and is
 * phase-coupled to adjacent scales via the CrystalBus. Components
 * within the same layer interact laterally; components across layers
 * interact vertically through the domain hierarchy.
 */

#include <opencog/temporal/types.hpp>
#include <opencog/temporal/crystal_bus.hpp>

#include <string>
#include <vector>

namespace opencog::temporal {

// ============================================================================
// TCN Component Instance
// ============================================================================

/**
 * @brief Runtime instance of a crystal component within a TCN
 *
 * Each component has:
 * - A position in the [domain, layer, scale, index] hierarchy
 * - A biological abbreviation and cognitive mapping
 * - Current phase state (tracked by the CrystalBus)
 * - Local activation that feeds back into the bus
 */
struct TCNComponent {
    // Position in hierarchy
    uint8_t domain_idx;     ///< Which spatial domain [0, a)
    uint8_t layer_idx;      ///< Which layer within domain [0, b)
    uint8_t scale_idx;      ///< Which temporal scale [0, c)
    uint8_t component_idx;  ///< Which component at this scale [0, d)

    // Identity
    std::string abbreviation;     ///< Nanobrain abbreviation (e.g., "Ax")
    std::string cognitive_label;  ///< Cognitive domain label
    TemporalScaleId scale;        ///< Temporal scale ID
    CognitiveProcess cognitive;   ///< Cognitive process mapping

    // State
    float local_activation{0.0f};  ///< [-1, +1]: component's local contribution
    float weight{1.0f};            ///< Contribution weight to layer output
};

// ============================================================================
// TCN Layer
// ============================================================================

/**
 * @brief A functional layer containing components across temporal scales
 *
 * In nn terms, this maps to an nn.Concat container — multiple scale-specific
 * modules whose outputs are concatenated into a single layer representation.
 */
struct TCNLayer {
    std::string name;
    uint8_t domain_idx;
    uint8_t layer_idx;
    std::vector<TCNComponent> components;

    /// Aggregate activation across all components in this layer
    [[nodiscard]] float aggregate_activation() const noexcept {
        if (components.empty()) return 0.0f;
        float sum = 0.0f;
        float weight_sum = 0.0f;
        for (const auto& c : components) {
            sum += c.local_activation * c.weight;
            weight_sum += c.weight;
        }
        return weight_sum > 0.0f ? sum / weight_sum : 0.0f;
    }
};

// ============================================================================
// TCN Domain
// ============================================================================

/**
 * @brief A spatial domain containing multiple functional layers
 *
 * In nn terms, this maps to an nn.Sequential container — layers
 * processed in sequence from input to output within the domain.
 */
struct TCNDomain {
    std::string name;
    uint8_t domain_idx;
    std::vector<TCNLayer> layers;

    /// Aggregate activation across all layers (sequential composition)
    [[nodiscard]] float aggregate_activation() const noexcept {
        if (layers.empty()) return 0.0f;
        // Sequential: output of last layer
        return layers.back().aggregate_activation();
    }
};

// ============================================================================
// Time Crystal Neuron
// ============================================================================

/**
 * @brief Complete Time Crystal Neuron model for the cognitive domain
 *
 * Constructs a [3,4,3,3] neuron (or custom structure) with components
 * mapped to cognitive processes. Maintains internal state via the
 * CrystalBus and provides tick-based evolution.
 *
 * Cognitive domain [3,4,3,3] = 108 total oscillator components:
 *   Domain 0 "Perception": 4 layers × 3 scales × 3 components = 36
 *   Domain 1 "Processing": 4 layers × 3 scales × 3 components = 36
 *   Domain 2 "Action":     4 layers × 3 scales × 3 components = 36
 *
 * Usage:
 *   TimeCrystalNeuron tcn("cognitive");
 *   tcn.tick(0.01f);  // Advance 10ms
 *   float coherence = tcn.global_coherence();
 */
class TimeCrystalNeuron {
public:
    /**
     * @brief Construct a TCN for the given context
     * @param context Domain name (e.g., "cognitive", "language", "financial")
     * @param structure [a,b,c,d] parameters (default: [3,4,3,3])
     */
    explicit TimeCrystalNeuron(std::string context = "cognitive",
                                TCNStructure structure = TCNStructure::default_neuron()) noexcept
        : context_(std::move(context))
        , structure_(structure)
        , bus_(CrystalBusConfig{.tick_interval_ms = 50.0f, .global_coupling = 0.5f}) {
        build_model();
    }

    // ---- Model access ----

    [[nodiscard]] const std::string& context() const noexcept { return context_; }
    [[nodiscard]] TCNStructure structure() const noexcept { return structure_; }
    [[nodiscard]] uint32_t total_components() const noexcept { return structure_.total_oscillators(); }

    [[nodiscard]] const std::vector<TCNDomain>& domains() const noexcept { return domains_; }
    [[nodiscard]] const CrystalBus& bus() const noexcept { return bus_; }
    [[nodiscard]] CrystalBus& bus() noexcept { return bus_; }

    // ---- State access ----

    /// Current oscillator state snapshot
    [[nodiscard]] OscillatorState state() const noexcept {
        return bus_.snapshot();
    }

    /// Global phase coherence (0 = desynchronized, 1 = fully synchronized)
    [[nodiscard]] float global_coherence() const noexcept {
        return bus_.global_coherence();
    }

    /// Hierarchical coherence (Universal vs Particular sets)
    [[nodiscard]] float hierarchical_coherence() const noexcept {
        return bus_.hierarchical_coherence();
    }

    /// Domain-specific activation
    [[nodiscard]] float domain_activation(uint8_t domain_idx) const noexcept {
        if (domain_idx < domains_.size()) {
            return domains_[domain_idx].aggregate_activation();
        }
        return 0.0f;
    }

    // ---- Tick / evolution ----

    /**
     * @brief Advance the neuron model by one time step
     *
     * 1. Update CrystalBus (phase evolution + coupling)
     * 2. Read bus activations into component local_activation
     * 3. Apply component-level interactions (lateral within layer)
     * 4. Components write back perturbations to bus
     */
    void tick(float dt) noexcept {
        // Phase 1: Bus evolution
        bus_.tick(dt);

        // Phase 2: Read activations from bus into components
        for (auto& domain : domains_) {
            for (auto& layer : domain.layers) {
                for (auto& comp : layer.components) {
                    comp.local_activation = bus_.activation(comp.scale);
                }
            }
        }

        // Phase 3: Lateral interactions within layers
        // (simplified: weighted average with neighbors)
        for (auto& domain : domains_) {
            for (auto& layer : domain.layers) {
                if (layer.components.size() < 2) continue;
                float layer_mean = layer.aggregate_activation();
                for (auto& comp : layer.components) {
                    // Blend toward layer mean (lateral coupling)
                    comp.local_activation = comp.local_activation * 0.8f +
                                           layer_mean * 0.2f;
                }
            }
        }

        ++tick_count_;
    }

    /// Inject stimulus at a specific cognitive process
    void stimulate(CognitiveProcess process, float intensity = 0.5f) noexcept {
        auto scale = static_cast<TemporalScaleId>(static_cast<uint8_t>(process));
        bus_.inject(scale, 0.0f, intensity);
    }

    // ---- Classification ----

    /// Which cognitive process is currently most active
    [[nodiscard]] CognitiveProcess dominant_process() const noexcept {
        auto state = bus_.snapshot();
        float max_val = 0.0f;
        size_t max_idx = 0;
        for (size_t i = 0; i < BIOLOGICAL_SCALE_COUNT; ++i) {
            float v = std::abs(state.activations[i]);
            if (v > max_val) {
                max_val = v;
                max_idx = i;
            }
        }
        return static_cast<CognitiveProcess>(max_idx);
    }

    /// Whether Universal Sets (global state) dominate Particular Sets (local)
    [[nodiscard]] bool is_global_dominant() const noexcept {
        auto state = bus_.snapshot();
        return state.universal_energy() > state.particular_energy();
    }

    [[nodiscard]] uint64_t tick_count() const noexcept { return tick_count_; }

    // ---- sys-n classification ----

    /// Get Universal Set component abbreviations (slow oscillators >= 0.5s)
    [[nodiscard]] std::vector<std::string> universal_sets() const noexcept {
        return universal_sets_;
    }

    /// Get Particular Set component abbreviations (fast oscillators < 0.5s)
    [[nodiscard]] std::vector<std::string> particular_sets() const noexcept {
        return particular_sets_;
    }

private:
    void build_model() noexcept {
        // Domain names for cognitive context
        static constexpr std::string_view COGNITIVE_DOMAINS[] = {
            "Perception", "Processing", "Action"
        };
        static constexpr std::string_view GENERIC_DOMAIN_PREFIX = "Domain_";

        // Cognitive component mappings per scale (for cognitive context)
        // Format: [scale_index][component_index] = {abbreviation, cognitive_label}
        struct CompMapping {
            std::string_view abbrev;
            std::string_view label;
        };

        static constexpr CompMapping COGNITIVE_COMPONENTS[9][3] = {
            // ULTRA_FAST (8ms)
            {{"Ax", "SignalEncoder"}, {"Pr-Ch(3)", "PatternDetector"}, {"Io-Ch", "SpikeGenerator"}},
            // FAST (26ms)
            {{"Io-Ch", "AttentionGate"}, {"Li", "NoveltyFilter"}, {"Ax", "RelevanceFilter"}},
            // MEDIUM_FAST (52ms)
            {{"Me", "FeatureBinder"}, {"Ac", "ContextMerger"}, {"Li", "ModalityIntegrator"}},
            // MEDIUM (110ms)
            {{"AIS[3]", "DecisionThreshold"}, {"An-n", "PrioritySelector"}, {"En-re-Mi", "ConflictResolver"}},
            // MEDIUM_SLOW (160ms)
            {{"Ch-Co", "ContextAssembler"}, {"Ac", "SchemaActivator"}, {"PNN", "FrameBuilder"}},
            // SLOW (250ms)
            {{"Fi-lo", "HebbianUpdater"}, {"Ax-d", "ErrorCorrector"}, {"Ca", "WeightAdjuster"}},
            // VERY_SLOW (330ms)
            {{"Soma", "ConceptFormer"}, {"Rh", "AbstractionEngine"}, {"Ep", "CategoryBuilder"}},
            // ULTRA_SLOW (500ms)
            {{"Bu", "WorkingMemBuffer"}, {"Nt", "AttentionSustainer"}, {"El", "GoalMaintainer"}},
            // SLOWEST (1s)
            {{"Me-Rh", "MetaCognitor"}, {"Bi", "StrategySelector"}, {"Sy-c", "GlobalCoherence"}}
        };

        domains_.clear();
        universal_sets_.clear();
        particular_sets_.clear();

        uint8_t a = structure_.domains;
        uint8_t b = structure_.layers;
        uint8_t c = structure_.scales;
        uint8_t d = structure_.components;

        for (uint8_t di = 0; di < a; ++di) {
            TCNDomain domain;
            if (di < 3) {
                domain.name = std::string(COGNITIVE_DOMAINS[di]);
            } else {
                domain.name = std::string(GENERIC_DOMAIN_PREFIX) + std::to_string(di);
            }
            domain.domain_idx = di;

            for (uint8_t li = 0; li < b; ++li) {
                TCNLayer layer;
                layer.name = "Layer_" + std::to_string(li);
                layer.domain_idx = di;
                layer.layer_idx = li;

                for (uint8_t si = 0; si < c; ++si) {
                    // Map to biological scale (wrap around if needed)
                    size_t scale_offset = (di * b + li) % BIOLOGICAL_SCALE_COUNT;
                    size_t scale_idx = (scale_offset + si) % BIOLOGICAL_SCALE_COUNT;
                    auto scale = static_cast<TemporalScaleId>(scale_idx);

                    for (uint8_t ci = 0; ci < d && ci < 3; ++ci) {
                        TCNComponent comp;
                        comp.domain_idx = di;
                        comp.layer_idx = li;
                        comp.scale_idx = si;
                        comp.component_idx = ci;
                        comp.scale = scale;
                        comp.cognitive = static_cast<CognitiveProcess>(scale_idx);

                        // Use cognitive mappings
                        if (scale_idx < 9 && ci < 3) {
                            comp.abbreviation = std::string(COGNITIVE_COMPONENTS[scale_idx][ci].abbrev);
                            comp.cognitive_label = std::string(COGNITIVE_COMPONENTS[scale_idx][ci].label);
                        } else {
                            comp.abbreviation = "C" + std::to_string(scale_idx) + "_" + std::to_string(ci);
                            comp.cognitive_label = "Component_" + std::to_string(scale_idx);
                        }

                        // sys-n classification
                        if (is_universal_set(scale)) {
                            if (std::find(universal_sets_.begin(), universal_sets_.end(),
                                         comp.abbreviation) == universal_sets_.end()) {
                                universal_sets_.push_back(comp.abbreviation);
                            }
                        } else if (is_particular_set(scale)) {
                            if (std::find(particular_sets_.begin(), particular_sets_.end(),
                                         comp.abbreviation) == particular_sets_.end()) {
                                particular_sets_.push_back(comp.abbreviation);
                            }
                        }

                        layer.components.push_back(std::move(comp));
                    }
                }
                domain.layers.push_back(std::move(layer));
            }
            domains_.push_back(std::move(domain));
        }
    }

    std::string context_;
    TCNStructure structure_;
    CrystalBus bus_;
    std::vector<TCNDomain> domains_;

    // sys-n classification
    std::vector<std::string> universal_sets_;
    std::vector<std::string> particular_sets_;

    uint64_t tick_count_{0};
};

} // namespace opencog::temporal

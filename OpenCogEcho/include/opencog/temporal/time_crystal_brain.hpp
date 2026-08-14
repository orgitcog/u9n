#pragma once
/**
 * @file time_crystal_brain.hpp
 * @brief Time Crystal Neural Network Brain (TCNN) — Nanobrain Figure 7.15
 *
 * Extends the single-neuron TCN model to whole-brain architecture with
 * 12 hierarchical levels, 8 brain regions, and 7 functional subsystems.
 *
 * Cognitive domain mapping:
 *   Level 1  (Microtubule)      → AtomRepresentation (primitive symbol encoding)
 *   Level 2  (Neuron)           → ConceptUnit (single concept processing via TCN)
 *   Level 3  (CorticalBranches) → ConceptColumn (conceptual clustering)
 *   Level 4  (CortexDomain)     → ReasoningDomain (PLN inference regions)
 *   Level 5  (Cerebellum)       → PredictionEngine (timing/prediction via Cerebellum)
 *   Level 6  (Hypothalamus)     → HomeostaticRegulator (VES coupling)
 *   Level 7  (Hippocampus)      → MemorySystem (valence memory encoding/recall)
 *   Level 8  (ThalamicBody)     → AttentionRelay (ECAN attention gating)
 *   Level 9  (SkinNerveNet)     → PeripheralSensing (touchpad/input)
 *   Level 10 (CranialNerve)     → IOChannel (NPU, o9c2 I/O pathways)
 *   Level 11 (ThoracicNerve)    → ReflexProcessor (VNS reflex arcs)
 *   Level 12 (BloodVessel)      → ResourceFlow (energy/compute management)
 *
 * The brain model contains TimeCrystalNeuron instances at Level 2 (the
 * cellular level), with higher levels organizing these into regions
 * and subsystems.
 */

#include <opencog/temporal/types.hpp>
#include <opencog/temporal/crystal_bus.hpp>
#include <opencog/temporal/time_crystal_neuron.hpp>

#include <string>
#include <vector>
#include <memory>

namespace opencog::temporal {

// ============================================================================
// Brain Component — Element within a brain region
// ============================================================================

/**
 * @brief A component within a specific brain region at a hierarchy level
 *
 * Each component has a Nanobrain abbreviation, a period, and a cognitive
 * domain mapping. At Level 2 (Neuron), components contain a full TCN.
 */
struct BrainComponent {
    std::string abbreviation;       ///< Nanobrain abbreviation
    std::string full_name;          ///< Human-readable name
    float period;                   ///< Characteristic oscillation period (seconds)
    HierarchyLevel level;           ///< Hierarchy level
    std::string cognitive_label;    ///< Cognitive domain mapping
    std::string role;               ///< Functional role (for subsystem components)
    float activation{0.0f};         ///< Current activation [-1, +1]
};

// ============================================================================
// Brain Region — Anatomical region with components
// ============================================================================

/**
 * @brief A brain region from the TCNN hierarchy
 *
 * Contains components specific to its hierarchy level, with cognitive
 * domain mappings for the opencog-modern architecture.
 */
struct BrainRegion {
    std::string name;
    BrainRegionId id;
    HierarchyLevel level;
    std::string scale;              ///< Spatial scale category
    std::string cognitive_label;    ///< Cognitive domain mapping
    std::vector<BrainComponent> components;

    /// Mean activation across all components
    [[nodiscard]] float mean_activation() const noexcept {
        if (components.empty()) return 0.0f;
        float sum = 0.0f;
        for (const auto& c : components) sum += std::abs(c.activation);
        return sum / static_cast<float>(components.size());
    }
};

// ============================================================================
// Brain Subsystem — Functional subsystem spanning regions
// ============================================================================

/**
 * @brief A functional subsystem from the TCNN model
 *
 * Subsystems cross brain region boundaries, implementing distributed
 * functional circuits (e.g., proprioception spans spinal cord, thalamus,
 * cerebellum, and cortex).
 */
struct BrainSubsystem {
    std::string name;
    SubsystemId id;
    std::string abbreviation;
    std::string cognitive_label;    ///< Cognitive domain mapping
    std::vector<BrainComponent> components;

    /// Mean activation across subsystem components
    [[nodiscard]] float mean_activation() const noexcept {
        if (components.empty()) return 0.0f;
        float sum = 0.0f;
        for (const auto& c : components) sum += std::abs(c.activation);
        return sum / static_cast<float>(components.size());
    }
};

// ============================================================================
// Time Crystal Brain
// ============================================================================

/**
 * @brief Complete Time Crystal Brain model for the cognitive domain
 *
 * Integrates 12 hierarchy levels, 8 brain regions, and 7 functional
 * subsystems into a unified model with cognitive domain mappings.
 * Contains a CrystalBus for multi-scale oscillation and references
 * to embedded TCN instances at the neuron level.
 *
 * The brain model serves as the structural scaffold connecting:
 * - VES (hormone bus) at Hypothalamus level (L6)
 * - VNS (nerve bus) at Thalamic/Brainstem levels (L8, L11)
 * - ECAN (attention) at Thalamic level (L8)
 * - PLN (reasoning) at Cortex level (L4)
 * - AtomSpace at Microtubule level (L1)
 *
 * Usage:
 *   TimeCrystalBrain brain;
 *   brain.tick(0.01f);
 *   float coherence = brain.global_coherence();
 */
class TimeCrystalBrain {
public:
    explicit TimeCrystalBrain() noexcept
        : bus_(CrystalBusConfig{.tick_interval_ms = 50.0f, .global_coupling = 0.4f}) {
        build_regions();
        build_subsystems();
    }

    // ---- Model access ----

    [[nodiscard]] const std::vector<BrainRegion>& regions() const noexcept { return regions_; }
    [[nodiscard]] const std::vector<BrainSubsystem>& subsystems() const noexcept { return subsystems_; }
    [[nodiscard]] const CrystalBus& bus() const noexcept { return bus_; }
    [[nodiscard]] CrystalBus& bus() noexcept { return bus_; }

    [[nodiscard]] size_t total_components() const noexcept {
        size_t count = 0;
        for (const auto& r : regions_) count += r.components.size();
        return count;
    }

    // ---- Region access ----

    /// Find region by ID
    [[nodiscard]] const BrainRegion* region(BrainRegionId id) const noexcept {
        for (const auto& r : regions_) {
            if (r.id == id) return &r;
        }
        return nullptr;
    }

    /// Find subsystem by ID
    [[nodiscard]] const BrainSubsystem* subsystem(SubsystemId id) const noexcept {
        for (const auto& s : subsystems_) {
            if (s.id == id) return &s;
        }
        return nullptr;
    }

    // ---- Tick / evolution ----

    /**
     * @brief Advance the brain model by one time step
     *
     * 1. Advance CrystalBus (phase evolution)
     * 2. Update region component activations from bus
     * 3. Cross-region interactions (hierarchy coupling)
     * 4. Subsystem integration
     */
    void tick(float dt) noexcept {
        // Phase 1: Bus evolution
        bus_.tick(dt);

        // Phase 2: Map bus activations to region components
        auto state = bus_.snapshot();
        for (auto& region : regions_) {
            for (auto& comp : region.components) {
                // Map component period to closest temporal scale
                auto scale = period_to_scale(comp.period);
                comp.activation = state[scale];
            }
        }

        // Phase 3: Subsystem components get blended from regions
        for (auto& subsys : subsystems_) {
            for (auto& comp : subsys.components) {
                auto scale = period_to_scale(comp.period);
                comp.activation = state[scale];
            }
        }

        ++tick_count_;
    }

    // ---- Metrics ----

    /// Global coherence across all temporal scales
    [[nodiscard]] float global_coherence() const noexcept {
        return bus_.global_coherence();
    }

    /// Hierarchical coherence (Universal vs Particular sets)
    [[nodiscard]] float hierarchical_coherence() const noexcept {
        return bus_.hierarchical_coherence();
    }

    /// Region-level activation summary
    [[nodiscard]] float region_activation(BrainRegionId id) const noexcept {
        auto* r = region(id);
        return r ? r->mean_activation() : 0.0f;
    }

    /// Subsystem-level activation summary
    [[nodiscard]] float subsystem_activation(SubsystemId id) const noexcept {
        auto* s = subsystem(id);
        return s ? s->mean_activation() : 0.0f;
    }

    [[nodiscard]] uint64_t tick_count() const noexcept { return tick_count_; }

private:
    /// Map a component's period to the closest temporal scale
    [[nodiscard]] static TemporalScaleId period_to_scale(float period) noexcept {
        float min_diff = 1e6f;
        size_t best = 0;
        for (size_t i = 0; i < BIOLOGICAL_SCALE_COUNT; ++i) {
            float diff = std::abs(period - SCALE_PERIODS[i]);
            if (diff < min_diff) {
                min_diff = diff;
                best = i;
            }
        }
        return static_cast<TemporalScaleId>(best);
    }

    void build_regions() noexcept {
        regions_.clear();

        // Region 0: Microtubule (Level 1, molecular)
        {
            BrainRegion r;
            r.name = "Microtubule"; r.id = BrainRegionId::MICROTUBULE;
            r.level = HierarchyLevel::MICROTUBULE; r.scale = "molecular";
            r.cognitive_label = "AtomRepresentation";
            r.components = {
                {"Dub-bell", "Dumbbell shape",  0.001f, HierarchyLevel::MICROTUBULE, "SymbolGeometry", "", 0.0f},
                {"Spiral",   "Spiral structure", 0.001f, HierarchyLevel::MICROTUBULE, "HelicalEncoding", "", 0.0f},
                {"Helix",    "Alpha helix",      0.002f, HierarchyLevel::MICROTUBULE, "ProteinDynamics", "", 0.0f},
                {"Pitch",    "Helix pitch",      0.002f, HierarchyLevel::MICROTUBULE, "SpatialPeriod", "", 0.0f},
                {"H2O-ext",  "External water",   0.003f, HierarchyLevel::MICROTUBULE, "InterfaceLayer", "", 0.0f},
                {"H2O-int",  "Internal water",   0.003f, HierarchyLevel::MICROTUBULE, "CavityState", "", 0.0f},
                {"Topo",     "Topology",         0.004f, HierarchyLevel::MICROTUBULE, "TopologyMap", "", 0.0f},
                {"Lattice",  "Surface lattice",  0.005f, HierarchyLevel::MICROTUBULE, "LatticePattern", "", 0.0f},
            };
            regions_.push_back(std::move(r));
        }

        // Region 1: Cortex Domain (Level 4, regional)
        {
            BrainRegion r;
            r.name = "CortexDomain"; r.id = BrainRegionId::CORTEX_DOMAIN;
            r.level = HierarchyLevel::CORTEX_DOMAIN; r.scale = "regional";
            r.cognitive_label = "ReasoningDomain";
            r.components = {
                {"A",     "Occipital lobe",     0.1f,  HierarchyLevel::CORTEX_DOMAIN, "VisualReasoning", "", 0.0f},
                {"B",     "Frontal lobe",       0.1f,  HierarchyLevel::CORTEX_DOMAIN, "ExecutiveFunction", "", 0.0f},
                {"C",     "Temporal lobe",      0.1f,  HierarchyLevel::CORTEX_DOMAIN, "SequenceReasoning", "", 0.0f},
                {"D",     "Parietal lobe",      0.1f,  HierarchyLevel::CORTEX_DOMAIN, "SpatialReasoning", "", 0.0f},
                {"E(6)",  "Neurological func",  0.12f, HierarchyLevel::CORTEX_DOMAIN, "FunctionalNuclei", "", 0.0f},
                {"F(6)",  "Lateral Brodmann",   0.12f, HierarchyLevel::CORTEX_DOMAIN, "LateralAreas", "", 0.0f},
                {"G(6)",  "Medial Brodmann",    0.12f, HierarchyLevel::CORTEX_DOMAIN, "MedialAreas", "", 0.0f},
                {"H(6)",  "Gyrus nuclei",       0.12f, HierarchyLevel::CORTEX_DOMAIN, "GyralOrganization", "", 0.0f},
                {"Vc",    "Visual cortex",      0.08f, HierarchyLevel::CORTEX_DOMAIN, "PatternRecognition", "", 0.0f},
                {"Mc",    "Motor cortex",       0.08f, HierarchyLevel::CORTEX_DOMAIN, "ActionPlanning", "", 0.0f},
                {"Sc",    "Somatosensory ctx",  0.08f, HierarchyLevel::CORTEX_DOMAIN, "BodyModel", "", 0.0f},
                {"Hp(6)", "Hippocampus",        0.15f, HierarchyLevel::CORTEX_DOMAIN, "EpisodicMemory", "", 0.0f},
                {"Ic",    "Insula cortex",      0.1f,  HierarchyLevel::CORTEX_DOMAIN, "Interoception", "", 0.0f},
            };
            regions_.push_back(std::move(r));
        }

        // Region 2: Cerebellum (Level 5, organ)
        {
            BrainRegion r;
            r.name = "Cerebellum"; r.id = BrainRegionId::CEREBELLUM;
            r.level = HierarchyLevel::CEREBELLUM; r.scale = "organ";
            r.cognitive_label = "PredictionEngine";
            r.components = {
                {"M", "Mid lobe",      0.2f,  HierarchyLevel::CEREBELLUM, "BalancePrediction", "", 0.0f},
                {"L", "Left lobe",     0.2f,  HierarchyLevel::CEREBELLUM, "MotorTiming_L", "", 0.0f},
                {"R", "Right lobe",    0.2f,  HierarchyLevel::CEREBELLUM, "MotorTiming_R", "", 0.0f},
                {"A", "PostQuadLob",   0.22f, HierarchyLevel::CEREBELLUM, "SequencePrediction", "", 0.0f},
                {"B", "AntLobule",     0.22f, HierarchyLevel::CEREBELLUM, "ErrorPrediction", "", 0.0f},
                {"C", "Folia",         0.18f, HierarchyLevel::CEREBELLUM, "ParallelFibers", "", 0.0f},
                {"D", "HorizFissure",  0.25f, HierarchyLevel::CEREBELLUM, "LobeDivision", "", 0.0f},
                {"E", "MiddleLob",     0.22f, HierarchyLevel::CEREBELLUM, "CoordinationCenter", "", 0.0f},
                {"F", "DorsolatFiss",  0.25f, HierarchyLevel::CEREBELLUM, "LateralBoundary", "", 0.0f},
                {"G", "PostlunateFiss",0.25f, HierarchyLevel::CEREBELLUM, "PosteriorBound", "", 0.0f},
                {"H", "InfSemilunar",  0.22f, HierarchyLevel::CEREBELLUM, "SpatialPredict", "", 0.0f},
                {"I", "Tonsil",        0.2f,  HierarchyLevel::CEREBELLUM, "VestibularPredict", "", 0.0f},
            };
            regions_.push_back(std::move(r));
        }

        // Region 3: Hypothalamus (Level 6, nuclear)
        {
            BrainRegion r;
            r.name = "Hypothalamus"; r.id = BrainRegionId::HYPOTHALAMUS;
            r.level = HierarchyLevel::HYPOTHALAMUS; r.scale = "nuclear";
            r.cognitive_label = "HomeostaticRegulator";
            r.components = {
                {"Lim",     "Limbic system",    0.3f,  HierarchyLevel::HYPOTHALAMUS, "EmotionRegulation", "", 0.0f},
                {"Mam",     "Mammillothalamic", 0.35f, HierarchyLevel::HYPOTHALAMUS, "MemoryPathway", "", 0.0f},
                {"HR",      "Heart rate",       0.8f,  HierarchyLevel::HYPOTHALAMUS, "ResourcePace", "", 0.0f},
                {"BP",      "Blood pressure",   1.0f,  HierarchyLevel::HYPOTHALAMUS, "ResourcePressure", "", 0.0f},
                {"Oxy",     "Oxytocin",         0.5f,  HierarchyLevel::HYPOTHALAMUS, "SocialBonding", "", 0.0f},
                {"Ant-pr",  "Anterior preoptic",0.3f,  HierarchyLevel::HYPOTHALAMUS, "TemperatureCtrl", "", 0.0f},
                {"Post-pr", "Posterior preopt", 0.3f,  HierarchyLevel::HYPOTHALAMUS, "SleepWake", "", 0.0f},
                {"Arc",     "Arcuate nucleus",  0.4f,  HierarchyLevel::HYPOTHALAMUS, "HormoneRegulation", "", 0.0f},
                {"Sup",     "Supraoptic",       0.35f, HierarchyLevel::HYPOTHALAMUS, "FluidBalance", "", 0.0f},
                {"Para",    "Paraventricular",  0.35f, HierarchyLevel::HYPOTHALAMUS, "StressResponse", "", 0.0f},
            };
            regions_.push_back(std::move(r));
        }

        // Region 4: Hippocampus (Level 7, nuclear)
        {
            BrainRegion r;
            r.name = "Hippocampus"; r.id = BrainRegionId::HIPPOCAMPUS;
            r.level = HierarchyLevel::HIPPOCAMPUS; r.scale = "nuclear";
            r.cognitive_label = "MemorySystem";
            r.components = {
                {"CAIV",     "CA4 region",         0.4f,  HierarchyLevel::HIPPOCAMPUS, "DentateHilus", "", 0.0f},
                {"CAII",     "CA2 region",         0.4f,  HierarchyLevel::HIPPOCAMPUS, "IntermediateRelay", "", 0.0f},
                {"CAI",      "CA1 region",         0.4f,  HierarchyLevel::HIPPOCAMPUS, "MemoryOutput", "", 0.0f},
                {"U-trap",   "U-shaped trap",      0.45f, HierarchyLevel::HIPPOCAMPUS, "SpatialEncoding", "", 0.0f},
                {"V-trap",   "V-shaped trap",      0.45f, HierarchyLevel::HIPPOCAMPUS, "TemporalEncoding", "", 0.0f},
                {"Lat-ant",  "Lat anterior nuc",   0.42f, HierarchyLevel::HIPPOCAMPUS, "SpatialMemory", "", 0.0f},
                {"Lat-post", "Lat posterior nuc",  0.42f, HierarchyLevel::HIPPOCAMPUS, "ContextMemory", "", 0.0f},
                {"Intra",    "Intralaminar nuc",   0.38f, HierarchyLevel::HIPPOCAMPUS, "ArousalGating", "", 0.0f},
                {"Ret",      "Reticular nucleus",  0.38f, HierarchyLevel::HIPPOCAMPUS, "MemoryGating", "", 0.0f},
            };
            regions_.push_back(std::move(r));
        }

        // Region 5: Thalamic Body (Level 8, relay)
        {
            BrainRegion r;
            r.name = "ThalamicBody"; r.id = BrainRegionId::THALAMIC_BODY;
            r.level = HierarchyLevel::THALAMIC_BODY; r.scale = "relay";
            r.cognitive_label = "AttentionRelay";
            r.components = {
                {"VM",       "Ventromedial nuc",  0.5f,  HierarchyLevel::THALAMIC_BODY, "MotorRelay", "", 0.0f},
                {"SC",       "Supra chiasmatic",  24.0f, HierarchyLevel::THALAMIC_BODY, "CircadianClock", "", 0.0f},
                {"Ant",      "Anterior nucleus",  0.5f,  HierarchyLevel::THALAMIC_BODY, "MemoryRelay", "", 0.0f},
                {"Post",     "Posterior nucleus",  0.5f,  HierarchyLevel::THALAMIC_BODY, "SensoryRelay", "", 0.0f},
                {"Med",      "Medial nuclei",     0.55f, HierarchyLevel::THALAMIC_BODY, "LimbicRelay", "", 0.0f},
                {"Lat",      "Lateral nuclei",    0.55f, HierarchyLevel::THALAMIC_BODY, "CorticalRelay", "", 0.0f},
                {"Med-dor",  "Medial dorsal",     0.52f, HierarchyLevel::THALAMIC_BODY, "ExecutiveRelay", "", 0.0f},
                {"Lat-post", "Lateral posterior",  0.52f, HierarchyLevel::THALAMIC_BODY, "VisualRelay", "", 0.0f},
            };
            regions_.push_back(std::move(r));
        }

        // Region 6: Cranial Nerve (Level 10, peripheral)
        {
            BrainRegion r;
            r.name = "CranialNerve"; r.id = BrainRegionId::CRANIAL_NERVE;
            r.level = HierarchyLevel::CRANIAL_NERVE; r.scale = "peripheral";
            r.cognitive_label = "IOChannel";
            r.components = {
                {"Olf", "Olfactory",         0.8f,   HierarchyLevel::CRANIAL_NERVE, "ChemicalInput", "", 0.0f},
                {"Opt", "Optic",             0.016f, HierarchyLevel::CRANIAL_NERVE, "VisualInput", "", 0.0f},
                {"Ocm", "Oculomotor",        0.1f,   HierarchyLevel::CRANIAL_NERVE, "GazeControl", "", 0.0f},
                {"Tro", "Trochlear",         0.1f,   HierarchyLevel::CRANIAL_NERVE, "EyeTracking", "", 0.0f},
                {"Tri", "Trigeminal",        0.05f,  HierarchyLevel::CRANIAL_NERVE, "FaceSensation", "", 0.0f},
                {"Abd", "Abducens",          0.1f,   HierarchyLevel::CRANIAL_NERVE, "LateralGaze", "", 0.0f},
                {"Fac", "Facial",            0.2f,   HierarchyLevel::CRANIAL_NERVE, "Expression", "", 0.0f},
                {"Ves", "Vestibulocochlear", 0.001f, HierarchyLevel::CRANIAL_NERVE, "BalanceAuditory", "", 0.0f},
                {"Glo", "Glossopharyngeal",  0.3f,   HierarchyLevel::CRANIAL_NERVE, "TasteSwallow", "", 0.0f},
                {"Vag", "Vagus",             0.8f,   HierarchyLevel::CRANIAL_NERVE, "AutonomicIO", "", 0.0f},
                {"Acc", "Accessory",         0.5f,   HierarchyLevel::CRANIAL_NERVE, "NeckMotor", "", 0.0f},
                {"Hyp", "Hypoglossal",       0.2f,   HierarchyLevel::CRANIAL_NERVE, "TongueMotor", "", 0.0f},
            };
            regions_.push_back(std::move(r));
        }

        // Region 7: Blood Vessel (Level 12, vascular)
        {
            BrainRegion r;
            r.name = "BloodVessel"; r.id = BrainRegionId::BLOOD_VESSEL;
            r.level = HierarchyLevel::BLOOD_VESSEL; r.scale = "vascular";
            r.cognitive_label = "ResourceFlow";
            r.components = {
                {"Sup-cer",  "Sup cerebral artery",  1.0f, HierarchyLevel::BLOOD_VESSEL, "CorticalSupply", "", 0.0f},
                {"Mid-cer",  "Mid cerebral artery",  1.0f, HierarchyLevel::BLOOD_VESSEL, "LateralSupply", "", 0.0f},
                {"Ant-spi",  "Ant spinal artery",    1.0f, HierarchyLevel::BLOOD_VESSEL, "SpinalSupply", "", 0.0f},
                {"Post-cer", "Post cerebral artery", 1.0f, HierarchyLevel::BLOOD_VESSEL, "PosteriorSupply", "", 0.0f},
                {"Bas",      "Basilar artery",       1.0f, HierarchyLevel::BLOOD_VESSEL, "BrainstemSupply", "", 0.0f},
                {"Pon",      "Pontine arteries",     1.0f, HierarchyLevel::BLOOD_VESSEL, "PontineSupply", "", 0.0f},
            };
            regions_.push_back(std::move(r));
        }
    }

    void build_subsystems() noexcept {
        subsystems_.clear();

        // Subsystem 0: Proprioception
        {
            BrainSubsystem s;
            s.name = "Proprioception"; s.id = SubsystemId::PROPRIOCEPTION;
            s.abbreviation = "Pr"; s.cognitive_label = "BodyAwareness";
            s.components = {
                {"Sc",    "Spinal cord",             0.05f, {}, "CentralPathway",  "pathway", 0.0f},
                {"Th",    "Thalamus",                0.5f,  {}, "SensoryRelay",    "relay",   0.0f},
                {"Cb",    "Cerebellum",              0.2f,  {}, "Coordination",    "coord",   0.0f},
                {"Pc",    "Proprioception cell",     0.02f, {}, "PositionSensor",  "sensor",  0.0f},
                {"Se(4)", "Sensor quartet",          0.01f, {}, "MultiModalInput", "input",   0.0f},
                {"Vpn",   "Ventral posterior nuclei",0.5f,  {}, "ThalamicRelay",   "relay",   0.0f},
                {"D[3]",  "Dorsal triad",            0.3f,  {}, "DorsalPathway",   "pathway", 0.0f},
                {"K[3]",  "Kinesthesia triad",       0.1f,  {}, "KinestheticSense","sense",   0.0f},
                {"Ex[4]", "External haptic",         0.05f, {}, "HapticInput",     "input",   0.0f},
            };
            subsystems_.push_back(std::move(s));
        }

        // Subsystem 1: Homeostatic
        {
            BrainSubsystem s;
            s.name = "Homeostatic"; s.id = SubsystemId::HOMEOSTATIC;
            s.abbreviation = "HoS"; s.cognitive_label = "ResourceManagement";
            s.components = {
                {"HoS",    "Homeostatic",       1.0f,  {}, "SystemBalance",    "system",  0.0f},
                {"Fb",     "Feedback",           0.5f,  {}, "ControlLoop",      "control", 0.0f},
                {"C*[4]",  "Brain stem quartet", 0.3f,  {}, "BrainstemCtrl",    "integr",  0.0f},
                {"Fls[4]", "Feedback local TC",  0.8f,  {}, "LocalRegulation",  "reg",     0.0f},
                {"B*[3]",  "Motor triad",        0.2f,  {}, "MotorOutput",      "output",  0.0f},
                {"Ac[5]",  "Activated quintet",  0.05f, {}, "SensoryInput",     "input",   0.0f},
                {"V[3]",   "Voluntary triad",    0.15f, {}, "VoluntaryCtrl",    "control", 0.0f},
            };
            subsystems_.push_back(std::move(s));
        }

        // Subsystem 2: Emotion/Personality
        {
            BrainSubsystem s;
            s.name = "Emotion"; s.id = SubsystemId::EMOTION;
            s.abbreviation = "Em/Pe"; s.cognitive_label = "AffectiveProcessing";
            s.components = {
                {"Em",  "Emotion",           0.5f,    {}, "AffectiveState",     "state",   0.0f},
                {"Tc",  "Time crystal",      0.33f,   {}, "TemporalPattern",    "pattern", 0.0f},
                {"Pe",  "Personality",       86400.0f, {}, "TraitDynamics",      "trait",   0.0f},
                {"Tha", "Thalamus",          0.5f,    {}, "EmotionIntegration", "integr",  0.0f},
                {"In",  "Insula",            0.3f,    {}, "Interoception",      "sense",   0.0f},
                {"Ci",  "Cingulate",         0.4f,    {}, "ConflictMonitor",    "monitor", 0.0f},
                {"St",  "Striatum",          0.2f,    {}, "RewardProcessing",   "reward",  0.0f},
                {"ACC", "Anterior cingulate", 0.35f,   {}, "ExecutiveEmotion",   "exec",    0.0f},
                {"PTC", "Personality TC",    86400.0f, {}, "PersonalityDynamics","dynamics",0.0f},
            };
            subsystems_.push_back(std::move(s));
        }

        // Subsystem 3: Olfactory
        {
            BrainSubsystem s;
            s.name = "Olfactory"; s.id = SubsystemId::OLFACTORY;
            s.abbreviation = "Ols"; s.cognitive_label = "ChemicalSensing";
            s.components = {
                {"Ols(2)", "Olfactory system",  0.5f, {}, "OlfactoryProcess", "system", 0.0f},
                {"N1(6)",  "Nose1 sensors",     0.1f, {}, "ChemicalInput_1",  "input",  0.0f},
                {"N2(6)",  "Nose2 sensors",     0.1f, {}, "ChemicalInput_2",  "input",  0.0f},
                {"Olc(2)", "Olfactory cortex",  0.3f, {}, "OdorRecognition",  "proc",   0.0f},
            };
            subsystems_.push_back(std::move(s));
        }

        // Subsystem 4: Entorhinal
        {
            BrainSubsystem s;
            s.name = "Entorhinal"; s.id = SubsystemId::ENTORHINAL;
            s.abbreviation = "EC"; s.cognitive_label = "NavigationSystem";
            s.components = {
                {"MEC(6)", "Medial entorhinal", 0.4f, {}, "SpatialNavigation", "spatial", 0.0f},
                {"LEC(5)", "Lateral entorhinal", 0.4f, {}, "ObjectRecognition", "object",  0.0f},
            };
            subsystems_.push_back(std::move(s));
        }

        // Subsystem 5: Spinal
        {
            BrainSubsystem s;
            s.name = "Spinal"; s.id = SubsystemId::SPINAL;
            s.abbreviation = "Sp"; s.cognitive_label = "SomatosensoryPath";
            s.components = {
                {"Sc[3]", "Spinal cord",      0.05f, {}, "SpinalPathway",   "pathway", 0.0f},
                {"Mc",    "Meissner corpuscle",0.01f, {}, "TouchSensor",     "sensor",  0.0f},
                {"Ep",    "Epidermis",         0.02f, {}, "SurfaceLayer",    "layer",   0.0f},
                {"De",    "Dermis",            0.03f, {}, "DeepLayer",       "layer",   0.0f},
                {"Hy",    "Hypodermis",        0.05f, {}, "SubcutaneousLyr", "layer",   0.0f},
                {"Nb",    "Nerve bundle",      0.01f, {}, "SignalBundle",    "trans",   0.0f},
                {"Sa",    "Sacral",            0.1f,  {}, "SacralSegment",   "segment", 0.0f},
                {"Lu",    "Lumbar",            0.1f,  {}, "LumbarSegment",   "segment", 0.0f},
                {"Ca",    "Cervical",          0.1f,  {}, "CervicalSegment", "segment", 0.0f},
            };
            subsystems_.push_back(std::move(s));
        }

        // Subsystem 6: Cognitive (opencog-specific addition)
        {
            BrainSubsystem s;
            s.name = "Cognitive"; s.id = SubsystemId::COGNITIVE;
            s.abbreviation = "Cog"; s.cognitive_label = "HigherCognition";
            s.components = {
                {"PLN",  "PLN inference",   0.1f,  {}, "LogicalReasoning",  "reason", 0.0f},
                {"ECAN", "Attention bank",  0.05f, {}, "AttentionAlloc",    "attn",   0.0f},
                {"PM",   "Pattern matcher", 0.08f, {}, "PatternRecognition","pattern",0.0f},
                {"URE",  "Rule engine",     0.12f, {}, "RuleApplication",   "infer",  0.0f},
                {"VES",  "Endocrine sys",   0.5f,  {}, "HormonalState",     "endo",   0.0f},
                {"VNS",  "Nervous system",  0.01f, {}, "NeuralSignaling",   "neural", 0.0f},
                {"AS",   "AtomSpace",       0.001f,{}, "KnowledgeBase",     "store",  0.0f},
            };
            subsystems_.push_back(std::move(s));
        }
    }

    CrystalBus bus_;
    std::vector<BrainRegion> regions_;
    std::vector<BrainSubsystem> subsystems_;
    uint64_t tick_count_{0};
};

} // namespace opencog::temporal

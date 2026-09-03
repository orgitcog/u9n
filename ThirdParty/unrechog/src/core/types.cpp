/**
 * @file types.cpp
 * @brief Core type implementations
 */

#include <opencog/core/types.hpp>

#include <unordered_map>

namespace opencog {

namespace {

// Type name registry
const std::unordered_map<AtomType, std::string_view> TYPE_NAMES = {
    // Special
    {AtomType::INVALID, "Invalid"},

    // Nodes
    {AtomType::NODE, "Node"},
    {AtomType::CONCEPT_NODE, "ConceptNode"},
    {AtomType::PREDICATE_NODE, "PredicateNode"},
    {AtomType::VARIABLE_NODE, "VariableNode"},
    {AtomType::NUMBER_NODE, "NumberNode"},
    {AtomType::TYPE_NODE, "TypeNode"},
    {AtomType::GROUNDED_OBJECT_NODE, "GroundedObjectNode"},
    {AtomType::ANCHOR_NODE, "AnchorNode"},
    {AtomType::SCHEMA_NODE, "SchemaNode"},
    {AtomType::GROUNDED_SCHEMA_NODE, "GroundedSchemaNode"},
    {AtomType::DEFINED_SCHEMA_NODE, "DefinedSchemaNode"},

    // Links
    {AtomType::LINK, "Link"},
    {AtomType::ORDERED_LINK, "OrderedLink"},
    {AtomType::UNORDERED_LINK, "UnorderedLink"},
    {AtomType::LIST_LINK, "ListLink"},
    {AtomType::SET_LINK, "SetLink"},

    // Logical
    {AtomType::AND_LINK, "AndLink"},
    {AtomType::OR_LINK, "OrLink"},
    {AtomType::NOT_LINK, "NotLink"},

    // Inheritance
    {AtomType::INHERITANCE_LINK, "InheritanceLink"},
    {AtomType::SIMILARITY_LINK, "SimilarityLink"},
    {AtomType::SUBSET_LINK, "SubsetLink"},
    {AtomType::INTENSIONAL_INHERITANCE_LINK, "IntensionalInheritanceLink"},
    {AtomType::MEMBER_LINK, "MemberLink"},

    // Evaluation
    {AtomType::EVALUATION_LINK, "EvaluationLink"},
    {AtomType::EXECUTION_LINK, "ExecutionLink"},
    {AtomType::EXECUTION_OUTPUT_LINK, "ExecutionOutputLink"},

    // Scoping
    {AtomType::SCOPE_LINK, "ScopeLink"},
    {AtomType::BIND_LINK, "BindLink"},
    {AtomType::GET_LINK, "GetLink"},
    {AtomType::PUT_LINK, "PutLink"},
    {AtomType::LAMBDA_LINK, "LambdaLink"},

    // Context
    {AtomType::CONTEXT_LINK, "ContextLink"},
    {AtomType::STATE_LINK, "StateLink"},
    {AtomType::DEFINE_LINK, "DefineLink"},

    // Temporal
    {AtomType::AT_TIME_LINK, "AtTimeLink"},
    {AtomType::TIME_INTERVAL_LINK, "TimeIntervalLink"},
    {AtomType::BEFORE_LINK, "BeforeLink"},
    {AtomType::OVERLAPS_LINK, "OverlapsLink"},

    // PLN
    {AtomType::IMPLICATION_LINK, "ImplicationLink"},
    {AtomType::EQUIVALENCE_LINK, "EquivalenceLink"},
    {AtomType::FORALL_LINK, "ForAllLink"},
    {AtomType::EXISTS_LINK, "ExistsLink"},
    {AtomType::AVERAGE_LINK, "AverageLink"},

    // Endocrine System Nodes
    {AtomType::HORMONE_NODE, "HormoneNode"},
    {AtomType::GLAND_NODE, "GlandNode"},
    {AtomType::EPISODE_NODE, "EpisodeNode"},
    {AtomType::VALENCE_ANCHOR_NODE, "ValenceAnchorNode"},
    {AtomType::FELT_SENSE_NODE, "FeltSenseNode"},
    {AtomType::MORAL_SIGNAL_NODE, "MoralSignalNode"},

    // Endocrine System Links
    {AtomType::VALENCE_LINK, "ValenceLink"},
    {AtomType::EPISODE_LINK, "EpisodeLink"},
    {AtomType::TEMPORAL_VALENCE_LINK, "TemporalValenceLink"},
    {AtomType::CAUSAL_LINK, "CausalLink"},
    {AtomType::HORMONE_MODULATION_LINK, "HormoneModulationLink"},
    {AtomType::AFFECTIVE_LINK, "AffectiveLink"},
    {AtomType::MORAL_EVALUATION_LINK, "MoralEvaluationLink"},
    {AtomType::EMPATHY_LINK, "EmpathyLink"},

    // Integration Nodes (NPU, o9c2, Guidance, Marduk)
    {AtomType::NPU_STATE_NODE, "NPUStateNode"},
    {AtomType::O9C2_STATE_NODE, "O9C2StateNode"},
    {AtomType::GUIDANCE_NODE, "GuidanceNode"},
    {AtomType::PERSONA_NODE, "PersonaNode"},
    {AtomType::MARDUK_STATE_NODE, "MardukStateNode"},
    {AtomType::MARDUK_MEMORY_NODE, "MardukMemoryNode"},
    {AtomType::MARDUK_TASK_NODE, "MardukTaskNode"},
    {AtomType::MARDUK_AUTONOMY_NODE, "MardukAutonomyNode"},

    // Integration Links
    {AtomType::NPU_MODULATION_LINK, "NPUModulationLink"},
    {AtomType::PERSONA_LINK, "PersonaLink"},
    {AtomType::GUIDANCE_LINK, "GuidanceLink"},
    {AtomType::EMERGENCE_LINK, "EmergenceLink"},
    {AtomType::MARDUK_MODULATION_LINK, "MardukModulationLink"},
    {AtomType::MARDUK_MEMORY_LINK, "MardukMemoryLink"},
    {AtomType::MARDUK_TASK_LINK, "MardukTaskLink"},
    {AtomType::MARDUK_FEEDBACK_LINK, "MardukFeedbackLink"},

    // VirtualTouchpad Nodes
    {AtomType::TOUCHPAD_STATE_NODE, "TouchpadStateNode"},
    {AtomType::TOUCHPAD_GESTURE_NODE, "TouchpadGestureNode"},
    {AtomType::TOUCHPAD_CONTACT_NODE, "TouchpadContactNode"},
    {AtomType::TOUCHPAD_MANIFOLD_NODE, "TouchpadManifoldNode"},

    // VirtualTouchpad Links
    {AtomType::TOUCHPAD_MODULATION_LINK, "TouchpadModulationLink"},
    {AtomType::TOUCHPAD_GESTURE_LINK, "TouchpadGestureLink"},
    {AtomType::TOUCHPAD_FEEDBACK_LINK, "TouchpadFeedbackLink"},
    {AtomType::TOUCHPAD_MANIFOLD_LINK, "TouchpadManifoldLink"},

    // Nervous System Nodes
    {AtomType::NEURAL_CHANNEL_NODE, "NeuralChannelNode"},
    {AtomType::NEURAL_NUCLEUS_NODE, "NeuralNucleusNode"},
    {AtomType::NEURAL_PATHWAY_NODE, "NeuralPathwayNode"},
    {AtomType::SYNAPSE_NODE, "SynapseNode"},
    {AtomType::REFLEX_ARC_NODE, "ReflexArcNode"},
    {AtomType::PROCESSING_LEVEL_NODE, "ProcessingLevelNode"},
    {AtomType::NEURAL_PATTERN_NODE, "NeuralPatternNode"},
    {AtomType::SENSORY_INPUT_NODE, "SensoryInputNode"},
    {AtomType::MOTOR_OUTPUT_NODE, "MotorOutputNode"},

    // Nervous System Links
    {AtomType::NEURAL_PATHWAY_LINK, "NeuralPathwayLink"},
    {AtomType::SYNAPTIC_LINK, "SynapticLink"},
    {AtomType::REFLEX_LINK, "ReflexLink"},
    {AtomType::NEUROENDOCRINE_LINK, "NeuroEndocrineLink"},
    {AtomType::SENSORY_LINK, "SensoryLink"},
    {AtomType::MOTOR_LINK, "MotorLink"},
    {AtomType::NEURAL_MODULATION_LINK, "NeuralModulationLink"},
    {AtomType::NEURAL_FEEDBACK_LINK, "NeuralFeedbackLink"},
    {AtomType::PLASTICITY_LINK, "PlasticityLink"},
    {AtomType::INTEGRATION_LINK, "IntegrationLink"},

    // Temporal Crystal System Nodes
    {AtomType::TEMPORAL_CRYSTAL_NODE, "TemporalCrystalNode"},
    {AtomType::OSCILLATOR_NODE, "OscillatorNode"},
    {AtomType::CRYSTAL_DOMAIN_NODE, "CrystalDomainNode"},
    {AtomType::CRYSTAL_LAYER_NODE, "CrystalLayerNode"},
    {AtomType::TEMPORAL_SCALE_NODE, "TemporalScaleNode"},
    {AtomType::BRAIN_REGION_NODE, "BrainRegionNode"},
    {AtomType::BRAIN_SUBSYSTEM_NODE, "BrainSubsystemNode"},
    {AtomType::HIERARCHY_LEVEL_NODE, "HierarchyLevelNode"},
    {AtomType::PHASE_PATTERN_NODE, "PhasePatternNode"},
    {AtomType::COGNITIVE_PROCESS_NODE, "CognitiveProcessNode"},

    // Temporal Crystal System Links
    {AtomType::PHASE_COUPLING_LINK, "PhaseCouplingLink"},
    {AtomType::TEMPORAL_HIERARCHY_LINK, "TemporalHierarchyLink"},
    {AtomType::OSCILLATOR_MODULATION_LINK, "OscillatorModulationLink"},
    {AtomType::CRYSTAL_PATHWAY_LINK, "CrystalPathwayLink"},
    {AtomType::BRAIN_REGION_LINK, "BrainRegionLink"},
    {AtomType::SUBSYSTEM_LINK, "SubsystemLink"},
    {AtomType::TEMPORAL_FEEDBACK_LINK, "TemporalFeedbackLink"},
    {AtomType::CRYSTAL_ENDOCRINE_LINK, "CrystalEndocrineLink"},
    {AtomType::CRYSTAL_NEURAL_LINK, "CrystalNeuralLink"},
    {AtomType::RESONANCE_LINK, "ResonanceLink"},

    // Entelechy System Nodes
    {AtomType::TEMPERAMENT_NODE, "TemperamentNode"},
    {AtomType::CHARACTER_NODE, "CharacterNode"},
    {AtomType::INTEROCEPTIVE_NODE, "InteroceptiveNode"},
    {AtomType::DEVELOPMENTAL_NODE, "DevelopmentalNode"},
    {AtomType::NARRATIVE_NODE, "NarrativeNode"},
    {AtomType::SOCIAL_ROLE_NODE, "SocialRoleNode"},
    {AtomType::ATTACHMENT_NODE, "AttachmentNode"},
    {AtomType::DISTRICT_NODE, "DistrictNode"},
    {AtomType::CIVIC_ANGEL_NODE, "CivicAngelNode"},
    {AtomType::ENTELECHY_NODE, "EntelechyNode"},
    {AtomType::CRITICAL_PERIOD_NODE, "CriticalPeriodNode"},
    {AtomType::TRAUMA_NODE, "TraumaNode"},
    {AtomType::LIFE_THEME_NODE, "LifeThemeNode"},
    {AtomType::POLYVAGAL_STATE_NODE, "PolyvagalStateNode"},
    {AtomType::ALLOSTATIC_NODE, "AllostaticNode"},
    {AtomType::SELF_MODEL_NODE, "SelfModelNode"},

    // AFI Nodes
    {AtomType::MARKOV_BLANKET_NODE, "MarkovBlanketNode"},
    {AtomType::GENERATIVE_MODEL_NODE, "GenerativeModelNode"},
    {AtomType::FREE_ENERGY_NODE, "FreeEnergyNode"},
    {AtomType::PRECISION_NODE, "PrecisionNode"},
    {AtomType::PREDICTION_ERROR_NODE, "PredictionErrorNode"},

    // Entelechy System Links
    {AtomType::TEMPERAMENT_LINK, "TemperamentLink"},
    {AtomType::DEVELOPMENTAL_LINK, "DevelopmentalLink"},
    {AtomType::NARRATIVE_LINK, "NarrativeLink"},
    {AtomType::SOCIAL_ROLE_LINK, "SocialRoleLink"},
    {AtomType::ATTACHMENT_LINK, "AttachmentLink"},
    {AtomType::DISTRICT_BOUNDARY_LINK, "DistrictBoundaryLink"},
    {AtomType::CIVIC_GOVERNANCE_LINK, "CivicGovernanceLink"},
    {AtomType::ENTELECHY_PROGRESS_LINK, "EntelechyProgressLink"},
    {AtomType::INTEROCEPTIVE_LINK, "InteroceptiveLink"},
    {AtomType::TRAUMA_ENCODING_LINK, "TraumaEncodingLink"},
    {AtomType::LIFE_THEME_LINK, "LifeThemeLink"},
    {AtomType::CRITICAL_PERIOD_LINK, "CriticalPeriodLink"},
    {AtomType::POLYVAGAL_LINK, "PolyvagalLink"},
    {AtomType::SELF_MODEL_LINK, "SelfModelLink"},
    {AtomType::ALLOSTATIC_LINK, "AllostaticLink"},

    // AFI Links
    {AtomType::BLANKET_BOUNDARY_LINK, "BlanketBoundaryLink"},
    {AtomType::FREE_ENERGY_LINK, "FreeEnergyLink"},
    {AtomType::PRECISION_WEIGHTING_LINK, "PrecisionWeightingLink"},
    {AtomType::PREDICTION_ERROR_LINK, "PredictionErrorLink"},
};

// Reverse lookup map (built lazily)
std::unordered_map<std::string_view, AtomType> NAME_TO_TYPE;

void ensure_reverse_map() {
    if (NAME_TO_TYPE.empty()) {
        for (const auto& [type, name] : TYPE_NAMES) {
            NAME_TO_TYPE[name] = type;
        }
    }
}

} // anonymous namespace

std::string_view type_name(AtomType type) noexcept {
    auto it = TYPE_NAMES.find(type);
    if (it != TYPE_NAMES.end()) {
        return it->second;
    }

    // Check if it's a user-defined type
    if (static_cast<uint16_t>(type) >= static_cast<uint16_t>(AtomType::USER_DEFINED)) {
        return "UserDefinedType";
    }

    return "UnknownType";
}

AtomType type_from_name(std::string_view name) noexcept {
    ensure_reverse_map();

    auto it = NAME_TO_TYPE.find(name);
    if (it != NAME_TO_TYPE.end()) {
        return it->second;
    }

    return AtomType::INVALID;
}

} // namespace opencog

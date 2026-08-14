#ifndef DEEP_TREE_ECHO_CORE_HPP
#define DEEP_TREE_ECHO_CORE_HPP

/**
 * @file deep_tree_echo/core.hpp
 * @brief Deep Tree Echo Cognitive Framework Core
 * 
 * Implements the core cognitive architecture for the Deep Tree Echo system,
 * integrating reservoir computing with 4e embodied cognition principles.
 * 
 * Architecture based on:
 * - Echo State Networks for temporal pattern processing
 * - Hierarchical membrane structure for cognitive organization
 * - Agent-Arena-Relation (AAR) geometric architecture for self-awareness
 * - 4e cognition: Embodied, Embedded, Enacted, Extended
 */

#include <Eigen/Dense>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <optional>

#include "activationsfunc.hpp"
#include "numpy.hpp"

namespace deep_tree_echo {

using Matrix = Eigen::MatrixXd;
using Vector = Eigen::VectorXd;
using Array = Eigen::ArrayXd;

// Forward declarations
class CognitiveNode;
class MembraneLayer;
class EchoState;
class AvatarInterface;

/**
 * @brief Cognitive membrane types in the Deep Tree Echo hierarchy
 */
enum class MembraneType {
    ROOT,           // System boundary
    COGNITIVE,      // Core processing
    MEMORY,         // Storage & retrieval
    REASONING,      // Inference & logic
    GRAMMAR,        // Symbolic processing
    EXTENSION,      // Plugin container
    SENSORY,        // Input processing
    MOTOR,          // Output/action generation
    INTROSPECTION,  // Self-reflection
    SECURITY        // Validation & control
};

/**
 * @brief 4e Cognition modality
 */
enum class CognitionModality {
    EMBODIED,   // Body-based cognition
    EMBEDDED,   // Environment-coupled cognition
    ENACTED,    // Action-based cognition
    EXTENDED    // Tool-extended cognition
};

/**
 * @brief Echo propagation mode
 */
enum class PropagationMode {
    FEEDFORWARD,    // Standard forward propagation
    FEEDBACK,       // Recurrent feedback
    LATERAL,        // Cross-membrane communication
    RESONANCE       // Bidirectional echo amplification
};

/**
 * @brief Configuration for Echo State processing
 */
struct EchoConfig {
    int reservoir_size = 500;
    double spectral_radius = 0.95;
    double leak_rate = 0.3;
    double input_scaling = 1.0;
    double feedback_scaling = 0.5;
    double sparsity = 0.1;
    double noise_level = 0.001;
    std::string activation = "tanh";
    bool use_bias = true;
    unsigned int seed = 0;
};

/**
 * @brief Echo State - Core reservoir computing unit
 * 
 * Implements the Echo State Network with extensions for
 * cognitive processing and embodied interaction.
 */
class EchoState {
public:
    EchoState() = default;
    explicit EchoState(const EchoConfig& config);
    
    // Initialize reservoir weights
    void initialize();
    void initialize_from_weights(const Matrix& W, const Matrix& Win);
    
    // State computation
    Vector compute_state(const Vector& input);
    Vector compute_state(const Vector& input, const Vector& feedback);
    Matrix compute_states(const Matrix& inputs);
    
    // Training
    void fit(const Matrix& inputs, const Matrix& targets, double ridge = 1e-6);
    Matrix predict(const Matrix& inputs);
    
    // State management
    void reset();
    Vector get_state() const { return state_; }
    void set_state(const Vector& state) { state_ = state; }
    
    // Introspection
    double get_spectral_radius() const;
    double get_memory_capacity() const;
    Matrix get_reservoir_weights() const { return W_; }
    
    // Configuration
    const EchoConfig& config() const { return config_; }
    bool is_trained() const { return trained_; }
    
private:
    EchoConfig config_;
    Matrix W_;          // Reservoir weights
    Matrix Win_;        // Input weights
    Matrix Wout_;       // Output weights
    Matrix Wfb_;        // Feedback weights
    Vector state_;      // Current reservoir state
    Vector bias_;       // Bias vector
    
    reservoircpp::activations::ActivationFunc activation_;
    bool initialized_ = false;
    bool trained_ = false;
    
    void scale_spectral_radius();
};

/**
 * @brief Cognitive Node - Processing unit in the membrane hierarchy
 */
class CognitiveNode {
public:
    CognitiveNode(const std::string& name, MembraneType type);
    
    // Processing
    virtual Vector process(const Vector& input);
    virtual void update();
    
    // Connections
    void connect_to(std::shared_ptr<CognitiveNode> target, double weight = 1.0);
    void receive_echo(const Vector& echo, const std::string& source);
    
    // State
    const std::string& name() const { return name_; }
    MembraneType type() const { return type_; }
    Vector get_output() const { return output_; }
    
    // Echo state integration
    void set_echo_state(std::shared_ptr<EchoState> echo) { echo_state_ = echo; }
    std::shared_ptr<EchoState> get_echo_state() const { return echo_state_; }
    
protected:
    std::string name_;
    MembraneType type_;
    Vector output_;
    std::shared_ptr<EchoState> echo_state_;
    
    std::vector<std::pair<std::shared_ptr<CognitiveNode>, double>> connections_;
    std::unordered_map<std::string, Vector> incoming_echoes_;
};

/**
 * @brief Membrane Layer - Organizational unit in the cognitive hierarchy
 */
class MembraneLayer {
public:
    MembraneLayer(const std::string& name, MembraneType type);
    
    // Node management
    void add_node(std::shared_ptr<CognitiveNode> node);
    std::shared_ptr<CognitiveNode> get_node(const std::string& name);
    
    // Processing
    void process_all(const Vector& input);
    void propagate_echoes(PropagationMode mode = PropagationMode::FEEDFORWARD);
    
    // Hierarchy
    void add_child_membrane(std::shared_ptr<MembraneLayer> child);
    std::shared_ptr<MembraneLayer> get_child(const std::string& name);
    
    // State
    const std::string& name() const { return name_; }
    MembraneType type() const { return type_; }
    Vector get_aggregate_output() const;
    
private:
    std::string name_;
    MembraneType type_;
    std::vector<std::shared_ptr<CognitiveNode>> nodes_;
    std::vector<std::shared_ptr<MembraneLayer>> children_;
};

/**
 * @brief Avatar Interface - 4e Embodied Cognition Bridge
 * 
 * Connects the cognitive system to the Unreal Engine avatar,
 * implementing embodied, embedded, enacted, and extended cognition.
 */
class AvatarInterface {
public:
    AvatarInterface();
    
    // Sensory input (Embodied)
    void receive_visual(const Matrix& visual_data);
    void receive_audio(const Vector& audio_data);
    void receive_proprioception(const Vector& body_state);
    
    // Motor output (Enacted)
    Vector generate_movement();
    Vector generate_expression();
    Vector generate_speech_params();
    
    // Environment coupling (Embedded)
    void update_environment_state(const Vector& env_state);
    Vector get_environment_response() const;
    
    // Tool integration (Extended)
    void register_tool(const std::string& name, std::function<Vector(const Vector&)> tool);
    Vector use_tool(const std::string& name, const Vector& input);
    
    // Cognitive integration
    void set_cognitive_core(std::shared_ptr<MembraneLayer> core) { cognitive_core_ = core; }
    void process_cycle();
    
    // State
    CognitionModality current_modality() const { return current_modality_; }
    void set_modality(CognitionModality modality) { current_modality_ = modality; }
    
private:
    std::shared_ptr<MembraneLayer> cognitive_core_;
    CognitionModality current_modality_ = CognitionModality::EMBODIED;
    
    // Sensory buffers
    Matrix visual_buffer_;
    Vector audio_buffer_;
    Vector proprioception_buffer_;
    Vector environment_state_;
    
    // Motor state
    Vector movement_state_;
    Vector expression_state_;
    Vector speech_state_;
    
    // Extended tools
    std::unordered_map<std::string, std::function<Vector(const Vector&)>> tools_;
    
    // Processing
    Vector integrate_sensory_input();
    void update_motor_output(const Vector& cognitive_output);
};

/**
 * @brief Deep Tree Echo - Main cognitive system
 * 
 * The complete Deep Tree Echo cognitive architecture integrating
 * reservoir computing with hierarchical membrane organization
 * and 4e embodied cognition.
 */
class DeepTreeEcho {
public:
    DeepTreeEcho();
    explicit DeepTreeEcho(const EchoConfig& config);
    
    // Initialization
    void initialize();
    void load_configuration(const std::string& config_path);
    
    // Membrane hierarchy
    void build_membrane_hierarchy();
    std::shared_ptr<MembraneLayer> get_membrane(const std::string& name);
    
    // Processing
    void process_input(const Vector& input);
    Vector get_output() const;
    void run_cognitive_cycle();
    
    // Avatar integration
    void attach_avatar(std::shared_ptr<AvatarInterface> avatar);
    std::shared_ptr<AvatarInterface> get_avatar() const { return avatar_; }
    
    // Training and learning
    void train(const Matrix& inputs, const Matrix& targets);
    void online_learn(const Vector& input, const Vector& target);
    
    // Introspection
    std::string get_state_report() const;
    double get_cognitive_load() const;
    std::vector<std::string> get_active_membranes() const;
    
    // Persistence
    void save_state(const std::string& path) const;
    void load_state(const std::string& path);
    
private:
    EchoConfig config_;
    std::shared_ptr<MembraneLayer> root_membrane_;
    std::shared_ptr<AvatarInterface> avatar_;
    
    // Core echo states for different cognitive functions
    std::shared_ptr<EchoState> perception_echo_;
    std::shared_ptr<EchoState> reasoning_echo_;
    std::shared_ptr<EchoState> memory_echo_;
    std::shared_ptr<EchoState> motor_echo_;
    
    Vector current_output_;
    bool initialized_ = false;
    
    // Internal processing
    void propagate_through_hierarchy(const Vector& input);
    void synchronize_echoes();
    void update_avatar_state();
};

} // namespace deep_tree_echo

#endif // DEEP_TREE_ECHO_CORE_HPP

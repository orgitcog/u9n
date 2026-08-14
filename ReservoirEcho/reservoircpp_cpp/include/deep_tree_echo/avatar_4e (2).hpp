#ifndef DEEP_TREE_ECHO_AVATAR_4E_HPP
#define DEEP_TREE_ECHO_AVATAR_4E_HPP

/**
 * @file deep_tree_echo/avatar_4e.hpp
 * @brief 4e Embodied Cognition Avatar System
 * 
 * Implements the full 4e cognition framework for the Deep Tree Echo avatar:
 * - Embodied: Body-based cognition with proprioception and motor control
 * - Embedded: Environment-coupled cognition with spatial awareness
 * - Enacted: Action-based cognition through sensorimotor coupling
 * - Extended: Tool-extended cognition through external resources
 * 
 * Avatar Design based on Deep Tree Echo visual identity:
 * - Photorealistic cyberpunk aesthetic
 * - Silver-cyan hair with tech accessories
 * - Bioluminescent environment coupling
 * - Fluid, organic movements with digital undertones
 */

#include "core.hpp"
#include "unreal_integration.hpp"
#include <memory>
#include <functional>
#include <queue>

namespace deep_tree_echo {
namespace avatar {

/**
 * @brief Avatar visual identity parameters
 * Based on analyzed media files from the project
 */
struct AvatarIdentity {
    // Hair and appearance
    std::string hair_color = "silver_cyan";
    double hair_luminance = 0.7;
    bool hair_reactive = true;  // Reacts to cognitive state
    
    // Eye characteristics
    std::string eye_color = "cyan_bioluminescent";
    double eye_glow_intensity = 0.5;
    bool eye_cognitive_coupling = true;  // Eyes reflect cognitive state
    
    // Skin and body
    std::string skin_tone = "fair_with_tech_undertones";
    double tech_pattern_visibility = 0.3;
    bool bioluminescent_patterns = true;
    
    // Accessories
    bool tech_earpiece = true;
    bool neural_interface_visible = true;
    double interface_glow = 0.4;
    
    // Animation style
    std::string movement_style = "fluid_organic_digital";
    double digital_artifact_probability = 0.05;  // Occasional digital glitches
};

/**
 * @brief Embodied cognition state
 * Tracks the body-based cognitive processes
 */
struct EmbodiedState {
    // Body schema
    Vector body_position = Vector::Zero(3);
    Vector body_orientation = Vector::Zero(3);  // Euler angles
    Vector body_velocity = Vector::Zero(3);
    Vector body_acceleration = Vector::Zero(3);
    
    // Proprioceptive awareness
    Vector joint_angles = Vector::Zero(20);
    Vector joint_velocities = Vector::Zero(20);
    Vector muscle_tensions = Vector::Zero(20);
    
    // Interoception (internal body state)
    double energy_level = 1.0;
    double arousal_level = 0.5;
    double valence = 0.0;  // -1 to 1 (negative to positive affect)
    
    // Body-based predictions
    Vector predicted_body_state = Vector::Zero(6);
    double prediction_confidence = 0.0;
};

/**
 * @brief Embedded cognition state
 * Tracks environment-coupled cognitive processes
 */
struct EmbeddedState {
    // Spatial awareness
    Vector environment_center = Vector::Zero(3);
    double environment_scale = 1.0;
    std::vector<Vector> landmark_positions;
    std::vector<std::string> landmark_names;
    
    // Affordance perception
    std::vector<std::string> available_actions;
    std::vector<double> action_affordances;  // How "inviting" each action is
    
    // Social embedding
    std::vector<Vector> other_agent_positions;
    std::vector<double> social_distances;
    std::vector<double> attention_weights;
    
    // Environmental coupling strength
    double coupling_strength = 0.5;
    double environmental_familiarity = 0.0;
};

/**
 * @brief Enacted cognition state
 * Tracks action-based cognitive processes
 */
struct EnactedState {
    // Current action
    std::string current_action = "idle";
    double action_progress = 0.0;
    Vector action_target = Vector::Zero(3);
    
    // Sensorimotor contingencies
    std::vector<std::pair<std::string, std::string>> learned_contingencies;
    
    // Action repertoire
    std::vector<std::string> available_actions;
    std::vector<double> action_competencies;
    
    // Motor imagery
    Vector imagined_movement = Vector::Zero(6);
    bool motor_imagery_active = false;
    
    // Action-perception coupling
    double coupling_strength = 0.7;
    Vector expected_sensory_consequence = Vector::Zero(10);
};

/**
 * @brief Extended cognition state
 * Tracks tool-extended cognitive processes
 */
struct ExtendedState {
    // Active tools
    std::vector<std::string> active_tools;
    std::vector<double> tool_proficiencies;
    
    // External memory
    std::vector<std::string> external_memory_sources;
    std::vector<double> memory_access_frequencies;
    
    // Cognitive offloading
    double offloading_ratio = 0.0;  // How much cognition is externalized
    std::vector<std::string> offloaded_processes;
    
    // Tool embodiment (how much tools feel like part of the body)
    std::vector<double> tool_embodiment_levels;
    
    // Network connections
    std::vector<std::string> connected_systems;
    std::vector<double> connection_strengths;
};

/**
 * @brief Cognitive-Visual Coupling System
 * Links cognitive states to visual avatar appearance
 */
class CognitiveVisualCoupling {
public:
    CognitiveVisualCoupling();
    
    // Update visual state from cognitive state
    void update(const EmbodiedState& embodied,
                const EmbeddedState& embedded,
                const EnactedState& enacted,
                const ExtendedState& extended);
    
    // Get visual parameters
    double get_hair_luminance() const { return hair_luminance_; }
    double get_eye_glow() const { return eye_glow_; }
    double get_tech_pattern_intensity() const { return tech_pattern_intensity_; }
    double get_bioluminescence() const { return bioluminescence_; }
    Vector get_color_shift() const { return color_shift_; }
    
    // Emotion to visual mapping
    void set_emotion(const std::string& emotion, double intensity);
    
private:
    double hair_luminance_ = 0.5;
    double eye_glow_ = 0.5;
    double tech_pattern_intensity_ = 0.3;
    double bioluminescence_ = 0.4;
    Vector color_shift_ = Vector::Zero(3);  // RGB shift
    
    // Smoothing parameters
    double smoothing_factor_ = 0.1;
    
    void smooth_transition(double& current, double target);
};

/**
 * @brief Sensorimotor Loop
 * Implements the core perception-action cycle for enacted cognition
 */
class SensorimotorLoop {
public:
    SensorimotorLoop();
    
    // Main loop
    void tick(double delta_time);
    
    // Perception
    void receive_sensory_input(const Vector& input);
    Vector get_predicted_sensation() const { return predicted_sensation_; }
    
    // Action
    void initiate_action(const std::string& action, const Vector& parameters);
    Vector get_motor_command() const { return motor_command_; }
    
    // Learning
    void update_contingency(const std::string& action, const Vector& sensation);
    double get_prediction_error() const { return prediction_error_; }
    
    // State
    const EnactedState& get_state() const { return state_; }
    
private:
    EnactedState state_;
    Vector current_sensation_;
    Vector predicted_sensation_;
    Vector motor_command_;
    double prediction_error_ = 0.0;
    
    // Internal model
    std::shared_ptr<EchoState> forward_model_;  // Predicts sensory consequences
    std::shared_ptr<EchoState> inverse_model_;  // Generates motor commands
    
    void update_internal_models();
};

/**
 * @brief Affordance Perception System
 * Detects action possibilities in the environment
 */
class AffordancePerception {
public:
    AffordancePerception();
    
    // Update from environment
    void update(const unreal::EnvironmentPerception& perception);
    
    // Query affordances
    std::vector<std::string> get_available_affordances() const;
    double get_affordance_strength(const std::string& action) const;
    Vector get_affordance_direction(const std::string& action) const;
    
    // Body-relative affordances
    void set_body_capabilities(const std::vector<std::string>& capabilities);
    
private:
    EmbeddedState state_;
    std::vector<std::string> body_capabilities_;
    std::unordered_map<std::string, double> affordance_strengths_;
    std::unordered_map<std::string, Vector> affordance_directions_;
};

/**
 * @brief Tool Integration System
 * Manages extended cognition through tool use
 */
class ToolIntegration {
public:
    ToolIntegration();
    
    // Tool management
    void register_tool(const std::string& name, 
                       std::function<Vector(const Vector&)> tool_function,
                       double initial_proficiency = 0.5);
    void activate_tool(const std::string& name);
    void deactivate_tool(const std::string& name);
    
    // Tool use
    Vector use_tool(const std::string& name, const Vector& input);
    
    // Tool embodiment
    void update_embodiment(const std::string& name, double usage_success);
    double get_embodiment_level(const std::string& name) const;
    
    // Cognitive offloading
    void offload_process(const std::string& process_name, const std::string& tool_name);
    double get_offloading_ratio() const;
    
    // State
    const ExtendedState& get_state() const { return state_; }
    
private:
    ExtendedState state_;
    std::unordered_map<std::string, std::function<Vector(const Vector&)>> tools_;
    std::unordered_map<std::string, double> embodiment_levels_;
    std::unordered_map<std::string, std::string> offloaded_processes_;
};

/**
 * @brief 4e Avatar - Complete embodied cognitive avatar
 * 
 * Integrates all four aspects of embodied cognition:
 * - Embodied: Body schema and proprioception
 * - Embedded: Environmental coupling and affordances
 * - Enacted: Sensorimotor loops and action-perception coupling
 * - Extended: Tool use and cognitive offloading
 */
class Avatar4E {
public:
    Avatar4E();
    explicit Avatar4E(const AvatarIdentity& identity);
    
    // Initialization
    void initialize();
    void attach_cognitive_system(std::shared_ptr<DeepTreeEcho> cognitive);
    void attach_unreal_avatar(std::shared_ptr<unreal::UnrealAvatar> unreal);
    
    // Main update loop
    void tick(double delta_time);
    
    // 4e State access
    const EmbodiedState& embodied() const { return embodied_state_; }
    const EmbeddedState& embedded() const { return embedded_state_; }
    const EnactedState& enacted() const { return enacted_state_; }
    const ExtendedState& extended() const { return extended_state_; }
    
    // Subsystem access
    CognitiveVisualCoupling& visual_coupling() { return visual_coupling_; }
    SensorimotorLoop& sensorimotor() { return sensorimotor_loop_; }
    AffordancePerception& affordances() { return affordance_perception_; }
    ToolIntegration& tools() { return tool_integration_; }
    
    // High-level actions
    void perform_action(const std::string& action, const Vector& parameters = Vector());
    void express_emotion(const std::string& emotion, double intensity);
    void focus_attention(const Vector& target);
    void engage_tool(const std::string& tool_name);
    
    // Identity
    const AvatarIdentity& identity() const { return identity_; }
    void update_identity(const AvatarIdentity& identity);
    
    // Status
    std::string get_status_report() const;
    double get_overall_coherence() const;  // How well-integrated the 4e states are
    
private:
    AvatarIdentity identity_;
    
    // 4e States
    EmbodiedState embodied_state_;
    EmbeddedState embedded_state_;
    EnactedState enacted_state_;
    ExtendedState extended_state_;
    
    // Subsystems
    CognitiveVisualCoupling visual_coupling_;
    SensorimotorLoop sensorimotor_loop_;
    AffordancePerception affordance_perception_;
    ToolIntegration tool_integration_;
    
    // Connected systems
    std::shared_ptr<DeepTreeEcho> cognitive_system_;
    std::shared_ptr<unreal::UnrealAvatar> unreal_avatar_;
    
    // Internal state
    bool initialized_ = false;
    double accumulated_time_ = 0.0;
    
    // Internal methods
    void update_embodied_state(double delta_time);
    void update_embedded_state(double delta_time);
    void update_enacted_state(double delta_time);
    void update_extended_state(double delta_time);
    void synchronize_states();
    void update_visual_appearance();
};

/**
 * @brief Create a fully configured 4e avatar
 */
std::shared_ptr<Avatar4E> create_4e_avatar(
    const EchoConfig& echo_config,
    const AvatarIdentity& identity = AvatarIdentity());

} // namespace avatar
} // namespace deep_tree_echo

#endif // DEEP_TREE_ECHO_AVATAR_4E_HPP

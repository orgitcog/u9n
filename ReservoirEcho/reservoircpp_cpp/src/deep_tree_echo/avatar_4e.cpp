/**
 * @file deep_tree_echo/avatar_4e.cpp
 * @brief Implementation of 4e Embodied Cognition Avatar System
 */

#include "deep_tree_echo/avatar_4e.hpp"
#include <sstream>
#include <algorithm>
#include <cmath>

namespace deep_tree_echo {
namespace avatar {

// ============================================================================
// CognitiveVisualCoupling Implementation
// ============================================================================

CognitiveVisualCoupling::CognitiveVisualCoupling() {
    color_shift_ = Vector::Zero(3);
}

void CognitiveVisualCoupling::update(const EmbodiedState& embodied,
                                      const EmbeddedState& embedded,
                                      const EnactedState& enacted,
                                      const ExtendedState& extended) {
    // Hair luminance based on arousal and cognitive activity
    double target_hair = 0.3 + 0.4 * embodied.arousal_level + 
                         0.3 * enacted.coupling_strength;
    smooth_transition(hair_luminance_, target_hair);
    
    // Eye glow based on attention and processing
    double target_eye = 0.4 + 0.3 * embedded.coupling_strength +
                        0.3 * (extended.offloading_ratio > 0 ? 0.8 : 0.2);
    smooth_transition(eye_glow_, target_eye);
    
    // Tech pattern intensity based on extended cognition
    double target_tech = 0.2 + 0.5 * extended.offloading_ratio +
                         0.3 * (extended.active_tools.size() > 0 ? 0.8 : 0.2);
    smooth_transition(tech_pattern_intensity_, target_tech);
    
    // Bioluminescence based on overall coherence and valence
    double target_bio = 0.3 + 0.3 * (embodied.valence + 1.0) / 2.0 +
                        0.4 * embodied.energy_level;
    smooth_transition(bioluminescence_, target_bio);
    
    // Color shift based on emotional valence
    Vector target_color(3);
    if (embodied.valence > 0) {
        // Positive: shift toward cyan/blue
        target_color << 0.0, 0.3 * embodied.valence, 0.5 * embodied.valence;
    } else {
        // Negative: shift toward purple/red
        target_color << -0.3 * embodied.valence, 0.0, 0.2 * std::abs(embodied.valence);
    }
    
    for (int i = 0; i < 3; ++i) {
        smooth_transition(color_shift_(i), target_color(i));
    }
}

void CognitiveVisualCoupling::set_emotion(const std::string& emotion, double intensity) {
    // Map emotions to visual parameters
    if (emotion == "joy" || emotion == "happy") {
        hair_luminance_ = 0.7 + 0.3 * intensity;
        eye_glow_ = 0.8 * intensity;
        bioluminescence_ = 0.6 + 0.4 * intensity;
        color_shift_ << 0.0, 0.3 * intensity, 0.4 * intensity;
    } else if (emotion == "focus" || emotion == "concentration") {
        hair_luminance_ = 0.5;
        eye_glow_ = 0.9 * intensity;
        tech_pattern_intensity_ = 0.6 + 0.4 * intensity;
        color_shift_ << 0.0, 0.5 * intensity, 0.3 * intensity;
    } else if (emotion == "curiosity") {
        hair_luminance_ = 0.6 + 0.2 * intensity;
        eye_glow_ = 0.7 * intensity;
        bioluminescence_ = 0.5 + 0.3 * intensity;
        color_shift_ << 0.1 * intensity, 0.4 * intensity, 0.2 * intensity;
    } else if (emotion == "calm" || emotion == "serene") {
        hair_luminance_ = 0.4;
        eye_glow_ = 0.4;
        bioluminescence_ = 0.3 + 0.2 * intensity;
        color_shift_ << 0.0, 0.2 * intensity, 0.3 * intensity;
    }
}

void CognitiveVisualCoupling::smooth_transition(double& current, double target) {
    current = current * (1.0 - smoothing_factor_) + target * smoothing_factor_;
}

// ============================================================================
// SensorimotorLoop Implementation
// ============================================================================

SensorimotorLoop::SensorimotorLoop() {
    current_sensation_ = Vector::Zero(10);
    predicted_sensation_ = Vector::Zero(10);
    motor_command_ = Vector::Zero(6);
    
    // Initialize internal models
    EchoConfig forward_config;
    forward_config.reservoir_size = 200;
    forward_config.spectral_radius = 0.9;
    forward_config.leak_rate = 0.3;
    forward_model_ = std::make_shared<EchoState>(forward_config);
    forward_model_->initialize();
    
    EchoConfig inverse_config;
    inverse_config.reservoir_size = 200;
    inverse_config.spectral_radius = 0.85;
    inverse_config.leak_rate = 0.4;
    inverse_model_ = std::make_shared<EchoState>(inverse_config);
    inverse_model_->initialize();
}

void SensorimotorLoop::tick(double delta_time) {
    // Update action progress
    if (state_.current_action != "idle") {
        state_.action_progress += delta_time;
        if (state_.action_progress >= 1.0) {
            state_.current_action = "idle";
            state_.action_progress = 0.0;
        }
    }
    
    // Compute prediction error
    if (current_sensation_.size() == predicted_sensation_.size()) {
        prediction_error_ = (current_sensation_ - predicted_sensation_).norm();
    }
    
    // Update coupling strength based on prediction accuracy
    state_.coupling_strength = std::max(0.1, 1.0 - prediction_error_ / 10.0);
    
    update_internal_models();
}

void SensorimotorLoop::receive_sensory_input(const Vector& input) {
    current_sensation_ = input;
    
    // Generate prediction for next sensation
    if (forward_model_ && motor_command_.size() > 0) {
        Vector combined(input.size() + motor_command_.size());
        combined.head(input.size()) = input;
        combined.tail(motor_command_.size()) = motor_command_;
        predicted_sensation_ = forward_model_->compute_state(combined);
    }
}

void SensorimotorLoop::initiate_action(const std::string& action, const Vector& parameters) {
    state_.current_action = action;
    state_.action_progress = 0.0;
    
    if (parameters.size() >= 3) {
        state_.action_target = parameters.head(3);
    }
    
    // Generate motor command using inverse model
    if (inverse_model_ && current_sensation_.size() > 0) {
        Vector goal(current_sensation_.size() + parameters.size());
        goal.head(current_sensation_.size()) = current_sensation_;
        if (parameters.size() > 0) {
            goal.tail(parameters.size()) = parameters;
        }
        motor_command_ = inverse_model_->compute_state(goal);
    }
}

void SensorimotorLoop::update_contingency(const std::string& action, const Vector& sensation) {
    // Store learned sensorimotor contingency
    // In a full implementation, this would update the internal models
    state_.learned_contingencies.push_back({action, "sensation_" + std::to_string(sensation.norm())});
}

void SensorimotorLoop::update_internal_models() {
    // Online learning update for internal models
    // Simplified version - full implementation would use proper online learning
}

// ============================================================================
// AffordancePerception Implementation
// ============================================================================

AffordancePerception::AffordancePerception() {
    body_capabilities_ = {"walk", "reach", "grasp", "look", "speak"};
}

void AffordancePerception::update(const unreal::EnvironmentPerception& perception) {
    state_.environment_center = perception.agent_position;
    state_.landmark_positions = perception.object_positions;
    state_.landmark_names = perception.detected_objects;
    state_.other_agent_positions = perception.other_agent_positions;
    
    // Compute affordances based on environment
    affordance_strengths_.clear();
    affordance_directions_.clear();
    
    // Walking affordance
    if (perception.nearest_obstacle_distance > 1.0) {
        affordance_strengths_["walk"] = std::min(1.0, perception.nearest_obstacle_distance / 5.0);
        affordance_directions_["walk"] = perception.agent_forward;
    }
    
    // Reaching affordance for nearby objects
    for (size_t i = 0; i < perception.object_positions.size(); ++i) {
        Vector to_object = perception.object_positions[i] - perception.agent_position;
        double distance = to_object.norm();
        if (distance < 2.0 && distance > 0) {
            std::string affordance_name = "reach_" + perception.detected_objects[i];
            affordance_strengths_[affordance_name] = 1.0 - distance / 2.0;
            affordance_directions_[affordance_name] = to_object.normalized();
        }
    }
    
    // Social affordances
    for (size_t i = 0; i < perception.other_agent_positions.size(); ++i) {
        Vector to_agent = perception.other_agent_positions[i] - perception.agent_position;
        double distance = to_agent.norm();
        if (distance < 5.0 && distance > 0) {
            affordance_strengths_["interact_agent_" + std::to_string(i)] = 1.0 - distance / 5.0;
            affordance_directions_["interact_agent_" + std::to_string(i)] = to_agent.normalized();
        }
    }
    
    // Update available actions
    state_.available_actions.clear();
    state_.action_affordances.clear();
    for (const auto& [name, strength] : affordance_strengths_) {
        state_.available_actions.push_back(name);
        state_.action_affordances.push_back(strength);
    }
}

std::vector<std::string> AffordancePerception::get_available_affordances() const {
    return state_.available_actions;
}

double AffordancePerception::get_affordance_strength(const std::string& action) const {
    auto it = affordance_strengths_.find(action);
    if (it != affordance_strengths_.end()) {
        return it->second;
    }
    return 0.0;
}

Vector AffordancePerception::get_affordance_direction(const std::string& action) const {
    auto it = affordance_directions_.find(action);
    if (it != affordance_directions_.end()) {
        return it->second;
    }
    return Vector::Zero(3);
}

void AffordancePerception::set_body_capabilities(const std::vector<std::string>& capabilities) {
    body_capabilities_ = capabilities;
}

// ============================================================================
// ToolIntegration Implementation
// ============================================================================

ToolIntegration::ToolIntegration() {}

void ToolIntegration::register_tool(const std::string& name,
                                     std::function<Vector(const Vector&)> tool_function,
                                     double initial_proficiency) {
    tools_[name] = tool_function;
    embodiment_levels_[name] = initial_proficiency * 0.5;  // Start at half embodiment
    
    // Add to state
    bool found = false;
    for (size_t i = 0; i < state_.active_tools.size(); ++i) {
        if (state_.active_tools[i] == name) {
            state_.tool_proficiencies[i] = initial_proficiency;
            found = true;
            break;
        }
    }
    if (!found) {
        state_.active_tools.push_back(name);
        state_.tool_proficiencies.push_back(initial_proficiency);
        state_.tool_embodiment_levels.push_back(initial_proficiency * 0.5);
    }
}

void ToolIntegration::activate_tool(const std::string& name) {
    auto it = tools_.find(name);
    if (it != tools_.end()) {
        // Move to front of active tools
        auto pos = std::find(state_.active_tools.begin(), state_.active_tools.end(), name);
        if (pos != state_.active_tools.end()) {
            state_.active_tools.erase(pos);
        }
        state_.active_tools.insert(state_.active_tools.begin(), name);
    }
}

void ToolIntegration::deactivate_tool(const std::string& name) {
    auto pos = std::find(state_.active_tools.begin(), state_.active_tools.end(), name);
    if (pos != state_.active_tools.end()) {
        state_.active_tools.erase(pos);
    }
}

Vector ToolIntegration::use_tool(const std::string& name, const Vector& input) {
    auto it = tools_.find(name);
    if (it != tools_.end()) {
        return it->second(input);
    }
    return input;
}

void ToolIntegration::update_embodiment(const std::string& name, double usage_success) {
    auto it = embodiment_levels_.find(name);
    if (it != embodiment_levels_.end()) {
        // Increase embodiment with successful use
        it->second = std::min(1.0, it->second + 0.01 * usage_success);
        
        // Update state
        for (size_t i = 0; i < state_.active_tools.size(); ++i) {
            if (state_.active_tools[i] == name) {
                state_.tool_embodiment_levels[i] = it->second;
                break;
            }
        }
    }
}

double ToolIntegration::get_embodiment_level(const std::string& name) const {
    auto it = embodiment_levels_.find(name);
    if (it != embodiment_levels_.end()) {
        return it->second;
    }
    return 0.0;
}

void ToolIntegration::offload_process(const std::string& process_name, const std::string& tool_name) {
    offloaded_processes_[process_name] = tool_name;
    
    // Update offloading ratio
    state_.offloaded_processes.clear();
    for (const auto& [proc, tool] : offloaded_processes_) {
        state_.offloaded_processes.push_back(proc);
    }
    state_.offloading_ratio = static_cast<double>(offloaded_processes_.size()) / 10.0;  // Assume max 10 processes
}

double ToolIntegration::get_offloading_ratio() const {
    return state_.offloading_ratio;
}

// ============================================================================
// Avatar4E Implementation
// ============================================================================

Avatar4E::Avatar4E() {}

Avatar4E::Avatar4E(const AvatarIdentity& identity) : identity_(identity) {}

void Avatar4E::initialize() {
    // Initialize embodied state
    embodied_state_.body_position = Vector::Zero(3);
    embodied_state_.body_orientation = Vector::Zero(3);
    embodied_state_.body_velocity = Vector::Zero(3);
    embodied_state_.joint_angles = Vector::Zero(20);
    embodied_state_.energy_level = 1.0;
    embodied_state_.arousal_level = 0.5;
    embodied_state_.valence = 0.0;
    
    // Initialize embedded state
    embedded_state_.environment_center = Vector::Zero(3);
    embedded_state_.coupling_strength = 0.5;
    
    // Initialize enacted state
    enacted_state_.current_action = "idle";
    enacted_state_.coupling_strength = 0.7;
    
    // Initialize extended state
    extended_state_.offloading_ratio = 0.0;
    
    // Register default tools
    tool_integration_.register_tool("memory_search", [](const Vector& query) {
        return query;  // Placeholder
    }, 0.8);
    
    tool_integration_.register_tool("calculation", [](const Vector& input) {
        return input * 2.0;  // Placeholder
    }, 0.9);
    
    initialized_ = true;
}

void Avatar4E::attach_cognitive_system(std::shared_ptr<DeepTreeEcho> cognitive) {
    cognitive_system_ = cognitive;
}

void Avatar4E::attach_unreal_avatar(std::shared_ptr<unreal::UnrealAvatar> unreal) {
    unreal_avatar_ = unreal;
}

void Avatar4E::tick(double delta_time) {
    if (!initialized_) return;
    
    accumulated_time_ += delta_time;
    
    // Update all 4e states
    update_embodied_state(delta_time);
    update_embedded_state(delta_time);
    update_enacted_state(delta_time);
    update_extended_state(delta_time);
    
    // Synchronize states
    synchronize_states();
    
    // Update visual appearance
    update_visual_appearance();
    
    // Update Unreal avatar if attached
    if (unreal_avatar_) {
        unreal_avatar_->tick(delta_time);
    }
}

void Avatar4E::update_embodied_state(double delta_time) {
    // Update body dynamics
    embodied_state_.body_position += embodied_state_.body_velocity * delta_time;
    embodied_state_.body_velocity += embodied_state_.body_acceleration * delta_time;
    
    // Decay acceleration
    embodied_state_.body_acceleration *= 0.9;
    
    // Update energy (slowly recover)
    embodied_state_.energy_level = std::min(1.0, embodied_state_.energy_level + 0.001 * delta_time);
    
    // Update arousal (decay toward baseline)
    embodied_state_.arousal_level = embodied_state_.arousal_level * 0.99 + 0.5 * 0.01;
    
    // Update valence (decay toward neutral)
    embodied_state_.valence *= 0.995;
}

void Avatar4E::update_embedded_state(double delta_time) {
    // Update environmental coupling
    if (unreal_avatar_) {
        // Get environment perception from Unreal
        embedded_state_.coupling_strength = std::min(1.0, embedded_state_.coupling_strength + 0.01 * delta_time);
    }
    
    // Update affordance perception
    if (unreal_avatar_) {
        // Would get perception from Unreal here
    }
}

void Avatar4E::update_enacted_state(double delta_time) {
    // Update sensorimotor loop
    sensorimotor_loop_.tick(delta_time);
    
    // Copy state from sensorimotor loop
    enacted_state_ = sensorimotor_loop_.get_state();
}

void Avatar4E::update_extended_state(double delta_time) {
    (void)delta_time;  // Will be used for time-based tool state updates
    // Update tool integration state
    extended_state_ = tool_integration_.get_state();
}

void Avatar4E::synchronize_states() {
    // Ensure coherence between 4e states
    
    // Embodied affects enacted (body state influences action capability)
    enacted_state_.coupling_strength *= embodied_state_.energy_level;
    
    // Embedded affects embodied (environment affects body state)
    embodied_state_.arousal_level = std::max(embodied_state_.arousal_level,
                                              embedded_state_.coupling_strength * 0.3);
    
    // Extended affects all (tool use modifies capabilities)
    if (extended_state_.offloading_ratio > 0.5) {
        embodied_state_.energy_level = std::min(1.0, embodied_state_.energy_level * 1.1);
    }
}

void Avatar4E::update_visual_appearance() {
    visual_coupling_.update(embodied_state_, embedded_state_, enacted_state_, extended_state_);
    
    // Apply to Unreal avatar if attached
    if (unreal_avatar_) {
        // Would set visual parameters here
    }
}

void Avatar4E::perform_action(const std::string& action, const Vector& parameters) {
    sensorimotor_loop_.initiate_action(action, parameters);
    
    // Reduce energy
    embodied_state_.energy_level = std::max(0.0, embodied_state_.energy_level - 0.05);
    
    // Increase arousal
    embodied_state_.arousal_level = std::min(1.0, embodied_state_.arousal_level + 0.1);
}

void Avatar4E::express_emotion(const std::string& emotion, double intensity) {
    visual_coupling_.set_emotion(emotion, intensity);
    
    // Update valence based on emotion
    if (emotion == "joy" || emotion == "happy" || emotion == "curiosity") {
        embodied_state_.valence = std::min(1.0, embodied_state_.valence + intensity * 0.3);
    } else if (emotion == "sad" || emotion == "fear" || emotion == "anger") {
        embodied_state_.valence = std::max(-1.0, embodied_state_.valence - intensity * 0.3);
    }
    
    // Update arousal
    embodied_state_.arousal_level = std::min(1.0, embodied_state_.arousal_level + intensity * 0.2);
}

void Avatar4E::focus_attention(const Vector& target) {
    // Update embedded state
    embedded_state_.environment_center = target;
    
    // Update enacted state
    enacted_state_.action_target = target;
    
    // Increase arousal slightly
    embodied_state_.arousal_level = std::min(1.0, embodied_state_.arousal_level + 0.05);
}

void Avatar4E::engage_tool(const std::string& tool_name) {
    tool_integration_.activate_tool(tool_name);
    
    // Update extended state
    extended_state_ = tool_integration_.get_state();
}

void Avatar4E::update_identity(const AvatarIdentity& identity) {
    identity_ = identity;
}

std::string Avatar4E::get_status_report() const {
    std::ostringstream report;
    report << "=== 4e Avatar Status Report ===" << std::endl;
    report << "Initialized: " << (initialized_ ? "Yes" : "No") << std::endl;
    report << "Accumulated Time: " << accumulated_time_ << "s" << std::endl;
    report << std::endl;
    
    report << "--- Embodied State ---" << std::endl;
    report << "Energy: " << embodied_state_.energy_level << std::endl;
    report << "Arousal: " << embodied_state_.arousal_level << std::endl;
    report << "Valence: " << embodied_state_.valence << std::endl;
    report << std::endl;
    
    report << "--- Embedded State ---" << std::endl;
    report << "Coupling Strength: " << embedded_state_.coupling_strength << std::endl;
    report << "Landmarks: " << embedded_state_.landmark_names.size() << std::endl;
    report << std::endl;
    
    report << "--- Enacted State ---" << std::endl;
    report << "Current Action: " << enacted_state_.current_action << std::endl;
    report << "Action Progress: " << enacted_state_.action_progress << std::endl;
    report << "Coupling Strength: " << enacted_state_.coupling_strength << std::endl;
    report << std::endl;
    
    report << "--- Extended State ---" << std::endl;
    report << "Active Tools: " << extended_state_.active_tools.size() << std::endl;
    report << "Offloading Ratio: " << extended_state_.offloading_ratio << std::endl;
    report << std::endl;
    
    report << "--- Visual State ---" << std::endl;
    report << "Hair Luminance: " << visual_coupling_.get_hair_luminance() << std::endl;
    report << "Eye Glow: " << visual_coupling_.get_eye_glow() << std::endl;
    report << "Bioluminescence: " << visual_coupling_.get_bioluminescence() << std::endl;
    
    return report.str();
}

double Avatar4E::get_overall_coherence() const {
    // Compute how well-integrated the 4e states are
    double coherence = 0.0;
    
    // Embodied-enacted coherence
    coherence += 0.25 * (1.0 - std::abs(embodied_state_.energy_level - enacted_state_.coupling_strength));
    
    // Embedded-embodied coherence
    coherence += 0.25 * embedded_state_.coupling_strength;
    
    // Extended-enacted coherence
    coherence += 0.25 * (1.0 - std::abs(extended_state_.offloading_ratio - 0.5));
    
    // Overall energy and valence coherence
    coherence += 0.25 * (embodied_state_.energy_level * (embodied_state_.valence + 1.0) / 2.0);
    
    return coherence;
}

// ============================================================================
// Factory Function
// ============================================================================

std::shared_ptr<Avatar4E> create_4e_avatar(const EchoConfig& echo_config,
                                            const AvatarIdentity& identity) {
    // Create cognitive system
    auto cognitive = std::make_shared<DeepTreeEcho>(echo_config);
    cognitive->initialize();
    
    // Create Unreal avatar
    auto unreal = unreal::create_avatar(echo_config);
    
    // Create 4e avatar
    auto avatar = std::make_shared<Avatar4E>(identity);
    avatar->attach_cognitive_system(cognitive);
    avatar->attach_unreal_avatar(unreal);
    avatar->initialize();
    
    return avatar;
}

} // namespace avatar
} // namespace deep_tree_echo

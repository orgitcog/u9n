/**
 * @file deep_tree_echo/unreal_integration.cpp
 * @brief Implementation of Unreal Engine Integration for Deep Tree Echo
 */

#include "deep_tree_echo/unreal_integration.hpp"
#include <sstream>
#include <algorithm>

namespace deep_tree_echo {
namespace unreal {

// ============================================================================
// CharacterController Implementation
// ============================================================================

CharacterController::CharacterController() {
    target_location_ = Vector::Zero(3);
}

void CharacterController::set_movement_input(const BlendSpaceParams& params) {
    current_movement_ = params;
    moving_ = (params.forward_speed != 0.0 || params.lateral_speed != 0.0);
}

void CharacterController::set_target_location(const Vector& location) {
    target_location_ = location;
}

void CharacterController::set_target_rotation(double yaw) {
    target_yaw_ = yaw;
}

void CharacterController::play_animation(const std::string& anim_name, double blend_time) {
    (void)blend_time;  // Will be used when integrating with Unreal animation system
    current_animation_ = anim_name;
    // In a real implementation, this would interface with Unreal's animation system
}

void CharacterController::set_animation_parameter(const std::string& param, double value) {
    anim_params_[param] = value;
}

void CharacterController::trigger_animation_event(const std::string& event_name) {
    (void)event_name;  // Will be used when integrating with Unreal animation notifies
    // Trigger animation notify in Unreal
}

void CharacterController::apply_impulse(const Vector& impulse) {
    (void)impulse;  // Will be used when integrating with Unreal physics
    // Apply physics impulse to character
}

void CharacterController::grab_object(const std::string& object_id) {
    (void)object_id;  // Will be used when integrating with Unreal interaction system
    // Implement object grabbing
}

void CharacterController::release_object() {
    // Release grabbed object
}

void CharacterController::update_from_cognitive_output(const Vector& output) {
    if (output.size() >= 4) {
        BlendSpaceParams params = BlendSpaceParams::from_vector(output.head(4));
        set_movement_input(params);
    }
}

Vector CharacterController::get_proprioception() const {
    Vector proprio(10);
    proprio << current_movement_.forward_speed,
               current_movement_.lateral_speed,
               current_movement_.turn_rate,
               current_movement_.stance_weight,
               target_location_(0), target_location_(1), target_location_(2),
               target_yaw_,
               grounded_ ? 1.0 : 0.0,
               moving_ ? 1.0 : 0.0;
    return proprio;
}

// ============================================================================
// FacialController Implementation
// ============================================================================

FacialController::FacialController() {
    gaze_target_ = Vector::Zero(3);
}

void FacialController::set_expression(const FacialExpression& expr) {
    current_expression_ = expr;
    blend_progress_ = 1.0;
}

void FacialController::blend_to_expression(const FacialExpression& target, double duration) {
    target_expression_ = target;
    blend_duration_ = duration;
    blend_progress_ = 0.0;
}

void FacialController::set_emotion(const std::string& emotion, double intensity) {
    FacialExpression expr;
    
    if (emotion == "happy" || emotion == "joy") {
        expr.lip_corner_pull = intensity;
        expr.cheek_raise = intensity * 0.7;
        expr.brow_inner_up = intensity * 0.3;
    } else if (emotion == "sad") {
        expr.lip_corner_depress = intensity;
        expr.brow_inner_up = intensity * 0.5;
        expr.upper_lid_raise = -intensity * 0.3;
    } else if (emotion == "angry") {
        expr.brow_lowerer = intensity;
        expr.lid_tighten = intensity * 0.5;
        expr.lip_stretch = intensity * 0.3;
    } else if (emotion == "surprised") {
        expr.brow_inner_up = intensity;
        expr.brow_outer_up = intensity;
        expr.upper_lid_raise = intensity;
        expr.jaw_drop = intensity * 0.5;
    } else if (emotion == "fear") {
        expr.brow_inner_up = intensity;
        expr.upper_lid_raise = intensity;
        expr.lip_stretch = intensity * 0.5;
    } else if (emotion == "disgust") {
        expr.brow_lowerer = intensity * 0.5;
        expr.lip_corner_depress = intensity;
        expr.cheek_raise = intensity * 0.3;
    }
    
    blend_to_expression(expr, 0.3);
}

void FacialController::set_viseme(int viseme_id, double weight) {
    current_expression_.current_viseme = viseme_id;
    current_expression_.viseme_weight = weight;
}

void FacialController::process_audio_for_lipsync(const Vector& audio_features) {
    // Simple viseme selection based on audio features
    if (audio_features.size() > 0) {
        // Map audio features to visemes
        double energy = audio_features.norm();
        if (energy > 0.5) {
            set_viseme(1, energy); // Open mouth
        } else {
            set_viseme(0, 1.0 - energy); // Closed mouth
        }
    }
}

void FacialController::set_gaze_target(const Vector& world_position) {
    gaze_target_ = world_position;
}

void FacialController::set_blink_rate(double blinks_per_minute) {
    blink_rate_ = blinks_per_minute;
}

void FacialController::update_from_cognitive_output(const Vector& output) {
    if (output.size() >= 12) {
        FacialExpression expr = FacialExpression::from_vector(output.head(12));
        blend_to_expression(expr, 0.1);
    }
}

Vector FacialController::get_expression_state() const {
    return current_expression_.to_vector();
}

// ============================================================================
// EnvironmentInterface Implementation
// ============================================================================

EnvironmentInterface::EnvironmentInterface() {}

void EnvironmentInterface::update_perception(const EnvironmentPerception& perception) {
    current_perception_ = perception;
    
    // Update interactable objects list
    interactable_objects_.clear();
    for (const auto& obj : perception.detected_objects) {
        interactable_objects_.push_back(obj);
    }
}

Vector EnvironmentInterface::get_perception_vector() const {
    // Encode perception into a vector
    Vector perception(20);
    perception.setZero();
    
    // Agent position
    if (current_perception_.agent_position.size() >= 3) {
        perception.head(3) = current_perception_.agent_position;
    }
    
    // Agent forward
    if (current_perception_.agent_forward.size() >= 3) {
        perception.segment(3, 3) = current_perception_.agent_forward;
    }
    
    // Nearest obstacle
    perception(6) = current_perception_.nearest_obstacle_distance;
    
    // Number of detected objects
    perception(7) = static_cast<double>(current_perception_.detected_objects.size());
    
    // Number of other agents
    perception(8) = static_cast<double>(current_perception_.other_agent_positions.size());
    
    // Ambient noise
    perception(9) = current_perception_.ambient_noise_level;
    
    // Sound direction
    if (current_perception_.sound_direction.size() >= 3) {
        perception.segment(10, 3) = current_perception_.sound_direction;
    }
    
    return perception;
}

void EnvironmentInterface::interact_with_object(const std::string& object_id,
                                                  const std::string& action) {
    (void)object_id;  // Will be used when integrating with Unreal
    (void)action;     // Will be used when integrating with Unreal
    // Send interaction command to Unreal
}

std::vector<std::string> EnvironmentInterface::get_interactable_objects() const {
    return interactable_objects_;
}

Vector EnvironmentInterface::find_path_to(const Vector& destination) {
    // Simple direct path (in real implementation, use navigation mesh)
    Vector direction = destination - current_perception_.agent_position;
    if (direction.norm() > 0) {
        direction.normalize();
    }
    return direction;
}

bool EnvironmentInterface::is_path_clear(const Vector& direction, double distance) {
    (void)direction;  // Will be used for raycast direction when integrating with Unreal
    return current_perception_.nearest_obstacle_distance > distance;
}

Vector EnvironmentInterface::get_nearest_agent_direction() const {
    if (current_perception_.other_agent_positions.empty()) {
        return Vector::Zero(3);
    }
    
    Vector nearest = current_perception_.other_agent_positions[0];
    double min_dist = (nearest - current_perception_.agent_position).norm();
    
    for (size_t i = 1; i < current_perception_.other_agent_positions.size(); ++i) {
        double dist = (current_perception_.other_agent_positions[i] - 
                       current_perception_.agent_position).norm();
        if (dist < min_dist) {
            min_dist = dist;
            nearest = current_perception_.other_agent_positions[i];
        }
    }
    
    Vector direction = nearest - current_perception_.agent_position;
    if (direction.norm() > 0) {
        direction.normalize();
    }
    return direction;
}

double EnvironmentInterface::get_social_distance() const {
    if (current_perception_.other_agent_positions.empty()) {
        return 100.0;
    }
    
    double min_dist = std::numeric_limits<double>::max();
    for (const auto& pos : current_perception_.other_agent_positions) {
        double dist = (pos - current_perception_.agent_position).norm();
        min_dist = std::min(min_dist, dist);
    }
    return min_dist;
}

// ============================================================================
// AudioInterface Implementation
// ============================================================================

AudioInterface::AudioInterface() {
    audio_features_ = Vector::Zero(20);
}

void AudioInterface::speak(const std::string& text) {
    (void)text;  // Will be used when integrating with TTS system
    speaking_ = true;
    // In real implementation, send to TTS system
}

void AudioInterface::set_voice_parameters(double pitch, double rate, double volume) {
    voice_pitch_ = pitch;
    voice_rate_ = rate;
    voice_volume_ = volume;
}

void AudioInterface::process_audio_input(const Vector& audio_samples) {
    // Extract audio features
    if (audio_samples.size() > 0) {
        // Simple feature extraction
        audio_features_(0) = audio_samples.mean();
        audio_features_(1) = audio_samples.maxCoeff();
        audio_features_(2) = audio_samples.minCoeff();
        audio_features_(3) = std::sqrt((audio_samples.array() - audio_samples.mean()).square().mean());
    }
}

Vector AudioInterface::get_audio_features() const {
    return audio_features_;
}

std::string AudioInterface::get_transcription() const {
    return last_transcription_;
}

void AudioInterface::play_sound(const std::string& sound_id, double volume) {
    (void)sound_id;  // Will be used when integrating with Unreal audio
    (void)volume;    // Will be used when integrating with Unreal audio
    // Play sound effect in Unreal
}

Vector AudioInterface::get_audio_perception_vector() const {
    return audio_features_;
}

// ============================================================================
// UnrealAvatar Implementation
// ============================================================================

UnrealAvatar::UnrealAvatar() {}

void UnrealAvatar::initialize() {
    active_ = true;
}

void UnrealAvatar::attach_to_cognitive_system(std::shared_ptr<DeepTreeEcho> cognitive) {
    cognitive_system_ = cognitive;
    
    // Create avatar interface for cognitive system
    auto avatar_interface = std::make_shared<AvatarInterface>();
    cognitive_system_->attach_avatar(avatar_interface);
}

void UnrealAvatar::tick(double delta_time) {
    if (!active_ || !cognitive_system_) return;
    
    accumulated_time_ += delta_time;
    
    // Send perception to cognitive system
    send_perception_to_cognitive();
    
    // Run cognitive cycle
    cognitive_system_->run_cognitive_cycle();
    
    // Receive actions from cognitive system
    receive_action_from_cognitive();
}

void UnrealAvatar::send_perception_to_cognitive() {
    Vector perception = aggregate_perception();
    cognitive_system_->process_input(perception);
}

void UnrealAvatar::receive_action_from_cognitive() {
    Vector output = cognitive_system_->get_output();
    distribute_actions(output);
}

Vector UnrealAvatar::aggregate_perception() const {
    // Combine all perception sources
    Vector env_perception = environment_.get_perception_vector();
    Vector audio_perception = audio_.get_audio_perception_vector();
    Vector proprio = character_.get_proprioception();
    
    int total_size = static_cast<int>(env_perception.size() + 
                                       audio_perception.size() + 
                                       proprio.size());
    Vector combined(total_size);
    
    int offset = 0;
    combined.segment(offset, env_perception.size()) = env_perception;
    offset += static_cast<int>(env_perception.size());
    
    combined.segment(offset, audio_perception.size()) = audio_perception;
    offset += static_cast<int>(audio_perception.size());
    
    combined.segment(offset, proprio.size()) = proprio;
    
    return combined;
}

void UnrealAvatar::distribute_actions(const Vector& cognitive_output) {
    if (cognitive_output.size() == 0) return;
    
    // Distribute to character controller (movement)
    if (cognitive_output.size() >= 4) {
        character_.update_from_cognitive_output(cognitive_output.head(4));
    }
    
    // Distribute to facial controller (expression)
    if (cognitive_output.size() >= 16) {
        face_.update_from_cognitive_output(cognitive_output.segment(4, 12));
    }
    
    // Audio output would be handled separately through speech synthesis
}

std::string UnrealAvatar::get_status_report() const {
    std::ostringstream report;
    report << "=== Unreal Avatar Status ===" << std::endl;
    report << "Active: " << (active_ ? "Yes" : "No") << std::endl;
    report << "Cognitive System: " << (cognitive_system_ ? "Attached" : "Not Attached") << std::endl;
    report << "Accumulated Time: " << accumulated_time_ << "s" << std::endl;
    report << "Moving: " << (character_.is_moving() ? "Yes" : "No") << std::endl;
    report << "Grounded: " << (character_.is_grounded() ? "Yes" : "No") << std::endl;
    
    if (cognitive_system_) {
        report << "Cognitive Load: " << cognitive_system_->get_cognitive_load() << std::endl;
    }
    
    return report.str();
}

// ============================================================================
// Factory Function
// ============================================================================

std::shared_ptr<UnrealAvatar> create_avatar(const EchoConfig& config) {
    auto cognitive = std::make_shared<DeepTreeEcho>(config);
    cognitive->initialize();
    
    auto avatar = std::make_shared<UnrealAvatar>();
    avatar->attach_to_cognitive_system(cognitive);
    avatar->initialize();
    
    return avatar;
}

} // namespace unreal
} // namespace deep_tree_echo

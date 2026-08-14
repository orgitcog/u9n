#ifndef DEEP_TREE_ECHO_UNREAL_INTEGRATION_HPP
#define DEEP_TREE_ECHO_UNREAL_INTEGRATION_HPP

/**
 * @file deep_tree_echo/unreal_integration.hpp
 * @brief Unreal Engine Integration for Deep Tree Echo
 * 
 * Provides the interface between the Deep Tree Echo cognitive system
 * and Unreal Engine for avatar embodiment and world interaction.
 * 
 * Key Unreal Engine Functions Integrated:
 * - Character movement and animation
 * - Facial expression and lip sync
 * - Physics-based interaction
 * - Audio/visual perception
 * - Environment sensing
 */

#include "core.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace deep_tree_echo {
namespace unreal {

/**
 * @brief Animation blend space parameters
 */
struct BlendSpaceParams {
    double forward_speed = 0.0;
    double lateral_speed = 0.0;
    double turn_rate = 0.0;
    double stance_weight = 1.0;
    
    Vector to_vector() const {
        Vector v(4);
        v << forward_speed, lateral_speed, turn_rate, stance_weight;
        return v;
    }
    
    static BlendSpaceParams from_vector(const Vector& v) {
        BlendSpaceParams p;
        if (v.size() >= 4) {
            p.forward_speed = v(0);
            p.lateral_speed = v(1);
            p.turn_rate = v(2);
            p.stance_weight = v(3);
        }
        return p;
    }
};

/**
 * @brief Facial expression parameters (FACS-based)
 */
struct FacialExpression {
    // Action Units (simplified FACS)
    double brow_inner_up = 0.0;      // AU1
    double brow_outer_up = 0.0;      // AU2
    double brow_lowerer = 0.0;       // AU4
    double upper_lid_raise = 0.0;    // AU5
    double cheek_raise = 0.0;        // AU6
    double lid_tighten = 0.0;        // AU7
    double lip_corner_pull = 0.0;    // AU12 (smile)
    double lip_corner_depress = 0.0; // AU15
    double lip_stretch = 0.0;        // AU20
    double jaw_drop = 0.0;           // AU26
    
    // Viseme for lip sync
    int current_viseme = 0;
    double viseme_weight = 0.0;
    
    Vector to_vector() const {
        Vector v(12);
        v << brow_inner_up, brow_outer_up, brow_lowerer, upper_lid_raise,
             cheek_raise, lid_tighten, lip_corner_pull, lip_corner_depress,
             lip_stretch, jaw_drop, static_cast<double>(current_viseme), viseme_weight;
        return v;
    }
    
    static FacialExpression from_vector(const Vector& v) {
        FacialExpression e;
        if (v.size() >= 12) {
            e.brow_inner_up = v(0);
            e.brow_outer_up = v(1);
            e.brow_lowerer = v(2);
            e.upper_lid_raise = v(3);
            e.cheek_raise = v(4);
            e.lid_tighten = v(5);
            e.lip_corner_pull = v(6);
            e.lip_corner_depress = v(7);
            e.lip_stretch = v(8);
            e.jaw_drop = v(9);
            e.current_viseme = static_cast<int>(v(10));
            e.viseme_weight = v(11);
        }
        return e;
    }
};

/**
 * @brief Body pose parameters
 */
struct BodyPose {
    // Head orientation (euler angles in radians)
    double head_pitch = 0.0;
    double head_yaw = 0.0;
    double head_roll = 0.0;
    
    // Eye gaze direction
    double gaze_horizontal = 0.0;
    double gaze_vertical = 0.0;
    
    // Hand positions (relative to body)
    Vector left_hand_pos = Vector::Zero(3);
    Vector right_hand_pos = Vector::Zero(3);
    
    // Gesture state
    int gesture_id = 0;
    double gesture_progress = 0.0;
    
    Vector to_vector() const {
        Vector v(15);
        v << head_pitch, head_yaw, head_roll, gaze_horizontal, gaze_vertical,
             left_hand_pos(0), left_hand_pos(1), left_hand_pos(2),
             right_hand_pos(0), right_hand_pos(1), right_hand_pos(2),
             static_cast<double>(gesture_id), gesture_progress, 0.0, 0.0;
        return v;
    }
};

/**
 * @brief Environment perception data
 */
struct EnvironmentPerception {
    // Visual scene understanding
    std::vector<std::string> detected_objects;
    std::vector<Vector> object_positions;
    
    // Spatial awareness
    Vector agent_position = Vector::Zero(3);
    Vector agent_forward = Vector::Zero(3);
    double nearest_obstacle_distance = 100.0;
    
    // Social awareness
    std::vector<Vector> other_agent_positions;
    std::vector<double> attention_weights;
    
    // Audio perception
    double ambient_noise_level = 0.0;
    Vector sound_direction = Vector::Zero(3);
};

/**
 * @brief Unreal Engine Character Controller Interface
 * 
 * Provides the bridge between cognitive output and character control.
 */
class CharacterController {
public:
    CharacterController();
    
    // Movement control
    void set_movement_input(const BlendSpaceParams& params);
    void set_target_location(const Vector& location);
    void set_target_rotation(double yaw);
    
    // Animation control
    void play_animation(const std::string& anim_name, double blend_time = 0.2);
    void set_animation_parameter(const std::string& param, double value);
    void trigger_animation_event(const std::string& event_name);
    
    // Physics interaction
    void apply_impulse(const Vector& impulse);
    void grab_object(const std::string& object_id);
    void release_object();
    
    // State queries
    BlendSpaceParams get_current_movement() const { return current_movement_; }
    bool is_grounded() const { return grounded_; }
    bool is_moving() const { return moving_; }
    
    // Cognitive integration
    void update_from_cognitive_output(const Vector& output);
    Vector get_proprioception() const;
    
private:
    BlendSpaceParams current_movement_;
    Vector target_location_;
    double target_yaw_ = 0.0;
    bool grounded_ = true;
    bool moving_ = false;
    
    // Animation state
    std::string current_animation_;
    std::unordered_map<std::string, double> anim_params_;
};

/**
 * @brief Facial Animation Controller
 * 
 * Controls facial expressions and lip sync for the avatar.
 */
class FacialController {
public:
    FacialController();
    
    // Expression control
    void set_expression(const FacialExpression& expr);
    void blend_to_expression(const FacialExpression& target, double duration);
    void set_emotion(const std::string& emotion, double intensity);
    
    // Lip sync
    void set_viseme(int viseme_id, double weight);
    void process_audio_for_lipsync(const Vector& audio_features);
    
    // Eye control
    void set_gaze_target(const Vector& world_position);
    void set_blink_rate(double blinks_per_minute);
    
    // State
    FacialExpression get_current_expression() const { return current_expression_; }
    
    // Cognitive integration
    void update_from_cognitive_output(const Vector& output);
    Vector get_expression_state() const;
    
private:
    FacialExpression current_expression_;
    FacialExpression target_expression_;
    double blend_progress_ = 1.0;
    double blend_duration_ = 0.0;
    
    Vector gaze_target_;
    double blink_timer_ = 0.0;
    double blink_rate_ = 15.0; // blinks per minute
};

/**
 * @brief Environment Interface
 * 
 * Handles perception of and interaction with the Unreal world.
 */
class EnvironmentInterface {
public:
    EnvironmentInterface();
    
    // Perception
    void update_perception(const EnvironmentPerception& perception);
    Vector get_perception_vector() const;
    
    // Object interaction
    void interact_with_object(const std::string& object_id, const std::string& action);
    std::vector<std::string> get_interactable_objects() const;
    
    // Navigation
    Vector find_path_to(const Vector& destination);
    bool is_path_clear(const Vector& direction, double distance);
    
    // Social awareness
    Vector get_nearest_agent_direction() const;
    double get_social_distance() const;
    
private:
    EnvironmentPerception current_perception_;
    std::vector<std::string> interactable_objects_;
};

/**
 * @brief Audio System Interface
 * 
 * Handles audio input/output for the avatar.
 */
class AudioInterface {
public:
    AudioInterface();
    
    // Speech synthesis
    void speak(const std::string& text);
    void set_voice_parameters(double pitch, double rate, double volume);
    bool is_speaking() const { return speaking_; }
    
    // Audio perception
    void process_audio_input(const Vector& audio_samples);
    Vector get_audio_features() const;
    std::string get_transcription() const;
    
    // Sound effects
    void play_sound(const std::string& sound_id, double volume = 1.0);
    
    // Cognitive integration
    Vector get_audio_perception_vector() const;
    
private:
    bool speaking_ = false;
    Vector audio_features_;
    std::string last_transcription_;
    
    double voice_pitch_ = 1.0;
    double voice_rate_ = 1.0;
    double voice_volume_ = 1.0;
};

/**
 * @brief Unreal Avatar - Complete avatar integration
 * 
 * Integrates all Unreal Engine subsystems with the Deep Tree Echo
 * cognitive architecture for full embodied cognition.
 */
class UnrealAvatar {
public:
    UnrealAvatar();
    
    // Initialization
    void initialize();
    void attach_to_cognitive_system(std::shared_ptr<DeepTreeEcho> cognitive);
    
    // Main update loop
    void tick(double delta_time);
    
    // Subsystem access
    CharacterController& character() { return character_; }
    FacialController& face() { return face_; }
    EnvironmentInterface& environment() { return environment_; }
    AudioInterface& audio() { return audio_; }
    
    // Cognitive bridge
    void send_perception_to_cognitive();
    void receive_action_from_cognitive();
    
    // State
    bool is_active() const { return active_; }
    void set_active(bool active) { active_ = active; }
    
    // Debug
    std::string get_status_report() const;
    
private:
    std::shared_ptr<DeepTreeEcho> cognitive_system_;
    
    CharacterController character_;
    FacialController face_;
    EnvironmentInterface environment_;
    AudioInterface audio_;
    
    bool active_ = false;
    double accumulated_time_ = 0.0;
    
    // Perception aggregation
    Vector aggregate_perception() const;
    
    // Action distribution
    void distribute_actions(const Vector& cognitive_output);
};

/**
 * @brief Create a fully configured Deep Tree Echo avatar
 */
std::shared_ptr<UnrealAvatar> create_avatar(const EchoConfig& config);

} // namespace unreal
} // namespace deep_tree_echo

#endif // DEEP_TREE_ECHO_UNREAL_INTEGRATION_HPP

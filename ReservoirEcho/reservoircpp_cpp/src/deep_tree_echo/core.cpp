/**
 * @file deep_tree_echo/core.cpp
 * @brief Implementation of Deep Tree Echo Cognitive Framework
 */

#include "deep_tree_echo/core.hpp"
#include <random>
#include <algorithm>
#include <sstream>

namespace deep_tree_echo {

// ============================================================================
// EchoState Implementation
// ============================================================================

EchoState::EchoState(const EchoConfig& config) : config_(config) {
    activation_ = reservoircpp::activations::get_function(config.activation);
}

void EchoState::initialize() {
    std::mt19937 gen(config_.seed == 0 ? std::random_device{}() : config_.seed);
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    std::normal_distribution<double> normal(0.0, 1.0);

    // Initialize reservoir weights with sparsity
    W_ = Matrix::Zero(config_.reservoir_size, config_.reservoir_size);
    for (int i = 0; i < config_.reservoir_size; ++i) {
        for (int j = 0; j < config_.reservoir_size; ++j) {
            if (dist(gen) < config_.sparsity) {
                W_(i, j) = normal(gen);
            }
        }
    }
    
    scale_spectral_radius();
    
    // Initialize input weights
    Win_ = Matrix::Zero(config_.reservoir_size, 1);
    for (int i = 0; i < config_.reservoir_size; ++i) {
        Win_(i, 0) = dist(gen) * config_.input_scaling;
    }
    
    // Initialize feedback weights
    Wfb_ = Matrix::Zero(config_.reservoir_size, 1);
    for (int i = 0; i < config_.reservoir_size; ++i) {
        Wfb_(i, 0) = dist(gen) * config_.feedback_scaling;
    }
    
    // Initialize bias
    if (config_.use_bias) {
        bias_ = Vector::Zero(config_.reservoir_size);
        for (int i = 0; i < config_.reservoir_size; ++i) {
            bias_(i) = dist(gen) * 0.1;
        }
    }
    
    // Initialize state
    state_ = Vector::Zero(config_.reservoir_size);
    
    initialized_ = true;
}

void EchoState::initialize_from_weights(const Matrix& W, const Matrix& Win) {
    W_ = W;
    Win_ = Win;
    config_.reservoir_size = static_cast<int>(W.rows());
    state_ = Vector::Zero(config_.reservoir_size);
    initialized_ = true;
}

void EchoState::scale_spectral_radius() {
    Eigen::EigenSolver<Matrix> solver(W_, false);
    double current_sr = solver.eigenvalues().cwiseAbs().maxCoeff();
    if (current_sr > 0) {
        W_ *= config_.spectral_radius / current_sr;
    }
}

Vector EchoState::compute_state(const Vector& input) {
    if (!initialized_) {
        throw std::runtime_error("EchoState not initialized");
    }
    
    // Resize input weights if needed
    if (Win_.cols() != input.size()) {
        std::mt19937 gen(config_.seed);
        std::uniform_real_distribution<double> dist(-1.0, 1.0);
        Win_ = Matrix::Zero(config_.reservoir_size, input.size());
        for (int i = 0; i < config_.reservoir_size; ++i) {
            for (int j = 0; j < input.size(); ++j) {
                Win_(i, j) = dist(gen) * config_.input_scaling;
            }
        }
    }
    
    // Compute pre-activation
    Vector pre_activation = Win_ * input + W_ * state_;
    
    if (config_.use_bias) {
        pre_activation += bias_;
    }
    
    // Add noise
    if (config_.noise_level > 0) {
        static std::mt19937 noise_gen(42);
        std::normal_distribution<double> noise_dist(0.0, config_.noise_level);
        for (int i = 0; i < pre_activation.size(); ++i) {
            pre_activation(i) += noise_dist(noise_gen);
        }
    }
    
    // Apply activation and leaky integration
    Array activated = activation_(pre_activation.array().matrix()).array();
    state_ = (1.0 - config_.leak_rate) * state_.array() + 
             config_.leak_rate * activated;
    
    return state_;
}

Vector EchoState::compute_state(const Vector& input, const Vector& feedback) {
    if (!initialized_) {
        throw std::runtime_error("EchoState not initialized");
    }
    
    // Resize feedback weights if needed
    if (Wfb_.cols() != feedback.size()) {
        std::mt19937 gen(config_.seed);
        std::uniform_real_distribution<double> dist(-1.0, 1.0);
        Wfb_ = Matrix::Zero(config_.reservoir_size, feedback.size());
        for (int i = 0; i < config_.reservoir_size; ++i) {
            for (int j = 0; j < feedback.size(); ++j) {
                Wfb_(i, j) = dist(gen) * config_.feedback_scaling;
            }
        }
    }
    
    // Resize input weights if needed
    if (Win_.cols() != input.size()) {
        std::mt19937 gen(config_.seed);
        std::uniform_real_distribution<double> dist(-1.0, 1.0);
        Win_ = Matrix::Zero(config_.reservoir_size, input.size());
        for (int i = 0; i < config_.reservoir_size; ++i) {
            for (int j = 0; j < input.size(); ++j) {
                Win_(i, j) = dist(gen) * config_.input_scaling;
            }
        }
    }
    
    // Compute pre-activation with feedback
    Vector pre_activation = Win_ * input + W_ * state_ + Wfb_ * feedback;
    
    if (config_.use_bias) {
        pre_activation += bias_;
    }
    
    // Apply activation and leaky integration
    Array activated = activation_(pre_activation.array().matrix()).array();
    state_ = (1.0 - config_.leak_rate) * state_.array() + 
             config_.leak_rate * activated;
    
    return state_;
}

Matrix EchoState::compute_states(const Matrix& inputs) {
    int n_samples = static_cast<int>(inputs.rows());
    Matrix states(n_samples, config_.reservoir_size);
    
    reset();
    
    for (int t = 0; t < n_samples; ++t) {
        Vector state = compute_state(inputs.row(t).transpose());
        states.row(t) = state.transpose();
    }
    
    return states;
}

void EchoState::fit(const Matrix& inputs, const Matrix& targets, double ridge) {
    Matrix states = compute_states(inputs);
    
    // Ridge regression
    Matrix StS = states.transpose() * states;
    StS.diagonal().array() += ridge;
    
    Wout_ = StS.ldlt().solve(states.transpose() * targets).transpose();
    trained_ = true;
}

Matrix EchoState::predict(const Matrix& inputs) {
    if (!trained_) {
        throw std::runtime_error("EchoState not trained");
    }
    
    Matrix states = compute_states(inputs);
    return states * Wout_.transpose();
}

void EchoState::reset() {
    state_ = Vector::Zero(config_.reservoir_size);
}

double EchoState::get_spectral_radius() const {
    Eigen::EigenSolver<Matrix> solver(W_, false);
    return solver.eigenvalues().cwiseAbs().maxCoeff();
}

double EchoState::get_memory_capacity() const {
    // Simplified memory capacity estimate
    return config_.reservoir_size * (1.0 - config_.sparsity) * config_.leak_rate;
}

// ============================================================================
// CognitiveNode Implementation
// ============================================================================

CognitiveNode::CognitiveNode(const std::string& name, MembraneType type)
    : name_(name), type_(type) {}

Vector CognitiveNode::process(const Vector& input) {
    if (echo_state_) {
        output_ = echo_state_->compute_state(input);
    } else {
        output_ = input;
    }
    return output_;
}

void CognitiveNode::update() {
    // Aggregate incoming echoes
    if (!incoming_echoes_.empty()) {
        Vector aggregate = Vector::Zero(output_.size());
        for (const auto& [source, echo] : incoming_echoes_) {
            if (echo.size() == aggregate.size()) {
                aggregate += echo;
            }
        }
        aggregate /= static_cast<double>(incoming_echoes_.size());
        
        // Blend with current output
        output_ = 0.7 * output_ + 0.3 * aggregate;
    }
}

void CognitiveNode::connect_to(std::shared_ptr<CognitiveNode> target, double weight) {
    connections_.emplace_back(target, weight);
}

void CognitiveNode::receive_echo(const Vector& echo, const std::string& source) {
    incoming_echoes_[source] = echo;
}

// ============================================================================
// MembraneLayer Implementation
// ============================================================================

MembraneLayer::MembraneLayer(const std::string& name, MembraneType type)
    : name_(name), type_(type) {}

void MembraneLayer::add_node(std::shared_ptr<CognitiveNode> node) {
    nodes_.push_back(node);
}

std::shared_ptr<CognitiveNode> MembraneLayer::get_node(const std::string& name) {
    for (auto& node : nodes_) {
        if (node->name() == name) {
            return node;
        }
    }
    return nullptr;
}

void MembraneLayer::process_all(const Vector& input) {
    for (auto& node : nodes_) {
        node->process(input);
    }
}

void MembraneLayer::propagate_echoes(PropagationMode mode) {
    // Propagate within this membrane
    for (auto& node : nodes_) {
        node->update();
    }
    
    // Propagate to children
    if (mode == PropagationMode::FEEDFORWARD || mode == PropagationMode::RESONANCE) {
        Vector aggregate = get_aggregate_output();
        for (auto& child : children_) {
            child->process_all(aggregate);
        }
    }
}

void MembraneLayer::add_child_membrane(std::shared_ptr<MembraneLayer> child) {
    children_.push_back(child);
}

std::shared_ptr<MembraneLayer> MembraneLayer::get_child(const std::string& name) {
    for (auto& child : children_) {
        if (child->name() == name) {
            return child;
        }
    }
    return nullptr;
}

Vector MembraneLayer::get_aggregate_output() const {
    if (nodes_.empty()) {
        return Vector();
    }
    
    Vector aggregate = nodes_[0]->get_output();
    for (size_t i = 1; i < nodes_.size(); ++i) {
        Vector output = nodes_[i]->get_output();
        if (output.size() == aggregate.size()) {
            aggregate += output;
        }
    }
    
    return aggregate / static_cast<double>(nodes_.size());
}

// ============================================================================
// AvatarInterface Implementation
// ============================================================================

AvatarInterface::AvatarInterface() {
    movement_state_ = Vector::Zero(4);
    expression_state_ = Vector::Zero(12);
    speech_state_ = Vector::Zero(8);
}

void AvatarInterface::receive_visual(const Matrix& visual_data) {
    visual_buffer_ = visual_data;
}

void AvatarInterface::receive_audio(const Vector& audio_data) {
    audio_buffer_ = audio_data;
}

void AvatarInterface::receive_proprioception(const Vector& body_state) {
    proprioception_buffer_ = body_state;
}

Vector AvatarInterface::generate_movement() {
    return movement_state_;
}

Vector AvatarInterface::generate_expression() {
    return expression_state_;
}

Vector AvatarInterface::generate_speech_params() {
    return speech_state_;
}

void AvatarInterface::update_environment_state(const Vector& env_state) {
    environment_state_ = env_state;
}

Vector AvatarInterface::get_environment_response() const {
    return environment_state_;
}

void AvatarInterface::register_tool(const std::string& name, 
                                     std::function<Vector(const Vector&)> tool) {
    tools_[name] = tool;
}

Vector AvatarInterface::use_tool(const std::string& name, const Vector& input) {
    auto it = tools_.find(name);
    if (it != tools_.end()) {
        return it->second(input);
    }
    return input;
}

void AvatarInterface::process_cycle() {
    if (!cognitive_core_) return;
    
    // Integrate sensory input
    Vector integrated = integrate_sensory_input();
    
    // Process through cognitive core
    cognitive_core_->process_all(integrated);
    cognitive_core_->propagate_echoes(PropagationMode::RESONANCE);
    
    // Update motor output
    Vector cognitive_output = cognitive_core_->get_aggregate_output();
    update_motor_output(cognitive_output);
}

Vector AvatarInterface::integrate_sensory_input() {
    // Combine all sensory modalities
    int total_size = 0;
    if (visual_buffer_.size() > 0) total_size += static_cast<int>(visual_buffer_.cols());
    if (audio_buffer_.size() > 0) total_size += static_cast<int>(audio_buffer_.size());
    if (proprioception_buffer_.size() > 0) total_size += static_cast<int>(proprioception_buffer_.size());
    
    if (total_size == 0) {
        return Vector::Zero(10);
    }
    
    Vector integrated(total_size);
    int offset = 0;
    
    if (visual_buffer_.size() > 0) {
        // Use mean of visual buffer columns
        Vector visual_mean = visual_buffer_.colwise().mean().transpose();
        integrated.segment(offset, visual_mean.size()) = visual_mean;
        offset += static_cast<int>(visual_mean.size());
    }
    
    if (audio_buffer_.size() > 0) {
        integrated.segment(offset, audio_buffer_.size()) = audio_buffer_;
        offset += static_cast<int>(audio_buffer_.size());
    }
    
    if (proprioception_buffer_.size() > 0) {
        integrated.segment(offset, proprioception_buffer_.size()) = proprioception_buffer_;
    }
    
    return integrated;
}

void AvatarInterface::update_motor_output(const Vector& cognitive_output) {
    if (cognitive_output.size() == 0) return;
    
    // Distribute cognitive output to motor channels
    int movement_size = std::min(4, static_cast<int>(cognitive_output.size()));
    movement_state_.head(movement_size) = cognitive_output.head(movement_size);
    
    if (cognitive_output.size() > 4) {
        int expr_size = std::min(12, static_cast<int>(cognitive_output.size()) - 4);
        expression_state_.head(expr_size) = cognitive_output.segment(4, expr_size);
    }
    
    if (cognitive_output.size() > 16) {
        int speech_size = std::min(8, static_cast<int>(cognitive_output.size()) - 16);
        speech_state_.head(speech_size) = cognitive_output.segment(16, speech_size);
    }
}

// ============================================================================
// DeepTreeEcho Implementation
// ============================================================================

DeepTreeEcho::DeepTreeEcho() {
    config_.reservoir_size = 500;
}

DeepTreeEcho::DeepTreeEcho(const EchoConfig& config) : config_(config) {}

void DeepTreeEcho::initialize() {
    // Create echo states for different cognitive functions
    perception_echo_ = std::make_shared<EchoState>(config_);
    perception_echo_->initialize();
    
    EchoConfig reasoning_config = config_;
    reasoning_config.reservoir_size = static_cast<int>(config_.reservoir_size * 1.5);
    reasoning_config.spectral_radius = 0.98;
    reasoning_echo_ = std::make_shared<EchoState>(reasoning_config);
    reasoning_echo_->initialize();
    
    EchoConfig memory_config = config_;
    memory_config.leak_rate = 0.1; // Slower leak for memory
    memory_echo_ = std::make_shared<EchoState>(memory_config);
    memory_echo_->initialize();
    
    EchoConfig motor_config = config_;
    motor_config.reservoir_size = static_cast<int>(config_.reservoir_size * 0.5);
    motor_config.leak_rate = 0.5; // Faster response for motor
    motor_echo_ = std::make_shared<EchoState>(motor_config);
    motor_echo_->initialize();
    
    // Build membrane hierarchy
    build_membrane_hierarchy();
    
    initialized_ = true;
}

void DeepTreeEcho::build_membrane_hierarchy() {
    // Create root membrane
    root_membrane_ = std::make_shared<MembraneLayer>("root", MembraneType::ROOT);
    
    // Cognitive membrane
    auto cognitive = std::make_shared<MembraneLayer>("cognitive", MembraneType::COGNITIVE);
    root_membrane_->add_child_membrane(cognitive);
    
    // Perception node
    auto perception_node = std::make_shared<CognitiveNode>("perception", MembraneType::SENSORY);
    perception_node->set_echo_state(perception_echo_);
    cognitive->add_node(perception_node);
    
    // Reasoning node
    auto reasoning_node = std::make_shared<CognitiveNode>("reasoning", MembraneType::REASONING);
    reasoning_node->set_echo_state(reasoning_echo_);
    cognitive->add_node(reasoning_node);
    
    // Memory membrane
    auto memory = std::make_shared<MembraneLayer>("memory", MembraneType::MEMORY);
    cognitive->add_child_membrane(memory);
    
    auto memory_node = std::make_shared<CognitiveNode>("episodic", MembraneType::MEMORY);
    memory_node->set_echo_state(memory_echo_);
    memory->add_node(memory_node);
    
    // Motor membrane
    auto motor = std::make_shared<MembraneLayer>("motor", MembraneType::MOTOR);
    root_membrane_->add_child_membrane(motor);
    
    auto motor_node = std::make_shared<CognitiveNode>("action", MembraneType::MOTOR);
    motor_node->set_echo_state(motor_echo_);
    motor->add_node(motor_node);
    
    // Connect nodes
    perception_node->connect_to(reasoning_node, 1.0);
    reasoning_node->connect_to(memory_node, 0.8);
    reasoning_node->connect_to(motor_node, 0.9);
    memory_node->connect_to(reasoning_node, 0.5);
}

std::shared_ptr<MembraneLayer> DeepTreeEcho::get_membrane(const std::string& name) {
    if (root_membrane_->name() == name) {
        return root_membrane_;
    }
    return root_membrane_->get_child(name);
}

void DeepTreeEcho::process_input(const Vector& input) {
    if (!initialized_) {
        throw std::runtime_error("DeepTreeEcho not initialized");
    }
    
    propagate_through_hierarchy(input);
    synchronize_echoes();
    
    if (avatar_) {
        update_avatar_state();
    }
}

Vector DeepTreeEcho::get_output() const {
    return current_output_;
}

void DeepTreeEcho::run_cognitive_cycle() {
    if (avatar_) {
        avatar_->process_cycle();
    }
    
    root_membrane_->propagate_echoes(PropagationMode::RESONANCE);
    current_output_ = root_membrane_->get_aggregate_output();
}

void DeepTreeEcho::attach_avatar(std::shared_ptr<AvatarInterface> avatar) {
    avatar_ = avatar;
    avatar_->set_cognitive_core(root_membrane_);
}

void DeepTreeEcho::train(const Matrix& inputs, const Matrix& targets) {
    // Train all echo states
    perception_echo_->fit(inputs, targets);
    reasoning_echo_->fit(inputs, targets);
    memory_echo_->fit(inputs, targets);
    motor_echo_->fit(inputs, targets);
}

void DeepTreeEcho::online_learn(const Vector& input, const Vector& target) {
    // Simple online learning update
    process_input(input);
    
    // Compute error
    Vector error = target - current_output_;
    
    // Update states based on error (simplified)
    // In a full implementation, this would use proper online learning algorithms
}

std::string DeepTreeEcho::get_state_report() const {
    std::ostringstream report;
    report << "=== Deep Tree Echo State Report ===" << std::endl;
    report << "Initialized: " << (initialized_ ? "Yes" : "No") << std::endl;
    report << "Reservoir Size: " << config_.reservoir_size << std::endl;
    report << "Spectral Radius: " << config_.spectral_radius << std::endl;
    report << "Leak Rate: " << config_.leak_rate << std::endl;
    
    if (perception_echo_) {
        report << "Perception Echo SR: " << perception_echo_->get_spectral_radius() << std::endl;
    }
    if (reasoning_echo_) {
        report << "Reasoning Echo SR: " << reasoning_echo_->get_spectral_radius() << std::endl;
    }
    
    report << "Avatar Attached: " << (avatar_ ? "Yes" : "No") << std::endl;
    
    return report.str();
}

double DeepTreeEcho::get_cognitive_load() const {
    // Estimate cognitive load from echo state activities
    double load = 0.0;
    
    if (perception_echo_) {
        load += perception_echo_->get_state().norm();
    }
    if (reasoning_echo_) {
        load += reasoning_echo_->get_state().norm();
    }
    if (memory_echo_) {
        load += memory_echo_->get_state().norm();
    }
    if (motor_echo_) {
        load += motor_echo_->get_state().norm();
    }
    
    return load / 4.0;
}

std::vector<std::string> DeepTreeEcho::get_active_membranes() const {
    std::vector<std::string> active;
    active.push_back(root_membrane_->name());
    // Add children recursively in a full implementation
    return active;
}

void DeepTreeEcho::propagate_through_hierarchy(const Vector& input) {
    root_membrane_->process_all(input);
    root_membrane_->propagate_echoes(PropagationMode::FEEDFORWARD);
    current_output_ = root_membrane_->get_aggregate_output();
}

void DeepTreeEcho::synchronize_echoes() {
    // Synchronize echo states across the hierarchy
    // This implements the "resonance" aspect of the cognitive architecture
}

void DeepTreeEcho::update_avatar_state() {
    if (!avatar_) return;
    
    // Send cognitive output to avatar
    avatar_->receive_proprioception(current_output_);
}

} // namespace deep_tree_echo

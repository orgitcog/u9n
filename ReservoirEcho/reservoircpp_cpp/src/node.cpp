/**
 * @file node.cpp
 * @brief Implementation of reservoircpp::Node base class
 *
 * Provides the method bodies declared in include/reservoircpp/node.hpp.
 * Previously this file was an empty stub auto-generated from Python,
 * causing linker errors for every symbol in the Node vtable.
 */
#include "reservoircpp/node.hpp"
#include <stdexcept>

namespace reservoircpp {

// Constructor
Node::Node(const std::string& name)
    : name_(name),
      fitted_(false),
      input_dim_(0),
      output_dim_(0),
      has_feedback_(false),
      batch_size_(1),
      state_() {}

// Call operator
Eigen::MatrixXd Node::operator()(const Eigen::MatrixXd& X) {
    state_ = _call(X);
    return state_;
}

void Node::reset() {
    state_ = Eigen::MatrixXd();
    fitted_ = false;
}

void Node::initialize(const Eigen::MatrixXd& X) {
    if (X.cols() > 0) {
        input_dim_ = static_cast<int>(X.cols());
    }
    if (output_dim_ == 0) {
        output_dim_ = input_dim_;
    }
    state_ = Eigen::MatrixXd::Zero(1, output_dim_);
}

Node& Node::fit(const Eigen::MatrixXd& X, const Eigen::MatrixXd& Y,
                 bool reset_state) {
    if (reset_state) {
        state_ = Eigen::MatrixXd::Zero(1, output_dim_);
    }
    _fit(X, Y);
    fitted_ = true;
    return *this;
}

Eigen::MatrixXd Node::run(const Eigen::MatrixXd& X, bool reset_state) {
    if (reset_state) {
        state_ = Eigen::MatrixXd::Zero(1, output_dim_);
    }
    Eigen::MatrixXd result(X.rows(), output_dim_);
    for (Eigen::Index i = 0; i < X.rows(); ++i) {
        result.row(i) = (*this)(X.row(i));
    }
    return result;
}

std::string Node::name() const { return name_; }
void Node::set_name(const std::string& name) { name_ = name; }
bool Node::is_fitted() const { return fitted_; }
Eigen::MatrixXd Node::state() const { return state_; }
int Node::input_dim() const { return input_dim_; }
int Node::output_dim() const { return output_dim_; }

std::shared_ptr<Node> Node::with_feedback(std::shared_ptr<Node> feedback_node) {
    feedback_nodes_.push_back(feedback_node);
    has_feedback_ = true;
    return shared_from_this();
}

bool Node::has_feedback() const { return has_feedback_; }

const std::vector<std::shared_ptr<Node>>& Node::feedback_nodes() const {
    return feedback_nodes_;
}

Node& Node::train(const Eigen::MatrixXd& X, const Eigen::MatrixXd& Y,
                   bool reset_state) {
    return fit(X, Y, reset_state);
}

} // namespace reservoircpp

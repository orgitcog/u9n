/**
 * @file model.cpp
 * @brief Implementation of reservoircpp::Model class
 *
 * Provides the method bodies declared in include/reservoircpp/model.hpp.
 * Previously this file was an empty stub auto-generated from Python,
 * causing linker errors when model_test tried to use Model.
 */
#include "reservoircpp/model.hpp"
#include <stdexcept>
#include <algorithm>

namespace reservoircpp {

// Constructor
Model::Model(const std::vector<std::shared_ptr<Node>>& nodes,
             const std::vector<std::pair<std::string, std::string>>& edges,
             const std::string& name)
    : Node(name), edges_(edges) {
    for (auto& n : nodes) {
        add_node(n);
    }
}

Model& Model::add_node(std::shared_ptr<Node> node, const std::string& name) {
    std::string key = name.empty() ? node->name() : name;
    nodes_.push_back(node);
    node_map_[key] = node;
    return *this;
}

Model& Model::connect(const std::string& from_node,
                       const std::string& to_node) {
    edges_.emplace_back(from_node, to_node);
    return *this;
}

void Model::update_graph() {
    ordered_nodes_.clear();
    ordered_nodes_ = nodes_;
}

std::shared_ptr<Node> Model::get_node(const std::string& name_or_index) {
    auto it = node_map_.find(name_or_index);
    if (it != node_map_.end()) return it->second;
    throw std::runtime_error("Node not found: " + name_or_index);
}

void Model::initialize(const Eigen::MatrixXd& X) {
    Node::initialize(X);
    for (auto& n : nodes_) {
        n->initialize(X);
    }
}

void Model::reset() {
    Node::reset();
    for (auto& n : nodes_) {
        n->reset();
    }
}

Model& Model::with_feedback(const std::string& from_node,
                             const std::string& to_node) {
    feedback_edges_.emplace_back(from_node, to_node);
    auto src = get_node(from_node);
    auto dst = get_node(to_node);
    dst->with_feedback(src);
    return *this;
}

Eigen::MatrixXd Model::_call(const Eigen::MatrixXd& X) {
    if (nodes_.empty()) return X;
    Eigen::MatrixXd data = X;
    for (auto& n : nodes_) {
        data = (*n)(data);
    }
    state_ = data;
    return data;
}

void Model::_fit(const Eigen::MatrixXd& X, const Eigen::MatrixXd& Y) {
    for (auto& n : nodes_) {
        n->fit(X, Y);
    }
    fitted_ = true;
}

} // namespace reservoircpp

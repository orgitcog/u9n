#ifndef RESERVOIRCPP_MODEL_HPP
#define RESERVOIRCPP_MODEL_HPP

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <Eigen/Dense>
#include "node.hpp"

namespace reservoircpp {

/**
 * Model class - for combining nodes into a computational graph
 * 
 * This is equivalent to the Python Model class in ReservoirPy
 */
class Model : public Node {
public:
    /**
     * Constructor
     * 
     * @param nodes Vector of nodes to combine
     * @param edges Vector of edge connections between nodes
     * @param name Name of the model
     */
    Model(const std::vector<std::shared_ptr<Node>>& nodes = {},
          const std::vector<std::pair<std::string, std::string>>& edges = {},
          const std::string& name = "");
    
    /**
     * Add a node to the model
     * 
     * @param node Node to add
     * @param name Optional name for the node
     * @return Reference to this model
     */
    Model& add_node(std::shared_ptr<Node> node, const std::string& name = "");
    
    /**
     * Connect nodes in the model
     * 
     * @param from_node Name or index of source node
     * @param to_node Name or index of target node
     * @return Reference to this model
     */
    Model& connect(const std::string& from_node, const std::string& to_node);
    
    /**
     * Update the computational graph
     */
    void update_graph();
    
    /**
     * Get a node by name or index
     * 
     * @param name_or_index Name or index of the node to retrieve
     * @return Pointer to the node
     */
    std::shared_ptr<Node> get_node(const std::string& name_or_index);
    
    /**
     * Initialize all nodes in the model
     * 
     * @param X Initial input data
     */
    void initialize(const Eigen::MatrixXd& X) override;
    
    /**
     * Reset all nodes in the model
     */
    void reset() override;
    
    /**
     * Add feedback connections to the model
     * 
     * @param from_node Name or index of source node
     * @param to_node Name or index of target node
     * @return Reference to this model
     */
    Model& with_feedback(const std::string& from_node, const std::string& to_node);

protected:
    std::vector<std::shared_ptr<Node>> nodes_;
    std::vector<std::pair<std::string, std::string>> edges_;
    std::unordered_map<std::string, std::shared_ptr<Node>> node_map_;
    std::vector<std::pair<std::string, std::string>> feedback_edges_;
    std::vector<std::string> input_nodes_;
    std::vector<std::string> output_nodes_;
    std::vector<std::shared_ptr<Node>> ordered_nodes_;
    
    /**
     * Internal implementation of call operation
     * 
     * @param X Input data
     * @return Output of the model
     */
    Eigen::MatrixXd _call(const Eigen::MatrixXd& X) override;
    
    /**
     * Internal implementation of fit operation
     * 
     * @param X Input training data
     * @param Y Target output data
     */
    void _fit(const Eigen::MatrixXd& X, const Eigen::MatrixXd& Y) override;
};

} // namespace reservoircpp

#endif // RESERVOIRCPP_MODEL_HPP

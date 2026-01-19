#ifndef RESERVOIRCPP_NODE_HPP
#define RESERVOIRCPP_NODE_HPP

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <Eigen/Dense>
#include "numpy.hpp"

namespace reservoircpp {

/**
 * Node class - base class for all reservoir computing components
 * 
 * This is equivalent to the Python Node class in ReservoirPy
 */
class Node : public std::enable_shared_from_this<Node> {
public:
    /**
     * Constructor
     * 
     * @param name Name of the node
     */
    explicit Node(const std::string& name = "");
    
    /**
     * Virtual destructor
     */
    virtual ~Node() = default;
    
    /**
     * Call the node with input data
     * 
     * @param X Input data
     * @return Output of the node
     */
    virtual Eigen::MatrixXd operator()(const Eigen::MatrixXd& X);
    
    /**
     * Reset the node state
     */
    virtual void reset();
    
    /**
     * Initialize the node with initial states
     * 
     * @param X Initial input data for initialization
     */
    virtual void initialize(const Eigen::MatrixXd& X);
    
    /**
     * Fit/train the node with training data
     * 
     * @param X Input training data
     * @param Y Target output data
     * @param reset_state Whether to reset state before fitting
     * @return This node instance
     */
    virtual Node& fit(const Eigen::MatrixXd& X, const Eigen::MatrixXd& Y, bool reset_state = true);
    
    /**
     * Run the node on input data
     * 
     * @param X Input data
     * @param reset_state Whether to reset state before running
     * @return Output of the node
     */
    virtual Eigen::MatrixXd run(const Eigen::MatrixXd& X, bool reset_state = true);
    
    /**
     * Get the name of the node
     * 
     * @return Name of the node
     */
    std::string name() const;
    
    /**
     * Set the name of the node
     * 
     * @param name New name for the node
     */
    void set_name(const std::string& name);
    
    /**
     * Check if the node is fitted/trained
     * 
     * @return True if the node is fitted, false otherwise
     */
    bool is_fitted() const;
    
    /**
     * Get the current state of the node
     * 
     * @return Current state matrix
     */
    Eigen::MatrixXd state() const;
    
    /**
     * Get the input dimension of the node
     * 
     * @return Input dimension
     */
    int input_dim() const;
    
    /**
     * Get the output dimension of the node
     * 
     * @return Output dimension
     */
    int output_dim() const;
    
    /**
     * Add a feedback connection from another node
     * 
     * @param feedback_node Node to take feedback from
     * @return Shared pointer to this node
     */
    std::shared_ptr<Node> with_feedback(std::shared_ptr<Node> feedback_node);
    
    /**
     * Check if the node has feedback connections
     * 
     * @return True if the node has feedback connections, false otherwise
     */
    bool has_feedback() const;
    
    /**
     * Get the feedback nodes
     * 
     * @return Vector of feedback nodes
     */
    const std::vector<std::shared_ptr<Node>>& feedback_nodes() const;
    
    /**
     * Alias for the fit method
     * 
     * @param X Input training data
     * @param Y Target output data
     * @param reset_state Whether to reset state before training
     * @return This node instance
     */
    Node& train(const Eigen::MatrixXd& X, const Eigen::MatrixXd& Y, bool reset_state = true);

protected:
    std::string name_;
    bool fitted_;
    int input_dim_;
    int output_dim_;
    bool has_feedback_;
    int batch_size_;
    Eigen::MatrixXd state_;
    std::vector<std::shared_ptr<Node>> feedback_nodes_;
    
    /**
     * Internal implementation of call operation
     * 
     * @param X Input data
     * @return Output of the node
     */
    virtual Eigen::MatrixXd _call(const Eigen::MatrixXd& X) = 0;
    
    /**
     * Internal implementation of fit operation
     * 
     * @param X Input training data
     * @param Y Target output data
     */
    virtual void _fit(const Eigen::MatrixXd& X, const Eigen::MatrixXd& Y) = 0;
};

} // namespace reservoircpp

#endif // RESERVOIRCPP_NODE_HPP

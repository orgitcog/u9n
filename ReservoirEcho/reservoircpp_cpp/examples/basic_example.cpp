#include <iostream>
#include <Eigen/Dense>
#include <random>
#include <memory>

#include "reservoircpp/node.hpp"
#include "reservoircpp/model.hpp"
#include "reservoircpp/numpy.hpp"

// Simple identity node for testing
class IdentityNode : public reservoircpp::Node {
public:
    explicit IdentityNode(const std::string& name = "identity") : Node(name) {
        input_dim_ = 0;  // Will be set during initialization
        output_dim_ = 0; // Will be set during initialization
    }
    
protected:
    Eigen::MatrixXd _call(const Eigen::MatrixXd& X) override {
        // Store state and return input directly
        state_ = X;
        return X;
    }
    
    void _fit(const Eigen::MatrixXd& X, const Eigen::MatrixXd& Y) override {
        // Identity node doesn't need training
        input_dim_ = X.cols();
        output_dim_ = Y.cols();
    }
};

// Simple linear node for testing
class LinearNode : public reservoircpp::Node {
public:
    explicit LinearNode(const std::string& name = "linear") : Node(name) {
        input_dim_ = 0;  // Will be set during initialization
        output_dim_ = 0; // Will be set during initialization
    }
    
protected:
    Eigen::MatrixXd _call(const Eigen::MatrixXd& X) override {
        if (!is_fitted()) {
            throw std::runtime_error("LinearNode must be fitted before use");
        }
        // Apply weights
        state_ = X * weights_;
        return state_;
    }
    
    void _fit(const Eigen::MatrixXd& X, const Eigen::MatrixXd& Y) override {
        input_dim_ = X.cols();
        output_dim_ = Y.cols();
        
        // Simple pseudo-inverse solution for W: X * W = Y => W = X^+ * Y
        weights_ = reservoircpp::pseudoInverse(X) * Y;
    }
    
private:
    Eigen::MatrixXd weights_;
};

int main() {
    std::cout << "ReservoirCpp Basic Example" << std::endl;
    
    // Create random data
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<> dist(0.0, 1.0);
    
    const int num_samples = 100;
    const int input_dim = 5;
    const int output_dim = 2;
    
    Eigen::MatrixXd X(num_samples, input_dim);
    for (int i = 0; i < num_samples; ++i) {
        for (int j = 0; j < input_dim; ++j) {
            X(i, j) = dist(gen);
        }
    }
    
    // Create target output Y = X * W + noise
    Eigen::MatrixXd W = Eigen::MatrixXd::Random(input_dim, output_dim);
    Eigen::MatrixXd Y = X * W;
    for (int i = 0; i < num_samples; ++i) {
        for (int j = 0; j < output_dim; ++j) {
            Y(i, j) += 0.01 * dist(gen);  // Add small noise
        }
    }
    
    // Create test nodes
    auto identity_node = std::make_shared<IdentityNode>("input");
    auto linear_node = std::make_shared<LinearNode>("output");
    
    // Create a model
    reservoircpp::Model model;
    model.add_node(identity_node);
    model.add_node(linear_node);
    model.connect("input", "output");
    
    // Train the model
    std::cout << "Training model..." << std::endl;
    model.fit(X, Y);
    
    // Use the model for prediction
    std::cout << "Testing model..." << std::endl;
    Eigen::MatrixXd X_test = Eigen::MatrixXd::Random(10, input_dim);
    Eigen::MatrixXd Y_pred = model.run(X_test);
    
    // Calculate and print MSE
    Eigen::MatrixXd Y_true = X_test * W;
    double mse = (Y_pred - Y_true).array().square().mean();
    std::cout << "Mean Squared Error: " << mse << std::endl;
    
    // Individual node access
    std::cout << "Accessing individual nodes:" << std::endl;
    auto retrieved_node = model.get_node("input");
    std::cout << "  Node name: " << retrieved_node->name() << std::endl;
    std::cout << "  Is fitted: " << (retrieved_node->is_fitted() ? "yes" : "no") << std::endl;
    
    return 0;
}

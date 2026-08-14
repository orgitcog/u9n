#include <gtest/gtest.h>
#include "reservoircpp/node.hpp"
#include <memory>

using namespace reservoircpp;

// Test Node implementation for testing
class TestNode : public Node {
public:
    explicit TestNode(const std::string& name = "test") : Node(name) {
        input_dim_ = 1;
        output_dim_ = 1;
    }
    
protected:
    Eigen::MatrixXd _call(const Eigen::MatrixXd& X) override {
        state_ = X;
        return X;
    }
    
    void _fit(const Eigen::MatrixXd& X, const Eigen::MatrixXd& Y) override {
        // Nothing to do in this test implementation
    }
};

// Test fixture for Node tests
class NodeTest : public ::testing::Test {
protected:
    void SetUp() override {
        node = std::make_shared<TestNode>("test_node");
    }
    
    std::shared_ptr<TestNode> node;
};

// Test basic Node functionality
TEST_F(NodeTest, BasicFunctionality) {
    EXPECT_EQ(node->name(), "test_node");
    EXPECT_FALSE(node->is_fitted());
    
    // Test initialization
    Eigen::MatrixXd X = Eigen::MatrixXd::Random(5, 1);
    node->initialize(X);
    EXPECT_EQ(node->input_dim(), 1);
    EXPECT_EQ(node->output_dim(), 1);
    
    // Test call
    Eigen::MatrixXd result = (*node)(X);
    EXPECT_EQ(result.rows(), X.rows());
    EXPECT_EQ(result.cols(), X.cols());
    
    // Test state
    Eigen::MatrixXd state = node->state();
    EXPECT_EQ(state.rows(), X.rows());
    EXPECT_EQ(state.cols(), X.cols());
    
    // Test fit
    Eigen::MatrixXd Y = Eigen::MatrixXd::Random(5, 1);
    node->fit(X, Y);
    EXPECT_TRUE(node->is_fitted());
    
    // Test reset
    node->reset();
    EXPECT_EQ(node->state().size(), 0);
}

TEST_F(NodeTest, Feedback) {
    auto feedback_node = std::make_shared<TestNode>("feedback_node");
    
    // Add feedback connection
    node->with_feedback(feedback_node);
    
    // Check feedback status
    EXPECT_TRUE(node->has_feedback());
    EXPECT_EQ(node->feedback_nodes().size(), 1);
    EXPECT_EQ(node->feedback_nodes()[0]->name(), "feedback_node");
}

// Note: main() is provided by GTest::Main

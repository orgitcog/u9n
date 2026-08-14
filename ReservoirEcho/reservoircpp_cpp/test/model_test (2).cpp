#include <gtest/gtest.h>
#include "reservoircpp/node.hpp"
#include "reservoircpp/model.hpp"
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

// Test fixture for Model tests
class ModelTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create test nodes
        node1 = std::make_shared<TestNode>("node1");
        node2 = std::make_shared<TestNode>("node2");
        
        // Create a model with the nodes
        model = std::make_shared<Model>();
        model->add_node(node1);
        model->add_node(node2);
    }
    
    std::shared_ptr<TestNode> node1;
    std::shared_ptr<TestNode> node2;
    std::shared_ptr<Model> model;
};

// Test basic Model functionality
TEST_F(ModelTest, BasicFunctionality) {
    EXPECT_EQ(model->name(), "");
    EXPECT_FALSE(model->is_fitted());
    
    // Test connecting nodes
    model->connect("node1", "node2");
    
    // Test get_node
    auto retrieved_node = model->get_node("node1");
    EXPECT_EQ(retrieved_node->name(), "node1");
    
    // Test initialization
    Eigen::MatrixXd X = Eigen::MatrixXd::Random(5, 1);
    model->initialize(X);
    
    // Test fit
    Eigen::MatrixXd Y = Eigen::MatrixXd::Random(5, 1);
    model->fit(X, Y);
    EXPECT_TRUE(model->is_fitted());
    
    // Test run
    Eigen::MatrixXd result = model->run(X);
    EXPECT_EQ(result.rows(), X.rows());
    EXPECT_EQ(result.cols(), X.cols());
    
    // Test reset
    model->reset();
    EXPECT_EQ(model->state().size(), 0);
}

TEST_F(ModelTest, Feedback) {
    // Add feedback connection
    model->with_feedback("node2", "node1");
    
    // Check feedback through node
    EXPECT_TRUE(node1->has_feedback());
    EXPECT_EQ(node1->feedback_nodes().size(), 1);
    EXPECT_EQ(node1->feedback_nodes()[0]->name(), "node2");
}

// Note: main() is provided by GTest::Main

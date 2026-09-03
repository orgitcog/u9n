#ifndef RESERVOIRCPP__BASE_HPP
#define RESERVOIRCPP__BASE_HPP

// Fixed include paths (removed double slashes and backslashes)
#include "reservoircpp/type/Shape.hpp"
#include "reservoircpp/type/MappedData.hpp"
#include "reservoircpp/typing/Sequence.hpp"
#include "reservoircpp/typing/Iterator.hpp"
#include "numpy.hpp"
#include "reservoircpp/utils/validation/check_vector.hpp"
#include "reservoircpp/typing/Dict.hpp"
#include "reservoircpp/typing/Any.hpp"
#include "reservoircpp/utils/progress.hpp"
#include "reservoircpp/abc/ABC.hpp"
#include "reservoircpp/abc/abstractmethod.hpp"
#include "reservoircpp/utils/validation/is_mapping.hpp"
#include "reservoircpp/uuid/uuid4.hpp"
#include "reservoircpp/typing/Union.hpp"
#include "reservoircpp/contextlib/contextmanager.hpp"

#include <Eigen/Dense>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <variant>

namespace reservoircpp {

using namespace Eigen;

// Forward declarations
class Node;
class Model;

/**
 * @brief Base class for reservoir computing components
 * 
 * Provides common functionality for nodes and models in the
 * Deep Tree Echo reservoir computing framework.
 */
class BaseComponent : public abc::ABC {
public:
    virtual ~BaseComponent() = default;

    // Unique identifier
    const uuid::UUID4& uuid() const { return uuid_; }
    
    // Name for identification
    const std::string& name() const { return name_; }
    void set_name(const std::string& name) { name_ = name; }

    // State management
    virtual void reset() = 0;
    virtual bool is_initialized() const = 0;

protected:
    BaseComponent() : uuid_(), name_("unnamed") {}
    explicit BaseComponent(const std::string& name) : uuid_(), name_(name) {}

private:
    uuid::UUID4 uuid_;
    std::string name_;
};

/**
 * @brief Get inputs for distant Nodes in a Model used as feedback or teacher
 * 
 * These inputs should be already computed by other Nodes.
 */
template<typename ModelType>
typing::StringDict<np::Vector> distant_model_inputs(const ModelType& model) {
    typing::StringDict<np::Vector> input_data;
    
    for (const auto& [parent, child] : model.edges()) {
        if (model.is_input_node(parent)) {
            input_data[child.name()] = parent.state_proxy();
        }
    }
    
    return input_data;
}

/**
 * @brief Decorator to allow timespans in node operations
 */
template<typename NodeType>
class TimespanWrapper {
public:
    explicit TimespanWrapper(NodeType& node) : node_(node) {}

    template<typename... Args>
    auto operator()(Args&&... args) {
        return node_.call_with_timespan(std::forward<Args>(args)...);
    }

private:
    NodeType& node_;
};

template<typename NodeType>
TimespanWrapper<NodeType> allow_timespans(NodeType& node) {
    return TimespanWrapper<NodeType>(node);
}

/**
 * @brief Configuration for reservoir nodes
 */
struct NodeConfig {
    std::string name = "node";
    int input_dim = 1;
    int output_dim = 1;
    int units = 100;
    double spectral_radius = 0.9;
    double leak_rate = 0.3;
    double input_scaling = 1.0;
    double bias_scaling = 0.0;
    double sparsity = 0.1;
    std::string activation = "tanh";
    unsigned int seed = 0;
    bool trainable = true;
};

/**
 * @brief State container for reservoir nodes
 */
struct NodeState {
    np::Vector state;
    np::Vector output;
    bool initialized = false;
    
    void reset(int size) {
        state = np::zeros(size);
        output = np::zeros(size);
        initialized = false;
    }
};

} // namespace reservoircpp

#endif // RESERVOIRCPP__BASE_HPP

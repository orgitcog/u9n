// TensorLogicEngine.h
// Tensor Logic Engine for Deep Tree Echo
// Unifies gradient-based learning (neural) with symbolic reasoning
// Based on tensor-logic.org: "logical rules and Einstein summation are the same operation"

#pragma once

#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <string>
#include <cmath>
#include <array>
#include <variant>

namespace DeepTreeEcho {

// ============================================================================
// Forward Declarations
// ============================================================================

struct FTensorShape;
struct FLogicalTensor;
struct FSymbolicRule;
struct FEinsteinExpression;
struct FGradientState;
class FTensorLogicEngine;

// ============================================================================
// Type Aliases
// ============================================================================

using TensorData = std::vector<float>;
using TensorIndex = std::vector<int32_t>;
using SymbolicBinding = std::unordered_map<std::string, float>;

// ============================================================================
// Enumerations
// ============================================================================

/**
 * @brief Type of tensor operation
 */
enum class ETensorOperation : uint8_t {
    Identity,           // Pass-through
    Contraction,        // Einstein summation
    OuterProduct,       // Tensor product
    Projection,         // Dimension reduction
    Injection,          // Dimension expansion
    LinearMap,          // Linear transformation
    NonLinear,          // Activation function
    Attention,          // Scaled dot-product attention
    Composition         // Rule composition
};

/**
 * @brief Activation functions for non-linear operations
 */
enum class EActivation : uint8_t {
    Identity,
    ReLU,
    Sigmoid,
    Tanh,
    Softmax,
    GELU,
    SiLU
};

/**
 * @brief Logical connectives for symbolic reasoning
 */
enum class ELogicalConnective : uint8_t {
    Conjunction,    // AND (tensor product)
    Disjunction,    // OR (max)
    Implication,    // A -> B (linear implication)
    Negation,       // NOT (1 - x)
    Universal,      // forall (min over dimension)
    Existential     // exists (max over dimension)
};

/**
 * @brief Mode of operation for tensor logic
 */
enum class ETensorLogicMode : uint8_t {
    Neural,         // Pure gradient-based
    Symbolic,       // Pure rule-based
    Hybrid          // Combined neural-symbolic
};

// ============================================================================
// Core Structures
// ============================================================================

/**
 * @brief Shape of a tensor
 */
struct FTensorShape {
    std::vector<int32_t> Dimensions;

    FTensorShape() = default;
    FTensorShape(std::initializer_list<int32_t> dims) : Dimensions(dims) {}
    explicit FTensorShape(const std::vector<int32_t>& dims) : Dimensions(dims) {}

    int32_t Rank() const { return static_cast<int32_t>(Dimensions.size()); }
    int64_t TotalElements() const;
    bool IsScalar() const { return Dimensions.empty(); }
    bool IsVector() const { return Dimensions.size() == 1; }
    bool IsMatrix() const { return Dimensions.size() == 2; }

    bool operator==(const FTensorShape& Other) const { return Dimensions == Other.Dimensions; }
    bool operator!=(const FTensorShape& Other) const { return !(*this == Other); }

    std::string ToString() const;
};

/**
 * @brief A logical tensor combining data with semantic meaning
 *
 * In tensor logic, tensors are not just numerical arrays but carry
 * logical semantics: each element represents truth degree in [0,1].
 */
struct FLogicalTensor {
    std::string Name;                   // Symbolic name
    FTensorShape Shape;                 // Tensor dimensions
    TensorData Data;                    // Actual values (truth degrees)
    bool RequiresGrad = false;          // For gradient computation
    TensorData Gradient;                // Gradient buffer

    // Constructors
    FLogicalTensor() = default;
    FLogicalTensor(const std::string& name, const FTensorShape& shape);
    FLogicalTensor(const std::string& name, const FTensorShape& shape, const TensorData& data);

    // Element access
    float& At(const TensorIndex& index);
    float At(const TensorIndex& index) const;
    float& At(int32_t flatIndex);
    float At(int32_t flatIndex) const;

    // Initialization
    void Zero();
    void Ones();
    void Random(float min = 0.0f, float max = 1.0f);
    void Uniform(float value);

    // Basic operations
    void Add(const FLogicalTensor& Other);
    void Multiply(float scalar);
    void Apply(std::function<float(float)> func);

    // Logical operations (returns truth degrees in [0,1])
    FLogicalTensor And(const FLogicalTensor& Other) const;     // min or product
    FLogicalTensor Or(const FLogicalTensor& Other) const;      // max
    FLogicalTensor Not() const;                                 // 1 - x
    FLogicalTensor Implies(const FLogicalTensor& Other) const; // Linear implication

    // Index conversion
    int32_t FlatIndex(const TensorIndex& index) const;
    TensorIndex UnflatIndex(int32_t flatIndex) const;

    // Utilities
    float Sum() const;
    float Max() const;
    float Min() const;
    float Mean() const;
    FLogicalTensor Softmax(int32_t dim = -1) const;
    FLogicalTensor Normalize() const;

    std::string ToString() const;
};

/**
 * @brief Einstein summation expression
 *
 * The core construct of tensor logic: all operations are expressed
 * as Einstein summations, unifying neural and symbolic computation.
 *
 * Example: "ij,jk->ik" represents matrix multiplication
 */
struct FEinsteinExpression {
    std::string Expression;                     // Full einsum expression
    std::vector<std::string> InputSubscripts;   // Per-input index labels
    std::string OutputSubscripts;               // Output index labels
    std::vector<char> SummedIndices;            // Indices to contract

    FEinsteinExpression() = default;
    explicit FEinsteinExpression(const std::string& expr);

    // Parse the expression
    void Parse();

    // Validate against input shapes
    bool Validate(const std::vector<FTensorShape>& inputShapes) const;

    // Compute output shape
    FTensorShape ComputeOutputShape(const std::vector<FTensorShape>& inputShapes) const;

    // Check if this is a contraction (has summed indices)
    bool IsContraction() const { return !SummedIndices.empty(); }

    std::string ToString() const { return Expression; }
};

/**
 * @brief A symbolic logical rule
 *
 * Rules in tensor logic are represented as tensor equations.
 * A rule "A, B |- C" becomes a tensor operation mapping A⊗B → C.
 */
struct FSymbolicRule {
    std::string Name;                           // Rule name
    std::vector<std::string> Premises;          // Premise terms
    std::string Conclusion;                     // Conclusion term
    FEinsteinExpression TensorForm;             // Tensor equation form
    float Confidence = 1.0f;                    // Rule confidence
    bool IsLearned = false;                     // True if learned from data

    FSymbolicRule() = default;
    FSymbolicRule(const std::string& name,
                  const std::vector<std::string>& premises,
                  const std::string& conclusion);

    // Convert to tensor equation
    void ToTensorForm();

    // Apply rule to tensors
    FLogicalTensor Apply(const std::vector<FLogicalTensor>& inputs) const;

    // Get as string: "A, B |- C"
    std::string ToString() const;
};

/**
 * @brief Gradient state for backpropagation
 */
struct FGradientState {
    std::unordered_map<std::string, TensorData> Gradients;  // Per-tensor gradients
    float LearningRate = 0.01f;
    float MomentumBeta = 0.9f;
    std::unordered_map<std::string, TensorData> Momentum;   // Momentum buffers

    void Zero();
    void Accumulate(const std::string& name, const TensorData& grad);
    void Step(std::unordered_map<std::string, FLogicalTensor>& parameters);
};

// ============================================================================
// Knowledge Base
// ============================================================================

/**
 * @brief Knowledge base storing facts and rules
 */
class FKnowledgeBase {
public:
    FKnowledgeBase() = default;

    // Facts (ground tensors)
    void AddFact(const std::string& name, const FLogicalTensor& tensor);
    const FLogicalTensor* GetFact(const std::string& name) const;
    bool HasFact(const std::string& name) const;
    void RemoveFact(const std::string& name);

    // Rules
    void AddRule(const FSymbolicRule& rule);
    const std::vector<FSymbolicRule>& GetRules() const { return Rules; }
    std::vector<FSymbolicRule> GetRulesWithPremise(const std::string& premise) const;
    std::vector<FSymbolicRule> GetRulesWithConclusion(const std::string& conclusion) const;

    // Query (forward chaining)
    FLogicalTensor Query(const std::string& goal, int maxDepth = 10);

    // Statistics
    size_t NumFacts() const { return Facts.size(); }
    size_t NumRules() const { return Rules.size(); }

private:
    std::unordered_map<std::string, FLogicalTensor> Facts;
    std::vector<FSymbolicRule> Rules;

    // Forward chaining helper
    FLogicalTensor ForwardChain(const std::string& goal,
                                 std::unordered_set<std::string>& visited,
                                 int depth, int maxDepth);
};

// ============================================================================
// Tensor Logic Engine
// ============================================================================

/**
 * @brief Main Tensor Logic Engine
 *
 * Implements the unification of neural networks and symbolic AI through
 * tensor equations. Key insight: logical rules and Einstein summation
 * are essentially the same operation.
 *
 * Features:
 * - Sound reasoning in embedding space
 * - Gradient-based learning of rules
 * - Symbolic inference with neural scalability
 * - Transformers, graphical models, kernel machines in unified framework
 */
class FTensorLogicEngine {
public:
    FTensorLogicEngine();
    ~FTensorLogicEngine();

    // ========================================================================
    // Initialization
    // ========================================================================

    void Initialize();
    void Reset();
    bool IsInitialized() const { return bInitialized; }

    // ========================================================================
    // Mode Control
    // ========================================================================

    void SetMode(ETensorLogicMode Mode) { CurrentMode = Mode; }
    ETensorLogicMode GetMode() const { return CurrentMode; }

    // ========================================================================
    // Tensor Operations (Einstein Summation)
    // ========================================================================

    /**
     * @brief Execute Einstein summation
     *
     * The fundamental operation of tensor logic. All neural and symbolic
     * operations reduce to Einstein summation.
     *
     * @param expression Einstein summation expression (e.g., "ij,jk->ik")
     * @param inputs Input tensors
     * @return Result tensor
     */
    FLogicalTensor Einsum(const std::string& expression,
                          const std::vector<FLogicalTensor>& inputs);

    /**
     * @brief Tensor contraction (generalized matrix multiply)
     */
    FLogicalTensor Contract(const FLogicalTensor& A, const FLogicalTensor& B,
                            const std::vector<std::pair<int32_t, int32_t>>& axes);

    /**
     * @brief Outer product (tensor product)
     */
    FLogicalTensor OuterProduct(const FLogicalTensor& A, const FLogicalTensor& B);

    /**
     * @brief Trace (sum of diagonal)
     */
    float Trace(const FLogicalTensor& A);

    // ========================================================================
    // Neural Operations
    // ========================================================================

    /**
     * @brief Linear transformation: Y = XW + b
     */
    FLogicalTensor Linear(const FLogicalTensor& input,
                          const FLogicalTensor& weight,
                          const FLogicalTensor* bias = nullptr);

    /**
     * @brief Apply activation function
     */
    FLogicalTensor Activate(const FLogicalTensor& input, EActivation activation);

    /**
     * @brief Scaled dot-product attention
     */
    FLogicalTensor Attention(const FLogicalTensor& query,
                             const FLogicalTensor& key,
                             const FLogicalTensor& value,
                             const FLogicalTensor* mask = nullptr);

    /**
     * @brief Multi-head attention
     */
    FLogicalTensor MultiHeadAttention(const FLogicalTensor& query,
                                       const FLogicalTensor& key,
                                       const FLogicalTensor& value,
                                       int32_t numHeads);

    // ========================================================================
    // Symbolic Operations
    // ========================================================================

    /**
     * @brief Apply logical connective
     */
    FLogicalTensor ApplyConnective(ELogicalConnective connective,
                                   const std::vector<FLogicalTensor>& operands);

    /**
     * @brief Apply symbolic rule
     */
    FLogicalTensor ApplyRule(const FSymbolicRule& rule,
                             const std::vector<FLogicalTensor>& inputs);

    /**
     * @brief Forward chaining inference
     */
    FLogicalTensor Infer(const std::string& goal);

    /**
     * @brief Backward chaining inference
     */
    bool Prove(const std::string& goal, SymbolicBinding& bindings);

    // ========================================================================
    // Hybrid Neural-Symbolic Operations
    // ========================================================================

    /**
     * @brief Neural-guided symbolic search
     *
     * Use neural network to prioritize which rules to apply.
     */
    FLogicalTensor NeuralGuidedInference(const std::string& goal,
                                          const FLogicalTensor& context);

    /**
     * @brief Symbolic-constrained neural learning
     *
     * Incorporate symbolic rules as constraints during training.
     */
    void ConstrainedLearning(const std::vector<FSymbolicRule>& constraints);

    /**
     * @brief Learn rules from data
     *
     * Gradient-based learning of symbolic rules from examples.
     */
    std::vector<FSymbolicRule> LearnRules(const std::vector<FLogicalTensor>& examples,
                                           const std::vector<FLogicalTensor>& labels);

    // ========================================================================
    // Knowledge Base
    // ========================================================================

    FKnowledgeBase& GetKnowledgeBase() { return KB; }
    const FKnowledgeBase& GetKnowledgeBase() const { return KB; }

    void AddFact(const std::string& name, const FLogicalTensor& tensor);
    void AddRule(const FSymbolicRule& rule);

    // ========================================================================
    // Gradient Computation
    // ========================================================================

    /**
     * @brief Enable gradient tracking
     */
    void EnableGrad(bool enable = true) { bGradEnabled = enable; }
    bool IsGradEnabled() const { return bGradEnabled; }

    /**
     * @brief Compute gradients via backpropagation
     */
    void Backward(const FLogicalTensor& loss);

    /**
     * @brief Get gradient state
     */
    FGradientState& GetGradientState() { return GradState; }

    /**
     * @brief Optimization step
     */
    void Step();

    // ========================================================================
    // Transformer Components
    // ========================================================================

    /**
     * @brief Transformer encoder layer
     */
    FLogicalTensor TransformerEncoderLayer(const FLogicalTensor& input,
                                            int32_t dModel,
                                            int32_t numHeads,
                                            int32_t dFF);

    /**
     * @brief Transformer decoder layer
     */
    FLogicalTensor TransformerDecoderLayer(const FLogicalTensor& input,
                                            const FLogicalTensor& memory,
                                            int32_t dModel,
                                            int32_t numHeads,
                                            int32_t dFF);

    // ========================================================================
    // Graphical Model Components
    // ========================================================================

    /**
     * @brief Factor graph message passing
     */
    FLogicalTensor FactorGraphPass(const std::vector<FLogicalTensor>& factors,
                                    const std::vector<std::vector<int32_t>>& connections);

    /**
     * @brief Belief propagation step
     */
    std::vector<FLogicalTensor> BeliefPropagation(
        const std::vector<FLogicalTensor>& beliefs,
        const std::vector<FLogicalTensor>& factors,
        int numIterations = 10);

    // ========================================================================
    // Kernel Methods
    // ========================================================================

    /**
     * @brief Kernel matrix computation
     */
    FLogicalTensor KernelMatrix(const FLogicalTensor& X,
                                 const FLogicalTensor& Y,
                                 const std::string& kernelType = "rbf",
                                 float gamma = 1.0f);

    /**
     * @brief Kernel attention (kernelized softmax)
     */
    FLogicalTensor KernelAttention(const FLogicalTensor& query,
                                    const FLogicalTensor& key,
                                    const FLogicalTensor& value);

    // ========================================================================
    // Cognitive Integration
    // ========================================================================

    /**
     * @brief Process cognitive state through tensor logic
     *
     * Integrates with Deep Tree Echo's 12-step cognitive loop.
     */
    FLogicalTensor ProcessCognitiveState(const FLogicalTensor& state,
                                          int32_t step,
                                          int32_t stream);

    /**
     * @brief Update beliefs using tensor logic
     */
    FLogicalTensor UpdateBeliefs(const FLogicalTensor& prior,
                                  const FLogicalTensor& observation);

    /**
     * @brief Action selection via tensor logic
     */
    FLogicalTensor SelectAction(const FLogicalTensor& beliefs,
                                 const FLogicalTensor& preferences);

private:
    bool bInitialized = false;
    bool bGradEnabled = false;
    ETensorLogicMode CurrentMode = ETensorLogicMode::Hybrid;

    FKnowledgeBase KB;
    FGradientState GradState;

    // Learnable parameters
    std::unordered_map<std::string, FLogicalTensor> Parameters;

    // Computation graph for backprop (simplified)
    struct ComputationNode {
        std::string Operation;
        std::vector<std::string> Inputs;
        std::string Output;
    };
    std::vector<ComputationNode> ComputationGraph;

    // Internal helpers
    FLogicalTensor EinsumBinary(const FEinsteinExpression& expr,
                                const FLogicalTensor& A,
                                const FLogicalTensor& B);
    FLogicalTensor ApplyActivation(const FLogicalTensor& input, EActivation act);
    void RecordOperation(const std::string& op,
                        const std::vector<std::string>& inputs,
                        const std::string& output);
};

} // namespace DeepTreeEcho

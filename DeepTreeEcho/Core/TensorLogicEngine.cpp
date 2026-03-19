// TensorLogicEngine.cpp
// Implementation of Tensor Logic Engine

#include "TensorLogicEngine.h"
#include <sstream>
#include <random>
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace DeepTreeEcho {

// ============================================================================
// FTensorShape Implementation
// ============================================================================

int64_t FTensorShape::TotalElements() const {
    if (Dimensions.empty()) return 1;
    int64_t total = 1;
    for (int32_t dim : Dimensions) {
        total *= dim;
    }
    return total;
}

std::string FTensorShape::ToString() const {
    std::ostringstream oss;
    oss << "(";
    for (size_t i = 0; i < Dimensions.size(); i++) {
        if (i > 0) oss << ", ";
        oss << Dimensions[i];
    }
    oss << ")";
    return oss.str();
}

// ============================================================================
// FLogicalTensor Implementation
// ============================================================================

FLogicalTensor::FLogicalTensor(const std::string& name, const FTensorShape& shape)
    : Name(name), Shape(shape) {
    Data.resize(static_cast<size_t>(shape.TotalElements()), 0.0f);
}

FLogicalTensor::FLogicalTensor(const std::string& name, const FTensorShape& shape,
                               const TensorData& data)
    : Name(name), Shape(shape), Data(data) {
    if (Data.size() != static_cast<size_t>(shape.TotalElements())) {
        Data.resize(static_cast<size_t>(shape.TotalElements()), 0.0f);
    }
}

int32_t FLogicalTensor::FlatIndex(const TensorIndex& index) const {
    if (index.size() != Shape.Dimensions.size()) return -1;

    int32_t flat = 0;
    int32_t stride = 1;
    for (int32_t i = static_cast<int32_t>(Shape.Dimensions.size()) - 1; i >= 0; i--) {
        flat += index[i] * stride;
        stride *= Shape.Dimensions[i];
    }
    return flat;
}

TensorIndex FLogicalTensor::UnflatIndex(int32_t flatIndex) const {
    TensorIndex index(Shape.Dimensions.size());
    for (int32_t i = static_cast<int32_t>(Shape.Dimensions.size()) - 1; i >= 0; i--) {
        index[i] = flatIndex % Shape.Dimensions[i];
        flatIndex /= Shape.Dimensions[i];
    }
    return index;
}

float& FLogicalTensor::At(const TensorIndex& index) {
    return Data[FlatIndex(index)];
}

float FLogicalTensor::At(const TensorIndex& index) const {
    return Data[FlatIndex(index)];
}

float& FLogicalTensor::At(int32_t flatIndex) {
    return Data[flatIndex];
}

float FLogicalTensor::At(int32_t flatIndex) const {
    return Data[flatIndex];
}

void FLogicalTensor::Zero() {
    std::fill(Data.begin(), Data.end(), 0.0f);
}

void FLogicalTensor::Ones() {
    std::fill(Data.begin(), Data.end(), 1.0f);
}

void FLogicalTensor::Random(float min, float max) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(min, max);
    for (float& val : Data) {
        val = dist(gen);
    }
}

void FLogicalTensor::Uniform(float value) {
    std::fill(Data.begin(), Data.end(), value);
}

void FLogicalTensor::Add(const FLogicalTensor& Other) {
    if (Data.size() != Other.Data.size()) return;
    for (size_t i = 0; i < Data.size(); i++) {
        Data[i] += Other.Data[i];
    }
}

void FLogicalTensor::Multiply(float scalar) {
    for (float& val : Data) {
        val *= scalar;
    }
}

void FLogicalTensor::Apply(std::function<float(float)> func) {
    for (float& val : Data) {
        val = func(val);
    }
}

FLogicalTensor FLogicalTensor::And(const FLogicalTensor& Other) const {
    // Fuzzy AND: min or product (using product for differentiability)
    FLogicalTensor result(Name + "_and_" + Other.Name, Shape);
    for (size_t i = 0; i < Data.size(); i++) {
        result.Data[i] = Data[i] * Other.Data[i % Other.Data.size()];
    }
    return result;
}

FLogicalTensor FLogicalTensor::Or(const FLogicalTensor& Other) const {
    // Fuzzy OR: max
    FLogicalTensor result(Name + "_or_" + Other.Name, Shape);
    for (size_t i = 0; i < Data.size(); i++) {
        result.Data[i] = std::max(Data[i], Other.Data[i % Other.Data.size()]);
    }
    return result;
}

FLogicalTensor FLogicalTensor::Not() const {
    // Fuzzy NOT: 1 - x
    FLogicalTensor result("not_" + Name, Shape);
    for (size_t i = 0; i < Data.size(); i++) {
        result.Data[i] = 1.0f - Data[i];
    }
    return result;
}

FLogicalTensor FLogicalTensor::Implies(const FLogicalTensor& Other) const {
    // Lukasiewicz implication: min(1, 1 - a + b)
    FLogicalTensor result(Name + "_implies_" + Other.Name, Shape);
    for (size_t i = 0; i < Data.size(); i++) {
        float a = Data[i];
        float b = Other.Data[i % Other.Data.size()];
        result.Data[i] = std::min(1.0f, 1.0f - a + b);
    }
    return result;
}

float FLogicalTensor::Sum() const {
    float sum = 0.0f;
    for (float val : Data) sum += val;
    return sum;
}

float FLogicalTensor::Max() const {
    if (Data.empty()) return 0.0f;
    return *std::max_element(Data.begin(), Data.end());
}

float FLogicalTensor::Min() const {
    if (Data.empty()) return 0.0f;
    return *std::min_element(Data.begin(), Data.end());
}

float FLogicalTensor::Mean() const {
    if (Data.empty()) return 0.0f;
    return Sum() / static_cast<float>(Data.size());
}

FLogicalTensor FLogicalTensor::Softmax(int32_t dim) const {
    FLogicalTensor result(Name + "_softmax", Shape);

    // Simple softmax over entire tensor for now
    float maxVal = Max();
    float sumExp = 0.0f;
    for (float val : Data) {
        sumExp += std::exp(val - maxVal);
    }
    for (size_t i = 0; i < Data.size(); i++) {
        result.Data[i] = std::exp(Data[i] - maxVal) / sumExp;
    }
    return result;
}

FLogicalTensor FLogicalTensor::Normalize() const {
    FLogicalTensor result(Name + "_normalized", Shape);
    float sum = Sum();
    if (sum > 1e-10f) {
        for (size_t i = 0; i < Data.size(); i++) {
            result.Data[i] = Data[i] / sum;
        }
    }
    return result;
}

std::string FLogicalTensor::ToString() const {
    std::ostringstream oss;
    oss << Name << " " << Shape.ToString() << " = [";
    for (size_t i = 0; i < std::min(Data.size(), size_t(10)); i++) {
        if (i > 0) oss << ", ";
        oss << Data[i];
    }
    if (Data.size() > 10) oss << ", ...";
    oss << "]";
    return oss.str();
}

// ============================================================================
// FEinsteinExpression Implementation
// ============================================================================

FEinsteinExpression::FEinsteinExpression(const std::string& expr) : Expression(expr) {
    Parse();
}

void FEinsteinExpression::Parse() {
    InputSubscripts.clear();
    SummedIndices.clear();

    // Find "->" separator
    size_t arrowPos = Expression.find("->");
    if (arrowPos == std::string::npos) {
        // Implicit output (sum over all indices)
        OutputSubscripts = "";
    } else {
        OutputSubscripts = Expression.substr(arrowPos + 2);
    }

    // Parse input subscripts
    std::string inputs = Expression.substr(0, arrowPos);
    std::string current;
    for (char c : inputs) {
        if (c == ',') {
            if (!current.empty()) {
                InputSubscripts.push_back(current);
                current.clear();
            }
        } else if (c != ' ') {
            current += c;
        }
    }
    if (!current.empty()) {
        InputSubscripts.push_back(current);
    }

    // Find summed indices (appear in inputs but not output)
    std::unordered_set<char> inputIndices;
    std::unordered_set<char> outputIndices(OutputSubscripts.begin(), OutputSubscripts.end());

    for (const auto& sub : InputSubscripts) {
        for (char c : sub) {
            if (std::isalpha(c)) {
                inputIndices.insert(c);
            }
        }
    }

    for (char c : inputIndices) {
        if (outputIndices.find(c) == outputIndices.end()) {
            SummedIndices.push_back(c);
        }
    }
}

bool FEinsteinExpression::Validate(const std::vector<FTensorShape>& inputShapes) const {
    if (inputShapes.size() != InputSubscripts.size()) return false;

    for (size_t i = 0; i < inputShapes.size(); i++) {
        if (inputShapes[i].Rank() != static_cast<int32_t>(InputSubscripts[i].length())) {
            return false;
        }
    }
    return true;
}

FTensorShape FEinsteinExpression::ComputeOutputShape(
    const std::vector<FTensorShape>& inputShapes) const {

    if (!Validate(inputShapes)) return FTensorShape({});

    // Map indices to dimensions
    std::unordered_map<char, int32_t> indexToDim;
    for (size_t i = 0; i < InputSubscripts.size(); i++) {
        for (size_t j = 0; j < InputSubscripts[i].length(); j++) {
            char idx = InputSubscripts[i][j];
            indexToDim[idx] = inputShapes[i].Dimensions[j];
        }
    }

    // Build output shape
    std::vector<int32_t> outputDims;
    for (char c : OutputSubscripts) {
        if (indexToDim.find(c) != indexToDim.end()) {
            outputDims.push_back(indexToDim[c]);
        }
    }

    return FTensorShape(outputDims);
}

// ============================================================================
// FSymbolicRule Implementation
// ============================================================================

FSymbolicRule::FSymbolicRule(const std::string& name,
                             const std::vector<std::string>& premises,
                             const std::string& conclusion)
    : Name(name), Premises(premises), Conclusion(conclusion) {
    ToTensorForm();
}

void FSymbolicRule::ToTensorForm() {
    // Convert rule to Einstein expression
    // A, B |- C becomes einsum over A⊗B → C
    std::ostringstream expr;
    for (size_t i = 0; i < Premises.size(); i++) {
        if (i > 0) expr << ",";
        expr << "i" << i;  // Each premise gets an index
    }
    expr << "->o";  // Output index
    TensorForm = FEinsteinExpression(expr.str());
}

FLogicalTensor FSymbolicRule::Apply(const std::vector<FLogicalTensor>& inputs) const {
    // Simple rule application: product of premises
    if (inputs.empty()) return FLogicalTensor();

    FLogicalTensor result = inputs[0];
    result.Name = Conclusion;

    for (size_t i = 1; i < inputs.size(); i++) {
        result = result.And(inputs[i]);
    }

    result.Multiply(Confidence);
    return result;
}

std::string FSymbolicRule::ToString() const {
    std::ostringstream oss;
    oss << Name << ": ";
    for (size_t i = 0; i < Premises.size(); i++) {
        if (i > 0) oss << ", ";
        oss << Premises[i];
    }
    oss << " |- " << Conclusion;
    return oss.str();
}

// ============================================================================
// FGradientState Implementation
// ============================================================================

void FGradientState::Zero() {
    for (auto& [name, grad] : Gradients) {
        std::fill(grad.begin(), grad.end(), 0.0f);
    }
}

void FGradientState::Accumulate(const std::string& name, const TensorData& grad) {
    if (Gradients.find(name) == Gradients.end()) {
        Gradients[name] = grad;
    } else {
        for (size_t i = 0; i < grad.size(); i++) {
            Gradients[name][i] += grad[i];
        }
    }
}

void FGradientState::Step(std::unordered_map<std::string, FLogicalTensor>& parameters) {
    for (auto& [name, param] : parameters) {
        if (Gradients.find(name) == Gradients.end()) continue;

        const auto& grad = Gradients[name];

        // Initialize momentum if needed
        if (Momentum.find(name) == Momentum.end()) {
            Momentum[name] = TensorData(grad.size(), 0.0f);
        }

        auto& mom = Momentum[name];

        // SGD with momentum
        for (size_t i = 0; i < param.Data.size(); i++) {
            mom[i] = MomentumBeta * mom[i] + (1.0f - MomentumBeta) * grad[i];
            param.Data[i] -= LearningRate * mom[i];
        }
    }

    Zero();
}

// ============================================================================
// FKnowledgeBase Implementation
// ============================================================================

void FKnowledgeBase::AddFact(const std::string& name, const FLogicalTensor& tensor) {
    Facts[name] = tensor;
}

const FLogicalTensor* FKnowledgeBase::GetFact(const std::string& name) const {
    auto it = Facts.find(name);
    if (it != Facts.end()) return &it->second;
    return nullptr;
}

bool FKnowledgeBase::HasFact(const std::string& name) const {
    return Facts.find(name) != Facts.end();
}

void FKnowledgeBase::RemoveFact(const std::string& name) {
    Facts.erase(name);
}

void FKnowledgeBase::AddRule(const FSymbolicRule& rule) {
    Rules.push_back(rule);
}

std::vector<FSymbolicRule> FKnowledgeBase::GetRulesWithPremise(const std::string& premise) const {
    std::vector<FSymbolicRule> result;
    for (const auto& rule : Rules) {
        for (const auto& p : rule.Premises) {
            if (p == premise) {
                result.push_back(rule);
                break;
            }
        }
    }
    return result;
}

std::vector<FSymbolicRule> FKnowledgeBase::GetRulesWithConclusion(
    const std::string& conclusion) const {
    std::vector<FSymbolicRule> result;
    for (const auto& rule : Rules) {
        if (rule.Conclusion == conclusion) {
            result.push_back(rule);
        }
    }
    return result;
}

FLogicalTensor FKnowledgeBase::Query(const std::string& goal, int maxDepth) {
    std::unordered_set<std::string> visited;
    return ForwardChain(goal, visited, 0, maxDepth);
}

FLogicalTensor FKnowledgeBase::ForwardChain(const std::string& goal,
                                             std::unordered_set<std::string>& visited,
                                             int depth, int maxDepth) {
    // Check if already computed
    if (visited.find(goal) != visited.end()) {
        return FLogicalTensor();  // Cycle detected
    }

    // Check if goal is a known fact
    if (HasFact(goal)) {
        return *GetFact(goal);
    }

    // Depth limit
    if (depth >= maxDepth) {
        return FLogicalTensor();
    }

    visited.insert(goal);

    // Find rules that conclude the goal
    auto applicableRules = GetRulesWithConclusion(goal);
    if (applicableRules.empty()) {
        return FLogicalTensor();
    }

    // Try each rule
    FLogicalTensor bestResult;
    float bestConfidence = 0.0f;

    for (const auto& rule : applicableRules) {
        // Get premises
        std::vector<FLogicalTensor> premises;
        bool allPremisesSatisfied = true;

        for (const auto& premise : rule.Premises) {
            FLogicalTensor premiseResult = ForwardChain(premise, visited, depth + 1, maxDepth);
            if (premiseResult.Data.empty()) {
                allPremisesSatisfied = false;
                break;
            }
            premises.push_back(premiseResult);
        }

        if (allPremisesSatisfied) {
            FLogicalTensor result = rule.Apply(premises);
            float conf = result.Mean();
            if (conf > bestConfidence) {
                bestConfidence = conf;
                bestResult = result;
            }
        }
    }

    // Cache result
    if (!bestResult.Data.empty()) {
        Facts[goal] = bestResult;
    }

    return bestResult;
}

// ============================================================================
// FTensorLogicEngine Implementation
// ============================================================================

FTensorLogicEngine::FTensorLogicEngine() = default;
FTensorLogicEngine::~FTensorLogicEngine() = default;

void FTensorLogicEngine::Initialize() {
    if (bInitialized) return;

    // Initialize default parameters
    GradState.LearningRate = 0.01f;
    GradState.MomentumBeta = 0.9f;

    bInitialized = true;
}

void FTensorLogicEngine::Reset() {
    KB = FKnowledgeBase();
    GradState = FGradientState();
    Parameters.clear();
    ComputationGraph.clear();
    bInitialized = false;
}

// ============================================================================
// Tensor Operations
// ============================================================================

FLogicalTensor FTensorLogicEngine::Einsum(const std::string& expression,
                                           const std::vector<FLogicalTensor>& inputs) {
    FEinsteinExpression expr(expression);

    if (inputs.size() == 0) {
        return FLogicalTensor();
    } else if (inputs.size() == 1) {
        // Unary operation (trace, etc.)
        return inputs[0];  // Simplified
    } else if (inputs.size() == 2) {
        return EinsumBinary(expr, inputs[0], inputs[1]);
    } else {
        // Chain of binary operations
        FLogicalTensor result = EinsumBinary(expr, inputs[0], inputs[1]);
        for (size_t i = 2; i < inputs.size(); i++) {
            result = EinsumBinary(expr, result, inputs[i]);
        }
        return result;
    }
}

FLogicalTensor FTensorLogicEngine::EinsumBinary(const FEinsteinExpression& expr,
                                                 const FLogicalTensor& A,
                                                 const FLogicalTensor& B) {
    // Simplified einsum for common cases

    std::vector<FTensorShape> shapes = {A.Shape, B.Shape};
    FTensorShape outShape = expr.ComputeOutputShape(shapes);

    FLogicalTensor result("einsum_result", outShape);

    // Handle common cases
    if (expr.Expression == "ij,jk->ik") {
        // Matrix multiplication
        if (A.Shape.Dimensions.size() == 2 && B.Shape.Dimensions.size() == 2) {
            int32_t M = A.Shape.Dimensions[0];
            int32_t K = A.Shape.Dimensions[1];
            int32_t N = B.Shape.Dimensions[1];

            result = FLogicalTensor("matmul", FTensorShape({M, N}));
            for (int32_t i = 0; i < M; i++) {
                for (int32_t j = 0; j < N; j++) {
                    float sum = 0.0f;
                    for (int32_t k = 0; k < K; k++) {
                        sum += A.At({i, k}) * B.At({k, j});
                    }
                    result.At({i, j}) = sum;
                }
            }
        }
    } else if (expr.Expression == "i,i->") {
        // Dot product
        result = FLogicalTensor("dot", FTensorShape({}));
        float sum = 0.0f;
        for (size_t i = 0; i < A.Data.size(); i++) {
            sum += A.Data[i] * B.Data[i];
        }
        result.Data = {sum};
    } else {
        // General case: outer product then contract
        result = OuterProduct(A, B);
    }

    if (bGradEnabled) {
        RecordOperation("einsum", {A.Name, B.Name}, result.Name);
    }

    return result;
}

FLogicalTensor FTensorLogicEngine::Contract(const FLogicalTensor& A, const FLogicalTensor& B,
                                             const std::vector<std::pair<int32_t, int32_t>>& axes) {
    // Generalized contraction
    // For now, delegate to einsum
    return Einsum("ij,jk->ik", {A, B});  // Simplified
}

FLogicalTensor FTensorLogicEngine::OuterProduct(const FLogicalTensor& A, const FLogicalTensor& B) {
    // Tensor product
    std::vector<int32_t> newDims = A.Shape.Dimensions;
    newDims.insert(newDims.end(), B.Shape.Dimensions.begin(), B.Shape.Dimensions.end());

    FLogicalTensor result("outer", FTensorShape(newDims));

    size_t idx = 0;
    for (float a : A.Data) {
        for (float b : B.Data) {
            result.Data[idx++] = a * b;
        }
    }

    return result;
}

float FTensorLogicEngine::Trace(const FLogicalTensor& A) {
    if (A.Shape.Rank() != 2) return 0.0f;
    if (A.Shape.Dimensions[0] != A.Shape.Dimensions[1]) return 0.0f;

    float trace = 0.0f;
    int32_t n = A.Shape.Dimensions[0];
    for (int32_t i = 0; i < n; i++) {
        trace += A.At({i, i});
    }
    return trace;
}

// ============================================================================
// Neural Operations
// ============================================================================

FLogicalTensor FTensorLogicEngine::Linear(const FLogicalTensor& input,
                                           const FLogicalTensor& weight,
                                           const FLogicalTensor* bias) {
    // Y = XW + b
    FLogicalTensor result = Einsum("ij,jk->ik", {input, weight});

    if (bias) {
        result.Add(*bias);
    }

    result.Name = "linear";
    return result;
}

FLogicalTensor FTensorLogicEngine::ApplyActivation(const FLogicalTensor& input, EActivation act) {
    FLogicalTensor result = input;
    result.Name = input.Name + "_activated";

    switch (act) {
        case EActivation::Identity:
            break;

        case EActivation::ReLU:
            result.Apply([](float x) { return std::max(0.0f, x); });
            break;

        case EActivation::Sigmoid:
            result.Apply([](float x) { return 1.0f / (1.0f + std::exp(-x)); });
            break;

        case EActivation::Tanh:
            result.Apply([](float x) { return std::tanh(x); });
            break;

        case EActivation::Softmax:
            result = input.Softmax();
            break;

        case EActivation::GELU:
            result.Apply([](float x) {
                return 0.5f * x * (1.0f + std::tanh(std::sqrt(2.0f / 3.14159f) *
                    (x + 0.044715f * x * x * x)));
            });
            break;

        case EActivation::SiLU:
            result.Apply([](float x) { return x / (1.0f + std::exp(-x)); });
            break;
    }

    return result;
}

FLogicalTensor FTensorLogicEngine::Activate(const FLogicalTensor& input, EActivation activation) {
    return ApplyActivation(input, activation);
}

FLogicalTensor FTensorLogicEngine::Attention(const FLogicalTensor& query,
                                              const FLogicalTensor& key,
                                              const FLogicalTensor& value,
                                              const FLogicalTensor* mask) {
    // Scaled dot-product attention: softmax(QK^T / sqrt(d_k)) * V

    // QK^T
    FLogicalTensor scores = Einsum("ij,kj->ik", {query, key});

    // Scale
    float scale = 1.0f / std::sqrt(static_cast<float>(key.Shape.Dimensions.back()));
    scores.Multiply(scale);

    // Mask (optional)
    if (mask) {
        for (size_t i = 0; i < scores.Data.size(); i++) {
            if (mask->Data[i % mask->Data.size()] < 0.5f) {
                scores.Data[i] = -1e9f;  // Large negative
            }
        }
    }

    // Softmax
    FLogicalTensor weights = scores.Softmax();

    // Attention output
    FLogicalTensor output = Einsum("ij,jk->ik", {weights, value});
    output.Name = "attention";

    return output;
}

FLogicalTensor FTensorLogicEngine::MultiHeadAttention(const FLogicalTensor& query,
                                                       const FLogicalTensor& key,
                                                       const FLogicalTensor& value,
                                                       int32_t numHeads) {
    // Simplified multi-head attention
    // In practice, would split into heads, process, and concatenate

    int32_t dModel = query.Shape.Dimensions.back();
    int32_t dK = dModel / numHeads;

    std::vector<FLogicalTensor> headOutputs;

    for (int32_t h = 0; h < numHeads; h++) {
        // For each head, compute attention (simplified - no projection)
        FLogicalTensor headOut = Attention(query, key, value);
        headOutputs.push_back(headOut);
    }

    // Concatenate heads (simplified - just average)
    FLogicalTensor result = headOutputs[0];
    for (size_t i = 1; i < headOutputs.size(); i++) {
        result.Add(headOutputs[i]);
    }
    result.Multiply(1.0f / static_cast<float>(numHeads));

    result.Name = "multihead_attention";
    return result;
}

// ============================================================================
// Symbolic Operations
// ============================================================================

FLogicalTensor FTensorLogicEngine::ApplyConnective(ELogicalConnective connective,
                                                    const std::vector<FLogicalTensor>& operands) {
    if (operands.empty()) return FLogicalTensor();

    FLogicalTensor result = operands[0];

    switch (connective) {
        case ELogicalConnective::Conjunction:
            for (size_t i = 1; i < operands.size(); i++) {
                result = result.And(operands[i]);
            }
            break;

        case ELogicalConnective::Disjunction:
            for (size_t i = 1; i < operands.size(); i++) {
                result = result.Or(operands[i]);
            }
            break;

        case ELogicalConnective::Negation:
            result = operands[0].Not();
            break;

        case ELogicalConnective::Implication:
            if (operands.size() >= 2) {
                result = operands[0].Implies(operands[1]);
            }
            break;

        case ELogicalConnective::Universal:
            // For all: min over elements
            result.Apply([&](float) { return result.Min(); });
            break;

        case ELogicalConnective::Existential:
            // Exists: max over elements
            result.Apply([&](float) { return result.Max(); });
            break;
    }

    return result;
}

FLogicalTensor FTensorLogicEngine::ApplyRule(const FSymbolicRule& rule,
                                              const std::vector<FLogicalTensor>& inputs) {
    return rule.Apply(inputs);
}

FLogicalTensor FTensorLogicEngine::Infer(const std::string& goal) {
    return KB.Query(goal);
}

bool FTensorLogicEngine::Prove(const std::string& goal, SymbolicBinding& bindings) {
    FLogicalTensor result = Infer(goal);
    if (result.Data.empty()) return false;

    // Check if truth degree exceeds threshold
    float confidence = result.Mean();
    bindings["confidence"] = confidence;
    return confidence > 0.5f;
}

// ============================================================================
// Hybrid Neural-Symbolic Operations
// ============================================================================

FLogicalTensor FTensorLogicEngine::NeuralGuidedInference(const std::string& goal,
                                                          const FLogicalTensor& context) {
    // Use context to score rules
    auto applicableRules = KB.GetRulesWithConclusion(goal);

    // Score rules based on context similarity
    std::vector<std::pair<float, size_t>> ruleScores;
    for (size_t i = 0; i < applicableRules.size(); i++) {
        // Simplified scoring
        float score = 1.0f / (i + 1);  // Prefer earlier rules
        ruleScores.emplace_back(score, i);
    }

    // Sort by score
    std::sort(ruleScores.begin(), ruleScores.end(), std::greater<>());

    // Apply best rule
    if (!ruleScores.empty()) {
        size_t bestIdx = ruleScores[0].second;
        const auto& bestRule = applicableRules[bestIdx];

        std::vector<FLogicalTensor> premises;
        for (const auto& p : bestRule.Premises) {
            FLogicalTensor premise = Infer(p);
            if (premise.Data.empty()) {
                premise = FLogicalTensor(p, FTensorShape({1}));
                premise.Uniform(0.5f);  // Unknown
            }
            premises.push_back(premise);
        }

        return bestRule.Apply(premises);
    }

    return FLogicalTensor();
}

void FTensorLogicEngine::ConstrainedLearning(const std::vector<FSymbolicRule>& constraints) {
    // Add constraints as penalty terms during learning
    for (const auto& constraint : constraints) {
        // Ensure rule is satisfied
        FLogicalTensor ruleOutput = Infer(constraint.Conclusion);
        // Add penalty if rule is violated
        // (Implementation would modify gradient computation)
    }
}

std::vector<FSymbolicRule> FTensorLogicEngine::LearnRules(
    const std::vector<FLogicalTensor>& examples,
    const std::vector<FLogicalTensor>& labels) {

    std::vector<FSymbolicRule> learnedRules;

    // Simplified rule learning: find correlations
    for (size_t i = 0; i < examples.size(); i++) {
        for (size_t j = 0; j < labels.size(); j++) {
            // Check if example predicts label
            float correlation = 0.0f;
            for (size_t k = 0; k < std::min(examples[i].Data.size(), labels[j].Data.size()); k++) {
                correlation += examples[i].Data[k] * labels[j].Data[k];
            }
            correlation /= static_cast<float>(std::max(examples[i].Data.size(), labels[j].Data.size()));

            if (correlation > 0.7f) {
                FSymbolicRule rule(
                    "learned_rule_" + std::to_string(i) + "_" + std::to_string(j),
                    {examples[i].Name},
                    labels[j].Name
                );
                rule.Confidence = correlation;
                rule.IsLearned = true;
                learnedRules.push_back(rule);
            }
        }
    }

    return learnedRules;
}

// ============================================================================
// Knowledge Base Wrappers
// ============================================================================

void FTensorLogicEngine::AddFact(const std::string& name, const FLogicalTensor& tensor) {
    KB.AddFact(name, tensor);
}

void FTensorLogicEngine::AddRule(const FSymbolicRule& rule) {
    KB.AddRule(rule);
}

// ============================================================================
// Gradient Computation
// ============================================================================

void FTensorLogicEngine::Backward(const FLogicalTensor& loss) {
    // Simplified backprop through computation graph
    // In practice, would use automatic differentiation

    // Initialize loss gradient
    GradState.Accumulate(loss.Name, std::vector<float>(loss.Data.size(), 1.0f));

    // Backward through graph (reverse order)
    for (auto it = ComputationGraph.rbegin(); it != ComputationGraph.rend(); ++it) {
        // Propagate gradients based on operation type
        // (Simplified - actual implementation would be more complex)
    }
}

void FTensorLogicEngine::Step() {
    GradState.Step(Parameters);
}

void FTensorLogicEngine::RecordOperation(const std::string& op,
                                          const std::vector<std::string>& inputs,
                                          const std::string& output) {
    if (bGradEnabled) {
        ComputationGraph.push_back({op, inputs, output});
    }
}

// ============================================================================
// Transformer Components
// ============================================================================

FLogicalTensor FTensorLogicEngine::TransformerEncoderLayer(const FLogicalTensor& input,
                                                            int32_t dModel,
                                                            int32_t numHeads,
                                                            int32_t dFF) {
    // Self-attention + FFN with residual connections

    // Self-attention
    FLogicalTensor attn = MultiHeadAttention(input, input, input, numHeads);

    // Residual + LayerNorm (simplified)
    FLogicalTensor afterAttn = input;
    afterAttn.Add(attn);
    afterAttn = afterAttn.Normalize();

    // Feed-forward network
    FLogicalTensor ff1Weight("ff1_w", FTensorShape({dModel, dFF}));
    ff1Weight.Random(-0.1f, 0.1f);
    FLogicalTensor ff2Weight("ff2_w", FTensorShape({dFF, dModel}));
    ff2Weight.Random(-0.1f, 0.1f);

    FLogicalTensor ffOut = Linear(afterAttn, ff1Weight);
    ffOut = Activate(ffOut, EActivation::GELU);
    ffOut = Linear(ffOut, ff2Weight);

    // Residual + LayerNorm
    afterAttn.Add(ffOut);
    afterAttn = afterAttn.Normalize();

    return afterAttn;
}

FLogicalTensor FTensorLogicEngine::TransformerDecoderLayer(const FLogicalTensor& input,
                                                            const FLogicalTensor& memory,
                                                            int32_t dModel,
                                                            int32_t numHeads,
                                                            int32_t dFF) {
    // Self-attention
    FLogicalTensor selfAttn = MultiHeadAttention(input, input, input, numHeads);
    FLogicalTensor afterSelf = input;
    afterSelf.Add(selfAttn);
    afterSelf = afterSelf.Normalize();

    // Cross-attention
    FLogicalTensor crossAttn = MultiHeadAttention(afterSelf, memory, memory, numHeads);
    FLogicalTensor afterCross = afterSelf;
    afterCross.Add(crossAttn);
    afterCross = afterCross.Normalize();

    // FFN
    FLogicalTensor ff1Weight("ff1_w", FTensorShape({dModel, dFF}));
    ff1Weight.Random(-0.1f, 0.1f);
    FLogicalTensor ff2Weight("ff2_w", FTensorShape({dFF, dModel}));
    ff2Weight.Random(-0.1f, 0.1f);

    FLogicalTensor ffOut = Linear(afterCross, ff1Weight);
    ffOut = Activate(ffOut, EActivation::GELU);
    ffOut = Linear(ffOut, ff2Weight);

    afterCross.Add(ffOut);
    afterCross = afterCross.Normalize();

    return afterCross;
}

// ============================================================================
// Graphical Model Components
// ============================================================================

FLogicalTensor FTensorLogicEngine::FactorGraphPass(
    const std::vector<FLogicalTensor>& factors,
    const std::vector<std::vector<int32_t>>& connections) {

    // Sum-product message passing
    if (factors.empty()) return FLogicalTensor();

    FLogicalTensor result = factors[0];
    for (size_t i = 1; i < factors.size(); i++) {
        result = result.And(factors[i]);
    }

    return result;
}

std::vector<FLogicalTensor> FTensorLogicEngine::BeliefPropagation(
    const std::vector<FLogicalTensor>& beliefs,
    const std::vector<FLogicalTensor>& factors,
    int numIterations) {

    std::vector<FLogicalTensor> currentBeliefs = beliefs;

    for (int iter = 0; iter < numIterations; iter++) {
        // Update each belief based on factors
        for (size_t i = 0; i < currentBeliefs.size(); i++) {
            FLogicalTensor newBelief = currentBeliefs[i];

            // Multiply with relevant factors
            for (const auto& factor : factors) {
                newBelief = newBelief.And(factor);
            }

            newBelief = newBelief.Normalize();
            currentBeliefs[i] = newBelief;
        }
    }

    return currentBeliefs;
}

// ============================================================================
// Kernel Methods
// ============================================================================

FLogicalTensor FTensorLogicEngine::KernelMatrix(const FLogicalTensor& X,
                                                 const FLogicalTensor& Y,
                                                 const std::string& kernelType,
                                                 float gamma) {
    int32_t nX = X.Shape.Dimensions[0];
    int32_t nY = Y.Shape.Dimensions[0];
    int32_t d = X.Shape.Dimensions.back();

    FLogicalTensor K("kernel", FTensorShape({nX, nY}));

    for (int32_t i = 0; i < nX; i++) {
        for (int32_t j = 0; j < nY; j++) {
            float val = 0.0f;

            if (kernelType == "linear") {
                // Linear kernel: K(x,y) = x^T y
                for (int32_t k = 0; k < d; k++) {
                    val += X.At({i, k}) * Y.At({j, k});
                }
            } else if (kernelType == "rbf") {
                // RBF kernel: K(x,y) = exp(-gamma * ||x-y||^2)
                float sqDist = 0.0f;
                for (int32_t k = 0; k < d; k++) {
                    float diff = X.At({i, k}) - Y.At({j, k});
                    sqDist += diff * diff;
                }
                val = std::exp(-gamma * sqDist);
            } else if (kernelType == "polynomial") {
                // Polynomial kernel: K(x,y) = (x^T y + 1)^degree
                for (int32_t k = 0; k < d; k++) {
                    val += X.At({i, k}) * Y.At({j, k});
                }
                val = std::pow(val + 1.0f, 3.0f);  // degree=3
            }

            K.At({i, j}) = val;
        }
    }

    return K;
}

FLogicalTensor FTensorLogicEngine::KernelAttention(const FLogicalTensor& query,
                                                    const FLogicalTensor& key,
                                                    const FLogicalTensor& value) {
    // Kernelized attention using random features
    // Approximates softmax attention with linear complexity

    // For simplicity, use RBF kernel
    FLogicalTensor K = KernelMatrix(query, key, "rbf", 1.0f);
    K = K.Normalize();

    return Einsum("ij,jk->ik", {K, value});
}

// ============================================================================
// Cognitive Integration
// ============================================================================

FLogicalTensor FTensorLogicEngine::ProcessCognitiveState(const FLogicalTensor& state,
                                                          int32_t step,
                                                          int32_t stream) {
    // Process through tensor logic based on cognitive phase

    // Steps 1,5,9 = Perceiving (stream 0)
    // Steps 2,6,10 = Acting (stream 1)
    // Steps 3,7,11 = Reflecting (stream 2)

    FLogicalTensor processed = state;
    processed.Name = "processed_step_" + std::to_string(step);

    int32_t phase = (step - 1) % 4;

    switch (phase) {
        case 0:  // Perception phase
            // Apply attention to focus on salient features
            processed = Attention(state, state, state);
            break;

        case 1:  // Integration phase
            // Combine with stored beliefs
            {
                FLogicalTensor beliefs = Infer("current_beliefs");
                if (!beliefs.Data.empty()) {
                    processed = processed.And(beliefs);
                }
            }
            break;

        case 2:  // Action phase
            // Select action based on state
            processed = processed.Softmax();
            break;

        case 3:  // Reflection phase
            // Update beliefs based on outcome
            AddFact("last_state", processed);
            break;
    }

    return processed;
}

FLogicalTensor FTensorLogicEngine::UpdateBeliefs(const FLogicalTensor& prior,
                                                  const FLogicalTensor& observation) {
    // Bayesian update: posterior ∝ likelihood × prior
    FLogicalTensor posterior = prior.And(observation);
    posterior = posterior.Normalize();
    posterior.Name = "posterior";
    return posterior;
}

FLogicalTensor FTensorLogicEngine::SelectAction(const FLogicalTensor& beliefs,
                                                 const FLogicalTensor& preferences) {
    // Action selection via tensor contraction
    // Score each action by expected utility

    FLogicalTensor utility = beliefs.And(preferences);
    FLogicalTensor actionProbs = utility.Softmax();
    actionProbs.Name = "action_probs";

    return actionProbs;
}

} // namespace DeepTreeEcho

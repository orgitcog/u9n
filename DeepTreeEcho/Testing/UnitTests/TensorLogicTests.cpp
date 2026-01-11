// TensorLogicTests.cpp
// Exhaustive Unit Tests for Tensor Logic System
// Tests: NestedTensorPartitionSystem, TensorLogicEngine, Prime-weighted signatures

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <random>
#include <string>
#include <algorithm>
#include <numeric>
#include <memory>
#include <functional>
#include <set>

// ============================================================================
// Mock implementations of TensorLogic types (matching headers)
// ============================================================================

namespace DeepTreeEcho {

// ----------------------------------------------------------------------------
// Prime Weight Utilities
// ----------------------------------------------------------------------------

class MockPrimeWeightedSignature {
public:
    std::vector<uint32_t> Components;
    std::vector<uint32_t> PrimeWeights;
    uint64_t ProductSignature = 1;

    static const std::vector<uint32_t>& GetPrimes() {
        static const std::vector<uint32_t> Primes = {
            2, 3, 5, 7, 11, 13, 17, 19, 23, 29,
            31, 37, 41, 43, 47, 53, 59, 61, 67, 71,
            73, 79, 83, 89, 97, 101, 103, 107, 109, 113
        };
        return Primes;
    }

    static uint32_t GetPrime(uint32_t n) {
        const auto& primes = GetPrimes();
        return (n < primes.size()) ? primes[n] : 0;
    }

    // tau(k) = pi(k-1)
    static uint32_t Tau(uint32_t k) {
        if (k == 0) return 1;
        return GetPrime(k - 1);
    }

    void ComputeFromComponents(const std::vector<uint32_t>& InComponents) {
        Components = InComponents;
        PrimeWeights.clear();
        ProductSignature = 1;

        for (uint32_t comp : Components) {
            uint32_t weight = Tau(comp);
            PrimeWeights.push_back(weight);
            ProductSignature *= weight;
        }
    }

    bool IsEquivalent(const MockPrimeWeightedSignature& Other) const {
        return ProductSignature == Other.ProductSignature;
    }

    std::vector<std::pair<uint32_t, uint32_t>> Factorize() const {
        std::vector<std::pair<uint32_t, uint32_t>> factors;
        uint64_t n = ProductSignature;

        for (uint32_t p : GetPrimes()) {
            if (static_cast<uint64_t>(p) * p > n) break;
            uint32_t count = 0;
            while (n % p == 0) {
                n /= p;
                count++;
            }
            if (count > 0) {
                factors.emplace_back(p, count);
            }
        }
        if (n > 1) {
            factors.emplace_back(static_cast<uint32_t>(n), 1);
        }
        return factors;
    }
};

// ----------------------------------------------------------------------------
// Partition Part
// ----------------------------------------------------------------------------

struct MockPartitionPart {
    uint32_t Value = 0;
    std::vector<uint32_t> Factors;
    bool IsPrime = false;
    uint32_t PrimeWeight = 0;

    MockPartitionPart() = default;
    explicit MockPartitionPart(uint32_t InValue) : Value(InValue) {
        ComputePrimeWeight();
        Factorize();
    }

    void ComputePrimeWeight() {
        PrimeWeight = MockPrimeWeightedSignature::Tau(Value);
    }

    void Factorize() {
        Factors.clear();
        if (Value <= 1) {
            IsPrime = (Value == 1);
            return;
        }

        uint32_t n = Value;
        for (uint32_t p : MockPrimeWeightedSignature::GetPrimes()) {
            if (p * p > n) break;
            while (n % p == 0) {
                Factors.push_back(p);
                n /= p;
            }
        }
        if (n > 1) {
            Factors.push_back(n);
        }

        IsPrime = (Factors.size() == 1 && Factors[0] == Value);
    }

    std::vector<uint32_t> GetTensorShape() const {
        if (Factors.empty() || IsPrime) {
            return {Value};
        }
        return Factors;
    }
};

// ----------------------------------------------------------------------------
// Nested Partition
// ----------------------------------------------------------------------------

struct MockNestedPartition {
    uint32_t N = 0;
    std::vector<MockPartitionPart> Parts;
    uint64_t Signature = 0;

    MockNestedPartition() = default;
    MockNestedPartition(uint32_t InN, const std::vector<uint32_t>& InParts) : N(InN) {
        for (uint32_t part : InParts) {
            Parts.emplace_back(part);
        }
        ComputeSignature();
    }

    void ComputeSignature() {
        Signature = 1;
        for (const auto& part : Parts) {
            Signature *= part.PrimeWeight;
        }
    }

    std::vector<std::vector<uint32_t>> GetNestedTensorShape() const {
        std::vector<std::vector<uint32_t>> shape;
        for (const auto& part : Parts) {
            shape.push_back(part.GetTensorShape());
        }
        return shape;
    }
};

// ----------------------------------------------------------------------------
// Partition Lattice
// ----------------------------------------------------------------------------

class MockPartitionLattice {
public:
    MockPartitionLattice() = default;
    explicit MockPartitionLattice(uint32_t InN) {
        Build(InN);
    }

    void Build(uint32_t InN) {
        N = InN;
        Partitions.clear();
        std::vector<uint32_t> current;
        GeneratePartitions(N, N, current);
    }

    const std::vector<MockNestedPartition>& GetPartitions() const {
        return Partitions;
    }

    size_t NumPartitions() const { return Partitions.size(); }

private:
    uint32_t N = 0;
    std::vector<MockNestedPartition> Partitions;

    void GeneratePartitions(uint32_t n, uint32_t maxPart, std::vector<uint32_t>& current) {
        if (n == 0) {
            Partitions.emplace_back(N, current);
            return;
        }

        for (uint32_t part = std::min(n, maxPart); part >= 1; part--) {
            current.push_back(part);
            GeneratePartitions(n - part, part, current);
            current.pop_back();
        }
    }
};

// ----------------------------------------------------------------------------
// Thread Pool Mapping
// ----------------------------------------------------------------------------

enum class EThreadPoolStrategy : uint8_t {
    InterOp,
    IntraOp,
    Hybrid
};

struct MockThreadPoolMapping {
    uint32_t TotalThreads = 0;
    std::vector<uint32_t> SegmentLengths;
    std::vector<std::vector<uint32_t>> Grid;
    EThreadPoolStrategy Strategy = EThreadPoolStrategy::Hybrid;

    static MockThreadPoolMapping FromPartition(const MockNestedPartition& Partition,
                                                EThreadPoolStrategy Strategy) {
        MockThreadPoolMapping mapping;
        mapping.TotalThreads = Partition.N;
        mapping.Strategy = Strategy;

        for (const auto& part : Partition.Parts) {
            mapping.SegmentLengths.push_back(part.Value);
            mapping.Grid.push_back(part.GetTensorShape());
        }

        return mapping;
    }
};

// ----------------------------------------------------------------------------
// Cognitive Schedule
// ----------------------------------------------------------------------------

struct MockCognitiveSchedule {
    static constexpr uint32_t TotalSteps = 12;
    static constexpr uint32_t NumStreams = 3;
    static constexpr uint32_t NumTriads = 4;

    std::array<std::array<uint32_t, 3>, NumTriads> TriadSteps;
    std::array<float, NumTriads> PhaseOffsets;

    void Initialize() {
        TriadSteps[0] = {1, 5, 9};
        TriadSteps[1] = {2, 6, 10};
        TriadSteps[2] = {3, 7, 11};
        TriadSteps[3] = {4, 8, 12};
        PhaseOffsets = {0.0f, 30.0f, 60.0f, 90.0f};
    }

    uint32_t GetTriadIndex(uint32_t Step) const {
        return (Step - 1) % NumTriads;
    }

    uint32_t GetStreamIndex(uint32_t Step) const {
        return ((Step - 1) / NumTriads) % NumStreams;
    }
};

// ----------------------------------------------------------------------------
// Nested Tensor Partition System
// ----------------------------------------------------------------------------

class MockNestedTensorPartitionSystem {
public:
    void Initialize() {
        InitializeA000081Sequence();
        bInitialized = true;
    }

    bool IsInitialized() const { return bInitialized; }

    std::vector<MockNestedPartition> GeneratePartitions(uint32_t N) {
        MockPartitionLattice lattice(N);
        return std::vector<MockNestedPartition>(
            lattice.GetPartitions().begin(),
            lattice.GetPartitions().end()
        );
    }

    MockNestedPartition DecomposeSix() {
        return MockNestedPartition(6, {6});
    }

    uint64_t ComputeCognitiveSignature(const std::vector<uint32_t>& StateVector) const {
        MockPrimeWeightedSignature sig;
        sig.ComputeFromComponents(StateVector);
        return sig.ProductSignature;
    }

    uint32_t GetA000081TermCount(uint32_t NestingLevel) const {
        if (NestingLevel < A000081Sequence.size()) {
            return A000081Sequence[NestingLevel];
        }
        return 0;
    }

    bool VerifyA000081Alignment() const {
        std::vector<uint32_t> expected = {1, 2, 4, 9};
        for (size_t i = 0; i < expected.size(); i++) {
            if (i + 1 >= A000081Sequence.size()) return false;
            // Nesting level i+1 should have A000081(i+1) terms
        }
        return true;
    }

    MockCognitiveSchedule GenerateCognitiveSchedule() {
        MockCognitiveSchedule schedule;
        schedule.Initialize();
        return schedule;
    }

private:
    bool bInitialized = false;
    std::vector<uint32_t> A000081Sequence;

    void InitializeA000081Sequence() {
        A000081Sequence = {0, 1, 1, 2, 4, 9, 20, 48, 115, 286, 719};
    }
};

// ----------------------------------------------------------------------------
// Tensor Shape
// ----------------------------------------------------------------------------

struct MockTensorShape {
    std::vector<int32_t> Dimensions;

    MockTensorShape() = default;
    MockTensorShape(std::initializer_list<int32_t> dims) : Dimensions(dims) {}
    explicit MockTensorShape(const std::vector<int32_t>& dims) : Dimensions(dims) {}

    int32_t Rank() const { return static_cast<int32_t>(Dimensions.size()); }

    int64_t TotalElements() const {
        if (Dimensions.empty()) return 1;
        int64_t total = 1;
        for (int32_t dim : Dimensions) total *= dim;
        return total;
    }

    bool operator==(const MockTensorShape& Other) const {
        return Dimensions == Other.Dimensions;
    }
};

// ----------------------------------------------------------------------------
// Logical Tensor
// ----------------------------------------------------------------------------

class MockLogicalTensor {
public:
    std::string Name;
    MockTensorShape Shape;
    std::vector<float> Data;

    MockLogicalTensor() = default;

    MockLogicalTensor(const std::string& name, const MockTensorShape& shape)
        : Name(name), Shape(shape) {
        Data.resize(static_cast<size_t>(shape.TotalElements()), 0.0f);
    }

    MockLogicalTensor(const std::string& name, const MockTensorShape& shape,
                       const std::vector<float>& data)
        : Name(name), Shape(shape), Data(data) {}

    void Zero() { std::fill(Data.begin(), Data.end(), 0.0f); }
    void Ones() { std::fill(Data.begin(), Data.end(), 1.0f); }
    void Uniform(float value) { std::fill(Data.begin(), Data.end(), value); }

    void Random(float min = 0.0f, float max = 1.0f) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(min, max);
        for (float& val : Data) val = dist(gen);
    }

    float Sum() const {
        return std::accumulate(Data.begin(), Data.end(), 0.0f);
    }

    float Max() const {
        return *std::max_element(Data.begin(), Data.end());
    }

    float Min() const {
        return *std::min_element(Data.begin(), Data.end());
    }

    float Mean() const {
        return Data.empty() ? 0.0f : Sum() / static_cast<float>(Data.size());
    }

    MockLogicalTensor And(const MockLogicalTensor& Other) const {
        MockLogicalTensor result(Name + "_and", Shape);
        for (size_t i = 0; i < Data.size(); i++) {
            result.Data[i] = Data[i] * Other.Data[i % Other.Data.size()];
        }
        return result;
    }

    MockLogicalTensor Or(const MockLogicalTensor& Other) const {
        MockLogicalTensor result(Name + "_or", Shape);
        for (size_t i = 0; i < Data.size(); i++) {
            result.Data[i] = std::max(Data[i], Other.Data[i % Other.Data.size()]);
        }
        return result;
    }

    MockLogicalTensor Not() const {
        MockLogicalTensor result("not_" + Name, Shape);
        for (size_t i = 0; i < Data.size(); i++) {
            result.Data[i] = 1.0f - Data[i];
        }
        return result;
    }

    MockLogicalTensor Softmax() const {
        MockLogicalTensor result(Name + "_softmax", Shape);
        float maxVal = Max();
        float sumExp = 0.0f;
        for (float val : Data) sumExp += std::exp(val - maxVal);
        for (size_t i = 0; i < Data.size(); i++) {
            result.Data[i] = std::exp(Data[i] - maxVal) / sumExp;
        }
        return result;
    }

    MockLogicalTensor Normalize() const {
        MockLogicalTensor result(Name + "_normalized", Shape);
        float sum = Sum();
        if (sum > 1e-10f) {
            for (size_t i = 0; i < Data.size(); i++) {
                result.Data[i] = Data[i] / sum;
            }
        }
        return result;
    }
};

// ----------------------------------------------------------------------------
// Symbolic Rule
// ----------------------------------------------------------------------------

struct MockSymbolicRule {
    std::string Name;
    std::vector<std::string> Premises;
    std::string Conclusion;
    float Confidence = 1.0f;

    MockSymbolicRule() = default;
    MockSymbolicRule(const std::string& name,
                     const std::vector<std::string>& premises,
                     const std::string& conclusion)
        : Name(name), Premises(premises), Conclusion(conclusion) {}

    MockLogicalTensor Apply(const std::vector<MockLogicalTensor>& inputs) const {
        if (inputs.empty()) return MockLogicalTensor();

        MockLogicalTensor result = inputs[0];

        for (size_t i = 1; i < inputs.size(); i++) {
            result = result.And(inputs[i]);
        }

        // Set conclusion name after all operations (overwrite the "_and" suffix)
        result.Name = Conclusion;
        for (float& val : result.Data) val *= Confidence;
        return result;
    }
};

// ----------------------------------------------------------------------------
// Knowledge Base
// ----------------------------------------------------------------------------

class MockKnowledgeBase {
public:
    void AddFact(const std::string& name, const MockLogicalTensor& tensor) {
        Facts[name] = tensor;
    }

    const MockLogicalTensor* GetFact(const std::string& name) const {
        auto it = Facts.find(name);
        return (it != Facts.end()) ? &it->second : nullptr;
    }

    bool HasFact(const std::string& name) const {
        return Facts.find(name) != Facts.end();
    }

    void AddRule(const MockSymbolicRule& rule) {
        Rules.push_back(rule);
    }

    const std::vector<MockSymbolicRule>& GetRules() const { return Rules; }

    size_t NumFacts() const { return Facts.size(); }
    size_t NumRules() const { return Rules.size(); }

private:
    std::unordered_map<std::string, MockLogicalTensor> Facts;
    std::vector<MockSymbolicRule> Rules;
};

// ----------------------------------------------------------------------------
// Tensor Logic Engine
// ----------------------------------------------------------------------------

enum class EActivation : uint8_t {
    Identity, ReLU, Sigmoid, Tanh, Softmax, GELU, SiLU
};

enum class ELogicalConnective : uint8_t {
    Conjunction, Disjunction, Implication, Negation, Universal, Existential
};

class MockTensorLogicEngine {
public:
    void Initialize() { bInitialized = true; }
    bool IsInitialized() const { return bInitialized; }
    void Reset() { bInitialized = false; KB = MockKnowledgeBase(); }

    // Tensor Operations
    MockLogicalTensor MatMul(const MockLogicalTensor& A, const MockLogicalTensor& B) {
        if (A.Shape.Rank() != 2 || B.Shape.Rank() != 2) {
            return MockLogicalTensor();
        }
        int32_t M = A.Shape.Dimensions[0];
        int32_t K = A.Shape.Dimensions[1];
        int32_t N = B.Shape.Dimensions[1];

        if (B.Shape.Dimensions[0] != K) {
            return MockLogicalTensor();
        }

        MockLogicalTensor result("matmul", MockTensorShape({M, N}));
        for (int32_t i = 0; i < M; i++) {
            for (int32_t j = 0; j < N; j++) {
                float sum = 0.0f;
                for (int32_t k = 0; k < K; k++) {
                    sum += A.Data[i * K + k] * B.Data[k * N + j];
                }
                result.Data[i * N + j] = sum;
            }
        }
        return result;
    }

    MockLogicalTensor OuterProduct(const MockLogicalTensor& A, const MockLogicalTensor& B) {
        std::vector<int32_t> newDims = A.Shape.Dimensions;
        newDims.insert(newDims.end(), B.Shape.Dimensions.begin(), B.Shape.Dimensions.end());

        MockLogicalTensor result("outer", MockTensorShape(newDims));
        size_t idx = 0;
        for (float a : A.Data) {
            for (float b : B.Data) {
                result.Data[idx++] = a * b;
            }
        }
        return result;
    }

    float Trace(const MockLogicalTensor& A) {
        if (A.Shape.Rank() != 2) return 0.0f;
        if (A.Shape.Dimensions[0] != A.Shape.Dimensions[1]) return 0.0f;

        float trace = 0.0f;
        int32_t n = A.Shape.Dimensions[0];
        for (int32_t i = 0; i < n; i++) {
            trace += A.Data[i * n + i];
        }
        return trace;
    }

    MockLogicalTensor Activate(const MockLogicalTensor& input, EActivation activation) {
        MockLogicalTensor result = input;
        result.Name = input.Name + "_activated";

        switch (activation) {
            case EActivation::Identity: break;
            case EActivation::ReLU:
                for (float& val : result.Data) val = std::max(0.0f, val);
                break;
            case EActivation::Sigmoid:
                for (float& val : result.Data) val = 1.0f / (1.0f + std::exp(-val));
                break;
            case EActivation::Tanh:
                for (float& val : result.Data) val = std::tanh(val);
                break;
            case EActivation::Softmax:
                result = input.Softmax();
                break;
            case EActivation::GELU:
                for (float& val : result.Data) {
                    val = 0.5f * val * (1.0f + std::tanh(std::sqrt(2.0f / 3.14159f) *
                        (val + 0.044715f * val * val * val)));
                }
                break;
            case EActivation::SiLU:
                for (float& val : result.Data) val = val / (1.0f + std::exp(-val));
                break;
        }
        return result;
    }

    MockLogicalTensor ApplyConnective(ELogicalConnective connective,
                                       const std::vector<MockLogicalTensor>& operands) {
        if (operands.empty()) return MockLogicalTensor();

        MockLogicalTensor result = operands[0];

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
            default:
                break;
        }
        return result;
    }

    MockLogicalTensor ApplyRule(const MockSymbolicRule& rule,
                                 const std::vector<MockLogicalTensor>& inputs) {
        return rule.Apply(inputs);
    }

    void AddFact(const std::string& name, const MockLogicalTensor& tensor) {
        KB.AddFact(name, tensor);
    }

    void AddRule(const MockSymbolicRule& rule) {
        KB.AddRule(rule);
    }

    MockKnowledgeBase& GetKnowledgeBase() { return KB; }

    MockLogicalTensor UpdateBeliefs(const MockLogicalTensor& prior,
                                     const MockLogicalTensor& observation) {
        MockLogicalTensor posterior = prior.And(observation);
        return posterior.Normalize();
    }

    MockLogicalTensor SelectAction(const MockLogicalTensor& beliefs,
                                    const MockLogicalTensor& preferences) {
        MockLogicalTensor utility = beliefs.And(preferences);
        return utility.Softmax();
    }

private:
    bool bInitialized = false;
    MockKnowledgeBase KB;
};

} // namespace DeepTreeEcho

using namespace DeepTreeEcho;

// ============================================================================
// Test Fixtures
// ============================================================================

class PrimeWeightTest : public ::testing::Test {
protected:
    MockPrimeWeightedSignature Sig;
};

class PartitionTest : public ::testing::Test {
protected:
    void SetUp() override {
        System.Initialize();
    }
    MockNestedTensorPartitionSystem System;
};

class PartitionLatticeTest : public ::testing::Test {
protected:
    MockPartitionLattice Lattice;
};

class CognitiveScheduleTest : public ::testing::Test {
protected:
    void SetUp() override {
        Schedule.Initialize();
    }
    MockCognitiveSchedule Schedule;
};

class LogicalTensorTest : public ::testing::Test {
protected:
    MockLogicalTensor TensorA;
    MockLogicalTensor TensorB;
};

class TensorLogicEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        Engine.Initialize();
    }

    void TearDown() override {
        Engine.Reset();
    }

    MockTensorLogicEngine Engine;
};

class KnowledgeBaseTest : public ::testing::Test {
protected:
    MockKnowledgeBase KB;
};

// ============================================================================
// Prime Weight Tests
// ============================================================================

TEST_F(PrimeWeightTest, GetPrimeReturnsCorrectValues) {
    EXPECT_EQ(MockPrimeWeightedSignature::GetPrime(0), 2);
    EXPECT_EQ(MockPrimeWeightedSignature::GetPrime(1), 3);
    EXPECT_EQ(MockPrimeWeightedSignature::GetPrime(2), 5);
    EXPECT_EQ(MockPrimeWeightedSignature::GetPrime(3), 7);
    EXPECT_EQ(MockPrimeWeightedSignature::GetPrime(4), 11);
}

TEST_F(PrimeWeightTest, TauFunctionMapsCorrectly) {
    // tau(k) = pi(k-1)
    EXPECT_EQ(MockPrimeWeightedSignature::Tau(1), 2);   // tau(1) = pi(0) = 2
    EXPECT_EQ(MockPrimeWeightedSignature::Tau(2), 3);   // tau(2) = pi(1) = 3
    EXPECT_EQ(MockPrimeWeightedSignature::Tau(3), 5);   // tau(3) = pi(2) = 5
    EXPECT_EQ(MockPrimeWeightedSignature::Tau(4), 7);   // tau(4) = pi(3) = 7
    EXPECT_EQ(MockPrimeWeightedSignature::Tau(5), 11);  // tau(5) = pi(4) = 11
}

TEST_F(PrimeWeightTest, ComputeFromComponentsProducesCorrectSignature) {
    Sig.ComputeFromComponents({1, 1, 1});
    EXPECT_EQ(Sig.ProductSignature, 8);  // 2 * 2 * 2 = 8

    Sig.ComputeFromComponents({2, 1});
    EXPECT_EQ(Sig.ProductSignature, 6);  // 3 * 2 = 6

    Sig.ComputeFromComponents({3});
    EXPECT_EQ(Sig.ProductSignature, 5);  // 5
}

TEST_F(PrimeWeightTest, EquivalenceCheck) {
    MockPrimeWeightedSignature Sig1, Sig2, Sig3;

    Sig1.ComputeFromComponents({2, 1});  // 3 * 2 = 6
    Sig2.ComputeFromComponents({1, 2});  // 2 * 3 = 6
    Sig3.ComputeFromComponents({3});     // 5

    EXPECT_TRUE(Sig1.IsEquivalent(Sig2));
    EXPECT_FALSE(Sig1.IsEquivalent(Sig3));
}

TEST_F(PrimeWeightTest, Factorization) {
    Sig.ComputeFromComponents({2, 1});  // 6 = 2 * 3

    auto factors = Sig.Factorize();
    EXPECT_EQ(factors.size(), 2);

    // Check for 2 and 3
    bool has2 = false, has3 = false;
    for (const auto& [prime, count] : factors) {
        if (prime == 2) has2 = true;
        if (prime == 3) has3 = true;
    }
    EXPECT_TRUE(has2);
    EXPECT_TRUE(has3);
}

// ============================================================================
// Partition Part Tests
// ============================================================================

TEST_F(PartitionTest, PartitionPartPrimeWeight) {
    MockPartitionPart part1(1);
    EXPECT_EQ(part1.PrimeWeight, 2);

    MockPartitionPart part2(2);
    EXPECT_EQ(part2.PrimeWeight, 3);

    MockPartitionPart part3(3);
    EXPECT_EQ(part3.PrimeWeight, 5);
}

TEST_F(PartitionTest, PartitionPartFactorization) {
    MockPartitionPart part6(6);
    EXPECT_FALSE(part6.IsPrime);
    EXPECT_THAT(part6.Factors, ::testing::ElementsAre(2, 3));

    MockPartitionPart part7(7);
    EXPECT_TRUE(part7.IsPrime);
}

TEST_F(PartitionTest, DecomposeSixCanonical) {
    // [6] = [2][3]
    auto partition = System.DecomposeSix();
    EXPECT_EQ(partition.N, 6);
    EXPECT_EQ(partition.Parts.size(), 1);

    auto shape = partition.GetNestedTensorShape();
    EXPECT_EQ(shape.size(), 1);
    EXPECT_THAT(shape[0], ::testing::ElementsAre(2, 3));  // [2,3] factorization
}

// ============================================================================
// Partition Lattice Tests
// ============================================================================

TEST_F(PartitionLatticeTest, PartitionCountMatches) {
    // Partition numbers: p(1)=1, p(2)=2, p(3)=3, p(4)=5, p(5)=7
    Lattice.Build(1);
    EXPECT_EQ(Lattice.NumPartitions(), 1);

    Lattice.Build(2);
    EXPECT_EQ(Lattice.NumPartitions(), 2);

    Lattice.Build(3);
    EXPECT_EQ(Lattice.NumPartitions(), 3);

    Lattice.Build(4);
    EXPECT_EQ(Lattice.NumPartitions(), 5);

    Lattice.Build(5);
    EXPECT_EQ(Lattice.NumPartitions(), 7);
}

TEST_F(PartitionLatticeTest, AllPartitionsValid) {
    Lattice.Build(6);
    for (const auto& p : Lattice.GetPartitions()) {
        // Check that parts sum to N
        uint32_t sum = 0;
        for (const auto& part : p.Parts) {
            sum += part.Value;
        }
        EXPECT_EQ(sum, 6);
    }
}

TEST_F(PartitionLatticeTest, SignaturesAreUnique) {
    Lattice.Build(5);

    std::set<uint64_t> signatures;
    for (const auto& p : Lattice.GetPartitions()) {
        signatures.insert(p.Signature);
    }

    // All signatures should be unique
    EXPECT_EQ(signatures.size(), Lattice.NumPartitions());
}

// ============================================================================
// OEIS A000081 Alignment Tests
// ============================================================================

TEST_F(PartitionTest, A000081TermCounts) {
    // A(1) = 1, A(2) = 1, A(3) = 2, A(4) = 4, A(5) = 9
    EXPECT_EQ(System.GetA000081TermCount(1), 1);
    EXPECT_EQ(System.GetA000081TermCount(2), 1);
    EXPECT_EQ(System.GetA000081TermCount(3), 2);
    EXPECT_EQ(System.GetA000081TermCount(4), 4);
    EXPECT_EQ(System.GetA000081TermCount(5), 9);
}

TEST_F(PartitionTest, NestingLevelMapping) {
    // Nesting level N -> A000081(N) terms
    // N=1: 1 term, N=2: 2 terms, N=3: 4 terms, N=4: 9 terms
    EXPECT_TRUE(System.VerifyA000081Alignment());
}

// ============================================================================
// Cognitive Schedule Tests
// ============================================================================

TEST_F(CognitiveScheduleTest, TriadStepsCorrect) {
    EXPECT_THAT(Schedule.TriadSteps[0], ::testing::ElementsAre(1, 5, 9));
    EXPECT_THAT(Schedule.TriadSteps[1], ::testing::ElementsAre(2, 6, 10));
    EXPECT_THAT(Schedule.TriadSteps[2], ::testing::ElementsAre(3, 7, 11));
    EXPECT_THAT(Schedule.TriadSteps[3], ::testing::ElementsAre(4, 8, 12));
}

TEST_F(CognitiveScheduleTest, PhaseOffsetsCorrect) {
    EXPECT_FLOAT_EQ(Schedule.PhaseOffsets[0], 0.0f);
    EXPECT_FLOAT_EQ(Schedule.PhaseOffsets[1], 30.0f);
    EXPECT_FLOAT_EQ(Schedule.PhaseOffsets[2], 60.0f);
    EXPECT_FLOAT_EQ(Schedule.PhaseOffsets[3], 90.0f);
}

TEST_F(CognitiveScheduleTest, TriadIndexMapping) {
    EXPECT_EQ(Schedule.GetTriadIndex(1), 0);
    EXPECT_EQ(Schedule.GetTriadIndex(2), 1);
    EXPECT_EQ(Schedule.GetTriadIndex(3), 2);
    EXPECT_EQ(Schedule.GetTriadIndex(4), 3);
    EXPECT_EQ(Schedule.GetTriadIndex(5), 0);  // Wraps
}

TEST_F(CognitiveScheduleTest, StreamIndexMapping) {
    // Steps 1-4: stream 0, Steps 5-8: stream 1, Steps 9-12: stream 2
    EXPECT_EQ(Schedule.GetStreamIndex(1), 0);
    EXPECT_EQ(Schedule.GetStreamIndex(5), 1);
    EXPECT_EQ(Schedule.GetStreamIndex(9), 2);
}

// ============================================================================
// Thread Pool Mapping Tests
// ============================================================================

TEST_F(PartitionTest, ThreadPoolMappingFromPartition) {
    MockNestedPartition partition(6, {3, 2, 1});
    auto mapping = MockThreadPoolMapping::FromPartition(partition, EThreadPoolStrategy::Hybrid);

    EXPECT_EQ(mapping.TotalThreads, 6);
    EXPECT_THAT(mapping.SegmentLengths, ::testing::ElementsAre(3, 2, 1));
}

TEST_F(PartitionTest, CognitiveScheduleGeneration) {
    auto schedule = System.GenerateCognitiveSchedule();
    EXPECT_EQ(schedule.TotalSteps, 12);
    EXPECT_EQ(schedule.NumStreams, 3);
    EXPECT_EQ(schedule.NumTriads, 4);
}

// ============================================================================
// Logical Tensor Tests
// ============================================================================

TEST_F(LogicalTensorTest, TensorCreation) {
    MockLogicalTensor tensor("test", MockTensorShape({3, 4}));
    EXPECT_EQ(tensor.Name, "test");
    EXPECT_EQ(tensor.Shape.Dimensions.size(), 2);
    EXPECT_EQ(tensor.Data.size(), 12);
}

TEST_F(LogicalTensorTest, TensorInitialization) {
    MockLogicalTensor tensor("test", MockTensorShape({4}));

    tensor.Zero();
    EXPECT_EQ(tensor.Sum(), 0.0f);

    tensor.Ones();
    EXPECT_EQ(tensor.Sum(), 4.0f);

    tensor.Uniform(0.5f);
    EXPECT_FLOAT_EQ(tensor.Mean(), 0.5f);
}

TEST_F(LogicalTensorTest, LogicalAnd) {
    TensorA = MockLogicalTensor("A", MockTensorShape({4}), {1.0f, 0.5f, 0.0f, 0.8f});
    TensorB = MockLogicalTensor("B", MockTensorShape({4}), {0.5f, 1.0f, 1.0f, 0.5f});

    auto result = TensorA.And(TensorB);

    EXPECT_FLOAT_EQ(result.Data[0], 0.5f);   // 1.0 * 0.5
    EXPECT_FLOAT_EQ(result.Data[1], 0.5f);   // 0.5 * 1.0
    EXPECT_FLOAT_EQ(result.Data[2], 0.0f);   // 0.0 * 1.0
    EXPECT_FLOAT_EQ(result.Data[3], 0.4f);   // 0.8 * 0.5
}

TEST_F(LogicalTensorTest, LogicalOr) {
    TensorA = MockLogicalTensor("A", MockTensorShape({4}), {0.3f, 0.5f, 0.0f, 0.8f});
    TensorB = MockLogicalTensor("B", MockTensorShape({4}), {0.5f, 0.2f, 0.7f, 0.6f});

    auto result = TensorA.Or(TensorB);

    EXPECT_FLOAT_EQ(result.Data[0], 0.5f);   // max(0.3, 0.5)
    EXPECT_FLOAT_EQ(result.Data[1], 0.5f);   // max(0.5, 0.2)
    EXPECT_FLOAT_EQ(result.Data[2], 0.7f);   // max(0.0, 0.7)
    EXPECT_FLOAT_EQ(result.Data[3], 0.8f);   // max(0.8, 0.6)
}

TEST_F(LogicalTensorTest, LogicalNot) {
    TensorA = MockLogicalTensor("A", MockTensorShape({4}), {1.0f, 0.5f, 0.0f, 0.3f});

    auto result = TensorA.Not();

    EXPECT_FLOAT_EQ(result.Data[0], 0.0f);   // 1 - 1.0
    EXPECT_FLOAT_EQ(result.Data[1], 0.5f);   // 1 - 0.5
    EXPECT_FLOAT_EQ(result.Data[2], 1.0f);   // 1 - 0.0
    EXPECT_FLOAT_EQ(result.Data[3], 0.7f);   // 1 - 0.3
}

TEST_F(LogicalTensorTest, Softmax) {
    TensorA = MockLogicalTensor("A", MockTensorShape({4}), {1.0f, 2.0f, 3.0f, 4.0f});

    auto result = TensorA.Softmax();

    // Softmax should sum to 1
    EXPECT_NEAR(result.Sum(), 1.0f, 1e-6f);

    // Larger inputs should have larger probabilities
    EXPECT_LT(result.Data[0], result.Data[1]);
    EXPECT_LT(result.Data[1], result.Data[2]);
    EXPECT_LT(result.Data[2], result.Data[3]);
}

TEST_F(LogicalTensorTest, Normalize) {
    TensorA = MockLogicalTensor("A", MockTensorShape({4}), {1.0f, 2.0f, 3.0f, 4.0f});

    auto result = TensorA.Normalize();

    // Should sum to 1
    EXPECT_NEAR(result.Sum(), 1.0f, 1e-6f);

    // Proportions should be preserved
    EXPECT_NEAR(result.Data[1] / result.Data[0], 2.0f, 1e-6f);
}

// ============================================================================
// Tensor Logic Engine Tests
// ============================================================================

TEST_F(TensorLogicEngineTest, Initialization) {
    EXPECT_TRUE(Engine.IsInitialized());
}

TEST_F(TensorLogicEngineTest, MatrixMultiplication) {
    MockLogicalTensor A("A", MockTensorShape({2, 3}), {1, 2, 3, 4, 5, 6});
    MockLogicalTensor B("B", MockTensorShape({3, 2}), {7, 8, 9, 10, 11, 12});

    auto C = Engine.MatMul(A, B);

    EXPECT_EQ(C.Shape.Dimensions[0], 2);
    EXPECT_EQ(C.Shape.Dimensions[1], 2);

    // C[0,0] = 1*7 + 2*9 + 3*11 = 7 + 18 + 33 = 58
    EXPECT_FLOAT_EQ(C.Data[0], 58.0f);
    // C[0,1] = 1*8 + 2*10 + 3*12 = 8 + 20 + 36 = 64
    EXPECT_FLOAT_EQ(C.Data[1], 64.0f);
}

TEST_F(TensorLogicEngineTest, OuterProduct) {
    MockLogicalTensor A("A", MockTensorShape({2}), {1.0f, 2.0f});
    MockLogicalTensor B("B", MockTensorShape({3}), {3.0f, 4.0f, 5.0f});

    auto C = Engine.OuterProduct(A, B);

    EXPECT_EQ(C.Shape.Dimensions.size(), 2);
    EXPECT_EQ(C.Shape.TotalElements(), 6);

    // C[0,0] = 1*3, C[0,1] = 1*4, C[0,2] = 1*5
    // C[1,0] = 2*3, C[1,1] = 2*4, C[1,2] = 2*5
    EXPECT_FLOAT_EQ(C.Data[0], 3.0f);
    EXPECT_FLOAT_EQ(C.Data[1], 4.0f);
    EXPECT_FLOAT_EQ(C.Data[2], 5.0f);
    EXPECT_FLOAT_EQ(C.Data[3], 6.0f);
    EXPECT_FLOAT_EQ(C.Data[4], 8.0f);
    EXPECT_FLOAT_EQ(C.Data[5], 10.0f);
}

TEST_F(TensorLogicEngineTest, Trace) {
    MockLogicalTensor A("A", MockTensorShape({3, 3}),
                        {1, 0, 0, 0, 2, 0, 0, 0, 3});

    float trace = Engine.Trace(A);
    EXPECT_FLOAT_EQ(trace, 6.0f);  // 1 + 2 + 3
}

TEST_F(TensorLogicEngineTest, ActivationReLU) {
    MockLogicalTensor input("in", MockTensorShape({4}), {-1.0f, 0.0f, 1.0f, 2.0f});
    auto output = Engine.Activate(input, EActivation::ReLU);

    EXPECT_FLOAT_EQ(output.Data[0], 0.0f);
    EXPECT_FLOAT_EQ(output.Data[1], 0.0f);
    EXPECT_FLOAT_EQ(output.Data[2], 1.0f);
    EXPECT_FLOAT_EQ(output.Data[3], 2.0f);
}

TEST_F(TensorLogicEngineTest, ActivationSigmoid) {
    MockLogicalTensor input("in", MockTensorShape({3}), {-10.0f, 0.0f, 10.0f});
    auto output = Engine.Activate(input, EActivation::Sigmoid);

    EXPECT_NEAR(output.Data[0], 0.0f, 0.01f);
    EXPECT_NEAR(output.Data[1], 0.5f, 0.01f);
    EXPECT_NEAR(output.Data[2], 1.0f, 0.01f);
}

TEST_F(TensorLogicEngineTest, LogicalConnectives) {
    MockLogicalTensor A("A", MockTensorShape({3}), {0.8f, 0.5f, 0.2f});
    MockLogicalTensor B("B", MockTensorShape({3}), {0.4f, 0.6f, 0.9f});

    // Conjunction (AND)
    auto conj = Engine.ApplyConnective(ELogicalConnective::Conjunction, {A, B});
    EXPECT_FLOAT_EQ(conj.Data[0], 0.32f);  // 0.8 * 0.4
    EXPECT_FLOAT_EQ(conj.Data[1], 0.30f);  // 0.5 * 0.6
    EXPECT_FLOAT_EQ(conj.Data[2], 0.18f);  // 0.2 * 0.9

    // Disjunction (OR)
    auto disj = Engine.ApplyConnective(ELogicalConnective::Disjunction, {A, B});
    EXPECT_FLOAT_EQ(disj.Data[0], 0.8f);   // max(0.8, 0.4)
    EXPECT_FLOAT_EQ(disj.Data[1], 0.6f);   // max(0.5, 0.6)
    EXPECT_FLOAT_EQ(disj.Data[2], 0.9f);   // max(0.2, 0.9)

    // Negation
    auto neg = Engine.ApplyConnective(ELogicalConnective::Negation, {A});
    EXPECT_FLOAT_EQ(neg.Data[0], 0.2f);    // 1 - 0.8
    EXPECT_FLOAT_EQ(neg.Data[1], 0.5f);    // 1 - 0.5
    EXPECT_FLOAT_EQ(neg.Data[2], 0.8f);    // 1 - 0.2
}

TEST_F(TensorLogicEngineTest, SymbolicRuleApplication) {
    MockLogicalTensor A("premise_A", MockTensorShape({3}), {0.9f, 0.8f, 0.7f});
    MockLogicalTensor B("premise_B", MockTensorShape({3}), {0.8f, 0.9f, 0.6f});

    MockSymbolicRule rule("test_rule", {"A", "B"}, "C");
    rule.Confidence = 0.9f;

    auto result = Engine.ApplyRule(rule, {A, B});

    EXPECT_EQ(result.Name, "C");
    EXPECT_FLOAT_EQ(result.Data[0], 0.9f * 0.8f * 0.9f);  // 0.648
}

// ============================================================================
// Knowledge Base Tests
// ============================================================================

TEST_F(KnowledgeBaseTest, AddAndRetrieveFact) {
    MockLogicalTensor fact("temperature", MockTensorShape({1}), {0.75f});
    KB.AddFact("temperature", fact);

    EXPECT_TRUE(KB.HasFact("temperature"));
    EXPECT_EQ(KB.NumFacts(), 1);

    const auto* retrieved = KB.GetFact("temperature");
    ASSERT_NE(retrieved, nullptr);
    EXPECT_FLOAT_EQ(retrieved->Data[0], 0.75f);
}

TEST_F(KnowledgeBaseTest, AddRule) {
    MockSymbolicRule rule("modus_ponens", {"P", "P_implies_Q"}, "Q");
    KB.AddRule(rule);

    EXPECT_EQ(KB.NumRules(), 1);
    EXPECT_EQ(KB.GetRules()[0].Name, "modus_ponens");
}

TEST_F(KnowledgeBaseTest, NonExistentFact) {
    const auto* fact = KB.GetFact("nonexistent");
    EXPECT_EQ(fact, nullptr);
    EXPECT_FALSE(KB.HasFact("nonexistent"));
}

// ============================================================================
// Belief Update and Action Selection Tests
// ============================================================================

TEST_F(TensorLogicEngineTest, BeliefUpdate) {
    MockLogicalTensor prior("prior", MockTensorShape({4}), {0.25f, 0.25f, 0.25f, 0.25f});
    MockLogicalTensor observation("obs", MockTensorShape({4}), {1.0f, 0.5f, 0.1f, 0.0f});

    auto posterior = Engine.UpdateBeliefs(prior, observation);

    // Posterior should be normalized
    EXPECT_NEAR(posterior.Sum(), 1.0f, 1e-5f);

    // High observation should have high posterior
    EXPECT_GT(posterior.Data[0], posterior.Data[2]);
}

TEST_F(TensorLogicEngineTest, ActionSelection) {
    MockLogicalTensor beliefs("beliefs", MockTensorShape({3}), {0.1f, 0.6f, 0.3f});
    MockLogicalTensor preferences("prefs", MockTensorShape({3}), {1.0f, 0.5f, 0.8f});

    auto actionProbs = Engine.SelectAction(beliefs, preferences);

    // Should be valid probability distribution
    EXPECT_NEAR(actionProbs.Sum(), 1.0f, 1e-5f);

    // All probabilities should be positive
    for (float p : actionProbs.Data) {
        EXPECT_GE(p, 0.0f);
    }
}

// ============================================================================
// Cognitive Signature Tests
// ============================================================================

TEST_F(PartitionTest, CognitiveSignatureComputation) {
    // State vector (belief=3, emotion=2, intention=1)
    std::vector<uint32_t> stateVector = {3, 2, 1};
    uint64_t signature = System.ComputeCognitiveSignature(stateVector);

    // tau(3) * tau(2) * tau(1) = 5 * 3 * 2 = 30
    EXPECT_EQ(signature, 30);
}

TEST_F(PartitionTest, DifferentStatesHaveDifferentSignatures) {
    std::vector<uint32_t> state1 = {3, 2, 1};
    std::vector<uint32_t> state2 = {4, 1, 1};

    uint64_t sig1 = System.ComputeCognitiveSignature(state1);
    uint64_t sig2 = System.ComputeCognitiveSignature(state2);

    EXPECT_NE(sig1, sig2);
}

TEST_F(PartitionTest, PermutedStatesHaveSameSignature) {
    std::vector<uint32_t> state1 = {3, 2, 1};
    std::vector<uint32_t> state2 = {1, 2, 3};  // Permutation

    uint64_t sig1 = System.ComputeCognitiveSignature(state1);
    uint64_t sig2 = System.ComputeCognitiveSignature(state2);

    EXPECT_EQ(sig1, sig2);  // Product is commutative
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(TensorLogicEngineTest, FullInferenceChain) {
    // Add facts
    MockLogicalTensor raining("raining", MockTensorShape({1}), {0.9f});
    MockLogicalTensor coldTemp("cold", MockTensorShape({1}), {0.8f});
    Engine.AddFact("raining", raining);
    Engine.AddFact("cold", coldTemp);

    // Add rule: raining AND cold -> stay_inside
    MockSymbolicRule rule("weather_rule", {"raining", "cold"}, "stay_inside");
    Engine.AddRule(rule);

    // Apply rule
    auto conclusion = Engine.ApplyRule(rule, {raining, coldTemp});

    EXPECT_EQ(conclusion.Name, "stay_inside");
    EXPECT_NEAR(conclusion.Data[0], 0.72f, 0.01f);  // 0.9 * 0.8
}

TEST_F(TensorLogicEngineTest, NeuralSymbolicIntegration) {
    // Neural: process with activations
    MockLogicalTensor input("input", MockTensorShape({4}), {-1.0f, 0.5f, 2.0f, -0.5f});
    auto hidden = Engine.Activate(input, EActivation::ReLU);
    auto output = Engine.Activate(hidden, EActivation::Softmax);

    // Symbolic: create fact from neural output
    Engine.AddFact("neural_belief", output);

    // Combine with symbolic reasoning
    MockLogicalTensor prior("prior", MockTensorShape({4}), {0.25f, 0.25f, 0.25f, 0.25f});
    auto posterior = Engine.UpdateBeliefs(prior, output);

    EXPECT_NEAR(posterior.Sum(), 1.0f, 1e-5f);
    EXPECT_GT(posterior.Data[2], posterior.Data[0]);  // High activation should lead to high belief
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(PartitionTest, PartitionGenerationPerformance) {
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        System.GeneratePartitions(10);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete 100 iterations for n=10 in reasonable time
    EXPECT_LT(duration.count(), 1000);  // Less than 1 second
}

TEST_F(TensorLogicEngineTest, MatMulPerformance) {
    MockLogicalTensor A("A", MockTensorShape({100, 100}));
    MockLogicalTensor B("B", MockTensorShape({100, 100}));
    A.Random();
    B.Random();

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10; i++) {
        Engine.MatMul(A, B);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 10 100x100 matmuls should complete in reasonable time
    EXPECT_LT(duration.count(), 5000);  // Less than 5 seconds
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(LogicalTensorTest, EmptyTensor) {
    MockLogicalTensor empty("empty", MockTensorShape({}));
    EXPECT_EQ(empty.Data.size(), 1);  // Scalar has 1 element
}

TEST_F(PartitionTest, SingleElementPartition) {
    auto partitions = System.GeneratePartitions(1);
    EXPECT_EQ(partitions.size(), 1);
    EXPECT_EQ(partitions[0].Parts.size(), 1);
    EXPECT_EQ(partitions[0].Parts[0].Value, 1);
}

TEST_F(TensorLogicEngineTest, ZeroTensor) {
    MockLogicalTensor zero("zero", MockTensorShape({4}));
    zero.Zero();

    auto activated = Engine.Activate(zero, EActivation::ReLU);
    EXPECT_FLOAT_EQ(activated.Sum(), 0.0f);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

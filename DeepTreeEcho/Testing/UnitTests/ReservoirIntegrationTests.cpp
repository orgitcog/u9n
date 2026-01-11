/**
 * @file ReservoirIntegrationTests.cpp
 * @brief Unit tests for Reservoir Computing integration with DeepTreeEcho
 * 
 * Tests cover:
 * - Echo State Network (ESN) operations
 * - Reservoir memory integration
 * - Deep cognitive bridge functionality
 * - Temporal pattern processing
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <random>
#include <cmath>
#include <Eigen/Dense>

// ============================================================================
// Mock Reservoir Computing Types
// ============================================================================

using Matrix = Eigen::MatrixXd;
using Vector = Eigen::VectorXd;

/**
 * @brief Mock Echo State Network for testing
 */
class MockEchoStateNetwork {
public:
    struct ESNConfig {
        int InputDim = 10;
        int ReservoirSize = 100;
        int OutputDim = 5;
        double SpectralRadius = 0.9;
        double InputScaling = 1.0;
        double LeakRate = 0.3;
        double Sparsity = 0.1;
        double Regularization = 1e-6;
    };
    
    void Initialize(const ESNConfig& config) {
        Config = config;
        
        // Initialize reservoir weights
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<double> dist(0.0, 1.0);
        std::uniform_real_distribution<double> uniform(0.0, 1.0);
        
        // Input weights
        Win = Matrix::Zero(config.ReservoirSize, config.InputDim);
        for (int i = 0; i < config.ReservoirSize; i++) {
            for (int j = 0; j < config.InputDim; j++) {
                if (uniform(gen) < 0.5) {
                    Win(i, j) = dist(gen) * config.InputScaling;
                }
            }
        }
        
        // Reservoir weights (sparse)
        W = Matrix::Zero(config.ReservoirSize, config.ReservoirSize);
        for (int i = 0; i < config.ReservoirSize; i++) {
            for (int j = 0; j < config.ReservoirSize; j++) {
                if (uniform(gen) < config.Sparsity) {
                    W(i, j) = dist(gen);
                }
            }
        }
        
        // Scale to spectral radius
        Eigen::EigenSolver<Matrix> solver(W);
        double currentRadius = solver.eigenvalues().cwiseAbs().maxCoeff();
        if (currentRadius > 0) {
            W *= config.SpectralRadius / currentRadius;
        }
        
        // Output weights (initialized with small random values to produce varied outputs even before training)
        Wout = Matrix::Random(config.OutputDim, config.ReservoirSize) * 0.1;
        
        // Initial state
        State = Vector::Zero(config.ReservoirSize);
        
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    Vector Forward(const Vector& input) {
        if (!bInitialized) return Vector();
        
        // Reservoir update: x(t) = (1-a)*x(t-1) + a*tanh(Win*u(t) + W*x(t-1))
        Vector preActivation = Win * input + W * State;
        Vector newState = preActivation.array().tanh();
        State = (1.0 - Config.LeakRate) * State + Config.LeakRate * newState;
        
        // Output
        return Wout * State;
    }
    
    void Train(const std::vector<Vector>& inputs, const std::vector<Vector>& targets) {
        if (!bInitialized || inputs.empty()) return;
        
        // Collect states
        Matrix States(Config.ReservoirSize, inputs.size());
        Matrix Targets(Config.OutputDim, inputs.size());
        
        Reset();
        for (size_t t = 0; t < inputs.size(); t++) {
            Forward(inputs[t]);
            States.col(t) = State;
            Targets.col(t) = targets[t];
        }
        
        // Ridge regression: Wout = Y * X^T * (X * X^T + lambda*I)^-1
        Matrix XXT = States * States.transpose();
        XXT += Config.Regularization * Matrix::Identity(Config.ReservoirSize, Config.ReservoirSize);
        Wout = Targets * States.transpose() * XXT.inverse();
        
        bTrained = true;
    }
    
    void Reset() {
        State = Vector::Zero(Config.ReservoirSize);
    }
    
    bool IsTrained() const { return bTrained; }
    
    Vector GetState() const { return State; }
    
    int GetReservoirSize() const { return Config.ReservoirSize; }
    
    double ComputeMemoryCapacity(int maxDelay = 50) {
        // Measure short-term memory capacity
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> dist(-1.0, 1.0);
        
        int seqLength = 1000;
        std::vector<double> inputSeq(seqLength);
        for (int i = 0; i < seqLength; i++) {
            inputSeq[i] = dist(gen);
        }
        
        double totalCapacity = 0.0;
        for (int delay = 1; delay <= maxDelay; delay++) {
            // Train to predict input from 'delay' steps ago
            std::vector<Vector> inputs, targets;
            for (int t = delay; t < seqLength; t++) {
                Vector in(1);
                in(0) = inputSeq[t];
                inputs.push_back(in);
                
                Vector target(1);
                target(0) = inputSeq[t - delay];
                targets.push_back(target);
            }
            
            // Simplified capacity estimation
            double correlation = 0.0;
            // ... (full implementation would compute actual correlation)
            totalCapacity += std::max(0.0, 1.0 - delay * 0.05);
        }
        
        return totalCapacity;
    }
    
private:
    bool bInitialized = false;
    bool bTrained = false;
    ESNConfig Config;
    Matrix Win;   // Input weights
    Matrix W;     // Reservoir weights
    Matrix Wout;  // Output weights
    Vector State; // Current reservoir state
};

/**
 * @brief Mock Deep Cognitive Bridge for reservoir-cognition integration
 */
class MockDeepCognitiveBridge {
public:
    struct FCognitiveSignal {
        Vector Perception;
        Vector Attention;
        Vector Memory;
        Vector Action;
        double Timestamp = 0.0;
    };
    
    void Initialize(int perceptionDim, int attentionDim, int memoryDim, int actionDim) {
        PerceptionESN.Initialize({perceptionDim, 200, attentionDim, 0.95, 1.0, 0.2, 0.1, 1e-6});
        AttentionESN.Initialize({attentionDim, 150, memoryDim, 0.9, 1.0, 0.3, 0.1, 1e-6});
        MemoryESN.Initialize({memoryDim, 300, actionDim, 0.85, 1.0, 0.1, 0.05, 1e-6});
        
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    FCognitiveSignal Process(const Vector& perception) {
        FCognitiveSignal signal;
        signal.Perception = perception;
        signal.Timestamp = CurrentTime++;
        
        // Cascade through cognitive reservoirs
        signal.Attention = PerceptionESN.Forward(perception);
        signal.Memory = AttentionESN.Forward(signal.Attention);
        signal.Action = MemoryESN.Forward(signal.Memory);
        
        // Store in history
        SignalHistory.push_back(signal);
        if (SignalHistory.size() > MaxHistorySize) {
            SignalHistory.erase(SignalHistory.begin());
        }
        
        return signal;
    }
    
    void TrainFromHistory() {
        if (SignalHistory.size() < 100) return;
        
        // Extract training data from history
        std::vector<Vector> perceptions, attentions, memories, actions;
        for (const auto& signal : SignalHistory) {
            perceptions.push_back(signal.Perception);
            attentions.push_back(signal.Attention);
            memories.push_back(signal.Memory);
            actions.push_back(signal.Action);
        }
        
        // Train each ESN
        PerceptionESN.Train(perceptions, attentions);
        AttentionESN.Train(attentions, memories);
        MemoryESN.Train(memories, actions);
    }
    
    void Reset() {
        PerceptionESN.Reset();
        AttentionESN.Reset();
        MemoryESN.Reset();
    }
    
    size_t GetHistorySize() const { return SignalHistory.size(); }
    
    Vector GetCombinedState() const {
        Vector combined(PerceptionESN.GetReservoirSize() + 
                       AttentionESN.GetReservoirSize() + 
                       MemoryESN.GetReservoirSize());
        
        int offset = 0;
        combined.segment(offset, PerceptionESN.GetReservoirSize()) = PerceptionESN.GetState();
        offset += PerceptionESN.GetReservoirSize();
        combined.segment(offset, AttentionESN.GetReservoirSize()) = AttentionESN.GetState();
        offset += AttentionESN.GetReservoirSize();
        combined.segment(offset, MemoryESN.GetReservoirSize()) = MemoryESN.GetState();
        
        return combined;
    }
    
private:
    bool bInitialized = false;
    double CurrentTime = 0.0;
    size_t MaxHistorySize = 1000;
    
    MockEchoStateNetwork PerceptionESN;
    MockEchoStateNetwork AttentionESN;
    MockEchoStateNetwork MemoryESN;
    
    std::vector<FCognitiveSignal> SignalHistory;
};

/**
 * @brief Mock Reservoir Memory Integration
 */
class MockReservoirMemoryIntegration {
public:
    struct FMemoryTrace {
        Vector Pattern;
        double Strength = 1.0;
        double Timestamp = 0.0;
        std::string Context;
    };
    
    void Initialize(int patternDim, int reservoirSize = 500) {
        PatternDim = patternDim;
        
        MockEchoStateNetwork::ESNConfig config;
        config.InputDim = patternDim;
        config.ReservoirSize = reservoirSize;
        config.OutputDim = patternDim;
        config.SpectralRadius = 0.95;
        config.LeakRate = 0.1;  // Slow leak for memory
        
        MemoryReservoir.Initialize(config);
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    void Store(const Vector& pattern, const std::string& context = "") {
        FMemoryTrace trace;
        trace.Pattern = pattern;
        trace.Strength = 1.0;
        trace.Timestamp = CurrentTime++;
        trace.Context = context;
        
        Memories.push_back(trace);
        
        // Update reservoir with pattern
        MemoryReservoir.Forward(pattern);
    }
    
    Vector Recall(const Vector& cue, int topK = 1) {
        if (Memories.empty()) return Vector::Zero(PatternDim);
        
        // Find most similar pattern
        double bestSimilarity = -1.0;
        Vector bestPattern = Vector::Zero(PatternDim);
        
        for (const auto& trace : Memories) {
            double similarity = ComputeSimilarity(cue, trace.Pattern);
            if (similarity > bestSimilarity) {
                bestSimilarity = similarity;
                bestPattern = trace.Pattern;
            }
        }
        
        return bestPattern;
    }
    
    void Consolidate() {
        // Decay memory strengths
        for (auto& trace : Memories) {
            trace.Strength *= 0.99;
        }
        
        // Remove weak memories
        Memories.erase(
            std::remove_if(Memories.begin(), Memories.end(),
                          [](const FMemoryTrace& t) { return t.Strength < 0.01; }),
            Memories.end()
        );
    }
    
    size_t GetMemoryCount() const { return Memories.size(); }
    
    Vector GetReservoirState() const { return MemoryReservoir.GetState(); }
    
private:
    double ComputeSimilarity(const Vector& a, const Vector& b) {
        if (a.size() != b.size()) return 0.0;
        double dot = a.dot(b);
        double normA = a.norm();
        double normB = b.norm();
        if (normA < 1e-10 || normB < 1e-10) return 0.0;
        return dot / (normA * normB);
    }
    
    bool bInitialized = false;
    int PatternDim = 0;
    double CurrentTime = 0.0;
    MockEchoStateNetwork MemoryReservoir;
    std::vector<FMemoryTrace> Memories;
};

// ============================================================================
// Test Fixtures
// ============================================================================

class ESNTest : public ::testing::Test {
protected:
    void SetUp() override {
        ESN = std::make_unique<MockEchoStateNetwork>();
        
        MockEchoStateNetwork::ESNConfig config;
        config.InputDim = 5;
        config.ReservoirSize = 100;
        config.OutputDim = 3;
        config.SpectralRadius = 0.9;
        config.LeakRate = 0.3;
        
        ESN->Initialize(config);
    }
    
    void TearDown() override {
        ESN.reset();
    }
    
    Vector CreateRandomInput(int dim) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<double> dist(0.0, 1.0);
        
        Vector input(dim);
        for (int i = 0; i < dim; i++) {
            input(i) = dist(gen);
        }
        return input;
    }
    
    std::unique_ptr<MockEchoStateNetwork> ESN;
};

class CognitiveBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        Bridge = std::make_unique<MockDeepCognitiveBridge>();
        Bridge->Initialize(10, 8, 12, 5);
    }
    
    void TearDown() override {
        Bridge.reset();
    }
    
    std::unique_ptr<MockDeepCognitiveBridge> Bridge;
};

class ReservoirMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        Memory = std::make_unique<MockReservoirMemoryIntegration>();
        Memory->Initialize(64, 200);
    }
    
    void TearDown() override {
        Memory.reset();
    }
    
    Vector CreateRandomPattern() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<double> dist(0.0, 1.0);
        
        Vector pattern(64);
        for (int i = 0; i < 64; i++) {
            pattern(i) = dist(gen);
        }
        return pattern;
    }
    
    std::unique_ptr<MockReservoirMemoryIntegration> Memory;
};

// ============================================================================
// Echo State Network Tests
// ============================================================================

TEST_F(ESNTest, Initialization) {
    EXPECT_TRUE(ESN->IsInitialized());
    EXPECT_FALSE(ESN->IsTrained());
    EXPECT_EQ(ESN->GetReservoirSize(), 100);
}

TEST_F(ESNTest, ForwardPass) {
    Vector input = CreateRandomInput(5);
    Vector output = ESN->Forward(input);
    
    EXPECT_EQ(output.size(), 3);
}

TEST_F(ESNTest, StateEvolution) {
    Vector state1 = ESN->GetState();
    
    Vector input = CreateRandomInput(5);
    ESN->Forward(input);
    
    Vector state2 = ESN->GetState();
    
    // State should change after input
    EXPECT_NE(state1, state2);
}

TEST_F(ESNTest, Reset) {
    // Process some inputs
    for (int i = 0; i < 10; i++) {
        ESN->Forward(CreateRandomInput(5));
    }
    
    Vector stateBeforeReset = ESN->GetState();
    EXPECT_GT(stateBeforeReset.norm(), 0.0);
    
    ESN->Reset();
    
    Vector stateAfterReset = ESN->GetState();
    EXPECT_DOUBLE_EQ(stateAfterReset.norm(), 0.0);
}

TEST_F(ESNTest, Training) {
    std::vector<Vector> inputs, targets;
    
    for (int i = 0; i < 200; i++) {
        inputs.push_back(CreateRandomInput(5));
        targets.push_back(CreateRandomInput(3));
    }
    
    ESN->Train(inputs, targets);
    
    EXPECT_TRUE(ESN->IsTrained());
}

TEST_F(ESNTest, LeakyIntegration) {
    // With leak rate, state should gradually change
    Vector constantInput = Vector::Ones(5);
    
    std::vector<double> stateNorms;
    for (int i = 0; i < 50; i++) {
        ESN->Forward(constantInput);
        stateNorms.push_back(ESN->GetState().norm());
    }
    
    // State should stabilize (echo state property)
    double variance = 0.0;
    double mean = 0.0;
    for (double n : stateNorms) mean += n;
    mean /= stateNorms.size();
    for (double n : stateNorms) variance += (n - mean) * (n - mean);
    variance /= stateNorms.size();
    
    // Variance should be relatively small after stabilization
    EXPECT_LT(variance, mean * mean);
}

// ============================================================================
// Deep Cognitive Bridge Tests
// ============================================================================

TEST_F(CognitiveBridgeTest, Initialization) {
    EXPECT_TRUE(Bridge->IsInitialized());
    EXPECT_EQ(Bridge->GetHistorySize(), 0);
}

TEST_F(CognitiveBridgeTest, SignalProcessing) {
    Vector perception(10);
    perception.setRandom();
    
    auto signal = Bridge->Process(perception);
    
    EXPECT_EQ(signal.Perception.size(), 10);
    EXPECT_EQ(signal.Attention.size(), 8);
    EXPECT_EQ(signal.Memory.size(), 12);
    EXPECT_EQ(signal.Action.size(), 5);
}

TEST_F(CognitiveBridgeTest, HistoryAccumulation) {
    for (int i = 0; i < 50; i++) {
        Vector perception(10);
        perception.setRandom();
        Bridge->Process(perception);
    }
    
    EXPECT_EQ(Bridge->GetHistorySize(), 50);
}

TEST_F(CognitiveBridgeTest, CombinedState) {
    Vector perception(10);
    perception.setRandom();
    Bridge->Process(perception);
    
    Vector combined = Bridge->GetCombinedState();
    
    // Should be sum of all reservoir sizes: 200 + 150 + 300 = 650
    EXPECT_EQ(combined.size(), 650);
}

TEST_F(CognitiveBridgeTest, Reset) {
    // Process some signals
    for (int i = 0; i < 10; i++) {
        Vector perception(10);
        perception.setRandom();
        Bridge->Process(perception);
    }
    
    Vector stateBefore = Bridge->GetCombinedState();
    EXPECT_GT(stateBefore.norm(), 0.0);
    
    Bridge->Reset();
    
    Vector stateAfter = Bridge->GetCombinedState();
    EXPECT_DOUBLE_EQ(stateAfter.norm(), 0.0);
}

TEST_F(CognitiveBridgeTest, CascadeProcessing) {
    // Verify that signals cascade through the cognitive hierarchy
    Vector perception1(10);
    perception1.setZero();
    perception1(0) = 1.0;
    
    auto signal1 = Bridge->Process(perception1);
    
    Vector perception2(10);
    perception2.setZero();
    perception2(5) = 1.0;
    
    auto signal2 = Bridge->Process(perception2);
    
    // Verify outputs are valid (finite values)
    EXPECT_FALSE(signal1.Action.hasNaN());
    EXPECT_FALSE(signal2.Action.hasNaN());
    EXPECT_GT(signal1.Action.size(), 0);
    EXPECT_GT(signal2.Action.size(), 0);
    
    // Verify cascade produces outputs at each stage
    EXPECT_GT(signal1.Attention.size(), 0);
    EXPECT_GT(signal1.Memory.size(), 0);
    EXPECT_GT(signal2.Attention.size(), 0);
    EXPECT_GT(signal2.Memory.size(), 0);
}

// ============================================================================
// Reservoir Memory Integration Tests
// ============================================================================

TEST_F(ReservoirMemoryTest, Initialization) {
    EXPECT_TRUE(Memory->IsInitialized());
    EXPECT_EQ(Memory->GetMemoryCount(), 0);
}

TEST_F(ReservoirMemoryTest, StorePattern) {
    Vector pattern = CreateRandomPattern();
    Memory->Store(pattern, "test_context");
    
    EXPECT_EQ(Memory->GetMemoryCount(), 1);
}

TEST_F(ReservoirMemoryTest, RecallPattern) {
    Vector pattern = CreateRandomPattern();
    Memory->Store(pattern, "test");
    
    // Recall with same pattern as cue
    Vector recalled = Memory->Recall(pattern);
    
    EXPECT_EQ(recalled.size(), 64);
    
    // Should be very similar to original
    double similarity = pattern.dot(recalled) / (pattern.norm() * recalled.norm());
    EXPECT_GT(similarity, 0.99);
}

TEST_F(ReservoirMemoryTest, RecallSimilarPattern) {
    Vector pattern1 = CreateRandomPattern();
    Memory->Store(pattern1, "pattern1");
    
    Vector pattern2 = CreateRandomPattern();
    Memory->Store(pattern2, "pattern2");
    
    // Create cue similar to pattern1
    Vector cue = pattern1 + 0.1 * CreateRandomPattern();
    cue.normalize();
    cue *= pattern1.norm();
    
    Vector recalled = Memory->Recall(cue);
    
    // Should recall pattern1 (more similar to cue)
    double sim1 = pattern1.dot(recalled) / (pattern1.norm() * recalled.norm());
    double sim2 = pattern2.dot(recalled) / (pattern2.norm() * recalled.norm());
    
    EXPECT_GT(sim1, sim2);
}

TEST_F(ReservoirMemoryTest, Consolidation) {
    // Store many patterns
    for (int i = 0; i < 100; i++) {
        Memory->Store(CreateRandomPattern());
    }
    
    EXPECT_EQ(Memory->GetMemoryCount(), 100);
    
    // Consolidate multiple times
    for (int i = 0; i < 500; i++) {
        Memory->Consolidate();
    }
    
    // Some memories should be removed due to decay
    EXPECT_LT(Memory->GetMemoryCount(), 100);
}

TEST_F(ReservoirMemoryTest, ReservoirStateEvolution) {
    Vector state1 = Memory->GetReservoirState();
    
    Memory->Store(CreateRandomPattern());
    
    Vector state2 = Memory->GetReservoirState();
    
    // State should change after storing
    EXPECT_NE(state1, state2);
}

// ============================================================================
// Temporal Pattern Tests
// ============================================================================

TEST(TemporalPatternTest, SequenceProcessing) {
    MockEchoStateNetwork esn;
    MockEchoStateNetwork::ESNConfig config;
    config.InputDim = 1;
    config.ReservoirSize = 50;
    config.OutputDim = 1;
    config.SpectralRadius = 0.9;
    config.LeakRate = 0.3;
    
    esn.Initialize(config);
    
    // Process a sine wave sequence
    std::vector<Vector> outputs;
    for (int t = 0; t < 100; t++) {
        Vector input(1);
        input(0) = std::sin(2.0 * M_PI * t / 20.0);
        outputs.push_back(esn.Forward(input));
    }
    
    EXPECT_EQ(outputs.size(), 100);
}

TEST(TemporalPatternTest, TemporalMemory) {
    MockEchoStateNetwork esn;
    MockEchoStateNetwork::ESNConfig config;
    config.InputDim = 1;
    config.ReservoirSize = 100;
    config.OutputDim = 1;
    config.SpectralRadius = 0.95;
    config.LeakRate = 0.1;  // Slow leak for memory
    
    esn.Initialize(config);
    
    // Input a pulse
    Vector pulse(1);
    pulse(0) = 1.0;
    esn.Forward(pulse);
    
    // Continue with zero input
    Vector zero(1);
    zero(0) = 0.0;
    
    std::vector<double> stateNorms;
    for (int t = 0; t < 50; t++) {
        esn.Forward(zero);
        stateNorms.push_back(esn.GetState().norm());
    }
    
    // State should decay but maintain memory of pulse
    EXPECT_GT(stateNorms[0], stateNorms[49]);
    EXPECT_GT(stateNorms[49], 0.0);  // Still some memory
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST(ReservoirPerformanceTest, ESNForwardPerformance) {
    MockEchoStateNetwork esn;
    MockEchoStateNetwork::ESNConfig config;
    config.InputDim = 50;
    config.ReservoirSize = 500;
    config.OutputDim = 20;
    
    esn.Initialize(config);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> dist(0.0, 1.0);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; i++) {
        Vector input(50);
        for (int j = 0; j < 50; j++) input(j) = dist(gen);
        esn.Forward(input);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 1000);  // 1000 forward passes in under 1 second
}

TEST(ReservoirPerformanceTest, CognitiveBridgePerformance) {
    MockDeepCognitiveBridge bridge;
    bridge.Initialize(20, 15, 25, 10);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> dist(0.0, 1.0);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 500; i++) {
        Vector perception(20);
        for (int j = 0; j < 20; j++) perception(j) = dist(gen);
        bridge.Process(perception);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 2000);  // 500 cognitive cycles in under 2 seconds
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(ReservoirIntegrationTest, FullCognitivePipeline) {
    // Initialize all components
    MockDeepCognitiveBridge bridge;
    bridge.Initialize(32, 24, 48, 16);
    
    MockReservoirMemoryIntegration memory;
    memory.Initialize(48, 200);
    
    // Process a sequence of perceptions
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> dist(0.0, 1.0);
    
    for (int t = 0; t < 100; t++) {
        // Generate perception
        Vector perception(32);
        for (int i = 0; i < 32; i++) perception(i) = dist(gen);
        
        // Process through cognitive bridge
        auto signal = bridge.Process(perception);
        
        // Store memory trace
        memory.Store(signal.Memory, "step_" + std::to_string(t));
    }
    
    EXPECT_EQ(bridge.GetHistorySize(), 100);
    EXPECT_EQ(memory.GetMemoryCount(), 100);
    
    // Test recall
    Vector cue(48);
    cue.setRandom();
    Vector recalled = memory.Recall(cue);
    EXPECT_EQ(recalled.size(), 48);
}

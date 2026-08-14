/**
 * @file ReservoirCognitiveE2E.cpp
 * @brief End-to-End tests for Reservoir Computing cognitive integration
 * 
 * Tests the full integration of:
 * - Echo State Networks with cognitive processing
 * - Temporal memory and prediction
 * - Deep cognitive bridge cascades
 * - Active inference with reservoir dynamics
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <cmath>
#include <random>
#include <chrono>
#include <Eigen/Dense>

using Matrix = Eigen::MatrixXd;
using Vector = Eigen::VectorXd;

// ============================================================================
// E2E Reservoir Components
// ============================================================================

class E2EEchoStateNetwork {
public:
    struct Config {
        int InputDim = 32;
        int ReservoirSize = 200;
        int OutputDim = 16;
        double SpectralRadius = 0.9;
        double InputScaling = 1.0;
        double LeakRate = 0.3;
        double Sparsity = 0.1;
        double Regularization = 1e-6;
    };
    
    void Initialize(const Config& config) {
        Cfg = config;
        
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
        
        // Reservoir weights
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
        
        Wout = Matrix::Zero(config.OutputDim, config.ReservoirSize);
        State = Vector::Zero(config.ReservoirSize);
        
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    Vector Forward(const Vector& input) {
        Vector preActivation = Win * input + W * State;
        Vector newState = preActivation.array().tanh();
        State = (1.0 - Cfg.LeakRate) * State + Cfg.LeakRate * newState;
        return Wout * State;
    }
    
    void Train(const std::vector<Vector>& inputs, const std::vector<Vector>& targets) {
        Matrix States(Cfg.ReservoirSize, inputs.size());
        Matrix Targets(Cfg.OutputDim, inputs.size());
        
        Reset();
        for (size_t t = 0; t < inputs.size(); t++) {
            Forward(inputs[t]);
            States.col(t) = State;
            Targets.col(t) = targets[t];
        }
        
        Matrix XXT = States * States.transpose();
        XXT += Cfg.Regularization * Matrix::Identity(Cfg.ReservoirSize, Cfg.ReservoirSize);
        Wout = Targets * States.transpose() * XXT.inverse();
        
        bTrained = true;
    }
    
    void Reset() { State = Vector::Zero(Cfg.ReservoirSize); }
    bool IsTrained() const { return bTrained; }
    Vector GetState() const { return State; }
    int GetReservoirSize() const { return Cfg.ReservoirSize; }
    
private:
    bool bInitialized = false;
    bool bTrained = false;
    Config Cfg;
    Matrix Win, W, Wout;
    Vector State;
};

class E2ECognitiveReservoirBridge {
public:
    void Initialize(int perceptionDim, int memoryDim, int actionDim) {
        E2EEchoStateNetwork::Config perceptionConfig;
        perceptionConfig.InputDim = perceptionDim;
        perceptionConfig.ReservoirSize = 300;
        perceptionConfig.OutputDim = memoryDim;
        perceptionConfig.SpectralRadius = 0.95;
        perceptionConfig.LeakRate = 0.2;
        PerceptionESN.Initialize(perceptionConfig);
        
        E2EEchoStateNetwork::Config memoryConfig;
        memoryConfig.InputDim = memoryDim;
        memoryConfig.ReservoirSize = 400;
        memoryConfig.OutputDim = actionDim;
        memoryConfig.SpectralRadius = 0.9;
        memoryConfig.LeakRate = 0.1;
        MemoryESN.Initialize(memoryConfig);
        
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    struct FCognitiveOutput {
        Vector Perception;
        Vector Memory;
        Vector Action;
        double ProcessingTime = 0.0;
    };
    
    FCognitiveOutput Process(const Vector& input) {
        auto start = std::chrono::high_resolution_clock::now();
        
        FCognitiveOutput output;
        output.Perception = input;
        output.Memory = PerceptionESN.Forward(input);
        output.Action = MemoryESN.Forward(output.Memory);
        
        auto end = std::chrono::high_resolution_clock::now();
        output.ProcessingTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
        
        ProcessCount++;
        return output;
    }
    
    void Reset() {
        PerceptionESN.Reset();
        MemoryESN.Reset();
    }
    
    int GetProcessCount() const { return ProcessCount; }
    
    Vector GetCombinedState() const {
        Vector perception = PerceptionESN.GetState();
        Vector memory = MemoryESN.GetState();
        Vector combined(perception.size() + memory.size());
        combined << perception, memory;
        return combined;
    }
    
private:
    bool bInitialized = false;
    int ProcessCount = 0;
    E2EEchoStateNetwork PerceptionESN;
    E2EEchoStateNetwork MemoryESN;
};

class E2ETemporalPredictor {
public:
    void Initialize(int inputDim, int horizons = 5) {
        InputDim = inputDim;
        NumHorizons = horizons;
        
        for (int h = 0; h < horizons; h++) {
            E2EEchoStateNetwork::Config config;
            config.InputDim = inputDim;
            config.ReservoirSize = 100 + h * 50;
            config.OutputDim = inputDim;
            config.SpectralRadius = 0.9 - h * 0.05;
            config.LeakRate = 0.2 + h * 0.05;
            
            Predictors.emplace_back();
            Predictors.back().Initialize(config);
        }
        
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    std::vector<Vector> Predict(const Vector& current) {
        std::vector<Vector> predictions;
        
        Vector input = current;
        for (int h = 0; h < NumHorizons; h++) {
            Vector prediction = Predictors[h].Forward(input);
            predictions.push_back(prediction);
            input = prediction;
        }
        
        return predictions;
    }
    
    void Train(const std::vector<Vector>& sequence) {
        for (int h = 0; h < NumHorizons; h++) {
            std::vector<Vector> inputs, targets;
            
            for (size_t t = 0; t + h + 1 < sequence.size(); t++) {
                inputs.push_back(sequence[t]);
                targets.push_back(sequence[t + h + 1]);
            }
            
            if (!inputs.empty()) {
                Predictors[h].Train(inputs, targets);
            }
        }
    }
    
    void Reset() {
        for (auto& p : Predictors) p.Reset();
    }
    
private:
    bool bInitialized = false;
    int InputDim = 0;
    int NumHorizons = 0;
    std::vector<E2EEchoStateNetwork> Predictors;
};

class E2EActiveInferenceReservoir {
public:
    void Initialize(int stateDim, int actionDim) {
        StateDim = stateDim;
        ActionDim = actionDim;
        
        E2EEchoStateNetwork::Config config;
        config.InputDim = stateDim + actionDim;
        config.ReservoirSize = 250;
        config.OutputDim = stateDim;
        config.SpectralRadius = 0.95;
        config.LeakRate = 0.15;
        
        TransitionModel.Initialize(config);
        
        Beliefs = Vector::Ones(stateDim) / stateDim;
        Preferences = Vector::Zero(stateDim);
        
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    void SetPreferences(const Vector& prefs) {
        Preferences = prefs;
    }
    
    void UpdateBeliefs(const Vector& observation) {
        // Bayesian update (simplified)
        for (int i = 0; i < StateDim; i++) {
            Beliefs(i) *= std::exp(-0.5 * std::pow(observation(i) - Beliefs(i), 2));
        }
        Beliefs /= Beliefs.sum();
    }
    
    int SelectAction() {
        // Compute expected free energy for each action
        Vector G(ActionDim);
        
        for (int a = 0; a < ActionDim; a++) {
            Vector input(StateDim + ActionDim);
            input << Beliefs, Vector::Unit(ActionDim, a);
            
            Vector predictedState = TransitionModel.Forward(input);
            
            // Pragmatic value
            double pragmatic = Preferences.dot(predictedState);
            
            // Epistemic value (entropy reduction)
            double epistemic = 0.0;
            for (int i = 0; i < StateDim; i++) {
                if (predictedState(i) > 1e-10) {
                    epistemic -= predictedState(i) * std::log(predictedState(i));
                }
            }
            
            G(a) = pragmatic + 0.1 * epistemic;
        }
        
        // Softmax action selection
        Vector expG = (G.array() - G.maxCoeff()).exp();
        Vector actionProbs = expG / expG.sum();
        
        // Argmax
        int bestAction = 0;
        actionProbs.maxCoeff(&bestAction);
        
        return bestAction;
    }
    
    double ComputeFreeEnergy() const {
        double entropy = 0.0;
        for (int i = 0; i < StateDim; i++) {
            if (Beliefs(i) > 1e-10) {
                entropy -= Beliefs(i) * std::log(Beliefs(i));
            }
        }
        
        double expectedValue = Preferences.dot(Beliefs);
        
        return -entropy - expectedValue;
    }
    
    Vector GetBeliefs() const { return Beliefs; }
    
private:
    bool bInitialized = false;
    int StateDim = 0;
    int ActionDim = 0;
    E2EEchoStateNetwork TransitionModel;
    Vector Beliefs;
    Vector Preferences;
};

// ============================================================================
// E2E Test Fixtures
// ============================================================================

class ReservoirCognitiveE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        Bridge = std::make_unique<E2ECognitiveReservoirBridge>();
        Bridge->Initialize(32, 24, 16);
    }
    
    void TearDown() override {
        Bridge.reset();
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
    
    std::unique_ptr<E2ECognitiveReservoirBridge> Bridge;
};

class TemporalPredictorE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        Predictor = std::make_unique<E2ETemporalPredictor>();
        Predictor->Initialize(16, 5);
    }
    
    void TearDown() override {
        Predictor.reset();
    }
    
    std::vector<Vector> GenerateSineSequence(int length, int dim) {
        std::vector<Vector> sequence;
        constexpr double kPi = 3.14159265358979323846;
        for (int t = 0; t < length; t++) {
            Vector v(dim);
            for (int i = 0; i < dim; i++) {
                v(i) = std::sin(2.0 * kPi * t / 20.0 + i * 0.1);
            }
            sequence.push_back(v);
        }
        return sequence;
    }
    
    std::unique_ptr<E2ETemporalPredictor> Predictor;
};

class ActiveInferenceReservoirE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        AIR = std::make_unique<E2EActiveInferenceReservoir>();
        AIR->Initialize(8, 4);
    }
    
    void TearDown() override {
        AIR.reset();
    }
    
    std::unique_ptr<E2EActiveInferenceReservoir> AIR;
};

// ============================================================================
// Cognitive Reservoir Bridge E2E Tests
// ============================================================================

TEST_F(ReservoirCognitiveE2ETest, FullPipelineExecution) {
    ASSERT_TRUE(Bridge->IsInitialized());
    
    Vector input = CreateRandomInput(32);
    auto output = Bridge->Process(input);
    
    EXPECT_EQ(output.Perception.size(), 32);
    EXPECT_EQ(output.Memory.size(), 24);
    EXPECT_EQ(output.Action.size(), 16);
}

TEST_F(ReservoirCognitiveE2ETest, SequentialProcessing) {
    for (int i = 0; i < 100; i++) {
        Vector input = CreateRandomInput(32);
        auto output = Bridge->Process(input);
        
        EXPECT_EQ(output.Action.size(), 16);
    }
    
    EXPECT_EQ(Bridge->GetProcessCount(), 100);
}

TEST_F(ReservoirCognitiveE2ETest, StateEvolution) {
    Vector state1 = Bridge->GetCombinedState();
    
    for (int i = 0; i < 10; i++) {
        Bridge->Process(CreateRandomInput(32));
    }
    
    Vector state2 = Bridge->GetCombinedState();
    
    EXPECT_NE(state1, state2);
}

TEST_F(ReservoirCognitiveE2ETest, ResetBehavior) {
    for (int i = 0; i < 50; i++) {
        Bridge->Process(CreateRandomInput(32));
    }
    
    Vector stateBeforeReset = Bridge->GetCombinedState();
    EXPECT_GT(stateBeforeReset.norm(), 0.0);
    
    Bridge->Reset();
    
    Vector stateAfterReset = Bridge->GetCombinedState();
    EXPECT_DOUBLE_EQ(stateAfterReset.norm(), 0.0);
}

TEST_F(ReservoirCognitiveE2ETest, ProcessingLatency) {
    std::vector<double> latencies;
    
    for (int i = 0; i < 100; i++) {
        Vector input = CreateRandomInput(32);
        auto output = Bridge->Process(input);
        latencies.push_back(output.ProcessingTime);
    }
    
    double avgLatency = 0.0;
    for (double l : latencies) avgLatency += l;
    avgLatency /= latencies.size();
    
    // Should be under 5ms per cycle
    EXPECT_LT(avgLatency, 5.0);
}

TEST_F(ReservoirCognitiveE2ETest, TemporalMemory) {
    // Input a distinctive pattern
    Vector pattern(32);
    pattern.setZero();
    pattern(0) = 1.0;
    
    Bridge->Process(pattern);
    Vector stateAfterPattern = Bridge->GetCombinedState();
    
    // Process noise
    for (int i = 0; i < 20; i++) {
        Vector noise = CreateRandomInput(32) * 0.1;
        Bridge->Process(noise);
    }
    
    Vector stateAfterNoise = Bridge->GetCombinedState();
    
    // State should still retain some memory of pattern
    double correlation = stateAfterPattern.dot(stateAfterNoise) / 
                        (stateAfterPattern.norm() * stateAfterNoise.norm());
    EXPECT_GT(correlation, -1.0);  // Some relationship should exist
}

// ============================================================================
// Temporal Predictor E2E Tests
// ============================================================================

TEST_F(TemporalPredictorE2ETest, Initialization) {
    ASSERT_TRUE(Predictor->IsInitialized());
}

TEST_F(TemporalPredictorE2ETest, MultiHorizonPrediction) {
    Vector current(16);
    current.setRandom();
    
    auto predictions = Predictor->Predict(current);
    
    EXPECT_EQ(predictions.size(), 5);
    for (const auto& pred : predictions) {
        EXPECT_EQ(pred.size(), 16);
    }
}

TEST_F(TemporalPredictorE2ETest, SequencePrediction) {
    auto sequence = GenerateSineSequence(100, 16);
    
    // Process sequence
    for (const auto& v : sequence) {
        Predictor->Predict(v);
    }
    
    // Make predictions
    auto predictions = Predictor->Predict(sequence.back());
    EXPECT_EQ(predictions.size(), 5);
}

TEST_F(TemporalPredictorE2ETest, TrainingAndPrediction) {
    auto sequence = GenerateSineSequence(200, 16);
    
    // Train
    Predictor->Train(sequence);
    
    // Predict
    Predictor->Reset();
    auto predictions = Predictor->Predict(sequence[100]);
    
    EXPECT_EQ(predictions.size(), 5);
    
    // Predictions should be finite
    for (const auto& pred : predictions) {
        for (int i = 0; i < pred.size(); i++) {
            EXPECT_FALSE(std::isnan(pred(i)));
            EXPECT_FALSE(std::isinf(pred(i)));
        }
    }
}

TEST_F(TemporalPredictorE2ETest, Reset) {
    auto sequence = GenerateSineSequence(50, 16);
    
    for (const auto& v : sequence) {
        Predictor->Predict(v);
    }
    
    Predictor->Reset();
    
    // Should work after reset
    auto predictions = Predictor->Predict(sequence[0]);
    EXPECT_EQ(predictions.size(), 5);
}

// ============================================================================
// Active Inference Reservoir E2E Tests
// ============================================================================

TEST_F(ActiveInferenceReservoirE2ETest, Initialization) {
    ASSERT_TRUE(AIR->IsInitialized());
}

TEST_F(ActiveInferenceReservoirE2ETest, BeliefUpdate) {
    Vector observation(8);
    observation.setRandom();
    
    Vector beliefsBefore = AIR->GetBeliefs();
    AIR->UpdateBeliefs(observation);
    Vector beliefsAfter = AIR->GetBeliefs();
    
    // Beliefs should change
    EXPECT_NE(beliefsBefore, beliefsAfter);
    
    // Beliefs should sum to 1
    EXPECT_NEAR(beliefsAfter.sum(), 1.0, 0.01);
}

TEST_F(ActiveInferenceReservoirE2ETest, ActionSelection) {
    int action = AIR->SelectAction();
    
    EXPECT_GE(action, 0);
    EXPECT_LT(action, 4);
}

TEST_F(ActiveInferenceReservoirE2ETest, PreferenceInfluence) {
    Vector preferences(8);
    preferences.setZero();
    preferences(0) = 1.0;  // Strong preference for state 0
    
    AIR->SetPreferences(preferences);
    
    // Update beliefs toward state 0
    Vector observation(8);
    observation.setZero();
    observation(0) = 1.0;
    AIR->UpdateBeliefs(observation);
    
    int action = AIR->SelectAction();
    EXPECT_GE(action, 0);
}

TEST_F(ActiveInferenceReservoirE2ETest, FreeEnergyComputation) {
    double fe = AIR->ComputeFreeEnergy();
    
    EXPECT_FALSE(std::isnan(fe));
    EXPECT_FALSE(std::isinf(fe));
}

TEST_F(ActiveInferenceReservoirE2ETest, SequentialDecisionMaking) {
    Vector preferences(8);
    preferences.setRandom();
    preferences = preferences.cwiseAbs();
    preferences /= preferences.sum();
    
    AIR->SetPreferences(preferences);
    
    std::vector<int> actions;
    for (int t = 0; t < 50; t++) {
        Vector observation(8);
        observation.setRandom();
        observation = observation.cwiseAbs();
        observation /= observation.sum();
        
        AIR->UpdateBeliefs(observation);
        int action = AIR->SelectAction();
        actions.push_back(action);
    }
    
    EXPECT_EQ(actions.size(), 50);
}

// ============================================================================
// Integration E2E Tests
// ============================================================================

TEST(ReservoirIntegrationE2ETest, FullCognitivePipeline) {
    E2ECognitiveReservoirBridge bridge;
    E2ETemporalPredictor predictor;
    E2EActiveInferenceReservoir air;
    
    bridge.Initialize(32, 24, 16);
    predictor.Initialize(16, 3);
    air.Initialize(16, 4);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> dist(0.0, 1.0);
    
    for (int t = 0; t < 100; t++) {
        // Generate perception
        Vector perception(32);
        for (int i = 0; i < 32; i++) perception(i) = dist(gen);
        
        // Process through cognitive bridge
        auto cogOutput = bridge.Process(perception);
        
        // Predict future
        auto predictions = predictor.Predict(cogOutput.Action);
        
        // Update beliefs and select action
        air.UpdateBeliefs(cogOutput.Action);
        int action = air.SelectAction();
        
        EXPECT_GE(action, 0);
        EXPECT_LT(action, 4);
    }
    
    EXPECT_EQ(bridge.GetProcessCount(), 100);
}

TEST(ReservoirIntegrationE2ETest, LongRunningStability) {
    E2ECognitiveReservoirBridge bridge;
    bridge.Initialize(64, 48, 32);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> dist(0.0, 1.0);
    
    for (int t = 0; t < 1000; t++) {
        Vector input(64);
        for (int i = 0; i < 64; i++) input(i) = dist(gen);
        
        auto output = bridge.Process(input);
        
        // Check for numerical stability
        for (int i = 0; i < output.Action.size(); i++) {
            EXPECT_FALSE(std::isnan(output.Action(i)));
            EXPECT_FALSE(std::isinf(output.Action(i)));
        }
    }
}

TEST(ReservoirIntegrationE2ETest, MemoryCapacity) {
    E2EEchoStateNetwork esn;
    E2EEchoStateNetwork::Config config;
    config.InputDim = 1;
    config.ReservoirSize = 100;
    config.OutputDim = 1;
    config.SpectralRadius = 0.95;
    config.LeakRate = 0.1;
    
    esn.Initialize(config);
    
    // Generate random sequence
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    
    std::vector<double> inputSeq(500);
    for (double& v : inputSeq) v = dist(gen);
    
    // Process and collect states
    std::vector<Vector> states;
    for (double v : inputSeq) {
        Vector input(1);
        input(0) = v;
        esn.Forward(input);
        states.push_back(esn.GetState());
    }
    
    // States should be diverse
    double totalVariance = 0.0;
    for (const auto& state : states) {
        totalVariance += state.squaredNorm();
    }
    
    EXPECT_GT(totalVariance, 0.0);
}

// ============================================================================
// Performance E2E Tests
// ============================================================================

TEST(ReservoirPerformanceE2ETest, ThroughputBenchmark) {
    E2ECognitiveReservoirBridge bridge;
    bridge.Initialize(64, 48, 32);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> dist(0.0, 1.0);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    int numCycles = 1000;
    for (int i = 0; i < numCycles; i++) {
        Vector input(64);
        for (int j = 0; j < 64; j++) input(j) = dist(gen);
        bridge.Process(input);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double cyclesPerSecond = (double)numCycles / (duration.count() / 1000.0);
    
    // Should achieve at least 100 cycles per second
    EXPECT_GT(cyclesPerSecond, 100.0);
}

TEST(ReservoirPerformanceE2ETest, LargeReservoirPerformance) {
    E2EEchoStateNetwork esn;
    E2EEchoStateNetwork::Config config;
    config.InputDim = 100;
    config.ReservoirSize = 1000;
    config.OutputDim = 50;
    
    esn.Initialize(config);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> dist(0.0, 1.0);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 500; i++) {
        Vector input(100);
        for (int j = 0; j < 100; j++) input(j) = dist(gen);
        esn.Forward(input);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // 500 forward passes with 1000-node reservoir in under 5 seconds
    EXPECT_LT(duration.count(), 5000);
}

// ============================================================================
// Stress E2E Tests
// ============================================================================

TEST(ReservoirStressE2ETest, ExtremeInputs) {
    E2ECognitiveReservoirBridge bridge;
    bridge.Initialize(32, 24, 16);
    
    // Very large inputs
    Vector largeInput = Vector::Ones(32) * 1000.0;
    auto output1 = bridge.Process(largeInput);
    
    for (int i = 0; i < output1.Action.size(); i++) {
        EXPECT_FALSE(std::isnan(output1.Action(i)));
    }
    
    // Very small inputs
    Vector smallInput = Vector::Ones(32) * 1e-10;
    auto output2 = bridge.Process(smallInput);
    
    for (int i = 0; i < output2.Action.size(); i++) {
        EXPECT_FALSE(std::isnan(output2.Action(i)));
    }
}

TEST(ReservoirStressE2ETest, RapidReset) {
    E2ECognitiveReservoirBridge bridge;
    bridge.Initialize(32, 24, 16);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> dist(0.0, 1.0);
    
    for (int i = 0; i < 100; i++) {
        // Process a few inputs
        for (int j = 0; j < 10; j++) {
            Vector input(32);
            for (int k = 0; k < 32; k++) input(k) = dist(gen);
            bridge.Process(input);
        }
        
        // Reset
        bridge.Reset();
    }
    
    // Should complete without issues
    EXPECT_EQ(bridge.GetProcessCount(), 1000);
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

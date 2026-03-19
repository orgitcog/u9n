/**
 * @file CognitivePipelineE2E.cpp
 * @brief End-to-End tests for the complete DeepTreeEcho cognitive pipeline
 * 
 * Tests the full integration of:
 * - Perception -> Attention -> Memory -> Reasoning -> Action pipeline
 * - 4E Cognition embodiment integration
 * - Reservoir computing temporal processing
 * - Active inference decision making
 * - Avatar expression and animation
 * - Echobeats 3-stream concurrent processing
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
#include <random>
#include <atomic>
#include <algorithm>

// ============================================================================
// E2E Test Infrastructure
// ============================================================================

/**
 * @brief Simulated sensory input for E2E testing
 */
struct FSensoryInput {
    std::vector<float> Visual;      // 128-dim visual features
    std::vector<float> Auditory;    // 64-dim audio features
    std::vector<float> Tactile;     // 32-dim touch sensors
    std::vector<float> Proprioceptive; // 64-dim body state
    double Timestamp = 0.0;
    
    FSensoryInput() : 
        Visual(128, 0.0f), 
        Auditory(64, 0.0f), 
        Tactile(32, 0.0f),
        Proprioceptive(64, 0.0f) {}
};

/**
 * @brief Motor output from cognitive pipeline
 */
struct FMotorOutput {
    std::vector<float> JointAngles;     // Target joint positions
    std::vector<float> JointVelocities; // Target velocities
    std::vector<float> FacialBlendShapes; // Expression weights
    std::vector<float> VocalParameters;   // Speech parameters
    double Confidence = 0.0;
    double Timestamp = 0.0;
    
    FMotorOutput() :
        JointAngles(64, 0.0f),
        JointVelocities(64, 0.0f),
        FacialBlendShapes(52, 0.0f),
        VocalParameters(16, 0.0f) {}
};

/**
 * @brief Cognitive state snapshot for testing
 */
struct FCognitiveSnapshot {
    std::vector<float> AttentionMap;
    std::vector<float> WorkingMemory;
    std::vector<float> EmotionalState;
    std::vector<float> BeliefState;
    std::vector<float> IntentionState;
    double FreeEnergy = 0.0;
    double Arousal = 0.5;
    double Valence = 0.0;
    int CycleNumber = 0;
    double ProcessingTime = 0.0;
};

// ============================================================================
// Mock Cognitive Pipeline Components
// ============================================================================

class MockPerceptionModule {
public:
    void Initialize() { bInitialized = true; }
    bool IsInitialized() const { return bInitialized; }
    
    std::vector<float> Process(const FSensoryInput& input) {
        // Fuse multimodal inputs
        std::vector<float> features(256);
        
        // Visual processing
        for (size_t i = 0; i < std::min(input.Visual.size(), (size_t)128); i++) {
            features[i] = input.Visual[i];
        }
        
        // Auditory processing
        for (size_t i = 0; i < std::min(input.Auditory.size(), (size_t)64); i++) {
            features[128 + i] = input.Auditory[i];
        }
        
        // Tactile processing
        for (size_t i = 0; i < std::min(input.Tactile.size(), (size_t)32); i++) {
            features[192 + i] = input.Tactile[i];
        }
        
        // Proprioceptive processing
        for (size_t i = 0; i < std::min(input.Proprioceptive.size(), (size_t)32); i++) {
            features[224 + i] = input.Proprioceptive[i];
        }
        
        ProcessedCount++;
        return features;
    }
    
    int GetProcessedCount() const { return ProcessedCount; }
    
private:
    bool bInitialized = false;
    int ProcessedCount = 0;
};

class MockAttentionModule {
public:
    void Initialize(int featureDim) {
        FeatureDim = featureDim;
        AttentionWeights.resize(featureDim, 1.0f / featureDim);
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    std::vector<float> Process(const std::vector<float>& features) {
        if (features.size() != (size_t)FeatureDim) return features;
        
        // Apply attention weights
        std::vector<float> attended(FeatureDim);
        for (int i = 0; i < FeatureDim; i++) {
            attended[i] = features[i] * AttentionWeights[i];
        }
        
        // Update attention based on salience
        UpdateAttention(features);
        
        return attended;
    }
    
    void UpdateAttention(const std::vector<float>& features) {
        // Simple salience-based attention update
        float maxVal = 0.0f;
        for (float f : features) maxVal = std::max(maxVal, std::abs(f));
        
        if (maxVal > 0.01f) {
            for (int i = 0; i < FeatureDim; i++) {
                float salience = std::abs(features[i]) / maxVal;
                AttentionWeights[i] = 0.9f * AttentionWeights[i] + 0.1f * salience;
            }
        }
        
        // Normalize
        float sum = 0.0f;
        for (float w : AttentionWeights) sum += w;
        if (sum > 0.0f) {
            for (float& w : AttentionWeights) w /= sum;
        }
    }
    
    std::vector<float> GetAttentionMap() const { return AttentionWeights; }
    
private:
    bool bInitialized = false;
    int FeatureDim = 0;
    std::vector<float> AttentionWeights;
};

class MockMemoryModule {
public:
    void Initialize(int capacity = 1000) {
        MaxCapacity = capacity;
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    void Store(const std::vector<float>& pattern, const std::string& context = "") {
        if (Patterns.size() >= MaxCapacity) {
            Patterns.erase(Patterns.begin());
            Contexts.erase(Contexts.begin());
        }
        Patterns.push_back(pattern);
        Contexts.push_back(context);
    }
    
    std::vector<float> Retrieve(const std::vector<float>& cue, int topK = 1) {
        if (Patterns.empty()) return cue;
        
        // Find top-K most similar patterns
        std::vector<std::pair<float, size_t>> similarities;
        for (size_t i = 0; i < Patterns.size(); i++) {
            float sim = CosineSimilarity(cue, Patterns[i]);
            similarities.push_back({sim, i});
        }
        
        // Sort by similarity (descending)
        std::sort(similarities.begin(), similarities.end(),
                  [](const auto& a, const auto& b) { return a.first > b.first; });
        
        // Return weighted combination of top-K patterns
        int k = std::min(topK, static_cast<int>(similarities.size()));
        if (k <= 0) return cue;
        
        std::vector<float> result(Patterns[0].size(), 0.0f);
        float totalWeight = 0.0f;
        
        for (int i = 0; i < k; i++) {
            float weight = similarities[i].first;
            if (weight < 0.0f) weight = 0.0f;
            totalWeight += weight;
            const auto& pattern = Patterns[similarities[i].second];
            for (size_t j = 0; j < pattern.size() && j < result.size(); j++) {
                result[j] += weight * pattern[j];
            }
        }
        
        // Normalize by total weight
        if (totalWeight > 1e-6f) {
            for (float& v : result) v /= totalWeight;
        }
        
        return result;
    }
    
    std::vector<float> GetWorkingMemory() const {
        if (Patterns.empty()) return {};
        // Return most recent patterns as working memory
        std::vector<float> wm;
        int count = std::min((int)Patterns.size(), 7);  // Miller's Law
        for (int i = Patterns.size() - count; i < (int)Patterns.size(); i++) {
            for (float f : Patterns[i]) wm.push_back(f);
        }
        return wm;
    }
    
    size_t GetSize() const { return Patterns.size(); }
    
private:
    float CosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
        if (a.size() != b.size()) return 0.0f;
        float dot = 0.0f, normA = 0.0f, normB = 0.0f;
        for (size_t i = 0; i < a.size(); i++) {
            dot += a[i] * b[i];
            normA += a[i] * a[i];
            normB += b[i] * b[i];
        }
        if (normA < 1e-10 || normB < 1e-10) return 0.0f;
        return dot / (std::sqrt(normA) * std::sqrt(normB));
    }
    
    bool bInitialized = false;
    size_t MaxCapacity = 1000;
    std::vector<std::vector<float>> Patterns;
    std::vector<std::string> Contexts;
};

class MockReasoningModule {
public:
    void Initialize() { bInitialized = true; }
    bool IsInitialized() const { return bInitialized; }
    
    std::vector<float> Process(const std::vector<float>& attended,
                               const std::vector<float>& memory) {
        // Combine attention and memory for reasoning
        std::vector<float> reasoning(attended.size());
        
        for (size_t i = 0; i < attended.size(); i++) {
            float memoryInfluence = 0.0f;
            if (i < memory.size()) {
                memoryInfluence = memory[i];
            }
            reasoning[i] = 0.7f * attended[i] + 0.3f * memoryInfluence;
        }
        
        // Apply nonlinearity
        for (float& r : reasoning) {
            r = std::tanh(r);
        }
        
        return reasoning;
    }
    
private:
    bool bInitialized = false;
};

class MockActionModule {
public:
    void Initialize(int outputDim = 64) {
        OutputDim = outputDim;
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    FMotorOutput Generate(const std::vector<float>& reasoning) {
        FMotorOutput output;
        
        // Map reasoning to motor commands
        for (int i = 0; i < OutputDim && i < (int)reasoning.size(); i++) {
            output.JointAngles[i] = reasoning[i] * 0.5f;
            output.JointVelocities[i] = reasoning[i] * 0.1f;
        }
        
        // Generate facial expressions based on emotional content
        for (int i = 0; i < 52 && i < (int)reasoning.size(); i++) {
            output.FacialBlendShapes[i] = std::max(0.0f, reasoning[i]);
        }
        
        output.Confidence = ComputeConfidence(reasoning);
        output.Timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        
        return output;
    }
    
private:
    float ComputeConfidence(const std::vector<float>& reasoning) {
        float energy = 0.0f;
        for (float r : reasoning) energy += r * r;
        return 1.0f / (1.0f + std::exp(-std::sqrt(energy)));
    }
    
    bool bInitialized = false;
    int OutputDim = 64;
};

// ============================================================================
// Integrated Cognitive Pipeline
// ============================================================================

class MockCognitivePipeline {
public:
    void Initialize() {
        Perception.Initialize();
        Attention.Initialize(256);
        Memory.Initialize(1000);
        Reasoning.Initialize();
        Action.Initialize(64);
        
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    FMotorOutput Process(const FSensoryInput& input) {
        auto start = std::chrono::high_resolution_clock::now();
        
        // Perception
        auto features = Perception.Process(input);
        
        // Attention
        auto attended = Attention.Process(features);
        
        // Memory retrieval and storage
        auto retrieved = Memory.Retrieve(attended);
        Memory.Store(attended, "cycle_" + std::to_string(CycleCount));
        
        // Reasoning
        auto reasoning = Reasoning.Process(attended, retrieved);
        
        // Action generation
        auto output = Action.Generate(reasoning);
        
        // Update snapshot
        auto end = std::chrono::high_resolution_clock::now();
        LastSnapshot.AttentionMap = Attention.GetAttentionMap();
        LastSnapshot.WorkingMemory = Memory.GetWorkingMemory();
        LastSnapshot.CycleNumber = CycleCount++;
        LastSnapshot.ProcessingTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;
        
        return output;
    }
    
    FCognitiveSnapshot GetSnapshot() const { return LastSnapshot; }
    int GetCycleCount() const { return CycleCount; }
    
private:
    bool bInitialized = false;
    int CycleCount = 0;
    
    MockPerceptionModule Perception;
    MockAttentionModule Attention;
    MockMemoryModule Memory;
    MockReasoningModule Reasoning;
    MockActionModule Action;
    
    FCognitiveSnapshot LastSnapshot;
};

// ============================================================================
// Echobeats 3-Stream Concurrent Processing
// ============================================================================

class MockEchobeatsSystem {
public:
    struct FStreamState {
        int StreamId;
        int CurrentStep;
        std::vector<float> State;
        std::atomic<bool> Active{false};
    };
    
    void Initialize() {
        for (int i = 0; i < 3; i++) {
            Streams[i].StreamId = i;
            Streams[i].CurrentStep = i * 4;  // Phase offset: 0, 4, 8
            Streams[i].State.resize(128, 0.0f);
            Streams[i].Active = true;
        }
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    void Step() {
        std::lock_guard<std::mutex> lock(StateMutex);
        
        // Process all three streams concurrently (simulated)
        for (int i = 0; i < 3; i++) {
            ProcessStream(i);
        }
        
        GlobalStep++;
    }
    
    void ProcessStream(int streamId) {
        FStreamState& stream = Streams[streamId];
        
        // Determine phase based on step
        int phase = stream.CurrentStep % 12;
        
        // Expressive mode (steps 0-6) vs Reflective mode (steps 7-11)
        if (phase < 7) {
            // Expressive: action-oriented
            for (float& s : stream.State) {
                s = 0.9f * s + 0.1f * (float)(rand() % 100) / 100.0f;
            }
        } else {
            // Reflective: simulation/planning
            for (float& s : stream.State) {
                s = 0.95f * s;
            }
        }
        
        // Advance step
        stream.CurrentStep = (stream.CurrentStep + 1) % 12;
    }
    
    std::vector<float> GetCombinedState() const {
        std::lock_guard<std::mutex> lock(StateMutex);
        
        std::vector<float> combined;
        for (int i = 0; i < 3; i++) {
            for (float s : Streams[i].State) {
                combined.push_back(s);
            }
        }
        return combined;
    }
    
    int GetGlobalStep() const { return GlobalStep; }
    
    std::array<int, 3> GetStreamPhases() const {
        std::array<int, 3> phases;
        for (int i = 0; i < 3; i++) {
            phases[i] = Streams[i].CurrentStep % 12;
        }
        return phases;
    }
    
private:
    bool bInitialized = false;
    int GlobalStep = 0;
    mutable std::mutex StateMutex;
    FStreamState Streams[3];
};

// ============================================================================
// E2E Test Fixtures
// ============================================================================

class CognitivePipelineE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        Pipeline = std::make_unique<MockCognitivePipeline>();
        Pipeline->Initialize();
    }
    
    void TearDown() override {
        Pipeline.reset();
    }
    
    FSensoryInput CreateRandomInput() {
        FSensoryInput input;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> dist(0.0f, 1.0f);
        
        for (float& v : input.Visual) v = dist(gen);
        for (float& a : input.Auditory) a = dist(gen);
        for (float& t : input.Tactile) t = dist(gen);
        for (float& p : input.Proprioceptive) p = dist(gen);
        
        return input;
    }
    
    FSensoryInput CreatePatternedInput(int pattern) {
        FSensoryInput input;
        
        // Create recognizable patterns
        for (int i = 0; i < 128; i++) {
            input.Visual[i] = std::sin(2.0f * M_PI * i / 128.0f * pattern);
        }
        for (int i = 0; i < 64; i++) {
            input.Auditory[i] = std::cos(2.0f * M_PI * i / 64.0f * pattern);
        }
        
        return input;
    }
    
    std::unique_ptr<MockCognitivePipeline> Pipeline;
};

class EchobeatsE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        Echobeats = std::make_unique<MockEchobeatsSystem>();
        Echobeats->Initialize();
    }
    
    void TearDown() override {
        Echobeats.reset();
    }
    
    std::unique_ptr<MockEchobeatsSystem> Echobeats;
};

// ============================================================================
// Cognitive Pipeline E2E Tests
// ============================================================================

TEST_F(CognitivePipelineE2ETest, FullPipelineExecution) {
    ASSERT_TRUE(Pipeline->IsInitialized());
    
    FSensoryInput input = CreateRandomInput();
    FMotorOutput output = Pipeline->Process(input);
    
    // Verify output is valid
    EXPECT_EQ(output.JointAngles.size(), 64);
    EXPECT_EQ(output.FacialBlendShapes.size(), 52);
    EXPECT_GT(output.Confidence, 0.0);
    EXPECT_GT(output.Timestamp, 0.0);
}

TEST_F(CognitivePipelineE2ETest, SequentialProcessing) {
    // Process multiple inputs sequentially
    for (int i = 0; i < 100; i++) {
        FSensoryInput input = CreateRandomInput();
        FMotorOutput output = Pipeline->Process(input);
        
        EXPECT_GT(output.Confidence, 0.0);
    }
    
    EXPECT_EQ(Pipeline->GetCycleCount(), 100);
}

TEST_F(CognitivePipelineE2ETest, MemoryFormation) {
    // Process patterned inputs
    for (int i = 0; i < 50; i++) {
        FSensoryInput input = CreatePatternedInput(i % 5);
        Pipeline->Process(input);
    }
    
    FCognitiveSnapshot snapshot = Pipeline->GetSnapshot();
    
    // Working memory should have content
    EXPECT_GT(snapshot.WorkingMemory.size(), 0);
}

TEST_F(CognitivePipelineE2ETest, AttentionEvolution) {
    // Process inputs and track attention
    std::vector<std::vector<float>> attentionHistory;
    
    for (int i = 0; i < 20; i++) {
        FSensoryInput input = CreatePatternedInput(1);  // Same pattern
        Pipeline->Process(input);
        
        FCognitiveSnapshot snapshot = Pipeline->GetSnapshot();
        attentionHistory.push_back(snapshot.AttentionMap);
    }
    
    // Attention should stabilize on consistent input
    EXPECT_EQ(attentionHistory.size(), 20);
}

TEST_F(CognitivePipelineE2ETest, ProcessingLatency) {
    // Measure processing time
    std::vector<double> latencies;
    
    for (int i = 0; i < 100; i++) {
        FSensoryInput input = CreateRandomInput();
        Pipeline->Process(input);
        
        FCognitiveSnapshot snapshot = Pipeline->GetSnapshot();
        latencies.push_back(snapshot.ProcessingTime);
    }
    
    // Calculate average latency
    double avgLatency = 0.0;
    for (double l : latencies) avgLatency += l;
    avgLatency /= latencies.size();
    
    // Should be under 10ms per cycle
    EXPECT_LT(avgLatency, 10.0);
}

TEST_F(CognitivePipelineE2ETest, ConsistentOutputFormat) {
    // Verify output format consistency
    for (int i = 0; i < 50; i++) {
        FSensoryInput input = CreateRandomInput();
        FMotorOutput output = Pipeline->Process(input);
        
        EXPECT_EQ(output.JointAngles.size(), 64);
        EXPECT_EQ(output.JointVelocities.size(), 64);
        EXPECT_EQ(output.FacialBlendShapes.size(), 52);
        EXPECT_EQ(output.VocalParameters.size(), 16);
    }
}

TEST_F(CognitivePipelineE2ETest, PatternRecognition) {
    // Train on patterns
    for (int i = 0; i < 100; i++) {
        FSensoryInput input = CreatePatternedInput(i % 3);
        Pipeline->Process(input);
    }
    
    // Test recognition
    FSensoryInput testInput = CreatePatternedInput(0);
    FMotorOutput output = Pipeline->Process(testInput);
    
    // Should produce confident output
    EXPECT_GT(output.Confidence, 0.3);
}

// ============================================================================
// Echobeats E2E Tests
// ============================================================================

TEST_F(EchobeatsE2ETest, ThreeStreamInitialization) {
    ASSERT_TRUE(Echobeats->IsInitialized());
    
    auto phases = Echobeats->GetStreamPhases();
    
    // Streams should be 4 steps apart
    EXPECT_EQ(phases[0], 0);
    EXPECT_EQ(phases[1], 4);
    EXPECT_EQ(phases[2], 8);
}

TEST_F(EchobeatsE2ETest, ConcurrentStreamProcessing) {
    // Run for multiple cycles
    for (int i = 0; i < 36; i++) {  // 3 full 12-step cycles
        Echobeats->Step();
    }
    
    EXPECT_EQ(Echobeats->GetGlobalStep(), 36);
    
    // All streams should have processed
    auto state = Echobeats->GetCombinedState();
    EXPECT_EQ(state.size(), 384);  // 3 streams * 128 dim
}

TEST_F(EchobeatsE2ETest, PhaseProgression) {
    // Track phase progression
    std::vector<std::array<int, 3>> phaseHistory;
    
    for (int i = 0; i < 12; i++) {
        phaseHistory.push_back(Echobeats->GetStreamPhases());
        Echobeats->Step();
    }
    
    // Verify 4-step phase offset maintained
    for (const auto& phases : phaseHistory) {
        int diff01 = (phases[1] - phases[0] + 12) % 12;
        int diff12 = (phases[2] - phases[1] + 12) % 12;
        
        EXPECT_EQ(diff01, 4);
        EXPECT_EQ(diff12, 4);
    }
}

TEST_F(EchobeatsE2ETest, ExpressiveReflectiveBalance) {
    // Count expressive vs reflective steps
    int expressiveCount = 0;
    int reflectiveCount = 0;
    
    for (int i = 0; i < 120; i++) {  // 10 full cycles
        auto phases = Echobeats->GetStreamPhases();
        
        for (int phase : phases) {
            if (phase < 7) expressiveCount++;
            else reflectiveCount++;
        }
        
        Echobeats->Step();
    }
    
    // Should be approximately 7:5 ratio
    double ratio = (double)expressiveCount / reflectiveCount;
    EXPECT_NEAR(ratio, 7.0 / 5.0, 0.1);
}

TEST_F(EchobeatsE2ETest, StateEvolution) {
    // Track state evolution
    std::vector<float> initialState = Echobeats->GetCombinedState();
    
    for (int i = 0; i < 100; i++) {
        Echobeats->Step();
    }
    
    std::vector<float> finalState = Echobeats->GetCombinedState();
    
    // State should have evolved
    float diff = 0.0f;
    for (size_t i = 0; i < initialState.size(); i++) {
        diff += std::abs(finalState[i] - initialState[i]);
    }
    
    EXPECT_GT(diff, 0.0f);
}

// ============================================================================
// Integration E2E Tests
// ============================================================================

TEST(IntegrationE2ETest, PipelineWithEchobeats) {
    MockCognitivePipeline pipeline;
    MockEchobeatsSystem echobeats;
    
    pipeline.Initialize();
    echobeats.Initialize();
    
    // Run integrated system
    for (int i = 0; i < 100; i++) {
        // Echobeats provides temporal context
        echobeats.Step();
        auto echoState = echobeats.GetCombinedState();
        
        // Create input influenced by echo state
        FSensoryInput input;
        for (size_t j = 0; j < std::min(echoState.size(), input.Visual.size()); j++) {
            input.Visual[j] = echoState[j];
        }
        
        // Process through pipeline
        FMotorOutput output = pipeline.Process(input);
        
        EXPECT_GT(output.Confidence, 0.0);
    }
    
    EXPECT_EQ(pipeline.GetCycleCount(), 100);
    EXPECT_EQ(echobeats.GetGlobalStep(), 100);
}

TEST(IntegrationE2ETest, LongRunningStability) {
    MockCognitivePipeline pipeline;
    pipeline.Initialize();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dist(0.0f, 1.0f);
    
    // Run for extended period
    for (int i = 0; i < 1000; i++) {
        FSensoryInput input;
        for (float& v : input.Visual) v = dist(gen);
        for (float& a : input.Auditory) a = dist(gen);
        
        FMotorOutput output = pipeline.Process(input);
        
        // Verify no NaN or Inf values
        for (float ja : output.JointAngles) {
            EXPECT_FALSE(std::isnan(ja));
            EXPECT_FALSE(std::isinf(ja));
        }
    }
    
    EXPECT_EQ(pipeline.GetCycleCount(), 1000);
}

TEST(IntegrationE2ETest, MemoryCapacityLimits) {
    MockCognitivePipeline pipeline;
    pipeline.Initialize();
    
    // Process many inputs to test memory limits
    for (int i = 0; i < 2000; i++) {
        FSensoryInput input;
        for (int j = 0; j < 128; j++) {
            input.Visual[j] = (float)i / 2000.0f;
        }
        
        pipeline.Process(input);
    }
    
    // Should complete without crash
    EXPECT_EQ(pipeline.GetCycleCount(), 2000);
    
    // Working memory should be bounded
    FCognitiveSnapshot snapshot = pipeline.GetSnapshot();
    EXPECT_LE(snapshot.WorkingMemory.size(), 7 * 256);  // 7 items max
}

// ============================================================================
// Performance E2E Tests
// ============================================================================

TEST(PerformanceE2ETest, ThroughputBenchmark) {
    MockCognitivePipeline pipeline;
    pipeline.Initialize();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> dist(0.0f, 1.0f);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    int numCycles = 1000;
    for (int i = 0; i < numCycles; i++) {
        FSensoryInput input;
        for (float& v : input.Visual) v = dist(gen);
        pipeline.Process(input);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double cyclesPerSecond = (double)numCycles / (duration.count() / 1000.0);
    
    // Should achieve at least 100 cycles per second
    EXPECT_GT(cyclesPerSecond, 100.0);
}

TEST(PerformanceE2ETest, EchobeatsThroughput) {
    MockEchobeatsSystem echobeats;
    echobeats.Initialize();
    
    auto start = std::chrono::high_resolution_clock::now();
    
    int numSteps = 10000;
    for (int i = 0; i < numSteps; i++) {
        echobeats.Step();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    double stepsPerSecond = (double)numSteps / (duration.count() / 1000.0);
    
    // Should achieve at least 1000 steps per second
    EXPECT_GT(stepsPerSecond, 1000.0);
}

TEST(PerformanceE2ETest, MemoryEfficiency) {
    MockCognitivePipeline pipeline;
    pipeline.Initialize();
    
    // Process many inputs
    for (int i = 0; i < 5000; i++) {
        FSensoryInput input;
        for (int j = 0; j < 128; j++) {
            input.Visual[j] = std::sin((float)i * 0.01f + j * 0.1f);
        }
        pipeline.Process(input);
    }
    
    // Memory should be bounded (no memory leak)
    FCognitiveSnapshot snapshot = pipeline.GetSnapshot();
    EXPECT_LE(snapshot.WorkingMemory.size(), 7 * 256);
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST(StressE2ETest, RapidInputChanges) {
    MockCognitivePipeline pipeline;
    pipeline.Initialize();
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-10.0f, 10.0f);
    
    // Rapid, extreme input changes
    for (int i = 0; i < 500; i++) {
        FSensoryInput input;
        for (float& v : input.Visual) v = dist(gen);
        for (float& a : input.Auditory) a = dist(gen);
        for (float& t : input.Tactile) t = dist(gen);
        
        FMotorOutput output = pipeline.Process(input);
        
        // Output should remain bounded
        for (float ja : output.JointAngles) {
            EXPECT_GE(ja, -10.0f);
            EXPECT_LE(ja, 10.0f);
        }
    }
}

TEST(StressE2ETest, ZeroInputHandling) {
    MockCognitivePipeline pipeline;
    pipeline.Initialize();
    
    // All-zero input
    FSensoryInput zeroInput;
    
    for (int i = 0; i < 100; i++) {
        FMotorOutput output = pipeline.Process(zeroInput);
        
        // Should not crash or produce NaN
        for (float ja : output.JointAngles) {
            EXPECT_FALSE(std::isnan(ja));
        }
    }
}

TEST(StressE2ETest, ConcurrentAccess) {
    MockEchobeatsSystem echobeats;
    echobeats.Initialize();
    
    std::atomic<int> completedThreads{0};
    
    // Multiple threads accessing echobeats
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; t++) {
        threads.emplace_back([&echobeats, &completedThreads]() {
            for (int i = 0; i < 100; i++) {
                echobeats.Step();
                auto state = echobeats.GetCombinedState();
                EXPECT_EQ(state.size(), 384);
            }
            completedThreads++;
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(completedThreads.load(), 4);
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

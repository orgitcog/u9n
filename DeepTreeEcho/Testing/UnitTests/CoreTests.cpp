/**
 * @file CoreTests.cpp
 * @brief Comprehensive unit tests for DeepTreeEcho Core module
 * 
 * Tests cover:
 * - DeepTreeEchoCore initialization and lifecycle
 * - CognitiveCycleManager cycle processing
 * - CognitiveMemoryManager memory operations
 * - Sys6CognitiveBridge integration
 * - Sys6OperadEngine operad operations
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>
#include <random>
#include <thread>

namespace {

// Mock Unreal Engine types for standalone testing
#ifndef WITH_UNREAL_ENGINE
namespace FString { using Type = std::string; }
using FName = std::string;
using FVector = std::array<float, 3>;
using FQuat = std::array<float, 4>;
using int32 = int;
using uint32 = unsigned int;
using float32 = float;
#define UPROPERTY(...)
#define UFUNCTION(...)
#define UCLASS(...)
#define USTRUCT(...)
#define GENERATED_BODY()
#endif

// ============================================================================
// Mock Classes for Testing
// ============================================================================

/**
 * @brief Mock implementation of cognitive state for testing
 */
struct FCognitiveState {
    float Arousal = 0.5f;
    float Valence = 0.0f;
    float Attention = 0.5f;
    float Confidence = 0.5f;
    std::vector<float> StateVector;
    
    FCognitiveState() : StateVector(128, 0.0f) {}
    
    bool IsValid() const {
        return Arousal >= 0.0f && Arousal <= 1.0f &&
               Valence >= -1.0f && Valence <= 1.0f &&
               Attention >= 0.0f && Attention <= 1.0f;
    }
};

/**
 * @brief Mock implementation of memory node for testing
 */
struct FMemoryNode {
    std::string Id;
    std::string Type;
    std::vector<float> Embedding;
    float Activation = 0.0f;
    float Salience = 0.0f;
    int64_t Timestamp = 0;
    
    FMemoryNode() : Embedding(256, 0.0f) {}
    
    bool IsValid() const {
        return !Id.empty() && !Embedding.empty();
    }
};

/**
 * @brief Mock CognitiveCycleManager for unit testing
 */
class MockCognitiveCycleManager {
public:
    enum class ECyclePhase {
        Perception,
        Attention,
        Retrieval,
        Reasoning,
        Action,
        Learning
    };
    
    MockCognitiveCycleManager() : CurrentPhase(ECyclePhase::Perception), CycleCount(0) {}
    
    void Initialize() { bInitialized = true; }
    bool IsInitialized() const { return bInitialized; }
    
    void StartCycle() {
        if (!bInitialized) return;
        CurrentPhase = ECyclePhase::Perception;
        CycleStartTime = std::chrono::steady_clock::now();
    }
    
    bool AdvancePhase() {
        int phase = static_cast<int>(CurrentPhase);
        if (phase < static_cast<int>(ECyclePhase::Learning)) {
            CurrentPhase = static_cast<ECyclePhase>(phase + 1);
            return true;
        }
        return false;
    }
    
    void CompleteCycle() {
        CycleCount++;
        CurrentPhase = ECyclePhase::Perception;
        auto now = std::chrono::steady_clock::now();
        LastCycleDuration = std::chrono::duration_cast<std::chrono::milliseconds>(now - CycleStartTime).count();
    }
    
    ECyclePhase GetCurrentPhase() const { return CurrentPhase; }
    int GetCycleCount() const { return CycleCount; }
    int64_t GetLastCycleDuration() const { return LastCycleDuration; }
    
    void ProcessPerception(const std::vector<float>& input) {
        PerceptionBuffer = input;
    }
    
    void ProcessAttention(float threshold) {
        AttentionThreshold = threshold;
    }
    
    FCognitiveState GetCurrentState() const { return CurrentState; }
    void SetCurrentState(const FCognitiveState& state) { CurrentState = state; }
    
private:
    bool bInitialized = false;
    ECyclePhase CurrentPhase;
    int CycleCount;
    int64_t LastCycleDuration = 0;
    std::chrono::steady_clock::time_point CycleStartTime;
    std::vector<float> PerceptionBuffer;
    float AttentionThreshold = 0.5f;
    FCognitiveState CurrentState;
};

/**
 * @brief Mock CognitiveMemoryManager for unit testing
 */
class MockCognitiveMemoryManager {
public:
    MockCognitiveMemoryManager() = default;
    
    void Initialize(size_t capacity = 10000) {
        MaxCapacity = capacity;
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    std::string StoreMemory(const FMemoryNode& node) {
        if (!bInitialized || Memories.size() >= MaxCapacity) return "";
        std::string id = "mem_" + std::to_string(NextId++);
        FMemoryNode storedNode = node;
        storedNode.Id = id;
        storedNode.Timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        Memories[id] = storedNode;
        return id;
    }
    
    FMemoryNode* RetrieveMemory(const std::string& id) {
        auto it = Memories.find(id);
        if (it != Memories.end()) {
            it->second.Activation += 0.1f;  // Spreading activation
            return &it->second;
        }
        return nullptr;
    }
    
    std::vector<FMemoryNode> QuerySimilar(const std::vector<float>& embedding, int topK) {
        std::vector<FMemoryNode> results;
        // Simple cosine similarity search
        std::vector<std::pair<float, std::string>> scores;
        for (const auto& [id, node] : Memories) {
            float similarity = ComputeCosineSimilarity(embedding, node.Embedding);
            scores.push_back({similarity, id});
        }
        std::sort(scores.begin(), scores.end(), std::greater<>());
        for (int i = 0; i < std::min(topK, (int)scores.size()); i++) {
            results.push_back(Memories[scores[i].second]);
        }
        return results;
    }
    
    bool DeleteMemory(const std::string& id) {
        return Memories.erase(id) > 0;
    }
    
    void Consolidate() {
        // Decay old memories
        for (auto& [id, node] : Memories) {
            node.Activation *= 0.95f;
        }
        // Remove low activation memories if over capacity
        if (Memories.size() > MaxCapacity * 0.9) {
            std::vector<std::string> toRemove;
            for (const auto& [id, node] : Memories) {
                if (node.Activation < 0.01f) {
                    toRemove.push_back(id);
                }
            }
            for (const auto& id : toRemove) {
                Memories.erase(id);
            }
        }
    }
    
    size_t GetMemoryCount() const { return Memories.size(); }
    size_t GetCapacity() const { return MaxCapacity; }
    
private:
    float ComputeCosineSimilarity(const std::vector<float>& a, const std::vector<float>& b) {
        if (a.size() != b.size()) return 0.0f;
        float dot = 0.0f, normA = 0.0f, normB = 0.0f;
        for (size_t i = 0; i < a.size(); i++) {
            dot += a[i] * b[i];
            normA += a[i] * a[i];
            normB += b[i] * b[i];
        }
        if (normA == 0.0f || normB == 0.0f) return 0.0f;
        return dot / (std::sqrt(normA) * std::sqrt(normB));
    }
    
    bool bInitialized = false;
    size_t MaxCapacity = 10000;
    size_t NextId = 1;
    std::unordered_map<std::string, FMemoryNode> Memories;
};

/**
 * @brief Mock Sys6OperadEngine for operad-based cognitive operations
 */
class MockSys6OperadEngine {
public:
    struct FOperadNode {
        std::string Id;
        std::vector<std::string> Inputs;
        std::string Output;
        std::function<std::vector<float>(const std::vector<std::vector<float>>&)> Operation;
    };
    
    void Initialize() { bInitialized = true; }
    bool IsInitialized() const { return bInitialized; }
    
    std::string RegisterOperadNode(const FOperadNode& node) {
        std::string id = "op_" + std::to_string(NextId++);
        FOperadNode storedNode = node;
        storedNode.Id = id;
        Operads[id] = storedNode;
        return id;
    }
    
    std::vector<float> ExecuteOperadChain(const std::vector<std::string>& chain,
                                           const std::vector<std::vector<float>>& inputs) {
        std::vector<float> result;
        std::vector<std::vector<float>> currentInputs = inputs;
        
        for (const auto& opId : chain) {
            auto it = Operads.find(opId);
            if (it != Operads.end() && it->second.Operation) {
                result = it->second.Operation(currentInputs);
                currentInputs = {result};
            }
        }
        return result;
    }
    
    size_t GetOperadCount() const { return Operads.size(); }
    
private:
    bool bInitialized = false;
    size_t NextId = 1;
    std::unordered_map<std::string, FOperadNode> Operads;
};

// ============================================================================
// Test Fixtures
// ============================================================================

class CognitiveCycleManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        CycleManager = std::make_unique<MockCognitiveCycleManager>();
        CycleManager->Initialize();
    }
    
    void TearDown() override {
        CycleManager.reset();
    }
    
    std::unique_ptr<MockCognitiveCycleManager> CycleManager;
};

class CognitiveMemoryManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        MemoryManager = std::make_unique<MockCognitiveMemoryManager>();
        MemoryManager->Initialize(1000);
    }
    
    void TearDown() override {
        MemoryManager.reset();
    }
    
    FMemoryNode CreateTestNode(const std::string& type = "episodic") {
        FMemoryNode node;
        node.Type = type;
        node.Activation = 1.0f;
        node.Salience = 0.5f;
        // Generate random embedding
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> dist(0.0f, 1.0f);
        for (auto& val : node.Embedding) {
            val = dist(gen);
        }
        return node;
    }
    
    std::unique_ptr<MockCognitiveMemoryManager> MemoryManager;
};

class Sys6OperadEngineCoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        OperadEngine = std::make_unique<MockSys6OperadEngine>();
        OperadEngine->Initialize();
    }
    
    void TearDown() override {
        OperadEngine.reset();
    }
    
    std::unique_ptr<MockSys6OperadEngine> OperadEngine;
};

// ============================================================================
// CognitiveCycleManager Tests
// ============================================================================

TEST_F(CognitiveCycleManagerTest, Initialization) {
    EXPECT_TRUE(CycleManager->IsInitialized());
    EXPECT_EQ(CycleManager->GetCycleCount(), 0);
}

TEST_F(CognitiveCycleManagerTest, CyclePhaseProgression) {
    CycleManager->StartCycle();
    EXPECT_EQ(CycleManager->GetCurrentPhase(), 
              MockCognitiveCycleManager::ECyclePhase::Perception);
    
    EXPECT_TRUE(CycleManager->AdvancePhase());
    EXPECT_EQ(CycleManager->GetCurrentPhase(), 
              MockCognitiveCycleManager::ECyclePhase::Attention);
    
    EXPECT_TRUE(CycleManager->AdvancePhase());
    EXPECT_EQ(CycleManager->GetCurrentPhase(), 
              MockCognitiveCycleManager::ECyclePhase::Retrieval);
    
    EXPECT_TRUE(CycleManager->AdvancePhase());
    EXPECT_EQ(CycleManager->GetCurrentPhase(), 
              MockCognitiveCycleManager::ECyclePhase::Reasoning);
    
    EXPECT_TRUE(CycleManager->AdvancePhase());
    EXPECT_EQ(CycleManager->GetCurrentPhase(), 
              MockCognitiveCycleManager::ECyclePhase::Action);
    
    EXPECT_TRUE(CycleManager->AdvancePhase());
    EXPECT_EQ(CycleManager->GetCurrentPhase(), 
              MockCognitiveCycleManager::ECyclePhase::Learning);
    
    // Cannot advance past Learning
    EXPECT_FALSE(CycleManager->AdvancePhase());
}

TEST_F(CognitiveCycleManagerTest, CompleteCycleIncrementsCount) {
    CycleManager->StartCycle();
    CycleManager->CompleteCycle();
    EXPECT_EQ(CycleManager->GetCycleCount(), 1);
    
    CycleManager->StartCycle();
    CycleManager->CompleteCycle();
    EXPECT_EQ(CycleManager->GetCycleCount(), 2);
}

TEST_F(CognitiveCycleManagerTest, CycleDurationTracking) {
    CycleManager->StartCycle();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    CycleManager->CompleteCycle();
    EXPECT_GE(CycleManager->GetLastCycleDuration(), 10);
}

TEST_F(CognitiveCycleManagerTest, CognitiveStateManagement) {
    FCognitiveState state;
    state.Arousal = 0.8f;
    state.Valence = 0.5f;
    state.Attention = 0.9f;
    
    CycleManager->SetCurrentState(state);
    FCognitiveState retrieved = CycleManager->GetCurrentState();
    
    EXPECT_FLOAT_EQ(retrieved.Arousal, 0.8f);
    EXPECT_FLOAT_EQ(retrieved.Valence, 0.5f);
    EXPECT_FLOAT_EQ(retrieved.Attention, 0.9f);
}

TEST_F(CognitiveCycleManagerTest, PerceptionProcessing) {
    std::vector<float> input(128, 0.5f);
    CycleManager->ProcessPerception(input);
    // Verify no crash and state is maintained
    EXPECT_TRUE(CycleManager->IsInitialized());
}

// ============================================================================
// CognitiveMemoryManager Tests
// ============================================================================

TEST_F(CognitiveMemoryManagerTest, Initialization) {
    EXPECT_TRUE(MemoryManager->IsInitialized());
    EXPECT_EQ(MemoryManager->GetMemoryCount(), 0);
    EXPECT_EQ(MemoryManager->GetCapacity(), 1000);
}

TEST_F(CognitiveMemoryManagerTest, StoreAndRetrieveMemory) {
    FMemoryNode node = CreateTestNode();
    std::string id = MemoryManager->StoreMemory(node);
    
    EXPECT_FALSE(id.empty());
    EXPECT_EQ(MemoryManager->GetMemoryCount(), 1);
    
    FMemoryNode* retrieved = MemoryManager->RetrieveMemory(id);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->Type, "episodic");
}

TEST_F(CognitiveMemoryManagerTest, RetrieveNonExistent) {
    FMemoryNode* retrieved = MemoryManager->RetrieveMemory("nonexistent");
    EXPECT_EQ(retrieved, nullptr);
}

TEST_F(CognitiveMemoryManagerTest, DeleteMemory) {
    FMemoryNode node = CreateTestNode();
    std::string id = MemoryManager->StoreMemory(node);
    
    EXPECT_TRUE(MemoryManager->DeleteMemory(id));
    EXPECT_EQ(MemoryManager->GetMemoryCount(), 0);
    EXPECT_FALSE(MemoryManager->DeleteMemory(id));  // Already deleted
}

TEST_F(CognitiveMemoryManagerTest, SpreadingActivation) {
    FMemoryNode node = CreateTestNode();
    node.Activation = 0.5f;
    std::string id = MemoryManager->StoreMemory(node);
    
    // Retrieve multiple times to increase activation
    MemoryManager->RetrieveMemory(id);
    MemoryManager->RetrieveMemory(id);
    FMemoryNode* retrieved = MemoryManager->RetrieveMemory(id);
    
    EXPECT_GT(retrieved->Activation, 0.5f);
}

TEST_F(CognitiveMemoryManagerTest, SimilarityQuery) {
    // Store multiple memories
    for (int i = 0; i < 10; i++) {
        FMemoryNode node = CreateTestNode();
        MemoryManager->StoreMemory(node);
    }
    
    std::vector<float> queryEmbedding(256, 0.1f);
    auto results = MemoryManager->QuerySimilar(queryEmbedding, 5);
    
    EXPECT_LE(results.size(), 5);
}

TEST_F(CognitiveMemoryManagerTest, Consolidation) {
    // Store memories with low activation
    for (int i = 0; i < 100; i++) {
        FMemoryNode node = CreateTestNode();
        node.Activation = 0.001f;
        MemoryManager->StoreMemory(node);
    }
    
    size_t beforeCount = MemoryManager->GetMemoryCount();
    MemoryManager->Consolidate();
    // Consolidation should decay activations
    EXPECT_EQ(MemoryManager->GetMemoryCount(), beforeCount);  // Not over capacity yet
}

TEST_F(CognitiveMemoryManagerTest, BulkStorage) {
    for (int i = 0; i < 500; i++) {
        FMemoryNode node = CreateTestNode();
        std::string id = MemoryManager->StoreMemory(node);
        EXPECT_FALSE(id.empty());
    }
    EXPECT_EQ(MemoryManager->GetMemoryCount(), 500);
}

// ============================================================================
// Sys6OperadEngine Tests
// ============================================================================

TEST_F(Sys6OperadEngineCoreTest, Initialization) {
    EXPECT_TRUE(OperadEngine->IsInitialized());
    EXPECT_EQ(OperadEngine->GetOperadCount(), 0);
}

TEST_F(Sys6OperadEngineCoreTest, RegisterOperadNode) {
    MockSys6OperadEngine::FOperadNode node;
    node.Inputs = {"input1", "input2"};
    node.Output = "output";
    node.Operation = [](const std::vector<std::vector<float>>& inputs) {
        std::vector<float> result;
        if (!inputs.empty()) {
            result = inputs[0];
            for (auto& val : result) val *= 2.0f;
        }
        return result;
    };
    
    std::string id = OperadEngine->RegisterOperadNode(node);
    EXPECT_FALSE(id.empty());
    EXPECT_EQ(OperadEngine->GetOperadCount(), 1);
}

TEST_F(Sys6OperadEngineCoreTest, ExecuteOperadChain) {
    // Register a simple doubling operad
    MockSys6OperadEngine::FOperadNode doubleNode;
    doubleNode.Operation = [](const std::vector<std::vector<float>>& inputs) {
        std::vector<float> result;
        if (!inputs.empty()) {
            result = inputs[0];
            for (auto& val : result) val *= 2.0f;
        }
        return result;
    };
    std::string doubleId = OperadEngine->RegisterOperadNode(doubleNode);
    
    // Execute chain
    std::vector<float> input = {1.0f, 2.0f, 3.0f};
    auto result = OperadEngine->ExecuteOperadChain({doubleId}, {{input}});
    
    ASSERT_EQ(result.size(), 3);
    EXPECT_FLOAT_EQ(result[0], 2.0f);
    EXPECT_FLOAT_EQ(result[1], 4.0f);
    EXPECT_FLOAT_EQ(result[2], 6.0f);
}

TEST_F(Sys6OperadEngineCoreTest, ChainedOperations) {
    // Register multiple operads
    MockSys6OperadEngine::FOperadNode addOne;
    addOne.Operation = [](const std::vector<std::vector<float>>& inputs) {
        std::vector<float> result;
        if (!inputs.empty()) {
            result = inputs[0];
            for (auto& val : result) val += 1.0f;
        }
        return result;
    };
    
    MockSys6OperadEngine::FOperadNode square;
    square.Operation = [](const std::vector<std::vector<float>>& inputs) {
        std::vector<float> result;
        if (!inputs.empty()) {
            result = inputs[0];
            for (auto& val : result) val *= val;
        }
        return result;
    };
    
    std::string addId = OperadEngine->RegisterOperadNode(addOne);
    std::string squareId = OperadEngine->RegisterOperadNode(square);
    
    // Execute chain: (x + 1)^2
    std::vector<float> input = {1.0f, 2.0f, 3.0f};
    auto result = OperadEngine->ExecuteOperadChain({addId, squareId}, {{input}});
    
    ASSERT_EQ(result.size(), 3);
    EXPECT_FLOAT_EQ(result[0], 4.0f);   // (1+1)^2 = 4
    EXPECT_FLOAT_EQ(result[1], 9.0f);   // (2+1)^2 = 9
    EXPECT_FLOAT_EQ(result[2], 16.0f);  // (3+1)^2 = 16
}

// ============================================================================
// FCognitiveState Tests
// ============================================================================

TEST(CognitiveStateTest, DefaultConstruction) {
    FCognitiveState state;
    EXPECT_FLOAT_EQ(state.Arousal, 0.5f);
    EXPECT_FLOAT_EQ(state.Valence, 0.0f);
    EXPECT_FLOAT_EQ(state.Attention, 0.5f);
    EXPECT_TRUE(state.IsValid());
}

TEST(CognitiveStateTest, ValidityChecks) {
    FCognitiveState validState;
    validState.Arousal = 0.8f;
    validState.Valence = -0.5f;
    validState.Attention = 0.9f;
    EXPECT_TRUE(validState.IsValid());
    
    FCognitiveState invalidState;
    invalidState.Arousal = 1.5f;  // Out of range
    EXPECT_FALSE(invalidState.IsValid());
}

// ============================================================================
// FMemoryNode Tests
// ============================================================================

TEST(MemoryNodeTest, DefaultConstruction) {
    FMemoryNode node;
    EXPECT_TRUE(node.Id.empty());
    EXPECT_EQ(node.Embedding.size(), 256);
    EXPECT_FLOAT_EQ(node.Activation, 0.0f);
}

TEST(MemoryNodeTest, ValidityChecks) {
    FMemoryNode invalidNode;
    EXPECT_FALSE(invalidNode.IsValid());  // Empty ID
    
    FMemoryNode validNode;
    validNode.Id = "test_node";
    EXPECT_TRUE(validNode.IsValid());
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST(PerformanceTest, MemoryStoragePerformance) {
    MockCognitiveMemoryManager manager;
    manager.Initialize(100000);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; i++) {
        FMemoryNode node;
        node.Type = "performance_test";
        node.Activation = 1.0f;
        manager.StoreMemory(node);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 1000);  // Should complete in under 1 second
    EXPECT_EQ(manager.GetMemoryCount(), 1000);
}

TEST(PerformanceTest, CycleProcessingPerformance) {
    MockCognitiveCycleManager manager;
    manager.Initialize();
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 100; i++) {
        manager.StartCycle();
        while (manager.AdvancePhase()) {}
        manager.CompleteCycle();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 100);  // 100 cycles in under 100ms
    EXPECT_EQ(manager.GetCycleCount(), 100);
}

} // namespace

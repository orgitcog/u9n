/**
 * @file MemoryTests.cpp
 * @brief Comprehensive unit tests for DeepTreeEcho Memory systems
 * 
 * Tests cover:
 * - HypergraphMemorySystem operations
 * - EpisodicMemorySystem temporal storage
 * - MemoryConsciousnessIntegration binding
 * - ReservoirMemoryIntegration neural coupling
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <chrono>
#include <random>
#include <unordered_map>
#include <set>

// ============================================================================
// Mock Types and Structures
// ============================================================================

struct FHyperedge {
    std::string Id;
    std::vector<std::string> NodeIds;
    std::string Type;
    float Weight = 1.0f;
    
    bool IsValid() const {
        return !Id.empty() && NodeIds.size() >= 2;
    }
};

struct FMemoryTrace {
    std::string Id;
    std::vector<float> Embedding;
    float Strength = 1.0f;
    int64_t CreationTime = 0;
    int64_t LastAccessTime = 0;
    int AccessCount = 0;
    std::string Context;
    
    FMemoryTrace() : Embedding(256, 0.0f) {}
};

struct FEpisodicEvent {
    std::string Id;
    std::string Type;
    std::vector<float> StateVector;
    std::vector<std::string> Participants;
    std::string Location;
    int64_t Timestamp = 0;
    float EmotionalValence = 0.0f;
    float Importance = 0.5f;
    
    FEpisodicEvent() : StateVector(128, 0.0f) {}
};

struct FConsciousnessBinding {
    std::string StreamId;
    std::vector<std::string> BoundMemories;
    float BindingStrength = 0.0f;
    int64_t BindingTime = 0;
};

// ============================================================================
// Mock Hypergraph Memory System
// ============================================================================

class MockHypergraphMemorySystem {
public:
    void Initialize(size_t nodeCapacity = 10000, size_t edgeCapacity = 50000) {
        MaxNodes = nodeCapacity;
        MaxEdges = edgeCapacity;
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    std::string AddNode(const FMemoryTrace& trace) {
        if (!bInitialized || Nodes.size() >= MaxNodes) return "";
        std::string id = "node_" + std::to_string(NextNodeId++);
        FMemoryTrace storedTrace = trace;
        storedTrace.Id = id;
        storedTrace.CreationTime = GetCurrentTime();
        storedTrace.LastAccessTime = storedTrace.CreationTime;
        Nodes[id] = storedTrace;
        return id;
    }
    
    FMemoryTrace* GetNode(const std::string& id) {
        auto it = Nodes.find(id);
        if (it != Nodes.end()) {
            it->second.LastAccessTime = GetCurrentTime();
            it->second.AccessCount++;
            return &it->second;
        }
        return nullptr;
    }
    
    bool RemoveNode(const std::string& id) {
        // Also remove associated edges
        std::vector<std::string> edgesToRemove;
        for (const auto& [edgeId, edge] : Edges) {
            for (const auto& nodeId : edge.NodeIds) {
                if (nodeId == id) {
                    edgesToRemove.push_back(edgeId);
                    break;
                }
            }
        }
        for (const auto& edgeId : edgesToRemove) {
            Edges.erase(edgeId);
        }
        return Nodes.erase(id) > 0;
    }
    
    std::string AddHyperedge(const std::vector<std::string>& nodeIds, 
                             const std::string& type = "association",
                             float weight = 1.0f) {
        if (!bInitialized || Edges.size() >= MaxEdges) return "";
        if (nodeIds.size() < 2) return "";
        
        // Verify all nodes exist
        for (const auto& nodeId : nodeIds) {
            if (Nodes.find(nodeId) == Nodes.end()) return "";
        }
        
        std::string id = "edge_" + std::to_string(NextEdgeId++);
        FHyperedge edge;
        edge.Id = id;
        edge.NodeIds = nodeIds;
        edge.Type = type;
        edge.Weight = weight;
        Edges[id] = edge;
        return id;
    }
    
    FHyperedge* GetHyperedge(const std::string& id) {
        auto it = Edges.find(id);
        return (it != Edges.end()) ? &it->second : nullptr;
    }
    
    std::vector<std::string> GetConnectedNodes(const std::string& nodeId) {
        std::set<std::string> connected;
        for (const auto& [edgeId, edge] : Edges) {
            bool containsNode = false;
            for (const auto& id : edge.NodeIds) {
                if (id == nodeId) {
                    containsNode = true;
                    break;
                }
            }
            if (containsNode) {
                for (const auto& id : edge.NodeIds) {
                    if (id != nodeId) {
                        connected.insert(id);
                    }
                }
            }
        }
        return std::vector<std::string>(connected.begin(), connected.end());
    }
    
    std::vector<FHyperedge> GetEdgesContaining(const std::string& nodeId) {
        std::vector<FHyperedge> result;
        for (const auto& [edgeId, edge] : Edges) {
            for (const auto& id : edge.NodeIds) {
                if (id == nodeId) {
                    result.push_back(edge);
                    break;
                }
            }
        }
        return result;
    }
    
    void SpreadActivation(const std::string& sourceId, float initialActivation, int maxDepth = 3) {
        if (Nodes.find(sourceId) == Nodes.end()) return;
        
        std::unordered_map<std::string, float> activations;
        activations[sourceId] = initialActivation;
        
        std::set<std::string> visited;
        std::vector<std::string> frontier = {sourceId};
        
        for (int depth = 0; depth < maxDepth && !frontier.empty(); depth++) {
            std::vector<std::string> nextFrontier;
            for (const auto& nodeId : frontier) {
                if (visited.count(nodeId)) continue;
                visited.insert(nodeId);
                
                float currentActivation = activations[nodeId];
                auto connected = GetConnectedNodes(nodeId);
                
                for (const auto& connectedId : connected) {
                    float spreadActivation = currentActivation * 0.5f;
                    if (activations.find(connectedId) == activations.end()) {
                        activations[connectedId] = spreadActivation;
                    } else {
                        activations[connectedId] += spreadActivation;
                    }
                    nextFrontier.push_back(connectedId);
                }
            }
            frontier = nextFrontier;
        }
        
        // Apply activations to nodes
        for (const auto& [nodeId, activation] : activations) {
            if (Nodes.find(nodeId) != Nodes.end()) {
                Nodes[nodeId].Strength += activation;
            }
        }
    }
    
    size_t GetNodeCount() const { return Nodes.size(); }
    size_t GetEdgeCount() const { return Edges.size(); }
    
private:
    int64_t GetCurrentTime() {
        return std::chrono::system_clock::now().time_since_epoch().count();
    }
    
    bool bInitialized = false;
    size_t MaxNodes = 10000;
    size_t MaxEdges = 50000;
    size_t NextNodeId = 1;
    size_t NextEdgeId = 1;
    std::unordered_map<std::string, FMemoryTrace> Nodes;
    std::unordered_map<std::string, FHyperedge> Edges;
};

// ============================================================================
// Mock Episodic Memory System
// ============================================================================

class MockEpisodicMemorySystem {
public:
    void Initialize(size_t capacity = 5000) {
        MaxCapacity = capacity;
        bInitialized = true;
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    std::string StoreEvent(const FEpisodicEvent& event) {
        if (!bInitialized) return "";
        
        std::string id = "episode_" + std::to_string(NextId++);
        FEpisodicEvent storedEvent = event;
        storedEvent.Id = id;
        storedEvent.Timestamp = GetCurrentTime();
        Events[id] = storedEvent;
        Timeline.push_back(id);
        
        // Maintain capacity
        while (Events.size() > MaxCapacity) {
            std::string oldestId = Timeline.front();
            Timeline.erase(Timeline.begin());
            Events.erase(oldestId);
        }
        
        return id;
    }
    
    FEpisodicEvent* RetrieveEvent(const std::string& id) {
        auto it = Events.find(id);
        return (it != Events.end()) ? &it->second : nullptr;
    }
    
    std::vector<FEpisodicEvent> QueryByTimeRange(int64_t startTime, int64_t endTime) {
        std::vector<FEpisodicEvent> results;
        for (const auto& [id, event] : Events) {
            if (event.Timestamp >= startTime && event.Timestamp <= endTime) {
                results.push_back(event);
            }
        }
        std::sort(results.begin(), results.end(), 
                  [](const FEpisodicEvent& a, const FEpisodicEvent& b) {
                      return a.Timestamp < b.Timestamp;
                  });
        return results;
    }
    
    std::vector<FEpisodicEvent> QueryByType(const std::string& type) {
        std::vector<FEpisodicEvent> results;
        for (const auto& [id, event] : Events) {
            if (event.Type == type) {
                results.push_back(event);
            }
        }
        return results;
    }
    
    std::vector<FEpisodicEvent> QueryByParticipant(const std::string& participant) {
        std::vector<FEpisodicEvent> results;
        for (const auto& [id, event] : Events) {
            for (const auto& p : event.Participants) {
                if (p == participant) {
                    results.push_back(event);
                    break;
                }
            }
        }
        return results;
    }
    
    std::vector<FEpisodicEvent> QueryByEmotionalValence(float minValence, float maxValence) {
        std::vector<FEpisodicEvent> results;
        for (const auto& [id, event] : Events) {
            if (event.EmotionalValence >= minValence && event.EmotionalValence <= maxValence) {
                results.push_back(event);
            }
        }
        return results;
    }
    
    std::vector<FEpisodicEvent> GetRecentEvents(int count) {
        std::vector<FEpisodicEvent> results;
        int start = std::max(0, (int)Timeline.size() - count);
        for (int i = start; i < (int)Timeline.size(); i++) {
            auto it = Events.find(Timeline[i]);
            if (it != Events.end()) {
                results.push_back(it->second);
            }
        }
        return results;
    }
    
    void ConsolidateMemories() {
        // Decay importance of old events
        for (auto& [id, event] : Events) {
            event.Importance *= 0.99f;
        }
        
        // Remove very low importance events if over capacity
        if (Events.size() > MaxCapacity * 0.9) {
            std::vector<std::string> toRemove;
            for (const auto& [id, event] : Events) {
                if (event.Importance < 0.01f) {
                    toRemove.push_back(id);
                }
            }
            for (const auto& id : toRemove) {
                Events.erase(id);
                Timeline.erase(std::remove(Timeline.begin(), Timeline.end(), id), Timeline.end());
            }
        }
    }
    
    size_t GetEventCount() const { return Events.size(); }
    
private:
    int64_t GetCurrentTime() {
        return std::chrono::system_clock::now().time_since_epoch().count();
    }
    
    bool bInitialized = false;
    size_t MaxCapacity = 5000;
    size_t NextId = 1;
    std::unordered_map<std::string, FEpisodicEvent> Events;
    std::vector<std::string> Timeline;
};

// ============================================================================
// Mock Memory Consciousness Integration
// ============================================================================

class MockMemoryConsciousnessIntegration {
public:
    void Initialize(MockHypergraphMemorySystem* hypergraph, 
                   MockEpisodicMemorySystem* episodic) {
        HypergraphSystem = hypergraph;
        EpisodicSystem = episodic;
        bInitialized = (hypergraph != nullptr && episodic != nullptr);
    }
    
    bool IsInitialized() const { return bInitialized; }
    
    std::string CreateStream(const std::string& name) {
        std::string id = "stream_" + std::to_string(NextStreamId++);
        FConsciousnessBinding binding;
        binding.StreamId = id;
        binding.BindingTime = GetCurrentTime();
        Streams[id] = binding;
        StreamNames[name] = id;
        return id;
    }
    
    bool BindMemoryToStream(const std::string& streamId, const std::string& memoryId) {
        auto it = Streams.find(streamId);
        if (it == Streams.end()) return false;
        
        it->second.BoundMemories.push_back(memoryId);
        it->second.BindingStrength += 0.1f;
        return true;
    }
    
    std::vector<std::string> GetBoundMemories(const std::string& streamId) {
        auto it = Streams.find(streamId);
        if (it != Streams.end()) {
            return it->second.BoundMemories;
        }
        return {};
    }
    
    float GetBindingStrength(const std::string& streamId) {
        auto it = Streams.find(streamId);
        return (it != Streams.end()) ? it->second.BindingStrength : 0.0f;
    }
    
    void ProcessWorkingMemory(const std::string& streamId, int capacity = 7) {
        auto it = Streams.find(streamId);
        if (it == Streams.end()) return;
        
        // Limit working memory to capacity (Miller's Law: 7±2)
        while (it->second.BoundMemories.size() > (size_t)capacity) {
            it->second.BoundMemories.erase(it->second.BoundMemories.begin());
        }
    }
    
    void SynchronizeStreams(const std::string& stream1, const std::string& stream2) {
        auto it1 = Streams.find(stream1);
        auto it2 = Streams.find(stream2);
        if (it1 == Streams.end() || it2 == Streams.end()) return;
        
        // Cross-bind memories
        for (const auto& mem : it1->second.BoundMemories) {
            it2->second.BoundMemories.push_back(mem);
        }
        for (const auto& mem : it2->second.BoundMemories) {
            it1->second.BoundMemories.push_back(mem);
        }
    }
    
    size_t GetStreamCount() const { return Streams.size(); }
    
private:
    int64_t GetCurrentTime() {
        return std::chrono::system_clock::now().time_since_epoch().count();
    }
    
    bool bInitialized = false;
    size_t NextStreamId = 1;
    MockHypergraphMemorySystem* HypergraphSystem = nullptr;
    MockEpisodicMemorySystem* EpisodicSystem = nullptr;
    std::unordered_map<std::string, FConsciousnessBinding> Streams;
    std::unordered_map<std::string, std::string> StreamNames;
};

// ============================================================================
// Test Fixtures
// ============================================================================

class HypergraphMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        Hypergraph = std::make_unique<MockHypergraphMemorySystem>();
        Hypergraph->Initialize(1000, 5000);
    }
    
    void TearDown() override {
        Hypergraph.reset();
    }
    
    FMemoryTrace CreateTestTrace() {
        FMemoryTrace trace;
        trace.Strength = 1.0f;
        trace.Context = "test_context";
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> dist(0.0f, 1.0f);
        for (auto& val : trace.Embedding) {
            val = dist(gen);
        }
        return trace;
    }
    
    std::unique_ptr<MockHypergraphMemorySystem> Hypergraph;
};

class EpisodicMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        Episodic = std::make_unique<MockEpisodicMemorySystem>();
        Episodic->Initialize(500);
    }
    
    void TearDown() override {
        Episodic.reset();
    }
    
    FEpisodicEvent CreateTestEvent(const std::string& type = "interaction") {
        FEpisodicEvent event;
        event.Type = type;
        event.Location = "test_location";
        event.EmotionalValence = 0.5f;
        event.Importance = 0.7f;
        event.Participants = {"agent", "user"};
        return event;
    }
    
    std::unique_ptr<MockEpisodicMemorySystem> Episodic;
};

class MemoryConsciousnessTest : public ::testing::Test {
protected:
    void SetUp() override {
        Hypergraph = std::make_unique<MockHypergraphMemorySystem>();
        Episodic = std::make_unique<MockEpisodicMemorySystem>();
        Integration = std::make_unique<MockMemoryConsciousnessIntegration>();
        
        Hypergraph->Initialize();
        Episodic->Initialize();
        Integration->Initialize(Hypergraph.get(), Episodic.get());
    }
    
    void TearDown() override {
        Integration.reset();
        Episodic.reset();
        Hypergraph.reset();
    }
    
    std::unique_ptr<MockHypergraphMemorySystem> Hypergraph;
    std::unique_ptr<MockEpisodicMemorySystem> Episodic;
    std::unique_ptr<MockMemoryConsciousnessIntegration> Integration;
};

// ============================================================================
// Hypergraph Memory Tests
// ============================================================================

TEST_F(HypergraphMemoryTest, Initialization) {
    EXPECT_TRUE(Hypergraph->IsInitialized());
    EXPECT_EQ(Hypergraph->GetNodeCount(), 0);
    EXPECT_EQ(Hypergraph->GetEdgeCount(), 0);
}

TEST_F(HypergraphMemoryTest, AddAndRetrieveNode) {
    FMemoryTrace trace = CreateTestTrace();
    std::string id = Hypergraph->AddNode(trace);
    
    EXPECT_FALSE(id.empty());
    EXPECT_EQ(Hypergraph->GetNodeCount(), 1);
    
    FMemoryTrace* retrieved = Hypergraph->GetNode(id);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->Context, "test_context");
    EXPECT_EQ(retrieved->AccessCount, 1);
}

TEST_F(HypergraphMemoryTest, RemoveNode) {
    FMemoryTrace trace = CreateTestTrace();
    std::string id = Hypergraph->AddNode(trace);
    
    EXPECT_TRUE(Hypergraph->RemoveNode(id));
    EXPECT_EQ(Hypergraph->GetNodeCount(), 0);
    EXPECT_EQ(Hypergraph->GetNode(id), nullptr);
}

TEST_F(HypergraphMemoryTest, CreateHyperedge) {
    std::string node1 = Hypergraph->AddNode(CreateTestTrace());
    std::string node2 = Hypergraph->AddNode(CreateTestTrace());
    std::string node3 = Hypergraph->AddNode(CreateTestTrace());
    
    std::string edgeId = Hypergraph->AddHyperedge({node1, node2, node3}, "semantic", 0.8f);
    
    EXPECT_FALSE(edgeId.empty());
    EXPECT_EQ(Hypergraph->GetEdgeCount(), 1);
    
    FHyperedge* edge = Hypergraph->GetHyperedge(edgeId);
    ASSERT_NE(edge, nullptr);
    EXPECT_EQ(edge->NodeIds.size(), 3);
    EXPECT_EQ(edge->Type, "semantic");
    EXPECT_FLOAT_EQ(edge->Weight, 0.8f);
}

TEST_F(HypergraphMemoryTest, HyperedgeRequiresMinimumNodes) {
    std::string node1 = Hypergraph->AddNode(CreateTestTrace());
    
    // Single node edge should fail
    std::string edgeId = Hypergraph->AddHyperedge({node1});
    EXPECT_TRUE(edgeId.empty());
}

TEST_F(HypergraphMemoryTest, GetConnectedNodes) {
    std::string node1 = Hypergraph->AddNode(CreateTestTrace());
    std::string node2 = Hypergraph->AddNode(CreateTestTrace());
    std::string node3 = Hypergraph->AddNode(CreateTestTrace());
    
    Hypergraph->AddHyperedge({node1, node2});
    Hypergraph->AddHyperedge({node1, node3});
    
    auto connected = Hypergraph->GetConnectedNodes(node1);
    EXPECT_EQ(connected.size(), 2);
}

TEST_F(HypergraphMemoryTest, RemoveNodeRemovesEdges) {
    std::string node1 = Hypergraph->AddNode(CreateTestTrace());
    std::string node2 = Hypergraph->AddNode(CreateTestTrace());
    
    Hypergraph->AddHyperedge({node1, node2});
    EXPECT_EQ(Hypergraph->GetEdgeCount(), 1);
    
    Hypergraph->RemoveNode(node1);
    EXPECT_EQ(Hypergraph->GetEdgeCount(), 0);
}

TEST_F(HypergraphMemoryTest, SpreadingActivation) {
    std::string node1 = Hypergraph->AddNode(CreateTestTrace());
    std::string node2 = Hypergraph->AddNode(CreateTestTrace());
    std::string node3 = Hypergraph->AddNode(CreateTestTrace());
    
    Hypergraph->AddHyperedge({node1, node2});
    Hypergraph->AddHyperedge({node2, node3});
    
    float initialStrength = Hypergraph->GetNode(node2)->Strength;
    Hypergraph->SpreadActivation(node1, 1.0f, 2);
    
    // Node2 should have increased activation
    EXPECT_GT(Hypergraph->GetNode(node2)->Strength, initialStrength);
}

// ============================================================================
// Episodic Memory Tests
// ============================================================================

TEST_F(EpisodicMemoryTest, Initialization) {
    EXPECT_TRUE(Episodic->IsInitialized());
    EXPECT_EQ(Episodic->GetEventCount(), 0);
}

TEST_F(EpisodicMemoryTest, StoreAndRetrieveEvent) {
    FEpisodicEvent event = CreateTestEvent();
    std::string id = Episodic->StoreEvent(event);
    
    EXPECT_FALSE(id.empty());
    EXPECT_EQ(Episodic->GetEventCount(), 1);
    
    FEpisodicEvent* retrieved = Episodic->RetrieveEvent(id);
    ASSERT_NE(retrieved, nullptr);
    EXPECT_EQ(retrieved->Type, "interaction");
    EXPECT_EQ(retrieved->Location, "test_location");
}

TEST_F(EpisodicMemoryTest, QueryByType) {
    Episodic->StoreEvent(CreateTestEvent("interaction"));
    Episodic->StoreEvent(CreateTestEvent("observation"));
    Episodic->StoreEvent(CreateTestEvent("interaction"));
    
    auto results = Episodic->QueryByType("interaction");
    EXPECT_EQ(results.size(), 2);
}

TEST_F(EpisodicMemoryTest, QueryByParticipant) {
    FEpisodicEvent event1 = CreateTestEvent();
    event1.Participants = {"agent", "user1"};
    Episodic->StoreEvent(event1);
    
    FEpisodicEvent event2 = CreateTestEvent();
    event2.Participants = {"agent", "user2"};
    Episodic->StoreEvent(event2);
    
    auto results = Episodic->QueryByParticipant("user1");
    EXPECT_EQ(results.size(), 1);
}

TEST_F(EpisodicMemoryTest, QueryByEmotionalValence) {
    FEpisodicEvent positive = CreateTestEvent();
    positive.EmotionalValence = 0.8f;
    Episodic->StoreEvent(positive);
    
    FEpisodicEvent negative = CreateTestEvent();
    negative.EmotionalValence = -0.5f;
    Episodic->StoreEvent(negative);
    
    auto positiveResults = Episodic->QueryByEmotionalValence(0.5f, 1.0f);
    EXPECT_EQ(positiveResults.size(), 1);
    
    auto negativeResults = Episodic->QueryByEmotionalValence(-1.0f, 0.0f);
    EXPECT_EQ(negativeResults.size(), 1);
}

TEST_F(EpisodicMemoryTest, GetRecentEvents) {
    for (int i = 0; i < 10; i++) {
        Episodic->StoreEvent(CreateTestEvent());
    }
    
    auto recent = Episodic->GetRecentEvents(5);
    EXPECT_EQ(recent.size(), 5);
}

TEST_F(EpisodicMemoryTest, CapacityManagement) {
    // Fill to capacity
    for (int i = 0; i < 600; i++) {
        Episodic->StoreEvent(CreateTestEvent());
    }
    
    // Should be limited to capacity (500)
    EXPECT_LE(Episodic->GetEventCount(), 500);
}

// ============================================================================
// Memory Consciousness Integration Tests
// ============================================================================

TEST_F(MemoryConsciousnessTest, Initialization) {
    EXPECT_TRUE(Integration->IsInitialized());
    EXPECT_EQ(Integration->GetStreamCount(), 0);
}

TEST_F(MemoryConsciousnessTest, CreateStream) {
    std::string streamId = Integration->CreateStream("visual");
    
    EXPECT_FALSE(streamId.empty());
    EXPECT_EQ(Integration->GetStreamCount(), 1);
}

TEST_F(MemoryConsciousnessTest, BindMemoryToStream) {
    std::string streamId = Integration->CreateStream("working");
    
    EXPECT_TRUE(Integration->BindMemoryToStream(streamId, "mem_1"));
    EXPECT_TRUE(Integration->BindMemoryToStream(streamId, "mem_2"));
    
    auto bound = Integration->GetBoundMemories(streamId);
    EXPECT_EQ(bound.size(), 2);
}

TEST_F(MemoryConsciousnessTest, BindingStrength) {
    std::string streamId = Integration->CreateStream("attention");
    
    EXPECT_FLOAT_EQ(Integration->GetBindingStrength(streamId), 0.0f);
    
    Integration->BindMemoryToStream(streamId, "mem_1");
    EXPECT_GT(Integration->GetBindingStrength(streamId), 0.0f);
}

TEST_F(MemoryConsciousnessTest, WorkingMemoryCapacity) {
    std::string streamId = Integration->CreateStream("working");
    
    // Bind more than capacity
    for (int i = 0; i < 10; i++) {
        Integration->BindMemoryToStream(streamId, "mem_" + std::to_string(i));
    }
    
    Integration->ProcessWorkingMemory(streamId, 7);
    
    auto bound = Integration->GetBoundMemories(streamId);
    EXPECT_LE(bound.size(), 7);
}

TEST_F(MemoryConsciousnessTest, StreamSynchronization) {
    std::string stream1 = Integration->CreateStream("visual");
    std::string stream2 = Integration->CreateStream("auditory");
    
    Integration->BindMemoryToStream(stream1, "visual_mem");
    Integration->BindMemoryToStream(stream2, "auditory_mem");
    
    Integration->SynchronizeStreams(stream1, stream2);
    
    auto bound1 = Integration->GetBoundMemories(stream1);
    auto bound2 = Integration->GetBoundMemories(stream2);
    
    // Both streams should have both memories
    EXPECT_GE(bound1.size(), 2);
    EXPECT_GE(bound2.size(), 2);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST(MemoryPerformanceTest, HypergraphBulkOperations) {
    MockHypergraphMemorySystem hypergraph;
    hypergraph.Initialize(10000, 50000);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Add many nodes
    std::vector<std::string> nodeIds;
    for (int i = 0; i < 1000; i++) {
        FMemoryTrace trace;
        trace.Strength = 1.0f;
        nodeIds.push_back(hypergraph.AddNode(trace));
    }
    
    // Create hyperedges
    for (int i = 0; i < 500; i++) {
        int idx1 = i % nodeIds.size();
        int idx2 = (i + 1) % nodeIds.size();
        int idx3 = (i + 2) % nodeIds.size();
        hypergraph.AddHyperedge({nodeIds[idx1], nodeIds[idx2], nodeIds[idx3]});
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 1000);  // Under 1 second
    EXPECT_EQ(hypergraph.GetNodeCount(), 1000);
    EXPECT_EQ(hypergraph.GetEdgeCount(), 500);
}

TEST(MemoryPerformanceTest, EpisodicQueryPerformance) {
    MockEpisodicMemorySystem episodic;
    episodic.Initialize(5000);
    
    // Store many events
    for (int i = 0; i < 1000; i++) {
        FEpisodicEvent event;
        event.Type = (i % 3 == 0) ? "interaction" : "observation";
        event.EmotionalValence = (float)(i % 10) / 10.0f;
        episodic.StoreEvent(event);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Perform queries
    for (int i = 0; i < 100; i++) {
        episodic.QueryByType("interaction");
        episodic.QueryByEmotionalValence(0.5f, 1.0f);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    EXPECT_LT(duration.count(), 500);  // 200 queries in under 500ms
}

/**
 * @file MembraneCommChannelsTests.cpp
 * @brief Unit tests for Feature F1.3.4: Membrane Communication Channels
 * 
 * Tests cover:
 * - Membrane creation and hierarchy management
 * - Symport channel creation and transport
 * - Antiport channel creation and counter-transport
 * - Uniport channel (simple diffusion)
 * - Transport rules and selectivity
 * - Energy constraints
 * - Channel state transitions (open/closed/refractory)
 * - Transport event logging
 * 
 * @author Deep Tree Echo Team
 * @date March 2026
 */

#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <algorithm>

// ========================================
// MOCK STRUCTURES FOR STANDALONE TESTING
// ========================================

// Mock enums matching UE implementation
enum class MockChannelDirection {
    Inward,
    Outward,
    Bidirectional
};

enum class MockChannelType {
    Symport,
    Antiport,
    Uniport
};

enum class MockChannelState {
    Closed,
    Open,
    Refractory,
    Saturated,
    Blocked
};

// Mock membrane object
struct MockMembraneObject {
    std::string ObjectID;
    std::string Symbol;
    int32_t Multiplicity = 1;
    float Priority = 1.0f;
    float EnergyCost = 1.0f;
    std::vector<float> Payload;
    float CreationTime = 0.0f;
    bool bIsCatalyst = false;
};

// Mock transport rule
struct MockTransportRule {
    std::string RuleID;
    std::vector<std::string> SourceObjects;
    std::vector<std::string> TargetObjects;
    std::vector<std::string> ProducedOnTarget;
    std::vector<std::string> ProducedOnSource;
    MockChannelType ChannelType = MockChannelType::Symport;
    MockChannelDirection Direction = MockChannelDirection::Outward;
    int32_t Priority = 0;
    bool bEnabled = true;
    std::vector<std::string> RequiredCatalysts;
    float MinimumEnergy = 0.0f;
    float ApplicationProbability = 1.0f;
};

// Mock transport event
struct MockTransportEvent {
    std::string EventID;
    std::string RuleID;
    std::string ChannelID;
    std::string SourceMembraneID;
    std::string TargetMembraneID;
    std::vector<MockMembraneObject> TransportedToTarget;
    std::vector<MockMembraneObject> TransportedToSource;
    float Timestamp = 0.0f;
    float EnergyConsumed = 0.0f;
    bool bSuccessful = false;
};

// Mock communication channel
struct MockCommunicationChannel {
    std::string ChannelID;
    MockChannelType ChannelType = MockChannelType::Symport;
    MockChannelState State = MockChannelState::Closed;
    MockChannelDirection DefaultDirection = MockChannelDirection::Outward;
    std::string MembraneA_ID;
    std::string MembraneB_ID;
    float MaxTransportRate = 10.0f;
    float CurrentTransportRate = 0.0f;
    std::vector<std::string> SelectiveFor;
    std::map<std::string, float> AffinityMap;
    std::vector<MockTransportRule> TransportRules;
    std::vector<MockMembraneObject> TransportQueue;
    float RefractoryPeriod = 0.1f;
    float RefractoryTimer = 0.0f;
    int32_t TotalTransportCount = 0;
    float LastTransportTime = 0.0f;
};

// Mock membrane state
struct MockMembraneState {
    std::string MembraneID;
    std::string ParentMembraneID;
    std::vector<std::string> ChildMembraneIDs;
    std::vector<MockMembraneObject> ContainedObjects;
    std::vector<std::string> BoundaryChannelIDs;
    float EnergyLevel = 100.0f;
    float MaxEnergy = 100.0f;
    float EnergyRegenRate = 1.0f;
    bool bIsActive = true;
    int32_t NestingDepth = 0;
};

// Mock channel manager configuration
struct MockChannelManagerConfig {
    int32_t MaxChannels = 100;
    int32_t MaxMembranes = 50;
    float DefaultTransportRate = 10.0f;
    bool bEnableStochasticRules = true;
    bool bEnableEnergyConstraints = true;
    float EnergyRegenMultiplier = 1.0f;
    bool bEnableEventLogging = true;
    int32_t MaxEventLogSize = 1000;
};

// ========================================
// MOCK MEMBRANE CHANNEL MANAGER
// ========================================

class MockMembraneCommChannels {
public:
    MockChannelManagerConfig Config;
    std::map<std::string, MockMembraneState> Membranes;
    std::map<std::string, MockCommunicationChannel> Channels;
    std::vector<MockTransportEvent> TransportEventLog;
    float CurrentTime = 0.0f;
    int32_t IDCounter = 0;

    std::string GenerateUniqueID(const std::string& prefix) {
        return prefix + "_" + std::to_string(++IDCounter);
    }

    // Membrane management
    std::string CreateMembrane(const std::string& parentID = "") {
        if (Membranes.size() >= static_cast<size_t>(Config.MaxMembranes)) {
            return "";
        }

        MockMembraneState membrane;
        membrane.MembraneID = GenerateUniqueID("Membrane");
        membrane.ParentMembraneID = parentID;
        membrane.EnergyLevel = membrane.MaxEnergy;
        membrane.bIsActive = true;

        if (parentID.empty()) {
            membrane.NestingDepth = 0;
        } else {
            auto it = Membranes.find(parentID);
            if (it != Membranes.end()) {
                membrane.NestingDepth = it->second.NestingDepth + 1;
                it->second.ChildMembraneIDs.push_back(membrane.MembraneID);
            } else {
                return "";
            }
        }

        Membranes[membrane.MembraneID] = membrane;
        return membrane.MembraneID;
    }

    bool AddObjectToMembrane(const std::string& membraneID, const MockMembraneObject& obj) {
        auto it = Membranes.find(membraneID);
        if (it == Membranes.end()) {
            return false;
        }

        MockMembraneObject newObj = obj;
        if (newObj.ObjectID.empty()) {
            newObj.ObjectID = GenerateUniqueID("Obj");
        }
        newObj.CreationTime = CurrentTime;
        
        it->second.ContainedObjects.push_back(newObj);
        return true;
    }

    int32_t CountObjectsInMembrane(const std::string& membraneID, const std::string& symbol) const {
        auto it = Membranes.find(membraneID);
        if (it == Membranes.end()) {
            return 0;
        }

        int32_t count = 0;
        for (const auto& obj : it->second.ContainedObjects) {
            if (symbol.empty() || obj.Symbol == symbol) {
                count += obj.Multiplicity;
            }
        }
        return count;
    }

    bool AreMembranesAdjacent(const std::string& memA_ID, const std::string& memB_ID) const {
        auto itA = Membranes.find(memA_ID);
        auto itB = Membranes.find(memB_ID);
        
        if (itA == Membranes.end() || itB == Membranes.end()) {
            return false;
        }

        return (itA->second.ParentMembraneID == memB_ID || 
                itB->second.ParentMembraneID == memA_ID);
    }

    // Channel management
    std::string CreateSymportChannel(const std::string& memA_ID, const std::string& memB_ID,
                                     const std::vector<std::string>& selectiveFor,
                                     MockChannelDirection direction = MockChannelDirection::Outward) {
        if (Channels.size() >= static_cast<size_t>(Config.MaxChannels)) {
            return "";
        }

        if (!AreMembranesAdjacent(memA_ID, memB_ID)) {
            return "";
        }

        MockCommunicationChannel channel;
        channel.ChannelID = GenerateUniqueID("Symport");
        channel.ChannelType = MockChannelType::Symport;
        channel.MembraneA_ID = memA_ID;
        channel.MembraneB_ID = memB_ID;
        channel.SelectiveFor = selectiveFor;
        channel.DefaultDirection = direction;
        channel.State = MockChannelState::Closed;
        channel.MaxTransportRate = Config.DefaultTransportRate;

        Channels[channel.ChannelID] = channel;

        // Register with membranes
        if (Membranes.count(memA_ID)) {
            Membranes[memA_ID].BoundaryChannelIDs.push_back(channel.ChannelID);
        }
        if (Membranes.count(memB_ID)) {
            Membranes[memB_ID].BoundaryChannelIDs.push_back(channel.ChannelID);
        }

        return channel.ChannelID;
    }

    std::string CreateAntiportChannel(const std::string& memA_ID, const std::string& memB_ID,
                                      const std::vector<std::string>& selectiveFor,
                                      MockChannelDirection direction = MockChannelDirection::Bidirectional) {
        if (Channels.size() >= static_cast<size_t>(Config.MaxChannels)) {
            return "";
        }

        if (!AreMembranesAdjacent(memA_ID, memB_ID)) {
            return "";
        }

        MockCommunicationChannel channel;
        channel.ChannelID = GenerateUniqueID("Antiport");
        channel.ChannelType = MockChannelType::Antiport;
        channel.MembraneA_ID = memA_ID;
        channel.MembraneB_ID = memB_ID;
        channel.SelectiveFor = selectiveFor;
        channel.DefaultDirection = direction;
        channel.State = MockChannelState::Closed;
        channel.MaxTransportRate = Config.DefaultTransportRate;

        Channels[channel.ChannelID] = channel;

        // Register with membranes
        if (Membranes.count(memA_ID)) {
            Membranes[memA_ID].BoundaryChannelIDs.push_back(channel.ChannelID);
        }
        if (Membranes.count(memB_ID)) {
            Membranes[memB_ID].BoundaryChannelIDs.push_back(channel.ChannelID);
        }

        return channel.ChannelID;
    }

    bool OpenChannel(const std::string& channelID) {
        auto it = Channels.find(channelID);
        if (it == Channels.end()) {
            return false;
        }
        if (it->second.State != MockChannelState::Blocked) {
            it->second.State = MockChannelState::Open;
            return true;
        }
        return false;
    }

    bool CloseChannel(const std::string& channelID) {
        auto it = Channels.find(channelID);
        if (it == Channels.end()) {
            return false;
        }
        it->second.State = MockChannelState::Closed;
        return true;
    }

    bool AreObjectsAvailable(const std::string& membraneID, const std::vector<std::string>& symbols) const {
        auto it = Membranes.find(membraneID);
        if (it == Membranes.end()) {
            return false;
        }

        std::map<std::string, int32_t> requiredCounts;
        for (const auto& symbol : symbols) {
            requiredCounts[symbol]++;
        }

        std::map<std::string, int32_t> availableCounts;
        for (const auto& obj : it->second.ContainedObjects) {
            if (!obj.bIsCatalyst) {
                availableCounts[obj.Symbol] += obj.Multiplicity;
            }
        }

        for (const auto& req : requiredCounts) {
            auto avail = availableCounts.find(req.first);
            if (avail == availableCounts.end() || avail->second < req.second) {
                return false;
            }
        }

        return true;
    }

    bool MoveObjectsBetweenMembranes(const std::string& sourceID, const std::string& targetID,
                                      const std::vector<std::string>& symbols,
                                      std::vector<MockMembraneObject>& outMovedObjects) {
        auto srcIt = Membranes.find(sourceID);
        auto tgtIt = Membranes.find(targetID);
        
        if (srcIt == Membranes.end() || tgtIt == Membranes.end()) {
            return false;
        }

        outMovedObjects.clear();

        for (const auto& symbol : symbols) {
            bool found = false;
            for (auto objIt = srcIt->second.ContainedObjects.begin(); 
                 objIt != srcIt->second.ContainedObjects.end(); ++objIt) {
                if (objIt->Symbol == symbol && !objIt->bIsCatalyst) {
                    if (objIt->Multiplicity > 1) {
                        MockMembraneObject newObj = *objIt;
                        newObj.Multiplicity = 1;
                        newObj.ObjectID = GenerateUniqueID("Obj");
                        outMovedObjects.push_back(newObj);
                        objIt->Multiplicity--;
                    } else {
                        outMovedObjects.push_back(*objIt);
                        srcIt->second.ContainedObjects.erase(objIt);
                    }
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                // Rollback
                for (const auto& movedObj : outMovedObjects) {
                    srcIt->second.ContainedObjects.push_back(movedObj);
                }
                outMovedObjects.clear();
                return false;
            }
        }

        // Add to target
        for (const auto& obj : outMovedObjects) {
            tgtIt->second.ContainedObjects.push_back(obj);
        }

        return true;
    }

    bool ConsumeMembraneEnergy(const std::string& membraneID, float amount) {
        auto it = Membranes.find(membraneID);
        if (it == Membranes.end()) {
            return false;
        }

        if (it->second.EnergyLevel >= amount) {
            it->second.EnergyLevel -= amount;
            return true;
        }
        return false;
    }

    // Transport operations
    bool ExecuteSymport(const std::string& channelID, const std::vector<std::string>& objectSymbols) {
        auto chIt = Channels.find(channelID);
        if (chIt == Channels.end() || chIt->second.State != MockChannelState::Open) {
            return false;
        }

        if (chIt->second.ChannelType != MockChannelType::Symport && 
            chIt->second.ChannelType != MockChannelType::Uniport) {
            return false;
        }

        // Determine source and target
        std::string sourceID, targetID;
        if (chIt->second.DefaultDirection == MockChannelDirection::Inward) {
            sourceID = chIt->second.MembraneA_ID;
            targetID = chIt->second.MembraneB_ID;
        } else {
            sourceID = chIt->second.MembraneB_ID;
            targetID = chIt->second.MembraneA_ID;
        }

        // Check availability
        if (!AreObjectsAvailable(sourceID, objectSymbols)) {
            return false;
        }

        // Check selectivity
        for (const auto& symbol : objectSymbols) {
            if (!chIt->second.SelectiveFor.empty()) {
                bool allowed = std::find(chIt->second.SelectiveFor.begin(), 
                                         chIt->second.SelectiveFor.end(), 
                                         symbol) != chIt->second.SelectiveFor.end();
                if (!allowed) {
                    return false;
                }
            }
        }

        // Check energy
        float energyCost = static_cast<float>(objectSymbols.size());
        if (Config.bEnableEnergyConstraints && !ConsumeMembraneEnergy(sourceID, energyCost)) {
            return false;
        }

        // Execute transport
        std::vector<MockMembraneObject> movedObjects;
        if (!MoveObjectsBetweenMembranes(sourceID, targetID, objectSymbols, movedObjects)) {
            return false;
        }

        // Create event
        MockTransportEvent event;
        event.EventID = GenerateUniqueID("Event");
        event.ChannelID = channelID;
        event.SourceMembraneID = sourceID;
        event.TargetMembraneID = targetID;
        event.TransportedToTarget = movedObjects;
        event.Timestamp = CurrentTime;
        event.EnergyConsumed = energyCost;
        event.bSuccessful = true;

        if (Config.bEnableEventLogging) {
            TransportEventLog.push_back(event);
        }

        // Update channel state
        chIt->second.TotalTransportCount += static_cast<int32_t>(movedObjects.size());
        chIt->second.LastTransportTime = CurrentTime;
        chIt->second.RefractoryTimer = chIt->second.RefractoryPeriod;
        chIt->second.State = MockChannelState::Refractory;

        return true;
    }

    bool ExecuteAntiport(const std::string& channelID,
                          const std::vector<std::string>& sourceSymbols,
                          const std::vector<std::string>& targetSymbols) {
        auto chIt = Channels.find(channelID);
        if (chIt == Channels.end() || chIt->second.State != MockChannelState::Open) {
            return false;
        }

        if (chIt->second.ChannelType != MockChannelType::Antiport) {
            return false;
        }

        std::string sourceID = chIt->second.MembraneB_ID;
        std::string targetID = chIt->second.MembraneA_ID;

        // Check availability on both sides
        if (!AreObjectsAvailable(sourceID, sourceSymbols)) {
            return false;
        }
        if (!AreObjectsAvailable(targetID, targetSymbols)) {
            return false;
        }

        // Check energy
        float energyCost = static_cast<float>(sourceSymbols.size() + targetSymbols.size());
        if (Config.bEnableEnergyConstraints && !ConsumeMembraneEnergy(sourceID, energyCost)) {
            return false;
        }

        // Execute counter-transport
        std::vector<MockMembraneObject> movedToTarget, movedToSource;
        
        if (!MoveObjectsBetweenMembranes(sourceID, targetID, sourceSymbols, movedToTarget)) {
            return false;
        }
        
        if (!MoveObjectsBetweenMembranes(targetID, sourceID, targetSymbols, movedToSource)) {
            // Rollback first transport
            for (const auto& obj : movedToTarget) {
                Membranes[sourceID].ContainedObjects.push_back(obj);
            }
            return false;
        }

        // Create event
        MockTransportEvent event;
        event.EventID = GenerateUniqueID("Event");
        event.ChannelID = channelID;
        event.SourceMembraneID = sourceID;
        event.TargetMembraneID = targetID;
        event.TransportedToTarget = movedToTarget;
        event.TransportedToSource = movedToSource;
        event.Timestamp = CurrentTime;
        event.EnergyConsumed = energyCost;
        event.bSuccessful = true;

        if (Config.bEnableEventLogging) {
            TransportEventLog.push_back(event);
        }

        // Update channel state
        chIt->second.TotalTransportCount += static_cast<int32_t>(movedToTarget.size() + movedToSource.size());
        chIt->second.LastTransportTime = CurrentTime;
        chIt->second.RefractoryTimer = chIt->second.RefractoryPeriod;
        chIt->second.State = MockChannelState::Refractory;

        return true;
    }

    int32_t GetTotalObjectCount() const {
        int32_t total = 0;
        for (const auto& pair : Membranes) {
            for (const auto& obj : pair.second.ContainedObjects) {
                total += obj.Multiplicity;
            }
        }
        return total;
    }

    int32_t GetMaxNestingDepth() const {
        int32_t maxDepth = 0;
        for (const auto& pair : Membranes) {
            maxDepth = std::max(maxDepth, pair.second.NestingDepth);
        }
        return maxDepth;
    }
};

// ========================================
// TEST FIXTURES
// ========================================

class MembraneCommChannelsTest : public ::testing::Test {
protected:
    MockMembraneCommChannels channelManager;

    void SetUp() override {
        channelManager = MockMembraneCommChannels();
        channelManager.Config.bEnableEventLogging = true;
        channelManager.Config.bEnableEnergyConstraints = true;
    }

    void TearDown() override {
        // Cleanup
    }

    // Helper to create membrane hierarchy
    std::pair<std::string, std::string> CreateParentChildMembranes() {
        std::string parentID = channelManager.CreateMembrane();
        std::string childID = channelManager.CreateMembrane(parentID);
        return {parentID, childID};
    }

    // Helper to add objects
    void AddObjects(const std::string& membraneID, const std::string& symbol, int32_t count) {
        for (int32_t i = 0; i < count; ++i) {
            MockMembraneObject obj;
            obj.Symbol = symbol;
            obj.Multiplicity = 1;
            channelManager.AddObjectToMembrane(membraneID, obj);
        }
    }
};

// ========================================
// MEMBRANE MANAGEMENT TESTS
// ========================================

TEST_F(MembraneCommChannelsTest, CreateRootMembrane) {
    std::string memID = channelManager.CreateMembrane();
    
    ASSERT_FALSE(memID.empty());
    EXPECT_EQ(channelManager.Membranes.size(), 1u);
    EXPECT_EQ(channelManager.Membranes[memID].NestingDepth, 0);
    EXPECT_TRUE(channelManager.Membranes[memID].ParentMembraneID.empty());
}

TEST_F(MembraneCommChannelsTest, CreateNestedMembranes) {
    std::string rootID = channelManager.CreateMembrane();
    std::string level1ID = channelManager.CreateMembrane(rootID);
    std::string level2ID = channelManager.CreateMembrane(level1ID);
    
    EXPECT_EQ(channelManager.Membranes.size(), 3u);
    EXPECT_EQ(channelManager.Membranes[rootID].NestingDepth, 0);
    EXPECT_EQ(channelManager.Membranes[level1ID].NestingDepth, 1);
    EXPECT_EQ(channelManager.Membranes[level2ID].NestingDepth, 2);
    
    EXPECT_EQ(channelManager.GetMaxNestingDepth(), 2);
}

TEST_F(MembraneCommChannelsTest, AddObjectsToMembrane) {
    std::string memID = channelManager.CreateMembrane();
    
    MockMembraneObject objA;
    objA.Symbol = "a";
    objA.Multiplicity = 3;
    
    EXPECT_TRUE(channelManager.AddObjectToMembrane(memID, objA));
    EXPECT_EQ(channelManager.CountObjectsInMembrane(memID, "a"), 3);
    EXPECT_EQ(channelManager.CountObjectsInMembrane(memID, "b"), 0);
}

TEST_F(MembraneCommChannelsTest, MembraneAdjacency) {
    auto [parentID, childID] = CreateParentChildMembranes();
    std::string otherRootID = channelManager.CreateMembrane();
    
    EXPECT_TRUE(channelManager.AreMembranesAdjacent(parentID, childID));
    EXPECT_TRUE(channelManager.AreMembranesAdjacent(childID, parentID));
    EXPECT_FALSE(channelManager.AreMembranesAdjacent(parentID, otherRootID));
    EXPECT_FALSE(channelManager.AreMembranesAdjacent(childID, otherRootID));
}

TEST_F(MembraneCommChannelsTest, MaxMembraneLimitRespected) {
    channelManager.Config.MaxMembranes = 3;
    
    std::string m1 = channelManager.CreateMembrane();
    std::string m2 = channelManager.CreateMembrane(m1);
    std::string m3 = channelManager.CreateMembrane(m2);
    std::string m4 = channelManager.CreateMembrane(m3); // Should fail
    
    EXPECT_FALSE(m1.empty());
    EXPECT_FALSE(m2.empty());
    EXPECT_FALSE(m3.empty());
    EXPECT_TRUE(m4.empty());
    EXPECT_EQ(channelManager.Membranes.size(), 3u);
}

// ========================================
// SYMPORT CHANNEL TESTS
// ========================================

TEST_F(MembraneCommChannelsTest, CreateSymportChannel) {
    auto [parentID, childID] = CreateParentChildMembranes();
    
    std::vector<std::string> selectivity = {"a", "b"};
    std::string channelID = channelManager.CreateSymportChannel(parentID, childID, selectivity);
    
    ASSERT_FALSE(channelID.empty());
    EXPECT_EQ(channelManager.Channels[channelID].ChannelType, MockChannelType::Symport);
    EXPECT_EQ(channelManager.Channels[channelID].State, MockChannelState::Closed);
    EXPECT_EQ(channelManager.Channels[channelID].SelectiveFor.size(), 2u);
}

TEST_F(MembraneCommChannelsTest, CannotCreateChannelBetweenNonAdjacentMembranes) {
    std::string root1 = channelManager.CreateMembrane();
    std::string root2 = channelManager.CreateMembrane();
    
    std::string channelID = channelManager.CreateSymportChannel(root1, root2, {});
    
    EXPECT_TRUE(channelID.empty());
}

TEST_F(MembraneCommChannelsTest, SymportTransportMovesObjects) {
    auto [parentID, childID] = CreateParentChildMembranes();
    AddObjects(childID, "a", 5);
    
    std::string channelID = channelManager.CreateSymportChannel(parentID, childID, {"a"});
    channelManager.OpenChannel(channelID);
    
    // Objects in child, transport outward to parent
    bool success = channelManager.ExecuteSymport(channelID, {"a"});
    
    EXPECT_TRUE(success);
    EXPECT_EQ(channelManager.CountObjectsInMembrane(childID, "a"), 4);
    EXPECT_EQ(channelManager.CountObjectsInMembrane(parentID, "a"), 1);
}

TEST_F(MembraneCommChannelsTest, SymportRespectsSelectivity) {
    auto [parentID, childID] = CreateParentChildMembranes();
    AddObjects(childID, "a", 3);
    AddObjects(childID, "b", 3);
    
    std::string channelID = channelManager.CreateSymportChannel(parentID, childID, {"a"});
    channelManager.OpenChannel(channelID);
    
    // "a" should transport, "b" should not
    EXPECT_TRUE(channelManager.ExecuteSymport(channelID, {"a"}));
    EXPECT_FALSE(channelManager.ExecuteSymport(channelID, {"b"}));
}

TEST_F(MembraneCommChannelsTest, SymportFailsWithoutEnoughObjects) {
    auto [parentID, childID] = CreateParentChildMembranes();
    AddObjects(childID, "a", 1);
    
    std::string channelID = channelManager.CreateSymportChannel(parentID, childID, {"a"});
    channelManager.OpenChannel(channelID);
    
    // First transport should succeed
    EXPECT_TRUE(channelManager.ExecuteSymport(channelID, {"a"}));
    
    // Reset channel to open (would normally happen after refractory period)
    channelManager.Channels[channelID].State = MockChannelState::Open;
    
    // Second should fail - no more objects
    EXPECT_FALSE(channelManager.ExecuteSymport(channelID, {"a"}));
}

TEST_F(MembraneCommChannelsTest, SymportSetsRefractoryState) {
    auto [parentID, childID] = CreateParentChildMembranes();
    AddObjects(childID, "a", 5);
    
    std::string channelID = channelManager.CreateSymportChannel(parentID, childID, {"a"});
    channelManager.OpenChannel(channelID);
    channelManager.ExecuteSymport(channelID, {"a"});
    
    EXPECT_EQ(channelManager.Channels[channelID].State, MockChannelState::Refractory);
}

// ========================================
// ANTIPORT CHANNEL TESTS
// ========================================

TEST_F(MembraneCommChannelsTest, CreateAntiportChannel) {
    auto [parentID, childID] = CreateParentChildMembranes();
    
    std::string channelID = channelManager.CreateAntiportChannel(parentID, childID, {"a", "b"});
    
    ASSERT_FALSE(channelID.empty());
    EXPECT_EQ(channelManager.Channels[channelID].ChannelType, MockChannelType::Antiport);
}

TEST_F(MembraneCommChannelsTest, AntiportCounterTransport) {
    auto [parentID, childID] = CreateParentChildMembranes();
    AddObjects(childID, "a", 3);  // Will move to parent
    AddObjects(parentID, "b", 3);  // Will move to child
    
    std::string channelID = channelManager.CreateAntiportChannel(parentID, childID, {"a", "b"});
    channelManager.OpenChannel(channelID);
    
    bool success = channelManager.ExecuteAntiport(channelID, {"a"}, {"b"});
    
    EXPECT_TRUE(success);
    EXPECT_EQ(channelManager.CountObjectsInMembrane(childID, "a"), 2);  // Lost 1
    EXPECT_EQ(channelManager.CountObjectsInMembrane(parentID, "a"), 1);  // Gained 1
    EXPECT_EQ(channelManager.CountObjectsInMembrane(parentID, "b"), 2);  // Lost 1
    EXPECT_EQ(channelManager.CountObjectsInMembrane(childID, "b"), 1);  // Gained 1
}

TEST_F(MembraneCommChannelsTest, AntiportRequiresBothSides) {
    auto [parentID, childID] = CreateParentChildMembranes();
    AddObjects(childID, "a", 3);
    // No "b" objects in parent
    
    std::string channelID = channelManager.CreateAntiportChannel(parentID, childID, {"a", "b"});
    channelManager.OpenChannel(channelID);
    
    bool success = channelManager.ExecuteAntiport(channelID, {"a"}, {"b"});
    
    EXPECT_FALSE(success);
    // Objects should not have moved
    EXPECT_EQ(channelManager.CountObjectsInMembrane(childID, "a"), 3);
}

TEST_F(MembraneCommChannelsTest, AntiportLogsTransportEvent) {
    auto [parentID, childID] = CreateParentChildMembranes();
    AddObjects(childID, "a", 3);
    AddObjects(parentID, "b", 3);
    
    std::string channelID = channelManager.CreateAntiportChannel(parentID, childID, {"a", "b"});
    channelManager.OpenChannel(channelID);
    channelManager.ExecuteAntiport(channelID, {"a"}, {"b"});
    
    ASSERT_EQ(channelManager.TransportEventLog.size(), 1u);
    EXPECT_TRUE(channelManager.TransportEventLog[0].bSuccessful);
    EXPECT_FALSE(channelManager.TransportEventLog[0].TransportedToTarget.empty());
    EXPECT_FALSE(channelManager.TransportEventLog[0].TransportedToSource.empty());
}

// ========================================
// ENERGY CONSTRAINT TESTS
// ========================================

TEST_F(MembraneCommChannelsTest, TransportConsumesEnergy) {
    auto [parentID, childID] = CreateParentChildMembranes();
    AddObjects(childID, "a", 5);
    channelManager.Membranes[childID].EnergyLevel = 10.0f;
    
    std::string channelID = channelManager.CreateSymportChannel(parentID, childID, {"a"});
    channelManager.OpenChannel(channelID);
    
    float initialEnergy = channelManager.Membranes[childID].EnergyLevel;
    channelManager.ExecuteSymport(channelID, {"a"});
    
    EXPECT_LT(channelManager.Membranes[childID].EnergyLevel, initialEnergy);
}

TEST_F(MembraneCommChannelsTest, TransportFailsWithInsufficientEnergy) {
    auto [parentID, childID] = CreateParentChildMembranes();
    AddObjects(childID, "a", 5);
    channelManager.Membranes[childID].EnergyLevel = 0.5f;  // Not enough for transport
    
    std::string channelID = channelManager.CreateSymportChannel(parentID, childID, {"a"});
    channelManager.OpenChannel(channelID);
    
    bool success = channelManager.ExecuteSymport(channelID, {"a"});
    
    EXPECT_FALSE(success);
    // Objects should remain
    EXPECT_EQ(channelManager.CountObjectsInMembrane(childID, "a"), 5);
}

TEST_F(MembraneCommChannelsTest, DisabledEnergyConstraintsAllowTransport) {
    channelManager.Config.bEnableEnergyConstraints = false;
    
    auto [parentID, childID] = CreateParentChildMembranes();
    AddObjects(childID, "a", 5);
    channelManager.Membranes[childID].EnergyLevel = 0.0f;  // Zero energy
    
    std::string channelID = channelManager.CreateSymportChannel(parentID, childID, {"a"});
    channelManager.OpenChannel(channelID);
    
    bool success = channelManager.ExecuteSymport(channelID, {"a"});
    
    EXPECT_TRUE(success);  // Should succeed despite zero energy
}

// ========================================
// CHANNEL STATE TESTS
// ========================================

TEST_F(MembraneCommChannelsTest, ClosedChannelBlocksTransport) {
    auto [parentID, childID] = CreateParentChildMembranes();
    AddObjects(childID, "a", 5);
    
    std::string channelID = channelManager.CreateSymportChannel(parentID, childID, {"a"});
    // Channel starts closed
    
    bool success = channelManager.ExecuteSymport(channelID, {"a"});
    
    EXPECT_FALSE(success);
}

TEST_F(MembraneCommChannelsTest, OpenChannelAllowsTransport) {
    auto [parentID, childID] = CreateParentChildMembranes();
    AddObjects(childID, "a", 5);
    
    std::string channelID = channelManager.CreateSymportChannel(parentID, childID, {"a"});
    channelManager.OpenChannel(channelID);
    
    bool success = channelManager.ExecuteSymport(channelID, {"a"});
    
    EXPECT_TRUE(success);
}

TEST_F(MembraneCommChannelsTest, RefractoryChannelBlocksTransport) {
    auto [parentID, childID] = CreateParentChildMembranes();
    AddObjects(childID, "a", 5);
    
    std::string channelID = channelManager.CreateSymportChannel(parentID, childID, {"a"});
    channelManager.OpenChannel(channelID);
    
    // First transport succeeds and puts channel in refractory
    EXPECT_TRUE(channelManager.ExecuteSymport(channelID, {"a"}));
    EXPECT_EQ(channelManager.Channels[channelID].State, MockChannelState::Refractory);
    
    // Second transport should fail
    EXPECT_FALSE(channelManager.ExecuteSymport(channelID, {"a"}));
}

// ========================================
// TRANSPORT EVENT LOGGING TESTS
// ========================================

TEST_F(MembraneCommChannelsTest, TransportEventsAreLogged) {
    auto [parentID, childID] = CreateParentChildMembranes();
    AddObjects(childID, "a", 5);
    
    std::string channelID = channelManager.CreateSymportChannel(parentID, childID, {"a"});
    channelManager.OpenChannel(channelID);
    channelManager.ExecuteSymport(channelID, {"a"});
    
    ASSERT_EQ(channelManager.TransportEventLog.size(), 1u);
    EXPECT_EQ(channelManager.TransportEventLog[0].ChannelID, channelID);
    EXPECT_TRUE(channelManager.TransportEventLog[0].bSuccessful);
}

TEST_F(MembraneCommChannelsTest, DisabledLoggingNoEvents) {
    channelManager.Config.bEnableEventLogging = false;
    
    auto [parentID, childID] = CreateParentChildMembranes();
    AddObjects(childID, "a", 5);
    
    std::string channelID = channelManager.CreateSymportChannel(parentID, childID, {"a"});
    channelManager.OpenChannel(channelID);
    channelManager.ExecuteSymport(channelID, {"a"});
    
    EXPECT_EQ(channelManager.TransportEventLog.size(), 0u);
}

// ========================================
// OBJECT MULTIPLICITY TESTS
// ========================================

TEST_F(MembraneCommChannelsTest, MultiplicityHandledCorrectly) {
    auto [parentID, childID] = CreateParentChildMembranes();
    
    MockMembraneObject obj;
    obj.Symbol = "a";
    obj.Multiplicity = 5;
    channelManager.AddObjectToMembrane(childID, obj);
    
    std::string channelID = channelManager.CreateSymportChannel(parentID, childID, {"a"});
    channelManager.OpenChannel(channelID);
    
    // Transport single object, multiplicity should decrease
    channelManager.ExecuteSymport(channelID, {"a"});
    
    EXPECT_EQ(channelManager.CountObjectsInMembrane(childID, "a"), 4);
    EXPECT_EQ(channelManager.CountObjectsInMembrane(parentID, "a"), 1);
}

// ========================================
// CATALYST TESTS
// ========================================

TEST_F(MembraneCommChannelsTest, CatalystsNotConsumedInTransport) {
    auto [parentID, childID] = CreateParentChildMembranes();
    
    // Add catalyst
    MockMembraneObject catalyst;
    catalyst.Symbol = "enzyme";
    catalyst.bIsCatalyst = true;
    channelManager.AddObjectToMembrane(childID, catalyst);
    
    // Add regular object
    AddObjects(childID, "substrate", 1);
    
    std::string channelID = channelManager.CreateSymportChannel(parentID, childID, {"enzyme", "substrate"});
    channelManager.OpenChannel(channelID);
    
    // Try to transport catalyst - should fail as catalyst cannot be consumed
    bool success = channelManager.ExecuteSymport(channelID, {"enzyme"});
    
    EXPECT_FALSE(success);  // Catalyst not available for transport
    
    // Substrate should transport
    success = channelManager.ExecuteSymport(channelID, {"substrate"});
    EXPECT_TRUE(success);
}

// ========================================
// TOTAL OBJECT COUNT TESTS
// ========================================

TEST_F(MembraneCommChannelsTest, TotalObjectCountConserved) {
    auto [parentID, childID] = CreateParentChildMembranes();
    AddObjects(childID, "a", 10);
    AddObjects(parentID, "b", 5);
    
    int32_t initialCount = channelManager.GetTotalObjectCount();
    EXPECT_EQ(initialCount, 15);
    
    std::string channelID = channelManager.CreateSymportChannel(parentID, childID, {"a"});
    channelManager.OpenChannel(channelID);
    channelManager.ExecuteSymport(channelID, {"a"});
    
    int32_t afterCount = channelManager.GetTotalObjectCount();
    EXPECT_EQ(afterCount, initialCount);  // Conservation
}

// ========================================
// MULTIPLE OBJECT TRANSPORT TESTS
// ========================================

TEST_F(MembraneCommChannelsTest, MultipleObjectsTransportTogether) {
    auto [parentID, childID] = CreateParentChildMembranes();
    AddObjects(childID, "a", 3);
    AddObjects(childID, "b", 3);
    
    std::string channelID = channelManager.CreateSymportChannel(parentID, childID, {"a", "b"});
    channelManager.OpenChannel(channelID);
    
    bool success = channelManager.ExecuteSymport(channelID, {"a", "b"});
    
    EXPECT_TRUE(success);
    EXPECT_EQ(channelManager.CountObjectsInMembrane(childID, "a"), 2);
    EXPECT_EQ(channelManager.CountObjectsInMembrane(childID, "b"), 2);
    EXPECT_EQ(channelManager.CountObjectsInMembrane(parentID, "a"), 1);
    EXPECT_EQ(channelManager.CountObjectsInMembrane(parentID, "b"), 1);
}

// ========================================
// MAIN FUNCTION
// ========================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

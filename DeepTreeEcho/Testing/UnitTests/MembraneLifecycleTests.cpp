/**
 * @file MembraneLifecycleTests.cpp
 * @brief Unit tests for Feature F1.3.3: Membrane Division/Dissolution
 * 
 * Tests cover:
 * - Membrane creation (CreateMembrane)
 * - Membrane division (DivideMembrane)
 * - Membrane dissolution (DissolveMembrane)
 * - Lifecycle validation (CanDivideMembrane, CanDissolveMembrane)
 * - Parent-child relationships (GetParentMembrane)
 * - Content propagation during lifecycle events
 * 
 * @author Deep Tree Echo Team
 * @date March 2026
 */

#include <gtest/gtest.h>
#include <memory>
#include <map>
#include <vector>
#include <string>
#include <algorithm>

namespace {

// Mock membrane structures for standalone testing (simulates UE environment)
enum class MockMembraneType : uint8_t
{
    Root = 0,
    Cognitive = 1,
    Memory = 2,
    Reasoning = 3,
    Grammar = 4,
    Extension = 5,
    Security = 6,
    Custom1 = 7,
    Custom2 = 8,
    Custom3 = 9
};

enum class MockMembraneLifecycleEvent : uint8_t
{
    Created,
    Divided,
    Dissolved,
    Merged
};

struct MockMembraneState
{
    MockMembraneType Type = MockMembraneType::Root;
    float Permeability = 0.5f;
    float Coherence = 1.0f;
    std::vector<int32_t> ChildMembranes;
    std::map<std::string, float> Contents;
};

struct MockMembraneDivisionResult
{
    bool bSuccess = false;
    MockMembraneType ParentType = MockMembraneType::Root;
    std::vector<MockMembraneType> ChildTypes;
    std::string ErrorMessage;
    float DivisionRatio = 0.5f;
};

struct MockMembraneDissolutionResult
{
    bool bSuccess = false;
    MockMembraneType DissolvedType = MockMembraneType::Root;
    MockMembraneType ReceiverType = MockMembraneType::Root;
    std::map<std::string, float> PropagatedContents;
    std::string ErrorMessage;
};

// Mock lifecycle event callback
struct MockLifecycleEventRecord
{
    MockMembraneLifecycleEvent EventType;
    MockMembraneType MembraneType;
    std::string Details;
};

/**
 * @brief Mock implementation of membrane lifecycle system
 */
class MockMembraneLifecycleSystem
{
public:
    std::map<MockMembraneType, MockMembraneState> Membranes;
    std::vector<MockLifecycleEventRecord> LifecycleEvents;

    MockMembraneLifecycleSystem()
    {
        InitializeMembranes();
    }

    void InitializeMembranes()
    {
        // Root membrane
        MockMembraneState RootMembrane;
        RootMembrane.Type = MockMembraneType::Root;
        RootMembrane.Permeability = 0.3f;
        RootMembrane.Coherence = 1.0f;
        RootMembrane.ChildMembranes = {1, 5, 6}; // Cognitive, Extension, Security
        Membranes[MockMembraneType::Root] = RootMembrane;

        // Cognitive membrane
        MockMembraneState CognitiveMembrane;
        CognitiveMembrane.Type = MockMembraneType::Cognitive;
        CognitiveMembrane.Permeability = 0.5f;
        CognitiveMembrane.Coherence = 1.0f;
        CognitiveMembrane.ChildMembranes = {2, 3, 4}; // Memory, Reasoning, Grammar
        Membranes[MockMembraneType::Cognitive] = CognitiveMembrane;

        // Memory membrane
        MockMembraneState MemoryMembrane;
        MemoryMembrane.Type = MockMembraneType::Memory;
        MemoryMembrane.Permeability = 0.7f;
        MemoryMembrane.Coherence = 1.0f;
        Membranes[MockMembraneType::Memory] = MemoryMembrane;

        // Reasoning membrane
        MockMembraneState ReasoningMembrane;
        ReasoningMembrane.Type = MockMembraneType::Reasoning;
        ReasoningMembrane.Permeability = 0.6f;
        ReasoningMembrane.Coherence = 1.0f;
        Membranes[MockMembraneType::Reasoning] = ReasoningMembrane;

        // Grammar membrane
        MockMembraneState GrammarMembrane;
        GrammarMembrane.Type = MockMembraneType::Grammar;
        GrammarMembrane.Permeability = 0.5f;
        GrammarMembrane.Coherence = 1.0f;
        Membranes[MockMembraneType::Grammar] = GrammarMembrane;

        // Extension membrane
        MockMembraneState ExtensionMembrane;
        ExtensionMembrane.Type = MockMembraneType::Extension;
        ExtensionMembrane.Permeability = 0.8f;
        ExtensionMembrane.Coherence = 1.0f;
        Membranes[MockMembraneType::Extension] = ExtensionMembrane;

        // Security membrane
        MockMembraneState SecurityMembrane;
        SecurityMembrane.Type = MockMembraneType::Security;
        SecurityMembrane.Permeability = 0.2f;
        SecurityMembrane.Coherence = 1.0f;
        Membranes[MockMembraneType::Security] = SecurityMembrane;
    }

    bool CreateMembrane(MockMembraneType NewType, MockMembraneType ParentType, 
                        float InitialPermeability = 0.5f, float InitialCoherence = 1.0f)
    {
        // Check if membrane already exists
        if (Membranes.find(NewType) != Membranes.end())
        {
            return false;
        }

        // Check if parent exists
        if (Membranes.find(ParentType) == Membranes.end())
        {
            return false;
        }

        // Create the new membrane
        MockMembraneState NewMembrane;
        NewMembrane.Type = NewType;
        NewMembrane.Permeability = std::clamp(InitialPermeability, 0.0f, 1.0f);
        NewMembrane.Coherence = std::clamp(InitialCoherence, 0.0f, 1.0f);
        Membranes[NewType] = NewMembrane;

        // Add as child to parent
        AddChildToParent(NewType, ParentType);

        // Record lifecycle event
        LifecycleEvents.push_back({
            MockMembraneLifecycleEvent::Created,
            NewType,
            "Created with permeability " + std::to_string(InitialPermeability)
        });

        return true;
    }

    MockMembraneDivisionResult DivideMembrane(MockMembraneType MembraneType, float DivisionRatio,
                                               MockMembraneType ChildType1, MockMembraneType ChildType2)
    {
        MockMembraneDivisionResult Result;
        Result.ParentType = MembraneType;
        Result.DivisionRatio = std::clamp(DivisionRatio, 0.0f, 1.0f);

        // Validate preconditions
        if (!CanDivideMembrane(MembraneType, ChildType1, ChildType2))
        {
            Result.bSuccess = false;
            Result.ErrorMessage = "Division preconditions not met";
            return Result;
        }

        auto ParentIt = Membranes.find(MembraneType);
        if (ParentIt == Membranes.end())
        {
            Result.bSuccess = false;
            Result.ErrorMessage = "Parent membrane not found";
            return Result;
        }

        MockMembraneState& ParentMembrane = ParentIt->second;

        // Create first child membrane
        MockMembraneState Child1;
        Child1.Type = ChildType1;
        Child1.Permeability = ParentMembrane.Permeability;
        Child1.Coherence = ParentMembrane.Coherence * 0.9f;

        // Create second child membrane
        MockMembraneState Child2;
        Child2.Type = ChildType2;
        Child2.Permeability = ParentMembrane.Permeability;
        Child2.Coherence = ParentMembrane.Coherence * 0.9f;

        // Distribute contents based on division ratio
        for (const auto& ContentPair : ParentMembrane.Contents)
        {
            float Value1 = ContentPair.second * Result.DivisionRatio;
            float Value2 = ContentPair.second * (1.0f - Result.DivisionRatio);

            if (Value1 > 0.01f)
            {
                Child1.Contents[ContentPair.first] = Value1;
            }
            if (Value2 > 0.01f)
            {
                Child2.Contents[ContentPair.first] = Value2;
            }
        }

        // Clear parent contents after distribution
        ParentMembrane.Contents.clear();

        // Add child membranes
        Membranes[ChildType1] = Child1;
        Membranes[ChildType2] = Child2;

        // Update parent's child list
        ParentMembrane.ChildMembranes.push_back(static_cast<int32_t>(ChildType1));
        ParentMembrane.ChildMembranes.push_back(static_cast<int32_t>(ChildType2));

        // Update result
        Result.bSuccess = true;
        Result.ChildTypes.push_back(ChildType1);
        Result.ChildTypes.push_back(ChildType2);

        // Record lifecycle event
        LifecycleEvents.push_back({
            MockMembraneLifecycleEvent::Divided,
            MembraneType,
            "Divided with ratio " + std::to_string(Result.DivisionRatio)
        });

        return Result;
    }

    MockMembraneDissolutionResult DissolveMembrane(MockMembraneType MembraneType)
    {
        MockMembraneDissolutionResult Result;
        Result.DissolvedType = MembraneType;

        // Cannot dissolve root membrane
        if (MembraneType == MockMembraneType::Root)
        {
            Result.bSuccess = false;
            Result.ErrorMessage = "Cannot dissolve root membrane";
            return Result;
        }

        // Find the membrane to dissolve
        auto DissolvedIt = Membranes.find(MembraneType);
        if (DissolvedIt == Membranes.end())
        {
            Result.bSuccess = false;
            Result.ErrorMessage = "Membrane not found";
            return Result;
        }

        MockMembraneState& DissolvedMembrane = DissolvedIt->second;

        // Find parent membrane
        MockMembraneType ParentType;
        if (!FindParentMembrane(MembraneType, ParentType))
        {
            Result.bSuccess = false;
            Result.ErrorMessage = "Parent membrane not found";
            return Result;
        }

        auto ParentIt = Membranes.find(ParentType);
        if (ParentIt == Membranes.end())
        {
            Result.bSuccess = false;
            Result.ErrorMessage = "Parent membrane state not accessible";
            return Result;
        }

        MockMembraneState& ParentMembrane = ParentIt->second;
        Result.ReceiverType = ParentType;

        // Propagate contents to parent with permeability attenuation
        float PropagationFactor = DissolvedMembrane.Permeability * ParentMembrane.Permeability;
        for (const auto& ContentPair : DissolvedMembrane.Contents)
        {
            float PropagatedValue = ContentPair.second * PropagationFactor;
            if (PropagatedValue > 0.01f)
            {
                Result.PropagatedContents[ContentPair.first] = PropagatedValue;

                // Add to parent or update existing
                auto ExistingIt = ParentMembrane.Contents.find(ContentPair.first);
                if (ExistingIt != ParentMembrane.Contents.end())
                {
                    ExistingIt->second += PropagatedValue;
                }
                else
                {
                    ParentMembrane.Contents[ContentPair.first] = PropagatedValue;
                }
            }
        }

        // Re-parent child membranes to the dissolved membrane's parent
        for (int32_t ChildIndex : DissolvedMembrane.ChildMembranes)
        {
            auto ChildIt = std::find(ParentMembrane.ChildMembranes.begin(), 
                                     ParentMembrane.ChildMembranes.end(), ChildIndex);
            if (ChildIt == ParentMembrane.ChildMembranes.end())
            {
                ParentMembrane.ChildMembranes.push_back(ChildIndex);
            }
        }

        // Remove dissolved membrane from parent's child list
        RemoveChildFromParent(MembraneType, ParentType);

        // Remove the membrane from the map
        Membranes.erase(MembraneType);

        Result.bSuccess = true;

        // Record lifecycle event
        LifecycleEvents.push_back({
            MockMembraneLifecycleEvent::Dissolved,
            MembraneType,
            "Dissolved into parent " + std::to_string(static_cast<int32_t>(ParentType))
        });

        return Result;
    }

    bool CanDissolveMembrane(MockMembraneType MembraneType) const
    {
        if (MembraneType == MockMembraneType::Root)
        {
            return false;
        }

        if (Membranes.find(MembraneType) == Membranes.end())
        {
            return false;
        }

        MockMembraneType ParentType;
        return FindParentMembrane(MembraneType, ParentType);
    }

    bool CanDivideMembrane(MockMembraneType MembraneType, 
                           MockMembraneType ChildType1, MockMembraneType ChildType2) const
    {
        if (Membranes.find(MembraneType) == Membranes.end())
        {
            return false;
        }

        if (ChildType1 == ChildType2)
        {
            return false;
        }

        if (Membranes.find(ChildType1) != Membranes.end() || 
            Membranes.find(ChildType2) != Membranes.end())
        {
            return false;
        }

        auto ParentIt = Membranes.find(MembraneType);
        if (ParentIt != Membranes.end() && ParentIt->second.Coherence < 0.1f)
        {
            return false;
        }

        return true;
    }

    bool GetParentMembrane(MockMembraneType MembraneType, MockMembraneType& OutParentType) const
    {
        return FindParentMembrane(MembraneType, OutParentType);
    }

    bool FindParentMembrane(MockMembraneType ChildType, MockMembraneType& OutParentType) const
    {
        int32_t ChildIndex = static_cast<int32_t>(ChildType);

        for (const auto& MembranePair : Membranes)
        {
            const auto& ChildList = MembranePair.second.ChildMembranes;
            if (std::find(ChildList.begin(), ChildList.end(), ChildIndex) != ChildList.end())
            {
                OutParentType = MembranePair.first;
                return true;
            }
        }

        return false;
    }

    void RemoveChildFromParent(MockMembraneType ChildType, MockMembraneType ParentType)
    {
        auto ParentIt = Membranes.find(ParentType);
        if (ParentIt != Membranes.end())
        {
            int32_t ChildIndex = static_cast<int32_t>(ChildType);
            auto& ChildList = ParentIt->second.ChildMembranes;
            ChildList.erase(std::remove(ChildList.begin(), ChildList.end(), ChildIndex), ChildList.end());
        }
    }

    void AddChildToParent(MockMembraneType ChildType, MockMembraneType ParentType)
    {
        auto ParentIt = Membranes.find(ParentType);
        if (ParentIt != Membranes.end())
        {
            int32_t ChildIndex = static_cast<int32_t>(ChildType);
            auto& ChildList = ParentIt->second.ChildMembranes;
            if (std::find(ChildList.begin(), ChildList.end(), ChildIndex) == ChildList.end())
            {
                ChildList.push_back(ChildIndex);
            }
        }
    }
};

// ============================================================================
// Test Fixtures
// ============================================================================

class MembraneLifecycleTest : public ::testing::Test
{
protected:
    std::unique_ptr<MockMembraneLifecycleSystem> system;

    void SetUp() override
    {
        system = std::make_unique<MockMembraneLifecycleSystem>();
    }

    void TearDown() override
    {
        system.reset();
    }
};

// ============================================================================
// Membrane Creation Tests
// ============================================================================

TEST_F(MembraneLifecycleTest, CreateMembrane_Success)
{
    bool result = system->CreateMembrane(MockMembraneType::Custom1, MockMembraneType::Root, 0.6f, 0.9f);
    
    EXPECT_TRUE(result);
    EXPECT_TRUE(system->Membranes.find(MockMembraneType::Custom1) != system->Membranes.end());
    
    auto& newMembrane = system->Membranes[MockMembraneType::Custom1];
    EXPECT_FLOAT_EQ(newMembrane.Permeability, 0.6f);
    EXPECT_FLOAT_EQ(newMembrane.Coherence, 0.9f);
}

TEST_F(MembraneLifecycleTest, CreateMembrane_AddsToParentChildList)
{
    system->CreateMembrane(MockMembraneType::Custom1, MockMembraneType::Root, 0.5f, 1.0f);
    
    auto& rootMembrane = system->Membranes[MockMembraneType::Root];
    int32_t customIndex = static_cast<int32_t>(MockMembraneType::Custom1);
    
    EXPECT_TRUE(std::find(rootMembrane.ChildMembranes.begin(), 
                          rootMembrane.ChildMembranes.end(), 
                          customIndex) != rootMembrane.ChildMembranes.end());
}

TEST_F(MembraneLifecycleTest, CreateMembrane_FailsIfAlreadyExists)
{
    // Memory membrane already exists from initialization
    bool result = system->CreateMembrane(MockMembraneType::Memory, MockMembraneType::Root, 0.5f, 1.0f);
    
    EXPECT_FALSE(result);
}

TEST_F(MembraneLifecycleTest, CreateMembrane_FailsIfParentDoesNotExist)
{
    // Custom3 doesn't exist yet, so it can't be a parent
    bool result = system->CreateMembrane(MockMembraneType::Custom1, MockMembraneType::Custom3, 0.5f, 1.0f);
    
    EXPECT_FALSE(result);
}

TEST_F(MembraneLifecycleTest, CreateMembrane_ClampsPermeability)
{
    system->CreateMembrane(MockMembraneType::Custom1, MockMembraneType::Root, 1.5f, 1.0f);
    
    auto& newMembrane = system->Membranes[MockMembraneType::Custom1];
    EXPECT_FLOAT_EQ(newMembrane.Permeability, 1.0f);
}

TEST_F(MembraneLifecycleTest, CreateMembrane_RecordsLifecycleEvent)
{
    system->CreateMembrane(MockMembraneType::Custom1, MockMembraneType::Root, 0.5f, 1.0f);
    
    EXPECT_EQ(system->LifecycleEvents.size(), 1u);
    EXPECT_EQ(system->LifecycleEvents[0].EventType, MockMembraneLifecycleEvent::Created);
    EXPECT_EQ(system->LifecycleEvents[0].MembraneType, MockMembraneType::Custom1);
}

// ============================================================================
// Membrane Division Tests
// ============================================================================

TEST_F(MembraneLifecycleTest, DivideMembrane_Success)
{
    auto result = system->DivideMembrane(MockMembraneType::Memory, 0.5f, 
                                         MockMembraneType::Custom1, MockMembraneType::Custom2);
    
    EXPECT_TRUE(result.bSuccess);
    EXPECT_EQ(result.ChildTypes.size(), 2u);
    EXPECT_EQ(result.ChildTypes[0], MockMembraneType::Custom1);
    EXPECT_EQ(result.ChildTypes[1], MockMembraneType::Custom2);
}

TEST_F(MembraneLifecycleTest, DivideMembrane_CreatesChildMembranes)
{
    system->DivideMembrane(MockMembraneType::Memory, 0.5f, 
                           MockMembraneType::Custom1, MockMembraneType::Custom2);
    
    EXPECT_TRUE(system->Membranes.find(MockMembraneType::Custom1) != system->Membranes.end());
    EXPECT_TRUE(system->Membranes.find(MockMembraneType::Custom2) != system->Membranes.end());
}

TEST_F(MembraneLifecycleTest, DivideMembrane_InheritsPermeability)
{
    float originalPermeability = system->Membranes[MockMembraneType::Memory].Permeability;
    
    system->DivideMembrane(MockMembraneType::Memory, 0.5f, 
                           MockMembraneType::Custom1, MockMembraneType::Custom2);
    
    EXPECT_FLOAT_EQ(system->Membranes[MockMembraneType::Custom1].Permeability, originalPermeability);
    EXPECT_FLOAT_EQ(system->Membranes[MockMembraneType::Custom2].Permeability, originalPermeability);
}

TEST_F(MembraneLifecycleTest, DivideMembrane_ReducesCoherence)
{
    float originalCoherence = system->Membranes[MockMembraneType::Memory].Coherence;
    
    system->DivideMembrane(MockMembraneType::Memory, 0.5f, 
                           MockMembraneType::Custom1, MockMembraneType::Custom2);
    
    EXPECT_NEAR(system->Membranes[MockMembraneType::Custom1].Coherence, originalCoherence * 0.9f, 0.01f);
    EXPECT_NEAR(system->Membranes[MockMembraneType::Custom2].Coherence, originalCoherence * 0.9f, 0.01f);
}

TEST_F(MembraneLifecycleTest, DivideMembrane_DistributesContentsEqually)
{
    // Add content to membrane before division
    system->Membranes[MockMembraneType::Memory].Contents["TestContent"] = 100.0f;
    
    system->DivideMembrane(MockMembraneType::Memory, 0.5f, 
                           MockMembraneType::Custom1, MockMembraneType::Custom2);
    
    EXPECT_FLOAT_EQ(system->Membranes[MockMembraneType::Custom1].Contents["TestContent"], 50.0f);
    EXPECT_FLOAT_EQ(system->Membranes[MockMembraneType::Custom2].Contents["TestContent"], 50.0f);
}

TEST_F(MembraneLifecycleTest, DivideMembrane_DistributesContentsWithRatio)
{
    system->Membranes[MockMembraneType::Memory].Contents["TestContent"] = 100.0f;
    
    system->DivideMembrane(MockMembraneType::Memory, 0.7f, 
                           MockMembraneType::Custom1, MockMembraneType::Custom2);
    
    EXPECT_FLOAT_EQ(system->Membranes[MockMembraneType::Custom1].Contents["TestContent"], 70.0f);
    EXPECT_FLOAT_EQ(system->Membranes[MockMembraneType::Custom2].Contents["TestContent"], 30.0f);
}

TEST_F(MembraneLifecycleTest, DivideMembrane_ClearsParentContents)
{
    system->Membranes[MockMembraneType::Memory].Contents["TestContent"] = 100.0f;
    
    system->DivideMembrane(MockMembraneType::Memory, 0.5f, 
                           MockMembraneType::Custom1, MockMembraneType::Custom2);
    
    EXPECT_TRUE(system->Membranes[MockMembraneType::Memory].Contents.empty());
}

TEST_F(MembraneLifecycleTest, DivideMembrane_AddsChildrenToParent)
{
    system->DivideMembrane(MockMembraneType::Memory, 0.5f, 
                           MockMembraneType::Custom1, MockMembraneType::Custom2);
    
    auto& parentChildren = system->Membranes[MockMembraneType::Memory].ChildMembranes;
    int32_t child1Index = static_cast<int32_t>(MockMembraneType::Custom1);
    int32_t child2Index = static_cast<int32_t>(MockMembraneType::Custom2);
    
    EXPECT_TRUE(std::find(parentChildren.begin(), parentChildren.end(), child1Index) != parentChildren.end());
    EXPECT_TRUE(std::find(parentChildren.begin(), parentChildren.end(), child2Index) != parentChildren.end());
}

TEST_F(MembraneLifecycleTest, DivideMembrane_FailsIfChildTypeExists)
{
    auto result = system->DivideMembrane(MockMembraneType::Cognitive, 0.5f, 
                                         MockMembraneType::Memory, MockMembraneType::Custom1);
    
    EXPECT_FALSE(result.bSuccess);
    EXPECT_EQ(result.ErrorMessage, "Division preconditions not met");
}

TEST_F(MembraneLifecycleTest, DivideMembrane_FailsIfSameChildTypes)
{
    auto result = system->DivideMembrane(MockMembraneType::Memory, 0.5f, 
                                         MockMembraneType::Custom1, MockMembraneType::Custom1);
    
    EXPECT_FALSE(result.bSuccess);
}

TEST_F(MembraneLifecycleTest, DivideMembrane_RecordsLifecycleEvent)
{
    system->DivideMembrane(MockMembraneType::Memory, 0.5f, 
                           MockMembraneType::Custom1, MockMembraneType::Custom2);
    
    EXPECT_EQ(system->LifecycleEvents.size(), 1u);
    EXPECT_EQ(system->LifecycleEvents[0].EventType, MockMembraneLifecycleEvent::Divided);
    EXPECT_EQ(system->LifecycleEvents[0].MembraneType, MockMembraneType::Memory);
}

// ============================================================================
// Membrane Dissolution Tests
// ============================================================================

TEST_F(MembraneLifecycleTest, DissolveMembrane_Success)
{
    auto result = system->DissolveMembrane(MockMembraneType::Grammar);
    
    EXPECT_TRUE(result.bSuccess);
    EXPECT_EQ(result.DissolvedType, MockMembraneType::Grammar);
    EXPECT_EQ(result.ReceiverType, MockMembraneType::Cognitive);
}

TEST_F(MembraneLifecycleTest, DissolveMembrane_RemovesFromMap)
{
    system->DissolveMembrane(MockMembraneType::Grammar);
    
    EXPECT_TRUE(system->Membranes.find(MockMembraneType::Grammar) == system->Membranes.end());
}

TEST_F(MembraneLifecycleTest, DissolveMembrane_PropagatesContentsToParent)
{
    system->Membranes[MockMembraneType::Grammar].Contents["TestContent"] = 100.0f;
    float grammarPermeability = system->Membranes[MockMembraneType::Grammar].Permeability;
    float cognitivePermeability = system->Membranes[MockMembraneType::Cognitive].Permeability;
    float expectedValue = 100.0f * grammarPermeability * cognitivePermeability;
    
    auto result = system->DissolveMembrane(MockMembraneType::Grammar);
    
    EXPECT_NEAR(system->Membranes[MockMembraneType::Cognitive].Contents["TestContent"], 
                expectedValue, 0.01f);
    EXPECT_NEAR(result.PropagatedContents["TestContent"], expectedValue, 0.01f);
}

TEST_F(MembraneLifecycleTest, DissolveMembrane_RemovesFromParentChildList)
{
    system->DissolveMembrane(MockMembraneType::Grammar);
    
    auto& parentChildren = system->Membranes[MockMembraneType::Cognitive].ChildMembranes;
    int32_t grammarIndex = static_cast<int32_t>(MockMembraneType::Grammar);
    
    EXPECT_TRUE(std::find(parentChildren.begin(), parentChildren.end(), grammarIndex) == parentChildren.end());
}

TEST_F(MembraneLifecycleTest, DissolveMembrane_ReparentsChildMembranes)
{
    // First create children of Memory
    system->DivideMembrane(MockMembraneType::Memory, 0.5f, 
                           MockMembraneType::Custom1, MockMembraneType::Custom2);
    
    // Now dissolve Memory
    system->DissolveMembrane(MockMembraneType::Memory);
    
    // Custom1 and Custom2 should now be children of Cognitive
    auto& cognitiveChildren = system->Membranes[MockMembraneType::Cognitive].ChildMembranes;
    int32_t custom1Index = static_cast<int32_t>(MockMembraneType::Custom1);
    int32_t custom2Index = static_cast<int32_t>(MockMembraneType::Custom2);
    
    EXPECT_TRUE(std::find(cognitiveChildren.begin(), cognitiveChildren.end(), custom1Index) != cognitiveChildren.end());
    EXPECT_TRUE(std::find(cognitiveChildren.begin(), cognitiveChildren.end(), custom2Index) != cognitiveChildren.end());
}

TEST_F(MembraneLifecycleTest, DissolveMembrane_FailsForRoot)
{
    auto result = system->DissolveMembrane(MockMembraneType::Root);
    
    EXPECT_FALSE(result.bSuccess);
    EXPECT_EQ(result.ErrorMessage, "Cannot dissolve root membrane");
}

TEST_F(MembraneLifecycleTest, DissolveMembrane_FailsIfNotFound)
{
    auto result = system->DissolveMembrane(MockMembraneType::Custom1);
    
    EXPECT_FALSE(result.bSuccess);
    EXPECT_EQ(result.ErrorMessage, "Membrane not found");
}

TEST_F(MembraneLifecycleTest, DissolveMembrane_RecordsLifecycleEvent)
{
    system->DissolveMembrane(MockMembraneType::Grammar);
    
    EXPECT_EQ(system->LifecycleEvents.size(), 1u);
    EXPECT_EQ(system->LifecycleEvents[0].EventType, MockMembraneLifecycleEvent::Dissolved);
    EXPECT_EQ(system->LifecycleEvents[0].MembraneType, MockMembraneType::Grammar);
}

// ============================================================================
// Validation Tests
// ============================================================================

TEST_F(MembraneLifecycleTest, CanDissolveMembrane_ReturnsTrueForValidMembrane)
{
    EXPECT_TRUE(system->CanDissolveMembrane(MockMembraneType::Memory));
    EXPECT_TRUE(system->CanDissolveMembrane(MockMembraneType::Cognitive));
}

TEST_F(MembraneLifecycleTest, CanDissolveMembrane_ReturnsFalseForRoot)
{
    EXPECT_FALSE(system->CanDissolveMembrane(MockMembraneType::Root));
}

TEST_F(MembraneLifecycleTest, CanDissolveMembrane_ReturnsFalseForNonexistent)
{
    EXPECT_FALSE(system->CanDissolveMembrane(MockMembraneType::Custom1));
}

TEST_F(MembraneLifecycleTest, CanDivideMembrane_ReturnsTrueForValidDivision)
{
    EXPECT_TRUE(system->CanDivideMembrane(MockMembraneType::Memory, 
                                          MockMembraneType::Custom1, MockMembraneType::Custom2));
}

TEST_F(MembraneLifecycleTest, CanDivideMembrane_ReturnsFalseIfChildExists)
{
    EXPECT_FALSE(system->CanDivideMembrane(MockMembraneType::Cognitive, 
                                           MockMembraneType::Memory, MockMembraneType::Custom1));
}

TEST_F(MembraneLifecycleTest, CanDivideMembrane_ReturnsFalseIfSameChildren)
{
    EXPECT_FALSE(system->CanDivideMembrane(MockMembraneType::Memory, 
                                           MockMembraneType::Custom1, MockMembraneType::Custom1));
}

TEST_F(MembraneLifecycleTest, CanDivideMembrane_ReturnsFalseIfLowCoherence)
{
    system->Membranes[MockMembraneType::Memory].Coherence = 0.05f;
    
    EXPECT_FALSE(system->CanDivideMembrane(MockMembraneType::Memory, 
                                           MockMembraneType::Custom1, MockMembraneType::Custom2));
}

// ============================================================================
// Parent-Child Relationship Tests
// ============================================================================

TEST_F(MembraneLifecycleTest, GetParentMembrane_ReturnsCorrectParent)
{
    MockMembraneType parentType;
    
    EXPECT_TRUE(system->GetParentMembrane(MockMembraneType::Memory, parentType));
    EXPECT_EQ(parentType, MockMembraneType::Cognitive);
    
    EXPECT_TRUE(system->GetParentMembrane(MockMembraneType::Cognitive, parentType));
    EXPECT_EQ(parentType, MockMembraneType::Root);
}

TEST_F(MembraneLifecycleTest, GetParentMembrane_ReturnsFalseForRoot)
{
    MockMembraneType parentType;
    
    EXPECT_FALSE(system->GetParentMembrane(MockMembraneType::Root, parentType));
}

TEST_F(MembraneLifecycleTest, GetParentMembrane_ReturnsFalseForNonexistent)
{
    MockMembraneType parentType;
    
    EXPECT_FALSE(system->GetParentMembrane(MockMembraneType::Custom1, parentType));
}

// ============================================================================
// Complex Lifecycle Scenario Tests
// ============================================================================

TEST_F(MembraneLifecycleTest, ComplexScenario_CreateDivideDissolve)
{
    // Create a new membrane
    EXPECT_TRUE(system->CreateMembrane(MockMembraneType::Custom1, MockMembraneType::Memory, 0.5f, 1.0f));
    
    // Add content to it
    system->Membranes[MockMembraneType::Custom1].Contents["Data"] = 50.0f;
    
    // Divide it
    auto divResult = system->DivideMembrane(MockMembraneType::Custom1, 0.6f, 
                                            MockMembraneType::Custom2, MockMembraneType::Custom3);
    EXPECT_TRUE(divResult.bSuccess);
    
    // Verify content distribution
    EXPECT_FLOAT_EQ(system->Membranes[MockMembraneType::Custom2].Contents["Data"], 30.0f);
    EXPECT_FLOAT_EQ(system->Membranes[MockMembraneType::Custom3].Contents["Data"], 20.0f);
    
    // Dissolve one child
    auto dissResult = system->DissolveMembrane(MockMembraneType::Custom2);
    EXPECT_TRUE(dissResult.bSuccess);
    EXPECT_EQ(dissResult.ReceiverType, MockMembraneType::Custom1);
    
    // Verify lifecycle events
    EXPECT_EQ(system->LifecycleEvents.size(), 3u);
    EXPECT_EQ(system->LifecycleEvents[0].EventType, MockMembraneLifecycleEvent::Created);
    EXPECT_EQ(system->LifecycleEvents[1].EventType, MockMembraneLifecycleEvent::Divided);
    EXPECT_EQ(system->LifecycleEvents[2].EventType, MockMembraneLifecycleEvent::Dissolved);
}

TEST_F(MembraneLifecycleTest, ContentPropagation_MultipleContents)
{
    // Add multiple contents
    system->Membranes[MockMembraneType::Memory].Contents["ContentA"] = 100.0f;
    system->Membranes[MockMembraneType::Memory].Contents["ContentB"] = 50.0f;
    system->Membranes[MockMembraneType::Memory].Contents["ContentC"] = 25.0f;
    
    // Divide with 40/60 ratio
    system->DivideMembrane(MockMembraneType::Memory, 0.4f, 
                           MockMembraneType::Custom1, MockMembraneType::Custom2);
    
    // Verify distribution
    EXPECT_FLOAT_EQ(system->Membranes[MockMembraneType::Custom1].Contents["ContentA"], 40.0f);
    EXPECT_FLOAT_EQ(system->Membranes[MockMembraneType::Custom1].Contents["ContentB"], 20.0f);
    EXPECT_FLOAT_EQ(system->Membranes[MockMembraneType::Custom1].Contents["ContentC"], 10.0f);
    
    EXPECT_FLOAT_EQ(system->Membranes[MockMembraneType::Custom2].Contents["ContentA"], 60.0f);
    EXPECT_FLOAT_EQ(system->Membranes[MockMembraneType::Custom2].Contents["ContentB"], 30.0f);
    EXPECT_FLOAT_EQ(system->Membranes[MockMembraneType::Custom2].Contents["ContentC"], 15.0f);
}



} // namespace

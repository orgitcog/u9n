/**
 * @file ObjectTransportRulesTests.cpp
 * @brief Unit tests for Feature F1.3.2: Object/Symbol Transport Rules
 *
 * Tests P-system membrane computing transport rules:
 * - Membrane creation and hierarchy
 * - Symbol management
 * - Transport rules (IN, OUT, HERE)
 * - Rule execution modes
 * - Catalysts and inhibitors
 *
 * @author Deep Tree Echo Team
 * @date March 2026
 * @version 1.0.0-alpha
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

// Include mock header for standalone testing
#include "ObjectTransportRulesMock.h"

using ::testing::_;
using ::testing::Return;
using ::testing::AtLeast;

// ============================================================================
// Test Fixtures
// ============================================================================

class ObjectTransportRulesTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        TransportRules = std::make_unique<MockObjectTransportRules>();
    }

    void TearDown() override
    {
        TransportRules.reset();
    }

    std::unique_ptr<MockObjectTransportRules> TransportRules;

    // Helper to create a symbol
    FMembraneSymbol CreateSymbol(const std::string& Name, int32 Multiplicity = 1)
    {
        return FMembraneSymbol(FName(Name), Multiplicity);
    }

    // Helper to create a multiset with one symbol
    FSymbolMultiset CreateMultiset(const std::string& Name, int32 Multiplicity = 1)
    {
        FSymbolMultiset Multiset;
        Multiset.AddSymbol(CreateSymbol(Name, Multiplicity));
        return Multiset;
    }

    // Helper to create a simple rule
    FTransportRule CreateRule(
        const std::string& RuleId,
        const std::string& LeftSymbol,
        const std::string& RightSymbol,
        ETransportDirection Direction = ETransportDirection::Here,
        int32 Priority = 0)
    {
        FTransportRule Rule;
        Rule.RuleId = FName(RuleId);
        Rule.Priority = Priority;
        Rule.Target.Direction = Direction;

        if (!LeftSymbol.empty())
        {
            Rule.LeftHandSide.AddSymbol(CreateSymbol(LeftSymbol));
        }

        if (!RightSymbol.empty())
        {
            Rule.RightHandSide.AddSymbol(CreateSymbol(RightSymbol));
        }

        return Rule;
    }
};

// ============================================================================
// Symbol Multiset Tests
// ============================================================================

TEST_F(ObjectTransportRulesTest, MultisetAddSymbol)
{
    FSymbolMultiset Multiset;
    FMembraneSymbol SymA = CreateSymbol("A", 1);

    Multiset.AddSymbol(SymA);
    EXPECT_EQ(Multiset.GetSymbolCount(FName("A")), 1);
    EXPECT_EQ(Multiset.GetTotalCount(), 1);

    // Adding same symbol should increase multiplicity
    Multiset.AddSymbol(SymA);
    EXPECT_EQ(Multiset.GetSymbolCount(FName("A")), 2);
    EXPECT_EQ(Multiset.GetTotalCount(), 2);
}

TEST_F(ObjectTransportRulesTest, MultisetRemoveSymbol)
{
    FSymbolMultiset Multiset;
    FMembraneSymbol SymA = CreateSymbol("A", 3);
    Multiset.AddSymbol(SymA);

    // Remove part of multiplicity
    bool Result = Multiset.RemoveSymbol(CreateSymbol("A", 1));
    EXPECT_TRUE(Result);
    EXPECT_EQ(Multiset.GetSymbolCount(FName("A")), 2);

    // Remove remaining
    Result = Multiset.RemoveSymbol(CreateSymbol("A", 2));
    EXPECT_TRUE(Result);
    EXPECT_EQ(Multiset.GetSymbolCount(FName("A")), 0);
    EXPECT_TRUE(Multiset.IsEmpty());
}

TEST_F(ObjectTransportRulesTest, MultisetContains)
{
    FSymbolMultiset Multiset;
    Multiset.AddSymbol(CreateSymbol("A", 3));
    Multiset.AddSymbol(CreateSymbol("B", 2));

    EXPECT_TRUE(Multiset.Contains(CreateSymbol("A", 1)));
    EXPECT_TRUE(Multiset.Contains(CreateSymbol("A", 3)));
    EXPECT_FALSE(Multiset.Contains(CreateSymbol("A", 4)));  // Not enough
    EXPECT_TRUE(Multiset.Contains(CreateSymbol("B", 2)));
    EXPECT_FALSE(Multiset.Contains(CreateSymbol("C", 1)));  // Not present
}

TEST_F(ObjectTransportRulesTest, MultisetContainsAll)
{
    FSymbolMultiset Multiset;
    Multiset.AddSymbol(CreateSymbol("A", 3));
    Multiset.AddSymbol(CreateSymbol("B", 2));
    Multiset.AddSymbol(CreateSymbol("C", 1));

    FSymbolMultiset Pattern;
    Pattern.AddSymbol(CreateSymbol("A", 2));
    Pattern.AddSymbol(CreateSymbol("B", 1));

    EXPECT_TRUE(Multiset.ContainsAll(Pattern));

    // Pattern with too many
    FSymbolMultiset TooMuch;
    TooMuch.AddSymbol(CreateSymbol("A", 5));
    EXPECT_FALSE(Multiset.ContainsAll(TooMuch));

    // Pattern with non-existent symbol
    FSymbolMultiset Missing;
    Missing.AddSymbol(CreateSymbol("X", 1));
    EXPECT_FALSE(Multiset.ContainsAll(Missing));
}

// ============================================================================
// Transport Rule Tests
// ============================================================================

TEST_F(ObjectTransportRulesTest, RuleIsValid)
{
    FTransportRule ValidRule = CreateRule("R1", "A", "B");
    EXPECT_TRUE(ValidRule.IsValid());

    // Empty rule is still valid if enabled
    FTransportRule EmptyRule;
    EmptyRule.RuleId = FName("R2");
    EmptyRule.bEnabled = true;
    EXPECT_FALSE(EmptyRule.IsValid());  // No LHS or RHS

    // Disabled rule is not valid
    FTransportRule DisabledRule = CreateRule("R3", "A", "B");
    DisabledRule.bEnabled = false;
    EXPECT_FALSE(DisabledRule.IsValid());
}

TEST_F(ObjectTransportRulesTest, RuleToString)
{
    FTransportRule Rule = CreateRule("MyRule", "A", "B", ETransportDirection::Out);
    FString Str = Rule.ToString();

    EXPECT_TRUE(Str.Contains(TEXT("MyRule")));
    EXPECT_TRUE(Str.Contains(TEXT("A")));
    EXPECT_TRUE(Str.Contains(TEXT("B")));
    EXPECT_TRUE(Str.Contains(TEXT("out")));
}

// ============================================================================
// Membrane System Initialization Tests
// ============================================================================

TEST_F(ObjectTransportRulesTest, InitializeCreatesSkiinMembrane)
{
    bool Result = TransportRules->Initialize(FName("Skin"));
    EXPECT_TRUE(Result);

    FName SkinId = TransportRules->GetSkinMembraneId();
    EXPECT_FALSE(SkinId.IsNone());

    const FMembraneCompartment* Skin = TransportRules->GetMembrane(SkinId);
    EXPECT_NE(Skin, nullptr);
    EXPECT_TRUE(Skin->IsSkin());
    EXPECT_EQ(Skin->Label, FName("Skin"));
}

TEST_F(ObjectTransportRulesTest, ResetClearsSystem)
{
    TransportRules->Initialize(FName("Skin"));
    FName ChildId = TransportRules->CreateMembrane(TransportRules->GetSkinMembraneId(), FName("Child"));
    TransportRules->AddSymbol(ChildId, CreateSymbol("A"));

    TransportRules->Reset();

    EXPECT_TRUE(TransportRules->GetSkinMembraneId().IsNone());
    EXPECT_EQ(TransportRules->GetAllMembraneIds().Num(), 0);
}

// ============================================================================
// Membrane Hierarchy Tests
// ============================================================================

TEST_F(ObjectTransportRulesTest, CreateChildMembrane)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    FName ChildId = TransportRules->CreateMembrane(SkinId, FName("Child1"));
    EXPECT_FALSE(ChildId.IsNone());

    const FMembraneCompartment* Child = TransportRules->GetMembrane(ChildId);
    EXPECT_NE(Child, nullptr);
    EXPECT_EQ(Child->ParentId, SkinId);
    EXPECT_EQ(Child->Label, FName("Child1"));
    EXPECT_FALSE(Child->IsSkin());

    // Verify parent has child in list
    TArray<FName> Children = TransportRules->GetChildMembranes(SkinId);
    EXPECT_EQ(Children.Num(), 1);
    EXPECT_EQ(Children[0], ChildId);
}

TEST_F(ObjectTransportRulesTest, CreateNestedMembranes)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    FName Level1 = TransportRules->CreateMembrane(SkinId, FName("L1"));
    FName Level2 = TransportRules->CreateMembrane(Level1, FName("L2"));
    FName Level3 = TransportRules->CreateMembrane(Level2, FName("L3"));

    EXPECT_FALSE(Level3.IsNone());

    const FMembraneCompartment* L3 = TransportRules->GetMembrane(Level3);
    EXPECT_NE(L3, nullptr);
    EXPECT_EQ(L3->ParentId, Level2);
}

TEST_F(ObjectTransportRulesTest, RemoveMembraneReleasesContents)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();
    FName ChildId = TransportRules->CreateMembrane(SkinId, FName("Child"));

    // Add symbols to child
    TransportRules->AddSymbol(ChildId, CreateSymbol("A", 3));
    TransportRules->AddSymbol(ChildId, CreateSymbol("B", 2));

    // Remove child with release
    bool Removed = TransportRules->RemoveMembrane(ChildId, true);
    EXPECT_TRUE(Removed);

    // Verify symbols transferred to parent
    FSymbolMultiset SkinSymbols = TransportRules->GetSymbols(SkinId);
    EXPECT_EQ(SkinSymbols.GetSymbolCount(FName("A")), 3);
    EXPECT_EQ(SkinSymbols.GetSymbolCount(FName("B")), 2);
}

TEST_F(ObjectTransportRulesTest, CannotRemoveSkinMembrane)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    bool Removed = TransportRules->RemoveMembrane(SkinId);
    EXPECT_FALSE(Removed);

    // Skin should still exist
    EXPECT_NE(TransportRules->GetMembrane(SkinId), nullptr);
}

// ============================================================================
// Symbol Management Tests
// ============================================================================

TEST_F(ObjectTransportRulesTest, AddAndGetSymbols)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    TransportRules->AddSymbol(SkinId, CreateSymbol("A", 2));
    TransportRules->AddSymbol(SkinId, CreateSymbol("B", 3));

    FSymbolMultiset Symbols = TransportRules->GetSymbols(SkinId);
    EXPECT_EQ(Symbols.GetSymbolCount(FName("A")), 2);
    EXPECT_EQ(Symbols.GetSymbolCount(FName("B")), 3);
    EXPECT_EQ(Symbols.GetTotalCount(), 5);
}

TEST_F(ObjectTransportRulesTest, MembraneContainsSymbols)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    TransportRules->AddSymbol(SkinId, CreateSymbol("A", 5));

    FSymbolMultiset Query = CreateMultiset("A", 3);
    EXPECT_TRUE(TransportRules->MembraneContains(SkinId, Query));

    FSymbolMultiset TooMany = CreateMultiset("A", 10);
    EXPECT_FALSE(TransportRules->MembraneContains(SkinId, TooMany));
}

// ============================================================================
// Transport Rule Management Tests
// ============================================================================

TEST_F(ObjectTransportRulesTest, AddAndGetRules)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    FTransportRule Rule1 = CreateRule("R1", "A", "B", ETransportDirection::Here);
    FTransportRule Rule2 = CreateRule("R2", "B", "C", ETransportDirection::Out);

    EXPECT_TRUE(TransportRules->AddRule(SkinId, Rule1));
    EXPECT_TRUE(TransportRules->AddRule(SkinId, Rule2));

    TArray<FTransportRule> Rules = TransportRules->GetRules(SkinId);
    EXPECT_EQ(Rules.Num(), 2);
}

TEST_F(ObjectTransportRulesTest, CannotAddDuplicateRuleId)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    FTransportRule Rule1 = CreateRule("R1", "A", "B");
    FTransportRule Rule2 = CreateRule("R1", "X", "Y");  // Same ID

    EXPECT_TRUE(TransportRules->AddRule(SkinId, Rule1));
    EXPECT_FALSE(TransportRules->AddRule(SkinId, Rule2));
}

TEST_F(ObjectTransportRulesTest, RemoveRule)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    TransportRules->AddRule(SkinId, CreateRule("R1", "A", "B"));
    TransportRules->AddRule(SkinId, CreateRule("R2", "B", "C"));

    EXPECT_TRUE(TransportRules->RemoveRule(SkinId, FName("R1")));
    EXPECT_EQ(TransportRules->GetRules(SkinId).Num(), 1);

    EXPECT_FALSE(TransportRules->RemoveRule(SkinId, FName("R1")));  // Already removed
}

// ============================================================================
// Transport Direction Tests: HERE
// ============================================================================

TEST_F(ObjectTransportRulesTest, TransformHereSimple)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    // Add initial symbols
    TransportRules->AddSymbol(SkinId, CreateSymbol("A", 2));

    // Transform A -> B
    FSymbolMultiset Consumed = CreateMultiset("A", 1);
    FSymbolMultiset Produced = CreateMultiset("B", 1);

    bool Result = TransportRules->TransformHere(SkinId, Consumed, Produced);
    EXPECT_TRUE(Result);

    FSymbolMultiset After = TransportRules->GetSymbols(SkinId);
    EXPECT_EQ(After.GetSymbolCount(FName("A")), 1);  // 2 - 1 = 1
    EXPECT_EQ(After.GetSymbolCount(FName("B")), 1);  // 0 + 1 = 1
}

TEST_F(ObjectTransportRulesTest, TransformHereFailsWithoutSymbols)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    // Try to transform without symbols
    FSymbolMultiset Consumed = CreateMultiset("A", 1);
    FSymbolMultiset Produced = CreateMultiset("B", 1);

    bool Result = TransportRules->TransformHere(SkinId, Consumed, Produced);
    EXPECT_FALSE(Result);
}

// ============================================================================
// Transport Direction Tests: IN
// ============================================================================

TEST_F(ObjectTransportRulesTest, TransportInToChild)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();
    FName ChildId = TransportRules->CreateMembrane(SkinId, FName("Child"));

    // Add symbols to skin
    TransportRules->AddSymbol(SkinId, CreateSymbol("A", 3));

    // Transport A from skin to child
    FSymbolMultiset ToTransport = CreateMultiset("A", 2);
    bool Result = TransportRules->TransportIn(SkinId, ToTransport, ChildId);
    EXPECT_TRUE(Result);

    // Verify transport
    EXPECT_EQ(TransportRules->GetSymbols(SkinId).GetSymbolCount(FName("A")), 1);  // 3 - 2 = 1
    EXPECT_EQ(TransportRules->GetSymbols(ChildId).GetSymbolCount(FName("A")), 2);
}

TEST_F(ObjectTransportRulesTest, TransportInFailsWithoutChild)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    TransportRules->AddSymbol(SkinId, CreateSymbol("A", 1));

    // Try to transport in without children
    FSymbolMultiset ToTransport = CreateMultiset("A", 1);
    bool Result = TransportRules->TransportIn(SkinId, ToTransport);
    EXPECT_FALSE(Result);
}

// ============================================================================
// Transport Direction Tests: OUT
// ============================================================================

TEST_F(ObjectTransportRulesTest, TransportOutToParent)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();
    FName ChildId = TransportRules->CreateMembrane(SkinId, FName("Child"));

    // Add symbols to child
    TransportRules->AddSymbol(ChildId, CreateSymbol("B", 5));

    // Transport B from child to skin
    FSymbolMultiset ToTransport = CreateMultiset("B", 3);
    bool Result = TransportRules->TransportOut(ChildId, ToTransport);
    EXPECT_TRUE(Result);

    // Verify transport
    EXPECT_EQ(TransportRules->GetSymbols(ChildId).GetSymbolCount(FName("B")), 2);  // 5 - 3 = 2
    EXPECT_EQ(TransportRules->GetSymbols(SkinId).GetSymbolCount(FName("B")), 3);
}

TEST_F(ObjectTransportRulesTest, TransportOutFailsFromSkin)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    TransportRules->AddSymbol(SkinId, CreateSymbol("A", 1));

    // Try to transport out from skin (has no parent)
    FSymbolMultiset ToTransport = CreateMultiset("A", 1);
    bool Result = TransportRules->TransportOut(SkinId, ToTransport);
    EXPECT_FALSE(Result);
}

// ============================================================================
// Rule Execution Tests
// ============================================================================

TEST_F(ObjectTransportRulesTest, CanApplyRuleWithSymbols)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    TransportRules->AddSymbol(SkinId, CreateSymbol("A", 2));
    FTransportRule Rule = CreateRule("R1", "A", "B");
    TransportRules->AddRule(SkinId, Rule);

    EXPECT_TRUE(TransportRules->CanApplyRule(SkinId, Rule));
}

TEST_F(ObjectTransportRulesTest, CannotApplyRuleWithoutSymbols)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    FTransportRule Rule = CreateRule("R1", "A", "B");
    TransportRules->AddRule(SkinId, Rule);

    // No symbols added
    EXPECT_FALSE(TransportRules->CanApplyRule(SkinId, Rule));
}

TEST_F(ObjectTransportRulesTest, GetApplicableRules)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    TransportRules->AddSymbol(SkinId, CreateSymbol("A", 1));
    TransportRules->AddSymbol(SkinId, CreateSymbol("B", 1));

    TransportRules->AddRule(SkinId, CreateRule("R1", "A", "X"));  // Applicable
    TransportRules->AddRule(SkinId, CreateRule("R2", "B", "Y"));  // Applicable
    TransportRules->AddRule(SkinId, CreateRule("R3", "C", "Z"));  // Not applicable (no C)

    TArray<FTransportRule> Applicable = TransportRules->GetApplicableRules(SkinId);
    EXPECT_EQ(Applicable.Num(), 2);
}

TEST_F(ObjectTransportRulesTest, ExecuteStepAppliesRules)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    TransportRules->AddSymbol(SkinId, CreateSymbol("A", 3));
    TransportRules->AddRule(SkinId, CreateRule("R1", "A", "B"));

    // Execute one step
    int32 Applied = TransportRules->ExecuteStep();
    EXPECT_GT(Applied, 0);

    FSymbolMultiset After = TransportRules->GetSymbols(SkinId);
    EXPECT_LT(After.GetSymbolCount(FName("A")), 3);  // Some A consumed
    EXPECT_GT(After.GetSymbolCount(FName("B")), 0);  // Some B produced
}

TEST_F(ObjectTransportRulesTest, ExecuteStepReturnsZeroWhenHalted)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    // No symbols, no rules can fire
    TransportRules->AddRule(SkinId, CreateRule("R1", "A", "B"));

    int32 Applied = TransportRules->ExecuteStep();
    EXPECT_EQ(Applied, 0);
}

// ============================================================================
// Catalyst and Inhibitor Tests
// ============================================================================

TEST_F(ObjectTransportRulesTest, CatalystNotConsumed)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    TransportRules->AddSymbol(SkinId, CreateSymbol("A", 1));
    TransportRules->AddSymbol(SkinId, CreateSymbol("C", 1));  // Catalyst

    FTransportRule Rule = CreateRule("R1", "A", "B");
    FMembraneSymbol Catalyst = CreateSymbol("C", 1);
    Catalyst.bIsCatalyst = true;
    Rule.Catalysts.AddSymbol(Catalyst);
    TransportRules->AddRule(SkinId, Rule);

    TransportRules->ExecuteStep();

    FSymbolMultiset After = TransportRules->GetSymbols(SkinId);
    EXPECT_EQ(After.GetSymbolCount(FName("C")), 1);  // Catalyst still present
    EXPECT_EQ(After.GetSymbolCount(FName("A")), 0);  // Consumed
    EXPECT_EQ(After.GetSymbolCount(FName("B")), 1);  // Produced
}

TEST_F(ObjectTransportRulesTest, InhibitorBlocksRule)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    TransportRules->AddSymbol(SkinId, CreateSymbol("A", 1));
    TransportRules->AddSymbol(SkinId, CreateSymbol("I", 1));  // Inhibitor

    FTransportRule Rule = CreateRule("R1", "A", "B");
    Rule.Inhibitors.AddSymbol(CreateSymbol("I", 1));
    TransportRules->AddRule(SkinId, Rule);

    EXPECT_FALSE(TransportRules->CanApplyRule(SkinId, Rule));

    int32 Applied = TransportRules->ExecuteStep();
    EXPECT_EQ(Applied, 0);
}

// ============================================================================
// Rule Priority Tests
// ============================================================================

TEST_F(ObjectTransportRulesTest, HigherPriorityRulesFirst)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    TransportRules->AddSymbol(SkinId, CreateSymbol("A", 1));

    FTransportRule LowPriority = CreateRule("R_Low", "A", "X", ETransportDirection::Here, 1);
    FTransportRule HighPriority = CreateRule("R_High", "A", "Y", ETransportDirection::Here, 10);

    TransportRules->AddRule(SkinId, LowPriority);
    TransportRules->AddRule(SkinId, HighPriority);

    TArray<FTransportRule> Applicable = TransportRules->GetApplicableRules(SkinId);
    EXPECT_EQ(Applicable.Num(), 2);
    // First rule should be high priority
    EXPECT_EQ(Applicable[0].RuleId, FName("R_High"));
}

// ============================================================================
// Statistics Tests
// ============================================================================

TEST_F(ObjectTransportRulesTest, StatisticsTrackExecution)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();
    FName ChildId = TransportRules->CreateMembrane(SkinId, FName("Child"));

    TransportRules->AddSymbol(SkinId, CreateSymbol("A", 5));

    TransportRules->AddRule(SkinId, CreateRule("R1", "A", "B", ETransportDirection::Here));
    TransportRules->AddRule(SkinId, CreateRule("R2", "A", "C", ETransportDirection::In));

    TransportRules->ExecuteSteps(10);

    FTransportStatistics Stats = TransportRules->GetStatistics();
    EXPECT_GT(Stats.TotalRulesEvaluated, 0);
    EXPECT_GT(Stats.RulesApplied, 0);
    EXPECT_GT(Stats.ComputationSteps, 0);
}

TEST_F(ObjectTransportRulesTest, ResetStatistics)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();
    TransportRules->AddSymbol(SkinId, CreateSymbol("A", 1));
    TransportRules->AddRule(SkinId, CreateRule("R1", "A", "B"));

    TransportRules->ExecuteStep();

    TransportRules->ResetStatistics();
    FTransportStatistics Stats = TransportRules->GetStatistics();
    EXPECT_EQ(Stats.RulesApplied, 0);
    EXPECT_EQ(Stats.ComputationSteps, 0);
}

// ============================================================================
// System Validation Tests
// ============================================================================

TEST_F(ObjectTransportRulesTest, ValidateSystemIntegrity)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();
    FName Child1 = TransportRules->CreateMembrane(SkinId, FName("C1"));
    FName Child2 = TransportRules->CreateMembrane(SkinId, FName("C2"));
    FName GrandChild = TransportRules->CreateMembrane(Child1, FName("GC"));

    EXPECT_TRUE(TransportRules->ValidateSystem());
}

TEST_F(ObjectTransportRulesTest, GetHierarchyString)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();
    TransportRules->CreateMembrane(SkinId, FName("Child"));

    FString Hierarchy = TransportRules->GetHierarchyString();
    EXPECT_TRUE(Hierarchy.Contains(TEXT("Skin")));
    EXPECT_TRUE(Hierarchy.Contains(TEXT("Child")));
}

// ============================================================================
// Complex Scenario Tests
// ============================================================================

TEST_F(ObjectTransportRulesTest, ChainedRuleExecution)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();

    // Chain: A -> B -> C -> D
    TransportRules->AddSymbol(SkinId, CreateSymbol("A", 1));
    TransportRules->AddRule(SkinId, CreateRule("R1", "A", "B"));
    TransportRules->AddRule(SkinId, CreateRule("R2", "B", "C"));
    TransportRules->AddRule(SkinId, CreateRule("R3", "C", "D"));

    // Execute multiple steps
    TransportRules->ExecuteSteps(10);

    FSymbolMultiset Final = TransportRules->GetSymbols(SkinId);
    // All A should have transformed to D eventually
    EXPECT_EQ(Final.GetSymbolCount(FName("A")), 0);
    EXPECT_EQ(Final.GetSymbolCount(FName("D")), 1);
}

TEST_F(ObjectTransportRulesTest, MultiLevelTransport)
{
    TransportRules->Initialize(FName("Skin"));
    FName SkinId = TransportRules->GetSkinMembraneId();
    FName Level1 = TransportRules->CreateMembrane(SkinId, FName("L1"));
    FName Level2 = TransportRules->CreateMembrane(Level1, FName("L2"));

    // Start with symbols in Level2
    TransportRules->AddSymbol(Level2, CreateSymbol("Message", 1));

    // Rule in L2: send message OUT
    TransportRules->AddRule(Level2, CreateRule("Send1", "Message", "Message", ETransportDirection::Out));
    // Rule in L1: send message OUT
    TransportRules->AddRule(Level1, CreateRule("Send2", "Message", "Message", ETransportDirection::Out));

    // Execute
    TransportRules->ExecuteSteps(5);

    // Message should have reached skin
    EXPECT_GT(TransportRules->GetSymbols(SkinId).GetSymbolCount(FName("Message")), 0);
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

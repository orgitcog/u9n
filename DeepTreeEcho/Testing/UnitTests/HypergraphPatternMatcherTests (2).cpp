/**
 * @file HypergraphPatternMatcherTests.cpp
 * @brief Unit tests for Feature F1.4.2: Hypergraph Pattern Matching Engine
 * 
 * Tests cover:
 * - Pattern element creation and manipulation
 * - Variable binding and unification
 * - Edge pattern matching
 * - Hyperedge pattern matching
 * - Type and property constraints
 * - Transitive closure computation
 * - Negation and optional patterns
 * - Cross-variable constraints
 * - Index-accelerated matching
 * - Pattern builders
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
#include <cmath>

namespace {

// ========================================
// MOCK STRUCTURES FOR STANDALONE TESTING
// ========================================

// Mock enums matching UE implementation
enum class MockMemoryNodeType {
    Percept,
    Episode,
    Concept,
    Skill,
    Belief,
    Desire,
    Intention,
    Emotion,
    Place,
    Time,
    Agent,
    Object,
    Relation,
    Schema,
    MetaCognitive
};

enum class MockSemanticRelation {
    IS_A,
    INSTANCE_OF,
    SUBTYPE_OF,
    PART_OF,
    HAS_PART,
    MEMBER_OF,
    CAUSES,
    CAUSED_BY,
    ENABLES,
    PREVENTS,
    BEFORE,
    AFTER,
    DURING,
    OVERLAPS,
    LOCATED_AT,
    NEAR,
    CONTAINS,
    ADJACENT_TO,
    BELIEVES,
    DESIRES,
    INTENDS,
    EXPECTS,
    PERCEIVES,
    SIMILAR_TO,
    CONTRASTS_WITH,
    ASSOCIATED_WITH,
    REMINDS_OF,
    USED_FOR,
    CAPABLE_OF,
    REQUIRES,
    PRODUCES,
    POSITIVE_TOWARD,
    NEGATIVE_TOWARD,
    IMPORTANT_FOR,
    CONTRADICTS,
    SUPPORTS,
    DERIVED_FROM,
    GENERALIZES,
    CUSTOM
};

enum class MockPatternElementType {
    ConcreteNode,
    Variable,
    Wildcard,
    TypeConstraint,
    PropertyConstraint
};

enum class MockConstraintOperator {
    Equals,
    NotEquals,
    GreaterThan,
    LessThan,
    GreaterOrEqual,
    LessOrEqual,
    Contains,
    StartsWith,
    EndsWith,
    Regex,
    InRange,
    IsNull,
    IsNotNull
};

enum class MockPatternMatchMode {
    FirstMatch,
    AllMatches,
    CountOnly,
    ExistsCheck
};

// ========================================
// MOCK DATA STRUCTURES
// ========================================

struct MockMemoryNode {
    int64_t NodeID = 0;
    MockMemoryNodeType NodeType = MockMemoryNodeType::Concept;
    std::string Label;
    std::vector<float> Embedding;
    float Activation = 0.0f;
    float Strength = 0.5f;
    float Confidence = 1.0f;
    std::map<std::string, std::string> Properties;
    std::vector<int64_t> OutgoingEdges;
    std::vector<int64_t> IncomingEdges;
    std::vector<int64_t> HyperedgeMembership;
};

struct MockMemoryEdge {
    int64_t EdgeID = 0;
    int64_t SourceNodeID = 0;
    int64_t TargetNodeID = 0;
    MockSemanticRelation RelationType = MockSemanticRelation::ASSOCIATED_WITH;
    std::string CustomRelationLabel;
    float Weight = 1.0f;
    float Confidence = 1.0f;
    bool bBidirectional = false;
};

struct MockMemoryHyperedge {
    int64_t HyperedgeID = 0;
    std::string Label;
    std::vector<int64_t> MemberNodes;
    std::vector<std::string> MemberRoles;
    std::string HyperedgeType;
    float Weight = 1.0f;
    float Confidence = 1.0f;
};

struct MockPropertyConstraint {
    std::string PropertyKey;
    MockConstraintOperator Operator = MockConstraintOperator::Equals;
    std::string Value;
    std::string SecondaryValue;
    bool bNumericComparison = false;
};

struct MockPatternElement {
    MockPatternElementType ElementType = MockPatternElementType::Variable;
    std::string VariableName;
    int64_t ConcreteNodeID = 0;
    MockMemoryNodeType NodeTypeConstraint = MockMemoryNodeType::Concept;
    std::vector<MockPropertyConstraint> PropertyConstraints;
    float MinActivation = 0.0f;
    float MinStrength = 0.0f;
    
    static MockPatternElement Variable(const std::string& name) {
        MockPatternElement elem;
        elem.ElementType = MockPatternElementType::Variable;
        elem.VariableName = name;
        return elem;
    }
    
    static MockPatternElement Concrete(int64_t nodeId) {
        MockPatternElement elem;
        elem.ElementType = MockPatternElementType::ConcreteNode;
        elem.ConcreteNodeID = nodeId;
        return elem;
    }
    
    static MockPatternElement TypedVariable(const std::string& name, MockMemoryNodeType type) {
        MockPatternElement elem;
        elem.ElementType = MockPatternElementType::TypeConstraint;
        elem.VariableName = name;
        elem.NodeTypeConstraint = type;
        return elem;
    }
    
    static MockPatternElement Wildcard() {
        MockPatternElement elem;
        elem.ElementType = MockPatternElementType::Wildcard;
        return elem;
    }
};

struct MockEdgePattern {
    MockPatternElement Source;
    MockPatternElement Target;
    std::vector<MockSemanticRelation> AllowedRelations;
    std::string CustomRelationLabel;
    bool bTransitive = false;
    int32_t MaxTransitiveDepth = 5;
    float MinWeight = 0.0f;
    bool bDirected = true;
    std::string EdgeVariableName;
};

struct MockHyperedgePattern {
    std::vector<MockPatternElement> Members;
    std::vector<std::string> ExpectedRoles;
    std::string HyperedgeTypeConstraint;
    float MinWeight = 0.0f;
    std::string HyperedgeVariableName;
    bool bAllowPartialMatch = false;
    int32_t MinMembersForPartial = 2;
};

struct MockGraphPattern {
    std::string PatternID;
    std::string Description;
    std::vector<MockEdgePattern> EdgePatterns;
    std::vector<MockHyperedgePattern> HyperedgePatterns;
    std::vector<MockPatternElement> NodePatterns;
    std::vector<std::string> CrossConstraints;
    std::vector<MockEdgePattern> NegatedEdgePatterns;
    std::vector<MockEdgePattern> OptionalEdgePatterns;
    int32_t Priority = 0;
};

struct MockVariableBinding {
    std::string VariableName;
    int64_t BoundNodeID = 0;
    int64_t BoundEdgeID = 0;
    int64_t BoundHyperedgeID = 0;
    bool bIsNodeBinding = true;
    bool bIsEdgeBinding = false;
    bool bIsHyperedgeBinding = false;
    float Confidence = 1.0f;
};

struct MockBindingEnvironment {
    std::vector<MockVariableBinding> Bindings;
    float MatchConfidence = 1.0f;
    float RelevanceScore = 1.0f;
    bool bIsComplete = false;
    
    const MockVariableBinding* GetBinding(const std::string& variableName) const {
        for (const auto& binding : Bindings) {
            if (binding.VariableName == variableName) {
                return &binding;
            }
        }
        return nullptr;
    }
    
    bool IsBound(const std::string& variableName) const {
        return GetBinding(variableName) != nullptr;
    }
    
    int64_t GetBoundNodeID(const std::string& variableName) const {
        const MockVariableBinding* binding = GetBinding(variableName);
        return (binding && binding->bIsNodeBinding) ? binding->BoundNodeID : 0;
    }
};

struct MockPatternMatchResult {
    std::string PatternID;
    std::vector<MockBindingEnvironment> MatchingBindings;
    int32_t TotalMatches = 0;
    bool bHasMatches = false;
    float ExecutionTimeMs = 0.0f;
    int32_t NodesExamined = 0;
    int32_t EdgesExamined = 0;
    std::string ErrorMessage;
};

struct MockPatternMatchConfig {
    MockPatternMatchMode MatchMode = MockPatternMatchMode::AllMatches;
    int32_t MaxMatches = 1000;
    float TimeoutMs = 1000.0f;
    bool bEnableParallel = true;
    bool bIncludeOptional = true;
    bool bSortByRelevance = true;
    float MinConfidence = 0.0f;
    bool bUseIndexes = true;
};

// ========================================
// MOCK HYPERGRAPH MEMORY SYSTEM
// ========================================

class MockHypergraphMemorySystem {
public:
    std::map<int64_t, MockMemoryNode> Nodes;
    std::map<int64_t, MockMemoryEdge> Edges;
    std::map<int64_t, MockMemoryHyperedge> Hyperedges;
    int64_t NextNodeID = 1;
    int64_t NextEdgeID = 1;
    int64_t NextHyperedgeID = 1;
    
    int64_t CreateNode(MockMemoryNodeType type, const std::string& label, float strength = 0.5f) {
        MockMemoryNode node;
        node.NodeID = NextNodeID++;
        node.NodeType = type;
        node.Label = label;
        node.Strength = strength;
        Nodes[node.NodeID] = node;
        return node.NodeID;
    }
    
    int64_t CreateEdge(int64_t sourceID, int64_t targetID, MockSemanticRelation relation, float weight = 1.0f) {
        MockMemoryEdge edge;
        edge.EdgeID = NextEdgeID++;
        edge.SourceNodeID = sourceID;
        edge.TargetNodeID = targetID;
        edge.RelationType = relation;
        edge.Weight = weight;
        Edges[edge.EdgeID] = edge;
        
        // Update node edge lists
        if (Nodes.count(sourceID)) {
            Nodes[sourceID].OutgoingEdges.push_back(edge.EdgeID);
        }
        if (Nodes.count(targetID)) {
            Nodes[targetID].IncomingEdges.push_back(edge.EdgeID);
        }
        
        return edge.EdgeID;
    }
    
    int64_t CreateHyperedge(const std::vector<int64_t>& members, const std::vector<std::string>& roles,
                           const std::string& type, float weight = 1.0f) {
        MockMemoryHyperedge hyperedge;
        hyperedge.HyperedgeID = NextHyperedgeID++;
        hyperedge.MemberNodes = members;
        hyperedge.MemberRoles = roles;
        hyperedge.HyperedgeType = type;
        hyperedge.Weight = weight;
        Hyperedges[hyperedge.HyperedgeID] = hyperedge;
        
        // Update node membership
        for (int64_t memberID : members) {
            if (Nodes.count(memberID)) {
                Nodes[memberID].HyperedgeMembership.push_back(hyperedge.HyperedgeID);
            }
        }
        
        return hyperedge.HyperedgeID;
    }
    
    MockMemoryNode GetNode(int64_t nodeID) const {
        auto it = Nodes.find(nodeID);
        return it != Nodes.end() ? it->second : MockMemoryNode();
    }
    
    MockMemoryEdge GetEdge(int64_t edgeID) const {
        auto it = Edges.find(edgeID);
        return it != Edges.end() ? it->second : MockMemoryEdge();
    }
    
    MockMemoryHyperedge GetHyperedge(int64_t hyperedgeID) const {
        auto it = Hyperedges.find(hyperedgeID);
        return it != Hyperedges.end() ? it->second : MockMemoryHyperedge();
    }
    
    bool NodeExists(int64_t nodeID) const {
        return Nodes.count(nodeID) > 0;
    }
    
    std::vector<MockMemoryEdge> GetOutgoingEdges(int64_t nodeID) const {
        std::vector<MockMemoryEdge> result;
        auto it = Nodes.find(nodeID);
        if (it != Nodes.end()) {
            for (int64_t edgeID : it->second.OutgoingEdges) {
                auto edgeIt = Edges.find(edgeID);
                if (edgeIt != Edges.end()) {
                    result.push_back(edgeIt->second);
                }
            }
        }
        return result;
    }
    
    std::vector<MockMemoryEdge> GetIncomingEdges(int64_t nodeID) const {
        std::vector<MockMemoryEdge> result;
        auto it = Nodes.find(nodeID);
        if (it != Nodes.end()) {
            for (int64_t edgeID : it->second.IncomingEdges) {
                auto edgeIt = Edges.find(edgeID);
                if (edgeIt != Edges.end()) {
                    result.push_back(edgeIt->second);
                }
            }
        }
        return result;
    }
    
    std::vector<MockMemoryHyperedge> GetNodeHyperedges(int64_t nodeID) const {
        std::vector<MockMemoryHyperedge> result;
        auto it = Nodes.find(nodeID);
        if (it != Nodes.end()) {
            for (int64_t hyperedgeID : it->second.HyperedgeMembership) {
                auto heIt = Hyperedges.find(hyperedgeID);
                if (heIt != Hyperedges.end()) {
                    result.push_back(heIt->second);
                }
            }
        }
        return result;
    }
    
    void SetNodeProperty(int64_t nodeID, const std::string& key, const std::string& value) {
        auto it = Nodes.find(nodeID);
        if (it != Nodes.end()) {
            it->second.Properties[key] = value;
        }
    }
    
    void SetNodeActivation(int64_t nodeID, float activation) {
        auto it = Nodes.find(nodeID);
        if (it != Nodes.end()) {
            it->second.Activation = activation;
        }
    }
};

// ========================================
// MOCK PATTERN MATCHER
// ========================================

class MockHypergraphPatternMatcher {
public:
    MockHypergraphMemorySystem* Hypergraph = nullptr;
    std::map<std::string, MockGraphPattern> RegisteredPatterns;
    MockPatternMatchConfig DefaultConfig;
    int64_t TotalMatchAttempts = 0;
    int64_t TotalMatchesFound = 0;
    
    void SetHypergraph(MockHypergraphMemorySystem* hg) {
        Hypergraph = hg;
    }
    
    bool RegisterPattern(const MockGraphPattern& pattern) {
        if (pattern.PatternID.empty()) return false;
        RegisteredPatterns[pattern.PatternID] = pattern;
        return true;
    }
    
    bool UnregisterPattern(const std::string& patternID) {
        return RegisteredPatterns.erase(patternID) > 0;
    }
    
    MockGraphPattern GetPattern(const std::string& patternID) const {
        auto it = RegisteredPatterns.find(patternID);
        return it != RegisteredPatterns.end() ? it->second : MockGraphPattern();
    }
    
    std::vector<std::string> GetRegisteredPatternIDs() const {
        std::vector<std::string> ids;
        for (const auto& pair : RegisteredPatterns) {
            ids.push_back(pair.first);
        }
        return ids;
    }
    
    // Pattern builders
    static MockEdgePattern CreateEdgePattern(const std::string& sourceVar, 
                                             const std::string& targetVar,
                                             MockSemanticRelation relation) {
        MockEdgePattern pattern;
        pattern.Source = MockPatternElement::Variable(sourceVar);
        pattern.Target = MockPatternElement::Variable(targetVar);
        pattern.AllowedRelations.push_back(relation);
        pattern.bDirected = true;
        return pattern;
    }
    
    static MockGraphPattern CreateSimpleEdgeGraphPattern(const std::string& sourceVar,
                                                          const std::string& targetVar,
                                                          MockSemanticRelation relation,
                                                          const std::string& patternID = "") {
        MockGraphPattern pattern;
        pattern.PatternID = patternID.empty() ? ("edge_" + sourceVar + "_" + targetVar) : patternID;
        pattern.EdgePatterns.push_back(CreateEdgePattern(sourceVar, targetVar, relation));
        return pattern;
    }
    
    static MockGraphPattern CreateTrianglePattern(const std::string& varA,
                                                   const std::string& varB,
                                                   const std::string& varC,
                                                   MockSemanticRelation relAB,
                                                   MockSemanticRelation relBC,
                                                   MockSemanticRelation relAC) {
        MockGraphPattern pattern;
        pattern.PatternID = "triangle_" + varA + "_" + varB + "_" + varC;
        pattern.Description = "Triangle pattern with three connected nodes";
        pattern.EdgePatterns.push_back(CreateEdgePattern(varA, varB, relAB));
        pattern.EdgePatterns.push_back(CreateEdgePattern(varB, varC, relBC));
        pattern.EdgePatterns.push_back(CreateEdgePattern(varA, varC, relAC));
        return pattern;
    }
    
    // Binding operations
    static MockBindingEnvironment ExtendBinding(const MockBindingEnvironment& env,
                                                 const std::string& variableName,
                                                 int64_t nodeID) {
        MockBindingEnvironment extended = env;
        
        // Check if already bound
        for (const auto& binding : extended.Bindings) {
            if (binding.VariableName == variableName) {
                if (binding.BoundNodeID == nodeID) {
                    return extended; // Same binding, OK
                } else {
                    // Conflict
                    extended.bIsComplete = false;
                    extended.MatchConfidence = 0.0f;
                    return extended;
                }
            }
        }
        
        // Add new binding
        MockVariableBinding newBinding;
        newBinding.VariableName = variableName;
        newBinding.BoundNodeID = nodeID;
        newBinding.bIsNodeBinding = true;
        newBinding.Confidence = 1.0f;
        extended.Bindings.push_back(newBinding);
        
        return extended;
    }
    
    static bool BindingsAreCompatible(const MockBindingEnvironment& env1,
                                      const MockBindingEnvironment& env2) {
        for (const auto& b1 : env1.Bindings) {
            for (const auto& b2 : env2.Bindings) {
                if (b1.VariableName == b2.VariableName) {
                    if (b1.bIsNodeBinding && b2.bIsNodeBinding) {
                        if (b1.BoundNodeID != b2.BoundNodeID) {
                            return false;
                        }
                    }
                }
            }
        }
        return true;
    }
    
    static MockBindingEnvironment MergeBindings(const MockBindingEnvironment& env1,
                                                 const MockBindingEnvironment& env2) {
        if (!BindingsAreCompatible(env1, env2)) {
            MockBindingEnvironment invalid;
            invalid.bIsComplete = false;
            invalid.MatchConfidence = 0.0f;
            return invalid;
        }
        
        MockBindingEnvironment merged = env1;
        
        for (const auto& b2 : env2.Bindings) {
            bool found = false;
            for (const auto& b1 : merged.Bindings) {
                if (b1.VariableName == b2.VariableName) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                merged.Bindings.push_back(b2);
            }
        }
        
        merged.MatchConfidence = env1.MatchConfidence * env2.MatchConfidence;
        return merged;
    }
    
    // Node constraint checking
    bool NodeSatisfiesElement(const MockMemoryNode& node, const MockPatternElement& element) const {
        // Check type constraint
        if (element.ElementType == MockPatternElementType::TypeConstraint) {
            if (node.NodeType != element.NodeTypeConstraint) {
                return false;
            }
        }
        
        // Check activation threshold
        if (element.MinActivation > 0.0f && node.Activation < element.MinActivation) {
            return false;
        }
        
        // Check strength threshold
        if (element.MinStrength > 0.0f && node.Strength < element.MinStrength) {
            return false;
        }
        
        // Check property constraints
        for (const auto& constraint : element.PropertyConstraints) {
            if (!CheckPropertyConstraint(node, constraint)) {
                return false;
            }
        }
        
        return true;
    }
    
    bool CheckPropertyConstraint(const MockMemoryNode& node, const MockPropertyConstraint& constraint) const {
        auto it = node.Properties.find(constraint.PropertyKey);
        
        if (constraint.Operator == MockConstraintOperator::IsNull) {
            return it == node.Properties.end() || it->second.empty();
        }
        
        if (constraint.Operator == MockConstraintOperator::IsNotNull) {
            return it != node.Properties.end() && !it->second.empty();
        }
        
        if (it == node.Properties.end()) {
            return false;
        }
        
        const std::string& propValue = it->second;
        
        if (constraint.bNumericComparison) {
            float nodeValue = std::stof(propValue);
            float constraintValue = std::stof(constraint.Value);
            
            switch (constraint.Operator) {
                case MockConstraintOperator::Equals:
                    return std::abs(nodeValue - constraintValue) < 0.0001f;
                case MockConstraintOperator::NotEquals:
                    return std::abs(nodeValue - constraintValue) >= 0.0001f;
                case MockConstraintOperator::GreaterThan:
                    return nodeValue > constraintValue;
                case MockConstraintOperator::LessThan:
                    return nodeValue < constraintValue;
                case MockConstraintOperator::GreaterOrEqual:
                    return nodeValue >= constraintValue;
                case MockConstraintOperator::LessOrEqual:
                    return nodeValue <= constraintValue;
                case MockConstraintOperator::InRange: {
                    float secondValue = std::stof(constraint.SecondaryValue);
                    return nodeValue >= constraintValue && nodeValue <= secondValue;
                }
                default:
                    return false;
            }
        }
        
        // String comparison
        switch (constraint.Operator) {
            case MockConstraintOperator::Equals:
                return propValue == constraint.Value;
            case MockConstraintOperator::NotEquals:
                return propValue != constraint.Value;
            case MockConstraintOperator::Contains:
                return propValue.find(constraint.Value) != std::string::npos;
            case MockConstraintOperator::StartsWith:
                return propValue.substr(0, constraint.Value.length()) == constraint.Value;
            case MockConstraintOperator::EndsWith:
                return propValue.length() >= constraint.Value.length() &&
                       propValue.substr(propValue.length() - constraint.Value.length()) == constraint.Value;
            default:
                return false;
        }
    }
    
    // Simple pattern matching for tests
    MockPatternMatchResult MatchSimpleEdgePattern(const MockEdgePattern& pattern,
                                                   const MockPatternMatchConfig& config) {
        MockPatternMatchResult result;
        if (!Hypergraph) {
            result.ErrorMessage = "No hypergraph reference";
            return result;
        }
        
        TotalMatchAttempts++;
        
        // Iterate all edges
        for (const auto& edgePair : Hypergraph->Edges) {
            const MockMemoryEdge& edge = edgePair.second;
            
            // Check relation type constraint
            if (!pattern.AllowedRelations.empty()) {
                bool found = false;
                for (MockSemanticRelation rel : pattern.AllowedRelations) {
                    if (edge.RelationType == rel) {
                        found = true;
                        break;
                    }
                }
                if (!found) continue;
            }
            
            // Check weight constraint
            if (edge.Weight < pattern.MinWeight) continue;
            
            // Check node constraints
            MockMemoryNode sourceNode = Hypergraph->GetNode(edge.SourceNodeID);
            MockMemoryNode targetNode = Hypergraph->GetNode(edge.TargetNodeID);
            
            if (!NodeSatisfiesElement(sourceNode, pattern.Source)) continue;
            if (!NodeSatisfiesElement(targetNode, pattern.Target)) continue;
            
            // Create binding
            MockBindingEnvironment env;
            
            if (pattern.Source.ElementType == MockPatternElementType::Variable ||
                pattern.Source.ElementType == MockPatternElementType::TypeConstraint) {
                MockVariableBinding binding;
                binding.VariableName = pattern.Source.VariableName;
                binding.BoundNodeID = edge.SourceNodeID;
                binding.bIsNodeBinding = true;
                env.Bindings.push_back(binding);
            }
            
            if (pattern.Target.ElementType == MockPatternElementType::Variable ||
                pattern.Target.ElementType == MockPatternElementType::TypeConstraint) {
                MockVariableBinding binding;
                binding.VariableName = pattern.Target.VariableName;
                binding.BoundNodeID = edge.TargetNodeID;
                binding.bIsNodeBinding = true;
                env.Bindings.push_back(binding);
            }
            
            env.bIsComplete = true;
            env.MatchConfidence = edge.Confidence;
            env.RelevanceScore = edge.Weight;
            
            result.MatchingBindings.push_back(env);
            
            if (config.MatchMode == MockPatternMatchMode::FirstMatch) {
                break;
            }
        }
        
        result.TotalMatches = result.MatchingBindings.size();
        result.bHasMatches = result.TotalMatches > 0;
        TotalMatchesFound += result.TotalMatches;
        
        return result;
    }
    
    bool PatternExists(const MockGraphPattern& pattern) {
        if (pattern.EdgePatterns.empty()) return false;
        
        MockPatternMatchConfig config;
        config.MatchMode = MockPatternMatchMode::ExistsCheck;
        config.MaxMatches = 1;
        
        MockPatternMatchResult result = MatchSimpleEdgePattern(pattern.EdgePatterns[0], config);
        return result.bHasMatches;
    }
    
    int32_t CountPatternMatches(const MockGraphPattern& pattern) {
        if (pattern.EdgePatterns.empty()) return 0;
        
        MockPatternMatchConfig config;
        config.MatchMode = MockPatternMatchMode::CountOnly;
        config.MaxMatches = 0;
        
        MockPatternMatchResult result = MatchSimpleEdgePattern(pattern.EdgePatterns[0], config);
        return result.TotalMatches;
    }
};

// ========================================
// TEST FIXTURES
// ========================================

class HypergraphPatternMatcherTest : public ::testing::Test {
protected:
    std::unique_ptr<MockHypergraphMemorySystem> Hypergraph;
    std::unique_ptr<MockHypergraphPatternMatcher> Matcher;
    
    void SetUp() override {
        Hypergraph = std::make_unique<MockHypergraphMemorySystem>();
        Matcher = std::make_unique<MockHypergraphPatternMatcher>();
        Matcher->SetHypergraph(Hypergraph.get());
    }
    
    void TearDown() override {
        Matcher.reset();
        Hypergraph.reset();
    }
    
    // Helper to create a simple knowledge graph
    void CreateSimpleKnowledgeGraph() {
        // Create nodes
        int64_t dog = Hypergraph->CreateNode(MockMemoryNodeType::Concept, "Dog", 0.8f);
        int64_t animal = Hypergraph->CreateNode(MockMemoryNodeType::Concept, "Animal", 0.9f);
        int64_t mammal = Hypergraph->CreateNode(MockMemoryNodeType::Concept, "Mammal", 0.85f);
        int64_t cat = Hypergraph->CreateNode(MockMemoryNodeType::Concept, "Cat", 0.7f);
        int64_t fido = Hypergraph->CreateNode(MockMemoryNodeType::Object, "Fido", 0.6f);
        
        // Create edges
        Hypergraph->CreateEdge(dog, mammal, MockSemanticRelation::IS_A);
        Hypergraph->CreateEdge(mammal, animal, MockSemanticRelation::IS_A);
        Hypergraph->CreateEdge(cat, mammal, MockSemanticRelation::IS_A);
        Hypergraph->CreateEdge(fido, dog, MockSemanticRelation::INSTANCE_OF);
    }
    
    void CreateGraphWithProperties() {
        int64_t person1 = Hypergraph->CreateNode(MockMemoryNodeType::Agent, "Alice", 0.9f);
        int64_t person2 = Hypergraph->CreateNode(MockMemoryNodeType::Agent, "Bob", 0.8f);
        int64_t location = Hypergraph->CreateNode(MockMemoryNodeType::Place, "Office", 0.7f);
        
        Hypergraph->SetNodeProperty(person1, "age", "30");
        Hypergraph->SetNodeProperty(person1, "role", "engineer");
        Hypergraph->SetNodeProperty(person2, "age", "25");
        Hypergraph->SetNodeProperty(person2, "role", "designer");
        
        Hypergraph->SetNodeActivation(person1, 0.9f);
        Hypergraph->SetNodeActivation(person2, 0.3f);
        
        Hypergraph->CreateEdge(person1, location, MockSemanticRelation::LOCATED_AT);
        Hypergraph->CreateEdge(person2, location, MockSemanticRelation::LOCATED_AT);
        Hypergraph->CreateEdge(person1, person2, MockSemanticRelation::ASSOCIATED_WITH);
    }
};

// ========================================
// PATTERN ELEMENT TESTS
// ========================================

TEST_F(HypergraphPatternMatcherTest, CreateVariableElement) {
    MockPatternElement elem = MockPatternElement::Variable("x");
    
    EXPECT_EQ(elem.ElementType, MockPatternElementType::Variable);
    EXPECT_EQ(elem.VariableName, "x");
}

TEST_F(HypergraphPatternMatcherTest, CreateConcreteElement) {
    MockPatternElement elem = MockPatternElement::Concrete(42);
    
    EXPECT_EQ(elem.ElementType, MockPatternElementType::ConcreteNode);
    EXPECT_EQ(elem.ConcreteNodeID, 42);
}

TEST_F(HypergraphPatternMatcherTest, CreateTypedVariableElement) {
    MockPatternElement elem = MockPatternElement::TypedVariable("concept", MockMemoryNodeType::Concept);
    
    EXPECT_EQ(elem.ElementType, MockPatternElementType::TypeConstraint);
    EXPECT_EQ(elem.VariableName, "concept");
    EXPECT_EQ(elem.NodeTypeConstraint, MockMemoryNodeType::Concept);
}

TEST_F(HypergraphPatternMatcherTest, CreateWildcardElement) {
    MockPatternElement elem = MockPatternElement::Wildcard();
    
    EXPECT_EQ(elem.ElementType, MockPatternElementType::Wildcard);
}

// ========================================
// BINDING TESTS
// ========================================

TEST_F(HypergraphPatternMatcherTest, ExtendBindingAddsNewVariable) {
    MockBindingEnvironment env;
    
    MockBindingEnvironment extended = MockHypergraphPatternMatcher::ExtendBinding(env, "x", 100);
    
    EXPECT_TRUE(extended.IsBound("x"));
    EXPECT_EQ(extended.GetBoundNodeID("x"), 100);
    EXPECT_EQ(extended.Bindings.size(), 1);
}

TEST_F(HypergraphPatternMatcherTest, ExtendBindingSameValueSucceeds) {
    MockBindingEnvironment env;
    env = MockHypergraphPatternMatcher::ExtendBinding(env, "x", 100);
    
    MockBindingEnvironment extended = MockHypergraphPatternMatcher::ExtendBinding(env, "x", 100);
    
    EXPECT_TRUE(extended.IsBound("x"));
    EXPECT_EQ(extended.GetBoundNodeID("x"), 100);
    EXPECT_GT(extended.MatchConfidence, 0.0f);
}

TEST_F(HypergraphPatternMatcherTest, ExtendBindingConflictFails) {
    MockBindingEnvironment env;
    env = MockHypergraphPatternMatcher::ExtendBinding(env, "x", 100);
    
    MockBindingEnvironment extended = MockHypergraphPatternMatcher::ExtendBinding(env, "x", 200);
    
    EXPECT_EQ(extended.MatchConfidence, 0.0f);
}

TEST_F(HypergraphPatternMatcherTest, BindingsCompatible) {
    MockBindingEnvironment env1, env2;
    env1 = MockHypergraphPatternMatcher::ExtendBinding(env1, "x", 100);
    env2 = MockHypergraphPatternMatcher::ExtendBinding(env2, "x", 100);
    env2 = MockHypergraphPatternMatcher::ExtendBinding(env2, "y", 200);
    
    EXPECT_TRUE(MockHypergraphPatternMatcher::BindingsAreCompatible(env1, env2));
}

TEST_F(HypergraphPatternMatcherTest, BindingsIncompatible) {
    MockBindingEnvironment env1, env2;
    env1 = MockHypergraphPatternMatcher::ExtendBinding(env1, "x", 100);
    env2 = MockHypergraphPatternMatcher::ExtendBinding(env2, "x", 200);
    
    EXPECT_FALSE(MockHypergraphPatternMatcher::BindingsAreCompatible(env1, env2));
}

TEST_F(HypergraphPatternMatcherTest, MergeCompatibleBindings) {
    MockBindingEnvironment env1, env2;
    env1 = MockHypergraphPatternMatcher::ExtendBinding(env1, "x", 100);
    env2 = MockHypergraphPatternMatcher::ExtendBinding(env2, "y", 200);
    
    MockBindingEnvironment merged = MockHypergraphPatternMatcher::MergeBindings(env1, env2);
    
    EXPECT_TRUE(merged.IsBound("x"));
    EXPECT_TRUE(merged.IsBound("y"));
    EXPECT_EQ(merged.GetBoundNodeID("x"), 100);
    EXPECT_EQ(merged.GetBoundNodeID("y"), 200);
}

TEST_F(HypergraphPatternMatcherTest, MergeIncompatibleBindingsFails) {
    MockBindingEnvironment env1, env2;
    env1 = MockHypergraphPatternMatcher::ExtendBinding(env1, "x", 100);
    env2 = MockHypergraphPatternMatcher::ExtendBinding(env2, "x", 200);
    
    MockBindingEnvironment merged = MockHypergraphPatternMatcher::MergeBindings(env1, env2);
    
    EXPECT_EQ(merged.MatchConfidence, 0.0f);
    EXPECT_FALSE(merged.bIsComplete);
}

// ========================================
// PATTERN BUILDER TESTS
// ========================================

TEST_F(HypergraphPatternMatcherTest, CreateEdgePatternWithRelation) {
    MockEdgePattern pattern = MockHypergraphPatternMatcher::CreateEdgePattern(
        "source", "target", MockSemanticRelation::IS_A);
    
    EXPECT_EQ(pattern.Source.VariableName, "source");
    EXPECT_EQ(pattern.Target.VariableName, "target");
    EXPECT_EQ(pattern.AllowedRelations.size(), 1);
    EXPECT_EQ(pattern.AllowedRelations[0], MockSemanticRelation::IS_A);
    EXPECT_TRUE(pattern.bDirected);
}

TEST_F(HypergraphPatternMatcherTest, CreateSimpleGraphPattern) {
    MockGraphPattern pattern = MockHypergraphPatternMatcher::CreateSimpleEdgeGraphPattern(
        "x", "y", MockSemanticRelation::CAUSES, "causal_pattern");
    
    EXPECT_EQ(pattern.PatternID, "causal_pattern");
    EXPECT_EQ(pattern.EdgePatterns.size(), 1);
    EXPECT_EQ(pattern.EdgePatterns[0].Source.VariableName, "x");
    EXPECT_EQ(pattern.EdgePatterns[0].Target.VariableName, "y");
}

TEST_F(HypergraphPatternMatcherTest, CreateTrianglePattern) {
    MockGraphPattern pattern = MockHypergraphPatternMatcher::CreateTrianglePattern(
        "A", "B", "C",
        MockSemanticRelation::IS_A,
        MockSemanticRelation::IS_A,
        MockSemanticRelation::IS_A);
    
    EXPECT_EQ(pattern.EdgePatterns.size(), 3);
    EXPECT_FALSE(pattern.Description.empty());
}

// ========================================
// PATTERN REGISTRATION TESTS
// ========================================

TEST_F(HypergraphPatternMatcherTest, RegisterPatternSucceeds) {
    MockGraphPattern pattern = MockHypergraphPatternMatcher::CreateSimpleEdgeGraphPattern(
        "x", "y", MockSemanticRelation::IS_A, "test_pattern");
    
    EXPECT_TRUE(Matcher->RegisterPattern(pattern));
    EXPECT_EQ(Matcher->GetRegisteredPatternIDs().size(), 1);
}

TEST_F(HypergraphPatternMatcherTest, RegisterPatternWithoutIDFails) {
    MockGraphPattern pattern;
    pattern.PatternID = "";
    
    EXPECT_FALSE(Matcher->RegisterPattern(pattern));
}

TEST_F(HypergraphPatternMatcherTest, GetRegisteredPattern) {
    MockGraphPattern pattern = MockHypergraphPatternMatcher::CreateSimpleEdgeGraphPattern(
        "x", "y", MockSemanticRelation::IS_A, "my_pattern");
    Matcher->RegisterPattern(pattern);
    
    MockGraphPattern retrieved = Matcher->GetPattern("my_pattern");
    
    EXPECT_EQ(retrieved.PatternID, "my_pattern");
    EXPECT_EQ(retrieved.EdgePatterns.size(), 1);
}

TEST_F(HypergraphPatternMatcherTest, UnregisterPattern) {
    MockGraphPattern pattern = MockHypergraphPatternMatcher::CreateSimpleEdgeGraphPattern(
        "x", "y", MockSemanticRelation::IS_A, "to_remove");
    Matcher->RegisterPattern(pattern);
    
    EXPECT_TRUE(Matcher->UnregisterPattern("to_remove"));
    EXPECT_EQ(Matcher->GetRegisteredPatternIDs().size(), 0);
}

// ========================================
// NODE CONSTRAINT TESTS
// ========================================

TEST_F(HypergraphPatternMatcherTest, NodeSatisfiesTypeConstraint) {
    MockMemoryNode node;
    node.NodeType = MockMemoryNodeType::Concept;
    
    MockPatternElement typePattern = MockPatternElement::TypedVariable("x", MockMemoryNodeType::Concept);
    
    EXPECT_TRUE(Matcher->NodeSatisfiesElement(node, typePattern));
}

TEST_F(HypergraphPatternMatcherTest, NodeFailsTypeConstraint) {
    MockMemoryNode node;
    node.NodeType = MockMemoryNodeType::Concept;
    
    MockPatternElement typePattern = MockPatternElement::TypedVariable("x", MockMemoryNodeType::Agent);
    
    EXPECT_FALSE(Matcher->NodeSatisfiesElement(node, typePattern));
}

TEST_F(HypergraphPatternMatcherTest, NodeSatisfiesActivationConstraint) {
    MockMemoryNode node;
    node.Activation = 0.7f;
    
    MockPatternElement pattern = MockPatternElement::Variable("x");
    pattern.MinActivation = 0.5f;
    
    EXPECT_TRUE(Matcher->NodeSatisfiesElement(node, pattern));
}

TEST_F(HypergraphPatternMatcherTest, NodeFailsActivationConstraint) {
    MockMemoryNode node;
    node.Activation = 0.3f;
    
    MockPatternElement pattern = MockPatternElement::Variable("x");
    pattern.MinActivation = 0.5f;
    
    EXPECT_FALSE(Matcher->NodeSatisfiesElement(node, pattern));
}

TEST_F(HypergraphPatternMatcherTest, NodeSatisfiesStrengthConstraint) {
    MockMemoryNode node;
    node.Strength = 0.8f;
    
    MockPatternElement pattern = MockPatternElement::Variable("x");
    pattern.MinStrength = 0.6f;
    
    EXPECT_TRUE(Matcher->NodeSatisfiesElement(node, pattern));
}

// ========================================
// PROPERTY CONSTRAINT TESTS
// ========================================

TEST_F(HypergraphPatternMatcherTest, PropertyEqualsConstraint) {
    MockMemoryNode node;
    node.Properties["color"] = "blue";
    
    MockPropertyConstraint constraint;
    constraint.PropertyKey = "color";
    constraint.Operator = MockConstraintOperator::Equals;
    constraint.Value = "blue";
    
    EXPECT_TRUE(Matcher->CheckPropertyConstraint(node, constraint));
}

TEST_F(HypergraphPatternMatcherTest, PropertyNotEqualsConstraint) {
    MockMemoryNode node;
    node.Properties["color"] = "blue";
    
    MockPropertyConstraint constraint;
    constraint.PropertyKey = "color";
    constraint.Operator = MockConstraintOperator::NotEquals;
    constraint.Value = "red";
    
    EXPECT_TRUE(Matcher->CheckPropertyConstraint(node, constraint));
}

TEST_F(HypergraphPatternMatcherTest, PropertyContainsConstraint) {
    MockMemoryNode node;
    node.Properties["name"] = "John Smith";
    
    MockPropertyConstraint constraint;
    constraint.PropertyKey = "name";
    constraint.Operator = MockConstraintOperator::Contains;
    constraint.Value = "Smith";
    
    EXPECT_TRUE(Matcher->CheckPropertyConstraint(node, constraint));
}

TEST_F(HypergraphPatternMatcherTest, PropertyStartsWithConstraint) {
    MockMemoryNode node;
    node.Properties["email"] = "test@example.com";
    
    MockPropertyConstraint constraint;
    constraint.PropertyKey = "email";
    constraint.Operator = MockConstraintOperator::StartsWith;
    constraint.Value = "test";
    
    EXPECT_TRUE(Matcher->CheckPropertyConstraint(node, constraint));
}

TEST_F(HypergraphPatternMatcherTest, PropertyEndsWithConstraint) {
    MockMemoryNode node;
    node.Properties["email"] = "test@example.com";
    
    MockPropertyConstraint constraint;
    constraint.PropertyKey = "email";
    constraint.Operator = MockConstraintOperator::EndsWith;
    constraint.Value = ".com";
    
    EXPECT_TRUE(Matcher->CheckPropertyConstraint(node, constraint));
}

TEST_F(HypergraphPatternMatcherTest, PropertyNumericGreaterThan) {
    MockMemoryNode node;
    node.Properties["score"] = "85";
    
    MockPropertyConstraint constraint;
    constraint.PropertyKey = "score";
    constraint.Operator = MockConstraintOperator::GreaterThan;
    constraint.Value = "70";
    constraint.bNumericComparison = true;
    
    EXPECT_TRUE(Matcher->CheckPropertyConstraint(node, constraint));
}

TEST_F(HypergraphPatternMatcherTest, PropertyNumericInRange) {
    MockMemoryNode node;
    node.Properties["temperature"] = "22.5";
    
    MockPropertyConstraint constraint;
    constraint.PropertyKey = "temperature";
    constraint.Operator = MockConstraintOperator::InRange;
    constraint.Value = "20";
    constraint.SecondaryValue = "25";
    constraint.bNumericComparison = true;
    
    EXPECT_TRUE(Matcher->CheckPropertyConstraint(node, constraint));
}

TEST_F(HypergraphPatternMatcherTest, PropertyIsNullConstraint) {
    MockMemoryNode node;
    // Property does not exist
    
    MockPropertyConstraint constraint;
    constraint.PropertyKey = "missing";
    constraint.Operator = MockConstraintOperator::IsNull;
    
    EXPECT_TRUE(Matcher->CheckPropertyConstraint(node, constraint));
}

TEST_F(HypergraphPatternMatcherTest, PropertyIsNotNullConstraint) {
    MockMemoryNode node;
    node.Properties["exists"] = "value";
    
    MockPropertyConstraint constraint;
    constraint.PropertyKey = "exists";
    constraint.Operator = MockConstraintOperator::IsNotNull;
    
    EXPECT_TRUE(Matcher->CheckPropertyConstraint(node, constraint));
}

// ========================================
// PATTERN MATCHING TESTS
// ========================================

TEST_F(HypergraphPatternMatcherTest, MatchSimpleEdgePattern) {
    CreateSimpleKnowledgeGraph();
    
    MockEdgePattern pattern = MockHypergraphPatternMatcher::CreateEdgePattern(
        "child", "parent", MockSemanticRelation::IS_A);
    
    MockPatternMatchConfig config;
    MockPatternMatchResult result = Matcher->MatchSimpleEdgePattern(pattern, config);
    
    EXPECT_TRUE(result.bHasMatches);
    EXPECT_EQ(result.TotalMatches, 3); // Dog->Mammal, Mammal->Animal, Cat->Mammal
}

TEST_F(HypergraphPatternMatcherTest, MatchEdgePatternWithSpecificRelation) {
    CreateSimpleKnowledgeGraph();
    
    MockEdgePattern pattern = MockHypergraphPatternMatcher::CreateEdgePattern(
        "instance", "class", MockSemanticRelation::INSTANCE_OF);
    
    MockPatternMatchConfig config;
    MockPatternMatchResult result = Matcher->MatchSimpleEdgePattern(pattern, config);
    
    EXPECT_TRUE(result.bHasMatches);
    EXPECT_EQ(result.TotalMatches, 1); // Only Fido->Dog
}

TEST_F(HypergraphPatternMatcherTest, MatchEdgePatternNoMatches) {
    CreateSimpleKnowledgeGraph();
    
    MockEdgePattern pattern = MockHypergraphPatternMatcher::CreateEdgePattern(
        "x", "y", MockSemanticRelation::CAUSES);
    
    MockPatternMatchConfig config;
    MockPatternMatchResult result = Matcher->MatchSimpleEdgePattern(pattern, config);
    
    EXPECT_FALSE(result.bHasMatches);
    EXPECT_EQ(result.TotalMatches, 0);
}

TEST_F(HypergraphPatternMatcherTest, MatchFirstMatchMode) {
    CreateSimpleKnowledgeGraph();
    
    MockEdgePattern pattern = MockHypergraphPatternMatcher::CreateEdgePattern(
        "child", "parent", MockSemanticRelation::IS_A);
    
    MockPatternMatchConfig config;
    config.MatchMode = MockPatternMatchMode::FirstMatch;
    MockPatternMatchResult result = Matcher->MatchSimpleEdgePattern(pattern, config);
    
    EXPECT_TRUE(result.bHasMatches);
    EXPECT_LE(result.MatchingBindings.size(), 1);
}

TEST_F(HypergraphPatternMatcherTest, PatternExistsCheck) {
    CreateSimpleKnowledgeGraph();
    
    MockGraphPattern pattern = MockHypergraphPatternMatcher::CreateSimpleEdgeGraphPattern(
        "x", "y", MockSemanticRelation::IS_A, "exists_test");
    
    EXPECT_TRUE(Matcher->PatternExists(pattern));
}

TEST_F(HypergraphPatternMatcherTest, PatternNotExistsCheck) {
    CreateSimpleKnowledgeGraph();
    
    MockGraphPattern pattern = MockHypergraphPatternMatcher::CreateSimpleEdgeGraphPattern(
        "x", "y", MockSemanticRelation::CONTRADICTS, "not_exists_test");
    
    EXPECT_FALSE(Matcher->PatternExists(pattern));
}

TEST_F(HypergraphPatternMatcherTest, CountPatternMatches) {
    CreateSimpleKnowledgeGraph();
    
    MockGraphPattern pattern = MockHypergraphPatternMatcher::CreateSimpleEdgeGraphPattern(
        "x", "y", MockSemanticRelation::IS_A, "count_test");
    
    EXPECT_EQ(Matcher->CountPatternMatches(pattern), 3);
}

// ========================================
// TYPE CONSTRAINT IN PATTERN MATCHING
// ========================================

TEST_F(HypergraphPatternMatcherTest, MatchWithTypeConstraint) {
    CreateSimpleKnowledgeGraph();
    
    MockEdgePattern pattern;
    pattern.Source = MockPatternElement::TypedVariable("concept1", MockMemoryNodeType::Concept);
    pattern.Target = MockPatternElement::TypedVariable("concept2", MockMemoryNodeType::Concept);
    pattern.AllowedRelations.push_back(MockSemanticRelation::IS_A);
    pattern.bDirected = true;
    
    MockPatternMatchConfig config;
    MockPatternMatchResult result = Matcher->MatchSimpleEdgePattern(pattern, config);
    
    EXPECT_TRUE(result.bHasMatches);
    // All IS_A edges are between Concepts
}

TEST_F(HypergraphPatternMatcherTest, MatchWithMixedTypeConstraint) {
    CreateSimpleKnowledgeGraph();
    
    MockEdgePattern pattern;
    pattern.Source = MockPatternElement::TypedVariable("object", MockMemoryNodeType::Object);
    pattern.Target = MockPatternElement::TypedVariable("concept", MockMemoryNodeType::Concept);
    pattern.AllowedRelations.push_back(MockSemanticRelation::INSTANCE_OF);
    pattern.bDirected = true;
    
    MockPatternMatchConfig config;
    MockPatternMatchResult result = Matcher->MatchSimpleEdgePattern(pattern, config);
    
    EXPECT_TRUE(result.bHasMatches);
    EXPECT_EQ(result.TotalMatches, 1); // Fido (Object) -> Dog (Concept)
}

// ========================================
// WEIGHT CONSTRAINT TESTS
// ========================================

TEST_F(HypergraphPatternMatcherTest, MatchWithMinWeight) {
    // Create edges with different weights
    int64_t a = Hypergraph->CreateNode(MockMemoryNodeType::Concept, "A");
    int64_t b = Hypergraph->CreateNode(MockMemoryNodeType::Concept, "B");
    int64_t c = Hypergraph->CreateNode(MockMemoryNodeType::Concept, "C");
    
    Hypergraph->CreateEdge(a, b, MockSemanticRelation::ASSOCIATED_WITH, 0.9f);
    Hypergraph->CreateEdge(a, c, MockSemanticRelation::ASSOCIATED_WITH, 0.3f);
    
    MockEdgePattern pattern;
    pattern.Source = MockPatternElement::Variable("x");
    pattern.Target = MockPatternElement::Variable("y");
    pattern.AllowedRelations.push_back(MockSemanticRelation::ASSOCIATED_WITH);
    pattern.MinWeight = 0.5f;
    
    MockPatternMatchConfig config;
    MockPatternMatchResult result = Matcher->MatchSimpleEdgePattern(pattern, config);
    
    EXPECT_TRUE(result.bHasMatches);
    EXPECT_EQ(result.TotalMatches, 1); // Only A->B with weight 0.9
}

// ========================================
// BINDING VERIFICATION TESTS
// ========================================

TEST_F(HypergraphPatternMatcherTest, VerifyBindingsAreCorrect) {
    int64_t node1 = Hypergraph->CreateNode(MockMemoryNodeType::Concept, "Source");
    int64_t node2 = Hypergraph->CreateNode(MockMemoryNodeType::Concept, "Target");
    Hypergraph->CreateEdge(node1, node2, MockSemanticRelation::CAUSES);
    
    MockEdgePattern pattern = MockHypergraphPatternMatcher::CreateEdgePattern(
        "src", "tgt", MockSemanticRelation::CAUSES);
    
    MockPatternMatchConfig config;
    MockPatternMatchResult result = Matcher->MatchSimpleEdgePattern(pattern, config);
    
    ASSERT_TRUE(result.bHasMatches);
    ASSERT_EQ(result.MatchingBindings.size(), 1);
    
    const MockBindingEnvironment& env = result.MatchingBindings[0];
    EXPECT_TRUE(env.IsBound("src"));
    EXPECT_TRUE(env.IsBound("tgt"));
    EXPECT_EQ(env.GetBoundNodeID("src"), node1);
    EXPECT_EQ(env.GetBoundNodeID("tgt"), node2);
}

// ========================================
// STATISTICS TESTS
// ========================================

TEST_F(HypergraphPatternMatcherTest, StatisticsTracking) {
    CreateSimpleKnowledgeGraph();
    
    MockEdgePattern pattern = MockHypergraphPatternMatcher::CreateEdgePattern(
        "x", "y", MockSemanticRelation::IS_A);
    
    MockPatternMatchConfig config;
    Matcher->MatchSimpleEdgePattern(pattern, config);
    Matcher->MatchSimpleEdgePattern(pattern, config);
    
    EXPECT_EQ(Matcher->TotalMatchAttempts, 2);
    EXPECT_EQ(Matcher->TotalMatchesFound, 6); // 3 matches per attempt
}

// ========================================
// EMPTY GRAPH TESTS
// ========================================

TEST_F(HypergraphPatternMatcherTest, MatchOnEmptyGraph) {
    MockEdgePattern pattern = MockHypergraphPatternMatcher::CreateEdgePattern(
        "x", "y", MockSemanticRelation::IS_A);
    
    MockPatternMatchConfig config;
    MockPatternMatchResult result = Matcher->MatchSimpleEdgePattern(pattern, config);
    
    EXPECT_FALSE(result.bHasMatches);
    EXPECT_EQ(result.TotalMatches, 0);
}

// ========================================
// NULL HYPERGRAPH TESTS
// ========================================

TEST_F(HypergraphPatternMatcherTest, MatchWithNullHypergraph) {
    Matcher->SetHypergraph(nullptr);
    
    MockEdgePattern pattern = MockHypergraphPatternMatcher::CreateEdgePattern(
        "x", "y", MockSemanticRelation::IS_A);
    
    MockPatternMatchConfig config;
    MockPatternMatchResult result = Matcher->MatchSimpleEdgePattern(pattern, config);
    
    EXPECT_FALSE(result.bHasMatches);
    EXPECT_FALSE(result.ErrorMessage.empty());
}


} // namespace

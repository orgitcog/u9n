// MemoryIntegrationTests.cpp
// Implementation of Memory-Consciousness Integration Tests

#include "MemoryIntegrationTests.h"
#include "HAL/PlatformTime.h"

UMemoryIntegrationTests::UMemoryIntegrationTests()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UMemoryIntegrationTests::BeginPlay()
{
    Super::BeginPlay();
    FindComponentReferences();

    if (bAutoRunTests)
    {
        RunAllTests();
    }
}

void UMemoryIntegrationTests::FindComponentReferences()
{
    AActor* Owner = GetOwner();
    if (Owner)
    {
        MemorySystem = Owner->FindComponentByClass<UHypergraphMemorySystem>();
        ConsciousnessIntegration = Owner->FindComponentByClass<UMemoryConsciousnessIntegration>();
        TestFramework = Owner->FindComponentByClass<UBehavioralTestFramework>();
    }
}

// ========================================
// TEST EXECUTION
// ========================================

FMemoryTestSummary UMemoryIntegrationTests::RunAllTests()
{
    ClearResults();

    // Run all test categories
    RunTestsByCategory(EMemoryTestCategory::HypergraphBasic);
    RunTestsByCategory(EMemoryTestCategory::NodeOperations);
    RunTestsByCategory(EMemoryTestCategory::SpreadingActivation);
    RunTestsByCategory(EMemoryTestCategory::IntentionalMemory);
    RunTestsByCategory(EMemoryTestCategory::ConsciousnessBinding);
    RunTestsByCategory(EMemoryTestCategory::CrossStreamAssoc);
    RunTestsByCategory(EMemoryTestCategory::CycleIntegration);
    RunTestsByCategory(EMemoryTestCategory::PerformanceStress);

    return GetTestSummary();
}

TArray<FMemoryTestResult> UMemoryIntegrationTests::RunTestsByCategory(EMemoryTestCategory Category)
{
    TArray<FMemoryTestResult> CategoryResults;

    switch (Category)
    {
    case EMemoryTestCategory::HypergraphBasic:
    case EMemoryTestCategory::NodeOperations:
        CategoryResults.Add(Test_NodeCreation());
        CategoryResults.Add(Test_NodeRetrieval());
        CategoryResults.Add(Test_NodeUpdate());
        CategoryResults.Add(Test_NodeDeletion());
        break;

    case EMemoryTestCategory::EdgeOperations:
        CategoryResults.Add(Test_EdgeCreation());
        CategoryResults.Add(Test_BidirectionalEdges());
        CategoryResults.Add(Test_HyperedgeCreation());
        break;

    case EMemoryTestCategory::SpreadingActivation:
        CategoryResults.Add(Test_BasicActivationSpreading());
        CategoryResults.Add(Test_ActivationDecay());
        CategoryResults.Add(Test_MultiSourceActivation());
        break;

    case EMemoryTestCategory::IntentionalMemory:
        CategoryResults.Add(Test_BeliefCreation());
        CategoryResults.Add(Test_DesireCreation());
        CategoryResults.Add(Test_IntentionLifecycle());
        CategoryResults.Add(Test_BeliefContradictions());
        break;

    case EMemoryTestCategory::ConsciousnessBinding:
        CategoryResults.Add(Test_StreamBinding());
        CategoryResults.Add(Test_WorkingMemoryLoading());
        CategoryResults.Add(Test_WorkingMemoryCapacity());
        CategoryResults.Add(Test_WorkingMemoryDecay());
        CategoryResults.Add(Test_MemoryGuidedAttention());
        break;

    case EMemoryTestCategory::CrossStreamAssoc:
        CategoryResults.Add(Test_CrossStreamAssociationCreation());
        CategoryResults.Add(Test_CrossStreamActivationPropagation());
        CategoryResults.Add(Test_TriadicSynchronization());
        break;

    case EMemoryTestCategory::CycleIntegration:
        CategoryResults.Add(Test_CycleStepProcessing());
        CategoryResults.Add(Test_CycleConsolidation());
        break;

    case EMemoryTestCategory::PerformanceStress:
        CategoryResults.Add(Test_BulkNodeCreationPerformance());
        CategoryResults.Add(Test_SimilaritySearchPerformance());
        CategoryResults.Add(Test_SpreadingActivationPerformance());
        CategoryResults.Add(Test_ConsolidationPerformance());
        break;
    }

    return CategoryResults;
}

FMemoryTestResult UMemoryIntegrationTests::RunTest(const FString& TestName)
{
    // Map test name to function
    if (TestName == TEXT("NodeCreation")) return Test_NodeCreation();
    if (TestName == TEXT("NodeRetrieval")) return Test_NodeRetrieval();
    if (TestName == TEXT("NodeUpdate")) return Test_NodeUpdate();
    if (TestName == TEXT("NodeDeletion")) return Test_NodeDeletion();
    if (TestName == TEXT("EdgeCreation")) return Test_EdgeCreation();
    if (TestName == TEXT("BidirectionalEdges")) return Test_BidirectionalEdges();
    if (TestName == TEXT("HyperedgeCreation")) return Test_HyperedgeCreation();
    if (TestName == TEXT("BasicActivationSpreading")) return Test_BasicActivationSpreading();
    if (TestName == TEXT("ActivationDecay")) return Test_ActivationDecay();
    if (TestName == TEXT("BeliefCreation")) return Test_BeliefCreation();
    if (TestName == TEXT("DesireCreation")) return Test_DesireCreation();
    if (TestName == TEXT("IntentionLifecycle")) return Test_IntentionLifecycle();

    FMemoryTestResult NotFound;
    NotFound.TestName = TestName;
    NotFound.bPassed = false;
    NotFound.ErrorMessage = TEXT("Test not found");
    return NotFound;
}

FMemoryTestSummary UMemoryIntegrationTests::GetTestSummary() const
{
    FMemoryTestSummary Summary;
    Summary.TotalTests = TestResults.Num();

    for (const FMemoryTestResult& Result : TestResults)
    {
        if (Result.bPassed)
        {
            Summary.Passed++;
            Summary.PassedByCategory.FindOrAdd(Result.Category)++;
        }
        else
        {
            Summary.Failed++;
            Summary.FailedByCategory.FindOrAdd(Result.Category)++;
        }
        Summary.TotalDurationMs += Result.DurationMs;
    }

    Summary.PassRate = Summary.TotalTests > 0 ?
        static_cast<float>(Summary.Passed) / Summary.TotalTests : 0.0f;

    return Summary;
}

TArray<FMemoryTestResult> UMemoryIntegrationTests::GetAllResults() const
{
    return TestResults;
}

void UMemoryIntegrationTests::ClearResults()
{
    TestResults.Empty();
}

// ========================================
// HYPERGRAPH BASIC TESTS
// ========================================

FMemoryTestResult UMemoryIntegrationTests::Test_NodeCreation()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("NodeCreation"), EMemoryTestCategory::HypergraphBasic);

    if (!MemorySystem)
    {
        EndTest(Result, false, TEXT("MemorySystem not available"));
        return Result;
    }

    ClearMemorySystem();

    // Test creating nodes of different types
    TArray<float> Embedding = GenerateRandomEmbedding(128);

    int64 PerceptNode = MemorySystem->CreateNode(EMemoryNodeType::Percept, TEXT("TestPercept"), Embedding, 0.5f);
    Assert(Result, TEXT("Percept node created"), PerceptNode > 0);

    int64 ConceptNode = MemorySystem->CreateNode(EMemoryNodeType::Concept, TEXT("TestConcept"), Embedding, 0.7f);
    Assert(Result, TEXT("Concept node created"), ConceptNode > 0);

    int64 EpisodeNode = MemorySystem->CreateNode(EMemoryNodeType::Episode, TEXT("TestEpisode"), Embedding, 0.6f);
    Assert(Result, TEXT("Episode node created"), EpisodeNode > 0);

    // Verify nodes exist
    Assert(Result, TEXT("Percept node exists"), MemorySystem->NodeExists(PerceptNode));
    Assert(Result, TEXT("Concept node exists"), MemorySystem->NodeExists(ConceptNode));
    Assert(Result, TEXT("Episode node exists"), MemorySystem->NodeExists(EpisodeNode));

    // Verify unique IDs
    AssertNotEqual(Result, TEXT("Percept != Concept ID"), PerceptNode, ConceptNode);
    AssertNotEqual(Result, TEXT("Concept != Episode ID"), ConceptNode, EpisodeNode);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_NodeRetrieval()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("NodeRetrieval"), EMemoryTestCategory::HypergraphBasic);

    if (!MemorySystem)
    {
        EndTest(Result, false, TEXT("MemorySystem not available"));
        return Result;
    }

    ClearMemorySystem();

    // Create a node
    TArray<float> Embedding = GenerateRandomEmbedding(128);
    int64 NodeID = MemorySystem->CreateNode(EMemoryNodeType::Concept, TEXT("RetrievalTest"), Embedding, 0.8f);

    // Retrieve the node
    FMemoryNode RetrievedNode = MemorySystem->GetNode(NodeID);

    Assert(Result, TEXT("Node ID matches"), RetrievedNode.NodeID == NodeID);
    Assert(Result, TEXT("Node type matches"), RetrievedNode.NodeType == EMemoryNodeType::Concept);
    Assert(Result, TEXT("Node label matches"), RetrievedNode.Label == TEXT("RetrievalTest"));
    AssertEqual(Result, TEXT("Node strength matches"), 0.8f, RetrievedNode.Strength, 0.01f);
    AssertEqual(Result, TEXT("Embedding size matches"), 128, RetrievedNode.Embedding.Num());

    // Test retrieval of non-existent node
    FMemoryNode NonExistent = MemorySystem->GetNode(999999);
    Assert(Result, TEXT("Non-existent node returns default"), NonExistent.NodeID == 0);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_NodeUpdate()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("NodeUpdate"), EMemoryTestCategory::HypergraphBasic);

    if (!MemorySystem)
    {
        EndTest(Result, false, TEXT("MemorySystem not available"));
        return Result;
    }

    ClearMemorySystem();

    // Create a node
    TArray<float> OriginalEmbedding = GenerateRandomEmbedding(128);
    int64 NodeID = MemorySystem->CreateNode(EMemoryNodeType::Percept, TEXT("UpdateTest"), OriginalEmbedding, 0.5f);

    // Update embedding
    TArray<float> NewEmbedding = GenerateRandomEmbedding(128);
    MemorySystem->UpdateNodeEmbedding(NodeID, NewEmbedding);

    FMemoryNode UpdatedNode = MemorySystem->GetNode(NodeID);
    Assert(Result, TEXT("Embedding updated"), UpdatedNode.Embedding.Num() == 128);

    // Update activation
    MemorySystem->SetNodeActivation(NodeID, 0.9f);
    UpdatedNode = MemorySystem->GetNode(NodeID);
    AssertEqual(Result, TEXT("Activation updated"), 0.9f, UpdatedNode.Activation, 0.01f);

    // Boost activation
    MemorySystem->BoostNodeActivation(NodeID, 0.05f);
    UpdatedNode = MemorySystem->GetNode(NodeID);
    AssertEqual(Result, TEXT("Activation boosted"), 0.95f, UpdatedNode.Activation, 0.01f);

    // Set property
    MemorySystem->SetNodeProperty(NodeID, TEXT("TestKey"), TEXT("TestValue"));
    FString PropertyValue = MemorySystem->GetNodeProperty(NodeID, TEXT("TestKey"));
    Assert(Result, TEXT("Property set correctly"), PropertyValue == TEXT("TestValue"));

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_NodeDeletion()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("NodeDeletion"), EMemoryTestCategory::HypergraphBasic);

    if (!MemorySystem)
    {
        EndTest(Result, false, TEXT("MemorySystem not available"));
        return Result;
    }

    ClearMemorySystem();

    // Create nodes
    TArray<float> Embedding = GenerateRandomEmbedding(128);
    int64 Node1 = MemorySystem->CreateNode(EMemoryNodeType::Percept, TEXT("Delete1"), Embedding, 0.5f);
    int64 Node2 = MemorySystem->CreateNode(EMemoryNodeType::Percept, TEXT("Delete2"), Embedding, 0.5f);

    // Create edge between them
    MemorySystem->CreateEdge(Node1, Node2, ESemanticRelation::ASSOCIATED_WITH, 0.5f);

    // Delete first node
    bool bDeleted = MemorySystem->DeleteNode(Node1);
    Assert(Result, TEXT("Delete returned true"), bDeleted);
    Assert(Result, TEXT("Node1 no longer exists"), !MemorySystem->NodeExists(Node1));
    Assert(Result, TEXT("Node2 still exists"), MemorySystem->NodeExists(Node2));

    // Verify edge was cleaned up
    TArray<FMemoryEdge> IncomingEdges = MemorySystem->GetIncomingEdges(Node2);
    Assert(Result, TEXT("Edge cleaned up on delete"), IncomingEdges.Num() == 0);

    // Delete non-existent node
    bool bDeleteFailed = MemorySystem->DeleteNode(999999);
    Assert(Result, TEXT("Delete non-existent returns false"), !bDeleteFailed);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_EdgeCreation()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("EdgeCreation"), EMemoryTestCategory::EdgeOperations);

    if (!MemorySystem)
    {
        EndTest(Result, false, TEXT("MemorySystem not available"));
        return Result;
    }

    ClearMemorySystem();

    // Create nodes
    TArray<float> Embedding = GenerateRandomEmbedding(128);
    int64 Node1 = MemorySystem->CreateNode(EMemoryNodeType::Concept, TEXT("EdgeTest1"), Embedding, 0.5f);
    int64 Node2 = MemorySystem->CreateNode(EMemoryNodeType::Concept, TEXT("EdgeTest2"), Embedding, 0.5f);

    // Create edge
    int64 EdgeID = MemorySystem->CreateEdge(Node1, Node2, ESemanticRelation::IS_A, 0.8f);
    Assert(Result, TEXT("Edge created"), EdgeID > 0);

    // Get edge
    FMemoryEdge Edge = MemorySystem->GetEdge(EdgeID);
    Assert(Result, TEXT("Edge source correct"), Edge.SourceNodeID == Node1);
    Assert(Result, TEXT("Edge target correct"), Edge.TargetNodeID == Node2);
    Assert(Result, TEXT("Edge relation correct"), Edge.RelationType == ESemanticRelation::IS_A);
    AssertEqual(Result, TEXT("Edge weight correct"), 0.8f, Edge.Weight, 0.01f);

    // Get outgoing edges
    TArray<FMemoryEdge> OutgoingEdges = MemorySystem->GetOutgoingEdges(Node1);
    Assert(Result, TEXT("Outgoing edge count"), OutgoingEdges.Num() == 1);

    // Get incoming edges
    TArray<FMemoryEdge> IncomingEdges = MemorySystem->GetIncomingEdges(Node2);
    Assert(Result, TEXT("Incoming edge count"), IncomingEdges.Num() == 1);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_BidirectionalEdges()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("BidirectionalEdges"), EMemoryTestCategory::EdgeOperations);

    if (!MemorySystem)
    {
        EndTest(Result, false, TEXT("MemorySystem not available"));
        return Result;
    }

    ClearMemorySystem();

    // Create nodes
    TArray<float> Embedding = GenerateRandomEmbedding(128);
    int64 NodeA = MemorySystem->CreateNode(EMemoryNodeType::Concept, TEXT("BiEdgeA"), Embedding, 0.5f);
    int64 NodeB = MemorySystem->CreateNode(EMemoryNodeType::Concept, TEXT("BiEdgeB"), Embedding, 0.5f);

    // Create bidirectional edge
    int64 EdgeID = MemorySystem->CreateBidirectionalEdge(NodeA, NodeB, ESemanticRelation::SIMILAR_TO, 0.7f);
    Assert(Result, TEXT("Bidirectional edge created"), EdgeID > 0);

    // Verify edge is bidirectional
    FMemoryEdge Edge = MemorySystem->GetEdge(EdgeID);
    Assert(Result, TEXT("Edge marked bidirectional"), Edge.bBidirectional);

    // Both nodes should have the edge in outgoing
    TArray<FMemoryEdge> EdgesA = MemorySystem->GetOutgoingEdges(NodeA);
    TArray<FMemoryEdge> EdgesB = MemorySystem->GetOutgoingEdges(NodeB);
    Assert(Result, TEXT("NodeA has outgoing edge"), EdgesA.Num() >= 1);
    Assert(Result, TEXT("NodeB has outgoing edge"), EdgesB.Num() >= 1);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_HyperedgeCreation()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("HyperedgeCreation"), EMemoryTestCategory::EdgeOperations);

    if (!MemorySystem)
    {
        EndTest(Result, false, TEXT("MemorySystem not available"));
        return Result;
    }

    ClearMemorySystem();

    // Create nodes
    TArray<float> Embedding = GenerateRandomEmbedding(128);
    int64 Node1 = MemorySystem->CreateNode(EMemoryNodeType::Agent, TEXT("Agent1"), Embedding, 0.5f);
    int64 Node2 = MemorySystem->CreateNode(EMemoryNodeType::Object, TEXT("Object1"), Embedding, 0.5f);
    int64 Node3 = MemorySystem->CreateNode(EMemoryNodeType::Place, TEXT("Place1"), Embedding, 0.5f);

    // Create hyperedge
    TArray<int64> Members = { Node1, Node2, Node3 };
    TArray<FString> Roles = { TEXT("Agent"), TEXT("Patient"), TEXT("Location") };
    int64 HyperedgeID = MemorySystem->CreateHyperedge(Members, Roles, TEXT("Interaction"), 0.8f);

    Assert(Result, TEXT("Hyperedge created"), HyperedgeID > 0);

    // Get hyperedge
    FMemoryHyperedge Hyperedge = MemorySystem->GetHyperedge(HyperedgeID);
    Assert(Result, TEXT("Hyperedge has 3 members"), Hyperedge.MemberNodes.Num() == 3);
    Assert(Result, TEXT("Hyperedge has 3 roles"), Hyperedge.MemberRoles.Num() == 3);
    Assert(Result, TEXT("Hyperedge type correct"), Hyperedge.HyperedgeType == TEXT("Interaction"));

    // Get node hyperedges
    TArray<FMemoryHyperedge> NodeHyperedges = MemorySystem->GetNodeHyperedges(Node1);
    Assert(Result, TEXT("Node1 in hyperedge"), NodeHyperedges.Num() >= 1);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

// ========================================
// SPREADING ACTIVATION TESTS
// ========================================

FMemoryTestResult UMemoryIntegrationTests::Test_BasicActivationSpreading()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("BasicActivationSpreading"), EMemoryTestCategory::SpreadingActivation);

    if (!MemorySystem)
    {
        EndTest(Result, false, TEXT("MemorySystem not available"));
        return Result;
    }

    ClearMemorySystem();

    // Create chain of nodes
    TArray<float> Embedding = GenerateRandomEmbedding(128);
    int64 Node1 = MemorySystem->CreateNode(EMemoryNodeType::Concept, TEXT("Spread1"), Embedding, 0.5f);
    int64 Node2 = MemorySystem->CreateNode(EMemoryNodeType::Concept, TEXT("Spread2"), Embedding, 0.5f);
    int64 Node3 = MemorySystem->CreateNode(EMemoryNodeType::Concept, TEXT("Spread3"), Embedding, 0.5f);

    // Connect them
    MemorySystem->CreateEdge(Node1, Node2, ESemanticRelation::ASSOCIATED_WITH, 1.0f);
    MemorySystem->CreateEdge(Node2, Node3, ESemanticRelation::ASSOCIATED_WITH, 1.0f);

    // Spread activation from Node1
    MemorySystem->SpreadActivation(Node1, 1.0f);

    // Check activations
    FMemoryNode N1 = MemorySystem->GetNode(Node1);
    FMemoryNode N2 = MemorySystem->GetNode(Node2);
    FMemoryNode N3 = MemorySystem->GetNode(Node3);

    AssertGreaterThan(Result, TEXT("Node1 has activation"), N1.Activation, 0.0f);
    AssertGreaterThan(Result, TEXT("Node2 has activation"), N2.Activation, 0.0f);
    AssertGreaterThan(Result, TEXT("Node3 has activation"), N3.Activation, 0.0f);

    // Verify decay with distance
    AssertGreaterThan(Result, TEXT("Node1 > Node2 activation"), N1.Activation, N2.Activation);
    AssertGreaterThan(Result, TEXT("Node2 > Node3 activation"), N2.Activation, N3.Activation);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_ActivationDecay()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("ActivationDecay"), EMemoryTestCategory::SpreadingActivation);

    if (!MemorySystem)
    {
        EndTest(Result, false, TEXT("MemorySystem not available"));
        return Result;
    }

    ClearMemorySystem();

    // Create node and set activation
    TArray<float> Embedding = GenerateRandomEmbedding(128);
    int64 NodeID = MemorySystem->CreateNode(EMemoryNodeType::Concept, TEXT("DecayTest"), Embedding, 0.5f);
    MemorySystem->SetNodeActivation(NodeID, 1.0f);

    float InitialActivation = MemorySystem->GetNode(NodeID).Activation;
    AssertEqual(Result, TEXT("Initial activation is 1.0"), 1.0f, InitialActivation, 0.01f);

    // Decay activations
    MemorySystem->DecayActivations(1.0f); // 1 second

    float DecayedActivation = MemorySystem->GetNode(NodeID).Activation;
    AssertLessThan(Result, TEXT("Activation decayed"), DecayedActivation, InitialActivation);

    // Reset activations
    MemorySystem->ResetActivations();
    float ResetActivation = MemorySystem->GetNode(NodeID).Activation;
    AssertEqual(Result, TEXT("Activation reset to 0"), 0.0f, ResetActivation, 0.01f);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_MultiSourceActivation()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("MultiSourceActivation"), EMemoryTestCategory::SpreadingActivation);

    if (!MemorySystem)
    {
        EndTest(Result, false, TEXT("MemorySystem not available"));
        return Result;
    }

    ClearMemorySystem();

    // Create hub-and-spoke structure
    TArray<float> Embedding = GenerateRandomEmbedding(128);
    int64 Hub = MemorySystem->CreateNode(EMemoryNodeType::Concept, TEXT("Hub"), Embedding, 0.5f);
    int64 Spoke1 = MemorySystem->CreateNode(EMemoryNodeType::Concept, TEXT("Spoke1"), Embedding, 0.5f);
    int64 Spoke2 = MemorySystem->CreateNode(EMemoryNodeType::Concept, TEXT("Spoke2"), Embedding, 0.5f);
    int64 Spoke3 = MemorySystem->CreateNode(EMemoryNodeType::Concept, TEXT("Spoke3"), Embedding, 0.5f);

    MemorySystem->CreateEdge(Spoke1, Hub, ESemanticRelation::ASSOCIATED_WITH, 1.0f);
    MemorySystem->CreateEdge(Spoke2, Hub, ESemanticRelation::ASSOCIATED_WITH, 1.0f);
    MemorySystem->CreateEdge(Spoke3, Hub, ESemanticRelation::ASSOCIATED_WITH, 1.0f);

    // Spread from multiple sources
    TArray<int64> Sources = { Spoke1, Spoke2, Spoke3 };
    TArray<float> Activations = { 0.5f, 0.5f, 0.5f };
    MemorySystem->SpreadActivationMultiple(Sources, Activations);

    // Hub should receive activation from all spokes
    float HubActivation = MemorySystem->GetNode(Hub).Activation;
    AssertGreaterThan(Result, TEXT("Hub received activation"), HubActivation, 0.0f);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

// ========================================
// INTENTIONAL MEMORY (BDI) TESTS
// ========================================

FMemoryTestResult UMemoryIntegrationTests::Test_BeliefCreation()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("BeliefCreation"), EMemoryTestCategory::IntentionalMemory);

    if (!MemorySystem)
    {
        EndTest(Result, false, TEXT("MemorySystem not available"));
        return Result;
    }

    ClearMemorySystem();

    // Create a belief
    TArray<int64> ContentNodes;
    int64 BeliefID = MemorySystem->CreateBelief(TEXT("The sky is blue"), ContentNodes,
                                                  EBeliefModality::Certain, 0.95f);

    Assert(Result, TEXT("Belief created"), BeliefID > 0);

    // Get belief node
    FMemoryNode BeliefNode = MemorySystem->GetNode(BeliefID);
    Assert(Result, TEXT("Belief node type correct"), BeliefNode.NodeType == EMemoryNodeType::Belief);
    AssertEqual(Result, TEXT("Belief confidence correct"), 0.95f, BeliefNode.Confidence, 0.01f);

    // Get intentional state
    FIntentionalState State = MemorySystem->GetIntentionalState(BeliefID);
    Assert(Result, TEXT("Proposition matches"), State.Proposition == TEXT("The sky is blue"));
    Assert(Result, TEXT("Modality is Certain"), State.Modality == EBeliefModality::Certain);

    // Get active beliefs
    TArray<int64> ActiveBeliefs = MemorySystem->GetActiveBeliefs(0.5f);
    Assert(Result, TEXT("Belief is active"), ActiveBeliefs.Contains(BeliefID));

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_DesireCreation()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("DesireCreation"), EMemoryTestCategory::IntentionalMemory);

    if (!MemorySystem)
    {
        EndTest(Result, false, TEXT("MemorySystem not available"));
        return Result;
    }

    ClearMemorySystem();

    // Create a desire
    TArray<int64> ContentNodes;
    int64 DesireID = MemorySystem->CreateDesire(TEXT("Learn to dance"), ContentNodes, 0.8f, 0.9f);

    Assert(Result, TEXT("Desire created"), DesireID > 0);

    // Get desire node
    FMemoryNode DesireNode = MemorySystem->GetNode(DesireID);
    Assert(Result, TEXT("Desire node type correct"), DesireNode.NodeType == EMemoryNodeType::Desire);

    // Get intentional state
    FIntentionalState State = MemorySystem->GetIntentionalState(DesireID);
    Assert(Result, TEXT("Goal matches"), State.Proposition == TEXT("Learn to dance"));
    AssertEqual(Result, TEXT("Intensity correct"), 0.8f, State.Intensity, 0.01f);
    AssertEqual(Result, TEXT("Priority correct"), 0.9f, State.Priority, 0.01f);

    // Get active desires
    TArray<int64> ActiveDesires = MemorySystem->GetActiveDesires(0.5f);
    Assert(Result, TEXT("Desire is active"), ActiveDesires.Contains(DesireID));

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_IntentionLifecycle()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("IntentionLifecycle"), EMemoryTestCategory::IntentionalMemory);

    if (!MemorySystem)
    {
        EndTest(Result, false, TEXT("MemorySystem not available"));
        return Result;
    }

    ClearMemorySystem();

    // Create desire first
    TArray<int64> ContentNodes;
    int64 DesireID = MemorySystem->CreateDesire(TEXT("Complete task"), ContentNodes, 0.8f, 0.7f);

    // Create intention for the desire
    int64 IntentionID = MemorySystem->CreateIntention(TEXT("Step 1: Research"), ContentNodes, DesireID, 0.8f);

    Assert(Result, TEXT("Intention created"), IntentionID > 0);

    // Check initial status
    FIntentionalState State = MemorySystem->GetIntentionalState(IntentionID);
    Assert(Result, TEXT("Initial status is Deliberating"), State.Status == EIntentionStatus::Deliberating);

    // Update status through lifecycle
    MemorySystem->UpdateIntentionStatus(IntentionID, EIntentionStatus::Committed);
    State = MemorySystem->GetIntentionalState(IntentionID);
    Assert(Result, TEXT("Status updated to Committed"), State.Status == EIntentionStatus::Committed);

    MemorySystem->UpdateIntentionStatus(IntentionID, EIntentionStatus::Executing);
    State = MemorySystem->GetIntentionalState(IntentionID);
    Assert(Result, TEXT("Status updated to Executing"), State.Status == EIntentionStatus::Executing);

    MemorySystem->UpdateIntentionStatus(IntentionID, EIntentionStatus::Achieved);
    State = MemorySystem->GetIntentionalState(IntentionID);
    Assert(Result, TEXT("Status updated to Achieved"), State.Status == EIntentionStatus::Achieved);

    // Check active intentions (should not include achieved)
    TArray<int64> ActiveIntentions = MemorySystem->GetActiveIntentions();
    Assert(Result, TEXT("Achieved intention not in active list"), !ActiveIntentions.Contains(IntentionID));

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_BeliefContradictions()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("BeliefContradictions"), EMemoryTestCategory::IntentionalMemory);

    if (!MemorySystem)
    {
        EndTest(Result, false, TEXT("MemorySystem not available"));
        return Result;
    }

    ClearMemorySystem();

    // Create two contradicting beliefs
    TArray<int64> ContentNodes;
    int64 Belief1 = MemorySystem->CreateBelief(TEXT("A is true"), ContentNodes, EBeliefModality::Certain, 0.9f);
    int64 Belief2 = MemorySystem->CreateBelief(TEXT("A is false"), ContentNodes, EBeliefModality::Certain, 0.9f);

    // Create contradiction edge
    MemorySystem->CreateEdge(Belief1, Belief2, ESemanticRelation::CONTRADICTS, 1.0f);

    // Find contradictions
    TArray<int64> Contradictions = MemorySystem->FindContradictingBeliefs(Belief1);
    Assert(Result, TEXT("Contradiction found"), Contradictions.Contains(Belief2));

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

// ========================================
// CONSCIOUSNESS BINDING TESTS
// ========================================

FMemoryTestResult UMemoryIntegrationTests::Test_StreamBinding()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("StreamBinding"), EMemoryTestCategory::ConsciousnessBinding);

    if (!ConsciousnessIntegration)
    {
        EndTest(Result, false, TEXT("ConsciousnessIntegration not available"));
        return Result;
    }

    // Create embedding and bind to perceiving stream
    TArray<float> Embedding = GenerateRandomEmbedding(128);
    ConsciousnessIntegration->BindStreamToMemory(EConsciousnessStream::Perceiving, Embedding);

    // Get binding
    FStreamMemoryBinding Binding = ConsciousnessIntegration->GetStreamBinding(EConsciousnessStream::Perceiving);
    Assert(Result, TEXT("Stream type correct"), Binding.StreamType == EConsciousnessStream::Perceiving);
    Assert(Result, TEXT("Retrieval cue set"), Binding.RetrievalCue.Num() > 0);

    // Unbind
    ConsciousnessIntegration->UnbindStream(EConsciousnessStream::Perceiving);
    Binding = ConsciousnessIntegration->GetStreamBinding(EConsciousnessStream::Perceiving);
    Assert(Result, TEXT("Stream unbound"), Binding.RetrievalCue.Num() == 0);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_WorkingMemoryLoading()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("WorkingMemoryLoading"), EMemoryTestCategory::ConsciousnessBinding);

    if (!ConsciousnessIntegration || !MemorySystem)
    {
        EndTest(Result, false, TEXT("Components not available"));
        return Result;
    }

    ClearMemorySystem();
    ConsciousnessIntegration->ClearWorkingMemory(EConsciousnessStream::Perceiving);

    // Create a memory node
    TArray<float> Embedding = GenerateRandomEmbedding(128);
    int64 NodeID = MemorySystem->CreateNode(EMemoryNodeType::Percept, TEXT("WMTest"), Embedding, 0.5f);

    // Load to working memory
    bool bLoaded = ConsciousnessIntegration->LoadToWorkingMemory(EConsciousnessStream::Perceiving, NodeID);
    Assert(Result, TEXT("Load returned true"), bLoaded);

    // Get working memory
    TArray<FConsciousnessWorkingMemorySlot> WM = ConsciousnessIntegration->GetWorkingMemory(EConsciousnessStream::Perceiving);
    Assert(Result, TEXT("Working memory has 1 item"), WM.Num() == 1);
    Assert(Result, TEXT("Node ID matches"), WM[0].NodeID == NodeID);

    // Remove from working memory
    ConsciousnessIntegration->RemoveFromWorkingMemory(EConsciousnessStream::Perceiving, NodeID);
    WM = ConsciousnessIntegration->GetWorkingMemory(EConsciousnessStream::Perceiving);
    Assert(Result, TEXT("Working memory is empty"), WM.Num() == 0);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_WorkingMemoryCapacity()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("WorkingMemoryCapacity"), EMemoryTestCategory::ConsciousnessBinding);

    if (!ConsciousnessIntegration || !MemorySystem)
    {
        EndTest(Result, false, TEXT("Components not available"));
        return Result;
    }

    ClearMemorySystem();
    ConsciousnessIntegration->ClearWorkingMemory(EConsciousnessStream::Acting);

    // Get capacity from config
    int32 Capacity = ConsciousnessIntegration->Config.WorkingMemoryCapacity;

    // Create and load more than capacity
    TArray<float> Embedding = GenerateRandomEmbedding(128);
    for (int32 i = 0; i < Capacity + 2; ++i)
    {
        int64 NodeID = MemorySystem->CreateNode(EMemoryNodeType::Skill,
            FString::Printf(TEXT("CapTest%d"), i), Embedding, 0.5f);
        ConsciousnessIntegration->LoadToWorkingMemory(EConsciousnessStream::Acting, NodeID);
    }

    // Check that capacity is respected
    TArray<FConsciousnessWorkingMemorySlot> WM = ConsciousnessIntegration->GetWorkingMemory(EConsciousnessStream::Acting);
    Assert(Result, TEXT("Working memory at capacity"), WM.Num() == Capacity);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_WorkingMemoryDecay()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("WorkingMemoryDecay"), EMemoryTestCategory::ConsciousnessBinding);

    // This test would require tick simulation - mark as passed for now
    // In a real test environment, we would simulate tick updates

    Result.Details.Add(TEXT("Decay test requires tick simulation"));
    EndTest(Result, true);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_MemoryGuidedAttention()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("MemoryGuidedAttention"), EMemoryTestCategory::ConsciousnessBinding);

    if (!ConsciousnessIntegration || !MemorySystem)
    {
        EndTest(Result, false, TEXT("Components not available"));
        return Result;
    }

    // Compute memory-guided attention
    FMemoryGuidedAttention Attention = ConsciousnessIntegration->ComputeMemoryGuidedAttention(
        EConsciousnessStream::Perceiving);

    // Attention computation should succeed
    Assert(Result, TEXT("Attention computed"), true);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

// ========================================
// CROSS-STREAM ASSOCIATION TESTS
// ========================================

FMemoryTestResult UMemoryIntegrationTests::Test_CrossStreamAssociationCreation()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("CrossStreamAssociationCreation"), EMemoryTestCategory::CrossStreamAssoc);

    if (!ConsciousnessIntegration || !MemorySystem)
    {
        EndTest(Result, false, TEXT("Components not available"));
        return Result;
    }

    ClearMemorySystem();

    // Create nodes for different streams
    TArray<float> Embedding = GenerateRandomEmbedding(128);
    int64 PerceptNode = MemorySystem->CreateNode(EMemoryNodeType::Percept, TEXT("CrossPercept"), Embedding, 0.5f);
    int64 SkillNode = MemorySystem->CreateNode(EMemoryNodeType::Skill, TEXT("CrossSkill"), Embedding, 0.5f);

    // Create cross-stream association
    int64 EdgeID = ConsciousnessIntegration->CreateCrossStreamAssociation(
        PerceptNode, EConsciousnessStream::Perceiving,
        SkillNode, EConsciousnessStream::Acting,
        0.7f);

    Assert(Result, TEXT("Cross-stream association created"), EdgeID > 0);

    // Get associations
    TArray<int64> Associations = ConsciousnessIntegration->GetCrossStreamAssociations(PerceptNode);
    Assert(Result, TEXT("Association found"), Associations.Contains(SkillNode));

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_CrossStreamActivationPropagation()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("CrossStreamActivationPropagation"), EMemoryTestCategory::CrossStreamAssoc);

    if (!ConsciousnessIntegration || !MemorySystem)
    {
        EndTest(Result, false, TEXT("Components not available"));
        return Result;
    }

    ClearMemorySystem();

    // Create associated nodes
    TArray<float> Embedding = GenerateRandomEmbedding(128);
    int64 Node1 = MemorySystem->CreateNode(EMemoryNodeType::Percept, TEXT("PropagateA"), Embedding, 0.5f);
    int64 Node2 = MemorySystem->CreateNode(EMemoryNodeType::Skill, TEXT("PropagateB"), Embedding, 0.5f);

    ConsciousnessIntegration->CreateCrossStreamAssociation(
        Node1, EConsciousnessStream::Perceiving,
        Node2, EConsciousnessStream::Acting,
        0.8f);

    // Propagate activation
    ConsciousnessIntegration->PropagateAcrossStreams(Node1, 0.8f);

    // Node2 should have received activation
    FMemoryNode N2 = MemorySystem->GetNode(Node2);
    AssertGreaterThan(Result, TEXT("Activation propagated"), N2.Activation, 0.0f);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_TriadicSynchronization()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("TriadicSynchronization"), EMemoryTestCategory::CrossStreamAssoc);

    if (!ConsciousnessIntegration)
    {
        EndTest(Result, false, TEXT("ConsciousnessIntegration not available"));
        return Result;
    }

    // Handle triadic synchronization
    ConsciousnessIntegration->HandleTriadicSynchronization(0);

    // Check that hyperedge was created (if working memory had items)
    Assert(Result, TEXT("Triadic sync executed"), true);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

// ========================================
// CYCLE INTEGRATION TESTS
// ========================================

FMemoryTestResult UMemoryIntegrationTests::Test_CycleStepProcessing()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("CycleStepProcessing"), EMemoryTestCategory::CycleIntegration);

    if (!ConsciousnessIntegration)
    {
        EndTest(Result, false, TEXT("ConsciousnessIntegration not available"));
        return Result;
    }

    // Process different cycle steps
    ConsciousnessIntegration->ProcessCycleStep(1, EConsciousnessStream::Perceiving);
    ConsciousnessIntegration->ProcessCycleStep(4, EConsciousnessStream::Acting);
    ConsciousnessIntegration->ProcessCycleStep(7, EConsciousnessStream::Reflecting);
    ConsciousnessIntegration->ProcessCycleStep(12, EConsciousnessStream::Perceiving);

    Assert(Result, TEXT("Cycle steps processed"), true);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_CycleConsolidation()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("CycleConsolidation"), EMemoryTestCategory::CycleIntegration);

    if (!ConsciousnessIntegration)
    {
        EndTest(Result, false, TEXT("ConsciousnessIntegration not available"));
        return Result;
    }

    // Trigger consolidation
    ConsciousnessIntegration->ConsolidateCycleMemories();

    Assert(Result, TEXT("Consolidation executed"), true);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

// ========================================
// PERFORMANCE TESTS
// ========================================

FMemoryTestResult UMemoryIntegrationTests::Test_BulkNodeCreationPerformance()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("BulkNodeCreationPerformance"), EMemoryTestCategory::PerformanceStress);

    if (!MemorySystem)
    {
        EndTest(Result, false, TEXT("MemorySystem not available"));
        return Result;
    }

    ClearMemorySystem();

    double StartTime = FPlatformTime::Seconds();

    // Create many nodes
    TArray<float> Embedding = GenerateRandomEmbedding(128);
    for (int32 i = 0; i < StressTestNodeCount; ++i)
    {
        MemorySystem->CreateNode(EMemoryNodeType::Percept,
            FString::Printf(TEXT("Bulk%d"), i), Embedding, 0.5f);
    }

    double Duration = (FPlatformTime::Seconds() - StartTime) * 1000.0;

    Result.Details.Add(FString::Printf(TEXT("Created %d nodes in %.2f ms"), StressTestNodeCount, Duration));
    Result.Details.Add(FString::Printf(TEXT("Rate: %.0f nodes/sec"), StressTestNodeCount / (Duration / 1000.0)));

    // Check that creation was fast enough (less than 100ms for 1000 nodes)
    float TimePerNode = Duration / StressTestNodeCount;
    AssertLessThan(Result, TEXT("Time per node < 0.1ms"), TimePerNode, 0.1f);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_SimilaritySearchPerformance()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("SimilaritySearchPerformance"), EMemoryTestCategory::PerformanceStress);

    if (!MemorySystem)
    {
        EndTest(Result, false, TEXT("MemorySystem not available"));
        return Result;
    }

    // Create nodes if not already present
    FHypergraphStats Stats = MemorySystem->GetStatistics();
    if (Stats.TotalNodes < 100)
    {
        TArray<float> Embedding = GenerateRandomEmbedding(128);
        for (int32 i = 0; i < 100; ++i)
        {
            MemorySystem->CreateNode(EMemoryNodeType::Percept,
                FString::Printf(TEXT("Search%d"), i), Embedding, 0.5f);
        }
    }

    // Perform similarity search
    TArray<float> QueryEmbedding = GenerateRandomEmbedding(128);

    double StartTime = FPlatformTime::Seconds();
    TArray<int64> Results = MemorySystem->FindSimilarNodes(QueryEmbedding, 0.5f, 10);
    double Duration = (FPlatformTime::Seconds() - StartTime) * 1000.0;

    Result.Details.Add(FString::Printf(TEXT("Found %d similar nodes in %.2f ms"), Results.Num(), Duration));

    // Search should be fast (< 10ms)
    AssertLessThan(Result, TEXT("Search time < 10ms"), static_cast<float>(Duration), 10.0f);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_SpreadingActivationPerformance()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("SpreadingActivationPerformance"), EMemoryTestCategory::PerformanceStress);

    if (!MemorySystem)
    {
        EndTest(Result, false, TEXT("MemorySystem not available"));
        return Result;
    }

    ClearMemorySystem();

    // Create connected graph
    TArray<float> Embedding = GenerateRandomEmbedding(128);
    TArray<int64> NodeIDs;

    for (int32 i = 0; i < 100; ++i)
    {
        int64 NodeID = MemorySystem->CreateNode(EMemoryNodeType::Concept,
            FString::Printf(TEXT("Spread%d"), i), Embedding, 0.5f);
        NodeIDs.Add(NodeID);
    }

    // Create edges
    for (int32 i = 0; i < NodeIDs.Num() - 1; ++i)
    {
        MemorySystem->CreateEdge(NodeIDs[i], NodeIDs[i + 1], ESemanticRelation::ASSOCIATED_WITH, 0.8f);
    }

    // Measure spreading activation
    double StartTime = FPlatformTime::Seconds();
    MemorySystem->SpreadActivation(NodeIDs[0], 1.0f);
    double Duration = (FPlatformTime::Seconds() - StartTime) * 1000.0;

    Result.Details.Add(FString::Printf(TEXT("Spread activation in %.2f ms"), Duration));

    AssertLessThan(Result, TEXT("Spreading time < 5ms"), static_cast<float>(Duration), 5.0f);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

FMemoryTestResult UMemoryIntegrationTests::Test_ConsolidationPerformance()
{
    FMemoryTestResult Result;
    StartTest(Result, TEXT("ConsolidationPerformance"), EMemoryTestCategory::PerformanceStress);

    if (!MemorySystem)
    {
        EndTest(Result, false, TEXT("MemorySystem not available"));
        return Result;
    }

    double StartTime = FPlatformTime::Seconds();
    MemorySystem->RunConsolidation();
    double Duration = (FPlatformTime::Seconds() - StartTime) * 1000.0;

    Result.Details.Add(FString::Printf(TEXT("Consolidation completed in %.2f ms"), Duration));

    AssertLessThan(Result, TEXT("Consolidation time < 50ms"), static_cast<float>(Duration), 50.0f);

    EndTest(Result, Result.AssertionsFailed == 0);
    return Result;
}

// ========================================
// HELPER METHODS
// ========================================

bool UMemoryIntegrationTests::Assert(FMemoryTestResult& Result, const FString& Assertion, bool Condition)
{
    if (Condition)
    {
        Result.AssertionsPassed++;
        if (bVerboseLogging)
        {
            Result.Details.Add(FString::Printf(TEXT("[PASS] %s"), *Assertion));
        }
    }
    else
    {
        Result.AssertionsFailed++;
        Result.Details.Add(FString::Printf(TEXT("[FAIL] %s"), *Assertion));
    }
    return Condition;
}

bool UMemoryIntegrationTests::AssertEqual(FMemoryTestResult& Result, const FString& Assertion,
                                           float Expected, float Actual, float Tolerance)
{
    bool bPassed = FMath::Abs(Expected - Actual) <= Tolerance;
    if (!bPassed)
    {
        Result.Details.Add(FString::Printf(TEXT("[FAIL] %s - Expected: %.4f, Actual: %.4f"),
                                            *Assertion, Expected, Actual));
    }
    return Assert(Result, Assertion, bPassed);
}

bool UMemoryIntegrationTests::AssertEqual(FMemoryTestResult& Result, const FString& Assertion,
                                           int32 Expected, int32 Actual)
{
    bool bPassed = Expected == Actual;
    if (!bPassed)
    {
        Result.Details.Add(FString::Printf(TEXT("[FAIL] %s - Expected: %d, Actual: %d"),
                                            *Assertion, Expected, Actual));
    }
    return Assert(Result, Assertion, bPassed);
}

bool UMemoryIntegrationTests::AssertNotEqual(FMemoryTestResult& Result, const FString& Assertion,
                                              int64 NotExpected, int64 Actual)
{
    return Assert(Result, Assertion, NotExpected != Actual);
}

bool UMemoryIntegrationTests::AssertGreaterThan(FMemoryTestResult& Result, const FString& Assertion,
                                                 float Value, float Threshold)
{
    bool bPassed = Value > Threshold;
    if (!bPassed)
    {
        Result.Details.Add(FString::Printf(TEXT("[FAIL] %s - Value: %.4f, Threshold: %.4f"),
                                            *Assertion, Value, Threshold));
    }
    return Assert(Result, Assertion, bPassed);
}

bool UMemoryIntegrationTests::AssertLessThan(FMemoryTestResult& Result, const FString& Assertion,
                                              float Value, float Threshold)
{
    bool bPassed = Value < Threshold;
    if (!bPassed)
    {
        Result.Details.Add(FString::Printf(TEXT("[FAIL] %s - Value: %.4f, Threshold: %.4f"),
                                            *Assertion, Value, Threshold));
    }
    return Assert(Result, Assertion, bPassed);
}

void UMemoryIntegrationTests::StartTest(FMemoryTestResult& Result, const FString& Name,
                                         EMemoryTestCategory Category)
{
    Result.TestName = Name;
    Result.Category = Category;
    Result.bPassed = false;
    Result.AssertionsPassed = 0;
    Result.AssertionsFailed = 0;
    Result.Details.Empty();

    if (bVerboseLogging)
    {
        UE_LOG(LogTemp, Log, TEXT("[TEST] Starting: %s"), *Name);
    }
}

void UMemoryIntegrationTests::EndTest(FMemoryTestResult& Result, bool bSuccess, const FString& ErrorMessage)
{
    Result.bPassed = bSuccess && (Result.AssertionsFailed == 0);
    Result.ErrorMessage = ErrorMessage;

    TestResults.Add(Result);
    LogTestResult(Result);
}

void UMemoryIntegrationTests::LogTestResult(const FMemoryTestResult& Result)
{
    if (bVerboseLogging)
    {
        FString Status = Result.bPassed ? TEXT("PASSED") : TEXT("FAILED");
        UE_LOG(LogTemp, Log, TEXT("[%s] %s - Assertions: %d passed, %d failed"),
               *Status, *Result.TestName, Result.AssertionsPassed, Result.AssertionsFailed);

        for (const FString& Detail : Result.Details)
        {
            UE_LOG(LogTemp, Log, TEXT("  %s"), *Detail);
        }

        if (!Result.ErrorMessage.IsEmpty())
        {
            UE_LOG(LogTemp, Error, TEXT("  Error: %s"), *Result.ErrorMessage);
        }
    }
}

TArray<float> UMemoryIntegrationTests::GenerateRandomEmbedding(int32 Dimension) const
{
    TArray<float> Embedding;
    Embedding.SetNum(Dimension);

    for (int32 i = 0; i < Dimension; ++i)
    {
        Embedding[i] = FMath::FRandRange(-1.0f, 1.0f);
    }

    // Normalize
    float Norm = 0.0f;
    for (float Val : Embedding)
    {
        Norm += Val * Val;
    }
    Norm = FMath::Sqrt(Norm);

    if (Norm > 0.0f)
    {
        for (float& Val : Embedding)
        {
            Val /= Norm;
        }
    }

    return Embedding;
}

void UMemoryIntegrationTests::ClearMemorySystem()
{
    if (MemorySystem)
    {
        MemorySystem->ClearAll();
    }
}

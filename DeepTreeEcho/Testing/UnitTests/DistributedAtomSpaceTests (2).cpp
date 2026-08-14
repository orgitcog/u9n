/**
 * @file DistributedAtomSpaceTests.cpp
 * @brief Unit tests for the DeepTreeEcho/Distributed module — the
 *        "inferno clusters as a distributed ATenSpace" layer:
 *        ATenSpaceLite (o9nn/ATenSpace port), InfernoNamespace
 *        (o9nn/infernos devopencog port), and ClusterAtomSpace
 *        (togai Cognitive Mesh + flarecog sharding + infernos coherence).
 *
 * Header-only module: included directly (translation-unit inclusion).
 */
#include <gtest/gtest.h>
#include <cmath>
#include <string>
#include <thread>
#include <vector>
#include "Distributed/ClusterAtomSpace.h"

using namespace dte::aspace;

// ============================================================================
// ATenSpaceLite — hypergraph container (o9nn/ATenSpace API parity)
// ============================================================================

TEST(ATenSpaceLiteTest, NodeDeduplication)
{
    AtomSpace space;
    auto cat1 = space.addNode(Atom::Type::CONCEPT_NODE, "cat");
    auto cat2 = space.addNode(Atom::Type::CONCEPT_NODE, "cat");
    EXPECT_EQ(cat1, cat2);
    EXPECT_EQ(space.size(), 1u);
    // Different type with same name is a distinct atom
    auto catPred = space.addNode(Atom::Type::PREDICATE_NODE, "cat");
    EXPECT_NE(cat1, catPred);
    EXPECT_EQ(space.size(), 2u);
}

TEST(ATenSpaceLiteTest, LinkDeduplicationAndIncomingSets)
{
    AtomSpace space;
    auto cat = space.addNode(Atom::Type::CONCEPT_NODE, "cat");
    auto mammal = space.addNode(Atom::Type::CONCEPT_NODE, "mammal");
    auto inh1 = space.addLink(Atom::Type::INHERITANCE_LINK, {cat, mammal});
    auto inh2 = space.addLink(Atom::Type::INHERITANCE_LINK, {cat, mammal});
    EXPECT_EQ(inh1, inh2);
    EXPECT_EQ(space.size(), 3u);
    // Incoming set of cat contains the inheritance link
    EXPECT_GE(cat->liveIncomingCount(), 1u);
    // Ordered outgoing set preserved
    auto link = std::dynamic_pointer_cast<Link>(inh1);
    ASSERT_NE(link, nullptr);
    EXPECT_EQ(link->getArity(), 2u);
    EXPECT_EQ(link->getOutgoingAtom(0), cat);
    EXPECT_EQ(link->getOutgoingAtom(1), mammal);
}

TEST(ATenSpaceLiteTest, RemoveAtomRespectsIncomingReferences)
{
    AtomSpace space;
    auto cat = space.addNode(Atom::Type::CONCEPT_NODE, "cat");
    auto mammal = space.addNode(Atom::Type::CONCEPT_NODE, "mammal");
    auto inh = space.addLink(Atom::Type::INHERITANCE_LINK, {cat, mammal});
    // cat is referenced by the link — removal must fail
    EXPECT_FALSE(space.removeAtom(cat));
    // the link itself has no incoming refs — removal succeeds
    EXPECT_TRUE(space.removeAtom(inh));
    EXPECT_EQ(space.size(), 2u);
}

TEST(ATenSpaceLiteTest, QuerySimilarRanksByCosineSimilarity)
{
    AtomSpace space;
    space.addNode(Atom::Type::CONCEPT_NODE, "east",  {1.0f, 0.0f, 0.0f});
    space.addNode(Atom::Type::CONCEPT_NODE, "north", {0.0f, 1.0f, 0.0f});
    space.addNode(Atom::Type::CONCEPT_NODE, "northeast", {0.7f, 0.7f, 0.0f});
    space.addNode(Atom::Type::CONCEPT_NODE, "unembedded");

    auto results = space.querySimilar({1.0f, 0.1f, 0.0f}, 10, 0.0f);
    ASSERT_GE(results.size(), 3u);
    auto top = std::dynamic_pointer_cast<Node>(results[0].first);
    ASSERT_NE(top, nullptr);
    EXPECT_EQ(top->getName(), "east");
    // strictly descending similarity
    for (size_t i = 1; i < results.size(); ++i)
        EXPECT_LE(results[i].second, results[i - 1].second);
    // top-k limit respected
    auto limited = space.querySimilar({1.0f, 0.1f, 0.0f}, 2, 0.0f);
    EXPECT_EQ(limited.size(), 2u);
}

TEST(ATenSpaceLiteTest, TruthValueCountWeightedMerge)
{
    TruthValue a{0.9f, 0.8f, 3.0f};
    TruthValue b{0.3f, 0.4f, 1.0f};
    TruthValue m = TruthValue::merge(a, b);
    EXPECT_NEAR(m.count, 4.0f, 1e-5f);
    EXPECT_NEAR(m.strength, (0.9f * 3 + 0.3f * 1) / 4.0f, 1e-5f);
    EXPECT_NEAR(m.confidence, (0.8f * 3 + 0.4f * 1) / 4.0f, 1e-5f);
    // merge is symmetric
    TruthValue m2 = TruthValue::merge(b, a);
    EXPECT_NEAR(m.strength, m2.strength, 1e-6f);
}

TEST(ATenSpaceLiteTest, ThreadSafeConcurrentInsertion)
{
    AtomSpace space;
    constexpr int kThreads = 4, kPerThread = 200;
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&space, t] {
            for (int i = 0; i < kPerThread; ++i)
                space.addNode(Atom::Type::CONCEPT_NODE,
                              "atom_" + std::to_string(t) + "_" + std::to_string(i));
        });
    }
    for (auto& th : threads) th.join();
    EXPECT_EQ(space.size(), static_cast<size_t>(kThreads * kPerThread));
}

// ============================================================================
// InfernoNamespace — /dev/opencog synthetic file tree (devopencog.c parity)
// ============================================================================

TEST(InfernoNamespaceTest, SystemGoalsInitializedFromKernel)
{
    CognitiveKernelState k;
    ASSERT_EQ(k.goals.size(), 4u);
    EXPECT_EQ(k.goals[0].description, "system_survival");
    EXPECT_EQ(k.goals[2].description, "distributed_coherence");
    EXPECT_FLOAT_EQ(k.goals[0].urgency, 1.0f);
    EXPECT_FLOAT_EQ(k.goals[2].importance, 0.8f);
}

TEST(InfernoNamespaceTest, FileListingMatchesDeviceTable)
{
    CognitiveKernelState k;
    InfernoNamespace fs(k);
    auto files = fs.list();
    ASSERT_EQ(files.size(), 8u);
    EXPECT_TRUE(fs.exists("/stats"));
    EXPECT_TRUE(fs.exists("atomspace"));
    EXPECT_TRUE(fs.exists("/distributed"));
    EXPECT_FALSE(fs.exists("/nonexistent"));
}

TEST(InfernoNamespaceTest, AtomspaceCreateAndClearCommands)
{
    CognitiveKernelState k;
    InfernoNamespace fs(k);
    EXPECT_TRUE(fs.writeFile("/atomspace", "create cat"));
    EXPECT_TRUE(fs.writeFile("/atomspace", "create dog\n"));
    EXPECT_EQ(k.space.size(), 2u);
    // TruthValue from devopencog write path: {0.8, 0.5, 1.0}
    auto cat = k.space.getNode(Atom::Type::CONCEPT_NODE, "cat");
    ASSERT_NE(cat, nullptr);
    EXPECT_FLOAT_EQ(cat->getTruthValue().strength, 0.8f);
    EXPECT_TRUE(fs.writeFile("/atomspace", "clear"));
    EXPECT_EQ(k.space.size(), 0u);
}

TEST(InfernoNamespaceTest, GoalReasonThinkAttentionVerbs)
{
    CognitiveKernelState k;
    InfernoNamespace fs(k);

    EXPECT_TRUE(fs.writeFile("/goals", "add explore_environment"));
    EXPECT_EQ(k.goals.size(), 5u);

    const auto cycles0 = k.reasoning_cycles;
    EXPECT_TRUE(fs.writeFile("/reason", "cycle"));
    EXPECT_EQ(k.reasoning_cycles, cycles0 + 1);
    EXPECT_TRUE(fs.writeFile("/reason", "threshold 0.42"));
    EXPECT_FLOAT_EQ(k.confidence_threshold, 0.42f);

    EXPECT_TRUE(fs.writeFile("/think", "focus"));
    EXPECT_FLOAT_EQ(k.attention_level, 1.0f);
    EXPECT_TRUE(fs.writeFile("/think", "relax"));
    EXPECT_FLOAT_EQ(k.attention_level, 0.5f);

    EXPECT_TRUE(fs.writeFile("/attention", "0.75"));
    EXPECT_FLOAT_EQ(k.attention_level, 0.75f);

    // Unknown command rejected
    EXPECT_FALSE(fs.writeFile("/reason", "bogus"));
}

TEST(InfernoNamespaceTest, ReadFilesReflectKernelState)
{
    CognitiveKernelState k;
    InfernoNamespace fs(k);
    fs.writeFile("/atomspace", "create cat");
    fs.writeFile("/reason", "cycle");

    auto stats = fs.readFile("/stats");
    EXPECT_NE(stats.find("total_atoms=1"), std::string::npos);
    EXPECT_NE(stats.find("reasoning_cycles=1"), std::string::npos);

    auto goals = fs.readFile("/goals");
    EXPECT_NE(goals.find("system_survival"), std::string::npos);
    EXPECT_NE(goals.find("distributed_coherence"), std::string::npos);

    auto dist = fs.readFile("/distributed");
    EXPECT_NE(dist.find("distributed_nodes=1"), std::string::npos);
    // Unknown path reads empty
    EXPECT_TRUE(fs.readFile("/bogus").empty());
}

TEST(InfernoNamespaceTest, DistributedSyncVerbInvokesHook)
{
    CognitiveKernelState k;
    InfernoNamespace fs(k);
    int sync_calls = 0;
    fs.setSyncHook([&sync_calls] { ++sync_calls; });
    EXPECT_TRUE(fs.writeFile("/distributed", "sync"));
    EXPECT_TRUE(fs.writeFile("/distributed", "sync"));
    EXPECT_EQ(sync_calls, 2);
}

TEST(InfernoNamespaceTest, ReasoningCycleRelaxesGoalSatisfaction)
{
    CognitiveKernelState k;
    const float s0 = k.goals[0].satisfaction.strength;
    k.reasoningCycle();
    EXPECT_GT(k.goals[0].satisfaction.strength, s0);
    // satisfaction saturates near 0.9
    for (int i = 0; i < 200; ++i) k.reasoningCycle();
    EXPECT_LE(k.goals[0].satisfaction.strength, 0.92f);
}

// ============================================================================
// ClusterAtomSpace — distributed mesh (togai + flarecog + infernos patterns)
// ============================================================================

TEST(ClusterAtomSpaceTest, NodeRegistrationUpdatesDistributedCount)
{
    ClusterAtomSpace cluster;
    auto a = cluster.registerNode("coghood", NodeRole::Coordinator);
    auto b = cluster.registerNode("cogcity", NodeRole::Inference);
    auto c = cluster.registerNode("mobile",  NodeRole::Sensor);
    EXPECT_EQ(cluster.nodeCount(), 3u);
    // Every node's kernel sees the cluster size (infernos distributed_nodes)
    EXPECT_EQ(a->kernel().distributed_nodes, 3);
    EXPECT_EQ(c->kernel().distributed_nodes, 3);
    EXPECT_TRUE(cluster.unregisterNode("mobile"));
    EXPECT_EQ(a->kernel().distributed_nodes, 2);
    EXPECT_EQ(cluster.node("mobile"), nullptr);
    EXPECT_EQ(cluster.node("cogcity"), b);
}

TEST(ClusterAtomSpaceTest, RendezvousShardingIsDeterministicAndStable)
{
    ClusterAtomSpace cluster;
    cluster.registerNode("n1", NodeRole::Storage);
    cluster.registerNode("n2", NodeRole::Storage);
    cluster.registerNode("n3", NodeRole::Storage);

    // Deterministic
    for (int i = 0; i < 20; ++i) {
        const std::string name = "atom_" + std::to_string(i);
        EXPECT_EQ(cluster.shardOf(name), cluster.shardOf(name));
    }
    // Reasonably spread across nodes
    int n1 = 0, n2 = 0, n3 = 0;
    for (int i = 0; i < 300; ++i) {
        const auto owner = cluster.shardOf("atom_" + std::to_string(i));
        if (owner == "n1") ++n1;
        else if (owner == "n2") ++n2;
        else ++n3;
    }
    EXPECT_GT(n1, 30); EXPECT_GT(n2, 30); EXPECT_GT(n3, 30);

    // Rendezvous stability: removing a node only remaps that node's atoms
    std::vector<std::string> before;
    for (int i = 0; i < 100; ++i)
        before.push_back(cluster.shardOf("atom_" + std::to_string(i)));
    cluster.unregisterNode("n3");
    int moved_from_survivors = 0;
    for (int i = 0; i < 100; ++i) {
        const auto after = cluster.shardOf("atom_" + std::to_string(i));
        if (before[i] != "n3" && after != before[i]) ++moved_from_survivors;
    }
    EXPECT_EQ(moved_from_survivors, 0);
}

TEST(ClusterAtomSpaceTest, LearnAndPublishReplicatesToAllNodes)
{
    ClusterAtomSpace cluster;
    auto a = cluster.registerNode("a", NodeRole::Inference);
    auto b = cluster.registerNode("b", NodeRole::Inference);
    auto c = cluster.registerNode("c", NodeRole::Inference);

    cluster.learnAndPublish("a", "reservoir_pattern", {0.9f, 0.7f, 1.0f});

    // learn() merges the observation {0.9,0.7,1} with the fresh atom's
    // default TV {1.0,1.0,1} => count-weighted average {0.95,0.85,2}.
    // The gossiped delta carries that post-merge TV to every replica.
    for (auto& n : {a, b, c}) {
        auto atom = n->kernel().space.getNode(Atom::Type::CONCEPT_NODE,
                                              "reservoir_pattern");
        ASSERT_NE(atom, nullptr) << "missing on node " << n->id();
        EXPECT_NEAR(atom->getTruthValue().strength, 0.95f, 1e-4f);
        EXPECT_GE(atom->getTruthValue().count, 2.0f - 1e-4f);
    }
}

TEST(ClusterAtomSpaceTest, DuplicateDeltasAreIdempotent)
{
    ClusterAtomSpace cluster;
    auto a = cluster.registerNode("a", NodeRole::Inference);
    auto b = cluster.registerNode("b", NodeRole::Inference);

    AtomDelta d = a->learn("cat", {0.8f, 0.6f, 1.0f});
    b->apply(d);
    const auto tv1 = b->kernel()
                         .space.getNode(Atom::Type::CONCEPT_NODE, "cat")
                         ->getTruthValue();
    // Re-applying the same sequence is a no-op (high-water mark gating)
    b->apply(d);
    const auto tv2 = b->kernel()
                         .space.getNode(Atom::Type::CONCEPT_NODE, "cat")
                         ->getTruthValue();
    EXPECT_FLOAT_EQ(tv1.count, tv2.count);
    EXPECT_FLOAT_EQ(tv1.strength, tv2.strength);
}

TEST(ClusterAtomSpaceTest, ConflictingObservationsConvergeByEvidence)
{
    ClusterAtomSpace cluster;
    auto a = cluster.registerNode("a", NodeRole::Sensor);
    auto b = cluster.registerNode("b", NodeRole::Sensor);

    // a strongly believes, b weakly disbelieves — replicas converge to the
    // count-weighted average on each receiving node.
    cluster.learnAndPublish("a", "threat_detected", {0.9f, 0.9f, 3.0f});
    cluster.learnAndPublish("b", "threat_detected", {0.1f, 0.5f, 1.0f});

    auto atomA = a->kernel().space.getNode(Atom::Type::CONCEPT_NODE,
                                           "threat_detected");
    auto atomB = b->kernel().space.getNode(Atom::Type::CONCEPT_NODE,
                                           "threat_detected");
    ASSERT_NE(atomA, nullptr);
    ASSERT_NE(atomB, nullptr);
    // Both nodes hold evidence from both observations
    EXPECT_GE(atomA->getTruthValue().count, 4.0f - 1e-4f);
    EXPECT_GE(atomB->getTruthValue().count, 4.0f - 1e-4f);
    // Strength strictly between the two observations, nearer the stronger one
    EXPECT_GT(atomA->getTruthValue().strength, 0.1f);
    EXPECT_LT(atomA->getTruthValue().strength, 0.9f);
    EXPECT_GT(atomA->getTruthValue().strength, 0.5f);
}

TEST(ClusterAtomSpaceTest, AntiEntropySyncConvergesAllReplicas)
{
    ClusterAtomSpace cluster;
    auto a = cluster.registerNode("a", NodeRole::Inference);
    auto b = cluster.registerNode("b", NodeRole::Inference);

    // Learn locally WITHOUT publishing — nodes diverge.
    a->learn("alpha", {0.8f, 0.7f, 2.0f});
    b->learn("beta",  {0.6f, 0.5f, 1.0f});
    EXPECT_EQ(a->kernel().space.getNode(Atom::Type::CONCEPT_NODE, "beta"),
              nullptr);

    // Anti-entropy pass (the `echo sync > /distributed` verb).
    cluster.sync();
    EXPECT_EQ(cluster.syncRounds(), 1u);

    EXPECT_NE(a->kernel().space.getNode(Atom::Type::CONCEPT_NODE, "beta"),
              nullptr);
    EXPECT_NE(b->kernel().space.getNode(Atom::Type::CONCEPT_NODE, "alpha"),
              nullptr);
    // Repeated sync is stable (no count inflation from anti-entropy)
    const auto count0 = b->kernel()
                            .space.getNode(Atom::Type::CONCEPT_NODE, "alpha")
                            ->getTruthValue()
                            .count;
    cluster.sync();
    const auto count1 = b->kernel()
                            .space.getNode(Atom::Type::CONCEPT_NODE, "alpha")
                            ->getTruthValue()
                            .count;
    EXPECT_FLOAT_EQ(count0, count1);
}

TEST(ClusterAtomSpaceTest, NamespaceSyncVerbTriggersClusterSync)
{
    ClusterAtomSpace cluster;
    auto a = cluster.registerNode("a", NodeRole::Coordinator);
    auto b = cluster.registerNode("b", NodeRole::Inference);
    b->learn("hidden_knowledge", {0.7f, 0.6f, 1.0f});

    // Drive the cluster through the Inferno file namespace on node a.
    EXPECT_TRUE(a->fs().writeFile("/distributed", "sync"));
    EXPECT_EQ(cluster.syncRounds(), 1u);
    EXPECT_NE(a->kernel().space.getNode(Atom::Type::CONCEPT_NODE,
                                        "hidden_knowledge"),
              nullptr);
}

TEST(ClusterAtomSpaceTest, GlobalQueryScatterGathersAcrossShards)
{
    ClusterAtomSpace cluster;
    cluster.registerNode("a", NodeRole::Inference);
    cluster.registerNode("b", NodeRole::Inference);

    // Embedded atoms living on different nodes (no sync).
    cluster.node("a")->learn("east",  {1.0f, 0.9f, 1.0f}, {1.0f, 0.0f});
    cluster.node("b")->learn("north", {1.0f, 0.9f, 1.0f}, {0.0f, 1.0f});
    cluster.node("b")->learn("northeast", {1.0f, 0.9f, 1.0f}, {0.7f, 0.7f});

    auto results = cluster.globalQuery({1.0f, 0.05f}, 10, 0.0f);
    ASSERT_GE(results.size(), 3u);
    EXPECT_EQ(results[0].first, "east");
    for (size_t i = 1; i < results.size(); ++i)
        EXPECT_LE(results[i].second, results[i - 1].second);
}

TEST(ClusterAtomSpaceTest, TransportHookReceivesPublishedDeltas)
{
    ClusterAtomSpace cluster;
    cluster.registerNode("a", NodeRole::Inference);
    std::vector<std::string> wire;
    cluster.setTransport([&wire](const AtomDelta& d) { wire.push_back(d.name); });
    cluster.learnAndPublish("a", "outbound_atom", {0.5f, 0.5f, 1.0f});
    ASSERT_EQ(wire.size(), 1u);
    EXPECT_EQ(wire[0], "outbound_atom");
}

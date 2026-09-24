#include <gtest/gtest.h>
#include "Membrane/MembraneDTEBridge.h"
#include "Relevance/RelevanceKernel.h"
#include "Distributed/ATenSpaceLite.h"
#include "Executive/Cog0TaskSubsystem.h"
#include "Avatar/EndocrineBus.h"

using namespace dte;
using namespace dte::membrane;

TEST(MembraneModuleTests, BridgeInitialization) {
    auto rr = std::make_shared<rr::RelevanceCore>();
    auto aspace = std::make_shared<aspace::AtomSpace>();
    auto exec = std::make_shared<Cog0TaskSubsystem>();
    auto endo = std::make_shared<EndocrineSystem>();

    MembraneDTEBridge bridge(rr, aspace, exec, endo);
    EXPECT_NE(bridge.getMembraneEngine(), nullptr);
    EXPECT_EQ(bridge.getCycleCount(), 0);
}

TEST(MembraneModuleTests, SynchronizeCycle) {
    auto rr = std::make_shared<rr::RelevanceCore>();
    auto aspace = std::make_shared<aspace::AtomSpace>();
    auto exec = std::make_shared<Cog0TaskSubsystem>();
    auto endo = std::make_shared<EndocrineSystem>();

    MembraneDTEBridge bridge(rr, aspace, exec, endo);
    
    std::vector<float> emb = {0.1f, 0.2f, 0.3f};
    bridge.synchronizeCycle("test_percept", emb);
    
    EXPECT_EQ(bridge.getCycleCount(), 1);
    
    // Check that the percept was added to the distributed ATenSpace
    // (since it should be in the attentional focus after perception)
    // The membrane bridge mirrors AF atoms; the P-Lingua atomspace bridge
    // suffixes names with the RR node id (e.g. "test_percept_4"), so scan
    // all concept nodes for a prefix match.
    bool found = false;
    for (const auto& h : aspace->getAtomsByType(aspace::Atom::Type::CONCEPT_NODE)) {
        auto n = std::dynamic_pointer_cast<aspace::Node>(h);
        if (n && n->getName().rfind("test_percept", 0) == 0) { found = true; break; }
    }
    EXPECT_TRUE(found);
}

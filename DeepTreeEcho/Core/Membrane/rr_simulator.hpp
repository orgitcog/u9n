// Vendored from ReZorg/RRR-P-Sys-Cog (Pure P-Lingua AGI with RR dynamics)
// Modified to strip serialization dependencies for DTE integration

#ifndef _RR_SIMULATOR_HPP_
#define _RR_SIMULATOR_HPP_

// RRSimulator is a standalone class that does not inherit from or reference
// plingua::simulator::Simulator, so <simulator/simulator.hpp> is deliberately
// not included here — that header's inline virtual methods require linking
// against the simulator object files (src/simulator/*.cpp), which header-only
// consumers of RRSimulator should not be forced to pull in. Code that needs
// the real simulator must include <simulator/simulator.hpp> directly.
#include "relevance_realization.hpp"
#include "atomspace_integration.hpp"
#include <memory>

namespace plingua { namespace rr {

// Enhanced simulator that transforms P-systems into RR hypergraphs
class RRSimulator {
public:
    RRSimulator() : time_step(0.1), rr_enabled(true), hypergraph(nullptr) {}
    virtual ~RRSimulator() { delete hypergraph; }
    
    // Initialize RR hypergraph from P-system configuration
    void initializeRRHypergraph() {
        if (!rr_enabled) return;
        
        delete hypergraph; // Clean up any existing hypergraph
        hypergraph = new RRHypergraph();
        
        // Create simple test environment for demo
        unsigned env_node = hypergraph->addMembraneNode(0, "environment", AARType::ARENA);
        membrane_to_node[0] = env_node;
        
        // Add a few test membranes
        unsigned agent_node = hypergraph->addMembraneNode(1, "agent_membrane", AARType::AGENT);
        unsigned arena_node = hypergraph->addMembraneNode(2, "arena_membrane", AARType::ARENA);
        
        membrane_to_node[1] = agent_node;
        membrane_to_node[2] = arena_node;
        
        // Create test relations
        hypergraph->addRelationEdge(agent_node, arena_node, RREdge::CO_CONSTRUCTION, 0.7);
        
        // Initialize AtomSpace integration
        initializeAtomSpaceIntegration();
    }
    
    // Run RR dynamics step
    void stepRRDynamics() {
        if (hypergraph && rr_enabled) {
            hypergraph->updateRelevanceRealization(time_step);
            
            // Update AtomSpace representation periodically
            static int step_count = 0;
            step_count++;
            if (step_count % 10 == 0) { // Update every 10 steps
                performAtomSpaceIntegration();
            }
        }
    }
    
    // Enable/disable RR processing
    void setRREnabled(bool enabled) { rr_enabled = enabled; }
    bool isRREnabled() const { return rr_enabled; }
    
    // Get the RR hypergraph for analysis
    const RRHypergraph* getRRHypergraph() const { return hypergraph; }
    
    // Compute overall system relevance
    double computeSystemRelevance() const {
        if (!hypergraph) return 0.0;
        
        double total_relevance = 0.0;
        size_t node_count = 0;
        
        for (auto it = hypergraph->nodes.begin(); it != hypergraph->nodes.end(); ++it) {
            total_relevance += it->second->computeRelevanceGradient();
            ++node_count;
        }
        
        return node_count > 0 ? total_relevance / node_count : 0.0;
    }
    
    // Get emergent patterns detected
    std::vector<std::string> getEmergentPatterns() const {
        std::vector<std::string> patterns;
        
        if (!hypergraph) return patterns;
        
        // Look for high-relevance clusters in RR hypergraph
        for (auto it = hypergraph->nodes.begin(); it != hypergraph->nodes.end(); ++it) {
            auto node = it->second;
            if (node->salience > 0.8 && node->affordance_realization > 0.7) {
                std::string type_str = (node->aarType == AARType::AGENT ? "agent" : 
                                      node->aarType == AARType::ARENA ? "arena" : "relation");
                patterns.push_back(std::string("High-relevance ") + type_str + " node: " + node->label);
            }
        }
        
        // Add AtomSpace patterns if integration is available
        if (atomspace_integrator) {
            auto as_patterns = atomspace_integrator->findEmergentPatterns();
            patterns.insert(patterns.end(), as_patterns.begin(), as_patterns.end());
        }
        
        return patterns;
    }

    // AtomSpace integration methods
    void initializeAtomSpaceIntegration() {
        if (!hypergraph) return;
        
        atomspace.reset(new plingua::atomspace::AtomSpace());
        atomspace_integrator.reset(new plingua::atomspace::RRAtomSpaceIntegrator(hypergraph, atomspace.get()));
    }
    
    void performAtomSpaceIntegration() {
        if (atomspace_integrator) {
            atomspace_integrator->performIntegration();
        }
    }
    
    const plingua::atomspace::AtomSpace* getAtomSpace() const {
        return atomspace.get();
    }

private:
    RRHypergraph* hypergraph;
    double time_step;
    bool rr_enabled;
    
    // AtomSpace integration
    std::shared_ptr<plingua::atomspace::AtomSpace> atomspace;
    std::shared_ptr<plingua::atomspace::RRAtomSpaceIntegrator> atomspace_integrator;
    
    // Mappings between P-system and RR components
    std::map<unsigned, unsigned> membrane_to_node;  // membrane_id -> node_id
    std::map<unsigned, unsigned> rule_to_node;      // rule_id -> node_id
    std::map<std::string, unsigned> object_to_node; // object_name -> node_id
};

}} // namespace plingua::rr

#endif // _RR_SIMULATOR_HPP_
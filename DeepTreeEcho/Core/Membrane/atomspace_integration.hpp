// Vendored from ReZorg/RRR-P-Sys-Cog (Pure P-Lingua AGI with RR dynamics)
// Modified to strip serialization dependencies for DTE integration

#ifndef _ATOMSPACE_INTEGRATION_HPP_
#define _ATOMSPACE_INTEGRATION_HPP_

#include <vector>
#include <map>
#include <set>
#include <string>
#include <memory>
#include <functional>
#include <cmath>
#include "relevance_realization.hpp"

namespace plingua { namespace atomspace {

// Simple atom representation for integration with OpenCog
struct Atom {
    enum Type { 
        CONCEPT_NODE, 
        PREDICATE_NODE, 
        EVALUATION_LINK, 
        IMPLICATION_LINK,
        INHERITANCE_LINK,
        SIMILARITY_LINK
    };
    
    unsigned id;
    Type type;
    std::string name;
    std::vector<unsigned> outgoing; // Links to other atoms
    
    // Truth value representation
    double strength;   // Probability/confidence
    double confidence; // Count-based confidence
    
    Atom(unsigned atom_id, Type atom_type, const std::string& atom_name)
        : id(atom_id), type(atom_type), name(atom_name), strength(0.5), confidence(0.5) {}
};

// Lightweight AtomSpace for membrane-RR integration
class AtomSpace {
public:
    std::map<unsigned, std::shared_ptr<Atom>> atoms;
    unsigned next_atom_id;
    
    AtomSpace() : next_atom_id(1) {}
    
    // Create atoms from RR components
    unsigned addConceptNode(const std::string& name, double strength = 0.5, double confidence = 0.5) {
        auto atom = std::make_shared<Atom>(next_atom_id++, Atom::CONCEPT_NODE, name);
        atom->strength = strength;
        atom->confidence = confidence;
        atoms[atom->id] = atom;
        return atom->id;
    }
    
    unsigned addPredicateNode(const std::string& name) {
        auto atom = std::make_shared<Atom>(next_atom_id++, Atom::PREDICATE_NODE, name);
        atoms[atom->id] = atom;
        return atom->id;
    }
    
    unsigned addEvaluationLink(unsigned predicate_id, const std::vector<unsigned>& args, 
                              double strength = 0.5, double confidence = 0.5) {
        auto atom = std::make_shared<Atom>(next_atom_id++, Atom::EVALUATION_LINK, "");
        atom->outgoing.push_back(predicate_id);
        atom->outgoing.insert(atom->outgoing.end(), args.begin(), args.end());
        atom->strength = strength;
        atom->confidence = confidence;
        atoms[atom->id] = atom;
        return atom->id;
    }
    
    unsigned addInheritanceLink(unsigned child_id, unsigned parent_id, 
                               double strength = 0.5, double confidence = 0.5) {
        auto atom = std::make_shared<Atom>(next_atom_id++, Atom::INHERITANCE_LINK, "");
        atom->outgoing.push_back(child_id);
        atom->outgoing.push_back(parent_id);
        atom->strength = strength;
        atom->confidence = confidence;
        atoms[atom->id] = atom;
        return atom->id;
    }
    
    unsigned addImplicationLink(unsigned antecedent_id, unsigned consequent_id, 
                               double strength = 0.5, double confidence = 0.5) {
        auto atom = std::make_shared<Atom>(next_atom_id++, Atom::IMPLICATION_LINK, "");
        atom->outgoing.push_back(antecedent_id);
        atom->outgoing.push_back(consequent_id);
        atom->strength = strength;
        atom->confidence = confidence;
        atoms[atom->id] = atom;
        return atom->id;
    }
    
    // Pattern matching utilities
    std::vector<unsigned> findAtomsOfType(Atom::Type type) const {
        std::vector<unsigned> result;
        for (auto it = atoms.begin(); it != atoms.end(); ++it) {
            if (it->second->type == type) {
                result.push_back(it->first);
            }
        }
        return result;
    }
    
    std::vector<unsigned> findAtomsByName(const std::string& name) const {
        std::vector<unsigned> result;
        for (auto it = atoms.begin(); it != atoms.end(); ++it) {
            if (it->second->name == name) {
                result.push_back(it->first);
            }
        }
        return result;
    }
    
    // Get atom by ID
    std::shared_ptr<Atom> getAtom(unsigned id) const {
        auto it = atoms.find(id);
        return it != atoms.end() ? it->second : nullptr;
    }
};

// Integration bridge between RR hypergraphs and AtomSpace
class RRAtomSpaceIntegrator {
public:
    RRAtomSpaceIntegrator(const plingua::rr::RRHypergraph* rr_graph, AtomSpace* atomspace)
        : rr_hypergraph(rr_graph), atom_space(atomspace),
          relates_predicate_id(0) {}
    
    // Convert RR nodes to AtomSpace concepts
    void convertRRNodesToAtoms() {
        if (!rr_hypergraph || !atom_space) return;
        
        for (auto it = rr_hypergraph->nodes.begin(); it != rr_hypergraph->nodes.end(); ++it) {
            auto rr_node = it->second;
            
            // Check if atom already exists for this RR node
            unsigned atom_id;
            auto existing_mapping = rr_node_to_atom.find(rr_node->id);
            if (existing_mapping != rr_node_to_atom.end()) {
                // Update existing atom
                atom_id = existing_mapping->second;
                auto atom = atom_space->getAtom(atom_id);
                if (atom) {
                    atom->strength = rr_node->salience;
                    atom->confidence = rr_node->affordance_realization;
                }
            } else {
                // Create new concept node for RR node
                std::string atom_name = rr_node->label + "_" + std::to_string(rr_node->id);
                atom_id = atom_space->addConceptNode(atom_name, rr_node->salience, rr_node->affordance_realization);
                
                // Store mapping
                rr_node_to_atom[rr_node->id] = atom_id;
                atom_to_rr_node[atom_id] = rr_node->id;
                
                // Add type information
                std::string type_name;
                switch (rr_node->nodeType) {
                    case plingua::rr::RRNode::MEMBRANE: type_name = "membrane"; break;
                    case plingua::rr::RRNode::RULE: type_name = "rule"; break;
                    case plingua::rr::RRNode::OBJECT: type_name = "object"; break;
                    case plingua::rr::RRNode::ENVIRONMENT: type_name = "environment"; break;
                }
                
                // Check if type atom already exists
                auto existing_type_atoms = atom_space->findAtomsByName(type_name);
                unsigned type_atom_id;
                if (!existing_type_atoms.empty()) {
                    type_atom_id = existing_type_atoms[0];
                } else {
                    type_atom_id = atom_space->addConceptNode(type_name);
                }
                atom_space->addInheritanceLink(atom_id, type_atom_id, 0.9, 0.9);
                
                // Add AAR type information
                std::string aar_name;
                switch (rr_node->aarType) {
                    case plingua::rr::AARType::AGENT: aar_name = "agent"; break;
                    case plingua::rr::AARType::ARENA: aar_name = "arena"; break;
                    case plingua::rr::AARType::RELATION: aar_name = "relation"; break;
                }
                
                // Check if AAR type atom already exists
                auto existing_aar_atoms = atom_space->findAtomsByName(aar_name);
                unsigned aar_atom_id;
                if (!existing_aar_atoms.empty()) {
                    aar_atom_id = existing_aar_atoms[0];
                } else {
                    aar_atom_id = atom_space->addConceptNode(aar_name);
                }
                atom_space->addInheritanceLink(atom_id, aar_atom_id, 0.9, 0.9);
            }
        }
    }
    
    // Convert RR edges to AtomSpace relations. Idempotent: the "relates"
    // PredicateNode is created once and cached, and each RR edge maps to a
    // single EvaluationLink that is updated on subsequent calls rather than
    // re-added. Without this guard, every invocation of performIntegration()
    // leaks a fresh PredicateNode and a fresh EvaluationLink per RR edge,
    // corrupting downstream PLN inference and growing the AtomSpace
    // unboundedly across cognitive cycles.
    void convertRREdgesToAtoms() {
        if (!rr_hypergraph || !atom_space) return;

        if (relates_predicate_id == 0 ||
            !atom_space->getAtom(relates_predicate_id)) {
            auto existing = atom_space->findAtomsByName("relates");
            relates_predicate_id = existing.empty()
                ? atom_space->addPredicateNode("relates")
                : existing.front();
        }

        for (auto it = rr_hypergraph->edges.begin(); it != rr_hypergraph->edges.end(); ++it) {
            auto rr_edge = it->second;

            // Find corresponding atoms for source and target nodes
            auto from_it = rr_node_to_atom.find(rr_edge->from_node);
            auto to_it = rr_node_to_atom.find(rr_edge->to_node);
            if (from_it == rr_node_to_atom.end() ||
                to_it   == rr_node_to_atom.end()) {
                continue;
            }

            auto edge_it = rr_edge_to_atom.find(rr_edge->id);
            if (edge_it != rr_edge_to_atom.end()) {
                // Edge already projected; refresh its truth values.
                auto atom = atom_space->getAtom(edge_it->second);
                if (atom) {
                    atom->strength   = rr_edge->strength;
                    atom->confidence = rr_edge->relevance_weight;
                    continue;
                }
                // Underlying atom was pruned; fall through to re-create.
                rr_edge_to_atom.erase(edge_it);
            }

            std::vector<unsigned> args = {from_it->second, to_it->second};
            unsigned eval_id = atom_space->addEvaluationLink(
                relates_predicate_id, args,
                rr_edge->strength, rr_edge->relevance_weight);
            rr_edge_to_atom[rr_edge->id] = eval_id;
        }
    }
    
    // Execute full conversion
    void performIntegration() {
        convertRRNodesToAtoms();
        convertRREdgesToAtoms();
    }
    
    // Query AtomSpace for RR-relevant patterns
    std::vector<std::string> findEmergentPatterns() const {
        std::vector<std::string> patterns;
        
        if (!atom_space) return patterns;
        
        // Find high-strength agent-arena relationships
        auto evaluation_atoms = atom_space->findAtomsOfType(Atom::EVALUATION_LINK);
        
        for (unsigned eval_id : evaluation_atoms) {
            auto eval_atom = atom_space->getAtom(eval_id);
            if (eval_atom && eval_atom->strength > 0.8 && eval_atom->outgoing.size() >= 3) {
                // Check if this links an agent and arena
                auto arg1_atom = atom_space->getAtom(eval_atom->outgoing[1]);
                auto arg2_atom = atom_space->getAtom(eval_atom->outgoing[2]);
                
                if (arg1_atom && arg2_atom) {
                    patterns.push_back("Strong relationship between " + 
                                     arg1_atom->name + " and " + arg2_atom->name + 
                                     " (strength: " + std::to_string(eval_atom->strength) + ")");
                }
            }
        }
        
        return patterns;
    }

    // Public so that higher-level integrators (e.g. OpenCogAGI) can translate
    // between AtomSpace atom IDs and RR node IDs. The AtomSpace, ECAN, and RR
    // hypergraph each maintain independent ID spaces, so callers crossing
    // subsystem boundaries MUST translate through these maps.
    std::map<unsigned, unsigned> rr_node_to_atom;  // RR node ID -> Atom ID
    std::map<unsigned, unsigned> atom_to_rr_node;  // Atom ID -> RR node ID
    std::map<unsigned, unsigned> rr_edge_to_atom;  // RR edge ID -> EvaluationLink Atom ID

private:
    const plingua::rr::RRHypergraph* rr_hypergraph;
    AtomSpace* atom_space;
    unsigned relates_predicate_id;                 // cached PredicateNode("relates")
};

}} // namespace plingua::atomspace

#endif // _ATOMSPACE_INTEGRATION_HPP_
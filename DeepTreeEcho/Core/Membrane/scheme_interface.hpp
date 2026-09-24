// Vendored from ReZorg/RRR-P-Sys-Cog (Pure P-Lingua AGI with RR dynamics)
// Modified to strip serialization dependencies for DTE integration

#ifndef _SCHEME_INTERFACE_HPP_
#define _SCHEME_INTERFACE_HPP_

#include <vector>
#include <map>
#include <set>
#include <string>
#include <memory>
#include <functional>
#include <iostream>
#include <sstream>
#include "relevance_realization.hpp"
#include "atomspace_integration.hpp"
#include "pln_integration.hpp"

namespace plingua { namespace scheme {

// Scheme-style expression evaluator for RR/AtomSpace interaction
class SchemeEvaluator {
public:
    SchemeEvaluator(plingua::rr::RRHypergraph* rr_graph, 
                    plingua::atomspace::AtomSpace* atomspace) 
        : rr_hypergraph(rr_graph), atom_space(atomspace), pln_engine(atomspace) {}
    
    // Evaluate Scheme-style expressions
    std::string evaluate(const std::string& expression) {
        std::istringstream iss(expression);
        return evaluateExpression(iss);
    }
    
    // Interactive REPL for RR/AtomSpace exploration
    void startREPL() {
        std::cout << "=== RR/AtomSpace Scheme Interface REPL ===" << std::endl;
        std::cout << "Type 'help' for commands, 'quit' to exit" << std::endl;
        
        std::string input;
        while (true) {
            std::cout << "scheme> ";
            std::getline(std::cin, input);
            
            if (input == "quit" || input == "exit") {
                break;
            } else if (input == "help") {
                printHelp();
            } else if (input.empty()) {
                continue;
            } else {
                try {
                    std::string result = evaluate(input);
                    std::cout << result << std::endl;
                } catch (const std::exception& e) {
                    std::cout << "Error: " << e.what() << std::endl;
                }
            }
        }
        
        std::cout << "Goodbye!" << std::endl;
    }

private:
    plingua::rr::RRHypergraph* rr_hypergraph;
    plingua::atomspace::AtomSpace* atom_space;
    plingua::pln::PLNInferenceEngine pln_engine;
    
    std::string evaluateExpression(std::istringstream& iss) {
        std::string line;
        std::getline(iss, line);
        
        if (line.find("(list-rr-nodes)") == 0) {
            return listRRNodes();
        } else if (line.find("(list-atoms)") == 0) {
            return listAtoms();
        } else if (line.find("(get-system-relevance)") == 0) {
            return getSystemRelevance();
        } else if (line.find("(run-pln-inference)") == 0) {
            return runPLNInference();
        } else if (line.find("(find-patterns)") == 0) {
            return findPatterns();
        } else if (line.find("(get-salience") == 0) {
            return getSalience(line);
        } else if (line.find("(update-salience") == 0) {
            return updateSalience(line);
        } else if (line.find("(find-atom") == 0) {
            return findAtom(line);
        } else {
            return "Unknown command: " + line;
        }
    }
    
    std::string listRRNodes() {
        if (!rr_hypergraph) return "No RR hypergraph available";
        
        std::ostringstream oss;
        oss << "(";
        bool first = true;
        for (auto it = rr_hypergraph->nodes.begin(); it != rr_hypergraph->nodes.end(); ++it) {
            if (!first) oss << " ";
            oss << "(" << it->second->label << " " << it->second->id 
                << " :salience " << it->second->salience << ")";
            first = false;
        }
        oss << ")";
        return oss.str();
    }
    
    std::string listAtoms() {
        if (!atom_space) return "No AtomSpace available";
        
        std::ostringstream oss;
        oss << "(";
        bool first = true;
        for (auto it = atom_space->atoms.begin(); it != atom_space->atoms.end(); ++it) {
            if (!first) oss << " ";
            oss << "(" << it->second->name << " " << it->second->id
                << " :strength " << it->second->strength << ")";
            first = false;
        }
        oss << ")";
        return oss.str();
    }
    
    std::string getSystemRelevance() {
        if (!rr_hypergraph) return "No RR hypergraph available";
        
        double total_relevance = 0.0;
        size_t node_count = 0;
        
        for (auto it = rr_hypergraph->nodes.begin(); it != rr_hypergraph->nodes.end(); ++it) {
            total_relevance += it->second->computeRelevanceGradient();
            ++node_count;
        }
        
        double system_relevance = node_count > 0 ? total_relevance / node_count : 0.0;
        return std::to_string(system_relevance);
    }
    
    std::string runPLNInference() {
        pln_engine.performInferenceCycle(rr_hypergraph);
        auto results = pln_engine.getInferenceResults();
        
        if (results.empty()) {
            return "No new inferences";
        } else {
            std::ostringstream oss;
            oss << "(";
            for (const auto& result : results) {
                oss << "\"" << result << "\" ";
            }
            oss << ")";
            return oss.str();
        }
    }
    
    std::string findPatterns() {
        std::vector<std::string> patterns;
        
        // Find RR patterns
        if (rr_hypergraph) {
            for (auto it = rr_hypergraph->nodes.begin(); it != rr_hypergraph->nodes.end(); ++it) {
                auto node = it->second;
                if (node->salience > 0.7 && node->affordance_realization > 0.6) {
                    patterns.push_back("High-relevance node: " + node->label);
                }
            }
        }
        
        // Find AtomSpace patterns
        if (atom_space) {
            auto evaluations = atom_space->findAtomsOfType(plingua::atomspace::Atom::EVALUATION_LINK);
            for (unsigned eval_id : evaluations) {
                auto eval_atom = atom_space->getAtom(eval_id);
                if (eval_atom && eval_atom->strength > 0.8) {
                    patterns.push_back("Strong evaluation link with strength " + std::to_string(eval_atom->strength));
                }
            }
        }
        
        std::ostringstream oss;
        oss << "(";
        for (const auto& pattern : patterns) {
            oss << "\"" << pattern << "\" ";
        }
        oss << ")";
        return oss.str();
    }
    
    std::string getSalience(const std::string& command) {
        // Extract node ID from command like "(get-salience node-1)"
        size_t start = command.find("node-");
        if (start == std::string::npos) return "Invalid node reference";
        
        start += 5; // Skip "node-"
        size_t end = command.find(")", start);
        if (end == std::string::npos) return "Invalid command format";
        
        std::string id_str = command.substr(start, end - start);
        unsigned node_id = std::stoul(id_str);
        
        if (rr_hypergraph && rr_hypergraph->nodes.count(node_id)) {
            return std::to_string(rr_hypergraph->nodes[node_id]->salience);
        } else {
            return "Node not found";
        }
    }
    
    std::string updateSalience(const std::string& command) {
        // Extract node ID and value from command like "(update-salience node-1 0.8)"
        size_t start = command.find("node-");
        if (start == std::string::npos) return "Invalid node reference";
        
        start += 5; // Skip "node-"
        size_t space = command.find(" ", start);
        if (space == std::string::npos) return "Invalid command format";
        
        std::string id_str = command.substr(start, space - start);
        unsigned node_id = std::stoul(id_str);
        
        size_t value_start = space + 1;
        size_t end = command.find(")", value_start);
        if (end == std::string::npos) return "Invalid command format";
        
        std::string value_str = command.substr(value_start, end - value_start);
        double new_salience = std::stod(value_str);
        
        if (rr_hypergraph && rr_hypergraph->nodes.count(node_id)) {
            rr_hypergraph->nodes[node_id]->salience = new_salience;
            return "Updated";
        } else {
            return "Node not found";
        }
    }
    
    std::string findAtom(const std::string& command) {
        // Extract atom name from command like "(find-atom \"agent\")"
        size_t start = command.find("\"");
        if (start == std::string::npos) return "Invalid atom name";
        
        start += 1; // Skip opening quote
        size_t end = command.find("\"", start);
        if (end == std::string::npos) return "Invalid command format";
        
        std::string atom_name = command.substr(start, end - start);
        
        if (atom_space) {
            auto atoms = atom_space->findAtomsByName(atom_name);
            if (!atoms.empty()) {
                std::ostringstream oss;
                oss << "(";
                for (unsigned id : atoms) {
                    auto atom = atom_space->getAtom(id);
                    if (atom) {
                        oss << "(" << atom->name << " " << atom->id 
                            << " :strength " << atom->strength << ") ";
                    }
                }
                oss << ")";
                return oss.str();
            } else {
                return "Atom not found";
            }
        } else {
            return "No AtomSpace available";
        }
    }
    
    void printHelp() {
        std::cout << "Available commands:" << std::endl;
        std::cout << "  (list-rr-nodes)           - List all RR nodes" << std::endl;
        std::cout << "  (list-atoms)              - List all atoms" << std::endl;
        std::cout << "  (get-system-relevance)    - Get overall system relevance" << std::endl;
        std::cout << "  (run-pln-inference)       - Run PLN inference cycle" << std::endl;
        std::cout << "  (find-patterns)           - Find emergent patterns" << std::endl;
        std::cout << "  (get-salience node-ID)    - Get salience of node" << std::endl;
        std::cout << "  (update-salience node-ID VALUE) - Update node salience" << std::endl;
        std::cout << "  (find-atom \"NAME\")         - Find atom by name" << std::endl;
        std::cout << "  help                      - Show this help" << std::endl;
        std::cout << "  quit/exit                 - Exit REPL" << std::endl;
    }
};

}} // namespace plingua::scheme

#endif // _SCHEME_INTERFACE_HPP_
// Vendored from ReZorg/RRR-P-Sys-Cog (Pure P-Lingua AGI with RR dynamics)
// Modified to strip serialization dependencies for DTE integration

#ifndef _OPENCOG_AGI_HPP_
#define _OPENCOG_AGI_HPP_

/*
 * opencog_agi.hpp
 *
 * Unified C++ interface for the complete OpenCog AGI system modelled as
 * a pure P-Lingua membrane computing architecture with relevance-realization
 * dynamics.
 *
 * This header integrates all subsystem bridges:
 *   - AtomSpace         (atomspace_integration.hpp)
 *   - PLN               (pln_integration.hpp)
 *   - ECAN              (ecan_integration.hpp)
 *   - MOSES             (moses_integration.hpp)
 *   - RR Hypergraph     (relevance_realization.hpp)
 *   - Scheme interface  (scheme_interface.hpp)
 *   - Persistence       (persistent_atomspace.hpp)
 *
 * The OpenCogAGI class provides a single entry point for:
 *   1. Perception: ingesting environment inputs
 *   2. Cognition:  one full cognitive cycle
 *   3. Action:     selecting and reporting the output action
 *   4. Learning:   incremental knowledge accumulation
 *   5. Introspection: querying internal cognitive state
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <chrono>

#include "relevance_realization.hpp"
#include "atomspace_integration.hpp"
#include "pln_integration.hpp"
#include "scheme_interface.hpp"
#include "persistent_atomspace.hpp"
#include "ecan_integration.hpp"
#include "moses_integration.hpp"

namespace plingua { namespace opencog {

// ─────────────────────────────────────────────────────────────────────────────
// OpenPsiDrive – motivational drive state
// ─────────────────────────────────────────────────────────────────────────────

struct OpenPsiDrive {
    std::string name;
    double      level;          // Current demand level [0, 1]
    double      satisfaction;   // Current satisfaction [0, 1]
    double      urgency;        // Derived: level - satisfaction

    OpenPsiDrive(const std::string& n, double lvl = 0.5, double sat = 0.5)
        : name(n), level(lvl), satisfaction(sat), urgency(lvl - sat) {}

    void update(double delta_level, double delta_sat) {
        level        = std::max(0.0, std::min(1.0, level + delta_level));
        satisfaction = std::max(0.0, std::min(1.0, satisfaction + delta_sat));
        urgency      = level - satisfaction;
    }

    bool isUrgent(double threshold = 0.3) const {
        return urgency > threshold;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// CognitiveAction – the action selected in a given cycle
// ─────────────────────────────────────────────────────────────────────────────

struct CognitiveAction {
    std::string name;
    double      confidence;
    std::string rationale;   // Which system produced this action

    CognitiveAction() : confidence(0.0) {}
    CognitiveAction(const std::string& n, double c, const std::string& r = "")
        : name(n), confidence(c), rationale(r) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// CognitiveStep – snapshot of one complete AGI cognitive cycle
// ─────────────────────────────────────────────────────────────────────────────

struct CognitiveStep {
    unsigned    step_number;
    std::string percept;
    CognitiveAction selected_action;
    double      rr_salience;
    double      rr_affordance;
    double      rr_coherence;
    double      pln_conclusions;
    std::size_t af_size;
    double      moses_best_score;
    double      primary_drive_urgency;

    CognitiveStep()
        : step_number(0), rr_salience(0), rr_affordance(0), rr_coherence(0),
          pln_conclusions(0), af_size(0), moses_best_score(-1.0),
          primary_drive_urgency(0) {}
};

// ─────────────────────────────────────────────────────────────────────────────
// OpenCogAGI – unified AGI cognitive engine
// ─────────────────────────────────────────────────────────────────────────────

class OpenCogAGI {
public:
    // ── Subsystem components ────────────────────────────────────────────
    rr::RRHypergraph                            rr_graph;
    atomspace::AtomSpace                        atomspace;
    pln::PLNInferenceEngine                     pln_engine;
    ecan::ECANEngine                            ecan_engine;
    moses::MOSESEngine                          moses_engine;
    scheme::SchemeEvaluator                     scheme_eval;
    persistent::PersistentAtomSpace             persistence;

    // ── OpenPsi drives ───────────────────────────────────────────────────
    std::vector<OpenPsiDrive>                   drives;

    // ── Integrator ───────────────────────────────────────────────────────
    atomspace::RRAtomSpaceIntegrator            rr_atomspace_bridge;

    // ── State ─────────────────────────────────────────────────────────────
    unsigned                                    cycle_count;
    std::vector<CognitiveStep>                  history;
    std::vector<std::string>                    pending_actions;

    // ── Configuration ────────────────────────────────────────────────────
    unsigned                                    ecan_steps_per_cycle;
    unsigned                                    pln_cycles_per_step;
    unsigned                                    moses_gens_per_cycle;
    double                                      rr_delta_time;

    explicit OpenCogAGI()
        : pln_engine(&atomspace),
          scheme_eval(&rr_graph, &atomspace),
          rr_atomspace_bridge(&rr_graph, &atomspace),
          cycle_count(0),
          ecan_steps_per_cycle(3),
          pln_cycles_per_step(2),
          moses_gens_per_cycle(1),
          rr_delta_time(0.1)
    {
        initialiseSubsystems();
    }

    // ── Public API ────────────────────────────────────────────────────────

    /**
     * perceive – ingest a raw percept string from the environment.
     * Creates/updates an AtomSpace concept and stimulates ECAN attention.
     * Repeated perception of the same concept reuses the existing AtomSpace
     * atom and RR node rather than creating duplicates.
     */
    unsigned perceive(const std::string& percept_name, double initial_salience = 0.5) {
        // Add to AtomSpace (idempotent)
        unsigned atom_id = findOrCreateConcept(percept_name, initial_salience, 0.6);

        // Find-or-create RR node so repeated perception does not fragment
        // attention across a growing set of duplicate nodes.
        unsigned rr_id = findOrCreateRRNode(percept_name, rr::AARType::ARENA);
        if (rr_graph.nodes.count(rr_id)) {
            rr_graph.nodes[rr_id]->salience = initial_salience;
        }

        // Stimulate ECAN attention using the RR node ID — ECAN's
        // attention_values map is keyed by RR IDs, not AtomSpace IDs.
        // Only register the atom on first sight; otherwise we would
        // overwrite the existing AttentionValue and wipe any LTI
        // accumulated by consolidateLTI() and the VLTI permanence flag.
        short sti_boost = static_cast<short>(initial_salience * 200);
        if (!ecan_engine.attention_values.count(rr_id)) {
            ecan_engine.registerAtom(rr_id, sti_boost);
        }
        ecan_engine.stimulate(rr_id, sti_boost);

        // Sync RR → AtomSpace (populates atom_to_rr_node / rr_node_to_atom).
        rr_atomspace_bridge.performIntegration();

        return atom_id;
    }

    /**
     * step – execute one complete AGI cognitive cycle.
     * Order: ECAN → PLN → MOSES → OpenPsi → RR
     */
    CognitiveStep step(const std::string& percept = "") {
        ++cycle_count;
        CognitiveStep cs;
        cs.step_number = cycle_count;
        cs.percept     = percept;

        if (!percept.empty()) {
            perceive(percept);
        }

        // 1. ECAN attention allocation
        auto wage_earners = collectWageEarners();
        for (unsigned i = 0; i < ecan_steps_per_cycle; ++i) {
            auto forgotten = ecan_engine.step(wage_earners);
            pruneAtoms(forgotten);
        }
        cs.af_size = ecan_engine.getAttentionalFocus().size();

        // 2. PLN inference over atoms in attentional focus
        for (unsigned i = 0; i < pln_cycles_per_step; ++i) {
            pln_engine.performInferenceCycle(&rr_graph);
        }
        cs.pln_conclusions = static_cast<double>(
            pln_engine.getInferenceResults().size());

        // 3. MOSES program learning (guided by RR salience)
        moses_engine.updateFeaturesFromRR(&rr_graph, 0.45);
        for (unsigned i = 0; i < moses_gens_per_cycle; ++i) {
            moses_engine.evolve();
        }
        cs.moses_best_score = moses_engine.best_overall.score;

        // 4. OpenPsi: update drives and select action
        updateDrives();
        auto action = selectAction();
        cs.selected_action = action;
        if (!action.name.empty()) {
            pending_actions.push_back(action.name);
        }

        // 5. RR dynamics update
        rr_graph.updateRelevanceRealization(rr_delta_time);
        cs.rr_salience    = averageRRSalience();
        cs.rr_affordance  = averageRRAffordance();
        cs.rr_coherence   = averageRRCoherence();

        // 6. Feedback: RR salience → ECAN STI
        for (const auto& kv : rr_graph.nodes) {
            ecan_engine.updateFromRRSalience(kv.first, kv.second->salience);
        }

        // Track most urgent drive
        if (!drives.empty()) {
            cs.primary_drive_urgency = drives[0].urgency;
        }

        history.push_back(cs);
        return cs;
    }

    /**
     * getAction – return and clear the oldest pending action.
     */
    std::string getAction() {
        if (pending_actions.empty()) return "";
        std::string action = pending_actions.front();
        pending_actions.erase(pending_actions.begin());
        return action;
    }

    /**
     * learnFromOutcome – update drives/AtomSpace after action result.
     * @param action_name   The action that was executed.
     * @param success       Whether the action achieved its goal.
     */
    void learnFromOutcome(const std::string& action_name, bool success) {
        // Update primary drive satisfaction
        for (auto& drive : drives) {
            if (drive.isUrgent()) {
                drive.update(0.0, success ? 0.3 : -0.1);
            }
        }

        // Reinforce or weaken AtomSpace concept
        auto atoms = atomspace.findAtomsByName(action_name);
        for (unsigned id : atoms) {
            auto atom = atomspace.getAtom(id);
            if (atom) {
                atom->strength   += success ? 0.1 : -0.05;
                atom->confidence = std::min(1.0, atom->confidence + 0.05);
                atom->strength   = std::max(0.0, std::min(1.0, atom->strength));
                // Reward in ECAN — translate AtomSpace id to the corresponding
                // RR node id, since ECAN is keyed by RR IDs.
                unsigned rr_id = atomToRRNode(id);
                if (rr_id != 0) {
                    ecan_engine.stimulate(rr_id, success ? 50 : -20);
                }
            }
        }
    }

    /**
     * query – Scheme-style query against the internal knowledge state.
     */
    std::string query(const std::string& scheme_expr) {
        return scheme_eval.evaluate(scheme_expr);
    }

    /**
     * saveState – persist AtomSpace and RR hypergraph to disk.
     */
    bool saveState(const std::string& as_file, const std::string& rr_file) {
        bool ok = persistence.saveToFile(&atomspace, as_file);
        ok = ok && persistence.saveRRHypergraph(&rr_graph, rr_file);
        return ok;
    }

    /**
     * loadState – restore AtomSpace and RR hypergraph from disk.
     */
    bool loadState(const std::string& as_file, const std::string& rr_file) {
        bool ok = persistence.loadFromFile(&atomspace, as_file);
        ok = ok && persistence.loadRRHypergraph(&rr_graph, rr_file);
        if (ok) rr_atomspace_bridge.performIntegration();
        return ok;
    }

    /**
     * report – formatted text summary of current cognitive state.
     */
    std::string report() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3);
        oss << "=== OpenCog AGI State (cycle " << cycle_count << ") ===\n";
        oss << "  AtomSpace atoms   : " << atomspace.atoms.size() << "\n";
        oss << "  RR nodes          : " << rr_graph.nodes.size() << "\n";
        oss << "  RR edges          : " << rr_graph.edges.size() << "\n";
        oss << "  ECAN AF size      : " << ecan_engine.attentional_focus.size() << "\n";
        oss << "  Hebbian links     : " << ecan_engine.hebbian_links.size() << "\n";
        oss << "  PLN results       : " << pln_engine.getInferenceResults().size() << "\n";
        oss << "  MOSES best score  : " << moses_engine.best_overall.score << "\n";
        oss << "  Avg RR salience   : " << averageRRSalience() << "\n";
        oss << "  Avg RR coherence  : " << averageRRCoherence() << "\n";
        oss << "  Pending actions   : " << pending_actions.size() << "\n";
        for (const auto& d : drives) {
            oss << "  Drive " << d.name << ": level=" << d.level
                << " sat=" << d.satisfaction
                << " urgency=" << d.urgency << "\n";
        }
        return oss.str();
    }

private:
    // ─────────────────────────────────────────────────────────────────────
    // Initialisation
    // ─────────────────────────────────────────────────────────────────────

    void initialiseSubsystems() {
        // Seed RR hypergraph with a basic agent-arena-relation triad
        unsigned agent_id    = rr_graph.addMembraneNode(1, "agi_agent",    rr::AARType::AGENT);
        unsigned arena_id    = rr_graph.addMembraneNode(2, "world_arena",  rr::AARType::ARENA);
        unsigned rel_id      = rr_graph.addMembraneNode(3, "interaction",  rr::AARType::RELATION);
        rr_graph.addRelationEdge(agent_id, arena_id, rr::RREdge::INTERACTION, 0.5);
        rr_graph.addRelationEdge(arena_id, rel_id,   rr::RREdge::CO_CONSTRUCTION, 0.5);
        rr_graph.addRelationEdge(rel_id,   agent_id, rr::RREdge::CO_CONSTRUCTION, 0.5);

        // Sync to AtomSpace
        rr_atomspace_bridge.performIntegration();

        // Register RR nodes in ECAN
        for (const auto& kv : rr_graph.nodes) {
            ecan_engine.registerAtom(kv.first, 50, 10);
        }

        // OpenPsi drives
        drives.push_back(OpenPsiDrive("competence", 0.5, 0.5));
        drives.push_back(OpenPsiDrive("integrity",  0.4, 0.6));
        drives.push_back(OpenPsiDrive("exploration",0.6, 0.3));

        // Seed MOSES with some training samples
        moses_engine.addTrainingSample({true, false, true}, true);
        moses_engine.addTrainingSample({false, true, false}, false);
        moses_engine.addTrainingSample({true, true, false}, true);

        // Initialise MOSES
        std::vector<unsigned> initial_features = {0, 1, 2};
        moses_engine.selected_features         = initial_features;
        moses_engine.initialise();
    }

    // ─────────────────────────────────────────────────────────────────────
    // Helpers
    // ─────────────────────────────────────────────────────────────────────

    // Find an existing RR node with the given label, or create a new one.
    // Ensures repeated perception of the same concept does not produce
    // duplicate RR nodes (each with a fresh ID) that fragment attention
    // tracking across multiple independent entries.
    unsigned findOrCreateRRNode(const std::string& label, rr::AARType aar_type) {
        for (const auto& kv : rr_graph.nodes) {
            if (kv.second && kv.second->label == label) {
                return kv.first;
            }
        }
        return rr_graph.addObjectNode(label, aar_type);
    }

    // Translate an AtomSpace atom ID to its corresponding RR node ID, or 0
    // if no mapping exists. First consults the integrator's explicit
    // mapping, then falls back to label lookup so atoms created directly
    // (e.g. via findOrCreateConcept) can still be matched to RR nodes with
    // the same name.
    unsigned atomToRRNode(unsigned atom_id) const {
        auto it = rr_atomspace_bridge.atom_to_rr_node.find(atom_id);
        if (it != rr_atomspace_bridge.atom_to_rr_node.end()) {
            return it->second;
        }
        auto atom = atomspace.getAtom(atom_id);
        if (!atom) return 0;
        for (const auto& kv : rr_graph.nodes) {
            if (kv.second && kv.second->label == atom->name) {
                return kv.first;
            }
        }
        return 0;
    }

    unsigned findOrCreateConcept(const std::string& name,
                                 double strength = 0.5,
                                 double confidence = 0.5) {
        auto existing = atomspace.findAtomsByName(name);
        if (!existing.empty()) {
            auto atom = atomspace.getAtom(existing[0]);
            if (atom) {
                atom->strength   = std::max(atom->strength,   strength);
                atom->confidence = std::max(atom->confidence, confidence);
            }
            return existing[0];
        }
        return atomspace.addConceptNode(name, strength, confidence);
    }

    std::vector<unsigned> collectWageEarners() {
        // Atoms that were part of recent PLN conclusions earn wages.
        // Wages are paid by ECAN, which is keyed by RR node IDs, so we must
        // translate each outgoing AtomSpace id to its RR counterpart before
        // handing the list to ecan_engine.step().
        std::vector<unsigned> earners;
        auto impl_links = atomspace.findAtomsOfType(atomspace::Atom::IMPLICATION_LINK);
        for (unsigned id : impl_links) {
            auto atom = atomspace.getAtom(id);
            if (atom && atom->strength > 0.7) {
                for (unsigned outgoing_id : atom->outgoing) {
                    unsigned rr_id = atomToRRNode(outgoing_id);
                    if (rr_id != 0) {
                        earners.push_back(rr_id);
                    }
                }
            }
        }
        return earners;
    }

    // `to_prune` contains RR node IDs (from ecan_engine.runForgetting()).
    // We must translate each RR ID to its mapped AtomSpace atom ID before
    // erasing, otherwise we would delete unrelated AtomSpace atoms that
    // happen to share the numeric ID. We also clean up the corresponding
    // entries in ECAN, the RR hypergraph, and the bidirectional mapping so
    // the three subsystems remain consistent.
    void pruneAtoms(const std::vector<unsigned>& to_prune) {
        for (unsigned rr_id : to_prune) {
            auto it = rr_atomspace_bridge.rr_node_to_atom.find(rr_id);
            if (it != rr_atomspace_bridge.rr_node_to_atom.end()) {
                unsigned atom_id = it->second;
                atomspace.atoms.erase(atom_id);
                rr_atomspace_bridge.atom_to_rr_node.erase(atom_id);
                rr_atomspace_bridge.rr_node_to_atom.erase(it);
            }
            ecan_engine.attention_values.erase(rr_id);
            rr_graph.nodes.erase(rr_id);
            rr_graph.agent_nodes.erase(rr_id);
            rr_graph.arena_nodes.erase(rr_id);
        }
    }

    void updateDrives() {
        for (auto& d : drives) {
            // Drives increase slowly if not satisfied
            d.update(0.01, 0.0);
        }
    }

    CognitiveAction selectAction() {
        // Find most urgent drive and select corresponding action
        OpenPsiDrive* most_urgent = nullptr;
        for (auto& d : drives) {
            if (!most_urgent || d.urgency > most_urgent->urgency) {
                most_urgent = &d;
            }
        }

        if (!most_urgent || !most_urgent->isUrgent()) {
            return CognitiveAction();
        }

        // Check if MOSES found a relevant program
        if (moses_engine.best_overall.tree && moses_engine.best_overall.score > -0.2) {
            std::string action = "execute_" + most_urgent->name
                               + "_via_" + moses_engine.best_overall.tree->toString();
            return CognitiveAction(action, 1.0 + moses_engine.best_overall.score,
                                   "MOSES+OpenPsi");
        }

        // Fall back to PLN-derived action
        auto impl_links = atomspace.findAtomsOfType(atomspace::Atom::IMPLICATION_LINK);
        for (unsigned id : impl_links) {
            auto atom = atomspace.getAtom(id);
            if (atom && atom->strength > 0.6) {
                return CognitiveAction("satisfy_" + most_urgent->name,
                                       atom->strength, "PLN+OpenPsi");
            }
        }

        // Default reflex
        return CognitiveAction("explore_" + most_urgent->name,
                               most_urgent->urgency, "OpenPsi");
    }

    double averageRRSalience() const {
        if (rr_graph.nodes.empty()) return 0.0;
        double total = 0.0;
        for (const auto& kv : rr_graph.nodes) total += kv.second->salience;
        return total / rr_graph.nodes.size();
    }

    double averageRRAffordance() const {
        if (rr_graph.nodes.empty()) return 0.0;
        double total = 0.0;
        for (const auto& kv : rr_graph.nodes) total += kv.second->affordance_realization;
        return total / rr_graph.nodes.size();
    }

    double averageRRCoherence() const {
        if (rr_graph.nodes.empty()) return 0.0;
        double total = 0.0;
        for (const auto& kv : rr_graph.nodes) total += kv.second->computeTrialecticCoherence();
        return total / rr_graph.nodes.size();
    }
};

}} // namespace plingua::opencog

#endif // _OPENCOG_AGI_HPP_

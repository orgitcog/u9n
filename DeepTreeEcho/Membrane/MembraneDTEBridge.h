#pragma once

// MembraneDTEBridge.h
// Bridges the pure P-Lingua AGI (RRR-P-Sys-Cog) with the Deep Tree Echo architecture.
// Maps:
// 1. Membrane RRHypergraph <-> DTE RelevanceKernel (dte::rr)
// 2. Membrane AtomSpace <-> DTE ATenSpaceLite (dte::aspace)
// 3. Membrane OpenPsi <-> DTE Cog0TaskSubsystem (dte::Cog0TaskSubsystem)
// 4. Membrane ECAN <-> DTE EndocrineSystem (dte::EndocrineSystem)

#include "opencog_agi.hpp"
#include "../Relevance/RelevanceKernel.h"
#include "../Distributed/ATenSpaceLite.h"
#include "../Executive/Cog0TaskSubsystem.h"
#include "../Avatar/EndocrineBus.h"

#include <memory>
#include <string>
#include <vector>

namespace dte {
namespace membrane {

/// MembraneDTEBridge — synchronizes the pure P-Lingua AGI cognitive cycle
/// with the Deep Tree Echo subsystems (reservoir, relevance, distributed,
/// executive, endocrine). Each synchronizeCycle() call runs one full
/// OpenCogAGI step and propagates state bidirectionally.
class MembraneDTEBridge {
public:
    MembraneDTEBridge(
        std::shared_ptr<dte::rr::RelevanceCore> rr_core,
        std::shared_ptr<dte::aspace::AtomSpace> atenspace,
        std::shared_ptr<dte::Cog0TaskSubsystem> executive,
        std::shared_ptr<dte::EndocrineSystem> endocrine
    ) : rr_core_(rr_core),
        atenspace_(atenspace),
        executive_(executive),
        endocrine_(endocrine)
    {
        agi_ = std::make_unique<plingua::opencog::OpenCogAGI>();
    }

    /// Run a synchronized cognitive cycle across both architectures.
    void synchronizeCycle(const std::string& percept,
                          const std::vector<float>& reservoir_embedding) {
        // 1. DTE Perception -> Membrane
        double base_salience = computeSalienceFromEmbedding(reservoir_embedding);
        agi_->perceive(percept, base_salience);

        // 2. Run Membrane Cognitive Cycle (ECAN -> PLN -> MOSES -> OpenPsi -> RR)
        auto step_result = agi_->step();

        // 3. Membrane -> DTE Relevance
        syncRelevanceState(step_result);

        // 4. Membrane -> DTE Distributed ATenSpace
        syncKnowledgeState();

        // 5. Membrane -> DTE Executive
        syncExecutiveState();

        // 6. Membrane -> DTE Endocrine
        syncEndocrineState(step_result);
    }

    const plingua::opencog::OpenCogAGI* getMembraneEngine() const {
        return agi_.get();
    }

    unsigned getCycleCount() const {
        return agi_ ? agi_->cycle_count : 0;
    }

private:
    std::unique_ptr<plingua::opencog::OpenCogAGI> agi_;
    std::shared_ptr<dte::rr::RelevanceCore> rr_core_;
    std::shared_ptr<dte::aspace::AtomSpace> atenspace_;
    std::shared_ptr<dte::Cog0TaskSubsystem> executive_;
    std::shared_ptr<dte::EndocrineSystem> endocrine_;

    double computeSalienceFromEmbedding(const std::vector<float>& emb) {
        if (emb.empty()) return 0.5;
        double sum_sq = 0.0;
        for (float v : emb) sum_sq += v * v;
        double norm = std::sqrt(sum_sq);
        // Bounded affine map: a zero-energy reservoir state still yields the
        // default perceptual salience (0.5); high-energy states saturate at 1.
        // salience = 0.5 + 0.5 * tanh(norm)  ∈ [0.5, 1.0)
        return 0.5 + 0.5 * std::tanh(norm);
    }

    void syncRelevanceState(const plingua::opencog::CognitiveStep& step) {
        if (!rr_core_) return;
        // Feed membrane RR coherence/salience as a provider observation
        // into the DTE RelevanceCore's context for the next relevance pass.
        // The RelevanceCore uses addProvider() for continuous sources;
        // here we inject a one-shot observation via setThreshold modulation.
        rr_core_->setThreshold(dte::rr::RelevanceMode::SelectiveAttention,
                               0.5 - step.rr_coherence * 0.3);
    }

    void syncKnowledgeState() {
        if (!atenspace_) return;
        // Extract high-STI atoms from the Membrane AtomSpace and mirror
        // them to ATenSpaceLite for cluster gossip.
        auto focus = agi_->ecan_engine.getAttentionalFocus();
        for (unsigned rr_id : focus) {
            if (agi_->rr_atomspace_bridge.rr_node_to_atom.count(rr_id)) {
                unsigned atom_id = agi_->rr_atomspace_bridge.rr_node_to_atom[rr_id];
                if (agi_->atomspace.atoms.count(atom_id)) {
                    auto& node = agi_->atomspace.atoms[atom_id];
                    // Mirror into the distributed ATenSpace with empty embedding
                    atenspace_->addNode(
                        dte::aspace::Atom::Type::CONCEPT_NODE,
                        node->name,
                        std::vector<float>()
                    );
                }
            }
        }
    }

    void syncExecutiveState() {
        if (!executive_) return;
        // Map OpenPsi drives to Cog0 goals
        for (const auto& drive : agi_->drives) {
            if (drive.isUrgent()) {
                executive_->addSubgoal(0, drive.name);
            }
        }
    }

    void syncEndocrineState(const plingua::opencog::CognitiveStep& step) {
        if (!endocrine_) return;
        // High RR coherence -> Serotonin (satisfaction/stability)
        if (step.rr_coherence > 0.7) {
            endocrine_->submitStimulus(dte::Stimulus("membrane_coherence", 0.8));
        }
        // High OpenPsi urgency -> Cortisol (stress/focus)
        if (step.primary_drive_urgency > 0.6) {
            endocrine_->submitStimulus(dte::Stimulus("membrane_urgency", 0.7));
        }
        // High PLN/MOSES activity -> Dopamine (reward/exploration)
        if (step.pln_conclusions > 5 || step.moses_best_score > 0.8) {
            endocrine_->submitStimulus(dte::Stimulus("membrane_insight", 0.9));
        }
    }
};

} // namespace membrane
} // namespace dte

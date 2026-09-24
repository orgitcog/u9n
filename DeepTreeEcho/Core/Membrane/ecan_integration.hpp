// Vendored from ReZorg/RRR-P-Sys-Cog (Pure P-Lingua AGI with RR dynamics)
// Modified to strip serialization dependencies for DTE integration

#ifndef _ECAN_INTEGRATION_HPP_
#define _ECAN_INTEGRATION_HPP_

/*
 * ecan_integration.hpp
 *
 * C++ simulation bridge for the ECAN (Economic Attention Network) subsystem
 * of the unified OpenCog AGI P-Lingua model.
 *
 * This header provides the C++ types and algorithms that correspond to the
 * membrane-computing rules defined in opencog_ecan.pli.  The simulator
 * layer calls these classes to drive the P-system dynamics.
 */

#include <vector>
#include <map>
#include <set>
#include <string>
#include <memory>
#include <cmath>
#include <algorithm>
#include "atomspace_integration.hpp"
#include "relevance_realization.hpp"

namespace plingua { namespace ecan {

// ─────────────────────────────────────────────────────────────────────────────
// AttentionValue – mirrors OpenCog's AttentionValue (STI / LTI / VLTI)
// ─────────────────────────────────────────────────────────────────────────────

struct AttentionValue {
    static constexpr short  STI_MIN  = -32768;
    static constexpr short  STI_MAX  =  32767;
    static constexpr unsigned short LTI_MIN  =      0;
    static constexpr unsigned short LTI_MAX  =  65535;

    short          sti;    // Short-term importance
    unsigned short lti;    // Long-term importance
    bool           vlti;   // Very long-term importance flag

    AttentionValue(short s = 0, unsigned short l = 0, bool v = false)
        : sti(s), lti(l), vlti(v) {}

    static AttentionValue createSTI(short s) {
        return AttentionValue(s, 0, false);
    }

    // STI bounded mutation
    void boostSTI(short delta) {
        int newSTI = static_cast<int>(sti) + delta;
        sti = static_cast<short>(std::max<int>(STI_MIN, std::min<int>(STI_MAX, newSTI)));
    }

    // Consolidate sustained STI into LTI
    void consolidateLTI(int boostAmount = 1) {
        unsigned int newLTI = static_cast<unsigned int>(lti) + boostAmount;
        lti = static_cast<unsigned short>(std::min<unsigned int>(LTI_MAX, newLTI));
    }

    bool isInAttentionalFocus(short threshold = 100) const {
        return sti >= threshold;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// HebbianLink – associative link between two atoms, updated by co-activation
// ─────────────────────────────────────────────────────────────────────────────

struct HebbianLink {
    unsigned atom_a;
    unsigned atom_b;
    double   weight;   // Hebbian weight in [0, 1]
    double   decay;    // Per-step decay rate

    HebbianLink(unsigned a, unsigned b, double w = 0.1, double d = 0.01)
        : atom_a(a), atom_b(b), weight(w), decay(d) {}

    // Hebbian update: weight increases on co-activation
    void updateOnCoActivation(double learning_rate = 0.05) {
        weight = std::min(1.0, weight + learning_rate * (1.0 - weight));
    }

    // Decay when atoms are not co-activated
    void applyDecay() {
        weight = std::max(0.0, weight - decay);
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// AttentionBank – manages the global STI pool (conservation law)
// ─────────────────────────────────────────────────────────────────────────────

class AttentionBank {
public:
    double totalSTI;            // Total STI in the system (conserved)
    double rentRate;            // Fraction of STI collected as rent per step
    double wageFraction;        // Fraction of rent redistributed as wages
    short  afThreshold;         // Minimum STI for attentional focus

    AttentionBank(double total = 10000.0, double rent = 0.01,
                  double wage = 0.7, short afThresh = 100)
        : totalSTI(total), rentRate(rent),
          wageFraction(wage), afThreshold(afThresh) {}

    // Collect rent from all atoms; return total rent collected
    double collectRent(std::map<unsigned, AttentionValue>& avs) {
        double rentCollected = 0.0;
        for (auto& kv : avs) {
            if (kv.second.sti > 0) {
                short rent = static_cast<short>(
                    std::ceil(kv.second.sti * rentRate));
                kv.second.boostSTI(-rent);
                rentCollected += rent;
            }
        }
        return rentCollected;
    }

    // Distribute wages to atoms that contributed to inference
    void payWages(std::map<unsigned, AttentionValue>& avs,
                  const std::vector<unsigned>& wage_earners,
                  double rent_pool) {
        if (wage_earners.empty()) return;
        double wage_per_atom = (rent_pool * wageFraction) / wage_earners.size();
        for (unsigned id : wage_earners) {
            if (avs.count(id)) {
                avs[id].boostSTI(static_cast<short>(wage_per_atom));
            }
        }
    }

    // Update attentional focus set
    std::set<unsigned> computeAF(const std::map<unsigned, AttentionValue>& avs) const {
        std::set<unsigned> af;
        for (const auto& kv : avs) {
            if (kv.second.sti >= afThreshold) {
                af.insert(kv.first);
            }
        }
        return af;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// ECANEngine – top-level ECAN simulator
// ─────────────────────────────────────────────────────────────────────────────

class ECANEngine {
public:
    std::map<unsigned, AttentionValue>  attention_values;
    std::vector<HebbianLink>            hebbian_links;
    AttentionBank                       bank;
    std::set<unsigned>                  attentional_focus;

    // Configuration
    double ltiConsolidationThreshold;  // STI boost count needed for LTI gain
    double forgettingThreshold;        // LTI below which atoms may be removed

    ECANEngine()
        : bank(), ltiConsolidationThreshold(3.0), forgettingThreshold(0.0) {}

    // Register an atom with an initial attention value
    void registerAtom(unsigned id, short sti = 0, unsigned short lti = 0) {
        attention_values[id] = AttentionValue(sti, lti, false);
    }

    // Boost STI of an atom (used access, inference result, etc.)
    void stimulate(unsigned id, short delta) {
        if (attention_values.count(id)) {
            attention_values[id].boostSTI(delta);
        }
    }

    // Update from RR salience (high salience → high STI injection)
    // Conversion: maps rr_salience in [0,1] to an STI delta in [-100, +100].
    // RR salience 1.0 → STI delta +100 (maximum boost)
    // RR salience 0.5 → STI delta   0  (neutral)
    // RR salience 0.0 → STI delta -100 (maximum decay)
    static constexpr double RR_SALIENCE_SCALE  = 200.0; // full-range scaling
    static constexpr double RR_SALIENCE_OFFSET = 100.0; // centres around 0
    void updateFromRRSalience(unsigned id, double rr_salience) {
        if (!attention_values.count(id)) {
            registerAtom(id);
        }
        short stiDelta = static_cast<short>(
            rr_salience * RR_SALIENCE_SCALE - RR_SALIENCE_OFFSET);
        attention_values[id].boostSTI(stiDelta);
    }

    // Spread STI from source atom to neighbours via Hebbian links
    void spreadSTI(unsigned source_id, double spread_factor = 0.2) {
        if (!attention_values.count(source_id)) return;
        short source_sti = attention_values[source_id].sti;
        if (source_sti <= 0) return;

        for (auto& link : hebbian_links) {
            unsigned neighbour = 0;
            if (link.atom_a == source_id) neighbour = link.atom_b;
            else if (link.atom_b == source_id) neighbour = link.atom_a;
            else continue;

            if (attention_values.count(neighbour)) {
                short delta = static_cast<short>(
                    source_sti * spread_factor * link.weight);
                attention_values[neighbour].boostSTI(delta);
                attention_values[source_id].boostSTI(-delta); // conservation
            }
        }
    }

    // Add or update a Hebbian link between two atoms
    void updateHebbianLink(unsigned a, unsigned b, bool co_activated) {
        for (auto& link : hebbian_links) {
            if ((link.atom_a == a && link.atom_b == b) ||
                (link.atom_a == b && link.atom_b == a)) {
                if (co_activated) link.updateOnCoActivation();
                else              link.applyDecay();
                return;
            }
        }
        // Create new Hebbian link
        if (co_activated) {
            hebbian_links.push_back(HebbianLink(a, b, 0.1));
        }
    }

    // Consolidate LTI from sustained STI
    void consolidateLTI() {
        for (auto& kv : attention_values) {
            AttentionValue& av = kv.second;
            if (av.sti >= static_cast<short>(bank.afThreshold) * 2) {
                av.consolidateLTI(2);
            } else if (av.sti >= bank.afThreshold) {
                av.consolidateLTI(1);
            }
        }
    }

    // Run forgetting: identify atoms eligible for removal
    std::vector<unsigned> runForgetting() {
        std::vector<unsigned> to_forget;
        for (const auto& kv : attention_values) {
            const AttentionValue& av = kv.second;
            if (!av.vlti && av.sti <= 0 && av.lti <= forgettingThreshold) {
                to_forget.push_back(kv.first);
            }
        }
        return to_forget;
    }

    // Execute a full ECAN step
    std::vector<unsigned> step(const std::vector<unsigned>& wage_earners) {
        // 1. Collect rent
        double rent = bank.collectRent(attention_values);

        // 2. Pay wages
        bank.payWages(attention_values, wage_earners, rent);

        // 3. Spread STI from AF members
        auto current_af = bank.computeAF(attention_values);
        for (unsigned af_id : current_af) {
            spreadSTI(af_id, 0.15);
        }
        attentional_focus = current_af;

        // 4. Consolidate LTI
        consolidateLTI();

        // 5. Hebbian decay for non-co-activated links
        for (auto& link : hebbian_links) {
            bool a_in_af = current_af.count(link.atom_a) > 0;
            bool b_in_af = current_af.count(link.atom_b) > 0;
            if (a_in_af && b_in_af) {
                link.updateOnCoActivation();
            } else {
                link.applyDecay();
            }
        }

        // 6. Forgetting
        return runForgetting();
    }

    // Export ECAN state as RR salience values
    double computeRRSalience(unsigned id) const {
        auto it = attention_values.find(id);
        if (it == attention_values.end()) return 0.0;
        // Normalise STI to [0, 1]
        return (static_cast<double>(it->second.sti) - AttentionValue::STI_MIN) /
               (AttentionValue::STI_MAX - AttentionValue::STI_MIN);
    }

    // Get attentional focus (set of high-STI atom IDs)
    const std::set<unsigned>& getAttentionalFocus() const {
        return attentional_focus;
    }

    // Report summary
    std::string summary() const {
        std::string s = "ECAN: atoms=" + std::to_string(attention_values.size());
        s += " AF=" + std::to_string(attentional_focus.size());
        s += " links=" + std::to_string(hebbian_links.size());
        return s;
    }
};

}} // namespace plingua::ecan

#endif // _ECAN_INTEGRATION_HPP_

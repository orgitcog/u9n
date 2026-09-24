// Vendored from ReZorg/RRR-P-Sys-Cog (Pure P-Lingua AGI with RR dynamics)
// Modified to strip serialization dependencies for DTE integration

#ifndef _MOSES_INTEGRATION_HPP_
#define _MOSES_INTEGRATION_HPP_

/*
 * moses_integration.hpp
 *
 * C++ simulation bridge for the MOSES (Meta-Optimizing Semantic Evolutionary
 * Search) subsystem of the unified OpenCog AGI P-Lingua model.
 *
 * This header provides the C++ types and algorithms that correspond to the
 * membrane-computing rules defined in opencog_moses.pli.  The simulator
 * layer calls these classes to drive program-learning P-system dynamics.
 */

#include <vector>
#include <map>
#include <set>
#include <string>
#include <memory>
#include <cmath>
#include <algorithm>
#include <functional>
#include <random>
#include "relevance_realization.hpp"
#include "atomspace_integration.hpp"

namespace plingua { namespace moses {

// ─────────────────────────────────────────────────────────────────────────────
// ComboNode – node in a combo program tree (simplified)
// ─────────────────────────────────────────────────────────────────────────────

struct ComboNode {
    enum class Type {
        AND, OR, NOT, FEATURE, CONSTANT_TRUE, CONSTANT_FALSE
    };

    Type                            type;
    unsigned                        feature_id;  // for FEATURE nodes
    std::vector<std::shared_ptr<ComboNode>> children;

    ComboNode(Type t, unsigned fid = 0) : type(t), feature_id(fid) {}

    // Depth of the tree
    int depth() const {
        if (children.empty()) return 1;
        int max_d = 0;
        for (const auto& c : children) {
            max_d = std::max(max_d, c->depth());
        }
        return 1 + max_d;
    }

    std::string toString() const {
        switch (type) {
            case Type::AND:  { std::string s = "(and";
                               for (const auto& c : children) s += " " + c->toString();
                               return s + ")"; }
            case Type::OR:   { std::string s = "(or";
                               for (const auto& c : children) s += " " + c->toString();
                               return s + ")"; }
            case Type::NOT:  return "(not " + (children.empty() ? "?" : children[0]->toString()) + ")";
            case Type::FEATURE: return "f" + std::to_string(feature_id);
            case Type::CONSTANT_TRUE:  return "#t";
            case Type::CONSTANT_FALSE: return "#f";
        }
        return "?";
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// BehavioralScore – fitness function for combo programs
// ─────────────────────────────────────────────────────────────────────────────

struct TrainingSample {
    std::vector<bool> inputs;
    bool              output;
};

class BehavioralScore {
public:
    std::vector<TrainingSample> training_data;

    void addSample(const std::vector<bool>& inputs, bool output) {
        training_data.push_back({inputs, output});
    }

    // Width of the input vector (taken from the widest training sample). A
    // ComboNode FEATURE's `feature_id` is used as a direct index into
    // `sample.inputs` in `evalNode`, so any feature ID ≥ this value is
    // out-of-bounds and always evaluates to `false`. Callers that derive
    // feature IDs from external sources (e.g. RR node IDs) must bound them
    // by this dimension to keep the feature space usable.
    std::size_t inputDimension() const {
        std::size_t dim = 0;
        for (const auto& s : training_data) {
            dim = std::max(dim, s.inputs.size());
        }
        return dim;
    }

    // Evaluate program on training data; returns score in [-1.0, 0.0]
    // (MOSES convention: 0 is perfect, -1.0 is worst possible)
    double evaluate(const ComboNode& program) const {
        if (training_data.empty()) return 0.0;
        int correct = 0;
        for (const auto& sample : training_data) {
            bool predicted = evalNode(program, sample.inputs);
            if (predicted == sample.output) ++correct;
        }
        double accuracy = static_cast<double>(correct) / training_data.size();
        return -(1.0 - accuracy);  // Negative: 0 = perfect
    }

private:
    bool evalNode(const ComboNode& node, const std::vector<bool>& inputs) const {
        switch (node.type) {
            case ComboNode::Type::CONSTANT_TRUE:  return true;
            case ComboNode::Type::CONSTANT_FALSE: return false;
            case ComboNode::Type::FEATURE:
                return node.feature_id < inputs.size() ? inputs[node.feature_id] : false;
            case ComboNode::Type::NOT:
                return node.children.empty() ? false : !evalNode(*node.children[0], inputs);
            case ComboNode::Type::AND: {
                bool result = true;
                for (const auto& c : node.children) result = result && evalNode(*c, inputs);
                return result;
            }
            case ComboNode::Type::OR: {
                bool result = false;
                for (const auto& c : node.children) result = result || evalNode(*c, inputs);
                return result;
            }
        }
        return false;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Deme – local island population of combo programs
// ─────────────────────────────────────────────────────────────────────────────

struct CandidateProgram {
    std::shared_ptr<ComboNode> tree;
    double                     score;

    CandidateProgram() : score(-1.0) {}
    CandidateProgram(std::shared_ptr<ComboNode> t, double s) : tree(t), score(s) {}

    bool operator<(const CandidateProgram& other) const {
        return score > other.score;  // Higher score = better
    }
};

class Deme {
public:
    std::vector<CandidateProgram> population;
    unsigned                      max_size;
    double                        mutation_rate;
    mutable std::mt19937          rng;

    Deme(unsigned max_sz = 100, double mut_rate = 0.1)
        : max_size(max_sz), mutation_rate(mut_rate),
          rng(std::random_device{}()) {}

    // Initialise random population from feature set
    void initialise(const std::vector<unsigned>& features, const BehavioralScore& scorer) {
        population.clear();
        for (unsigned i = 0; i < max_size; ++i) {
            auto tree = randomProgram(features);
            double score = scorer.evaluate(*tree);
            population.push_back(CandidateProgram(tree, score));
        }
        std::sort(population.begin(), population.end());
    }

    // Tournament selection: pick best of k random candidates. Callers must
    // ensure `population` is non-empty; an empty population would divide by
    // zero via `rng() % population.size()`.
    const CandidateProgram& tournamentSelect(unsigned k = 3) const {
        unsigned best = rng() % population.size();
        for (unsigned i = 1; i < k; ++i) {
            unsigned candidate = rng() % population.size();
            if (population[candidate].score > population[best].score) {
                best = candidate;
            }
        }
        return population[best];
    }

    // Single-step evolution: selection + crossover + mutation
    void evolveStep(const BehavioralScore& scorer, const std::vector<unsigned>& features) {
        // Tournament selection requires a non-empty population; bail out
        // early if the deme was never initialised (e.g. no features were
        // selected at MOSESEngine::initialise() time).
        if (population.empty()) return;

        std::vector<CandidateProgram> next_gen;
        next_gen.reserve(max_size);

        // Elitism: keep best
        next_gen.push_back(population[0]);

        while (next_gen.size() < max_size) {
            const auto& parent_a = tournamentSelect();
            const auto& parent_b = tournamentSelect();

            auto offspring = crossover(*parent_a.tree, *parent_b.tree);
            if (static_cast<double>(rng()) / rng.max() < mutation_rate) {
                offspring = mutate(*offspring, features);
            }

            double score = scorer.evaluate(*offspring);
            next_gen.push_back(CandidateProgram(offspring, score));
        }

        population = next_gen;
        std::sort(population.begin(), population.end());
    }

    // Best candidate
    const CandidateProgram& best() const {
        return population.front();
    }

    bool hasImproved(double prev_best) const {
        return !population.empty() && population.front().score > prev_best;
    }

private:
    // Generate a random combo tree from available features
    std::shared_ptr<ComboNode> randomProgram(const std::vector<unsigned>& features, int depth = 0) {
        if (depth >= 3 || features.empty() || rng() % 3 == 0) {
            // Leaf: feature or constant
            if (!features.empty() && rng() % 4 != 0) {
                unsigned fid = features[rng() % features.size()];
                return std::make_shared<ComboNode>(ComboNode::Type::FEATURE, fid);
            } else {
                return std::make_shared<ComboNode>(
                    rng() % 2 == 0 ? ComboNode::Type::CONSTANT_TRUE
                                   : ComboNode::Type::CONSTANT_FALSE);
            }
        }

        // Internal node
        ComboNode::Type op;
        int op_choice = rng() % 3;
        if (op_choice == 0) op = ComboNode::Type::AND;
        else if (op_choice == 1) op = ComboNode::Type::OR;
        else op = ComboNode::Type::NOT;

        auto node = std::make_shared<ComboNode>(op);
        int num_children = (op == ComboNode::Type::NOT) ? 1 : (1 + rng() % 3);
        for (int i = 0; i < num_children; ++i) {
            node->children.push_back(randomProgram(features, depth + 1));
        }
        return node;
    }

    // Subtree crossover
    std::shared_ptr<ComboNode> crossover(const ComboNode& a, const ComboNode& b) {
        // Simple: return a copy of parent_a's root with one child from parent_b
        auto result = std::make_shared<ComboNode>(a.type, a.feature_id);
        for (const auto& c : a.children) {
            result->children.push_back(std::make_shared<ComboNode>(*c));
        }
        if (!result->children.empty() && !b.children.empty()) {
            unsigned idx = rng() % result->children.size();
            unsigned src = rng() % b.children.size();
            result->children[idx] = std::make_shared<ComboNode>(*b.children[src]);
        }
        return result;
    }

    // Point mutation: change one node
    std::shared_ptr<ComboNode> mutate(const ComboNode& node,
                                      const std::vector<unsigned>& features) {
        auto result = std::make_shared<ComboNode>(node.type, node.feature_id);
        for (const auto& c : node.children) {
            result->children.push_back(std::make_shared<ComboNode>(*c));
        }
        if (!result->children.empty() && rng() % 2 == 0) {
            unsigned idx = rng() % result->children.size();
            result->children[idx] = randomProgram(features, 2);
        } else if (!features.empty()) {
            unsigned fid = features[rng() % features.size()];
            result->feature_id = fid;
        }
        return result;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// MOSESEngine – top-level MOSES simulator
// ─────────────────────────────────────────────────────────────────────────────

class MOSESEngine {
public:
    std::vector<Deme>           metapopulation;
    BehavioralScore             scorer;
    std::vector<unsigned>       selected_features;  // RR-guided feature set
    CandidateProgram            best_overall;

    unsigned max_demes;
    unsigned steps_per_deme;

    MOSESEngine(unsigned max_demes_ = 5, unsigned steps = 20)
        : max_demes(max_demes_), steps_per_deme(steps) {}

    // Add training sample
    void addTrainingSample(const std::vector<bool>& inputs, bool output) {
        scorer.addSample(inputs, output);
    }

    // Update feature set from RR salience (high salience → feature included).
    //
    // `feature_id` values are consumed by `BehavioralScore::evalNode` as
    // direct 0-based indices into each training sample's `inputs` vector.
    // RR node IDs, by contrast, start at 1 and grow unboundedly as new
    // nodes are added, so using them verbatim as feature IDs would leave
    // `inputs[0]` unreachable and cause every feature ID ≥ input dimension
    // to silently evaluate to `false` (regardless of salience). To keep
    // the feature space usable we rank salient RR nodes by salience and
    // map them onto contiguous 0-based indices bounded by the training
    // data's input dimension.
    void updateFeaturesFromRR(const plingua::rr::RRHypergraph* rr_graph,
                               double salience_threshold = 0.5) {
        selected_features.clear();
        const std::size_t dim = scorer.inputDimension();
        if (dim == 0) return;

        std::vector<std::pair<double, unsigned>> ranked;
        ranked.reserve(rr_graph->nodes.size());
        for (const auto& kv : rr_graph->nodes) {
            if (kv.second && kv.second->salience >= salience_threshold) {
                ranked.emplace_back(kv.second->salience, kv.first);
            }
        }
        std::sort(ranked.begin(), ranked.end(),
                  [](const std::pair<double, unsigned>& a,
                     const std::pair<double, unsigned>& b) {
                      return a.first > b.first;
                  });

        const std::size_t count = std::min(ranked.size(), dim);
        selected_features.reserve(count);
        for (std::size_t i = 0; i < count; ++i) {
            selected_features.push_back(static_cast<unsigned>(i));
        }
    }

    // Initialise metapopulation
    void initialise() {
        metapopulation.clear();
        for (unsigned d = 0; d < max_demes; ++d) {
            Deme deme(50, 0.1 + d * 0.05);
            if (!selected_features.empty()) {
                deme.initialise(selected_features, scorer);
            }
            metapopulation.push_back(std::move(deme));
        }
    }

    // Run one MOSES generation across all demes
    void evolve() {
        for (auto& deme : metapopulation) {
            for (unsigned s = 0; s < steps_per_deme; ++s) {
                deme.evolveStep(scorer, selected_features);
            }

            // Update best overall
            if (!deme.population.empty()) {
                const auto& db = deme.best();
                if (!best_overall.tree || db.score > best_overall.score) {
                    best_overall = db;
                }
            }
        }
    }

    // Export best program score as RR salience update
    double computeRRSalience() const {
        // Normalise MOSES score (which is in (-1, 0]) to (0, 1)
        return 1.0 + best_overall.score;  // score = 0 → salience = 1.0
    }

    // Summary
    std::string summary() const {
        std::string s = "MOSES: demes=" + std::to_string(metapopulation.size());
        s += " features=" + std::to_string(selected_features.size());
        if (best_overall.tree) {
            s += " best_score=" + std::to_string(best_overall.score);
            s += " best=" + best_overall.tree->toString();
        }
        return s;
    }
};

}} // namespace plingua::moses

#endif // _MOSES_INTEGRATION_HPP_

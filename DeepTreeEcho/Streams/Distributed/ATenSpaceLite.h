#pragma once

/*
 * ATenSpaceLite.h — Tensor-embedded AtomSpace for Deep Tree Echo
 *
 * Ported from o9nn/ATenSpace (aten/src/ATen/atomspace/{Atom.h, AtomSpace.h,
 * TruthValue.h}) with the torch::Tensor dependency replaced by plain
 * std::vector<float> embeddings so the hypergraph can live inside the
 * DeepTreeEcho engine without libtorch. API shape (Atom/Node/Link/AtomSpace,
 * addNode/addLink dedup, incoming sets, querySimilar cosine top-k) is
 * preserved 1:1 from the upstream ATenSpace so future re-binding to ATen
 * tensors is a type-swap.
 *
 * Provenance:
 *   - o9nn/ATenSpace  aten/src/ATen/atomspace/Atom.h       (Node/Link types)
 *   - o9nn/ATenSpace  aten/src/ATen/atomspace/AtomSpace.h  (container/query)
 *   - o9nn/infernos   os/port/opencog.c                    (TruthValue fields)
 */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace dte {
namespace aspace {

/** Plain embedding vector (stand-in for at::Tensor). */
using Embedding = std::vector<float>;

/** Cosine similarity between two embeddings (0 if degenerate). */
inline float cosineSimilarity(const Embedding& a, const Embedding& b) {
    if (a.empty() || a.size() != b.size()) return 0.0f;
    float dot = 0.0f, na = 0.0f, nb = 0.0f;
    for (size_t i = 0; i < a.size(); ++i) {
        dot += a[i] * b[i];
        na  += a[i] * a[i];
        nb  += b[i] * b[i];
    }
    if (na <= 0.0f || nb <= 0.0f) return 0.0f;
    return dot / (std::sqrt(na) * std::sqrt(nb));
}

/**
 * TruthValue — uncertain-reasoning triple.
 * Field semantics follow the infernos kernel struct (strength, confidence,
 * count) which generalizes the upstream 2-element tensor {strength, confidence}.
 */
struct TruthValue {
    float strength   = 1.0f;  ///< probability-like degree of truth
    float confidence = 1.0f;  ///< confidence in the strength estimate
    float count      = 1.0f;  ///< evidence count (used for distributed merge)

    /**
     * Count-weighted merge of two distributed observations of the same atom.
     * This is the "distributed_coherence" rule: evidence accumulates, and
     * strength/confidence converge to the count-weighted average.
     */
    static TruthValue merge(const TruthValue& a, const TruthValue& b) {
        TruthValue out;
        const float total = a.count + b.count;
        if (total <= 0.0f) return out;
        out.strength   = (a.strength * a.count + b.strength * b.count) / total;
        out.confidence = (a.confidence * a.count + b.confidence * b.count) / total;
        out.count      = total;
        return out;
    }
};

class AtomSpace;

/**
 * Atom — base class for all atoms (Node or Link) in the hypergraph.
 * Immutable identity (hash) + mutable TruthValue/attention, exactly as in
 * upstream ATenSpace.
 */
class Atom : public std::enable_shared_from_this<Atom> {
public:
    using Handle     = std::shared_ptr<Atom>;
    using WeakHandle = std::weak_ptr<Atom>;

    enum class Type {
        NODE,
        LINK,
        CONCEPT_NODE,
        PREDICATE_NODE,
        VARIABLE_NODE,
        INHERITANCE_LINK,
        IMPLICATION_LINK,
        EVALUATION_LINK,
        LIST_LINK,
        ORDERED_LINK,
        UNORDERED_LINK,
        AND_LINK,
        OR_LINK,
        NOT_LINK,
        MEMBER_LINK,
        SUBSET_LINK,
        CONTEXT_LINK,
        SEQUENTIAL_LINK,
        SIMULTANEOUS_LINK,
        SIMILARITY_LINK,
        EXECUTION_LINK,
        HEBBIAN_LINK,
        SYMMETRIC_HEBBIAN_LINK,
        ASYMMETRIC_HEBBIAN_LINK,
        INVERSE_HEBBIAN_LINK,
        TYPED_VARIABLE_NODE,
        GLOB_NODE
    };

    virtual ~Atom() = default;

    virtual Type getType() const = 0;
    virtual std::string getTypeName() const = 0;
    virtual bool isNode() const = 0;
    virtual bool isLink() const = 0;

    size_t getHash() const { return hash_; }

    void setTruthValue(const TruthValue& tv) { truth_value_ = tv; }
    TruthValue getTruthValue() const { return truth_value_; }

    void setAttention(float attention) { attention_ = attention; }
    float getAttention() const { return attention_; }

    const std::vector<WeakHandle>& getIncomingSet() const { return incoming_set_; }
    void addIncoming(WeakHandle atom) { incoming_set_.push_back(std::move(atom)); }

    /** Number of still-live incoming references. */
    size_t liveIncomingCount() const {
        size_t n = 0;
        for (const auto& w : incoming_set_)
            if (!w.expired()) ++n;
        return n;
    }

    virtual std::string toString() const = 0;
    virtual bool equals(const Atom& other) const = 0;

protected:
    Atom() : hash_(0), attention_(0.0f) {}

    void computeHash(const std::string& content) {
        hash_ = std::hash<std::string>{}(content);
    }

    size_t hash_;
    TruthValue truth_value_;
    float attention_;
    std::vector<WeakHandle> incoming_set_;
};

/** Node — a named entity/concept, optionally with an embedding. */
class Node : public Atom {
public:
    Node(Type type, std::string name)
        : type_(type), name_(std::move(name)) {
        computeHash(getTypeName() + ":" + name_);
    }

    Node(Type type, std::string name, Embedding embedding)
        : type_(type), name_(std::move(name)), embedding_(std::move(embedding)) {
        computeHash(getTypeName() + ":" + name_);
    }

    Type getType() const override { return type_; }

    std::string getTypeName() const override {
        switch (type_) {
            case Type::NODE:                 return "Node";
            case Type::CONCEPT_NODE:         return "ConceptNode";
            case Type::PREDICATE_NODE:       return "PredicateNode";
            case Type::VARIABLE_NODE:        return "VariableNode";
            case Type::TYPED_VARIABLE_NODE:  return "TypedVariableNode";
            case Type::GLOB_NODE:            return "GlobNode";
            default:                         return "Node";
        }
    }

    bool isNode() const override { return true; }
    bool isLink() const override { return false; }

    const std::string& getName() const { return name_; }

    void setEmbedding(Embedding embedding) { embedding_ = std::move(embedding); }
    const Embedding& getEmbedding() const { return embedding_; }
    bool hasEmbedding() const { return !embedding_.empty(); }

    std::string toString() const override {
        return "(" + getTypeName() + " \"" + name_ + "\")";
    }

    bool equals(const Atom& other) const override {
        if (!other.isNode()) return false;
        const auto& o = static_cast<const Node&>(other);
        return type_ == o.type_ && name_ == o.name_;
    }

private:
    Type type_;
    std::string name_;
    Embedding embedding_;
};

/** Link — an ordered relationship over atoms, forming the hypergraph. */
class Link : public Atom {
public:
    using OutgoingSet = std::vector<Handle>;

    Link(Type type, OutgoingSet outgoing)
        : type_(type), outgoing_(std::move(outgoing)) {
        computeHashFromOutgoing();
    }

    Type getType() const override { return type_; }

    std::string getTypeName() const override {
        switch (type_) {
            case Type::LINK:                     return "Link";
            case Type::INHERITANCE_LINK:         return "InheritanceLink";
            case Type::IMPLICATION_LINK:         return "ImplicationLink";
            case Type::EVALUATION_LINK:          return "EvaluationLink";
            case Type::LIST_LINK:                return "ListLink";
            case Type::ORDERED_LINK:             return "OrderedLink";
            case Type::UNORDERED_LINK:           return "UnorderedLink";
            case Type::AND_LINK:                 return "AndLink";
            case Type::OR_LINK:                  return "OrLink";
            case Type::NOT_LINK:                 return "NotLink";
            case Type::MEMBER_LINK:              return "MemberLink";
            case Type::SUBSET_LINK:              return "SubsetLink";
            case Type::CONTEXT_LINK:             return "ContextLink";
            case Type::SEQUENTIAL_LINK:          return "SequentialLink";
            case Type::SIMULTANEOUS_LINK:        return "SimultaneousLink";
            case Type::SIMILARITY_LINK:          return "SimilarityLink";
            case Type::EXECUTION_LINK:           return "ExecutionLink";
            case Type::HEBBIAN_LINK:             return "HebbianLink";
            case Type::SYMMETRIC_HEBBIAN_LINK:   return "SymmetricHebbianLink";
            case Type::ASYMMETRIC_HEBBIAN_LINK:  return "AsymmetricHebbianLink";
            case Type::INVERSE_HEBBIAN_LINK:     return "InverseHebbianLink";
            default:                             return "Link";
        }
    }

    bool isNode() const override { return false; }
    bool isLink() const override { return true; }

    const OutgoingSet& getOutgoingSet() const { return outgoing_; }
    size_t getArity() const { return outgoing_.size(); }

    Handle getOutgoingAtom(size_t index) const {
        return index < outgoing_.size() ? outgoing_[index] : nullptr;
    }

    std::string toString() const override {
        std::string result = "(" + getTypeName();
        for (const auto& atom : outgoing_)
            result += "\n  " + atom->toString();
        result += ")";
        return result;
    }

    bool equals(const Atom& other) const override {
        if (!other.isLink()) return false;
        const auto& o = static_cast<const Link&>(other);
        if (type_ != o.type_ || outgoing_.size() != o.outgoing_.size()) return false;
        for (size_t i = 0; i < outgoing_.size(); ++i)
            if (!outgoing_[i]->equals(*o.outgoing_[i])) return false;
        return true;
    }

private:
    void computeHashFromOutgoing() {
        std::string content = getTypeName() + ":";
        for (const auto& atom : outgoing_)
            content += std::to_string(atom->getHash()) + ",";
        computeHash(content);
    }

    Type type_;
    OutgoingSet outgoing_;
};

/**
 * AtomSpace — thread-safe hypergraph container with dedup and semantic query.
 * Behavior mirrors upstream ATenSpace::AtomSpace, with querySimilar computing
 * cosine similarity in plain C++ instead of batched torch ops.
 */
class AtomSpace {
public:
    using Handle  = Atom::Handle;
    using AtomSet = std::unordered_set<Handle>;

    AtomSpace() = default;
    ~AtomSpace() = default;
    AtomSpace(const AtomSpace&) = delete;
    AtomSpace& operator=(const AtomSpace&) = delete;

    Handle addNode(Atom::Type type, const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string key = nodeKey(type, name);
        auto it = node_index_.find(key);
        if (it != node_index_.end()) return it->second;
        Handle node = std::make_shared<Node>(type, name);
        node_index_[key] = node;
        atoms_.insert(node);
        return node;
    }

    Handle addNode(Atom::Type type, const std::string& name, Embedding embedding) {
        std::lock_guard<std::mutex> lock(mutex_);
        const std::string key = nodeKey(type, name);
        auto it = node_index_.find(key);
        if (it != node_index_.end()) {
            if (auto n = std::dynamic_pointer_cast<Node>(it->second))
                n->setEmbedding(std::move(embedding));
            return it->second;
        }
        Handle node = std::make_shared<Node>(type, name, std::move(embedding));
        node_index_[key] = node;
        atoms_.insert(node);
        return node;
    }

    Handle addLink(Atom::Type type, const std::vector<Handle>& outgoing) {
        std::lock_guard<std::mutex> lock(mutex_);
        Handle link = std::make_shared<Link>(type, outgoing);
        const size_t hash = link->getHash();
        auto range = link_index_.equal_range(hash);
        for (auto it = range.first; it != range.second; ++it)
            if (it->second->equals(*link)) return it->second;
        link_index_.insert({hash, link});
        atoms_.insert(link);
        for (const auto& atom : outgoing)
            atom->addIncoming(link);
        return link;
    }

    Handle getNode(Atom::Type type, const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = node_index_.find(nodeKey(type, name));
        return it != node_index_.end() ? it->second : nullptr;
    }

    bool removeAtom(const Handle& atom) {
        if (!atom) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        if (atom->liveIncomingCount() > 0)
            return false;  // referenced atoms cannot be removed
        if (atom->isNode()) {
            if (auto n = std::dynamic_pointer_cast<Node>(atom))
                node_index_.erase(nodeKey(atom->getType(), n->getName()));
        } else {
            auto range = link_index_.equal_range(atom->getHash());
            for (auto it = range.first; it != range.second; ++it) {
                if (it->second == atom) { link_index_.erase(it); break; }
            }
        }
        atoms_.erase(atom);
        return true;
    }

    AtomSet getAtoms() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return atoms_;
    }

    std::vector<Handle> getAtomsByType(Atom::Type type) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<Handle> result;
        for (const auto& atom : atoms_)
            if (atom->getType() == type) result.push_back(atom);
        return result;
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return atoms_.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        atoms_.clear();
        node_index_.clear();
        link_index_.clear();
    }

    /** Cosine-similarity top-k semantic query over node embeddings. */
    std::vector<std::pair<Handle, float>> querySimilar(
            const Embedding& query, size_t k = 10, float threshold = 0.0f) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::pair<Handle, float>> results;
        for (const auto& atom : atoms_) {
            if (!atom->isNode()) continue;
            auto n = std::dynamic_pointer_cast<Node>(atom);
            if (!n || !n->hasEmbedding()) continue;
            const float sim = cosineSimilarity(query, n->getEmbedding());
            if (sim >= threshold) results.emplace_back(atom, sim);
        }
        std::sort(results.begin(), results.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        if (results.size() > k) results.resize(k);
        return results;
    }

private:
    static std::string nodeKey(Atom::Type type, const std::string& name) {
        return std::to_string(static_cast<int>(type)) + ":" + name;
    }

    mutable std::mutex mutex_;
    AtomSet atoms_;
    std::unordered_map<std::string, Handle> node_index_;
    std::unordered_multimap<size_t, Handle> link_index_;
};

} // namespace aspace
} // namespace dte

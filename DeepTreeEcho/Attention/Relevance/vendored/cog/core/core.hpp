// VENDORED from cogpy/cogpy-hpp @ main (2026-07-19) for DTE Relevance module. Do not edit in place; sync upstream.
// cog/core/core.hpp — Shared foundation types for all CogPy modules
// Header-only, C++11, zero external dependencies
// SPDX-License-Identifier: MIT
#ifndef COG_CORE_HPP
#define COG_CORE_HPP

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <sstream>
#include <stdexcept>
#include <atomic>
#include <array>
#include <type_traits>
#include <limits>
#include <utility>

namespace cog {

// ─────────────────────────────────────────────────────────────────────────────
// Handle & UUID
// ─────────────────────────────────────────────────────────────────────────────
using Handle = uint32_t;
using UUID   = uint64_t;

static constexpr Handle UNDEFINED_HANDLE = 0;

// ─────────────────────────────────────────────────────────────────────────────
// Atom Type Enumeration
// ─────────────────────────────────────────────────────────────────────────────
enum class AtomType : uint16_t {
    NOTYPE              = 0,
    // Nodes
    NODE                = 1,
    CONCEPT_NODE        = 2,
    PREDICATE_NODE      = 3,
    SCHEMA_NODE         = 4,
    GROUNDED_SCHEMA_NODE= 5,
    NUMBER_NODE         = 6,
    VARIABLE_NODE       = 7,
    ANCHOR_NODE         = 8,
    TYPE_NODE           = 9,
    DEFINED_SCHEMA_NODE = 10,
    // Links
    LINK                = 64,
    ORDERED_LINK        = 65,
    UNORDERED_LINK      = 66,
    LIST_LINK           = 67,
    SET_LINK            = 68,
    MEMBER_LINK         = 69,
    INHERITANCE_LINK    = 70,
    SIMILARITY_LINK     = 71,
    EVALUATION_LINK     = 72,
    EXECUTION_LINK      = 73,
    EXECUTION_OUTPUT_LINK = 74,
    BIND_LINK           = 75,
    AND_LINK            = 76,
    OR_LINK             = 77,
    NOT_LINK            = 78,
    IMPLICATION_LINK    = 79,
    EQUIVALENCE_LINK    = 80,
    CONTEXT_LINK        = 81,
    STATE_LINK          = 82,
    DEFINE_LINK         = 83,
    LAMBDA_LINK         = 84,
    PUT_LINK            = 85,
    GET_LINK            = 86,
    SATISFACTION_LINK   = 87,
    // Sentinel
    ATOM_TYPE_COUNT     = 128
};

inline bool is_node(AtomType t) { return static_cast<uint16_t>(t) < 64; }
inline bool is_link(AtomType t) { return static_cast<uint16_t>(t) >= 64; }

inline const char* atom_type_name(AtomType t) {
    switch (t) {
        case AtomType::NOTYPE:              return "NoType";
        case AtomType::NODE:                return "Node";
        case AtomType::CONCEPT_NODE:        return "ConceptNode";
        case AtomType::PREDICATE_NODE:      return "PredicateNode";
        case AtomType::SCHEMA_NODE:         return "SchemaNode";
        case AtomType::GROUNDED_SCHEMA_NODE:return "GroundedSchemaNode";
        case AtomType::NUMBER_NODE:         return "NumberNode";
        case AtomType::VARIABLE_NODE:       return "VariableNode";
        case AtomType::ANCHOR_NODE:         return "AnchorNode";
        case AtomType::TYPE_NODE:           return "TypeNode";
        case AtomType::DEFINED_SCHEMA_NODE: return "DefinedSchemaNode";
        case AtomType::LINK:                return "Link";
        case AtomType::ORDERED_LINK:        return "OrderedLink";
        case AtomType::UNORDERED_LINK:      return "UnorderedLink";
        case AtomType::LIST_LINK:           return "ListLink";
        case AtomType::SET_LINK:            return "SetLink";
        case AtomType::MEMBER_LINK:         return "MemberLink";
        case AtomType::INHERITANCE_LINK:    return "InheritanceLink";
        case AtomType::SIMILARITY_LINK:     return "SimilarityLink";
        case AtomType::EVALUATION_LINK:     return "EvaluationLink";
        case AtomType::EXECUTION_LINK:      return "ExecutionLink";
        case AtomType::EXECUTION_OUTPUT_LINK:return "ExecutionOutputLink";
        case AtomType::BIND_LINK:           return "BindLink";
        case AtomType::AND_LINK:            return "AndLink";
        case AtomType::OR_LINK:             return "OrLink";
        case AtomType::NOT_LINK:            return "NotLink";
        case AtomType::IMPLICATION_LINK:    return "ImplicationLink";
        case AtomType::EQUIVALENCE_LINK:    return "EquivalenceLink";
        case AtomType::CONTEXT_LINK:        return "ContextLink";
        case AtomType::STATE_LINK:          return "StateLink";
        case AtomType::DEFINE_LINK:         return "DefineLink";
        case AtomType::LAMBDA_LINK:         return "LambdaLink";
        case AtomType::PUT_LINK:            return "PutLink";
        case AtomType::GET_LINK:            return "GetLink";
        case AtomType::SATISFACTION_LINK:   return "SatisfactionLink";
        default:                            return "Unknown";
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// TruthValue — Simple probabilistic truth value
// ─────────────────────────────────────────────────────────────────────────────
struct TruthValue {
    float strength;
    float confidence;

    TruthValue() : strength(0.0f), confidence(0.0f) {}
    TruthValue(float s, float c) : strength(s), confidence(c) {}

    // Merge: weighted average by confidence
    TruthValue merge(const TruthValue& other) const {
        float total_c = confidence + other.confidence;
        if (total_c < 1e-9f) return TruthValue(0.5f, 0.0f);
        float s = (strength * confidence + other.strength * other.confidence) / total_c;
        float c = std::min(total_c, 1.0f);
        return TruthValue(s, c);
    }

    // PLN deduction: P(A→C) from P(A→B) and P(B→C)
    static TruthValue deduction(const TruthValue& ab, const TruthValue& bc) {
        float sAC = ab.strength * bc.strength;
        float cAC = ab.confidence * bc.confidence * 0.9f;
        return TruthValue(sAC, cAC);
    }

    // PLN induction
    static TruthValue induction(const TruthValue& ab, const TruthValue& cb) {
        float s = ab.strength * cb.strength;
        float c = ab.confidence * cb.confidence * 0.5f;
        return TruthValue(s, c);
    }

    // PLN abduction
    static TruthValue abduction(const TruthValue& ab, const TruthValue& ac) {
        float s = ab.strength * ac.strength;
        float c = ab.confidence * ac.confidence * 0.5f;
        return TruthValue(s, c);
    }

    // PLN revision
    static TruthValue revision(const TruthValue& a, const TruthValue& b) {
        float w1 = a.confidence / (1.0f - a.confidence + 1e-9f);
        float w2 = b.confidence / (1.0f - b.confidence + 1e-9f);
        float w = w1 + w2;
        float s = (w1 * a.strength + w2 * b.strength) / (w + 1e-9f);
        float c = w / (w + 1.0f);
        return TruthValue(s, c);
    }

    bool operator==(const TruthValue& o) const {
        return std::fabs(strength - o.strength) < 1e-6f &&
               std::fabs(confidence - o.confidence) < 1e-6f;
    }
    bool operator!=(const TruthValue& o) const { return !(*this == o); }

    std::string to_string() const {
        std::ostringstream os;
        os << "(stv " << strength << " " << confidence << ")";
        return os.str();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// AttentionValue — ECAN attention economics
// ─────────────────────────────────────────────────────────────────────────────
struct AttentionValue {
    int16_t sti;   // Short-term importance
    int16_t lti;   // Long-term importance
    int16_t vlti;  // Very long-term importance

    AttentionValue() : sti(0), lti(0), vlti(0) {}
    AttentionValue(int16_t s, int16_t l, int16_t v) : sti(s), lti(l), vlti(v) {}

    // Hebbian: increase STI by amount
    void stimulate(int16_t amount) {
        int32_t next = static_cast<int32_t>(sti) + amount;
        sti = static_cast<int16_t>(std::max<int32_t>(-32768, std::min<int32_t>(32767, next)));
    }

    // Decay: reduce STI toward zero
    void decay(float rate = 0.05f) {
        sti = static_cast<int16_t>(static_cast<float>(sti) * (1.0f - rate));
    }

    // Rent: tax on STI for ECAN economics
    void pay_rent(int16_t rent) {
        stimulate(-rent);
    }

    // Promote to LTI if STI is high enough
    void consolidate(int16_t threshold = 100) {
        if (sti > threshold) {
            int32_t next = static_cast<int32_t>(lti) + 1;
            lti = static_cast<int16_t>(std::min<int32_t>(32767, next));
        }
    }

    bool in_attentional_focus(int16_t af_boundary = 50) const {
        return sti >= af_boundary;
    }

    std::string to_string() const {
        std::ostringstream os;
        os << "(av " << sti << " " << lti << " " << vlti << ")";
        return os.str();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Atom — Base atom in the hypergraph
// ─────────────────────────────────────────────────────────────────────────────
struct Atom {
    Handle         handle;
    AtomType       type;
    std::string    name;          // For nodes
    std::vector<Handle> outgoing; // For links
    TruthValue     tv;
    AttentionValue av;
    UUID           uuid;

    Atom() : handle(UNDEFINED_HANDLE), type(AtomType::NOTYPE), uuid(0) {}

    bool is_node() const { return cog::is_node(type); }
    bool is_link() const { return cog::is_link(type); }

    std::string to_short_string() const {
        std::ostringstream os;
        os << "(" << atom_type_name(type);
        if (is_node()) os << " \"" << name << "\"";
        os << " " << tv.to_string() << ")";
        return os.str();
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// AtomSpace — In-memory hypergraph container
// ─────────────────────────────────────────────────────────────────────────────
class AtomSpace {
public:
    AtomSpace() : next_handle_(1), next_uuid_(1) {}

    // Add a node
    Handle add_node(AtomType type, const std::string& name,
                    const TruthValue& tv = TruthValue(1.0f, 0.0f)) {
        assert(is_node(type));
        // Dedup: return existing if same type+name
        auto key = make_node_key(type, name);
        auto it = node_index_.find(key);
        if (it != node_index_.end()) {
            atoms_[it->second].tv = TruthValue::revision(atoms_[it->second].tv, tv);
            return it->second;
        }
        Handle h = next_handle_++;
        Atom a;
        a.handle = h;
        a.type = type;
        a.name = name;
        a.tv = tv;
        a.uuid = next_uuid_++;
        atoms_[h] = a;
        node_index_[key] = h;
        type_index_[type].push_back(h);
        return h;
    }

    // Add a link
    Handle add_link(AtomType type, const std::vector<Handle>& outgoing,
                    const TruthValue& tv = TruthValue(1.0f, 0.0f)) {
        assert(is_link(type));
        // Dedup: return existing if same type+outgoing
        auto key = make_link_key(type, outgoing);
        auto it = link_index_.find(key);
        if (it != link_index_.end()) {
            atoms_[it->second].tv = TruthValue::revision(atoms_[it->second].tv, tv);
            return it->second;
        }
        Handle h = next_handle_++;
        Atom a;
        a.handle = h;
        a.type = type;
        a.outgoing = outgoing;
        a.tv = tv;
        a.uuid = next_uuid_++;
        atoms_[h] = a;
        link_index_[key] = h;
        type_index_[type].push_back(h);
        // Update incoming sets
        for (auto oh : outgoing) {
            incoming_[oh].push_back(h);
        }
        return h;
    }

    // Retrieve atom by handle
    const Atom* get_atom(Handle h) const {
        auto it = atoms_.find(h);
        return (it != atoms_.end()) ? &it->second : nullptr;
    }

    Atom* get_atom_mut(Handle h) {
        auto it = atoms_.find(h);
        return (it != atoms_.end()) ? &it->second : nullptr;
    }

    // Get all atoms of a given type
    std::vector<Handle> get_by_type(AtomType type) const {
        auto it = type_index_.find(type);
        if (it != type_index_.end()) return it->second;
        return {};
    }

    // Get incoming set (links containing this atom)
    std::vector<Handle> get_incoming(Handle h) const {
        auto it = incoming_.find(h);
        if (it != incoming_.end()) return it->second;
        return {};
    }

    // Remove an atom
    bool remove_atom(Handle h) {
        auto it = atoms_.find(h);
        if (it == atoms_.end()) return false;
        const Atom& a = it->second;
        // Remove from type index
        auto& tv = type_index_[a.type];
        tv.erase(std::remove(tv.begin(), tv.end(), h), tv.end());
        // Remove from node/link index
        if (a.is_node()) {
            node_index_.erase(make_node_key(a.type, a.name));
        } else {
            link_index_.erase(make_link_key(a.type, a.outgoing));
        }
        // Remove from incoming sets
        for (auto oh : a.outgoing) {
            auto& inc = incoming_[oh];
            inc.erase(std::remove(inc.begin(), inc.end(), h), inc.end());
        }
        atoms_.erase(it);
        return true;
    }

    // Size
    size_t size() const { return atoms_.size(); }

    // Iterate all atoms
    template<typename Fn>
    void foreach_atom(Fn fn) const {
        for (auto& kv : atoms_) fn(kv.second);
    }

    // Simple pattern match: find all links of given type with outgoing matching pattern
    // Pattern handles: UNDEFINED_HANDLE acts as wildcard
    std::vector<Handle> pattern_match(AtomType link_type,
                                       const std::vector<Handle>& pattern) const {
        std::vector<Handle> results;
        auto handles = get_by_type(link_type);
        for (auto h : handles) {
            const Atom* a = get_atom(h);
            if (!a || a->outgoing.size() != pattern.size()) continue;
            bool match = true;
            for (size_t i = 0; i < pattern.size(); ++i) {
                if (pattern[i] != UNDEFINED_HANDLE && pattern[i] != a->outgoing[i]) {
                    match = false;
                    break;
                }
            }
            if (match) results.push_back(h);
        }
        return results;
    }

    // Clear all atoms
    void clear() {
        atoms_.clear();
        node_index_.clear();
        link_index_.clear();
        type_index_.clear();
        incoming_.clear();
        next_handle_ = 1;
        next_uuid_ = 1;
    }

private:
    Handle next_handle_;
    UUID   next_uuid_;
    std::unordered_map<Handle, Atom> atoms_;
    std::unordered_map<std::string, Handle> node_index_;
    std::unordered_map<std::string, Handle> link_index_;
    struct AtomTypeHash {
        size_t operator()(AtomType t) const {
            return std::hash<uint16_t>()(static_cast<uint16_t>(t));
        }
    };
    std::unordered_map<AtomType, std::vector<Handle>, AtomTypeHash> type_index_;
    std::unordered_map<Handle, std::vector<Handle>> incoming_;

    static std::string make_node_key(AtomType t, const std::string& name) {
        return std::to_string(static_cast<uint16_t>(t)) + ":" + name;
    }

    static std::string make_link_key(AtomType t, const std::vector<Handle>& out) {
        std::string key = std::to_string(static_cast<uint16_t>(t)) + ":";
        for (auto h : out) { key += std::to_string(h) + ","; }
        return key;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Arena allocator — for kernel-safe, no-heap scenarios
// ─────────────────────────────────────────────────────────────────────────────
class Arena {
public:
    explicit Arena(size_t capacity)
        : buf_(new char[capacity]), capacity_(capacity), offset_(0) {}

    ~Arena() { delete[] buf_; }

    void* alloc(size_t size, size_t align = 8) {
        size_t aligned = (offset_ + align - 1) & ~(align - 1);
        if (aligned + size > capacity_) return nullptr;
        void* ptr = buf_ + aligned;
        offset_ = aligned + size;
        return ptr;
    }

    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* mem = alloc(sizeof(T), alignof(T));
        if (!mem) return nullptr;
        return new(mem) T(std::forward<Args>(args)...);
    }

    void reset() { offset_ = 0; }
    size_t used() const { return offset_; }
    size_t capacity() const { return capacity_; }

private:
    char*  buf_;
    size_t capacity_;
    size_t offset_;

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
};

// ─────────────────────────────────────────────────────────────────────────────
// Spinlock — lightweight thread synchronization
// ─────────────────────────────────────────────────────────────────────────────
class Spinlock {
public:
    Spinlock() : flag_(false) {}

    void lock() {
        bool expected = false;
        while (!flag_.compare_exchange_weak(expected, true,
               std::memory_order_acquire, std::memory_order_relaxed)) {
            expected = false;
        }
    }

    void unlock() {
        flag_.store(false, std::memory_order_release);
    }

    class Guard {
    public:
        explicit Guard(Spinlock& s) : s_(s) { s_.lock(); }
        ~Guard() { s_.unlock(); }
    private:
        Spinlock& s_;
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
    };

private:
    std::atomic<bool> flag_;
};

} // namespace cog

#endif // COG_CORE_HPP

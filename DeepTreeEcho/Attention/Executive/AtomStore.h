/*
// Vendored from cogpy/cog-zero standalone cog0 (Gap B/C: forward-chaining reasoning over AtomStore).
 * standalone/include/cog0/AtomStore.h
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Standalone in-memory knowledge graph (replaces AtomSpace for standalone build).
 * Atoms are typed, named nodes or links connecting other atoms.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace cog0 {

// -----------------------------------------------------------------------
// Atom types

enum class AtomType {
    CONCEPT,     // Named concept node
    PREDICATE,   // Named predicate node
    VARIABLE,    // Variable node (for pattern matching)
    LIST,        // Ordered list link
    EVALUATION,  // Evaluation link: (Predicate, ListLink(args…))
    INHERITANCE, // Inheritance link: A -> B
    SIMILARITY,  // Similarity link: A ~ B
    IMPLICATION, // Implication link: A => B
    EXECUTION,   // Execution link: (Schema, args, result)
    STATE,       // State link: current value of something
    CUSTOM       // Catch-all for user-defined types
};

std::string atomTypeName(AtomType t);

// -----------------------------------------------------------------------
// TruthValue — simple (strength, confidence) pair

struct TruthValue {
    double strength   = 1.0; // [0,1]
    double confidence = 1.0; // [0,1]

    TruthValue() = default;
    TruthValue(double s, double c) : strength(s), confidence(c) {}

    bool operator==(const TruthValue& o) const {
        constexpr double eps = 1e-9;
        return std::fabs(strength - o.strength) < eps &&
               std::fabs(confidence - o.confidence) < eps;
    }
    bool operator!=(const TruthValue& o) const { return !(*this == o); }
};

// -----------------------------------------------------------------------
// Atom

class AtomStore;

class Atom {
public:
    using Handle = std::shared_ptr<Atom>;
    using HandleVec = std::vector<Handle>;

    // Node constructor
    Atom(AtomType type, std::string name);
    // Link constructor
    Atom(AtomType type, HandleVec outgoing);

    AtomType       type()     const { return _type; }
    const std::string& name() const { return _name; }
    const HandleVec&   out()  const { return _out; }
    bool           isNode()   const { return _out.empty(); }
    bool           isLink()   const { return !_out.empty(); }

    TruthValue     tv()       const { return _tv; }
    void           setTV(TruthValue tv) { _tv = tv; }

    // Attention value (short-term importance, long-term importance)
    double sti() const { return _sti; }
    double lti() const { return _lti; }
    void   setSTI(double v) { _sti = v; }
    void   setLTI(double v) { _lti = v; }

    // Unique id assigned by AtomStore
    size_t id() const { return _id; }

    std::string toStr() const;

private:
    friend class AtomStore;
    size_t     _id   = 0;
    AtomType   _type;
    std::string _name;
    HandleVec  _out;
    TruthValue _tv;
    double     _sti = 0.0;
    double     _lti = 0.0;
};

using Handle    = Atom::Handle;
using HandleVec = Atom::HandleVec;

// -----------------------------------------------------------------------
// AtomStore — thread-safe in-memory graph

class AtomStore {
public:
    AtomStore();

    // Add or retrieve a concept/predicate/variable node by (type, name)
    Handle addNode(AtomType type, const std::string& name);
    // Add or retrieve a link by (type, outgoing set)
    Handle addLink(AtomType type, HandleVec outgoing);

    // Retrieve without creating
    [[nodiscard]] Handle getNode(AtomType type, const std::string& name) const;
    [[nodiscard]] Handle getLink(AtomType type, const HandleVec& outgoing) const;

    // All atoms of a given type — O(1) via type index
    [[nodiscard]] HandleVec getByType(AtomType type) const;

    // All atoms whose outgoing set contains the given atom
    [[nodiscard]] HandleVec getIncoming(const Handle& h) const;

    // Remove an atom (and clean up incoming index)
    void remove(const Handle& h);

    // Number of atoms in store
    [[nodiscard]] size_t size() const;

    // Clear all atoms
    void clear();

    // Persistence: serialize all atoms to a file; returns true on success
    [[nodiscard]] bool saveToFile(const std::string& path) const;

    // Persistence: clear store and restore atoms from a file; returns true on success
    [[nodiscard]] bool loadFromFile(const std::string& path);

private:
    mutable std::mutex _mutex;
    size_t _nextId = 1;

    std::unordered_map<size_t, Handle> _byId;
    // Nodes: key = type*1000000 + hash(name)  (collision-safe via map)
    std::unordered_map<std::string, Handle> _nodeIndex;   // key = type:name
    std::unordered_map<std::string, Handle> _linkIndex;   // key = type:id0:id1:...
    std::unordered_map<size_t, std::vector<size_t>> _incoming; // atom id -> ids of links containing it

    // Type index — O(1) lookup of all atoms of a given type
    std::unordered_map<int, std::vector<Handle>> _typeIndex;

    std::string nodeKey(AtomType t, const std::string& name) const;
    std::string linkKey(AtomType t, const HandleVec& out) const;
    void registerIncoming(const Handle& link);
    void unregisterIncoming(const Handle& link);
};

} // namespace cog0

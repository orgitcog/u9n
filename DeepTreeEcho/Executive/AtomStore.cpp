/*
// Vendored from cogpy/cog-zero standalone cog0 (Gap B/C: forward-chaining reasoning over AtomStore).
 * standalone/src/AtomStore.cpp
 *
 * Copyright (C) 2024 OpenCog Foundation
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <sstream>
#include <stdexcept>

#include "AtomStore.h"
#include "Cog0Logger.h"

namespace cog0 {

// -----------------------------------------------------------------------
// Helpers

std::string atomTypeName(AtomType t) {
    switch (t) {
        case AtomType::CONCEPT:     return "Concept";
        case AtomType::PREDICATE:   return "Predicate";
        case AtomType::VARIABLE:    return "Variable";
        case AtomType::LIST:        return "List";
        case AtomType::EVALUATION:  return "Evaluation";
        case AtomType::INHERITANCE: return "Inheritance";
        case AtomType::SIMILARITY:  return "Similarity";
        case AtomType::IMPLICATION: return "Implication";
        case AtomType::EXECUTION:   return "Execution";
        case AtomType::STATE:       return "State";
        case AtomType::CUSTOM:      return "Custom";
    }
    return "Unknown";
}

// -----------------------------------------------------------------------
// Atom

Atom::Atom(AtomType type, std::string name)
    : _type(type), _name(std::move(name)) {}

Atom::Atom(AtomType type, HandleVec outgoing)
    : _type(type), _out(std::move(outgoing)) {}

std::string Atom::toStr() const {
    std::ostringstream oss;
    oss << atomTypeName(_type) << "(";
    if (isNode()) {
        oss << '"' << _name << '"';
    } else {
        for (size_t i = 0; i < _out.size(); ++i) {
            if (i) oss << ", ";
            oss << (_out[i] ? _out[i]->toStr() : "NULL");
        }
    }
    oss << ")[" << _tv.strength << "," << _tv.confidence << "]";
    return oss.str();
}

// -----------------------------------------------------------------------
// AtomStore

AtomStore::AtomStore() = default;

std::string AtomStore::nodeKey(AtomType t, const std::string& name) const {
    return std::to_string(static_cast<int>(t)) + ":" + name;
}

std::string AtomStore::linkKey(AtomType t, const HandleVec& out) const {
    std::ostringstream oss;
    oss << static_cast<int>(t);
    for (const auto& h : out) {
        oss << ":" << (h ? h->id() : 0);
    }
    return oss.str();
}

Handle AtomStore::addNode(AtomType type, const std::string& name) {
    std::lock_guard<std::mutex> lk(_mutex);
    std::string key = nodeKey(type, name);
    auto it = _nodeIndex.find(key);
    if (it != _nodeIndex.end()) return it->second;

    auto atom  = std::make_shared<Atom>(type, name);
    atom->_id  = _nextId++;
    _byId[atom->_id]  = atom;
    _nodeIndex[key]   = atom;
    _typeIndex[static_cast<int>(type)].push_back(atom);
    return atom;
}

Handle AtomStore::addLink(AtomType type, HandleVec outgoing) {
    std::lock_guard<std::mutex> lk(_mutex);
    std::string key = linkKey(type, outgoing);
    auto it = _linkIndex.find(key);
    if (it != _linkIndex.end()) return it->second;

    auto atom  = std::make_shared<Atom>(type, std::move(outgoing));
    atom->_id  = _nextId++;
    _byId[atom->_id]  = atom;
    _linkIndex[key]   = atom;
    _typeIndex[static_cast<int>(type)].push_back(atom);
    registerIncoming(atom);
    return atom;
}

Handle AtomStore::getNode(AtomType type, const std::string& name) const {
    std::lock_guard<std::mutex> lk(_mutex);
    auto it = _nodeIndex.find(nodeKey(type, name));
    return (it != _nodeIndex.end()) ? it->second : nullptr;
}

Handle AtomStore::getLink(AtomType type, const HandleVec& out) const {
    std::lock_guard<std::mutex> lk(_mutex);
    auto it = _linkIndex.find(linkKey(type, out));
    return (it != _linkIndex.end()) ? it->second : nullptr;
}

HandleVec AtomStore::getByType(AtomType type) const {
    std::lock_guard<std::mutex> lk(_mutex);
    auto it = _typeIndex.find(static_cast<int>(type));
    if (it == _typeIndex.end()) return {};
    return it->second;
}

HandleVec AtomStore::getIncoming(const Handle& h) const {
    if (!h) return {};
    std::lock_guard<std::mutex> lk(_mutex);
    HandleVec result;
    auto it = _incoming.find(h->id());
    if (it != _incoming.end()) {
        for (size_t lid : it->second) {
            auto jt = _byId.find(lid);
            if (jt != _byId.end()) result.push_back(jt->second);
        }
    }
    return result;
}

void AtomStore::remove(const Handle& h) {
    if (!h) return;
    std::lock_guard<std::mutex> lk(_mutex);
    if (h->isLink()) unregisterIncoming(h);
    _byId.erase(h->id());
    if (h->isNode())
        _nodeIndex.erase(nodeKey(h->type(), h->name()));
    else
        _linkIndex.erase(linkKey(h->type(), h->out()));

    // Remove from type index
    auto ti = _typeIndex.find(static_cast<int>(h->type()));
    if (ti != _typeIndex.end()) {
        auto& vec = ti->second;
        vec.erase(std::remove(vec.begin(), vec.end(), h), vec.end());
        if (vec.empty()) _typeIndex.erase(ti);
    }
}

size_t AtomStore::size() const {
    std::lock_guard<std::mutex> lk(_mutex);
    return _byId.size();
}

void AtomStore::clear() {
    std::lock_guard<std::mutex> lk(_mutex);
    _byId.clear();
    _nodeIndex.clear();
    _linkIndex.clear();
    _incoming.clear();
    _typeIndex.clear();
    _nextId = 1;
}

// -----------------------------------------------------------------------
// Persistence helpers (private static)

namespace {

// Escape a string for the snapshot format (backslash-escapes " and \)
std::string quoteAtomName(const std::string& s) {
    std::string r;
    r.reserve(s.size() + 2);
    r += '"';
    for (char c : s) {
        if (c == '"')       { r += '\\'; r += '"'; }
        else if (c == '\\') { r += '\\'; r += '\\'; }
        else if (c == '\n') { r += '\\'; r += 'n'; }
        else                 { r += c; }
    }
    r += '"';
    return r;
}

// Read a quoted string starting after the opening '"' is consumed.
// Caller must position the stream just before the opening '"'.
// Returns empty string if the opening quote is not found or the stream fails.
std::string readQuotedAtomName(std::istream& is) {
    char c;
    // Skip whitespace and find the opening quote
    while (is.get(c) && c != '"') {}
    if (!is) return ""; // opening quote not found
    std::string result;
    while (is.get(c) && c != '"') {
        if (c == '\\') {
            char esc;
            if (is.get(esc)) {
                switch (esc) {
                    case '"':  result += '"';  break;
                    case '\\': result += '\\'; break;
                    case 'n':  result += '\n'; break;
                    default:   result += esc;  break;
                }
            }
        } else {
            result += c;
        }
    }
    return result;
}

} // anonymous namespace

bool AtomStore::saveToFile(const std::string& path) const {
    std::lock_guard<std::mutex> lk(_mutex);

    std::ofstream fs(path);
    if (!fs.is_open()) return false;

    // Collect all atoms sorted by id (topological order: lower ids come first)
    std::vector<Handle> sorted;
    sorted.reserve(_byId.size());
    for (const auto& kv : _byId) sorted.push_back(kv.second);
    std::sort(sorted.begin(), sorted.end(),
              [](const Handle& a, const Handle& b) { return a->id() < b->id(); });

    fs << "# cog0-atomstore-snapshot v1\n";
    fs << "next_id " << _nextId << "\n";

    for (const auto& atom : sorted) {
        if (atom->isNode()) {
            fs << "node "
               << atom->id() << " "
               << static_cast<int>(atom->type()) << " "
               << quoteAtomName(atom->name()) << " "
               << atom->tv().strength << " "
               << atom->tv().confidence << " "
               << atom->sti() << " "
               << atom->lti() << "\n";
        } else {
            fs << "link "
               << atom->id() << " "
               << static_cast<int>(atom->type()) << " "
               << atom->out().size();
            for (const auto& out : atom->out())
                fs << " " << (out ? out->id() : 0);
            fs << " "
               << atom->tv().strength << " "
               << atom->tv().confidence << " "
               << atom->sti() << " "
               << atom->lti() << "\n";
        }
    }
    return fs.good();
}

bool AtomStore::loadFromFile(const std::string& path) {
    std::ifstream fs(path);
    if (!fs.is_open()) return false;

    std::lock_guard<std::mutex> lk(_mutex);
    _byId.clear();
    _nodeIndex.clear();
    _linkIndex.clear();
    _incoming.clear();
    _typeIndex.clear();
    _nextId = 1;

    // Maps saved id -> newly created handle (needed to wire links)
    std::unordered_map<size_t, Handle> idMap;

    std::string line;
    while (std::getline(fs, line)) {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream iss(line);
        std::string kind;
        iss >> kind;

        if (kind == "next_id") {
            iss >> _nextId;

        } else if (kind == "node") {
            size_t id; int typeInt;
            iss >> id >> typeInt;
            std::string name = readQuotedAtomName(iss);
            double tvs = 1.0, tvc = 1.0, sti = 0.0, lti = 0.0;
            iss >> tvs >> tvc >> sti >> lti;

            auto atom = std::make_shared<Atom>(static_cast<AtomType>(typeInt), name);
            atom->_id = id;
            atom->setTV({tvs, tvc});
            atom->setSTI(sti);
            atom->setLTI(lti);

            _byId[id] = atom;
            _nodeIndex[nodeKey(static_cast<AtomType>(typeInt), name)] = atom;
            _typeIndex[typeInt].push_back(atom);
            idMap[id] = atom;

        } else if (kind == "link") {
            size_t id; int typeInt; size_t nOut;
            iss >> id >> typeInt >> nOut;

            HandleVec outgoing;
            outgoing.reserve(nOut);
            for (size_t i = 0; i < nOut; ++i) {
                size_t outId;
                iss >> outId;
                auto it = idMap.find(outId);
                if (it == idMap.end()) {
                    // Snapshot references an id that hasn't been created yet —
                    // this indicates a malformed or out-of-order snapshot file.
                    logger().error("[AtomStore] loadFromFile: missing atom id="
                                   + std::to_string(outId));
                    return false;
                }
                outgoing.push_back(it->second);
            }
            double tvs = 1.0, tvc = 1.0, sti = 0.0, lti = 0.0;
            iss >> tvs >> tvc >> sti >> lti;

            auto atom = std::make_shared<Atom>(static_cast<AtomType>(typeInt), outgoing);
            atom->_id = id;
            atom->setTV({tvs, tvc});
            atom->setSTI(sti);
            atom->setLTI(lti);

            _byId[id] = atom;
            _linkIndex[linkKey(static_cast<AtomType>(typeInt), outgoing)] = atom;
            _typeIndex[typeInt].push_back(atom);
            registerIncoming(atom); // _mutex already held by this function
            idMap[id] = atom;
        }
    }
    return true;
}

void AtomStore::registerIncoming(const Handle& link) {
    // _mutex already held by caller
    for (const auto& out : link->out()) {
        if (out) _incoming[out->id()].push_back(link->id());
    }
}

void AtomStore::unregisterIncoming(const Handle& link) {
    // _mutex already held by caller
    for (const auto& out : link->out()) {
        if (!out) continue;
        auto it = _incoming.find(out->id());
        if (it == _incoming.end()) continue;
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), link->id()), vec.end());
        if (vec.empty()) _incoming.erase(it);
    }
}

} // namespace cog0

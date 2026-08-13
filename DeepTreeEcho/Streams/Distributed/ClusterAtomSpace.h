#pragma once

/*
 * ClusterAtomSpace.h — Distributed ATenSpace cluster for Deep Tree Echo
 *
 * Synthesizes three distribution patterns from Dan's ecosystem:
 *
 *   1. o9nn/togai (9mly) "Cognitive Mesh API": node/agent registry with
 *      capability descriptors — here, CogNode registration with roles.
 *   2. o9nn/flarecog "Worker Swarms": dispatch-worker hash sharding — here,
 *      shardOf() consistently routes each atom to an owner node.
 *   3. o9nn/infernos OpenCog kernel "distributed_coherence": delta-sync with
 *      TruthValue count-weighted merge so replicas converge.
 *
 * Each CogNode owns a CognitiveKernelState (an InfernoNamespace can be bound
 * over it) and replicates atoms it learns to the cluster via AtomDelta
 * gossip. The transport is pluggable — the default in-process transport is
 * a direct function call, and the AgentMessageBus (Executive/) topic
 * transport can serve inter-process clusters.
 */

#include "ATenSpaceLite.h"
#include "InfernoNamespace.h"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace dte {
namespace aspace {

/** Node role in the mesh, following togai's Cognitive Mesh agent kinds. */
enum class NodeRole {
    Coordinator,   ///< dispatch/registry owner (flarecog platform-api)
    Inference,     ///< heavy compute node (CogCity pattern)
    Sensor,        ///< embodiment ingress (togai sensor binding)
    Effector,      ///< embodiment egress (togai effector binding)
    Storage        ///< persistence node
};

/** A serializable atom mutation propagated between nodes. */
struct AtomDelta {
    std::string node_id;       ///< origin node
    uint64_t sequence = 0;     ///< origin-local monotonic sequence
    Atom::Type type = Atom::Type::CONCEPT_NODE;
    std::string name;          ///< node name (nodes only in v1 protocol)
    TruthValue tv;
    float attention = 0.0f;
    Embedding embedding;       ///< optional embedding payload
};

/**
 * CogNode — one member of the distributed ATenSpace.
 * Owns a kernel state; produces deltas for local learning; applies remote
 * deltas with count-weighted TruthValue merge (never regresses evidence).
 */
class CogNode {
public:
    CogNode(std::string id, NodeRole role)
        : id_(std::move(id)), role_(role), ns_(kernel_) {}

    const std::string& id() const { return id_; }
    NodeRole role() const { return role_; }
    CognitiveKernelState& kernel() { return kernel_; }
    InfernoNamespace& fs() { return ns_; }

    /** Learn an atom locally and emit the delta for cluster gossip. */
    AtomDelta learn(const std::string& name, const TruthValue& tv,
                    Embedding embedding = {}) {
        auto atom = embedding.empty()
                        ? kernel_.space.addNode(Atom::Type::CONCEPT_NODE, name)
                        : kernel_.space.addNode(Atom::Type::CONCEPT_NODE, name,
                                                embedding);
        // Merge with any existing local knowledge.
        atom->setTruthValue(TruthValue::merge(atom->getTruthValue(), tv));

        AtomDelta d;
        d.node_id = id_;
        d.sequence = ++sequence_;
        d.type = Atom::Type::CONCEPT_NODE;
        d.name = name;
        d.tv = atom->getTruthValue();
        d.attention = atom->getAttention();
        d.embedding = std::move(embedding);
        return d;
    }

    /** Apply a delta received from another node (idempotent, converging). */
    void apply(const AtomDelta& d) {
        if (d.node_id == id_) return;  // own echo
        auto& seen = applied_seq_[d.node_id];
        if (d.sequence <= seen) return;  // stale/duplicate
        seen = d.sequence;

        auto existing = kernel_.space.getNode(d.type, d.name);
        if (existing) {
            existing->setTruthValue(
                TruthValue::merge(existing->getTruthValue(), d.tv));
            if (!d.embedding.empty()) {
                if (auto n = std::dynamic_pointer_cast<Node>(existing))
                    n->setEmbedding(d.embedding);
            }
        } else {
            auto atom = d.embedding.empty()
                            ? kernel_.space.addNode(d.type, d.name)
                            : kernel_.space.addNode(d.type, d.name, d.embedding);
            atom->setTruthValue(d.tv);
            atom->setAttention(d.attention);
        }
    }

private:
    std::string id_;
    NodeRole role_;
    CognitiveKernelState kernel_;
    InfernoNamespace ns_;
    uint64_t sequence_ = 0;
    std::map<std::string, uint64_t> applied_seq_;  ///< per-origin high-water mark
};

/**
 * ClusterAtomSpace — the mesh coordinator.
 *
 * - registerNode(): togai-style mesh registration
 * - shardOf(): flarecog-style consistent hash dispatch (owner node per atom)
 * - publish(): gossip a delta to all nodes (in-process transport by default,
 *   or an external transport hook for AgentMessageBus / sockets)
 * - sync(): full anti-entropy pass — every node's atoms merged everywhere,
 *   the `echo sync > /distributed` verb from the Inferno kernel
 * - globalQuery(): scatter-gather semantic query across all shards
 */
class ClusterAtomSpace {
public:
    using Transport = std::function<void(const AtomDelta&)>;

    /** Register a node in the mesh; returns the created node. */
    std::shared_ptr<CogNode> registerNode(const std::string& id, NodeRole role) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto node = std::make_shared<CogNode>(id, role);
        nodes_[id] = node;
        // Wire the namespace sync verb to a cluster anti-entropy pass.
        node->fs().setSyncHook([this] { this->sync(); });
        for (auto& [nid, n] : nodes_)
            n->kernel().distributed_nodes = static_cast<int>(nodes_.size());
        return node;
    }

    bool unregisterNode(const std::string& id) {
        std::lock_guard<std::mutex> lock(mutex_);
        const bool erased = nodes_.erase(id) > 0;
        for (auto& [nid, n] : nodes_)
            n->kernel().distributed_nodes = static_cast<int>(nodes_.size());
        return erased;
    }

    size_t nodeCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return nodes_.size();
    }

    std::shared_ptr<CogNode> node(const std::string& id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodes_.find(id);
        return it != nodes_.end() ? it->second : nullptr;
    }

    /** Optional external transport (e.g. AgentMessageBus publish). */
    void setTransport(Transport t) { transport_ = std::move(t); }

    /**
     * Consistent-hash shard owner for an atom name (flarecog dispatch-worker
     * pattern). Deterministic across the cluster for a fixed node set.
     */
    std::string shardOf(const std::string& atom_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (nodes_.empty()) return {};
        // Rendezvous (highest-random-weight) hashing: stable under node churn.
        std::string best;
        uint64_t best_w = 0;
        for (const auto& [nid, n] : nodes_) {
            const uint64_t w = mix(std::hash<std::string>{}(atom_name),
                                   std::hash<std::string>{}(nid));
            if (best.empty() || w > best_w) {
                best_w = w;
                best = nid;
            }
        }
        return best;
    }

    /**
     * Write an atom into the cluster: applied at its shard owner, then
     * gossiped to every node so replicas converge.
     */
    void publish(const std::string& origin_id, const AtomDelta& delta) {
        std::vector<std::shared_ptr<CogNode>> targets;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [nid, n] : nodes_)
                if (nid != origin_id) targets.push_back(n);
        }
        for (auto& n : targets) n->apply(delta);
        if (transport_) transport_(delta);
    }

    /** Convenience: learn at a node and gossip in one step. */
    void learnAndPublish(const std::string& node_id, const std::string& name,
                         const TruthValue& tv, Embedding embedding = {}) {
        std::shared_ptr<CogNode> origin;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = nodes_.find(node_id);
            if (it == nodes_.end()) return;
            origin = it->second;
        }
        const AtomDelta d = origin->learn(name, tv, std::move(embedding));
        publish(node_id, d);
    }

    /**
     * Anti-entropy pass: every concept atom on every node is re-broadcast so
     * all replicas hold the merged TruthValue (distributed_coherence goal).
     */
    void sync() {
        std::vector<std::shared_ptr<CogNode>> all;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [nid, n] : nodes_) all.push_back(n);
        }
        for (auto& src : all) {
            auto atoms =
                src->kernel().space.getAtomsByType(Atom::Type::CONCEPT_NODE);
            for (const auto& atom : atoms) {
                auto n = std::dynamic_pointer_cast<Node>(atom);
                if (!n) continue;
                AtomDelta d;
                d.node_id = src->id();
                d.sequence = 0;  // anti-entropy bypasses sequence gating
                d.type = Atom::Type::CONCEPT_NODE;
                d.name = n->getName();
                d.tv = n->getTruthValue();
                d.attention = n->getAttention();
                if (n->hasEmbedding()) d.embedding = n->getEmbedding();
                for (auto& dst : all) {
                    if (dst->id() == src->id()) continue;
                    applyAntiEntropy(*dst, d);
                }
            }
        }
        ++sync_rounds_;
    }

    uint64_t syncRounds() const { return sync_rounds_; }

    /** Scatter-gather semantic query across every node's shard. */
    std::vector<std::pair<std::string, float>> globalQuery(
            const Embedding& query, size_t k = 10, float threshold = 0.0f) const {
        std::vector<std::shared_ptr<CogNode>> all;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [nid, n] : nodes_) all.push_back(n);
        }
        // name -> best similarity (dedupe replicas)
        std::map<std::string, float> best;
        for (const auto& n : all) {
            auto local = n->kernel().space.querySimilar(query, k, threshold);
            for (const auto& [handle, sim] : local) {
                auto node = std::dynamic_pointer_cast<Node>(handle);
                if (!node) continue;
                auto it = best.find(node->getName());
                if (it == best.end() || sim > it->second)
                    best[node->getName()] = sim;
            }
        }
        std::vector<std::pair<std::string, float>> out(best.begin(), best.end());
        std::sort(out.begin(), out.end(),
                  [](const auto& a, const auto& b) { return a.second > b.second; });
        if (out.size() > k) out.resize(k);
        return out;
    }

    /** Total distinct atoms across the cluster (post-sync this is uniform). */
    size_t totalAtoms() const {
        std::lock_guard<std::mutex> lock(mutex_);
        size_t total = 0;
        for (const auto& [nid, n] : nodes_) total += n->kernel().space.size();
        return total;
    }

private:
    static void applyAntiEntropy(CogNode& dst, const AtomDelta& d) {
        auto existing = dst.kernel().space.getNode(d.type, d.name);
        if (existing) {
            // Converge to the higher-evidence value without double counting:
            // take the TV with greater count (post-merge values dominate).
            const auto cur = existing->getTruthValue();
            if (d.tv.count > cur.count) existing->setTruthValue(d.tv);
            if (!d.embedding.empty()) {
                if (auto n = std::dynamic_pointer_cast<Node>(existing))
                    if (!n->hasEmbedding()) n->setEmbedding(d.embedding);
            }
        } else {
            auto atom = d.embedding.empty()
                            ? dst.kernel().space.addNode(d.type, d.name)
                            : dst.kernel().space.addNode(d.type, d.name,
                                                         d.embedding);
            atom->setTruthValue(d.tv);
            atom->setAttention(d.attention);
        }
    }

    static uint64_t mix(uint64_t a, uint64_t b) {
        // splitmix64-style avalanche over the pair
        uint64_t x = a ^ (b + 0x9e3779b97f4a7c15ULL + (a << 6) + (a >> 2));
        x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27; x *= 0x94d049bb133111ebULL;
        x ^= x >> 31;
        return x;
    }

    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<CogNode>> nodes_;
    Transport transport_;
    uint64_t sync_rounds_ = 0;
};

} // namespace aspace
} // namespace dte

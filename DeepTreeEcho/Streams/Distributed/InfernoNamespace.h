#pragma once

/*
 * InfernoNamespace.h — /dev/opencog synthetic file namespace for Deep Tree Echo
 *
 * Ported from o9nn/infernos os/port/devopencog.c (Inferno kernel device) and
 * os/port/opencog.c (OpenCogKernel global state). The Inferno idea: expose
 * the cognitive kernel as a file tree so any client — local or across a
 * Styx/9P mount — interacts with cognition through read/write on paths.
 *
 * Files (read):
 *   /stats        — kernel statistics
 *   /atomspace    — local atomspace summary
 *   /goals        — active goal listing
 *   /reason       — reasoning cycle counters
 *   /think        — think time / cognitive load / attention
 *   /attention    — system + local attention levels
 *   /patterns     — pattern matcher status
 *   /distributed  — cluster node status
 *
 * Commands (write):
 *   /atomspace    "create <name>" | "clear"
 *   /goals        "add <description>" | "clear"
 *   /reason       "cycle" | "threshold <f>"
 *   /think        "focus" | "relax"
 *   /attention    "<level 0..1>"
 *   /distributed  "sync"
 *
 * The namespace is transport-agnostic: bind it to AgentMessageBus (cogdiod
 * 9P patterns, already in Executive/) to serve it across cluster nodes.
 */

#include "ATenSpaceLite.h"

#include <chrono>
#include <cstdio>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace dte {
namespace aspace {

/** A cognitive goal, following the infernos kernel Goal struct. */
struct CogGoal {
    uint64_t id = 0;
    std::string description;
    float urgency = 0.7f;
    float importance = 0.8f;
    TruthValue satisfaction{0.0f, 0.0f, 0.0f};
    int64_t created_ns = 0;
};

/**
 * CognitiveKernelState — port of the infernos OpenCogKernel global state,
 * scoped per-node instead of per-machine.
 */
struct CognitiveKernelState {
    AtomSpace space;                       ///< this node's atomspace
    std::vector<CogGoal> goals;            ///< active goals
    uint64_t reasoning_cycles = 0;
    float confidence_threshold = 0.1f;
    float attention_level = 0.5f;
    float motivation = 0.5f;
    float system_attention = 1.0f;
    int64_t think_time_ns = 0;
    int cognitive_load = 0;
    int distributed_nodes = 1;
    uint64_t next_goal_id = 1;

    CognitiveKernelState() {
        // System goals from opencoginit() in o9nn/infernos os/port/opencog.c
        addGoal("system_survival", 1.0f, 1.0f);
        addGoal("resource_optimization", 0.8f, 0.9f);
        addGoal("distributed_coherence", 0.9f, 0.8f);
        addGoal("cognitive_efficiency", 0.7f, 0.8f);
    }

    CogGoal& addGoal(const std::string& desc, float urgency, float importance) {
        CogGoal g;
        g.id = next_goal_id++;
        g.description = desc;
        g.urgency = urgency;
        g.importance = importance;
        g.created_ns = nowNs();
        goals.push_back(g);
        return goals.back();
    }

    /** Port of reasoning_cycle + inference_step (satisfaction relaxation). */
    void reasoningCycle(int steps = 10) {
        const auto t0 = nowNs();
        for (int i = 0; i < steps; ++i) {
            for (auto& goal : goals) {
                if (goal.satisfaction.strength < 0.9f) {
                    goal.satisfaction.strength += 0.01f;
                    goal.satisfaction.confidence += 0.005f;
                }
            }
        }
        ++reasoning_cycles;
        think_time_ns += nowNs() - t0;
    }

    static int64_t nowNs() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }
};

/**
 * InfernoNamespace — the synthetic file tree over a CognitiveKernelState.
 *
 * readFile(path)          -> file contents (as devopencog's read switch)
 * writeFile(path, data)   -> command dispatch (as devopencog's write switch)
 * list()                  -> directory listing
 *
 * An optional syncHook is invoked by `echo sync > /distributed`, mirroring
 * the kernel's cognitive_schedule() trigger; the cluster layer installs it.
 */
class InfernoNamespace {
public:
    using SyncHook = std::function<void()>;

    explicit InfernoNamespace(CognitiveKernelState& kernel) : k_(kernel) {}

    void setSyncHook(SyncHook hook) { sync_hook_ = std::move(hook); }

    std::vector<std::string> list() const {
        return {"stats",   "atomspace", "goals",    "reason",
                "think",   "attention", "patterns", "distributed"};
    }

    bool exists(const std::string& path) const {
        const std::string p = normalize(path);
        for (const auto& f : list())
            if (f == p) return true;
        return false;
    }

    /** Read a synthetic file. Unknown path returns empty string. */
    std::string readFile(const std::string& path) const {
        const std::string p = normalize(path);
        std::ostringstream os;
        if (p == "stats") {
            os << "OpenCog Kernel-based AGI (DTE node)\n"
               << "==================================\n"
               << "total_atoms=" << k_.space.size() << "\n"
               << "reasoning_cycles=" << k_.reasoning_cycles << "\n"
               << "system_attention=" << fmt(k_.system_attention) << "\n"
               << "distributed_nodes=" << k_.distributed_nodes << "\n"
               << "active_goals=" << k_.goals.size() << "\n";
        } else if (p == "atomspace") {
            os << "Local AtomSpace:\n"
               << "  Atoms: " << k_.space.size() << "\n";
        } else if (p == "goals") {
            os << "Active Goals:\n";
            for (const auto& g : k_.goals) {
                os << "  Goal " << g.id << ": " << g.description
                   << " (urgency=" << fmt(g.urgency)
                   << ", importance=" << fmt(g.importance)
                   << ", satisfaction=" << fmt(g.satisfaction.strength) << ")\n";
            }
        } else if (p == "reason") {
            os << "reasoning_cycles=" << k_.reasoning_cycles << "\n"
               << "confidence_threshold=" << fmt(k_.confidence_threshold) << "\n";
        } else if (p == "think") {
            os << "think_time=" << k_.think_time_ns << "\n"
               << "cognitive_load=" << k_.cognitive_load << "\n"
               << "attention=" << fmt(k_.attention_level) << "\n";
        } else if (p == "attention") {
            os << "system_attention=" << fmt(k_.system_attention) << "\n"
               << "process_attention=" << fmt(k_.attention_level) << "\n";
        } else if (p == "patterns") {
            os << "Pattern Matcher Status:\n"
               << "  Similarity function: active\n"
               << "  Unification: active\n";
        } else if (p == "distributed") {
            os << "distributed_nodes=" << k_.distributed_nodes << "\n"
               << "network_coherence=active\n"
               << "distributed_reasoning=active\n";
        }
        return os.str();
    }

    /**
     * Write a command to a synthetic file. Returns true if the command was
     * recognized and applied (mirrors devopencog's opencogwrite dispatch).
     */
    bool writeFile(const std::string& path, const std::string& data) {
        const std::string p = normalize(path);
        std::string cmd, arg;
        splitCommand(data, cmd, arg);

        if (p == "atomspace") {
            if (cmd == "create" && !arg.empty()) {
                TruthValue tv{0.8f, 0.5f, 1.0f};
                auto atom = k_.space.addNode(Atom::Type::CONCEPT_NODE, arg);
                atom->setTruthValue(tv);
                return true;
            }
            if (cmd == "clear") { k_.space.clear(); return true; }
        } else if (p == "goals") {
            if (cmd == "add" && !arg.empty()) {
                k_.addGoal(arg, 0.7f, 0.8f);
                return true;
            }
            if (cmd == "clear") { k_.goals.clear(); return true; }
        } else if (p == "reason") {
            if (cmd == "cycle") { k_.reasoningCycle(); return true; }
            if (cmd == "threshold" && !arg.empty()) {
                k_.confidence_threshold = std::strtof(arg.c_str(), nullptr);
                return true;
            }
        } else if (p == "think") {
            if (cmd == "focus") {
                k_.attention_level = 1.0f;
                k_.motivation = std::min(1.0f, k_.motivation + 0.1f);
                return true;
            }
            if (cmd == "relax") {
                k_.attention_level = 0.5f;
                k_.motivation = std::max(0.0f, k_.motivation - 0.1f);
                return true;
            }
        } else if (p == "attention") {
            const float level = std::strtof(cmd.c_str(), nullptr);
            if (level >= 0.0f && level <= 1.0f && !cmd.empty()) {
                k_.attention_level = level;
                return true;
            }
        } else if (p == "distributed") {
            if (cmd == "sync") {
                if (sync_hook_) sync_hook_();
                return true;
            }
        }
        return false;
    }

private:
    static std::string normalize(const std::string& path) {
        std::string p = path;
        while (!p.empty() && p.front() == '/') p.erase(p.begin());
        return p;
    }

    static void splitCommand(const std::string& data, std::string& cmd,
                             std::string& arg) {
        std::string trimmed = data;
        while (!trimmed.empty() &&
               (trimmed.back() == '\n' || trimmed.back() == '\r'))
            trimmed.pop_back();
        const auto sp = trimmed.find(' ');
        if (sp == std::string::npos) {
            cmd = trimmed;
            arg.clear();
        } else {
            cmd = trimmed.substr(0, sp);
            arg = trimmed.substr(sp + 1);
        }
    }

    static std::string fmt(float v) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(v));
        return buf;
    }

    CognitiveKernelState& k_;
    SyncHook sync_hook_;
};

} // namespace aspace
} // namespace dte

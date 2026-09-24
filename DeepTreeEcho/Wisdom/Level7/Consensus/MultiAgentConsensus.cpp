// ═══════════════════════════════════════════════════════════════════════════
// MultiAgentConsensus.cpp
// Compilation unit for FMultiAgentConsensus — Level 7 distributed collective.
//
// FMultiAgentConsensus is a fully header-only class. All method bodies are
// defined inline in MultiAgentConsensus.h to enable:
//   - Peer discovery and heartbeat management inlined for low latency
//   - Cognitive Raft leader election logic kept at call sites
//   - Knowledge proposal voting inlined with commitment side effects
//   - Trust update computations inlined alongside vote processing
//
// The Multi-Agent Consensus system implements a Raft-inspired protocol for
// distributed cognitive collectives: multiple DTE instances negotiate shared
// understanding, elect the wisest leader, validate knowledge proposals via
// majority vote, and maintain a committed wisdom manifold that transcends
// any individual instance. Trust is earned through consistent, coherent
// contributions; knowledge fused across the collective is richer than any
// single perspective.
//
// Feature:  F1.7.0 — Level 7 Distributed Cognitive Collective
// Phase:    1.7 — Transcendent Autonomy (Level 7)
// Epic:     E6 — Transcendent Distributed Intelligence
// ═══════════════════════════════════════════════════════════════════════════

#include "MultiAgentConsensus.h"

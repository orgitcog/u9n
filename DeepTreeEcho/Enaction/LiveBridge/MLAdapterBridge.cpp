// ═══════════════════════════════════════════════════════════════════════════
// MLAdapterBridge.cpp
// Compilation unit for FMLAdapterBridge — UE MLAdapter protocol bridge.
//
// FMLAdapterBridge is a fully header-only class. All method bodies are
// defined inline in MLAdapterBridge.h to enable:
//   - Observation packet serialisation inlined for zero-copy path
//   - Action deserialization inlined alongside packet reception
//   - Health-check heartbeat logic kept at call sites
//   - Socket I/O helpers inlined for tight polling loop
//
// The ML Adapter Bridge implements the Unreal Engine MLAdapter protocol,
// allowing external Python processes (e.g., an OpenAI Gym wrapper) to
// receive observations from and send actions back to the DTE embodied
// agent over a localhost TCP socket during live training sessions.
//
// Feature:  F1.5.6 — ML Adapter Bridge
// Phase:    1.5 — LiveBridge
// Epic:     E4 — Live-Learning Bridge
// ═══════════════════════════════════════════════════════════════════════════

#include "MLAdapterBridge.h"

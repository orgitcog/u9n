// ═══════════════════════════════════════════════════════════════════════════
// LockFreeMessageQueue.cpp
// Compilation unit for TLockFreeMessageQueue<T> — wait-free cognitive bus.
//
// TLockFreeMessageQueue<T> is a fully header-only, template-based class.
// All method bodies are defined inline in LockFreeMessageQueue.h to enable:
//   - Complete template instantiation at the include site
//   - Wait-free enqueue/dequeue using std::atomic with sequentially-consistent
//     ordering for the cognitive message bus hot path
//   - Inline SPSC ring-buffer read/write without mutex overhead
//
// This file provides a compilation unit for build-system hygiene and verifies
// that the template header compiles cleanly without external dependencies.
//
// Feature:  F1.3.1 — Lock-Free Message Queue
// Phase:    1.3 — Core Pipeline Infrastructure
// Epic:     E1 — Foundation & Core Integration
// ═══════════════════════════════════════════════════════════════════════════

#include "LockFreeMessageQueue.h"

// ═══════════════════════════════════════════════════════════════════════════
// echo_ml.cpp
// C++ compilation unit for the EchoML lightweight inference framework.
//
// The core function bodies are provided as ISO C source files compiled
// separately by the build system:
//   echo_tensor.c    — EchoTensor alloc/free/fill/copy and SIMD ops
//   echo_layers.c    — EchoDense and EchoEmbedding forward passes
//   echo_reservoir.c — EchoReservoir step/forward and sparse matvec
//   echo_noi_bridge.cpp — Noetic Inference bridge (C++ wrapper)
//
// This file:
//   1. Includes echo_ml.h in a C++ translation unit to verify that the
//      extern "C" declarations remain ABI-compatible with the C sources.
//   2. Serves as the anchor for any future C++ helper wrappers that sit
//      above the C interface (e.g. RAII wrappers, C++ span overloads).
//
// Feature:  F1.3.4 — EchoML Inference Engine
// Phase:    1.3 — Core Pipeline Infrastructure
// Epic:     E1 — Foundation & Core Integration
// ═══════════════════════════════════════════════════════════════════════════

#include "echo_ml.h"

// ─────────────────────────────────────────────────────────────────────────────
// C++ RAII helpers
// ─────────────────────────────────────────────────────────────────────────────
// These thin wrappers provide value-type resource management for the C structs
// without introducing any overhead on the hot inference path.

#ifdef __cplusplus

#include <utility>

namespace echo {

/// RAII wrapper for EchoEngine — owns initialisation / teardown lifecycle.
class EngineOwner {
public:
    explicit EngineOwner(const EchoConfig& cfg) {
        int result = echo_engine_init(&engine_, &cfg);
        if (result != ECHO_OK) {
            // Clean up any partial allocation from failed init
            echo_engine_free(&engine_);
            engine_ = {};
            initialized_ = false;
        } else {
            initialized_ = true;
        }
    }

    ~EngineOwner() {
        if (initialized_) {
            echo_engine_free(&engine_);
        }
    }

    // Non-copyable, movable
    EngineOwner(const EngineOwner&) = delete;
    EngineOwner& operator=(const EngineOwner&) = delete;

    EngineOwner(EngineOwner&& o) noexcept
        : engine_(o.engine_), initialized_(o.initialized_) {
        o.initialized_ = false;
        o.engine_ = {};  // Zero out source to prevent accidental use of stale pointers
    }

    bool valid()    const { return initialized_; }
    EchoEngine*     get() { return initialized_ ? &engine_ : nullptr; }

    void reset() {
        if (initialized_) {
            echo_engine_reset(&engine_);
        }
    }

private:
    EchoEngine engine_{};
    bool       initialized_{false};
};

/// RAII wrapper for EchoDict — owns load / free lifecycle.
class DictOwner {
public:
    explicit DictOwner(const char* path) {
        loaded_ = (echo_dict_load(&dict_, path) == ECHO_OK);
    }

    ~DictOwner() {
        if (loaded_) {
            echo_dict_free(&dict_);
        }
    }

    DictOwner(const DictOwner&) = delete;
    DictOwner& operator=(const DictOwner&) = delete;

    bool        valid()  const { return loaded_; }
    EchoDict*   get()          { return loaded_ ? &dict_ : nullptr; }

private:
    EchoDict dict_{};
    bool     loaded_{false};
};

} // namespace echo

#endif // __cplusplus

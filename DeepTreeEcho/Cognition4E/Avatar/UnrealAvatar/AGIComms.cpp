// ═══════════════════════════════════════════════════════════════════════════
// AGIComms.cpp
// Compilation unit for AGI communication data structures.
//
// AGIComms.h is a header-only USTRUCT/UENUM declaration file containing the
// UE-reflected data structures shared between the AGI core and the Unreal
// avatar layer:
//   - EPCGCommandType    — Spawn / Modify / Destroy PCG graph commands
//   - FPCGCommand        — A single PCG graph invocation with location/params
//   - FEchoStreamState   — Per-stream cognitive snapshot (load, valence, phase)
//   - FContextTerm       — A weighted term in the global context
//   - FGlobalContext     — Level + term array for multi-level attention
//   - FCognitiveState    — Aggregate of echo streams + global context
//   - FAGIStateUpdateMessage — Root message: cognitive state + PCG commands
//
// All structs use GENERATED_BODY() macros for UE5 reflection. No class
// methods require implementation — this translation unit exists solely to
// satisfy the UE build system's requirement that every .generated.h has a
// corresponding .cpp that includes the owning header.
//
// Feature:  F1.2.4 — Avatar AGI Communication Protocol
// Phase:    1.2 — Embodied Systems
// Epic:     E2 — Avatar Embodiment
// ═══════════════════════════════════════════════════════════════════════════

#include "AGIComms.h"

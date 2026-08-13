// ═══════════════════════════════════════════════════════════════════════════
// EigenToUEConverter.cpp
// Compilation unit for the EigenToUEConverter namespace.
//
// EigenToUEConverter provides only inline free functions and no class state.
// All conversions are defined inline in EigenToUEConverter.h to enable:
//   - Single-instruction scalar copy (Vector3f → FVector double cast)
//   - Compiler loop unrolling for fixed-size matrix copies
//   - Elimination of the conversion overhead via constant propagation
//
// Feature:  F1.1.4 — Type Conversion System (Eigen → UE)
// Phase:    1.1 — Neural-Symbolic Bridge Architecture
// Epic:     E1 — Foundation & Core Integration
// ═══════════════════════════════════════════════════════════════════════════

#include "EigenToUEConverter.h"

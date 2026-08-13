// ═══════════════════════════════════════════════════════════════════════════
// UEToEigenConverter.cpp
// Compilation unit for the UEToEigenConverter namespace.
//
// UEToEigenConverter provides only inline free functions and no class state.
// All conversions are defined inline in UEToEigenConverter.h to enable:
//   - Single-instruction scalar copy (FVector double → Eigen float cast)
//   - Compiler loop unrolling for fixed-size matrix copies
//   - Elimination of the conversion overhead via constant propagation
//
// Feature:  F1.1.4 — Type Conversion System (UE → Eigen)
// Phase:    1.1 — Neural-Symbolic Bridge Architecture
// Epic:     E1 — Foundation & Core Integration
// ═══════════════════════════════════════════════════════════════════════════

#include "UEToEigenConverter.h"

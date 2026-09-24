// ═══════════════════════════════════════════════════════════════════════════
// IdentityCoreMLP.cpp
// Compilation unit for FIdentityCoreMLP — identity-core multi-layer perceptron.
//
// FIdentityCoreMLP is a fully header-only class. All method bodies are
// defined inline in IdentityCoreMLP.h to enable:
//   - Inlined forward-pass matrix multiplications using Eigen BLAS paths
//   - Zero-copy layer activation with in-place tanh/relu applied by Eigen
//   - Compile-time network depth unrolling for small fixed-size architectures
//
// Feature:  F1.2.3 — Identity Core MLP
// Phase:    1.2 — Persona Layer
// Epic:     E1 — Foundation & Core Integration
// ═══════════════════════════════════════════════════════════════════════════

#include "IdentityCoreMLP.h"

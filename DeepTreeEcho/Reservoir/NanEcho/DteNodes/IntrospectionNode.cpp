// ═══════════════════════════════════════════════════════════════════════════
// IntrospectionNode.cpp
// Compilation unit for FIntrospectionNode — meta-cognitive monitoring node.
//
// FIntrospectionNode is a fully header-only class. All method bodies are
// defined inline in IntrospectionNode.h to enable:
//   - Inline self-model accuracy update using Eigen running mean/variance
//   - Zero-overhead cognitive-load sampling from reservoir activation norms
//   - Tight entropy computation loop for surprise/novelty detection
//
// Feature:  F1.1.5 — Introspection Node
// Phase:    1.1 — NanEcho DteNodes Layer
// Epic:     E1 — Foundation & Core Integration
// ═══════════════════════════════════════════════════════════════════════════

#include "IntrospectionNode.h"

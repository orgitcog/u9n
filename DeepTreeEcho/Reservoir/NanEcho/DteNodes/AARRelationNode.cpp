// ═══════════════════════════════════════════════════════════════════════════
// AARRelationNode.cpp
// Compilation unit for FAARRelationNode — Affective-Action-Relation node.
//
// FAARRelationNode is a fully header-only class. All method bodies are
// defined inline in AARRelationNode.h to enable:
//   - Eigen-vectorised affective valence × arousal projection
//   - Inline action-utility scoring over typed relation slots
//   - Tight loops for relation-set lookup and update
//
// Feature:  F1.1.3 — AAR Relation Node
// Phase:    1.1 — NanEcho DteNodes Layer
// Epic:     E1 — Foundation & Core Integration
// ═══════════════════════════════════════════════════════════════════════════

#include "AARRelationNode.h"

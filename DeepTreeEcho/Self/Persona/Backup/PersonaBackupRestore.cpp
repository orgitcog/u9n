// ═══════════════════════════════════════════════════════════════════════════
// PersonaBackupRestore.cpp
// Compilation unit for FPersonaBackupRestore — persona serialisation system.
//
// FPersonaBackupRestore is a fully header-only class. All method bodies are
// defined inline in PersonaBackupRestore.h to enable:
//   - Inline persona-state serialisation to UE FArchive
//   - Zero-overhead identity-vector checksum using Eigen norm computation
//   - Static helper functions for persona file path resolution
//
// Feature:  F1.2.4 — Persona Backup & Restore
// Phase:    1.2 — Persona Layer
// Epic:     E1 — Foundation & Core Integration
// ═══════════════════════════════════════════════════════════════════════════

#include "PersonaBackupRestore.h"

// ═══════════════════════════════════════════════════════════════════════════
// IonCognitiveShell.cpp
// Compilation unit for IonCognitiveShell — Virtual Hardware Cognitive Shell.
//
// FIonCognitiveShell is a fully header-only class. All method bodies are
// defined inline in IonCognitiveShell.h to enable:
//   - Direct inlining of dispatch-table hot paths
//   - Access to UE5 FPlatformTime intrinsics at call sites
//   - Zero-overhead handler dispatch via std::function stored in DispatchIVT
//
// This file provides a single compilation unit to verify that the header
// compiles correctly in the UE5 build environment and to anchor any
// future non-inline helpers.
// ═══════════════════════════════════════════════════════════════════════════

#include "IonCognitiveShell.h"

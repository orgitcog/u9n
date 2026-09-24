// ═══════════════════════════════════════════════════════════════════════════
// VirtualControllerDriver.cpp
// Compilation unit for FVirtualControllerDriver — gamepad abstraction.
//
// FVirtualControllerDriver is a fully header-only class. All method bodies
// are defined inline in VirtualControllerDriver.h to enable:
//   - SDL2/XInput controller polling directly from the header
//   - Dead-zone clamping and axis normalization inlined
//   - Button-state delta computation kept at call sites
//   - Native VirtualController input injection inlined
//
// The virtual controller driver bridges physical gamepad inputs into the
// DTE autonomy pipeline's action space, normalizing inputs across backends
// (SDL2, XInput, PS5 DualSense) into a unified FControllerState snapshot.
//
// Feature:  F1.4.3 — Virtual Controller Driver
// Phase:    1.4 — Embodied Systems Layer
// Epic:     E2 — Embodied Cognition & Avatar Systems
// ═══════════════════════════════════════════════════════════════════════════

#include "VirtualControllerDriver.h"

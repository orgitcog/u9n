// ═══════════════════════════════════════════════════════════════════════════
// VisionSystem.cpp
// Compilation unit for FVisionSystem — DTE's visual cortex.
//
// FVisionSystem is a fully header-only class. All method bodies are
// defined inline in VisionSystem.h to enable:
//   - Sobel edge detection computed in-place without virtual dispatch
//   - Spatial pooling grid loop fully unrollable at compile time
//   - Color histogram and temporal differencing inlined for SIMD
//   - Saliency center-surround computation inlined at call sites
//
// The vision system implements a biologically-inspired hierarchy:
//   Raw pixels → Grayscale → Spatial Pool → Edge Detect →
//   Color Histogram → Temporal Diff → Saliency → 256D Feature Vector
//
// Feature:  F1.4.2 — Vision System
// Phase:    1.4 — Embodied Systems Layer
// Epic:     E2 — Embodied Cognition & Avatar Systems
// ═══════════════════════════════════════════════════════════════════════════

#include "VisionSystem.h"

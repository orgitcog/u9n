/**
 * @file touchpad_adapter.cpp
 * @brief Thin compilation unit for TouchpadEndocrineAdapter
 *
 * Ensures the vtable for TouchpadInterface is emitted in exactly one
 * translation unit — same pattern as marduk_adapter.cpp.
 */

#include <opencog/endocrine/touchpad_adapter.hpp>

// Intentionally empty — the header-only adapter and interface are
// fully defined in touchpad_adapter.hpp and touchpad_types.hpp.
// This file anchors the vtable for TouchpadInterface.

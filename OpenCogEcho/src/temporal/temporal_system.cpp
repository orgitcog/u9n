/**
 * @file temporal_system.cpp
 * @brief Thin compilation unit for TemporalSystem facade
 *
 * Most of the temporal system is header-only templates and
 * inline methods. This file ensures the translation unit exists
 * and can hold any future non-inline implementations.
 */

#include <opencog/temporal/temporal_system.hpp>

// Currently all methods are inline in the header.
// Future: event dispatch, SIMD batch operations, etc.

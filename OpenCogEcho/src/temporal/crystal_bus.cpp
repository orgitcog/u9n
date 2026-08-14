/**
 * @file crystal_bus.cpp
 * @brief Thin compilation unit for CrystalBus
 *
 * CrystalBus is mostly header-only. This file ensures the
 * translation unit exists for any future out-of-line methods.
 */

#include <opencog/temporal/crystal_bus.hpp>

// Currently all methods are inline in the header.
// Future SIMD-optimized tick() can be moved here.

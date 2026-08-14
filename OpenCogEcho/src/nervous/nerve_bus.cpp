/**
 * @file nerve_bus.cpp
 * @brief NerveBus compilation unit
 *
 * Thin TU that ensures NerveBus non-inline methods are compiled once.
 * Most of the implementation is header-only for inlining of hot paths.
 */

#include <opencog/nervous/nerve_bus.hpp>

namespace opencog::nerv {

// All implementation is currently in the header for inlining.
// This TU exists for any future out-of-line methods and to ensure
// the header compiles cleanly as a standalone unit.

} // namespace opencog::nerv

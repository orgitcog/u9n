/**
 * @file nervous_system.cpp
 * @brief NervousSystem compilation unit
 *
 * Thin TU that ensures NervousSystem non-inline methods are compiled once.
 * Most implementation is header-only for inlining of hot paths.
 */

#include <opencog/nervous/nervous_system.hpp>

namespace opencog::nerv {

// All implementation is currently in the header for inlining.
// This TU exists for any future out-of-line methods and to ensure
// the header compiles cleanly as a standalone unit.

} // namespace opencog::nerv

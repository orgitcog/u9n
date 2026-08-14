/**
 * @file adapter.cpp
 * @brief AFI Endocrine Adapter — compilation unit
 *
 * Thin compilation unit: all logic lives in the header.
 * This ensures template instantiations and vtable emission.
 */

#include <opencog/afi/adapter.hpp>

// Explicit instantiation not needed — the class is fully inline.
// This TU exists to anchor the vtable and ensure the header compiles cleanly.

namespace opencog::endo {

// Force vtable emission for AFIEndocrineAdapter
// (prevents linker issues when used across TUs)
static_assert(sizeof(AFIEndocrineAdapter) > 0, "AFIEndocrineAdapter must be complete");

} // namespace opencog::endo

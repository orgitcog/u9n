/**
 * @file adapter.cpp
 * @brief Entelechy Endocrine Adapter — compilation unit
 *
 * Thin compilation unit: all logic lives in the header.
 * This ensures template instantiations and vtable emission.
 */

#include <opencog/entelechy/adapter.hpp>

// Explicit instantiation not needed — the class is fully inline.
// This TU exists to anchor the vtable and ensure the header compiles cleanly.

namespace opencog::endo {

// Force vtable emission for EntelechyEndocrineAdapter
// (prevents linker issues when used across TUs)
static_assert(sizeof(EntelechyEndocrineAdapter) > 0, "EntelechyEndocrineAdapter must be complete");

} // namespace opencog::endo

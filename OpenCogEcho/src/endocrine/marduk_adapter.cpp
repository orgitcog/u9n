/**
 * @file marduk_adapter.cpp
 * @brief Marduk Endocrine Adapter — compilation unit
 *
 * Thin compilation unit: all logic lives in the header.
 * This ensures template instantiations and vtable emission.
 */

#include <opencog/endocrine/marduk_adapter.hpp>

// Explicit instantiation not needed — the class is fully inline.
// This TU exists to anchor the vtable and ensure the header compiles cleanly.

namespace opencog::endo {

// Force vtable emission for MardukInterface
// (prevents linker issues when MardukInterface is used across TUs)
static_assert(sizeof(MardukEndocrineAdapter) > 0, "MardukEndocrineAdapter must be complete");

} // namespace opencog::endo

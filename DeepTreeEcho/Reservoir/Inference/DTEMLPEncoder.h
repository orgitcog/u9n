/*
 * DeepTreeEcho/Inference/DTEMLPEncoder.h
 *
 * Thin wrapper around the vendored coggml DTE-MLP identity encoder
 * (dte_mlp_vendored.hpp — cogpy/coggml include/cog/dte_mlp.hpp,
 * standalone C++11 header, no external dependencies).
 *
 * The DTE-MLP encodes the Deep Tree Echo identity mesh: an AAR triad
 * (Agent/Arena/Relation, 5 dims each), ontogenetic autonomy state
 * (9 dims), Echobeats phase (5 dims), plus compressed hypergraph and
 * reservoir latents. The MLP weights ARE the persistent checkpoint —
 * loading weights = restoring self.
 *
 * Gap A (in-engine inference) of the DTE integration map. The alias
 * layer below exposes the vendored types in the `dte` namespace used
 * by the rest of DeepTreeEcho/Inference, and adds a convenience bridge
 * from DTEReservoirCore state into the IdentityVector reservoir latent.
 */

#ifndef DTE_INFERENCE_MLP_ENCODER_H
#define DTE_INFERENCE_MLP_ENCODER_H

#include "dte_mlp_vendored.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace dte
{

// ---------------------------------------------------------------
// Namespace aliases: vendored cog::dte types exposed as dte::mlp
// ---------------------------------------------------------------
namespace mlp = ::cog::dte;

using IdentityVector = mlp::IdentityVector;
using AutonomyLevel = mlp::AutonomyLevel;

/**
 * Copy an arbitrary-precision reservoir state (e.g. from
 * DTEReservoirCore::get_state()) into the float reservoir latent of
 * an IdentityVector, truncating or zero-padding to the identity's
 * configured reservoir_dim.
 */
inline void encode_reservoir_state(IdentityVector& identity,
                                   const std::vector<double>& reservoir_state)
{
    const std::size_t n = identity.reservoir_state.size();
    for (std::size_t i = 0; i < n; ++i) {
        identity.reservoir_state[i] =
            (i < reservoir_state.size())
                ? static_cast<float>(reservoir_state[i])
                : 0.0f;
    }
}

/**
 * Extract the reservoir latent from an IdentityVector as doubles,
 * suitable for feeding back into DTEReservoirCore workflows.
 */
inline std::vector<double> decode_reservoir_state(const IdentityVector& identity)
{
    std::vector<double> out(identity.reservoir_state.size());
    std::transform(identity.reservoir_state.begin(),
                   identity.reservoir_state.end(),
                   out.begin(),
                   [](float f) { return static_cast<double>(f); });
    return out;
}

} // namespace dte

#endif // DTE_INFERENCE_MLP_ENCODER_H

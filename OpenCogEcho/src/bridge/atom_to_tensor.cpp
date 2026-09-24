/**
 * @file atom_to_tensor.cpp
 * @brief Implementation of OpenCog atom → Eigen tensor conversions
 *
 * Feature F1.1.3: Type Conversion Layer
 */

#include <opencog/bridge/atom_to_tensor.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <string>
#include <unordered_map>

namespace opencog::bridge {

// ============================================================================
// nodes_to_activation
// ============================================================================

ConversionResult<Eigen::VectorXf>
nodes_to_activation(const AtomSpace& space,
                    const std::vector<Handle>& nodes)
{
    if (nodes.empty())
        return ConversionResult<Eigen::VectorXf>::fail(ConversionError::EmptyInput);

    Eigen::VectorXf vec(static_cast<int>(nodes.size()));
    int skipped = 0;

    for (int i = 0; i < static_cast<int>(nodes.size()); ++i) {
        const Handle& h = nodes[static_cast<size_t>(i)];
        if (!space.contains(h)) {
            vec[i] = 0.0f;
            ++skipped;
            continue;
        }
        TruthValue tv = space.get_tv(h);
        vec[i] = tv.strength;
    }

    auto quality = (skipped == 0) ? ConversionQuality::Lossless
                                   : ConversionQuality::NormalPrecision;
    return ConversionResult<Eigen::VectorXf>::success(
        std::move(vec), static_cast<int>(nodes.size()) - skipped, skipped, quality);
}

// ============================================================================
// links_to_matrix
// ============================================================================

ConversionResult<Eigen::MatrixXf>
links_to_matrix(const AtomSpace& space,
                const std::vector<Handle>& links)
{
    if (links.empty())
        return ConversionResult<Eigen::MatrixXf>::fail(ConversionError::EmptyInput);

    // First pass: discover unique source and target atoms, assign indices
    std::unordered_map<uint64_t, int> src_index;
    std::unordered_map<uint64_t, int> tgt_index;

    struct Entry {
        int src;
        int tgt;
        float weight;
    };
    std::vector<Entry> entries;
    entries.reserve(links.size());
    int skipped = 0;

    for (auto& link_h : links) {
        if (!space.contains(link_h)) {
            ++skipped;
            continue;
        }

        // EvaluationLink should have arity 2: (pred, ListLink(src, tgt))
        auto outgoing = space.get_outgoing(link_h);
        if (outgoing.size() != 2) {
            ++skipped;
            continue;
        }

        // Second element should be a ListLink with 2 elements
        Handle list_h = outgoing[1];
        if (!space.contains(list_h)) {
            ++skipped;
            continue;
        }

        auto list_out = space.get_outgoing(list_h);
        if (list_out.size() != 2) {
            ++skipped;
            continue;
        }

        Handle src_h = list_out[0];
        Handle tgt_h = list_out[1];

        uint64_t src_id = src_h.id().value;
        uint64_t tgt_id = tgt_h.id().value;

        if (src_index.find(src_id) == src_index.end())
            src_index[src_id] = static_cast<int>(src_index.size());
        if (tgt_index.find(tgt_id) == tgt_index.end())
            tgt_index[tgt_id] = static_cast<int>(tgt_index.size());

        TruthValue tv = space.get_tv(link_h);
        entries.push_back({src_index[src_id], tgt_index[tgt_id], tv.strength});
    }

    if (entries.empty())
        return ConversionResult<Eigen::MatrixXf>::fail(ConversionError::EmptyInput);

    // Build matrix
    int rows = static_cast<int>(src_index.size());
    int cols = static_cast<int>(tgt_index.size());
    Eigen::MatrixXf mat = Eigen::MatrixXf::Zero(rows, cols);

    for (auto& e : entries) {
        mat(e.src, e.tgt) = e.weight;
    }

    auto quality = (skipped == 0) ? ConversionQuality::Lossless
                                   : ConversionQuality::NormalPrecision;
    return ConversionResult<Eigen::MatrixXf>::success(
        std::move(mat), static_cast<int>(entries.size()), skipped, quality);
}

// ============================================================================
// atom_to_state
// ============================================================================

ConversionResult<Eigen::VectorXf>
atom_to_state(const AtomSpace& space,
              Handle state_link)
{
    if (!space.contains(state_link))
        return ConversionResult<Eigen::VectorXf>::fail(ConversionError::AtomNotFound);

    // StateLink → (AnchorNode, ListLink)
    auto outgoing = space.get_outgoing(state_link);
    if (outgoing.size() != 2)
        return ConversionResult<Eigen::VectorXf>::fail(ConversionError::InvalidAtomType);

    Handle list_h = outgoing[1];
    if (!space.contains(list_h))
        return ConversionResult<Eigen::VectorXf>::fail(ConversionError::AtomNotFound);

    auto elements = space.get_outgoing(list_h);
    if (elements.empty())
        return ConversionResult<Eigen::VectorXf>::fail(ConversionError::EmptyInput);

    Eigen::VectorXf state(static_cast<int>(elements.size()));
    int skipped = 0;

    for (int i = 0; i < static_cast<int>(elements.size()); ++i) {
        Handle num_h = elements[static_cast<size_t>(i)];
        if (!space.contains(num_h)) {
            state[i] = 0.0f;
            ++skipped;
            continue;
        }

        // Parse the NumberNode name as a float
        auto name = space.get_name(num_h);
        float val = 0.0f;
        auto [ptr, ec] = std::from_chars(name.data(), name.data() + name.size(), val);
        if (ec != std::errc{}) {
            state[i] = 0.0f;
            ++skipped;
        } else {
            state[i] = val;
        }
    }

    auto quality = (skipped == 0) ? ConversionQuality::HighPrecision
                                   : ConversionQuality::NormalPrecision;
    return ConversionResult<Eigen::VectorXf>::success(
        std::move(state), static_cast<int>(elements.size()) - skipped, skipped, quality);
}

// ============================================================================
// attention_focus_to_vector
// ============================================================================

ConversionResult<Eigen::VectorXf>
attention_focus_to_vector(const AtomSpace& space,
                          const AttentionBank& bank,
                          size_t max_size)
{
    auto af = bank.get_attentional_focus();
    if (af.empty())
        return ConversionResult<Eigen::VectorXf>::fail(ConversionError::EmptyInput);

    size_t count = (max_size > 0 && af.size() > max_size) ? max_size : af.size();
    Eigen::VectorXf vec(static_cast<int>(count));
    int skipped = 0;

    for (size_t i = 0; i < count; ++i) {
        Handle h = space.make_handle(af[i]);
        if (!space.contains(h)) {
            vec[static_cast<int>(i)] = 0.0f;
            ++skipped;
            continue;
        }
        AttentionValue av = space.get_av(h);
        vec[static_cast<int>(i)] = av.sti;
    }

    auto quality = (skipped == 0) ? ConversionQuality::Lossless
                                   : ConversionQuality::NormalPrecision;
    return ConversionResult<Eigen::VectorXf>::success(
        std::move(vec), static_cast<int>(count) - skipped, skipped, quality);
}

} // namespace opencog::bridge

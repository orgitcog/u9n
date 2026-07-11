/**
 * @file attention_bank.cpp
 * @brief AttentionBank implementation
 */

#include <opencog/attention/attention_bank.hpp>

#include <algorithm>
#include <cmath>

namespace opencog {

AttentionBank::AttentionBank(AtomSpace& space, ECANConfig config)
    : space_(space)
    , config_(config)
    , sti_funds_(config.initial_sti_funds)
    , lti_funds_(config.initial_lti_funds)
    , af_boundary_(config.af_boundary)
{
}

AttentionBank::~AttentionBank() = default;

// ============================================================================
// Core Operations
// ============================================================================

float AttentionBank::stimulate(AtomId id, float amount) {
    if (!space_.contains(id)) return 0.0f;

    // Deduct from funds
    float available = sti_funds_.load(std::memory_order_relaxed);
    float actual_amount = std::min(amount, available);

    while (!sti_funds_.compare_exchange_weak(available,
            available - actual_amount,
            std::memory_order_release,
            std::memory_order_relaxed)) {
        actual_amount = std::min(amount, available);
    }

    // Add to atom's STI
    AttentionValue av = space_.atom_table().get_av(id);
    av.sti += actual_amount;
    space_.atom_table().set_av(id, av);

    return av.sti;
}

void AttentionBank::transfer_sti(AtomId from, AtomId to, float amount) {
    if (!space_.contains(from) || !space_.contains(to)) return;

    auto& table = space_.atom_table();
    AttentionValue av_from = table.get_av(from);
    AttentionValue av_to = table.get_av(to);

    float actual = std::min(amount, av_from.sti);
    av_from.sti -= actual;
    av_to.sti += actual;

    table.set_av(from, av_from);
    table.set_av(to, av_to);
}

void AttentionBank::spread_activation(AtomId source) {
    if (!space_.contains(source)) return;

    auto& table = space_.atom_table();
    AttentionValue av = table.get_av(source);

    float to_spread = av.sti * config_.spreading_rate;
    if (to_spread <= 0.0f) return;

    // Get neighbors (incoming links and their other members)
    auto incoming = table.get_incoming(source);
    if (incoming.empty()) return;

    float per_neighbor = to_spread / static_cast<float>(incoming.size());

    for (AtomId link_id : incoming) {
        // Spread to the link itself
        float link_share = per_neighbor * 0.5f;
        transfer_sti(source, link_id, link_share);

        // Spread to other atoms in the link
        auto outgoing = table.get_outgoing(link_id);
        float member_share = per_neighbor * 0.5f / static_cast<float>(outgoing.size());

        for (AtomId member : outgoing) {
            if (member != source) {
                float weight = compute_hebbian_weight(source, member);
                transfer_sti(source, member, member_share * weight);
            }
        }
    }
}

void AttentionBank::update_cycle() {
    collect_rent();
    decay_lti();
    update_af_boundary();
}

// ============================================================================
// Attention Queries
// ============================================================================

std::vector<AtomId> AttentionBank::get_attentional_focus() const {
    std::vector<std::pair<AtomId, float>> candidates;
    float boundary = af_boundary_.load(std::memory_order_relaxed);

    space_.for_each_atom([&](Handle h) {
        AttentionValue av = space_.get_av(h);
        if (av.sti >= boundary) {
            candidates.emplace_back(h.id(), av.sti);
        }
    });

    // Sort by STI descending
    std::sort(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; });

    // Extract IDs, respecting max size
    std::vector<AtomId> result;
    size_t max_size = config_.af_max_size;
    result.reserve(std::min(candidates.size(), max_size));

    for (size_t i = 0; i < candidates.size() && i < max_size; ++i) {
        result.push_back(candidates[i].first);
    }

    return result;
}

std::vector<AtomId> AttentionBank::get_attentional_focus_by_type(AtomType type) const {
    std::vector<AtomId> result;
    float boundary = af_boundary_.load(std::memory_order_relaxed);

    for (Handle h : space_.get_atoms_by_type(type)) {
        if (space_.get_av(h).sti >= boundary) {
            result.push_back(h.id());
        }
    }

    return result;
}

bool AttentionBank::in_attentional_focus(AtomId id) const {
    if (!space_.contains(id)) return false;
    float boundary = af_boundary_.load(std::memory_order_relaxed);
    return space_.atom_table().get_av(id).sti >= boundary;
}

float AttentionBank::get_af_boundary() const noexcept {
    return af_boundary_.load(std::memory_order_relaxed);
}

// ============================================================================
// Fund Management
// ============================================================================

float AttentionBank::get_sti_funds() const noexcept {
    return sti_funds_.load(std::memory_order_relaxed);
}

float AttentionBank::get_lti_funds() const noexcept {
    return lti_funds_.load(std::memory_order_relaxed);
}

void AttentionBank::add_sti_funds(float amount) {
    sti_funds_.fetch_add(amount, std::memory_order_relaxed);
}

void AttentionBank::add_lti_funds(float amount) {
    lti_funds_.fetch_add(amount, std::memory_order_relaxed);
}

// ============================================================================
// Statistics
// ============================================================================

size_t AttentionBank::get_af_size() const noexcept {
    return af_size_.load(std::memory_order_relaxed);
}

float AttentionBank::get_total_sti() const {
    float total = 0.0f;
    space_.for_each_atom([&](Handle h) {
        total += space_.get_av(h).sti;
    });
    return total;
}

float AttentionBank::get_max_sti() const {
    float max_sti = std::numeric_limits<float>::lowest();
    space_.for_each_atom([&](Handle h) {
        max_sti = std::max(max_sti, space_.get_av(h).sti);
    });
    return max_sti;
}

float AttentionBank::get_min_sti() const {
    float min_sti = std::numeric_limits<float>::max();
    space_.for_each_atom([&](Handle h) {
        min_sti = std::min(min_sti, space_.get_av(h).sti);
    });
    return min_sti;
}

// ============================================================================
// Forgetting
// ============================================================================

size_t AttentionBank::mark_for_forgetting() {
    forgetting_candidates_.clear();

    space_.for_each_atom([&](Handle h) {
        AttentionValue av = space_.get_av(h);
        if (av.vlti == 0 && av.sti < config_.forgetting_threshold) {
            forgetting_candidates_.push_back(h.id());
        }
    });

    return forgetting_candidates_.size();
}

size_t AttentionBank::forget() {
    size_t forgotten = 0;

    for (AtomId id : forgetting_candidates_) {
        if (space_.contains(id)) {
            if (forget_callback_) {
                forget_callback_(id);
            }
            if (space_.remove(id, false)) {
                ++forgotten;
            }
        }
    }

    forgetting_candidates_.clear();
    return forgotten;
}

void AttentionBank::on_forget(std::function<void(AtomId)> callback) {
    forget_callback_ = std::move(callback);
}

// ============================================================================
// Internal Helpers
// ============================================================================

void AttentionBank::collect_rent() {
    float total_rent = 0.0f;
    float boundary = af_boundary_.load(std::memory_order_relaxed);

    space_.for_each_atom([&](Handle h) {
        AttentionValue av = space_.get_av(h);

        // STI rent for atoms in attentional focus
        if (av.sti >= boundary) {
            float rent = config_.sti_rent;
            av.sti -= rent;
            total_rent += rent;
        }

        // LTI rent for all atoms
        float lti_rent = config_.lti_rent;
        av.lti = static_cast<int16_t>(std::max(
            static_cast<int>(av.lti) - static_cast<int>(lti_rent),
            static_cast<int>(std::numeric_limits<int16_t>::min())
        ));

        space_.set_av(h, av);
    });

    // Return rent to funds
    sti_funds_.fetch_add(total_rent, std::memory_order_relaxed);
}

void AttentionBank::decay_lti() {
    space_.for_each_atom([&](Handle h) {
        AttentionValue av = space_.get_av(h);
        float decay = static_cast<float>(av.lti) * config_.lti_decay_rate;
        av.lti = static_cast<int16_t>(static_cast<float>(av.lti) - decay);
        space_.set_av(h, av);
    });
}

void AttentionBank::update_af_boundary() {
    // Adjust boundary to maintain target AF size
    size_t current_size = 0;
    float boundary = af_boundary_.load(std::memory_order_relaxed);

    space_.for_each_atom([&](Handle h) {
        if (space_.get_av(h).sti >= boundary) {
            ++current_size;
        }
    });

    af_size_.store(current_size, std::memory_order_relaxed);

    // Adjust boundary if AF is too large or too small
    if (current_size > config_.af_max_size * 1.2) {
        // Raise boundary
        af_boundary_.fetch_add(1.0f, std::memory_order_relaxed);
    } else if (current_size < config_.af_max_size * 0.8 && boundary > config_.af_boundary) {
        // Lower boundary (but not below minimum)
        af_boundary_.fetch_sub(1.0f, std::memory_order_relaxed);
    }
}

float AttentionBank::compute_hebbian_weight(AtomId from, AtomId to) const {
    // Simple co-activation weight
    // In a full implementation, this would track HebbianLinks
    AttentionValue av_from = space_.atom_table().get_av(from);
    AttentionValue av_to = space_.atom_table().get_av(to);

    // Both in AF = stronger connection
    float boundary = af_boundary_.load(std::memory_order_relaxed);
    if (av_from.sti >= boundary && av_to.sti >= boundary) {
        return config_.hebbian_weight * 1.5f;
    }

    return config_.hebbian_weight;
}

// ============================================================================
// ImportanceDiffusionAgent
// ============================================================================

ImportanceDiffusionAgent::ImportanceDiffusionAgent(AttentionBank& bank, AtomSpace& space)
    : bank_(bank), space_(space)
{
}

ImportanceDiffusionAgent::~ImportanceDiffusionAgent() {
    stop();
}

void ImportanceDiffusionAgent::start() {
    running_.store(true, std::memory_order_relaxed);
    // In a full implementation, this would spawn a thread
}

void ImportanceDiffusionAgent::stop() {
    running_.store(false, std::memory_order_relaxed);
}

void ImportanceDiffusionAgent::run_once() {
    // Get atoms in attentional focus and spread their activation
    auto af = bank_.get_attentional_focus();
    for (AtomId id : af) {
        bank_.spread_activation(id);
    }
}

// ============================================================================
// ForgettingAgent
// ============================================================================

ForgettingAgent::ForgettingAgent(AttentionBank& bank, AtomSpace& space)
    : bank_(bank), space_(space)
{
}

ForgettingAgent::~ForgettingAgent() {
    stop();
}

void ForgettingAgent::start() {
    running_.store(true, std::memory_order_relaxed);
}

void ForgettingAgent::stop() {
    running_.store(false, std::memory_order_relaxed);
}

void ForgettingAgent::run_once() {
    bank_.mark_for_forgetting();
    bank_.forget();
}

} // namespace opencog

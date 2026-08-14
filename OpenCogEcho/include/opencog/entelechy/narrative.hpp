#pragma once
/**
 * @file narrative.hpp
 * @brief Narrative Identity system (McAdams / Bruner)
 *
 * Models the life-story level of personality (McAdams' Level 3):
 * - Chapter detection via valence-trajectory shift analysis
 * - Theme classification from emotional tone (agency, communion, etc.)
 * - Redemption / contamination arc detection (McAdams' key sequences)
 * - Coherence and identity-strength metrics
 *
 * Scientific basis:
 *   McAdams, D. P. (2001). The psychology of life stories.
 *   Bruner, J. (1991). The narrative construction of reality.
 *
 * Design: Header-only logic, thin .cpp compilation unit.
 */

#include <opencog/entelechy/types.hpp>
#include <opencog/endocrine/types.hpp>
#include <opencog/core/types.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <vector>

namespace opencog::entelechy {

// ============================================================================
// NarrativeIdentity — Life-story level personality (McAdams Level 3)
// ============================================================================

class NarrativeIdentity {
public:
    explicit NarrativeIdentity(uint64_t start_tick = 0) {
        // Begin the life story with a GROWTH chapter
        start_new_chapter(start_tick, NarrativeTheme::GROWTH, ValenceSignature::neutral());
    }

    // ---- Main update (called periodically, e.g. every ~100 ticks) ----------

    /**
     * @brief Advance the narrative with a new valence observation.
     * @param current_tick  Simulation tick
     * @param current_valence  Current affective state
     * @return true if a chapter boundary was detected and a new chapter opened
     */
    bool update(uint64_t current_tick, const ValenceSignature& current_valence) {
        // Record in sliding window
        valence_window_.push_back(ValenceEntry{current_tick, current_valence});

        // Prune old entries (keep at most 2x window_size_ for bounded memory)
        prune_window();

        // Update current chapter's emotional tone as running average
        if (!chapters_.empty()) {
            auto& cur = chapters_.back();
            // Blend toward current valence (exponential moving average)
            constexpr float alpha = 0.05f;
            cur.emotional_tone = cur.emotional_tone.blend(current_valence, alpha);
        }

        // Check for chapter boundary
        if (detect_boundary(current_valence)) {
            // Close current chapter
            if (!chapters_.empty()) {
                auto& cur = chapters_.back();
                cur.end_tick = current_tick;
                cur.dominant_theme = classify_theme(cur);

                // Check for McAdams redemption/contamination arcs and tag
                // (need at least the chapter being closed + the new one we are about to open)
            }

            // Classify the new chapter theme from the incoming valence
            NarrativeTheme new_theme = classify_theme_from_valence(current_valence);
            start_new_chapter(current_tick, new_theme, current_valence);

            // Post-hoc arc tagging: check the two most recent closed+open chapters
            if (is_redemption_arc()) {
                chapters_.back().dominant_theme = NarrativeTheme::REDEMPTION;
            } else if (is_contamination_arc()) {
                chapters_.back().dominant_theme = NarrativeTheme::CONTAMINATION;
            }

            return true;
        }

        return false;
    }

    // ---- Episode recording -------------------------------------------------

    /// Add a key episode AtomId to the current (open) chapter
    void record_episode(AtomId episode) {
        if (!chapters_.empty()) {
            chapters_.back().key_episodes.push_back(episode);
        }
    }

    // ---- Accessors ---------------------------------------------------------

    [[nodiscard]] const NarrativeChapter& current_chapter() const noexcept {
        return chapters_.back();
    }

    [[nodiscard]] size_t chapter_count() const noexcept {
        return chapters_.size();
    }

    [[nodiscard]] const std::vector<NarrativeChapter>& chapters() const noexcept {
        return chapters_;
    }

    // ---- Theme analysis ----------------------------------------------------

    /**
     * @brief Most frequent theme across the entire life story.
     */
    [[nodiscard]] NarrativeTheme dominant_life_theme() const noexcept {
        size_t best_idx = 0;
        size_t best_count = 0;
        for (size_t i = 0; i < NARRATIVE_THEME_COUNT; ++i) {
            if (theme_counts_[i] > best_count) {
                best_count = theme_counts_[i];
                best_idx = i;
            }
        }
        return static_cast<NarrativeTheme>(best_idx);
    }

    /**
     * @brief Narrative coherence: ratio of dominant-theme chapters to total.
     *
     * 1.0 = all chapters share the same theme (highly coherent narrative).
     * Approaches 1/NARRATIVE_THEME_COUNT for maximally diverse stories.
     */
    [[nodiscard]] float narrative_coherence() const noexcept {
        if (chapters_.empty()) return 0.0f;
        size_t max_count = *std::max_element(theme_counts_.begin(), theme_counts_.end());
        return static_cast<float>(max_count) / static_cast<float>(chapters_.size());
    }

    /**
     * @brief Identity strength: grows with both coherence AND accumulated experience.
     *
     * strength = coherence * log2(1 + chapter_count) * 0.2, clamped to [0, 1].
     */
    [[nodiscard]] float identity_strength() const noexcept {
        float coherence = narrative_coherence();
        float experience = std::log2(1.0f + static_cast<float>(chapters_.size()));
        return std::clamp(coherence * experience * 0.2f, 0.0f, 1.0f);
    }

    /**
     * @brief Frequency of a specific theme as a fraction of total chapters.
     */
    [[nodiscard]] float theme_frequency(NarrativeTheme theme) const noexcept {
        if (chapters_.empty()) return 0.0f;
        auto idx = static_cast<size_t>(theme);
        if (idx >= NARRATIVE_THEME_COUNT) return 0.0f;
        return static_cast<float>(theme_counts_[idx]) / static_cast<float>(chapters_.size());
    }

    // ---- Arc detection (McAdams' key sequences) ----------------------------

    /**
     * @brief Redemption arc: previous chapter negative, current chapter positive.
     *
     * McAdams: "a demonstrably bad or affectively negative event or
     * circumstance is followed by a demonstrably good or positive outcome."
     */
    [[nodiscard]] bool is_redemption_arc() const noexcept {
        if (chapters_.size() < 2) return false;
        const auto& prev = chapters_[chapters_.size() - 2];
        const auto& curr = chapters_.back();
        return prev.emotional_tone.valence < -0.2f
            && curr.emotional_tone.valence >  0.2f;
    }

    /**
     * @brief Contamination arc: previous chapter positive, current chapter negative.
     *
     * McAdams: "a good or positive event is followed by a bad or negative outcome."
     */
    [[nodiscard]] bool is_contamination_arc() const noexcept {
        if (chapters_.size() < 2) return false;
        const auto& prev = chapters_[chapters_.size() - 2];
        const auto& curr = chapters_.back();
        return prev.emotional_tone.valence >  0.2f
            && curr.emotional_tone.valence < -0.2f;
    }

    // ---- Configuration -----------------------------------------------------

    void set_window_size(size_t ticks) { window_size_ = ticks; }

private:
    std::vector<NarrativeChapter> chapters_;
    size_t window_size_{50};  ///< Sliding window for detecting valence shifts

    /// Timestamped valence entry for the sliding window
    struct ValenceEntry {
        uint64_t tick;
        ValenceSignature valence;
    };
    std::vector<ValenceEntry> valence_window_;

    /// Per-theme occurrence counts (updated when chapters are created)
    std::array<size_t, NARRATIVE_THEME_COUNT> theme_counts_{};

    // ---- Internal helpers --------------------------------------------------

    /// Prune valence_window_ to at most 2 * window_size_ entries
    void prune_window() {
        size_t max_entries = window_size_ * 2;
        if (valence_window_.size() > max_entries) {
            valence_window_.erase(
                valence_window_.begin(),
                valence_window_.begin()
                    + static_cast<ptrdiff_t>(valence_window_.size() - max_entries));
        }
    }

    /**
     * @brief Detect a chapter boundary from a significant valence shift.
     *
     * A boundary is triggered when the current valence differs from the
     * window average by more than 0.3 on either the valence or arousal axis.
     */
    [[nodiscard]] bool detect_boundary(const ValenceSignature& current) const {
        if (valence_window_.size() < 2) return false;

        // Compute window average over the most recent window_size_ entries
        float sum_v = 0.0f;
        float sum_a = 0.0f;
        size_t count = 0;

        size_t start = 0;
        if (valence_window_.size() > window_size_) {
            start = valence_window_.size() - window_size_;
        }

        for (size_t i = start; i < valence_window_.size(); ++i) {
            sum_v += valence_window_[i].valence.valence;
            sum_a += valence_window_[i].valence.arousal;
            ++count;
        }

        if (count == 0) return false;

        float avg_v = sum_v / static_cast<float>(count);
        float avg_a = sum_a / static_cast<float>(count);

        bool valence_shift = std::abs(current.valence - avg_v) > 0.3f;
        bool arousal_shift = std::abs(current.arousal - avg_a) > 0.3f;

        return valence_shift || arousal_shift;
    }

    /**
     * @brief Classify theme from a chapter's accumulated emotional tone.
     *
     * Mapping (based on Russell's circumplex quadrants):
     *   valence > 0.3 AND arousal > 0.5  -> AGENCY     (active positive)
     *   valence > 0.3 AND arousal < 0.3  -> COMMUNION   (calm positive)
     *   valence < -0.3 AND arousal > 0.5 -> PROTECTION  (stressed negative)
     *   valence < -0.3 AND arousal < 0.3 -> STABILITY   (withdrawn)
     *   |valence| <= 0.3 AND arousal > 0.6 -> EXPLORATION (high arousal neutral)
     *   otherwise                           -> GROWTH     (moderate trajectory)
     */
    [[nodiscard]] NarrativeTheme classify_theme(const NarrativeChapter& chapter) const {
        return classify_theme_from_valence(chapter.emotional_tone);
    }

    [[nodiscard]] static NarrativeTheme classify_theme_from_valence(
            const ValenceSignature& tone) noexcept {
        if (tone.valence > 0.3f && tone.arousal > 0.5f)
            return NarrativeTheme::AGENCY;
        if (tone.valence > 0.3f && tone.arousal < 0.3f)
            return NarrativeTheme::COMMUNION;
        if (tone.valence < -0.3f && tone.arousal > 0.5f)
            return NarrativeTheme::PROTECTION;
        if (tone.valence < -0.3f && tone.arousal < 0.3f)
            return NarrativeTheme::STABILITY;
        if (std::abs(tone.valence) <= 0.3f && tone.arousal > 0.6f)
            return NarrativeTheme::EXPLORATION;
        return NarrativeTheme::GROWTH;
    }

    /// Open a new chapter and update theme counts
    void start_new_chapter(uint64_t tick, NarrativeTheme theme,
                           const ValenceSignature& tone) {
        NarrativeChapter ch;
        ch.start_tick = tick;
        ch.end_tick = 0;  // open
        ch.dominant_theme = theme;
        ch.emotional_tone = tone;
        ch.coherence = 0.5f;
        chapters_.push_back(std::move(ch));

        // Update theme frequency table
        auto idx = static_cast<size_t>(theme);
        if (idx < NARRATIVE_THEME_COUNT) {
            ++theme_counts_[idx];
        }
    }
};

} // namespace opencog::entelechy

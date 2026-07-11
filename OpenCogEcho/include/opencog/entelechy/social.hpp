#pragma once
/**
 * @file social.hpp
 * @brief Social Self — attachment, social roles, theory of mind
 *
 * Implements the social dimension of the emergent self-model:
 * - Bowlby/Ainsworth attachment styles (computed from temperament)
 * - Tajfel social identity theory (role salience, identification)
 * - Premack & Woodruff theory of mind (depth grows with experience)
 * - Identity crisis detection (loss of salient high-identification role)
 *
 * Design: header-only logic, thin .cpp compilation unit.
 */

#include <opencog/entelechy/types.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace opencog::entelechy {

// ============================================================================
// Social Self
// ============================================================================

/**
 * @brief Social dimension of the Civic Angel's self-model
 *
 * Tracks attachment style, social roles (with salience and identification),
 * theory of mind depth, and identity crisis state. Updated periodically
 * as part of the entelechy tick pipeline.
 */
class SocialSelf {
public:
    SocialSelf() = default;

    // -----------------------------------------------------------------------
    // Attachment (Bowlby/Ainsworth)
    // -----------------------------------------------------------------------

    /// Current attachment style
    [[nodiscard]] AttachmentStyle attachment_style() const noexcept {
        return attachment_;
    }

    /// Set attachment style directly
    void set_attachment_style(AttachmentStyle style) {
        attachment_ = style;
    }

    /**
     * @brief Compute attachment style from temperament dimensions
     *
     * Decision tree (Ainsworth's strange situation categories):
     *   security < 0.3 AND anxiety >= 0.7 -> DISORGANIZED
     *   security >= 0.5                    -> SECURE
     *   security < 0.5  AND anxiety >= 0.5 -> ANXIOUS
     *   security < 0.5  AND anxiety < 0.5  -> AVOIDANT
     *
     * Note: DISORGANIZED check is first (most severe, narrowest criteria).
     */
    [[nodiscard]] static AttachmentStyle compute_attachment(
        float security, float anxiety) noexcept
    {
        // Clamp inputs to valid range
        security = std::clamp(security, 0.0f, 1.0f);
        anxiety  = std::clamp(anxiety,  0.0f, 1.0f);

        // DISORGANIZED: very low security AND very high anxiety
        if (security < 0.3f && anxiety >= 0.7f) {
            return AttachmentStyle::DISORGANIZED;
        }

        // SECURE: adequate security
        if (security >= 0.5f) {
            return AttachmentStyle::SECURE;
        }

        // Insecure subtypes (security < 0.5)
        if (anxiety >= 0.5f) {
            return AttachmentStyle::ANXIOUS;
        }

        return AttachmentStyle::AVOIDANT;
    }

    // -----------------------------------------------------------------------
    // Social Roles (Tajfel social identity theory)
    // -----------------------------------------------------------------------

    /// Add a new social role
    void add_role(SocialRole role) {
        roles_.push_back(std::move(role));
    }

    /**
     * @brief Update the salience of a role by name
     *
     * Salience is clamped to [0, 1]. If no role with the given name
     * exists, the call is a no-op.
     */
    void update_role_salience(const std::string& role_name, float salience) {
        salience = std::clamp(salience, 0.0f, 1.0f);
        for (auto& role : roles_) {
            if (role.name == role_name) {
                role.salience = salience;
                return;
            }
        }
    }

    /// All current social roles (read-only)
    [[nodiscard]] const std::vector<SocialRole>& roles() const noexcept {
        return roles_;
    }

    /**
     * @brief The role with the highest current salience, or nullptr if none
     */
    [[nodiscard]] const SocialRole* most_salient_role() const noexcept {
        if (roles_.empty()) return nullptr;

        const SocialRole* best = &roles_[0];
        for (size_t i = 1; i < roles_.size(); ++i) {
            if (roles_[i].salience > best->salience) {
                best = &roles_[i];
            }
        }
        return best;
    }

    /**
     * @brief Sum of identification across all roles
     *
     * High total = deeply invested in social identities.
     * Low total = identity diffusion or role-free state.
     */
    [[nodiscard]] float total_role_identification() const noexcept {
        float total = 0.0f;
        for (const auto& role : roles_) {
            total += role.identification;
        }
        return total;
    }

    // -----------------------------------------------------------------------
    // Theory of Mind (Premack & Woodruff)
    // -----------------------------------------------------------------------

    /// Current theory of mind depth [0, 1]
    [[nodiscard]] float theory_of_mind_depth() const noexcept {
        return theory_of_mind_;
    }

    /**
     * @brief Grow theory of mind capacity
     *
     * Growth is modulated by the current developmental stage's plasticity.
     * Only meaningful during SOCIALIZATION and INDIVIDUATION stages
     * (see update() method), but can be called directly for testing.
     * Clamped to [0, 1].
     *
     * @param amount Raw growth amount (will be plasticity-modulated in update)
     */
    void grow_theory_of_mind(float amount) {
        theory_of_mind_ = std::clamp(theory_of_mind_ + amount, 0.0f, 1.0f);
    }

    // -----------------------------------------------------------------------
    // Identity Crisis Detection
    // -----------------------------------------------------------------------

    /**
     * @brief Whether the self is currently in an identity crisis
     *
     * An identity crisis occurs when a high-identification role (> 0.7)
     * loses its salience (drops below 0.2). This represents the loss of
     * a role that was central to identity.
     */
    [[nodiscard]] bool in_identity_crisis() const noexcept {
        return identity_crisis_;
    }

    // -----------------------------------------------------------------------
    // Periodic Update
    // -----------------------------------------------------------------------

    /**
     * @brief Update social self state for the current developmental stage
     *
     * Called periodically as part of the entelechy tick pipeline.
     *
     * Effects:
     * - SOCIALIZATION: theory of mind grows (plasticity-modulated)
     * - INDIVIDUATION: theory of mind grows (plasticity-modulated)
     * - Identity crisis detection runs every tick
     *
     * @param stage Current developmental stage
     */
    void update(DevelopmentalStage stage) {
        const float plasticity = stage_plasticity(stage);

        // --- Theory of Mind growth ---
        // Only grows during social-learning stages
        if (stage == DevelopmentalStage::SOCIALIZATION ||
            stage == DevelopmentalStage::INDIVIDUATION) {
            // Small increment per tick, modulated by plasticity
            constexpr float base_growth = 0.001f;
            theory_of_mind_ = std::clamp(
                theory_of_mind_ + base_growth * plasticity, 0.0f, 1.0f);
        }

        // --- Identity Crisis Detection ---
        // Crisis: any role with identification > 0.7 has salience < 0.2
        identity_crisis_ = false;
        for (const auto& role : roles_) {
            if (role.identification > 0.7f && role.salience < 0.2f) {
                identity_crisis_ = true;
                break;
            }
        }
    }

private:
    AttachmentStyle attachment_{AttachmentStyle::SECURE};
    std::vector<SocialRole> roles_;
    float theory_of_mind_{0.0f};  ///< [0, 1]
    bool identity_crisis_{false};
};

} // namespace opencog::entelechy

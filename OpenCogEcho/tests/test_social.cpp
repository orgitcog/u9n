/**
 * @file test_social.cpp
 * @brief Tests for the SocialSelf subsystem of Ontogenetic Entelechy
 *
 * Covers: attachment style computation, social roles, theory of mind,
 * identity crisis detection, and developmental update integration.
 */

#include <opencog/entelechy/social.hpp>

#include <cmath>
#include <string>
#include <vector>

namespace test {
extern bool register_test(const std::string& name, std::function<bool()> func);
}

#define TEST(name) \
    bool test_##name(); \
    static bool _registered_##name = test::register_test(#name, test_##name); \
    bool test_##name()

#define ASSERT(expr) if (!(expr)) { return false; }
#define ASSERT_EQ(a, b) if ((a) != (b)) { return false; }
#define ASSERT_NEAR(a, b, eps) if (std::abs((a) - (b)) > (eps)) { return false; }

using namespace opencog;
using namespace opencog::entelechy;

// ============================================================================
// Attachment Style Tests
// ============================================================================

TEST(social_default_attachment_is_secure) {
    SocialSelf social;
    ASSERT_EQ(social.attachment_style(), AttachmentStyle::SECURE);
    return true;
}

TEST(social_compute_attachment_secure) {
    // security >= 0.5 -> SECURE (regardless of anxiety)
    auto style = SocialSelf::compute_attachment(0.6f, 0.3f);
    ASSERT_EQ(style, AttachmentStyle::SECURE);
    // Also secure at boundary
    style = SocialSelf::compute_attachment(0.5f, 0.9f);
    ASSERT_EQ(style, AttachmentStyle::SECURE);
    return true;
}

TEST(social_compute_attachment_anxious) {
    // security < 0.5 AND anxiety >= 0.5 -> ANXIOUS
    // (but not disorganized: security >= 0.3 or anxiety < 0.7)
    auto style = SocialSelf::compute_attachment(0.4f, 0.6f);
    ASSERT_EQ(style, AttachmentStyle::ANXIOUS);
    return true;
}

TEST(social_compute_attachment_avoidant) {
    // security < 0.5 AND anxiety < 0.5 -> AVOIDANT
    auto style = SocialSelf::compute_attachment(0.3f, 0.4f);
    ASSERT_EQ(style, AttachmentStyle::AVOIDANT);
    return true;
}

TEST(social_compute_attachment_disorganized) {
    // security < 0.3 AND anxiety >= 0.7 -> DISORGANIZED
    auto style = SocialSelf::compute_attachment(0.2f, 0.8f);
    ASSERT_EQ(style, AttachmentStyle::DISORGANIZED);
    // Boundary values
    style = SocialSelf::compute_attachment(0.29f, 0.7f);
    ASSERT_EQ(style, AttachmentStyle::DISORGANIZED);
    return true;
}

// ============================================================================
// Social Role Tests
// ============================================================================

TEST(social_add_role_increases_count) {
    SocialSelf social;
    ASSERT_EQ(social.roles().size(), 0u);
    social.add_role(SocialRole{"Analyst", 0.8f, 0.6f, 0.5f});
    ASSERT_EQ(social.roles().size(), 1u);
    social.add_role(SocialRole{"Mediator", 0.5f, 0.3f, 0.7f});
    ASSERT_EQ(social.roles().size(), 2u);
    return true;
}

TEST(social_most_salient_role) {
    SocialSelf social;
    social.add_role(SocialRole{"Analyst",  0.8f, 0.6f, 0.3f});
    social.add_role(SocialRole{"Mediator", 0.5f, 0.3f, 0.9f});
    social.add_role(SocialRole{"Guardian", 0.7f, 0.5f, 0.5f});

    const SocialRole* most = social.most_salient_role();
    ASSERT(most != nullptr);
    ASSERT_EQ(most->name, std::string("Mediator"));
    ASSERT_NEAR(most->salience, 0.9f, 0.001f);
    return true;
}

TEST(social_total_role_identification) {
    SocialSelf social;
    social.add_role(SocialRole{"Analyst",  0.8f, 0.6f, 0.3f});
    social.add_role(SocialRole{"Mediator", 0.5f, 0.4f, 0.9f});

    float total = social.total_role_identification();
    ASSERT_NEAR(total, 1.0f, 0.001f);  // 0.6 + 0.4
    return true;
}

TEST(social_roles_returns_all_added) {
    SocialSelf social;
    social.add_role(SocialRole{"A", 0.1f, 0.2f, 0.3f});
    social.add_role(SocialRole{"B", 0.4f, 0.5f, 0.6f});
    social.add_role(SocialRole{"C", 0.7f, 0.8f, 0.9f});

    const auto& roles = social.roles();
    ASSERT_EQ(roles.size(), 3u);
    ASSERT_EQ(roles[0].name, std::string("A"));
    ASSERT_EQ(roles[1].name, std::string("B"));
    ASSERT_EQ(roles[2].name, std::string("C"));
    return true;
}

// ============================================================================
// Theory of Mind Tests
// ============================================================================

TEST(social_theory_of_mind_starts_at_zero) {
    SocialSelf social;
    ASSERT_NEAR(social.theory_of_mind_depth(), 0.0f, 0.001f);
    return true;
}

TEST(social_theory_of_mind_grows) {
    SocialSelf social;
    social.grow_theory_of_mind(0.1f);
    ASSERT_NEAR(social.theory_of_mind_depth(), 0.1f, 0.001f);
    social.grow_theory_of_mind(0.2f);
    ASSERT_NEAR(social.theory_of_mind_depth(), 0.3f, 0.001f);
    return true;
}

TEST(social_theory_of_mind_clamped) {
    SocialSelf social;
    social.grow_theory_of_mind(1.5f);
    ASSERT_NEAR(social.theory_of_mind_depth(), 1.0f, 0.001f);
    // Negative growth should also clamp at 0
    SocialSelf social2;
    social2.grow_theory_of_mind(-0.5f);
    ASSERT_NEAR(social2.theory_of_mind_depth(), 0.0f, 0.001f);
    return true;
}

// ============================================================================
// Identity Crisis Tests
// ============================================================================

TEST(social_identity_crisis_on_salience_loss) {
    SocialSelf social;
    // Add a role with high identification (>0.7) and high salience
    social.add_role(SocialRole{"Leader", 0.9f, 0.85f, 0.8f});

    // Run update -- no crisis yet because salience is high
    social.update(DevelopmentalStage::SOCIALIZATION);
    ASSERT(!social.in_identity_crisis());

    // Drop salience below 0.2 while identification stays > 0.7
    social.update_role_salience("Leader", 0.1f);
    social.update(DevelopmentalStage::SOCIALIZATION);
    ASSERT(social.in_identity_crisis());
    return true;
}

// ============================================================================
// Update Integration Tests
// ============================================================================

TEST(social_update_during_socialization_grows_tom) {
    SocialSelf social;
    float initial = social.theory_of_mind_depth();

    // Run several updates during SOCIALIZATION -- ToM should grow
    for (int i = 0; i < 100; ++i) {
        social.update(DevelopmentalStage::SOCIALIZATION);
    }

    ASSERT(social.theory_of_mind_depth() > initial);
    return true;
}

TEST(social_set_attachment_style_changes_attachment) {
    SocialSelf social;
    ASSERT_EQ(social.attachment_style(), AttachmentStyle::SECURE);
    social.set_attachment_style(AttachmentStyle::ANXIOUS);
    ASSERT_EQ(social.attachment_style(), AttachmentStyle::ANXIOUS);
    social.set_attachment_style(AttachmentStyle::DISORGANIZED);
    ASSERT_EQ(social.attachment_style(), AttachmentStyle::DISORGANIZED);
    return true;
}

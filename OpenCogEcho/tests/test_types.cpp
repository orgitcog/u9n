/**
 * @file test_types.cpp
 * @brief Tests for core types
 */

#include <opencog/core/types.hpp>
#include <cmath>
#include <functional>
#include <string>

namespace test {
extern bool register_test(const std::string& name, std::function<bool()> func);
}

#define TEST(name) \
    bool test_##name(); \
    static bool _registered_##name = test::register_test(#name, test_##name); \
    bool test_##name()

#define ASSERT(expr) if (!(expr)) { return false; }
#define ASSERT_EQ(a, b) if ((a) != (b)) { return false; }
#define ASSERT_NE(a, b) if ((a) == (b)) { return false; }
#define ASSERT_NEAR(a, b, eps) if (std::abs((a) - (b)) > (eps)) { return false; }

using namespace opencog;

TEST(AtomId_default_invalid) {
    AtomId id;
    ASSERT(!id.valid());
    ASSERT(!id);
    return true;
}

TEST(AtomId_explicit_valid) {
    AtomId id{42};
    ASSERT(id.valid());
    ASSERT(id);
    ASSERT_EQ(id.value, 42u);
    return true;
}

TEST(AtomId_make_with_generation) {
    AtomId id = AtomId::make(1000, 5);
    ASSERT_EQ(id.index(), 1000u);
    ASSERT_EQ(id.generation(), 5);
    return true;
}

TEST(AtomId_comparison) {
    AtomId a{10};
    AtomId b{20};
    AtomId c{10};

    ASSERT(a == c);
    ASSERT(a != b);
    ASSERT(a < b);
    return true;
}

TEST(TruthValue_default) {
    TruthValue tv;
    ASSERT_NEAR(tv.strength, 0.0f, 0.0001f);
    ASSERT_NEAR(tv.confidence, 0.0f, 0.0001f);
    return true;
}

TEST(TruthValue_construction) {
    TruthValue tv{0.8f, 0.9f};
    ASSERT_NEAR(tv.strength, 0.8f, 0.0001f);
    ASSERT_NEAR(tv.confidence, 0.9f, 0.0001f);
    return true;
}

TEST(TruthValue_simple) {
    TruthValue tv = TruthValue::simple(0.7f);
    ASSERT_NEAR(tv.strength, 0.7f, 0.0001f);
    ASSERT_NEAR(tv.confidence, 0.9f, 0.0001f);
    return true;
}

TEST(TruthValue_is_true) {
    TruthValue high{0.8f, 0.9f};
    TruthValue low{0.2f, 0.9f};

    ASSERT(high.is_true());
    ASSERT(!low.is_true());
    ASSERT(low.is_true(0.1f));  // With lower threshold
    return true;
}

TEST(TruthValue_count_conversion) {
    TruthValue tv{0.7f, 0.5f};
    float count = tv.count();
    ASSERT(count > 0.0f);

    TruthValue reconstructed = TruthValue::from_count(0.7f, count);
    ASSERT_NEAR(reconstructed.strength, tv.strength, 0.01f);
    ASSERT_NEAR(reconstructed.confidence, tv.confidence, 0.01f);
    return true;
}

TEST(AttentionValue_default) {
    AttentionValue av;
    ASSERT_NEAR(av.sti, 0.0f, 0.0001f);
    ASSERT_EQ(av.lti, 0);
    ASSERT_EQ(av.vlti, 0);
    return true;
}

TEST(AttentionValue_disposable) {
    AttentionValue av = AttentionValue::disposable();
    ASSERT(av.is_disposable());
    ASSERT(!av.in_attentional_focus());
    return true;
}

TEST(AtomType_is_node) {
    ASSERT(is_node(AtomType::CONCEPT_NODE));
    ASSERT(is_node(AtomType::VARIABLE_NODE));
    ASSERT(!is_node(AtomType::INHERITANCE_LINK));
    ASSERT(!is_node(AtomType::AND_LINK));
    return true;
}

TEST(AtomType_is_link) {
    ASSERT(!is_link(AtomType::CONCEPT_NODE));
    ASSERT(is_link(AtomType::INHERITANCE_LINK));
    ASSERT(is_link(AtomType::AND_LINK));
    ASSERT(is_link(AtomType::IMPLICATION_LINK));
    return true;
}

TEST(type_name_lookup) {
    ASSERT_EQ(type_name(AtomType::CONCEPT_NODE), "ConceptNode");
    ASSERT_EQ(type_name(AtomType::INHERITANCE_LINK), "InheritanceLink");
    ASSERT_EQ(type_name(AtomType::AND_LINK), "AndLink");
    return true;
}

TEST(type_from_name_lookup) {
    ASSERT_EQ(type_from_name("ConceptNode"), AtomType::CONCEPT_NODE);
    ASSERT_EQ(type_from_name("InheritanceLink"), AtomType::INHERITANCE_LINK);
    ASSERT_EQ(type_from_name("Unknown"), AtomType::INVALID);
    return true;
}

TEST(hash_combine) {
    uint64_t h1 = hash_combine(0, 42);
    uint64_t h2 = hash_combine(0, 43);
    uint64_t h3 = hash_combine(0, 42);

    ASSERT_EQ(h1, h3);  // Same inputs produce same hash
    ASSERT_NE(h1, h2);  // Different inputs produce different hash
    return true;
}

TEST(AtomHeader_flags) {
    AtomHeader header;
    ASSERT(!header.has_flag(AtomHeader::FLAG_CHECKED));

    header.set_flag(AtomHeader::FLAG_CHECKED);
    ASSERT(header.has_flag(AtomHeader::FLAG_CHECKED));

    header.clear_flag(AtomHeader::FLAG_CHECKED);
    ASSERT(!header.has_flag(AtomHeader::FLAG_CHECKED));
    return true;
}

TEST(Handle_default_invalid) {
    Handle h;
    ASSERT(!h.valid());
    ASSERT(!h);
    return true;
}

TEST(sizes_are_correct) {
    // Verify our compact types are actually compact
    ASSERT_EQ(sizeof(AtomId), 8u);
    ASSERT_EQ(sizeof(TruthValue), 8u);
    ASSERT_EQ(sizeof(AttentionValue), 8u);
    ASSERT_EQ(sizeof(AtomHeader), 16u);
    ASSERT_EQ(sizeof(Handle), 16u);
    return true;
}

TEST(layer2_reserved_type_ranges) {
    // Reserved layer-2 node types (10000-10499) must classify as nodes
    ASSERT(is_node(AtomType::HORMONE_NODE));
    ASSERT(!is_link(AtomType::HORMONE_NODE));
    ASSERT(is_node(AtomType::PREDICTION_ERROR_NODE));
    ASSERT(!is_link(AtomType::PREDICTION_ERROR_NODE));

    // Reserved layer-2 link types (10500+) must classify as links
    ASSERT(is_link(AtomType::VALENCE_LINK));
    ASSERT(!is_node(AtomType::VALENCE_LINK));
    ASSERT(is_link(AtomType::PREDICTION_ERROR_LINK));
    ASSERT(!is_node(AtomType::PREDICTION_ERROR_LINK));

    // Core ranges unchanged
    ASSERT(is_node(AtomType::CONCEPT_NODE));
    ASSERT(is_link(AtomType::INHERITANCE_LINK));
    return true;
}

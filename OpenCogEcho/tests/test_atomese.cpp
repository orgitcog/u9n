/**
 * @file test_atomese.cpp
 * @brief Tests for the Atomese S-expression parser and loader
 */

#include <opencog/atomese/loader.hpp>
#include <opencog/core/types.hpp>
#include <opencog/atomspace/atomspace.hpp>

// Use the same test framework as the rest of the project
extern bool register_test(const char* name, bool(*fn)());
#define TEST(name) \
    bool test_##name(); \
    static bool _registered_##name = register_test(#name, test_##name); \
    bool test_##name()
#define ASSERT(expr) if (!(expr)) { return false; }
#define ASSERT_EQ(a, b) if ((a) != (b)) { return false; }
#define ASSERT_NE(a, b) if ((a) == (b)) { return false; }
#define ASSERT_GT(a, b) if (!((a) > (b))) { return false; }
#define ASSERT_GE(a, b) if (!((a) >= (b))) { return false; }

using namespace opencog;
using namespace opencog::atomese;

// ============================================================================
// Parser Tests
// ============================================================================

TEST(atomese_parse_empty) {
    auto exprs = parse("");
    ASSERT_EQ(exprs.size(), 0u);
    return true;
}

TEST(atomese_parse_comment_only) {
    auto exprs = parse("; This is a comment\n; Another comment");
    ASSERT_EQ(exprs.size(), 0u);
    return true;
}

TEST(atomese_parse_single_node) {
    auto exprs = parse(R"((ConceptNode "Cat"))");
    ASSERT_EQ(exprs.size(), 1u);
    ASSERT(exprs[0].is_list);
    ASSERT_EQ(exprs[0].children.size(), 2u);
    ASSERT_EQ(exprs[0].children[0].atom, "ConceptNode");
    ASSERT_EQ(exprs[0].children[1].atom, "Cat");
    return true;
}

TEST(atomese_parse_nested_link) {
    auto exprs = parse(R"(
        (InheritanceLink
            (ConceptNode "Cat")
            (ConceptNode "Animal"))
    )");
    ASSERT_EQ(exprs.size(), 1u);
    ASSERT(exprs[0].is_list);
    ASSERT_EQ(exprs[0].children.size(), 3u);
    ASSERT_EQ(exprs[0].children[0].atom, "InheritanceLink");
    // children[1] and [2] are sub-lists
    ASSERT(exprs[0].children[1].is_list);
    ASSERT(exprs[0].children[2].is_list);
    return true;
}

TEST(atomese_parse_deeply_nested) {
    auto exprs = parse(R"(
        (EvaluationLink
            (PredicateNode "has-name")
            (ListLink
                (ConceptNode "Pattern-1")
                (ConceptNode "Independent Regions")))
    )");
    ASSERT_EQ(exprs.size(), 1u);
    auto& eval = exprs[0];
    ASSERT_EQ(eval.children.size(), 3u);
    ASSERT_EQ(eval.children[0].atom, "EvaluationLink");
    // children[2] is the ListLink
    auto& list_link = eval.children[2];
    ASSERT(list_link.is_list);
    ASSERT_EQ(list_link.children.size(), 3u); // ListLink + 2 ConceptNodes
    return true;
}

TEST(atomese_parse_multiple_expressions) {
    auto exprs = parse(R"(
        (ConceptNode "A")
        (ConceptNode "B")
        (InheritanceLink
            (ConceptNode "A")
            (ConceptNode "B"))
    )");
    ASSERT_EQ(exprs.size(), 3u);
    return true;
}

TEST(atomese_parse_with_comments) {
    auto exprs = parse(R"(
        ; This is a pattern
        (ConceptNode "Pattern-1")
        ; This links them
        (ImplicationLink
            (ConceptNode "Pattern-0")
            (ConceptNode "Pattern-1"))
    )");
    ASSERT_EQ(exprs.size(), 2u);
    return true;
}

TEST(atomese_parse_string_with_special_chars) {
    auto exprs = parse(R"((ConceptNode "Hello \"world\" with\nnewline"))");
    ASSERT_EQ(exprs.size(), 1u);
    ASSERT_EQ(exprs[0].children[1].atom, "Hello \"world\" with\nnewline");
    return true;
}

// ============================================================================
// Loader Tests — Basic Nodes
// ============================================================================

TEST(atomese_load_single_concept) {
    AtomSpace as;
    auto result = load_string(as, R"((ConceptNode "Cat"))");
    ASSERT(result.ok());
    ASSERT_EQ(result.nodes_created, 1u);
    ASSERT_EQ(result.links_created, 0u);
    ASSERT_EQ(as.size(), 1u);

    auto h = as.get_node(AtomType::CONCEPT_NODE, "Cat");
    ASSERT(h.valid());
    return true;
}

TEST(atomese_load_predicate_node) {
    AtomSpace as;
    auto result = load_string(as, R"((PredicateNode "has-name"))");
    ASSERT(result.ok());
    ASSERT_EQ(result.nodes_created, 1u);

    auto h = as.get_node(AtomType::PREDICATE_NODE, "has-name");
    ASSERT(h.valid());
    return true;
}

TEST(atomese_load_duplicate_node_idempotent) {
    AtomSpace as;
    auto result = load_string(as, R"(
        (ConceptNode "Cat")
        (ConceptNode "Cat")
        (ConceptNode "Cat")
    )");
    ASSERT(result.ok());
    // nodes_created counts each add_node call, but AtomSpace deduplicates
    ASSERT_EQ(as.size(), 1u);
    return true;
}

// ============================================================================
// Loader Tests — Links
// ============================================================================

TEST(atomese_load_inheritance_link) {
    AtomSpace as;
    auto result = load_string(as, R"(
        (InheritanceLink
            (ConceptNode "Cat")
            (ConceptNode "Animal"))
    )");
    ASSERT(result.ok());
    ASSERT_GE(result.nodes_created, 2u);
    ASSERT_EQ(result.links_created, 1u);

    auto cat = as.get_node(AtomType::CONCEPT_NODE, "Cat");
    auto animal = as.get_node(AtomType::CONCEPT_NODE, "Animal");
    ASSERT(cat.valid());
    ASSERT(animal.valid());

    auto link = as.get_link(AtomType::INHERITANCE_LINK, {cat, animal});
    ASSERT(link.valid());
    return true;
}

TEST(atomese_load_evaluation_link) {
    AtomSpace as;
    auto result = load_string(as, R"(
        (EvaluationLink
            (PredicateNode "has-name")
            (ListLink
                (ConceptNode "Pattern-1")
                (ConceptNode "Independent Regions")))
    )");
    ASSERT(result.ok());
    ASSERT_GE(result.links_created, 2u); // EvaluationLink + ListLink
    ASSERT_GE(result.nodes_created, 3u); // PredicateNode + 2 ConceptNodes
    return true;
}

TEST(atomese_load_implication_link) {
    AtomSpace as;
    auto result = load_string(as, R"(
        (ImplicationLink
            (ConceptNode "Pattern-0")
            (ConceptNode "Pattern-1"))
    )");
    ASSERT(result.ok());
    ASSERT_EQ(result.links_created, 1u);

    auto p0 = as.get_node(AtomType::CONCEPT_NODE, "Pattern-0");
    auto p1 = as.get_node(AtomType::CONCEPT_NODE, "Pattern-1");
    ASSERT(p0.valid());
    ASSERT(p1.valid());

    auto link = as.get_link(AtomType::IMPLICATION_LINK, {p0, p1});
    ASSERT(link.valid());
    return true;
}

TEST(atomese_load_member_link) {
    AtomSpace as;
    auto result = load_string(as, R"(
        (MemberLink
            (ConceptNode "Pattern-1")
            (ConceptNode "Sequence-1"))
    )");
    ASSERT(result.ok());
    ASSERT_EQ(result.links_created, 1u);
    return true;
}

// ============================================================================
// Loader Tests — Real Pattern Data
// ============================================================================

TEST(atomese_load_pattern_block) {
    AtomSpace as;
    // Simulates a real pattern from pattern_language.scm
    auto result = load_string(as, R"(
        ; Pattern 0: Pattern Language
        (ConceptNode "Pattern-0-Pattern Language")

        (EvaluationLink
          (PredicateNode "has-number")
          (ListLink
            (ConceptNode "Pattern-0-Pattern Language")
            (ConceptNode "0")))

        (EvaluationLink
          (PredicateNode "has-name")
          (ListLink
            (ConceptNode "Pattern-0-Pattern Language")
            (ConceptNode "Pattern Language")))

        (EvaluationLink
          (PredicateNode "has-evidence-level")
          (ListLink
            (ConceptNode "Pattern-0-Pattern Language")
            (ConceptNode "2")))

        (ImplicationLink
          (ConceptNode "Pattern-0-Pattern Language")
          (ConceptNode "Pattern-1"))

        (ImplicationLink
          (ConceptNode "Pattern-0-Pattern Language")
          (ConceptNode "Pattern-2"))
    )");
    ASSERT(result.ok());
    ASSERT_EQ(result.expressions_parsed, 6u);
    ASSERT_GT(result.links_created, 0u);

    // Verify pattern node exists
    auto pattern0 = as.get_node(AtomType::CONCEPT_NODE, "Pattern-0-Pattern Language");
    ASSERT(pattern0.valid());

    // Verify implication links
    auto p1 = as.get_node(AtomType::CONCEPT_NODE, "Pattern-1");
    auto p2 = as.get_node(AtomType::CONCEPT_NODE, "Pattern-2");
    ASSERT(p1.valid());
    ASSERT(p2.valid());

    auto imp1 = as.get_link(AtomType::IMPLICATION_LINK, {pattern0, p1});
    auto imp2 = as.get_link(AtomType::IMPLICATION_LINK, {pattern0, p2});
    ASSERT(imp1.valid());
    ASSERT(imp2.valid());
    return true;
}

TEST(atomese_load_relationship_types) {
    AtomSpace as;
    auto result = load_string(as, R"(
        (ConceptNode "RelationType-Dependency")
        (ConceptNode "RelationType-Complement")

        (EvaluationLink
          (PredicateNode "has-description")
          (ListLink
            (ConceptNode "RelationType-Dependency")
            (ConceptNode "One pattern must precede or follow another")))

        (EvaluationLink
          (PredicateNode "has-relationship")
          (ListLink
            (ConceptNode "Pattern-1")
            (ConceptNode "Pattern-7")
            (ConceptNode "RelationType-Complement")))
    )");
    ASSERT(result.ok());

    // Verify relationship evaluation exists
    auto pred = as.get_node(AtomType::PREDICATE_NODE, "has-relationship");
    ASSERT(pred.valid());

    // Verify 3-ary ListLink for the relationship
    auto p1 = as.get_node(AtomType::CONCEPT_NODE, "Pattern-1");
    auto p7 = as.get_node(AtomType::CONCEPT_NODE, "Pattern-7");
    auto comp = as.get_node(AtomType::CONCEPT_NODE, "RelationType-Complement");
    ASSERT(p1.valid());
    ASSERT(p7.valid());
    ASSERT(comp.valid());
    return true;
}

TEST(atomese_load_category_and_inheritance) {
    AtomSpace as;
    auto result = load_string(as, R"(
        (ConceptNode "Category-Towns")
        (ConceptNode "Pattern-1-Independent Regions")

        (InheritanceLink
            (ConceptNode "Pattern-1-Independent Regions")
            (ConceptNode "Category-Towns"))

        (InheritanceLink
            (ConceptNode "Pattern-2-Distribution of Towns")
            (ConceptNode "Category-Towns"))
    )");
    ASSERT(result.ok());

    auto towns = as.get_node(AtomType::CONCEPT_NODE, "Category-Towns");
    ASSERT(towns.valid());

    // Check that both patterns inherit from Category-Towns
    auto incoming = as.get_incoming_by_type(towns, AtomType::INHERITANCE_LINK);
    ASSERT_EQ(incoming.size(), 2u);
    return true;
}

// ============================================================================
// Loader Tests — Error Handling
// ============================================================================

TEST(atomese_load_unknown_type) {
    AtomSpace as;
    auto result = load_string(as, R"((FooBarNode "test"))");
    ASSERT(!result.ok());
    ASSERT_EQ(result.errors, 1u);
    ASSERT_GT(result.error_messages.size(), 0u);
    return true;
}

TEST(atomese_load_empty_link) {
    AtomSpace as;
    auto result = load_string(as, R"((InheritanceLink))");
    ASSERT(!result.ok());
    ASSERT_EQ(result.errors, 1u);
    return true;
}

TEST(atomese_load_node_missing_name) {
    AtomSpace as;
    auto result = load_string(as, R"((ConceptNode))");
    ASSERT(!result.ok());
    ASSERT_EQ(result.errors, 1u);
    return true;
}

TEST(atomese_load_partial_errors) {
    AtomSpace as;
    // Mix of valid and invalid expressions
    auto result = load_string(as, R"(
        (ConceptNode "Valid")
        (BogusNode "Invalid")
        (ConceptNode "AlsoValid")
    )");
    ASSERT_EQ(result.errors, 1u);
    ASSERT_EQ(result.expressions_parsed, 3u);
    // The two valid nodes should still be loaded
    ASSERT(as.get_node(AtomType::CONCEPT_NODE, "Valid").valid());
    ASSERT(as.get_node(AtomType::CONCEPT_NODE, "AlsoValid").valid());
    return true;
}

// ============================================================================
// Loader Tests — Statistics
// ============================================================================

TEST(atomese_load_statistics) {
    AtomSpace as;
    auto result = load_string(as, R"(
        (ConceptNode "A")
        (ConceptNode "B")
        (ConceptNode "C")
        (InheritanceLink
            (ConceptNode "A")
            (ConceptNode "B"))
        (InheritanceLink
            (ConceptNode "B")
            (ConceptNode "C"))
    )");
    ASSERT(result.ok());
    ASSERT_EQ(result.expressions_parsed, 5u);
    ASSERT_EQ(result.links_created, 2u);
    ASSERT_GT(result.total(), 0u);
    return true;
}

TEST(atomese_load_result_merge) {
    LoadResult a;
    a.nodes_created = 10;
    a.links_created = 5;
    a.expressions_parsed = 15;

    LoadResult b;
    b.nodes_created = 3;
    b.links_created = 2;
    b.expressions_parsed = 5;
    b.errors = 1;
    b.error_messages.push_back("test error");

    a.merge(b);
    ASSERT_EQ(a.nodes_created, 13u);
    ASSERT_EQ(a.links_created, 7u);
    ASSERT_EQ(a.expressions_parsed, 20u);
    ASSERT_EQ(a.errors, 1u);
    ASSERT_EQ(a.error_messages.size(), 1u);
    return true;
}

// ============================================================================
// Loader Tests — Edge Cases
// ============================================================================

TEST(atomese_load_long_string_values) {
    AtomSpace as;
    auto result = load_string(as, R"(
        (EvaluationLink
          (PredicateNode "has-solution")
          (ListLink
            (ConceptNode "Pattern-0")
            (ConceptNode "Create a structured language of 253 interconnected patterns organized hierarchically from large-scale regional planning (Towns) to building design (Buildings) to construction details (Construction).")))
    )");
    ASSERT(result.ok());
    // Verify the long string was preserved
    auto sol = as.get_node(AtomType::CONCEPT_NODE,
        "Create a structured language of 253 interconnected patterns organized "
        "hierarchically from large-scale regional planning (Towns) to building "
        "design (Buildings) to construction details (Construction).");
    ASSERT(sol.valid());
    return true;
}

TEST(atomese_load_empty_string) {
    AtomSpace as;
    auto result = load_string(as, "");
    ASSERT(result.ok());
    ASSERT_EQ(result.expressions_parsed, 0u);
    ASSERT_EQ(as.size(), 0u);
    return true;
}

TEST(atomese_load_whitespace_only) {
    AtomSpace as;
    auto result = load_string(as, "   \n\t\n   ");
    ASSERT(result.ok());
    ASSERT_EQ(result.expressions_parsed, 0u);
    return true;
}

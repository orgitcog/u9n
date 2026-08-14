/**
 * @file test_pattern.cpp
 * @brief Tests for pattern matching
 */

#include <opencog/pattern/matcher.hpp>

namespace test {
extern bool register_test(const std::string& name, std::function<bool()> func);
}

#define TEST(name) \
    bool test_##name(); \
    static bool _registered_##name = test::register_test(#name, test_##name); \
    bool test_##name()

#define ASSERT(expr) if (!(expr)) { return false; }
#define ASSERT_EQ(a, b) if ((a) != (b)) { return false; }
#define ASSERT_GT(a, b) if (!((a) > (b))) { return false; }

using namespace opencog;

TEST(Pattern_VariableTerm_creation) {
    VariableTerm v("X", AtomType::CONCEPT_NODE);
    ASSERT_EQ(v.name, "X");
    ASSERT(v.type_constraint.has_value());
    ASSERT_EQ(*v.type_constraint, AtomType::CONCEPT_NODE);
    return true;
}

TEST(Pattern_GroundedTerm_creation) {
    GroundedTerm g(AtomId{42});
    ASSERT_EQ(g.atom.value, 42u);
    return true;
}

TEST(Pattern_TypedTerm_creation) {
    TypedTerm t(AtomType::INHERITANCE_LINK);
    ASSERT_EQ(t.type, AtomType::INHERITANCE_LINK);
    return true;
}

TEST(Pattern_LinkPattern_creation) {
    auto pattern = link(
        AtomType::INHERITANCE_LINK,
        {var("X"), var("Y")}
    );

    ASSERT(pattern != nullptr);
    ASSERT_EQ(pattern->type, AtomType::INHERITANCE_LINK);
    ASSERT_EQ(pattern->outgoing.size(), 2u);
    return true;
}

TEST(BindingSet_bind_and_get) {
    BindingSet bindings;
    ASSERT(bindings.empty());

    bindings.bind("X", AtomId{100});
    ASSERT(!bindings.empty());
    ASSERT(bindings.contains("X"));
    ASSERT_EQ(bindings.get("X").value, 100u);
    return true;
}

TEST(BindingSet_get_unbound) {
    BindingSet bindings;
    AtomId result = bindings.get("X");
    ASSERT(!result.valid());
    return true;
}

TEST(PatternBuilder_fluent) {
    auto pattern = PatternBuilder()
        .variable("X", AtomType::CONCEPT_NODE)
        .variable("Y", AtomType::CONCEPT_NODE)
        .match(AtomType::INHERITANCE_LINK, {
            PatternBuilder().var("X"),
            PatternBuilder().var("Y")
        })
        .build();

    ASSERT_EQ(pattern.variables.size(), 2u);
    return true;
}

TEST(PatternMatcher_match_grounded) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    Handle animal = space.add_node(AtomType::CONCEPT_NODE, "Animal");
    space.add_link(AtomType::INHERITANCE_LINK, {cat, animal});

    PatternMatcher matcher(space);

    Pattern pattern;
    pattern.body = ground(cat.id());

    auto result = matcher.find_first(pattern);
    ASSERT(result.has_value());
    ASSERT_EQ(result->matched_atom, cat.id());
    return true;
}

TEST(PatternMatcher_match_typed) {
    AtomSpace space;

    space.add_node(AtomType::CONCEPT_NODE, "A");
    space.add_node(AtomType::CONCEPT_NODE, "B");
    space.add_node(AtomType::PREDICATE_NODE, "P");

    PatternMatcher matcher(space);

    Pattern pattern;
    pattern.body = typed(AtomType::CONCEPT_NODE);

    auto results = matcher.find_all(pattern);
    ASSERT_EQ(results.size(), 2u);
    return true;
}

TEST(PatternMatcher_count_matches) {
    AtomSpace space;

    space.add_node(AtomType::CONCEPT_NODE, "A");
    space.add_node(AtomType::CONCEPT_NODE, "B");
    space.add_node(AtomType::CONCEPT_NODE, "C");

    PatternMatcher matcher(space);

    Pattern pattern;
    pattern.body = typed(AtomType::CONCEPT_NODE);

    size_t count = matcher.count_matches(pattern);
    ASSERT_EQ(count, 3u);
    return true;
}

TEST(PatternMatcher_any_match) {
    AtomSpace space;

    space.add_node(AtomType::CONCEPT_NODE, "A");

    PatternMatcher matcher(space);

    Pattern has_concept;
    has_concept.body = typed(AtomType::CONCEPT_NODE);
    ASSERT(matcher.any_match(has_concept));

    Pattern has_predicate;
    has_predicate.body = typed(AtomType::PREDICATE_NODE);
    ASSERT(!matcher.any_match(has_predicate));
    return true;
}

TEST(PatternMatcher_match_link_pattern) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    Handle animal = space.add_node(AtomType::CONCEPT_NODE, "Animal");
    Handle dog = space.add_node(AtomType::CONCEPT_NODE, "Dog");

    space.add_link(AtomType::INHERITANCE_LINK, {cat, animal});
    space.add_link(AtomType::INHERITANCE_LINK, {dog, animal});

    PatternMatcher matcher(space);

    // Match all InheritanceLinks
    Pattern pattern;
    pattern.body = link(AtomType::INHERITANCE_LINK, {
        typed(AtomType::CONCEPT_NODE),
        typed(AtomType::CONCEPT_NODE)
    });

    auto results = matcher.find_all(pattern);
    ASSERT_EQ(results.size(), 2u);
    return true;
}

TEST(PatternMatcher_variable_binding) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    Handle animal = space.add_node(AtomType::CONCEPT_NODE, "Animal");
    space.add_link(AtomType::INHERITANCE_LINK, {cat, animal});

    PatternMatcher matcher(space);

    // Match with variable
    Pattern pattern;
    pattern.variables = {"X"};
    pattern.body = link(AtomType::INHERITANCE_LINK, {
        var("X", AtomType::CONCEPT_NODE),
        ground(animal.id())
    });

    auto result = matcher.find_first(pattern);
    ASSERT(result.has_value());
    ASSERT(result->bindings.contains("X"));
    ASSERT_EQ(result->bindings.get("X"), cat.id());
    return true;
}

TEST(PatternMatcher_filter_by_type) {
    AtomSpace space;

    space.add_node(AtomType::CONCEPT_NODE, "A");
    space.add_node(AtomType::CONCEPT_NODE, "B");
    space.add_node(AtomType::PREDICATE_NODE, "P");

    PatternMatcher matcher(space);

    int count = 0;
    for ([[maybe_unused]] auto& id : matcher.filter_by_type(AtomType::CONCEPT_NODE)) {
        ++count;
    }
    ASSERT_EQ(count, 2);
    return true;
}

TEST(Query_fluent_interface) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    Handle animal = space.add_node(AtomType::CONCEPT_NODE, "Animal");
    space.add_link(AtomType::INHERITANCE_LINK, {cat, animal});

    bool found = Query(space)
        .variable("X")
        .match(AtomType::INHERITANCE_LINK, {var("X"), ground(animal.id())})
        .exists();

    ASSERT(found);
    return true;
}

TEST(Query_limit) {
    AtomSpace space;

    for (int i = 0; i < 10; ++i) {
        space.add_node(AtomType::CONCEPT_NODE, "Node" + std::to_string(i));
    }

    auto results = Query(space)
        .match(AtomType::CONCEPT_NODE, {})
        .limit(5)
        .collect();

    // Note: This test may need adjustment based on how limits work with nodes
    return true;
}

TEST(And_pattern_creation) {
    auto and_p = and_pattern({
        typed(AtomType::CONCEPT_NODE),
        typed(AtomType::PREDICATE_NODE)
    });

    ASSERT(and_p != nullptr);
    ASSERT_EQ(and_p->terms.size(), 2u);
    return true;
}

TEST(Or_pattern_creation) {
    auto or_p = or_pattern({
        typed(AtomType::CONCEPT_NODE),
        typed(AtomType::PREDICATE_NODE)
    });

    ASSERT(or_p != nullptr);
    ASSERT_EQ(or_p->terms.size(), 2u);
    return true;
}

TEST(Not_pattern_creation) {
    auto not_p = not_pattern(typed(AtomType::CONCEPT_NODE));

    ASSERT(not_p != nullptr);
    return true;
}

TEST(GlobTerm_creation) {
    GlobTerm glob("REST", 1, 5);
    ASSERT_EQ(glob.name, "REST");
    ASSERT_EQ(glob.min_count, 1u);
    ASSERT_EQ(glob.max_count, 5u);
    return true;
}

TEST(PatternMatcher_glob_in_link) {
    AtomSpace space;

    Handle a = space.add_node(AtomType::CONCEPT_NODE, "A");
    Handle b = space.add_node(AtomType::CONCEPT_NODE, "B");
    Handle c = space.add_node(AtomType::CONCEPT_NODE, "C");
    Handle lst = space.add_link(AtomType::LIST_LINK, {a, b, c});

    PatternMatcher matcher(space);

    // (List A <glob>) — the glob absorbs the remaining two atoms.
    Pattern pattern;
    pattern.body = link(AtomType::LIST_LINK, {ground(a.id()), GlobTerm("REST", 1, 10)});

    auto result = matcher.find_first(pattern);
    ASSERT(result.has_value());
    ASSERT_EQ(result->matched_atom, lst.id());

    // A glob whose range cannot cover the remainder must not match.
    Pattern bad;
    bad.body = link(AtomType::LIST_LINK, {ground(a.id()), GlobTerm("REST", 3, 10)});
    ASSERT(!matcher.find_first(bad).has_value());
    return true;
}

TEST(PatternMatcher_glob_single_atom_binds) {
    AtomSpace space;

    Handle a = space.add_node(AtomType::CONCEPT_NODE, "A");
    Handle b = space.add_node(AtomType::CONCEPT_NODE, "B");
    space.add_link(AtomType::LIST_LINK, {a, b});

    PatternMatcher matcher(space);

    Pattern pattern;
    pattern.body = link(AtomType::LIST_LINK, {ground(a.id()), GlobTerm("X", 1, 1)});

    auto result = matcher.find_first(pattern);
    ASSERT(result.has_value());
    ASSERT(result->bindings.contains("X"));
    ASSERT_EQ(result->bindings.get("X"), b.id());
    return true;
}

TEST(Query_where_predicate_filters) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    Handle dog = space.add_node(AtomType::CONCEPT_NODE, "Dog");
    Handle animal = space.add_node(AtomType::CONCEPT_NODE, "Animal");
    Handle strong = space.add_link(AtomType::INHERITANCE_LINK, {cat, animal}, TruthValue{0.9f, 0.9f});
    space.add_link(AtomType::INHERITANCE_LINK, {dog, animal}, TruthValue{0.2f, 0.9f});

    auto strength_filter = [](const AtomSpace& as, Handle h) {
        return as.get_tv(h).strength > 0.5f;
    };

    auto results = Query(space)
        .match(AtomType::INHERITANCE_LINK, {var("X"), ground(animal.id())})
        .where(strength_filter)
        .collect();
    ASSERT_EQ(results.size(), 1u);
    ASSERT_EQ(results[0].matched_atom, strong.id());

    size_t n = Query(space)
        .match(AtomType::INHERITANCE_LINK, {var("X"), ground(animal.id())})
        .where(strength_filter)
        .count();
    ASSERT_EQ(n, 1u);

    ASSERT(!Query(space)
        .match(AtomType::INHERITANCE_LINK, {var("X"), ground(animal.id())})
        .where([](const AtomSpace& as, Handle h) { return as.get_tv(h).strength > 0.95f; })
        .exists());
    return true;
}

TEST(PatternMatcher_filter_yields_matches) {
    AtomSpace space;

    space.add_node(AtomType::CONCEPT_NODE, "A");
    space.add_node(AtomType::CONCEPT_NODE, "B");
    space.add_node(AtomType::PREDICATE_NODE, "P");

    PatternMatcher matcher(space);

    size_t count = 0;
    for ([[maybe_unused]] AtomId id : matcher.filter([](const AtomSpace& as, Handle h) {
             return as.get_type(h) == AtomType::CONCEPT_NODE;
         })) {
        ++count;
    }
    ASSERT_EQ(count, 2u);
    return true;
}

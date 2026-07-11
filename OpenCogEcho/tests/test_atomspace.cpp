/**
 * @file test_atomspace.cpp
 * @brief Tests for AtomSpace
 */

#include <opencog/atomspace/atomspace.hpp>

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

using namespace opencog;

TEST(AtomSpace_empty_on_creation) {
    AtomSpace space;
    ASSERT_EQ(space.size(), 0u);
    ASSERT_EQ(space.node_count(), 0u);
    ASSERT_EQ(space.link_count(), 0u);
    return true;
}

TEST(AtomSpace_add_node) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    ASSERT(cat.valid());
    ASSERT_EQ(space.size(), 1u);
    ASSERT_EQ(space.node_count(), 1u);

    ASSERT_EQ(space.get_type(cat), AtomType::CONCEPT_NODE);
    ASSERT_EQ(space.get_name(cat), "Cat");
    return true;
}

TEST(AtomSpace_add_node_with_tv) {
    AtomSpace space;

    TruthValue tv{0.9f, 0.8f};
    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat", tv);

    TruthValue retrieved = space.get_tv(cat);
    ASSERT_EQ(retrieved.strength, tv.strength);
    ASSERT_EQ(retrieved.confidence, tv.confidence);
    return true;
}

TEST(AtomSpace_duplicate_node_returns_same) {
    AtomSpace space;

    Handle cat1 = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    Handle cat2 = space.add_node(AtomType::CONCEPT_NODE, "Cat");

    ASSERT_EQ(cat1.id(), cat2.id());
    ASSERT_EQ(space.size(), 1u);
    return true;
}

TEST(AtomSpace_different_nodes) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    Handle dog = space.add_node(AtomType::CONCEPT_NODE, "Dog");

    ASSERT_NE(cat.id(), dog.id());
    ASSERT_EQ(space.size(), 2u);
    return true;
}

TEST(AtomSpace_add_link) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    Handle animal = space.add_node(AtomType::CONCEPT_NODE, "Animal");

    Handle inheritance = space.add_link(AtomType::INHERITANCE_LINK, {cat, animal});
    ASSERT(inheritance.valid());
    ASSERT_EQ(space.link_count(), 1u);

    ASSERT_EQ(space.get_type(inheritance), AtomType::INHERITANCE_LINK);
    ASSERT_EQ(space.get_arity(inheritance), 2u);
    return true;
}

TEST(AtomSpace_link_outgoing) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    Handle animal = space.add_node(AtomType::CONCEPT_NODE, "Animal");
    Handle inheritance = space.add_link(AtomType::INHERITANCE_LINK, {cat, animal});

    auto outgoing = space.get_outgoing(inheritance);
    ASSERT_EQ(outgoing.size(), 2u);
    ASSERT_EQ(outgoing[0].id(), cat.id());
    ASSERT_EQ(outgoing[1].id(), animal.id());
    return true;
}

TEST(AtomSpace_duplicate_link_returns_same) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    Handle animal = space.add_node(AtomType::CONCEPT_NODE, "Animal");

    Handle link1 = space.add_link(AtomType::INHERITANCE_LINK, {cat, animal});
    Handle link2 = space.add_link(AtomType::INHERITANCE_LINK, {cat, animal});

    ASSERT_EQ(link1.id(), link2.id());
    ASSERT_EQ(space.link_count(), 1u);
    return true;
}

TEST(AtomSpace_get_node) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    Handle retrieved = space.get_node(AtomType::CONCEPT_NODE, "Cat");

    ASSERT(retrieved.valid());
    ASSERT_EQ(retrieved.id(), cat.id());
    return true;
}

TEST(AtomSpace_get_nonexistent_node) {
    AtomSpace space;

    Handle retrieved = space.get_node(AtomType::CONCEPT_NODE, "NonExistent");
    ASSERT(!retrieved.valid());
    return true;
}

TEST(AtomSpace_get_link) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    Handle animal = space.add_node(AtomType::CONCEPT_NODE, "Animal");
    Handle link = space.add_link(AtomType::INHERITANCE_LINK, {cat, animal});

    Handle retrieved = space.get_link(AtomType::INHERITANCE_LINK, {cat, animal});
    ASSERT(retrieved.valid());
    ASSERT_EQ(retrieved.id(), link.id());
    return true;
}

TEST(AtomSpace_incoming_set) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    Handle animal = space.add_node(AtomType::CONCEPT_NODE, "Animal");
    Handle link = space.add_link(AtomType::INHERITANCE_LINK, {cat, animal});

    auto cat_incoming = space.get_incoming(cat);
    ASSERT_EQ(cat_incoming.size(), 1u);
    ASSERT_EQ(cat_incoming[0].id(), link.id());

    auto animal_incoming = space.get_incoming(animal);
    ASSERT_EQ(animal_incoming.size(), 1u);
    return true;
}

TEST(AtomSpace_incoming_by_type) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    Handle animal = space.add_node(AtomType::CONCEPT_NODE, "Animal");
    Handle furry = space.add_node(AtomType::CONCEPT_NODE, "Furry");

    space.add_link(AtomType::INHERITANCE_LINK, {cat, animal});
    space.add_link(AtomType::SIMILARITY_LINK, {cat, furry});

    auto inheritance_incoming = space.get_incoming_by_type(cat, AtomType::INHERITANCE_LINK);
    ASSERT_EQ(inheritance_incoming.size(), 1u);

    auto similarity_incoming = space.get_incoming_by_type(cat, AtomType::SIMILARITY_LINK);
    ASSERT_EQ(similarity_incoming.size(), 1u);
    return true;
}

TEST(AtomSpace_get_atoms_by_type) {
    AtomSpace space;

    space.add_node(AtomType::CONCEPT_NODE, "Cat");
    space.add_node(AtomType::CONCEPT_NODE, "Dog");
    space.add_node(AtomType::PREDICATE_NODE, "is-fluffy");

    auto concepts = space.get_atoms_by_type(AtomType::CONCEPT_NODE);
    ASSERT_EQ(concepts.size(), 2u);

    auto predicates = space.get_atoms_by_type(AtomType::PREDICATE_NODE);
    ASSERT_EQ(predicates.size(), 1u);
    return true;
}

TEST(AtomSpace_set_tv) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    TruthValue new_tv{0.5f, 0.6f};
    space.set_tv(cat, new_tv);

    TruthValue retrieved = space.get_tv(cat);
    ASSERT_EQ(retrieved.strength, new_tv.strength);
    ASSERT_EQ(retrieved.confidence, new_tv.confidence);
    return true;
}

TEST(AtomSpace_set_av) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    AttentionValue av{10.0f, 5, 1};
    space.set_av(cat, av);

    AttentionValue retrieved = space.get_av(cat);
    ASSERT_EQ(retrieved.sti, av.sti);
    ASSERT_EQ(retrieved.lti, av.lti);
    ASSERT_EQ(retrieved.vlti, av.vlti);
    return true;
}

TEST(AtomSpace_contains) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    ASSERT(space.contains(cat));
    ASSERT(space.contains(cat.id()));

    Handle invalid;
    ASSERT(!space.contains(invalid));
    return true;
}

TEST(AtomSpace_remove_node) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    ASSERT_EQ(space.size(), 1u);

    bool removed = space.remove(cat);
    ASSERT(removed);
    ASSERT_EQ(space.size(), 0u);
    ASSERT(!space.contains(cat));
    return true;
}

TEST(AtomSpace_remove_prevents_with_incoming) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    Handle animal = space.add_node(AtomType::CONCEPT_NODE, "Animal");
    space.add_link(AtomType::INHERITANCE_LINK, {cat, animal});

    // Can't remove cat because it has incoming links
    bool removed = space.remove(cat, false);
    ASSERT(!removed);
    ASSERT(space.contains(cat));
    return true;
}

TEST(AtomSpace_remove_recursive) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    Handle animal = space.add_node(AtomType::CONCEPT_NODE, "Animal");
    Handle link = space.add_link(AtomType::INHERITANCE_LINK, {cat, animal});

    // Recursive remove should work
    bool removed = space.remove(cat, true);
    ASSERT(removed);
    ASSERT(!space.contains(cat));
    ASSERT(!space.contains(link));  // Link should also be removed
    return true;
}

TEST(AtomSpace_to_string) {
    AtomSpace space;

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat", TruthValue{0.9f, 0.8f});
    std::string str = space.to_string(cat);

    ASSERT(str.find("ConceptNode") != std::string::npos);
    ASSERT(str.find("Cat") != std::string::npos);
    return true;
}

TEST(AtomSpace_for_each_atom) {
    AtomSpace space;

    space.add_node(AtomType::CONCEPT_NODE, "A");
    space.add_node(AtomType::CONCEPT_NODE, "B");
    space.add_node(AtomType::CONCEPT_NODE, "C");

    int count = 0;
    space.for_each_atom([&count](Handle) { ++count; });
    ASSERT_EQ(count, 3);
    return true;
}

TEST(AtomSpace_clear) {
    AtomSpace space;

    space.add_node(AtomType::CONCEPT_NODE, "A");
    space.add_node(AtomType::CONCEPT_NODE, "B");
    ASSERT_EQ(space.size(), 2u);

    space.clear();
    ASSERT_EQ(space.size(), 0u);
    return true;
}

/**
 * @file test_attention.cpp
 * @brief Tests for AttentionBank (ECAN)
 */

#include <opencog/attention/attention_bank.hpp>

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
#define ASSERT_LT(a, b) if (!((a) < (b))) { return false; }

using namespace opencog;

TEST(AttentionBank_initial_funds) {
    AtomSpace space;
    ECANConfig config;
    config.initial_sti_funds = 1000.0f;
    config.initial_lti_funds = 500.0f;

    AttentionBank bank(space, config);

    ASSERT_EQ(bank.get_sti_funds(), 1000.0f);
    ASSERT_EQ(bank.get_lti_funds(), 500.0f);
    return true;
}

TEST(AttentionBank_stimulate) {
    AtomSpace space;
    AttentionBank bank(space);

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    float initial_funds = bank.get_sti_funds();

    float new_sti = bank.stimulate(cat.id(), 10.0f);

    ASSERT_GT(new_sti, 0.0f);
    ASSERT_LT(bank.get_sti_funds(), initial_funds);
    return true;
}

TEST(AttentionBank_transfer_sti) {
    AtomSpace space;
    AttentionBank bank(space);

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    Handle dog = space.add_node(AtomType::CONCEPT_NODE, "Dog");

    bank.stimulate(cat.id(), 20.0f);
    bank.transfer_sti(cat.id(), dog.id(), 5.0f);

    AttentionValue av_cat = space.get_av(cat);
    AttentionValue av_dog = space.get_av(dog);

    ASSERT_GT(av_cat.sti, 0.0f);
    ASSERT_GT(av_dog.sti, 0.0f);
    return true;
}

TEST(AttentionBank_attentional_focus) {
    AtomSpace space;
    ECANConfig config;
    config.af_boundary = 5.0f;

    AttentionBank bank(space, config);

    Handle high = space.add_node(AtomType::CONCEPT_NODE, "High");
    Handle low = space.add_node(AtomType::CONCEPT_NODE, "Low");

    bank.stimulate(high.id(), 20.0f);
    bank.stimulate(low.id(), 1.0f);

    ASSERT(bank.in_attentional_focus(high.id()));
    ASSERT(!bank.in_attentional_focus(low.id()));

    auto af = bank.get_attentional_focus();
    ASSERT_EQ(af.size(), 1u);
    return true;
}

TEST(AttentionBank_add_funds) {
    AtomSpace space;
    AttentionBank bank(space);

    float initial_sti = bank.get_sti_funds();
    float initial_lti = bank.get_lti_funds();

    bank.add_sti_funds(100.0f);
    bank.add_lti_funds(50.0f);

    ASSERT_EQ(bank.get_sti_funds(), initial_sti + 100.0f);
    ASSERT_EQ(bank.get_lti_funds(), initial_lti + 50.0f);
    return true;
}

TEST(AttentionBank_forgetting_candidates) {
    AtomSpace space;
    ECANConfig config;
    config.forgetting_threshold = 0.0f;

    AttentionBank bank(space, config);

    Handle disposable = space.add_node(AtomType::CONCEPT_NODE, "Disposable");
    space.set_av(disposable, AttentionValue{-10.0f, 0, 0});

    Handle important = space.add_node(AtomType::CONCEPT_NODE, "Important");
    space.set_av(important, AttentionValue{-10.0f, 0, 1});  // VLTI set

    size_t marked = bank.mark_for_forgetting();
    ASSERT_EQ(marked, 1u);  // Only disposable should be marked
    return true;
}

TEST(AttentionBank_update_cycle) {
    AtomSpace space;
    AttentionBank bank(space);

    Handle cat = space.add_node(AtomType::CONCEPT_NODE, "Cat");
    bank.stimulate(cat.id(), 100.0f);

    float initial_sti = space.get_av(cat).sti;

    // Run update cycle (collects rent)
    bank.update_cycle();

    // STI should decrease due to rent
    float new_sti = space.get_av(cat).sti;
    ASSERT_LT(new_sti, initial_sti);
    return true;
}

TEST(AttentionBank_spread_activation) {
    AtomSpace space;
    AttentionBank bank(space);

    Handle a = space.add_node(AtomType::CONCEPT_NODE, "A");
    Handle b = space.add_node(AtomType::CONCEPT_NODE, "B");
    space.add_link(AtomType::SIMILARITY_LINK, {a, b});

    bank.stimulate(a.id(), 100.0f);
    bank.spread_activation(a.id());

    // B should have received some attention
    float b_sti = space.get_av(b).sti;
    ASSERT_GT(b_sti, 0.0f);
    return true;
}

TEST(AttentionBank_total_sti) {
    AtomSpace space;
    AttentionBank bank(space);

    Handle a = space.add_node(AtomType::CONCEPT_NODE, "A");
    Handle b = space.add_node(AtomType::CONCEPT_NODE, "B");

    bank.stimulate(a.id(), 10.0f);
    bank.stimulate(b.id(), 20.0f);

    float total = bank.get_total_sti();
    ASSERT_GT(total, 0.0f);
    return true;
}

TEST(AttentionBank_min_max_sti) {
    AtomSpace space;
    AttentionBank bank(space);

    Handle high = space.add_node(AtomType::CONCEPT_NODE, "High");
    Handle low = space.add_node(AtomType::CONCEPT_NODE, "Low");

    bank.stimulate(high.id(), 100.0f);
    bank.stimulate(low.id(), 10.0f);

    float max_sti = bank.get_max_sti();
    float min_sti = bank.get_min_sti();

    ASSERT_GT(max_sti, min_sti);
    return true;
}

TEST(ImportanceDiffusionAgent_run_once) {
    AtomSpace space;
    AttentionBank bank(space);
    ImportanceDiffusionAgent agent(bank, space);

    Handle a = space.add_node(AtomType::CONCEPT_NODE, "A");
    Handle b = space.add_node(AtomType::CONCEPT_NODE, "B");
    space.add_link(AtomType::INHERITANCE_LINK, {a, b});

    bank.stimulate(a.id(), 50.0f);
    agent.run_once();

    // Should complete without error
    return true;
}

TEST(ForgettingAgent_run_once) {
    AtomSpace space;
    AttentionBank bank(space);
    ForgettingAgent agent(bank, space);

    Handle a = space.add_node(AtomType::CONCEPT_NODE, "A");
    space.set_av(a, AttentionValue{-1000.0f, 0, 0});

    agent.run_once();

    // Atom should be forgotten
    ASSERT(!space.contains(a));
    return true;
}

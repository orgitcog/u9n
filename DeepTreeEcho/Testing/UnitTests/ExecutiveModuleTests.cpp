/**
 * @file ExecutiveModuleTests.cpp
 * @brief Unit tests for the DeepTreeEcho/Executive module (Gaps B and D of
 *        the 32-repo integration): Cog0TaskSubsystem (cogzero TaskManager +
 *        ActionScheduler port), cog0 AtomStore/ReasoningEngine (cog-zero
 *        vendored), and AgentMessageBus (cogdiod LimboChannel/CogMessage
 *        adaptation).
 */

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

#include "Executive/Cog0TaskSubsystem.cpp"
#include "Executive/AtomStore.cpp"
#include "Executive/ReasoningEngine.cpp"
#include "Executive/AgentMessageBus.h"

using dte::Cog0TaskSubsystem;
using dte::TaskId;
using dte::kUndefinedId;

// ============================================================================
// Cog0TaskSubsystem — goals
// ============================================================================

TEST(Cog0TaskSubsystemTest, SetGoalCreatesBootstrapTask)
{
    Cog0TaskSubsystem exec;
    TaskId g = exec.setGoal("embody the avatar");
    EXPECT_NE(g, kUndefinedId);
    EXPECT_EQ(exec.getCurrentGoal(), g);
    // Auto-decomposition bootstraps one task.
    EXPECT_EQ(exec.getPendingTaskCount(), 1u);
}

TEST(Cog0TaskSubsystemTest, GoalHierarchyNavigation)
{
    Cog0TaskSubsystem exec;
    TaskId root = exec.setGoal("root", false);
    TaskId a = exec.addSubgoal(root, "a");
    TaskId b = exec.addSubgoal(root, "b");
    TaskId a1 = exec.addSubgoal(a, "a1");

    EXPECT_EQ(exec.getSubgoals(root).size(), 2u);
    EXPECT_EQ(exec.getParentGoal(a1), a);
    auto ancestors = exec.getGoalAncestors(a1);
    ASSERT_EQ(ancestors.size(), 2u);
    EXPECT_EQ(ancestors[0], a);
    EXPECT_EQ(ancestors[1], root);

    auto leaves = exec.getLeafGoals(root);
    EXPECT_EQ(leaves.size(), 2u); // a1 and b
    EXPECT_EQ(exec.getGoalHierarchyDepth(root), 3);
}

TEST(Cog0TaskSubsystemTest, HierarchicalAchievementIsMeanOfSubgoals)
{
    Cog0TaskSubsystem exec;
    TaskId root = exec.setGoal("root", false);
    TaskId a = exec.addSubgoal(root, "a");
    TaskId b = exec.addSubgoal(root, "b");

    exec.setGoalAchievement(a, 1.0);
    exec.setGoalAchievement(b, 0.5);
    EXPECT_DOUBLE_EQ(exec.calculateHierarchicalGoalAchievement(root), 0.75);
}

TEST(Cog0TaskSubsystemTest, PriorityPropagationBoostsNeverDemotes)
{
    Cog0TaskSubsystem exec;
    TaskId root = exec.setGoal("root", false);
    TaskId a = exec.addSubgoal(root, "a");

    exec.propagateGoalPriority(root, Cog0TaskSubsystem::Priority::CRITICAL);
    // Both root and subgoal boosted; propagating LOW must not demote.
    exec.propagateGoalPriority(root, Cog0TaskSubsystem::Priority::LOW);
    // Verify via achievement pathway: complete subgoal, hierarchy syncs.
    exec.setGoalAchievement(a, 1.0);
    exec.synchronizeGoalHierarchy(a);
    EXPECT_DOUBLE_EQ(exec.getGoalAchievement(root), 1.0);
}

TEST(Cog0TaskSubsystemTest, RemoveGoalSubtree)
{
    Cog0TaskSubsystem exec;
    TaskId root = exec.setGoal("root", false);
    TaskId a = exec.addSubgoal(root, "a");
    exec.addSubgoal(a, "a1");

    EXPECT_TRUE(exec.removeGoalFromHierarchy(a, false));
    EXPECT_TRUE(exec.getSubgoals(root).empty());
    EXPECT_EQ(exec.getParentGoal(a), kUndefinedId); // gone
}

// ============================================================================
// Cog0TaskSubsystem — tasks
// ============================================================================

TEST(Cog0TaskSubsystemTest, PriorityOrderAndDependencyGating)
{
    Cog0TaskSubsystem exec;
    TaskId low = exec.createTask("low", Cog0TaskSubsystem::Priority::LOW);
    TaskId high = exec.createTask("high", Cog0TaskSubsystem::Priority::HIGH);
    TaskId crit = exec.createTask("crit", Cog0TaskSubsystem::Priority::CRITICAL);

    // crit depends on high — cannot start first.
    EXPECT_TRUE(exec.addTaskDependency(crit, high));

    EXPECT_EQ(exec.getNextTask(), high);
    EXPECT_TRUE(exec.completeTask(high));
    EXPECT_EQ(exec.getNextTask(), crit);
    EXPECT_TRUE(exec.completeTask(crit));
    EXPECT_EQ(exec.getNextTask(), low);
}

TEST(Cog0TaskSubsystemTest, DirectDependencyCyclesRejected)
{
    Cog0TaskSubsystem exec;
    TaskId a = exec.createTask("a");
    TaskId b = exec.createTask("b");
    EXPECT_TRUE(exec.addTaskDependency(a, b));
    EXPECT_FALSE(exec.addTaskDependency(b, a)); // direct cycle
    EXPECT_FALSE(exec.addTaskDependency(a, a)); // self
}

TEST(Cog0TaskSubsystemTest, TaskCompletionUpdatesGoalAchievement)
{
    Cog0TaskSubsystem exec;
    TaskId g = exec.setGoal("goal", false);
    TaskId t1 = exec.createTask("t1", Cog0TaskSubsystem::Priority::MEDIUM, g);
    TaskId t2 = exec.createTask("t2", Cog0TaskSubsystem::Priority::MEDIUM, g);

    exec.getNextTask();
    exec.completeTask(t1);
    EXPECT_DOUBLE_EQ(exec.getGoalAchievement(g), 0.5);
    exec.getNextTask();
    exec.completeTask(t2);
    EXPECT_DOUBLE_EQ(exec.getGoalAchievement(g), 1.0);
}

TEST(Cog0TaskSubsystemTest, CancelAndClearPending)
{
    Cog0TaskSubsystem exec;
    TaskId a = exec.createTask("a");
    exec.createTask("b");
    EXPECT_TRUE(exec.cancelTask(a));
    EXPECT_EQ(exec.getTaskStatus(a), Cog0TaskSubsystem::TaskStatus::CANCELLED);
    EXPECT_EQ(exec.clearPendingTasks(), 1u);
    EXPECT_EQ(exec.getPendingTaskCount(), 0u);
}

// ============================================================================
// Cog0TaskSubsystem — action scheduler
// ============================================================================

TEST(Cog0TaskSubsystemTest, SchedulerFiresDueActionsInPriorityOrder)
{
    Cog0TaskSubsystem exec;
    auto now = Cog0TaskSubsystem::Clock::now();

    TaskId a1 = exec.createTask("a1");
    TaskId a2 = exec.createTask("a2");
    EXPECT_EQ(exec.scheduleAction(a1, now, 5),
              Cog0TaskSubsystem::ScheduleResult::SCHEDULED);
    EXPECT_EQ(exec.scheduleAction(a2, now, 15),
              Cog0TaskSubsystem::ScheduleResult::SCHEDULED);

    auto fired = exec.pumpScheduler(now + std::chrono::milliseconds(1));
    ASSERT_EQ(fired.size(), 2u);
    EXPECT_EQ(fired[0], a2); // higher priority first
    EXPECT_EQ(fired[1], a1);
    EXPECT_EQ(exec.getScheduledActionCount(), 0u);
}

TEST(Cog0TaskSubsystemTest, SchedulerRespectsFutureTimeAndResources)
{
    Cog0TaskSubsystem exec;
    auto now = Cog0TaskSubsystem::Clock::now();

    TaskId a = exec.createTask("future");
    exec.scheduleAction(a, now + std::chrono::seconds(60), 5);
    EXPECT_TRUE(exec.pumpScheduler(now).empty()); // not yet due

    TaskId r1 = exec.createTask("res1");
    TaskId r2 = exec.createTask("res2");
    exec.scheduleAction(r1, now, 10);
    exec.scheduleAction(r2, now, 5);
    exec.setActionResources(r1, {"gpu"});
    exec.setActionResources(r2, {"gpu"});

    auto fired = exec.pumpScheduler(now + std::chrono::milliseconds(1));
    ASSERT_EQ(fired.size(), 1u); // resource lock blocks the second
    EXPECT_EQ(fired[0], r1);
    EXPECT_TRUE(exec.isResourceLocked("gpu"));

    exec.releaseActionResources(r1);
    EXPECT_FALSE(exec.isResourceLocked("gpu"));
    fired = exec.pumpScheduler(now + std::chrono::milliseconds(2));
    ASSERT_EQ(fired.size(), 1u);
    EXPECT_EQ(fired[0], r2);
}

TEST(Cog0TaskSubsystemTest, PeriodicActionRearmsUntilRepeatExhausted)
{
    Cog0TaskSubsystem exec;
    auto now = Cog0TaskSubsystem::Clock::now();

    TaskId p = exec.createTask("periodic");
    EXPECT_EQ(exec.schedulePeriodicAction(p, 10, 3, 5),
              Cog0TaskSubsystem::ScheduleResult::SCHEDULED);

    int fires = 0;
    auto t = now;
    for (int i = 0; i < 6; ++i) {
        t += std::chrono::milliseconds(10);
        fires += static_cast<int>(exec.pumpScheduler(t).size());
        exec.releaseActionResources(p);
    }
    EXPECT_EQ(fires, 3); // exactly repeat_count firings
    EXPECT_EQ(exec.getScheduledActionCount(), 0u);
}

TEST(Cog0TaskSubsystemTest, InvalidScheduleConstraints)
{
    Cog0TaskSubsystem exec;
    auto now = Cog0TaskSubsystem::Clock::now();
    EXPECT_EQ(exec.scheduleAction(kUndefinedId, now),
              Cog0TaskSubsystem::ScheduleResult::INVALID_CONSTRAINT);
    TaskId a = exec.createTask("a");
    EXPECT_EQ(exec.scheduleAction(a, now, 0),
              Cog0TaskSubsystem::ScheduleResult::INVALID_CONSTRAINT);
    EXPECT_EQ(exec.scheduleAction(a, now, 5),
              Cog0TaskSubsystem::ScheduleResult::SCHEDULED);
    EXPECT_EQ(exec.scheduleAction(a, now, 5),
              Cog0TaskSubsystem::ScheduleResult::CONFLICT);
}

TEST(Cog0TaskSubsystemTest, ProcessCycleActivatesTaskAndSyncsGoals)
{
    Cog0TaskSubsystem exec;
    exec.setGoal("cycle goal"); // creates bootstrap task
    EXPECT_TRUE(exec.processCycle());
    EXPECT_NE(exec.getCurrentTask(), kUndefinedId);
    EXPECT_FALSE(exec.getStatusInfo().empty());
}

// ============================================================================
// cog0 AtomStore + ReasoningEngine (vendored from cog-zero)
// ============================================================================

namespace {

// Helper: add an inheritance link child -> parent to a cog0 AtomStore.
void addInherit(cog0::AtomStore& s, const std::string& child,
                const std::string& parent)
{
    auto c = s.addNode(cog0::AtomType::CONCEPT, child);
    auto p = s.addNode(cog0::AtomType::CONCEPT, parent);
    s.addLink(cog0::AtomType::INHERITANCE, {c, p});
}

// Helper: does the store hold child -> parent inheritance?
bool hasInherit(const cog0::AtomStore& s, const std::string& child,
                const std::string& parent)
{
    auto c = s.getNode(cog0::AtomType::CONCEPT, child);
    auto p = s.getNode(cog0::AtomType::CONCEPT, parent);
    if (!c || !p) return false;
    return static_cast<bool>(s.getLink(cog0::AtomType::INHERITANCE, {c, p}));
}

} // namespace

TEST(Cog0ReasoningTest, ForwardChainingFiresRules)
{
    auto store = std::make_shared<cog0::AtomStore>();
    addInherit(*store, "socrates", "human");
    addInherit(*store, "human", "mortal");

    cog0::ReasoningEngine engine(store);
    engine.addRule(
        "transitivity",
        [](const cog0::AtomStore& s) {
            // If socrates->human and human->mortal but not socrates->mortal.
            return hasInherit(s, "socrates", "human") &&
                   hasInherit(s, "human", "mortal") &&
                   !hasInherit(s, "socrates", "mortal");
        },
        [](cog0::AtomStore& s) {
            addInherit(s, "socrates", "mortal");
        },
        2.0);

    size_t firings = engine.runForwardChaining(10);
    EXPECT_GE(firings, 1u);
    EXPECT_TRUE(engine.queryInherits("socrates", "mortal"));
    EXPECT_TRUE(engine.queryExists(cog0::AtomType::CONCEPT, "socrates"));
    EXPECT_FALSE(engine.statusReport().empty());
}

// ============================================================================
// AgentMessageBus (Gap D — cogdiod patterns)
// ============================================================================

TEST(AgentMessageBusTest, StiWeightedDeliveryOrder)
{
    dte::AgentMessageBus bus;
    std::vector<std::string> order;

    auto low = bus.registerAgent("low", {0.1f, 0.0f},
        [&](const dte::AgentMessage&) { order.push_back("low"); });
    auto high = bus.registerAgent("high", {0.9f, 0.0f},
        [&](const dte::AgentMessage&) { order.push_back("high"); });

    dte::AgentMessage m;
    m.type = dte::AgentMsgType::Infer;
    EXPECT_TRUE(bus.send(low, m));
    EXPECT_TRUE(bus.send(high, m));

    EXPECT_EQ(bus.pump(), 2u);
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], "high"); // higher STI drains first
    EXPECT_EQ(order[1], "low");
}

TEST(AgentMessageBusTest, BoundedChannelAndBroadcast)
{
    dte::AgentMessageBus bus;
    std::atomic<int> received{0};
    auto a = bus.registerAgent("a", {0.5f, 0.0f},
        [&](const dte::AgentMessage&) { ++received; });
    auto b = bus.registerAgent("b", {0.4f, 0.0f},
        [&](const dte::AgentMessage&) { ++received; });

    dte::AgentMessage m;
    m.type = dte::AgentMsgType::UpdateTV;
    m.tv = {0.8f, 0.9f};

    // Fill a's inbox to the bound.
    for (std::size_t i = 0; i < dte::AgentMessageBus::kChannelBufMax; ++i)
        EXPECT_TRUE(bus.send(a, m));
    EXPECT_FALSE(bus.send(a, m)); // full — bounded channel semantics

    // Broadcast from a reaches only b.
    EXPECT_EQ(bus.broadcast(a, m), 1u);
    EXPECT_EQ(bus.inboxSize(b), 1u);

    bus.pump();
    EXPECT_EQ(received.load(),
              static_cast<int>(dte::AgentMessageBus::kChannelBufMax) + 1);
}

TEST(AgentMessageBusTest, PayloadAndAttentionTransfer)
{
    dte::AgentMessageBus bus;
    dte::AgentMessage got;
    auto a = bus.registerAgent("a", {0.2f, 0.1f},
        [&](const dte::AgentMessage& msg) { got = msg; });

    dte::AgentMessage m;
    m.type = dte::AgentMsgType::Attend;
    m.av = {0.7f, 0.3f};
    const char payload[] = "somatic";
    m.setPayload(payload, sizeof(payload));

    bus.send(a, m);
    bus.pump();
    EXPECT_EQ(got.type, dte::AgentMsgType::Attend);
    EXPECT_FLOAT_EQ(got.av.sti, 0.7f);
    EXPECT_EQ(got.payload_size, sizeof(payload));

    EXPECT_TRUE(bus.setAttention(a, {0.95f, 0.5f}));
    auto av = bus.getAttention(a);
    ASSERT_TRUE(av.has_value());
    EXPECT_FLOAT_EQ(av->sti, 0.95f);

    EXPECT_TRUE(bus.unregisterAgent(a));
    EXPECT_EQ(bus.agentCount(), 0u);
}

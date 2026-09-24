/*
 * DeepTreeEcho/Executive/Cog0TaskSubsystem.h
 *
 * Executive control layer for Deep Tree Echo (Gap B of the DTE
 * integration map): hierarchical goal decomposition, priority task
 * scheduling, dependency tracking, and temporally-constrained action
 * scheduling.
 *
 * Ported from cogpy/cogzero agentzero-core TaskManager + ActionScheduler
 * (AGPLv3). All AtomSpace dependencies (Handle / AtomSpace / TruthValue /
 * octomap spatial atoms) are replaced with plain integer IDs and POD
 * records so the subsystem compiles standalone (C++17, no UE deps) and
 * can later be wrapped by a UCog0TaskSubsystem UE component.
 *
 * Semantics preserved from upstream:
 *  - TaskStatus / Priority enums and their values
 *  - Priority-based ready-task selection gated by dependency completion
 *  - Hierarchical goals: subgoals, ancestors, leaf goals, hierarchy depth,
 *    hierarchical achievement (mean of leaf achievements), priority
 *    propagation, and status synchronization
 *  - ActionScheduler timing modes: absolute, relative delay, deadline,
 *    periodic (with repeat counts), resource locks and dependency gating
 */

#ifndef DTE_EXECUTIVE_COG0_TASK_SUBSYSTEM_H
#define DTE_EXECUTIVE_COG0_TASK_SUBSYSTEM_H

#include <chrono>
#include <cstdint>
#include <map>
#include <queue>
#include <string>
#include <vector>

namespace dte
{

/// Plain identifier replacing AtomSpace Handle. 0 == undefined.
using TaskId = std::uint64_t;
constexpr TaskId kUndefinedId = 0;

/**
 * Cog0TaskSubsystem — goal/task executive with an embedded action
 * scheduler. Single-threaded by design; drive it from the cognitive
 * loop via processCycle().
 */
class Cog0TaskSubsystem
{
public:
    // ---------------------------------------------------------
    // Enums (values preserved from cogzero)
    // ---------------------------------------------------------

    enum class TaskStatus {
        PENDING,
        ACTIVE,
        COMPLETED,
        FAILED,
        CANCELLED,
        SUSPENDED
    };

    enum class Priority : int {
        LOW = 1,
        MEDIUM = 5,
        HIGH = 10,
        CRITICAL = 20
    };

    enum class ScheduleResult {
        SCHEDULED,
        CONFLICT,
        DEPENDENCY_UNMET,
        INVALID_CONSTRAINT,
        QUEUE_FULL
    };

    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    // ---------------------------------------------------------
    // Records (replace AtomSpace atoms)
    // ---------------------------------------------------------

    struct Goal {
        TaskId id = kUndefinedId;
        std::string description;
        TaskId parent = kUndefinedId;
        std::vector<TaskId> subgoals;
        Priority priority = Priority::MEDIUM;
        /// Achievement in [0,1] — replaces upstream TruthValue strength.
        double achievement = 0.0;
        bool completed = false;
    };

    struct Task {
        TaskId id = kUndefinedId;
        std::string description;
        TaskId goal = kUndefinedId;
        Priority priority = Priority::MEDIUM;
        TaskStatus status = TaskStatus::PENDING;
        std::vector<TaskId> dependencies;
    };

    struct ScheduledAction {
        TaskId action = kUndefinedId;
        TimePoint scheduled_time{};
        TimePoint deadline{};
        std::vector<TaskId> dependencies;
        std::vector<std::string> required_resources;
        int priority = 5;
        bool has_deadline = false;
        bool is_periodic = false;
        std::chrono::milliseconds period{0};
        int repeat_count = 1;
    };

    Cog0TaskSubsystem() = default;

    // ---------------------------------------------------------
    // Goal management
    // ---------------------------------------------------------

    /// Set a new primary goal. Optionally auto-decompose (creates one
    /// bootstrap task per goal, mirroring upstream default behavior).
    TaskId setGoal(const std::string& goal_description, bool auto_decompose = true);

    /// Add a subgoal beneath an existing goal.
    TaskId addSubgoal(TaskId parent_goal, const std::string& subgoal_description);

    TaskId getCurrentGoal() const { return _current_goal; }

    /// Direct achievement in [0,1] for a single goal.
    double getGoalAchievement(TaskId goal) const;

    /// Mark achievement of a goal directly (e.g. from task completion).
    void setGoalAchievement(TaskId goal, double achievement);

    /// Hierarchical achievement: leaves report their own achievement,
    /// internal goals report the mean of their subgoals (upstream
    /// calculateHierarchicalGoalAchievement).
    double calculateHierarchicalGoalAchievement(TaskId goal) const;

    std::vector<TaskId> getSubgoals(TaskId parent_goal) const;
    TaskId getParentGoal(TaskId subgoal) const;
    std::vector<TaskId> getGoalAncestors(TaskId goal) const;
    std::vector<TaskId> getLeafGoals(TaskId root_goal = kUndefinedId) const;
    int getGoalHierarchyDepth(TaskId goal = kUndefinedId) const;

    /// High-priority parents boost subgoal priorities (never demote).
    bool propagateGoalPriority(TaskId goal, Priority priority);

    /// Parent completes when all subgoals complete; propagates upward.
    bool synchronizeGoalHierarchy(TaskId goal);

    /// Remove goal and (optionally) its whole subtree.
    bool removeGoalFromHierarchy(TaskId goal, bool preserve_orphans = false);

    // ---------------------------------------------------------
    // Task management
    // ---------------------------------------------------------

    TaskId createTask(const std::string& task_description,
                      Priority priority = Priority::MEDIUM,
                      TaskId goal = kUndefinedId);

    bool addTaskDependency(TaskId task, TaskId dependency);

    /// Highest-priority PENDING task whose dependencies are all
    /// COMPLETED; marks it ACTIVE and returns it (kUndefinedId if none).
    TaskId getNextTask();

    bool completeTask(TaskId task, bool success = true);
    bool cancelTask(TaskId task);

    TaskId getCurrentTask() const { return _current_task; }
    TaskStatus getTaskStatus(TaskId task) const;
    std::size_t getPendingTaskCount() const;
    std::vector<TaskId> getTasksByStatus(TaskStatus status) const;
    std::size_t clearPendingTasks();

    // ---------------------------------------------------------
    // Action scheduling (from cogzero ActionScheduler)
    // ---------------------------------------------------------

    ScheduleResult scheduleAction(TaskId action, TimePoint scheduled_time,
                                  int priority = 5);
    ScheduleResult scheduleActionAfter(TaskId action, int delay_ms,
                                       int priority = 5);
    ScheduleResult scheduleActionBefore(TaskId action, TimePoint deadline,
                                        int priority = 5);
    ScheduleResult schedulePeriodicAction(TaskId action, int period_ms,
                                          int repeat_count = -1,
                                          int priority = 5);

    /// Attach dependency and resource constraints to a scheduled action.
    bool setActionDependencies(TaskId action, std::vector<TaskId> deps);
    bool setActionResources(TaskId action, std::vector<std::string> resources);

    bool cancelScheduledAction(TaskId action);

    /**
     * Release due actions: an action fires when its scheduled time has
     * arrived, its dependencies are complete, and its resources are
     * free. Periodic actions re-arm until repeat_count is exhausted
     * (repeat_count < 0 means forever). Fired actions acquire their
     * resource locks; call releaseActionResources() when done.
     *
     * @param now Current time (injectable for deterministic tests)
     * @return actions fired this cycle, in priority order
     */
    std::vector<TaskId> pumpScheduler(TimePoint now = Clock::now());

    /// Release resource locks held by a fired action.
    void releaseActionResources(TaskId action);

    std::size_t getScheduledActionCount() const { return _scheduled.size(); }
    bool isResourceLocked(const std::string& resource) const;

    // ---------------------------------------------------------
    // Configuration (upstream flags preserved)
    // ---------------------------------------------------------

    void setMaxConcurrentTasks(int max_tasks) { _max_concurrent_tasks = max_tasks; }
    void setGoalDecompositionEnabled(bool enable) { _enable_goal_decomposition = enable; }
    void setPrioritySchedulingEnabled(bool enable) { _enable_priority_scheduling = enable; }
    void setMaxScheduledActions(int m) { _max_scheduled_actions = m; }

    // ---------------------------------------------------------
    // Cognitive-loop entry point and introspection
    // ---------------------------------------------------------

    /// One executive cycle: pump scheduler, pick next task if idle,
    /// synchronize goal hierarchy. Returns true on success (upstream
    /// processTaskManagement).
    bool processCycle(TimePoint now = Clock::now());

    /// JSON status string (upstream getStatusInfo).
    std::string getStatusInfo() const;

private:
    TaskId nextId() { return ++_id_counter; }
    const Goal* findGoal(TaskId id) const;
    Goal* findGoal(TaskId id);
    const Task* findTask(TaskId id) const;
    Task* findTask(TaskId id);
    bool dependenciesMet(const std::vector<TaskId>& deps) const;
    bool resourcesFree(const std::vector<std::string>& resources) const;
    int hierarchyDepthRec(TaskId goal, int depth) const;
    void collectLeaves(TaskId goal, std::vector<TaskId>& out) const;
    void syncGoalDownward(TaskId goal);

    TaskId _id_counter = 0;

    std::map<TaskId, Goal> _goals;
    std::map<TaskId, Task> _tasks;
    std::map<TaskId, ScheduledAction> _scheduled;
    std::map<std::string, TaskId> _resource_locks;

    TaskId _current_goal = kUndefinedId;
    TaskId _current_task = kUndefinedId;

    int _max_concurrent_tasks = 1;
    int _max_scheduled_actions = 256;
    bool _enable_goal_decomposition = true;
    bool _enable_priority_scheduling = true;
};

} // namespace dte

#endif // DTE_EXECUTIVE_COG0_TASK_SUBSYSTEM_H

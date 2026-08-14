/*
 * DeepTreeEcho/Executive/Cog0TaskSubsystem.cpp
 *
 * Ported from cogpy/cogzero agentzero-core TaskManager.cpp +
 * ActionScheduler.cpp — AtomSpace stripped, executive semantics
 * preserved. See header for details.
 */

#include "Cog0TaskSubsystem.h"

#include <algorithm>
#include <sstream>

namespace dte
{

// -----------------------------------------------------------------
// Internal lookup helpers
// -----------------------------------------------------------------

const Cog0TaskSubsystem::Goal* Cog0TaskSubsystem::findGoal(TaskId id) const
{
    auto it = _goals.find(id);
    return (it == _goals.end()) ? nullptr : &it->second;
}

Cog0TaskSubsystem::Goal* Cog0TaskSubsystem::findGoal(TaskId id)
{
    auto it = _goals.find(id);
    return (it == _goals.end()) ? nullptr : &it->second;
}

const Cog0TaskSubsystem::Task* Cog0TaskSubsystem::findTask(TaskId id) const
{
    auto it = _tasks.find(id);
    return (it == _tasks.end()) ? nullptr : &it->second;
}

Cog0TaskSubsystem::Task* Cog0TaskSubsystem::findTask(TaskId id)
{
    auto it = _tasks.find(id);
    return (it == _tasks.end()) ? nullptr : &it->second;
}

bool Cog0TaskSubsystem::dependenciesMet(const std::vector<TaskId>& deps) const
{
    for (TaskId d : deps) {
        const Task* t = findTask(d);
        if (!t || t->status != TaskStatus::COMPLETED) return false;
    }
    return true;
}

bool Cog0TaskSubsystem::resourcesFree(
    const std::vector<std::string>& resources) const
{
    for (const auto& r : resources)
        if (_resource_locks.count(r)) return false;
    return true;
}

// -----------------------------------------------------------------
// Goal management
// -----------------------------------------------------------------

TaskId Cog0TaskSubsystem::setGoal(const std::string& goal_description,
                                  bool auto_decompose)
{
    Goal g;
    g.id = nextId();
    g.description = goal_description;
    _goals[g.id] = g;
    _current_goal = g.id;

    // Upstream decomposeGoal(): bootstrap the goal with an initial task.
    if (auto_decompose && _enable_goal_decomposition)
        createTask("bootstrap: " + goal_description, Priority::MEDIUM, g.id);

    return g.id;
}

TaskId Cog0TaskSubsystem::addSubgoal(TaskId parent_goal,
                                     const std::string& subgoal_description)
{
    Goal* parent = findGoal(parent_goal);
    if (!parent) return kUndefinedId;

    Goal g;
    g.id = nextId();
    g.description = subgoal_description;
    g.parent = parent_goal;
    g.priority = parent->priority;
    _goals[g.id] = g;
    parent->subgoals.push_back(g.id);
    return g.id;
}

double Cog0TaskSubsystem::getGoalAchievement(TaskId goal) const
{
    const Goal* g = findGoal(goal);
    return g ? g->achievement : 0.0;
}

void Cog0TaskSubsystem::setGoalAchievement(TaskId goal, double achievement)
{
    Goal* g = findGoal(goal);
    if (!g) return;
    g->achievement = std::clamp(achievement, 0.0, 1.0);
    if (g->achievement >= 1.0) g->completed = true;
}

double Cog0TaskSubsystem::calculateHierarchicalGoalAchievement(TaskId goal) const
{
    const Goal* g = findGoal(goal);
    if (!g) return 0.0;
    if (g->subgoals.empty()) return g->achievement;

    double sum = 0.0;
    for (TaskId sg : g->subgoals)
        sum += calculateHierarchicalGoalAchievement(sg);
    return sum / static_cast<double>(g->subgoals.size());
}

std::vector<TaskId> Cog0TaskSubsystem::getSubgoals(TaskId parent_goal) const
{
    const Goal* g = findGoal(parent_goal);
    return g ? g->subgoals : std::vector<TaskId>{};
}

TaskId Cog0TaskSubsystem::getParentGoal(TaskId subgoal) const
{
    const Goal* g = findGoal(subgoal);
    return g ? g->parent : kUndefinedId;
}

std::vector<TaskId> Cog0TaskSubsystem::getGoalAncestors(TaskId goal) const
{
    std::vector<TaskId> ancestors;
    const Goal* g = findGoal(goal);
    while (g && g->parent != kUndefinedId) {
        ancestors.push_back(g->parent);
        g = findGoal(g->parent);
    }
    return ancestors;
}

void Cog0TaskSubsystem::collectLeaves(TaskId goal,
                                      std::vector<TaskId>& out) const
{
    const Goal* g = findGoal(goal);
    if (!g) return;
    if (g->subgoals.empty()) {
        out.push_back(goal);
        return;
    }
    for (TaskId sg : g->subgoals) collectLeaves(sg, out);
}

std::vector<TaskId> Cog0TaskSubsystem::getLeafGoals(TaskId root_goal) const
{
    if (root_goal == kUndefinedId) root_goal = _current_goal;
    std::vector<TaskId> leaves;
    collectLeaves(root_goal, leaves);
    return leaves;
}

int Cog0TaskSubsystem::hierarchyDepthRec(TaskId goal, int depth) const
{
    const Goal* g = findGoal(goal);
    if (!g) return depth;
    int max_depth = depth;
    for (TaskId sg : g->subgoals)
        max_depth = std::max(max_depth, hierarchyDepthRec(sg, depth + 1));
    return max_depth;
}

int Cog0TaskSubsystem::getGoalHierarchyDepth(TaskId goal) const
{
    if (goal == kUndefinedId) goal = _current_goal;
    if (!findGoal(goal)) return 0;
    return hierarchyDepthRec(goal, 1);
}

bool Cog0TaskSubsystem::propagateGoalPriority(TaskId goal, Priority priority)
{
    Goal* g = findGoal(goal);
    if (!g) return false;

    // Boost, never demote (upstream semantics).
    if (static_cast<int>(priority) > static_cast<int>(g->priority))
        g->priority = priority;

    for (TaskId sg : g->subgoals)
        propagateGoalPriority(sg, priority);
    return true;
}

bool Cog0TaskSubsystem::synchronizeGoalHierarchy(TaskId goal)
{
    Goal* g = findGoal(goal);
    if (!g) return false;

    // Downward pass: refresh this goal's completion from its subtree.
    syncGoalDownward(goal);

    // Upward pass: walk ancestors iteratively, refreshing each from its
    // direct children only (no re-descent — avoids down/up recursion
    // cycles between parent and child).
    TaskId cursor = g->parent;
    while (cursor != kUndefinedId) {
        Goal* p = findGoal(cursor);
        if (!p) break;
        if (!p->subgoals.empty()) {
            bool all_done = true;
            for (TaskId sg : p->subgoals) {
                const Goal* child = findGoal(sg);
                if (!child || !child->completed) { all_done = false; break; }
            }
            if (all_done) {
                p->completed = true;
                p->achievement = 1.0;
            }
        }
        cursor = p->parent;
    }
    return true;
}

void Cog0TaskSubsystem::syncGoalDownward(TaskId goal)
{
    Goal* g = findGoal(goal);
    if (!g || g->subgoals.empty()) return;

    bool all_done = true;
    for (TaskId sg : g->subgoals) {
        syncGoalDownward(sg);
        const Goal* child = findGoal(sg);
        if (!child || !child->completed) all_done = false;
    }
    if (all_done) {
        g->completed = true;
        g->achievement = 1.0;
    }
}

bool Cog0TaskSubsystem::removeGoalFromHierarchy(TaskId goal,
                                                bool preserve_orphans)
{
    Goal* g = findGoal(goal);
    if (!g) return false;

    // Detach from parent.
    if (g->parent != kUndefinedId) {
        Goal* parent = findGoal(g->parent);
        if (parent) {
            auto& sv = parent->subgoals;
            sv.erase(std::remove(sv.begin(), sv.end(), goal), sv.end());
        }
    }

    std::vector<TaskId> children = g->subgoals;
    if (preserve_orphans) {
        for (TaskId c : children) {
            Goal* child = findGoal(c);
            if (child) child->parent = kUndefinedId;
        }
    } else {
        for (TaskId c : children)
            removeGoalFromHierarchy(c, false);
    }

    _goals.erase(goal);
    if (_current_goal == goal) _current_goal = kUndefinedId;
    return true;
}

// -----------------------------------------------------------------
// Task management
// -----------------------------------------------------------------

TaskId Cog0TaskSubsystem::createTask(const std::string& task_description,
                                     Priority priority, TaskId goal)
{
    Task t;
    t.id = nextId();
    t.description = task_description;
    t.priority = priority;
    t.goal = goal;
    _tasks[t.id] = t;
    return t.id;
}

bool Cog0TaskSubsystem::addTaskDependency(TaskId task, TaskId dependency)
{
    Task* t = findTask(task);
    if (!t || !findTask(dependency) || task == dependency) return false;

    // Reject direct cycles (upstream dependency validation).
    const Task* d = findTask(dependency);
    if (std::find(d->dependencies.begin(), d->dependencies.end(), task) !=
        d->dependencies.end())
        return false;

    if (std::find(t->dependencies.begin(), t->dependencies.end(),
                  dependency) == t->dependencies.end())
        t->dependencies.push_back(dependency);
    return true;
}

TaskId Cog0TaskSubsystem::getNextTask()
{
    Task* best = nullptr;
    for (auto& [id, t] : _tasks) {
        if (t.status != TaskStatus::PENDING) continue;
        if (!dependenciesMet(t.dependencies)) continue;
        if (!best) { best = &t; continue; }
        if (_enable_priority_scheduling &&
            static_cast<int>(t.priority) > static_cast<int>(best->priority))
            best = &t;
    }
    if (!best) return kUndefinedId;

    best->status = TaskStatus::ACTIVE;
    _current_task = best->id;
    return best->id;
}

bool Cog0TaskSubsystem::completeTask(TaskId task, bool success)
{
    Task* t = findTask(task);
    if (!t) return false;

    t->status = success ? TaskStatus::COMPLETED : TaskStatus::FAILED;
    if (_current_task == task) _current_task = kUndefinedId;

    // Goal achievement update: completing a task nudges its goal
    // (upstream calculateGoalAchievement over associated tasks).
    if (success && t->goal != kUndefinedId) {
        std::size_t total = 0, done = 0;
        for (const auto& [id, other] : _tasks) {
            if (other.goal != t->goal) continue;
            ++total;
            if (other.status == TaskStatus::COMPLETED) ++done;
        }
        if (total > 0)
            setGoalAchievement(t->goal,
                               static_cast<double>(done) /
                                   static_cast<double>(total));
    }
    return true;
}

bool Cog0TaskSubsystem::cancelTask(TaskId task)
{
    Task* t = findTask(task);
    if (!t) return false;
    if (t->status != TaskStatus::PENDING && t->status != TaskStatus::ACTIVE)
        return false;
    t->status = TaskStatus::CANCELLED;
    if (_current_task == task) _current_task = kUndefinedId;
    return true;
}

Cog0TaskSubsystem::TaskStatus Cog0TaskSubsystem::getTaskStatus(TaskId task) const
{
    const Task* t = findTask(task);
    return t ? t->status : TaskStatus::CANCELLED;
}

std::size_t Cog0TaskSubsystem::getPendingTaskCount() const
{
    std::size_t n = 0;
    for (const auto& [id, t] : _tasks)
        if (t.status == TaskStatus::PENDING) ++n;
    return n;
}

std::vector<TaskId> Cog0TaskSubsystem::getTasksByStatus(TaskStatus status) const
{
    std::vector<TaskId> out;
    for (const auto& [id, t] : _tasks)
        if (t.status == status) out.push_back(id);
    return out;
}

std::size_t Cog0TaskSubsystem::clearPendingTasks()
{
    std::size_t n = 0;
    for (auto& [id, t] : _tasks) {
        if (t.status == TaskStatus::PENDING) {
            t.status = TaskStatus::CANCELLED;
            ++n;
        }
    }
    return n;
}

// -----------------------------------------------------------------
// Action scheduling
// -----------------------------------------------------------------

Cog0TaskSubsystem::ScheduleResult
Cog0TaskSubsystem::scheduleAction(TaskId action, TimePoint scheduled_time,
                                  int priority)
{
    if (action == kUndefinedId || priority < 1 || priority > 20)
        return ScheduleResult::INVALID_CONSTRAINT;
    if (static_cast<int>(_scheduled.size()) >= _max_scheduled_actions)
        return ScheduleResult::QUEUE_FULL;
    if (_scheduled.count(action))
        return ScheduleResult::CONFLICT;

    ScheduledAction sa;
    sa.action = action;
    sa.scheduled_time = scheduled_time;
    sa.priority = priority;
    _scheduled[action] = std::move(sa);
    return ScheduleResult::SCHEDULED;
}

Cog0TaskSubsystem::ScheduleResult
Cog0TaskSubsystem::scheduleActionAfter(TaskId action, int delay_ms,
                                       int priority)
{
    if (delay_ms < 0) return ScheduleResult::INVALID_CONSTRAINT;
    return scheduleAction(action,
                          Clock::now() + std::chrono::milliseconds(delay_ms),
                          priority);
}

Cog0TaskSubsystem::ScheduleResult
Cog0TaskSubsystem::scheduleActionBefore(TaskId action, TimePoint deadline,
                                        int priority)
{
    ScheduleResult r = scheduleAction(action, Clock::now(), priority);
    if (r == ScheduleResult::SCHEDULED) {
        _scheduled[action].deadline = deadline;
        _scheduled[action].has_deadline = true;
    }
    return r;
}

Cog0TaskSubsystem::ScheduleResult
Cog0TaskSubsystem::schedulePeriodicAction(TaskId action, int period_ms,
                                          int repeat_count, int priority)
{
    if (period_ms <= 0) return ScheduleResult::INVALID_CONSTRAINT;
    ScheduleResult r = scheduleAction(
        action, Clock::now() + std::chrono::milliseconds(period_ms), priority);
    if (r == ScheduleResult::SCHEDULED) {
        _scheduled[action].is_periodic = true;
        _scheduled[action].period = std::chrono::milliseconds(period_ms);
        _scheduled[action].repeat_count = repeat_count;
    }
    return r;
}

bool Cog0TaskSubsystem::setActionDependencies(TaskId action,
                                              std::vector<TaskId> deps)
{
    auto it = _scheduled.find(action);
    if (it == _scheduled.end()) return false;
    it->second.dependencies = std::move(deps);
    return true;
}

bool Cog0TaskSubsystem::setActionResources(TaskId action,
                                           std::vector<std::string> resources)
{
    auto it = _scheduled.find(action);
    if (it == _scheduled.end()) return false;
    it->second.required_resources = std::move(resources);
    return true;
}

bool Cog0TaskSubsystem::cancelScheduledAction(TaskId action)
{
    releaseActionResources(action);
    return _scheduled.erase(action) > 0;
}

std::vector<TaskId> Cog0TaskSubsystem::pumpScheduler(TimePoint now)
{
    // Gather ready actions.
    std::vector<ScheduledAction*> ready;
    for (auto& [id, sa] : _scheduled) {
        if (sa.scheduled_time > now) continue;
        if (sa.has_deadline && now > sa.deadline) continue; // missed deadline
        if (!dependenciesMet(sa.dependencies)) continue;
        if (!resourcesFree(sa.required_resources)) continue;
        ready.push_back(&sa);
    }

    // Priority order (upstream priority_queue ordering).
    std::sort(ready.begin(), ready.end(),
              [](const ScheduledAction* a, const ScheduledAction* b) {
                  return a->priority > b->priority;
              });

    std::vector<TaskId> fired;
    std::vector<TaskId> to_remove;
    for (ScheduledAction* sa : ready) {
        // Re-check resources: an earlier fired action may have locked them.
        if (!resourcesFree(sa->required_resources)) continue;

        // Acquire resource locks.
        for (const auto& r : sa->required_resources)
            _resource_locks[r] = sa->action;

        fired.push_back(sa->action);

        if (sa->is_periodic &&
            (sa->repeat_count < 0 || --sa->repeat_count > 0)) {
            sa->scheduled_time = now + sa->period;
        } else {
            to_remove.push_back(sa->action);
        }
    }

    // Expire missed-deadline one-shot actions.
    for (auto& [id, sa] : _scheduled)
        if (sa.has_deadline && now > sa.deadline && !sa.is_periodic)
            to_remove.push_back(id);

    for (TaskId id : to_remove) _scheduled.erase(id);
    return fired;
}

void Cog0TaskSubsystem::releaseActionResources(TaskId action)
{
    for (auto it = _resource_locks.begin(); it != _resource_locks.end();) {
        if (it->second == action)
            it = _resource_locks.erase(it);
        else
            ++it;
    }
}

bool Cog0TaskSubsystem::isResourceLocked(const std::string& resource) const
{
    return _resource_locks.count(resource) > 0;
}

// -----------------------------------------------------------------
// Cognitive-loop entry point
// -----------------------------------------------------------------

bool Cog0TaskSubsystem::processCycle(TimePoint now)
{
    pumpScheduler(now);

    if (_current_task == kUndefinedId)
        getNextTask();

    if (_current_goal != kUndefinedId)
        synchronizeGoalHierarchy(_current_goal);

    return true;
}

std::string Cog0TaskSubsystem::getStatusInfo() const
{
    std::ostringstream os;
    os << "{\"current_goal\":" << _current_goal
       << ",\"current_task\":" << _current_task
       << ",\"goals\":" << _goals.size()
       << ",\"tasks\":" << _tasks.size()
       << ",\"pending_tasks\":" << getPendingTaskCount()
       << ",\"scheduled_actions\":" << _scheduled.size()
       << ",\"resource_locks\":" << _resource_locks.size() << "}";
    return os.str();
}

} // namespace dte

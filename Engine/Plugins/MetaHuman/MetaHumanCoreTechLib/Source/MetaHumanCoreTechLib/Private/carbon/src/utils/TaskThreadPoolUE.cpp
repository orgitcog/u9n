// Copyright Epic Games, Inc. All Rights Reserved.
#ifdef WITH_EDITOR
#include "carbon/utils/TaskThreadPoolUE.h"
#include <Runtime/Core/Public/Templates/UniquePtr.h>
#include <Runtime/Core/Public/Async/TaskGraphInterfaces.h>
#include <carbon/utils/TaskThreadPoolUE.h>
#include <Runtime/Core/Public/Misc/QueuedThreadPool.h>
#include <Async/ParallelFor.h>
#include <Tasks/Task.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

std::shared_ptr<TaskThreadPoolUE> TaskThreadPoolUE::GlobalInstance(bool createIfNotAvailable, int numThreads) { return std::make_shared<TaskThreadPoolUE>(numThreads); }

TaskThreadPoolUE::TaskThreadPoolUE()
{
    m_numThreads = FTaskGraphInterface::Get().GetNumBackgroundThreads();
}

TaskThreadPoolUE::TaskThreadPoolUE(int numThreads)
{
    m_numThreads = std::min<int>((numThreads > 0 ? numThreads : MaxNumThreads()), MaxNumThreads());
}

TaskThreadPoolUE::~TaskThreadPoolUE()
{
    Stop();
}

void TaskThreadPoolUE::Stop() {}

size_t TaskThreadPoolUE::MaxNumThreads()
{
    return FTaskGraphInterface::Get().GetNumBackgroundThreads();
}

size_t TaskThreadPoolUE::NumThreads() const
{
    return m_numThreads;
}

void TaskThreadPoolUE::SetNumThreads(int numThreads)
{
    m_numThreads = std::clamp<int>(numThreads, 1, (int)MaxNumThreads());
}

void TaskThreadPoolUE::RunTask()
{
    // The tasks are kicked off automatically
}

TaskFutureUE TaskThreadPoolUE::AddTask(std::function<void()>&& Function)
{
    FGraphEventRef task = FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(Function), TStatId(), nullptr, ENamedThreads::AnyBackgroundThreadNormalTask);
    return task;
}

void TaskThreadPoolUE::AddTaskRangeAndWait(int numTasks, const std::function<void(int, int)>& processFunction, int numThreadsToUseHint)
{
    using namespace UE::Tasks;

    if (numTasks == 0)
    {
        return;
    }

    if (numTasks == 1)
    {
        processFunction(0, numTasks);
        return;
    }

    const int maxNumThreads = static_cast<int>(NumThreads());
    const int numThreadsToUse = std::min<int>(numTasks, (numThreadsToUseHint > 0 && numThreadsToUseHint < maxNumThreads) ? numThreadsToUseHint : maxNumThreads);
    const int tasksPerThread = numTasks / numThreadsToUse;
    const int additionalTasks = numTasks - tasksPerThread * numThreadsToUse;

    FGraphEventArray tasks;
    tasks.Reserve(numThreadsToUse);
    int taskIndex = 0;

    for (int threadIndex = 0; threadIndex < numThreadsToUse; ++threadIndex)
    {
        const int numTasksForThisThread = (threadIndex < additionalTasks) ? (tasksPerThread + 1) : tasksPerThread;

        tasks.Add(FFunctionGraphTask::CreateAndDispatchWhenReady(
                      [this, &processFunction, taskIndex, numTasksForThisThread]()
            {
                processFunction(taskIndex, taskIndex + numTasksForThisThread);
            }, TStatId(), nullptr, ENamedThreads::AnyBackgroundThreadNormalTask));

        taskIndex += numTasksForThisThread;
    }

    FTaskGraphInterface::Get().WaitUntilTasksComplete(tasks, ENamedThreads::AnyBackgroundThreadNormalTask);
}

void TaskThreadPoolUE::ParallelFor(int number, std::function<void(int32)>&& Body) {
    ::ParallelFor(static_cast<int32>(number), MoveTemp(Body), EParallelForFlags::None);
}

struct TaskFutureUE::Data
{
    FGraphEventRef m_task;
};

TaskFutureUE::TaskFutureUE() : m_data(std::make_unique<Data>())
{}

TaskFutureUE::~TaskFutureUE()
{}

TaskFutureUE::TaskFutureUE(FGraphEventRef Task) : m_data(std::make_unique<Data>())
{
    m_data->m_task = Task;
}

TaskFutureUE::TaskFutureUE(TaskFutureUE&& other)
{
    m_data = std::move(other.m_data);
}

TaskFutureUE& TaskFutureUE::operator=(TaskFutureUE&& other)
{
    m_data = std::move(other.m_data);

    return *this;
}

bool TaskFutureUE::Valid() const
{
    return m_data->m_task.IsValid();
}

void TaskFutureUE::Wait() { FTaskGraphInterface::Get().WaitUntilTasksComplete({ m_data->m_task }, ENamedThreads::AnyBackgroundThreadNormalTask);
}

TaskFuturesUE::~TaskFuturesUE()
{
    Wait();
}

void TaskFuturesUE::Add(TaskFutureUE&& future) { m_futures.emplace_back(std::move(future)); }

void TaskFuturesUE::Wait()
{
    for (size_t i = 0; i < m_futures.size(); ++i)
    {
        m_futures[i].Wait();
    }
    m_futures.clear();
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

#endif // WITH_EDITOR

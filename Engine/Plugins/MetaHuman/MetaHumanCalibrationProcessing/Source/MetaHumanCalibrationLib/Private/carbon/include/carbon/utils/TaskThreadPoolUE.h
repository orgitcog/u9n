// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include <carbon/Common.h>

#include <atomic>
#include <functional>
#include <future>
#include <vector>
#include <memory>

// fwd decl; must be done here as this code comes from titan research library
template <typename ReferencedType>
class TRefCountPtr;
using FGraphEvent = class FBaseGraphTask;
using FGraphEventRef = TRefCountPtr<class FBaseGraphTask>;

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)


class TaskFutureUE;
class TaskThreadPoolUE
{
public:
    static std::shared_ptr<TaskThreadPoolUE> GlobalInstance(bool createIfNotAvailable, int numThreads = MaxNumThreads());
    static void ParallelFor(int number, std::function<void(int)>&& Body);

    TaskThreadPoolUE();
    TaskThreadPoolUE(int numThreads);
    ~TaskThreadPoolUE();

    static size_t MaxNumThreads();

    void Stop();
    size_t NumThreads() const;
    void SetNumThreads(int numThreads);
    TaskFutureUE AddTask(std::function<void()>&& InFunction);
    void AddTaskRangeAndWait(int numTasks, const std::function<void(int, int)>& processFunction, int numThreadsToUseHint = -1);
    void RunTask();

private:
    std::atomic<int> m_numThreads{};
};

class TaskFutureUE
{
public:
    TaskFutureUE();
    ~TaskFutureUE();

    TaskFutureUE(FGraphEventRef Task);

    TaskFutureUE(TaskFutureUE&&);
    TaskFutureUE(const TaskFutureUE&) = delete;
    TaskFutureUE& operator=(TaskFutureUE&&);
    TaskFutureUE& operator=(const TaskFutureUE&) = delete;

    bool Valid() const;
    // bool Ready() const;
    void Wait();

private:
    struct Data;
    std::unique_ptr<Data> m_data;
};

class TaskFuturesUE
{
public:
    TaskFuturesUE() = default;
    ~TaskFuturesUE();

    void Reserve(size_t size) { m_futures.reserve(size); }

    void Add(TaskFutureUE&& future);
    void Wait();

private:
    std::vector<TaskFutureUE> m_futures;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

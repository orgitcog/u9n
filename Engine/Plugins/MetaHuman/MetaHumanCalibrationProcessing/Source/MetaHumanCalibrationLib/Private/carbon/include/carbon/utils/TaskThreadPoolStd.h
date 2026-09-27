// Copyright Epic Games, Inc. All Rights Reserved.
//
#pragma once

#include <carbon/Common.h>
#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <future>
#include <thread>
#include <vector>
#include "carbon/utils/TaskThreadPoolUE.h"

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)


// A collection of classes that implement Asynchronous execution.
// When running outside of UE the custom implementation in XXXX is used.
// When running within UE the function calls are passed to native UE types.

class TaskFutureStd;
class TaskThreadPoolStd
{
public:
    static std::shared_ptr<TaskThreadPoolStd> GlobalInstance(bool createIfNotAvailable, int numThreads = MaxNumThreads());
    static void ParallelFor(int number, std::function<void(int)>&& Body);

    TaskThreadPoolStd(int numThreads);
    ~TaskThreadPoolStd();

    void Stop();

    size_t NumThreads() const { return m_workerThreads.size(); }
    void SetNumThreads(int numThreads);
    static size_t MaxNumThreads();

    TaskFutureStd AddTask(std::function<void()>&& task);

    void AddTaskRangeAndWait(int numTasks, const std::function<void(int, int)>& processFunction, int numThreadsToUseHint = -1);

    //! explicitly run a task
    void RunTask();

private:
    void RunWorkerThread();

private:
    std::atomic<bool> m_stop;
    std::mutex m_mutex;
    std::condition_variable m_conditionVariable;
    std::deque<std::packaged_task<void()>> m_tasks;
    std::vector<std::thread> m_workerThreads;
};

class TaskFutureStd
{
public:
    TaskFutureStd() : m_future(), m_pool(nullptr) {}
    TaskFutureStd(std::future<void>&& future, TaskThreadPoolStd* pool) : m_future(std::move(future)), m_pool(pool) {}
    TaskFutureStd(TaskFutureStd&&) = default;
    TaskFutureStd(const TaskFutureStd&) = delete;
    TaskFutureStd& operator=(TaskFutureStd&&) = default;
    TaskFutureStd& operator=(const TaskFutureStd&) = delete;

    bool Valid() const;
    bool Ready() const;
    void Wait();

private:
    std::future<void> m_future;
    TaskThreadPoolStd* m_pool;
};

class TaskFuturesStd
{
public:
    TaskFuturesStd() = default;
    ~TaskFuturesStd();

    void Reserve(size_t size) { m_futures.reserve(size); }

    void Add(TaskFutureStd&& future);
    void Wait();

private:
    std::vector<TaskFutureStd> m_futures;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

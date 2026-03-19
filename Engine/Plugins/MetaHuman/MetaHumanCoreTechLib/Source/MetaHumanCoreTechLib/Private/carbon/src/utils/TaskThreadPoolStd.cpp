// Copyright Epic Games, Inc. All Rights Reserved.
#ifndef WITH_EDITOR
#include <carbon/utils/TaskThreadPoolStd.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

std::shared_ptr<TaskThreadPoolStd> TaskThreadPoolStd::GlobalInstance(bool createIfNotAvailable, int numThreads)
{
    static std::mutex mutex;
    static std::weak_ptr<TaskThreadPoolStd> weakPtr;
    std::unique_lock<std::mutex> lock(mutex);
    auto ptr = weakPtr.lock();
    if (!ptr && createIfNotAvailable)
    {
        ptr = std::make_shared<TaskThreadPoolStd>(numThreads);
        weakPtr = ptr;
    }
    return ptr;
}

TaskThreadPoolStd::TaskThreadPoolStd(int numThreads) : m_stop(false)
{
    if (numThreads <= 0 || numThreads > (int)MaxNumThreads()) numThreads = (int)MaxNumThreads();

    LOG_VERBOSE("Creating thread pool with {} threads.", numThreads);
    for (int i = 0; i < numThreads; ++i)
    {
        m_workerThreads.emplace_back(std::thread([&](int threadNum) {
                #if defined(__APPLE__)
                std::string threadName = std::string("TaskThread ") + std::to_string(threadNum);
                pthread_setname_np(threadName.c_str());
                #elif !defined(_MSC_VER)
                std::string threadName = std::string("TaskThread ") + std::to_string(threadNum);
                pthread_setname_np(pthread_self(), threadName.c_str());
                #else
                (void)threadNum;
                #endif
                RunWorkerThread();
            }, i));
    }
}

TaskThreadPoolStd::~TaskThreadPoolStd()
{
    Stop();
}

size_t TaskThreadPoolStd::MaxNumThreads()
{
    return std::thread::hardware_concurrency();
}

void TaskThreadPoolStd::SetNumThreads(int numThreads)
{
    if (numThreads <= 0 || numThreads > (int)MaxNumThreads()) numThreads = (int)MaxNumThreads();
    const int currNumThreads = (int)m_workerThreads.size();
    if (numThreads != currNumThreads)
    {
        LOG_WARNING("Changing the number of threads is not supported");
    }
}

void TaskThreadPoolStd::Stop()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_stop || m_workerThreads.empty()) { return; }

    m_stop = true;
    lock.unlock();
    m_conditionVariable.notify_all();
    for (size_t i = 0; i < m_workerThreads.size(); ++i)
    {
        if (m_workerThreads[i].joinable())
        {
            // for now detach here as we have a strange bug on windows where this will not join even if the threads have finished
            m_workerThreads[i].join();
        }
        else
        {
            LOG_ERROR("thread {} is not joinable", i);
        }
    }
    m_workerThreads.clear();
}

void TaskThreadPoolStd::RunWorkerThread()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_tasks.size() > 0)
        {
            std::packaged_task<void()> packagedTask = std::move(m_tasks.front());
            m_tasks.pop_front();
            lock.unlock();
            if (packagedTask.valid())
            {
                packagedTask();
            }
        }
        else if (m_stop)
        {
            return;
        }
        else
        {
            m_conditionVariable.wait(lock);
        }
    }
}

void TaskThreadPoolStd::RunTask()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_tasks.size() > 0)
    {
        std::packaged_task<void()> packagedTask = std::move(m_tasks.front());
        m_tasks.pop_front();
        lock.unlock();
        if (packagedTask.valid())
        {
            packagedTask();
        }
    }
}

TaskFutureStd TaskThreadPoolStd::AddTask(std::function<void()>&& task)
{
    if (m_stop)
    {
        CARBON_CRITICAL("no tasks should be added when the thread pool has been stopped");
    }
    if (!task)
    {
        return TaskFutureStd(std::future<void>(), this);
    }
    std::packaged_task<void()> packagedTask(std::move(task));
    std::future<void> future = packagedTask.get_future();
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_tasks.emplace_back(std::move(packagedTask));
    }
    m_conditionVariable.notify_one();
    return TaskFutureStd(std::move(future), this);
}

void TaskThreadPoolStd::ParallelFor(int numTasks, std::function<void(int)>&& processFunction)
{
    // parallel for is only applicable when there is a global thread pool
    auto threadPool = GlobalInstance(/*createIfNotAvailable=*/false);
    if (threadPool)
    {
        threadPool->AddTaskRangeAndWait(numTasks, [&processFunction](int start, int end) {
                for (int i = start; i < end; ++i)
                {
                    processFunction(i);
                }
            });
    }
    else
    {
        for (int i = 0; i < numTasks; ++i)
        {
            processFunction(i);
        }
    }
}

void TaskThreadPoolStd::AddTaskRangeAndWait(int numTasks, const std::function<void(int, int)>& processFunction, int numThreadsToUseHint)
{
    if (m_stop)
    {
        CARBON_CRITICAL("no tasks should be added when the thread pool has been stopped");
    }
    if (numTasks == 0) { return; }
    if (numTasks == 1)
    {
        processFunction(0, numTasks);
        return;
    }

    const int maxNumThreads = static_cast<int>(NumThreads());
    const int numThreadsToUse = std::min<int>(numTasks, (numThreadsToUseHint > 0 && numThreadsToUseHint < maxNumThreads) ? numThreadsToUseHint : maxNumThreads);
    const int tasksPerThread = numTasks / numThreadsToUse;
    const int additionalTasks = numTasks - tasksPerThread * numThreadsToUse;
    std::vector<std::future<void>> futures;
    futures.reserve(numThreadsToUse);
    std::vector<std::packaged_task<void()>> tasks;
    int taskIndex = 0;

    for (int k = 0; k < numThreadsToUse; ++k)
    {
        const int numTasksForThisThread = (k < additionalTasks) ? (tasksPerThread + 1) : tasksPerThread;

        std::packaged_task<void()> packagedTask(std::bind([](const std::function<void(int, int)>& processFunction, int start, int size){
                                                          processFunction(start, start + size);
                }, processFunction, taskIndex, numTasksForThisThread));
        futures.emplace_back(packagedTask.get_future());
        tasks.emplace_back(std::move(packagedTask));
        taskIndex += numTasksForThisThread;
    }

    {
        std::unique_lock<std::mutex> lock(m_mutex);
        for (auto&& task : tasks)
        {
            m_tasks.emplace_back(std::move(task));
        }
    }
    m_conditionVariable.notify_all();

    for (auto& future : futures)
    {
        while (future.wait_for(std::chrono::seconds(0)) == std::future_status::timeout)
        {
            // run other tasks while waiting
            RunTask();
        }
        future.get();
    }
    if (taskIndex != numTasks)
    {
        CARBON_CRITICAL("incorrect parallel call");
    }
}

bool TaskFutureStd::Valid() const
{
    return m_future.valid();
}

bool TaskFutureStd::Ready() const
{
    return (m_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready);
}

void TaskFutureStd::Wait()
{
    if (Valid())
    {
        while (!Ready())
        {
            m_pool->RunTask();
        }
        m_future.get();
    }
}

TaskFuturesStd::~TaskFuturesStd()
{
    Wait();
}

void TaskFuturesStd::Add(TaskFutureStd&& future) { m_futures.emplace_back(std::move(future)); }

void TaskFuturesStd::Wait()
{
    for (size_t i = 0; i < m_futures.size(); ++i)
    {
        m_futures[i].Wait();
    }
    m_futures.clear();
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

#endif

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
#include "carbon/utils/TaskThreadPool.h"

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

namespace TaskThreadPoolUtils {

/**
 * @brief Run the tasks range with the provided thread pool. If the thread pool is not available, run all tasks in one call to processing function
 *
 * Thread pool will call the processing function (which takes the start and end indices) in parallel until the whole range
 * of tasks is processed. If the thread pool is not available, the processing function will be called for the whole range in the thread
 * that called this function
 *
 * @note It is assumed that the first tasks starts with index 0 and that the last is at index (range - 1)
 *
 * @param[in] threadPool            - thread pool to use
 * @param[in] range                 - range of tasks
 * @param[in] processingFunction    - function that takes start and end index for the tasks to be processed
 * @param[in] numThreadsToUseHint   - optional, hint on how many threads to use at once (default is -1 which means use max available)
 */
void RunTaskRangeAndWait(std::shared_ptr<TaskThreadPool> threadPool,
                         int range,
                         const std::function<void(int, int)> &processFunction,
                         int numThreadsToUseHint = -1);

/**
 * @brief Create thread pool (optional) and run the tasks range. If the thread pool is not available, run all tasks in one call to processing function
 *
 * Thread pool will call the processing function (which takes the start and end indices) in parallel until the whole range
 * of tasks is processed. If the thread pool is not available (not created or cant be created), the processing function will be called for the whole
 * range in the thread that called this function
 *
 * @note It is assumed that the first tasks starts with index 0 and that the last is at index (range - 1)
 *
 * @param[in] range                             - range of tasks
 * @param[in] processingFunction                - function that takes start and end index for the tasks to be processed
 * @param[in] createThreadPoolIfNotAvailable    - optional, indicates should the thread pool be created if not already available (default is true)
 * @param[in] numThreadsToUseHint               - optional, hint on how many threads to use at once (default is -1 which means use max available)
 */
void RunTaskRangeAndWait(int range,
                         const std::function<void(int, int)> &processFunction,
                         int numThreadsToUseHint = -1,
                         bool createThreadPoolIfNotAvailable = true);
                         
} // namespace TaskThreadPoolUtils

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

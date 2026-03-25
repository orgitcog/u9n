// Copyright Epic Games, Inc. All Rights Reserved.
//

// A collection of classes that implement Asynchronous execution.
// When running outside of UE the TaskThreadPoolStd class (and associated types) are used.
// When running inside of UE the TaskThreadPoolUE class (and associated types) are used.

#pragma once
#ifdef WITH_EDITOR

#include "carbon/utils/TaskThreadPoolUE.h"

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

using TaskFuture = TaskFutureUE;
using TaskFutures = TaskFuturesUE;
using TaskThreadPool = TaskThreadPoolUE;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

#else

#include "carbon/utils/TaskThreadPoolStd.h"

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

using TaskFuture = TaskFutureStd;
using TaskFutures = TaskFuturesStd;
using TaskThreadPool = TaskThreadPoolStd;

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

#endif

// Copyright Epic Games, Inc. All Rights Reserved.

#include "IO/Http/Client.h"

#if !defined(NO_UE_INCLUDES)
#include <Containers/Array.h>
#include <HAL/CriticalSection.h>
#include <HAL/IConsoleManager.h>
#include <IO/IoBuffer.h>
#include <IO/Http/LaneTrace.h>
#include <Math/UnrealMathUtility.h>
#include <Memory/MemoryView.h>
#include <Misc/ByteSwap.h>
#include <Misc/ScopeExit.h>
#include <Misc/ScopeLock.h>
#include <Misc/StringBuilder.h>
#include <ProfilingDebugging/CpuProfilerTrace.h>
#include <Tasks/Task.h>
#include <Trace/Trace.h>
#endif

#include <atomic>

#include "Misc.inl"
#include "Socket.inl"
#include "Peer.inl"
#include "Socks.inl"
#include "ConnectionPool.inl"
#include "Activity.inl"
#include "Api.inl"
#include "Loop.inl"
#include "Test.inl"

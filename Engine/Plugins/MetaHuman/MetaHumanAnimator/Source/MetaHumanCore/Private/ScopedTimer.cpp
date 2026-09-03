// Copyright Epic Games, Inc. All Rights Reserved.

#include <ScopedTimer.h>

#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "MetaHumanTrace.h"

namespace epic
{
namespace core
{

ScopedTimer::ScopedTimer(const char* function_, const Logger& logger_) noexcept:
    m_function{function_},
    m_logger{ logger_ },
	m_pimpl{std::chrono::steady_clock::now() }
{
#if MHA_ENABLE_TRACE
#if CPUPROFILERTRACE_ENABLED
	FCpuProfilerTrace::OutputBeginEvent(FCpuProfilerTrace::OutputEventType(function_));
#endif
#endif
}

ScopedTimer::~ScopedTimer() noexcept
{
#if MHA_ENABLE_TRACE
#if CPUPROFILERTRACE_ENABLED
    auto stop_ = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(stop_ - m_pimpl.m_start).count();
	m_logger.Log(LogLevel::DEBUG, "[PROFILING] Function %s : %d ms.\n", m_function, static_cast<int>(duration));

	FCpuProfilerTrace::OutputEndEvent();
#endif
#endif
}

}//namespace core
}//namespace epic

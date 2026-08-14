// Copyright Epic Games, Inc. All Rights Reserved.

#include <carbon/common/Logger.h>
#include <stdarg.h>
#include <stdio.h>
#include <mutex>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

const char* LogLevel2String(LogLevel logLevel)
{
    switch (logLevel)
    {
        case LogLevel::DEBUG: return "[DEBUG]:";
        case LogLevel::INFO: return "[INFO]:";
        case LogLevel::WARNING: return "[WARNING]:";
        case LogLevel::CRITICAL: return "[CRITICAL]:";
        case LogLevel::VERBOSE: return "[VERBOSE]:";
        case LogLevel::FATAL: return "[FATAL]:";
        case LogLevel::ERR:
        default: return "[ERROR]:";
    }
}

Logger::Logger() noexcept = default;

Logger::Logger(LogFunction logFunction) noexcept :
    m_logFunction(logFunction)
{
}

Logger::~Logger() noexcept = default;

void Logger::DefaultLogger(LogLevel logLevel, const char* format, ...)
{
    static std::mutex mut{};
    std::lock_guard logGuard(mut);
    printf("%s", LogLevel2String(logLevel));

    va_list Args;
    va_start(Args, format);
    vprintf(format, Args);
    va_end(Args);

    if (logLevel == LogLevel::FATAL)
    {
        exit(-1);
    }
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

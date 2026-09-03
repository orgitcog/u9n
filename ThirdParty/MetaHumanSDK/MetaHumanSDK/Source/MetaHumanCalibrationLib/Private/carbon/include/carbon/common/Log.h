// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <exception>

#include <carbon/common/External.h>
#include <carbon/common/Format.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

// ! @returns the file basename as a constexpr
constexpr const char* ConstexprBasename(const char* path) {
    const char* file = path;
    while (*path) {
        const char c = *path++;
        if ((c == '/') || (c == '\\')) {
            file = path;
        }
    }
    return file;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

#define LOG_INTERNAL(LogLevel,...)  LOGGER.Log(LogLevel, "(%s, l%d): %s\n\0", TITAN_NAMESPACE::ConstexprBasename(__FILE__), static_cast<int>(__LINE__),\
TITAN_NAMESPACE::fmt::format(__VA_ARGS__).c_str())

#define LOG_CONDITION(format,condition,args)  LOGGER.Log(TITAN_NAMESPACE::LogLevel::ERR, format, condition, TITAN_NAMESPACE::ConstexprBasename(__FILE__), static_cast<int>(__LINE__),args)
 
#define LOG_INFO(...)  LOGGER.Log(TITAN_NAMESPACE::LogLevel::INFO, "%s\n\0", TITAN_NAMESPACE::fmt::format(__VA_ARGS__).c_str())

#define LOG_WARNING(...) LOG_INTERNAL(TITAN_NAMESPACE::LogLevel::WARNING, __VA_ARGS__)

#define LOG_ERROR(...) LOG_INTERNAL(TITAN_NAMESPACE::LogLevel::ERR, __VA_ARGS__)

#define LOG_CRITICAL(...) LOG_INTERNAL(TITAN_NAMESPACE::LogLevel::CRITICAL, __VA_ARGS__);

#define LOG_VERBOSE(...) LOG_INTERNAL(TITAN_NAMESPACE::LogLevel::VERBOSE, __VA_ARGS__)

#define LOG_FATAL(...) LOG_INTERNAL(TITAN_NAMESPACE::LogLevel::FATAL, __VA_ARGS__)

#define LOG_PRECONDITION(failedPrecondition, ...) LOG_CONDITION("FAILED PRECONDITION - %s  in (%s, l%d): %s\n\0", TITAN_NAMESPACE::fmt::to_string(failedPrecondition).c_str(), TITAN_NAMESPACE::fmt::format(__VA_ARGS__).c_str());

#define LOG_POSTCONDITION(failedPostcondition, ...) LOG_CONDITION("FAILED POSTCONDITION - %s  in (%s, l%d): %s\n\0", TITAN_NAMESPACE::fmt::to_string(failedPostcondition).c_str(), TITAN_NAMESPACE::fmt::format(__VA_ARGS__).c_str());

#define LOG_ASSERT(failedAssert, ...) LOG_CONDITION("FAILED ASSERT - %s  in (%s, l%d): %s\n\0", TITAN_NAMESPACE::fmt::to_string(failedAssert).c_str(), TITAN_NAMESPACE::fmt::format(__VA_ARGS__).c_str());

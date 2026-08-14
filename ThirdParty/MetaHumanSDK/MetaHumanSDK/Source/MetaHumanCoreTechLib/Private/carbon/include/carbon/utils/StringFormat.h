// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>

#include <cstdio>
#include <memory>
#include <string>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <typename ... Args>
std::string StringFormat(const std::string& format, Args ... args)
{
    const size_t size = std::snprintf(nullptr, 0, format.c_str(), args ...) + 1; // Extra space for '\0'
    if (size <= 0)
    {
        CARBON_CRITICAL("Error during formatting of {}", format);
    }
    std::unique_ptr<char[]> buf(new char[size]);
    std::snprintf(buf.get(), size, format.c_str(), args ...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

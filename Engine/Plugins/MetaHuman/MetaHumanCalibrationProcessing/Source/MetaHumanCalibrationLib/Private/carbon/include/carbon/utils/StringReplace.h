// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>

#include <string>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

inline std::string ReplaceSubstring(const std::string& input, const std::string& pattern, const std::string& replacement)
{
    if (pattern == replacement) { return input; }

    std::string newString = input;
    while (true)
    {
        auto pos = newString.find(pattern);
        if (pos == std::string::npos) { return newString; }
        newString.erase(pos, pattern.size());
        newString.insert(pos, replacement);
    }
    return newString;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

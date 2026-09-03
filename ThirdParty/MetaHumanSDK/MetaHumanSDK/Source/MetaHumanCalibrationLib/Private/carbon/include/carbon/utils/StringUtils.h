// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>

#include <algorithm>
#include <string>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

inline bool StringEndsWith(const std::string& input, const std::string& suffix)
{
    if (suffix.size() > input.size()) { return false; }
    return (input.substr(input.size() - suffix.size()) == suffix);
}

inline bool StringStartsWith(const std::string& input, const std::string& prefix)
{
    if (prefix.size() > input.size()) { return false; }
    return (input.substr(0, prefix.size()) == prefix);
}

inline std::string StringToLower(const std::string& input)
{
    std::string str = input;
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c){
            return static_cast<unsigned char>(std::tolower(c));
        });
    return str;
}

inline std::vector<std::string> Split(const std::string& str, const std::string& delim)
{
    if (delim.empty()) { return { str }; }
    std::vector<std::string> tokens;
    size_t start = 0;
    while (start < str.size())
    {
        size_t pos = str.find(delim, start);
        if (pos == std::string::npos)
        {
            tokens.push_back(str.substr(start));
            break;
        }
        else
        {
            tokens.push_back(str.substr(start, pos - start));
            start = pos + delim.size();
        }
        if (start == str.size())
        {
            tokens.push_back("");
        }
    }
    return tokens;
}

inline std::string Trim(const std::string& s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isspace(static_cast<unsigned char>(*it)))
    {
        ++it;
    }

    std::string::const_reverse_iterator rit = s.rbegin();
    while (rit.base() != it && std::isspace(static_cast<unsigned char>(*rit)))
    {
        ++rit;
    }

    return std::string(it, rit.base());
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

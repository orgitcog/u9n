// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <carbon/io/Utils.h>

#include <array>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

//! Inefficient yet simple CSV reader. Does not support escaped or quoated commas and newlines
inline std::vector<std::vector<std::string>> ReadCSVFile(const std::string& filename, char delim = ',')
{
    std::vector<std::vector<std::string>> csvTokens(1);

    std::string allData = TITAN_NAMESPACE::ReadFile(filename);
    size_t pos = 0;
    std::string arr;
    arr.resize(allData.size());
    size_t idx = 0;
    while (pos < allData.size())
    {
        char c = allData[pos];
        pos++;
        if (c == '\r') { continue; }
        if (c == delim)
        {
            csvTokens.back().push_back(std::string(arr.begin(), arr.begin() + idx));
            idx = 0;
        }
        else if (c == '\n')
        {
            csvTokens.back().push_back(std::string(arr.begin(), arr.begin() + idx));
            csvTokens.push_back({});
            idx = 0;
        }
        else
        {
            arr[idx] = c;
            idx++;
        }
    }
    if (idx > 0)
    {
        csvTokens.back().push_back(std::string(arr.begin(), arr.begin() + idx));
    }
    if (csvTokens.back().empty())
    {
        csvTokens.pop_back();
    }

    return csvTokens;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

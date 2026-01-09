// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <nls/serialization/CSVReader.h>
#include <carbon/common/Format.h>
#include <carbon/io/Utils.h>
#include <carbon/utils/Timer.h>
#include <carbon/common/EigenDenseBackwardsCompatible.h>

#include <map>
#include <sstream>
#include <string>
#include <vector>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

template <class T>
inline bool LoadQsa(const std::string& filename, std::map<int, std::map<std::string, T>>& valuesPerFrameAndControl)
{
    const std::vector<std::vector<std::string>> csvData = ReadCSVFile(filename);

    if (csvData.empty())
    {
        LOG_ERROR("empty qsa file {}", filename);
        return false;
    }
    const std::vector<std::string> expectedTokens = { "control_name", "frame_number", "control_value" };
    if (csvData[0] != expectedTokens)
    {
        LOG_ERROR("invalid header of qsa file {}", filename);
        return false;
    }

    valuesPerFrameAndControl.clear();

    const int numRows = int(csvData.size());
    for (int i = 1; i < numRows; ++i)
    {
        const int frameNumber = std::stoi(csvData[i][1]);
        const std::string& controlName = csvData[i][0];
        const T controlValue = T(std::stod(csvData[i][2]));
        auto it = valuesPerFrameAndControl.find(frameNumber);
        if (it == valuesPerFrameAndControl.end())
        {
            it = valuesPerFrameAndControl.insert({ frameNumber, std::map<std::string, T>() }).first;
        }
        it->second[controlName] = controlValue;
    }

    return true;
}

template <class T>
inline bool WriteQsa(const std::string& filename,
                     const std::vector<std::string>& guiControlNames,
                     const std::vector<Eigen::VectorX<T>>& guiControls,
                     const std::vector<int>& frameNumbers,
                     bool exportControlsThatAreAlwaysZero = true)
{
    std::ostringstream out;
    if (!out)
    {
        LOG_ERROR("failed to write csv file {}", filename);
        return false;
    }
    if (frameNumbers.size() != guiControls.size())
    {
        LOG_ERROR("number of frames and gui controls do not match");
        return false;
    }
    for (size_t i = 0; i < guiControls.size(); ++i)
    {
        if (int(guiControls[i].size()) != int(guiControlNames.size()))
        {
            LOG_ERROR("gui controls for frame {} does not match size of gui control names: {} vs {}", i, guiControls[i].size(), guiControlNames.size());
            return false;
        }
    }

    out << "control_name,frame_number,control_value" << std::endl;
    for (size_t i = 0; i < guiControlNames.size(); ++i)
    {
        Eigen::VectorX<T> values(frameNumbers.size());
        for (int j = 0; j < int(frameNumbers.size()); ++j)
        {
            values[j] = guiControls[j][i];
        }
        if (exportControlsThatAreAlwaysZero || (values.cwiseAbs().sum() > 0))
        {
            for (int j = 0; j < int(frameNumbers.size()); ++j)
            {
                out << guiControlNames[i] << "," << frameNumbers[j] << "," << TITAN_NAMESPACE::fmt::to_string((double)values[j]) << std::endl;
            }
        }
    }

    TITAN_NAMESPACE::WriteFile(filename, out.str());

    return true;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

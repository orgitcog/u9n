// Copyright Epic Games, Inc. All Rights Reserved.

#include <rigcalibration/RigCalibrationConfigurationData.h>

#include "RigCalibrationUtils.h"

#include <nls/geometry/GeometryHelpers.h>
#include <nls/serialization/EigenSerialization.h>
#include <nls/serialization/BinarySerialization.h>
#include <rig/RigUtils.h>

#include <carbon/io/Utils.h>

#include <filesystem>
#include <mutex>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

const std::shared_ptr<RigCalibrationParams> RigCalibrationConfigurationData::Load(const RigCalibrationDatabaseDescription& pcaDatabaseModelHandler)
{
    auto rigCalibrationParams = std::make_shared<RigCalibrationParams>();

    if (!pcaDatabaseModelHandler.GetCalibrationConfigurationFile().empty())
    {
        auto currentConfig = rigCalibrationParams->ToConfiguration();
        LoadConfiguration(pcaDatabaseModelHandler.GetCalibrationConfigurationFile(), std::string {}, currentConfig);
        rigCalibrationParams->SetFromConfiguration(currentConfig);
    }

    return rigCalibrationParams;
}

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

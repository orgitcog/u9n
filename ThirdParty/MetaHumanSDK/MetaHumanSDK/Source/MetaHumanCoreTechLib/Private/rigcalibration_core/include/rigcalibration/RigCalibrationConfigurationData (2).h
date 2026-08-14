// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <rigcalibration/RigCalibrationDatabaseDescription.h>
#include <rigcalibration/RigCalibrationParams.h>

#include <string>
#include <memory>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

class RigCalibrationConfigurationData
{
public:
    static const std::shared_ptr<RigCalibrationParams> Load(const RigCalibrationDatabaseDescription& modelHandler);
};
CARBON_NAMESPACE_END(TITAN_NAMESPACE)

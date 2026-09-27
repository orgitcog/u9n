// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <rigcalibration/ModelData.h>
#include <rigcalibration/RigCalibrationParams.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

class RigCalibrationCore
{
public:
    static std::map<std::string, Eigen::VectorXf> CalibrateExpressionsAndSkinning(const std::shared_ptr<ModelData> &data,
                                                                                  const std::map<std::string, Eigen::VectorXf> &currentParams,
                                                                                  RigCalibrationParams params,
                                                                                  bool linearize = false);
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

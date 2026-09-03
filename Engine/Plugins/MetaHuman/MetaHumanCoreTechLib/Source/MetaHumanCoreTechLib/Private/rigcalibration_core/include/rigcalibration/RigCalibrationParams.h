// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <nls/utils/Configuration.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE)

struct RigCalibrationParams
{
    //! @regularization
    float regularization { 0.5f };

    void SetFromConfiguration(const Configuration& config);

    Configuration ToConfiguration() const;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE)

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <carbon/Common.h>
#include <calib/Object.h>

CARBON_NAMESPACE_BEGIN(TITAN_NAMESPACE::calib)

struct ObjectPlaneInternal : ObjectPlane
{
    virtual void setProjectionFlag(bool) = 0;
};

CARBON_NAMESPACE_END(TITAN_NAMESPACE::calib)

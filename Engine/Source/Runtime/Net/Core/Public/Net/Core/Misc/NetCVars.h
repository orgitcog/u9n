// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

namespace UE::Net
{

NETCORE_API extern int32 CVar_ForceConnectionViewerPriority;
NETCORE_API extern bool bAutoRegisterReplicatedProperties;
NETCORE_API extern bool bEnsureForMissingProperties;
NETCORE_API extern bool bReplicateCustomDeltaPropertiesInRepIndexOrder;

}

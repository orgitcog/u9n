// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"

class FAppleDeviceList
{
public:
	using FDeviceMap = TMap<FString, FString>;

	static const FDeviceMap DeviceMap;
};
// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class METAHUMANPLATFORM_API FMetaHumanPhysicalDeviceProvider
{
public:
	static bool GetLUIDs(FString& OutUEPhysicalDeviceLUID, TArray<FString>& OutAllPhysicalDeviceLUIDs);
	static int32 GetVRAMInMB();
};
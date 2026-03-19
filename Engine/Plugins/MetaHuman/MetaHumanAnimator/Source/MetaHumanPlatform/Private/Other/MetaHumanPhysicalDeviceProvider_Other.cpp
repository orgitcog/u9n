// Copyright Epic Games, Inc. All Rights Reserved.

#if !PLATFORM_WINDOWS

#include "MetaHumanPhysicalDeviceProvider.h"

/**
  * The ability to query hardware GPU devices is only supported on Windows.
  * For other platforms we simply return false which generally indicates that
  * a software based fallback should be used.
  *
  * The values within the parameters are not affected.
  * 
  * @param OutUEPhysicalDeviceLUID 
  * @param OutAllPhysicalDeviceLUIDs 
  * @return false
  */
 bool FMetaHumanPhysicalDeviceProvider::GetLUIDs(FString& OutUEPhysicalDeviceLUID, TArray<FString>& OutAllPhysicalDeviceLUIDs)
 {
 	return false;
 }

 int32 FMetaHumanPhysicalDeviceProvider::GetVRAMInMB()
 {
	 return -1;
 }

#endif
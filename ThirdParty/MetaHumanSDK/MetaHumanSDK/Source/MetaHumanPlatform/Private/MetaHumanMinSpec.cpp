// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanMinSpec.h"
#include "MetaHumanPlatformLog.h"
#include "MetaHumanPhysicalDeviceProvider.h"
#include "MetaHumanSupportedRHI.h"
#include "HAL/IConsoleManager.h"
#include "DynamicRHI.h"

#include "MetaHumanFaceTrackerInterface.h"
#include "Features/IModularFeatures.h"

#define LOCTEXT_NAMESPACE "MetaHumanPlatform"

namespace
{
	TAutoConsoleVariable<bool> CVarCheckMinSpec{
		TEXT("mh.Core.CheckMinSpec"),
		true,
		TEXT("If set to true, warn if minimum specification is not met"),
		ECVF_Default
	};
}

bool FMetaHumanMinSpec::bIsInitialized = false;
bool FMetaHumanMinSpec::bIsSupported = false;

bool FMetaHumanMinSpec::IsSupported()
{
	if (!bIsInitialized && GDynamicRHI) // Dont initialize too early, ensure a RHI is set
	{
		bIsInitialized = true;
		bIsSupported = true;

		const FName& FeatureName = IDepthProcessingMetadataProvider::GetModularFeatureName();
		// If the modular feature is available that means the Depth Processing plugin is enabled
		if (IModularFeatures::Get().IsModularFeatureAvailable(FeatureName))
		{
		if (CVarCheckMinSpec.GetValueOnAnyThread())
		{
			FString PhysicalDeviceLUID;
			TArray<FString> PhysicalDeviceLUIDs;
				// Min Spec to run Depth Processing plugin is 8GB of VRAM, 8 Cores (Including Hyperthreads) 32 GB of RAM and support for RHI
				bIsSupported &= FMetaHumanPhysicalDeviceProvider::GetLUIDs(PhysicalDeviceLUID, PhysicalDeviceLUIDs) && !PhysicalDeviceLUIDs.IsEmpty();
				bIsSupported &= FPlatformMisc::NumberOfCoresIncludingHyperthreads() >= 8;
				bIsSupported &= FPlatformMemory::GetPhysicalGBRam() >= 32;
				bIsSupported &= FMetaHumanPhysicalDeviceProvider::GetVRAMInMB() >= 7000;
				bIsSupported &= FMetaHumanSupportedRHI::IsSupported();
		}
		else
		{
			UE_LOG(LogMetaHumanPlatform, Display, TEXT("Min spec check disabled"));
		}
	}
	}

	return bIsSupported;
}

FText FMetaHumanMinSpec::GetMinSpec()
{
	return LOCTEXT("MinSpec", "8 CPU threads, 32Gb memory, 8Gb VRAM, DirectX 12 RHI and Vulkan");
}

#undef LOCTEXT_NAMESPACE
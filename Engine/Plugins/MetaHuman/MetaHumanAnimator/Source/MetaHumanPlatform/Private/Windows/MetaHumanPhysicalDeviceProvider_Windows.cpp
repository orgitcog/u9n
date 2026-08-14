// Copyright Epic Games, Inc. All Rights Reserved.

#if PLATFORM_WINDOWS

#include "MetaHumanPhysicalDeviceProvider.h"
#include "MetaHumanPlatformLog.h"
#include "MetaHumanFaceTrackerInterface.h"

#include "ID3D12DynamicRHI.h"
#include "Features/IModularFeatures.h"

 bool FMetaHumanPhysicalDeviceProvider::GetLUIDs(FString& OutUEPhysicalDeviceLUID, TArray<FString>& OutAllPhysicalDeviceLUIDs)
 {
 	ID3D12DynamicRHI* RHI = ((GDynamicRHI && GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::D3D12) ? GetID3D12DynamicRHI() : nullptr);
 	if (!RHI)
 	{
 		UE_LOG(LogMetaHumanPlatform, Warning, TEXT("Unable to enumerate GPUs - unsupported RHI"));
 		return false;
 	}

 	const TArray<FD3D12MinimalAdapterDesc> Adapters = RHI->RHIGetAdapterDescs();
 	if (Adapters.Num() != 1)
 	{
 		UE_LOG(LogMetaHumanPlatform, Warning, TEXT("Unable to enumerate GPUs - multiple adapters"));
 		return false;
 	}
 	OutUEPhysicalDeviceLUID = FString::Printf(TEXT("%08x"), Adapters[0].Desc.AdapterLuid.LowPart);

 	const FName& FeatureName = IDepthProcessingMetadataProvider::GetModularFeatureName();
 	if (IModularFeatures::Get().IsModularFeatureAvailable(FeatureName))
 	{
 		IDepthProcessingMetadataProvider& DepthProcessingMetadata = IModularFeatures::Get().GetModularFeature<IDepthProcessingMetadataProvider>(FeatureName);
 		return DepthProcessingMetadata.ListPhysicalDeviceLUIDs(OutAllPhysicalDeviceLUIDs);
 	}

 	return false;
 }

int32 FMetaHumanPhysicalDeviceProvider::GetVRAMInMB()
 {
 	int32 VRAMInMB = -1;

 	if (GDynamicRHI && GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::D3D12)
 	{
 		ID3D12DynamicRHI* DynamicRHI = GetID3D12DynamicRHI();
 		VRAMInMB = DynamicRHI->RHIGetAdapterDescs()[0].Desc.DedicatedVideoMemory / (1024 * 1024);
 	}

 	return VRAMInMB;
 }

#endif
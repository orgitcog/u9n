// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanSupportedRHI.h"
#include "MetaHumanCoreLog.h"
#include "DynamicRHI.h"

#define LOCTEXT_NAMESPACE "MetaHumanCore"

namespace
{
	TAutoConsoleVariable<bool> CVarCheckRHI{
		TEXT("mh.Core.CheckRHI"),
		true,
		TEXT("If set to true, restricts processing to RHIs known to be supported"),
		ECVF_Default
	};
}

bool FMetaHumanSupportedRHI::bIsInitialized = false;
bool FMetaHumanSupportedRHI::bIsSupported = false;

bool FMetaHumanSupportedRHI::IsSupported()
{
	if (!bIsInitialized && GDynamicRHI) // Dont initialize too early, ensure a RHI is set
	{
		bIsInitialized = true;

		if (CVarCheckRHI.GetValueOnAnyThread())
		{
			bIsSupported = GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::D3D12;
		}
		else
		{
			bIsSupported = true;
			UE_LOG(LogMetaHumanCore, Display, TEXT("RHI check disabled"));
		}
	}

	return bIsSupported;
}

FText FMetaHumanSupportedRHI::GetSupportedRHINames()
{
	return LOCTEXT("SupportedRHI", "DirectX 12");
}

#undef LOCTEXT_NAMESPACE
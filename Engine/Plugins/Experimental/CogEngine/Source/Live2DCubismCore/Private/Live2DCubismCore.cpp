// Copyright Epic Games, Inc. All Rights Reserved.

#include "Live2DCubismCore.h"

#define LOCTEXT_NAMESPACE "FLive2DCubismCoreModule"

void FLive2DCubismCoreModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	UE_LOG(LogTemp, Log, TEXT("Live2DCubismCore: Module started"));
}

void FLive2DCubismCoreModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	UE_LOG(LogTemp, Log, TEXT("Live2DCubismCore: Module shutdown"));
}

bool FLive2DCubismCoreModule::IsSDKAvailable() const
{
	// In a real implementation, this would check if Live2D SDK is properly loaded
	return true;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FLive2DCubismCoreModule, Live2DCubismCore)

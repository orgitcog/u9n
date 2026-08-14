// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkFaceDiscoveryModule.h"

void FLiveLinkFaceDiscoveryModule::StartupModule()
{
	IModuleInterface::StartupModule();
}

void FLiveLinkFaceDiscoveryModule::ShutdownModule()
{
	IModuleInterface::ShutdownModule();
}
    
IMPLEMENT_MODULE(FLiveLinkFaceDiscoveryModule, LiveLinkFaceDiscovery)
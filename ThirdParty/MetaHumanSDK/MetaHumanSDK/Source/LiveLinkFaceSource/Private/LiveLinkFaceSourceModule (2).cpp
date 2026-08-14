// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkFaceSourceModule.h"
#include "Misc/CoreDelegates.h"

IMPLEMENT_MODULE(FLiveLinkFaceSourceModule, LiveLinkFaceSource);

void FLiveLinkFaceSourceModule::ShutdownModule()
{
	IModuleInterface::ShutdownModule();
}

void FLiveLinkFaceSourceModule::StartupModule()
{
	IModuleInterface::StartupModule();
}

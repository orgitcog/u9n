// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanLiveLinkSourceModule.h"

void FMetaHumanLiveLinkSourceModule::StartupModule()
{
	IModuleInterface::StartupModule();
}

void FMetaHumanLiveLinkSourceModule::ShutdownModule()
{
	IModuleInterface::ShutdownModule();
}

IMPLEMENT_MODULE(FMetaHumanLiveLinkSourceModule, MetaHumanLiveLinkSource)

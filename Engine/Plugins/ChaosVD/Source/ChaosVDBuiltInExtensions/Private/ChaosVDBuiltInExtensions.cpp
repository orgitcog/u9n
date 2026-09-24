// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDBuiltInExtensions.h"

#include "AccelerationStructures/ChaosVDAccelerationStructuresExtension.h"
#include "GenericDebugDraw/ChaosVDGenericDebugDrawExtension.h"

void FChaosVDBuiltInExtensionsModule::StartupModule()
{
	CreateAndRegisterExtensionInstance<FChaosVDGenericDebugDrawExtension>();
	CreateAndRegisterExtensionInstance<FChaosVDAccelerationStructuresExtension>();
}

void FChaosVDBuiltInExtensionsModule::ShutdownModule()
{
	UnregisterCreatedExtensions();
}

void FChaosVDBuiltInExtensionsModule::UnregisterCreatedExtensions()
{
	for (const TWeakPtr<FChaosVDExtension>& Extension : AvailableExtensions)
	{
		if(const TSharedPtr<FChaosVDExtension>& ExtensionPtr = Extension.Pin())
		{
			FChaosVDExtensionsManager::Get().UnRegisterExtension(ExtensionPtr.ToSharedRef());
		}
	}

	AvailableExtensions.Reset();
}

IMPLEMENT_MODULE(FChaosVDBuiltInExtensionsModule, ChaosVDBuiltInExtensions)

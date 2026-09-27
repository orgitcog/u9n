// Copyright Epic Games, Inc. All Rights Reserved.

#include "ColorGradingMixerObjectFilterRegistry.h"

const IColorGradingMixerObjectHierarchyConfig* FColorGradingMixerObjectFilterRegistry::GetClassHierarchyConfig(TSubclassOf<AActor> Class)
{
	if (TSharedPtr<IColorGradingMixerObjectHierarchyConfig>* Config = HierarchyConfigInstances.Find(Class))
	{
		return Config->Get();
	}

	// Find the config for this class or its ancestors
	while (Class)
	{
		if (FGetObjectHierarchyConfig* ConfigDelegate = HierarchyConfigs.Find(Class))
		{
			TSharedRef<IColorGradingMixerObjectHierarchyConfig> Config = ConfigDelegate->Execute();
			HierarchyConfigInstances.Emplace(Class, Config);

			return Config.ToSharedPtr().Get();
		}

		Class = Class->GetSuperClass();
	}

	// No config found. Cache nullptr so we don't have to look this up again
	HierarchyConfigInstances.Add(Class, nullptr);
	return nullptr;
}
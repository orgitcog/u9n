// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "TypeFamily/ChannelTypeFamily.h"

class FAudioCoreExperimentalModule final : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		Audio::RegisterChannelLayouts();
	}
	
	virtual void ShutdownModule() override
	{}
};
    
IMPLEMENT_MODULE(FAudioCoreExperimentalModule, AudioExperimentalRuntime)
// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "ShaderCore.h"
#include "Interfaces/IPluginManager.h"

class FInterchangeAssetsModule : public IModuleInterface
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("InterchangeAssets"))->GetBaseDir(), TEXT("Shaders"));
		AddShaderSourceDirectoryMapping(TEXT("/Plugin/InterchangeAssets"), PluginShaderDir);
	}

	virtual void ShutdownModule() override
	{}
};

IMPLEMENT_MODULE(FInterchangeAssetsModule, InterchangeAssets)


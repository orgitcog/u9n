// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointerFwd.h"

class AActor;
class ICEClonerSceneTreeCustomResolver;
class ULevel;

class FAvalancheEffectorsModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface

protected:
	static TSharedPtr<ICEClonerSceneTreeCustomResolver> CreateSceneTreeResolver(ULevel* InLevel);
};

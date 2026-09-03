// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointerFwd.h"

class IActorModifierSceneTreeCustomResolver;
class ULevel;

class FAvalancheModifiersModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface interface

private:
	TSharedPtr<IActorModifierSceneTreeCustomResolver> GetSceneTreeResolver(ULevel* InLevel);
};
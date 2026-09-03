// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Delegates/IDelegateInstance.h"
#include "Modules/ModuleInterface.h"
#include "UObject/NameTypes.h"

class FAvaSceneStateEditorModule : public IModuleInterface
{
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface

	void RegisterCustomizations();
	void UnregisterCustomizations();

	FDelegateHandle OnEditorBuildHandle;

	TArray<FName> CustomizedTypes;
};

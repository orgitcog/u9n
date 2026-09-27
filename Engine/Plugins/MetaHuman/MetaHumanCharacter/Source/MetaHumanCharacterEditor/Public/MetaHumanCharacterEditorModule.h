// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Delegates/Delegate.h"

class FMetaHumanCharacterEditorModule : public IModuleInterface
{
public:

	static FMetaHumanCharacterEditorModule& GetChecked();

	/** Returns true if the optional MetaHuman Content has been installed with the Engine and is available to the plugin */
	METAHUMANCHARACTEREDITOR_API static bool IsOptionalMetaHumanContentInstalled();

	//~Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~End IModuleInterface interface

	DECLARE_EVENT_OneParam(FMetaHumanCharacterEditorModule, FOnRegisterLayoutExtensions, class FLayoutExtender&);
	FOnRegisterLayoutExtensions& OnRegisterLayoutExtensions()
	{
		return RegisterLayoutExtensions;
	}

private:

	FOnRegisterLayoutExtensions RegisterLayoutExtensions;
};
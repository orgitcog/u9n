// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "Delegates/Delegate.h"

namespace UE::MetaHuman
{
extern METAHUMANCHARACTERPALETTEEDITOR_API const FName MessageLogName;
}

class FMetaHumanCharacterPaletteEditorModule : public IModuleInterface
{
public:

	static FMetaHumanCharacterPaletteEditorModule& GetChecked();

	// Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface interface

	DECLARE_EVENT_OneParam(FMetaHumanCharacterPaletteEditorModule, FOnRegisterLayoutExtensions, class FLayoutExtender&);
	FOnRegisterLayoutExtensions& OnRegisterLayoutExtensions()
	{
		return RegisterLayoutExtensions;
	}

private:
	FOnRegisterLayoutExtensions RegisterLayoutExtensions;
};
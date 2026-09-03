// Copyright Epic Games, Inc. All Rights Reserved.

#pragma  once

#include "Modules/ModuleInterface.h"

/**
* The public interface to this module
*/
class MASSGAMEPLAYEDITOR_API FMassGameplayEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

protected:
	void RegisterSectionMappings();
};

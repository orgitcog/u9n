// Copyright Epic Games, Inc. All Rights Reserved.

#include "TedsAlertsModule.h"

#include "Modules/ModuleManager.h"

namespace UE::Editor::DataStorage
{
	void FTedsAlertsModule::StartupModule()
	{
		IModuleInterface::StartupModule();

		FModuleManager::Get().LoadModule(TEXT("TypedElementFramework"));	
	}

	void FTedsAlertsModule::ShutdownModule()
	{
		IModuleInterface::ShutdownModule();
	}
} // namespace UE::Editor::DataStorage

IMPLEMENT_MODULE(UE::Editor::DataStorage::FTedsAlertsModule, TedsAlerts);

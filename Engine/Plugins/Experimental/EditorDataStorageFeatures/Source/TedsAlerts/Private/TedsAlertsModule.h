// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

namespace UE::Editor::DataStorage
{
	class FTedsAlertsModule
		: public IModuleInterface
	{
	public:

		FTedsAlertsModule() = default;

		// IModuleInterface interface
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;
	};
} // namespace UE::Editor::DataStorage

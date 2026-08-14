// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ADMSpatialization.h"
#include "Modules/ModuleInterface.h"


namespace UE::ADM::Spatialization
{
	class ADMSPATIALIZATION_API FModule : public IModuleInterface
	{
	public:
		FADMSpatializationFactory& GetFactory();

		virtual void StartupModule() override;
		virtual void ShutdownModule() override;

	private:
		FADMSpatializationFactory SpatializationFactory;
	};
} // namespace UE::ADM::Spatialization

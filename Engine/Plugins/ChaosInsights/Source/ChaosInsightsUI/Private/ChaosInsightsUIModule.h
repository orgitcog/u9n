// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

#include "LockRegionTrack.h"

namespace ChaosInsights
{

	class FChaosInsightsUIModule : public IModuleInterface
	{
	public:
		// IModuleInterface interface
		virtual void StartupModule() override;
		virtual void ShutdownModule() override;

	private:
		FLockRegionsSharedState TimingViewExtender;
	};

}

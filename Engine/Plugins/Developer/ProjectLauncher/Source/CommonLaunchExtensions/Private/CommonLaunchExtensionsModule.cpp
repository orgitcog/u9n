// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleInterface.h"
#include "Templates/SharedPointer.h"
#include "ProjectLauncherModule.h"
#include "Globals/GlobalsLaunchExtension.h"
#include "Insights/InsightsLaunchExtension.h"

class FCommonLaunchExtensionsModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{
		Globals = MakeShared<FGlobalsLaunchExtension>();
		Insights = MakeShared<FInsightsLaunchExtension>();

		IProjectLauncherModule::Get().RegisterExtension(Globals.ToSharedRef());
		IProjectLauncherModule::Get().RegisterExtension(Insights.ToSharedRef());
	}

	virtual void ShutdownModule() override
	{
		if (IProjectLauncherModule* ProjectLauncher = IProjectLauncherModule::TryGet())
		{
			ProjectLauncher->UnregisterExtension(Globals.ToSharedRef());
			ProjectLauncher->UnregisterExtension(Insights.ToSharedRef());
		}

		Globals.Reset();
		Insights.Reset();
	}

private:
	TSharedPtr<FGlobalsLaunchExtension> Globals;
	TSharedPtr<FInsightsLaunchExtension> Insights;

};


IMPLEMENT_MODULE(FCommonLaunchExtensionsModule, CommonLaunchExtensions);


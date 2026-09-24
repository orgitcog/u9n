// Copyright Epic Games, Inc. All Rights Reserved.
#include "AudioInsightsEditorModule.h"

#include "AudioInsightsEditorLog.h"
#include "AudioInsightsStyle.h"
#include "Framework/Docking/TabManager.h"
#include "IAudioInsightsModule.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "Views/AudioAnalyzerRackDashboardViewFactory.h"
#include "Views/AudioBusesDashboardViewFactory.h"
#include "Views/AudioMetersDashboardViewFactory.h"
#include "Views/LogDashboardViewFactory.h"
#include "Views/MixerSourceDashboardViewFactory.h"
#include "Views/SoundDashboardViewFactory.h"
#include "Views/SubmixesDashboardViewFactory.h"
#include "Views/ViewportDashboardViewFactory.h"
#include "Views/VirtualLoopDashboardViewFactory.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "AudioInsights"

DEFINE_LOG_CATEGORY(LogAudioInsightsEditor);

namespace UE::Audio::Insights
{
	void FAudioInsightsEditorModule::StartupModule()
	{
		// Don't run providers in any commandlet to avoid additional, unnecessary overhead as audio insights is dormant.
		if (!IsRunningCommandlet())
		{
			RegisterMenus();

			DashboardFactory = MakeShared<FEditorDashboardFactory>();
			
			// @TODO UE-274216: Decide what to do with the Viewport dashboard
			//DashboardFactory->RegisterViewFactory(MakeShared<FViewportDashboardViewFactory>());
			DashboardFactory->RegisterViewFactory(MakeShared<FLogDashboardViewFactory>());
			DashboardFactory->RegisterViewFactory(MakeShared<FSoundDashboardViewFactory>());
			DashboardFactory->RegisterViewFactory(MakeShared<FMixerSourceDashboardViewFactory>());
			DashboardFactory->RegisterViewFactory(MakeShared<FVirtualLoopDashboardViewFactory>());
			DashboardFactory->RegisterViewFactory(MakeShared<FSubmixesDashboardViewFactory>());
			DashboardFactory->RegisterViewFactory(MakeShared<FAudioBusesDashboardViewFactory>());
			DashboardFactory->RegisterViewFactory(MakeShared<FAudioMetersDashboardViewFactory>());
			DashboardFactory->RegisterViewFactory(MakeShared<FAudioAnalyzerRackDashboardViewFactory>());
		}
	}

	void FAudioInsightsEditorModule::ShutdownModule()
	{
		if (!IsRunningCommandlet())
		{
			DashboardFactory.Reset();
		}
	}

	void FAudioInsightsEditorModule::RegisterDashboardViewFactory(TSharedRef<IDashboardViewFactory> InDashboardFactory)
	{
		DashboardFactory->RegisterViewFactory(InDashboardFactory);
	}

	void FAudioInsightsEditorModule::UnregisterDashboardViewFactory(FName InName)
	{
		DashboardFactory->UnregisterViewFactory(InName);
	}

	::Audio::FDeviceId FAudioInsightsEditorModule::GetDeviceId() const
	{
		return DashboardFactory->GetDeviceId();
	}

	bool FAudioInsightsEditorModule::IsModuleLoaded()
	{
		return FModuleManager::Get().IsModuleLoaded(AudioInsightsEditorModuleName);
	}

	FAudioInsightsEditorModule& FAudioInsightsEditorModule::GetChecked()
	{
		return static_cast<FAudioInsightsEditorModule&>(FModuleManager::LoadModuleChecked<IAudioInsightsEditorModule>(AudioInsightsEditorModuleName));
	}

	IAudioInsightsTraceModule& FAudioInsightsEditorModule::GetTraceModule()
	{
		IAudioInsightsModule& InsightsModule = IAudioInsightsModule::GetChecked();
		return InsightsModule.GetTraceModule();
	}

	TSharedRef<FEditorDashboardFactory> FAudioInsightsEditorModule::GetDashboardFactory()
	{
		return DashboardFactory->AsShared();
	}

	const TSharedRef<FEditorDashboardFactory> FAudioInsightsEditorModule::GetDashboardFactory() const
	{
		return DashboardFactory->AsShared();
	}

	TSharedRef<SDockTab> FAudioInsightsEditorModule::CreateDashboardTabWidget(const FSpawnTabArgs& Args)
	{
		return DashboardFactory->MakeDockTabWidget(Args);
	}

	void FAudioInsightsEditorModule::RegisterMenus()
	{
		const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner("AudioInsights", FOnSpawnTab::CreateRaw(this, &FAudioInsightsEditorModule::CreateDashboardTabWidget))
			.SetDisplayName(LOCTEXT("OpenDashboard_TabDisplayName", "Audio Insights"))
			.SetTooltipText(LOCTEXT("OpenDashboard_TabTooltip", "Opens Audio Insights, an extensible suite of tools and visualizers which enable monitoring and debugging audio in the Unreal Engine."))
			.SetGroup(MenuStructure.GetToolsCategory())
			.SetIcon(FSlateStyle::Get().CreateIcon("AudioInsights.Icon.Dashboard"));
	};
} // namespace UE::Audio::Insights
#undef LOCTEXT_NAMESPACE // AudioInsights

IMPLEMENT_MODULE(UE::Audio::Insights::FAudioInsightsEditorModule, AudioInsightsEditor)

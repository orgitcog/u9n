// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanSDKEditor.h"

#include "EngineAnalytics.h"
#include "Import/MetaHumanAssetUpdateHandler.h"
#include "MetaHumanSDKSettings.h"

#include "Cloud/MetaHumanServiceRequest.h"

#include "ISettingsModule.h"
#include "Logging/StructuredLog.h"
#include "Modules/ModuleManager.h"
#include "UI/MetaHumanManager.h"

#define LOCTEXT_NAMESPACE "MetaHumanSDKEditor"

DEFINE_LOG_CATEGORY(LogMetaHumanSDK);

namespace UE::MetaHuman
{
class FMetaHumanSDKEditorModule final
	: public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->RegisterSettings("Project",
											"Plugins",
											"MetaHumanSDK",
											LOCTEXT("SectionName", "MetaHuman SDK"),
											LOCTEXT("SectionDescription", "Settings for the MetaHuman SDK"),
											GetMutableDefault<UMetaHumanSDKSettings>()
			);
		}
		// ensure we have a valid environment
		ServiceAuthentication::InitialiseAuthEnvironment({});
		FMetaHumanManager::Initialize();
	}

	virtual void ShutdownModule() override
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "MetaHumanSDK");
		}

		ServiceAuthentication::ShutdownAuthEnvironment();
		FMetaHumanAssetUpdateHandler::Shutdown();
		FMetaHumanManager::Shutdown();
	}
};

IMPLEMENT_MODULE(FMetaHumanSDKEditorModule, MetaHumanSDKEditor);

void AnalyticsEvent(FString&& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (FEngineAnalytics::IsAvailable())
	{
		FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.MetaHumanCharacter.") + EventName, Attributes);
	}
}
}

#undef LOCTEXT_NAMESPACE

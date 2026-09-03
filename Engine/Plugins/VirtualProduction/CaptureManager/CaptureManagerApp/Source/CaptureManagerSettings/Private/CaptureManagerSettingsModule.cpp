// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureManagerSettingsModule.h"

#include "ISettingsModule.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Settings/CaptureManagerSettings.h"
#include "Settings/CaptureManagerSettingsCustomization.h"

#define LOCTEXT_NAMESPACE "CaptureManagerSettings"

void FCaptureManagerSettingsModule::StartupModule()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FCaptureManagerSettingsModule::PostEngineInit);
	FCoreDelegates::OnEnginePreExit.AddRaw(this, &FCaptureManagerSettingsModule::EnginePreExit);

	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout(UCaptureManagerSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FCaptureManagerSettingsCustomization::MakeInstance));
}

void FCaptureManagerSettingsModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
	FCoreDelegates::OnEnginePreExit.RemoveAll(this);
}

void FCaptureManagerSettingsModule::PostEngineInit()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		UCaptureManagerSettings* Settings = GetMutableDefault<UCaptureManagerSettings>();
		check(Settings);

		// Sanity check that the default upload host name is not empty
		check(!Settings->DefaultUploadHostName.IsEmpty());

		SettingsModule->RegisterSettings("Project", "Plugins", "Capture Manager",
			LOCTEXT("CaptureManagerSettingsName", "Capture Manager"),
			LOCTEXT("CaptureManagerDescription", "Configure Capture Manager."),
			Settings
		);
	}
}

void FCaptureManagerSettingsModule::EnginePreExit()
{
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Plugins", "Capture Manager");
	}
}

IMPLEMENT_MODULE(FCaptureManagerSettingsModule, CaptureManagerSettings)

#undef LOCTEXT_NAMESPACE
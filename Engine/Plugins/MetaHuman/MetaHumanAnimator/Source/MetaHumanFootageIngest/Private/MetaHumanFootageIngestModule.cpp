// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanFootageIngestModule.h"

#include "CaptureManager.h"
#include "CaptureManagerWidget.h"
#include "MetaHumanFootageRetrievalWindowStyle.h"

#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "MetaHumanFootageIngestModule"
static const FName MetaHumanFootageRetrievalWindowTabName("MetaHumanFootageRetrieval");
static const FName CaptureManagerWindowTabName("CaptureManager");

void FMetaHumanFootageIngestModule::StartupModule()
{
	// Initializes and registers the Footage Retrieval styles
	FMetaHumanFootageRetrievalWindowStyle::Get();
	FMetaHumanFootageRetrievalWindowStyle::Register();
	FMetaHumanFootageRetrievalWindowStyle::ReloadTextures();

	FCaptureManager::Initialize();
	CaptureManager = FCaptureManager::Get();

	// We need to terminate on pre-exit because otherwise we won't be able to do any IsValid() checks
	// on assets while terminating since the asset manager is already deinitialized at that point.
	FCoreDelegates::OnPreExit.AddLambda([this]() {
			FCaptureManager::Terminate();
		});
}

void FMetaHumanFootageIngestModule::ShutdownModule()
{
	UToolMenus::UnregisterOwner(this);
	
	// Unregister the styles used by the plugin
	FMetaHumanFootageRetrievalWindowStyle::Unregister();
}

void FMetaHumanFootageIngestModule::CaptureManagerMenuSelected()
{
	CaptureManager->Show();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMetaHumanFootageIngestModule, MetaHumanFootageIngest)
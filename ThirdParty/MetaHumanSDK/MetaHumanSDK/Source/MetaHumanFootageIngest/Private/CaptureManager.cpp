// Copyright Epic Games, Inc. All Rights Reserved.

#include "CaptureManager.h"
#include "CaptureManagerWidget.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Docking/WorkspaceItem.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Docking/LayoutService.h"
#include "Widgets/Docking/SDockTab.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Interfaces/IMainFrameModule.h"
#include "ToolMenus.h"
#include "WorkspaceMenuStructureModule.h"
#include "WorkspaceMenuStructure.h"
#include "MetaHumanFootageRetrievalWindowStyle.h"
#include "CaptureManagerCommands.h"
#include "CaptureManagerLog.h"

#include "ContentBrowserDataSubsystem.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "IContentBrowserDataModule.h"
#include "Internationalization/Regex.h"

#define LOCTEXT_NAMESPACE "CaptureManager"

FCaptureManager* FCaptureManager::Instance = nullptr;

static const FName TabName("CaptureManager");

FCaptureManager* FCaptureManager::Get()
{
	return Instance;
}

void FCaptureManager::Initialize()
{
	if (!Instance)
	{
		Instance = new FCaptureManager();
	}
}

void FCaptureManager::Terminate()
{
	if (Instance)
	{
		delete Instance;
	}
}

void FCaptureManager::Show()
{
	FGlobalTabmanager::Get()->TryInvokeTab(TabName);
}

TWeakPtr<SDockTab> FCaptureManager::ShowMonitoringTab(UMetaHumanCaptureSource* const CaptureSource)
{
	if (CaptureManagerWidget.IsValid())
	{
		return CaptureManagerWidget->ShowMonitoringTab(CaptureSource);
	}
	return nullptr;
}

FCaptureManager::FCaptureManager()
{
	Commands = MakeShared<FCaptureManagerCommands>();
	Commands->RegisterCommands();
	RegisterTabSpawner();

	// Update the default asset creation path when a UEFN project is loaded. We lack access to more direct valkyrie functions to detect this, so we 
	// use the map loaded event instead.
	OnMapOpenedDelegateHandle = FEditorDelegates::OnMapOpened.AddRaw(this, &FCaptureManager::OnMapOpened);
}

FCaptureManager::~FCaptureManager()
{
	Commands->Unregister();
	UnregisterTabSpawner();

	FEditorDelegates::OnMapOpened.Remove(OnMapOpenedDelegateHandle);
}

void FCaptureManager::RegisterTabSpawner()
{
	auto SpawnMainTab = [this](const FSpawnTabArgs& Args) -> TSharedRef<SDockTab>
		{
			const TSharedRef<SDockTab> DockTab = SNew(SDockTab)
				.Label(LOCTEXT("MainTabTitle", "Capture Manager"))
				.TabRole(ETabRole::MajorTab)
				.OnCanCloseTab_Raw(this, &FCaptureManager::OnCanCloseCaptureTab)
				.OnTabClosed_Raw(this, &FCaptureManager::OnCaptureManagerTabClosed);


			DockTab->SetContent(
				SAssignNew(CaptureManagerWidget, SCaptureManagerWidget, Args.GetOwnerWindow(), DockTab)
				.CaptureManagerCommands(Commands)
			);

			return DockTab;
		};

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabName, FOnSpawnTab::CreateLambda(MoveTemp(SpawnMainTab)))
		.SetDisplayName(LOCTEXT("MainTabTitle", "Capture Manager"))
		.SetTooltipText(LOCTEXT("CaptureManagerToolTip", "Control capture sources and ingest footage"))
		.SetIcon(FSlateIcon(FMetaHumanFootageRetrievalWindowStyle::Get().GetStyleSetName(), TEXT("CaptureManager.Tabs.CaptureManager")))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());
}

void FCaptureManager::OnCaptureManagerTabClosed(TSharedRef<SDockTab> ClosedTab)
{
	if (CaptureManagerWidget.IsValid())
	{
		CaptureManagerWidget->OnClose();
	}
}

bool FCaptureManager::OnCanCloseCaptureTab()
{
	if (CaptureManagerWidget.IsValid())
	{
		return CaptureManagerWidget->CanClose();
	}

	return true;
}

void FCaptureManager::UnregisterTabSpawner()
{
	FGlobalTabmanager::Get()->UnregisterTabSpawner(TabName);
}

void FCaptureManager::OnMapOpened([[maybe_unused]] const FString& FileName, [[maybe_unused]] bool bAsTemplate)
{
	if (CaptureManagerWidget)
	{
		CaptureManagerWidget->UpdateDefaultAssetCreationLocation();
	}
}

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterEditorUILayer.h"
#include "MetaHumanCharacterEditorModule.h"
#include "Toolkits/IToolkit.h"


void UMetaHumanCharacterEditorUISubsystem::Initialize(FSubsystemCollectionBase& InCollection)
{
	FMetaHumanCharacterEditorModule::GetChecked().OnRegisterLayoutExtensions().AddUObject(this, &UMetaHumanCharacterEditorUISubsystem::RegisterLayoutExtensions);
}

void UMetaHumanCharacterEditorUISubsystem::Deinitialize()
{
	FMetaHumanCharacterEditorModule::GetChecked().OnRegisterLayoutExtensions().RemoveAll(this);
}

void UMetaHumanCharacterEditorUISubsystem::RegisterLayoutExtensions(FLayoutExtender& InExtender)
{
	FTabManager::FTab NewTab{ FTabId{ UAssetEditorUISubsystem::TopLeftTabID }, ETabState::ClosedTab };
	InExtender.ExtendStack(TEXT("EditorSidePanelArea"), ELayoutExtensionPosition::After, NewTab);
}

FMetaHumanCharacterEditorModeUILayer::FMetaHumanCharacterEditorModeUILayer(const IToolkitHost* InToolkitHost)
	: FAssetEditorModeUILayer{ InToolkitHost }
{
}

void FMetaHumanCharacterEditorModeUILayer::OnToolkitHostingStarted(const TSharedRef<IToolkit>& InToolkit)
{
	if (!InToolkit->IsAssetEditor())
	{
		FAssetEditorModeUILayer::OnToolkitHostingStarted(InToolkit);
		HostedToolkit = InToolkit;
		InToolkit->SetModeUILayer(SharedThis(this));
		InToolkit->RegisterTabSpawners(ToolkitHost->GetTabManager().ToSharedRef());
		RegisterModeTabSpawners();
		OnToolkitHostReadyForUI.ExecuteIfBound();
	}
}

void FMetaHumanCharacterEditorModeUILayer::OnToolkitHostingFinished(const TSharedRef<IToolkit>& InToolkit)
{
	if (HostedToolkit.IsValid() && HostedToolkit.Pin() == InToolkit)
	{
		FAssetEditorModeUILayer::OnToolkitHostingFinished(InToolkit);
	}
}

TSharedPtr<FWorkspaceItem> FMetaHumanCharacterEditorModeUILayer::GetModeMenuCategory() const
{
	check(MetaHumanCharacterEditorMenuCategory);
	return MetaHumanCharacterEditorMenuCategory;
}

void FMetaHumanCharacterEditorModeUILayer::SetModeMenuCategory(TSharedPtr<FWorkspaceItem> InMenuCategory)
{
	MetaHumanCharacterEditorMenuCategory = InMenuCategory;
}

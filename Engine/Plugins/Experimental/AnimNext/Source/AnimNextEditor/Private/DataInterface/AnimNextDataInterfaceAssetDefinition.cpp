// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNextDataInterfaceAssetDefinition.h"

#include "AnimNextRigVMAssetEditorData.h"
#include "ContentBrowserMenuContexts.h"
#include "Editor.h"
#include "IWorkspaceEditorModule.h"
#include "ToolMenus.h"
#include "UncookedOnlyUtils.h"
#include "UObject/SavePackage.h"
#include "Workspace/AnimNextWorkspaceFactory.h"

#define LOCTEXT_NAMESPACE "AnimNextAssetDefinitions"

EAssetCommandResult UAssetDefinition_AnimNextDataInterface::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	using namespace UE::Workspace;

	for (UAnimNextDataInterface* Asset : OpenArgs.LoadObjects<UAnimNextDataInterface>())
	{
		IWorkspaceEditorModule& WorkspaceEditorModule = FModuleManager::Get().LoadModuleChecked<IWorkspaceEditorModule>("WorkspaceEditor");
		WorkspaceEditorModule.OpenWorkspaceForObject(Asset, EOpenWorkspaceMethod::Default, UAnimNextWorkspaceFactory::StaticClass());
	}

	return EAssetCommandResult::Handled;
}

namespace UE::AnimNext::Editor
{

static FDelayedAutoRegisterHelper AutoRegisterGraphMenuItems(EDelayedRegisterRunPhase::EndOfEngineInit, []
{
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);

		UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UAnimNextDataInterface::StaticClass());

		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
		Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			auto SetExternalPackagingStatus = [](const FToolMenuContext& InContext, bool bInUseExternalPackaging)
			{
				UContentBrowserAssetContextMenuContext* AssetContext = InContext.FindContext<UContentBrowserAssetContextMenuContext>();
				if(AssetContext == nullptr)
				{
					return;
				}

				TArray<UAnimNextRigVMAsset*> Assets;
				for (const FAssetData& AssetData : AssetContext->SelectedAssets)
				{
					if(!AssetData.GetClass()->IsChildOf(UAnimNextRigVMAsset::StaticClass()))
					{
						continue;
					}

					bool bUsesExternalPackages = true;
					AssetData.GetTagValue<bool>(UAnimNextRigVMAssetEditorData::GetUsesExternalPackagesPropertyName(), bUsesExternalPackages);

					if(bUsesExternalPackages == bInUseExternalPackaging)
					{
						// Status is as requested, skip
						continue;
					}
					
					UAnimNextRigVMAsset* Asset = Cast<UAnimNextRigVMAsset>(AssetData.GetAsset());
					if(Asset == nullptr)
					{
						continue;
					}

					Assets.Add(Asset);

					UAnimNextRigVMAssetEditorData* EditorData = UncookedOnly::FUtils::GetEditorData<UAnimNextRigVMAssetEditorData>(Asset);
					if(EditorData == nullptr)
					{
						continue;
					}

					ensure(EditorData->IsUsingExternalPackages() == bUsesExternalPackages);
				}

				UAnimNextRigVMAssetEditorData::SetUseExternalPackages(Assets, bInUseExternalPackaging);
			};

			auto ExternalPackagingStatusMatches = [](const FToolMenuContext& InContext, bool bInUsesExternalPackaging)
			{
				UContentBrowserAssetContextMenuContext* AssetContext = InContext.FindContext<UContentBrowserAssetContextMenuContext>();
				if(AssetContext == nullptr)
				{
					return false;
				}

				for (const FAssetData& AssetData : AssetContext->SelectedAssets)
				{
					if(!AssetData.GetClass()->IsChildOf(UAnimNextRigVMAsset::StaticClass()))
					{
						continue;
					}

					bool bUsesExternalPackages = true;
					AssetData.GetTagValue<bool>(UAnimNextRigVMAssetEditorData::GetUsesExternalPackagesPropertyName(), bUsesExternalPackages);

					if(bUsesExternalPackages == bInUsesExternalPackaging)
					{
						return true;
					}
				}

				return false;
			};

			FToolUIAction UIAction;
			UIAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda([&SetExternalPackagingStatus](const FToolMenuContext& InContext)
			{
				SetExternalPackagingStatus(InContext, true);
			});
			UIAction.IsActionVisibleDelegate = FToolMenuIsActionButtonVisible::CreateLambda([&ExternalPackagingStatusMatches](const FToolMenuContext& InContext)
			{
				return ExternalPackagingStatusMatches(InContext, false);
			});

			InSection.AddMenuEntry(
				"EnableExternalPackages",
				LOCTEXT("EnableExternalPackagesLabel", "Use External Packages"),
				LOCTEXT("EnableExternalPackagesTooltip", "Set the asset(s) to use external packaging for its entries (graphs, variables etc.)\nThis will create the external packages for all entries, add them to version control if enabled and save all packages.\nWarning: This operation cannot be undone, so a connection to version control is recommended."),
				FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Edit"),
				UIAction);

			UIAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda([&SetExternalPackagingStatus](const FToolMenuContext& InContext)
			{
				SetExternalPackagingStatus(InContext, false);
			});
			UIAction.IsActionVisibleDelegate = FToolMenuIsActionButtonVisible::CreateLambda([&ExternalPackagingStatusMatches](const FToolMenuContext& InContext)
			{
				return ExternalPackagingStatusMatches(InContext, true);
			});

			InSection.AddMenuEntry(
				"DisableExternalPackages",
				LOCTEXT("DisableExternalPackagesLabel", "Use Single Package"),
				LOCTEXT("DisableExternalPackagesTooltip", "Set the asset(s) to use a single package.\nThis will remove any external packages for existing entries, remove them from version control if enabled and save all packages.\nWarning: This operation cannot be undone, so a connection to version control is recommended."),
				FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Edit"),
				UIAction);
		}));
	}));
});

}

#undef LOCTEXT_NAMESPACE
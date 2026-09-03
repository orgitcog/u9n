// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanDepthGeneratorModule.h"
#include "CaptureData.h"
#include "MetaHumanDepthGenerator.h"
#include "ContentBrowserMenuContexts.h"
#include "ToolMenu.h"
#include "ToolMenuDelegates.h"

#if WITH_EDITOR
#include "MetaHumanDepthGeneratorAutoReimport.h"
#include "Settings/EditorLoadingSavingSettings.h"
#endif

#define LOCTEXT_NAMESPACE "MetaHumanDepthGeneratorModule"

void FMetaHumanDepthGeneratorModule::StartupModule()
{
	FCoreDelegates::OnPostEngineInit.Add(FSimpleDelegate::CreateRaw(this, &FMetaHumanDepthGeneratorModule::PostEngineInit));

	UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UFootageCaptureData::StaticClass());
	FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
	Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
	{
		if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection))
		{
			const TAttribute<FText> Label = LOCTEXT("GenerateDepth", "Generate Depth");
			const TAttribute<FText> ToolTip = LOCTEXT("GenerateDepth_Tooltip", "Generate depth images using the current stereo views and camera calibration");
			const FSlateIcon Icon = FSlateIcon(TEXT("MetaHumanIdentityStyle"), TEXT("ClassIcon.FootageCaptureData"), TEXT("ClassIcon.FootageCaptureData"));
				
			FToolUIAction UIAction;
			UIAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& InContext)
			{
				const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);

				UFootageCaptureData* FootageCaptureData = Context->LoadFirstSelectedObject<UFootageCaptureData>();
				
				if (FootageCaptureData)
				{
					TStrongObjectPtr<UMetaHumanDepthGenerator> DepthGeneration(NewObject<UMetaHumanDepthGenerator>());
					DepthGeneration->Process(FootageCaptureData);
				}
			});
			InSection.AddMenuEntry("GenerateFootageCaptureDataDepth", Label, ToolTip, Icon, UIAction);
		}
	}));
}

void FMetaHumanDepthGeneratorModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
}

void FMetaHumanDepthGeneratorModule::PostEngineInit()
{
#if WITH_EDITOR
	using namespace UE::MetaHuman;

	UEditorLoadingSavingSettings* Settings = GetMutableDefault<UEditorLoadingSavingSettings>();

	if (!ensure(IsValid(Settings)))
	{
		return;
	}

	// We are doing the same operation in the capture source module, so make sure they're both using the game thread
	check(IsInGameThread());

	const FString Wildcard = FString::Format(TEXT("*/{0}/*.exr"), { UMetaHumanGenerateDepthWindowOptions::ImageSequenceDirectoryName });

	// Note: We use /Game for both UE and UEFN. We rely on the project root being mapped to /Game in UEFN.
	const TArray<FAutoReimportDirectoryConfig> DirectoryConfigs = DepthGeneratorUpdateAutoReimportExclusion(
		TEXT("/Game/"), 
		Wildcard, 
		Settings->AutoReimportDirectorySettings
	);

	if (DepthGeneratorDirectoryConfigsAreDifferent(Settings->AutoReimportDirectorySettings, DirectoryConfigs))
	{
		Settings->AutoReimportDirectorySettings = DirectoryConfigs;
		Settings->SaveConfig();
		Settings->OnSettingChanged().Broadcast(GET_MEMBER_NAME_CHECKED(UEditorLoadingSavingSettings, AutoReimportDirectorySettings));
	}
#endif
}

IMPLEMENT_MODULE(FMetaHumanDepthGeneratorModule, MetaHumanDepthGenerator)

#undef LOCTEXT_NAMESPACE

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "CaptureData.h"
#include "MetaHumanCalibrationGenerator.h"
#include "ContentBrowserMenuContexts.h"
#include "ToolMenu.h"
#include "ToolMenuDelegates.h"

#define LOCTEXT_NAMESPACE "MetaHumanCalibrationGeneratorModule"

class FMetaHumanCalibrationGeneratorModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{
		UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(UFootageCaptureData::StaticClass());
		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
		Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection))
			{
				const TAttribute<FText> Label = LOCTEXT("GenerateCalibration", "Generate Calibration");
				const TAttribute<FText> ToolTip = LOCTEXT("GenerateCalibration_Tooltip", "Generate calibration lens files for the stereo camera pair");
				const FSlateIcon Icon = FSlateIcon(TEXT("MetaHumanIdentityStyle"), TEXT("ClassIcon.FootageCaptureData"), TEXT("ClassIcon.FootageCaptureData"));
				
				FToolUIAction UIAction;
				UIAction.ExecuteAction = FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext& InContext)
				{
					const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext);

					// TODO: Perform calibration on multiselected takes
					// Currently only does calibration for the first take
					UFootageCaptureData* FootageCaptureData = Context->LoadFirstSelectedObject<UFootageCaptureData>();
					if (FootageCaptureData)
					{
						TStrongObjectPtr<UMetaHumanCalibrationGenerator> StereoCalibrationGenerator(NewObject<UMetaHumanCalibrationGenerator>());
						StereoCalibrationGenerator->Process(FootageCaptureData);
					}
				});
				InSection.AddMenuEntry("GenerateFootageCaptureDataCalibration", Label, ToolTip, Icon, UIAction);
			}
		}));
	}

	virtual void ShutdownModule() override
	{
	}
};

IMPLEMENT_MODULE(FMetaHumanCalibrationGeneratorModule, MetaHumanCalibrationGenerator)

#undef LOCTEXT_NAMESPACE
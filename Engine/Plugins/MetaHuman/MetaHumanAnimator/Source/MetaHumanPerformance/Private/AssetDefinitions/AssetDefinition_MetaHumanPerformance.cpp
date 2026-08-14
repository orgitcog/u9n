// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_MetaHumanPerformance.h"

#include "ImageSequencePathChecker.h"
#include "MetaHumanPerformance.h"
#include "MetaHumanPerformanceEditor.h"
#include "MetaHumanCoreEditorModule.h"
#include "MetaHumanSupportedRHI.h"
#include "MetaHumanMinSpec.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "MetaHumanPerformance"

FText UAssetDefinition_MetaHumanPerformance::GetAssetDisplayName() const
{
	return NSLOCTEXT("MetaHuman", "PerformanceAssetName", "MetaHuman Performance");
}

FLinearColor UAssetDefinition_MetaHumanPerformance::GetAssetColor() const
{
	return FColor::Red;
}

TSoftClassPtr<UObject> UAssetDefinition_MetaHumanPerformance::GetAssetClass() const
{
	return UMetaHumanPerformance::StaticClass();
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_MetaHumanPerformance::GetAssetCategories() const
{
	return FModuleManager::GetModuleChecked<IMetaHumanCoreEditorModule>(TEXT("MetaHumanCoreEditor")).GetMetaHumanAssetCategoryPath();
}

EAssetCommandResult UAssetDefinition_MetaHumanPerformance::OpenAssets(const FAssetOpenArgs& InOpenArgs) const
{
	UE::CaptureData::FImageSequencePathChecker ImageSequencePathChecker(GetAssetDisplayName());

	for (UMetaHumanPerformance* Performance  : InOpenArgs.LoadObjects<UMetaHumanPerformance>())
	{
		if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			UMetaHumanPerformanceEditor* IdentityAssetEditor = NewObject<UMetaHumanPerformanceEditor>(AssetEditorSubsystem, NAME_None, RF_Transient);
			IdentityAssetEditor->SetObjectToEdit(Performance);
			IdentityAssetEditor->Initialize();

			FString FunctionalityMessage;

			if (!FMetaHumanMinSpec::IsSupported())
			{
				FunctionalityMessage += FText::Format(LOCTEXT("MinSpecPerformanceMessage", "Minimum specification for using a Performance is not met. Stability and performance maybe effected.\n\nMinimum specification is: {0}."), FMetaHumanMinSpec::GetMinSpec()).ToString();
			}

			if (!FMetaHumanSupportedRHI::IsSupported())
			{
				if (!FunctionalityMessage.IsEmpty())
				{
					FunctionalityMessage += TEXT("\n\n");
				}

				FunctionalityMessage += FText::Format(LOCTEXT("UnsupportedRHIPerformanceMessage", "Processing a Performance will not be possible with the current RHI. To enable processing make sure the RHI is set to {0}."), FMetaHumanSupportedRHI::GetSupportedRHINames()).ToString();
			}

			if (!FunctionalityMessage.IsEmpty())
			{
				FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(FunctionalityMessage), LOCTEXT("MinSpecPerformanceTitle", "Minimum specification"));
			}
		}

		if (Performance)
		{
			const UFootageCaptureData* FootageCaptureData = Performance->FootageCaptureData;

			if (FootageCaptureData)
			{
				ImageSequencePathChecker.Check(*FootageCaptureData);
			}
		}
	}

	if (ImageSequencePathChecker.HasError())
	{
		ImageSequencePathChecker.DisplayDialog();
	}

	return EAssetCommandResult::Handled;
}

#undef LOCTEXT_NAMESPACE

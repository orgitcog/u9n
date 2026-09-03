// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosOutfitAsset/AssetDefinition_OutfitAsset.h"
#include "ChaosOutfitAsset/OutfitAsset.h"
#include "Dataflow/DataflowEditor.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "Toolkits/SimpleAssetEditor.h"

#define LOCTEXT_NAMESPACE "AssetDefinition_OutfitAsset"

namespace UE::Chaos::OutfitAsset::Private
{
	static const FLinearColor OutfitAssetColor = FColor(162, 108, 99);
}

FText UAssetDefinition_OutfitAsset::GetAssetDisplayName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_OutfitAsset", "OutfitAsset");
}

TSoftClassPtr<UObject> UAssetDefinition_OutfitAsset::GetAssetClass() const
{
	return UChaosOutfitAsset::StaticClass();
}

FLinearColor UAssetDefinition_OutfitAsset::GetAssetColor() const
{
	return UE::Chaos::OutfitAsset::Private::OutfitAssetColor;
}

TConstArrayView<FAssetCategoryPath> UAssetDefinition_OutfitAsset::GetAssetCategories() const
{
	static const auto Categories = { EAssetCategoryPaths::Physics };
	return Categories;
}

UThumbnailInfo* UAssetDefinition_OutfitAsset::LoadThumbnailInfo(const FAssetData& InAsset) const
{
	return UE::Editor::FindOrCreateThumbnailInfo(InAsset.GetAsset(), USceneThumbnailInfo::StaticClass());
}

EAssetCommandResult UAssetDefinition_OutfitAsset::OpenAssets(const FAssetOpenArgs& OpenArgs) const
{
	TArray<UChaosOutfitAsset*> OutfitObjects = OpenArgs.LoadObjects<UChaosOutfitAsset>();

	// For now the Dataflow editor only works on one asset at a time
	if (OutfitObjects.Num() > 0 && OutfitObjects[0])
	{
		if (OutfitObjects[0]->GetDataflowInstance().GetDataflowAsset())
		{
			// Has a Dataflow Asset, open in the Dataflow editor
			UAssetEditorSubsystem* const AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
			UDataflowEditor* const AssetEditor = NewObject<UDataflowEditor>(AssetEditorSubsystem, NAME_None, RF_Transient);
			AssetEditor->RegisterToolCategories({"General", "Cloth"});

			static const TCHAR* const Path = TEXT("/ChaosOutfitAsset/BP_OutfitPreview.BP_OutfitPreview_C");
			const TSubclassOf<AActor> PreviewClass = StaticLoadClass(AActor::StaticClass(), nullptr, Path);
			AssetEditor->Initialize({ OutfitObjects[0] }, PreviewClass);
		}
		else
		{
			FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, OpenArgs.ToolkitHost, OutfitObjects[0]);
		}
		return EAssetCommandResult::Handled;
	}
	return EAssetCommandResult::Unhandled;
}

#undef LOCTEXT_NAMESPACE

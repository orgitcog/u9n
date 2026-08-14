// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanItemEditorPipeline.h"

#include "MetaHumanCharacterPalette.h"
#include "MetaHumanItemPipeline.h"

#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"

#if WITH_EDITOR

void UMetaHumanItemEditorPipeline::BuildItemSynchronous(
	const FMetaHumanPaletteItemPath& ItemPath,
	TNotNull<const UMetaHumanWardrobeItem*> WardrobeItem,
	const FInstancedStruct& BuildInput,
	TArrayView<const FMetaHumanPinnedSlotSelection> SortedPinnedSlotSelections,
	TArrayView<const FMetaHumanPaletteItemPath> SortedItemsToExclude,
	FMetaHumanPaletteBuildCacheEntry& BuildCache,
	EMetaHumanCharacterPaletteBuildQuality Quality,
	ITargetPlatform* TargetPlatform,
	TNotNull<UObject*> OuterForGeneratedObjects,
	FMetaHumanPaletteBuiltData& OutBuiltData) const
{
	FSharedEventRef Event;

	BuildItem(
		ItemPath,
		WardrobeItem,
		BuildInput,
		SortedPinnedSlotSelections,
		SortedItemsToExclude,
		BuildCache,
		Quality,
		TargetPlatform,
		OuterForGeneratedObjects,
		FOnBuildComplete::CreateLambda(
			[&OutBuiltData, Event](FMetaHumanPaletteBuiltData&& BuiltData)
			{
				OutBuiltData = MoveTemp(BuiltData);
				Event->Trigger();
			}));

	Event->Wait();
}

bool UMetaHumanItemEditorPipeline::TryUnpackItemAssets(
	TNotNull<const UMetaHumanWardrobeItem*> WardrobeItem,
	const FMetaHumanPaletteItemPath& BaseItemPath,
	TMap<FMetaHumanPaletteItemPath, FMetaHumanPipelineBuiltData>& ItemBuiltData,
	const FString& UnpackFolder,
	const FTryUnpackObjectDelegate& TryUnpackObjectDelegate) const
{
	// TODO: Unpack sub-items first

	for (const FMetaHumanGeneratedAssetMetadata& AssetMetadata : ItemBuiltData[BaseItemPath].Metadata)
	{
		if (!AssetMetadata.Object)
		{
			continue;
		}

		FString AssetPackagePath = UnpackFolder;

		if (!AssetMetadata.PreferredSubfolderPath.IsEmpty())
		{
			if (AssetMetadata.bSubfolderIsAbsolute)
			{
				AssetPackagePath = AssetMetadata.PreferredSubfolderPath;
			}
			else
			{
				AssetPackagePath = AssetPackagePath / AssetMetadata.PreferredSubfolderPath;
			}
		}

		if (!AssetMetadata.PreferredName.IsEmpty())
		{
			AssetPackagePath = AssetPackagePath / AssetMetadata.PreferredName;
		}
		else
		{
			AssetPackagePath = AssetPackagePath / AssetMetadata.Object->GetName();
		}

		if (!TryUnpackObjectDelegate.Execute(AssetMetadata.Object, AssetPackagePath))
		{
			return false;
		}
	}

	return true;
}

TNotNull<const UMetaHumanItemPipeline*> UMetaHumanItemEditorPipeline::GetRuntimePipeline() const
{
	// The editor pipeline is assumed to be a direct subobject of the runtime pipeline.
	//
	// Pipelines with a different setup can override this function.

	return CastChecked<UMetaHumanItemPipeline>(GetOuter());
}

TNotNull<const UMetaHumanCharacterPipeline*> UMetaHumanItemEditorPipeline::GetRuntimeCharacterPipeline() const
{
	return GetRuntimePipeline();
}

#endif // WITH_EDITOR

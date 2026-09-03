// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/MetaHumanOutfitEditorPipeline.h"

#include "Item/MetaHumanOutfitPipeline.h"
#include "MetaHumanDefaultEditorPipelineLog.h"
#include "MetaHumanWardrobeItem.h"

#include "Logging/StructuredLog.h"

UMetaHumanOutfitEditorPipeline::UMetaHumanOutfitEditorPipeline()
{
	Specification = CreateDefaultSubobject<UMetaHumanCharacterEditorPipelineSpecification>("Specification");
	Specification->BuildInputStruct = FMetaHumanOutfitPipelineBuildInput::StaticStruct();
}

void UMetaHumanOutfitEditorPipeline::BuildItem(
	const FMetaHumanPaletteItemPath& ItemPath,
	TNotNull<const UMetaHumanWardrobeItem*> WardrobeItem,
	const FInstancedStruct& BuildInput,
	TArrayView<const FMetaHumanPinnedSlotSelection> SortedPinnedSlotSelections,
	TArrayView<const FMetaHumanPaletteItemPath> SortedItemsToExclude,
	FMetaHumanPaletteBuildCacheEntry& BuildCache,
	EMetaHumanCharacterPaletteBuildQuality Quality,
	ITargetPlatform* TargetPlatform,
	TNotNull<UObject*> OuterForGeneratedObjects,
	const FOnBuildComplete& OnComplete) const
{
	if (!BuildInput.GetPtr<FMetaHumanOutfitPipelineBuildInput>())
	{
		UE_LOGFMT(LogMetaHumanDefaultEditorPipeline, Error, "Build input not provided to Outfit pipeline during build");
		
		OnComplete.ExecuteIfBound(FMetaHumanPaletteBuiltData());
		return;
	}

	const UObject* LoadedAsset = WardrobeItem->PrincipalAsset.LoadSynchronous();

	const FMetaHumanOutfitPipelineBuildInput& OutfitBuildInput = BuildInput.Get<FMetaHumanOutfitPipelineBuildInput>();

	FMetaHumanPaletteBuiltData BuiltDataResult;
	FMetaHumanPipelineBuiltData& OutfitBuiltData = BuiltDataResult.ItemBuiltData.Add(ItemPath);
	FMetaHumanOutfitPipelineBuildOutput& OutfitBuildOutput = OutfitBuiltData.BuildOutput.InitializeAs<FMetaHumanOutfitPipelineBuildOutput>();

	OnComplete.ExecuteIfBound(MoveTemp(BuiltDataResult));
}

TNotNull<const UMetaHumanCharacterEditorPipelineSpecification*> UMetaHumanOutfitEditorPipeline::GetSpecification() const
{
	return Specification;
}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Item/MetaHumanSkeletalMeshEditorPipeline.h"

#include "Item/MetaHumanSkeletalMeshPipeline.h"
#include "MetaHumanDefaultEditorPipelineLog.h"
#include "MetaHumanWardrobeItem.h"

#include "Engine/SkeletalMesh.h"
#include "SkeletalMeshTypes.h"
#include "Logging/StructuredLog.h"

UMetaHumanSkeletalMeshEditorPipeline::UMetaHumanSkeletalMeshEditorPipeline()
{
	Specification = CreateDefaultSubobject<UMetaHumanCharacterEditorPipelineSpecification>("Specification");
	Specification->BuildInputStruct = FMetaHumanSkeletalMeshPipelineBuildInput::StaticStruct();
}

void UMetaHumanSkeletalMeshEditorPipeline::BuildItem(
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
	if (!BuildInput.GetPtr<FMetaHumanSkeletalMeshPipelineBuildInput>())
	{
		UE_LOGFMT(LogMetaHumanDefaultEditorPipeline, Error, "Build input not provided to SkeletalMesh pipeline during build");
		
		OnComplete.ExecuteIfBound(FMetaHumanPaletteBuiltData());
		return;
	}

	const UObject* LoadedAsset = WardrobeItem->PrincipalAsset.LoadSynchronous();
	const USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(LoadedAsset);
	if (!SkeletalMesh)
	{
		UE_LOGFMT(LogMetaHumanDefaultEditorPipeline, Error, "SkeletalMesh pipeline failed to load skeletal mesh {SkeletalMesh} during build", WardrobeItem->PrincipalAsset.ToString());
		
		OnComplete.ExecuteIfBound(FMetaHumanPaletteBuiltData());
		return;
	}

	FMetaHumanPaletteBuiltData BuiltDataResult;
	FMetaHumanPipelineBuiltData& SkeletalMeshBuiltData = BuiltDataResult.ItemBuiltData.Add(ItemPath);
	FMetaHumanSkeletalMeshPipelineBuildOutput& SkeletalMeshBuildOutput = SkeletalMeshBuiltData.BuildOutput.InitializeAs<FMetaHumanSkeletalMeshPipelineBuildOutput>();

	// This pipeline only updates the material, nothing to do here at the moment.

	OnComplete.ExecuteIfBound(MoveTemp(BuiltDataResult));
}

TNotNull<const UMetaHumanCharacterEditorPipelineSpecification*> UMetaHumanSkeletalMeshEditorPipeline::GetSpecification() const
{
	return Specification;
}

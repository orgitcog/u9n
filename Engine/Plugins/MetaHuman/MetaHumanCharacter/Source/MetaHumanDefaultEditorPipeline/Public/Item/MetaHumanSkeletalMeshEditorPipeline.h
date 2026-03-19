// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanItemEditorPipeline.h"

#include "MetaHumanSkeletalMeshEditorPipeline.generated.h"

class USkeletalMesh;
class UTexture;

USTRUCT()
struct FMetaHumanSkeletalMeshPipelineBuildInput
{
	GENERATED_BODY()

};

UCLASS(EditInlineNew)
class METAHUMANDEFAULTEDITORPIPELINE_API UMetaHumanSkeletalMeshEditorPipeline : public UMetaHumanItemEditorPipeline
{
	GENERATED_BODY()

	UMetaHumanSkeletalMeshEditorPipeline();

	virtual void BuildItem(
		const FMetaHumanPaletteItemPath& ItemPath,
		TNotNull<const UMetaHumanWardrobeItem*> WardrobeItem,
		const FInstancedStruct& BuildInput,
		TArrayView<const FMetaHumanPinnedSlotSelection> SortedPinnedSlotSelections,
		TArrayView<const FMetaHumanPaletteItemPath> SortedItemsToExclude,
		FMetaHumanPaletteBuildCacheEntry& BuildCache,
		EMetaHumanCharacterPaletteBuildQuality Quality,
		ITargetPlatform* TargetPlatform,
		TNotNull<UObject*> OuterForGeneratedObjects,
		const FOnBuildComplete& OnComplete) const override;

	virtual TNotNull<const UMetaHumanCharacterEditorPipelineSpecification*> GetSpecification() const override;

private:
	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterEditorPipelineSpecification> Specification;
};

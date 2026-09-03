// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterTestEditorPipeline.h"

#include "MetaHumanWardrobeItem.h"
#include "Tests/MetaHumanCharacterTestPipeline.h"

#include "AssetRegistry/AssetData.h"

void UMetaHumanCharacterTestEditorPipeline::BuildCollection(
	TNotNull<const UMetaHumanCollection*> Collection,
	TNotNull<UObject*> OuterForGeneratedAssets,
	const TArray<FMetaHumanPinnedSlotSelection>& SortedPinnedSlotSelections,
	const TArray<FMetaHumanPaletteItemPath>& SortedItemsToExclude,
	const FInstancedStruct& BuildInput,
	EMetaHumanCharacterPaletteBuildQuality Quality,
	ITargetPlatform* TargetPlatform,
	const FOnBuildComplete& OnComplete) const
{
	// This function and some of the ones below are not used by tests yet, and so don't need an
	// implementation yet. They will be implemented if needed for testing.
	unimplemented();
}

bool UMetaHumanCharacterTestEditorPipeline::CanBuild() const
{
	return true;
}

void UMetaHumanCharacterTestEditorPipeline::UnpackCollectionAssets(
	TNotNull<UMetaHumanCollection*> Collection, 
	FMetaHumanCollectionBuiltData& CollectionBuiltData,
	const FOnUnpackComplete& OnComplete) const
{
	unimplemented();
}

bool UMetaHumanCharacterTestEditorPipeline::TryUnpackInstanceAssets(
	TNotNull<UMetaHumanCharacterInstance*> Instance,
	FInstancedStruct& AssemblyOutput,
	TArray<FMetaHumanGeneratedAssetMetadata>& AssemblyAssetMetadata,
	const FString& TargetFolder) const
{
	unimplemented();
	return false;
}

TNotNull<const UMetaHumanCharacterEditorPipelineSpecification*> UMetaHumanCharacterTestEditorPipeline::GetSpecification() const
{
	return GetDefault<UMetaHumanCharacterEditorPipelineSpecification>();
}

TSubclassOf<AActor> UMetaHumanCharacterTestEditorPipeline::GetEditorActorClass() const
{
	unimplemented();
	return nullptr;
}

bool UMetaHumanCharacterTestEditorPipeline::ShouldGenerateCollectionAndInstanceAssets() const
{
	return true;
}

UBlueprint* UMetaHumanCharacterTestEditorPipeline::WriteActorBlueprint(const FString& InBlueprintPath) const
{
	unimplemented();
	return nullptr;
}

bool UMetaHumanCharacterTestEditorPipeline::UpdateActorBlueprint(const UMetaHumanCharacterInstance* InCharacterInstance, UBlueprint* InBlueprint) const
{
	unimplemented();
	return false;
}

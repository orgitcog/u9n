// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCollectionEditorPipeline.h"

#include "MetaHumanCharacterTestEditorPipeline.generated.h"

class UMetaHumanCharacterTestPipeline;

/**
 * A minimal pipeline used for automated tests, similar to a mock.
 */
UCLASS()
class UMetaHumanCharacterTestEditorPipeline : public UMetaHumanCollectionEditorPipeline
{
	GENERATED_BODY()

public:
	// Begin UMetaHumanCharacterEditorPipeline interface
	virtual void BuildCollection(
		TNotNull<const UMetaHumanCollection*> Collection,
		TNotNull<UObject*> OuterForGeneratedAssets,
		const TArray<FMetaHumanPinnedSlotSelection>& SortedPinnedSlotSelections,
		const TArray<FMetaHumanPaletteItemPath>& SortedItemsToExclude,
		const FInstancedStruct& BuildInput,
		EMetaHumanCharacterPaletteBuildQuality Quality,
		ITargetPlatform* TargetPlatform,
		const FOnBuildComplete& OnComplete) const override;

	virtual bool CanBuild() const override;

	virtual void UnpackCollectionAssets(
		TNotNull<UMetaHumanCollection*> CharacterPalette,
		FMetaHumanCollectionBuiltData& CollectionBuiltData,
		const FOnUnpackComplete& OnComplete) const override;

	virtual bool TryUnpackInstanceAssets(
		TNotNull<UMetaHumanCharacterInstance*> Instance,
		FInstancedStruct& AssemblyOutput,
		TArray<FMetaHumanGeneratedAssetMetadata>& AssemblyAssetMetadata,
		const FString& TargetFolder) const override;

	virtual TNotNull<const UMetaHumanCharacterEditorPipelineSpecification*> GetSpecification() const override;

	virtual TSubclassOf<AActor> GetEditorActorClass() const override;

	virtual bool ShouldGenerateCollectionAndInstanceAssets() const override;

	virtual UBlueprint* WriteActorBlueprint(
		const FString& InBlueprintPath) const override;
	
	virtual bool UpdateActorBlueprint(
		const UMetaHumanCharacterInstance* InCharacterInstance,
		UBlueprint* InBlueprint) const override;
	// End UMetaHumanCharacterEditorPipeline interface
};

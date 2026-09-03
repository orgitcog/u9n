// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCharacterPipeline.h"

#include "MetaHumanCollectionPipeline.generated.h"

class UMetaHumanCollection;
class UMetaHumanCollectionEditorPipeline;

/** A Collection-specific subclass of Character Pipeline */
UCLASS(Abstract, MinimalAPI)
class UMetaHumanCollectionPipeline : public UMetaHumanCharacterPipeline
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	/** Returns the editor-only component of this pipeline */
	virtual const UMetaHumanCollectionEditorPipeline* GetEditorPipeline() const
		PURE_VIRTUAL(UMetaHumanCollectionPipeline::GetEditorPipeline,return nullptr;);

	/** Override to narrow down the return type for collection pipelines */
	virtual UMetaHumanCollectionEditorPipeline* GetMutableEditorPipeline()
		PURE_VIRTUAL(UMetaHumanCollectionPipeline::GetMutableEditorPipeline, return nullptr;);
#endif

	/**
	 * Takes the opaque built data from the Collection and evaluates it with the given parameters
	 * to produce the meshes (etc) and populate the Assembly Output.
	 * 
	 * All entries in SlotSelections are guaranteed to reference valid items in the Collection.
	 */
	virtual void AssembleCollection(
		TNotNull<const UMetaHumanCollection*> Collection,
		EMetaHumanCharacterPaletteBuildQuality Quality,
		const TArray<FMetaHumanPipelineSlotSelectionData>& SlotSelections,
		const FInstancedStruct& AssemblyInput,
		TNotNull<UObject*> OuterForGeneratedObjects,
		const FOnAssemblyComplete& OnComplete) const
		PURE_VIRTUAL(UMetaHumanCollectionPipeline::AssembleCollection,);

	/**
	 * Returns an actor class that supports Character Instances targeting this pipeline.
	 *
	 * The returned class must implement IMetaHumanCharacterActorInterface.
	 *
	 * May return null.
	 */
	virtual TSubclassOf<AActor> GetActorClass() const
		PURE_VIRTUAL(UMetaHumanCollectionPipeline::GetActorClass, return nullptr;);

	/**
	 * Returns an item pipeline instance for given asset class.
	 * 
	 * This utility can be used to provide common item pipeline class for all
	 * principal assets of the given class, effectively removing the necessity
	 * for defining a pipeline for each wardrobe item asset.
	 */
	virtual const UMetaHumanItemPipeline* GetFallbackItemPipelineForAssetType(const TSoftClassPtr<UObject>& InAssetClass) const
	{
		return nullptr;
	}
};

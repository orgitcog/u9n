// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCharacterEditorPipeline.h"

#include "MetaHumanCollectionEditorPipeline.generated.h"

struct FInstancedPropertyBag;
struct FMetaHumanCollectionBuiltData;
class UMetaHumanCollectionPipeline;

/** 
 * The Build Input struct that will be set by the Character editor for builds initiated from there.
 * 
 * If your pipeline has a custom Build Input struct, have it inherit from this one for 
 * compatibility with the Character editor.
 */
USTRUCT(BlueprintType)
struct FMetaHumanBuildInputBase
{
	GENERATED_BODY()

	/**
	 * The Character being edited by this Character editor.
	 * 
	 * Pipelines should use the preview assets for this Character when building.
	 */
	UPROPERTY()
	FMetaHumanPaletteItemKey EditorPreviewCharacter;
};

/**
 * The editor-only component of a UMetaHumanCollectionPipeline.
 */
UCLASS(Abstract, MinimalAPI)
class UMetaHumanCollectionEditorPipeline : public UMetaHumanCharacterEditorPipeline
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	DECLARE_DELEGATE_TwoParams(FOnBuildComplete, EMetaHumanBuildStatus /* Status */, TSharedPtr<FMetaHumanCollectionBuiltData> /* BuiltData */);

	/**
	 * Called before BuildCollection
	 */
	METAHUMANCHARACTERPALETTE_API virtual bool PreBuildCollection(TNotNull<UMetaHumanCollection*> InCollection, const FString& InCharacterName);

	/**
	 * Build the Collection
	 * 
	 * For any slot that has at least one entry in SortedPinnedSlotSelections, the slot will be 
	 * locked to the given selections at assembly time and users won't be able to make any
	 * assignment to the slot on the Instance.
	 * 
	 * A null selection may be pinned by having a slot selection that specifies the null item.
	 * 
	 * Items assigned to a pinned slot that are not part of the pinned selection will be added to 
	 * SortedItemsToExclude automatically.
	 * 
	 * SortedPinnedSlotSelections is a hint to the pipeline that no other selection will be 
	 * possible for the slot. The pipeline is not obliged to use this information for anything, but
	 * it may choose to bake certain data into its build output based on its knowledge about which
	 * items will be selected at assembly time. 
	 * 
	 * Items named in pinned selections should still produce build output, and will still have 
	 * AssembleItem called for them when the Instance is assembled. Pinned items may still produce 
	 * Instance Parameters during assembly.
	 */
	virtual void BuildCollection(
		TNotNull<const UMetaHumanCollection*> Collection,
		TNotNull<UObject*> OuterForGeneratedAssets,
		const TArray<FMetaHumanPinnedSlotSelection>& SortedPinnedSlotSelections,
		const TArray<FMetaHumanPaletteItemPath>& SortedItemsToExclude,
		const FInstancedStruct& BuildInput,
		EMetaHumanCharacterPaletteBuildQuality Quality,
		ITargetPlatform* TargetPlatform,
		const FOnBuildComplete& OnComplete) const
		PURE_VIRTUAL(UMetaHumanCollectionEditorPipeline::BuildCollection,);

	/**
	 * Utility to check if the pipeline has valid properties to build and unpack a collection
	 * Gives the opportunity for backwards compatibility checks and user prompts before even attempting to unpack
	 */
	virtual bool CanBuild() const
		PURE_VIRTUAL(UMetaHumanCollectionEditorPipeline::CanBuild, return true;);

	/**
	 * IMPORTANT: Don't call this directly. Call UnpackAssets on the Collection.
	 * 
	 * Builds the Collection if necessary and then moves any internal assets such as meshes and 
	 * textures out to their own packages, so that they are visible in the Content Browser.
	 * 
	 * These assets will still be referenced from the Collection's built data.
	 */
	virtual void UnpackCollectionAssets(
		TNotNull<UMetaHumanCollection*> Collection,
		FMetaHumanCollectionBuiltData& CollectionBuiltData,
		const FOnUnpackComplete& OnComplete) const
		PURE_VIRTUAL(UMetaHumanCollectionEditorPipeline::UnpackCollectionAssets,);

	/**
	 * IMPORTANT: Don't call this directly. Call TryUnpack on the Instance.
	 * 
	 * Unpacks any assets generated during Assembly and contained in the Instance itself.
	 */
	virtual bool TryUnpackInstanceAssets(
		TNotNull<UMetaHumanCharacterInstance*> Instance,
		FInstancedStruct& AssemblyOutput,
		TArray<FMetaHumanGeneratedAssetMetadata>& AssemblyAssetMetadata,
		const FString& TargetFolder) const
		PURE_VIRTUAL(UMetaHumanCollectionEditorPipeline::TryUnpackInstanceAssets,return false;);

	/** Returns the runtime pipeline instance corresponding to this editor pipeline instance. */
	METAHUMANCHARACTERPALETTE_API virtual TNotNull<const UMetaHumanCollectionPipeline*> GetRuntimePipeline() const;

	/** Calls GetRuntimePipeline. No need for subclasses to implement this. */
	METAHUMANCHARACTERPALETTE_API virtual TNotNull<const UMetaHumanCharacterPipeline*> GetRuntimeCharacterPipeline() const override;

	/**
	 * Returns an actor class that supports Character Instances targeting this pipeline.
	 * 
	 * This actor type will be used in the Character editor viewport.
	 *
	 * The returned class must implement IMetaHumanCharacterEditorActorInterface.
	 *
	 * If this returns null, it will be treated as an error but callers will handle it gracefully.
	 * Pipelines that don't have their own editor actor class can return AMetaHumanCharacterEditorActor::StaticClass().
	 */
	virtual TSubclassOf<AActor> GetEditorActorClass() const
		PURE_VIRTUAL(UMetaHumanCollectionEditorPipeline::GetEditorActorClass, return nullptr;);

	/**
	 * Returns whether the pipeline should generate Palette and Instance assets.
	 */
	virtual bool ShouldGenerateCollectionAndInstanceAssets() const
		PURE_VIRTUAL(UMetaHumanCollectionEditorPipeline::ShouldGenerateCollectionAndInstanceAssets, return true;);

	/**
	 * Generates a blueprint actor asset on the given path.
	 * 
	 * Resulting blueprint should be an asset based on or a duplicate of the GetActorClass(), but this is
	 * implementation dependent.
	 */
	virtual UBlueprint* WriteActorBlueprint(
		const FString& InBlueprintPath) const
		PURE_VIRTUAL(UMetaHumanCollectionEditorPipeline::WriteActorBlueprint, return nullptr;);
	
	/**
	 * Updates the the given blueprint asset with the given character instance.
	 * 
	 * In the implementation, user can add components to the blueprint or reconfigure
	 * it depending on the parameters (e.g. legacy, export quality etc.).
	 */
	virtual bool UpdateActorBlueprint(
		const UMetaHumanCharacterInstance* InCharacterInstance,
		UBlueprint* InBlueprint) const
		PURE_VIRTUAL(UMetaHumanCollectionEditorPipeline::UpdateActorBlueprint, return false;);
#endif // WITH_EDITOR
};

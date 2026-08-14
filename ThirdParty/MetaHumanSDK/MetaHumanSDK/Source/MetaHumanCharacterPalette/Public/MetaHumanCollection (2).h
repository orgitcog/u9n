// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCharacterPalette.h"

#include "MetaHumanCharacterEditorPipeline.h"
#include "MetaHumanPinnedSlotSelection.h"

#include "MetaHumanCollection.generated.h"

class UMetaHumanCollectionPipeline;
class UMetaHumanCollectionEditorPipeline;

#if WITH_EDITORONLY_DATA
UENUM()
enum class EMetaHumanCharacterUnpackPathMode : uint8
{
	/**
	 * Assets will be unpacked to a subfolder of the Palette's current folder.
	 * The subfolder will have the same name as the Palette.
	 *
	 * For example, when a Palette called A is at path /Game/Palettes, its assets
	 * would be unpacked to the /Game/Palettes/A folder.
	 */
	SubfolderNamedForPalette UMETA(Hidden),

	/** UnpackFolderPath is a relative path from the folder containing the Palette */
	Relative,

	/** UnpackFolderPath is an absolute path */
	Absolute
};

UENUM()
enum class EMetaHumanCharacterAssetsUnpackResult : uint8
{
	Succeeded,
	Failed
};

DECLARE_DELEGATE_OneParam(FOnMetaHumanCharacterAssetsUnpacked, EMetaHumanCharacterAssetsUnpackResult /* Result */);
#endif

/** The output of the Character Pipeline's build step for a specific platform */
USTRUCT()
struct METAHUMANCHARACTERPALETTE_API FMetaHumanCollectionBuiltData
{
	GENERATED_BODY()

public:
	/** Returns true if this built data has been populated from a successful build */
	bool IsValid() const;

	UPROPERTY()
	FMetaHumanPaletteBuiltData PaletteBuiltData;

	UPROPERTY()
	TArray<FMetaHumanPinnedSlotSelection> SortedPinnedSlotSelections;

	/** The level of quality this data was built for */
	UPROPERTY()
	EMetaHumanCharacterPaletteBuildQuality Quality = EMetaHumanCharacterPaletteBuildQuality::Production;
};

/**
 * A collection of character parts (e.g. MetaHuman Characters, clothing, hairstyles) that target 
 * slots on a Character Pipeline.
 * 
 * Create a Character Instance from a Collection to assemble a renderable character from the parts
 * contained in the Collection.
 */
UCLASS(BlueprintType)
class METAHUMANCHARACTERPALETTE_API UMetaHumanCollection : public UMetaHumanCharacterPalette
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPaletteBuilt, EMetaHumanCharacterPaletteBuildQuality /* Quality */);
	DECLARE_MULTICAST_DELEGATE(FOnPipelineChanged);
	
	UMetaHumanCollection();

	// Collections should not be created or modified outside the editor
#if WITH_EDITOR
	DECLARE_DELEGATE_OneParam(FOnBuildComplete, EMetaHumanBuildStatus /* Status */);
	
	/** Builds the collection so that Character Instances can assemble characters from it */
	void Build(
		const FInstancedStruct& BuildInput,
		EMetaHumanCharacterPaletteBuildQuality Quality,
		ITargetPlatform* TargetPlatform,
		const FOnBuildComplete& OnComplete,
		const TArray<FMetaHumanPinnedSlotSelection>& PinnedSlotSelections = TArray<FMetaHumanPinnedSlotSelection>(),
		const TArray<FMetaHumanPaletteItemPath>& ItemsToExclude = TArray<FMetaHumanPaletteItemPath>());

	/**
	 * Moves any built assets stored within this Collection to their own asset packages, making them
	 * standalone assets that can be referenced from other objects.
	 *
	 * The Collection can still be used as normal after this, as it will still reference the unpacked
	 * assets as it did before they were unpacked.
	 */
	void UnpackAssets(const FOnMetaHumanCharacterAssetsUnpacked& OnComplete = FOnMetaHumanCharacterAssetsUnpacked());
	
	/** 
	 * Sets the default Pipeline from the project settings.
	 * 
	 * Call this after constructing a Collection if you don't have a specific Pipeline to use.
	 */
	void SetDefaultPipeline();

	/**
	 * Set the Pipeline for this Collection to use.
	 * 
	 * It's not necessary to provide each Collection instance with a unique Pipeline instance, as the 
	 * Pipeline is intended to be stateless. However, the Pipeline instance will be editable in
	 * Details panels wherever the Collection is visible for editing, and users editing a Pipeline's
	 * properties from there may be surprised if these edits affect other Collections.
	 */
	void SetPipeline(TNotNull<UMetaHumanCollectionPipeline*> InPipeline);
	
	/**
	 * Sets the Pipeline to be an instance of the given class
	 */
	void SetPipelineFromClass(TSubclassOf<UMetaHumanCollectionPipeline> InPipelineClass);

	/** Convenience function to access the editor pipeline */
	const UMetaHumanCollectionEditorPipeline* GetEditorPipeline() const;
		
	virtual const UMetaHumanCharacterEditorPipeline* GetPaletteEditorPipeline() const override;
#endif

	/**
	 * The Pipeline targeted by this Collection. 
	 * 
	 * May be null if the user hasn't set a pipeline yet.
	 */
	UMetaHumanCollectionPipeline* GetMutablePipeline();
	
	/**
	 * The Pipeline targeted by this Collection. 
	 * 
	 * May be null if the user hasn't set a pipeline yet.
	 */
	const UMetaHumanCollectionPipeline* GetPipeline() const;

	virtual const UMetaHumanCharacterPipeline* GetPalettePipeline() const override;

	/** Note that the returned data is not guaranteed to be valid. Call IsValid on the result to check. */
	const FMetaHumanCollectionBuiltData& GetBuiltData(EMetaHumanCharacterPaletteBuildQuality Quality) const;
	
	/** 
	 * The Collection contains a default instance that is used for preview. 
	 * 
	 * The default instance is a valid instance in its own right and can be used at runtime.
	 * 
	 * Guaranteed to be non-null.
	 */
	TNotNull<UMetaHumanCharacterInstance*> GetMutableDefaultInstance();
	TNotNull<const UMetaHumanCharacterInstance*> GetDefaultInstance() const;
		
#if WITH_EDITORONLY_DATA
	void SetBuiltData(EMetaHumanCharacterPaletteBuildQuality Quality, FMetaHumanCollectionBuiltData&& Data);
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	// Begin UObject interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	// End UObject interface
#endif // WITH_EDITOR

	/**
	 * Resolves virtual slots in the given array of selections.
	 * 
	 * For each selection in the array, if that selection targets a virtual slot, it will be 
	 * updated to target the underlying real slot.
	 * 
	 * Once this is done, code using the resulting array can just operate on real slots and doesn't
	 * have to handle virtual slots at all.
	 * 
	 * This function assumes that PipelineSpec is valid (i.e. PipelineSpec->IsValid() returns true),
	 * otherwise it may assert or never return.
	 * 
	 * Any selections that don't resolve to valid items in the Collection will be omitted from the 
	 * returned array.
	 */
	[[nodiscard]] TArray<FMetaHumanPipelineSlotSelectionData> PropagateVirtualSlotSelections(const TArray<FMetaHumanPipelineSlotSelectionData>& Selections) const;
	
#if WITH_EDITOR
	/** 
	 * Delegate fired when a new Pipeline has been set on this Collection.
	 * 
	 * This can only happen in editor.
	 * 
	 * If any changes need to be made to the Collection in response to the Pipeline changing, such as
	 * removing items from slots that don't exist on the new Pipeline, those changes will be made
	 * before this delegate is fired.
	 */
	FOnPipelineChanged OnPipelineChanged;
#endif // WITH_EDITOR
	
#if WITH_EDITORONLY_DATA
	/** The mode for determining which folder to unpack the Collection's assets to */
	UPROPERTY(EditAnywhere, Category = "Targets", DisplayName = "Root Relative", meta = (Tooltip = "The mode for determining whether the Root path is relative to the character asset path"))
	EMetaHumanCharacterUnpackPathMode UnpackPathMode = EMetaHumanCharacterUnpackPathMode::Relative;

	/** The folder path that assets will be unpacked to. Interpreted according to UnpackPathMode. */
	UPROPERTY(EditAnywhere, Category = "Targets", DisplayName = "Root Path", meta = (EditCondition = "UnpackPathMode != EMetaHumanCharacterUnpackPathMode::SubfolderNamedForPalette", EditConditionHides, Tooltip = "Project directory to place the generated assets"))
	FString UnpackFolderPath;

	/** Returns the folder path where the assets will be unpacked, depending on the UnpackPathMode */
	FString GetUnpackFolder() const;
#endif // WITH_EDITORONLY_DATA

	/** Delegate fired when the Collection has finished building, if it succeeded */
	FOnPaletteBuilt OnPaletteBuilt;

private:
	/** The MetaHuman Collection Pipeline used to build this collection */
	UPROPERTY(EditAnywhere, Instanced, Category = "Character")
	TObjectPtr<UMetaHumanCollectionPipeline> Pipeline;
	
	UPROPERTY()
	TObjectPtr<UMetaHumanCharacterInstance> DefaultInstance;

	UPROPERTY()
	FMetaHumanCollectionBuiltData ProductionBuiltData;

#if WITH_EDITORONLY_DATA
	/** 
	 * Built data from Preview quality builds.
	 * 
	 * Should be recreated as needed for preview. Shouldn't be saved or copied.
	 */
	UPROPERTY(Transient, DuplicateTransient)
	FMetaHumanCollectionBuiltData PreviewBuiltData;

	/**
	 * A per-item cache that persists between builds.
	 * 
	 * There is not guaranteed to be an entry for every item.
	 */
	UPROPERTY()
	TMap<FMetaHumanPaletteItemPath, FMetaHumanPaletteBuildCacheEntry> ItemBuildCache;

	UPROPERTY()
	FMetaHumanPaletteBuildCacheEntry PaletteBuildCache;
#endif // WITH_EDITORONLY_DATA

	/** True if the assets in this Collection have been unpacked and are in their own packages. */
	UPROPERTY()
	bool bIsUnpacked;
};

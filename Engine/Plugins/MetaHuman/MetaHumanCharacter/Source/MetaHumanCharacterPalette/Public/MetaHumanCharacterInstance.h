// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCollectionPipeline.h"
#include "MetaHumanPaletteItemPath.h"
#include "MetaHumanPipelineSlotSelection.h"
#include "MetaHumanPipelineSlotSelectionData.h"

#include "HAL/IConsoleManager.h"
#include "MetaHumanParameterMappingTable.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
#include "UObject/Object.h"

#include "MetaHumanCharacterInstance.generated.h"

struct FMetaHumanPaletteItemKey;
struct FMetaHumanPipelineSlotSelection;
class UMetaHumanCollection;

UENUM()
enum class EMetaHumanCharacterAssemblyResult : uint8
{
	Succeeded,
	Failed
};

/** 
 * Determines how pipeline slots that don't have an item selected for them should be handled when 
 * the Character Instance is converted to a set of pinned slot selections.
 */
enum class EMetaHumanUnusedSlotBehavior : uint8
{
	/** 
	 * Unused slots should be left unpinned.
	 * 
	 * In the built Collection, the user will be able to assign these slots using a Character Instance.
	 */
	Unpinned,

	/** Unused slots should be "pinned to empty", so they will not be assignable by a Character Instance. */
	PinnedToEmpty
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FMetaHumanCharacterAssembled, EMetaHumanCharacterAssemblyResult, Result);
DECLARE_DELEGATE_OneParam(FMetaHumanCharacterAssembledNative, EMetaHumanCharacterAssemblyResult /* Result */);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMetaHumanCharacterInstanceUpdated);
DECLARE_DYNAMIC_DELEGATE(FMetaHumanCharacterInstanceUpdated_Unicast);
DECLARE_MULTICAST_DELEGATE(FMetaHumanCharacterInstanceUpdatedNative);

/**
 * Used to assemble a renderable character from a MetaHuman Collection.
 * 
 * Can be either an asset used in the editor or a transient object generated at runtime.
 */
UCLASS(BlueprintType)
class METAHUMANCHARACTERPALETTE_API UMetaHumanCharacterInstance : public UObject
{
	GENERATED_BODY()

public:
	/** 
	 * Runs the associated Character Pipeline's assembly function to populate the AssemblyOutput. 
	 * 
	 * Fails gracefully if no MetaHuman Collection is set.
	 */
	UFUNCTION(BlueprintCallable, Category = CharacterInstance)
	void Assemble(EMetaHumanCharacterPaletteBuildQuality Quality, const FMetaHumanCharacterAssembled& OnAssembled);
	void Assemble(EMetaHumanCharacterPaletteBuildQuality Quality, const FMetaHumanCharacterAssembledNative& OnAssembledNative = FMetaHumanCharacterAssembledNative());
	void Assemble(EMetaHumanCharacterPaletteBuildQuality Quality, const FMetaHumanCharacterAssembled& OnAssembled, const FMetaHumanCharacterAssembledNative& OnAssembledNative);

	/** Fetch the result of the last assembly, if any */
	UFUNCTION(BlueprintCallable, Category = CharacterInstance)
	const FInstancedStruct& GetAssemblyOutput() const;

	/** 
	 * Clear the result of the last assembly.
	 * 
	 * GetAssemblyOutput will return an empty struct after calling this.
	 * 
	 * Instance Parameters are not cleared by this function.
	 */
	UFUNCTION(BlueprintCallable, Category = CharacterInstance)
	void ClearAssemblyOutput();

	/** 
	 * Set the MetaHuman Collection that this instance will assemble from.
	 * 
	 * Call with nullptr to clear the existing Collection.
	 */
	UFUNCTION(BlueprintCallable, Category = CharacterInstance)
	void SetMetaHumanCollection(UMetaHumanCollection* InCollection);

	/** 
	 * Return the MetaHuman Collection that this instance will assemble from.
	 * 
	 * Returns nullptr if no collection has been set.
	 */
	UFUNCTION(BlueprintCallable, Category = CharacterInstance)
	UMetaHumanCollection* GetMetaHumanCollection() const
	{
		return Collection;
	}

	/** 
	 * Remove any existing selections for this slot and select only the given item.
	 * 
	 * If ItemKey is NAME_None, no item will be selected for this slot.
	 */
	void SetSingleSlotSelection(FName SlotName, const FMetaHumanPaletteItemKey& ItemKey);
	void SetSingleSlotSelection(const FMetaHumanPaletteItemPath& ParentItemPath, FName SlotName, const FMetaHumanPaletteItemKey& ItemKey);

	// Adds the provided slot selection if valid, e.g. won't allow duplicate selections or multiple selections for slots that don't allow it
	[[nodiscard]] bool TryAddSlotSelection(const FMetaHumanPipelineSlotSelection& Selection);

	/** 
	 * Get a single item selection for this slot, if there is at least one.
	 * 
	 * If there are multiple selections for this slot, this function returns an arbitrary selection
	 * and repeated calls may return a different selection.
	 */
	[[nodiscard]] bool TryGetAnySlotSelection(FName SlotName, FMetaHumanPaletteItemKey& OutItemKey) const;
	[[nodiscard]] bool TryGetAnySlotSelection(const FMetaHumanPaletteItemPath& ParentItemPath, FName SlotName, FMetaHumanPaletteItemKey& OutItemKey) const;

	[[nodiscard]] static bool TryGetAnySlotSelection(
		const TArray<FMetaHumanPipelineSlotSelectionData>& SlotSelections, 
		FName SlotName, 
		FMetaHumanPaletteItemKey& OutItemKey);

	[[nodiscard]] static bool TryGetAnySlotSelection(
		const TArray<FMetaHumanPipelineSlotSelectionData>& SlotSelections, 
		const FMetaHumanPaletteItemPath& ParentItemPath, 
		FName SlotName, 
		FMetaHumanPaletteItemKey& OutItemKey);

	bool ContainsSlotSelection(const FMetaHumanPipelineSlotSelection& Selection) const;
	// Returns true if the selection existed and was removed, false if it didn't exist
	bool TryRemoveSlotSelection(const FMetaHumanPipelineSlotSelection& Selection);

	const TArray<FMetaHumanPipelineSlotSelectionData>& GetSlotSelectionData() const;

	/** 
	 * Formats the slot selections and overridden instance parameters stored in this instance to be
	 * passed into a Collection build as pinned selections.
	 */
	TArray<FMetaHumanPinnedSlotSelection> ToPinnedSlotSelections(EMetaHumanUnusedSlotBehavior UnusedSlotBehavior) const;

	/** 
	 * Returns the Instance Parameters produced by the last assembly and their original values.
	 * 
	 * Instance Parameters are values that can be set after assembly, such as material parameters.
	 * 
	 * Their actual effect is determined by the Character Pipeline associated with the item at the
	 * corresponding item path. The parameter output for the empty item path contains the
	 * Collection Pipeline's parameters.
	 */
	const TMap<FMetaHumanPaletteItemPath, FInstancedPropertyBag>& GetAssemblyInstanceParameters() const;

	/** 
	 * Returns the Instance Parameters stored in this instance as overrides.
	 * 
	 * These overrides will be applied to any assembly created through this instance.
	 * 
	 * They may include overrides for items and parameters that don't exist on the current assembly.
	 * These persist indefinitely until explicitly cleared.
	 */
	const TMap<FMetaHumanPaletteItemPath, FInstancedPropertyBag>& GetOverriddenInstanceParameters() const;

	/**
	 * Returns the Instance Parameters produced by the last assembly and their effective values.
	 * 
	 * If a parameter has an override value (from GetOverriddenInstanceParameters), it will be set 
	 * to this override value. Otherwise, it will be set to the original value from the assembly
	 * (from GetAssemblyInstanceParameters).
	 */
	FInstancedPropertyBag GetCurrentInstanceParametersForItem(const FMetaHumanPaletteItemPath& ItemPath) const;

	/**
	 * Set the overridden Instance Parameter values for a given item, or the Collection itself if
	 * an empty item path is specified.
	 * 
	 * Overridden values set via this function will persist indefinitely until explicitly cleared.
	 * 
	 * These parameter values will be applied immediately if there's a valid assembly. If Assemble 
	 * is called after this and a new assembly is created, they will automatically be applied to 
	 * that assembly and any future assemblies.
	 */
	void OverrideInstanceParameters(const FMetaHumanPaletteItemPath& ItemPath, const FInstancedPropertyBag& NewInstanceParameterValues);

	/** Functions to clear overridden Instance Parameters */
	void ClearAllOverriddenInstanceParameters();
	void ClearOverriddenInstanceParameters(const FMetaHumanPaletteItemPath& ItemPath);

#if WITH_EDITOR
	/** 
	 * Unpacks only the assets contained in this Instance.
	 * 
	 * Does not unpack assets from the Collection that this Instance depends on.
	 * 
	 * This function is intended for internal use and may change or be removed in future.
	 */
	bool TryUnpack(const FString& TargetFolder);
#endif // WITH_EDITOR

	// Begin UObject interface
	virtual void BeginDestroy() override;
	// End UObject interface

	// TODO: The OnInstanceUpdated interface is WIP and will change
	UFUNCTION(BlueprintCallable, Category = CharacterInstance)
	void RegisterOnInstanceUpdated(const FMetaHumanCharacterInstanceUpdated_Unicast& Delegate);

	UFUNCTION(BlueprintCallable, Category = CharacterInstance)
	void UnregisterOnInstanceUpdated(UObject* Object);

	// If non-null, this overrides the runtime pipeline on the Collection
	//
	// Must be the same class as the Collection's runtime pipeline, or a subclass of it
	UPROPERTY(EditAnywhere, Category = CharacterInstance)
	TObjectPtr<UMetaHumanCollectionPipeline> OverridePipelineInstance;

	/** This delegate is mutable so that code that has a const pointer can't change the parameters, but can register for updates */
	mutable FMetaHumanCharacterInstanceUpdatedNative OnInstanceUpdatedNative;

private:
	void ApplyOverriddenInstanceParameters(const FMetaHumanPaletteItemPath& ItemPath) const;

	/*
	 * A structure produced by the Character Pipeline that contains the assets belonging to this
	 * instance, such as meshes and materials.
	 *
	 * The type of this struct is guaranteed to be Collection->GetPipeline()->GetSpecification()->AssemblyOutputStruct
	 */
	UPROPERTY(Transient)
	FInstancedStruct AssemblyOutput;
	
	UPROPERTY(Transient)
	TMap<FMetaHumanPaletteItemPath, FInstancedPropertyBag> AssemblyInstanceParameters;

	/**
	 * Opaque data about the instance parameters that only has meaning to the pipeline.
	 * 
	 * This is private data that should not be exposed via a public API on this object, to prevent 
	 * callers from relying on the implementation details of pipelines that may change.
	 */
	UPROPERTY(Transient)
	TMap<FMetaHumanPaletteItemPath, FInstancedStruct> AssemblyInstanceParameterContext;
		
	UPROPERTY()
	TMap<FMetaHumanPaletteItemPath, FInstancedPropertyBag> OverriddenInstanceParameters;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	TArray<FMetaHumanGeneratedAssetMetadata> AssemblyAssetMetadata;
#endif

	/** 
	 * The selected items for slots on the Pipeline.
	 * 
	 * Slots with no selection will select the default item for the slot.
	 * 
	 * Some slots allow multiple selections. This is determined by the UMetaHumanCharacterPipelineSpecification.
	 * 
	 * The order of items in this array has no significance. Consider it an unordered set.
	 */
	UPROPERTY(VisibleAnywhere, Category = CharacterInstance)
	TArray<FMetaHumanPipelineSlotSelectionData> SlotSelections;

	UPROPERTY()
	TObjectPtr<UMetaHumanCollection> Collection;

	void OnPaletteBuilt(EMetaHumanCharacterPaletteBuildQuality Quality);

	UPROPERTY(Transient, BlueprintAssignable, Category = CharacterInstance)
	FMetaHumanCharacterInstanceUpdated OnInstanceUpdated;

	FDelegateHandle OnPaletteBuiltHandle;
};

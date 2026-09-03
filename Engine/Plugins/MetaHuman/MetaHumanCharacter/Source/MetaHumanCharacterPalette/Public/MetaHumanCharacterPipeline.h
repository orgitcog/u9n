// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanCharacterPaletteItem.h"
#include "MetaHumanCharacterPipelineSpecification.h"
#include "MetaHumanPaletteItemPath.h"
#include "MetaHumanParameterMappingTable.h"
#include "MetaHumanPipelineSlotSelectionData.h"

#include "Misc/NotNull.h"
#include "StructUtils/InstancedStruct.h"
#include "StructUtils/PropertyBag.h"
#include "Templates/SubclassOf.h"
#include "UObject/Interface.h"

#include "MetaHumanCharacterPipeline.generated.h"


class AActor;
struct FMetaHumanPinnedSlotSelection;
class UMetaHumanCharacterEditorPipeline;
class UMetaHumanCharacterInstance;
class UMetaHumanCharacterPipelineSpecification;
class UScriptStruct;

/** 
 * The level of quality that the Palette content should be or was built for.
 * 
 * In future, Pipelines may be able to define their own quality levels. For now, this is a fixed list.
 */
UENUM()
enum class EMetaHumanCharacterPaletteBuildQuality : uint8
{
	/** Full, shipping quality */
	Production,
	/** Reduced quality for the purpose of quick preview while editing */
	Preview
};

/**
 * Metadata about a generated asset, usually one that is not in its own package.
 * 
 * This is used when unpacking assets into their own packages, to give them friendly names and
 * helpful paths chosen by the system that generated them.
 */
USTRUCT()
struct METAHUMANCHARACTERPALETTE_API FMetaHumanGeneratedAssetMetadata
{
	GENERATED_BODY()

	FMetaHumanGeneratedAssetMetadata() = default;

	FMetaHumanGeneratedAssetMetadata(TObjectPtr<UObject> InObject, const FString& InPreferredSubfolderPath, const FString& InPreferredName, bool bInSubfolderIsAbsolute = false)
		: Object(InObject)
		, PreferredSubfolderPath(InPreferredSubfolderPath)
		, bSubfolderIsAbsolute(bInSubfolderIsAbsolute)
		, PreferredName(InPreferredName)
	{
	}

	UPROPERTY()
	TObjectPtr<UObject> Object;

	/**
	 * A hint providing a useful subfolder path that this asset could be unpacked to.
	 *
	 * May contain multiple path elements, e.g. "Face/Textures".
	 */
	UPROPERTY()
	FString PreferredSubfolderPath;

	/** If true, treat PreferedSubfolderPath as an absolute package path. */
	UPROPERTY()
	bool bSubfolderIsAbsolute = false;

	/** A hint providing a useful name that this asset could be given when it's unpacked. */
	UPROPERTY()
	FString PreferredName;
};

/**
 * Output produced during assembly to specify what Instance Parameters are available. 
 * 
 * The set of supported parameters may change from one assembly to the next due to a number of 
 * factors, so users must always refer to the Parameter Output from the latest assembly to see what
 * parameters are available and the context that should be passed when setting them.
 * 
 * See the Instance Parameter functions on UMetaHumanCharacterInstance for more info.
 */
USTRUCT()
struct METAHUMANCHARACTERPALETTE_API FMetaHumanInstanceParameterOutput
{
	GENERATED_BODY()

	/**
	 * Each supported Instance Parameter should have a property set to the default value of the 
	 * parameter in this property bag.
	 */
	UPROPERTY()
	FInstancedPropertyBag Parameters;

	/**
	 * A context struct that has meaning only to the pipeline instance that produced it.
	 * 
	 * Other code should not try to parse this struct.
	 */
	UPROPERTY()
	FInstancedStruct ParameterContext;
};

/**
 * The result of assembling a Character Instance using a Collection.
 */
USTRUCT()
struct METAHUMANCHARACTERPALETTE_API FMetaHumanAssemblyOutput
{
	GENERATED_BODY()

	/**
	 * The main data produced by the pipeline.
	 * 
	 * Its type should match AssemblyOutputStruct from the pipeline specification.
	 */
	UPROPERTY()
	FInstancedStruct PipelineAssemblyOutput;

	/** An array of every asset generated for this assembly */
	UPROPERTY()
	TArray<FMetaHumanGeneratedAssetMetadata> Metadata;

	/**
	 * The Instance Parameters that are available to set on this assembly and their default values.
	 * 
	 * Items are not required to support Instance Parameters, so there may not be an entry for 
	 * every assembled item.
	 * 
	 * The Collection's own Instance Parameters are stored using the empty item path as the key.
	 */
	UPROPERTY()
	TMap<FMetaHumanPaletteItemPath, FMetaHumanInstanceParameterOutput> InstanceParameters;
};

/**
 * A Pipeline contains the functionality for building a Palette and assembling Character Instances
 * from it.
 * 
 * Each Pipeline owns an Editor Pipeline that provides its editor-only functionality, such as 
 * building the Palette.
 * 
 * The Pipeline itself is responsible for assembling Character Instances, so this can be done
 * either in editor or in a cooked build.
 */
UCLASS(Abstract, MinimalAPI)
class UMetaHumanCharacterPipeline : public UObject
{
	GENERATED_BODY()

public:
	/** If the AssemblyOutput parameter is invalid, the evaluation failed */
	using FOnAssemblyComplete = TDelegate<void(FMetaHumanAssemblyOutput&& /* AssemblyOutput */)>;

#if WITH_EDITOR
	/** 
	 * Create an instance of this pipeline's corresponding Editor Pipeline class and use it as 
	 * this pipeline's editor-only component, i.e. GetEditorPipeline should return this new 
	 * instance.
	 */
	virtual void SetDefaultEditorPipeline()
		PURE_VIRTUAL(UMetaHumanCharacterPipeline::SetDefaultEditorPipeline,);
#endif

	/**
	 * Apply the Instance Parameter values to the Assembly Output.
	 * 
	 * The Parameters argument is guaranteed to have the same members as the struct returned in the
	 * FMetaHumanInstanceParameterOutput that was produced during assembly.
	 * 
	 * ParameterContext will be the same struct returned in FMetaHumanInstanceParameterOutput.
	 * 
	 * Implementations can assume that no member variables on this Pipeline instance have been 
	 * modified since the assembly was done.
	 * 
	 * If the parameter values are out of the expected range, implementations should try to handle 
	 * this gracefully, e.g. by clamping to the allowed range. They may log warnings or errors 
	 * about this, but should not assert or crash if at all possible.
	 */
	METAHUMANCHARACTERPALETTE_API virtual void SetInstanceParameters(const FInstancedStruct& ParameterContext, const FInstancedPropertyBag& Parameters) const;

	/**
	 * Returns the specification implemented by this pipeline.
	 * 
	 * Should always returns a valid pointer.
	 */
	virtual TNotNull<const UMetaHumanCharacterPipelineSpecification*> GetSpecification() const
		PURE_VIRTUAL(UMetaHumanCharacterPipeline::GetSpecification, return NewObject<UMetaHumanCharacterPipelineSpecification>(););

	/**
	 * Takes a sorted list of selections and returns a view of any selections that relate to the filtered item or its sub-items.
	 */
	METAHUMANCHARACTERPALETTE_API static TArrayView<const FMetaHumanPinnedSlotSelection> FilterPinnedSlotSelectionsToItem(
		TArrayView<const FMetaHumanPinnedSlotSelection> SortedSlotSelections, 
		const FMetaHumanPaletteItemPath& FilteredItem);

	/** Takes a sorted list of item paths and returns a view of any that include the filtered item. */
	METAHUMANCHARACTERPALETTE_API static TArrayView<const FMetaHumanPaletteItemPath> FilterItemPaths(
		TArrayView<const FMetaHumanPaletteItemPath> SortedItemPaths, 
		const FMetaHumanPaletteItemPath& FilteredItem);
};

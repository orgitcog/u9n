// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanPipelineSlotSelection.h"

#include "StructUtils/PropertyBag.h"

#include "MetaHumanPinnedSlotSelection.generated.h"

/** 
 * An item pinned to a slot at build time
 * 
 * At assembly time, if a slot has any pinned items, it won't be able to have non-pinned items 
 * selected for it.
 */
USTRUCT()
struct METAHUMANCHARACTERPALETTE_API FMetaHumanPinnedSlotSelection
{
	GENERATED_BODY()

public:
	FMetaHumanPinnedSlotSelection() = default;

	static bool IsItemPinned(TConstArrayView<FMetaHumanPinnedSlotSelection> SortedSelections, const FMetaHumanPaletteItemPath& ItemPath);
	static bool TryGetPinnedItem(TConstArrayView<FMetaHumanPinnedSlotSelection> SortedSelections, const FMetaHumanPaletteItemPath& ItemPath, const FMetaHumanPinnedSlotSelection*& OutPinnedItem);

	UPROPERTY(EditAnywhere, Category = "Pipeline")
	FMetaHumanPipelineSlotSelection Selection;
	
	/**
	 * If the pipeline does any baking at build time that would use instance parameters, it should
	 * use these values.
	 * 
	 * If the baking is such that the parameters are no longer settable (e.g. material parameters 
	 * a material that gets baked to a texture at build time), it shouldn't expose these parameters
	 * as instance parameters during assembly.
	 * 
	 * If the parameters are still settable, they will be passed in again after assembly, so 
	 * pipelines don't have to store this data at build time if they don't do anything with it.
	 */
	UPROPERTY(EditAnywhere, Category = "Pipeline")
	FInstancedPropertyBag InstanceParameters;
};

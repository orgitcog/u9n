// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanPipelineSlotSelection.h"

#include "MetaHumanPipelineSlotSelectionData.generated.h"

struct FMetaHumanPipelineSlotSelection;
class UMetaHumanItemPipeline;

/** 
 * An item selected for a slot, with additional data about the selection.
 * 
 * It's a fine distinction, but FMetaHumanPipelineSlotSelection is like a key that identifies the 
 * selection, which may be passed around via public APIs. This struct contains data that is used to
 * process the selection.
 */
USTRUCT()
struct METAHUMANCHARACTERPALETTE_API FMetaHumanPipelineSlotSelectionData
{
	GENERATED_BODY()

public:
	FMetaHumanPipelineSlotSelectionData() = default;

	explicit FMetaHumanPipelineSlotSelectionData(const FMetaHumanPipelineSlotSelection& InSelection)
		: Selection(InSelection)
	{
	}

	UPROPERTY()
	FMetaHumanPipelineSlotSelection Selection;

	// This overrides the pipeline on the palette for this item (not implemented yet)
	UPROPERTY()
	TObjectPtr<UMetaHumanItemPipeline> OverridePipelineInstance;
};

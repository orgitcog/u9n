// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "MetaHumanCharacterEditorPipelineSpecification.generated.h"

USTRUCT()
struct METAHUMANCHARACTERPALETTE_API FMetaHumanCharacterPipelineSlotEditorData
{
	GENERATED_BODY()

	/** The type of the expected Build Input struct for this slot */
	UPROPERTY()
	TObjectPtr<UScriptStruct> BuildInputStruct;
};


UCLASS()
class METAHUMANCHARACTERPALETTE_API UMetaHumanCharacterEditorPipelineSpecification : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * The type of the expected Build Input struct for this palette.
	 * 
	 * This is on the editor specification, rather than the runtime specification, so that it can
	 * reference editor-only types.
	 */
	UPROPERTY()
	TObjectPtr<UScriptStruct> BuildInputStruct;

	/** 
	 * Editor-only data for slots defined in the runtime pipeline spec.
	 * 
	 * Slots in the runtime pipeline spec, i.e. UMetaHumanCharacterPipelineSpecification, may or 
	 * may not have editor-only data here.
	 * 
	 * The runtime pipeline spec is the source of truth for what slots exist on the pipeline. 
	 * SlotEditorData is not guaranteed to contain all slots, so when iterating slots, for example,
	 * use the runtime pipeline spec instead and use SlotEditorData only as a lookup.
	 */
	UPROPERTY()
	TMap<FName, FMetaHumanCharacterPipelineSlotEditorData> SlotEditorData;
};

// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanItemPipeline.h"

#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"

void UMetaHumanItemPipeline::AssembleItemSynchronous(
	const FMetaHumanPaletteItemPath& BaseItemPath,
	const TArray<FMetaHumanPipelineSlotSelectionData>& SlotSelections,
	const FMetaHumanPaletteBuiltData& ItemBuiltData,
	const FInstancedStruct& AssemblyInput,
	TNotNull<UObject*> OuterForGeneratedObjects,
	FMetaHumanAssemblyOutput& OutAssemblyOutput) const
{
	FSharedEventRef Event;

	AssembleItem(
		BaseItemPath,
		SlotSelections,
		ItemBuiltData,
		AssemblyInput,
		OuterForGeneratedObjects,
		FOnAssemblyComplete::CreateLambda(
			[&OutAssemblyOutput, Event](FMetaHumanAssemblyOutput&& AssemblyOutput)
			{
				OutAssemblyOutput = MoveTemp(AssemblyOutput);
				Event->Trigger();
			}));

	Event->Wait();
}

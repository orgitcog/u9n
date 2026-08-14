// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCharacterTestPipeline.h"

#include "MetaHumanCollectionEditorPipeline.h"

void UMetaHumanCharacterTestPipeline::SetSpecification(UMetaHumanCharacterPipelineSpecification* InSpecification)
{
	Specification = InSpecification;
}

#if WITH_EDITOR
void UMetaHumanCharacterTestPipeline::SetDefaultEditorPipeline()
{
	EditorPipeline = nullptr;

	const TSoftClassPtr<UMetaHumanCollectionEditorPipeline> SoftEditorPipelineClass(FSoftObjectPath(TEXT("/Script/MetaHumanCharacterPaletteEditor.MetaHumanCharacterTestEditorPipeline")));
	const TSubclassOf<UMetaHumanCollectionEditorPipeline> EditorPipelineClass(SoftEditorPipelineClass.Get());
	if (EditorPipelineClass)
	{
		EditorPipeline = NewObject<UMetaHumanCollectionEditorPipeline>(this, EditorPipelineClass);
	}
}

const UMetaHumanCollectionEditorPipeline* UMetaHumanCharacterTestPipeline::GetEditorPipeline() const
{
	return EditorPipeline;
}

UMetaHumanCollectionEditorPipeline* UMetaHumanCharacterTestPipeline::GetMutableEditorPipeline()
{
	return EditorPipeline;
}

#endif // WITH_EDITOR

TNotNull<const UMetaHumanCharacterPipelineSpecification*> UMetaHumanCharacterTestPipeline::GetSpecification() const
{
	return Specification;
}

void UMetaHumanCharacterTestPipeline::AssembleCollection(
	TNotNull<const UMetaHumanCollection*> Collection,
	EMetaHumanCharacterPaletteBuildQuality Quality,
	const TArray<FMetaHumanPipelineSlotSelectionData>& SlotSelections,
	const FInstancedStruct& AssemblyInput,
	TNotNull<UObject*> OuterForGeneratedObjects,
	const FOnAssemblyComplete& OnComplete) const
{
	unimplemented();
}

TSubclassOf<AActor> UMetaHumanCharacterTestPipeline::GetActorClass() const
{
	unimplemented();
	return nullptr;
}

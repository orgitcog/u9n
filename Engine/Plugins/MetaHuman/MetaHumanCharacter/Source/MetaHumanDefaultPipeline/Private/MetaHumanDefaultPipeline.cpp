// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanDefaultPipeline.h"

#include "MetaHumanCollectionEditorPipeline.h"

#if WITH_EDITOR
void UMetaHumanDefaultPipeline::SetDefaultEditorPipeline()
{
	EditorPipeline = nullptr;

	const TSoftClassPtr<UMetaHumanCollectionEditorPipeline> SoftEditorPipelineClass(FSoftObjectPath(TEXT("/Script/MetaHumanDefaultEditorPipeline.MetaHumanDefaultEditorPipeline")));
	const TSubclassOf<UMetaHumanCollectionEditorPipeline> EditorPipelineClass(SoftEditorPipelineClass.Get());
	if (EditorPipelineClass)
	{
		EditorPipeline = NewObject<UMetaHumanCollectionEditorPipeline>(this, EditorPipelineClass);
	}
}

const UMetaHumanCollectionEditorPipeline* UMetaHumanDefaultPipeline::GetEditorPipeline() const
{
	return EditorPipeline;
}

UMetaHumanCollectionEditorPipeline* UMetaHumanDefaultPipeline::GetMutableEditorPipeline()
{
	return EditorPipeline;
}

#endif // WITH_EDITOR

TSubclassOf<AActor> UMetaHumanDefaultPipeline::GetActorClass() const
{
	return ActorClass;
}

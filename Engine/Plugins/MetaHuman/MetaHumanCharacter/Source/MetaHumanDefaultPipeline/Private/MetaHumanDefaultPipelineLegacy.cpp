// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanDefaultPipelineLegacy.h"

#include "MetaHumanCollectionEditorPipeline.h"

#include "GameFramework/Actor.h"

#if WITH_EDITOR
void UMetaHumanDefaultPipelineLegacy::SetDefaultEditorPipeline()
{
	EditorPipeline = nullptr;

	const TSoftClassPtr<UMetaHumanCollectionEditorPipeline> SoftEditorPipelineClass(FSoftObjectPath(TEXT("/Script/MetaHumanDefaultEditorPipeline.MetaHumanDefaultEditorPipelineLegacy")));
	const TSubclassOf<UMetaHumanCollectionEditorPipeline> EditorPipelineClass(SoftEditorPipelineClass.Get());
	if (EditorPipelineClass)
	{
		EditorPipeline = NewObject<UMetaHumanCollectionEditorPipeline>(this, EditorPipelineClass);
	}
}

const UMetaHumanCollectionEditorPipeline* UMetaHumanDefaultPipelineLegacy::GetEditorPipeline() const
{
	return EditorPipeline;
}

UMetaHumanCollectionEditorPipeline* UMetaHumanDefaultPipelineLegacy::GetMutableEditorPipeline()
{
	return EditorPipeline;
}

#endif // WITH_EDITOR

TSubclassOf<AActor> UMetaHumanDefaultPipelineLegacy::GetActorClass() const
{
	return AActor::StaticClass();
}

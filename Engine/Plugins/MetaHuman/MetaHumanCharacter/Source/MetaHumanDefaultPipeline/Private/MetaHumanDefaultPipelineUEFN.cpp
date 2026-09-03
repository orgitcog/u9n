// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanDefaultPipelineUEFN.h"

#include "MetaHumanCollectionEditorPipeline.h"

#if WITH_EDITOR

constexpr FStringView DefaultUEFNPipeline = TEXTVIEW("/Script/MetaHumanDefaultEditorPipeline.MetaHumanDefaultEditorPipelineUEFN");

void UMetaHumanDefaultPipelineUEFN::SetDefaultEditorPipeline()
{
	EditorPipeline = nullptr;

	const TSoftClassPtr<UMetaHumanCollectionEditorPipeline> SoftEditorPipelineClass{ FSoftObjectPath(DefaultUEFNPipeline) };

	const TSubclassOf<UMetaHumanCollectionEditorPipeline> EditorPipelineClass(SoftEditorPipelineClass.Get());
	if (EditorPipelineClass)
	{
		EditorPipeline = NewObject<UMetaHumanCollectionEditorPipeline>(this, EditorPipelineClass);
	}
}

#endif

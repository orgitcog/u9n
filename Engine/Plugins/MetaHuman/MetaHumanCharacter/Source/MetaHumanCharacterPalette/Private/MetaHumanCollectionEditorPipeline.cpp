// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanCollectionEditorPipeline.h"

#include "MetaHumanCollectionPipeline.h"

#if WITH_EDITOR

bool UMetaHumanCollectionEditorPipeline::PreBuildCollection(TNotNull<UMetaHumanCollection*> InCollection, const FString& InCharacterName)
{
	return true;
}

TNotNull<const UMetaHumanCollectionPipeline*> UMetaHumanCollectionEditorPipeline::GetRuntimePipeline() const
{
	// The editor pipeline is assumed to be a direct subobject of the runtime pipeline.
	//
	// Pipelines with a different setup can override this function.

	return CastChecked<UMetaHumanCollectionPipeline>(GetOuter());
}

TNotNull<const UMetaHumanCharacterPipeline*> UMetaHumanCollectionEditorPipeline::GetRuntimeCharacterPipeline() const
{
	return CastChecked<UMetaHumanCharacterPipeline>(GetRuntimePipeline());
}

#endif // WITH_EDITOR

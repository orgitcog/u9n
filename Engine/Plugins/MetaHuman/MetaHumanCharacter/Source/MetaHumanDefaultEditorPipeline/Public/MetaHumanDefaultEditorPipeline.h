// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanDefaultEditorPipelineBase.h"

#include "MetaHumanDefaultEditorPipeline.generated.h"

/**
 * Editor pipeline for UMetaHumanDefaultPipeline
 */
UCLASS(EditInlineNew)
class METAHUMANDEFAULTEDITORPIPELINE_API UMetaHumanDefaultEditorPipeline : public UMetaHumanDefaultEditorPipelineBase
{
	GENERATED_BODY()

public:
	virtual bool ShouldGenerateCollectionAndInstanceAssets() const override
	{
		return true;
	}
	virtual UBlueprint* WriteActorBlueprint(const FString& InBlueprintPath) const override;
	virtual bool UpdateActorBlueprint(const UMetaHumanCharacterInstance* InCharacterInstance, UBlueprint* InBlueprint) const override;
};

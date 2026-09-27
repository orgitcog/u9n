// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanDefaultEditorPipelineBase.h"

#include "MetaHumanDefaultEditorPipelineLegacy.generated.h"

/**
 * Editor pipeline for UMetaHumanDefaultPipelineLegacy
 */
UCLASS(EditInlineNew)
class METAHUMANDEFAULTEDITORPIPELINE_API UMetaHumanDefaultEditorPipelineLegacy : public UMetaHumanDefaultEditorPipelineBase
{
	GENERATED_BODY()

public:
	// Blueprint actor class to duplicate when creating a new blueprint
	UPROPERTY(EditAnywhere, Category = "Character")
	TSubclassOf<AActor> TemplateClass;

	virtual bool ShouldGenerateCollectionAndInstanceAssets() const override
	{
		return false;
	}
	virtual UBlueprint* WriteActorBlueprint(const FString& InBlueprintPath) const override;
	virtual bool UpdateActorBlueprint(const UMetaHumanCharacterInstance* InCharacterInstance, UBlueprint* InBlueprint) const override;
};

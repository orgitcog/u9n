// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanDefaultPipelineBase.h"

#include "MetaHumanDefaultPipeline.generated.h"

class UMetaHumanCharacterEditorPipeline;

/**
 * The default pipeline for building MetaHumans.
 * 
 * This pipeline should cover the needs of most users who are making simple MetaHumans.
 * 
 * Note that this class is itself abstract. A Blueprint subclass should be used to reference the 
 * content this pipeline depends on.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew)
class METAHUMANDEFAULTPIPELINE_API UMetaHumanDefaultPipeline : public UMetaHumanDefaultPipelineBase
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual void SetDefaultEditorPipeline() override;
	virtual const UMetaHumanCollectionEditorPipeline* GetEditorPipeline() const override;
	virtual UMetaHumanCollectionEditorPipeline* GetMutableEditorPipeline() override;
#endif

	virtual TSubclassOf<AActor> GetActorClass() const override;

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, NoClear, Instanced, Category = "Character", meta = (FullyExpand, AllowedClasses = "/Script/MetaHumanDefaultEditorPipeline.MetaHumanDefaultEditorPipeline"))
	TObjectPtr<UMetaHumanCollectionEditorPipeline> EditorPipeline;
#endif

	UPROPERTY(EditAnywhere, Category = "Character", meta = (MustImplement = "/Script/MetaHumanCharacterPalette.MetaHumanCharacterActorInterface"))
	TSubclassOf<AActor> ActorClass;
};

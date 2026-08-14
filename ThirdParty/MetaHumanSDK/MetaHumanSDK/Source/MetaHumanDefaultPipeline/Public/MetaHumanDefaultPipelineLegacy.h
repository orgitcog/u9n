// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MetaHumanDefaultPipelineBase.h"

#include "MetaHumanDefaultPipelineLegacy.generated.h"

/**
 * Pipeline for building legacy actor-based MetaHumans.
 * 
 * This pipeline produces MetaHuman assets with the same structure as those produced by the 
 * cloud-based MetaHuman Creator app as much as possible, and is useful for users who have their
 * own tooling built around that structure. New users should use the non-legacy pipeline instead.
 * 
 * Note that this class is itself abstract. A Blueprint subclass should be used to reference the 
 * content this pipeline depends on.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew)
class METAHUMANDEFAULTPIPELINE_API UMetaHumanDefaultPipelineLegacy : public UMetaHumanDefaultPipelineBase
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	virtual void SetDefaultEditorPipeline() override;
	virtual const UMetaHumanCollectionEditorPipeline* GetEditorPipeline() const override;
	virtual UMetaHumanCollectionEditorPipeline* GetMutableEditorPipeline() override;
#endif

	virtual TSubclassOf<AActor> GetActorClass() const override;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, NoClear, Instanced, Category = "Character", meta = (FullyExpand, AllowedClasses = "/Script/MetaHumanDefaultEditorPipeline.MetaHumanDefaultEditorPipelineLegacy"))
	TObjectPtr<UMetaHumanCollectionEditorPipeline> EditorPipeline;
#endif
};

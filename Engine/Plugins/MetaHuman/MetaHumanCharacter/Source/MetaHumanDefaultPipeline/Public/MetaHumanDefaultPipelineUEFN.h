// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanDefaultPipelineLegacy.h"

#include "MetaHumanDefaultPipelineUEFN.generated.h"

#pragma once

/**
 * Pipeline for building UEFN actor-based MetaHumans.
 *
 * This pipeline is based on the legacy version since the UEFN structure for MetaHumans is the
 * same as the one from the legacy pipelines but it will save the assets at the end of the build
 * process and will add the MetaHuman Component for UEFN in the MetaHuman blueprint.
 *
 * Note that this class is itself abstract. A Blueprint subclass should be used to reference the
 * content this pipeline depends on.
 */
UCLASS(Abstract)
class METAHUMANDEFAULTPIPELINE_API UMetaHumanDefaultPipelineUEFN : public UMetaHumanDefaultPipelineLegacy
{
	GENERATED_BODY()

public:
	//~Begin UMetaHumanDefaultPipelineLegacy interface
#if WITH_EDITOR
	virtual void SetDefaultEditorPipeline() override;
#endif
	//~End UMetaHumanDefaultPipelineLegacy interface

	UPROPERTY(EditAnywhere, Category = "Build", Meta = (FilePathFilter = "UEFN Project (*.uefnproject)|*.uefnproject"))
	FFilePath UefnProjectFilePath;
};
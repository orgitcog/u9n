// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InterchangePipelineConfigurationBase.h"

#include "InterchangePipelineConfigurationGeneric.generated.h"

class UInterchangeBaseNodeContainer;
class UInterchangeSourceData;

UCLASS(BlueprintType, editinlinenew)
class INTERCHANGEEDITORPIPELINES_API UInterchangePipelineConfigurationGeneric : public UInterchangePipelineConfigurationBase
{
	GENERATED_BODY()

public:

protected:

	virtual EInterchangePipelineConfigurationDialogResult ShowPipelineConfigurationDialog(TArray<FInterchangeStackInfo>& PipelineStacks
		, TArray<UInterchangePipelineBase*>& OutPipelines
		, TWeakObjectPtr<UInterchangeSourceData> SourceData
		, TWeakObjectPtr <UInterchangeTranslatorBase> Translator
		, TWeakObjectPtr<UInterchangeBaseNodeContainer> BaseNodeContainer) override;
	virtual EInterchangePipelineConfigurationDialogResult ShowScenePipelineConfigurationDialog(TArray<FInterchangeStackInfo>& PipelineStacks
		, TArray<UInterchangePipelineBase*>& OutPipelines
		, TWeakObjectPtr<UInterchangeSourceData> SourceData
		, TWeakObjectPtr <UInterchangeTranslatorBase> Translator
		, TWeakObjectPtr<UInterchangeBaseNodeContainer> BaseNodeContainer) override;
	virtual EInterchangePipelineConfigurationDialogResult ShowReimportPipelineConfigurationDialog(TArray<FInterchangeStackInfo>& PipelineStacks
		, TArray<UInterchangePipelineBase*>& OutPipelines
		, TWeakObjectPtr<UInterchangeSourceData> SourceData
		, TWeakObjectPtr <UInterchangeTranslatorBase> Translator
		, TWeakObjectPtr<UInterchangeBaseNodeContainer> BaseNodeContainer
		, TWeakObjectPtr <UObject> ReimportAsset
		, bool bSceneImport) override;
	virtual EInterchangePipelineConfigurationDialogResult ShowTestPlanConfigurationDialog(TArray<FInterchangeStackInfo>& PipelineStacks
		, TArray<UInterchangePipelineBase*>& OutPipelines
		, TWeakObjectPtr<UInterchangeSourceData> SourceData
		, TWeakObjectPtr <UInterchangeTranslatorBase> Translator
		, TWeakObjectPtr<UInterchangeBaseNodeContainer> BaseNodeContainer
		, TWeakObjectPtr <UObject> ReimportAsset
		, bool bSceneImport
		, bool bReimport) override;
};

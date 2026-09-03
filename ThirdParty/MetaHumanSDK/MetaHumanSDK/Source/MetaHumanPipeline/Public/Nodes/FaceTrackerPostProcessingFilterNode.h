// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Pipeline/Node.h"
#include "UObject/ObjectPtr.h"
#include "DNAAsset.h"
#include "FrameAnimationData.h"

class IFaceTrackerPostProcessingFilter;

namespace UE::MetaHuman::Pipeline
{

class METAHUMANPIPELINE_API FFaceTrackerPostProcessingFilterNode : public FNode
{
public:

	FFaceTrackerPostProcessingFilterNode(const FString& InName);

	virtual bool Start(const TSharedPtr<FPipelineData>& InPipelineData) override;
	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;
	virtual bool End(const TSharedPtr<FPipelineData>& InPipelineData) override;

	FString TemplateData;
	FString ConfigData;
	FString DefinitionsData;
	FString HierarchicalDefinitionsData;
	FString DNAFile;
	TWeakObjectPtr<UDNAAsset> DNAAsset;
	TArray<FFrameAnimationData> FrameData;
	FString DebuggingFolder;
	bool bSolveForTweakers = false;

	enum ErrorCode
	{
		FailedToInitialize = 0
	};

protected:

	TSharedPtr<IFaceTrackerPostProcessingFilter> Filter = nullptr;
	int32 FrameNumber = 0;
};

// The managed node is a version of the above that take care of loading the correct config
// rather than these being specified by an externally.

class METAHUMANPIPELINE_API FFaceTrackerPostProcessingFilterManagedNode : public FFaceTrackerPostProcessingFilterNode
{
public:

	FFaceTrackerPostProcessingFilterManagedNode(const FString& InName);
};

}

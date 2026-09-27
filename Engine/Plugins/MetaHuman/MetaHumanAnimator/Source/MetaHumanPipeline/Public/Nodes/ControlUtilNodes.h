// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Pipeline/Node.h"
#include "Pipeline/PipelineData.h"
#include "FrameRange.h"


namespace UE::MetaHuman::Pipeline
{

class METAHUMANPIPELINE_API FDropFrameNode : public FNode
{
public:

	FDropFrameNode(const FString& InName);

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

	int32 DropEvery = -1;
	TArray<FFrameRange> ExcludedFrames;
};

}

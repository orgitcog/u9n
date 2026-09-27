// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Pipeline/Node.h"
#include "Pipeline/PipelineData.h"
#include "HAL/ThreadSafeBool.h"



namespace UE::MetaHuman::Pipeline
{

class METAHUMANPIPELINECORE_API FNeutralFrameNode : public FNode
{
public:

	FNeutralFrameNode(const FString& InName);

	virtual bool Process(const TSharedPtr<FPipelineData>& InPipelineData) override;

	FThreadSafeBool bIsNeutralFrame = false;
};

}

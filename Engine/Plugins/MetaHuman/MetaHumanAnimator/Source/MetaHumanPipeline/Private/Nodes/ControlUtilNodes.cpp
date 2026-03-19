// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/ControlUtilNodes.h"

namespace UE::MetaHuman::Pipeline
{

FDropFrameNode::FDropFrameNode(const FString& InName) : FNode("DropFrame", InName)
{
}

bool FDropFrameNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	bool bDropFrame = false;

	const int32 FrameNumber = InPipelineData->GetFrameNumber();

	bDropFrame = (DropEvery > 0 && FrameNumber % DropEvery == 0);

	if (!bDropFrame)
	{
		bDropFrame = FFrameRange::ContainsFrame(FrameNumber, ExcludedFrames);
	}

	if (bDropFrame)
	{
		InPipelineData->SetDropFrame(true);
	}

	return true;
}

}

// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/AnimationUtilNodes.h"

namespace UE::MetaHuman::Pipeline
{

FAnimationMergeNode::FAnimationMergeNode(const FString& InName) : FNode("AnimationMerge", InName)
{
	Pins.Add(FPin("Animation In 1", EPinDirection::Input, EPinType::Animation, 0));
	Pins.Add(FPin("Animation In 2", EPinDirection::Input, EPinType::Animation, 1));
	Pins.Add(FPin("Animation Out", EPinDirection::Output, EPinType::Animation));
}

bool FAnimationMergeNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	const FFrameAnimationData& Animation0 = InPipelineData->GetData<FFrameAnimationData>(Pins[0]);
	const FFrameAnimationData& Animation1 = InPipelineData->GetData<FFrameAnimationData>(Pins[1]);

	FFrameAnimationData Output = Animation0;

	for (const auto& Control : Animation1.AnimationData)
	{
		if (Output.AnimationData.Contains(Control.Key))
		{
			Output.AnimationData[Control.Key] = Control.Value;
		}
		else
		{
			InPipelineData->SetErrorNodeCode(ErrorCode::UnknownControlValue);
			InPipelineData->SetErrorNodeMessage(TEXT("Unknown control value: ") + Control.Key);
			return false;
		}
	}

	Output.AudioProcessingMode = Animation1.AudioProcessingMode;

	InPipelineData->SetData<FFrameAnimationData>(Pins[2], MoveTemp(Output));

	return true;
}

}

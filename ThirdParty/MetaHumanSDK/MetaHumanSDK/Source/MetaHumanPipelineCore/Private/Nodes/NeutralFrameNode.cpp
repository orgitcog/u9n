// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/NeutralFrameNode.h"



namespace UE::MetaHuman::Pipeline
{

FNeutralFrameNode::FNeutralFrameNode(const FString& InName) : FNode("NeutralFrame", InName)
{
	Pins.Add(FPin("Neutral Frame Out", EPinDirection::Output, EPinType::Bool));
}

bool FNeutralFrameNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	const bool bIsNeutralFrameCopy = bIsNeutralFrame;
	if (bIsNeutralFrameCopy)
	{
		bIsNeutralFrame = false;
	}

	InPipelineData->SetData<bool>(Pins[0], bIsNeutralFrameCopy);

	return true;
}

}
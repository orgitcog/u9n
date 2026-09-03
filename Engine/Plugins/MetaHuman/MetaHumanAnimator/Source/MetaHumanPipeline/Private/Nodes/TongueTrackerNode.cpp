// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/TongueTrackerNode.h"
#include "Templates/UniquePtr.h"

#if WITH_EDITOR

namespace UE::MetaHuman::Pipeline
{

	const TArray<FString> FTongueTrackerNode::AffectedRawTongueControls = {
		TEXT("CTRL_expressions_tongueBendDown"),
		TEXT("CTRL_expressions_tongueBendUp"),
		TEXT("CTRL_expressions_tongueRight"),
		TEXT("CTRL_expressions_tongueDown"),
		TEXT("CTRL_expressions_tongueIn"),
		TEXT("CTRL_expressions_tongueLeft"),
		TEXT("CTRL_expressions_tongueNarrow"),
		TEXT("CTRL_expressions_tongueOut"),
		TEXT("CTRL_expressions_tonguePress"),
		TEXT("CTRL_expressions_tongueRoll"),
		TEXT("CTRL_expressions_tongueThick"),
		TEXT("CTRL_expressions_tongueThin"),
		TEXT("CTRL_expressions_tongueTipUp"),
		TEXT("CTRL_expressions_tongueTipDown"),
		TEXT("CTRL_expressions_tongueTipLeft"),
		TEXT("CTRL_expressions_tongueTipRight"),
		TEXT("CTRL_expressions_tongueTwistLeft"),
		TEXT("CTRL_expressions_tongueTwistRight"),
		TEXT("CTRL_expressions_tongueUp"),
		TEXT("CTRL_expressions_tongueWide")
};

FTongueTrackerNode::FTongueTrackerNode(const FString& InName) : FSpeechToAnimNode("TongueTracker", InName)
{
}

FTongueTrackerNode::~FTongueTrackerNode() = default;

bool FTongueTrackerNode::PostConversionModifyRawControls(TMap<FString, float>& InOutAnimationFrame, FString& OutErrorMsg)
{
	// Extract tongue controls
	TMap<FString, float> CurAnimationFrame = InOutAnimationFrame;
	InOutAnimationFrame.Empty();
	for (const FString& TongueControlName : AffectedRawTongueControls)
	{
		if (!CurAnimationFrame.Contains(TongueControlName))
		{
			OutErrorMsg = "Failed to extract tongue UI controls from tongue animation result. Please upgrade your MetaHuman Identity to the latest MetaHuman rig version.";
			return false;
		}
		InOutAnimationFrame.Add(TongueControlName, CurAnimationFrame[TongueControlName]);
	}

	return true;
}

bool FTongueTrackerNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	int32 InternalFrameIndex = InPipelineData->GetFrameNumber() - ProcessingStartFrameOffset;

	if (InternalFrameIndex < Animation.Num())
	{
		FFrameAnimationData AnimationData;
		AnimationData.AudioProcessingMode = EAudioProcessingMode::TongueTracking;
		AnimationData.AnimationData = MoveTemp(Animation[InternalFrameIndex]);
		
		InPipelineData->SetData<FFrameAnimationData>(Pins[0], MoveTemp(AnimationData));

		return true;
	}
	else
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::InvalidFrame);
		InPipelineData->SetErrorNodeMessage("Invalid frame");
		return false;
	}
}

}

#endif // WITH_EDITOR
// Copyright Epic Games, Inc. All Rights Reserved.

#include "Nodes/HyprsenseRealtimeSmoothingNode.h"



namespace UE::MetaHuman::Pipeline
{

FHyprsenseRealtimeSmoothingNode::FHyprsenseRealtimeSmoothingNode(const FString& InName) : FNode("HyprsenseRealtimeSmoothing", InName)
{
	Pins.Add(FPin("Animation In", EPinDirection::Input, EPinType::Animation));
	Pins.Add(FPin("Animation Out", EPinDirection::Output, EPinType::Animation));
}

bool FHyprsenseRealtimeSmoothingNode::Start(const TSharedPtr<FPipelineData>& InPipelineData)
{
	Smoothing = MakeShared<FMetaHumanRealtimeSmoothing>(Parameters);
	Keys.Reset();

	return true;
}

bool FHyprsenseRealtimeSmoothingNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	const FFrameAnimationData& Input = InPipelineData->GetData<FFrameAnimationData>(Pins[0]);
	FFrameAnimationData Output = Input;

	// Can only smooth when we have valid animation
	if (Input.AnimationData.IsEmpty())
	{
		InPipelineData->SetData<FFrameAnimationData>(Pins[1], MoveTemp(Output));

		return true;
	}

	// Get smoothing key names on first frame - these are the animation curve names plus head pose info
	if (Keys.IsEmpty())
	{
		for (const TPair<FString, float>& AnimPair : Input.AnimationData)
		{
			Keys.Add(FName(AnimPair.Key));
		}

		Keys.Add(FName("HeadRoll"));
		Keys.Add(FName("HeadPitch"));
		Keys.Add(FName("HeadYaw"));
		Keys.Add(FName("HeadTranslationX"));
		Keys.Add(FName("HeadTranslationY"));
		Keys.Add(FName("HeadTranslationZ"));
	}

	const FRotator HeadRotator = Input.Pose.Rotator();
	const FVector HeadTranslation = Input.Pose.GetTranslation();

	// Fill in values to be smoothed
	TArray<float> Values;
	Input.AnimationData.GenerateValueArray(Values);

	Values.Add(HeadRotator.Roll);
	Values.Add(HeadRotator.Pitch);
	Values.Add(HeadRotator.Yaw);
	Values.Add(HeadTranslation.X);
	Values.Add(HeadTranslation.Y);
	Values.Add(HeadTranslation.Z);

	// Do smoothing
	if (!Smoothing->ProcessFrame(Keys, Values, DeltaTime))
	{
		InPipelineData->SetErrorNodeCode(ErrorCode::SmoothingFailed);
		InPipelineData->SetErrorNodeMessage(TEXT("Smoothing failed"));
		return false;
	}

	// Fill in output animation with new smoothed values
	int32 ValueIndex = 0;
	for (TPair<FString, float>& AnimPair : Output.AnimationData)
	{
		AnimPair.Value = Values[ValueIndex++];
	}

	// Convert back to the expected pose from the head bone relative transformation output by the smoothing - the opposite of the above
	const float Roll = Values[ValueIndex++];
	const float Pitch = Values[ValueIndex++];
	const float Yaw = Values[ValueIndex++];
	const FRotator NewHeadRotator(Pitch, Yaw, Roll);

	const float X = Values[ValueIndex++];
	const float Y = Values[ValueIndex++];
	const float Z = Values[ValueIndex++];
	const FVector NewHeadTranslation(X, Y, Z);

	const FTransform NewHeadPose(NewHeadRotator, NewHeadTranslation);

	Output.Pose = NewHeadPose;

	InPipelineData->SetData<FFrameAnimationData>(Pins[1], MoveTemp(Output));

	return true;
}

bool FHyprsenseRealtimeSmoothingNode::End(const TSharedPtr<FPipelineData>& InPipelineData)
{
	Smoothing.Reset();
	Keys.Reset();

	return true;
}

}
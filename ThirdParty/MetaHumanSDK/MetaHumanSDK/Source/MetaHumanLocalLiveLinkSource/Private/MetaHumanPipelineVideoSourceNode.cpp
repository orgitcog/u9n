// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPipelineVideoSourceNode.h"



namespace UE::MetaHuman::Pipeline
{

FVideoSourceNode::FVideoSourceNode(const FString& InName) : FNode("VideoSource", InName)
{
	Pins.Add(FPin("UE Image Out", EPinDirection::Output, EPinType::UE_Image));
	Pins.Add(FPin("UE Image Sample Time Out", EPinDirection::Output, EPinType::QualifiedFrameTime));
	Pins.Add(FPin("Dropped Frame Out", EPinDirection::Output, EPinType::Bool));
	Pins.Add(FPin("UE Image Sample Time Source Out", EPinDirection::Output, EPinType::Int));
}

bool FVideoSourceNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	FUEImageDataType Image;
	FQualifiedFrameTime ImageSampleTime;
	FMetaHumanLocalLiveLinkSubject::ETimeSource ImageSampleTimeSource = FMetaHumanLocalLiveLinkSubject::ETimeSource::NotSet;
	bool bDroppedFrame = false;

	int32 NumSamples = 0;

	while (NumSamples == 0)
	{ 
		if (*bAbort)
		{
			return false;
		}

		{
			FScopeLock Lock(&Mutex);

			NumSamples = VideoSamples.Num();

			if (!ErrorMessage.IsEmpty())
			{
				InPipelineData->SetErrorNodeCode(ErrorCode::GeneralError);
				InPipelineData->SetErrorNodeMessage(ErrorMessage);

				return false;
			}
		}

		if (NumSamples == 0)
		{
			FPlatformProcess::Sleep(0.001f);
		}
	}

	if (NumSamples == 0)
	{
		return false;
	}
	else
	{
		bDroppedFrame = NumSamples > 1;

		FScopeLock Lock(&Mutex);

		Image = MoveTemp(VideoSamples[NumSamples - 1].Image);
		ImageSampleTime = VideoSamples[NumSamples - 1].Time;
		ImageSampleTimeSource = VideoSamples[NumSamples - 1].TimeSource;

		VideoSamples.Reset();
	}

	InPipelineData->SetData<FUEImageDataType>(Pins[0], MoveTemp(Image));
	InPipelineData->SetData<FQualifiedFrameTime>(Pins[1], ImageSampleTime);
	InPipelineData->SetData<bool>(Pins[2], bDroppedFrame);
	InPipelineData->SetData<int32>(Pins[3], static_cast<uint8>(ImageSampleTimeSource));

	return true;
}

bool FVideoSourceNode::End(const TSharedPtr<FPipelineData>& InPipelineData)
{
	FScopeLock Lock(&Mutex);

	VideoSamples.Reset();
	ErrorMessage = "";

	return true;
}

void FVideoSourceNode::AddVideoSample(FVideoSample&& InVideoSample)
{
	FScopeLock Lock(&Mutex);

	VideoSamples.Add(MoveTemp(InVideoSample));
}

void FVideoSourceNode::SetError(const FString& InErrorMessage)
{
	FScopeLock Lock(&Mutex);

	ErrorMessage = InErrorMessage;
}

}
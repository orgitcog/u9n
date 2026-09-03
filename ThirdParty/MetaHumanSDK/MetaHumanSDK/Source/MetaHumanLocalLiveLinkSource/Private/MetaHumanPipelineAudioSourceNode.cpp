// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanPipelineAudioSourceNode.h"



namespace UE::MetaHuman::Pipeline
{

FAudioSourceNode::FAudioSourceNode(const FString& InName) : FNode("AudioSource", InName)
{
	Pins.Add(FPin("Audio Out", EPinDirection::Output, EPinType::Audio));
	Pins.Add(FPin("Audio Sample Time Out", EPinDirection::Output, EPinType::QualifiedFrameTime));
	Pins.Add(FPin("Audio Sample Time Source Out", EPinDirection::Output, EPinType::Int));
}

bool FAudioSourceNode::Process(const TSharedPtr<FPipelineData>& InPipelineData)
{
	FAudioDataType Audio;
	FQualifiedFrameTime AudioSampleTime;
	FMetaHumanLocalLiveLinkSubject::ETimeSource AudioSampleTimeSource = FMetaHumanLocalLiveLinkSubject::ETimeSource::NotSet;

	int32 NumSamples = 0;

	while (NumSamples == 0)
	{ 
		if (*bAbort)
		{
			return false;
		}

		{
			FScopeLock Lock(&Mutex);

			NumSamples = AudioSamples.Num();

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
		FScopeLock Lock(&Mutex);

		Audio = MoveTemp(AudioSamples[0].Audio);
		AudioSampleTime = AudioSamples[0].Time;
		AudioSampleTimeSource = AudioSamples[0].TimeSource;

		AudioSamples.RemoveAt(0);
	}

	InPipelineData->SetData<FAudioDataType>(Pins[0], MoveTemp(Audio));
	InPipelineData->SetData<FQualifiedFrameTime>(Pins[1], AudioSampleTime);
	InPipelineData->SetData<int32>(Pins[2], static_cast<uint8>(AudioSampleTimeSource));

	return true;
}

bool FAudioSourceNode::End(const TSharedPtr<FPipelineData>& InPipelineData)
{
	FScopeLock Lock(&Mutex);

	AudioSamples.Reset();
	ErrorMessage = "";

	return true;
}

void FAudioSourceNode::AddAudioSample(FAudioSample&& InAudioSample)
{
	FScopeLock Lock(&Mutex);

	AudioSamples.Add(MoveTemp(InAudioSample));
}

void FAudioSourceNode::SetError(const FString& InErrorMessage)
{
	FScopeLock Lock(&Mutex);

	ErrorMessage = InErrorMessage;
}

}
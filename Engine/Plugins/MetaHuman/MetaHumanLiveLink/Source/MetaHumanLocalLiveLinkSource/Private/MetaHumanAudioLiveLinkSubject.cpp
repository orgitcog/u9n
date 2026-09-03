// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanAudioLiveLinkSubject.h"

#include "MetaHumanPipelineMediaPlayerUENode.h"

#include "Async/Async.h"



FMetaHumanAudioLiveLinkSubject::FMetaHumanAudioLiveLinkSubject(ILiveLinkClient* InLiveLinkClient, const FGuid& InSourceGuid, const FName& InSubjectName, UMetaHumanAudioLiveLinkSubjectSettings* InSettings) : FMetaHumanAudioBaseLiveLinkSubject(InLiveLinkClient, InSourceGuid, InSubjectName, InSettings)
{
	AnalyticsItems.Add(TEXT("DeviceFormat"), InSettings->MediaSourceCreateParams.AudioTrackFormatName);

	if (InSettings->MediaSourceCreateParams.AudioURL.StartsWith(UE::MetaHuman::Pipeline::FMediaPlayerNode::BundleURL))
	{
		AnalyticsItems.Add(TEXT("DeviceModel"), TEXT("MediaBundle"));
	}
	else
	{
		AnalyticsItems.Add(TEXT("DeviceModel"), InSettings->MediaSourceCreateParams.AudioName);
	}

	MediaPlayer = MakeShared<UE::MetaHuman::Pipeline::FMediaPlayerUENode>("MediaPlayer");
	MediaPlayer->StartTimeout = InSettings->MediaSourceCreateParams.StartTimeout;
	MediaPlayer->FormatWaitTime = InSettings->MediaSourceCreateParams.FormatWaitTime;
	MediaPlayer->SampleTimeout = InSettings->MediaSourceCreateParams.SampleTimeout;
	MediaPlayer->Play("", -1, -1, InSettings->MediaSourceCreateParams.AudioURL, InSettings->MediaSourceCreateParams.AudioTrack, InSettings->MediaSourceCreateParams.AudioTrackFormat);

	for (UE::MetaHuman::Pipeline::FPin& Pin : MediaPlayer->Pins)
	{
		Pin.Address = MediaPlayer->Name + "." + Pin.Name;
	}
}

FMetaHumanAudioLiveLinkSubject::~FMetaHumanAudioLiveLinkSubject()
{
	AsyncTask(ENamedThreads::GameThread, [MediaPlayer = this->MediaPlayer]()
	{
		if (!MediaPlayer->Close())
		{
			UE_LOG(LogMetaHumanLocalLiveLinkSubject, Warning, TEXT("Failed to close media player"));
		}
	});

	MediaPlayer.Reset();
}

void FMetaHumanAudioLiveLinkSubject::MediaSamplerMain()
{
	TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> PipelineData;

	MediaPlayer->bAbort = GetIsRunningPtr();

	PipelineData = MakeShared<UE::MetaHuman::Pipeline::FPipelineData>();
	if (!MediaPlayer->Start(PipelineData))
	{
		SetError(PipelineData->GetErrorNodeMessage());
		return;
	}

	int32 Frame = 0;

	while (IsRunning())
	{
		PipelineData = MakeShared<UE::MetaHuman::Pipeline::FPipelineData>();
		PipelineData->SetFrameNumber(Frame++);
		if (!MediaPlayer->Process(PipelineData))
		{
			SetError(PipelineData->GetErrorNodeMessage());
			break;
		}

		UE::MetaHuman::Pipeline::FAudioDataType Audio = PipelineData->MoveData<UE::MetaHuman::Pipeline::FAudioDataType>(MediaPlayer->Name + ".Audio Out");

		FAudioSample AudioSample;
		AudioSample.NumChannels = Audio.NumChannels;
		AudioSample.SampleRate = Audio.SampleRate;
		AudioSample.NumSamples = Audio.NumSamples;
		AudioSample.Data = MoveTemp(Audio.Data);
		AudioSample.Time = PipelineData->GetData<FQualifiedFrameTime>(MediaPlayer->Name + TEXT(".Audio Sample Time Out"));
		AudioSample.TimeSource = static_cast<ETimeSource>(PipelineData->GetData<int32>(MediaPlayer->Name + TEXT(".Audio Sample Time Source Out")));

		AddAudioSample(MoveTemp(AudioSample));
	}

	PipelineData = MakeShared<UE::MetaHuman::Pipeline::FPipelineData>();
	if (!MediaPlayer->End(PipelineData))
	{
		SetError(PipelineData->GetErrorNodeMessage());
		return;
	}
}

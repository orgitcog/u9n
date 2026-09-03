// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanVideoLiveLinkSubject.h"

#include "MetaHumanPipelineMediaPlayerUENode.h"
#include "MetaHumanPipelineMediaPlayerWMFNode.h"
#include "MetaHumanPipelineMediaPlayerWMFReaderNode.h"

#include "Async/Async.h"



TAutoConsoleVariable<FString> CVarMetaHumanLiveLinkMediaPlayer
{
	TEXT("mh.LiveLink.MediaPlayer"),
	"WMF",
	TEXT("Controls which media player is used. Options are \"WMF\", \"WMFReader\" or \"UE\""),
	ECVF_Default
};

FMetaHumanVideoLiveLinkSubject::FMetaHumanVideoLiveLinkSubject(ILiveLinkClient* InLiveLinkClient, const FGuid& InSourceGuid, const FName& InSubjectName, UMetaHumanVideoLiveLinkSubjectSettings* InSettings) : FMetaHumanVideoBaseLiveLinkSubject(InLiveLinkClient, InSourceGuid, InSubjectName, InSettings)
{
	AnalyticsItems.Add(TEXT("DeviceFormat"), InSettings->MediaSourceCreateParams.VideoTrackFormatName);

	if (InSettings->MediaSourceCreateParams.VideoURL.StartsWith(UE::MetaHuman::Pipeline::FMediaPlayerNode::BundleURL))
	{
		AnalyticsItems.Add(TEXT("DeviceModel"), TEXT("MediaBundle"));

		MediaPlayer = MakeShared<UE::MetaHuman::Pipeline::FMediaPlayerUENode>("MediaPlayerUE");
	}
	else
	{
		AnalyticsItems.Add(TEXT("DeviceModel"), InSettings->MediaSourceCreateParams.VideoName);

		const FString MediaPlayerType = CVarMetaHumanLiveLinkMediaPlayer.GetValueOnAnyThread();

		if (MediaPlayerType == "WMF")
		{
			MediaPlayer = MakeShared<UE::MetaHuman::Pipeline::FMediaPlayerWMFNode>("MediaPlayerWMF");
		}
		else if (MediaPlayerType == "WMFReader")
		{
			MediaPlayer = MakeShared<UE::MetaHuman::Pipeline::FMediaPlayerWMFReaderNode>("MediaPlayerWMFReader");
		}
		else if (MediaPlayerType == "UE")
		{ 
			MediaPlayer = MakeShared<UE::MetaHuman::Pipeline::FMediaPlayerUENode>("MediaPlayerUE");
		}
		else
		{
			UE_LOG(LogMetaHumanLocalLiveLinkSubject, Warning, TEXT("Unknown media player option: %s"), *MediaPlayerType);

			MediaPlayer = MakeShared<UE::MetaHuman::Pipeline::FMediaPlayerWMFNode>("MediaPlayerWMF");
		}
	}

	UE_LOG(LogMetaHumanLocalLiveLinkSubject, Display, TEXT("Using media player: %s"), *MediaPlayer->Name);

	MediaPlayer->StartTimeout = InSettings->MediaSourceCreateParams.StartTimeout;
	MediaPlayer->FormatWaitTime = InSettings->MediaSourceCreateParams.FormatWaitTime;
	MediaPlayer->SampleTimeout = InSettings->MediaSourceCreateParams.SampleTimeout;
	MediaPlayer->Play(InSettings->MediaSourceCreateParams.VideoURL, InSettings->MediaSourceCreateParams.VideoTrack, InSettings->MediaSourceCreateParams.VideoTrackFormat);

	for (UE::MetaHuman::Pipeline::FPin& Pin : MediaPlayer->Pins)
	{
		Pin.Address = MediaPlayer->Name + "." + Pin.Name;
	}
}

FMetaHumanVideoLiveLinkSubject::~FMetaHumanVideoLiveLinkSubject()
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

void FMetaHumanVideoLiveLinkSubject::MediaSamplerMain()
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

		UE::MetaHuman::Pipeline::FUEImageDataType Image = PipelineData->MoveData<UE::MetaHuman::Pipeline::FUEImageDataType>(MediaPlayer->Name + ".UE Image Out");

		FVideoSample VideoSample;
		VideoSample.Width = Image.Width;
		VideoSample.Height = Image.Height;
		VideoSample.Data = MoveTemp(Image.Data);
		VideoSample.Time = PipelineData->GetData<FQualifiedFrameTime>(MediaPlayer->Name + TEXT(".UE Image Sample Time Out"));
		VideoSample.TimeSource = static_cast<ETimeSource>(PipelineData->GetData<int32>(MediaPlayer->Name + TEXT(".UE Image Sample Time Source Out")));

		AddVideoSample(MoveTemp(VideoSample));
	}

	PipelineData = MakeShared<UE::MetaHuman::Pipeline::FPipelineData>();
	if (!MediaPlayer->End(PipelineData))
	{
		SetError(PipelineData->GetErrorNodeMessage());
		return;
	}
}

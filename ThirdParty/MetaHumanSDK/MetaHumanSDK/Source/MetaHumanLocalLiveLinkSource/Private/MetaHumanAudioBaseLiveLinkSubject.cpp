// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetaHumanAudioBaseLiveLinkSubject.h"

#include "MetaHumanPipelineAudioSourceNode.h"
#include "Nodes/RealtimeSpeechToAnimNode.h"
#include "Nodes/AudioUtilNodes.h"

DEFINE_LOG_CATEGORY_STATIC(LogMetaHumanAudioBaseLiveLinkSourceProcessing, Log, All);



FMetaHumanAudioBaseLiveLinkSubject::FMetaHumanAudioBaseLiveLinkSubject(ILiveLinkClient* InLiveLinkClient, const FGuid& InSourceGuid, const FName& InSubjectName, UMetaHumanAudioBaseLiveLinkSubjectSettings* InSettings) : FMetaHumanMediaSamplerLiveLinkSubject(InLiveLinkClient, InSourceGuid, InSubjectName, InSettings)
{
	AnalyticsItems.Add(TEXT("DeviceType"), TEXT("Audio"));

	// Create pipeline

	AudioSource = MakeShared<UE::MetaHuman::Pipeline::FAudioSourceNode>("MediaPlayer");

	TSharedPtr<UE::MetaHuman::Pipeline::FAudioConvertNode> Convert = MakeShared<UE::MetaHuman::Pipeline::FAudioConvertNode>("Convert");
	Convert->NumChannels = 1;
	Convert->SampleRate = 16000;

	RealtimeAudioSolver = MakeShared<UE::MetaHuman::Pipeline::FRealtimeSpeechToAnimNode>("RealtimeAudioSolver");
	if (!RealtimeAudioSolver->LoadModels())
	{
		UE_LOG(LogMetaHumanAudioBaseLiveLinkSourceProcessing, Warning, TEXT("Failed to load realtime model"));
	}

	Pipeline.AddNode(AudioSource);
	Pipeline.AddNode(Convert);
	Pipeline.AddNode(RealtimeAudioSolver);

	Pipeline.MakeConnection(AudioSource, Convert);
	Pipeline.MakeConnection(Convert, RealtimeAudioSolver);
}

void FMetaHumanAudioBaseLiveLinkSubject::ExtractPipelineData(TSharedPtr<UE::MetaHuman::Pipeline::FPipelineData> InPipelineData)
{
	Animation = InPipelineData->MoveData<FFrameAnimationData>(RealtimeAudioSolver->Name + TEXT(".Animation Out"));

	SceneTime = InPipelineData->GetData<FQualifiedFrameTime>(AudioSource->Name + TEXT(".Audio Sample Time Out"));

	// Latency timestamps
	Timestamps.Reset();
	Timestamps.Add(TEXT("Sample Timestamp"), SceneTime.AsSeconds());
	Timestamps.Add(TEXT("Processing Start"), InPipelineData->GetMarkerEndTime(AudioSource->Name));
	Timestamps.Add(TEXT("Processing End"), FDateTime::Now().GetTimeOfDay().GetTotalSeconds());
}

void FMetaHumanAudioBaseLiveLinkSubject::AddAudioSample(FAudioSample&& InAudioSample)
{
	UE::MetaHuman::Pipeline::FAudioSourceNode::FAudioSample PipelineAudioSample;

	PipelineAudioSample.Audio.NumChannels = InAudioSample.NumChannels;
	PipelineAudioSample.Audio.SampleRate = InAudioSample.SampleRate;
	PipelineAudioSample.Audio.NumSamples = InAudioSample.NumSamples;
	PipelineAudioSample.Audio.Data = MoveTemp(InAudioSample.Data);
	PipelineAudioSample.Time = InAudioSample.Time;
	PipelineAudioSample.TimeSource = InAudioSample.TimeSource;

	AudioSource->AddAudioSample(MoveTemp(PipelineAudioSample));
}

void FMetaHumanAudioBaseLiveLinkSubject::SetError(const FString& InErrorMessage)
{
	AudioSource->SetError(InErrorMessage);
}
